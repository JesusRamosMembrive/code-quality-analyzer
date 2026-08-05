#include "codeqa/frontend/TreeSitterParser.h"

#include <tree_sitter/api.h>

#include <utility>

extern "C" const TSLanguage* tree_sitter_c(void);
extern "C" const TSLanguage* tree_sitter_cpp(void);

namespace codeqa::frontend
{

namespace
{

const TSLanguage* languageFor(Grammar grammar)
{
    switch (grammar)
    {
    case Grammar::C:
        return tree_sitter_c();
    case Grammar::Cpp:
        return tree_sitter_cpp();
    }

    return tree_sitter_cpp();
}

} // namespace

SyntaxTree::SyntaxTree(TSTree* tree)
    : tree_(tree)
{
}

SyntaxTree::~SyntaxTree()
{
    if (tree_ != nullptr)
    {
        ts_tree_delete(tree_);
    }
}

SyntaxTree::SyntaxTree(SyntaxTree&& other) noexcept
    : tree_(std::exchange(other.tree_, nullptr))
{
}

SyntaxTree& SyntaxTree::operator=(SyntaxTree&& other) noexcept
{
    if (this != &other)
    {
        if (tree_ != nullptr)
        {
            ts_tree_delete(tree_);
        }

        tree_ = std::exchange(other.tree_, nullptr);
    }

    return *this;
}

const char* SyntaxTree::rootNodeType() const
{
    return ts_node_type(ts_tree_root_node(tree_));
}

bool SyntaxTree::rootHasError() const
{
    return ts_node_has_error(ts_tree_root_node(tree_));
}

TreeSitterParser::TreeSitterParser()
    : parser_(ts_parser_new())
{
}

TreeSitterParser::~TreeSitterParser()
{
    if (parser_ != nullptr)
    {
        ts_parser_delete(parser_);
    }
}

TreeSitterParser::TreeSitterParser(TreeSitterParser&& other) noexcept
    : parser_(std::exchange(other.parser_, nullptr))
{
}

TreeSitterParser& TreeSitterParser::operator=(TreeSitterParser&& other) noexcept
{
    if (this != &other)
    {
        if (parser_ != nullptr)
        {
            ts_parser_delete(parser_);
        }

        parser_ = std::exchange(other.parser_, nullptr);
    }

    return *this;
}

std::optional<SyntaxTree> TreeSitterParser::parse(Grammar grammar, std::string_view source)
{
    if (parser_ == nullptr)
    {
        return std::nullopt;
    }

    if (!ts_parser_set_language(parser_, languageFor(grammar)))
    {
        return std::nullopt;
    }

    TSTree* tree = ts_parser_parse_string(
        parser_, nullptr, source.data(), static_cast<uint32_t>(source.size()));

    if (tree == nullptr)
    {
        return std::nullopt;
    }

    return SyntaxTree{tree};
}

} // namespace codeqa::frontend
