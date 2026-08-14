#include "codeqa/application/ApplicationFacade.h"

#include "codeqa/application/DeleteSnapshot.h"
#include "codeqa/application/GetApplicationInfo.h"
#include "codeqa/application/ListSnapshots.h"
#include "codeqa/application/LoadProjectConfig.h"
#include "codeqa/application/LoadSnapshot.h"
#include "codeqa/application/RunAnalysis.h"
#include "codeqa/application/SaveProjectConfig.h"
#include "codeqa/application/SaveSnapshot.h"
#include "codeqa/config/DefaultConfig.h"
#include "codeqa/history/CompareSnapshots.h"
#include "codeqa/report/ReportJson.h"

#include <exception>
#include <system_error>
#include <utility>

namespace codeqa::application
{

namespace
{

void appendWarning(std::optional<std::string>& warning, const std::string& message)
{
    if (warning.has_value())
    {
        *warning += '\n';
        *warning += message;
        return;
    }
    warning = message;
}

LanguageOverride toLanguageOverride(const codeqa::config::HeaderOverrideConfig& headerOverride)
{
    return LanguageOverride{headerOverride.pattern, headerOverride.language};
}

ReportComparison toReportComparison(
    const std::string& previousTimestamp, const codeqa::history::SnapshotComparison& comparison)
{
    return ReportComparison{
        previousTimestamp, comparison.newCount, comparison.existingCount, comparison.resolvedCount};
}

} // namespace

ProjectConfigLoadResult ApplicationFacade::loadProjectConfig(const std::filesystem::path& projectRoot) const
{
    const std::filesystem::path path = projectRoot / "codeqa.json";
    std::error_code existsError;
    const bool exists = std::filesystem::exists(path, existsError);

    if (existsError)
    {
        return ProjectConfigLoadResult{
            std::nullopt,
            true,
            ApplicationError{"Error [CONFIG007]: could not inspect configuration file '" + path.string() + "'."}};
    }
    if (!exists)
    {
        return ProjectConfigLoadResult{codeqa::config::defaultConfig(), false, {}};
    }

    const LoadProjectConfigResult loaded = LoadProjectConfig{}.execute(path);
    if (!loaded.config.has_value())
    {
        return ProjectConfigLoadResult{std::nullopt, true, ApplicationError{loaded.error}};
    }
    return ProjectConfigLoadResult{loaded.config, true, {}};
}

ApplicationError ApplicationFacade::saveProjectConfig(
    const std::filesystem::path& projectRoot, const codeqa::config::ProjectConfig& config) const
{
    return ApplicationError{SaveProjectConfig{}.execute(projectRoot, config)};
}

AnalysisFacadeResult ApplicationFacade::analyzeProject(
    const FacadeAnalysisRequest& request, const AnalysisProgressCallback progress) const
{
    AnalysisFacadeResult result;

    try
    {
        RunAnalysisRequest analysisRequest;
        analysisRequest.analysis.discovery.rootPath = request.projectRoot;
        analysisRequest.analysis.discovery.excludePatterns = request.excludePatterns;
        for (const codeqa::config::HeaderOverrideConfig& headerOverride : request.config.headerOverrides)
        {
            analysisRequest.analysis.languageOverrides.push_back(toLanguageOverride(headerOverride));
        }
        analysisRequest.analysis.languageOverrides.insert(
            analysisRequest.analysis.languageOverrides.end(),
            request.languageOverrides.begin(),
            request.languageOverrides.end());
        analysisRequest.config = request.config;

        const RunAnalysisResult analysis = RunAnalysis{}.execute(analysisRequest, progress);

        const std::vector<std::string> previousSnapshots = ListSnapshots{}.execute(request.projectRoot);
        codeqa::history::FindingKeySet previousKeys;
        if (!previousSnapshots.empty())
        {
            const std::string& previousTimestamp = previousSnapshots.back();
            const std::optional<nlohmann::json> previousReport =
                LoadSnapshot{}.execute(request.projectRoot, previousTimestamp);

            if (!previousReport.has_value())
            {
                appendWarning(
                    result.warning,
                    "Warning: previous snapshot '" + previousTimestamp
                        + "' could not be read; skipping comparison.");
            }
            else
            {
                try
                {
                    previousKeys = codeqa::history::extractFindingKeys(*previousReport);
                    const codeqa::history::SnapshotComparison comparison = codeqa::history::compareSnapshots(
                        codeqa::history::extractFindingKeys(analysis.findings), previousKeys);
                    result.comparison = toReportComparison(previousTimestamp, comparison);
                }
                catch (const nlohmann::json::exception&)
                {
                    previousKeys.clear();
                    appendWarning(
                        result.warning,
                        "Warning: previous snapshot '" + previousTimestamp
                            + "' has an unexpected shape; skipping comparison.");
                }
            }
        }

        const bool qualityGateFailed = codeqa::history::hasNewFindingAtOrAboveSeverity(
            analysis.findings, previousKeys, request.config.qualityGate.minSeverity);
        result.qualityGateOutcome =
            qualityGateFailed ? QualityGateOutcome::Failed : QualityGateOutcome::Passed;

        if (progress)
        {
            progress(AnalysisPhase::PreparingReport);
        }

        const std::string toolVersion = GetApplicationInfo{}.execute().version;
        result.reportJson = codeqa::report::toReportJson(
            analysis.model, analysis.metrics, analysis.findings, request.config, toolVersion);
        result.document = makeReportDocument(
            analysis, request.config, toolVersion, result.qualityGateOutcome, result.comparison);

        result.savedSnapshotPath = SaveSnapshot{}.execute(request.projectRoot, *result.reportJson);
        if (!result.savedSnapshotPath.has_value())
        {
            appendWarning(result.warning, "Warning: could not save snapshot.");
        }
    }
    catch (const std::exception& error)
    {
        result.error.message = std::string("Error [APPLICATION001]: analysis failed: ") + error.what();
    }

    return result;
}

ReportDocumentResult ApplicationFacade::openReport(const std::filesystem::path& path) const
{
    return loadReportDocument(path);
}

SnapshotListResult ApplicationFacade::listSnapshots(const std::filesystem::path& projectRoot) const
{
    SnapshotListResult result;
    const std::vector<std::string> timestamps = ListSnapshots{}.execute(projectRoot);
    const std::filesystem::path snapshotsDirectory = projectRoot / ".codeqa" / "snapshots";

    for (auto timestamp = timestamps.rbegin(); timestamp != timestamps.rend(); ++timestamp)
    {
        result.snapshots.push_back(SnapshotEntry{*timestamp, snapshotsDirectory / (*timestamp + ".json")});
    }
    return result;
}

ReportDocumentResult ApplicationFacade::openSnapshot(
    const std::filesystem::path& projectRoot, const std::string& timestamp) const
{
    if (!isValidSnapshotTimestamp(timestamp))
    {
        return ReportDocumentResult{
            std::nullopt,
            "Error [SNAPSHOT001]: invalid snapshot timestamp '" + timestamp + "'."};
    }

    const std::optional<nlohmann::json> snapshot = LoadSnapshot{}.execute(projectRoot, timestamp);
    if (!snapshot.has_value())
    {
        return ReportDocumentResult{
            std::nullopt,
            "Error [SNAPSHOT005]: snapshot '" + timestamp + "' could not be read."};
    }

    ReportDocumentResult result = parseReportDocument(*snapshot);
    if (result.document.has_value())
    {
        result.document->qualityGateOutcome = QualityGateOutcome::NotEvaluated;
        result.document->comparison.reset();
    }
    return result;
}

ApplicationError ApplicationFacade::deleteSnapshot(
    const std::filesystem::path& projectRoot, const std::string& timestamp) const
{
    return ApplicationError{DeleteSnapshot{}.execute(projectRoot, timestamp)};
}

} // namespace codeqa::application
