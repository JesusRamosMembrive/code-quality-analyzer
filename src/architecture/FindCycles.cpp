#include "codeqa/architecture/IncludeGraph.h"

#include <algorithm>

namespace codeqa::architecture
{

namespace
{

struct TarjanState
{
    std::vector<long long> index;
    std::vector<long long> lowLink;
    std::vector<bool> onStack;
    std::vector<std::size_t> stack;
    long long nextIndex{0};
    std::vector<DependencyCycle> cycles;
};

void strongConnect(const IncludeGraph& graph, std::size_t vertex, TarjanState& state)
{
    state.index[vertex] = state.nextIndex;
    state.lowLink[vertex] = state.nextIndex;
    ++state.nextIndex;
    state.stack.push_back(vertex);
    state.onStack[vertex] = true;

    for (const std::size_t neighbor : graph.outgoingEdges[vertex])
    {
        if (state.index[neighbor] == -1)
        {
            strongConnect(graph, neighbor, state);
            state.lowLink[vertex] = std::min(state.lowLink[vertex], state.lowLink[neighbor]);
        }
        else if (state.onStack[neighbor])
        {
            state.lowLink[vertex] = std::min(state.lowLink[vertex], state.index[neighbor]);
        }
    }

    if (state.lowLink[vertex] == state.index[vertex])
    {
        DependencyCycle component;
        std::size_t member = 0;
        do
        {
            member = state.stack.back();
            state.stack.pop_back();
            state.onStack[member] = false;
            component.files.push_back(graph.filePaths[member]);
        } while (member != vertex);

        if (component.files.size() > 1)
        {
            state.cycles.push_back(std::move(component));
        }
    }
}

} // namespace

std::vector<DependencyCycle> findCycles(const IncludeGraph& graph)
{
    TarjanState state;
    const std::size_t fileCount = graph.filePaths.size();
    state.index.assign(fileCount, -1);
    state.lowLink.assign(fileCount, -1);
    state.onStack.assign(fileCount, false);

    for (std::size_t vertex = 0; vertex < fileCount; ++vertex)
    {
        if (state.index[vertex] == -1)
        {
            strongConnect(graph, vertex, state);
        }
    }

    return std::move(state.cycles);
}

} // namespace codeqa::architecture
