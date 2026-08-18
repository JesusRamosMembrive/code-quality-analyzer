pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
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

            PulseButton {
                objectName: "historySelectProjectButton"
                text: "Select project…"
                enabled: historyPage.controller && !historyPage.controller.analysisRunning
                onClicked: projectFolderDialog.open()
            }

            PulseButton {
                objectName: "refreshSnapshotsButton"
                text: "Refresh"
                enabled: historyPage.hasProject && !historyPage.controller.analysisRunning
                onClicked: historyPage.controller.refreshSnapshots()
            }

            Label {
                Layout.fillWidth: true
                text: historyPage.hasProject
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
            color: "#ff6b6b"
            wrapMode: Text.Wrap
        }

        Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !snapshotList.count
            text: historyPage.hasProject
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
                readonly property string timestampText: timestamp
                property alias openAction: openButton
                property alias deleteAction: deleteButton

                background: Rectangle {
                    color: "transparent"

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: "#333844"
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    spacing: 12

                    Label {
                        Layout.fillWidth: true
                        text: snapshotRow.timestamp
                        font.family: "monospace"
                        textFormat: Text.PlainText
                    }

                    PulseButton {
                        id: openButton
                        text: "Open"
                        enabled: historyPage.controller && !historyPage.controller.analysisRunning
                        onClicked: historyPage.controller.openSnapshot(snapshotRow.timestamp)
                    }

                    PulseButton {
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

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                text: "Compare:"
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
            }

            ComboBox {
                id: toSnapshotCombo
                objectName: "toSnapshotCombo"
                Layout.preferredWidth: 200
                model: historyPage.controller ? historyPage.controller.snapshots : null
                textRole: "timestamp"
            }

            PulseButton {
                id: compareSnapshotsButton
                objectName: "compareSnapshotsButton"
                text: "Compare"
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
