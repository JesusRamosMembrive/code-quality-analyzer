#pragma once

#include "codeqa/core/Diagnostic.h"
#include "codeqa/core/DiagnosticSeverity.h"
#include "codeqa/metrics/MetricsModels.h"
#include "codeqa/model/Models.h"
#include "codeqa/rules/RuleModels.h"

#include <cstddef>
#include <filesystem>
#include <string>

namespace codeqa::report
{

const char* severityTag(codeqa::core::DiagnosticSeverity severity);

std::string relativeDisplayPath(const std::filesystem::path& path, const std::filesystem::path& root);

struct SeverityCounts
{
    std::size_t errors{0};
    std::size_t warnings{0};
    std::size_t information{0};
};

void countSeverity(SeverityCounts& counts, codeqa::core::DiagnosticSeverity severity);

struct ReportSummary
{
    std::size_t cCount{0};
    std::size_t cppCount{0};
    std::size_t unavailableCount{0};
    std::size_t functionCount{0};
    SeverityCounts diagnosticCounts;
    SeverityCounts findingCounts;
    std::size_t filesWithFindings{0};
};

ReportSummary computeReportSummary(
    const codeqa::model::ProjectModel& model,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::rules::ProjectFindingsModel& findings);

} // namespace codeqa::report
