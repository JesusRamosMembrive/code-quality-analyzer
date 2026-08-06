#pragma once

#include "codeqa/frontend/Grammar.h"
#include "codeqa/frontend/ParsedFile.h"
#include "codeqa/frontend/TreeSitterParser.h"

#include <filesystem>
#include <string>

namespace codeqa::frontend
{

class StructuralExtractor;

[[nodiscard]] ParsedFile parseSourceFile(
    const std::filesystem::path& path,
    Grammar grammar,
    const std::string& grammarReason,
    TreeSitterParser& parser,
    StructuralExtractor& extractor);

} // namespace codeqa::frontend
