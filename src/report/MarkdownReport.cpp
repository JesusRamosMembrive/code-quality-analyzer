#include "codeqa/report/MarkdownReport.h"

#include "codeqa/core/EnumJson.h"

#include <cstddef>
#include <filesystem>
#include <string>

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

std::string relativeDisplayPath(const std::filesystem::path& path, const std::filesystem::path& root)
{
    return std::filesystem::relative(path, root).generic_string();
}

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

    output << "## Summary\n\n"
              "| Metric | Value |\n"
              "| --- | --- |\n"
              "| Root | "
           << model.rootPath.generic_string()
           << " |\n"
              "| Files analyzed | "
           << model.files.size() << " (C: " << cCount << ", C++: " << cppCount << ", unavailable: " << unavailableCount
           << ") |\n"
              "| Functions measured | "
           << functionCount
           << " |\n"
              "| Diagnostics | "
           << (diagnosticCounts.errors + diagnosticCounts.warnings + diagnosticCounts.information)
           << " (errors: " << diagnosticCounts.errors << ", warnings: " << diagnosticCounts.warnings
           << ", information: " << diagnosticCounts.information
           << ") |\n"
              "| Findings | "
           << (findingCounts.errors + findingCounts.warnings) << " (errors: " << findingCounts.errors
           << ", warnings: " << findingCounts.warnings << ") |\n\n";

    const std::size_t total = findingCounts.errors + findingCounts.warnings;

    output << "Result: ";

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
