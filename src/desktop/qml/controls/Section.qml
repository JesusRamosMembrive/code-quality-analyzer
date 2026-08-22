import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".." as Root

ColumnLayout {
    id: root

    property string title: ""

    Layout.fillWidth: true
    spacing: Root.CodeQaTheme.spaceMd

    Label {
        text: root.title.toUpperCase()
        font.pixelSize: 12
        font.bold: true
        font.letterSpacing: 1.2
        color: Root.CodeQaTheme.mutedInk
        Layout.leftMargin: Root.CodeQaTheme.spaceXs
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        Layout.bottomMargin: Root.CodeQaTheme.spaceXs
        color: Root.CodeQaTheme.border
    }
}
