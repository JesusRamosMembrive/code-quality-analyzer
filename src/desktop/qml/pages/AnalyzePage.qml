pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import ".." as Root
import "../controls"

ScrollView {
    id: root

    property var controller: null
    readonly property bool analysisRunning: controller ? controller.analysisRunning : false
    readonly property bool hasProject: controller && controller.currentProject.toString() !== ""
    readonly property bool canEdit: controller && controller.configurationAvailable && !analysisRunning
    readonly property bool hasInlineError: controller && controller.errorMessage !== ""

    function configurationStatusText() {
        if (!root.hasProject) {
            return "No project selected"
        }
        if (!root.controller) {
            return "No project selected"
        }
        if (root.analysisRunning) {
            return "Analysis running"
        }
        if (!root.controller.configurationAvailable) {
            return "Configuration unavailable"
        }
        if (root.controller.configurationDirty) {
            return "Unsaved changes"
        }
        return "Configuration ready"
    }

    function configurationStatusDetail() {
        if (!root.hasProject) {
            return "Select a local C or C++ project to configure and analyze."
        }
        if (!root.controller) {
            return "Select a local C or C++ project to configure and analyze."
        }
        if (root.analysisRunning) {
            return root.controller.analysisPhase !== ""
                ? root.controller.analysisPhase
                : "CodeQA is analyzing the current project."
        }
        if (!root.controller.configurationAvailable) {
            return "Configuration could not be loaded. The existing file will not be overwritten."
        }
        if (root.controller.configurationDirty) {
            return "Configuration has unsaved changes."
        }
        return "Configuration loaded."
    }

    function configurationStatusKind() {
        if (!root.hasProject) {
            return "neutral"
        }
        if (root.analysisRunning) {
            return "warning"
        }
        if (root.controller && !root.controller.configurationAvailable) {
            return "warning"
        }
        if (root.controller && root.controller.configurationDirty) {
            return "warning"
        }
        return "success"
    }

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
        width: Math.max(root.availableWidth, 900)
        spacing: Root.CodeQaTheme.spaceLg

        WorkbenchSurface {
            id: analyzePageSurface
            objectName: "analyzePageSurface"
            Layout.fillWidth: true
            elevated: true
            implicitHeight: analyzeHeaderContent.implicitHeight + Root.CodeQaTheme.spaceXl * 2

            ColumnLayout {
                id: analyzeHeaderContent
                anchors.fill: parent
                anchors.margins: Root.CodeQaTheme.spaceXl
                spacing: Root.CodeQaTheme.spaceMd

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Root.CodeQaTheme.spaceLg

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Root.CodeQaTheme.spaceXs

                        Label {
                            text: "Analyze project"
                            color: Root.CodeQaTheme.ink
                            font.pixelSize: 28
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.hasProject
                                ? root.controller.currentProject.toString()
                                : "Select a local C or C++ project to configure and analyze."
                            color: Root.CodeQaTheme.mutedInk
                            elide: Text.ElideMiddle
                            wrapMode: Text.Wrap
                        }
                    }

                    WorkbenchButton {
                        id: selectProjectButton
                        objectName: "selectProjectButton"
                        text: "Select project…"
                        variant: "secondary"
                        enabled: root.controller && !root.analysisRunning
                        onClicked: projectDialog.open()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Root.CodeQaTheme.spaceMd

                    StatusBadge {
                        text: root.configurationStatusText()
                        status: root.configurationStatusKind()
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.configurationStatusDetail()
                        color: Root.CodeQaTheme.mutedInk
                        wrapMode: Text.Wrap
                    }
                }
            }
        }

        WorkbenchSurface {
            id: analyzeInlineErrorSurface
            Layout.fillWidth: true
            visible: root.hasInlineError
            tintColor: Root.CodeQaTheme.errorSoft
            implicitHeight: analyzeInlineErrorLabel.implicitHeight + Root.CodeQaTheme.spaceLg * 2

            Label {
                id: analyzeInlineErrorLabel
                anchors.fill: parent
                anchors.margins: Root.CodeQaTheme.spaceLg
                text: root.controller ? root.controller.errorMessage : ""
                color: Root.CodeQaTheme.error
                wrapMode: Text.Wrap
            }
        }

        WorkbenchSurface {
            id: rulesSurface
            Layout.fillWidth: true
            enabled: root.canEdit
            opacity: enabled ? 1.0 : 0.74
            implicitHeight: rulesContent.implicitHeight + Root.CodeQaTheme.spaceLg * 2

            ColumnLayout {
                id: rulesContent
                anchors.fill: parent
                anchors.margins: Root.CodeQaTheme.spaceLg
                spacing: Root.CodeQaTheme.spaceMd

                Section {
                    title: "Rules"
                    Layout.fillWidth: true

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Root.CodeQaTheme.spaceSm

                        RowLayout {
                            Layout.fillWidth: true

                            Label {
                                Layout.preferredWidth: 360
                                text: "RULE"
                                font.pixelSize: 11
                                font.letterSpacing: 1
                                color: Root.CodeQaTheme.mutedInk
                            }

                            Label {
                                Layout.preferredWidth: 90
                                text: "ENABLED"
                                font.pixelSize: 11
                                font.letterSpacing: 1
                                color: Root.CodeQaTheme.mutedInk
                            }

                            Label {
                                Layout.preferredWidth: 140
                                text: "WARNING LIMIT"
                                font.pixelSize: 11
                                font.letterSpacing: 1
                                color: Root.CodeQaTheme.mutedInk
                            }

                            Label {
                                Layout.preferredWidth: 140
                                text: "HIGH LIMIT"
                                font.pixelSize: 11
                                font.letterSpacing: 1
                                color: Root.CodeQaTheme.mutedInk
                            }

                            Item {
                                Layout.fillWidth: true
                            }
                        }

                        ListView {
                            id: ruleThresholdTable
                            objectName: "ruleThresholdTable"
                            Layout.fillWidth: true
                            Layout.preferredHeight: contentHeight
                            interactive: false
                            spacing: Root.CodeQaTheme.spaceSm
                            model: root.controller ? root.controller.rules : null

                            delegate: WorkbenchSurface {
                                id: ruleCard
                                required property var model

                                width: ruleThresholdTable.width
                                height: 52
                                tintColor: Root.CodeQaTheme.surfaceSubtle

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: Root.CodeQaTheme.spaceMd
                                    spacing: Root.CodeQaTheme.spaceMd

                                    Label {
                                        Layout.preferredWidth: 360
                                        text: ruleCard.model.id === undefined ? "" : ruleCard.model.id
                                        color: Root.CodeQaTheme.ink
                                        elide: Text.ElideRight
                                    }

                                    CheckBox {
                                        Layout.preferredWidth: 90
                                        checked: ruleCard.model.enabled === undefined ? false : ruleCard.model.enabled
                                        onToggled: {
                                            if (root.controller && ruleCard.model.id !== undefined) {
                                                root.controller.setRuleEnabled(ruleCard.model.id, checked)
                                            }
                                        }
                                    }

                                    TextField {
                                        id: warningLimitField
                                        visible: ruleCard.model.id !== "security.unsafe-string-api"
                                            && ruleCard.model.id !== "memory.uninitialized-pointer"
                                            && ruleCard.model.id !== "memory.array-parameter-decay"
                                        Layout.preferredWidth: 140
                                        text: ruleCard.model.warningLimit === undefined
                                            ? ""
                                            : ruleCard.model.warningLimit.toString()
                                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                                        validator: IntValidator {}
                                        onEditingFinished: {
                                            if (acceptableInput && highLimitField.acceptableInput
                                                    && root.controller && ruleCard.model.id !== undefined) {
                                                root.controller.setRuleLimits(
                                                    ruleCard.model.id, Number(text), Number(highLimitField.text))
                                            }
                                        }
                                    }

                                    TextField {
                                        id: highLimitField
                                        visible: ruleCard.model.id !== "security.unsafe-string-api"
                                            && ruleCard.model.id !== "memory.uninitialized-pointer"
                                            && ruleCard.model.id !== "memory.array-parameter-decay"
                                        Layout.preferredWidth: 140
                                        text: ruleCard.model.highLimit === undefined
                                            ? ""
                                            : ruleCard.model.highLimit.toString()
                                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                                        validator: IntValidator {}
                                        onEditingFinished: {
                                            if (acceptableInput && warningLimitField.acceptableInput
                                                    && root.controller && ruleCard.model.id !== undefined) {
                                                root.controller.setRuleLimits(
                                                    ruleCard.model.id, Number(warningLimitField.text), Number(text))
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
                }
            }
        }

        WorkbenchSurface {
            id: headerOverridesSurface
            Layout.fillWidth: true
            enabled: root.canEdit
            opacity: enabled ? 1.0 : 0.74
            implicitHeight: headerOverridesContent.implicitHeight + Root.CodeQaTheme.spaceLg * 2

            ColumnLayout {
                id: headerOverridesContent
                anchors.fill: parent
                anchors.margins: Root.CodeQaTheme.spaceLg
                spacing: Root.CodeQaTheme.spaceMd

                Section {
                    title: "Header language overrides"
                    Layout.fillWidth: true

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Root.CodeQaTheme.spaceMd

                        ListView {
                            id: headerOverrideTable
                            objectName: "headerOverrideTable"
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.max(contentHeight, 1)
                            interactive: false
                            spacing: Root.CodeQaTheme.spaceSm
                            model: root.controller ? root.controller.headerOverrides : null

                            delegate: WorkbenchSurface {
                                id: headerCard
                                required property int index
                                required property var model

                                width: headerOverrideTable.width
                                height: 54
                                tintColor: Root.CodeQaTheme.surfaceSubtle

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: Root.CodeQaTheme.spaceMd
                                    spacing: Root.CodeQaTheme.spaceMd

                                    TextField {
                                        id: patternField
                                        Layout.fillWidth: true
                                        placeholderText: "Header pattern, for example include/**/*.h"
                                        text: headerCard.model.pattern === undefined ? "" : headerCard.model.pattern
                                        onEditingFinished: {
                                            if (root.controller) {
                                                root.controller.updateHeaderOverride(
                                                    headerCard.index, text, languageSelector.currentText)
                                            }
                                        }
                                    }

                                    ComboBox {
                                        id: languageSelector
                                        objectName: "headerLanguageSelector"
                                        Layout.preferredWidth: 120
                                        model: ["c", "cpp"]
                                        currentIndex: headerCard.model.language === "cpp" ? 1 : 0
                                        onActivated: {
                                            if (root.controller) {
                                                root.controller.updateHeaderOverride(
                                                    headerCard.index, patternField.text, currentText)
                                            }
                                        }
                                    }

                                    WorkbenchButton {
                                        text: "Remove"
                                        variant: "quiet"
                                        onClicked: root.controller.removeHeaderOverride(headerCard.index)
                                    }
                                }
                            }
                        }

                        WorkbenchSurface {
                            id: headerOverrideAddSurface
                            Layout.fillWidth: true
                            tintColor: Root.CodeQaTheme.surfaceSubtle
                            implicitHeight: headerOverrideAddContent.implicitHeight + Root.CodeQaTheme.spaceMd * 2

                            RowLayout {
                                id: headerOverrideAddContent
                                anchors.fill: parent
                                anchors.margins: Root.CodeQaTheme.spaceMd
                                spacing: Root.CodeQaTheme.spaceMd

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

                                WorkbenchButton {
                                    text: "Add override"
                                    variant: "secondary"
                                    onClicked: {
                                        root.controller.addHeaderOverride(
                                            newHeaderPattern.text, newHeaderLanguage.currentText)
                                        newHeaderPattern.clear()
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        WorkbenchSurface {
            id: exclusionsSurface
            Layout.fillWidth: true
            enabled: root.hasProject && !root.analysisRunning
            opacity: enabled ? 1.0 : 0.74
            implicitHeight: exclusionsContent.implicitHeight + Root.CodeQaTheme.spaceLg * 2

            ColumnLayout {
                id: exclusionsContent
                anchors.fill: parent
                anchors.margins: Root.CodeQaTheme.spaceLg
                spacing: Root.CodeQaTheme.spaceMd

                Section {
                    title: "Exclusions"
                    Layout.fillWidth: true

                    TextField {
                        id: excludePatternsField
                        objectName: "excludePatternsField"
                        Layout.fillWidth: true
                        placeholderText: "Comma-separated glob patterns to skip, for example json.hpp, third_party/**"
                        text: root.controller && root.controller.excludePatterns !== undefined
                            ? root.controller.excludePatterns
                            : ""
                        onEditingFinished: {
                            if (root.controller) {
                                root.controller.excludePatterns = text
                            }
                        }
                    }
                }
            }
        }

        WorkbenchSurface {
            id: qualityGateSurface
            Layout.fillWidth: true
            enabled: root.canEdit
            opacity: enabled ? 1.0 : 0.74
            implicitHeight: qualityGateContent.implicitHeight + Root.CodeQaTheme.spaceLg * 2

            ColumnLayout {
                id: qualityGateContent
                anchors.fill: parent
                anchors.margins: Root.CodeQaTheme.spaceLg
                spacing: Root.CodeQaTheme.spaceMd

                Section {
                    title: "Quality gate"
                    Layout.fillWidth: true

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Root.CodeQaTheme.spaceMd

                        Label {
                            text: "Minimum finding severity"
                            color: Root.CodeQaTheme.ink
                        }

                        ComboBox {
                            id: qualityGateSelector
                            objectName: "qualityGateSelector"
                            model: ["information", "warning", "error"]
                            currentIndex: {
                                if (!root.controller) {
                                    return 1
                                }
                                const matchedIndex = qualityGateSelector.find(root.controller.qualityGateSeverity)
                                return matchedIndex >= 0 ? matchedIndex : 1
                            }
                            onActivated: root.controller.setQualityGateSeverity(currentText)
                        }

                        Item {
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }

        WorkbenchSurface {
            id: configurationActionsSurface
            Layout.fillWidth: true
            tintColor: Root.CodeQaTheme.surfaceSubtle
            implicitHeight: configurationActionsContent.implicitHeight + Root.CodeQaTheme.spaceLg * 2

            RowLayout {
                id: configurationActionsContent
                anchors.fill: parent
                anchors.margins: Root.CodeQaTheme.spaceLg
                spacing: Root.CodeQaTheme.spaceMd

                WorkbenchButton {
                    id: resetConfigurationButton
                    objectName: "resetConfigurationButton"
                    text: "Reset"
                    variant: "secondary"
                    enabled: root.controller && root.hasProject && !root.analysisRunning
                    onClicked: root.controller.resetConfiguration()
                }

                WorkbenchButton {
                    id: saveConfigurationButton
                    objectName: "saveConfigurationButton"
                    text: "Save"
                    variant: "secondary"
                    enabled: root.canEdit
                    onClicked: root.controller.saveConfiguration()
                }

                Item {
                    Layout.fillWidth: true
                }

                WorkbenchButton {
                    id: startAnalysisButton
                    objectName: "startAnalysisButton"
                    text: "Analyze project"
                    variant: "primary"
                    enabled: root.canEdit
                    onClicked: root.controller.startAnalysis()
                }
            }
        }

        Item {
            Layout.preferredHeight: Root.CodeQaTheme.spaceSm
        }
    }
}
