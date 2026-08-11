#include "codeqa/report/TextReport.h"

#include "codeqa/core/EnumJson.h"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace codeqa::report
{

namespace
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

struct SeverityCounts
{
    std::size_t errors{0};
    std::size_t warnings{0};
    std::size_t information{0};
};

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
    std::size_t cCount = 0;
    std::size_t cppCount = 0;
    std::size_t unavailableCount = 0;

    for (const codeqa::model::FileModel& file : model.files)
    {
        if (file.language == codeqa::core::Language::C)
        {
            ++cCount;
        }
        else if (file.language == codeqa::core::Language::Cpp)
        {
            ++cppCount;
        }

        if (file.state == codeqa::model::AnalysisState::Unavailable)
        {
            ++unavailableCount;
        }
    }

    std::size_t functionCount = 0;
    for (const codeqa::metrics::FileMetricsModel& file : metrics.files)
    {
        functionCount += file.functions.size();
    }

    SeverityCounts diagnosticCounts;
    for (const codeqa::model::FileModel& file : model.files)
    {
        for (const codeqa::core::Diagnostic& diagnostic : file.diagnostics)
        {
            countSeverity(diagnosticCounts, diagnostic.severity);
        }
    }

    SeverityCounts findingCounts;
    for (const codeqa::rules::FileFindingsModel& file : findings.files)
    {
        for (const codeqa::rules::Finding& finding : file.findings)
        {
            countSeverity(findingCounts, finding.severity);
        }
    }

    output << "Summary\n"
              "-------\n"
              "Root: "
           << model.rootPath.generic_string() << "\n"
                                                  "Files analyzed: "
           << model.files.size() << " (C: " << cCount << ", C++: " << cppCount << ", unavailable: " << unavailableCount
           << ")\n"
              "Functions measured: "
           << functionCount << "\n"
                                "Diagnostics: "
           << (diagnosticCounts.errors + diagnosticCounts.warnings + diagnosticCounts.information)
           << " (errors: " << diagnosticCounts.errors << ", warnings: " << diagnosticCounts.warnings
           << ", information: " << diagnosticCounts.information
           << ")\n"
              "Findings: "
           << (findingCounts.errors + findingCounts.warnings) << " (errors: " << findingCounts.errors
           << ", warnings: " << findingCounts.warnings << ")\n";
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

void writeFindingsBySeverity(std::ostream& output, const codeqa::rules::ProjectFindingsModel& findings)
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
               << located.filePath.generic_string() << ':' << finding.location.line << ':' << finding.location.column
               << "  " << finding.symbolName << "\n    " << finding.description
               << " (observed=" << (finding.observedValue.has_value() ? *finding.observedValue : 0)
               << ", limit=" << (finding.limitValue.has_value() ? *finding.limitValue : 0) << ")\n";
    }
}

void writeFindingsByFile(std::ostream& output, const codeqa::rules::ProjectFindingsModel& findings)
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

        output << file.path.generic_string() << '\n';

        for (const codeqa::rules::Finding& finding : file.findings)
        {
            output << "  [" << severityTag(finding.severity) << "] " << finding.ruleId << "  " << finding.location.line
                   << ':' << finding.location.column << "  " << finding.symbolName << " - " << finding.description
                   << '\n';
        }
    }
}

void writeDiagnostics(std::ostream& output, const codeqa::model::ProjectModel& model)
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

        output << file.path.generic_string() << '\n';

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
    const codeqa::rules::ProjectFindingsModel& findings)
{
    output << "Code Quality Analysis Report\n=============================\n\n";
    writeSummary(output, model, metrics, findings);
    writeFindingsBySeverity(output, findings);
    writeFindingsByFile(output, findings);
    writeDiagnostics(output, model);
    writeResult(output, findings);
}

} // namespace codeqa::report
