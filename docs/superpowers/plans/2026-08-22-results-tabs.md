# Results Tabs Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Split the Results view into four focused QML tabs, starting on `Overview`, while preserving finding filters, selection, file metrics, suppressed findings, and Markdown export.

**Architecture:** Keep `ResultsPage.qml` as a thin coordinator for the four-tab `TabBar`, `StackLayout`, shared controller, and export dialog. Extract the current summary, finding inspection, file metrics, and suppressed findings sections into four sibling QML components with explicit `controller` properties; keep finding selection and filter synchronization inside the Findings component.

**Tech Stack:** Qt 6.11, Qt Quick Controls, Qt Quick Layouts, QML, CMake, Qt Quick Test, CTest.

**Spec:** `docs/superpowers/specs/2026-08-22-results-tabs-design.md`

## Global Constraints

- The `TabBar` labels must be exactly `Overview`, `Findings`, `Files and metrics`, and `Suppressed`, in that order.
- `Overview` must be selected initially with `currentIndex: 0`.
- No C++ controller APIs, report serialization, or analysis semantics may change.
- Existing controller properties remain `findingSeverityFilter`, `findingRuleFilter`, `findingSearchText`, `findings`, `files`, and `suppressedFindings`.
- Do not add filtering, sorting, charts, or navigation outside the four requested tabs.
- Reuse the existing Warm Workbench controls and theme; do not introduce a second visual language.
- Preserve the existing empty states, model roles, Markdown export callback, and selected-finding behavior.
- Add new QML pages to both the production QML module and the desktop QML smoke-test resource list.

---

## File Map

**Create:**

- `src/desktop/qml/pages/ResultsOverview.qml` — overview heading, export action, quality-gate and severity summary cards, and report context row.
- `src/desktop/qml/pages/ResultsFindings.qml` — finding filters, list/detail split, selection state, filter synchronization, inline error, and empty state.
- `src/desktop/qml/pages/ResultsFilesMetrics.qml` — existing file metrics list and empty state.
- `src/desktop/qml/pages/ResultsSuppressed.qml` — existing suppressed findings list and empty state.

**Modify:**

- `src/desktop/qml/pages/ResultsPage.qml` — reduce to the shared export dialog plus four-tab shell and child wiring.
- `src/desktop/CMakeLists.txt` — register the four new production QML files in `qt_add_qml_module`.
- `tests/CMakeLists.txt` — copy and embed the four new QML files in `codeqa_desktop_qml_smoke`.
- `tests/desktop/qml/tst_DesktopShell.qml` — add the tab contract tests and select the Findings tab in tests that inspect finding delegates.

**Do not modify:** controller/model headers or implementations, report serialization, sidebar navigation, or unrelated untracked workspace artifacts.

---

### Task 1: Write the Results tab contract tests (RED)

**Files:**

- Modify: `tests/desktop/qml/tst_DesktopShell.qml` near the existing Results tests around `test_suppressed_findings_tab_lists_rows_and_shows_empty_state()`.
- Test: `tests/desktop/qml/tst_DesktopShell.qml`

**Interfaces:**

- Consumes: the current `Main.qml` shell and the existing `fixtureController`.
- Produces: executable expectations for `resultsTabs`, `overviewTab`, `findingsTab`, `filesMetricsTab`, `suppressedTab`, and `overviewSummary` that later QML changes must satisfy.

- [ ] **Step 1: Add a failing initial-tab test.**

Add this test before the existing suppressed-findings test:

```qml
    function test_results_tabs_start_on_overview_in_the_requested_order() {
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.controller = fixtureController
        shell.selectPage("Results")

        const tabs = shell.findChild("resultsTabs")
        const overviewTab = shell.findChild("overviewTab")
        const findingsTab = shell.findChild("findingsTab")
        const filesMetricsTab = shell.findChild("filesMetricsTab")
        const suppressedTab = shell.findChild("suppressedTab")

        verify(tabs !== null)
        verify(overviewTab !== null)
        if (!overviewTab) {
            return
        }

        compare(tabs.count, 4)
        compare(tabs.currentIndex, 0)
        compare(overviewTab.text, "Overview")
        compare(findingsTab.text, "Findings")
        compare(filesMetricsTab.text, "Files and metrics")
        compare(suppressedTab.text, "Suppressed")
        verify(shell.findChild("overviewSummary") !== null)
    }
```

- [ ] **Step 2: Add a failing focused-view test.**

Add a test that changes the tab index and checks the stable object names that the extracted components must preserve:

```qml
    function test_results_tabs_expose_findings_files_and_suppressed_views() {
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.controller = fixtureController
        shell.selectPage("Results")
        const tabs = shell.findChild("resultsTabs")

        verify(tabs !== null)
        tabs.currentIndex = 1
        verify(shell.findChild("findingTable") !== null)
        verify(shell.findChild("findingDetails") !== null)

        tabs.currentIndex = 2
        verify(shell.findChild("fileMetricsList") !== null)

        tabs.currentIndex = 3
        verify(shell.findChild("suppressedFindingsList") !== null)
    }
```

- [ ] **Step 3: Run the QML smoke test and verify the failure is meaningful.**

Run:

```powershell
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' --build buildQt --target codeqa_desktop_qml_smoke
$env:PATH = 'C:\Qt\6.11.0\llvm-mingw_64\bin;' + $env:PATH
& 'C:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir buildQt -R '^codeqa_desktop_qml_smoke$' --output-on-failure
```

Expected: the test executable runs, and the new initial-tab test fails because the current implementation exposes three tabs and has no `overviewTab`. Do not change production QML before observing this failure.

- [ ] **Step 4: Commit the RED tests.**

```powershell
git add -- tests/desktop/qml/tst_DesktopShell.qml
git commit -m "test: specify results tab contract"
```

### Task 2: Extract the four focused QML components and register them

**Files:**

- Create: `src/desktop/qml/pages/ResultsOverview.qml`
- Create: `src/desktop/qml/pages/ResultsFindings.qml`
- Create: `src/desktop/qml/pages/ResultsFilesMetrics.qml`
- Create: `src/desktop/qml/pages/ResultsSuppressed.qml`
- Modify: `src/desktop/CMakeLists.txt:105-118`
- Modify: `tests/CMakeLists.txt:242-345`

**Interfaces:**

- Consumes: the existing controls (`SummaryCard`, `FilterBar`, `FindingRow`, `FindingDetails`, and `WorkbenchSurface`) and the controller properties listed in the spec.
- Produces: four importable local QML components, each with `property var controller: null`; `ResultsOverview` and `ResultsFindings` also produce `signal exportMarkdownRequested()`.

- [ ] **Step 1: Create the shared component skeletons with explicit interfaces.**

Each new file must use this root shape and import the existing controls:

```qml
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import ".." as Root
import "../controls"

Item {
    id: root

    property var controller: null
}
```

Add `signal exportMarkdownRequested()` only to `ResultsOverview.qml` and `ResultsFindings.qml`; their export buttons call `root.exportMarkdownRequested()`.

- [ ] **Step 2: Build `ResultsOverview.qml` from the existing summary block.**

Use a full-size `ColumnLayout` with the existing heading style and `SummaryCard` controls. Preserve the existing object names `qualityGateSummaryCard`, `findingsSummaryCard`, `errorsSummaryCard`, and `warningsSummaryCard`; add `objectName: "overviewSummary"` to the `GridLayout`. Keep the current values:

```qml
value: root.controller ? root.controller.qualityGateStatus : "Not evaluated"
value: String(root.controller ? root.controller.findings.findingCount : 0)
trend: String(root.controller ? root.controller.files.fileCount : 0) + " files analyzed"
value: String(root.controller ? root.controller.findings.errorCount : 0)
value: String(root.controller ? root.controller.findings.warningCount : 0)
```

Add a compact `WorkbenchSurface` context row with `objectName: "overviewContext"` that displays the analyzed-file count and current quality-gate status. Do not access `suppressedFindings.rowCount()` from QML because that model has no summary property.

- [ ] **Step 3: Build `ResultsFindings.qml` from the current first-stack-item content.**

Move the `selectedFinding` property, `selectFinding()`, `clearFindingSelection()`, and the `Connections` that mirrors controller filters into this component. The existing export button emits `exportMarkdownRequested()` instead of opening a dialog locally; the parent keeps the dialog and opens it from the signal. Retain the `exportMarkdownButton` object name on the Findings export button.

Preserve these object names and behavior: `resultsTitle`, `filterBar` behavior, `resultsErrorMessage`, `findingTable`, `findingEmptyState`, and `findingDetails`. Keep the existing two-column `RowLayout`, but let it consume the full available tab height without the summary cards above it. The finding delegate roles remain `severity`, `ruleId`, `symbolName`, `path`, `line`, `column`, `observedValue`, `limitValue`, `description`, `recommendation`, and `metricSummary`.

- [ ] **Step 4: Build `ResultsFilesMetrics.qml` from the current file metrics stack item.**

Keep `fileMetricsList`, every required delegate property, `metricsColumn`, function-metric labels, and the existing `No file metrics are available.` empty state unchanged. Only replace the outer stack-item wrapper with an `Item` whose contents fill the component.

- [ ] **Step 5: Build `ResultsSuppressed.qml` from the current suppressed stack item.**

Keep `suppressedFindingsList`, the roles `ruleId`, `symbolName`, `path`, `line`, and `column`, the existing row layout, `suppressedFindingsEmptyLabel`, and the text `No suppressed findings in this run.` unchanged. Only replace the outer stack-item wrapper with an `Item` whose contents fill the component.

- [ ] **Step 6: Register the new files in production and smoke-test resources.**

Add these entries to `src/desktop/CMakeLists.txt` inside `qt_add_qml_module(codeqa_desktop ... QML_FILES)`:

```cmake
        qml/pages/ResultsOverview.qml
        qml/pages/ResultsFindings.qml
        qml/pages/ResultsFilesMetrics.qml
        qml/pages/ResultsSuppressed.qml
```

Add `configure_file(... COPYONLY)` entries for all four files after the existing `ResultsPage.qml` copy in `tests/CMakeLists.txt`, and add the four generated paths to the `qt_add_resources(codeqa_desktop_qml_smoke desktop_shell_resources FILES ...)` list. No new `qmldir` entry is needed because these are local files under the `pages` resource directory.

- [ ] **Step 7: Build the QML smoke target to verify all extracted components compile.**

Run:

```powershell
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' --build buildQt --target codeqa_desktop_qml_smoke
```

Expected: the target configures and compiles the four new QML resources. The new contract tests remain red until the Results shell is rewired.

- [ ] **Step 8: Commit the extracted components and resource registration.**

```powershell
git add -- src/desktop/qml/pages/ResultsOverview.qml src/desktop/qml/pages/ResultsFindings.qml src/desktop/qml/pages/ResultsFilesMetrics.qml src/desktop/qml/pages/ResultsSuppressed.qml src/desktop/CMakeLists.txt tests/CMakeLists.txt
git commit -m "refactor: split results views into qml components"
```

### Task 3: Rewire `ResultsPage` into the four-tab shell (GREEN)

**Files:**

- Modify: `src/desktop/qml/pages/ResultsPage.qml`
- Modify: `tests/desktop/qml/tst_DesktopShell.qml`

**Interfaces:**

- Consumes: the four components and their `controller`/`exportMarkdownRequested` interfaces from Task 2.
- Produces: a `ResultsPage` shell whose tab buttons are named `overviewTab`, `findingsTab`, `filesMetricsTab`, and `suppressedTab`, with `resultsTabs.currentIndex: 0`.

- [ ] **Step 1: Replace the existing mixed StackLayout body with the shell.**

Keep the current `FileDialog` in `ResultsPage.qml` and keep its `onAccepted` call to `root.controller.exportMarkdown(selectedFile)`. The `ColumnLayout` should contain:

```qml
TabBar {
    id: resultsTabs
    objectName: "resultsTabs"
    Layout.fillWidth: true
    currentIndex: 0

    TabButton { id: overviewTab; objectName: "overviewTab"; text: "Overview" }
    TabButton { id: findingsTab; objectName: "findingsTab"; text: "Findings" }
    TabButton { id: filesMetricsTab; objectName: "filesMetricsTab"; text: "Files and metrics" }
    TabButton { id: suppressedTab; objectName: "suppressedTab"; text: "Suppressed" }
}

StackLayout {
    Layout.fillWidth: true
    Layout.fillHeight: true
    currentIndex: resultsTabs.currentIndex

    ResultsOverview {
        objectName: "resultsOverviewPage"
        controller: root.controller
        onExportMarkdownRequested: exportMarkdownDialog.open()
    }
    ResultsFindings {
        objectName: "resultsFindingsPage"
        controller: root.controller
        onExportMarkdownRequested: exportMarkdownDialog.open()
    }
    ResultsFilesMetrics {
        objectName: "resultsFilesMetricsPage"
        controller: root.controller
    }
    ResultsSuppressed {
        objectName: "resultsSuppressedPage"
        controller: root.controller
    }
}
```

Do not add a second controller or duplicate filter state in `ResultsPage`.

- [ ] **Step 2: Update finding-specific tests to select the Findings tab.**

In `test_selecting_a_finding_shows_its_details()` and `test_filtering_out_the_selected_finding_clears_details()`, set `shell.findChild("resultsTabs").currentIndex = 1` immediately after selecting the Results page and before reading `findingTable`. Keep all existing selection and filter assertions unchanged. In `test_suppressed_findings_tab_lists_rows_and_shows_empty_state()`, change the tab index from `2` to `3`.

- [ ] **Step 3: Run the targeted QML smoke test and verify GREEN.**

Run:

```powershell
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' --build buildQt --target codeqa_desktop_qml_smoke
$env:PATH = 'C:\Qt\6.11.0\llvm-mingw_64\bin;' + $env:PATH
& 'C:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir buildQt -R '^codeqa_desktop_qml_smoke$' --output-on-failure
```

Expected: `codeqa_desktop_qml_smoke` passes, including the new four-tab tests and the existing finding/filter/suppressed behavior tests.

- [ ] **Step 4: Commit the shell integration and test updates.**

```powershell
git add -- src/desktop/qml/pages/ResultsPage.qml tests/desktop/qml/tst_DesktopShell.qml
git commit -m "feat: add overview results tab"
```

### Task 4: Verify the complete implementation and hand off

**Files:**

- Verify: `src/desktop/qml/pages/ResultsPage.qml`
- Verify: `src/desktop/qml/pages/ResultsOverview.qml`
- Verify: `src/desktop/qml/pages/ResultsFindings.qml`
- Verify: `src/desktop/qml/pages/ResultsFilesMetrics.qml`
- Verify: `src/desktop/qml/pages/ResultsSuppressed.qml`
- Verify: `tests/desktop/qml/tst_DesktopShell.qml`

**Interfaces:**

- Consumes: the completed four-tab Results implementation.
- Produces: fresh build/test evidence and a clean tracked worktree.

- [ ] **Step 1: Check formatting and the final diff.**

Run:

```powershell
git diff --check HEAD~3 HEAD
git diff --stat HEAD~3 HEAD
git status --short --branch
```

Expected: no whitespace errors; only the intended Results QML, CMake, test, and plan/spec commits are tracked changes. Existing unrelated untracked paths (`.superpowers/`, build directories, `pictures/`, `tmp/`, and `AGENTS.md`) remain untouched.

- [ ] **Step 2: Build and run the complete MSVC CTest suite.**

Run:

```powershell
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' --build build-ui --config Debug
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$env:PATH = 'C:\Qt\6.11.0\msvc2022_64\bin;' + $env:PATH
& 'C:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir build-ui -C Debug --output-on-failure
```

Expected: all 15 tests pass, including `codeqa_desktop_qml_smoke`.

- [ ] **Step 3: Record the final commit and verification output.**

Run:

```powershell
git show --stat --oneline --summary HEAD
git diff --check HEAD~1 HEAD
git status --short --branch
```

Report the four tab labels, `Overview` as the initial tab, the preserved Findings list/detail behavior, and the exact test count from the fresh CTest run.
