#pragma once

#include "codeqa/core/DiagnosticSeverity.h"
#include "codeqa/core/SourceLocation.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace codeqa::rules
{

struct Finding
{
    std::string ruleId;
    codeqa::core::DiagnosticSeverity severity{codeqa::core::DiagnosticSeverity::Information};
    std::string description;
    std::string recommendation;
    std::optional<long long> observedValue;
    std::optional<long long> limitValue;
    codeqa::core::SourceLocation location;
    std::string symbolName;
};

struct FileFindingsModel
{
    std::filesystem::path path;
    std::vector<Finding> findings;
};

struct ProjectFindingsModel
{
    std::filesystem::path rootPath;
    std::vector<FileFindingsModel> files;
};

} // namespace codeqa::rules
