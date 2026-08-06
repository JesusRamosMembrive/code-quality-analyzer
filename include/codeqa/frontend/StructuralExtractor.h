#pragma once

#include "codeqa/frontend/Grammar.h"

#include <filesystem>
#include <memory>
#include <string_view>

namespace codeqa::frontend
{

struct ParsedFile;
class SyntaxTree;

class StructuralExtractor
{
public:
    StructuralExtractor();
    ~StructuralExtractor();

    StructuralExtractor(const StructuralExtractor&) = delete;
    StructuralExtractor& operator=(const StructuralExtractor&) = delete;

    StructuralExtractor(StructuralExtractor&&) noexcept;
    StructuralExtractor& operator=(StructuralExtractor&&) noexcept;

    void extract(
        const SyntaxTree& tree,
        Grammar grammar,
        std::string_view source,
        const std::filesystem::path& path,
        ParsedFile& result) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace codeqa::frontend
