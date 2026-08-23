import QtQuick

import ".." as Root
import "../controls"

Item {
    id: root

    property var controller: null

    objectName: "analysisProgressPage"

    AnalysisProgressPanel {
        id: analysisProgressPanel
        anchors.centerIn: parent
        width: Math.max(0, Math.min(parent.width - (Root.CodeQaTheme.spaceXl * 2), 1120))
        running: true
        phaseText: root.controller ? root.controller.analysisPhase : ""
        projectPath: root.controller ? root.controller.currentProject.toString() : ""
    }
}
