import QtQuick
import QtQuick.Effects
import ".." as Root

Rectangle {
    id: root

    property bool selected: false
    property bool elevated: false
    property color tintColor: "transparent"

    radius: Root.CodeQaTheme.radiusMd
    border.width: 1
    border.color: root.selected ? Root.CodeQaTheme.accent : Root.CodeQaTheme.border
    color: root.selected
        ? Root.CodeQaTheme.accentSoft
        : (root.tintColor.a > 0 ? root.tintColor : Root.CodeQaTheme.surface)

    layer.enabled: root.elevated
    layer.effect: MultiEffect {
        shadowEnabled: root.elevated
        shadowColor: Qt.rgba(0.2, 0.16, 0.1, 0.16)
        shadowVerticalOffset: 3
        shadowHorizontalOffset: 0
        shadowBlur: 0.6
    }
}
