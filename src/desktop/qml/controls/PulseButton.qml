import QtQuick
import QtQuick.Templates as T

// Button whose ring pulses while hovered. Adapted from QML-SnippetsExamples'
// styles/qmlsnippetsstyle/buttons/PulseButton.qml, without that repo's Style
// singleton — CodeQA hardcodes its own colors per file.
T.Button {
    id: root

    property color pulseColor: "#2f6fed"
    property int pulseDuration: 1200

    readonly property color fillColor: root.checked ? pulseColor : "#ffffff"
    readonly property color textColor: root.checked ? "#ffffff" : "#1c1f26"

    hoverEnabled: true
    leftPadding: 14
    rightPadding: 14
    topPadding: 8
    bottomPadding: 8

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                             implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                              implicitContentHeight + topPadding + bottomPadding, 36)

    opacity: root.enabled ? 1.0 : 0.45

    background: Item {
        anchors.fill: parent

        Rectangle {
            id: pulseRing
            anchors.centerIn: parent
            width: parent.width
            height: parent.height
            radius: 6
            color: "transparent"
            border.color: root.pulseColor
            border.width: 2
            opacity: 0

            SequentialAnimation on opacity {
                loops: Animation.Infinite
                running: root.hovered && root.enabled && !root.pressed
                NumberAnimation { to: 0.8; duration: root.pulseDuration / 2 }
                NumberAnimation { to: 0; duration: root.pulseDuration / 2 }
            }

            SequentialAnimation on scale {
                loops: Animation.Infinite
                running: root.hovered && root.enabled && !root.pressed
                NumberAnimation { from: 1; to: 1.08; duration: root.pulseDuration / 2 }
                NumberAnimation { from: 1.08; to: 1; duration: root.pulseDuration / 2 }
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: 6
            color: root.pressed ? Qt.darker(root.fillColor, 1.15) : root.fillColor
            border.color: root.checked ? "transparent" : "#d5d9e0"
            border.width: root.checked ? 0 : 1
        }
    }

    contentItem: Text {
        text: root.text
        font: root.font
        color: root.textColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
