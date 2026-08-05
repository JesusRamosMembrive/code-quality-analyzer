#pragma once

#include "codeqa/frontend/Grammar.h"

#include <cstddef>
#include <optional>
#include <string_view>

struct TSParser;
struct TSTree;

namespace codeqa::frontend
{

/// Owns a parsed syntax tree. Non-copyable, movable: the underlying TSTree
/// must be deleted exactly once.
class SyntaxTree
{
public:
    explicit SyntaxTree(TSTree* tree);
    ~SyntaxTree();

    SyntaxTree(const SyntaxTree&) = delete;
    SyntaxTree& operator=(const SyntaxTree&) = delete;

    SyntaxTree(SyntaxTree&& other) noexcept;
    SyntaxTree& operator=(SyntaxTree&& other) noexcept;

    [[nodiscard]] const char* rootNodeType() const;
    [[nodiscard]] bool rootHasError() const;

    /// Only the frontend's own translation units use this.
    [[nodiscard]] TSTree* handle() const { return tree_; }

private:
    TSTree* tree_{nullptr};
};

/// Owns a reusable TSParser. Non-copyable, movable.
class TreeSitterParser
{
public:
    TreeSitterParser();
    ~TreeSitterParser();

    TreeSitterParser(const TreeSitterParser&) = delete;
    TreeSitterParser& operator=(const TreeSitterParser&) = delete;

    TreeSitterParser(TreeSitterParser&& other) noexcept;
    TreeSitterParser& operator=(TreeSitterParser&& other) noexcept;

    [[nodiscard]] std::optional<SyntaxTree> parse(Grammar grammar, std::string_view source);

private:
    TSParser* parser_{nullptr};
};

} // namespace codeqa::frontend
