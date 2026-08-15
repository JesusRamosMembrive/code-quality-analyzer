import QtQuick
import QtQuick.Effects

// Frosted-glass panel. Captures backgroundItem, blurs it, and layers a tint,
// a depth gradient, and a light edge on top. Adapted from QML-SnippetsExamples'
// examples/frostglass/GlassCard.qml — tintColor is exposed (instead of a fixed
// white tint) so panels sitting over dark UI chrome can keep readable content.
Rectangle {
    id: root

    // Item to capture and blur. Must be an ancestor or a sibling sharing the
    // same coordinate system as this panel (NOT a ShaderEffectSource itself).
    property Item backgroundItem

    property real blurRadius: 28
    property color tintColor: Qt.rgba(0, 0, 0, 0.35)
    property real borderOpacity: 0.12

    clip: true
    color: "transparent"

    ShaderEffectSource {
        id: bgCapture
        sourceItem: root.backgroundItem
        x: root.backgroundItem ? -root.x : 0
        y: root.backgroundItem ? -root.y : 0
        width: root.backgroundItem ? root.backgroundItem.width : root.width
        height: root.backgroundItem ? root.backgroundItem.height : root.height
        // backgroundItem exists only to be blurred into this panel — it must
        // never be drawn at its own on-screen position (that's what leaked
        // past the panel's edge when this was false).
        hideSource: true
        live: true
    }

    MultiEffect {
        source: bgCapture
        x: bgCapture.x
        y: bgCapture.y
        width: bgCapture.width
        height: bgCapture.height

        blurEnabled: root.backgroundItem !== null && root.backgroundItem !== undefined
        blur: 1.0
        blurMax: Math.max(4, Math.round(root.blurRadius))
        blurMultiplier: 0.5
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: root.tintColor
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.08) }
            GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, 0.0) }
            GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.18) }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: "transparent"
        border.color: Qt.rgba(1, 1, 1, root.borderOpacity)
        border.width: 1
    }
}
