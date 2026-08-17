# GUI: Suppressed Findings Visibility — Design

**Date:** 2026-08-17
**Status:** approved for specification review
**Track:** post-MVP, roadmap v2 track #1 — see `docs/02-roadmap-v2.md`.

## 1. Goal

Surface, in the desktop GUI, which findings were suppressed via an inline
`// codeqa-disable-next-line <rule-id>` comment during the current analysis
run. Today this information is computed and then silently discarded —
`ApplyRules::execute` filters a suppressed `Finding` out before it ever
reaches any model, report, or GUI. This track makes that already-computed
data visible, read-only, for the run that just completed.

The roadmap originally described this track as "ver/gestionar" (view/manage)
supresiones. Scoping decisions made during brainstorming narrowed it:

- **View only.** No GUI affordance to create or remove a suppression comment
  in the user's source files. That remains a manual source edit. Adding
  write access to a user's source tree from the GUI is a materially bigger,
  riskier feature, deferred indefinitely (not just to a later track — no
  need for one is established by anything in this spec).
- **Live-computed only, not persisted.** Suppressed findings are attached to
  the in-memory `ReportDocument` built from the run that just finished. They
  are **not** added to the JSON/text report schema and **not** saved into
  `.codeqa/snapshots/*.json`. Opening a past snapshot in the History page
  will not show that run's suppressions. This mirrors how the JSON/text
  report schema stays untouched — no `schemaVersion` bump, no new
  `CONFIG0xx`/report codes, nothing for `codeqa::report` to learn.

## 2. Scope and boundaries

### In scope

- `codeqa::rules::FileFindingsModel` gains a second bucket
  (`suppressedFindings`) alongside the existing `findings`, populated in the
  same loop in `ApplyRules::execute` that already computes suppression today
  — no new suppression-detection logic, just not discarding the result.
- `codeqa::application::ReportFile` gains the same field, populated only on
  the live-analysis path (`makeReportDocument`), left empty on the
  parsed-from-JSON path (`parseReportDocument`) — the existing optional/
  backward-compatible pattern already used for `duplicates`, except here
  there is no JSON key to look for at all, so the field is simply never set.
- Desktop: a new `SuppressedFindingListModel` (mirrors `FindingListModel`'s
  row shape — file/finding pointers, path), exposed on `AppController` as a
  third `CONSTANT` list-model property, populated in the same
  begin/apply/end block that already updates `findings_`/`files_` after
  every analysis/snapshot load.
- `ResultsPage.qml` gains a third tab ("Suprimidos") next to the existing
  two (`resultsTabs`/`StackLayout` at `src/desktop/qml/pages/ResultsPage.qml`
  lines ~129-143), listing rule id, file:line, and symbol name per
  suppressed finding. Read-only — no detail panel interaction is required
  beyond what a plain list already gives for free.

### Explicitly out of scope

- Creating/removing/editing suppression comments from the GUI (source-file
  mutation) — see §1.
- Persisting suppressed findings to the JSON/text report or to snapshots —
  see §1. A future track could revisit this if History-page visibility for
  past runs turns out to matter; nothing here blocks that.
- CLI/text-report surfacing. `codeqa::report::renderText`/`toReportJson` are
  not touched.
- Any change to how suppression itself is detected (`isSuppressed`,
  `suppressedRuleIdOnLine` in `ApplyRules.cpp`) — this track only stops
  throwing the result away.

## 3. Architecture

```text
ApplyRules::execute (src/application/ApplyRules.cpp)
  for each Finding a rule produces:
    isSuppressed? -> push to FileFindingsModel::suppressedFindings (NEW)
    else          -> push to FileFindingsModel::findings (unchanged)
                        |
                        v
        codeqa::rules::ProjectFindingsModel
     (already flows unchanged through RunAnalysisResult::findings
      and ApplicationFacade -- no plumbing changes needed there)
                        |
                        v
     ReportDocumentFactory::makeReportDocument (live path only)
       ReportFile::suppressedFindings = analysis.findings.files[i].suppressedFindings
                        |
                        v
          AppController -- SuppressedFindingListModel (NEW)
                        |
                        v
              ResultsPage.qml -- third tab "Suprimidos"
```

`ReportDocumentFactory::parseFile` (the snapshot/JSON-loading path used by
the History page) is not touched — `ReportFile::suppressedFindings` simply
stays default-constructed (empty) when a document is reconstructed from a
saved report, which is exactly the desired behavior from §1.

## 4. Model extension

```cpp
// include/codeqa/rules/RuleModels.h
struct FileFindingsModel
{
    std::filesystem::path path;
    std::vector<Finding> findings;
    std::vector<Finding> suppressedFindings;  // NEW
};
```

```cpp
// include/codeqa/application/ReportDocument.h
struct ReportFile
{
    // ...existing fields unchanged...
    std::vector<codeqa::rules::Finding> suppressedFindings;  // NEW
};
```

No new types. A suppressed finding is exactly the `Finding` the rule already
produced — same fields, same severity/message/location — just routed to a
different vector instead of being dropped.

## 5. `ApplyRules::execute` change

Today (`src/application/ApplyRules.cpp`, current lines ~257-263):

```cpp
for (codeqa::rules::Finding& finding : rule->evaluate(context))
{
    if (!isSuppressed(finding, file.comments, sourceLines))
    {
        fileFindings.findings.push_back(std::move(finding));
    }
}
```

Becomes:

```cpp
for (codeqa::rules::Finding& finding : rule->evaluate(context))
{
    if (isSuppressed(finding, file.comments, sourceLines))
    {
        fileFindings.suppressedFindings.push_back(std::move(finding));
    }
    else
    {
        fileFindings.findings.push_back(std::move(finding));
    }
}
```

No other line in the function changes. `isSuppressed`/`suppressedRuleIdOnLine`
are untouched.

## 6. Desktop model and controller

`SuppressedFindingListModel` (`src/desktop/ReportModels.h`/`.cpp`): copy of
`FindingListModel`'s shape (`Row{file, finding, path}`, `rebuild`/
`beginDocumentReset`/`applyDocument`/`endDocumentReset`, same dangling-pointer
caveat already documented on `FindingListModel::Row`), reading
`ReportFile::suppressedFindings` instead of `ReportFile::findings`. No
severity/rule/search filter properties — the existing `FindingListModel`
filters exist because that list can get long and needs triage; suppressed
findings are already a curated, typically small list the user chose to
exclude, so a first cut ships without filtering. `errorCount`/`warningCount`
style aggregate properties are also not needed — this is a raw list, not a
gate-relevant summary.

`AppController`: new `Q_PROPERTY(SuppressedFindingListModel* suppressedFindings
READ suppressedFindings CONSTANT)`, a matching getter, a
`SuppressedFindingListModel suppressedFindings_` member, wired into the
existing begin/apply/end block (`AppController.cpp`, current lines ~868-890)
alongside `findings_` and `files_`.

## 7. QML

`ResultsPage.qml`'s existing `TabBar`/`StackLayout` (lines ~129-143) gains a
third `TabButton`/`StackLayout` page. Layout: a plain `ListView` bound to
`appController.suppressedFindings`, one row per suppressed finding showing
rule id, `path:line`, and symbol name — the same row-rendering approach
already used for the findings tab's `findingTable` `ListView`, without the
detail-panel/filter-bar chrome that tab has (out of scope, see §2). An empty
state ("No hay findings suprimidos en esta ejecución.") when the model has
zero rows, matching the project's existing "empty sections omitted/labeled"
convention from the text/JSON report.

## 8. Error handling / edge cases

- No suppressions this run → the new tab shows the empty-state message, not
  an error and not a hidden tab (a hidden tab would make users wonder if the
  feature is missing rather than simply inapplicable).
- Opening a past snapshot from History → the tab is present but always shows
  the empty state, since `suppressedFindings` is never populated on that
  path (§1, §3). Not documented as a bug anywhere the user would see it
  inside this track's scope, but worth a one-line note in this spec's own
  acceptance criteria so nobody rediscovers it as a "regression" later.
- A suppression comment naming a rule id that doesn't exist, or that never
  matched a real finding (a stale/typo'd `codeqa-disable-next-line`) —
  unchanged from today's behavior: `isSuppressed` only matches an actual
  `Finding` a rule produced, so a stale directive simply never suppresses
  anything and never appears in either list. Not a regression introduced by
  this track.

## 9. Testing and verification strategy

- `ApplyRulesTest.cpp`: a fixture function with a `codeqa-disable-next-line`
  comment above a would-be finding → asserts it appears in
  `suppressedFindings` and does **not** appear in `findings`. A sibling
  fixture with no suppression comment → asserts its finding still lands in
  `findings` and `suppressedFindings` stays empty (regression guard for the
  existing behavior).
- `ReportDocumentFactoryTest.cpp`: live-path test asserting
  `ReportFile::suppressedFindings` is populated from a `RunAnalysisResult`
  containing suppressed findings; a parse-path test (round-tripping a
  serialized report with no `suppressedFindings` key) asserting the field
  stays empty — proves §1/§3's "not persisted" decision is actually
  load-bearing, not just prose.
- Qt Quick Test addition to `tst_DesktopShell.qml`: the mocked
  `fixtureController` gains a `suppressedFindings` fake list model; a new
  test function asserts the new tab is reachable and renders the mocked
  rows, plus one asserting the empty-state message shows when the model is
  empty.

## 10. Acceptance criteria

- A project analyzed with at least one `codeqa-disable-next-line` comment
  above an otherwise-real finding shows that finding under the new
  "Suprimidos" tab in the desktop GUI, and does **not** show it under the
  regular findings tab or count toward `findingCount`/`errorCount`/
  `warningCount` (those already only read `findings_`, untouched by this
  change — verify no regression, don't re-derive them).
- A project with zero suppressions shows the tab with an explicit empty
  state, not a missing/hidden tab.
- CLI text/JSON output (`codeqa analyze --format text|json`) is
  byte-for-byte unchanged by this track — no new keys, no new sections.
- Opening a saved snapshot via the History page shows the "Suprimidos" tab's
  empty state regardless of whether that historical run actually had
  suppressions (documented limitation, not a bug — see §8).
- Existing findings/duplication/quality-gate/history tests all still pass
  unmodified (no regression).
