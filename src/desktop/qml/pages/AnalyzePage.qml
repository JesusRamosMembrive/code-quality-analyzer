pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "../controls"

ScrollView {
    id: root

    property var controller: null
    readonly property bool analysisRunning: controller ? controller.analysisRunning : false
    readonly property bool hasProject: controller && controller.currentProject.toString() !== ""
    readonly property bool canEdit: controller && controller.configurationAvailable && !analysisRunning

    clip: true

    FolderDialog {
        id: projectDialog
        title: "Select project folder"

        onAccepted: {
            if (root.controller) {
                root.controller.selectProject(selectedFolder)
            }
        }
    }

    ColumnLayout {
        width: Math.max(root.availableWidth, 860)
        spacing: 18

        RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    text: "Analyze project"
                    font.pixelSize: 26
                    font.bold: true
                }

                Label {
                    Layout.fillWidth: true
                    text: root.hasProject
                        ? root.controller.currentProject.toString()
                        : "Select a local C or C++ project to configure and analyze."
                    color: "#9aa1b0"
                    elide: Text.ElideMiddle
                }
            }

            PulseButton {
                id: selectProjectButton
                objectName: "selectProjectButton"
                text: "Select project…"
                enabled: root.controller && !root.analysisRunning
                onClicked: projectDialog.open()
            }
        }

        Label {
            Layout.fillWidth: true
            text: !root.hasProject
                ? "No project selected"
                : !root.controller.configurationAvailable
                    ? "Configuration could not be loaded. The existing file will not be overwritten."
                    : root.controller.configurationDirty
                        ? "Configuration has unsaved changes."
                        : "Configuration loaded."
            color: root.controller && !root.controller.configurationAvailable && root.hasProject
                ? "#ffb454"
                : "#9aa1b0"
            wrapMode: Text.Wrap
        }

        Label {
            Layout.fillWidth: true
            visible: root.controller && root.controller.errorMessage !== ""
            text: root.controller ? root.controller.errorMessage : ""
            color: "#ff6b6b"
            wrapMode: Text.Wrap
        }

        Section {
            title: "Rules"
            Layout.fillWidth: true
            enabled: root.canEdit

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.preferredWidth: 360
                        text: "RULE"
                        font.pixelSize: 11
                        font.letterSpacing: 1
                        color: "#9aa1b0"
                    }

                    Label {
                        Layout.preferredWidth: 90
                        text: "ENABLED"
                        font.pixelSize: 11
                        font.letterSpacing: 1
                        color: "#9aa1b0"
                    }

                    Label {
                        Layout.preferredWidth: 140
                        text: "WARNING LIMIT"
                        font.pixelSize: 11
                        font.letterSpacing: 1
                        color: "#9aa1b0"
                    }

                    Label {
                        Layout.preferredWidth: 140
                        text: "HIGH LIMIT"
                        font.pixelSize: 11
                        font.letterSpacing: 1
                        color: "#9aa1b0"
                    }

                    Item {
                        Layout.fillWidth: true
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#333844"
                }

                ListView {
                    id: ruleThresholdTable
                    objectName: "ruleThresholdTable"
                    Layout.fillWidth: true
                    Layout.preferredHeight: contentHeight
                    interactive: false
                    spacing: 4
                    model: root.controller ? root.controller.rules : null

                    delegate: RowLayout {
                        id: ruleRow

                        required property var model

                        width: ruleThresholdTable.width
                        height: 42

                        Label {
                            Layout.preferredWidth: 360
                            text: ruleRow.model.id === undefined ? "" : ruleRow.model.id
                            elide: Text.ElideRight
                        }

                        CheckBox {
                            Layout.preferredWidth: 90
                            checked: ruleRow.model.enabled === undefined ? false : ruleRow.model.enabled
                            onToggled: {
                                if (root.controller && ruleRow.model.id !== undefined) {
                                    root.controller.setRuleEnabled(ruleRow.model.id, checked)
                                }
                            }
                        }

                        TextField {
                            id: warningLimitField
                            Layout.preferredWidth: 140
                            text: ruleRow.model.warningLimit === undefined
                                ? ""
                                : ruleRow.model.warningLimit.toString()
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            validator: IntValidator {}
                            onEditingFinished: {
                                if (acceptableInput && highLimitField.acceptableInput
                                        && root.controller && ruleRow.model.id !== undefined) {
                                    root.controller.setRuleLimits(
                                        ruleRow.model.id, Number(text), Number(highLimitField.text))
                                }
                            }
                        }

                        TextField {
                            id: highLimitField
                            Layout.preferredWidth: 140
                            text: ruleRow.model.highLimit === undefined
                                ? ""
                                : ruleRow.model.highLimit.toString()
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            validator: IntValidator {}
                            onEditingFinished: {
                                if (acceptableInput && warningLimitField.acceptableInput
                                        && root.controller && ruleRow.model.id !== undefined) {
                                    root.controller.setRuleLimits(
                                        ruleRow.model.id, Number(warningLimitField.text), Number(text))
                                }
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }

        Section {
            title: "Header language overrides"
            Layout.fillWidth: true
            enabled: root.canEdit

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                ListView {
                    id: headerOverrideTable
                    objectName: "headerOverrideTable"
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(contentHeight, 1)
                    interactive: false
                    spacing: 6
                    model: root.controller ? root.controller.headerOverrides : null

                    delegate: RowLayout {
                        id: headerRow

                        required property int index
                        required property var model

                        width: headerOverrideTable.width
                        height: 42

                        TextField {
                            id: patternField
                            Layout.fillWidth: true
                            placeholderText: "Header pattern, for example include/**/*.h"
                            text: headerRow.model.pattern === undefined ? "" : headerRow.model.pattern
                            onEditingFinished: {
                                if (root.controller) {
                                    root.controller.updateHeaderOverride(
                                        headerRow.index, text, languageSelector.currentText)
                                }
                            }
                        }

                        ComboBox {
                            id: languageSelector
                            objectName: "headerLanguageSelector"
                            Layout.preferredWidth: 120
                            model: ["c", "cpp"]
                            currentIndex: headerRow.model.language === "cpp" ? 1 : 0
                            onActivated: {
                                if (root.controller) {
                                    root.controller.updateHeaderOverride(
                                        headerRow.index, patternField.text, currentText)
                                }
                            }
                        }

                        PulseButton {
                            text: "Remove"
                            onClicked: root.controller.removeHeaderOverride(headerRow.index)
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    TextField {
                        id: newHeaderPattern
                        Layout.fillWidth: true
                        placeholderText: "Header pattern, for example include/**/*.h"
                    }

                    ComboBox {
                        id: newHeaderLanguage
                        Layout.preferredWidth: 120
                        model: ["c", "cpp"]
                    }

                    PulseButton {
                        text: "Add override"
                        onClicked: {
                            root.controller.addHeaderOverride(
                                newHeaderPattern.text, newHeaderLanguage.currentText)
                            newHeaderPattern.clear()
                        }
                    }
                }
            }
        }

        Section {
            title: "Exclusions"
            Layout.fillWidth: true
            enabled: root.hasProject && !root.analysisRunning

            RowLayout {
                Layout.fillWidth: true

                TextField {
                    id: excludePatternsField
                    objectName: "excludePatternsField"
                    Layout.fillWidth: true
                    placeholderText: "Comma-separated glob patterns to skip, for example json.hpp, third_party/**"
                    text: root.controller ? root.controller.excludePatterns : ""
                    onEditingFinished: {
                        if (root.controller) {
                            root.controller.excludePatterns = text
                        }
                    }
                }
            }
        }

        Section {
            title: "Quality gate"
            Layout.fillWidth: true
            enabled: root.canEdit

            RowLayout {
                Layout.fillWidth: true

                Label {
                    text: "Minimum finding severity"
                }

                ComboBox {
                    id: qualityGateSelector
                    objectName: "qualityGateSelector"
                    model: ["information", "warning", "error"]
                    currentIndex: root.controller
                        ? Math.max(0, find(root.controller.qualityGateSeverity))
                        : 1
                    onActivated: root.controller.setQualityGateSeverity(currentText)
                }

                Item {
                    Layout.fillWidth: true
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            PulseButton {
                id: resetConfigurationButton
                objectName: "resetConfigurationButton"
                text: "Reset"
                enabled: root.controller && root.hasProject && !root.analysisRunning
                onClicked: root.controller.resetConfiguration()
            }

            PulseButton {
                id: saveConfigurationButton
                objectName: "saveConfigurationButton"
                text: "Save"
                enabled: root.canEdit
                onClicked: root.controller.saveConfiguration()
            }

            Item {
                Layout.fillWidth: true
            }

            BusyIndicator {
                id: analysisBusyIndicator
                objectName: "analysisBusyIndicator"
                running: root.analysisRunning
                visible: running
            }

            Label {
                id: analysisPhaseLabel
                objectName: "analysisPhaseLabel"
                text: root.controller ? root.controller.analysisPhase : ""
                visible: root.analysisRunning
            }

            PulseButton {
                id: startAnalysisButton
                objectName: "startAnalysisButton"
                text: "Analyze project"
                enabled: root.canEdit
                onClicked: root.controller.startAnalysis()
            }
        }

        Item {
            Layout.preferredHeight: 12
        }
    }
}
