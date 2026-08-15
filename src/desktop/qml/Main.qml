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
    height: 800
    minimumWidth: 960
    minimumHeight: 600
    title: "CodeQA"
    color: "#1a1d24"

    font.family: "Segoe UI Variable Text"
    font.pointSize: 11

    palette.window: "#1a1d24"
    palette.windowText: "#e7e9ee"
    palette.base: "#242832"
    palette.text: "#e7e9ee"
    palette.placeholderText: "#9aa1b0"
    palette.button: "#2a2f3a"
    palette.buttonText: "#e7e9ee"
    palette.highlight: "#2f6fed"
    palette.highlightedText: "#ffffff"

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
        const contentMatch = findDescendant(contentItem, childObjectName)
        return contentMatch ? contentMatch : findDescendant(header, childObjectName)
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

    // Confined to (and a bit wider than) the sidebar's own footprint so it
    // never shows through the main content pages. Blurring a smooth gradient
    // produces the same smooth gradient (no visible texture), so the glass
    // effect needs actual shapes/edges behind it — hence the overlapping
    // circles instead of a flat wash.
    Item {
        id: backdrop
        width: 230
        height: parent.height
        // Without this, children wider/taller than backdrop itself (the
        // decorative circles) paint straight onto the screen past its
        // bounds — they're not confined by GlassPanel's own clip since
        // backdrop is a directly rendered sibling, not a child of the panel.
        clip: true

        Rectangle {
            anchors.fill: parent
            color: "#20242c"
        }

        Rectangle {
            width: 220
            height: 220
            radius: width / 2
            color: "#2f6fed"
            x: -70
            y: -50
        }

        Rectangle {
            width: 180
            height: 180
            radius: width / 2
            color: "#22d3c7"
            x: 90
            y: parent.height * 0.35
        }

        Rectangle {
            width: 260
            height: 260
            radius: width / 2
            color: "#ff6f91"
            opacity: 0.85
            x: -60
            y: parent.height * 0.7
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        GlassPanel {
            Layout.fillHeight: true
            Layout.preferredWidth: 190
            backgroundItem: backdrop
            blurRadius: 40
            tintColor: Qt.rgba(0, 0, 0, 0.28)

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10

                Label {
                    text: "CodeQA"
                    color: "white"
                    font.pixelSize: 22
                    font.bold: true
                }

                PulseButton {
                    id: analyzeNavigationButton
                    objectName: "analyzeNavigationButton"
                    Layout.fillWidth: true
                    text: "Analyze"
                    checkable: true
                    checked: shell.selectedPage === "Analyze"
                    onClicked: shell.selectPage("Analyze")
                }

                PulseButton {
                    id: historyNavigationButton
                    objectName: "historyNavigationButton"
                    Layout.fillWidth: true
                    text: "History"
                    checkable: true
                    checked: shell.selectedPage === "History"
                    onClicked: shell.selectPage("History")
                }

                PulseButton {
                    id: resultsNavigationButton
                    objectName: "resultsNavigationButton"
                    Layout.fillWidth: true
                    text: "Results"
                    checkable: true
                    checked: shell.selectedPage === "Results"
                    onClicked: shell.selectPage("Results")
                }

                Item {
                    Layout.fillHeight: true
                }

                PulseButton {
                    id: openReportButton
                    objectName: "openReportButton"
                    Layout.fillWidth: true
                    text: "Open report…"
                    enabled: shell.controller && !shell.controller.analysisRunning
                    onClicked: openReportDialog.open()
                }

                PulseButton {
                    id: clearRecentPathsButton
                    objectName: "clearRecentPathsButton"
                    Layout.fillWidth: true
                    text: "Clear recent paths"
                    enabled: shell.controller && !shell.controller.analysisRunning
                    onClicked: {
                        if (shell.controller) {
                            shell.controller.clearRecentPaths()
                        }
                    }
                }
            }
        }

        StackLayout {
            id: pageStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 16
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
