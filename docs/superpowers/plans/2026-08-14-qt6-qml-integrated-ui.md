# Qt 6 / QML Integrated Desktop UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver a Windows-only Qt 6/QML application that runs CodeQA analyses, manages project-local snapshots and `codeqa.json`, and presents report JSON in one read-only findings-first workspace.

**Architecture:** Keep the analysis core Qt-free. A shared C++ `ApplicationFacade` maps both fresh in-memory analysis results and imported schema-version-1 JSON into `ReportDocument`; the CLI and Qt adapter call it. Qt/QML, worker dispatch, QAbstractListModel classes, and QSettings live in a separate desktop support target.

**Tech Stack:** C++17, CMake 3.16+, Qt 6 Core/Gui/Qml/Quick/Quick Controls 2/Quick Dialogs 2/Concurrent/Test/QuickTest, doctest, nlohmann/json, Windows, `windeployqt`.

## Global Constraints

- Keep `codeqa_core` Qt-free; neither `include/codeqa/` nor `src/application/` may include a Qt header or expose a Qt type.
- Build the desktop executable under `CODEQA_BUILD_DESKTOP` on Windows only; preserve the current CLI output and exit-code behavior.
- All QML copy is English. Add no network clients, telemetry, cloud storage, collaboration, source editing, annotation, suppression, or raw JSON editing feature.
- Reports and findings are read-only. Configuration is writable only through the semantic `codeqa.json` form and an explicit Save action.
- Store configuration only at `<project>/codeqa.json` and snapshots only at `<project>/.codeqa/snapshots`. Importing a report does not write at all.
- Fresh analysis keeps the current CLI snapshot behavior. Configuration save and snapshot deletion require an explicit user action.
- A failed quality gate is a completed analysis with a warning state. Historical and imported schema-version-1 reports show `Not evaluated` because that outcome is not stored in their JSON.
- Filesystem errors retain the currently displayed report and identify both the relevant path and CodeQA validation error.
- Follow the repository’s C++ style. Register doctest files in `tests/CMakeLists.txt`; do not commit any build directory.

---

## File Structure

| Path | Responsibility |
| --- | --- |
| `include/codeqa/application/AnalysisProgress.h` | Qt-free analysis phase enum and callback. |
| `include/codeqa/application/ReportDocument.h` | Normalized read-only report DTO and gate state. |
| `include/codeqa/application/ReportDocumentFactory.h`, `src/application/ReportDocumentFactory.cpp` | Fresh-result mapping and version-1 report decoding. |
| `include/codeqa/application/ApplicationFacade.h`, `src/application/ApplicationFacade.cpp` | Shared analysis/report/config/snapshot orchestration. |
| `include/codeqa/application/SaveProjectConfig.h`, `src/application/SaveProjectConfig.cpp` | Validated explicit configuration writer. |
| `include/codeqa/application/DeleteSnapshot.h`, `src/application/DeleteSnapshot.cpp` | Timestamp-restricted snapshot deletion. |
| `src/desktop/CMakeLists.txt`, `src/desktop/main.cpp` | Desktop target and QML entry point. |
| `src/desktop/AppController.{h,cpp}` | Main-thread bridge, worker dispatch, current report and `QSettings`. |
| `src/desktop/ReportModels.{h,cpp}` | QML-facing findings, files/functions, config, and snapshot models. |
| `src/desktop/qml/Main.qml` | Sidebar shell and global actions. |
| `src/desktop/qml/pages/{AnalyzePage,HistoryPage,ResultsPage}.qml` | The three application pages. |
| `tests/unit/{ReportDocument,ApplicationFacade,SaveProjectConfig,DeleteSnapshot}Test.cpp` | New Qt-free behavior tests. |
| `tests/desktop/AppControllerTest.cpp`, `tests/desktop/qml/tst_DesktopShell.qml` | Qt adapter and QML smoke coverage. |
| `docs/desktop-ui.md` | Build, deployment, privacy, and manual package validation guide. |

## Task 1: Add a Windows Qt desktop build shell

**Files:**

- Modify: `CMakeLists.txt`, `tests/CMakeLists.txt`
- Create: `src/desktop/CMakeLists.txt`, `src/desktop/main.cpp`, `src/desktop/qml/Main.qml`
- Create: `tests/desktop/qml/tst_DesktopShell.qml`

**Interfaces:**

- Consumes: `codeqa_core` and `CODEQA_BUILD_TESTS`.
- Produces: `codeqa_desktop` and CTest test `codeqa_desktop_qml_smoke`.

- [ ] **Step 1: Write the failing QML shell test**

  ```qml
  import QtQuick
  import QtTest
  import CodeQA.Desktop

  TestCase {
      name: "DesktopShell"
      function test_shell_has_the_three_primary_pages() {
          const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
          verify(shell !== null)
          compare(shell.findChild("analyzeNavigationButton").text, "Analyze")
          compare(shell.findChild("historyNavigationButton").text, "History")
          compare(shell.findChild("resultsNavigationButton").text, "Results")
      }
  }
  ```

- [ ] **Step 2: Run the test target before it exists**

  ```powershell
  cmake -S . -B build -DCODEQA_BUILD_DESKTOP=ON
  cmake --build build --config Debug --target codeqa_desktop_qml_smoke
  ```

  Expected: CMake or the build fails because the desktop target/module has not been created.

- [ ] **Step 3: Add the minimal CMake target and QML module**

  In root CMake, add `option(CODEQA_BUILD_DESKTOP "Build the Qt 6 desktop application" ${WIN32})`. Inside `if(WIN32 AND CODEQA_BUILD_DESKTOP)`, require `Qt6` components `Core`, `Gui`, `Qml`, `Quick`, `QuickControls2`, `QuickDialogs2`, `Concurrent`, `Test`, and `QuickTest`; then add `src/desktop`.

  `src/desktop/CMakeLists.txt` begins with:

  ```cmake
  qt_add_executable(codeqa_desktop main.cpp)
  target_link_libraries(codeqa_desktop PRIVATE
      codeqa_core Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
      Qt6::QuickControls2 Qt6::QuickDialogs2 Qt6::Concurrent)
  qt_add_qml_module(codeqa_desktop
      URI CodeQA.Desktop VERSION 1.0
      QML_FILES qml/Main.qml)
  ```

  `main.cpp` starts a `QGuiApplication`, loads `CodeQA.Desktop/Main` with `QQmlApplicationEngine::loadFromModule`, and exits with failure if there is no root object. `Main.qml` supplies the three named English buttons only; its remaining behavior is added by the subsequent tasks in this plan.

- [ ] **Step 4: Verify the QML shell and CLI-only build**

  ```powershell
  cmake --build build --config Debug --target codeqa_desktop
  ctest --test-dir build -C Debug --output-on-failure -R codeqa_desktop_qml_smoke
  cmake -S . -B build-headless -DCODEQA_BUILD_DESKTOP=OFF
  cmake --build build-headless --config Debug --target codeqa
  ```

  Expected: the QML test passes with Qt installed; the existing CLI remains buildable without Qt.

- [ ] **Step 5: Commit the build shell**

  ```powershell
  git add CMakeLists.txt src/desktop tests/CMakeLists.txt tests/desktop
  git commit -m "feat: add Qt desktop build shell"
  ```

## Task 2: Create the Qt-free report document and strict JSON decoder

**Files:**

- Create: `include/codeqa/application/ReportDocument.h`, `include/codeqa/application/ReportDocumentFactory.h`
- Create: `src/application/ReportDocumentFactory.cpp`, `tests/unit/ReportDocumentTest.cpp`
- Modify: `CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: `RunAnalysisResult`, `ProjectConfig`, current severity/location types, and JSON emitted by `toReportJson`.
- Produces:

  ```cpp
  enum class QualityGateOutcome { Passed, Failed, NotEvaluated };
  struct ReportComparison {
      std::string previousTimestamp;
      std::size_t newCount{0};
      std::size_t existingCount{0};
      std::size_t resolvedCount{0};
  };
  struct ReportDocumentResult {
      std::optional<ReportDocument> document;
      std::optional<std::string> error;
  };
  [[nodiscard]] ReportDocument makeReportDocument(
      const RunAnalysisResult&, const config::ProjectConfig&, const std::string&,
      QualityGateOutcome, const std::optional<ReportComparison>&);
  [[nodiscard]] ReportDocumentResult parseReportDocument(const nlohmann::json&);
  [[nodiscard]] ReportDocumentResult loadReportDocument(const std::filesystem::path&);
  ```

- [ ] **Step 1: Write failing doctests for fresh and imported inputs**

  ```cpp
  TEST_CASE("parseReportDocument accepts a schema-version-1 CodeQA report")
  {
      const auto result = codeqa::application::parseReportDocument(makeCompleteReportJson());
      REQUIRE(result.document.has_value());
      CHECK(result.document->files.front().findings.front().ruleId == "complexity.cyclomatic");
      CHECK(result.document->qualityGateOutcome == codeqa::application::QualityGateOutcome::NotEvaluated);
  }

  TEST_CASE("parseReportDocument rejects an unsupported schema version")
  {
      nlohmann::json json = makeCompleteReportJson();
      json["schemaVersion"] = 2;
      const auto result = codeqa::application::parseReportDocument(json);
      CHECK_FALSE(result.document.has_value());
      REQUIRE(result.error.has_value());
      CHECK(result.error->find("schemaVersion 2") != std::string::npos);
  }
  ```

- [ ] **Step 2: Run the focused tests to verify the expected failure**

  ```powershell
  cmake --build build --config Debug --target codeqa_tests
  .\build\Debug\codeqa_tests.exe --test-case="parseReportDocument*"
  ```

  Expected: compilation fails because the report-document API is absent.

- [ ] **Step 3: Implement the DTO and deterministic parser**

  Define a presentation DTO containing metadata, effective `ProjectConfig`, files, diagnostics, function metric values, findings, optional comparison, and `QualityGateOutcome`. Keep JSON file paths relative exactly as serialized. Decode `schemaVersion == 1`, `metadata`, `config`, and `files`; validate the embedded config by calling `config::parseConfig(document.at("config").dump())`; validate each required mapped field before `get()`.

  Return `REPORT001` for unreadable reports, `REPORT002` for malformed JSON, `REPORT003` for version mismatch, and `REPORT004` for missing/wrongly typed required fields. Extra version-1 keys remain tolerated. Direct conversion iterates aligned `model.files`, `metrics.files`, and `findings.files`; it does not serialize or parse JSON.

- [ ] **Step 4: Run focused and report-regression tests**

  ```powershell
  cmake --build build --config Debug --target codeqa_tests
  .\build\Debug\codeqa_tests.exe --test-case="*Report*"
  ```

  Expected: direct mapping, valid import, malformed JSON, missing field, unsupported schema, `ReportJsonTest`, and `FileJsonTest` all pass.

- [ ] **Step 5: Commit the normalized report boundary**

  ```powershell
  git add include/codeqa/application/ReportDocument.h include/codeqa/application/ReportDocumentFactory.h src/application/ReportDocumentFactory.cpp tests/unit/ReportDocumentTest.cpp CMakeLists.txt tests/CMakeLists.txt
  git commit -m "feat: add normalized report document"
  ```

## Task 3: Extract shared analysis, config, and snapshot orchestration

**Files:**

- Create: `include/codeqa/application/{AnalysisProgress,ApplicationFacade,SaveProjectConfig,DeleteSnapshot}.h`
- Create: `src/application/{ApplicationFacade,SaveProjectConfig,DeleteSnapshot}.cpp`
- Modify: `include/codeqa/application/{AnalyzeProject,RunAnalysis}.h`, `src/application/{AnalyzeProject,RunAnalysis}.cpp`, `src/cli/RunApplication.cpp`
- Create: `tests/unit/{ApplicationFacade,SaveProjectConfig,DeleteSnapshot}Test.cpp`
- Modify: `CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: pipeline services, snapshot services, `CompareSnapshots`, report factory, and current report JSON writer.
- Produces these Qt-free request/result types and `ApplicationFacade` methods:

  ```cpp
  struct ApplicationError {
      std::optional<std::string> message;
      [[nodiscard]] bool hasError() const { return message.has_value(); }
  };
  struct FacadeAnalysisRequest {
      std::filesystem::path projectRoot;
      config::ProjectConfig config;
      std::vector<std::string> excludePatterns;
  };
  struct AnalysisFacadeResult {
      std::optional<ReportDocument> document;
      std::optional<nlohmann::json> reportJson;
      std::optional<std::filesystem::path> savedSnapshotPath;
      std::optional<ReportComparison> comparison;
      std::optional<std::string> warning;
      ApplicationError error;
      QualityGateOutcome qualityGateOutcome{QualityGateOutcome::NotEvaluated};
  };
  struct ProjectConfigLoadResult {
      std::optional<config::ProjectConfig> config;
      bool fileExists{false};
      ApplicationError error;
  };
  struct SnapshotEntry { std::string timestamp; std::filesystem::path path; };
  struct SnapshotListResult { std::vector<SnapshotEntry> snapshots; ApplicationError error; };
  class ApplicationFacade {
  public:
      [[nodiscard]] ProjectConfigLoadResult loadProjectConfig(const std::filesystem::path&) const;
      [[nodiscard]] ApplicationError saveProjectConfig(const std::filesystem::path&, const config::ProjectConfig&) const;
      [[nodiscard]] AnalysisFacadeResult analyzeProject(const FacadeAnalysisRequest&, AnalysisProgressCallback = {}) const;
      [[nodiscard]] ReportDocumentResult openReport(const std::filesystem::path&) const;
      [[nodiscard]] SnapshotListResult listSnapshots(const std::filesystem::path&) const;
      [[nodiscard]] ReportDocumentResult openSnapshot(const std::filesystem::path&, const std::string&) const;
      [[nodiscard]] ApplicationError deleteSnapshot(const std::filesystem::path&, const std::string&) const;
  };
  ```

- [ ] **Step 1: Write failing facade behavior tests**

  ```cpp
  TEST_CASE("ApplicationFacade completes an analysis, saves a snapshot, and reports a failed quality gate")
  {
      TempProject project{sourceWithWarningFinding()};
      std::vector<codeqa::application::AnalysisPhase> phases;
      const auto result = codeqa::application::ApplicationFacade{}.analyzeProject(
          {project.root(), codeqa::config::defaultConfig(), {}},
          [&phases](const auto phase) { phases.push_back(phase); });

      REQUIRE(result.document.has_value());
      CHECK(result.qualityGateOutcome == codeqa::application::QualityGateOutcome::Failed);
      CHECK(result.savedSnapshotPath.has_value());
      CHECK(phases.back() == codeqa::application::AnalysisPhase::PreparingReport);
  }

  TEST_CASE("DeleteSnapshot rejects a path traversal timestamp")
  {
      TempProject project{};
      const auto result = codeqa::application::ApplicationFacade{}.deleteSnapshot(project.root(), "../../outside");
      CHECK(result.hasError());
      CHECK(std::filesystem::exists(project.outsideSentinel()));
  }
  ```

- [ ] **Step 2: Run the facade test before implementation**

  ```powershell
  cmake --build build --config Debug --target codeqa_tests
  .\build\Debug\codeqa_tests.exe --test-case="ApplicationFacade*"
  ```

  Expected: compilation fails until the facade and its progress/config/delete interfaces exist.

- [ ] **Step 3: Implement the facade and refactor only CLI orchestration**

  Define phases `DiscoveringFiles`, `ParsingSourceFiles`, `MeasuringFunctions`, `ApplyingRules`, and `PreparingReport`, with `std::function<void(AnalysisPhase)>` callback. Extend `AnalyzeProject` to emit the first two around real discovery and parse work; have `RunAnalysis` forward that callback and emit metrics/rules. The facade emits `PreparingReport` while directly mapping the result and producing JSON for snapshots/CLI.

  Before a new snapshot, load the newest existing snapshot, use the existing multiset comparison and quality-gate rules, then save the raw report JSON. Save failure is a warning with a usable in-memory document. Reverse `ListSnapshots` only in facade UI results; CLI `snapshot list` stays ascending. `openSnapshot` parses it to `ReportDocument` and marks it `NotEvaluated` without writing.

  A missing `<root>/codeqa.json` returns defaults and `fileExists == false`; an existing unreadable config returns an error. Validate `toJson(config).dump()` through `parseConfig` before `SaveProjectConfig` writes prettified JSON. Accept for deletion only timestamps matching `YYYYMMDDTHHMMSSZ`, append `.json` below the exact snapshot directory, and remove with an error-code overload.

  Change `runAnalyze` to call the facade after its existing argument/config parsing. Preserve output formatting, raw snapshot content, warnings, comparison display, and `QualityGateFailed` exit code.

- [ ] **Step 4: Verify facade tests and all CLI behavior**

  ```powershell
  cmake --build build --config Debug
  .\build\Debug\codeqa_tests.exe --test-case="ApplicationFacade*,SaveProjectConfig*,DeleteSnapshot*,RunAnalysis*,History*"
  ctest --test-dir build -C Debug --output-on-failure
  ```

  Expected: existing CLI integration remains unchanged; new tests cover defaults, explicit save, UI history ordering, invalid previous snapshots, and deletion boundaries.

- [ ] **Step 5: Commit the shared application facade**

  ```powershell
  git add include/codeqa/application src/application src/cli/RunApplication.cpp tests/unit CMakeLists.txt tests/CMakeLists.txt
  git commit -m "feat: share analysis workflow through application facade"
  ```

## Task 4: Expose the facade to QML through asynchronous Qt models

**Files:**

- Create: `src/desktop/AppController.h`, `src/desktop/AppController.cpp`
- Create: `src/desktop/ReportModels.h`, `src/desktop/ReportModels.cpp`
- Modify: `src/desktop/main.cpp`, `src/desktop/CMakeLists.txt`, `tests/CMakeLists.txt`
- Create: `tests/desktop/AppControllerTest.cpp`

**Interfaces:**

- Consumes: `ApplicationFacade`, `ReportDocument`, QSettings, and Qt Concurrent.
- Produces context property `appController` with `analysisRunning`, `analysisPhase`, `errorMessage`, `qualityGateStatus`, and `findings`, `files`, and `snapshots` model properties. Its invokables are `startAnalysis`, `openReport`, `openSnapshot`, `refreshSnapshots`, `deleteSnapshot`, and `clearRecentPaths`.

- [ ] **Step 1: Write failing Qt adapter tests for document replacement**

  ```cpp
  void AppControllerTest::invalidImportKeepsTheCurrentReport()
  {
      AppController controller{temporarySettingsPath()};
      controller.openReport(validReportUrl());
      QTRY_VERIFY(controller.findings()->rowCount() == 1);

      controller.openReport(QUrl::fromLocalFile("Z:/missing/report.json"));

      QCOMPARE(controller.findings()->rowCount(), 1);
      QVERIFY(controller.errorMessage().contains("REPORT001"));
  }
  ```

- [ ] **Step 2: Build the new desktop test target and establish failure**

  Define `codeqa_desktop_support` for reusable Qt bridge code. Register `codeqa_desktop_tests` against `codeqa_desktop_support`, `Qt6::Test`, `Qt6::Core`, and `Qt6::Concurrent`, then run:

  ```powershell
  cmake --build build --config Debug --target codeqa_desktop_tests
  ctest --test-dir build -C Debug --output-on-failure -R codeqa_desktop_tests
  ```

  Expected: build failure because controller and models are absent.

- [ ] **Step 3: Implement safe worker dispatch and read-only models**

  `AppController` owns the facade, current `ReportDocument`, model instances, and a `QFutureWatcher<AnalysisFacadeResult>`. Start the facade with `QtConcurrent::run`; relay each phase to the GUI thread through queued `QMetaObject::invokeMethod`. Replace the current document/models only after a result contains a document. On error, retain existing rows and set `errorMessage`.

  Implement `FindingListModel` roles `severity`, `ruleId`, `symbolName`, `path`, `line`, `column`, `observedValue`, `limitValue`, `description`, `recommendation`, and `metricSummary`. Filter from the immutable document and sort Error, Warning, Information, then path/location. Implement `FileListModel` for secondary files/function metrics and `SnapshotListModel` for newest-first timestamps. None of these models alters `ReportDocument`.

  Persist only `recentProjects`, `recentReports`, last page, and filters under `QSettings("CodeQA", "CodeQA Desktop")`. `clearRecentPaths` removes the first two keys and their in-memory lists but retains the currently displayed report. Never persist source/report/snapshot bytes.

- [ ] **Step 4: Run adapter tests for success and failure cases**

  ```powershell
  cmake --build build --config Debug --target codeqa_desktop_tests
  ctest --test-dir build -C Debug --output-on-failure -R codeqa_desktop_tests
  ```

  Expected: test successful replacement, real-phase order, failed quality gate, missing/malformed report import, snapshot-open failure, and recent-path clearing.

- [ ] **Step 5: Commit the QML adapter boundary**

  ```powershell
  git add src/desktop/AppController.h src/desktop/AppController.cpp src/desktop/ReportModels.h src/desktop/ReportModels.cpp src/desktop/main.cpp src/desktop/CMakeLists.txt tests/desktop/AppControllerTest.cpp tests/CMakeLists.txt
  git commit -m "feat: expose CodeQA reports to QML"
  ```

## Task 5: Build the read-only, findings-first Results workspace

**Files:**

- Modify: `src/desktop/qml/Main.qml`, `tests/desktop/qml/tst_DesktopShell.qml`
- Create: `src/desktop/qml/pages/ResultsPage.qml`

**Interfaces:**

- Consumes: controller report models, `qualityGateStatus`, `currentReportChanged`, and filter invokables.
- Produces: sidebar-routed Results with filters, table, detail panel, summary, and a secondary file/metrics view.

- [ ] **Step 1: Extend the QML smoke test for findings-first, read-only behavior**

  ```qml
  function test_results_page_is_findings_first_and_read_only() {
      const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
      shell.selectPage("Results")
      compare(shell.findChild("resultsTitle").text, "Findings")
      verify(shell.findChild("findingTable") !== null)
      verify(shell.findChild("saveFindingButton") === null)
  }
  ```

- [ ] **Step 2: Run the QML test before implementing Results**

  ```powershell
  cmake --build build --config Debug --target codeqa_desktop_qml_smoke
  ctest --test-dir build -C Debug --output-on-failure -R codeqa_desktop_qml_smoke
  ```

  Expected: the smoke test fails because the Results page and its controls do not exist.

- [ ] **Step 3: Implement sidebar navigation and Results**

  Turn `Main.qml` into a persistent left `ColumnLayout` plus `StackLayout`, and provide `function selectPage(name)` for the smoke test. Its global **Open report…** action uses `FileDialog` from `QtQuick.Dialogs` and calls `appController.openReport` with the accepted local URL; it never creates a snapshot.

  `ResultsPage.qml` opens on Findings. Bind severity/rule/text controls to filter invokables, render `FindingListModel`, and bind a detail panel to the selected finding’s description, recommendation, observed/limit values, source location, and related metrics. Put files and function metrics under a secondary `TabBar`. Bind summary counts/gate outcome, including the exact `Not evaluated` text. Add no report editor, annotation, source action, save finding action, or suppression control.

- [ ] **Step 4: Run Results smoke and adapter tests**

  ```powershell
  cmake --build build --config Debug --target codeqa_desktop codeqa_desktop_tests codeqa_desktop_qml_smoke
  ctest --test-dir build -C Debug --output-on-failure -R "codeqa_desktop_(tests|qml_smoke)"
  ```

  Expected: Results is navigable, English labels match, a finding is filterable/selectable, and no report mutation UI exists.

- [ ] **Step 5: Commit Results**

  ```powershell
  git add src/desktop/qml/Main.qml src/desktop/qml/pages/ResultsPage.qml tests/desktop/qml/tst_DesktopShell.qml
  git commit -m "feat: add findings-first results workspace"
  ```

## Task 6: Add semantic configuration and analysis controls

**Files:**

- Modify: `src/desktop/AppController.{h,cpp}`, `src/desktop/ReportModels.{h,cpp}`, `src/desktop/qml/Main.qml`
- Create: `src/desktop/qml/pages/AnalyzePage.qml`
- Modify: `tests/desktop/AppControllerTest.cpp`, `tests/desktop/qml/tst_DesktopShell.qml`

**Interfaces:**

- Consumes: facade configuration/analysis methods and the rule/header-override models.
- Produces controller invokables `selectProject(const QUrl&)`, `saveConfiguration()`, `resetConfiguration()`, `setQualityGateSeverity(const QString&)`, and semantic rule/header-override mutation methods.

- [ ] **Step 1: Write failing tests for explicit config writes and phase UI**

  ```cpp
  void AppControllerTest::analysisUsesUnsavedFormValuesButDoesNotWriteConfiguration()
  {
      TempProject project{sourceWithWarningFinding()};
      AppController controller{temporarySettingsPath()};
      controller.selectProject(QUrl::fromLocalFile(project.root()));
      controller.rules()->setLimit("complexity.cyclomatic", 1, 2);
      controller.startAnalysis();
      QTRY_VERIFY(!controller.analysisRunning());

      QVERIFY(!std::filesystem::exists(project.root() / "codeqa.json"));
      QCOMPARE(controller.findings()->rowCount(), 1);
  }
  ```

  ```qml
  function test_analyze_page_has_semantic_controls() {
      const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
      shell.selectPage("Analyze")
      verify(shell.findChild("ruleThresholdTable") !== null)
      verify(shell.findChild("rawConfigEditor") === null)
      compare(shell.findChild("saveConfigurationButton").text, "Save")
  }
  ```

- [ ] **Step 2: Run targeted tests before the semantic form exists**

  ```powershell
  cmake --build build --config Debug --target codeqa_desktop_tests codeqa_desktop_qml_smoke
  ctest --test-dir build -C Debug --output-on-failure -R "codeqa_desktop_(tests|qml_smoke)"
  ```

  Expected: the controller/form contract is missing.

- [ ] **Step 3: Implement configuration and analysis state**

  Selecting a project loads `<project>/codeqa.json`; a missing file fills the form from `defaultConfig` and is marked unsaved. Expose eight ordered rule rows (`id`, `enabled`, `warningLimit`, `highLimit`), editable header overrides (`pattern`, `language` constrained to `c` or `cpp`), and an `information`/`warning`/`error` gate selector.

  `saveConfiguration` builds `ProjectConfig` from the semantic models and calls the facade. `resetConfiguration` refills only the form. `startAnalysis` passes the current in-memory form configuration to the facade and never saves it first. Disable conflicting project/config/snapshot-changing controls while a run is active, show an indeterminate `BusyIndicator`, and display precisely `Discovering files`, `Parsing source files`, `Measuring functions`, `Applying rules`, and `Preparing report`. On success select Results; on gate failure show its warning state there rather than an exception dialog.

- [ ] **Step 4: Verify config and analysis flow**

  ```powershell
  cmake --build build --config Debug --target codeqa_desktop_tests codeqa_desktop_qml_smoke
  ctest --test-dir build -C Debug --output-on-failure -R "codeqa_desktop_(tests|qml_smoke)"
  ```

  Expected: Save creates validated JSON, invalid data does not overwrite it, Reset is non-persistent, analysis uses unsaved values, and English phase/state copy is visible.

- [ ] **Step 5: Commit Analyze and config UI**

  ```powershell
  git add src/desktop/AppController.h src/desktop/AppController.cpp src/desktop/ReportModels.h src/desktop/ReportModels.cpp src/desktop/qml/Main.qml src/desktop/qml/pages/AnalyzePage.qml tests/desktop/AppControllerTest.cpp tests/desktop/qml/tst_DesktopShell.qml
  git commit -m "feat: add project analysis and semantic config UI"
  ```

## Task 7: Add History and finish Windows delivery verification

**Files:**

- Create: `src/desktop/qml/pages/HistoryPage.qml`, `docs/desktop-ui.md`
- Modify: `src/desktop/qml/Main.qml`, `src/desktop/AppController.{h,cpp}`, `src/desktop/CMakeLists.txt`
- Modify: `tests/desktop/AppControllerTest.cpp`, `tests/desktop/qml/tst_DesktopShell.qml`

**Interfaces:**

- Consumes: facade `listSnapshots`, `openSnapshot`, `deleteSnapshot`; snapshot model; QML `Dialog`; `windeployqt`.
- Produces: History behavior and CMake target `codeqa_desktop_deploy` which creates a local deployment directory.

- [ ] **Step 1: Write failing tests for non-destructive History actions**

  ```cpp
  void AppControllerTest::historyOpensWithoutCreatingAnotherSnapshot()
  {
      TempProject project{};
      project.writeSnapshot("20260101T000000Z", completeReportJson());
      AppController controller{temporarySettingsPath()};
      controller.selectProject(QUrl::fromLocalFile(project.root()));
      controller.refreshSnapshots();
      QCOMPARE(controller.snapshots()->data(controller.snapshots()->index(0), SnapshotListModel::TimestampRole).toString(), "20260101T000000Z");

      controller.openSnapshot("20260101T000000Z");

      QCOMPARE(project.snapshotCount(), 1);
      QCOMPARE(controller.qualityGateStatus(), "Not evaluated");
  }
  ```

- [ ] **Step 2: Run the History test before its implementation**

  ```powershell
  cmake --build build --config Debug --target codeqa_desktop_tests
  ctest --test-dir build -C Debug --output-on-failure -R codeqa_desktop_tests
  ```

  Expected: the test fails until snapshot UI/controller actions exist.

- [ ] **Step 3: Implement confirmed snapshot management and deployment**

  `HistoryPage.qml` provides a project folder picker, refresh action, and a `ListView` over newest-first snapshot rows. **Open** calls `openSnapshot`. **Delete** opens a confirmation `Dialog` naming the selected timestamp; only its accepted handler calls `deleteSnapshot`. On deletion success refresh the list. On error show an inline path/error message. Neither action creates a snapshot, and failures leave Results unchanged.

  Add custom target `codeqa_desktop_deploy`: copy `$<TARGET_FILE:codeqa_desktop>` to `${CMAKE_BINARY_DIR}/deploy/codeqa_desktop`, invoke the configured `windeployqt` executable with `--qmldir ${CMAKE_CURRENT_SOURCE_DIR}/qml --dir ${CMAKE_BINARY_DIR}/deploy/codeqa_desktop`, and depend on `codeqa_desktop`. When the deployment target is requested without `windeployqt` from the selected Qt installation, fail configuration with a clear diagnostic.

  Write `docs/desktop-ui.md` with required Qt modules, Qt-enabled and CLI-only configure commands, snapshot/config locations, local-only/QSettings privacy constraints, deploy command, and clean-Windows-machine manual import/launch check.

- [ ] **Step 4: Run the complete verification matrix**

  ```powershell
  cmake -S . -B build -DCODEQA_BUILD_DESKTOP=ON -DCODEQA_WARNINGS_AS_ERRORS=ON
  cmake --build build --config Debug
  ctest --test-dir build -C Debug --output-on-failure
  cmake --build build --config Debug --target codeqa_desktop_deploy
  ```

  Expected: core, CLI, Qt adapter, and QML tests pass. On a Windows machine with no developer Qt path, confirm `build/deploy/codeqa_desktop/codeqa_desktop.exe` starts, imports a copied version-1 report, and creates no files next to it.

- [ ] **Step 5: Commit History, deployment, and documentation**

  ```powershell
  git add src/desktop/qml/pages/HistoryPage.qml src/desktop/qml/Main.qml src/desktop/AppController.h src/desktop/AppController.cpp src/desktop/CMakeLists.txt tests/desktop/AppControllerTest.cpp tests/desktop/qml/tst_DesktopShell.qml docs/desktop-ui.md
  git commit -m "feat: add snapshot history to desktop UI"
  ```

## Plan Self-Review

### Specification coverage

| Requirement | Tasks |
| --- | --- |
| Windows Qt 6/QML executable alongside the CLI | 1, 7 |
| Shared Qt-free facade and direct fresh-result mapping | 2, 3 |
| Strict version-1 report import with no import writes | 2, 4, 5 |
| Real progress and quality-gate warning behavior | 3, 4, 6 |
| Semantic `codeqa.json` form and explicit persistence | 3, 6 |
| CLI-compatible snapshot path, newest-first history, and safe deletion | 3, 7 |
| English sidebar with Analyze, History, Results; findings first | 1, 5, 6, 7 |
| Read-only reports/findings | 2, 5 |
| Local `QSettings` recents and clear action | 4 |
| Core, adapter, QML, CLI, and packaged-Windows verification | 1 through 7 |

### Consistency checks

- Fresh analysis directly creates `ReportDocument`; raw JSON is produced only for the preserved snapshot/CLI contract, never reparsed for the UI.
- `PreparingReport` belongs to the facade, where document/JSON construction happens; the other four phases originate in the current pipeline.
- The existing ascending `ListSnapshots` behavior remains for CLI output; the facade reverses only its own UI list.
- Version-1 imported/snapshot JSON does not store a quality-gate decision, so `Not evaluated` is explicit rather than inferred.
- The plan contains no raw JSON editor, report mutation route, network feature, non-Windows deliverable, placeholder, or deferred implementation step.
