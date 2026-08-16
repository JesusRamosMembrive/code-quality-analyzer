#include "AppController.h"

#include "codeqa/application/ReportDocumentAnalysis.h"
#include "codeqa/config/DefaultConfig.h"
#include "codeqa/core/EnumJson.h"
#include "codeqa/report/MarkdownReport.h"

#include <QDir>
#include <QMetaObject>
#include <QSettings>
#include <QtConcurrentRun>

#include <algorithm>
#include <fstream>
#include <utility>

namespace
{

constexpr qsizetype maximumRecentPaths = 10;

QString phaseText(const codeqa::application::AnalysisPhase phase)
{
    switch (phase)
    {
    case codeqa::application::AnalysisPhase::DiscoveringFiles:
        return QStringLiteral("Discovering files");
    case codeqa::application::AnalysisPhase::ParsingSourceFiles:
        return QStringLiteral("Parsing source files");
    case codeqa::application::AnalysisPhase::MeasuringFunctions:
        return QStringLiteral("Measuring functions");
    case codeqa::application::AnalysisPhase::ApplyingRules:
        return QStringLiteral("Applying rules");
    case codeqa::application::AnalysisPhase::PreparingReport:
        return QStringLiteral("Preparing report");
    }
    return {};
}

QString qualityGateText(const codeqa::application::QualityGateOutcome outcome)
{
    switch (outcome)
    {
    case codeqa::application::QualityGateOutcome::Passed:
        return QStringLiteral("Passed");
    case codeqa::application::QualityGateOutcome::Failed:
        return QStringLiteral("Failed");
    case codeqa::application::QualityGateOutcome::NotEvaluated:
        return QStringLiteral("Not evaluated");
    }
    return QStringLiteral("Not evaluated");
}

QString normalizedSettingsPath(const std::filesystem::path& path)
{
    return QDir::cleanPath(QString::fromStdWString(path.wstring()));
}

bool isSupportedSeverityFilter(const QString& severity)
{
    return severity.isEmpty()
        || codeqa::core::diagnosticSeverityFromJsonString(severity.toStdString()).has_value();
}

} // namespace

AppController::AppController(QObject* parent)
    : QObject(parent),
      settings_(std::make_unique<QSettings>(QStringLiteral("CodeQA"), QStringLiteral("CodeQA Desktop")))
{
    initializeSettingsState();
    initializeConfigurationState();
    connect(&analysisWatcher_, &QFutureWatcherBase::finished, this, &AppController::finishAnalysis);
}

AppController::AppController(const QString& settingsPath, QObject* parent)
    : QObject(parent),
      settings_(std::make_unique<QSettings>(settingsPath, QSettings::IniFormat))
{
    initializeSettingsState();
    initializeConfigurationState();
    connect(&analysisWatcher_, &QFutureWatcherBase::finished, this, &AppController::finishAnalysis);
}

AppController::~AppController()
{
    // launchAnalysis() captures a raw `this` into a QtConcurrent worker and
    // its queued progress callback. This wait is what keeps that safe: it
    // guarantees the worker lambda has returned (and every synchronous call
    // it issued back into `this` has been made) before member teardown below
    // begins. Do not remove or reorder without replacing the raw capture.
    if (analysisWatcher_.isRunning())
    {
        analysisWatcher_.waitForFinished();
    }
}

bool AppController::analysisRunning() const
{
    return analysisRunning_;
}

QString AppController::analysisPhase() const
{
    return analysisPhase_;
}

QString AppController::errorMessage() const
{
    return errorMessage_;
}

QString AppController::qualityGateStatus() const
{
    return qualityGateStatus_;
}

QUrl AppController::currentProject() const
{
    return currentProjectPath_.empty()
        ? QUrl{}
        : QUrl::fromLocalFile(normalizedSettingsPath(currentProjectPath_));
}

bool AppController::configurationAvailable() const
{
    return configurationAvailable_;
}

bool AppController::configurationDirty() const
{
    return configurationDirty_;
}

QString AppController::qualityGateSeverity() const
{
    return qualityGateSeverity_;
}

RuleConfigModel* AppController::rules() const
{
    return const_cast<RuleConfigModel*>(&rules_);
}

HeaderOverrideModel* AppController::headerOverrides() const
{
    return const_cast<HeaderOverrideModel*>(&headerOverrides_);
}

FindingListModel* AppController::findings() const
{
    return const_cast<FindingListModel*>(&findings_);
}

FileListModel* AppController::files() const
{
    return const_cast<FileListModel*>(&files_);
}

SnapshotListModel* AppController::snapshots() const
{
    return const_cast<SnapshotListModel*>(&snapshots_);
}

QString AppController::lastPage() const
{
    return lastPage_;
}

QString AppController::findingSeverityFilter() const
{
    return findings_.severityFilter();
}

QString AppController::findingRuleFilter() const
{
    return findings_.ruleFilter();
}

QString AppController::findingSearchText() const
{
    return findings_.searchText();
}

QString AppController::excludePatterns() const
{
    return excludePatterns_;
}

QStringList AppController::recentProjects() const
{
    return recentProjects_;
}

QStringList AppController::recentReports() const
{
    return recentReports_;
}

void AppController::setExcludePatterns(const QString& patterns)
{
    if (excludePatterns_ == patterns)
    {
        return;
    }
    excludePatterns_ = patterns;
    emit excludePatternsChanged(excludePatterns_);
}

void AppController::setLastPage(const QString& page)
{
    const QString normalized = page.trimmed();
    if (normalized.isEmpty() || lastPage_ == normalized)
    {
        return;
    }
    lastPage_ = normalized;
    settings_->setValue(QStringLiteral("ui/lastPage"), lastPage_);
    persistSettings();
    emit lastPageChanged(lastPage_);
}

void AppController::selectProject(const QUrl& projectRoot)
{
    if (!requireIdle(QStringLiteral("selecting a project")))
    {
        return;
    }

    setCurrentProject(projectRoot);
    if (currentProjectPath_.empty())
    {
        configurationLoadError_.clear();
        applyConfiguration(codeqa::config::defaultConfig(), false);
        setConfigurationAvailable(false);
        return;
    }

    const codeqa::application::ProjectConfigLoadResult result =
        facade_.loadProjectConfig(currentProjectPath_);
    if (!result.config.has_value())
    {
        configurationLoadError_ = QString::fromStdString(
            result.error.message.value_or("Error [CONFIG007]: unable to load configuration."));
        applyConfiguration(codeqa::config::defaultConfig(), false);
        setConfigurationAvailable(false);
        setErrorMessage(configurationLoadError_);
        return;
    }

    configurationLoadError_.clear();
    applyConfiguration(std::move(*result.config), !result.fileExists);
    setConfigurationAvailable(true);
    setErrorMessage({});
}

void AppController::saveConfiguration()
{
    if (!requireEditableConfiguration(QStringLiteral("saving configuration")))
    {
        return;
    }

    const codeqa::application::ApplicationError result =
        facade_.saveProjectConfig(currentProjectPath_, currentFormConfiguration());
    if (result.hasError())
    {
        setErrorMessage(QString::fromStdString(*result.message));
        return;
    }

    setConfigurationDirty(false);
    setErrorMessage({});
}

void AppController::resetConfiguration()
{
    if (!requireIdle(QStringLiteral("resetting configuration")))
    {
        return;
    }
    if (!requireCurrentProject(QStringLiteral("resetting configuration")))
    {
        return;
    }

    applyConfiguration(codeqa::config::defaultConfig(), true);
    if (configurationAvailable_)
    {
        setErrorMessage({});
    }
    else
    {
        setErrorMessage(configurationLoadError_);
    }
}

void AppController::setQualityGateSeverity(const QString& severity)
{
    if (!requireEditableConfiguration(QStringLiteral("changing the quality gate")))
    {
        return;
    }

    const QString normalized = severity.trimmed().toLower();
    if (!codeqa::core::diagnosticSeverityFromJsonString(normalized.toStdString()).has_value())
    {
        setErrorMessage(QStringLiteral(
            "Error [DESKTOP005]: quality gate severity must be information, warning, or error."));
        return;
    }
    if (qualityGateSeverity_ != normalized)
    {
        qualityGateSeverity_ = normalized;
        emit qualityGateSeverityChanged(qualityGateSeverity_);
        setConfigurationDirty(true);
    }
    setErrorMessage({});
}

void AppController::setRuleEnabled(const QString& ruleId, const bool enabled)
{
    if (!requireEditableConfiguration(QStringLiteral("changing a rule")))
    {
        return;
    }
    if (!rules_.setEnabled(ruleId, enabled))
    {
        setErrorMessage(QStringLiteral("Error [DESKTOP006]: unknown rule '%1'.").arg(ruleId));
        return;
    }
    setErrorMessage({});
}

void AppController::setRuleLimits(
    const QString& ruleId, const qlonglong warningLimit, const qlonglong highLimit)
{
    if (!requireEditableConfiguration(QStringLiteral("changing rule limits")))
    {
        return;
    }
    if (warningLimit < 0 || highLimit < 0)
    {
        setErrorMessage(QStringLiteral("Error [DESKTOP014]: rule limits must not be negative."));
        return;
    }
    if (!rules_.setLimit(ruleId, warningLimit, highLimit))
    {
        setErrorMessage(QStringLiteral("Error [DESKTOP006]: unknown rule '%1'.").arg(ruleId));
        return;
    }
    setErrorMessage({});
}

void AppController::addHeaderOverride(const QString& pattern, const QString& language)
{
    if (!requireEditableConfiguration(QStringLiteral("adding a header override")))
    {
        return;
    }
    const std::optional<codeqa::core::Language> parsedLanguage = supportedLanguage(language);
    if (!parsedLanguage.has_value())
    {
        setErrorMessage(QStringLiteral(
            "Error [DESKTOP007]: header override language must be c or cpp."));
        return;
    }

    headerOverrides_.addOverride(
        codeqa::config::HeaderOverrideConfig{pattern.trimmed().toStdString(), *parsedLanguage});
    setErrorMessage({});
}

void AppController::updateHeaderOverride(
    const int row, const QString& pattern, const QString& language)
{
    if (!requireEditableConfiguration(QStringLiteral("changing a header override")))
    {
        return;
    }
    const std::optional<codeqa::core::Language> parsedLanguage = supportedLanguage(language);
    if (!parsedLanguage.has_value())
    {
        setErrorMessage(QStringLiteral(
            "Error [DESKTOP007]: header override language must be c or cpp."));
        return;
    }
    if (!headerOverrides_.updateOverride(
            row,
            codeqa::config::HeaderOverrideConfig{pattern.trimmed().toStdString(), *parsedLanguage}))
    {
        setErrorMessage(QStringLiteral("Error [DESKTOP008]: header override row is no longer available."));
        return;
    }
    setErrorMessage({});
}

void AppController::removeHeaderOverride(const int row)
{
    if (!requireEditableConfiguration(QStringLiteral("removing a header override")))
    {
        return;
    }
    if (!headerOverrides_.removeOverride(row))
    {
        setErrorMessage(QStringLiteral("Error [DESKTOP008]: header override row is no longer available."));
        return;
    }
    setErrorMessage({});
}

void AppController::startAnalysis()
{
    if (!requireEditableConfiguration(QStringLiteral("starting analysis")))
    {
        return;
    }
    launchAnalysis(currentFormConfiguration());
}

void AppController::openReport(const QUrl& reportUrl)
{
    if (!requireIdle(QStringLiteral("opening a report")))
    {
        return;
    }

    const std::filesystem::path path = localPath(reportUrl);
    if (path.empty())
    {
        setErrorMessage(QStringLiteral("Error [DESKTOP003]: choose a local report file."));
        return;
    }

    codeqa::application::ReportDocumentResult result = facade_.openReport(path);
    if (!result.document.has_value())
    {
        setErrorMessage(QString::fromStdString(result.error.value_or("Error [REPORT001]: unable to read report.")));
        return;
    }

    replaceDocument(std::move(*result.document));
    addRecentReport(normalizedSettingsPath(path));
}

void AppController::exportMarkdown(const QUrl& destination)
{
    if (!currentDocument_)
    {
        setErrorMessage(QStringLiteral("Error [DESKTOP010]: run or open an analysis before exporting a report."));
        return;
    }

    const std::filesystem::path path = localPath(destination);
    if (path.empty())
    {
        setErrorMessage(QStringLiteral("Error [DESKTOP011]: choose a local destination for the report."));
        return;
    }

    std::ofstream output(path);
    if (!output)
    {
        setErrorMessage(
            QStringLiteral("Error [DESKTOP012]: could not write '%1'.").arg(QString::fromStdString(path.string())));
        return;
    }

    const codeqa::application::RunAnalysisResult analysis =
        codeqa::application::toAnalysisResult(*currentDocument_);
    codeqa::report::renderMarkdown(output, analysis.model, analysis.metrics, analysis.findings);
    setErrorMessage({});
}

void AppController::openSnapshot(const QString& timestamp)
{
    if (!requireIdle(QStringLiteral("opening a snapshot")))
    {
        return;
    }
    if (!requireCurrentProject(QStringLiteral("opening a snapshot")))
    {
        return;
    }

    codeqa::application::ReportDocumentResult result =
        facade_.openSnapshot(currentProjectPath_, timestamp.toStdString());
    if (!result.document.has_value())
    {
        setErrorMessage(QString::fromStdString(result.error.value_or("Error [SNAPSHOT006]: unable to open snapshot.")));
        return;
    }

    replaceDocument(std::move(*result.document));
}

void AppController::refreshSnapshots()
{
    if (!requireIdle(QStringLiteral("refreshing snapshots")))
    {
        return;
    }
    if (!requireCurrentProject(QStringLiteral("refreshing snapshots")))
    {
        return;
    }

    codeqa::application::SnapshotListResult result = facade_.listSnapshots(currentProjectPath_);
    if (result.error.hasError())
    {
        setErrorMessage(QString::fromStdString(*result.error.message));
        return;
    }
    snapshots_.setSnapshots(std::move(result.snapshots));
    setErrorMessage(configurationLoadError_);
}

void AppController::deleteSnapshot(const QString& timestamp)
{
    if (!requireIdle(QStringLiteral("deleting a snapshot")))
    {
        return;
    }
    if (!requireCurrentProject(QStringLiteral("deleting a snapshot")))
    {
        return;
    }

    const codeqa::application::ApplicationError result =
        facade_.deleteSnapshot(currentProjectPath_, timestamp.toStdString());
    if (result.hasError())
    {
        setErrorMessage(QString::fromStdString(*result.message));
        return;
    }
    refreshSnapshots();
}

void AppController::clearRecentPaths()
{
    recentProjects_.clear();
    recentReports_.clear();
    settings_->remove(QStringLiteral("recentProjects"));
    settings_->remove(QStringLiteral("recentReports"));
    persistSettings();
    emit recentPathsChanged();
}

void AppController::setFindingSeverityFilter(const QString& severity)
{
    const QString normalized = severity.trimmed().toLower();
    if (!isSupportedSeverityFilter(normalized))
    {
        setErrorMessage(QStringLiteral(
            "Error [DESKTOP004]: severity filter must be error, warning, information, or empty."));
        return;
    }
    if (findings_.severityFilter() == normalized)
    {
        return;
    }
    findings_.setSeverityFilter(normalized);
    settings_->setValue(QStringLiteral("filters/severity"), normalized);
    persistSettings();
    emit findingSeverityFilterChanged(normalized);
}

void AppController::setFindingRuleFilter(const QString& ruleId)
{
    const QString normalized = ruleId.trimmed();
    if (findings_.ruleFilter() == normalized)
    {
        return;
    }
    findings_.setRuleFilter(normalized);
    settings_->setValue(QStringLiteral("filters/ruleId"), normalized);
    persistSettings();
    emit findingRuleFilterChanged(normalized);
}

void AppController::setFindingSearchText(const QString& searchText)
{
    const QString normalized = searchText.trimmed();
    if (findings_.searchText() == normalized)
    {
        return;
    }
    findings_.setSearchText(normalized);
    settings_->setValue(QStringLiteral("filters/searchText"), normalized);
    persistSettings();
    emit findingSearchTextChanged(normalized);
}

void AppController::startAnalysis(const QUrl& projectRoot)
{
    selectProject(projectRoot);
    startAnalysis();
}

void AppController::startAnalysis(
    const QUrl& projectRoot, codeqa::config::ProjectConfig config)
{
    if (analysisRunning_)
    {
        setErrorMessage(QStringLiteral("Error [DESKTOP002]: an analysis is already running."));
        return;
    }
    setCurrentProject(projectRoot);
    if (!requireCurrentProject(QStringLiteral("starting analysis")))
    {
        return;
    }
    launchAnalysis(std::move(config));
}

void AppController::openSnapshot(const QUrl& projectRoot, const QString& timestamp)
{
    if (analysisRunning_)
    {
        openSnapshot(timestamp);
        return;
    }
    setCurrentProject(projectRoot);
    openSnapshot(timestamp);
}

void AppController::refreshSnapshots(const QUrl& projectRoot)
{
    if (analysisRunning_)
    {
        refreshSnapshots();
        return;
    }
    setCurrentProject(projectRoot);
    refreshSnapshots();
}

void AppController::deleteSnapshot(const QUrl& projectRoot, const QString& timestamp)
{
    if (analysisRunning_)
    {
        deleteSnapshot(timestamp);
        return;
    }
    setCurrentProject(projectRoot);
    deleteSnapshot(timestamp);
}

void AppController::initializeSettingsState()
{
    recentProjects_ = settings_->value(QStringLiteral("recentProjects")).toStringList();
    recentReports_ = settings_->value(QStringLiteral("recentReports")).toStringList();
    lastPage_ = settings_->value(QStringLiteral("ui/lastPage"), QStringLiteral("Analyze")).toString();

    QString severity = settings_->value(QStringLiteral("filters/severity")).toString().trimmed().toLower();
    if (!isSupportedSeverityFilter(severity))
    {
        severity.clear();
    }
    findings_.setSeverityFilter(severity);
    findings_.setRuleFilter(settings_->value(QStringLiteral("filters/ruleId")).toString());
    findings_.setSearchText(settings_->value(QStringLiteral("filters/searchText")).toString());
}

void AppController::initializeConfigurationState()
{
    applyConfiguration(codeqa::config::defaultConfig(), false);
    connect(&rules_, &RuleConfigModel::configurationChanged, this, [this]()
    {
        setConfigurationDirty(true);
    });
    connect(&headerOverrides_, &HeaderOverrideModel::configurationChanged, this, [this]()
    {
        setConfigurationDirty(true);
    });
}

void AppController::setCurrentProject(const QUrl& projectRoot)
{
    const QUrl previous = currentProject();
    const std::filesystem::path path = localPath(projectRoot);
    if (path.empty())
    {
        currentProjectPath_.clear();
        if (currentProject() != previous)
        {
            snapshots_.setSnapshots({});
            setExcludePatterns({});
            emit currentProjectChanged(currentProject());
        }
        setErrorMessage(QStringLiteral("Error [DESKTOP001]: choose a local project folder."));
        return;
    }
    currentProjectPath_ = path;
    if (currentProject() != previous)
    {
        snapshots_.setSnapshots({});
        setExcludePatterns({});
        emit currentProjectChanged(currentProject());
    }
    addRecentProject(normalizedSettingsPath(path));
}

bool AppController::requireIdle(const QString& action)
{
    if (!analysisRunning_)
    {
        return true;
    }
    setErrorMessage(QStringLiteral("Error [DESKTOP002]: wait for the current analysis before %1.").arg(action));
    return false;
}

bool AppController::requireCurrentProject(const QString& action)
{
    if (!currentProjectPath_.empty())
    {
        return true;
    }
    setErrorMessage(QStringLiteral("Error [DESKTOP001]: select a project before %1.").arg(action));
    return false;
}

bool AppController::requireEditableConfiguration(const QString& action)
{
    if (!requireIdle(action))
    {
        return false;
    }
    if (!requireCurrentProject(action))
    {
        return false;
    }
    if (configurationAvailable_)
    {
        return true;
    }

    setErrorMessage(configurationLoadError_.isEmpty()
        ? QStringLiteral("Error [DESKTOP009]: load a valid project configuration before %1.").arg(action)
        : configurationLoadError_);
    return false;
}

std::optional<codeqa::core::Language> AppController::supportedLanguage(const QString& language) const
{
    return codeqa::core::languageFromJsonString(language.trimmed().toLower().toStdString());
}

codeqa::config::ProjectConfig AppController::currentFormConfiguration() const
{
    codeqa::config::ProjectConfig config;
    config.rules = rules_.rules();
    config.headerOverrides = headerOverrides_.overrides();
    config.qualityGate.minSeverity = codeqa::core::diagnosticSeverityFromJsonString(
        qualityGateSeverity_.toStdString()).value_or(codeqa::core::DiagnosticSeverity::Warning);
    return config;
}

void AppController::applyConfiguration(codeqa::config::ProjectConfig config, const bool dirty)
{
    rules_.setRules(config.rules);
    headerOverrides_.setOverrides(std::move(config.headerOverrides));

    const QString severity = QString::fromStdString(
        codeqa::core::diagnosticSeverityToJsonString(config.qualityGate.minSeverity));
    if (qualityGateSeverity_ != severity)
    {
        qualityGateSeverity_ = severity;
        emit qualityGateSeverityChanged(qualityGateSeverity_);
    }
    setConfigurationDirty(dirty);
}

void AppController::launchAnalysis(codeqa::config::ProjectConfig config)
{
    setErrorMessage({});
    if (!analysisPhase_.isEmpty())
    {
        analysisPhase_.clear();
        emit analysisPhaseChanged(analysisPhase_);
    }
    analysisProjectPath_ = currentProjectPath_;
    setAnalysisRunning(true);

    codeqa::application::FacadeAnalysisRequest request;
    request.projectRoot = analysisProjectPath_;
    request.config = std::move(config);
    for (const QString& pattern : excludePatterns_.split(QLatin1Char(','), Qt::SkipEmptyParts))
    {
        const QString trimmed = pattern.trimmed();
        if (!trimmed.isEmpty())
        {
            request.excludePatterns.push_back(trimmed.toStdString());
        }
    }
    const codeqa::application::ApplicationFacade facade = facade_;

    analysisWatcher_.setFuture(QtConcurrent::run([this, facade, request]()
    {
        return facade.analyzeProject(request, [this](const codeqa::application::AnalysisPhase phase)
        {
            QMetaObject::invokeMethod(this, [this, phase]()
            {
                applyAnalysisPhase(phase);
            }, Qt::QueuedConnection);
        });
    }));
}

void AppController::applyAnalysisPhase(const codeqa::application::AnalysisPhase phase)
{
    const QString next = phaseText(phase);
    if (analysisPhase_ == next)
    {
        return;
    }
    analysisPhase_ = next;
    emit analysisPhaseChanged(analysisPhase_);
}

void AppController::finishAnalysis()
{
    codeqa::application::AnalysisFacadeResult result = analysisWatcher_.result();
    const std::filesystem::path completedProjectPath = analysisProjectPath_;
    if (result.document.has_value())
    {
        codeqa::application::SnapshotListResult snapshotResult = facade_.listSnapshots(completedProjectPath);
        std::optional<std::vector<codeqa::application::SnapshotEntry>> snapshots;
        QString message = result.warning.has_value() ? QString::fromStdString(*result.warning) : QString{};
        if (snapshotResult.error.hasError())
        {
            if (!message.isEmpty())
            {
                message += QLatin1Char('\n');
            }
            message += QString::fromStdString(*snapshotResult.error.message);
        }
        else
        {
            snapshots = std::move(snapshotResult.snapshots);
        }

        replaceDocument(std::move(*result.document), std::move(snapshots), std::move(message));
        analysisProjectPath_.clear();
        setAnalysisRunning(false);
        return;
    }

    setErrorMessage(QString::fromStdString(
        result.error.message.value_or("Error [APPLICATION001]: analysis did not produce a report.")));
    analysisProjectPath_.clear();
    setAnalysisRunning(false);
}

void AppController::replaceDocument(
    codeqa::application::ReportDocument document,
    std::optional<std::vector<codeqa::application::SnapshotEntry>> snapshots,
    QString message)
{
    auto next = std::make_shared<const codeqa::application::ReportDocument>(std::move(document));
    const QString nextQualityGateStatus = qualityGateText(next->qualityGateOutcome);
    const bool qualityGateChanged = qualityGateStatus_ != nextQualityGateStatus;
    const bool errorChanged = errorMessage_ != message;

    findings_.beginDocumentReset();
    files_.beginDocumentReset();
    if (snapshots.has_value())
    {
        snapshots_.beginSnapshotsReset();
    }

    currentDocument_ = next;
    findings_.applyDocument(next);
    files_.applyDocument(next);
    if (snapshots.has_value())
    {
        snapshots_.applySnapshots(std::move(*snapshots));
    }
    qualityGateStatus_ = nextQualityGateStatus;
    errorMessage_ = std::move(message);

    if (snapshots.has_value())
    {
        snapshots_.endSnapshotsReset();
    }
    files_.endDocumentReset();
    findings_.endDocumentReset();

    if (qualityGateChanged)
    {
        emit qualityGateStatusChanged(qualityGateStatus_);
    }
    if (errorChanged)
    {
        emit errorMessageChanged(errorMessage_);
    }
    emit currentReportChanged();
}

void AppController::setAnalysisRunning(const bool running)
{
    if (analysisRunning_ == running)
    {
        return;
    }
    analysisRunning_ = running;
    emit analysisRunningChanged(analysisRunning_);
}

void AppController::setErrorMessage(const QString& message)
{
    if (errorMessage_ == message)
    {
        return;
    }
    errorMessage_ = message;
    emit errorMessageChanged(errorMessage_);
}

void AppController::persistSettings()
{
    settings_->sync();
    if (settings_->status() != QSettings::NoError)
    {
        setErrorMessage(QStringLiteral("Error [DESKTOP013]: could not save application settings."));
    }
}

void AppController::setConfigurationAvailable(const bool available)
{
    if (configurationAvailable_ == available)
    {
        return;
    }
    configurationAvailable_ = available;
    emit configurationAvailableChanged(configurationAvailable_);
}

void AppController::setConfigurationDirty(const bool dirty)
{
    if (configurationDirty_ == dirty)
    {
        return;
    }
    configurationDirty_ = dirty;
    emit configurationDirtyChanged(configurationDirty_);
}

void AppController::addRecentProject(const QString& path)
{
    addRecentPath(recentProjects_, path);
    settings_->setValue(QStringLiteral("recentProjects"), recentProjects_);
    persistSettings();
    emit recentPathsChanged();
}

void AppController::addRecentReport(const QString& path)
{
    addRecentPath(recentReports_, path);
    settings_->setValue(QStringLiteral("recentReports"), recentReports_);
    persistSettings();
    emit recentPathsChanged();
}

void AppController::addRecentPath(QStringList& paths, const QString& path)
{
    paths.removeAll(path);
    paths.prepend(path);
    while (paths.size() > maximumRecentPaths)
    {
        paths.removeLast();
    }
}

std::filesystem::path AppController::localPath(const QUrl& url)
{
    if (!url.isLocalFile())
    {
        return {};
    }
    return std::filesystem::path(url.toLocalFile().toStdWString());
}
