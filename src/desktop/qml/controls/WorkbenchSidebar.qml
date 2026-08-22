import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".." as Root

Rectangle {
    id: root

    property string selectedPage: "Results"
    property bool analysisRunning: false
    signal pageRequested(string page)
    signal openReportRequested()
    signal clearRecentPathsRequested()

    color: Root.CodeQaTheme.sidebar

    function isSelected(page) {
        return root.selectedPage === page
    }

    component NavigationButton: Item {
        id: navRoot

        required property string title
        required property string page
        required property string objectNameValue

        implicitHeight: 44
        implicitWidth: 160

        WorkbenchButton {
            id: button
            objectName: navRoot.objectNameValue
            anchors.fill: parent
            text: navRoot.title
            variant: "quiet"
            highlighted: root.isSelected(navRoot.page)
            font.bold: root.isSelected(navRoot.page)

            background: Rectangle {
                radius: Root.CodeQaTheme.radiusMd
                color: root.isSelected(navRoot.page) ? Root.CodeQaTheme.accentSoft : "transparent"
                border.width: root.isSelected(navRoot.page) ? 1 : 0
                border.color: Root.CodeQaTheme.accent
            }

            contentItem: Text {
                text: button.text
                font: button.font
                color: root.isSelected(navRoot.page) ? Root.CodeQaTheme.ink : Root.CodeQaTheme.sidebarText
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            onClicked: root.pageRequested(navRoot.page)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Root.CodeQaTheme.spaceLg
        spacing: Root.CodeQaTheme.spaceLg

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Root.CodeQaTheme.spaceXs

            Label {
                text: "CodeQA"
                color: Root.CodeQaTheme.surface
                font.pixelSize: 24
                font.bold: true
            }

            Label {
                text: "Warm Workbench"
                color: Root.CodeQaTheme.sidebarMutedText
                opacity: 0.72
                font.pixelSize: 12
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Root.CodeQaTheme.spaceSm

            Label {
                text: "Workspace"
                color: Root.CodeQaTheme.sidebarMutedText
                opacity: 0.62
                font.pixelSize: 11
                font.letterSpacing: 1
            }

            NavigationButton {
                title: "Analyze"
                page: "Analyze"
                objectNameValue: "analyzeNavigationButton"
                Layout.fillWidth: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Root.CodeQaTheme.spaceSm

            Label {
                text: "Explore"
                color: Root.CodeQaTheme.sidebarMutedText
                opacity: 0.62
                font.pixelSize: 11
                font.letterSpacing: 1
            }

            NavigationButton {
                title: "History"
                page: "History"
                objectNameValue: "historyNavigationButton"
                Layout.fillWidth: true
            }

            NavigationButton {
                title: "Results"
                page: "Results"
                objectNameValue: "resultsNavigationButton"
                Layout.fillWidth: true
            }
        }

        Item {
            Layout.fillHeight: true
        }

        WorkbenchSurface {
            Layout.fillWidth: true
            tintColor: Root.CodeQaTheme.sidebarSurfaceTint
            border.color: Root.CodeQaTheme.sidebarSurfaceBorder

            implicitHeight: contextLayout.implicitHeight + Root.CodeQaTheme.spaceLg * 2

            ColumnLayout {
                id: contextLayout
                anchors.fill: parent
                anchors.margins: Root.CodeQaTheme.spaceLg
                spacing: Root.CodeQaTheme.spaceSm

                Label {
                    text: "Project context"
                    color: Root.CodeQaTheme.surface
                    font.pixelSize: 13
                    font.bold: true
                }

                Label {
                    Layout.fillWidth: true
                    text: root.selectedPage === "Analyze"
                        ? "Configure the active project and launch analysis."
                        : root.selectedPage === "History"
                            ? "Review recent snapshots and compare changes."
                            : "Inspect findings and exported reports."
                    color: Root.CodeQaTheme.sidebarMutedText
                    opacity: 0.72
                    wrapMode: Text.Wrap
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Root.CodeQaTheme.sidebarDivider
                }

                WorkbenchButton {
                    id: openReportButton
                    objectName: "openReportButton"
                    Layout.fillWidth: true
                    text: "Open report..."
                    variant: "secondary"
                    enabled: !root.analysisRunning
                    onClicked: root.openReportRequested()
                }

                WorkbenchButton {
                    id: clearRecentPathsButton
                    objectName: "clearRecentPathsButton"
                    Layout.fillWidth: true
                    text: "Clear recent paths"
                    variant: "quiet"
                    enabled: !root.analysisRunning
                    onClicked: root.clearRecentPathsRequested()
                }
            }
        }
    }
}
