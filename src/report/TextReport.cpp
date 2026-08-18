#include "codeqa/report/TextReport.h"

#include "ReportTextUtilities.h"
#include "codeqa/core/EnumJson.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace codeqa::report
{

namespace
{

struct LocatedFinding
{
    std::filesystem::path filePath;
    const codeqa::rules::Finding* finding{nullptr};
};

void writeSummary(
    std::ostream& output,
    const codeqa::model::ProjectModel& model,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::rules::ProjectFindingsModel& findings)
{
    const ReportSummary summary = computeReportSummary(model, metrics, findings);

    output << "Summary\n"
              "-------\n"
              "Root: "
           << model.rootPath.generic_string() << "\n"
                                                  "Files analyzed: "
           << model.files.size() << " (C: " << summary.cCount << ", C++: " << summary.cppCount
           << ", unavailable: " << summary.unavailableCount
           << ")\n"
              "Functions measured: "
           << summary.functionCount << "\n"
                                "Diagnostics: "
           << (summary.diagnosticCounts.errors + summary.diagnosticCounts.warnings
               + summary.diagnosticCounts.information)
           << " (errors: " << summary.diagnosticCounts.errors << ", warnings: " << summary.diagnosticCounts.warnings
           << ", information: " << summary.diagnosticCounts.information
           << ")\n"
              "Findings: "
           << (summary.findingCounts.errors + summary.findingCounts.warnings)
           << " (errors: " << summary.findingCounts.errors << ", warnings: " << summary.findingCounts.warnings
           << ")\n";
}

std::vector<LocatedFinding> collectFindingsBySeverity(
    const codeqa::rules::ProjectFindingsModel& findings, codeqa::core::DiagnosticSeverity severity)
{
    std::vector<LocatedFinding> located;

    for (const codeqa::rules::FileFindingsModel& file : findings.files)
    {
        for (const codeqa::rules::Finding& finding : file.findings)
        {
            if (finding.severity == severity)
            {
                located.push_back(LocatedFinding{file.path, &finding});
            }
        }
    }

    return located;
}

void writeFindingsBySeverity(
    std::ostream& output, const codeqa::rules::ProjectFindingsModel& findings, const std::filesystem::path& rootPath)
{
    std::vector<LocatedFinding> ordered =
        collectFindingsBySeverity(findings, codeqa::core::DiagnosticSeverity::Error);
    const std::vector<LocatedFinding> warnings =
        collectFindingsBySeverity(findings, codeqa::core::DiagnosticSeverity::Warning);
    ordered.insert(ordered.end(), warnings.begin(), warnings.end());

    if (ordered.empty())
    {
        return;
    }

    output << "\nFindings by severity\n---------------------\n";

    for (const LocatedFinding& located : ordered)
    {
        const codeqa::rules::Finding& finding = *located.finding;
        output << '[' << severityTag(finding.severity) << "] " << finding.ruleId << "  "
               << relativeDisplayPath(located.filePath, rootPath) << ':' << finding.location.line << ':'
               << finding.location.column << "  " << finding.symbolName << "\n    " << finding.description
               << " (observed=" << (finding.observedValue.has_value() ? *finding.observedValue : 0)
               << ", limit=" << (finding.limitValue.has_value() ? *finding.limitValue : 0) << ")\n";
    }
}

void writeFindingsByFile(
    std::ostream& output, const codeqa::rules::ProjectFindingsModel& findings, const std::filesystem::path& rootPath)
{
    bool any = false;
    for (const codeqa::rules::FileFindingsModel& file : findings.files)
    {
        if (!file.findings.empty())
        {
            any = true;
            break;
        }
    }

    if (!any)
    {
        return;
    }

    output << "\nFindings by file\n-----------------\n";

    for (const codeqa::rules::FileFindingsModel& file : findings.files)
    {
        if (file.findings.empty())
        {
            continue;
        }

        output << relativeDisplayPath(file.path, rootPath) << '\n';

        for (const codeqa::rules::Finding& finding : file.findings)
        {
            output << "  [" << severityTag(finding.severity) << "] " << finding.ruleId << "  " << finding.location.line
                   << ':' << finding.location.column << "  " << finding.symbolName << " - " << finding.description
                   << '\n';
        }
    }
}

void writeDuplicates(
    std::ostream& output, const codeqa::duplication::ProjectDuplicationModel& duplicates,
    const std::filesystem::path& rootPath)
{
    if (duplicates.groups.empty())
    {
        return;
    }

    output << "\nDuplicated code\n----------------\n";

    std::size_t groupNumber = 1;
    for (const codeqa::duplication::DuplicateGroup& group : duplicates.groups)
    {
        output << "Group " << groupNumber << " (" << group.members.size() << " copies)\n";

        for (const codeqa::duplication::DuplicateMember& member : group.members)
        {
            output << "  " << relativeDisplayPath(member.range.begin.filePath, rootPath) << ':'
                   << member.range.begin.line << "  " << member.qualifiedName << '\n';
        }

        ++groupNumber;
    }
}

void writeDiagnostics(
    std::ostream& output, const codeqa::model::ProjectModel& model, const std::filesystem::path& rootPath)
{
    bool any = false;
    for (const codeqa::model::FileModel& file : model.files)
    {
        if (!file.diagnostics.empty())
        {
            any = true;
            break;
        }
    }

    if (!any)
    {
        return;
    }

    output << "\nDiagnostics\n-----------\n";

    for (const codeqa::model::FileModel& file : model.files)
    {
        if (file.diagnostics.empty())
        {
            continue;
        }

        output << relativeDisplayPath(file.path, rootPath) << '\n';

        for (const codeqa::core::Diagnostic& diagnostic : file.diagnostics)
        {
            output << "  line " << diagnostic.location.line << ": "
                   << codeqa::core::diagnosticSeverityToJsonString(diagnostic.severity) << " [" << diagnostic.code
                   << "] " << diagnostic.message << '\n';
        }
    }
}

void writeResult(std::ostream& output, const codeqa::rules::ProjectFindingsModel& findings)
{
    SeverityCounts findingCounts;
    std::size_t filesWithFindings = 0;

    for (const codeqa::rules::FileFindingsModel& file : findings.files)
    {
        if (!file.findings.empty())
        {
            ++filesWithFindings;
        }

        for (const codeqa::rules::Finding& finding : file.findings)
        {
            countSeverity(findingCounts, finding.severity);
        }
    }

    const std::size_t total = findingCounts.errors + findingCounts.warnings;

    output << "\nResult: ";

    if (total == 0)
    {
        output << "no findings.\n";
        return;
    }

    output << total << " finding" << (total == 1 ? "" : "s") << " (" << findingCounts.errors << " error"
           << (findingCounts.errors == 1 ? "" : "s") << ", " << findingCounts.warnings << " warning"
           << (findingCounts.warnings == 1 ? "" : "s") << ") across " << filesWithFindings << " file"
           << (filesWithFindings == 1 ? "" : "s") << ".\n";
}

} // namespace

void renderText(
    std::ostream& output,
    const codeqa::model::ProjectModel& model,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::rules::ProjectFindingsModel& findings,
    const codeqa::duplication::ProjectDuplicationModel& duplicates)
{
    output << "Code Quality Analysis Report\n=============================\n\n";
    writeSummary(output, model, metrics, findings);
    writeFindingsBySeverity(output, findings, model.rootPath);
    writeFindingsByFile(output, findings, model.rootPath);
    writeDuplicates(output, duplicates, model.rootPath);
    writeDiagnostics(output, model, model.rootPath);
    writeResult(output, findings);
}

} // namespace codeqa::report
