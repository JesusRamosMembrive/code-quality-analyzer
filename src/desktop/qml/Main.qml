import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "pages"
import "controls"

ApplicationWindow {
    id: shell
    visible: true
    width: 1280
    height: 900
    minimumWidth: 960
    minimumHeight: 600
    title: "CodeQA"
    color: CodeQaTheme.canvas

    font.family: "Segoe UI Variable Text"
    font.pointSize: 11

    palette.window: CodeQaTheme.canvas
    palette.windowText: CodeQaTheme.ink
    palette.base: CodeQaTheme.surface
    palette.text: CodeQaTheme.ink
    palette.placeholderText: CodeQaTheme.mutedInk
    palette.button: CodeQaTheme.surface
    palette.buttonText: CodeQaTheme.ink
    palette.highlight: CodeQaTheme.accent
    palette.highlightedText: CodeQaTheme.onAccent

    property var controller: typeof appController !== "undefined" ? appController : null
    property string selectedPage: controller && controller.lastPage !== "" ? controller.lastPage : "Results"

    function selectPage(name) {
        if (name !== "Analyze" && name !== "History" && name !== "Results") {
            return
        }

        selectedPage = name
        if (controller) {
            controller.lastPage = name
        }
    }

    function findChild(childObjectName) {
        return findDescendant(contentItem, childObjectName)
    }

    function findDescendant(parentObject, childObjectName) {
        if (!parentObject) {
            return null
        }

        if (parentObject.objectName === childObjectName) {
            return parentObject
        }

        const children = parentObject.children
        if (!children) {
            return null
        }

        for (let index = 0; index < children.length; ++index) {
            const match = findDescendant(children[index], childObjectName)
            if (match) {
                return match
            }
        }

        return null
    }

    Connections {
        target: shell.controller
        ignoreUnknownSignals: true

        function onCurrentReportChanged() {
            shell.selectPage("Results")
        }
    }

    FileDialog {
        id: openReportDialog
        title: "Open report"
        nameFilters: ["JSON report files (*.json)", "All files (*)"]

        onAccepted: {
            if (shell.controller) {
                shell.controller.openReport(selectedFile)
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        WorkbenchSidebar {
            id: workbenchSidebar
            objectName: "workbenchSidebar"
            Layout.fillHeight: true
            Layout.preferredWidth: CodeQaTheme.sidebarWidth
            selectedPage: shell.selectedPage
            analysisRunning: shell.controller ? shell.controller.analysisRunning : false
            onPageRequested: function(page) { shell.selectPage(page) }
            onOpenReportRequested: openReportDialog.open()
            onClearRecentPathsRequested: {
                if (shell.controller) {
                    shell.controller.clearRecentPaths()
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            WorkbenchHeader {
                id: workbenchHeader
                objectName: "workbenchHeader"
                Layout.fillWidth: true
                Layout.margins: CodeQaTheme.spaceXl
                Layout.bottomMargin: 0
                pageTitle: shell.selectedPage
                projectName: shell.controller
                    ? shell.controller.currentProject.toString()
                    : "No project selected"
                branchText: "Local project"
            }

            StackLayout {
                id: pageStack
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: CodeQaTheme.spaceXl
                currentIndex: shell.selectedPage === "Analyze" ? 0 : shell.selectedPage === "History" ? 1 : 2

                NumberAnimation {
                    id: pageFadeIn
                    target: pageStack
                    property: "opacity"
                    from: 0
                    to: 1
                    duration: 180
                    easing.type: Easing.OutCubic
                }

                onCurrentIndexChanged: pageFadeIn.restart()

                AnalyzePage {
                    id: analyzePage
                    controller: shell.controller
                }

                HistoryPage {
                    id: historyPage
                    controller: shell.controller
                }

                ResultsPage {
                    id: resultsPage
                    controller: shell.controller
                }
            }
        }
    }
}
