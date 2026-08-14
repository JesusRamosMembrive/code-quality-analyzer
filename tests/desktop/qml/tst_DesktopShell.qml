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

    QtObject {
        id: fixtureController
        property var findings: fixtureFindings
        property var files: fixtureFiles
        property string qualityGateStatus: "Not evaluated"
        property string lastPage: "Results"
        property string findingSeverityFilter: ""
        property string findingRuleFilter: ""
        property string findingSearchText: ""

        function setFindingSeverityFilter(value) {
            findingSeverityFilter = value
        }

        function setFindingRuleFilter(value) {
            findingRuleFilter = value
        }

        function setFindingSearchText(value) {
            findingSearchText = value
        }
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

    function test_selecting_a_finding_shows_its_details() {
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
}
