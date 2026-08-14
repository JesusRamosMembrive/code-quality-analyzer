import QtQuick
import QtTest
import CodeQA.Desktop

TestCase {
    name: "DesktopShell"

    function test_shell_has_the_three_primary_pages() {
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        verify(shell !== null)
        compare(shell.findChild("analyzeNavigationButton").text, "Analyze")
        compare(shell.findChild("historyNavigationButton").text, "History")
        compare(shell.findChild("resultsNavigationButton").text, "Results")
    }
}
