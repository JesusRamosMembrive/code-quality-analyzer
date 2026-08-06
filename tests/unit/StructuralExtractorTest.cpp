#include "doctest/doctest.h"

#include "codeqa/frontend/ParseSourceFile.h"
#include "codeqa/frontend/StructuralExtractor.h"

#include <filesystem>
#include <fstream>
#include <string>

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
