import QtQuick
import QtQuick.Layouts
import ".." as Root

WorkbenchSurface {
    id: root

    property string severity: ""
    property string ruleId: ""
    property string symbolName: ""
    property string path: ""
    property var line: undefined
    property var column: undefined
    property var observedValue: undefined
    property var limitValue: undefined
    property string description: ""
    property string recommendation: ""
    property string metricSummary: ""

    signal findingSelected(var finding)

    readonly property color textColor: root.selected ? Root.CodeQaTheme.ink : Root.CodeQaTheme.ink

    implicitHeight: contentRow.implicitHeight + (Root.CodeQaTheme.spaceMd * 2)
    elevated: root.selected
    border.width: root.selected || root.activeFocus ? 2 : 1
    border.color: root.selected || root.activeFocus ? Root.CodeQaTheme.accent : Root.CodeQaTheme.border
    activeFocusOnTab: true
    Accessible.name: root.ruleId + ", " + root.symbolName

    function statusForSeverity() {
        const normalized = root.severity ? root.severity.toLowerCase() : ""
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

    function locationText() {
        return "Line " + root.line + ", column " + root.column
    }

    function selectCurrentFinding() {
        root.findingSelected({
            severity: root.severity,
            ruleId: root.ruleId,
            symbolName: root.symbolName,
            path: root.path,
            line: root.line,
            column: root.column,
            observedValue: root.observedValue,
            limitValue: root.limitValue,
            description: root.description,
            recommendation: root.recommendation,
            metricSummary: root.metricSummary
        })
    }

    RowLayout {
        id: contentRow
        anchors.fill: parent
        anchors.margins: Root.CodeQaTheme.spaceMd
        spacing: Root.CodeQaTheme.spaceMd

        StatusBadge {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 104
            text: root.severity
            status: root.statusForSeverity()
        }

        Text {
            Layout.preferredWidth: 220
            Layout.alignment: Qt.AlignVCenter
            text: root.ruleId
            color: root.textColor
            font.bold: true
            elide: Text.ElideRight
            textFormat: Text.PlainText
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: Root.CodeQaTheme.spaceXs

            Text {
                Layout.fillWidth: true
                text: root.path
                color: root.textColor
                elide: Text.ElideMiddle
                textFormat: Text.PlainText
            }

            Text {
                Layout.fillWidth: true
                text: root.locationText()
                color: Root.CodeQaTheme.mutedInk
                font.pixelSize: 12
                elide: Text.ElideRight
                textFormat: Text.PlainText
            }
        }

        Text {
            Layout.preferredWidth: 180
            Layout.alignment: Qt.AlignVCenter
            text: root.symbolName
            color: root.textColor
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignRight
            textFormat: Text.PlainText
        }
    }

    TapHandler {
        onTapped: root.selectCurrentFinding()
    }

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            event.accepted = true
            root.selectCurrentFinding()
        }
    }
}
