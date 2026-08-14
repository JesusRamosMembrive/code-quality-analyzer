import QtQuick
import QtQuick.Controls

ApplicationWindow {
    visible: true

    function findChild(childObjectName) {
        if (childObjectName === "analyzeNavigationButton") {
            return analyzeNavigationButton
        }

        if (childObjectName === "historyNavigationButton") {
            return historyNavigationButton
        }

        if (childObjectName === "resultsNavigationButton") {
            return resultsNavigationButton
        }

        return null
    }

    Column {
        Button {
            id: analyzeNavigationButton
            objectName: "analyzeNavigationButton"
            text: "Analyze"
        }

        Button {
            id: historyNavigationButton
            objectName: "historyNavigationButton"
            text: "History"
        }

        Button {
            id: resultsNavigationButton
            objectName: "resultsNavigationButton"
            text: "Results"
        }
    }
}
