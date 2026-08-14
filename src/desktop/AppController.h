#pragma once

#include "ReportModels.h"

#include "codeqa/application/ApplicationFacade.h"
#include "codeqa/config/ConfigModels.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

class QSettings;

class AppController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool analysisRunning READ analysisRunning NOTIFY analysisRunningChanged)
    Q_PROPERTY(QString analysisPhase READ analysisPhase NOTIFY analysisPhaseChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString qualityGateStatus READ qualityGateStatus NOTIFY qualityGateStatusChanged)
    Q_PROPERTY(QUrl currentProject READ currentProject NOTIFY currentProjectChanged)
    Q_PROPERTY(bool configurationAvailable READ configurationAvailable NOTIFY configurationAvailableChanged)
    Q_PROPERTY(bool configurationDirty READ configurationDirty NOTIFY configurationDirtyChanged)
    Q_PROPERTY(QString qualityGateSeverity READ qualityGateSeverity NOTIFY qualityGateSeverityChanged)
    Q_PROPERTY(RuleConfigModel* rules READ rules CONSTANT)
    Q_PROPERTY(HeaderOverrideModel* headerOverrides READ headerOverrides CONSTANT)
    Q_PROPERTY(FindingListModel* findings READ findings CONSTANT)
    Q_PROPERTY(FileListModel* files READ files CONSTANT)
    Q_PROPERTY(SnapshotListModel* snapshots READ snapshots CONSTANT)
    Q_PROPERTY(QString lastPage READ lastPage WRITE setLastPage NOTIFY lastPageChanged)
    Q_PROPERTY(QString findingSeverityFilter READ findingSeverityFilter NOTIFY findingSeverityFilterChanged)
    Q_PROPERTY(QString findingRuleFilter READ findingRuleFilter NOTIFY findingRuleFilterChanged)
    Q_PROPERTY(QString findingSearchText READ findingSearchText NOTIFY findingSearchTextChanged)

public:
    explicit AppController(QObject* parent = nullptr);
    explicit AppController(const QString& settingsPath, QObject* parent = nullptr);
    ~AppController() override;

    [[nodiscard]] bool analysisRunning() const;
    [[nodiscard]] QString analysisPhase() const;
    [[nodiscard]] QString errorMessage() const;
    [[nodiscard]] QString qualityGateStatus() const;
    [[nodiscard]] QUrl currentProject() const;
    [[nodiscard]] bool configurationAvailable() const;
    [[nodiscard]] bool configurationDirty() const;
    [[nodiscard]] QString qualityGateSeverity() const;
    [[nodiscard]] RuleConfigModel* rules();
    [[nodiscard]] HeaderOverrideModel* headerOverrides();
    [[nodiscard]] FindingListModel* findings();
    [[nodiscard]] FileListModel* files();
    [[nodiscard]] SnapshotListModel* snapshots();
    [[nodiscard]] QString lastPage() const;
    [[nodiscard]] QString findingSeverityFilter() const;
    [[nodiscard]] QString findingRuleFilter() const;
    [[nodiscard]] QString findingSearchText() const;
    [[nodiscard]] QStringList recentProjects() const;
    [[nodiscard]] QStringList recentReports() const;

    void setLastPage(const QString& page);

    Q_INVOKABLE void selectProject(const QUrl& projectRoot);
    Q_INVOKABLE void saveConfiguration();
    Q_INVOKABLE void resetConfiguration();
    Q_INVOKABLE void setQualityGateSeverity(const QString& severity);
    Q_INVOKABLE void setRuleEnabled(const QString& ruleId, bool enabled);
    Q_INVOKABLE void setRuleLimits(const QString& ruleId, qlonglong warningLimit, qlonglong highLimit);
    Q_INVOKABLE void addHeaderOverride(const QString& pattern, const QString& language);
    Q_INVOKABLE void updateHeaderOverride(int row, const QString& pattern, const QString& language);
    Q_INVOKABLE void removeHeaderOverride(int row);
    Q_INVOKABLE void startAnalysis();
    Q_INVOKABLE void openReport(const QUrl& reportUrl);
    Q_INVOKABLE void openSnapshot(const QString& timestamp);
    Q_INVOKABLE void refreshSnapshots();
    Q_INVOKABLE void deleteSnapshot(const QString& timestamp);
    Q_INVOKABLE void clearRecentPaths();
    Q_INVOKABLE void setFindingSeverityFilter(const QString& severity);
    Q_INVOKABLE void setFindingRuleFilter(const QString& ruleId);
    Q_INVOKABLE void setFindingSearchText(const QString& searchText);

    void startAnalysis(const QUrl& projectRoot);
    void startAnalysis(const QUrl& projectRoot, codeqa::config::ProjectConfig config);
    void openSnapshot(const QUrl& projectRoot, const QString& timestamp);
    void refreshSnapshots(const QUrl& projectRoot);
    void deleteSnapshot(const QUrl& projectRoot, const QString& timestamp);

signals:
    void analysisRunningChanged(bool running);
    void analysisPhaseChanged(const QString& phase);
    void errorMessageChanged(const QString& message);
    void qualityGateStatusChanged(const QString& status);
    void currentProjectChanged(const QUrl& projectRoot);
    void configurationAvailableChanged(bool available);
    void configurationDirtyChanged(bool dirty);
    void qualityGateSeverityChanged(const QString& severity);
    void currentReportChanged();
    void lastPageChanged(const QString& page);
    void findingSeverityFilterChanged(const QString& severity);
    void findingRuleFilterChanged(const QString& ruleId);
    void findingSearchTextChanged(const QString& searchText);
    void recentPathsChanged();

private:
    void initializeSettingsState();
    void initializeConfigurationState();
    void setCurrentProject(const QUrl& projectRoot);
    [[nodiscard]] bool requireCurrentProject(const QString& action);
    [[nodiscard]] bool requireEditableConfiguration(const QString& action);
    [[nodiscard]] std::optional<codeqa::core::Language> supportedLanguage(const QString& language) const;
    [[nodiscard]] codeqa::config::ProjectConfig currentFormConfiguration() const;
    void applyConfiguration(codeqa::config::ProjectConfig config, bool dirty);
    void launchAnalysis(codeqa::config::ProjectConfig config);
    void applyAnalysisPhase(codeqa::application::AnalysisPhase phase);
    void finishAnalysis();
    void replaceDocument(
        codeqa::application::ReportDocument document,
        std::optional<std::vector<codeqa::application::SnapshotEntry>> snapshots = std::nullopt,
        QString message = {});
    void setAnalysisRunning(bool running);
    void setErrorMessage(const QString& message);
    void setConfigurationAvailable(bool available);
    void setConfigurationDirty(bool dirty);
    void addRecentProject(const QString& path);
    void addRecentReport(const QString& path);
    static void addRecentPath(QStringList& paths, const QString& path);
    [[nodiscard]] static std::filesystem::path localPath(const QUrl& url);

    codeqa::application::ApplicationFacade facade_;
    std::filesystem::path currentProjectPath_;
    std::filesystem::path analysisProjectPath_;
    std::shared_ptr<const codeqa::application::ReportDocument> currentDocument_;
    RuleConfigModel rules_;
    HeaderOverrideModel headerOverrides_;
    FindingListModel findings_;
    FileListModel files_;
    SnapshotListModel snapshots_;
    QFutureWatcher<codeqa::application::AnalysisFacadeResult> analysisWatcher_;
    std::unique_ptr<QSettings> settings_;
    QStringList recentProjects_;
    QStringList recentReports_;
    QString lastPage_;
    QString analysisPhase_;
    QString errorMessage_;
    QString qualityGateStatus_{QStringLiteral("Not evaluated")};
    QString qualityGateSeverity_{QStringLiteral("warning")};
    QString configurationLoadError_;
    bool analysisRunning_{false};
    bool configurationAvailable_{false};
    bool configurationDirty_{false};
};
