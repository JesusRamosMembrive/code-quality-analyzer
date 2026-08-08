#include "doctest/doctest.h"

#include "codeqa/frontend/ParseSourceFile.h"
#include "codeqa/frontend/StructuralExtractor.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{

class TempSourceFile
{
public:
    TempSourceFile(const std::string& name, const std::string& content)
        : path_(std::filesystem::temp_directory_path() / "codeqa_structural_extractor_test" / name)
    {
        std::filesystem::create_directories(path_.parent_path());
        std::ofstream(path_) << content;
    }

    ~TempSourceFile()
    {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    TempSourceFile(const TempSourceFile&) = delete;
    TempSourceFile& operator=(const TempSourceFile&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("extracts functions and typed parameters from C")
{
    const TempSourceFile file{
        "functions.c",
        "static int apply(int value, int (*operation)(int))\n"
        "{\n"
        "    return operation(value);\n"
        "}\n"
        "\n"
        "int sum(int count, ...)\n"
        "{\n"
        "    return count;\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::C, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 2);
    CHECK(result.functions[0].name == "apply");
    CHECK(result.functions[0].kind == codeqa::frontend::FunctionKind::Free);
    CHECK(result.functions[0].isStatic);
    REQUIRE(result.functions[0].parameters.size() == 2);
    CHECK(
        result.functions[0].parameters[1].passing
        == codeqa::frontend::ParameterPassing::FunctionPointer);

    CHECK(result.functions[1].name == "sum");
    CHECK(result.functions[1].kind == codeqa::frontend::FunctionKind::Free);
    CHECK(result.functions[1].isVariadic);
}

TEST_CASE("does not treat the word static inside a comment as the storage specifier")
{
    const TempSourceFile file{
        "commented-static.cpp",
        "int /* static analysis */ foo()\n"
        "{\n"
        "    return 1;\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 1);
    CHECK_FALSE(result.functions[0].isStatic);
}

TEST_CASE("extracts typed parameters from C++")
{
    const TempSourceFile file{
        "parameters.cpp",
        "int update(int& target, const int* source, int optional = 7)\n"
        "{\n"
        "    target += *source;\n"
        "    return optional;\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 1);
    REQUIRE(result.functions[0].parameters.size() == 3);
    CHECK(
        result.functions[0].parameters[0].passing
        == codeqa::frontend::ParameterPassing::Reference);
    CHECK(
        result.functions[0].parameters[1].passing
        == codeqa::frontend::ParameterPassing::Pointer);
    CHECK(
        result.functions[0].parameters[2].passing
        == codeqa::frontend::ParameterPassing::Value);
    CHECK_FALSE(result.functions[0].parameters[0].hasDefaultValue);
    CHECK_FALSE(result.functions[0].parameters[1].hasDefaultValue);
    CHECK(result.functions[0].parameters[2].hasDefaultValue);
}

TEST_CASE("derives C++ parameter passing from the declarator only")
{
    const TempSourceFile file{
        "parameter-declarators.cpp",
        "#include <vector>\n"
        "void classify(std::vector<int*> values, int value = *fallback,\n"
        "    int flags = left & right, int* pointer, int& reference,\n"
        "    int (*callback)(int));\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 1);
    REQUIRE(result.functions[0].parameters.size() == 6);
    CHECK(
        result.functions[0].parameters[0].passing
        == codeqa::frontend::ParameterPassing::Value);
    CHECK(
        result.functions[0].parameters[1].passing
        == codeqa::frontend::ParameterPassing::Value);
    CHECK(
        result.functions[0].parameters[2].passing
        == codeqa::frontend::ParameterPassing::Value);
    CHECK(
        result.functions[0].parameters[3].passing
        == codeqa::frontend::ParameterPassing::Pointer);
    CHECK(
        result.functions[0].parameters[4].passing
        == codeqa::frontend::ParameterPassing::Reference);
    CHECK(
        result.functions[0].parameters[5].passing
        == codeqa::frontend::ParameterPassing::FunctionPointer);
}

TEST_CASE("extracts the declared type of each parameter, excluding declarator markers")
{
    const TempSourceFile file{
        "parameter-types.cpp",
        "void classify(bool flag, const bool constFlag, bool* flagPointer,\n"
        "    bool& flagReference, int value, void (*callback)(int))\n"
        "{\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 1);
    REQUIRE(result.functions[0].parameters.size() == 6);
    CHECK(result.functions[0].parameters[0].declaredType == "bool");
    CHECK(result.functions[0].parameters[1].declaredType == "const bool");
    CHECK(result.functions[0].parameters[2].declaredType == "bool");
    CHECK(result.functions[0].parameters[3].declaredType == "bool");
    CHECK(result.functions[0].parameters[4].declaredType == "int");
    CHECK(result.functions[0].parameters[5].declaredType == "void");
}

TEST_CASE("extracts the declared type of unnamed and void C parameters")
{
    const TempSourceFile file{
        "unnamed-parameter-types.c",
        "int ignoresArgument(int)\n"
        "{\n"
        "    return 0;\n"
        "}\n"
        "\n"
        "int noArguments(void)\n"
        "{\n"
        "    return 0;\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::C, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 2);
    REQUIRE(result.functions[0].parameters.size() == 1);
    CHECK(result.functions[0].parameters[0].declaredType == "int");
    REQUIRE(result.functions[1].parameters.size() == 1);
    CHECK(result.functions[1].parameters[0].declaredType == "void");
}

TEST_CASE("extracts a C function with a void parameter list")
{
    const TempSourceFile file{
        "void-parameters.c",
        "int no_arguments(void)\n"
        "{\n"
        "    return 0;\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::C, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 1);
    CHECK(result.functions[0].name == "no_arguments");
    REQUIRE(result.functions[0].parameters.size() == 1);
    CHECK(result.functions[0].parameters[0].name.empty());
}

TEST_CASE("extracts a C++ constructor")
{
    const TempSourceFile file{
        "constructor.cpp",
        "class Counter\n"
        "{\n"
        "public:\n"
        "    Counter()\n"
        "    {\n"
        "    }\n"
        "};\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 1);
    CHECK(result.functions[0].name == "Counter");
    CHECK(result.functions[0].kind == codeqa::frontend::FunctionKind::Constructor);
}

TEST_CASE("classifies operators from their C++ declarators")
{
    const TempSourceFile file{
        "operator-kinds.cpp",
        "int operatorHelper() { return 1; }\n"
        "int cooperator() { return 2; }\n"
        "struct Number\n"
        "{\n"
        "    Number operator+(const Number&) const { return {}; }\n"
        "};\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 3);
    CHECK(result.functions[0].name == "operatorHelper");
    CHECK(result.functions[0].kind == codeqa::frontend::FunctionKind::Free);
    CHECK(result.functions[1].name == "cooperator");
    CHECK(result.functions[1].kind == codeqa::frontend::FunctionKind::Free);
    CHECK(result.functions[2].name == "operator+");
    CHECK(result.functions[2].kind == codeqa::frontend::FunctionKind::Operator);
}

TEST_CASE("extracts a C++ lambda")
{
    const TempSourceFile file{
        "lambda.cpp",
        "int countPositive(int value)\n"
        "{\n"
        "    const auto predicate = [](int candidate) { return candidate > 0; };\n"
        "    return predicate(value);\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 2);
    CHECK(result.functions[1].name == "<lambda>");
    CHECK(result.functions[1].kind == codeqa::frontend::FunctionKind::Lambda);
    REQUIRE(result.functions[1].parameters.size() == 1);
    CHECK(result.functions[1].parameters[0].name == "candidate");
}

TEST_CASE("extracts function body locals and control flow")
{
    const TempSourceFile file{
        "body-structure.c",
        "int helper(int value) { return value; }\n"
        "int scan(int count) {\n"
        "    int total = 0;\n"
        "    for (int index = 0; index < count; ++index) {\n"
        "        if (index ? 1 : 0) {\n"
        "            while (total < 10) { do { ++total; } while (total < 5); break; }\n"
        "            continue;\n"
        "        }\n"
        "    }\n"
        "    switch (total) { case 0: return helper(total); default: return total; }\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::C, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 2);
    const codeqa::frontend::FunctionModel& scan = result.functions[1];
    REQUIRE(scan.localVariables.size() == 2);
    CHECK(scan.localVariables[0].name == "total");
    CHECK(scan.localVariables[0].declaredType == "int");
    CHECK(scan.localVariables[0].hasInitializer);
    CHECK(scan.localVariables[0].lexicalDepth == 0);
    CHECK(scan.localVariables[1].name == "index");
    CHECK(scan.localVariables[1].declaredType == "int");
    CHECK(scan.localVariables[1].hasInitializer);
    CHECK(scan.localVariables[1].lexicalDepth == 1);

    const std::vector<codeqa::frontend::ControlFlowKind> expectedFlow{
        codeqa::frontend::ControlFlowKind::For,
        codeqa::frontend::ControlFlowKind::If,
        codeqa::frontend::ControlFlowKind::Conditional,
        codeqa::frontend::ControlFlowKind::While,
        codeqa::frontend::ControlFlowKind::DoWhile,
        codeqa::frontend::ControlFlowKind::Break,
        codeqa::frontend::ControlFlowKind::Continue,
        codeqa::frontend::ControlFlowKind::Switch,
        codeqa::frontend::ControlFlowKind::Case,
        codeqa::frontend::ControlFlowKind::Return,
        codeqa::frontend::ControlFlowKind::Default,
        codeqa::frontend::ControlFlowKind::Return};
    REQUIRE(scan.controlFlow.size() == expectedFlow.size());

    for (std::size_t index = 0; index < expectedFlow.size(); ++index)
    {
        CHECK(scan.controlFlow[index].kind == expectedFlow[index]);
    }

    CHECK(scan.controlFlow[0].kind == codeqa::frontend::ControlFlowKind::For);
    CHECK(scan.controlFlow[1].kind == codeqa::frontend::ControlFlowKind::If);
    CHECK(scan.controlFlow[1].conditionRange.has_value());
    CHECK_FALSE(scan.controlFlow[9].conditionRange.has_value());
}

TEST_CASE("extracts C++ range-for iterator locals and control flow")
{
    const TempSourceFile file{
        "range-for-body.cpp",
        "void visit(const int* values, int count) {\n"
        "    int total = 0;\n"
        "    for (const auto& item : values) {\n"
        "        total += item;\n"
        "    }\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.diagnostics.empty());
    REQUIRE(result.functions.size() == 1);
    const codeqa::frontend::FunctionModel& visit = result.functions[0];
    REQUIRE(visit.localVariables.size() == 2);
    CHECK(visit.localVariables[0].name == "total");
    CHECK(visit.localVariables[1].name == "item");
    CHECK(visit.localVariables[1].declaredType == "const auto&");
    CHECK_FALSE(visit.localVariables[1].hasInitializer);
    CHECK(visit.localVariables[1].lexicalDepth == 1);
    CHECK(visit.localVariables[0].range.begin.offset < visit.localVariables[1].range.begin.offset);
    REQUIRE(visit.controlFlow.size() == 1);
    CHECK(visit.controlFlow[0].kind == codeqa::frontend::ControlFlowKind::For);
}

TEST_CASE("extracts syntactic calls without resolving symbols")
{
    const TempSourceFile file{
        "syntactic-calls.cpp",
        "struct Sink { int emit(int value); };\n"
        "int helper(int value) { return value; }\n"
        "Sink factory();\n"
        "using Callback = int (*)(int);\n"
        "Callback callbackFactory();\n"
        "int run(Sink* sink, int (*callback)(int), int value) {\n"
        "    int first = helper(value);\n"
        "    int second = sink->emit(value);\n"
        "    int third = callback(value);\n"
        "    int fourth = callbackFactory()(value);\n"
        "    return factory().emit(first + second + third) + fourth;\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    const codeqa::frontend::FunctionModel* run = nullptr;

    for (const codeqa::frontend::FunctionModel& function : result.functions)
    {
        if (function.name == "run")
        {
            run = &function;
            break;
        }
    }

    REQUIRE(run != nullptr);
    REQUIRE(run->calls.size() == 7);
    CHECK(run->calls[0].calleeText == "helper");
    CHECK(run->calls[0].kind == codeqa::frontend::CallKind::Named);
    CHECK(run->calls[0].argumentCount == 1);
    CHECK(run->calls[1].calleeText == "sink->emit");
    CHECK(run->calls[1].kind == codeqa::frontend::CallKind::Member);
    CHECK(run->calls[1].argumentCount == 1);
    CHECK(run->calls[2].calleeText == "callback");
    CHECK(run->calls[2].kind == codeqa::frontend::CallKind::Named);
    CHECK(run->calls[2].argumentCount == 1);
    CHECK(run->calls[3].calleeText == "callbackFactory");
    CHECK(run->calls[3].kind == codeqa::frontend::CallKind::Named);
    CHECK(run->calls[3].argumentCount == 0);
    CHECK(run->calls[4].calleeText == "callbackFactory()");
    CHECK(run->calls[4].kind == codeqa::frontend::CallKind::Indirect);
    CHECK(run->calls[4].argumentCount == 1);
    CHECK(run->calls[5].calleeText == "factory");
    CHECK(run->calls[5].kind == codeqa::frontend::CallKind::Named);
    CHECK(run->calls[5].argumentCount == 0);
    CHECK(run->calls[6].calleeText == "factory().emit");
    CHECK(run->calls[6].kind == codeqa::frontend::CallKind::Member);
    CHECK(run->calls[6].argumentCount == 1);

    for (std::size_t index = 1; index < run->calls.size(); ++index)
    {
        CHECK(run->calls[index - 1].range.begin.offset <= run->calls[index].range.begin.offset);
    }
}

TEST_CASE("keeps lambda calls out of the enclosing callable")
{
    const TempSourceFile file{
        "lambda-body.cpp",
        "int helper(int value) { return value; }\n"
        "void run() {\n"
        "    int outer = 0;\n"
        "    []() { int inner = helper(1); if (inner) { return; } }();\n"
        "    outer = helper(outer);\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 3);
    const codeqa::frontend::FunctionModel& run = result.functions[1];
    const codeqa::frontend::FunctionModel& lambda = result.functions[2];
    REQUIRE(run.localVariables.size() == 1);
    CHECK(run.localVariables[0].name == "outer");
    CHECK(run.controlFlow.empty());
    CHECK(lambda.kind == codeqa::frontend::FunctionKind::Lambda);
    REQUIRE(lambda.localVariables.size() == 1);
    CHECK(lambda.localVariables[0].name == "inner");
    REQUIRE(lambda.controlFlow.size() == 2);
    CHECK(lambda.controlFlow[0].kind == codeqa::frontend::ControlFlowKind::If);
    CHECK(lambda.controlFlow[1].kind == codeqa::frontend::ControlFlowKind::Return);
    REQUIRE(run.calls.size() == 2);
    CHECK(run.calls[0].kind == codeqa::frontend::CallKind::Indirect);
    CHECK(run.calls[1].calleeText == "helper");
    REQUIRE(lambda.calls.size() == 1);
    CHECK(lambda.calls[0].calleeText == "helper");
}

TEST_CASE("keeps local method body data out of the enclosing callable")
{
    const TempSourceFile file{
        "local-method-body.cpp",
        "void run() {\n"
        "    int outer = 0;\n"
        "    struct Local {\n"
        "        void member() { int inner = 1; if (inner) { return; } }\n"
        "    };\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 2);
    const codeqa::frontend::FunctionModel& run = result.functions[0];
    const codeqa::frontend::FunctionModel& member = result.functions[1];
    REQUIRE(run.localVariables.size() == 1);
    CHECK(run.localVariables[0].name == "outer");
    CHECK(run.controlFlow.empty());
    CHECK(member.name == "member");
    REQUIRE(member.localVariables.size() == 1);
    CHECK(member.localVariables[0].name == "inner");
    REQUIRE(member.controlFlow.size() == 2);
    CHECK(member.controlFlow[0].kind == codeqa::frontend::ControlFlowKind::If);
    CHECK(member.controlFlow[1].kind == codeqa::frontend::ControlFlowKind::Return);
}

TEST_CASE("recovers reliable body data from partial files")
{
    const TempSourceFile file{
        "partial-body.c",
        "int valid(void) { int stable = 1; return stable; }\n"
        "int broken(void) { int value = ; }\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::C, "detected language", parser, extractor);

    REQUIRE_FALSE(result.diagnostics.empty());
    REQUIRE(result.functions.size() == 2);
    const codeqa::frontend::FunctionModel& valid = result.functions[0];
    const codeqa::frontend::FunctionModel& broken = result.functions[1];
    REQUIRE(valid.localVariables.size() == 1);
    CHECK(valid.localVariables[0].name == "stable");
    REQUIRE(valid.controlFlow.size() == 1);
    CHECK(valid.controlFlow[0].kind == codeqa::frontend::ControlFlowKind::Return);
    CHECK(broken.localVariables.empty());
    CHECK(broken.controlFlow.empty());
}

TEST_CASE("skips a function definition with no declarator instead of crashing")
{
    const TempSourceFile file{
        "missing-declarator.cpp",
        "void f\n"
        "{\n"
        "}\n"
        "\n"
        "int valid()\n"
        "{\n"
        "    return 1;\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 1);
    CHECK(result.functions[0].name == "valid");
}

TEST_CASE("extracts const method prototypes and template ranges")
{
    const TempSourceFile file{
        "template-ranges.cpp",
        "template <typename T>\n"
        "T largest(T left, T right)\n"
        "{\n"
        "    return left < right ? right : left;\n"
        "}\n"
        "\n"
        "class Counter\n"
        "{\n"
        "public:\n"
        "    int value() const;\n"
        "};\n"
        "\n"
        "template <typename T>\n"
        "class Box {};\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    const codeqa::frontend::FunctionModel* largest = nullptr;
    const codeqa::frontend::FunctionModel* value = nullptr;

    for (const codeqa::frontend::FunctionModel& function : result.functions)
    {
        if (function.name == "largest")
        {
            largest = &function;
        }
        else if (function.name == "value")
        {
            value = &function;
        }
    }

    const codeqa::frontend::ClassModel* box = nullptr;

    for (const codeqa::frontend::ClassModel& type : result.classes)
    {
        if (type.name == "Box")
        {
            box = &type;
        }
    }

    CHECK(largest != nullptr);

    if (largest != nullptr)
    {
        CHECK(largest->range.begin.line == 1);
        CHECK(largest->range.end.line == 5);
    }

    CHECK(value != nullptr);

    if (value != nullptr)
    {
        CHECK(value->kind == codeqa::frontend::FunctionKind::Prototype);
    }

    CHECK(box != nullptr);

    if (box != nullptr)
    {
        CHECK(box->range.begin.line == 13);
    }
}

TEST_CASE("extracts types from C++ namespaces and declarations")
{
    const TempSourceFile file{
        "types.cpp",
        "namespace outer {\n"
        "namespace {\n"
        "class Base {};\n"
        "class Derived : public Base {};\n"
        "template <typename T> class Box {};\n"
        "enum class Mode { Fast };\n"
        "}\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.classes.size() == 6);
    CHECK(result.classes[0].name == "outer");
    CHECK(result.classes[0].kind == codeqa::frontend::ClassKind::Namespace);
    CHECK(result.classes[1].name.empty());
    CHECK(result.classes[1].kind == codeqa::frontend::ClassKind::Namespace);
    CHECK(result.classes[3].name == "Derived");
    CHECK(result.classes[3].kind == codeqa::frontend::ClassKind::Class);
    CHECK(result.classes[3].baseSpecifiers == std::vector<std::string>{"Base"});
    CHECK(result.classes[4].name == "Box");
    CHECK(result.classes[4].isTemplate);
    CHECK(result.classes[5].name == "Mode");
    CHECK(result.classes[5].kind == codeqa::frontend::ClassKind::Enum);
    CHECK(result.classes[5].isScopedEnum);
}

TEST_CASE("detects scoped C++ enums from their syntax")
{
    const TempSourceFile file{
        "scoped-enums.cpp",
        "enum\n"
        "class Mode { Fast };\n"
        "enum /* comment */ class State { Ready };\n"
        "enum Plain { Value };\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.classes.size() == 3);
    CHECK(result.classes[0].name == "Mode");
    CHECK(result.classes[0].isScopedEnum);
    CHECK(result.classes[1].name == "State");
    CHECK(result.classes[1].isScopedEnum);
    CHECK(result.classes[2].name == "Plain");
    CHECK_FALSE(result.classes[2].isScopedEnum);
}

TEST_CASE("extracts include dependencies")
{
    const TempSourceFile file{
        "includes.cpp",
        "#include <vector>\n"
        "#include \"local.h\"\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.includes.size() == 2);
    CHECK(result.includes[0].target == "vector");
    CHECK(result.includes[0].delimiter == codeqa::frontend::IncludeDelimiter::System);
    CHECK(result.includes[0].range.begin.line == 1);
    CHECK(result.includes[1].target == "local.h");
    CHECK(result.includes[1].delimiter == codeqa::frontend::IncludeDelimiter::Local);
    CHECK(result.includes[1].range.begin.line == 2);
}

TEST_CASE("extracts file scope globals from C")
{
    const TempSourceFile file{
        "file-scope-globals.c",
        "static int cache = 1;\n"
        "extern const char* title;\n"
        "typedef unsigned long Size;\n"
        "int function(void) { int local = 0; return local; }\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::C, "detected language", parser, extractor);

    REQUIRE(result.globalVariables.size() == 2);
    CHECK(result.globalVariables[0].name == "cache");
    CHECK(result.globalVariables[0].declaredType == "int");
    CHECK(result.globalVariables[0].isStatic);
    CHECK_FALSE(result.globalVariables[0].isExtern);
    CHECK(result.globalVariables[0].hasInitializer);
    CHECK(result.globalVariables[1].name == "title");
    CHECK(result.globalVariables[1].declaredType == "const char*");
    CHECK_FALSE(result.globalVariables[1].isStatic);
    CHECK(result.globalVariables[1].isExtern);
    CHECK_FALSE(result.globalVariables[1].hasInitializer);
    CHECK(result.globalVariables[0].range.begin.line == 1);
    CHECK(result.globalVariables[1].range.begin.line == 2);
}

TEST_CASE("extracts typedef aliases from C and C++ namespace scope")
{
    const TempSourceFile cFile{
        "typedef-alias.c",
        "typedef unsigned long Size;\n"
        "int function(void) { typedef int LocalSize; return 0; }\n"};
    const TempSourceFile cppFile{
        "namespace-globals.cpp",
        "namespace catalog {\n"
        "extern const char* title;\n"
        "typedef unsigned long Size;\n"
        "void function() { int local = 0; (void)local; }\n"
        "class Entry { int member; };\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile cResult = codeqa::frontend::parseSourceFile(
        cFile.path(), codeqa::frontend::Grammar::C, "detected language", parser, extractor);
    const codeqa::frontend::ParsedFile cppResult = codeqa::frontend::parseSourceFile(
        cppFile.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(cResult.typeAliases.size() == 1);
    CHECK(cResult.typeAliases[0].name == "Size");
    CHECK(cResult.typeAliases[0].aliasedType == "unsigned long");
    CHECK(cResult.globalVariables.empty());

    REQUIRE(cppResult.globalVariables.size() == 1);
    CHECK(cppResult.globalVariables[0].name == "title");
    CHECK(cppResult.globalVariables[0].declaredType == "const char*");
    CHECK(cppResult.globalVariables[0].isExtern);
    CHECK_FALSE(cppResult.globalVariables[0].hasInitializer);
    REQUIRE(cppResult.typeAliases.size() == 1);
    CHECK(cppResult.typeAliases[0].name == "Size");
    CHECK(cppResult.typeAliases[0].aliasedType == "unsigned long");
}

TEST_CASE("extracts the base type for every file scope declarator")
{
    const TempSourceFile file{"multiple-globals.c", "int a, b;\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::C, "detected language", parser, extractor);

    REQUIRE(result.globalVariables.size() == 2);
    CHECK(result.globalVariables[0].name == "a");
    CHECK(result.globalVariables[0].declaredType == "int");
    CHECK(result.globalVariables[1].name == "b");
    CHECK(result.globalVariables[1].declaredType == "int");
}

TEST_CASE("extracts the base type for every typedef declarator")
{
    const TempSourceFile file{"multiple-aliases.c", "typedef int A, *B;\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::C, "detected language", parser, extractor);

    REQUIRE(result.typeAliases.size() == 2);
    CHECK(result.typeAliases[0].name == "A");
    CHECK(result.typeAliases[0].aliasedType == "int");
    CHECK(result.typeAliases[1].name == "B");
    CHECK(result.typeAliases[1].aliasedType == "int*");
}

TEST_CASE("extracts function type typedef aliases from C and C++")
{
    const TempSourceFile cFile{"function-type-alias.c", "typedef int Handler(int);\n"};
    const TempSourceFile cppFile{
        "function-type-alias.cpp",
        "namespace ns { struct Arg {}; }\n"
        "typedef int Handler(ns::Arg);\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile cResult = codeqa::frontend::parseSourceFile(
        cFile.path(), codeqa::frontend::Grammar::C, "detected language", parser, extractor);
    const codeqa::frontend::ParsedFile cppResult = codeqa::frontend::parseSourceFile(
        cppFile.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(cResult.typeAliases.size() == 1);
    CHECK(cResult.typeAliases[0].name == "Handler");
    CHECK(cResult.typeAliases[0].aliasedType == "int(int)");
    CHECK(cResult.functions.empty());
    REQUIRE(cppResult.typeAliases.size() == 1);
    CHECK(cppResult.typeAliases[0].name == "Handler");
    CHECK(cppResult.typeAliases[0].aliasedType == "int(ns::Arg)");
    CHECK(cppResult.functions.empty());
}

TEST_CASE("extracts qualified global definitions outside their lexical scopes")
{
    const TempSourceFile file{
        "qualified-globals.cpp",
        "namespace ns { struct Holder { static int count; }; }\n"
        "int ns::value = 0;\n"
        "int ns::Holder::count = 0;\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.globalVariables.size() == 2);
    CHECK(result.globalVariables[0].name == "ns::value");
    CHECK(result.globalVariables[0].declaredType == "int");
    CHECK(result.globalVariables[1].name == "ns::Holder::count");
    CHECK(result.globalVariables[1].declaredType == "int");
}

TEST_CASE("extracts file scope function pointers as globals")
{
    const TempSourceFile file{"function-pointer.c", "int (*callback)(int);\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::C, "detected language", parser, extractor);

    REQUIRE(result.globalVariables.size() == 1);
    CHECK(result.globalVariables[0].name == "callback");
    CHECK(result.globalVariables[0].declaredType == "int(*)(int)");
    CHECK(result.functions.empty());
}

TEST_CASE("extracts functions that return pointers")
{
    const TempSourceFile file{
        "pointer-return-functions.c",
        "int *f(int);\n"
        "int *defined(int value) { return value ? 0 : 0; }\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::C, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 2);
    CHECK(result.functions[0].name == "f");
    CHECK(result.functions[0].kind == codeqa::frontend::FunctionKind::Prototype);
    CHECK(result.functions[1].name == "defined");
    CHECK(result.functions[1].kind == codeqa::frontend::FunctionKind::Free);
    CHECK(result.globalVariables.empty());
}

TEST_CASE("extracts C++ prototypes that return a pointer or reference")
{
    const TempSourceFile file{
        "pointer-reference-return-prototypes.cpp",
        "int& freeRef();\n"
        "\n"
        "class Widget\n"
        "{\n"
        "public:\n"
        "    int* getValue();\n"
        "    const char* name() const;\n"
        "    int& memberRef();\n"
        "};\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 4);
    CHECK(result.functions[0].name == "freeRef");
    CHECK(result.functions[1].name == "getValue");
    CHECK(result.functions[2].name == "name");
    CHECK(result.functions[3].name == "memberRef");
}

TEST_CASE("extracts only function pointers as globals from mixed declarations")
{
    const TempSourceFile file{
        "mixed-function-declarators.c",
        "int (*fp)(int), f(int*);\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::C, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 1);
    CHECK(result.functions[0].name == "f");
    REQUIRE(result.globalVariables.size() == 1);
    CHECK(result.globalVariables[0].name == "fp");
    CHECK(result.globalVariables[0].declaredType == "int(*)(int)");
}

TEST_CASE("captures the parameter list range for functions and prototypes")
{
    const TempSourceFile file{
        "parameter-list-range.cpp",
        "int add(int left, int right);\n"
        "\n"
        "int add(int left, int right)\n"
        "{\n"
        "    return left + right;\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 2);

    for (const codeqa::frontend::FunctionModel& function : result.functions)
    {
        REQUIRE(function.parameterListRange.has_value());
        CHECK(function.parameterListRange->begin.line == function.range.begin.line);
    }
}

TEST_CASE("leaves the parameter list range empty for a parameterless lambda")
{
    const TempSourceFile file{
        "lambda-no-parameters.cpp",
        "void run()\n"
        "{\n"
        "    auto action = []{ return 1; };\n"
        "    action();\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    const auto lambda = std::find_if(
        result.functions.begin(),
        result.functions.end(),
        [](const codeqa::frontend::FunctionModel& function) {
            return function.kind == codeqa::frontend::FunctionKind::Lambda;
        });

    REQUIRE(lambda != result.functions.end());
    CHECK_FALSE(lambda->parameterListRange.has_value());
}

TEST_CASE("collects line and block comments anywhere in the file")
{
    const TempSourceFile file{
        "comments.cpp",
        "// leading comment\n"
        "int add(int left, int right)\n"
        "{\n"
        "    /* block\n"
        "       comment */\n"
        "    return left + right; // trailing comment\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.comments.size() == 3);
    CHECK(result.comments[0].begin.line == 1);
    CHECK(result.comments[1].begin.line == 4);
    CHECK(result.comments[1].end.line == 5);
    CHECK(result.comments[2].begin.line == 6);
}

TEST_CASE("counts statements at any nesting depth, excluding compound blocks")
{
    const TempSourceFile file{
        "statement-count.cpp",
        "int classify(int value)\n"
        "{\n"
        "    int result = 0;\n"
        "\n"
        "    if (value > 0)\n"
        "    {\n"
        "        result = value;\n"
        "    }\n"
        "\n"
        "    return result;\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 1);
    // declaration (int result = 0;), if_statement, expression_statement
    // (result = value;), return_statement -- the compound_statement braces
    // around the if-body do not add an entry.
    CHECK(result.functions[0].statements.size() == 4);
}

TEST_CASE("counts a for-loop once and excludes its header clauses")
{
    const TempSourceFile file{
        "for-loop-statement-count.cpp",
        "int sum(int count)\n"
        "{\n"
        "    int total = 0;\n"
        "\n"
        "    for (int index = 0; index < count; ++index)\n"
        "    {\n"
        "        total += index;\n"
        "    }\n"
        "\n"
        "    return total;\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 1);
    // declaration (int total = 0;), for_statement (once -- its init clause
    // "int index = 0" does not add a second entry), expression_statement
    // (total += index;), return_statement.
    CHECK(result.functions[0].statements.size() == 4);
}
