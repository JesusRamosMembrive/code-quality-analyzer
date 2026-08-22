import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".." as Root

Item {
    id: root

    property string projectName: "No project selected"
    property string branchText: ""
    property string pageTitle: "Results"

    implicitHeight: headerSurface.implicitHeight

    WorkbenchSurface {
        id: headerSurface
        anchors.fill: parent
        elevated: true

        implicitHeight: contentLayout.implicitHeight + Root.CodeQaTheme.spaceXl * 2

        RowLayout {
            id: contentLayout
            anchors.fill: parent
            anchors.margins: Root.CodeQaTheme.spaceXl
            spacing: Root.CodeQaTheme.spaceLg

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Root.CodeQaTheme.spaceXs

                RowLayout {
                    spacing: Root.CodeQaTheme.spaceSm

                    Rectangle {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        radius: 10
                        color: Root.CodeQaTheme.accentSoft
                        border.width: 1
                        border.color: Root.CodeQaTheme.accent

                        Label {
                            anchors.centerIn: parent
                            text: "CQ"
                            color: Root.CodeQaTheme.ink
                            font.bold: true
                        }
                    }

                    ColumnLayout {
                        spacing: 0

                        Label {
                            text: "CodeQA"
                            color: Root.CodeQaTheme.ink
                            font.pixelSize: 18
                            font.bold: true
                        }

                        Label {
                            text: root.pageTitle
                            color: Root.CodeQaTheme.mutedInk
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: root.projectName
                    color: Root.CodeQaTheme.ink
                    elide: Text.ElideMiddle
                }
            }

            RowLayout {
                spacing: Root.CodeQaTheme.spaceMd

                ColumnLayout {
                    spacing: 0

                    Label {
                        text: root.branchText
                        color: Root.CodeQaTheme.ink
                        horizontalAlignment: Text.AlignRight
                        font.bold: true
                    }

                    Label {
                        text: "Workbench ready"
                        color: Root.CodeQaTheme.mutedInk
                        horizontalAlignment: Text.AlignRight
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    radius: 20
                    color: Root.CodeQaTheme.sidebar

                    Label {
                        anchors.centerIn: parent
                        text: "QA"
                        color: Root.CodeQaTheme.surface
                        font.bold: true
                    }
                }
            }
        }
    }
}
