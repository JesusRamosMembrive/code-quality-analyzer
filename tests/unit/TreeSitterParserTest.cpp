#include "doctest/doctest.h"

#include "codeqa/frontend/TreeSitterParser.h"

#include <optional>
#include <string>

TEST_CASE("the parser produces a tree for valid C")
{
    codeqa::frontend::TreeSitterParser parser;
    std::optional<codeqa::frontend::SyntaxTree> tree =
        parser.parse(codeqa::frontend::Grammar::C, "int main(void) { return 0; }");

    REQUIRE(tree.has_value());
    CHECK(std::string{tree->rootNodeType()} == "translation_unit");
    CHECK_FALSE(tree->rootHasError());
}

TEST_CASE("the parser produces a tree for valid C++")
{
    codeqa::frontend::TreeSitterParser parser;
    std::optional<codeqa::frontend::SyntaxTree> tree =
        parser.parse(codeqa::frontend::Grammar::Cpp, "class Foo { int bar; };");

    REQUIRE(tree.has_value());
    CHECK(std::string{tree->rootNodeType()} == "translation_unit");
    CHECK_FALSE(tree->rootHasError());
}

TEST_CASE("the parser reports an error tree for broken source")
{
    codeqa::frontend::TreeSitterParser parser;
    std::optional<codeqa::frontend::SyntaxTree> tree =
        parser.parse(codeqa::frontend::Grammar::Cpp, "int broken( {");

    REQUIRE(tree.has_value());
    CHECK(tree->rootHasError());
}

TEST_CASE("an empty source still yields a tree")
{
    codeqa::frontend::TreeSitterParser parser;
    std::optional<codeqa::frontend::SyntaxTree> tree =
        parser.parse(codeqa::frontend::Grammar::C, "");

    REQUIRE(tree.has_value());
    CHECK(std::string{tree->rootNodeType()} == "translation_unit");
}

TEST_CASE("a parser can be reused for several files")
{
    codeqa::frontend::TreeSitterParser parser;

    const std::optional<codeqa::frontend::SyntaxTree> first =
        parser.parse(codeqa::frontend::Grammar::C, "int a(void) { return 1; }");
    const std::optional<codeqa::frontend::SyntaxTree> second =
        parser.parse(codeqa::frontend::Grammar::Cpp, "int b() { return 2; }");

    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    CHECK_FALSE(first->rootHasError());
    CHECK_FALSE(second->rootHasError());
}

TEST_CASE("a syntax tree can be moved without double-freeing")
{
    codeqa::frontend::TreeSitterParser parser;
    std::optional<codeqa::frontend::SyntaxTree> original =
        parser.parse(codeqa::frontend::Grammar::C, "int a(void) { return 1; }");

    REQUIRE(original.has_value());

    const codeqa::frontend::SyntaxTree moved = std::move(*original);

    CHECK(std::string{moved.rootNodeType()} == "translation_unit");
}
