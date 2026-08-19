#include "codeqa/application/ReportDocumentAnalysis.h"

#include <filesystem>
#include <utility>

namespace codeqa::application
{

namespace
{

/// Rewrites every duplicate member's stored (relative) filePath to the rootPath-prefixed
/// absolute form, mirroring how the per-file loop below absolutizes reportFile.path.
void absolutizeDuplicateMembers(
    codeqa::duplication::ProjectDuplicationModel& duplicates, const std::filesystem::path& rootPath)
{
    for (codeqa::duplication::DuplicateGroup& group : duplicates.groups)
    {
        for (codeqa::duplication::DuplicateMember& member : group.members)
        {
            member.range.begin.filePath = rootPath / member.range.begin.filePath;
            member.range.end.filePath = rootPath / member.range.end.filePath;
        }
    }
}

/// Mirrors absolutizeDuplicateMembers above: rewrites every architecture path from its
/// stored (relative) form to the rootPath-prefixed absolute form. Safe to call even if a
/// path were already absolute -- std::filesystem::path::operator/ with an absolute
/// right-hand side discards the left side per the standard, making this idempotent.
void absolutizeArchitecturePaths(
    codeqa::architecture::ProjectArchitectureModel& architecture, const std::filesystem::path& rootPath)
{
    for (codeqa::architecture::FileDependencyMetrics& file : architecture.files)
    {
        file.path = rootPath / file.path;
    }

    for (codeqa::architecture::DependencyCycle& cycle : architecture.cycles)
    {
        for (std::filesystem::path& path : cycle.files)
        {
            path = rootPath / path;
        }
    }

    for (codeqa::architecture::FanOutFinding& finding : architecture.fanOutFindings)
    {
        finding.path = rootPath / finding.path;
    }
}

} // namespace

RunAnalysisResult toAnalysisResult(const ReportDocument& document)
{
    RunAnalysisResult analysis;
    analysis.model.rootPath = document.metadata.rootPath;
    analysis.metrics.rootPath = analysis.model.rootPath;
    analysis.findings.rootPath = analysis.model.rootPath;
    analysis.duplicates = document.duplicates;
    absolutizeDuplicateMembers(analysis.duplicates, analysis.model.rootPath);
    analysis.architecture = document.architecture;
    absolutizeArchitecturePaths(analysis.architecture, analysis.model.rootPath);

    for (const ReportFile& reportFile : document.files)
    {
        const std::filesystem::path absolutePath = analysis.model.rootPath / reportFile.path;

        codeqa::model::FileModel modelFile;
        modelFile.path = absolutePath;
        modelFile.language = reportFile.language;
        modelFile.state = reportFile.state;
        modelFile.diagnostics = reportFile.diagnostics;
        analysis.model.files.push_back(std::move(modelFile));

        codeqa::metrics::FileMetricsModel metricsFile;
        metricsFile.path = absolutePath;
        metricsFile.functions = reportFile.metrics;
        analysis.metrics.files.push_back(std::move(metricsFile));

        codeqa::rules::FileFindingsModel findingsFile;
        findingsFile.path = absolutePath;
        findingsFile.findings = reportFile.findings;
        analysis.findings.files.push_back(std::move(findingsFile));
    }

    return analysis;
}

} // namespace codeqa::application
