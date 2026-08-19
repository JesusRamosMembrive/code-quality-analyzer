#pragma once

#include "codeqa/core/DiagnosticSeverity.h"

#include <cstddef>
#include <filesystem>
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

struct ProjectArchitectureModel
{
    std::vector<FileDependencyMetrics> files;
    std::vector<DependencyCycle> cycles;
    std::vector<FanOutFinding> fanOutFindings;
    std::size_t externalOrUnresolvedIncludes{0};
    std::size_t ambiguousIncludes{0};
};

} // namespace codeqa::architecture
