pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import ".." as Root
import "../controls"

Item {
    id: root

    property var controller: null

    ListView {
        id: fileMetricsList
        objectName: "fileMetricsList"
        anchors.fill: parent
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
}
