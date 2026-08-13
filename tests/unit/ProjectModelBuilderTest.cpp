#include "doctest/doctest.h"

#include "codeqa/model/ProjectModelBuilder.h"

#include <filesystem>
#include <string>
#include <vector>

namespace
{

codeqa::core::SourceRange rangeFor(
    const std::filesystem::path& path,
    std::size_t beginOffset,
    std::size_t endOffset)
{
    return codeqa::core::SourceRange{
        codeqa::core::SourceLocation{path, 1, beginOffset + 1, beginOffset},
        codeqa::core::SourceLocation{path, 1, endOffset + 1, endOffset}};
}

} // namespace

TEST_CASE("copies declared parameter types, parameter list ranges, comments and statements")
{
    const std::filesystem::path rootPath{"project"};
    const std::filesystem::path sourcePath{"project/src/greet.cpp"};

    codeqa::frontend::ParsedFile sourceFile;
    sourceFile.path = sourcePath;
    sourceFile.language = codeqa::core::Language::Cpp;
    sourceFile.grammar = codeqa::frontend::Grammar::Cpp;
    sourceFile.parsed = true;
    sourceFile.comments = {rangeFor(sourcePath, 0, 10)};
    sourceFile.functions.push_back(codeqa::frontend::FunctionModel{
        "greet",
        codeqa::frontend::FunctionKind::Free,
        rangeFor(sourcePath, 20, 60),
        {codeqa::frontend::ParameterModel{
            "loud", codeqa::frontend::ParameterPassing::Value, false, "bool"}},
        false,
        false});
    sourceFile.functions[0].parameterListRange = rangeFor(sourcePath, 25, 35);
    sourceFile.functions[0].statements = {rangeFor(sourcePath, 40, 50), rangeFor(sourcePath, 50, 59)};

    const codeqa::model::ProjectModel result = codeqa::model::ProjectModelBuilder{}.build(
        rootPath, std::vector<codeqa::frontend::ParsedFile>{sourceFile});

    REQUIRE(result.files.size() == 1);
    REQUIRE(result.files[0].comments.size() == 1);
    CHECK(result.files[0].comments[0].begin.offset == 0);
    CHECK(result.files[0].comments[0].end.offset == 10);
    REQUIRE(result.files[0].callables.size() == 1);
    const codeqa::model::CallableModel& callable = result.files[0].callables[0];
    REQUIRE(callable.parameters.size() == 1);
    CHECK(callable.parameters[0].declaredType == "bool");
    REQUIRE(callable.parameterListRange.has_value());
    CHECK(callable.parameterListRange->begin.offset == 25);
    CHECK(callable.parameterListRange->end.offset == 35);
    REQUIRE(callable.statements.size() == 2);
    CHECK(callable.statements[0].begin.offset == 40);
    CHECK(callable.statements[0].end.offset == 50);
    CHECK(callable.statements[1].begin.offset == 50);
    CHECK(callable.statements[1].end.offset == 59);
}

TEST_CASE("builds deterministic common project model from parsed frontend files")
{
    const std::filesystem::path rootPath{"project"};
    const std::filesystem::path sourcePath{"project/src/zeta.cpp"};
    const std::filesystem::path unavailablePath{"project/src/alpha.cpp"};

    codeqa::frontend::ParsedFile sourceFile;
    sourceFile.path = sourcePath;
    sourceFile.language = codeqa::core::Language::Cpp;
    sourceFile.grammar = codeqa::frontend::Grammar::Cpp;
    sourceFile.parsed = true;
    sourceFile.diagnostics.push_back(codeqa::core::Diagnostic{
        "PARSE001",
        codeqa::core::DiagnosticSeverity::Warning,
        "recoverable parse issue",
        codeqa::core::SourceLocation{sourcePath, 8, 3, 88}});
    sourceFile.includes.push_back(codeqa::frontend::IncludeDependency{
        "widget.h", codeqa::frontend::IncludeDelimiter::Local, rangeFor(sourcePath, 1, 20)});
    sourceFile.classes.push_back(codeqa::frontend::ClassModel{
        "outer", codeqa::frontend::ClassKind::Namespace, rangeFor(sourcePath, 30, 200)});
    sourceFile.classes.push_back(codeqa::frontend::ClassModel{
        "Box", codeqa::frontend::ClassKind::Class, rangeFor(sourcePath, 60, 180), true});
    sourceFile.functions.push_back(codeqa::frontend::FunctionModel{
        "value",
        codeqa::frontend::FunctionKind::Method,
        rangeFor(sourcePath, 90, 120),
        {codeqa::frontend::ParameterModel{
            "target", codeqa::frontend::ParameterPassing::Reference, true}},
        true,
        false});
    sourceFile.functions[0].localVariables = {
        {"late", "int", rangeFor(sourcePath, 90, 91), false, 1},
        {"first", "int", rangeFor(sourcePath, 10, 11), true, 0},
        {"second", "int", rangeFor(sourcePath, 10, 11), false, 0}};
    sourceFile.functions[0].controlFlow = {
        {codeqa::frontend::ControlFlowKind::Return, rangeFor(sourcePath, 90, 91), 1, {}},
        {codeqa::frontend::ControlFlowKind::If,
         rangeFor(sourcePath, 10, 11),
         0,
         rangeFor(sourcePath, 12, 13)},
        {codeqa::frontend::ControlFlowKind::For, rangeFor(sourcePath, 10, 11), 0, {}}};
    sourceFile.functions[0].calls = {
        {"lateCall", codeqa::frontend::CallKind::Named, 0, rangeFor(sourcePath, 90, 91)},
        {"firstCall", codeqa::frontend::CallKind::Member, 2, rangeFor(sourcePath, 10, 11)},
        {"secondCall", codeqa::frontend::CallKind::Indirect, 1, rangeFor(sourcePath, 10, 11)}};
    sourceFile.globalVariables.push_back(codeqa::frontend::GlobalVariableModel{
        "count", "int", rangeFor(sourcePath, 35, 45), true, false, true});
    sourceFile.typeAliases.push_back(codeqa::frontend::TypeAliasModel{
        "Size", "unsigned long", rangeFor(sourcePath, 46, 55)});

    codeqa::frontend::ParsedFile unavailableFile;
    unavailableFile.path = unavailablePath;
    unavailableFile.language = codeqa::core::Language::C;
    unavailableFile.grammar = codeqa::frontend::Grammar::C;
    unavailableFile.parsed = false;
    unavailableFile.diagnostics.push_back(codeqa::core::Diagnostic{
        "PARSE003",
        codeqa::core::DiagnosticSeverity::Error,
        "could not read source file",
        codeqa::core::SourceLocation{unavailablePath, 1, 1, 0}});
    unavailableFile.functions.push_back(codeqa::frontend::FunctionModel{
        "mustNotAppear", codeqa::frontend::FunctionKind::Free, rangeFor(unavailablePath, 1, 5)});

    const codeqa::model::ProjectModel result = codeqa::model::ProjectModelBuilder{}.build(
        rootPath, std::vector<codeqa::frontend::ParsedFile>{sourceFile, unavailableFile});

    REQUIRE(result.rootPath == rootPath);
    REQUIRE(result.files.size() == 2);
    CHECK(result.files[0].path == unavailablePath);
    CHECK(result.files[0].language == codeqa::core::Language::C);
    CHECK(result.files[0].state == codeqa::model::AnalysisState::Unavailable);
    REQUIRE(result.files[0].diagnostics.size() == 1);
    CHECK(result.files[0].diagnostics[0].code == "PARSE003");
    CHECK(result.files[0].diagnostics[0].message == "could not read source file");
    CHECK(result.files[0].diagnostics[0].location.filePath == unavailablePath);
    CHECK(result.files[0].callables.empty());
    CHECK(result.files[0].types.empty());
    CHECK(result.files[0].namespaces.empty());
    CHECK(result.files[0].dependencies.empty());
    CHECK(result.files[0].globalVariables.empty());
    CHECK(result.files[0].typeAliases.empty());

    const codeqa::model::FileModel& modelFile = result.files[1];
    CHECK(modelFile.path == sourcePath);
    CHECK(modelFile.language == codeqa::core::Language::Cpp);
    CHECK(modelFile.state == codeqa::model::AnalysisState::Partial);
    REQUIRE(modelFile.diagnostics.size() == 1);
    CHECK(modelFile.diagnostics[0].code == "PARSE001");
    CHECK(modelFile.diagnostics[0].message == "recoverable parse issue");
    CHECK(modelFile.diagnostics[0].location.line == 8);
    REQUIRE(modelFile.dependencies.size() == 1);
    CHECK(modelFile.dependencies[0].target == "widget.h");
    CHECK(modelFile.dependencies[0].kind == codeqa::model::IncludeKind::Local);
    REQUIRE(modelFile.namespaces.size() == 1);
    CHECK(modelFile.namespaces[0].symbol.name == "outer");
    REQUIRE(modelFile.types.size() == 1);
    CHECK(modelFile.types[0].symbol.name == "Box");
    CHECK(modelFile.types[0].symbol.qualifiedName == "outer::Box");
    CHECK(modelFile.types[0].kind == codeqa::model::TypeKind::Class);
    REQUIRE(modelFile.types[0].cppDetails.has_value());
    CHECK(modelFile.types[0].cppDetails->isTemplate);
    REQUIRE(modelFile.callables.size() == 1);
    CHECK(modelFile.callables[0].symbol.name == "value");
    CHECK(modelFile.callables[0].symbol.qualifiedName == "outer::Box::value");
    CHECK(modelFile.callables[0].kind == codeqa::model::CallableKind::Method);
    REQUIRE(modelFile.callables[0].parameters.size() == 1);
    CHECK(modelFile.callables[0].parameters[0].passing == codeqa::model::ParameterPassing::Reference);
    CHECK(modelFile.callables[0].parameters[0].hasDefaultValue);
    CHECK(modelFile.callables[0].isStatic);
    REQUIRE(modelFile.callables[0].localVariables.size() == 3);
    CHECK(modelFile.callables[0].localVariables[0].symbol.name == "first");
    CHECK(modelFile.callables[0].localVariables[0].symbol.qualifiedName == "first");
    CHECK(modelFile.callables[0].localVariables[0].symbol.range.begin.offset == 10);
    CHECK(modelFile.callables[0].localVariables[0].symbol.range.end.offset == 11);
    CHECK(modelFile.callables[0].localVariables[0].hasInitializer);
    CHECK(modelFile.callables[0].localVariables[1].symbol.name == "second");
    CHECK(modelFile.callables[0].localVariables[2].symbol.name == "late");
    CHECK(modelFile.callables[0].localVariables[2].symbol.qualifiedName == "outer::Box::late");
    REQUIRE(modelFile.callables[0].controlFlow.size() == 3);
    CHECK(modelFile.callables[0].controlFlow[0].kind == codeqa::model::ControlFlowKind::If);
    REQUIRE(modelFile.callables[0].controlFlow[0].conditionRange.has_value());
    CHECK(modelFile.callables[0].controlFlow[0].conditionRange->begin.offset == 12);
    CHECK(modelFile.callables[0].controlFlow[0].conditionRange->end.offset == 13);
    CHECK(modelFile.callables[0].controlFlow[1].kind == codeqa::model::ControlFlowKind::For);
    CHECK_FALSE(modelFile.callables[0].controlFlow[1].conditionRange.has_value());
    CHECK(modelFile.callables[0].controlFlow[2].kind == codeqa::model::ControlFlowKind::Return);
    REQUIRE(modelFile.callables[0].calls.size() == 3);
    CHECK(modelFile.callables[0].calls[0].calleeText == "firstCall");
    CHECK(modelFile.callables[0].calls[0].kind == codeqa::model::CallKind::Member);
    CHECK(modelFile.callables[0].calls[1].calleeText == "secondCall");
    CHECK(modelFile.callables[0].calls[1].kind == codeqa::model::CallKind::Indirect);
    CHECK(modelFile.callables[0].calls[2].calleeText == "lateCall");
    CHECK(modelFile.callables[0].calls[2].kind == codeqa::model::CallKind::Named);
    REQUIRE(modelFile.globalVariables.size() == 1);
    CHECK(modelFile.globalVariables[0].symbol.qualifiedName == "outer::count");
    CHECK(modelFile.globalVariables[0].declaredType == "int");
    CHECK(modelFile.globalVariables[0].isStatic);
    CHECK(modelFile.globalVariables[0].hasInitializer);
    REQUIRE(modelFile.typeAliases.size() == 1);
    CHECK(modelFile.typeAliases[0].symbol.qualifiedName == "outer::Size");
    CHECK(modelFile.typeAliases[0].aliasedType == "unsigned long");
}

TEST_CASE("preserves qualified callable names and ignores anonymous namespaces")
{
    const std::filesystem::path sourcePath{"project/source.cpp"};

    codeqa::frontend::ParsedFile sourceFile;
    sourceFile.path = sourcePath;
    sourceFile.parsed = true;
    sourceFile.classes = {
        codeqa::frontend::ClassModel{
            "outer", codeqa::frontend::ClassKind::Namespace, rangeFor(sourcePath, 1, 100)},
        codeqa::frontend::ClassModel{
            "", codeqa::frontend::ClassKind::Namespace, rangeFor(sourcePath, 10, 90)},
        codeqa::frontend::ClassModel{
            "Box", codeqa::frontend::ClassKind::Class, rangeFor(sourcePath, 20, 80)}};
    sourceFile.functions = {
        codeqa::frontend::FunctionModel{
            "outer::Box::declaredElsewhere",
            codeqa::frontend::FunctionKind::Method,
            rangeFor(sourcePath, 30, 40)},
        codeqa::frontend::FunctionModel{
            "value", codeqa::frontend::FunctionKind::Method, rangeFor(sourcePath, 41, 50)}};

    const codeqa::model::ProjectModel result = codeqa::model::ProjectModelBuilder{}.build(
        "project", std::vector<codeqa::frontend::ParsedFile>{sourceFile});

    REQUIRE(result.files.size() == 1);
    CHECK(result.files[0].state == codeqa::model::AnalysisState::Complete);
    REQUIRE(result.files[0].namespaces.size() == 2);
    CHECK(result.files[0].namespaces[1].symbol.name.empty());
    CHECK(result.files[0].namespaces[1].symbol.qualifiedName.empty());
    REQUIRE(result.files[0].callables.size() == 2);
    CHECK(result.files[0].callables[0].symbol.qualifiedName == "outer::Box::declaredElsewhere");
    CHECK(result.files[0].callables[1].symbol.qualifiedName == "outer::Box::value");
}

TEST_CASE("preserves qualified global identities outside lexical scope ranges")
{
    const std::filesystem::path sourcePath{"project/source.cpp"};

    codeqa::frontend::ParsedFile sourceFile;
    sourceFile.path = sourcePath;
    sourceFile.parsed = true;
    sourceFile.classes = {
        codeqa::frontend::ClassModel{
            "ns", codeqa::frontend::ClassKind::Namespace, rangeFor(sourcePath, 1, 20)},
        codeqa::frontend::ClassModel{
            "Holder", codeqa::frontend::ClassKind::Struct, rangeFor(sourcePath, 5, 15)}};
    sourceFile.globalVariables = {
        codeqa::frontend::GlobalVariableModel{
            "ns::value", "int", rangeFor(sourcePath, 30, 40)},
        codeqa::frontend::GlobalVariableModel{
            "ns::Holder::count", "int", rangeFor(sourcePath, 41, 55)}};

    const codeqa::model::ProjectModel result = codeqa::model::ProjectModelBuilder{}.build(
        "project", std::vector<codeqa::frontend::ParsedFile>{sourceFile});

    REQUIRE(result.files.size() == 1);
    REQUIRE(result.files[0].globalVariables.size() == 2);
    CHECK(result.files[0].globalVariables[0].symbol.name == "ns::value");
    CHECK(result.files[0].globalVariables[0].symbol.qualifiedName == "ns::value");
    CHECK(result.files[0].globalVariables[1].symbol.name == "ns::Holder::count");
    CHECK(result.files[0].globalVariables[1].symbol.qualifiedName == "ns::Holder::count");
}

TEST_CASE("merges an out-of-line qualified name with an enclosing namespace it omits")
{
    const std::filesystem::path sourcePath{"project/source.cpp"};

    codeqa::frontend::ParsedFile sourceFile;
    sourceFile.path = sourcePath;
    sourceFile.parsed = true;
    sourceFile.classes = {
        codeqa::frontend::ClassModel{
            "app", codeqa::frontend::ClassKind::Namespace, rangeFor(sourcePath, 1, 200)},
        codeqa::frontend::ClassModel{
            "Engine", codeqa::frontend::ClassKind::Class, rangeFor(sourcePath, 20, 80)}};
    sourceFile.functions = {
        // Prototype, inside the class body: enclosed by both `app` and `Engine`.
        codeqa::frontend::FunctionModel{
            "run", codeqa::frontend::FunctionKind::Method, rangeFor(sourcePath, 30, 40)},
        // Out-of-line definition, still inside `namespace app { ... }` but physically
        // outside the class body, so its source text only spells out "Engine::run".
        codeqa::frontend::FunctionModel{
            "Engine::run", codeqa::frontend::FunctionKind::Method, rangeFor(sourcePath, 100, 150)}};

    const codeqa::model::ProjectModel result = codeqa::model::ProjectModelBuilder{}.build(
        "project", std::vector<codeqa::frontend::ParsedFile>{sourceFile});

    REQUIRE(result.files.size() == 1);
    REQUIRE(result.files[0].callables.size() == 2);
    CHECK(result.files[0].callables[0].symbol.qualifiedName == "app::Engine::run");
    CHECK(result.files[0].callables[1].symbol.qualifiedName == "app::Engine::run");
}

TEST_CASE("sorts every model collection by source range with stable ties")
{
    const std::filesystem::path sourcePath{"project/source.cpp"};

    codeqa::frontend::ParsedFile sourceFile;
    sourceFile.path = sourcePath;
    sourceFile.parsed = true;
    sourceFile.includes = {
        codeqa::frontend::IncludeDependency{
            "late.h", codeqa::frontend::IncludeDelimiter::Local, rangeFor(sourcePath, 90, 91)},
        codeqa::frontend::IncludeDependency{
            "first.h", codeqa::frontend::IncludeDelimiter::Local, rangeFor(sourcePath, 10, 11)},
        codeqa::frontend::IncludeDependency{
            "second.h", codeqa::frontend::IncludeDelimiter::Local, rangeFor(sourcePath, 10, 11)}};
    sourceFile.functions = {
        codeqa::frontend::FunctionModel{
            "lateFunction", codeqa::frontend::FunctionKind::Free, rangeFor(sourcePath, 90, 91)},
        codeqa::frontend::FunctionModel{
            "firstFunction", codeqa::frontend::FunctionKind::Free, rangeFor(sourcePath, 10, 11)},
        codeqa::frontend::FunctionModel{
            "secondFunction", codeqa::frontend::FunctionKind::Free, rangeFor(sourcePath, 10, 11)}};
    sourceFile.classes = {
        codeqa::frontend::ClassModel{
            "lateType", codeqa::frontend::ClassKind::Class, rangeFor(sourcePath, 90, 91)},
        codeqa::frontend::ClassModel{
            "firstType", codeqa::frontend::ClassKind::Struct, rangeFor(sourcePath, 10, 11)},
        codeqa::frontend::ClassModel{
            "secondType", codeqa::frontend::ClassKind::Union, rangeFor(sourcePath, 10, 11)},
        codeqa::frontend::ClassModel{
            "lateNamespace", codeqa::frontend::ClassKind::Namespace, rangeFor(sourcePath, 80, 81)},
        codeqa::frontend::ClassModel{
            "firstNamespace", codeqa::frontend::ClassKind::Namespace, rangeFor(sourcePath, 20, 21)},
        codeqa::frontend::ClassModel{
            "secondNamespace", codeqa::frontend::ClassKind::Namespace, rangeFor(sourcePath, 20, 21)}};
    sourceFile.globalVariables = {
        codeqa::frontend::GlobalVariableModel{
            "lateVariable", "int", rangeFor(sourcePath, 90, 91)},
        codeqa::frontend::GlobalVariableModel{
            "firstVariable", "int", rangeFor(sourcePath, 10, 11)},
        codeqa::frontend::GlobalVariableModel{
            "secondVariable", "int", rangeFor(sourcePath, 10, 11)}};
    sourceFile.typeAliases = {
        codeqa::frontend::TypeAliasModel{"LateAlias", "int", rangeFor(sourcePath, 90, 91)},
        codeqa::frontend::TypeAliasModel{"FirstAlias", "int", rangeFor(sourcePath, 10, 11)},
        codeqa::frontend::TypeAliasModel{"SecondAlias", "int", rangeFor(sourcePath, 10, 11)}};

    const codeqa::model::ProjectModel result = codeqa::model::ProjectModelBuilder{}.build(
        "project", std::vector<codeqa::frontend::ParsedFile>{sourceFile});

    REQUIRE(result.files.size() == 1);
    const codeqa::model::FileModel& file = result.files[0];
    REQUIRE(file.dependencies.size() == 3);
    CHECK(file.dependencies[0].target == "first.h");
    CHECK(file.dependencies[1].target == "second.h");
    CHECK(file.dependencies[2].target == "late.h");
    REQUIRE(file.callables.size() == 3);
    CHECK(file.callables[0].symbol.name == "firstFunction");
    CHECK(file.callables[1].symbol.name == "secondFunction");
    CHECK(file.callables[2].symbol.name == "lateFunction");
    REQUIRE(file.types.size() == 3);
    CHECK(file.types[0].symbol.name == "firstType");
    CHECK(file.types[1].symbol.name == "secondType");
    CHECK(file.types[2].symbol.name == "lateType");
    REQUIRE(file.namespaces.size() == 3);
    CHECK(file.namespaces[0].symbol.name == "firstNamespace");
    CHECK(file.namespaces[1].symbol.name == "secondNamespace");
    CHECK(file.namespaces[2].symbol.name == "lateNamespace");
    REQUIRE(file.globalVariables.size() == 3);
    CHECK(file.globalVariables[0].symbol.name == "firstVariable");
    CHECK(file.globalVariables[1].symbol.name == "secondVariable");
    CHECK(file.globalVariables[2].symbol.name == "lateVariable");
    REQUIRE(file.typeAliases.size() == 3);
    CHECK(file.typeAliases[0].symbol.name == "FirstAlias");
    CHECK(file.typeAliases[1].symbol.name == "SecondAlias");
    CHECK(file.typeAliases[2].symbol.name == "LateAlias");
}

TEST_CASE("a Free-kind function is never class-qualified, even when a corrupted class range geometrically contains it")
{
    const std::filesystem::path rootPath{"project"};
    const std::filesystem::path sourcePath{"project/src/format.h"};

    codeqa::frontend::ParsedFile sourceFile;
    sourceFile.path = sourcePath;
    sourceFile.language = codeqa::core::Language::Cpp;
    sourceFile.grammar = codeqa::frontend::Grammar::Cpp;
    sourceFile.parsed = true;

    // Simulates the fmt write_int_data bug: a struct whose extracted range is corrupted large
    // enough to geometrically swallow a later, unrelated free function's start.
    sourceFile.classes.push_back(codeqa::frontend::ClassModel{
        "write_int_data", codeqa::frontend::ClassKind::Struct, rangeFor(sourcePath, 0, 5000)});

    sourceFile.functions.push_back(codeqa::frontend::FunctionModel{
        "write_significand",
        codeqa::frontend::FunctionKind::Free,
        rangeFor(sourcePath, 2000, 2100),
        {},
        false,
        false});

    const codeqa::model::ProjectModel result = codeqa::model::ProjectModelBuilder{}.build(
        rootPath, std::vector<codeqa::frontend::ParsedFile>{sourceFile});

    REQUIRE(result.files.size() == 1);
    REQUIRE(result.files[0].callables.size() == 1);
    CHECK(result.files[0].callables[0].symbol.qualifiedName == "write_significand");
}

TEST_CASE("a Free-kind function nested in a namespace is still namespace-qualified")
{
    const std::filesystem::path rootPath{"project"};
    const std::filesystem::path sourcePath{"project/src/detail.cpp"};

    codeqa::frontend::ParsedFile sourceFile;
    sourceFile.path = sourcePath;
    sourceFile.language = codeqa::core::Language::Cpp;
    sourceFile.grammar = codeqa::frontend::Grammar::Cpp;
    sourceFile.parsed = true;

    sourceFile.classes.push_back(codeqa::frontend::ClassModel{
        "detail", codeqa::frontend::ClassKind::Namespace, rangeFor(sourcePath, 0, 200)});

    sourceFile.functions.push_back(codeqa::frontend::FunctionModel{
        "helper",
        codeqa::frontend::FunctionKind::Free,
        rangeFor(sourcePath, 20, 60),
        {},
        false,
        false});

    const codeqa::model::ProjectModel result = codeqa::model::ProjectModelBuilder{}.build(
        rootPath, std::vector<codeqa::frontend::ParsedFile>{sourceFile});

    REQUIRE(result.files.size() == 1);
    REQUIRE(result.files[0].callables.size() == 1);
    CHECK(result.files[0].callables[0].symbol.qualifiedName == "detail::helper");
}

TEST_CASE("a Method-kind function is still class-qualified")
{
    const std::filesystem::path rootPath{"project"};
    const std::filesystem::path sourcePath{"project/src/box.cpp"};

    codeqa::frontend::ParsedFile sourceFile;
    sourceFile.path = sourcePath;
    sourceFile.language = codeqa::core::Language::Cpp;
    sourceFile.grammar = codeqa::frontend::Grammar::Cpp;
    sourceFile.parsed = true;

    sourceFile.classes.push_back(codeqa::frontend::ClassModel{
        "Box", codeqa::frontend::ClassKind::Class, rangeFor(sourcePath, 0, 200)});

    sourceFile.functions.push_back(codeqa::frontend::FunctionModel{
        "open",
        codeqa::frontend::FunctionKind::Method,
        rangeFor(sourcePath, 20, 60),
        {},
        false,
        false});

    const codeqa::model::ProjectModel result = codeqa::model::ProjectModelBuilder{}.build(
        rootPath, std::vector<codeqa::frontend::ParsedFile>{sourceFile});

    REQUIRE(result.files.size() == 1);
    REQUIRE(result.files[0].callables.size() == 1);
    CHECK(result.files[0].callables[0].symbol.qualifiedName == "Box::open");
}
