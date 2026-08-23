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
        font.pixelSize: Root.CodeQaTheme.sectionLabelPixelSize
        font.bold: Root.CodeQaTheme.sectionLabelBold
        font.letterSpacing: Root.CodeQaTheme.sectionLabelLetterSpacing
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
