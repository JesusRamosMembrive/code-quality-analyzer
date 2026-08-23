#include "doctest/doctest.h"

#include "codeqa/report/FileJson.h"

TEST_CASE("fileJson serializes a file's language, state, diagnostics and structural symbols")
{
    codeqa::model::FileModel file;
    file.path = "src/sample.cpp";
    file.language = codeqa::core::Language::Cpp;
    file.state = codeqa::model::AnalysisState::Partial;

    codeqa::core::Diagnostic diagnostic;
    diagnostic.code = "PARSE001";
    diagnostic.severity = codeqa::core::DiagnosticSeverity::Warning;
    diagnostic.message = "recovered from an error";
    diagnostic.location.line = 4;
    diagnostic.location.column = 2;
    file.diagnostics.push_back(diagnostic);

    codeqa::model::DependencyModel dependency;
    dependency.target = "vector";
    dependency.kind = codeqa::model::IncludeKind::System;
    file.dependencies.push_back(dependency);

    codeqa::model::NamespaceModel namespaceModel;
    namespaceModel.symbol.name = "app";
    namespaceModel.symbol.qualifiedName = "app";
    file.namespaces.push_back(namespaceModel);

    codeqa::model::TypeModel type;
    type.symbol.name = "Widget";
    type.symbol.qualifiedName = "app::Widget";
    type.kind = codeqa::model::TypeKind::Struct;
    codeqa::model::CppTypeDetails details;
    details.isTemplate = true;
    details.baseSpecifiers.push_back("Base");
    type.cppDetails = details;
    file.types.push_back(type);

    codeqa::model::GlobalVariableModel global;
    global.symbol.name = "counter";
    global.symbol.qualifiedName = "app::counter";
    global.declaredType = "int";
    global.isStatic = true;
    file.globalVariables.push_back(global);

    codeqa::model::TypeAliasModel alias;
    alias.symbol.name = "WidgetPtr";
    alias.symbol.qualifiedName = "app::WidgetPtr";
    alias.aliasedType = "Widget*";
    file.typeAliases.push_back(alias);

    file.comments.push_back(codeqa::core::SourceRange{});

    codeqa::model::CallableModel callable;
    callable.symbol.name = "run";
    callable.symbol.qualifiedName = "app::Widget::run";
    callable.kind = codeqa::model::CallableKind::Method;

    codeqa::model::ParameterModel parameter;
    parameter.name = "value";
    parameter.passing = codeqa::model::ParameterPassing::Reference;
    parameter.declaredType = "int&";
    callable.parameters.push_back(parameter);

    codeqa::model::ControlFlowModel controlFlow;
    controlFlow.kind = codeqa::model::ControlFlowKind::If;
    controlFlow.nestingDepth = 1;
    callable.controlFlow.push_back(controlFlow);

    codeqa::model::CallModel call;
    call.calleeText = "helper";
    call.kind = codeqa::model::CallKind::Member;
    call.argumentCount = 1;
    callable.calls.push_back(call);

    file.callables.push_back(callable);

    const nlohmann::json document = codeqa::report::fileJson(file);

    CHECK(document.at("path").get<std::string>() == "src/sample.cpp");
    CHECK(document.at("language").get<std::string>() == "cpp");
    CHECK(document.at("state").get<std::string>() == "partial");

    REQUIRE(document.at("diagnostics").size() == 1);
    CHECK(document.at("diagnostics")[0].at("severity").get<std::string>() == "warning");
    CHECK(document.at("diagnostics")[0].at("location").at("line").get<int>() == 4);

    REQUIRE(document.at("dependencies").size() == 1);
    CHECK(document.at("dependencies")[0].at("kind").get<std::string>() == "system");

    REQUIRE(document.at("namespaces").size() == 1);
    CHECK(document.at("namespaces")[0].at("qualifiedName").get<std::string>() == "app");

    REQUIRE(document.at("types").size() == 1);
    CHECK(document.at("types")[0].at("kind").get<std::string>() == "struct");
    CHECK(document.at("types")[0].at("cppDetails").at("isTemplate").get<bool>());
    CHECK(document.at("types")[0].at("cppDetails").at("baseSpecifiers")[0].get<std::string>() == "Base");

    REQUIRE(document.at("globalVariables").size() == 1);
    CHECK(document.at("globalVariables")[0].at("isStatic").get<bool>());

    REQUIRE(document.at("typeAliases").size() == 1);
    CHECK(document.at("typeAliases")[0].at("aliasedType").get<std::string>() == "Widget*");

    REQUIRE(document.at("comments").size() == 1);

    REQUIRE(document.at("callables").size() == 1);
    const nlohmann::json& callableJson = document.at("callables")[0];
    CHECK(callableJson.at("kind").get<std::string>() == "method");
    REQUIRE(callableJson.at("parameters").size() == 1);
    CHECK(callableJson.at("parameters")[0].at("passing").get<std::string>() == "reference");
    REQUIRE(callableJson.at("controlFlow").size() == 1);
    CHECK(callableJson.at("controlFlow")[0].at("kind").get<std::string>() == "if");
    REQUIRE(callableJson.at("calls").size() == 1);
    CHECK(callableJson.at("calls")[0].at("kind").get<std::string>() == "member");
}

TEST_CASE("fileJson omits cppDetails when a type has none")
{
    codeqa::model::FileModel file;
    file.path = "src/plain.c";
    file.language = codeqa::core::Language::C;

    codeqa::model::TypeModel type;
    type.symbol.name = "Point";
    type.symbol.qualifiedName = "Point";
    type.kind = codeqa::model::TypeKind::Struct;
    file.types.push_back(type);

    const nlohmann::json document = codeqa::report::fileJson(file);

    CHECK(document.at("language").get<std::string>() == "c");
    CHECK_FALSE(document.at("types")[0].contains("cppDetails"));
}

TEST_CASE("fileJson serializes Array parameter passing as \"array\"")
{
    codeqa::model::FileModel file;
    file.path = "src/sample.cpp";
    file.language = codeqa::core::Language::Cpp;

    codeqa::model::CallableModel callable;
    callable.symbol.name = "classify";
    callable.symbol.qualifiedName = "classify";

    codeqa::model::ParameterModel parameter;
    parameter.name = "items";
    parameter.passing = codeqa::model::ParameterPassing::Array;
    parameter.declaredType = "int";
    callable.parameters.push_back(parameter);

    file.callables.push_back(callable);

    const nlohmann::json document = codeqa::report::fileJson(file);

    REQUIRE(document.at("callables").size() == 1);
    const nlohmann::json& callableJson = document.at("callables")[0];
    REQUIRE(callableJson.at("parameters").size() == 1);
    CHECK(callableJson.at("parameters")[0].at("passing").get<std::string>() == "array");
}

TEST_CASE("fileJson serializes a macro with its parameters and body")
{
    codeqa::model::FileModel file;
    file.path = "src/macros.h";
    file.language = codeqa::core::Language::C;

    codeqa::model::MacroModel macro;
    macro.symbol.name = "SQUARE";
    macro.symbol.qualifiedName = "SQUARE";
    macro.isFunctionLike = true;
    macro.parameters = {"x"};
    macro.isVariadic = false;
    macro.body = "x*x";
    file.macros.push_back(macro);

    const nlohmann::json document = codeqa::report::fileJson(file);

    REQUIRE(document.at("macros").size() == 1);
    const nlohmann::json& macroJson = document.at("macros")[0];
    CHECK(macroJson.at("qualifiedName").get<std::string>() == "SQUARE");
    CHECK(macroJson.at("isFunctionLike").get<bool>());
    REQUIRE(macroJson.at("parameters").size() == 1);
    CHECK(macroJson.at("parameters")[0].get<std::string>() == "x");
    CHECK_FALSE(macroJson.at("isVariadic").get<bool>());
    CHECK(macroJson.at("body").get<std::string>() == "x*x");
}
