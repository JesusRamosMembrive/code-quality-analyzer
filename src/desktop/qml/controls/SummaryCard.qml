import QtQuick
import QtQuick.Layouts
import ".." as Root

WorkbenchSurface {
    id: root

    property string label: ""
    property string value: ""
    property string trend: ""
    property string status: "neutral"

    readonly property color accentColor: {
        switch (root.status) {
        case "success":
            return Root.CodeQaTheme.success
        case "error":
            return Root.CodeQaTheme.error
        case "warning":
            return Root.CodeQaTheme.warning
        case "not-evaluated":
            return Root.CodeQaTheme.mutedInk
        default:
            return Root.CodeQaTheme.accent
        }
    }

    readonly property color tint: {
        switch (root.status) {
        case "success":
            return Qt.rgba(0.31, 0.57, 0.46, 0.12)
        case "error":
            return Qt.rgba(0.74, 0.37, 0.32, 0.1)
        case "warning":
            return Qt.rgba(0.70, 0.48, 0.18, 0.1)
        case "not-evaluated":
            return Root.CodeQaTheme.surfaceSubtle
        default:
            return Root.CodeQaTheme.surface
        }
    }

    tintColor: root.tint
    implicitHeight: 132

    Rectangle {
        width: 4
        radius: width / 2
        color: root.accentColor
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: Root.CodeQaTheme.spaceMd
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Root.CodeQaTheme.spaceLg
        anchors.leftMargin: Root.CodeQaTheme.spaceXl
        spacing: Root.CodeQaTheme.spaceSm

        RowLayout {
            Layout.fillWidth: true
            spacing: Root.CodeQaTheme.spaceSm

            Text {
                Layout.fillWidth: true
                text: root.label.toUpperCase()
                color: Root.CodeQaTheme.mutedInk
                font.pixelSize: 11
                font.bold: true
            }

            Rectangle {
                Layout.alignment: Qt.AlignVCenter
                implicitWidth: 10
                implicitHeight: 10
                radius: width / 2
                color: root.accentColor
            }
        }

        Text {
            Layout.fillWidth: true
            text: root.value
            color: Root.CodeQaTheme.ink
            font.pixelSize: 28
            font.bold: true
            wrapMode: Text.Wrap
        }

        Text {
            Layout.fillWidth: true
            visible: root.trend !== ""
            text: root.trend
            color: Root.CodeQaTheme.mutedInk
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }
    }
}
