#pragma once

#include "codeqa/application/AnalysisProgress.h"
#include "codeqa/application/DetectFileLanguage.h"
#include "codeqa/application/ReportDocumentFactory.h"
#include "codeqa/config/ConfigModels.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace codeqa::application
{

struct ApplicationError
{
    std::optional<std::string> message;

    [[nodiscard]] bool hasError() const { return message.has_value(); }
};

struct FacadeAnalysisRequest
{
    std::filesystem::path projectRoot;
    codeqa::config::ProjectConfig config;
    std::vector<std::string> excludePatterns;
    std::vector<LanguageOverride> languageOverrides;
};

struct AnalysisFacadeResult
{
    std::optional<ReportDocument> document;
    std::optional<nlohmann::json> reportJson;
    std::optional<std::filesystem::path> savedSnapshotPath;
    std::optional<ReportComparison> comparison;
    std::optional<std::string> warning;
    ApplicationError error;
    QualityGateOutcome qualityGateOutcome{QualityGateOutcome::NotEvaluated};
};

struct ProjectConfigLoadResult
{
    std::optional<codeqa::config::ProjectConfig> config;
    bool fileExists{false};
    ApplicationError error;
};

struct SnapshotEntry
{
    std::string timestamp;
    std::filesystem::path path;
};

struct SnapshotListResult
{
    std::vector<SnapshotEntry> snapshots;
    ApplicationError error;
};

class ApplicationFacade
{
public:
    [[nodiscard]] ProjectConfigLoadResult loadProjectConfig(const std::filesystem::path& projectRoot) const;
    [[nodiscard]] ApplicationError saveProjectConfig(
        const std::filesystem::path& projectRoot, const codeqa::config::ProjectConfig& config) const;
    [[nodiscard]] AnalysisFacadeResult analyzeProject(
        const FacadeAnalysisRequest& request, AnalysisProgressCallback progress = {}) const;
    [[nodiscard]] ReportDocumentResult openReport(const std::filesystem::path& path) const;
    [[nodiscard]] SnapshotListResult listSnapshots(const std::filesystem::path& projectRoot) const;
    [[nodiscard]] ReportDocumentResult openSnapshot(
        const std::filesystem::path& projectRoot, const std::string& timestamp) const;
    [[nodiscard]] ApplicationError deleteSnapshot(
        const std::filesystem::path& projectRoot, const std::string& timestamp) const;
};

} // namespace codeqa::application
