#pragma once

#include "codeqa/core/Diagnostic.h"
#include "codeqa/core/Language.h"
#include "codeqa/core/SourceRange.h"
#include "codeqa/frontend/Grammar.h"
#include "codeqa/frontend/StructuralModels.h"

#include <filesystem>
#include <string>
#include <vector>

namespace codeqa::frontend
{

struct ParsedFile
{
    std::filesystem::path path;
    codeqa::core::Language language{codeqa::core::Language::Unknown};
    Grammar grammar{Grammar::Cpp};
    std::string grammarReason;
    std::vector<codeqa::core::Diagnostic> diagnostics;
    std::vector<FunctionModel> functions;
    std::vector<ClassModel> classes;
    std::vector<IncludeDependency> includes;
    std::vector<GlobalVariableModel> globalVariables;
    std::vector<TypeAliasModel> typeAliases;
    std::vector<MacroModel> macros;
    std::vector<core::SourceRange> comments;
    bool parsed{false};
};

} // namespace codeqa::frontend
