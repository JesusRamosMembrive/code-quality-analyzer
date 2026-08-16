#pragma once

#include "codeqa/config/ConfigModels.h"
#include "codeqa/core/Diagnostic.h"
#include "codeqa/duplication/DuplicationModels.h"
#include "codeqa/metrics/MetricsModels.h"
#include "codeqa/model/Models.h"
#include "codeqa/rules/RuleModels.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace codeqa::application
{

enum class QualityGateOutcome
{
    Passed,
    Failed,
    NotEvaluated
};

struct ReportComparison
{
    std::string previousTimestamp;
    std::size_t newCount{0};
    std::size_t existingCount{0};
    std::size_t resolvedCount{0};
};

struct ReportMetadata
{
    std::string rootPath;
    std::string toolName;
    std::string toolVersion;
};

struct ReportFile
{
    std::filesystem::path path;
    codeqa::core::Language language{codeqa::core::Language::Unknown};
    codeqa::model::AnalysisState state{codeqa::model::AnalysisState::Unavailable};
    std::vector<codeqa::core::Diagnostic> diagnostics;
    std::vector<codeqa::metrics::FunctionMetricsModel> metrics;
    std::vector<codeqa::rules::Finding> findings;
};

struct ReportDocument
{
    ReportMetadata metadata;
    codeqa::config::ProjectConfig config;
    std::vector<ReportFile> files;
    std::optional<ReportComparison> comparison;
    QualityGateOutcome qualityGateOutcome{QualityGateOutcome::NotEvaluated};
    codeqa::duplication::ProjectDuplicationModel duplicates;
};

} // namespace codeqa::application
