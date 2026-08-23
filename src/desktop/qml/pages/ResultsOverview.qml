pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import ".." as Root
import "../controls"

Item {
    id: root

    property var controller: null

    function qualityGateCardStatus() {
        if (!root.controller) {
            return "not-evaluated"
        }

        const statusText = root.controller.qualityGateStatus ? String(root.controller.qualityGateStatus).toLowerCase() : ""
        if (statusText === "not evaluated") {
            return "not-evaluated"
        }
        if (statusText === "passed") {
            return "success"
        }
        if (statusText === "failed") {
            return "error"
        }
        return "neutral"
    }

    signal exportMarkdownRequested()

    ColumnLayout {
        anchors.fill: parent
        spacing: Root.CodeQaTheme.spaceLg

        RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Root.CodeQaTheme.spaceXs

                Label {
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
                text: "Export Markdown"
                variant: "secondary"
                onClicked: root.exportMarkdownRequested()
            }
        }

        GridLayout {
            id: resultsSummary
            objectName: "overviewSummary"
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

        WorkbenchSurface {
            objectName: "overviewContext"
            Layout.fillWidth: true
            tintColor: Root.CodeQaTheme.surfaceSubtle
            implicitHeight: contextColumn.implicitHeight + Root.CodeQaTheme.spaceLg * 2

            ColumnLayout {
                id: contextColumn
                anchors.fill: parent
                anchors.margins: Root.CodeQaTheme.spaceLg
                spacing: Root.CodeQaTheme.spaceXs

                Label {
                    Layout.fillWidth: true
                    text: "Run context"
                    color: Root.CodeQaTheme.ink
                    font.bold: true
                }

                Label {
                    Layout.fillWidth: true
                    text: String(root.controller ? root.controller.files.fileCount : 0) + " files analyzed"
                    color: Root.CodeQaTheme.mutedInk
                    wrapMode: Text.Wrap
                }

                Label {
                    Layout.fillWidth: true
                    text: "Quality gate: " + (root.controller ? root.controller.qualityGateStatus : "Not evaluated")
                    color: Root.CodeQaTheme.mutedInk
                    wrapMode: Text.Wrap
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
