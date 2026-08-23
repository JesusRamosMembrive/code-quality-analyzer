import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import ".." as Root

Rectangle {
    id: root

    property bool running: false
    property string phaseText: ""
    property string projectPath: ""

    readonly property var phaseLabels: [
        "Discovering files",
        "Parsing source files",
        "Measuring functions",
        "Applying rules",
        "Preparing report"
    ]
    readonly property var phaseShortLabels: ["Files", "Parse", "Metrics", "Rules", "Report"]
    readonly property int phaseCount: phaseLabels.length
    readonly property int activePhaseIndex: {
        const phaseIndex = phaseLabels.indexOf(root.phaseText)
        return phaseIndex >= 0 ? phaseIndex : 0
    }

    objectName: "analysisProgressPanel"
    visible: root.running
    implicitHeight: 258
    radius: Root.CodeQaTheme.radiusLg
    color: Root.CodeQaTheme.sidebar
    border.width: 1
    border.color: Root.CodeQaTheme.sidebarSurfaceBorder

    RowLayout {
        anchors.fill: parent
        anchors.margins: Root.CodeQaTheme.spaceXl
        spacing: Root.CodeQaTheme.spaceXl

        Item {
            id: analysisProgressRing
            objectName: "analysisProgressRing"
            property bool running: root.running

            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 178
            Layout.preferredHeight: 178

            Canvas {
                id: ringTrack
                anchors.fill: parent

                onPaint: {
                    const context = getContext("2d")
                    const center = width / 2
                    const radius = (Math.min(width, height) / 2) - 16

                    context.clearRect(0, 0, width, height)
                    context.beginPath()
                    context.arc(center, center, radius, 0, Math.PI * 2)
                    context.lineWidth = 11
                    context.strokeStyle = Qt.rgba(1, 1, 1, 0.12)
                    context.stroke()
                }
            }

            Canvas {
                id: ringArc
                anchors.fill: parent

                onPaint: {
                    const context = getContext("2d")
                    const center = width / 2
                    const radius = (Math.min(width, height) / 2) - 16
                    const startAngle = -Math.PI / 2
                    const sweepAngle = Math.PI * 1.35

                    context.clearRect(0, 0, width, height)
                    context.beginPath()
                    context.arc(center, center, radius, startAngle, startAngle + sweepAngle)
                    context.lineWidth = 11
                    context.lineCap = "round"
                    context.strokeStyle = Root.CodeQaTheme.accent
                    context.stroke()
                }

                RotationAnimation on rotation {
                    from: 0
                    to: 360
                    duration: 2400
                    loops: Animation.Infinite
                    running: analysisProgressRing.running
                }
            }

            Rectangle {
                anchors.centerIn: parent
                width: 112
                height: 112
                radius: width / 2
                color: Qt.rgba(0.09, 0.12, 0.11, 0.72)
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.12)

                Column {
                    anchors.centerIn: parent
                    spacing: 2

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "%1 / %2".arg(root.activePhaseIndex + 1).arg(root.phaseCount)
                        color: Root.CodeQaTheme.sidebarText
                        font.pixelSize: 24
                        font.bold: true
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "PHASE"
                        color: Root.CodeQaTheme.sidebarMutedText
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 1.5
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: Root.CodeQaTheme.spaceSm

            RowLayout {
                Layout.fillWidth: true
                spacing: Root.CodeQaTheme.spaceSm

                Rectangle {
                    Layout.preferredWidth: 9
                    Layout.preferredHeight: 9
                    radius: width / 2
                    color: Root.CodeQaTheme.accent

                    SequentialAnimation on opacity {
                        loops: Animation.Infinite
                        running: root.running
                        NumberAnimation { to: 0.35; duration: 720 }
                        NumberAnimation { to: 1.0; duration: 720 }
                    }
                }

                Text {
                    text: "LIVE ANALYSIS"
                    color: Root.CodeQaTheme.accent
                    font.pixelSize: Root.CodeQaTheme.sectionLabelPixelSize
                    font.bold: true
                    font.letterSpacing: Root.CodeQaTheme.sectionLabelLetterSpacing
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: "WORKING"
                    color: Root.CodeQaTheme.sidebarMutedText
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1.1
                }
            }

            Text {
                Layout.fillWidth: true
                text: "Mapping your codebase"
                color: Root.CodeQaTheme.sidebarText
                font.pixelSize: 28
                font.bold: true
            }

            Text {
                id: analysisProgressPhaseLabel
                objectName: "analysisProgressPhaseLabel"
                Layout.fillWidth: true
                text: root.phaseText !== "" ? root.phaseText : "Preparing analysis"
                color: Root.CodeQaTheme.sidebarText
                font.pixelSize: 18
                font.bold: true
            }

            Text {
                Layout.fillWidth: true
                text: root.projectPath !== "" ? root.projectPath : "Preparing project"
                color: Root.CodeQaTheme.sidebarMutedText
                elide: Text.ElideMiddle
                font.pixelSize: 12
            }

            Item {
                Layout.preferredHeight: Root.CodeQaTheme.spaceSm
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Root.CodeQaTheme.spaceSm

                Repeater {
                    model: root.phaseLabels

                    delegate: ColumnLayout {
                        property bool active: index <= root.activePhaseIndex

                        objectName: "analysisProgressPhaseStep" + index
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        spacing: Root.CodeQaTheme.spaceXs

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 6
                            radius: height / 2
                            color: parent.active
                                ? Root.CodeQaTheme.accent
                                : Qt.rgba(1, 1, 1, 0.16)
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.phaseShortLabels[index]
                            color: parent.active
                                ? Root.CodeQaTheme.sidebarText
                                : Root.CodeQaTheme.sidebarMutedText
                            font.pixelSize: 11
                            font.bold: parent.active
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }
}
