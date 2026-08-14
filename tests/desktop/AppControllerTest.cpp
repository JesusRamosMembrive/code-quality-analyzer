#include "AppController.h"
#include "ReportModels.h"

#include "codeqa/config/ConfigJson.h"
#include "codeqa/config/DefaultConfig.h"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace
{

struct FindingSpec
{
    std::string path;
    std::string severity;
    std::string ruleId;
    std::string symbolName;
    std::size_t line;
    std::size_t cyclomaticComplexity{12};
};

nlohmann::json functionMetricsJson(
    const std::string& symbolName,
    const std::size_t line,
    const std::size_t cyclomaticComplexity)
{
    return {
        {"symbol",
         {{"name", symbolName},
          {"qualifiedName", "sample::" + symbolName},
          {"range",
           {{"begin", {{"line", line}, {"column", 1}}},
            {"end", {{"line", line + 3}, {"column", 1}}}}}}},
        {"size",
         {{"physicalLines", 4},
          {"codeLines", 3},
          {"blankLines", 0},
          {"commentLines", 1},
          {"statementCount", 2}}},
        {"signature",
         {{"parameterCount", 2},
          {"signatureLength", 18},
          {"booleanParameterCount", 0}}},
        {"complexity",
         {{"cyclomaticComplexity", cyclomaticComplexity},
          {"maxConditionComplexity", 3},
          {"maxNestingDepth", 2},
          {"branchCount", 4},
          {"returnCount", 1}}}};
}

nlohmann::json reportJson(const std::filesystem::path& root, const std::vector<FindingSpec>& findings)
{
    nlohmann::json report{
        {"schemaVersion", 1},
        {"metadata",
         {{"rootPath", root.generic_string()},
          {"toolName", "codeqa"},
          {"toolVersion", "9.9.9"}}},
        {"config", codeqa::config::toJson(codeqa::config::defaultConfig())},
        {"files", nlohmann::json::array()}};

    std::map<std::string, std::vector<FindingSpec>> byFile;
    for (const FindingSpec& finding : findings)
    {
        byFile[finding.path].push_back(finding);
    }

    for (const auto& [path, fileFindings] : byFile)
    {
        nlohmann::json file{
            {"path", path},
            {"language", "cpp"},
            {"state", "complete"},
            {"diagnostics", nlohmann::json::array()},
            {"metrics", nlohmann::json::array()},
            {"findings", nlohmann::json::array()}};

        for (const FindingSpec& finding : fileFindings)
        {
            file["metrics"].push_back(functionMetricsJson(
                finding.symbolName, finding.line, finding.cyclomaticComplexity));
            file["findings"].push_back({
                {"ruleId", finding.ruleId},
                {"severity", finding.severity},
                {"description", "description for " + finding.ruleId},
                {"recommendation", "recommendation for " + finding.ruleId},
                {"observedValue", 12},
                {"limitValue", 10},
                {"location", {{"line", finding.line}, {"column", 3}}},
                {"symbolName", finding.symbolName}});
        }
        report["files"].push_back(std::move(file));
    }

    return report;
}

QString writeReport(
    const QTemporaryDir& directory,
    const QString& name,
    const std::vector<FindingSpec>& findings)
{
    const QString path = directory.filePath(name);
    std::ofstream output(std::filesystem::path(path.toStdWString()));
    output << reportJson(std::filesystem::path(directory.path().toStdWString()), findings).dump(2);
    return path;
}

std::vector<FindingSpec> oneFinding(const std::string& ruleId = "complexity.cyclomatic")
{
    return {{"src/sample.cpp", "warning", ruleId, "run", 7}};
}

QString settingsPath(const QTemporaryDir& directory)
{
    return directory.filePath("settings.ini");
}

void writeTextFile(const QString& path, const QByteArray& contents)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(contents), contents.size());
}

} // namespace

class AppControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void selectingProjectLoadsSemanticConfiguration();
    void missingConfigurationUsesUnsavedDefaultsWithoutWriting();
    void invalidConfigurationIsReportedAndNeverOverwritten();
    void saveConfigurationWritesOnlyExplicitValidatedForm();
    void invalidFormDoesNotOverwriteExistingConfiguration();
    void resetConfigurationChangesOnlyTheForm();
    void headerOverrideLanguagesAreConstrained();
    void analysisUsesUnsavedFormValuesButDoesNotWriteConfiguration();
    void invalidUnsavedFormAnalysisHasNoProgressReportOrFilesystemEffects();
    void successfulImportReplacesTheCurrentReport();
    void invalidImportKeepsTheCurrentReport();
    void malformedImportKeepsTheCurrentReport();
    void findingsExposeRolesAndSortBySeverityThenLocation();
    void summaryCountsTrackDocumentsAndFilteredFindings();
    void overloadedFindingsUseMetricsFromTheirOwnSourceRange();
    void replacementPreservesPersistedFindingFilters();
    void urlSnapshotActionDuringAnalysisCannotChangeProjectOrCompletionSnapshots();
    void analysisReportsRealPhasesAndFailedQualityGate();
    void analysisModelResetSeesOneConsistentCompletedState();
    void idleSignalCanStartTheNextAnalysisAfterCompletionIsPublished();
    void workerAnalysisFailureKeepsTheCurrentReport();
    void emptyProjectAnalysisIsActionableAndKeepsTheCurrentReport();
    void selectingAnotherProjectClearsSnapshotsFromPreviousProject();
    void snapshotRefreshKeepsInvalidConfigurationDiagnostic();
    void historyOpensWithoutCreatingAnotherSnapshot();
    void snapshotOpenFailureKeepsTheCurrentReport();
    void snapshotDeleteFailureKeepsTheCurrentReport();
    void snapshotsAreNewestFirstAndDeleteRefreshesTheModel();
    void clearRecentPathsRetainsTheCurrentReport();
};

void AppControllerTest::selectingProjectLoadsSemanticConfiguration()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.rules.at("complexity.cyclomatic").enabled = false;
    config.rules.at("complexity.cyclomatic").warningLimit = 6;
    config.rules.at("complexity.cyclomatic").highLimit = 9;
    config.headerOverrides.push_back({"include/**/*.h", codeqa::core::Language::Cpp});
    config.qualityGate.minSeverity = codeqa::core::DiagnosticSeverity::Error;
    writeTextFile(
        directory.filePath("codeqa.json"),
        QByteArray::fromStdString(codeqa::config::toJson(config).dump(2)));
    AppController controller(settingsPath(directory));

    controller.selectProject(QUrl::fromLocalFile(directory.path()));

    QCOMPARE(controller.currentProject(), QUrl::fromLocalFile(directory.path()));
    QVERIFY(controller.configurationAvailable());
    QVERIFY(!controller.configurationDirty());
    QCOMPARE(controller.errorMessage(), QString{});
    QCOMPARE(controller.rules()->rowCount(), 8);
    const QStringList expectedRuleIds = {
        "size.function-length",
        "size.local-variables",
        "signature.parameter-count",
        "signature.consecutive-booleans",
        "complexity.cyclomatic",
        "complexity.returns",
        "complexity.condition",
        "complexity.nesting"};
    for (int row = 0; row < expectedRuleIds.size(); ++row)
    {
        QCOMPARE(
            controller.rules()->data(controller.rules()->index(row), RuleConfigModel::IdRole).toString(),
            expectedRuleIds.at(row));
    }
    const QModelIndex cyclomatic = controller.rules()->index(4);
    QCOMPARE(controller.rules()->data(cyclomatic, RuleConfigModel::EnabledRole).toBool(), false);
    QCOMPARE(controller.rules()->data(cyclomatic, RuleConfigModel::WarningLimitRole).toLongLong(), 6LL);
    QCOMPARE(controller.rules()->data(cyclomatic, RuleConfigModel::HighLimitRole).toLongLong(), 9LL);
    QCOMPARE(controller.headerOverrides()->rowCount(), 1);
    QCOMPARE(
        controller.headerOverrides()->data(
            controller.headerOverrides()->index(0), HeaderOverrideModel::PatternRole).toString(),
        QString("include/**/*.h"));
    QCOMPARE(
        controller.headerOverrides()->data(
            controller.headerOverrides()->index(0), HeaderOverrideModel::LanguageRole).toString(),
        QString("cpp"));
    QCOMPARE(controller.qualityGateSeverity(), QString("error"));
}

void AppControllerTest::missingConfigurationUsesUnsavedDefaultsWithoutWriting()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    AppController controller(settingsPath(directory));

    controller.selectProject(QUrl::fromLocalFile(directory.path()));

    QVERIFY(controller.configurationAvailable());
    QVERIFY(controller.configurationDirty());
    QCOMPARE(controller.rules()->rowCount(), 8);
    QCOMPARE(
        controller.rules()->data(controller.rules()->index(0), RuleConfigModel::WarningLimitRole).toLongLong(),
        50LL);
    QCOMPARE(controller.qualityGateSeverity(), QString("warning"));
    QVERIFY(!QFile::exists(directory.filePath("codeqa.json")));
}

void AppControllerTest::invalidConfigurationIsReportedAndNeverOverwritten()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QByteArray invalid = "{\n  \"rules\": { \"unknown.rule\": {} }\n}\n";
    writeTextFile(directory.filePath("codeqa.json"), invalid);
    AppController controller(settingsPath(directory));

    controller.selectProject(QUrl::fromLocalFile(directory.path()));
    QVERIFY(!controller.configurationAvailable());
    QVERIFY(controller.errorMessage().contains("CONFIG003"));
    controller.resetConfiguration();
    controller.saveConfiguration();

    QFile source(directory.filePath("codeqa.json"));
    QVERIFY(source.open(QIODevice::ReadOnly));
    QCOMPARE(source.readAll(), invalid);
    QVERIFY(controller.errorMessage().contains("CONFIG003"));
}

void AppControllerTest::saveConfigurationWritesOnlyExplicitValidatedForm()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    AppController controller(settingsPath(directory));
    controller.selectProject(QUrl::fromLocalFile(directory.path()));
    controller.setRuleEnabled("complexity.cyclomatic", false);
    controller.setRuleLimits("complexity.cyclomatic", 6, 9);
    controller.addHeaderOverride("include/**/*.h", "cpp");
    controller.setQualityGateSeverity("information");

    QVERIFY(!QFile::exists(directory.filePath("codeqa.json")));
    controller.saveConfiguration();

    QVERIFY(QFile::exists(directory.filePath("codeqa.json")));
    const codeqa::application::ProjectConfigLoadResult loaded =
        codeqa::application::ApplicationFacade{}.loadProjectConfig(
            std::filesystem::path(directory.path().toStdWString()));
    QVERIFY(loaded.config.has_value());
    QVERIFY(!loaded.error.hasError());
    const auto& cyclomatic = loaded.config->rules.at("complexity.cyclomatic");
    QVERIFY(!cyclomatic.enabled);
    QCOMPARE(cyclomatic.warningLimit, 6LL);
    QCOMPARE(cyclomatic.highLimit, 9LL);
    QCOMPARE(loaded.config->headerOverrides.size(), std::size_t{1});
    QCOMPARE(loaded.config->headerOverrides.front().pattern, std::string("include/**/*.h"));
    QCOMPARE(loaded.config->headerOverrides.front().language, codeqa::core::Language::Cpp);
    QCOMPARE(
        loaded.config->qualityGate.minSeverity,
        codeqa::core::DiagnosticSeverity::Information);
    QVERIFY(!controller.configurationDirty());
    QCOMPARE(controller.errorMessage(), QString{});
}

void AppControllerTest::invalidFormDoesNotOverwriteExistingConfiguration()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QByteArray original = QByteArray::fromStdString(
        codeqa::config::toJson(codeqa::config::defaultConfig()).dump(2));
    writeTextFile(directory.filePath("codeqa.json"), original);
    AppController controller(settingsPath(directory));
    controller.selectProject(QUrl::fromLocalFile(directory.path()));

    controller.setRuleLimits("complexity.cyclomatic", 20, 10);
    controller.saveConfiguration();

    QFile source(directory.filePath("codeqa.json"));
    QVERIFY(source.open(QIODevice::ReadOnly));
    QCOMPARE(source.readAll(), original);
    QVERIFY(controller.configurationDirty());
    QVERIFY(controller.errorMessage().contains("CONFIG008"));
}

void AppControllerTest::resetConfigurationChangesOnlyTheForm()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.rules.at("size.function-length").warningLimit = 12;
    config.rules.at("size.function-length").highLimit = 20;
    const QByteArray original = QByteArray::fromStdString(codeqa::config::toJson(config).dump(2));
    writeTextFile(directory.filePath("codeqa.json"), original);
    AppController controller(settingsPath(directory));
    controller.selectProject(QUrl::fromLocalFile(directory.path()));

    controller.resetConfiguration();

    QCOMPARE(
        controller.rules()->data(controller.rules()->index(0), RuleConfigModel::WarningLimitRole).toLongLong(),
        50LL);
    QVERIFY(controller.configurationDirty());
    QFile source(directory.filePath("codeqa.json"));
    QVERIFY(source.open(QIODevice::ReadOnly));
    QCOMPARE(source.readAll(), original);
}

void AppControllerTest::headerOverrideLanguagesAreConstrained()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    AppController controller(settingsPath(directory));
    controller.selectProject(QUrl::fromLocalFile(directory.path()));

    controller.addHeaderOverride("include/**/*.h", "rust");

    QCOMPARE(controller.headerOverrides()->rowCount(), 0);
    QVERIFY(controller.errorMessage().contains("must be c or cpp"));
    controller.addHeaderOverride("include/**/*.h", "c");
    QCOMPARE(controller.headerOverrides()->rowCount(), 1);
    QCOMPARE(controller.errorMessage(), QString{});
}

void AppControllerTest::analysisUsesUnsavedFormValuesButDoesNotWriteConfiguration()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTextFile(
        directory.filePath("sample.cpp"),
        "int classify(int value)\n"
        "{\n"
        "    if (value > 0)\n"
        "    {\n"
        "        return 1;\n"
        "    }\n"
        "    return 0;\n"
        "}\n");
    AppController controller(settingsPath(directory));
    controller.selectProject(QUrl::fromLocalFile(directory.path()));
    controller.rules()->setLimit("complexity.cyclomatic", 1, 3);

    controller.startAnalysis();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.analysisRunning(), 30000);

    QVERIFY(!QFile::exists(directory.filePath("codeqa.json")));
    QCOMPARE(controller.findings()->rowCount(), 1);
    QCOMPARE(
        controller.findings()->data(controller.findings()->index(0), FindingListModel::RuleIdRole).toString(),
        QString("complexity.cyclomatic"));
}

void AppControllerTest::invalidUnsavedFormAnalysisHasNoProgressReportOrFilesystemEffects()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTextFile(
        directory.filePath("sample.cpp"),
        "int sample()\n"
        "{\n"
        "    return 0;\n"
        "}\n");
    AppController controller(settingsPath(directory));
    controller.selectProject(QUrl::fromLocalFile(directory.path()));
    controller.setRuleLimits("complexity.cyclomatic", 20, 10);

    controller.startAnalysis();

    QTRY_VERIFY_WITH_TIMEOUT(!controller.analysisRunning(), 30000);
    QCOMPARE(controller.analysisPhase(), QString{});
    QCOMPARE(controller.findings()->rowCount(), 0);
    QCOMPARE(controller.files()->rowCount(), 0);
    QCOMPARE(controller.snapshots()->rowCount(), 0);
    QVERIFY(controller.errorMessage().contains("CONFIG008"));
    QVERIFY(!QFile::exists(directory.filePath("codeqa.json")));
    QVERIFY(!QFile::exists(directory.filePath(".codeqa")));
}

void AppControllerTest::successfulImportReplacesTheCurrentReport()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString first = writeReport(directory, "first.json", oneFinding("first.rule"));
    const QString second = writeReport(
        directory,
        "second.json",
        {{"src/one.cpp", "error", "second.one", "one", 2},
         {"src/two.cpp", "information", "second.two", "two", 4}});
    AppController controller(settingsPath(directory));

    controller.openReport(QUrl::fromLocalFile(first));
    QTRY_COMPARE(controller.findings()->rowCount(), 1);
    controller.openReport(QUrl::fromLocalFile(second));

    QTRY_COMPARE(controller.findings()->rowCount(), 2);
    QCOMPARE(controller.errorMessage(), QString{});
    QCOMPARE(controller.qualityGateStatus(), QString("Not evaluated"));
}

void AppControllerTest::invalidImportKeepsTheCurrentReport()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString valid = writeReport(directory, "valid.json", oneFinding());
    AppController controller(settingsPath(directory));
    controller.openReport(QUrl::fromLocalFile(valid));
    QTRY_COMPARE(controller.findings()->rowCount(), 1);

    controller.openReport(QUrl::fromLocalFile(directory.filePath("missing.json")));

    QCOMPARE(controller.findings()->rowCount(), 1);
    QVERIFY(controller.errorMessage().contains("REPORT001"));
}

void AppControllerTest::malformedImportKeepsTheCurrentReport()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString valid = writeReport(directory, "valid.json", oneFinding());
    const QString malformed = directory.filePath("malformed.json");
    writeTextFile(malformed, "{ invalid");
    AppController controller(settingsPath(directory));
    controller.openReport(QUrl::fromLocalFile(valid));

    controller.openReport(QUrl::fromLocalFile(malformed));

    QCOMPARE(controller.findings()->rowCount(), 1);
    QVERIFY(controller.errorMessage().contains("REPORT002"));
}

void AppControllerTest::findingsExposeRolesAndSortBySeverityThenLocation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString report = writeReport(
        directory,
        "roles.json",
        {{"src/zeta.cpp", "information", "info.rule", "infoSymbol", 1},
         {"src/bravo.cpp", "error", "error.late", "lateError", 10},
         {"src/alpha.cpp", "warning", "warning.rule", "warningSymbol", 1},
         {"src/bravo.cpp", "error", "error.early", "earlyError", 2}});
    AppController controller(settingsPath(directory));

    controller.openReport(QUrl::fromLocalFile(report));

    FindingListModel* model = controller.findings();
    QCOMPARE(model->rowCount(), 4);
    const QHash<int, QByteArray> roles = model->roleNames();
    const QList<QByteArray> expectedRoles = {
        "severity", "ruleId", "symbolName", "path", "line", "column",
        "observedValue", "limitValue", "description", "recommendation", "metricSummary"};
    for (const QByteArray& role : expectedRoles)
    {
        QVERIFY2(roles.values().contains(role), role.constData());
    }

    QCOMPARE(model->data(model->index(0), FindingListModel::RuleIdRole).toString(), QString("error.early"));
    QCOMPARE(model->data(model->index(1), FindingListModel::RuleIdRole).toString(), QString("error.late"));
    QCOMPARE(model->data(model->index(2), FindingListModel::SeverityRole).toString(), QString("warning"));
    QCOMPARE(model->data(model->index(3), FindingListModel::SeverityRole).toString(), QString("information"));
    QCOMPARE(model->data(model->index(0), FindingListModel::LineRole).toULongLong(), 2ULL);
    QVERIFY(model->data(model->index(0), FindingListModel::MetricSummaryRole).toString().contains("Cyclomatic: 12"));
    QCOMPARE(controller.files()->rowCount(), 3);
    const QVariantList functionMetrics = controller.files()->data(
        controller.files()->index(0), FileListModel::FunctionMetricsRole).toList();
    QVERIFY(!functionMetrics.isEmpty());
    const QVariantMap metric = functionMetrics.front().toMap();
    QVERIFY(metric.contains("blankLines"));
    QVERIFY(metric.contains("commentLines"));
    QVERIFY(metric.contains("sourceRange"));
    QVERIFY(metric.contains("maxConditionRange"));
}

void AppControllerTest::summaryCountsTrackDocumentsAndFilteredFindings()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString initial = writeReport(
        directory,
        "summary-initial.json",
        {{"src/a.cpp", "error", "error.first", "first", 1},
         {"src/a.cpp", "warning", "warning.only", "second", 8},
         {"src/b.cpp", "error", "error.second", "third", 2},
         {"src/c.cpp", "information", "info.only", "fourth", 3}});
    const QString replacement = writeReport(
        directory,
        "summary-replacement.json",
        {{"src/d.cpp", "warning", "warning.replacement", "fifth", 4},
         {"src/e.cpp", "information", "info.replacement", "sixth", 5}});
    AppController controller(settingsPath(directory));

    controller.openReport(QUrl::fromLocalFile(initial));

    QCOMPARE(controller.files()->fileCount(), 3);
    QCOMPARE(controller.files()->diagnosticCount(), 0);
    QCOMPARE(controller.findings()->findingCount(), 4);
    QCOMPARE(controller.findings()->errorCount(), 2);
    QCOMPARE(controller.findings()->warningCount(), 1);

    QSignalSpy summarySpy(controller.findings(), &FindingListModel::summaryChanged);
    controller.setFindingSeverityFilter("warning");
    QCOMPARE(summarySpy.count(), 1);
    QCOMPARE(controller.files()->fileCount(), 3);
    QCOMPARE(controller.findings()->findingCount(), 1);
    QCOMPARE(controller.findings()->errorCount(), 0);
    QCOMPARE(controller.findings()->warningCount(), 1);

    controller.openReport(QUrl::fromLocalFile(replacement));
    QCOMPARE(controller.files()->fileCount(), 2);
    QCOMPARE(controller.findings()->findingCount(), 1);
    QCOMPARE(controller.findings()->errorCount(), 0);
    QCOMPARE(controller.findings()->warningCount(), 1);

    controller.setFindingSeverityFilter({});
    QCOMPARE(controller.findings()->findingCount(), 2);
    QCOMPARE(controller.findings()->errorCount(), 0);
    QCOMPARE(controller.findings()->warningCount(), 1);
}

void AppControllerTest::overloadedFindingsUseMetricsFromTheirOwnSourceRange()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString report = writeReport(
        directory,
        "overloads.json",
        {{"src/overloads.cpp", "warning", "complexity.cyclomatic", "sample::run", 3, 4},
         {"src/overloads.cpp", "warning", "complexity.cyclomatic", "sample::run", 20, 20}});
    AppController controller(settingsPath(directory));

    controller.openReport(QUrl::fromLocalFile(report));

    QCOMPARE(controller.findings()->rowCount(), 2);
    QVERIFY(controller.findings()->data(
        controller.findings()->index(0), FindingListModel::MetricSummaryRole).toString().contains("Cyclomatic: 4"));
    QVERIFY(controller.findings()->data(
        controller.findings()->index(1), FindingListModel::MetricSummaryRole).toString().contains("Cyclomatic: 20"));
}

void AppControllerTest::replacementPreservesPersistedFindingFilters()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString report = writeReport(
        directory,
        "filtered.json",
        {{"src/a.cpp", "error", "target.rule", "target", 1},
         {"src/b.cpp", "warning", "other.rule", "other", 2}});
    const QString replacement = writeReport(
        directory,
        "replacement.json",
        {{"src/c.cpp", "error", "target.rule", "targetAgain", 3},
         {"src/d.cpp", "information", "target.rule", "filteredOut", 4}});
    const QString path = settingsPath(directory);
    AppController controller(path);
    controller.setLastPage("Results");
    controller.setFindingSeverityFilter("error");
    controller.setFindingRuleFilter("target.rule");
    controller.setFindingSearchText("target");

    controller.openReport(QUrl::fromLocalFile(report));
    QCOMPARE(controller.findings()->rowCount(), 1);
    controller.openReport(QUrl::fromLocalFile(replacement));

    QCOMPARE(controller.findings()->rowCount(), 1);
    QSettings persisted(path, QSettings::IniFormat);
    QCOMPARE(persisted.value("ui/lastPage").toString(), QString("Results"));
    QCOMPARE(persisted.value("filters/severity").toString(), QString("error"));
    QCOMPARE(persisted.value("filters/ruleId").toString(), QString("target.rule"));
    QCOMPARE(persisted.value("filters/searchText").toString(), QString("target"));

    AppController restored(path);
    restored.openReport(QUrl::fromLocalFile(replacement));
    QCOMPARE(restored.lastPage(), QString("Results"));
    QCOMPARE(restored.property("findingSeverityFilter").toString(), QString("error"));
    QCOMPARE(restored.property("findingRuleFilter").toString(), QString("target.rule"));
    QCOMPARE(restored.property("findingSearchText").toString(), QString("target"));
    QCOMPARE(restored.findings()->rowCount(), 1);
}

void AppControllerTest::urlSnapshotActionDuringAnalysisCannotChangeProjectOrCompletionSnapshots()
{
    QTemporaryDir analysisProject;
    QTemporaryDir alternateProject;
    QVERIFY(analysisProject.isValid());
    QVERIFY(alternateProject.isValid());
    writeTextFile(
        analysisProject.filePath("sample.c"),
        "int classify(int a, int b, int c, int d, int e)\n"
        "{\n"
        "    return a + b + c + d + e;\n"
        "}\n");

    const QString alternateSnapshots = alternateProject.filePath(".codeqa/snapshots");
    QVERIFY(QDir{}.mkpath(alternateSnapshots));
    const QByteArray alternateReport = QByteArray::fromStdString(reportJson(
        std::filesystem::path(alternateProject.path().toStdWString()), oneFinding()).dump(2));
    writeTextFile(QDir(alternateSnapshots).filePath("20260101T000000Z.json"), alternateReport);
    writeTextFile(QDir(alternateSnapshots).filePath("20260201T000000Z.json"), alternateReport);

    AppController controller(settingsPath(analysisProject));
    const QUrl analysisProjectUrl = QUrl::fromLocalFile(analysisProject.path());
    controller.selectProject(analysisProjectUrl);
    QSignalSpy currentProjectSpy(&controller, &AppController::currentProjectChanged);
    QSignalSpy recentPathsSpy(&controller, &AppController::recentPathsChanged);
    bool blockedActionAttempted = false;
    QUrl projectDuringBlockedAction;
    QStringList recentProjectsDuringBlockedAction;
    QString blockedActionError;
    connect(&controller, &AppController::analysisPhaseChanged, &controller, [&](const QString& phase)
    {
        if (blockedActionAttempted || phase.isEmpty())
        {
            return;
        }

        blockedActionAttempted = true;
        controller.refreshSnapshots(QUrl::fromLocalFile(alternateProject.path()));
        projectDuringBlockedAction = controller.currentProject();
        recentProjectsDuringBlockedAction = controller.recentProjects();
        blockedActionError = controller.errorMessage();
    });

    controller.startAnalysis();

    QTRY_VERIFY_WITH_TIMEOUT(blockedActionAttempted, 30000);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.analysisRunning(), 30000);
    QCOMPARE(projectDuringBlockedAction, analysisProjectUrl);
    QCOMPARE(
        recentProjectsDuringBlockedAction,
        QStringList{QDir::cleanPath(analysisProject.path())});
    QVERIFY(blockedActionError.contains("DESKTOP002"));
    QCOMPARE(currentProjectSpy.count(), 0);
    QCOMPARE(recentPathsSpy.count(), 0);
    QCOMPARE(controller.currentProject(), analysisProjectUrl);
    QCOMPARE(controller.snapshots()->rowCount(), 1);
    QCOMPARE(controller.findings()->rowCount(), 1);
    QCOMPARE(controller.errorMessage(), QString{});
}

void AppControllerTest::analysisReportsRealPhasesAndFailedQualityGate()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTextFile(
        directory.filePath("sample.c"),
        "int classify(int a, int b, int c, int d, int e)\n"
        "{\n"
        "    return a + b + c + d + e;\n"
        "}\n");
    AppController controller(settingsPath(directory));
    QSignalSpy phaseSpy(&controller, &AppController::analysisPhaseChanged);
    QSignalSpy runningSpy(&controller, &AppController::analysisRunningChanged);

    controller.startAnalysis(QUrl::fromLocalFile(directory.path()));

    QTRY_VERIFY_WITH_TIMEOUT(!controller.analysisRunning(), 30000);
    QVERIFY(runningSpy.count() >= 2);
    QStringList phases;
    for (const QList<QVariant>& arguments : phaseSpy)
    {
        phases.push_back(arguments.at(0).toString());
    }
    QCOMPARE(
        phases,
        QStringList({
            "Discovering files",
            "Parsing source files",
            "Measuring functions",
            "Applying rules",
            "Preparing report"}));
    QCOMPARE(controller.qualityGateStatus(), QString("Failed"));
    QCOMPARE(controller.findings()->rowCount(), 1);
    QCOMPARE(controller.snapshots()->rowCount(), 1);
    QCOMPARE(controller.errorMessage(), QString{});
}

void AppControllerTest::analysisModelResetSeesOneConsistentCompletedState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTextFile(
        directory.filePath("sample.c"),
        "int classify(int a, int b, int c, int d, int e)\n"
        "{\n"
        "    return a + b + c + d + e;\n"
        "}\n");
    const QString previous = writeReport(
        directory,
        "previous.json",
        {{"src/old-one.cpp", "information", "old.one", "oldOne", 2},
         {"src/old-two.cpp", "warning", "old.two", "oldTwo", 4}});
    AppController controller(settingsPath(directory));
    controller.openReport(QUrl::fromLocalFile(previous));
    QCOMPARE(controller.files()->rowCount(), 2);
    QCOMPARE(controller.qualityGateStatus(), QString("Not evaluated"));

    int filesAtReset = -1;
    int snapshotsAtReset = -1;
    QString qualityGateAtReset;
    QString errorAtReset;
    bool runningAtReset = false;
    connect(controller.findings(), &QAbstractItemModel::modelReset, &controller, [&]()
    {
        filesAtReset = controller.files()->rowCount();
        snapshotsAtReset = controller.snapshots()->rowCount();
        qualityGateAtReset = controller.qualityGateStatus();
        errorAtReset = controller.errorMessage();
        runningAtReset = controller.analysisRunning();
    });

    controller.startAnalysis(QUrl::fromLocalFile(directory.path()));

    QTRY_VERIFY_WITH_TIMEOUT(!controller.analysisRunning(), 30000);
    QCOMPARE(controller.findings()->rowCount(), 1);
    QCOMPARE(filesAtReset, 1);
    QCOMPARE(snapshotsAtReset, 1);
    QCOMPARE(qualityGateAtReset, QString("Failed"));
    QCOMPARE(errorAtReset, QString{});
    QVERIFY(runningAtReset);
}

void AppControllerTest::idleSignalCanStartTheNextAnalysisAfterCompletionIsPublished()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTextFile(
        directory.filePath("sample.c"),
        "int classify(int a, int b, int c, int d, int e)\n"
        "{\n"
        "    return a + b + c + d + e;\n"
        "}\n");
    AppController controller(settingsPath(directory));
    QList<int> snapshotCountsAtIdle;
    QStringList gatesAtIdle;
    QStringList errorsAtIdle;

    connect(&controller, &AppController::analysisRunningChanged, &controller, [&](const bool running)
    {
        if (running)
        {
            return;
        }

        snapshotCountsAtIdle.push_back(controller.snapshots()->rowCount());
        gatesAtIdle.push_back(controller.qualityGateStatus());
        errorsAtIdle.push_back(controller.errorMessage());
        if (snapshotCountsAtIdle.size() == 1)
        {
            controller.startAnalysis();
        }
    });

    controller.startAnalysis(QUrl::fromLocalFile(directory.path()));

    QTRY_COMPARE_WITH_TIMEOUT(snapshotCountsAtIdle.size(), 2, 30000);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.analysisRunning(), 30000);
    QCOMPARE(snapshotCountsAtIdle.front(), 1);
    QVERIFY(snapshotCountsAtIdle.back() >= 1);
    QCOMPARE(gatesAtIdle, QStringList({"Failed", "Passed"}));
    QCOMPARE(errorsAtIdle, QStringList({"", ""}));
    QCOMPARE(controller.snapshots()->rowCount(), snapshotCountsAtIdle.back());
}

void AppControllerTest::workerAnalysisFailureKeepsTheCurrentReport()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString valid = writeReport(directory, "current.json", oneFinding());
    AppController controller(settingsPath(directory));
    controller.openReport(QUrl::fromLocalFile(valid));
    QCOMPARE(controller.findings()->rowCount(), 1);
    QCOMPARE(controller.qualityGateStatus(), QString("Not evaluated"));
    QSignalSpy runningSpy(&controller, &AppController::analysisRunningChanged);

    controller.startAnalysis(
        QUrl::fromLocalFile(directory.path()), codeqa::config::ProjectConfig{});

    QTRY_VERIFY_WITH_TIMEOUT(!controller.analysisRunning(), 30000);
    QVERIFY(runningSpy.count() >= 2);
    QCOMPARE(controller.findings()->rowCount(), 1);
    QCOMPARE(controller.qualityGateStatus(), QString("Not evaluated"));
    QVERIFY(controller.errorMessage().contains("APPLICATION001"));
}

void AppControllerTest::emptyProjectAnalysisIsActionableAndKeepsTheCurrentReport()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString valid = writeReport(directory, "valid.json", oneFinding());
    AppController controller(settingsPath(directory));
    controller.openReport(QUrl::fromLocalFile(valid));

    controller.startAnalysis();

    QVERIFY(!controller.analysisRunning());
    QCOMPARE(controller.findings()->rowCount(), 1);
    QVERIFY(controller.errorMessage().contains("project", Qt::CaseInsensitive));
}

void AppControllerTest::selectingAnotherProjectClearsSnapshotsFromPreviousProject()
{
    QTemporaryDir settingsDirectory;
    QTemporaryDir firstProject;
    QTemporaryDir secondProject;
    QVERIFY(settingsDirectory.isValid());
    QVERIFY(firstProject.isValid());
    QVERIFY(secondProject.isValid());
    const QString snapshots = firstProject.filePath(".codeqa/snapshots");
    QVERIFY(QDir{}.mkpath(snapshots));
    const QByteArray report = QByteArray::fromStdString(reportJson(
        std::filesystem::path(firstProject.path().toStdWString()), oneFinding()).dump(2));
    writeTextFile(QDir(snapshots).filePath("20260101T000000Z.json"), report);
    AppController controller(settingsPath(settingsDirectory));

    controller.selectProject(QUrl::fromLocalFile(firstProject.path()));
    controller.refreshSnapshots();
    QCOMPARE(controller.snapshots()->rowCount(), 1);

    controller.selectProject(QUrl::fromLocalFile(secondProject.path()));

    QCOMPARE(controller.currentProject(), QUrl::fromLocalFile(secondProject.path()));
    QCOMPARE(controller.snapshots()->rowCount(), 0);
}

void AppControllerTest::snapshotRefreshKeepsInvalidConfigurationDiagnostic()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTextFile(
        directory.filePath("codeqa.json"),
        "{\n  \"rules\": { \"unknown.rule\": {} }\n}\n");
    const QString snapshots = directory.filePath(".codeqa/snapshots");
    QVERIFY(QDir{}.mkpath(snapshots));
    const QByteArray report = QByteArray::fromStdString(reportJson(
        std::filesystem::path(directory.path().toStdWString()), oneFinding()).dump(2));
    writeTextFile(QDir(snapshots).filePath("20260101T000000Z.json"), report);
    AppController controller(settingsPath(directory));

    controller.selectProject(QUrl::fromLocalFile(directory.path()));
    QVERIFY(!controller.configurationAvailable());
    QVERIFY(controller.errorMessage().contains("CONFIG003"));

    controller.refreshSnapshots();

    QCOMPARE(controller.snapshots()->rowCount(), 1);
    QVERIFY(controller.errorMessage().contains("CONFIG003"));
}

void AppControllerTest::historyOpensWithoutCreatingAnotherSnapshot()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString snapshots = directory.filePath(".codeqa/snapshots");
    QVERIFY(QDir{}.mkpath(snapshots));
    const QByteArray report = QByteArray::fromStdString(reportJson(
        std::filesystem::path(directory.path().toStdWString()), oneFinding()).dump(2));
    writeTextFile(QDir(snapshots).filePath("20260101T000000Z.json"), report);
    AppController controller(settingsPath(directory));
    controller.selectProject(QUrl::fromLocalFile(directory.path()));
    controller.refreshSnapshots();
    QCOMPARE(
        controller.snapshots()->data(
            controller.snapshots()->index(0), SnapshotListModel::TimestampRole).toString(),
        QString("20260101T000000Z"));

    controller.openSnapshot("20260101T000000Z");

    QCOMPARE(QDir(snapshots).entryList({"*.json"}, QDir::Files).size(), 1);
    QCOMPARE(controller.qualityGateStatus(), QString("Not evaluated"));
}

void AppControllerTest::snapshotOpenFailureKeepsTheCurrentReport()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString valid = writeReport(directory, "valid.json", oneFinding());
    const QString snapshots = directory.filePath(".codeqa/snapshots");
    QVERIFY(QDir{}.mkpath(snapshots));
    writeTextFile(QDir(snapshots).filePath("20260101T000000Z.json"), "{ invalid");
    AppController controller(settingsPath(directory));
    controller.openReport(QUrl::fromLocalFile(valid));

    controller.openSnapshot(QUrl::fromLocalFile(directory.path()), "20260101T000000Z");

    QCOMPARE(controller.findings()->rowCount(), 1);
    QVERIFY(controller.errorMessage().contains("REPORT002"));
}

void AppControllerTest::snapshotDeleteFailureKeepsTheCurrentReport()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString valid = writeReport(directory, "valid.json", oneFinding());
    AppController controller(settingsPath(directory));
    controller.openReport(QUrl::fromLocalFile(valid));
    QCOMPARE(controller.findings()->rowCount(), 1);

    controller.selectProject(QUrl::fromLocalFile(directory.path()));
    controller.deleteSnapshot("20260101T000000Z");

    QCOMPARE(controller.findings()->rowCount(), 1);
    QVERIFY(controller.errorMessage().contains(".codeqa"));
    QVERIFY(controller.errorMessage().contains("20260101T000000Z"));
}

void AppControllerTest::snapshotsAreNewestFirstAndDeleteRefreshesTheModel()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString snapshots = directory.filePath(".codeqa/snapshots");
    QVERIFY(QDir{}.mkpath(snapshots));
    const QByteArray report = QByteArray::fromStdString(reportJson(
        std::filesystem::path(directory.path().toStdWString()), oneFinding()).dump(2));
    writeTextFile(QDir(snapshots).filePath("20260101T000000Z.json"), report);
    writeTextFile(QDir(snapshots).filePath("20260301T000000Z.json"), report);
    AppController controller(settingsPath(directory));

    controller.refreshSnapshots(QUrl::fromLocalFile(directory.path()));

    QCOMPARE(controller.snapshots()->rowCount(), 2);
    QCOMPARE(
        controller.snapshots()->data(controller.snapshots()->index(0), SnapshotListModel::TimestampRole).toString(),
        QString("20260301T000000Z"));
    controller.deleteSnapshot(QUrl::fromLocalFile(directory.path()), "20260301T000000Z");
    QCOMPARE(controller.snapshots()->rowCount(), 1);
    QVERIFY(!QFile::exists(QDir(snapshots).filePath("20260301T000000Z.json")));
}

void AppControllerTest::clearRecentPathsRetainsTheCurrentReport()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString valid = writeReport(directory, "valid.json", oneFinding());
    const QString path = settingsPath(directory);
    {
        QSettings seed(path, QSettings::IniFormat);
        seed.setValue("recentProjects", QStringList{directory.path()});
        seed.setValue("recentReports", QStringList{valid});
        seed.setValue("ui/lastPage", "History");
        seed.setValue("filters/severity", "warning");
        seed.setValue("filters/ruleId", "complexity.cyclomatic");
        seed.setValue("filters/searchText", "sample");
    }
    AppController controller(path);
    controller.openReport(QUrl::fromLocalFile(valid));
    QCOMPARE(controller.findings()->rowCount(), 1);

    controller.clearRecentPaths();

    QCOMPARE(controller.findings()->rowCount(), 1);
    QVERIFY(controller.recentProjects().isEmpty());
    QVERIFY(controller.recentReports().isEmpty());
    QSettings persisted(path, QSettings::IniFormat);
    QVERIFY(!persisted.contains("recentProjects"));
    QVERIFY(!persisted.contains("recentReports"));
    QVERIFY(persisted.contains("ui/lastPage"));
    QCOMPARE(persisted.value("ui/lastPage").toString(), QString("History"));
    QStringList keys = persisted.allKeys();
    keys.sort();
    QCOMPARE(
        keys,
        QStringList({
            "filters/ruleId",
            "filters/searchText",
            "filters/severity",
            "ui/lastPage"}));
}

QTEST_GUILESS_MAIN(AppControllerTest)

#include "AppControllerTest.moc"
