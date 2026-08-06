#include "codeqa/frontend/TreeSitterParser.h"

#include "TreeSitterInternals.h"

#include <memory>
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

void collectSyntaxErrors(TSNode node, std::vector<SyntaxError>& errors)
{
    if (ts_node_is_missing(node))
    {
        const TSPoint point = ts_node_start_point(node);
        errors.push_back(SyntaxError{
            static_cast<std::size_t>(point.row),
            static_cast<std::size_t>(point.column),
            true});
        return;
    }

    if (ts_node_is_error(node))
    {
        const TSPoint point = ts_node_start_point(node);
        errors.push_back(SyntaxError{
            static_cast<std::size_t>(point.row),
            static_cast<std::size_t>(point.column),
            false});
        return;
    }

    if (!ts_node_has_error(node))
    {
        return;
    }

    const uint32_t childCount = ts_node_child_count(node);

    for (uint32_t index = 0; index < childCount; ++index)
    {
        collectSyntaxErrors(ts_node_child(node, index), errors);
    }
}

} // namespace

SyntaxTree::Impl::Impl(TSTree* tree)
    : tree_(tree)
{
}

SyntaxTree::Impl::~Impl()
{
    if (tree_ != nullptr)
    {
        ts_tree_delete(tree_);
    }
}

TSNode SyntaxTree::Impl::rootNode() const
{
    return ts_tree_root_node(tree_);
}

SyntaxTree::SyntaxTree(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl))
{
}

SyntaxTree::~SyntaxTree() = default;

SyntaxTree::SyntaxTree(SyntaxTree&&) noexcept = default;

SyntaxTree& SyntaxTree::operator=(SyntaxTree&&) noexcept = default;

const char* SyntaxTree::rootNodeType() const
{
    return ts_node_type(impl_->rootNode());
}

bool SyntaxTree::rootHasError() const
{
    return ts_node_has_error(impl_->rootNode());
}

std::vector<SyntaxError> SyntaxTree::syntaxErrors() const
{
    std::vector<SyntaxError> errors;

    if (impl_ != nullptr)
    {
        collectSyntaxErrors(impl_->rootNode(), errors);
    }

    return errors;
}

TreeSitterParser::Impl::Impl()
    : parser_(ts_parser_new())
{
}

TreeSitterParser::Impl::~Impl()
{
    if (parser_ != nullptr)
    {
        ts_parser_delete(parser_);
    }
}

TSParser* TreeSitterParser::Impl::parser() const
{
    return parser_;
}

TreeSitterParser::TreeSitterParser()
    : impl_(std::make_unique<Impl>())
{
}

TreeSitterParser::~TreeSitterParser() = default;

TreeSitterParser::TreeSitterParser(TreeSitterParser&&) noexcept = default;

TreeSitterParser& TreeSitterParser::operator=(TreeSitterParser&&) noexcept = default;

std::optional<SyntaxTree> TreeSitterParser::parse(Grammar grammar, std::string_view source)
{
    if (impl_ == nullptr || impl_->parser() == nullptr)
    {
        return std::nullopt;
    }

    if (!ts_parser_set_language(impl_->parser(), languageFor(grammar)))
    {
        return std::nullopt;
    }

    TSTree* tree = ts_parser_parse_string(
        impl_->parser(), nullptr, source.data(), static_cast<uint32_t>(source.size()));

    if (tree == nullptr)
    {
        return std::nullopt;
    }

    return SyntaxTree{std::make_unique<SyntaxTree::Impl>(tree)};
}

} // namespace codeqa::frontend
