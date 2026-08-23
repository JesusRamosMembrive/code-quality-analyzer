#include "codeqa/architecture/IncludeGraph.h"
#include "codeqa/infrastructure/GlobMatcher.h"

#include <optional>

namespace codeqa::architecture
{

namespace
{

// Layers is a std::map, so this iterates layer names in alphabetical order -- the
// alphabetically-first layer whose pattern matches wins when a file matches more than one.
std::optional<std::string> layerOf(
    const std::filesystem::path& relativePath, const std::map<std::string, std::vector<std::string>>& layers)
{
    for (const auto& [layerName, patterns] : layers)
    {
        for (const std::string& pattern : patterns)
        {
            if (codeqa::infrastructure::matchesExcludePattern(pattern, relativePath))
            {
                return layerName;
            }
        }
    }

    return std::nullopt;
}

bool isForbiddenPair(
    const std::vector<codeqa::config::LayerDependency>& forbiddenDependencies, const std::string& from,
    const std::string& to)
{
    for (const codeqa::config::LayerDependency& pair : forbiddenDependencies)
    {
        if (pair.from == from && pair.to == to)
        {
            return true;
        }
    }

    return false;
}

} // namespace

std::vector<ForbiddenDependencyViolation> computeForbiddenDependencies(
    const IncludeGraph& graph, const std::filesystem::path& rootPath, const codeqa::config::ArchitectureConfig& config)
{
    std::vector<ForbiddenDependencyViolation> violations;

    if (config.layers.empty() || config.forbiddenDependencies.empty())
    {
        return violations;
    }

    std::vector<std::optional<std::string>> layerPerFile;
    layerPerFile.reserve(graph.filePaths.size());
    for (const std::filesystem::path& path : graph.filePaths)
    {
        layerPerFile.push_back(layerOf(path.lexically_relative(rootPath), config.layers));
    }

    for (std::size_t fromIndex = 0; fromIndex < graph.filePaths.size(); ++fromIndex)
    {
        if (!layerPerFile[fromIndex].has_value())
        {
            continue;
        }

        for (const std::size_t toIndex : graph.outgoingEdges[fromIndex])
        {
            if (!layerPerFile[toIndex].has_value())
            {
                continue;
            }

            const std::string& fromLayer = *layerPerFile[fromIndex];
            const std::string& toLayer = *layerPerFile[toIndex];

            if (isForbiddenPair(config.forbiddenDependencies, fromLayer, toLayer))
            {
                violations.push_back(ForbiddenDependencyViolation{
                    graph.filePaths[fromIndex], fromLayer, graph.filePaths[toIndex], toLayer});
            }
        }
    }

    return violations;
}

} // namespace codeqa::architecture
