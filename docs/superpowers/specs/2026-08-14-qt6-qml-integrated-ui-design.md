# Qt 6 / QML Integrated Desktop UI — Design

**Date:** 2026-08-14
**Status:** approved for specification review
**Target:** Windows first release

## 1. Goal

Add a local Windows desktop application to CodeQA using Qt 6 and QML. The
application complements, rather than replaces, the existing CLI. Both clients
use the same C++ application services, configuration format, quality-gate
semantics, and project-local snapshot storage.

The desktop UI makes large analysis reports navigable without editing their
contents. It can:

- run an analysis for a project;
- edit and explicitly save the project's `codeqa.json` configuration;
- list, open, and delete snapshots in `<project>/.codeqa/snapshots`;
- open a standalone CodeQA report JSON;
- present all reports through one read-only results workspace.

All user-visible QML text is English. The application makes no network calls
and collects no telemetry.

## 2. Scope and boundaries

### In scope

- Qt 6, Qt Quick, QML, and Qt Quick Controls 2.
- A Windows desktop executable built alongside the existing `codeqa` CLI.
- A shared C++ facade for GUI and CLI workflows.
- Import and validation of CodeQA report JSON with `schemaVersion` 1.
- Semantic configuration controls for every supported `codeqa.json` field:
  rules and their thresholds, header-language overrides, and quality-gate
  minimum severity.
- Local persistence of recent project/report paths and UI preferences, with a
  user-triggered clearing action.
- Progress phases, actionable errors, and quality-gate status.

### Explicitly out of scope

- Replacing or removing the CLI.
- A raw text editor for either report JSON or `codeqa.json`.
- Editing findings, adding annotations, inline suppressions, or source editing.
- Analysis cancellation. The current core has no safe cancellation boundary, so
  the first GUI release will not expose a misleading Cancel action.
- Non-Windows distribution, remote projects, cloud storage, telemetry, or
  collaboration features.
- Changing the current snapshot schema or moving snapshots out of the project.

## 3. Architecture

The core stays independent of Qt. Qt-specific types live only in the GUI
adapter layer.

```text
                        +------------------+
                        | codeqa CLI       |
                        +--------+---------+
                                 |
                                 v
 +------------------+   +------------------------+   +----------------------+
 | Qt/QML desktop   |-->| C++ application facade |-->| Existing core layers |
 | codeqa_desktop   |   | (no Qt dependencies)   |   | parse/model/metrics/ |
 +------------------+   +------------------------+   | rules/config/history |
           |                        |                +----------------------+
           |                        v
           |              +-------------------+
           +------------->| ReportDocument    |
                          +-------------------+
                               ^          ^
                               |          |
                  new analysis |          | report JSON / snapshot JSON
                               |          |
                        +------+----------+------+
                        | ReportDocument factory |
                        +------------------------+
```

### 3.1 Shared C++ application facade

Extract the workflow orchestration currently embedded in CLI handling into
Qt-free application services. The CLI becomes an argument/parser-and-output
adapter to those services; the GUI receives the same behavior through a Qt
bridge.

The facade has operations with explicit request/result types for:

- `analyzeProject`;
- `openReport`;
- `loadProjectConfig` and `saveProjectConfig`;
- `listSnapshots`, `openSnapshot`, and `deleteSnapshot`.

`analyzeProject` reports the real phases `Discovering files`, `Parsing source
files`, `Measuring functions`, `Applying rules`, and `Preparing report` through
a progress callback. It preserves the existing quality-gate behavior: a failed
gate returns a completed report plus a gate-failed status, not a technical
failure.

### 3.2 One read-only report model

`ReportDocument` is a Qt-free, normalized presentation DTO. It contains report
metadata, effective configuration, files, diagnostics, per-function metrics,
findings, optional snapshot-comparison data, and an optional quality-gate
outcome.

There are two inputs to the same model:

1. A freshly completed analysis maps its in-memory model/metrics/findings into
   `ReportDocument` directly.
2. A JSON report or snapshot is validated and decoded into `ReportDocument`.

The GUI never serializes an analysis to JSON and reparses it merely to display
it. JSON remains the portable import and snapshot format.

A fresh analysis carries its quality-gate outcome into `ReportDocument`. A
standalone or historical schema-version-1 report that does not persist that
outcome is displayed as **Not evaluated**, rather than inferring a result.

The JSON decoder accepts schema version 1 only and returns a precise validation
error for unreadable files, malformed JSON, missing required fields, or an
unsupported schema version. It never modifies the imported file.

### 3.3 Qt/QML bridge

The GUI adapter owns Qt concerns only: worker-thread execution, properties,
signals, `QAbstractListModel` implementations, file dialogs, and `QSettings`.
It exposes the facade's data to QML without allowing QML to call parsing,
filesystem, or JSON code directly.

The bridge owns the current `ReportDocument`. Replacing it after an analysis,
snapshot open, or report import updates the Results models atomically. An input
failure leaves the previous report visible.

## 4. Application layout and behavior

The shell has a persistent left sidebar with three English pages:

| Page | Responsibility |
| --- | --- |
| **Analyze** | Select a project, edit the semantic configuration, run analysis, show progress, and open the resulting report. |
| **History** | Select a project, list its snapshots newest first, open one, or delete one after confirmation. |
| **Results** | Show one current read-only `ReportDocument`, regardless of whether it came from a fresh analysis, a snapshot, or an imported report. |

Global actions include **Open report…**, **Clear recent projects**, and normal
Windows file/folder dialogs.

### 4.1 Analyze

The page starts with a project-folder picker and a configuration summary. It
loads `<project>/codeqa.json` if present; otherwise it presents CodeQA's
defaults. The configuration area uses semantic controls, not a JSON text area:

- enabled state plus warning/error thresholds per rule;
- header-language override rows;
- quality-gate minimum severity.

`Save` validates and writes `<project>/codeqa.json`. `Reset to defaults`
changes only the form until `Save` is chosen. Running an analysis does not write
configuration merely because the user opened or inspected it.

During analysis, the page disables conflicting project-changing actions and
shows an indeterminate progress indicator plus the current real phase. On a
completed analysis, the app switches to **Results**. If the quality gate fails,
the report remains available and the UI displays a non-technical warning state.

### 4.2 History

History uses exactly the same `<project>/.codeqa/snapshots` location and JSON
format as the CLI. Opening a snapshot creates no new snapshot. Deletion always
requires confirmation and affects only the selected snapshot file. Failed reads
or stale list entries are reported inline and do not clear the current report.

### 4.3 Results

Results is read-only and defaults to **Findings first**:

- a severity/rule filter rail and search by rule, symbol, or path;
- a central table sorted by severity, with rule, symbol, path, location,
  observed value, and limit;
- a detail panel with the selected finding's description, recommendation, and
  related metrics;
- file-oriented navigation and per-function metrics as secondary views or
  filters, not the default landing view;
- summary counts for files, diagnostics, errors, warnings, and quality-gate
  result (or **Not evaluated** when an imported report carries no persisted
  outcome).

The report has no editing, annotations, source changes, or suppression actions.
**Open report…** imports a standalone JSON report without writing to its source
project or creating snapshots.

## 5. Persistence, privacy, and writes

Project data remains project-local and compatible with the CLI:

- configuration: `<project>/codeqa.json`;
- snapshots: `<project>/.codeqa/snapshots`.

The app stores only its own recent project paths, recent report paths, and UI
preferences through `QSettings` in the current Windows user's local profile.
**Clear recent projects** removes this app-local data. No source code, report
content, or snapshot content is copied to an external service.

Writes require an explicit user action except that running analysis retains the
existing CLI behavior of saving its snapshot in the selected project. Saving
configuration and deleting a snapshot are explicit actions; both surface any
filesystem failure to the user.

## 6. Error handling

Errors are local, actionable, and non-destructive:

- invalid or inaccessible project path: explain the path problem before
  analysis begins;
- invalid `codeqa.json`: show the existing configuration diagnostic and do not
  overwrite the file;
- malformed/incompatible report JSON: identify the file and validation reason;
- inaccessible snapshots: keep the current document and annotate the affected
  history row;
- output/save failure: retain the in-memory result and explain what was not
  persisted;
- quality-gate failure: show Results and a distinct warning state, not an
  exception dialog.

## 7. Build and distribution

The existing CMake CLI target remains unchanged in behavior. Add a Windows
desktop executable (provisional target name `codeqa_desktop`) linked to
`codeqa_core` and Qt 6 Core, Gui, Qml, Quick, and Quick Controls 2. Define the
QML module with `qt_add_qml_module` and keep all QML resources inside the
application module.

The Windows distribution bundles the required Qt runtime, plugins, and QML
imports using `windeployqt`. It is a local desktop package; no web server or
browser is part of the product.

## 8. Verification strategy

- Existing CLI/unit/integration/corpus tests continue to pass unchanged.
- New pure-C++ tests cover facade operations, report-document creation, report
  JSON validation, config save validation, history ordering, and deletion
  boundaries.
- Qt tests cover adapter state transitions and signals, including successful
  analysis, quality-gate failure, invalid input, report import, and snapshot
  actions.
- QML smoke tests instantiate the shell and each page, verify the English
  labels, and bind a representative `ReportDocument`.
- Windows packaging validation starts the deployed application on a machine or
  clean environment with no developer Qt path required.

## 9. Acceptance criteria

- A Windows user can run an analysis, see real progress phases, and inspect the
  report in the Results page.
- A user can import a valid schema-version-1 report JSON and inspect it through
  the identical Results experience without any write to the report's project.
- A user can inspect and explicitly save semantic `codeqa.json` changes.
- The GUI reads, opens, and deletes CLI-compatible snapshots at the established
  project-local location.
- Findings are read-only, Results defaults to a sidebar-driven findings-first
  layout, and all visible UI strings are English.
- The GUI works fully offline and offers a way to clear app-local recent paths.
- The CLI and existing test suite remain operational, and the Windows deployed
  GUI launches outside the development environment.
