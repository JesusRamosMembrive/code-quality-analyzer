# Warm Workbench UI Alternative Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the approved Warm Workbench visual redesign in the CodeQA Qt 6/QML desktop application, starting with the shared shell and findings-first Results page, then applying the same component system to Analyze and History.

**Architecture:** Keep `AppController`, `ReportModels`, the core services, report schema, configuration semantics, and snapshot storage unchanged. Add a QML-only theme and component layer, refactor `Main.qml` into a Workbench shell, and compose the three pages from reusable surfaces, controls, summary cards, finding rows, and detail panels.

**Tech Stack:** C++17 application, Qt 6 QML/Quick/Quick Controls 2/Quick Effects, Qt Quick Test, CMake, Windows desktop target.

**Spec:** `docs/superpowers/specs/2026-08-22-ui-alternative-design.md`

## Global Constraints

- Keep all visible UI strings in English.
- Keep Results read-only and findings-first.
- Keep analysis, report import, configuration, snapshots, and recent-path behavior in `AppController` and the existing core services.
- Keep the UI self-contained in CodeQA; `QML-SnippetsExamples` is a reference source, not a runtime or build dependency.
- Do not change analysis rules, report schema, configuration semantics, snapshot storage, or C++ application-service behavior.
- Preserve existing QML controller-facing properties, invokable methods, and important test object names unless the task explicitly adds a new visual object name.
- Use role-based theme tokens instead of scattering Warm Workbench color literals through page files.
- Use responsive QML layouts; do not implement the desktop layout with page-specific pixel coordinates.
- Preserve the current minimum desktop window size and make narrower windows usable.
- Run the Qt-free CTest suite and the Qt/QML smoke test before claiming completion.

---

### Task 1: Add Warm Workbench theme tokens and primitive controls

**Files:**
- Create: `src/desktop/qml/CodeQaTheme.qml`
- Create: `src/desktop/qml/controls/WorkbenchSurface.qml`
- Create: `src/desktop/qml/controls/WorkbenchButton.qml`
- Create: `src/desktop/qml/controls/StatusBadge.qml`
- Modify: `src/desktop/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- `CodeQaTheme` exposes the approved color roles (`canvas`, `surface`, `surfaceSubtle`, `ink`, `mutedInk`, `border`, `sidebar`, `sidebarText`, `accent`, `accentSoft`, `success`, `error`, `warning`, `link`), spacing steps, corner radii, and standard dimensions.
- `WorkbenchSurface` exposes `property bool selected`, `property bool elevated`, and `property color tintColor`; its default visual is a Warm Workbench bordered surface.
- `WorkbenchButton` exposes `property string variant` with the values `primary`, `secondary`, and `quiet`; its `text`, `enabled`, `hovered`, `pressed`, `activeFocus`, and `highlighted` states remain compatible with `QtQuick.Templates as T`.
- `StatusBadge` exposes `property string status` with `success`, `error`, `warning`, `neutral`, and `not-evaluated` values plus the existing `text` property.

- [ ] **Step 1: Define the theme roles and dimensions.**

  Create `CodeQaTheme.qml` as a QML singleton and use the exact values from the approved spec:

  ```qml
  pragma Singleton
  import QtQuick

  QtObject {
      readonly property color canvas: "#f3eee6"
      readonly property color surface: "#fffdf9"
      readonly property color surfaceSubtle: "#ebe4d9"
      readonly property color ink: "#342f2a"
      readonly property color mutedInk: "#81776b"
      readonly property color border: "#ded5c8"
      readonly property color sidebar: "#2f3835"
      readonly property color sidebarText: "#dbe3dc"
      readonly property color accent: "#d59a3f"
      readonly property color accentSoft: "#f6e7c7"
      readonly property color success: "#4f9276"
      readonly property color error: "#bd5e51"
      readonly property color warning: "#b27b2d"
      readonly property color link: "#4f6585"

      readonly property int spaceXs: 4
      readonly property int spaceSm: 8
      readonly property int spaceMd: 12
      readonly property int spaceLg: 18
      readonly property int spaceXl: 24
      readonly property int radiusSm: 7
      readonly property int radiusMd: 11
      readonly property int radiusLg: 14
      readonly property int sidebarWidth: 208
      readonly property int compactSidebarWidth: 64
      readonly property int contentMaxWidth: 1480
  }
  ```

- [ ] **Step 2: Implement the reusable surface.**

  Make `WorkbenchSurface.qml` a `Rectangle` with `color` bound to `tintColor` when provided, otherwise `CodeQaTheme.surface`; use `CodeQaTheme.border` for the 1px border, `CodeQaTheme.radiusMd` for the default radius, and a subtle shadow only when `elevated` is true. `selected` must switch the border and background to `CodeQaTheme.accent` and `CodeQaTheme.accentSoft` without changing the content API.

- [ ] **Step 3: Implement button variants and focus states.**

  Derive `WorkbenchButton.qml` from `QtQuick.Templates as T`. Map `variant` to these states: `primary` uses amber fill with white text, `secondary` uses `surface` with ink text and border, and `quiet` uses transparent fill with muted text. Add a visible `border.width` or focus ring whenever `activeFocus` is true; do not rely only on hover or color change for keyboard focus.

- [ ] **Step 4: Implement semantic status badges.**

  Render a compact pill with a colored dot and text. Map `success` to green, `error` to red, `warning` to amber, `neutral` to muted ink, and `not-evaluated` to muted ink on `surfaceSubtle`. Keep the component usable from both summary cards and inline error/status messages.

- [ ] **Step 5: Register the QML types and smoke-test resources.**

  In `src/desktop/CMakeLists.txt`, add the new files to `qt_add_qml_module(codeqa_desktop ...)`, set `QT_QML_SINGLETON_TYPE TRUE` on `qml/CodeQaTheme.qml`, and add stable aliases matching the existing `qml/...` resource layout. In `tests/CMakeLists.txt`, copy the same files into the QML smoke-test resource tree and add them to `qt_add_resources`.

- [ ] **Step 6: Compile the desktop module and check the patch.**

  Run:

  ```powershell
  cmake --build buildQt --config Debug --target codeqa_desktop
  git diff --check
  ```

  Expected: the desktop target compiles with no QML resource or singleton-registration error. The build directory is not added to Git.

- [ ] **Step 7: Commit the theme primitives.**

  ```powershell
  git add src/desktop/qml/CodeQaTheme.qml src/desktop/qml/controls/WorkbenchSurface.qml src/desktop/qml/controls/WorkbenchButton.qml src/desktop/qml/controls/StatusBadge.qml src/desktop/CMakeLists.txt tests/CMakeLists.txt
  git commit -m "feat: add warm workbench theme primitives"
  ```

### Task 2: Replace the glass shell with the Workbench header and sidebar

**Files:**
- Create: `src/desktop/qml/controls/WorkbenchSidebar.qml`
- Create: `src/desktop/qml/controls/WorkbenchHeader.qml`
- Modify: `src/desktop/qml/Main.qml`
- Modify: `tests/desktop/qml/tst_DesktopShell.qml`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- `WorkbenchSidebar` consumes `property string selectedPage`, `property bool analysisRunning`, and emits `signal pageRequested(string page)`; its Open report and Clear recent paths buttons remain wired by `Main.qml` to the existing dialog and controller actions.
- `WorkbenchHeader` consumes `property string projectName`, `property string branchText`, and `property string pageTitle`; it exposes `objectName: "workbenchHeader"`.
- `Main.qml` continues to expose `selectPage(name)`, `findChild(childObjectName)`, and the existing controller binding.
- Existing object names `analyzeNavigationButton`, `historyNavigationButton`, `resultsNavigationButton`, `openReportButton`, and `clearRecentPathsButton` remain available to the QML tests.

- [ ] **Step 1: Add a failing shell test for the new visual boundaries.**

  Add this test to `tests/desktop/qml/tst_DesktopShell.qml` before changing `Main.qml`:

  ```qml
  function test_warm_workbench_shell_exposes_header_and_sidebar() {
      const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
      verify(shell.findChild("workbenchHeader") !== null)
      verify(shell.findChild("workbenchSidebar") !== null)
      compare(shell.findChild("analyzeNavigationButton").text, "Analyze")
      compare(shell.findChild("historyNavigationButton").text, "History")
      compare(shell.findChild("resultsNavigationButton").text, "Results")
  }
  ```

  Run `ctest --test-dir buildQt -C Debug -R codeqa_desktop_qml_smoke --output-on-failure` and confirm the new test fails because the new object names do not exist yet.

- [ ] **Step 2: Implement `WorkbenchSidebar.qml`.**

  Use a `ColumnLayout` over `CodeQaTheme.sidebar`, with sections labeled `Workspace` and `Explore`, navigation items for Analyze, History, and Results, and a bottom project-context surface. Highlight the active item with `CodeQaTheme.accent` and `CodeQaTheme.ink`. Keep the global Open report and Clear recent paths buttons in the bottom action area. Use `WorkbenchButton` for actions and a small internal navigation delegate for page requests.

- [ ] **Step 3: Implement `WorkbenchHeader.qml`.**

  Build a top `WorkbenchSurface`/`Rectangle` with CodeQA identity on the left and project name, branch/current context, and a compact avatar/status treatment on the right. `pageTitle` is a display property only; page navigation remains controlled by `Main.qml`.

- [ ] **Step 4: Refactor `Main.qml` around the new shell.**

  Remove the `backdrop` decorative circles, `GlassPanel` shell instance, and glass-specific palette. Set the application/window palette to the theme roles. Compose the window as:

  ```qml
  RowLayout {
      anchors.fill: parent
      spacing: 0

      WorkbenchSidebar {
          id: workbenchSidebar
          objectName: "workbenchSidebar"
          Layout.fillHeight: true
          Layout.preferredWidth: CodeQaTheme.sidebarWidth
          selectedPage: shell.selectedPage
          analysisRunning: shell.controller ? shell.controller.analysisRunning : false
          onPageRequested: shell.selectPage(page)
      }

      ColumnLayout {
          Layout.fillWidth: true
          Layout.fillHeight: true
          spacing: 0

          WorkbenchHeader {
              id: workbenchHeader
              objectName: "workbenchHeader"
              Layout.fillWidth: true
              pageTitle: shell.selectedPage
              projectName: shell.controller ? shell.controller.currentProject.toString() : "No project selected"
          }

          StackLayout {
              id: pageStack
              Layout.fillWidth: true
              Layout.fillHeight: true
              Layout.margins: CodeQaTheme.spaceXl
              currentIndex: shell.selectedPage === "Analyze" ? 0 : shell.selectedPage === "History" ? 1 : 2
              // Keep the existing AnalyzePage, HistoryPage, and ResultsPage instances.
          }
      }
  }
  ```

  Wire `openReportButton` to the existing `FileDialog` and `clearRecentPathsButton` to the existing controller call. Do not alter `Connections` behavior that switches to Results after `currentReportChanged`.

- [ ] **Step 5: Register the shell controls in the smoke resources.**

  Add `WorkbenchSidebar.qml` and `WorkbenchHeader.qml` to `src/desktop/CMakeLists.txt` and `tests/CMakeLists.txt` using the same copy/resource pattern established in Task 1.

- [ ] **Step 6: Run the shell-focused test and commit.**

  Run:

  ```powershell
  cmake --build buildQt --config Debug --target codeqa_desktop_qml_smoke
  ctest --test-dir buildQt -C Debug -R codeqa_desktop_qml_smoke --output-on-failure
  ```

  Expected: the new header/sidebar test passes and the existing three-page navigation and global-action tests remain green.

- [ ] **Step 7: Commit the shell migration.**

  ```powershell
  git add src/desktop/qml/Main.qml src/desktop/qml/controls/WorkbenchSidebar.qml src/desktop/qml/controls/WorkbenchHeader.qml src/desktop/CMakeLists.txt tests/CMakeLists.txt tests/desktop/qml/tst_DesktopShell.qml
  git commit -m "feat: add warm workbench desktop shell"
  ```

### Task 3: Rebuild Results from reusable Workbench components

**Files:**
- Create: `src/desktop/qml/controls/SummaryCard.qml`
- Create: `src/desktop/qml/controls/FilterBar.qml`
- Create: `src/desktop/qml/controls/FindingRow.qml`
- Create: `src/desktop/qml/controls/FindingDetails.qml`
- Modify: `src/desktop/qml/pages/ResultsPage.qml`
- Modify: `src/desktop/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/desktop/qml/tst_DesktopShell.qml`

**Interfaces:**
- `SummaryCard` consumes `property string label`, `property string value`, `property string trend`, and `property string status`.
- `FilterBar` consumes `property string severityValue`, `property string ruleValue`, and `property string searchValue`; it emits `signal severitySelected(string severity)`, `signal ruleEdited(string ruleId)`, and `signal searchEdited(string searchText)`.
- `FindingRow` exposes the exact model roles already provided by `FindingListModel`: `severity`, `ruleId`, `symbolName`, `path`, `line`, `column`, `observedValue`, `limitValue`, `description`, `recommendation`, and `metricSummary`; it emits `signal findingSelected(var finding)` and has `property bool selected`.
- `FindingDetails` consumes `property var finding` and preserves `objectName: "findingDetails"` on its title label so existing tests can inspect the selected rule ID.

- [ ] **Step 1: Add the Results acceptance tests before refactoring the page.**

  Add these checks to `tests/desktop/qml/tst_DesktopShell.qml`:

  ```qml
  function test_results_has_workbench_summary_cards_and_detail_surface() {
      resetFixtureFindings()
      const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
      shell.controller = fixtureController
      shell.selectPage("Results")

      verify(shell.findChild("qualityGateSummaryCard") !== null)
      verify(shell.findChild("findingsSummaryCard") !== null)
      verify(shell.findChild("errorsSummaryCard") !== null)
      verify(shell.findChild("warningsSummaryCard") !== null)
      verify(shell.findChild("findingTable") !== null)
      verify(shell.findChild("findingDetails") !== null)
      verify(shell.findChild("saveFindingButton") === null)
  }

  function test_results_row_selection_keeps_existing_finding_values() {
      resetFixtureFindings()
      const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
      shell.controller = fixtureController
      shell.selectPage("Results")

      const table = shell.findChild("findingTable")
      tryVerify(function() { return table.count === 2 })
      mouseClick(table.itemAtIndex(1), table.itemAtIndex(1).width / 2, table.itemAtIndex(1).height / 2)

      compare(shell.findChild("findingDetails").text, "second.rule")
      verify(shell.findChild("findingDetailsDescription").text.indexOf("second finding") !== -1)
      verify(shell.findChild("findingDetailsRecommendation").text.indexOf("reduce parameters") !== -1)
  }
  ```

  Run the QML smoke test and confirm the new tests fail because the Workbench summary-card object names do not exist.

- [ ] **Step 2: Implement `SummaryCard.qml` and connect controller counts.**

  Use a `WorkbenchSurface` with a small uppercase label, a prominent value, an optional trend, and a status variant. In `ResultsPage.qml`, preserve these existing bindings exactly: `controller.files.fileCount`, `controller.findings.findingCount`, `controller.findings.errorCount`, `controller.findings.warningCount`, and `controller.qualityGateStatus`. Add object names `qualityGateSummaryCard`, `findingsSummaryCard`, `errorsSummaryCard`, and `warningsSummaryCard` to the four cards.

- [ ] **Step 3: Implement `FilterBar.qml` without changing controller semantics.**

  Replace the page-local filter row with a reusable component. Keep the existing severity mapping (`""`, `"error"`, `"warning"`, `"information"`) and call the same `AppController` invokables:

  ```qml
  onSeveritySelected: root.controller.setFindingSeverityFilter(severity)
  onRuleEdited: root.controller.setFindingRuleFilter(ruleId)
  onSearchEdited: root.controller.setFindingSearchText(searchText)
  ```

  Preserve object names `findingSeverityFilter`, `findingRuleFilter`, and `findingSearchFilter` on the underlying controls so existing tests remain meaningful.

- [ ] **Step 4: Implement `FindingRow.qml` with the approved table hierarchy.**

  Use a `RowLayout` with severity, rule ID, path/location, and symbol columns. Use `Text.ElideRight` for rule and symbol and `Text.ElideMiddle` for paths. Put the selected state on the surrounding `WorkbenchSurface`; use `TapHandler` to emit a complete finding object containing every role required by `FindingDetails`.

- [ ] **Step 5: Implement `FindingDetails.qml` as a dedicated read-only panel.**

  Show the selected severity badge, rule title, description, recommendation, path/location, observed versus limit values, and metric summary. Use explicit object names `findingDetails`, `findingDetailsDescription`, `findingDetailsRecommendation`, `findingDetailsObservedValue`, `findingDetailsLimitValue`, `findingDetailsPath`, `findingDetailsLocation`, and `findingDetailsMetrics`. When `finding` is null, show `Finding details` and `Select a finding to inspect its details.`

- [ ] **Step 6: Refactor `ResultsPage.qml` while keeping secondary tabs.**

  Preserve the existing `resultsTabs` and `StackLayout` so Files and metrics and Suppressed remain available. The first tab must compose the heading/actions, summary cards, `FilterBar`, `findingTable`, and `FindingDetails`. Keep the current `clearFindingSelection()` behavior on report/filter changes and keep the existing export dialog/controller call. Replace hardcoded dark colors with `CodeQaTheme` and reusable components.

- [ ] **Step 7: Register the Results components and run focused tests.**

  Add the four control files to both QML module resource lists. Run:

  ```powershell
  cmake --build buildQt --config Debug --target codeqa_desktop_qml_smoke
  ctest --test-dir buildQt -C Debug -R codeqa_desktop_qml_smoke --output-on-failure
  ```

  Expected: the new summary-card and detail tests pass; the existing tests for filtering, suppressed findings, file metrics, export, and read-only Results also pass.

- [ ] **Step 8: Commit the Results redesign.**

  ```powershell
  git add src/desktop/qml/controls/SummaryCard.qml src/desktop/qml/controls/FilterBar.qml src/desktop/qml/controls/FindingRow.qml src/desktop/qml/controls/FindingDetails.qml src/desktop/qml/pages/ResultsPage.qml src/desktop/CMakeLists.txt tests/CMakeLists.txt tests/desktop/qml/tst_DesktopShell.qml
  git commit -m "feat: redesign results with warm workbench components"
  ```

### Task 4: Restyle Analyze and History without changing workflows

**Files:**
- Modify: `src/desktop/qml/pages/AnalyzePage.qml`
- Modify: `src/desktop/qml/pages/HistoryPage.qml`
- Modify: `src/desktop/qml/controls/Section.qml`
- Modify: `tests/desktop/qml/tst_DesktopShell.qml`

**Interfaces:**
- Keep every existing controller call in `AnalyzePage.qml`: `selectProject`, `setRuleEnabled`, `setRuleLimits`, `updateHeaderOverride`, `removeHeaderOverride`, `addHeaderOverride`, `saveConfiguration`, `resetConfiguration`, and `startAnalysis`.
- Keep every existing controller call in `HistoryPage.qml`: `selectProject`, `refreshSnapshots`, `openSnapshot`, `deleteSnapshot`, and `compareSnapshots`.
- Preserve current object names `selectProjectButton`, `startAnalysisButton`, `saveConfigurationButton`, `resetConfigurationButton`, `ruleThresholdTable`, `headerOverrideTable`, `historySelectProjectButton`, `refreshSnapshotsButton`, `snapshotList`, `compareSnapshotsButton`, and `historyErrorMessage`.

- [ ] **Step 1: Add regression assertions for existing page actions.**

  Extend the existing Analyze and History tests with visual-boundary checks:

  ```qml
  function test_analyze_uses_workbench_surfaces_and_keeps_semantic_controls() {
      const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
      shell.controller = fixtureController
      shell.selectPage("Analyze")
      verify(shell.findChild("analyzePageSurface") !== null)
      verify(shell.findChild("ruleThresholdTable") !== null)
      verify(shell.findChild("headerOverrideTable") !== null)
      compare(shell.findChild("startAnalysisButton").text, "Analyze project")
  }

  function test_history_uses_workbench_surfaces_and_keeps_snapshot_actions() {
      resetHistoryCalls()
      const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
      shell.controller = fixtureController
      shell.selectPage("History")
      verify(shell.findChild("historyPageSurface") !== null)
      verify(shell.findChild("snapshotList") !== null)
      verify(shell.findChild("compareSnapshotsButton") !== null)
      verify(shell.findChild("refreshSnapshotsButton").enabled)
  }
  ```

  Run the QML smoke test and confirm these new object names fail before the restyle.

- [ ] **Step 2: Replace `Section.qml` styling with a Workbench section header.**

  Keep its existing `property string title` and `ColumnLayout` usage, but bind its separator, title color, font, spacing, and margins to `CodeQaTheme`. Do not change the pages' existing section titles or child layout APIs.

- [ ] **Step 3: Restyle Analyze page surfaces and controls.**

  Add a top Workbench surface for project selection and analysis status, wrap Rules, Header language overrides, Quality gate, and action footer in `WorkbenchSurface`, replace `PulseButton` instances with `WorkbenchButton` variants, and replace hardcoded dark colors with theme roles. Keep the real analysis phase, busy indicator, inline errors, disabled-state logic, semantic rule editors, and explicit Save/Reset behavior unchanged.

- [ ] **Step 4: Restyle History page surfaces and snapshot rows.**

  Add `objectName: "historyPageSurface"` to the main history content surface, use Workbench surfaces for the project selector, comparison controls, snapshot list rows, and inline errors, and preserve the delete confirmation dialog's timestamp text and controller call. Keep newest-first ordering and the non-destructive open behavior.

- [ ] **Step 5: Run the regression suite and commit.**

  Run:

  ```powershell
  cmake --build buildQt --config Debug --target codeqa_desktop_qml_smoke
  ctest --test-dir buildQt -C Debug -R codeqa_desktop_qml_smoke --output-on-failure
  ```

  Expected: all existing Analyze and History behavior tests pass, including invalid configuration, progress disabling, snapshot ordering, deletion confirmation, and comparison counts.

- [ ] **Step 6: Commit the remaining page restyle.**

  ```powershell
  git add src/desktop/qml/pages/AnalyzePage.qml src/desktop/qml/pages/HistoryPage.qml src/desktop/qml/controls/Section.qml tests/desktop/qml/tst_DesktopShell.qml
  git commit -m "feat: restyle analyze and history pages"
  ```

### Task 5: Add responsive behavior, keyboard focus, and empty/error states

**Files:**
- Modify: `src/desktop/qml/Main.qml`
- Modify: `src/desktop/qml/controls/WorkbenchSidebar.qml`
- Modify: `src/desktop/qml/controls/WorkbenchButton.qml`
- Modify: `src/desktop/qml/controls/FindingRow.qml`
- Modify: `src/desktop/qml/controls/FindingDetails.qml`
- Modify: `src/desktop/qml/pages/ResultsPage.qml`
- Modify: `tests/desktop/qml/tst_DesktopShell.qml`

**Interfaces:**
- Keep the minimum window size at 960×600 unless the visual check proves a larger minimum is required for a usable table.
- Keep keyboard navigation order: sidebar navigation, header actions, filters, finding rows, then detail actions.
- Keep empty-state copy exact: `No findings match the current filters.` and `Select a finding to inspect its details.`

- [ ] **Step 1: Add narrow-window and empty-state tests.**

  Add these QML checks:

  ```qml
  function test_results_empty_state_remains_visible_after_filtering_all_findings() {
      resetFixtureFindings()
      const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
      shell.controller = fixtureController
      shell.selectPage("Results")
      fixtureController.setFindingSearchText("does-not-exist")
      const table = shell.findChild("findingTable")
      tryVerify(function() { return table.count === 0 })
      verify(shell.findChild("findingEmptyState").visible)
      compare(shell.findChild("findingEmptyState").text, "No findings match the current filters.")
  }

  function test_shell_keeps_all_primary_navigation_at_minimum_size() {
      const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
      shell.width = 960
      shell.height = 600
      verify(shell.findChild("analyzeNavigationButton").visible)
      verify(shell.findChild("historyNavigationButton").visible)
      verify(shell.findChild("resultsNavigationButton").visible)
  }
  ```

- [ ] **Step 2: Implement the compact sidebar state.**

  Use the existing `Layout` widths and a QML state or width binding to switch from `CodeQaTheme.sidebarWidth` to `CodeQaTheme.compactSidebarWidth` below the chosen breakpoint. Keep icon/tooltips and all actions available; do not remove navigation items or replace them with an inaccessible overflow menu.

- [ ] **Step 3: Add keyboard focus and pressed/hovered states.**

  Ensure `WorkbenchButton` has a visible focus indicator, `FindingRow` supports keyboard activation through its handler, filters retain focus styling, and selected rows remain distinguishable without relying on color alone. Use `Accessible.name` or equivalent accessible text for icon-only controls introduced by the compact sidebar.

- [ ] **Step 4: Add explicit Results empty/error states.**

  Give the findings list empty label `objectName: "findingEmptyState"`, style it with a `WorkbenchSurface`, and keep it visible only when `findingTable.count === 0`. Show controller error text inline on the page using the same error color/status role; do not clear the previous report when an import or refresh fails.

- [ ] **Step 5: Verify at both layouts and commit.**

  Run the QML smoke test, then manually launch the desktop at 1280×900 and 960×600. Confirm the sidebar, filter controls, finding table, detail panel, Analyze controls, and History actions remain reachable and readable. Commit:

  ```powershell
  git add src/desktop/qml/Main.qml src/desktop/qml/controls/WorkbenchSidebar.qml src/desktop/qml/controls/WorkbenchButton.qml src/desktop/qml/controls/FindingRow.qml src/desktop/qml/controls/FindingDetails.qml src/desktop/qml/pages/ResultsPage.qml tests/desktop/qml/tst_DesktopShell.qml
  git commit -m "feat: make warm workbench responsive and accessible"
  ```

### Task 6: Run complete verification and package smoke check

**Files:**
- Modify: `tests/desktop/qml/tst_DesktopShell.qml` only if a verified regression requires a missing assertion; do not change product behavior in this task.

**Interfaces:**
- The final branch must contain the approved visual system, all three restyled pages, and no C++ API changes.

- [ ] **Step 1: Run the Qt-free baseline suite.**

  Run from the repository root:

  ```powershell
  cmake -S . -B build-cli -DCODEQA_BUILD_DESKTOP=OFF -DCODEQA_WARNINGS_AS_ERRORS=ON
  cmake --build build-cli --config Debug
  ctest --test-dir build-cli -C Debug --output-on-failure
  ```

  Expected: all CLI/core unit, integration, and corpus tests pass.

- [ ] **Step 2: Configure and build the Qt desktop target with strict warnings.**

  Run with the supported Windows Qt kit:

  ```powershell
  & 'C:\Qt\Tools\CMake_64\bin\cmake.exe' -S . -B build-ui `
    -DCMAKE_PREFIX_PATH='C:\Qt\6.11.0\msvc2022_64' `
    -DCODEQA_BUILD_DESKTOP=ON `
    -DCODEQA_WARNINGS_AS_ERRORS=ON
  & 'C:\Qt\Tools\CMake_64\bin\cmake.exe' --build build-ui --config Debug
  ```

  Expected: `codeqa_desktop`, `codeqa_desktop_tests`, and the QML smoke target build without warnings promoted to errors.

- [ ] **Step 3: Run the complete Qt test suite.**

  Run:

  ```powershell
  & 'C:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir build-ui -C Debug --output-on-failure
  ```

  Expected: desktop C++ tests, QML smoke tests, unit tests, integration tests, and corpus checks all pass.

- [ ] **Step 4: Run the deployed GUI check.**

  Build the deployment target:

  ```powershell
  & 'C:\Qt\Tools\CMake_64\bin\cmake.exe' --build build-ui --config Release --target codeqa_desktop_deploy
  ```

  Launch `build-ui/deploy/codeqa_desktop/codeqa_desktop.exe` and verify that the Warm Workbench shell opens, Results can display a known report, Analyze and History open, and no QML import/plugin error appears.

- [ ] **Step 5: Inspect the final branch and report results.**

  Run:

  ```powershell
  git diff master...UI-Alternative --stat
  git status --short --branch
  ```

  Confirm that generated build folders and existing unrelated untracked files are not staged. Report the exact build and CTest commands plus pass/fail counts in the handoff.
