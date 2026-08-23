#pragma once

#include "codeqa/architecture/ArchitectureModels.h"
#include "codeqa/config/ConfigModels.h"
#include "codeqa/model/Models.h"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace codeqa::architecture
{

struct IncludeGraph
{
    std::vector<std::filesystem::path> filePaths;
    std::vector<std::vector<std::size_t>> outgoingEdges;
    std::size_t externalOrUnresolvedIncludes{0};
    std::size_t ambiguousIncludes{0};
};

[[nodiscard]] IncludeGraph resolveIncludeGraph(const codeqa::model::ProjectModel& project);

[[nodiscard]] std::vector<DependencyCycle> findCycles(const IncludeGraph& graph);
[[nodiscard]] std::vector<FileDependencyMetrics> computeFanInOut(const IncludeGraph& graph);
[[nodiscard]] std::vector<ForbiddenDependencyViolation> computeForbiddenDependencies(
    const IncludeGraph& graph,
    const std::filesystem::path& rootPath,
    const codeqa::config::ArchitectureConfig& config);

} // namespace codeqa::architecture
