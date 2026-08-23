#pragma once

#include "codeqa/core/DiagnosticSeverity.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace codeqa::architecture
{

struct FileDependencyMetrics
{
    std::filesystem::path path;
    std::size_t fanIn{0};
    std::size_t fanOut{0};
};

struct DependencyCycle
{
    std::vector<std::filesystem::path> files;
};

struct FanOutFinding
{
    std::filesystem::path path;
    std::size_t fanOut{0};
    codeqa::core::DiagnosticSeverity severity{codeqa::core::DiagnosticSeverity::Warning};
};

struct ForbiddenDependencyViolation
{
    std::filesystem::path fromPath;
    std::string fromLayer;
    std::filesystem::path toPath;
    std::string toLayer;
};

struct ProjectArchitectureModel
{
    std::vector<FileDependencyMetrics> files;
    std::vector<DependencyCycle> cycles;
    std::vector<FanOutFinding> fanOutFindings;
    std::vector<ForbiddenDependencyViolation> forbiddenDependencyViolations;
    std::size_t externalOrUnresolvedIncludes{0};
    std::size_t ambiguousIncludes{0};
};

} // namespace codeqa::architecture
