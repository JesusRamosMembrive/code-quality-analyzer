#include "AppController.h"

#include "codeqa/config/DefaultConfig.h"

#include <QDir>
#include <QMetaObject>
#include <QSettings>
#include <QtConcurrentRun>

#include <algorithm>
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
        || severity == QStringLiteral("error")
        || severity == QStringLiteral("warning")
        || severity == QStringLiteral("information");
}

} // namespace

AppController::AppController(QObject* parent)
    : QObject(parent),
      currentConfig_(codeqa::config::defaultConfig()),
      settings_(std::make_unique<QSettings>(QStringLiteral("CodeQA"), QStringLiteral("CodeQA Desktop")))
{
    initializeSettingsState();
    connect(&analysisWatcher_, &QFutureWatcherBase::finished, this, &AppController::finishAnalysis);
}

AppController::AppController(const QString& settingsPath, QObject* parent)
    : QObject(parent),
      currentConfig_(codeqa::config::defaultConfig()),
      settings_(std::make_unique<QSettings>(settingsPath, QSettings::IniFormat))
{
    initializeSettingsState();
    connect(&analysisWatcher_, &QFutureWatcherBase::finished, this, &AppController::finishAnalysis);
}

AppController::~AppController()
{
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

FindingListModel* AppController::findings()
{
    return &findings_;
}

FileListModel* AppController::files()
{
    return &files_;
}

SnapshotListModel* AppController::snapshots()
{
    return &snapshots_;
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

QStringList AppController::recentProjects() const
{
    return recentProjects_;
}

QStringList AppController::recentReports() const
{
    return recentReports_;
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
    settings_->sync();
    emit lastPageChanged(lastPage_);
}

void AppController::startAnalysis()
{
    if (analysisRunning_)
    {
        setErrorMessage(QStringLiteral("Error [DESKTOP002]: an analysis is already running."));
        return;
    }
    if (!requireCurrentProject(QStringLiteral("starting analysis")))
    {
        return;
    }

    setErrorMessage({});
    if (!analysisPhase_.isEmpty())
    {
        analysisPhase_.clear();
        emit analysisPhaseChanged(analysisPhase_);
    }
    setAnalysisRunning(true);

    codeqa::application::FacadeAnalysisRequest request;
    request.projectRoot = currentProjectPath_;
    request.config = currentConfig_;
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

void AppController::openReport(const QUrl& reportUrl)
{
    if (analysisRunning_)
    {
        setErrorMessage(QStringLiteral("Error [DESKTOP002]: wait for the current analysis before opening a report."));
        return;
    }

    const std::filesystem::path path = localPath(reportUrl);
    if (path.empty())
    {
        setErrorMessage(QStringLiteral("Error [DESKTOP003]: choose a local report file."));
        return;
    }

    const codeqa::application::ReportDocumentResult result = facade_.openReport(path);
    if (!result.document.has_value())
    {
        setErrorMessage(QString::fromStdString(result.error.value_or("Error [REPORT001]: unable to read report.")));
        return;
    }

    replaceDocument(std::move(*result.document));
    addRecentReport(normalizedSettingsPath(path));
    setErrorMessage({});
}

void AppController::openSnapshot(const QString& timestamp)
{
    if (analysisRunning_)
    {
        setErrorMessage(QStringLiteral("Error [DESKTOP002]: wait for the current analysis before opening a snapshot."));
        return;
    }
    if (!requireCurrentProject(QStringLiteral("opening a snapshot")))
    {
        return;
    }

    const codeqa::application::ReportDocumentResult result =
        facade_.openSnapshot(currentProjectPath_, timestamp.toStdString());
    if (!result.document.has_value())
    {
        setErrorMessage(QString::fromStdString(result.error.value_or("Error [SNAPSHOT006]: unable to open snapshot.")));
        return;
    }

    replaceDocument(std::move(*result.document));
    setErrorMessage({});
}

void AppController::refreshSnapshots()
{
    if (analysisRunning_)
    {
        setErrorMessage(QStringLiteral("Error [DESKTOP002]: wait for the current analysis before refreshing snapshots."));
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
    setErrorMessage({});
}

void AppController::deleteSnapshot(const QString& timestamp)
{
    if (analysisRunning_)
    {
        setErrorMessage(QStringLiteral("Error [DESKTOP002]: wait for the current analysis before deleting a snapshot."));
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
    settings_->sync();
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
    settings_->sync();
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
    settings_->sync();
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
    settings_->sync();
    emit findingSearchTextChanged(normalized);
}

void AppController::startAnalysis(const QUrl& projectRoot)
{
    setCurrentProject(projectRoot);
    startAnalysis();
}

void AppController::startAnalysis(
    const QUrl& projectRoot, codeqa::config::ProjectConfig config)
{
    setCurrentProject(projectRoot);
    currentConfig_ = std::move(config);
    startAnalysis();
}

void AppController::openSnapshot(const QUrl& projectRoot, const QString& timestamp)
{
    setCurrentProject(projectRoot);
    openSnapshot(timestamp);
}

void AppController::refreshSnapshots(const QUrl& projectRoot)
{
    setCurrentProject(projectRoot);
    refreshSnapshots();
}

void AppController::deleteSnapshot(const QUrl& projectRoot, const QString& timestamp)
{
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

void AppController::setCurrentProject(const QUrl& projectRoot)
{
    const std::filesystem::path path = localPath(projectRoot);
    if (path.empty())
    {
        currentProjectPath_.clear();
        setErrorMessage(QStringLiteral("Error [DESKTOP001]: choose a local project folder."));
        return;
    }
    currentProjectPath_ = path;
    addRecentProject(normalizedSettingsPath(path));
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
    const codeqa::application::AnalysisFacadeResult result = analysisWatcher_.result();
    if (result.document.has_value())
    {
        replaceDocument(*result.document);
        setAnalysisRunning(false);
        refreshSnapshots();
        setErrorMessage(result.warning.has_value() ? QString::fromStdString(*result.warning) : QString{});
        return;
    }

    setErrorMessage(QString::fromStdString(
        result.error.message.value_or("Error [APPLICATION001]: analysis did not produce a report.")));
    setAnalysisRunning(false);
}

void AppController::replaceDocument(codeqa::application::ReportDocument document)
{
    auto next = std::make_shared<const codeqa::application::ReportDocument>(std::move(document));
    currentDocument_ = next;
    findings_.setDocument(next);
    files_.setDocument(next);
    setQualityGateStatus(qualityGateText(next->qualityGateOutcome));
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

void AppController::setQualityGateStatus(const QString& status)
{
    if (qualityGateStatus_ == status)
    {
        return;
    }
    qualityGateStatus_ = status;
    emit qualityGateStatusChanged(qualityGateStatus_);
}

void AppController::addRecentProject(const QString& path)
{
    addRecentPath(recentProjects_, path);
    settings_->setValue(QStringLiteral("recentProjects"), recentProjects_);
    settings_->sync();
    emit recentPathsChanged();
}

void AppController::addRecentReport(const QString& path)
{
    addRecentPath(recentReports_, path);
    settings_->setValue(QStringLiteral("recentReports"), recentReports_);
    settings_->sync();
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
