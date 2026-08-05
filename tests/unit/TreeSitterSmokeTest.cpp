#include "doctest/doctest.h"

#include <tree_sitter/api.h>

#include <cstring>
#include <string>

extern "C" const TSLanguage* tree_sitter_c(void);
extern "C" const TSLanguage* tree_sitter_cpp(void);

namespace
{

std::string parseAndGetRootType(const TSLanguage* language, const std::string& source)
{
    TSParser* parser = ts_parser_new();
    REQUIRE(parser != nullptr);
    REQUIRE(ts_parser_set_language(parser, language));

    TSTree* tree = ts_parser_parse_string(
        parser, nullptr, source.c_str(), static_cast<uint32_t>(source.size()));
    REQUIRE(tree != nullptr);

    const TSNode root = ts_tree_root_node(tree);
    const std::string rootType = ts_node_type(root);
    const bool hasError = ts_node_has_error(root);

    ts_tree_delete(tree);
    ts_parser_delete(parser);

    CHECK_FALSE(hasError);
    return rootType;
}

} // namespace

TEST_CASE("the vendored C grammar parses a trivial translation unit")
{
    CHECK(parseAndGetRootType(tree_sitter_c(), "int main(void) { return 0; }")
        == "translation_unit");
}

TEST_CASE("the vendored C++ grammar parses a trivial translation unit")
{
    CHECK(parseAndGetRootType(tree_sitter_cpp(), "int main() { return 0; }")
        == "translation_unit");
}

TEST_CASE("the vendored C++ grammar parses a class with a method")
{
    CHECK(parseAndGetRootType(tree_sitter_cpp(), "class Foo { public: int bar() { return 1; } };")
        == "translation_unit");
}
