#include "codeqa/report/MarkdownReport.h"

#include "ReportTextUtilities.h"
#include "codeqa/core/EnumJson.h"

#include <cstddef>
#include <filesystem>
#include <string>

namespace codeqa::report
{

namespace
{

void writeSummary(
    std::ostream& output,
    const codeqa::model::ProjectModel& model,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::rules::ProjectFindingsModel& findings)
{
    const ReportSummary summary = computeReportSummary(model, metrics, findings);

    output << "## Summary\n\n"
              "| Metric | Value |\n"
              "| --- | --- |\n"
              "| Root | "
           << model.rootPath.generic_string()
           << " |\n"
              "| Files analyzed | "
           << model.files.size() << " (C: " << summary.cCount << ", C++: " << summary.cppCount
           << ", unavailable: " << summary.unavailableCount
           << ") |\n"
              "| Functions measured | "
           << summary.functionCount
           << " |\n"
              "| Diagnostics | "
           << (summary.diagnosticCounts.errors + summary.diagnosticCounts.warnings
               + summary.diagnosticCounts.information)
           << " (errors: " << summary.diagnosticCounts.errors << ", warnings: " << summary.diagnosticCounts.warnings
           << ", information: " << summary.diagnosticCounts.information
           << ") |\n"
              "| Findings | "
           << (summary.findingCounts.errors + summary.findingCounts.warnings)
           << " (errors: " << summary.findingCounts.errors << ", warnings: " << summary.findingCounts.warnings
           << ") |\n\n";

    const std::size_t total = summary.findingCounts.errors + summary.findingCounts.warnings;

    output << "Result: ";

    if (total == 0)
    {
        output << "no findings.\n";
        return;
    }

    output << total << " finding" << (total == 1 ? "" : "s") << " (" << summary.findingCounts.errors << " error"
           << (summary.findingCounts.errors == 1 ? "" : "s") << ", " << summary.findingCounts.warnings << " warning"
           << (summary.findingCounts.warnings == 1 ? "" : "s") << ") across " << summary.filesWithFindings << " file"
           << (summary.filesWithFindings == 1 ? "" : "s") << ".\n";
}

void writeFindingChecklistItem(std::ostream& output, const codeqa::rules::Finding& finding)
{
    output << "- [ ] **[" << severityTag(finding.severity) << "]** `" << finding.ruleId << "` \xE2\x80\x94 `"
           << finding.symbolName << "` \xE2\x80\x94 " << finding.location.line << ':' << finding.location.column
           << " \xE2\x80\x94 " << finding.description
           << " (observed=" << (finding.observedValue.has_value() ? *finding.observedValue : 0)
           << ", limit=" << (finding.limitValue.has_value() ? *finding.limitValue : 0) << ")\n";
}

void writeDiagnosticChecklistItem(std::ostream& output, const codeqa::core::Diagnostic& diagnostic)
{
    output << "- [ ] **[" << severityTag(diagnostic.severity) << "]** `" << diagnostic.code << "` \xE2\x80\x94 "
           << diagnostic.location.line << ':' << diagnostic.location.column << " \xE2\x80\x94 " << diagnostic.message
           << '\n';
}

void writeFileSections(
    std::ostream& output,
    const codeqa::model::ProjectModel& model,
    const codeqa::rules::ProjectFindingsModel& findings)
{
    for (std::size_t index = 0; index < model.files.size(); ++index)
    {
        const codeqa::model::FileModel& file = model.files[index];
        const codeqa::rules::FileFindingsModel* fileFindings =
            index < findings.files.size() ? &findings.files[index] : nullptr;

        const bool hasFindings = fileFindings != nullptr && !fileFindings->findings.empty();
        const bool hasDiagnostics = !file.diagnostics.empty();

        if (!hasFindings && !hasDiagnostics)
        {
            continue;
        }

        output << "\n## " << relativeDisplayPath(file.path, model.rootPath) << "\n\n";

        if (hasFindings)
        {
            for (const codeqa::rules::Finding& finding : fileFindings->findings)
            {
                writeFindingChecklistItem(output, finding);
            }
        }

        for (const codeqa::core::Diagnostic& diagnostic : file.diagnostics)
        {
            writeDiagnosticChecklistItem(output, diagnostic);
        }
    }
}

} // namespace

void renderMarkdown(
    std::ostream& output,
    const codeqa::model::ProjectModel& model,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::rules::ProjectFindingsModel& findings)
{
    output << "# Code Quality Analysis Report\n\n";
    writeSummary(output, model, metrics, findings);
    writeFileSections(output, model, findings);
}

} // namespace codeqa::report
