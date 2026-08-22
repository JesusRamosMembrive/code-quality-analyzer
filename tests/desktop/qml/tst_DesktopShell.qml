import QtQuick
import QtTest
import CodeQA.Desktop

TestCase {
    name: "DesktopShell"

    Component {
        id: themeProbeComponent

        QtObject {
            property color canvas: CodeQaTheme.canvas
            property color accent: CodeQaTheme.accent
            property int sidebarWidth: CodeQaTheme.sidebarWidth
            property int contentMaxWidth: CodeQaTheme.contentMaxWidth
        }
    }

    ListModel {
        id: fixtureFindings
        property int findingCount: count
        property int errorCount: 1
        property int warningCount: 1

        ListElement {
            severity: "error"
            ruleId: "first.rule"
            symbolName: "firstFunction"
            path: "first.cpp"
            line: 3
            column: 5
            observedValue: 8
            limitValue: 4
            description: "first finding"
            recommendation: "reduce complexity"
            metricSummary: "Cyclomatic: 8"
        }

        ListElement {
            severity: "warning"
            ruleId: "second.rule"
            symbolName: "secondFunction"
            path: "second.cpp"
            line: 9
            column: 2
            observedValue: 3
            limitValue: 2
            description: "second finding"
            recommendation: "reduce parameters"
            metricSummary: "Parameters: 3"
        }
    }

    ListModel {
        id: fixtureSuppressedFindings

        ListElement {
            ruleId: "signature.parameter-count"
            symbolName: "manyParams"
            path: "suppressed.cpp"
            line: 4
            column: 1
        }
    }

    ListModel {
        id: fixtureFiles
        property int fileCount: 3
        property int diagnosticCount: 4
    }

    ListModel {
        id: fixtureSnapshots

        ListElement {
            timestamp: "20260301T000000Z"
            path: "C:/projects/sample/.codeqa/snapshots/20260301T000000Z.json"
        }

        ListElement {
            timestamp: "20260101T000000Z"
            path: "C:/projects/sample/.codeqa/snapshots/20260101T000000Z.json"
        }
    }

    ListModel {
        id: fixtureRules

        ListElement {
            ruleId: "complexity.cyclomatic"
            enabled: true
            warningLimit: 10
            highLimit: 15
        }
    }

    ListModel {
        id: fixtureHeaderOverrides

        ListElement {
            pattern: "include/**/*.h"
            language: "cpp"
        }
    }

    QtObject {
        id: fixtureController
        property var findings: fixtureFindings
        property var suppressedFindings: fixtureSuppressedFindings
        property int snapshotComparisonNewCount: 0
        property int snapshotComparisonExistingCount: 0
        property int snapshotComparisonResolvedCount: 0
        property bool snapshotComparisonAvailable: false
        property string lastCompareFrom: ""
        property string lastCompareTo: ""
        property var files: fixtureFiles
        property var snapshots: fixtureSnapshots
        property var rules: fixtureRules
        property var headerOverrides: fixtureHeaderOverrides
        property string qualityGateStatus: "Not evaluated"
        property string qualityGateSeverity: "warning"
        property string lastPage: "Results"
        property string findingSeverityFilter: ""
        property string findingRuleFilter: ""
        property string findingSearchText: ""
        property url currentProject: "file:///C:/projects/sample"
        property bool configurationAvailable: true
        property bool configurationDirty: false
        property bool analysisRunning: false
        property string analysisPhase: ""
        property string errorMessage: ""
        property int selectProjectCalls: 0
        property int refreshSnapshotCalls: 0
        property int openSnapshotCalls: 0
        property int deleteSnapshotCalls: 0
        property int clearRecentPathsCalls: 0
        property url selectedProjectUrl: ""
        property string openedTimestamp: ""
        property string deletedTimestamp: ""
        property bool nextProjectConfigurationAvailable: true
        property string nextProjectErrorMessage: ""

        signal currentReportChanged()

        function selectProject(projectRoot) {
            ++selectProjectCalls
            selectedProjectUrl = projectRoot
            currentProject = projectRoot
            configurationAvailable = nextProjectConfigurationAvailable
            errorMessage = nextProjectErrorMessage
        }
        function saveConfiguration() {}
        function resetConfiguration() {}
        function setQualityGateSeverity(severity) {
            qualityGateSeverity = severity
        }
        function setRuleEnabled(ruleId, value) {}
        function setRuleLimits(ruleId, warningLimit, highLimit) {}
        function addHeaderOverride(pattern, language) {}
        function updateHeaderOverride(row, pattern, language) {}
        function removeHeaderOverride(row) {}
        function startAnalysis() {}
        function refreshSnapshots() {
            ++refreshSnapshotCalls
            errorMessage = configurationAvailable ? "" : nextProjectErrorMessage
        }
        function openSnapshot(timestamp) {
            ++openSnapshotCalls
            openedTimestamp = timestamp
        }
        function deleteSnapshot(timestamp) {
            ++deleteSnapshotCalls
            deletedTimestamp = timestamp
        }
        function compareSnapshots(fromTimestamp, toTimestamp) {
            lastCompareFrom = fromTimestamp
            lastCompareTo = toTimestamp
            snapshotComparisonNewCount = 1
            snapshotComparisonExistingCount = 1
            snapshotComparisonResolvedCount = 1
            snapshotComparisonAvailable = true
        }
        function clearRecentPaths() {
            ++clearRecentPathsCalls
        }

        function setFindingSeverityFilter(value) {
            findingSeverityFilter = value
            fixtureFindings.clear()
            if (value === "") {
                resetFixtureFindings()
            } else if (value === "warning") {
                fixtureFindings.append(warningFinding)
                fixtureFindings.errorCount = 0
                fixtureFindings.warningCount = 1
            }
        }

        function setFindingRuleFilter(value) {
            findingRuleFilter = value
        }

        function setFindingSearchText(value) {
            findingSearchText = value
            fixtureFindings.clear()
            if (value === "") {
                fixtureFindings.append(errorFinding)
                fixtureFindings.append(warningFinding)
                fixtureFindings.errorCount = 1
                fixtureFindings.warningCount = 1
            } else {
                fixtureFindings.errorCount = 0
                fixtureFindings.warningCount = 0
            }
        }
    }

    property var errorFinding: ({
        severity: "error",
        ruleId: "first.rule",
        symbolName: "firstFunction",
        path: "first.cpp",
        line: 3,
        column: 5,
        observedValue: 8,
        limitValue: 4,
        description: "first finding",
        recommendation: "reduce complexity",
        metricSummary: "Cyclomatic: 8"
    })
    property var warningFinding: ({
        severity: "warning",
        ruleId: "second.rule",
        symbolName: "secondFunction",
        path: "second.cpp",
        line: 9,
        column: 2,
        observedValue: 3,
        limitValue: 2,
        description: "second finding",
        recommendation: "reduce parameters",
        metricSummary: "Parameters: 3"
    })

    function resetFixtureFindings() {
        fixtureFindings.clear()
        fixtureFindings.append(errorFinding)
        fixtureFindings.append(warningFinding)
        fixtureFindings.errorCount = 1
        fixtureFindings.warningCount = 1
        fixtureController.findingSeverityFilter = ""
    }

    function resetHistoryCalls() {
        fixtureController.selectProjectCalls = 0
        fixtureController.refreshSnapshotCalls = 0
        fixtureController.openSnapshotCalls = 0
        fixtureController.deleteSnapshotCalls = 0
        fixtureController.selectedProjectUrl = ""
        fixtureController.openedTimestamp = ""
        fixtureController.deletedTimestamp = ""
        fixtureController.errorMessage = ""
        fixtureController.currentProject = "file:///C:/projects/sample"
        fixtureController.configurationAvailable = true
        fixtureController.nextProjectConfigurationAvailable = true
        fixtureController.nextProjectErrorMessage = ""
    }

    function findVisualText(parent, expectedText) {
        if (!parent) {
            return null
        }
        if (parent.text === expectedText) {
            return parent
        }
        const children = parent.children
        if (!children) {
            return null
        }
        for (let index = 0; index < children.length; ++index) {
            const match = findVisualText(children[index], expectedText)
            if (match) {
                return match
            }
        }
        return null
    }

    function test_shell_has_the_three_primary_pages() {
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        verify(shell !== null)

        const analyzeButton = shell.findChild("analyzeNavigationButton")
        verify(analyzeButton !== null)
        compare(analyzeButton.objectName, "analyzeNavigationButton")
        compare(analyzeButton.text, "Analyze")

        const historyButton = shell.findChild("historyNavigationButton")
        verify(historyButton !== null)
        compare(historyButton.objectName, "historyNavigationButton")
        compare(historyButton.text, "History")

        const resultsButton = shell.findChild("resultsNavigationButton")
        verify(resultsButton !== null)
        compare(resultsButton.objectName, "resultsNavigationButton")
        compare(resultsButton.text, "Results")
    }

    function test_warm_workbench_shell_exposes_header_and_sidebar() {
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        verify(shell.findChild("workbenchHeader") !== null)
        verify(shell.findChild("workbenchSidebar") !== null)
        compare(shell.findChild("analyzeNavigationButton").text, "Analyze")
        compare(shell.findChild("historyNavigationButton").text, "History")
        compare(shell.findChild("resultsNavigationButton").text, "Results")
    }

    function test_warm_workbench_theme_singleton_exposes_approved_roles() {
        const probe = createTemporaryObject(themeProbeComponent, this)

        verify(probe !== null)
        compare(probe.canvas.toString(), "#f3eee6")
        compare(probe.accent.toString(), "#d59a3f")
        compare(probe.sidebarWidth, 208)
        compare(probe.contentMaxWidth, 1480)
    }

    function test_workbench_primitives_instantiate_with_expected_states() {
        const root = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/controls/WorkbenchSurface.qml"), this, {
            selected: true
        })
        verify(root !== null)
        compare(root.color.toString(), "#f6e7c7")
        compare(root.border.color.toString(), "#d59a3f")

        const button = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/controls/WorkbenchButton.qml"), this, {
            text: "Analyze project",
            variant: "secondary"
        })
        verify(button !== null)
        compare(button.text, "Analyze project")
        compare(button.variant, "secondary")

        const badge = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/controls/StatusBadge.qml"), this, {
            text: "Not evaluated",
            status: "not-evaluated"
        })
        verify(badge !== null)
        compare(badge.text, "Not evaluated")
        compare(badge.status, "not-evaluated")
    }

    function test_workbench_keyboard_focus_is_tab_reachable() {
        const button = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/controls/WorkbenchButton.qml"), this, {
            text: "Analyze project"
        })
        verify(button !== null)
        compare(button.activeFocusOnTab, true)

        resetFixtureFindings()
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.controller = fixtureController
        shell.selectPage("Results")
        shell.show()
        shell.requestActivate()
        const table = shell.findChild("findingTable")
        tryVerify(function() { return table.count === 2 })
        const row = table.itemAtIndex(0).children[0]
        verify(row !== null)
        compare(row.activeFocusOnTab, true)

        table.forceActiveFocus()
        row.forceActiveFocus()
        tryCompare(row, "activeFocus", true)
        verify(row.activeFocus)
        row.selectCurrentFinding()
        tryCompare(shell.findChild("findingDetails"), "text", "first.rule")
    }

    function test_results_page_is_findings_first_and_read_only() {
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.selectPage("Results")
        compare(shell.findChild("resultsTitle").text, "Findings")
        verify(shell.findChild("findingTable") !== null)
        verify(shell.findChild("resultsSummary") !== null)
        verify(shell.findChild("saveFindingButton") === null)
    }

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
        compare(shell.findChild("findingDetailsObservedValue").text, "Observed value: 3")
        compare(shell.findChild("findingDetailsLimitValue").text, "Limit value: 2")
        compare(shell.findChild("findingDetailsPath").text, "Path: second.cpp")
        compare(shell.findChild("findingDetailsLocation").text, "Location: Line 9, column 2")
        compare(shell.findChild("findingDetailsMetrics").text, "Metrics: Parameters: 3")
    }

    function test_results_summary_uses_controller_model_counts() {
        resetFixtureFindings()
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.controller = fixtureController
        shell.selectPage("Results")

        compare(shell.findChild("qualityGateSummaryCard").value, "Not evaluated")
        compare(shell.findChild("findingsSummaryCard").value, "2")
        compare(shell.findChild("findingsSummaryCard").trend, "3 files analyzed")
        compare(shell.findChild("errorsSummaryCard").value, "1")
        compare(shell.findChild("warningsSummaryCard").value, "1")

        fixtureController.qualityGateSeverity = "error"
        fixtureController.qualityGateStatus = "Passed"
        tryCompare(shell.findChild("qualityGateSummaryCard"), "status", "success")

        fixtureController.qualityGateSeverity = "warning"
        fixtureController.qualityGateStatus = "Failed"
        tryCompare(shell.findChild("qualityGateSummaryCard"), "status", "error")

        fixtureController.qualityGateStatus = "Not evaluated"
        tryCompare(shell.findChild("qualityGateSummaryCard"), "status", "not-evaluated")

        fixtureController.setFindingSeverityFilter("warning")
        tryCompare(shell.findChild("findingsSummaryCard"), "value", "1")
        compare(shell.findChild("errorsSummaryCard").value, "0")
        compare(shell.findChild("warningsSummaryCard").value, "1")
    }

    function test_results_empty_state_remains_visible_after_filtering_all_findings() {
        resetFixtureFindings()
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.controller = fixtureController
        shell.selectPage("Results")

        fixtureController.setFindingSearchText("does-not-exist")

        const table = shell.findChild("findingTable")
        tryVerify(function() { return table.count === 0 })
        const emptyState = shell.findChild("findingEmptyState")
        verify(emptyState !== null)
        tryCompare(emptyState, "visible", true)
        compare(emptyState.text, "No findings match the current filters.")
    }

    function test_results_error_state_is_inline_and_theme_styled() {
        resetFixtureFindings()
        fixtureController.errorMessage = "Error [REPORT001]: report could not be imported"
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.controller = fixtureController
        shell.selectPage("Results")

        const errorState = shell.findChild("resultsErrorMessage")
        verify(errorState !== null)
        verify(errorState.visible)
        compare(errorState.text, "Error [REPORT001]: report could not be imported")
        compare(errorState.color.toString(), CodeQaTheme.error.toString())
    }

    function test_shell_keeps_all_primary_navigation_at_minimum_size() {
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.width = 960
        shell.height = 600

        const sidebar = shell.findChild("workbenchSidebar")
        tryCompare(sidebar, "compact", true)
        tryCompare(sidebar, "width", CodeQaTheme.compactSidebarWidth)
        const analyzeButton = shell.findChild("analyzeNavigationButton")
        const historyButton = shell.findChild("historyNavigationButton")
        const resultsButton = shell.findChild("resultsNavigationButton")
        compare(analyzeButton.text, "A")
        compare(historyButton.text, "H")
        compare(resultsButton.text, "R")
        compare(analyzeButton.Accessible.name, "Analyze")
        compare(analyzeButton.navigationTooltip, "Analyze")
        verify(shell.findChild("analyzeNavigationButton").visible)
        verify(shell.findChild("historyNavigationButton").visible)
        verify(shell.findChild("resultsNavigationButton").visible)

        shell.width = 1120
        tryCompare(sidebar, "compact", true)
        shell.width = 1280
        tryCompare(sidebar, "compact", false)
        tryCompare(sidebar, "width", CodeQaTheme.sidebarWidth)
        compare(analyzeButton.text, "Analyze")
    }

    function test_clear_recent_paths_action_is_visible_calls_controller_and_is_disabled_during_analysis() {
        fixtureController.analysisRunning = false
        fixtureController.clearRecentPathsCalls = 0
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.controller = fixtureController

        const action = shell.findChild("clearRecentPathsButton")
        verify(action !== null)
        compare(action.text, "Clear recent paths")
        compare(action.visible, true)
        compare(action.enabled, true)
        mouseClick(action, action.width / 2, action.height / 2)
        compare(fixtureController.clearRecentPathsCalls, 1)

        fixtureController.analysisRunning = true
        compare(action.enabled, false)
        mouseClick(action, action.width / 2, action.height / 2)
        compare(fixtureController.clearRecentPathsCalls, 1)
        fixtureController.analysisRunning = false
    }

    function test_analyze_page_has_semantic_controls() {
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.controller = fixtureController
        shell.selectPage("Analyze")

        verify(shell.findChild("ruleThresholdTable") !== null)
        verify(shell.findChild("headerOverrideTable") !== null)
        verify(shell.findChild("rawConfigEditor") === null)
        compare(shell.findChild("saveConfigurationButton").text, "Save")
        compare(shell.findChild("resetConfigurationButton").text, "Reset")
        compare(shell.findChild("startAnalysisButton").text, "Analyze project")
        compare(shell.findChild("headerLanguageSelector").currentText, "cpp")
    }

    function test_analyze_uses_workbench_surfaces_and_keeps_semantic_controls() {
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.controller = fixtureController
        shell.selectPage("Analyze")
        verify(shell.findChild("analyzePageSurface") !== null)
        verify(shell.findChild("ruleThresholdTable") !== null)
        verify(shell.findChild("headerOverrideTable") !== null)
        compare(shell.findChild("startAnalysisButton").text, "Analyze project")
    }

    function test_history_lists_newest_first_and_open_is_non_destructive() {
        resetHistoryCalls()
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.controller = fixtureController
        shell.selectPage("History")

        const history = shell.findChild("historyPage")
        verify(history !== null)
        const list = shell.findChild("snapshotList")
        verify(list !== null)
        tryCompare(list, "count", 2)
        tryVerify(function() { return list.itemAtIndex(0) !== null })
        const newest = list.itemAtIndex(0)
        compare(newest.timestampText, "20260301T000000Z")
        const openPoint = newest.openAction.mapToItem(shell.contentItem, 0, 0)
        mouseClick(shell.contentItem,
                   openPoint.x + newest.openAction.width / 2,
                   openPoint.y + newest.openAction.height / 2)

        compare(fixtureController.openSnapshotCalls, 1)
        compare(fixtureController.openedTimestamp, "20260301T000000Z")
        compare(fixtureController.deleteSnapshotCalls, 0)
    }

    function test_history_project_selection_refreshes_available_configuration() {
        resetHistoryCalls()
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.controller = fixtureController
        shell.selectPage("History")
        const history = shell.findChild("historyPage")

        history.selectProject("file:///C:/projects/alternate")

        compare(fixtureController.selectProjectCalls, 1)
        compare(fixtureController.selectedProjectUrl, "file:///C:/projects/alternate")
        compare(fixtureController.refreshSnapshotCalls, 1)
        compare(shell.findChild("refreshSnapshotsButton").enabled, true)
    }

    function test_history_invalid_configuration_selection_refreshes_and_keeps_error_inline() {
        resetHistoryCalls()
        fixtureController.nextProjectConfigurationAvailable = false
        fixtureController.nextProjectErrorMessage = "Error [CONFIG003]: unknown rule in C:/projects/invalid/codeqa.json"
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.controller = fixtureController
        shell.selectPage("History")
        const history = shell.findChild("historyPage")

        history.selectProject("file:///C:/projects/invalid")

        compare(fixtureController.selectProjectCalls, 1)
        compare(fixtureController.refreshSnapshotCalls, 1)
        compare(shell.findChild("refreshSnapshotsButton").enabled, true)
        compare(shell.findChild("historyErrorMessage").text,
                "Error [CONFIG003]: unknown rule in C:/projects/invalid/codeqa.json")
    }

    function test_history_shows_the_selected_project_path() {
        resetHistoryCalls()
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.controller = fixtureController
        shell.selectPage("History")

        verify(findVisualText(shell.findChild("historyPage"), "file:///C:/projects/sample") !== null)
    }

    function test_history_delete_requires_named_confirmation() {
        resetHistoryCalls()
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.controller = fixtureController
        shell.selectPage("History")
        const history = shell.findChild("historyPage")
        const list = shell.findChild("snapshotList")
        tryCompare(list, "count", 2)
        tryVerify(function() { return list.itemAtIndex(0) !== null })
        const newest = list.itemAtIndex(0)

        const deletePoint = newest.deleteAction.mapToItem(shell.contentItem, 0, 0)
        mouseClick(shell.contentItem,
                   deletePoint.x + newest.deleteAction.width / 2,
                   deletePoint.y + newest.deleteAction.height / 2)
        compare(fixtureController.deleteSnapshotCalls, 0)
        compare(history.deleteConfirmation.title, "Delete snapshot?")
        verify(history.deleteConfirmationText.indexOf("20260301T000000Z") !== -1)
        history.deleteConfirmation.reject()
        compare(fixtureController.deleteSnapshotCalls, 0)

        const secondDeletePoint = newest.deleteAction.mapToItem(shell.contentItem, 0, 0)
        mouseClick(shell.contentItem,
                   secondDeletePoint.x + newest.deleteAction.width / 2,
                   secondDeletePoint.y + newest.deleteAction.height / 2)
        history.deleteConfirmation.accept()
        compare(fixtureController.deleteSnapshotCalls, 1)
        compare(fixtureController.deletedTimestamp, "20260301T000000Z")
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

    function test_history_compare_snapshots_shows_counts() {
        fixtureController.snapshotComparisonAvailable = false
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.controller = fixtureController
        shell.selectPage("History")

        const fromCombo = shell.findChild("fromSnapshotCombo")
        const toCombo = shell.findChild("toSnapshotCombo")
        verify(fromCombo !== null)
        verify(toCombo !== null)
        fromCombo.currentIndex = 1
        toCombo.currentIndex = 0

        const compareButton = shell.findChild("compareSnapshotsButton")
        verify(compareButton !== null)
        verify(compareButton.enabled)
        mouseClick(compareButton, compareButton.width / 2, compareButton.height / 2)

        compare(fixtureController.lastCompareFrom, "20260101T000000Z")
        compare(fixtureController.lastCompareTo, "20260301T000000Z")
        const resultLabel = shell.findChild("snapshotComparisonResult")
        verify(resultLabel !== null)
        tryVerify(function() { return resultLabel.visible === true })
        verify(resultLabel.text.indexOf("1") !== -1)
    }

    function test_running_analysis_disables_history_actions() {
        fixtureController.analysisRunning = true
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.controller = fixtureController
        shell.selectPage("History")
        const list = shell.findChild("snapshotList")
        tryCompare(list, "count", 2)
        tryVerify(function() { return list.itemAtIndex(0) !== null })
        const newest = list.itemAtIndex(0)

        compare(shell.findChild("historySelectProjectButton").enabled, false)
        compare(shell.findChild("refreshSnapshotsButton").enabled, false)
        compare(newest.openAction.enabled, false)
        compare(newest.deleteAction.enabled, false)

        fixtureController.analysisRunning = false
    }

    function test_history_compare_button_disabled_when_equal_or_analysis_running() {
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.controller = fixtureController
        shell.selectPage("History")

        const fromCombo = shell.findChild("fromSnapshotCombo")
        const toCombo = shell.findChild("toSnapshotCombo")
        const compareButton = shell.findChild("compareSnapshotsButton")

        fromCombo.currentIndex = 1
        toCombo.currentIndex = 0
        verify(compareButton.enabled)

        toCombo.currentIndex = 1
        compare(compareButton.enabled, false)

        toCombo.currentIndex = 0
        verify(compareButton.enabled)

        fixtureController.analysisRunning = true
        compare(compareButton.enabled, false)
        fixtureController.analysisRunning = false
    }

    function test_running_analysis_disables_conflicting_controls_and_shows_phase() {
        fixtureController.analysisRunning = true
        fixtureController.analysisPhase = "Measuring functions"
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.controller = fixtureController
        shell.selectPage("Analyze")

        compare(shell.findChild("selectProjectButton").enabled, false)
        compare(shell.findChild("saveConfigurationButton").enabled, false)
        compare(shell.findChild("resetConfigurationButton").enabled, false)
        compare(shell.findChild("startAnalysisButton").enabled, false)
        compare(shell.findChild("openReportButton").enabled, false)
        compare(shell.findChild("analysisBusyIndicator").running, true)
        compare(shell.findChild("analysisPhaseLabel").text, "Measuring functions")

        fixtureController.analysisRunning = false
        fixtureController.analysisPhase = ""
    }

    function test_completed_analysis_selects_results() {
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.controller = fixtureController
        shell.selectPage("Analyze")

        fixtureController.currentReportChanged()

        tryCompare(shell, "selectedPage", "Results")
    }

    function test_suppressed_findings_tab_lists_rows_and_shows_empty_state() {
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.controller = fixtureController
        shell.selectPage("Results")

        const suppressedList = shell.findChild("suppressedFindingsList")
        verify(suppressedList !== null)
        tryVerify(function() { return suppressedList.count === 1 })

        // StackLayout only instantiates ListView delegates for the current
        // tab, so the "Suprimidos" tab must be selected before its rows
        // can be inspected via itemAtIndex().
        shell.findChild("resultsTabs").currentIndex = 2
        tryVerify(function() { return suppressedList.itemAtIndex(0) !== null })

        const row = suppressedList.itemAtIndex(0)
        verify(row !== null)

        fixtureSuppressedFindings.clear()
        tryVerify(function() { return suppressedList.count === 0 })

        const emptyLabel = shell.findChild("suppressedFindingsEmptyLabel")
        tryVerify(function() { return emptyLabel.visible === true })

        fixtureSuppressedFindings.append({
            ruleId: "signature.parameter-count",
            symbolName: "manyParams",
            path: "suppressed.cpp",
            line: 4,
            column: 1
        })
    }

    function test_selecting_a_finding_shows_its_details() {
        resetFixtureFindings()
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.controller = fixtureController
        shell.selectPage("Results")

        const table = shell.findChild("findingTable")
        tryVerify(function() { return table.count === 2 })

        const secondFinding = table.itemAtIndex(1)
        verify(secondFinding !== null)
        mouseClick(secondFinding, secondFinding.width / 2, secondFinding.height / 2)

        const details = shell.findChild("findingDetails")
        tryVerify(function() { return details.text === "second.rule" })
    }

    function test_filtering_out_the_selected_finding_clears_details() {
        resetFixtureFindings()
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.controller = fixtureController
        shell.selectPage("Results")

        const table = shell.findChild("findingTable")
        tryVerify(function() { return table.count === 2 })
        const firstFinding = table.itemAtIndex(0)
        mouseClick(firstFinding, firstFinding.width / 2, firstFinding.height / 2)

        const details = shell.findChild("findingDetails")
        tryVerify(function() { return details.text === "first.rule" })

        const severityFilter = shell.findChild("findingSeverityFilter")
        severityFilter.currentIndex = 2
        severityFilter.activated(2)

        tryVerify(function() { return table.count === 1 })
        tryVerify(function() { return details.text === "Finding details" })
    }
}
