import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "pages"

ApplicationWindow {
    id: shell
    visible: true
    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 600
    title: "CodeQA"

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

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12

            Item {
                Layout.fillWidth: true
            }

            Button {
                objectName: "openReportButton"
                text: "Open report…"
                enabled: shell.controller && !shell.controller.analysisRunning
                onClicked: openReportDialog.open()
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 190
            color: "#20242c"

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

                Button {
                    id: analyzeNavigationButton
                    objectName: "analyzeNavigationButton"
                    Layout.fillWidth: true
                    text: "Analyze"
                    checkable: true
                    checked: shell.selectedPage === "Analyze"
                    onClicked: shell.selectPage("Analyze")
                }

                Button {
                    id: historyNavigationButton
                    objectName: "historyNavigationButton"
                    Layout.fillWidth: true
                    text: "History"
                    checkable: true
                    checked: shell.selectedPage === "History"
                    onClicked: shell.selectPage("History")
                }

                Button {
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
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: shell.selectedPage === "Analyze" ? 0 : shell.selectedPage === "History" ? 1 : 2

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
