# Minimal Code Duplication Detection — Design

**Date:** 2026-08-16
**Status:** approved for specification review
**Track:** post-MVP, not a numbered Fase — see `docs/01-cpp-mvp-roadmap.md`, "Fases
posteriores al MVP" → Duplicación. Mirrors the Qt6/QML desktop UI precedent: its
own vertical slice, tracked separately from the Fase 0-12 CLI roadmap.

## 1. Goal

Detect duplicated functions/methods across a project and report them as a new,
dedicated section of the analysis output — not as `Finding`s. This is the
first, deliberately minimal slice of the roadmap's "Duplicación" topic:

- **Type-1 clones**: identical modulo whitespace/comments.
- **Type-2 clones**: identical modulo whitespace/comments, identifiers, and
  literal/type-name identifiers (the standard Roy & Cordy definition — every
  identifier and literal token normalized to a placeholder, everything else
  compared verbatim).

Type-3 (near-miss, statement-level edits) clone detection, sub-function
(block-level) duplication, cross-snapshot duplication history, quality-gate
participation, and GUI surfacing are explicitly deferred — see §2.

## 2. Scope and boundaries

### In scope

- Whole-callable comparison only (a "callable" is anything already modeled as
  `codeqa::model::CallableModel` — free functions, methods, constructors,
  destructors, operators; `Prototype`/`Lambda` included since they already
  carry a `CallableModel`).
- Cross-file and cross-language comparison (a C function and a C++ method that
  normalize identically are still reported as a group — deliberately not
  special-cased, see §6).
- One new pure-computation layer, `codeqa::duplication`, consuming
  `codeqa::model::ProjectModel` — no disk I/O, mirrors `codeqa::metrics` and
  `codeqa::rules`.
- A configurable minimum-size filter to suppress trivial matches (e.g.
  one-line getters), and a project-wide enable/disable toggle.
- New JSON (`duplicates: DuplicateGroup[]`) and text report sections.

### Explicitly out of scope (this phase)

- Type-3 / near-miss clones (needs sequence alignment or winnowing — a later
  "advanced duplication" phase, once Type-1/2 data shows whether it's worth
  the added complexity).
- Sub-function / block-level duplication (sliding windows over statements).
  Needs a new windowing model; deferred until whole-function detection proves
  the approach is useful.
- Quality-gate participation. Duplicate groups do not affect the exit code in
  this phase — there is no real-project data yet to pick a sane default
  threshold. Revisit once the corpus/validation pass (§9) produces evidence.
- Snapshot/history diffing (new/existing/resolved tracking for duplicate
  groups, mirroring `codeqa::history`'s `Finding` tracking). Deferred to a
  follow-up once the minimal version is validated.
- Any GUI (Qt/QML) surface. The desktop shell is its own track; wiring a new
  report section into it is a separate follow-up, same as it was for existing
  report sections.
- A generalized "project-scoped rule" abstraction (`ProjectRule` next to
  `Rule`). Duplication would be the only concrete use case today; the
  project's own established convention (Fase 7 design doc) rejects introducing
  a type hierarchy without a second concrete case. Revisit if/when
  architecture/include-graph analysis (also inherently project-scoped) is
  built — at that point two real cases would justify it.

## 3. Architecture

```text
 Frontend (tree-sitter, per file)         Model (Fase 5, CST-agnostic)
 src/frontend/StructuralExtractor.cpp -->  src/model/ProjectModelBuilder.cpp
   walks CST leaves in each callable's       carries CallableModel through,
   range, builds normalizedTokenSequence      including the new field
   + normalizedTokenCount
                                                      |
                                                      v
                                          codeqa::model::ProjectModel
                                                      |
                                                      v
                              +--------------------------------------------+
                              | codeqa::duplication (new, sibling to        |
                              | codeqa::metrics / codeqa::rules)            |
                              | detectDuplicates(ProjectModel, config)      |
                              |   -> ProjectDuplicationModel                |
                              +--------------------------------------------+
                                                      |
                                                      v
                              codeqa::report (renderText / toReportJson)
                                extended with a `duplicates` section
```

`codeqa::duplication` depends only on `codeqa::model`/`codeqa::core`, same
layering rule the other analysis layers already follow. It is invoked from
`codeqa::application` (the same place that today chains
`AnalyzeProject → MeasureProject → ApplyRules`), after the project model is
fully built, before reporting.

## 4. Model extension

`codeqa::model::CallableModel` gains two fields, populated in
`StructuralExtractor.cpp` (the only place that still has raw tree-sitter node
types available — the model layer stays CST-agnostic, matching Fase 5's own
stated goal):

```cpp
struct CallableModel
{
    // ...existing fields unchanged...
    std::string normalizedTokenSequence;  // space-joined normalized tokens
    std::size_t normalizedTokenCount{0};  // token count, used by the minimum-size filter
};
```

## 5. Normalization algorithm

For each callable's `SymbolIdentity::range`, walk the tree-sitter CST leaf
tokens it contains (comments and whitespace are already excluded — tree-sitter
does not emit them as normal tokens):

- Node type `identifier`, `field_identifier`, or `type_identifier` → emit the
  placeholder `ID`.
- Node type `number_literal`, `string_literal`, or `char_literal` → emit the
  placeholder `LIT`.
- Any other leaf (keywords, operators, punctuation) → emit its literal text
  unchanged.

Join emitted tokens with a single space into `normalizedTokenSequence`; record
the emitted-token count as `normalizedTokenCount`. The callable's own name is
not special-cased — it is itself an `identifier` token and normalizes like any
other, so two identically-shaped functions with different names still match
without extra logic.

This treats type names (`uint16_t` vs. `uint32_t`) as renamable identifiers
too, per the standard Type-2 definition — noted explicitly here so it reads as
an intentional classification choice, not an oversight.

## 6. Matching algorithm and configuration

`detectDuplicates` groups all callables project-wide:

1. Skip callables with `normalizedTokenCount < config.minNormalizedTokens`.
2. Bucket the rest by `std::hash<std::string>` of `normalizedTokenSequence`
   (fast, expected O(n)) — an internal implementation detail only, never
   serialized or compared across runs/builds (`std::hash` has no
   cross-implementation stability guarantee, so it must never leak into a
   golden/corpus fixture that could run under GCC, Clang, or MSVC).
3. Within each bucket, verify full `normalizedTokenSequence` string equality
   before grouping — the hash is only a fast pre-filter, never the correctness
   check, so a collision cannot fabricate a false duplicate group.
4. Buckets that end up with 2+ verified-equal members become a
   `DuplicateGroup`; singletons are discarded.

Groups and their members are sorted deterministically (by first member's
file path then start offset, members within a group the same way) — matching
the project's existing determinism requirement for reports.

No cross-language special-casing (§2): the algorithm does not look at
`FileModel::language` at all.

New types (`codeqa::duplication`):

```cpp
struct DuplicateMember
{
    std::string qualifiedName;
    core::SourceRange range;   // range.begin.filePath / range.end.filePath already
                                // carry the file path (see core::SourceLocation) —
                                // no separate filePath field, avoiding a second
                                // source of truth for the same value
};

struct DuplicateGroup
{
    std::vector<DuplicateMember> members;  // always size() >= 2
};

struct ProjectDuplicationModel
{
    std::vector<DuplicateGroup> groups;
};
```

Configuration — new top-level `codeqa::config::DuplicationConfig`, parsed from
a new `"duplication"` key in `codeqa.json`:

```json
{ "duplication": { "enabled": true, "minNormalizedTokens": 30 } }
```

`minNormalizedTokens: 30` is an unvalidated starting guess, flagged as such —
same treatment Fase 7 gave its 8 rule thresholds. It merges by whole-block
replacement when present (mirrors `qualityGate`'s merge, not `rules{}`'s
per-rule-id merge, since this isn't a rule-id map). New `CONFIG0xx` codes
extend the existing validation taxonomy for a non-boolean `enabled`, a
negative/zero `minNormalizedTokens`, or an unknown key under `duplication`.
`enabled: false` skips `detectDuplicates` entirely (empty
`ProjectDuplicationModel`, not an empty-but-computed one).

## 7. Reporting

**JSON** (`codeqa::report::toReportJson`): new top-level `duplicates` array,
one entry per `DuplicateGroup`, each member giving `qualifiedName`, relative
`filePath`, and location — same shape/conventions already used for `findings`
entries. Additive field; existing `schemaVersion` consumers ignore unknown
keys already (report JSON has no strict-unknown-key rejection today, unlike
`codeqa.json`), so no version bump is required.

**Text** (`codeqa::report::renderText`): new section, printed after Findings
and before Diagnostics, omitted entirely when `duplicates` is empty (matching
the existing "empty sections omitted" convention). Each group lists its
members with file:line.

## 8. Error handling / edge cases

- A callable overlapping a `PARSE001`/`PARSE002`/`PARSE004` diagnostic is
  still included — unlike `ApplyRules`'s confidence filter, duplication isn't
  a per-symbol correctness judgment about *that* function, it's a structural
  comparison; excluding parse-damaged functions would just make real
  duplicates of otherwise-fine code silently disappear whenever one copy sits
  in a partially-parsed file. Deliberate deviation from the `ApplyRules`
  precedent, called out explicitly since it looks inconsistent otherwise.
- Empty project / no callables above the size floor → `ProjectDuplicationModel{}`
  with an empty `groups` vector, not an error.
- `enabled: false` and a project with zero eligible callables are
  indistinguishable in the output (both produce an empty `duplicates` array) —
  acceptable, matches how disabled rules already behave in `findings`.

## 9. Testing and verification strategy

- Unit tests for the normalization step (`StructuralExtractorTest.cpp` or a
  new sibling): identifier rename preserves `normalizedTokenSequence`
  equality; literal value change preserves it; whitespace/comment-only change
  preserves it; a real structural change (different statement, different
  operator) changes it.
- Unit tests for `detectDuplicates`: two verified-equal callables form a
  group; a lone callable below `minNormalizedTokens` never appears; a
  synthetic same-hash/different-sequence pair (if feasible to construct
  directly against the hash-bucket step) does **not** get grouped, proving the
  full-equality check is load-bearing and not just the hash.
- New corpus fixtures under `tests/corpus/duplication/` (same pattern as
  `tests/corpus/size/`, `tests/corpus/complexity/`): an exact clone pair, a
  renamed (Type-2) clone pair, a below-threshold pair that must not match, and
  one documented "looks like a false positive" pair (two trivial functions
  that normalize equal but arguably aren't meaningful duplication) to make the
  threshold trade-off visible in the test suite itself, not just in prose.
- Informal sanity check against the real SQLite/fmt corpus before closing this
  phase (same practice as Fase 4b/6b) — not a formal precision ledger. A
  Fase-11-style systematic validation pass is explicitly deferred until real
  usage produces enough duplicate groups to judge.

## 10. Acceptance criteria

- `codeqa analyze` on a project containing a hand-copied function (identical
  or trivially renamed) reports it as a duplicate group in both text and JSON
  output.
- A function below `minNormalizedTokens` never appears in any group.
- `duplication.enabled: false` in `codeqa.json` produces no `duplicates`
  output and does not change any other report section.
- Findings, diagnostics, metrics, snapshot/history behavior, and the quality
  gate are all unaffected by this feature (no regression in existing tests).
- Determinism: re-running `analyze` on an unchanged project produces
  byte-identical `duplicates` output.
- The real-corpus sanity check (§9) surfaces at least one true positive and
  the review explicitly notes any suspected false positive found along the
  way, even though no formal ledger is produced this phase.
