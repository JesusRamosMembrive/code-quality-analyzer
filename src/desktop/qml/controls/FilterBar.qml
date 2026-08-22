import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".." as Root

WorkbenchSurface {
    id: root

    property string severityValue: ""
    property string ruleValue: ""
    property string searchValue: ""

    signal severitySelected(string severity)
    signal ruleEdited(string ruleId)
    signal searchEdited(string searchText)

    implicitHeight: filterLayout.implicitHeight + (Root.CodeQaTheme.spaceLg * 2)

    function severityIndexForValue(severity) {
        const normalized = severity ? severity.toLowerCase() : ""
        if (normalized === "error") {
            return 1
        }
        if (normalized === "warning") {
            return 2
        }
        if (normalized === "information") {
            return 3
        }
        return 0
    }

    onSeverityValueChanged: severityFilter.currentIndex = severityIndexForValue(severityValue)
    onRuleValueChanged: {
        if (ruleFilter.text !== ruleValue) {
            ruleFilter.text = ruleValue
        }
    }
    onSearchValueChanged: {
        if (searchFilter.text !== searchValue) {
            searchFilter.text = searchValue
        }
    }

    Component.onCompleted: {
        severityFilter.currentIndex = severityIndexForValue(root.severityValue)
        ruleFilter.text = root.ruleValue
        searchFilter.text = root.searchValue
    }

    RowLayout {
        id: filterLayout
        anchors.fill: parent
        anchors.margins: Root.CodeQaTheme.spaceLg
        spacing: Root.CodeQaTheme.spaceMd

        ComboBox {
            id: severityFilter
            objectName: "findingSeverityFilter"
            Layout.preferredWidth: 160
            model: ["All severities", "Error", "Warning", "Information"]

            onActivated: root.severitySelected(currentIndex === 0 ? "" : currentText.toLowerCase())
        }

        TextField {
            id: ruleFilter
            objectName: "findingRuleFilter"
            Layout.fillWidth: true
            placeholderText: "Filter by rule ID"

            onTextEdited: root.ruleEdited(text)
        }

        TextField {
            id: searchFilter
            objectName: "findingSearchFilter"
            Layout.fillWidth: true
            placeholderText: "Search rule, symbol, or path"

            onTextEdited: root.searchEdited(text)
        }
    }
}
