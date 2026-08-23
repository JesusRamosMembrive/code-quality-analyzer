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
        id: suppressedFindingsList
        objectName: "suppressedFindingsList"
        anchors.fill: parent
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
