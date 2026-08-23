import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".." as Root

WorkbenchSurface {
    id: root

    property var finding: null

    readonly property bool hasFinding: root.finding !== null

    function detailValue(value) {
        return value === undefined || value === null || value === "" ? "Not available" : String(value)
    }

    function statusForSeverity() {
        if (!root.hasFinding) {
            return "not-evaluated"
        }

        const normalized = root.finding.severity ? String(root.finding.severity).toLowerCase() : ""
        if (normalized === "error") {
            return "error"
        }
        if (normalized === "warning") {
            return "warning"
        }
        if (normalized === "information") {
            return "neutral"
        }
        return "not-evaluated"
    }

    implicitWidth: 380

    ScrollView {
        id: detailScroll
        anchors.fill: parent
        anchors.margins: Root.CodeQaTheme.spaceLg
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: detailScroll.availableWidth
            spacing: Root.CodeQaTheme.spaceMd

            StatusBadge {
                visible: root.hasFinding
                text: root.hasFinding ? root.finding.severity : ""
                status: root.statusForSeverity()
            }

            Text {
                id: findingDetails
                objectName: "findingDetails"
                Layout.fillWidth: true
                text: root.hasFinding ? root.detailValue(root.finding.ruleId) : "Finding details"
                color: Root.CodeQaTheme.ink
                font.pixelSize: 20
                font.bold: true
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            Text {
                Layout.fillWidth: true
                visible: !root.hasFinding
                text: "Select a finding to inspect its details."
                color: Root.CodeQaTheme.mutedInk
                wrapMode: Text.Wrap
            }

            Text {
                id: findingDetailsDescription
                objectName: "findingDetailsDescription"
                Layout.fillWidth: true
                visible: root.hasFinding
                text: "Description: " + root.detailValue(root.hasFinding ? root.finding.description : undefined)
                color: Root.CodeQaTheme.ink
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            Text {
                id: findingDetailsRecommendation
                objectName: "findingDetailsRecommendation"
                Layout.fillWidth: true
                visible: root.hasFinding
                text: "Recommendation: " + root.detailValue(root.hasFinding ? root.finding.recommendation : undefined)
                color: Root.CodeQaTheme.ink
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            Text {
                id: findingDetailsObservedValue
                objectName: "findingDetailsObservedValue"
                Layout.fillWidth: true
                visible: root.hasFinding
                text: "Observed value: " + root.detailValue(root.hasFinding ? root.finding.observedValue : undefined)
                color: Root.CodeQaTheme.ink
                wrapMode: Text.Wrap
            }

            Text {
                id: findingDetailsLimitValue
                objectName: "findingDetailsLimitValue"
                Layout.fillWidth: true
                visible: root.hasFinding
                text: "Limit value: " + root.detailValue(root.hasFinding ? root.finding.limitValue : undefined)
                color: Root.CodeQaTheme.ink
                wrapMode: Text.Wrap
            }

            Text {
                id: findingDetailsPath
                objectName: "findingDetailsPath"
                Layout.fillWidth: true
                visible: root.hasFinding
                text: "Path: " + root.detailValue(root.hasFinding ? root.finding.path : undefined)
                color: Root.CodeQaTheme.ink
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            Text {
                id: findingDetailsLocation
                objectName: "findingDetailsLocation"
                Layout.fillWidth: true
                visible: root.hasFinding
                text: "Location: Line " + root.detailValue(root.hasFinding ? root.finding.line : undefined)
                    + ", column " + root.detailValue(root.hasFinding ? root.finding.column : undefined)
                color: Root.CodeQaTheme.ink
                wrapMode: Text.Wrap
            }

            Text {
                id: findingDetailsMetrics
                objectName: "findingDetailsMetrics"
                Layout.fillWidth: true
                visible: root.hasFinding
                text: "Metrics: " + root.detailValue(root.hasFinding ? root.finding.metricSummary : undefined)
                color: Root.CodeQaTheme.ink
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }
        }
    }
}
