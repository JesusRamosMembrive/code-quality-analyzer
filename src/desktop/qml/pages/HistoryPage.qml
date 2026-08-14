import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Item {
    id: historyPage
    objectName: "historyPage"

    property var controller: null
    property string pendingDeleteTimestamp: ""
    property alias deleteConfirmation: deleteConfirmationDialog
    property alias deleteConfirmationText: deleteConfirmationMessage.text

    function selectProject(projectRoot) {
        if (!controller || controller.analysisRunning) {
            return
        }

        controller.selectProject(projectRoot)
        if (controller.configurationAvailable) {
            controller.refreshSnapshots()
        }
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
        anchors.margins: 24
        spacing: 16

        Label {
            text: "Snapshot history"
            font.pixelSize: 24
            font.bold: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                objectName: "historySelectProjectButton"
                text: "Select project…"
                enabled: historyPage.controller && !historyPage.controller.analysisRunning
                onClicked: projectFolderDialog.open()
            }

            Button {
                objectName: "refreshSnapshotsButton"
                text: "Refresh"
                enabled: historyPage.controller
                    && historyPage.controller.currentProject.toString() !== ""
                    && historyPage.controller.configurationAvailable
                    && !historyPage.controller.analysisRunning
                onClicked: historyPage.controller.refreshSnapshots()
            }

            Label {
                Layout.fillWidth: true
                text: historyPage.controller
                    && historyPage.controller.currentProject.toString() !== ""
                    ? historyPage.controller.currentProject.toString()
                    : "No project selected"
                elide: Text.ElideMiddle
                horizontalAlignment: Text.AlignRight
            }
        }

        Label {
            id: historyErrorMessage
            objectName: "historyErrorMessage"
            Layout.fillWidth: true
            visible: text !== ""
            text: historyPage.controller ? historyPage.controller.errorMessage : ""
            color: "#b3261e"
            wrapMode: Text.Wrap
        }

        Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !snapshotList.count
            text: historyPage.controller
                && historyPage.controller.currentProject.toString() !== ""
                ? "No snapshots found in .codeqa/snapshots."
                : "Select a project to view its snapshots."
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        ListView {
            id: snapshotList
            objectName: "snapshotList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: count > 0
            spacing: 8
            clip: true
            model: historyPage.controller ? historyPage.controller.snapshots : null

            delegate: Frame {
                id: snapshotRow
                width: ListView.view.width
                required property string timestamp
                property string timestampText: timestamp
                property alias openAction: openButton
                property alias deleteAction: deleteButton

                RowLayout {
                    anchors.fill: parent
                    spacing: 12

                    Label {
                        Layout.fillWidth: true
                        text: snapshotRow.timestamp
                        font.family: "monospace"
                    }

                    Button {
                        id: openButton
                        text: "Open"
                        enabled: historyPage.controller && !historyPage.controller.analysisRunning
                        onClicked: historyPage.controller.openSnapshot(snapshotRow.timestamp)
                    }

                    Button {
                        id: deleteButton
                        text: "Delete"
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
