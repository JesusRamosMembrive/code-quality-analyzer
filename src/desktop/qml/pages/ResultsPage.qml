pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import ".." as Root

Item {
    id: root

    property var controller: null

    FileDialog {
        id: exportMarkdownDialog
        title: "Export report as Markdown"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Markdown files (*.md)", "All files (*)"]
        defaultSuffix: "md"

        onAccepted: {
            if (root.controller) {
                root.controller.exportMarkdown(selectedFile)
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Root.CodeQaTheme.spaceLg

        TabBar {
            id: resultsTabs
            objectName: "resultsTabs"
            Layout.fillWidth: true
            currentIndex: 0

            TabButton {
                id: overviewTab
                objectName: "overviewTab"
                text: "Overview"
            }

            TabButton {
                id: findingsTab
                objectName: "findingsTab"
                text: "Findings"
            }

            TabButton {
                id: filesMetricsTab
                objectName: "filesMetricsTab"
                text: "Files and metrics"
            }

            TabButton {
                id: suppressedTab
                objectName: "suppressedTab"
                text: "Suppressed"
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: resultsTabs.currentIndex

            ResultsOverview {
                objectName: "resultsOverviewPage"
                Layout.fillWidth: true
                Layout.fillHeight: true
                controller: root.controller
                onExportMarkdownRequested: exportMarkdownDialog.open()
            }

            ResultsFindings {
                objectName: "resultsFindingsPage"
                Layout.fillWidth: true
                Layout.fillHeight: true
                controller: root.controller
                onExportMarkdownRequested: exportMarkdownDialog.open()
            }

            ResultsFilesMetrics {
                objectName: "resultsFilesMetricsPage"
                Layout.fillWidth: true
                Layout.fillHeight: true
                controller: root.controller
            }

            ResultsSuppressed {
                objectName: "resultsSuppressedPage"
                Layout.fillWidth: true
                Layout.fillHeight: true
                controller: root.controller
            }
        }
    }
}
