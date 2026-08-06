#pragma once

#include "codeqa/frontend/Grammar.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace codeqa::frontend
{

class StructuralExtractor;
class TreeSitterParser;

struct SyntaxError
{
    std::size_t line{0};
    std::size_t column{0};
    bool missing{false};
};

/// Owns a parsed syntax tree. Non-copyable and movable.
class SyntaxTree
{
public:
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
    friend class TreeSitterParser;

    class Impl;

    explicit SyntaxTree(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl_;
};

/// Owns a reusable parser. Non-copyable and movable.
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
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace codeqa::frontend
