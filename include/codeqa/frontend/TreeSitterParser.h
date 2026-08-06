#pragma once

#include "codeqa/frontend/Grammar.h"

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

struct TSParser;
struct TSTree;

namespace codeqa::frontend
{

class StructuralExtractor;

struct SyntaxError
{
    std::size_t line{0};
    std::size_t column{0};
    bool missing{false};
};

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
    [[nodiscard]] std::vector<SyntaxError> syntaxErrors() const;

private:
    friend class StructuralExtractor;

    [[nodiscard]] TSTree* handle() const;

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
