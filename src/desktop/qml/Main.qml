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
    readonly property int analysisProgressMinimumDurationMs: 2000
    property double analysisStartedAt: 0
    property bool pendingResultsNavigation: false

    function selectPage(name) {
        if (name !== "Analyze" && name !== "History" && name !== "Results") {
            return
        }

        selectedPage = name
        if (controller) {
            controller.lastPage = name
        }
    }

    function beginAnalysisTiming() {
        analysisStartedAt = Date.now()
        pendingResultsNavigation = false
        resultsNavigationTimer.stop()
    }

    function requestResultsPage() {
        if (!shell.controller) {
            shell.selectPage("Results")
            return
        }

        if (shell.analysisStartedAt > 0 && shell.controller.analysisRunning) {
            shell.pendingResultsNavigation = true
            return
        }

        if (shell.pendingResultsNavigation) {
            scheduleResultsNavigation()
            return
        }

        shell.selectPage("Results")
    }

    function scheduleResultsNavigation() {
        if (!shell.pendingResultsNavigation) {
            return
        }

        if (shell.analysisStartedAt <= 0) {
            shell.pendingResultsNavigation = false
            shell.selectPage("Results")
            return
        }

        const elapsed = Date.now() - shell.analysisStartedAt
        const remaining = Math.max(shell.analysisProgressMinimumDurationMs - elapsed, 0)
        if (remaining > 0) {
            resultsNavigationTimer.interval = remaining
            resultsNavigationTimer.start()
            return
        }

        resultsNavigationTimer.stop()
        shell.pendingResultsNavigation = false
        shell.analysisStartedAt = 0
        shell.selectPage("Results")
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

    onControllerChanged: {
        if (shell.controller && shell.controller.analysisRunning) {
            beginAnalysisTiming()
        }
    }

    Component.onCompleted: {
        if (shell.controller && shell.controller.analysisRunning) {
            beginAnalysisTiming()
        }
    }

    Timer {
        id: resultsNavigationTimer
        objectName: "resultsNavigationMinimumTimer"
        interval: shell.analysisProgressMinimumDurationMs
        repeat: false
        onTriggered: {
            shell.pendingResultsNavigation = false
            shell.analysisStartedAt = 0
            shell.selectPage("Results")
        }
    }

    Connections {
        target: shell.controller
        ignoreUnknownSignals: true

        function onCurrentReportChanged() {
            shell.requestResultsPage()
        }

        function onAnalysisRunningChanged(running) {
            if (running) {
                shell.beginAnalysisTiming()
            } else {
                shell.scheduleResultsNavigation()
            }
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
            Layout.preferredWidth: workbenchSidebar.preferredWidth
            compact: shell.width <= 1120
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
