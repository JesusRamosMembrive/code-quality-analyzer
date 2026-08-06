#pragma once

#include "codeqa/core/Diagnostic.h"
#include "codeqa/frontend/Grammar.h"

#include <filesystem>
#include <string>
#include <vector>

namespace codeqa::frontend
{

struct ParsedFile
{
    std::filesystem::path path;
    Grammar grammar{Grammar::Cpp};
    std::string grammarReason;
    std::vector<codeqa::core::Diagnostic> diagnostics;
    bool parsed{false};
};

} // namespace codeqa::frontend
