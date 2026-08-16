import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    property string title: ""

    Layout.fillWidth: true
    spacing: 10

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: "#333844"
    }

    Label {
        text: root.title.toUpperCase()
        font.pixelSize: 12
        font.bold: true
        font.letterSpacing: 1
        color: "#9aa1b0"
    }
}
