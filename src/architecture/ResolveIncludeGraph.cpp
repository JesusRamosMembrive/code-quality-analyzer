#include "codeqa/architecture/IncludeGraph.h"

#include <algorithm>

namespace codeqa::architecture
{

namespace
{

bool endsWithPathSegment(const std::string& path, const std::string& suffix)
{
    if (suffix.empty() || suffix.size() > path.size())
    {
        return false;
    }

    if (path.size() == suffix.size())
    {
        return path == suffix;
    }

    const std::size_t offset = path.size() - suffix.size();
    return path.compare(offset, suffix.size(), suffix) == 0 && path[offset - 1] == '/';
}

} // namespace

IncludeGraph resolveIncludeGraph(const codeqa::model::ProjectModel& project)
{
    IncludeGraph graph;
    graph.filePaths.reserve(project.files.size());
    for (const codeqa::model::FileModel& file : project.files)
    {
        graph.filePaths.push_back(file.path);
    }
    graph.outgoingEdges.resize(project.files.size());

    for (std::size_t fromIndex = 0; fromIndex < project.files.size(); ++fromIndex)
    {
        std::vector<std::size_t> resolved;

        for (const codeqa::model::DependencyModel& dependency : project.files[fromIndex].dependencies)
        {
            if (dependency.kind == codeqa::model::IncludeKind::System)
            {
                continue;
            }

            const std::string target = std::filesystem::path(dependency.target).generic_string();
            std::vector<std::size_t> matches;

            for (std::size_t toIndex = 0; toIndex < project.files.size(); ++toIndex)
            {
                if (endsWithPathSegment(project.files[toIndex].path.generic_string(), target))
                {
                    matches.push_back(toIndex);
                }
            }

            // Filter out self-includes
            std::vector<std::size_t> nonSelfMatches;
            for (std::size_t match : matches)
            {
                if (match != fromIndex)
                {
                    nonSelfMatches.push_back(match);
                }
            }

            if (nonSelfMatches.empty())
            {
                // Only count as unresolved if no non-self matches were found
                if (matches.empty())
                {
                    ++graph.externalOrUnresolvedIncludes;
                }
                continue;
            }

            if (nonSelfMatches.size() > 1)
            {
                ++graph.ambiguousIncludes;
            }

            const std::size_t toIndex = nonSelfMatches.front();
            if (std::find(resolved.begin(), resolved.end(), toIndex) == resolved.end())
            {
                resolved.push_back(toIndex);
            }
        }

        graph.outgoingEdges[fromIndex] = std::move(resolved);
    }

    return graph;
}

} // namespace codeqa::architecture
