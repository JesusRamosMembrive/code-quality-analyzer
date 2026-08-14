import QtQuick
import QtTest
import CodeQA.Desktop

TestCase {
    name: "DesktopShell"

    function test_shell_has_the_three_primary_pages() {
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        verify(shell !== null)

        const analyzeButton = shell.findChild("analyzeNavigationButton")
        verify(analyzeButton !== null)
        compare(analyzeButton.objectName, "analyzeNavigationButton")
        compare(analyzeButton.text, "Analyze")

        const historyButton = shell.findChild("historyNavigationButton")
        verify(historyButton !== null)
        compare(historyButton.objectName, "historyNavigationButton")
        compare(historyButton.text, "History")

        const resultsButton = shell.findChild("resultsNavigationButton")
        verify(resultsButton !== null)
        compare(resultsButton.objectName, "resultsNavigationButton")
        compare(resultsButton.text, "Results")
    }
}
