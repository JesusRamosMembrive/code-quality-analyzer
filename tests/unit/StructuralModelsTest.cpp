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
    CHECK(function.isStatic);
    CHECK(function.isVariadic);
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
