import QtQuick
import QtQuick.Layouts
import ".." as Root

Rectangle {
    id: root

    property string status: "neutral"
    property alias text: badgeText.text

    readonly property color statusColor: {
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
            return Root.CodeQaTheme.mutedInk
        }
    }

    readonly property color backgroundColor: {
        switch (root.status) {
        case "success":
            return Qt.rgba(0.31, 0.57, 0.46, 0.14)
        case "error":
            return Qt.rgba(0.74, 0.37, 0.32, 0.14)
        case "warning":
            return Qt.rgba(0.7, 0.48, 0.18, 0.14)
        case "not-evaluated":
            return Root.CodeQaTheme.surfaceSubtle
        default:
            return Qt.rgba(0.51, 0.47, 0.42, 0.12)
        }
    }

    color: root.backgroundColor
    radius: height / 2
    border.width: 1
    border.color: root.status === "not-evaluated" ? Root.CodeQaTheme.border : "transparent"
    implicitWidth: badgeRow.implicitWidth + (Root.CodeQaTheme.spaceSm * 2)
    implicitHeight: Math.max(28, badgeRow.implicitHeight + (Root.CodeQaTheme.spaceXs * 2))

    RowLayout {
        id: badgeRow
        anchors.centerIn: parent
        spacing: Root.CodeQaTheme.spaceSm

        Rectangle {
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: 8
            implicitHeight: 8
            radius: width / 2
            color: root.statusColor
        }

        Text {
            id: badgeText
            Layout.alignment: Qt.AlignVCenter
            color: Root.CodeQaTheme.ink
            font.pixelSize: 12
            font.bold: root.status !== "neutral"
            text: ""
        }
    }
}
