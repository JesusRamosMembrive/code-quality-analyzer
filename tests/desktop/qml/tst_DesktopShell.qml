import QtQuick
import QtTest
import CodeQA.Desktop

TestCase {
    name: "DesktopShell"

    ListModel {
        id: fixtureFindings

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
        id: fixtureFiles
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
        property var files: fixtureFiles
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

        signal currentReportChanged()

        function selectProject(projectRoot) {}
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

        function setFindingSeverityFilter(value) {
            findingSeverityFilter = value
            fixtureFindings.clear()
            if (value === "") {
                resetFixtureFindings()
            } else if (value === "warning") {
                fixtureFindings.append(warningFinding)
            }
        }

        function setFindingRuleFilter(value) {
            findingRuleFilter = value
        }

        function setFindingSearchText(value) {
            findingSearchText = value
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
        fixtureController.findingSeverityFilter = ""
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

    function test_results_page_is_findings_first_and_read_only() {
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.selectPage("Results")
        compare(shell.findChild("resultsTitle").text, "Findings")
        verify(shell.findChild("findingTable") !== null)
        verify(shell.findChild("resultsSummary") !== null)
        verify(shell.findChild("saveFindingButton") === null)
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
