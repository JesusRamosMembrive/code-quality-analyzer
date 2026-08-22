pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import ".." as Root
import "../controls"

Item {
    id: root

    property var controller: null
    property var selectedFinding: null

    function selectFinding(finding) {
        selectedFinding = finding
    }

    function clearFindingSelection() {
        findingTable.currentIndex = -1
        selectedFinding = null
    }

    function qualityGateCardStatus() {
        if (!root.controller) {
            return "not-evaluated"
        }

        const statusText = root.controller.qualityGateStatus ? String(root.controller.qualityGateStatus).toLowerCase() : ""
        if (statusText === "not evaluated") {
            return "not-evaluated"
        }

        const severityText = root.controller.qualityGateSeverity ? String(root.controller.qualityGateSeverity).toLowerCase() : ""
        if (severityText === "error") {
            return "error"
        }
        if (severityText === "warning") {
            return "warning"
        }
        if (severityText === "success") {
            return "success"
        }
        return "neutral"
    }

    Connections {
        target: root.controller

        function onCurrentReportChanged() {
            root.clearFindingSelection()
        }

        function onFindingSeverityFilterChanged(severity) {
            root.clearFindingSelection()
            filterBar.severityValue = severity
        }

        function onFindingRuleFilterChanged(ruleId) {
            root.clearFindingSelection()
            filterBar.ruleValue = ruleId
        }

        function onFindingSearchTextChanged(searchText) {
            root.clearFindingSelection()
            filterBar.searchValue = searchText
        }
    }

    FileDialog {
        id: exportMarkdownDialog
        title: "Export report as Markdown"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Markdown files (*.md)", "All files (*)"]
        defaultSuffix: "md"

        onAccepted: {
            if (root.controller) {
                root.controller.exportMarkdown(selectedFile)
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Root.CodeQaTheme.spaceLg

        TabBar {
            id: resultsTabs
            objectName: "resultsTabs"
            Layout.fillWidth: true
            currentIndex: 0

            TabButton {
                text: "Findings"
            }

            TabButton {
                text: "Files and metrics"
            }

            TabButton {
                text: "Suppressed"
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: resultsTabs.currentIndex

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    spacing: Root.CodeQaTheme.spaceLg

                    RowLayout {
                        Layout.fillWidth: true

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Root.CodeQaTheme.spaceXs

                            Label {
                                id: resultsTitle
                                objectName: "resultsTitle"
                                text: "Findings"
                                color: Root.CodeQaTheme.ink
                                font.pixelSize: 28
                                font.bold: true
                            }

                            Label {
                                Layout.fillWidth: true
                                text: "Review the latest findings, narrow the list, and inspect a rule in detail."
                                color: Root.CodeQaTheme.mutedInk
                                wrapMode: Text.Wrap
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        WorkbenchButton {
                            id: exportMarkdownButton
                            objectName: "exportMarkdownButton"
                            text: "Export Markdown"
                            variant: "secondary"
                            onClicked: exportMarkdownDialog.open()
                        }
                    }

                    GridLayout {
                        id: resultsSummary
                        objectName: "resultsSummary"
                        Layout.fillWidth: true
                        columns: width >= 900 ? 4 : 2
                        columnSpacing: Root.CodeQaTheme.spaceMd
                        rowSpacing: Root.CodeQaTheme.spaceMd

                        SummaryCard {
                            objectName: "qualityGateSummaryCard"
                            Layout.fillWidth: true
                            label: "Quality gate"
                            value: root.controller ? root.controller.qualityGateStatus : "Not evaluated"
                            trend: ""
                            status: root.qualityGateCardStatus()
                        }

                        SummaryCard {
                            objectName: "findingsSummaryCard"
                            Layout.fillWidth: true
                            label: "Matching findings"
                            value: String(root.controller ? root.controller.findings.findingCount : 0)
                            trend: String(root.controller ? root.controller.files.fileCount : 0) + " files analyzed"
                            status: "neutral"
                        }

                        SummaryCard {
                            objectName: "errorsSummaryCard"
                            Layout.fillWidth: true
                            label: "Matching errors"
                            value: String(root.controller ? root.controller.findings.errorCount : 0)
                            trend: ""
                            status: "error"
                        }

                        SummaryCard {
                            objectName: "warningsSummaryCard"
                            Layout.fillWidth: true
                            label: "Matching warnings"
                            value: String(root.controller ? root.controller.findings.warningCount : 0)
                            trend: ""
                            status: "warning"
                        }
                    }

                    FilterBar {
                        id: filterBar
                        Layout.fillWidth: true
                        severityValue: root.controller ? root.controller.findingSeverityFilter : ""
                        ruleValue: root.controller ? root.controller.findingRuleFilter : ""
                        searchValue: root.controller ? root.controller.findingSearchText : ""

                        onSeveritySelected: function(severity) {
                            if (root.controller) {
                                root.clearFindingSelection()
                                root.controller.setFindingSeverityFilter(severity)
                            }
                        }

                        onRuleEdited: function(ruleId) {
                            if (root.controller) {
                                root.clearFindingSelection()
                                root.controller.setFindingRuleFilter(ruleId)
                            }
                        }

                        onSearchEdited: function(searchText) {
                            if (root.controller) {
                                root.clearFindingSelection()
                                root.controller.setFindingSearchText(searchText)
                            }
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        width: parent.width
                        height: parent.height

                        RowLayout {
                            anchors.fill: parent
                            spacing: Root.CodeQaTheme.spaceLg

                        WorkbenchSurface {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            elevated: true

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: Root.CodeQaTheme.spaceLg
                                spacing: Root.CodeQaTheme.spaceSm

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Root.CodeQaTheme.spaceMd

                                    Label {
                                        Layout.preferredWidth: 104
                                        text: "Severity"
                                        color: Root.CodeQaTheme.mutedInk
                                        font.bold: true
                                    }

                                    Label {
                                        Layout.preferredWidth: 220
                                        text: "Rule"
                                        color: Root.CodeQaTheme.mutedInk
                                        font.bold: true
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: "Path and location"
                                        color: Root.CodeQaTheme.mutedInk
                                        font.bold: true
                                    }

                                    Label {
                                        Layout.preferredWidth: 180
                                        text: "Symbol"
                                        color: Root.CodeQaTheme.mutedInk
                                        font.bold: true
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }

                                ListView {
                                    id: findingTable
                                    objectName: "findingTable"
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    clip: true
                                    currentIndex: -1
                                    model: root.controller ? root.controller.findings : null
                                    spacing: Root.CodeQaTheme.spaceSm

                                    ScrollBar.vertical: ScrollBar {}

                                    Component.onCompleted: currentIndex = -1
                                    onModelChanged: currentIndex = -1

                                    delegate: Item {
                                        id: findingDelegate
                                        required property int index
                                        required property string severity
                                        required property string ruleId
                                        required property string symbolName
                                        required property string path
                                        required property var line
                                        required property var column
                                        required property var observedValue
                                        required property var limitValue
                                        required property string description
                                        required property string recommendation
                                        required property string metricSummary

                                        width: findingTable.width
                                        height: Math.max(72, findingRow.implicitHeight)
                                        property bool isCurrent: ListView.isCurrentItem

                                        FindingRow {
                                            id: findingRow
                                            anchors.fill: parent
                                            severity: findingDelegate.severity
                                            ruleId: findingDelegate.ruleId
                                            symbolName: findingDelegate.symbolName
                                            path: findingDelegate.path
                                            line: findingDelegate.line
                                            column: findingDelegate.column
                                            observedValue: findingDelegate.observedValue
                                            limitValue: findingDelegate.limitValue
                                            description: findingDelegate.description
                                            recommendation: findingDelegate.recommendation
                                            metricSummary: findingDelegate.metricSummary
                                            selected: findingDelegate.isCurrent

                                            onFindingSelected: function(finding) {
                                                findingTable.currentIndex = findingDelegate.index
                                                root.selectFinding(finding)
                                            }
                                        }
                                    }

                                    Label {
                                        anchors.centerIn: parent
                                        visible: findingTable.count === 0
                                        text: "No findings match the current filters."
                                        color: Root.CodeQaTheme.mutedInk
                                    }
                                }
                            }
                        }

                        FindingDetails {
                            Layout.fillHeight: true
                            Layout.preferredWidth: 380
                            elevated: true
                            finding: root.selectedFinding
                        }
                        }
                    }
                }
            }

            ListView {
                id: fileMetricsList
                clip: true
                model: root.controller ? root.controller.files : null
                spacing: Root.CodeQaTheme.spaceMd

                delegate: WorkbenchSurface {
                    required property string path
                    required property string language
                    required property string fileState
                    required property var diagnosticCount
                    required property var findingCount
                    required property var functionCount
                    required property var functionMetrics

                    width: fileMetricsList.width
                    elevated: true
                    height: metricsColumn.implicitHeight + (Root.CodeQaTheme.spaceLg * 2)

                    ColumnLayout {
                        id: metricsColumn
                        anchors.fill: parent
                        anchors.margins: Root.CodeQaTheme.spaceLg
                        spacing: Root.CodeQaTheme.spaceSm

                        Label {
                            Layout.fillWidth: true
                            text: path
                            color: Root.CodeQaTheme.ink
                            font.bold: true
                            elide: Text.ElideMiddle
                            textFormat: Text.PlainText
                        }

                        Label {
                            text: "Language: " + language + " | State: " + fileState + " | Diagnostics: " + diagnosticCount
                                + " | Findings: " + findingCount + " | Functions: " + functionCount
                            color: Root.CodeQaTheme.mutedInk
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }

                        Repeater {
                            model: functionMetrics

                            delegate: Label {
                                required property var modelData
                                Layout.fillWidth: true
                                text: modelData.symbolName + " (line " + modelData.line + "): Cyclomatic "
                                    + modelData.cyclomaticComplexity + ", Nesting " + modelData.maxNestingDepth
                                    + ", Parameters " + modelData.parameterCount + ", Code lines " + modelData.codeLines
                                color: Root.CodeQaTheme.ink
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                            }
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: fileMetricsList.count === 0
                    text: "No file metrics are available."
                    color: Root.CodeQaTheme.mutedInk
                }
            }

            ListView {
                id: suppressedFindingsList
                objectName: "suppressedFindingsList"
                clip: true
                model: root.controller ? root.controller.suppressedFindings : null
                spacing: Root.CodeQaTheme.spaceSm

                delegate: WorkbenchSurface {
                    required property string ruleId
                    required property string symbolName
                    required property string path
                    required property var line
                    required property var column

                    width: suppressedFindingsList.width
                    elevated: true
                    height: suppressedRow.implicitHeight + (Root.CodeQaTheme.spaceMd * 2)

                    RowLayout {
                        id: suppressedRow
                        anchors.fill: parent
                        anchors.margins: Root.CodeQaTheme.spaceMd
                        spacing: Root.CodeQaTheme.spaceMd

                        Label {
                            Layout.preferredWidth: 180
                            text: ruleId
                            color: Root.CodeQaTheme.ink
                            elide: Text.ElideRight
                            textFormat: Text.PlainText
                        }

                        Label {
                            Layout.fillWidth: true
                            text: path + ":" + line + ":" + column
                            color: Root.CodeQaTheme.mutedInk
                            elide: Text.ElideMiddle
                            textFormat: Text.PlainText
                        }

                        Label {
                            Layout.preferredWidth: 150
                            text: symbolName
                            color: Root.CodeQaTheme.ink
                            elide: Text.ElideRight
                            textFormat: Text.PlainText
                        }
                    }
                }

                Label {
                    id: suppressedFindingsEmptyLabel
                    objectName: "suppressedFindingsEmptyLabel"
                    anchors.centerIn: parent
                    visible: suppressedFindingsList.count === 0
                    text: "No suppressed findings in this run."
                    color: Root.CodeQaTheme.mutedInk
                }
            }
        }
    }
}
