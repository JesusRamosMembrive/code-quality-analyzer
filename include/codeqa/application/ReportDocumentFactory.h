#pragma once

#include "codeqa/application/ReportDocument.h"
#include "codeqa/application/RunAnalysis.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <optional>
#include <string>

namespace codeqa::application
{

struct ReportDocumentResult
{
    std::optional<ReportDocument> document;
    std::optional<std::string> error;
};

[[nodiscard]] ReportDocument makeReportDocument(
    const RunAnalysisResult& analysis,
    const codeqa::config::ProjectConfig& config,
    const std::string& toolVersion,
    QualityGateOutcome qualityGateOutcome,
    const std::optional<ReportComparison>& comparison);

[[nodiscard]] ReportDocumentResult parseReportDocument(const nlohmann::json& document);
[[nodiscard]] ReportDocumentResult loadReportDocument(const std::filesystem::path& path);

} // namespace codeqa::application
