pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import ".." as Root
import "../controls"

Item {
    id: root

    property var controller: null
    property var selectedFinding: null

    signal exportMarkdownRequested()

    function selectFinding(finding) {
        selectedFinding = finding
    }

    function clearFindingSelection() {
        findingTable.currentIndex = -1
        selectedFinding = null
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
                onClicked: root.exportMarkdownRequested()
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

        WorkbenchSurface {
            id: resultsErrorSurface
            Layout.fillWidth: true
            visible: resultsErrorMessage.text !== ""
            tintColor: Root.CodeQaTheme.errorSoft
            implicitHeight: resultsErrorMessage.implicitHeight + Root.CodeQaTheme.spaceLg * 2

            Label {
                id: resultsErrorMessage
                objectName: "resultsErrorMessage"
                anchors.fill: parent
                anchors.margins: Root.CodeQaTheme.spaceLg
                text: root.controller ? root.controller.errorMessage : ""
                color: Root.CodeQaTheme.error
                wrapMode: Text.Wrap
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

                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            ListView {
                                id: findingTable
                                objectName: "findingTable"
                                anchors.fill: parent
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
                            }

                            WorkbenchSurface {
                                id: findingEmptyState
                                objectName: "findingEmptyState"
                                property alias text: emptyStateLabel.text
                                anchors.centerIn: parent
                                width: Math.min(440, Math.max(240, parent.width - Root.CodeQaTheme.spaceXl * 2))
                                height: emptyStateLabel.implicitHeight + Root.CodeQaTheme.spaceLg * 2
                                visible: findingTable.count === 0
                                tintColor: Root.CodeQaTheme.surfaceSubtle

                                Label {
                                    id: emptyStateLabel
                                    anchors.fill: parent
                                    anchors.margins: Root.CodeQaTheme.spaceLg
                                    text: "No findings match the current filters."
                                    color: Root.CodeQaTheme.mutedInk
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    wrapMode: Text.Wrap
                                }
                            }
                        }
                    }
                }

                FindingDetails {
                    objectName: "findingDetails"
                    Layout.fillHeight: true
                    Layout.preferredWidth: 380
                    elevated: true
                    finding: root.selectedFinding
                }
            }
        }
    }
}
