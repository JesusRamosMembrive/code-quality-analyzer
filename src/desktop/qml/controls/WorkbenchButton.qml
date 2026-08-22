import QtQuick
import QtQuick.Templates as T
import ".." as Root

T.Button {
    id: root

    property string variant: "primary"

    readonly property bool primaryVariant: root.variant === "primary"
    readonly property bool secondaryVariant: root.variant === "secondary"
    readonly property bool quietVariant: root.variant === "quiet"
    readonly property color baseFillColor: root.primaryVariant
        ? Root.CodeQaTheme.accent
        : (root.secondaryVariant ? Root.CodeQaTheme.surface : "transparent")
    readonly property color baseBorderColor: root.primaryVariant
        ? Root.CodeQaTheme.accent
        : (root.secondaryVariant ? Root.CodeQaTheme.border : "transparent")
    readonly property color baseTextColor: root.primaryVariant
        ? "#ffffff"
        : (root.quietVariant ? Root.CodeQaTheme.mutedInk : Root.CodeQaTheme.ink)

    hoverEnabled: true
    leftPadding: Root.CodeQaTheme.spaceLg
    rightPadding: Root.CodeQaTheme.spaceLg
    topPadding: Root.CodeQaTheme.spaceSm
    bottomPadding: Root.CodeQaTheme.spaceSm
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                             implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                              implicitContentHeight + topPadding + bottomPadding,
                              36)
    opacity: root.enabled ? 1.0 : 0.55

    background: Item {
        implicitWidth: 120
        implicitHeight: 36

        Rectangle {
            anchors.fill: parent
            radius: Root.CodeQaTheme.radiusSm
            color: {
                if (!root.enabled) {
                    return root.primaryVariant ? Root.CodeQaTheme.accentSoft : Root.CodeQaTheme.surfaceSubtle
                }
                if (root.pressed) {
                    return root.primaryVariant
                        ? Qt.darker(root.baseFillColor, 1.12)
                        : (root.secondaryVariant
                            ? Root.CodeQaTheme.surfaceSubtle
                            : Qt.rgba(0.2, 0.18, 0.14, 0.12))
                }
                if (root.hovered) {
                    return root.primaryVariant
                        ? Qt.lighter(root.baseFillColor, 1.05)
                        : (root.secondaryVariant ? Root.CodeQaTheme.surfaceSubtle : Root.CodeQaTheme.accentSoft)
                }
                if (root.highlighted && root.quietVariant) {
                    return Root.CodeQaTheme.accentSoft
                }
                return root.baseFillColor
            }
            border.width: root.activeFocus ? 2 : (root.secondaryVariant ? 1 : 0)
            border.color: root.activeFocus ? Root.CodeQaTheme.accent : root.baseBorderColor
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            radius: Root.CodeQaTheme.radiusSm + 2
            color: "transparent"
            border.width: root.activeFocus ? 1 : 0
            border.color: Qt.rgba(0.835, 0.604, 0.247, 0.35)
            visible: root.activeFocus
        }
    }

    contentItem: Text {
        text: root.text
        font: root.font
        color: {
            if (!root.enabled) {
                return Root.CodeQaTheme.mutedInk
            }
            if (root.highlighted && !root.primaryVariant) {
                return Root.CodeQaTheme.ink
            }
            return root.baseTextColor
        }
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
