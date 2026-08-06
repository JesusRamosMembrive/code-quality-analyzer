using TSParser = int;
using TSTree = int;

#include "doctest/doctest.h"

#include "codeqa/frontend/TreeSitterParser.h"

#include <type_traits>

TEST_CASE("the public parser API is independent of tree-sitter type names")
{
    CHECK_FALSE(std::is_copy_constructible_v<codeqa::frontend::SyntaxTree>);
    CHECK(std::is_move_constructible_v<codeqa::frontend::SyntaxTree>);
    CHECK_FALSE(std::is_copy_constructible_v<codeqa::frontend::TreeSitterParser>);
    CHECK(std::is_move_constructible_v<codeqa::frontend::TreeSitterParser>);
}
