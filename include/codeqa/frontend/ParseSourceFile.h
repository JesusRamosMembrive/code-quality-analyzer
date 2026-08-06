#pragma once

#include "codeqa/frontend/Grammar.h"
#include "codeqa/frontend/ParsedFile.h"
#include "codeqa/frontend/TreeSitterParser.h"

#include <filesystem>
#include <string>

namespace codeqa::frontend
{

[[nodiscard]] ParsedFile parseSourceFile(
    const std::filesystem::path& path,
    Grammar grammar,
    const std::string& grammarReason,
    TreeSitterParser& parser);

} // namespace codeqa::frontend
