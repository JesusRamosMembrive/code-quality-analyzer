#include "ReportTextUtilities.h"

namespace codeqa::report
{

const char* severityTag(codeqa::core::DiagnosticSeverity severity)
{
    switch (severity)
    {
    case codeqa::core::DiagnosticSeverity::Error:
        return "ERROR";
    case codeqa::core::DiagnosticSeverity::Warning:
        return "WARNING";
    case codeqa::core::DiagnosticSeverity::Information:
        return "INFORMATION";
    }

    return "ERROR";
}

std::string relativeDisplayPath(const std::filesystem::path& path, const std::filesystem::path& root)
{
    return std::filesystem::relative(path, root).generic_string();
}

void countSeverity(SeverityCounts& counts, codeqa::core::DiagnosticSeverity severity)
{
    switch (severity)
    {
    case codeqa::core::DiagnosticSeverity::Error:
        ++counts.errors;
        break;
    case codeqa::core::DiagnosticSeverity::Warning:
        ++counts.warnings;
        break;
    case codeqa::core::DiagnosticSeverity::Information:
        ++counts.information;
        break;
    }
}

ReportSummary computeReportSummary(
    const codeqa::model::ProjectModel& model,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::rules::ProjectFindingsModel& findings)
{
    ReportSummary summary;

    for (const codeqa::model::FileModel& file : model.files)
    {
        if (file.language == codeqa::core::Language::C)
        {
            ++summary.cCount;
        }
        else if (file.language == codeqa::core::Language::Cpp)
        {
            ++summary.cppCount;
        }

        if (file.state == codeqa::model::AnalysisState::Unavailable)
        {
            ++summary.unavailableCount;
        }

        for (const codeqa::core::Diagnostic& diagnostic : file.diagnostics)
        {
            countSeverity(summary.diagnosticCounts, diagnostic.severity);
        }
    }

    for (const codeqa::metrics::FileMetricsModel& file : metrics.files)
    {
        summary.functionCount += file.functions.size();
    }

    for (const codeqa::rules::FileFindingsModel& file : findings.files)
    {
        if (!file.findings.empty())
        {
            ++summary.filesWithFindings;
        }

        for (const codeqa::rules::Finding& finding : file.findings)
        {
            countSeverity(summary.findingCounts, finding.severity);
        }
    }

    return summary;
}

} // namespace codeqa::report
