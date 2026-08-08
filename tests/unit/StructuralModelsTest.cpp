#include "doctest/doctest.h"

#include "codeqa/frontend/StructuralModels.h"

TEST_CASE("structural models retain the attributes required by the corpus")
{
    codeqa::frontend::ParameterModel parameter{
        "callback",
        codeqa::frontend::ParameterPassing::FunctionPointer,
        true};
    codeqa::frontend::FunctionModel function;
    function.name = "run";
    function.kind = codeqa::frontend::FunctionKind::Free;
    function.parameters = {parameter};
    function.isStatic = true;
    function.isVariadic = true;

    CHECK(function.parameters.front().name == "callback");
    CHECK(function.parameters.front().passing
        == codeqa::frontend::ParameterPassing::FunctionPointer);
    CHECK(function.parameters.front().hasDefaultValue);
    CHECK(function.parameters.front().declaredType.empty());
    CHECK(function.isStatic);
    CHECK(function.isVariadic);

    function.localVariables.push_back(codeqa::frontend::LocalVariableModel{
        "index", "int", {}, true, 1});
    function.controlFlow.push_back(codeqa::frontend::ControlFlowModel{
        codeqa::frontend::ControlFlowKind::For, {}, 0, {}});
    function.calls.push_back(codeqa::frontend::CallModel{
        "callback", codeqa::frontend::CallKind::Named, 1, {}});

    CHECK(function.localVariables[0].name == "index");
    CHECK(function.localVariables[0].declaredType == "int");
    CHECK(function.localVariables[0].hasInitializer);
    CHECK(function.localVariables[0].lexicalDepth == 1);
    CHECK_FALSE(function.controlFlow[0].conditionRange.has_value());
    CHECK(function.calls[0].kind == codeqa::frontend::CallKind::Named);
    CHECK_FALSE(function.parameterListRange.has_value());
    CHECK(function.statements.empty());

    codeqa::frontend::CallModel defaultCall;
    CHECK(defaultCall.kind == codeqa::frontend::CallKind::Named);
}

TEST_CASE("class and include models default to typed, non-generic values")
{
    codeqa::frontend::ClassModel type;
    codeqa::frontend::IncludeDependency include;

    CHECK(type.kind == codeqa::frontend::ClassKind::Class);
    CHECK_FALSE(type.isTemplate);
    CHECK_FALSE(type.isScopedEnum);
    CHECK(include.delimiter == codeqa::frontend::IncludeDelimiter::Local);
}
