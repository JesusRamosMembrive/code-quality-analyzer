# Results Tabs Design

## Goal

Split the current results view into four focused tabs so users can understand the analysis at a glance and inspect individual issues with more space:

1. `Overview`
2. `Findings`
3. `Files and metrics`
4. `Suppressed`

`Overview` is the initial tab whenever the `Results` page is opened.

## Context

`src/desktop/qml/pages/ResultsPage.qml` already owns a `TabBar` and `StackLayout`, but its first stack item currently combines the overview summary cards, filters, finding list, and finding details. The controller already exposes the data required by the four views, so this change is a QML composition refactor with no backend or report-model changes.

## Design

### Results shell

`ResultsPage.qml` remains the coordinator. It owns:

- the four-tab `TabBar`;
- the `StackLayout` synchronized with the tab index;
- the shared `controller` property passed to each tab component;
- the existing Markdown export dialog and action wiring.

The `TabBar` labels are exactly `Overview`, `Findings`, `Files and metrics`, and `Suppressed`, in that order. Its `currentIndex` is `0` so `Overview` is selected on initial construction.

### Overview

Create a focused QML component for the report summary. It contains:

- the page heading and Markdown export action;
- `Quality gate` status;
- total matching findings and analyzed-file count;
- matching errors;
- matching warnings;
- a compact context row showing analyzed files and suppressed findings, using values already exposed by the controller.

The overview consumes controller values only. It does not own finding filters or finding selection.

### Findings

Create a focused QML component for issue inspection. It contains:

- the `Findings` heading and Markdown export action;
- the existing severity, rule, and search filters;
- a large two-column work area;
- the finding list with severity, rule, path/location, and symbol columns;
- the selected-finding detail panel;
- the existing empty state and inline error state.

Finding selection and filter synchronization remain local to this component. A current-report change clears the selected issue. Changing any finding filter clears the selection before applying the new controller filter.

The summary cards are removed from this tab so the list/detail area receives the available vertical space and the issue content matches the supplied reference layout.

### Files and metrics

Move the existing file metrics `ListView` into its own focused component without changing its model roles, row content, or empty state.

### Suppressed

Move the existing suppressed-findings `ListView` into its own focused component without changing its model roles, row content, or empty state. The tab keeps its current empty-state text and model behavior.

## Data flow and compatibility

- No C++ controller APIs are added or changed.
- Each tab receives the same `controller` object.
- Existing filter properties remain `findingSeverityFilter`, `findingRuleFilter`, and `findingSearchText`.
- Existing model properties remain `findings`, `files`, and `suppressedFindings`.
- Opening a report continues to select the `Results` page; the Results page then starts on `Overview`.
- The export action continues to call `controller.exportMarkdown()`.

## Testing and acceptance criteria

Add QML smoke coverage for:

- the four tab labels and their order;
- `Overview` being selected at initial construction;
- overview summary controls being present;
- selecting `Findings` exposing the finding table, filters, and detail panel;
- selecting `Files and metrics` exposing the file metrics list;
- selecting `Suppressed` preserving the existing list and empty-state behavior;
- selecting a finding and changing a filter still clearing the detail panel.

Run the targeted QML smoke test and the complete CTest suite. The implementation is accepted when all existing behavior remains covered and the complete suite passes.

## Scope boundaries

- Do not change controller APIs, report serialization, or analysis semantics.
- Do not add new filtering, sorting, charts, or navigation outside the four requested tabs.
- Keep the existing Warm Workbench visual language and reusable controls.
