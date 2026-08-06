using TSNode = int;
using TSParser = int;
using TSTree = int;

#include "doctest/doctest.h"
#include "codeqa/model/Models.h"
#include "codeqa/model/ProjectModelBuilder.h"

TEST_CASE("the public model API is independent of tree-sitter type names")
{
    codeqa::model::CallableModel callable;
    codeqa::model::ProjectModelBuilder builder;
    CHECK(callable.localVariables.empty());
    CHECK(sizeof(builder) > 0);
}
