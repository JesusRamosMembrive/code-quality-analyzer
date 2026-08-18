# GUI: Arbitrary Snapshot Comparison — Design

**Date:** 2026-08-18
**Status:** approved for specification review
**Track:** post-MVP, roadmap v2 track #2 — see `docs/02-roadmap-v2.md`.

## 1. Goal

Let the desktop GUI compare any two saved snapshots for the current project and
show how many findings are new, still-existing, and resolved between them —
the same `newCount`/`existingCount`/`resolvedCount` shape already computed
automatically for the quality gate (current run vs. the single immediately-
previous snapshot), now available on demand for any pair a user picks from
the History page.

## 2. Scope and boundaries

### In scope

- One new `ApplicationFacade::compareSnapshots(projectRoot, fromTimestamp,
  toTimestamp)` method. It is pure wiring: it loads two already-saved
  snapshots via the existing `LoadSnapshot`, extracts a `FindingKeySet` from
  each via the existing `codeqa::history::extractFindingKeys(const
  nlohmann::json&)` overload, and calls the existing
  `codeqa::history::compareSnapshots(current, previous)` — no changes to
  `codeqa::history` at all.
- `AppController::compareSnapshots(fromTimestamp, toTimestamp)`
  (`Q_INVOKABLE`), exposing the three counts as `Q_PROPERTY`s and reusing
  the existing `errorMessage` property for failures.
- `HistoryPage.qml`: two snapshot pickers and a "Compare" action, showing
  the three counts (or the error) inline.

### Explicitly out of scope

- **Which specific findings** are new/existing/resolved. `SnapshotComparison`
  only ever carries counts (`include/codeqa/history/SnapshotComparison.h`)
  — this track does not extend it or add a sibling type that carries
  identities. Scoped down explicitly during brainstorming: aggregate counts
  only, matching exactly what the quality gate already computes today.
  Per-finding detail is a candidate for a later iteration if the counts
  alone turn out not to be enough.
- Persisting a comparison result anywhere (no new snapshot field, no report
  schema change).
- Any change to `codeqa::history::compareSnapshots`/`extractFindingKeys`,
  the quality gate, or the automatic current-vs-previous comparison already
  wired into `ApplicationFacade::analyzeProject`.
- CLI surface. `codeqa snapshot list|show` are untouched; this is a
  desktop-GUI-only affordance, matching how `codeqa-disable-next-line`
  visibility (track #1) was also GUI-only.

## 3. Architecture

```text
HistoryPage.qml (two ComboBoxes + "Compare" button)
        |
        v
AppController::compareSnapshots(from, to)          [Q_INVOKABLE]
        |
        v
ApplicationFacade::compareSnapshots(root, from, to)  (new)
        |
        +--> LoadSnapshot{}.execute(root, from)  --> nlohmann::json   (existing)
        +--> LoadSnapshot{}.execute(root, to)    --> nlohmann::json   (existing)
        |
        v
codeqa::history::extractFindingKeys(json) x2                          (existing)
        |
        v
codeqa::history::compareSnapshots(toKeys, fromKeys) -> SnapshotComparison
                                                                        (existing)
```

`ApplicationFacade::compareSnapshots` is the only new function in
`codeqa::application`/`codeqa::history`. It follows the exact error-handling
shape already used inside `ApplicationFacade::analyzeProject` for the
automatic previous-snapshot comparison (`src/application/ApplicationFacade.cpp`,
current lines ~147-179): an unreadable snapshot and a malformed one are
handled as two distinct, named failure cases rather than collapsed into one.

## 4. Facade contract

```cpp
// include/codeqa/application/ApplicationFacade.h
struct SnapshotComparisonResult
{
    std::optional<codeqa::history::SnapshotComparison> comparison;
    ApplicationError error;
};

class ApplicationFacade
{
public:
    // ...existing methods unchanged...
    [[nodiscard]] SnapshotComparisonResult compareSnapshots(
        const std::filesystem::path& projectRoot,
        const std::string& fromTimestamp,
        const std::string& toTimestamp) const;
};
```

```cpp
// src/application/ApplicationFacade.cpp
SnapshotComparisonResult ApplicationFacade::compareSnapshots(
    const std::filesystem::path& projectRoot,
    const std::string& fromTimestamp,
    const std::string& toTimestamp) const
{
    SnapshotComparisonResult result;

    const std::optional<nlohmann::json> fromReport = LoadSnapshot{}.execute(projectRoot, fromTimestamp);
    if (!fromReport.has_value())
    {
        result.error.message = "Error [APPLICATION003]: could not load snapshot '" + fromTimestamp + "'.";
        return result;
    }

    const std::optional<nlohmann::json> toReport = LoadSnapshot{}.execute(projectRoot, toTimestamp);
    if (!toReport.has_value())
    {
        result.error.message = "Error [APPLICATION003]: could not load snapshot '" + toTimestamp + "'.";
        return result;
    }

    try
    {
        const codeqa::history::FindingKeySet fromKeys = codeqa::history::extractFindingKeys(*fromReport);
        const codeqa::history::FindingKeySet toKeys = codeqa::history::extractFindingKeys(*toReport);
        result.comparison = codeqa::history::compareSnapshots(toKeys, fromKeys);
    }
    catch (const nlohmann::json::exception&)
    {
        result.error.message = "Error [APPLICATION004]: a snapshot has an unexpected shape.";
    }

    return result;
}
```

`fromTimestamp`/`toTimestamp` are caller-ordered: `to` is treated as
"current" and `from` as "previous" in `compareSnapshots`'s existing
current/previous vocabulary, so "new" means "present in `to`, absent in
`from`" and "resolved" means the reverse — the natural reading of
"comparing from an older snapshot to a newer one." The GUI does not enforce
which picker is chronologically older; a user comparing "backwards" simply
sees new/resolved swapped from what they might expect, which is a correct
result for the pair they actually chose, not a bug to guard against.

New error codes: `APPLICATION003` (a snapshot file couldn't be read — reuses
the same condition `LoadSnapshot` already reports, just surfaced through a
new caller) and `APPLICATION004` (a snapshot file parsed as JSON but not the
expected report shape). Both are read-only diagnostics; neither can be
triggered by anything other than a corrupted/missing `.codeqa/snapshots/*.json`
file, since the timestamps offered to the user always come from
`ListSnapshots`, which only lists files that already exist on disk.

## 5. Desktop controller

`AppController` (`src/desktop/AppController.h`/`.cpp`):

```cpp
Q_PROPERTY(int snapshotComparisonNewCount READ snapshotComparisonNewCount NOTIFY snapshotComparisonChanged)
Q_PROPERTY(int snapshotComparisonExistingCount READ snapshotComparisonExistingCount NOTIFY snapshotComparisonChanged)
Q_PROPERTY(int snapshotComparisonResolvedCount READ snapshotComparisonResolvedCount NOTIFY snapshotComparisonChanged)
Q_PROPERTY(bool snapshotComparisonAvailable READ snapshotComparisonAvailable NOTIFY snapshotComparisonChanged)
...
Q_INVOKABLE void compareSnapshots(const QString& fromTimestamp, const QString& toTimestamp);
...
signals:
    void snapshotComparisonChanged();
```

`compareSnapshots` calls `facade_.compareSnapshots(currentProjectPath_,
fromTimestamp.toStdString(), toTimestamp.toStdString())`. On success, it
stores the three counts and sets `snapshotComparisonAvailable = true`, then
emits `snapshotComparisonChanged()`. On failure, it calls the existing
`setErrorMessage(...)` with the facade's error text and sets
`snapshotComparisonAvailable = false` — reusing the error-surfacing
convention every other `AppController` action already uses, not inventing a
second error channel. Selecting a different project (`setCurrentProject`)
resets `snapshotComparisonAvailable` to `false`, the same way it already
resets other per-project transient state.

## 6. QML

`HistoryPage.qml` gains, below the existing `snapshotList` `ListView`:

- Two `ComboBox`es (`fromSnapshotCombo`/`toSnapshotCombo`, both
  `objectName`d for tests), each `model: historyPage.controller.snapshots`
  with `textRole: "timestamp"` — the same list the page's own `ListView`
  already binds, no new model.
- A "Compare" `PulseButton`, `enabled` only when both combos have a
  selection and their timestamps differ, calling
  `historyPage.controller.compareSnapshots(fromSnapshotCombo.currentText,
  toSnapshotCombo.currentText)`.
- A result `Label` (`objectName: "snapshotComparisonResult"`) showing `"N
  new, M existing, P resolved"` when
  `controller.snapshotComparisonAvailable` is true, otherwise hidden — no
  separate empty-state copy needed here (unlike track #1's suppressed-
  findings tab), since this is an on-demand action with a button, not an
  always-visible tab.

Read-only: neither combo nor the result label offers any action beyond
picking two timestamps and reading three numbers.

## 7. Error handling / edge cases

- Fewer than 2 snapshots exist for the current project: both combos are
  simply short (0 or 1 entries) and the Compare button stays disabled
  (can't pick two distinct timestamps) — no special-cased message, the
  disabled button is self-explanatory alongside the page's existing "No
  snapshots found" label.
- A snapshot is deleted (via the page's existing Delete action) after being
  selected in a combo but before Compare is pressed: `LoadSnapshot` returns
  `nullopt` for the now-missing file, surfaced as the ordinary
  `APPLICATION003` error through `errorMessage`, same as any other
  unreadable-snapshot path in this codebase.
- Comparing a snapshot to itself: prevented at the UI layer (Compare stays
  disabled when both combos hold the same timestamp) rather than in the
  facade — the facade has no reason to forbid it (it would correctly report
  0/N/0), but there's no useful reason to let a user reach that state either.

## 8. Testing and verification strategy

- `tests/unit/ApplicationFacadeTest.cpp`: a real two-snapshot fixture (write
  two report JSONs to `.codeqa/snapshots/` directly, matching this file's
  existing fixture style) asserting `compareSnapshots` returns correct
  counts for a real added/removed/unchanged finding mix; one test for each
  new error path (missing timestamp -> `APPLICATION003`; malformed snapshot
  JSON -> `APPLICATION004`).
- `tests/desktop/AppControllerTest.cpp`: one test driving
  `controller.compareSnapshots(from, to)` against two real snapshots written
  to a temp project, asserting the three `Q_PROPERTY` values and
  `snapshotComparisonAvailable`; one test asserting a failure sets
  `errorMessage` and leaves `snapshotComparisonAvailable` false.
- `tests/desktop/qml/tst_DesktopShell.qml`: extend the mock
  `fixtureController` with `snapshotComparisonNewCount`/`ExistingCount`/
  `ResolvedCount`/`Available` and a `compareSnapshots(from, to)` recorder
  function; one test selecting both combos and pressing Compare asserts the
  result label shows the mocked counts.

## 9. Acceptance criteria

- Given two real snapshots of the same project with at least one added and
  one removed finding between them, selecting them in the History page and
  pressing Compare shows the correct new/existing/resolved counts.
- Comparing a snapshot to itself is not reachable through the UI (Compare
  stays disabled).
- A deleted/corrupted snapshot produces the existing `errorMessage` surface,
  not a crash or a silent no-op.
- No change to `codeqa::history`'s public API, the quality gate's own
  automatic comparison, or any CLI command's behavior or output.
- Existing findings/duplication/quality-gate/history/suppressed-findings
  tests all still pass unmodified.
