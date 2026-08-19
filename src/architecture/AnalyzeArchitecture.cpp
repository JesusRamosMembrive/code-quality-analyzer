#include "codeqa/architecture/AnalyzeArchitecture.h"

#include "codeqa/architecture/IncludeGraph.h"

namespace codeqa::architecture
{

ProjectArchitectureModel analyzeArchitecture(
    const codeqa::model::ProjectModel& project, const codeqa::config::ArchitectureConfig& config)
{
    ProjectArchitectureModel result;

    if (!config.enabled)
    {
        return result;
    }

    const IncludeGraph graph = resolveIncludeGraph(project);
    result.files = computeFanInOut(graph);
    result.cycles = findCycles(graph);
    result.externalOrUnresolvedIncludes = graph.externalOrUnresolvedIncludes;
    result.ambiguousIncludes = graph.ambiguousIncludes;

    for (const FileDependencyMetrics& file : result.files)
    {
        if (static_cast<long long>(file.fanOut) < config.fanOutWarning)
        {
            continue;
        }

        FanOutFinding finding;
        finding.path = file.path;
        finding.fanOut = file.fanOut;
        finding.severity = static_cast<long long>(file.fanOut) >= config.fanOutHigh
            ? codeqa::core::DiagnosticSeverity::Error
            : codeqa::core::DiagnosticSeverity::Warning;
        result.fanOutFindings.push_back(finding);
    }

    return result;
}

} // namespace codeqa::architecture
