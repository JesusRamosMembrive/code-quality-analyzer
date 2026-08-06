#pragma once

#include "codeqa/frontend/TreeSitterParser.h"

#include <tree_sitter/api.h>

namespace codeqa::frontend
{

class SyntaxTree::Impl
{
public:
    explicit Impl(TSTree* tree);
    ~Impl();

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    [[nodiscard]] TSNode rootNode() const;

private:
    TSTree* tree_{nullptr};
};

class TreeSitterParser::Impl
{
public:
    Impl();
    ~Impl();

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    [[nodiscard]] TSParser* parser() const;

private:
    TSParser* parser_{nullptr};
};

} // namespace codeqa::frontend
