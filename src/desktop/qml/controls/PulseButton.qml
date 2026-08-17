import QtQuick
import QtQuick.Templates as T

// Button whose ring pulses while hovered. Adapted from QML-SnippetsExamples'
// styles/qmlsnippetsstyle/buttons/PulseButton.qml, without that repo's Style
// singleton — CodeQA hardcodes its own colors per file.
T.Button {
    id: root

    property color pulseColor: "#2f6fed"
    property int pulseDuration: 1200
    // T.Button already provides `highlighted` (unlike `checked`, it's a pure
    // styling flag the control never writes itself on click), so it's safe
    // to bind it externally for "this is the active nav item" without the
    // checkable/checked footgun.

    readonly property color fillColor: root.highlighted ? pulseColor : "#ffffff"
    readonly property color textColor: root.highlighted ? "#ffffff" : "#1c1f26"

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

            readonly property bool pulsing: root.hovered && root.enabled && !root.pressed

            SequentialAnimation on opacity {
                loops: Animation.Infinite
                running: pulseRing.pulsing
                NumberAnimation { to: 0.8; duration: root.pulseDuration / 2 }
                NumberAnimation { to: 0; duration: root.pulseDuration / 2 }
            }

            SequentialAnimation on scale {
                loops: Animation.Infinite
                running: pulseRing.pulsing
                NumberAnimation { from: 1; to: 1.08; duration: root.pulseDuration / 2 }
                NumberAnimation { from: 1.08; to: 1; duration: root.pulseDuration / 2 }
            }

            // The pulse can stop mid-cycle (pulsing turns false while opacity/
            // scale are partway through a loop), leaving a visible ring or an
            // enlarged button. Snap both back to their base values whenever
            // pulsing stops.
            NumberAnimation {
                target: pulseRing
                property: "opacity"
                to: 0
                duration: 150
                running: !pulseRing.pulsing
            }

            NumberAnimation {
                target: pulseRing
                property: "scale"
                to: 1
                duration: 150
                running: !pulseRing.pulsing
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: 6
            color: root.pressed ? Qt.darker(root.fillColor, 1.15) : root.fillColor
            border.color: root.highlighted ? "transparent" : "#d5d9e0"
            border.width: root.highlighted ? 0 : 1
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
