import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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

    function detailValue(value) {
        return value === undefined || value === null || value === "" ? "Not available" : String(value)
    }

    Connections {
        target: root.controller

        function onCurrentReportChanged() {
            root.clearFindingSelection()
        }

        function onFindingSeverityFilterChanged(severity) {
            root.clearFindingSelection()
            severityFilter.currentIndex = severityFilter.indexForSeverity(severity)
        }

        function onFindingRuleFilterChanged(ruleId) {
            root.clearFindingSelection()
            ruleFilter.text = ruleId
        }

        function onFindingSearchTextChanged(searchText) {
            root.clearFindingSelection()
            searchFilter.text = searchText
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12

        RowLayout {
            Layout.fillWidth: true

            Label {
                id: resultsTitle
                objectName: "resultsTitle"
                text: "Findings"
                font.pixelSize: 26
                font.bold: true
            }

            Item {
                Layout.fillWidth: true
            }

            RowLayout {
                id: resultsSummary
                objectName: "resultsSummary"
                spacing: 12

                Label {
                    objectName: "summaryFilesLabel"
                    text: "Files: " + (root.controller ? root.controller.files.fileCount : 0)
                }

                Label {
                    objectName: "summaryDiagnosticsLabel"
                    text: "Diagnostics: " + (root.controller ? root.controller.files.diagnosticCount : 0)
                }

                Label {
                    objectName: "summaryFindingsLabel"
                    text: "Matching findings: " + (root.controller ? root.controller.findings.findingCount : 0)
                }

                Label {
                    objectName: "summaryErrorsLabel"
                    text: "Matching errors: " + (root.controller ? root.controller.findings.errorCount : 0)
                }

                Label {
                    objectName: "summaryWarningsLabel"
                    text: "Matching warnings: " + (root.controller ? root.controller.findings.warningCount : 0)
                }

                Label {
                    text: "Quality gate: " + (root.controller ? root.controller.qualityGateStatus : "Not evaluated")
                }
            }
        }

        TabBar {
            id: resultsTabs
            Layout.fillWidth: true
            currentIndex: 0

            TabButton {
                text: "Findings"
            }

            TabButton {
                text: "Files and metrics"
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: resultsTabs.currentIndex

            Item {
                RowLayout {
                    anchors.fill: parent
                    spacing: 14

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: 3
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true

                            ComboBox {
                                id: severityFilter
                                objectName: "findingSeverityFilter"
                                Layout.preferredWidth: 150
                                model: ["All severities", "Error", "Warning", "Information"]

                                function indexForSeverity(severity) {
                                    const normalized = severity ? severity.toLowerCase() : ""
                                    if (normalized === "error") {
                                        return 1
                                    }
                                    if (normalized === "warning") {
                                        return 2
                                    }
                                    if (normalized === "information") {
                                        return 3
                                    }
                                    return 0
                                }

                                Component.onCompleted: currentIndex = indexForSeverity(root.controller ? root.controller.findingSeverityFilter : "")
                                onActivated: {
                                    if (root.controller) {
                                        root.clearFindingSelection()
                                        root.controller.setFindingSeverityFilter(currentIndex === 0 ? "" : currentText.toLowerCase())
                                    }
                                }
                            }

                            TextField {
                                id: ruleFilter
                                objectName: "findingRuleFilter"
                                Layout.fillWidth: true
                                placeholderText: "Filter by rule ID"
                                text: root.controller ? root.controller.findingRuleFilter : ""
                                onTextEdited: {
                                    if (root.controller) {
                                        root.clearFindingSelection()
                                        root.controller.setFindingRuleFilter(text)
                                    }
                                }
                            }

                            TextField {
                                id: searchFilter
                                objectName: "findingSearchFilter"
                                Layout.fillWidth: true
                                placeholderText: "Search rule, symbol, or path"
                                text: root.controller ? root.controller.findingSearchText : ""
                                onTextEdited: {
                                    if (root.controller) {
                                        root.clearFindingSelection()
                                        root.controller.setFindingSearchText(text)
                                    }
                                }
                            }
                        }

                        ListView {
                            id: findingTable
                            objectName: "findingTable"
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: root.controller ? root.controller.findings : null
                            spacing: 4

                            ScrollBar.vertical: ScrollBar {}

                            delegate: Rectangle {
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
                                height: findingRow.implicitHeight + 16
                                radius: 4
                                color: ListView.isCurrentItem ? "#dceeff" : "#f4f6f8"

                                RowLayout {
                                    id: findingRow
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 12

                                    Label {
                                        Layout.preferredWidth: 90
                                        text: severity
                                        font.bold: true
                                    }

                                    Label {
                                        Layout.preferredWidth: 180
                                        text: ruleId
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: path + ":" + line + ":" + column
                                        elide: Text.ElideMiddle
                                    }

                                    Label {
                                        Layout.preferredWidth: 150
                                        text: symbolName
                                        elide: Text.ElideRight
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        findingTable.currentIndex = index
                                        root.selectFinding({
                                            severity: severity,
                                            ruleId: ruleId,
                                            symbolName: symbolName,
                                            path: path,
                                            line: line,
                                            column: column,
                                            observedValue: observedValue,
                                            limitValue: limitValue,
                                            description: description,
                                            recommendation: recommendation,
                                            metricSummary: metricSummary
                                        })
                                    }
                                }
                            }

                            Label {
                                anchors.centerIn: parent
                                visible: findingTable.count === 0
                                text: "No findings match the current filters."
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillHeight: true
                        Layout.preferredWidth: 350
                        color: "#f4f6f8"
                        radius: 4

                        ScrollView {
                            id: detailScroll
                            anchors.fill: parent
                            anchors.margins: 14
                            clip: true
                            contentWidth: availableWidth
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                            ColumnLayout {
                                width: detailScroll.availableWidth
                                spacing: 10

                                Label {
                                    id: findingDetails
                                    objectName: "findingDetails"
                                    Layout.fillWidth: true
                                    text: root.selectedFinding ? root.selectedFinding.ruleId : "Finding details"
                                    font.pixelSize: 18
                                    font.bold: true
                                    wrapMode: Text.Wrap
                                }

                                Label {
                                    visible: !root.selectedFinding
                                    Layout.fillWidth: true
                                    text: "Select a finding to inspect its details."
                                    wrapMode: Text.Wrap
                                }

                                Label {
                                    visible: root.selectedFinding
                                    Layout.fillWidth: true
                                    text: "Description: " + root.detailValue(root.selectedFinding ? root.selectedFinding.description : undefined)
                                    wrapMode: Text.Wrap
                                }

                                Label {
                                    visible: root.selectedFinding
                                    Layout.fillWidth: true
                                    text: "Recommendation: " + root.detailValue(root.selectedFinding ? root.selectedFinding.recommendation : undefined)
                                    wrapMode: Text.Wrap
                                }

                                Label {
                                    visible: root.selectedFinding
                                    Layout.fillWidth: true
                                    text: "Observed value: " + root.detailValue(root.selectedFinding ? root.selectedFinding.observedValue : undefined)
                                    wrapMode: Text.Wrap
                                }

                                Label {
                                    visible: root.selectedFinding
                                    Layout.fillWidth: true
                                    text: "Limit value: " + root.detailValue(root.selectedFinding ? root.selectedFinding.limitValue : undefined)
                                    wrapMode: Text.Wrap
                                }

                                Label {
                                    visible: root.selectedFinding
                                    Layout.fillWidth: true
                                    text: "Path: " + root.detailValue(root.selectedFinding ? root.selectedFinding.path : undefined)
                                    wrapMode: Text.Wrap
                                }

                                Label {
                                    visible: root.selectedFinding
                                    Layout.fillWidth: true
                                    text: "Location: Line " + root.detailValue(root.selectedFinding ? root.selectedFinding.line : undefined)
                                        + ", column " + root.detailValue(root.selectedFinding ? root.selectedFinding.column : undefined)
                                    wrapMode: Text.Wrap
                                }

                                Label {
                                    visible: root.selectedFinding
                                    Layout.fillWidth: true
                                    text: "Metrics: " + root.detailValue(root.selectedFinding ? root.selectedFinding.metricSummary : undefined)
                                    wrapMode: Text.Wrap
                                }
                            }
                        }
                    }
                }
            }

            ListView {
                id: fileMetricsList
                clip: true
                model: root.controller ? root.controller.files : null
                spacing: 8

                delegate: Rectangle {
                    required property string path
                    required property string language
                    required property string state
                    required property var diagnosticCount
                    required property var findingCount
                    required property var functionCount
                    required property var functionMetrics

                    width: fileMetricsList.width
                    height: metricsColumn.implicitHeight + 20
                    radius: 4
                    color: "#f4f6f8"

                    ColumnLayout {
                        id: metricsColumn
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 6

                        Label {
                            Layout.fillWidth: true
                            text: path
                            font.bold: true
                            elide: Text.ElideMiddle
                        }

                        Label {
                            text: "Language: " + language + " | State: " + state + " | Diagnostics: " + diagnosticCount
                                + " | Findings: " + findingCount + " | Functions: " + functionCount
                            wrapMode: Text.Wrap
                        }

                        Repeater {
                            model: functionMetrics

                            delegate: Label {
                                required property var modelData
                                Layout.fillWidth: true
                                text: modelData.symbolName + " (line " + modelData.line + "): Cyclomatic "
                                    + modelData.cyclomaticComplexity + ", Nesting " + modelData.maxNestingDepth
                                    + ", Parameters " + modelData.parameterCount + ", Code lines " + modelData.codeLines
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: fileMetricsList.count === 0
                    text: "No file metrics are available."
                }
            }
        }
    }
}
