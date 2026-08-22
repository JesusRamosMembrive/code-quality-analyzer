pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import ".." as Root
import "../controls"

Item {
    id: historyPage
    objectName: "historyPage"

    property var controller: null
    property string pendingDeleteTimestamp: ""
    readonly property bool hasProject: controller && controller.currentProject.toString() !== ""
    property alias deleteConfirmation: deleteConfirmationDialog
    property alias deleteConfirmationText: deleteConfirmationMessage.text

    function selectProject(projectRoot) {
        if (!controller || controller.analysisRunning) {
            return
        }

        controller.selectProject(projectRoot)
        controller.refreshSnapshots()
    }

    FolderDialog {
        id: projectFolderDialog
        title: "Select project folder"
        currentFolder: historyPage.controller ? historyPage.controller.currentProject : ""
        onAccepted: historyPage.selectProject(selectedFolder)
    }

    Dialog {
        id: deleteConfirmationDialog
        title: "Delete snapshot?"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.Cancel

        contentItem: Label {
            id: deleteConfirmationMessage
            text: "Delete snapshot " + historyPage.pendingDeleteTimestamp + "? This cannot be undone."
            wrapMode: Text.Wrap
        }

        onAccepted: {
            const timestamp = historyPage.pendingDeleteTimestamp
            historyPage.pendingDeleteTimestamp = ""
            if (historyPage.controller && !historyPage.controller.analysisRunning && timestamp !== "") {
                historyPage.controller.deleteSnapshot(timestamp)
            }
        }
        onRejected: historyPage.pendingDeleteTimestamp = ""
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Root.CodeQaTheme.spaceLg

        WorkbenchSurface {
            id: historyPageSurface
            objectName: "historyPageSurface"
            Layout.fillWidth: true
            Layout.fillHeight: true
            width: parent.width
            height: parent.height
            elevated: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Root.CodeQaTheme.spaceXl
                spacing: Root.CodeQaTheme.spaceLg

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Root.CodeQaTheme.spaceXs

                    Label {
                        text: "Snapshot history"
                        color: Root.CodeQaTheme.ink
                        font.pixelSize: 28
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        text: "Inspect previous analysis runs, reopen a snapshot, or compare two saved points in time."
                        color: Root.CodeQaTheme.mutedInk
                        wrapMode: Text.Wrap
                    }
                }

                WorkbenchSurface {
                    Layout.fillWidth: true
                    tintColor: Root.CodeQaTheme.surfaceSubtle

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Root.CodeQaTheme.spaceLg
                        spacing: Root.CodeQaTheme.spaceMd

                        WorkbenchButton {
                            objectName: "historySelectProjectButton"
                            text: "Select project…"
                            variant: "secondary"
                            enabled: historyPage.controller && !historyPage.controller.analysisRunning
                            onClicked: projectFolderDialog.open()
                        }

                        WorkbenchButton {
                            objectName: "refreshSnapshotsButton"
                            text: "Refresh"
                            variant: "secondary"
                            enabled: historyPage.hasProject && !historyPage.controller.analysisRunning
                            onClicked: historyPage.controller.refreshSnapshots()
                        }

                        Label {
                            Layout.fillWidth: true
                            text: historyPage.hasProject
                                ? historyPage.controller.currentProject.toString()
                                : "No project selected"
                            color: Root.CodeQaTheme.mutedInk
                            elide: Text.ElideMiddle
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }

                WorkbenchSurface {
                    Layout.fillWidth: true
                    visible: historyErrorMessage.text !== ""
                    tintColor: Root.CodeQaTheme.errorSoft

                    Label {
                        id: historyErrorMessage
                        objectName: "historyErrorMessage"
                        anchors.fill: parent
                        anchors.margins: Root.CodeQaTheme.spaceLg
                        visible: text !== ""
                        text: historyPage.controller ? historyPage.controller.errorMessage : ""
                        color: Root.CodeQaTheme.error
                        wrapMode: Text.Wrap
                    }
                }

                WorkbenchSurface {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    width: parent.width
                    height: Math.max(240, parent.height - 270)

                    Item {
                        anchors.fill: parent
                        anchors.margins: Root.CodeQaTheme.spaceLg

                        Label {
                            anchors.centerIn: parent
                            visible: !snapshotList.count
                            text: historyPage.hasProject
                                ? "No snapshots found in .codeqa/snapshots."
                                : "Select a project to view its snapshots."
                            color: Root.CodeQaTheme.mutedInk
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        ListView {
                            id: snapshotList
                            objectName: "snapshotList"
                            anchors.fill: parent
                            visible: count > 0
                            spacing: Root.CodeQaTheme.spaceSm
                            clip: true
                            model: historyPage.controller ? historyPage.controller.snapshots : null

                            delegate: WorkbenchSurface {
                                id: snapshotRow
                                required property string timestamp
                                readonly property string timestampText: timestamp
                                property alias openAction: openButton
                                property alias deleteAction: deleteButton

                                width: ListView.view.width
                                height: 60
                                tintColor: Root.CodeQaTheme.surfaceSubtle

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: Root.CodeQaTheme.spaceMd
                                    spacing: Root.CodeQaTheme.spaceMd

                                    Label {
                                        Layout.fillWidth: true
                                        text: snapshotRow.timestamp
                                        color: Root.CodeQaTheme.ink
                                        font.family: "monospace"
                                        textFormat: Text.PlainText
                                    }

                                    WorkbenchButton {
                                        id: openButton
                                        text: "Open"
                                        variant: "secondary"
                                        enabled: historyPage.controller && !historyPage.controller.analysisRunning
                                        onClicked: historyPage.controller.openSnapshot(snapshotRow.timestamp)
                                    }

                                    WorkbenchButton {
                                        id: deleteButton
                                        text: "Delete"
                                        variant: "quiet"
                                        enabled: historyPage.controller && !historyPage.controller.analysisRunning
                                        onClicked: {
                                            historyPage.pendingDeleteTimestamp = snapshotRow.timestamp
                                            deleteConfirmationDialog.open()
                                        }
                                    }
                                }
                            }

                            ScrollBar.vertical: ScrollBar {}
                        }
                    }
                }

                WorkbenchSurface {
                    Layout.fillWidth: true
                    tintColor: Root.CodeQaTheme.surfaceSubtle

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Root.CodeQaTheme.spaceLg
                        spacing: Root.CodeQaTheme.spaceMd

                        Label {
                            text: "Compare:"
                            color: Root.CodeQaTheme.ink
                        }

                        ComboBox {
                            id: fromSnapshotCombo
                            objectName: "fromSnapshotCombo"
                            Layout.preferredWidth: 200
                            model: historyPage.controller ? historyPage.controller.snapshots : null
                            textRole: "timestamp"
                        }

                        Label {
                            text: "→"
                            color: Root.CodeQaTheme.mutedInk
                        }

                        ComboBox {
                            id: toSnapshotCombo
                            objectName: "toSnapshotCombo"
                            Layout.preferredWidth: 200
                            model: historyPage.controller ? historyPage.controller.snapshots : null
                            textRole: "timestamp"
                        }

                        WorkbenchButton {
                            id: compareSnapshotsButton
                            objectName: "compareSnapshotsButton"
                            text: "Compare"
                            variant: "primary"
                            enabled: historyPage.controller
                                && !historyPage.controller.analysisRunning
                                && fromSnapshotCombo.currentText !== ""
                                && toSnapshotCombo.currentText !== ""
                                && fromSnapshotCombo.currentText !== toSnapshotCombo.currentText
                            onClicked: historyPage.controller.compareSnapshots(
                                fromSnapshotCombo.currentText, toSnapshotCombo.currentText)
                        }

                        Label {
                            id: snapshotComparisonResult
                            objectName: "snapshotComparisonResult"
                            Layout.fillWidth: true
                            color: Root.CodeQaTheme.mutedInk
                            elide: Text.ElideRight
                            visible: historyPage.controller && historyPage.controller.snapshotComparisonAvailable
                            text: historyPage.controller
                                ? historyPage.controller.snapshotComparisonNewCount + " new, "
                                    + historyPage.controller.snapshotComparisonExistingCount + " existing, "
                                    + historyPage.controller.snapshotComparisonResolvedCount + " resolved"
                                : ""
                        }
                    }
                }
            }
        }
    }
}
