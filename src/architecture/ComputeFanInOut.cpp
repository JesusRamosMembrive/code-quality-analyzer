#include "codeqa/architecture/IncludeGraph.h"

namespace codeqa::architecture
{

std::vector<FileDependencyMetrics> computeFanInOut(const IncludeGraph& graph)
{
    std::vector<std::size_t> fanIn(graph.filePaths.size(), 0);
    for (const std::vector<std::size_t>& edges : graph.outgoingEdges)
    {
        for (const std::size_t target : edges)
        {
            ++fanIn[target];
        }
    }

    std::vector<FileDependencyMetrics> result;
    result.reserve(graph.filePaths.size());
    for (std::size_t index = 0; index < graph.filePaths.size(); ++index)
    {
        result.push_back(FileDependencyMetrics{
            graph.filePaths[index], fanIn[index], graph.outgoingEdges[index].size()});
    }

    return result;
}

} // namespace codeqa::architecture
