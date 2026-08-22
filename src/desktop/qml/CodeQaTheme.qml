pragma Singleton

import QtQuick

QtObject {
    readonly property color canvas: "#f3eee6"
    readonly property color surface: "#fffdf9"
    readonly property color surfaceSubtle: "#ebe4d9"
    readonly property color onAccent: surface
    readonly property color ink: "#342f2a"
    readonly property color mutedInk: "#81776b"
    readonly property color border: "#ded5c8"
    readonly property color sidebar: "#2f3835"
    readonly property color sidebarText: "#dbe3dc"
    readonly property color sidebarMutedText: "#dbe3de"
    readonly property color sidebarSurfaceTint: "#14fffdf9"
    readonly property color sidebarSurfaceBorder: "#38f0c982"
    readonly property color sidebarDivider: "#2ef0c982"
    readonly property color accent: "#d59a3f"
    readonly property color accentSoft: "#f6e7c7"
    readonly property color success: "#4f9276"
    readonly property color successSoft: "#e5f0e8"
    readonly property color error: "#bd5e51"
    readonly property color errorSoft: "#f4dfdc"
    readonly property color warning: "#b27b2d"
    readonly property color warningSoft: "#f6e9d2"
    readonly property color link: "#4f6585"

    readonly property int spaceXs: 4
    readonly property int spaceSm: 8
    readonly property int spaceMd: 12
    readonly property int spaceLg: 18
    readonly property int spaceXl: 24
    readonly property int radiusSm: 7
    readonly property int radiusMd: 11
    readonly property int radiusLg: 14
    readonly property int sectionLabelPixelSize: 12
    readonly property bool sectionLabelBold: true
    readonly property real sectionLabelLetterSpacing: 1.2
    readonly property int sidebarWidth: 208
    readonly property int compactSidebarWidth: 64
    readonly property int contentMaxWidth: 1480
}
