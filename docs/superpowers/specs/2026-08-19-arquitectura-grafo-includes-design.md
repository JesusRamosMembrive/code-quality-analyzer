# Arquitectura: Grafo de Includes (Ciclos + Fan-out) — Design

**Date:** 2026-08-19
**Status:** approved for specification review
**Track:** post-MVP, roadmap v2 track #4 — see `docs/02-roadmap-v2.md`.

## 1. Goal

Give CodeQA a project-wide dependency graph built from `#include` directives,
and two analyses on top of it: circular-dependency detection and fan-out
(efferent coupling) findings. This is a deliberately narrow first slice of
track #4 (the roadmap row also lists "capas" and "dependencias prohibidas"),
scoped down during brainstorming to the two pieces that need no new
user-facing configuration schema beyond a single warning/high pair.

## 2. Scope and boundaries

### In scope

- Resolving `codeqa::model::DependencyModel::target` (today just the raw
  `#include` text) against the project's own discovered files, to build a
  directed graph where nodes are files and edges are "file A includes
  project file B".
- Circular-dependency detection over that graph (strongly connected
  components of size ≥ 2).
- Fan-out (efferent coupling: how many distinct project files a file
  includes) as a threshold-based finding. Fan-in (afferent coupling: how
  many files include this one) is computed and reported for every file,
  but **never** produces a finding — a file being widely included is
  usually good design (the "stable dependencies" principle), not a smell,
  and giving it a threshold would produce false positives on legitimate
  foundational headers. This was an explicit brainstorm decision, not an
  oversight.
- A new sibling layer, `codeqa::architecture`, analogous to
  `codeqa::duplication` in every structural respect: its own model types,
  its own `ProjectConfig` sub-config, wired into `RunAnalysis` alongside
  `detectDuplicates`, its own section in the JSON/text/Markdown reports.
  **Not** part of `codeqa::rules` — confirmed by reading
  `ApplyRules::execute` directly: it iterates `project.files` then
  `file.callables`, builds one `RuleContext{metrics, callable}` per
  function, and calls every `Rule::evaluate` against it. There is no
  file-level or project-level rule execution path today, and cycles /
  fan-out are inherently not about one function — they don't fit
  `Rule`/`RuleContext` at all, the same way duplication doesn't.

### Explicitly out of scope (deferred, not forgotten)

- **Capas** (layers) and **dependencias prohibidas** (forbidden pairs).
  Both need the user to define new domain concepts (what is a layer, which
  pairs are forbidden) — a real config-schema design question of its own,
  bigger than this slice. Candidate future sub-slice of the same track #4.
- **Desktop GUI surface.** `docs/02-roadmap-v2.md` §4 already noted this
  explicitly before this track's brainstorm: "El track 4 (arquitectura) no
  incluye visualización en la GUI por defecto." Confirmed still true and
  carried forward as-is.
- **Quality gate integration.** The gate (`hasNewFindingAtOrAboveSeverity`)
  operates over `codeqa::rules::Finding`/`FindingKey`. Architecture findings
  are a structurally different shape (a cycle spans multiple files; it has
  no single `symbolName`/`ruleId` pair matching the rest of that type) and
  are not wired into gate evaluation, matching how duplication's own
  findings aren't either.
- **Suppression via `// codeqa-disable-next-line`.** That mechanism keys
  off a single line immediately above a finding's location
  (`ApplyRules.cpp`'s `suppressedRuleIdOnLine`). A cycle has no single
  natural line; a fan-out finding's "location" would be the whole file, not
  a line inside it. No suppression mechanism ships in this slice.
- Any change to `codeqa::rules`, `Rule`/`RuleContext`, or any of the 9
  existing rules (8 threshold + `security.unsafe-string-api`).
- Resolving includes through actual compiler search-path semantics
  (`-I` flags, `compile_commands.json`). CodeQA has no such input anywhere
  in its pipeline today; resolution here is a project-relative heuristic,
  consistent with how the rest of the analyzer works without a real
  compiler frontend (see track #7, Clang frontend, explicitly deferred).

## 3. Architecture

```text
codeqa::model::ProjectModel (DependencyModel.target = raw #include text,
already flows through the existing frontend/model layers, untouched here)
        |
        v
resolveIncludeGraph(project) -> IncludeGraph        (new, src/architecture/)
        |  Local (`"..."`) includes matched by path suffix against
        |  project.files; System (`<...>`) includes always excluded;
        |  self-includes dropped; 0 matches = external/unresolved
        |  (expected, not an error); >1 match = ambiguous (first wins,
        |  counted)
        v
findCycles(graph) -> vector<DependencyCycle>         (new, Tarjan's SCC)
computeFanInOut(graph) -> vector<FileDependencyMetrics>  (new, degree count)
        |
        v
analyzeArchitecture(project, config) -> ProjectArchitectureModel
        (new, src/architecture/AnalyzeArchitecture.cpp — orchestrates the
        three steps above, applies config.architecture's fan-out threshold)
        |
        v
RunAnalysis::execute (existing) — one new line, mirrors the existing
        result.duplicates = detectDuplicates(...) call exactly
        |
        v
toReportJson / renderText (existing) — one new parameter each, mirrors
        duplicates' own parameter exactly; new "architecture" section,
        rendered only when non-empty (mirrors writeDuplicates).
        renderMarkdown is untouched — verified duplication never extended
        it either (see §9)
```

`IncludeGraph` (the resolved graph) is an internal type, not part of the
public `codeqa::architecture` model — mirrors how duplication's own
internal hashing/matching machinery isn't part of `ProjectDuplicationModel`
either. It lives in a source-local header, `src/architecture/IncludeGraph.h`
(same convention as `src/report/ReportTextUtilities.h`: not under
`include/`, but still directly unit-testable, since the resolution
heuristic is exactly the kind of logic that deserves its own tests
independent of the cycle/fan-out algorithms built on top of it).

## 4. Data model

```cpp
// src/architecture/IncludeGraph.h (source-local, directly testable)
namespace codeqa::architecture
{

struct IncludeGraph
{
    std::vector<std::filesystem::path> filePaths;        // index-aligned
    std::vector<std::vector<std::size_t>> outgoingEdges;  // outgoingEdges[i] = indices file i includes
    std::size_t externalOrUnresolvedIncludes{0};          // 0-match Local includes (expected: vendored/quoted external headers)
    std::size_t ambiguousIncludes{0};                     // >1-match Local includes (first match wins, deterministically)
};

[[nodiscard]] IncludeGraph resolveIncludeGraph(const codeqa::model::ProjectModel& project);

} // namespace codeqa::architecture
```

```cpp
// include/codeqa/architecture/ArchitectureModels.h (public)
namespace codeqa::architecture
{

struct FileDependencyMetrics
{
    std::filesystem::path path;
    std::size_t fanIn{0};
    std::size_t fanOut{0};
};

struct DependencyCycle
{
    std::vector<std::filesystem::path> files;  // one strongly connected component, size >= 2
};

struct FanOutFinding
{
    std::filesystem::path path;
    std::size_t fanOut{0};
    codeqa::core::DiagnosticSeverity severity{codeqa::core::DiagnosticSeverity::Warning};
};

struct ProjectArchitectureModel
{
    std::vector<FileDependencyMetrics> files;   // every project file, always populated (informative, no verdict)
    std::vector<DependencyCycle> cycles;
    std::vector<FanOutFinding> fanOutFindings;  // only files at/above config.fanOutWarning
    std::size_t externalOrUnresolvedIncludes{0};
    std::size_t ambiguousIncludes{0};
};

} // namespace codeqa::architecture
```

```cpp
// include/codeqa/architecture/AnalyzeArchitecture.h (public entry point)
namespace codeqa::architecture
{

[[nodiscard]] ProjectArchitectureModel analyzeArchitecture(
    const codeqa::model::ProjectModel& project, const codeqa::config::ArchitectureConfig& config);

} // namespace codeqa::architecture
```

Self-includes (a file whose resolved include graph would point back at
itself) are dropped during resolution — never added as an edge. A 1-node
self-loop is not a meaningful "cycle" for this rule's purpose and would
conflate with genuine multi-file cycles in the SCC output.

## 5. Cycle detection

Standard Tarjan's strongly-connected-components algorithm over
`IncludeGraph`'s adjacency list (linear time, no external dependency — not
something the standard library provides, but a well-known, compact
algorithm, consistent with `codeqa::duplication`'s own hand-rolled
matching). Every SCC of size ≥ 2 becomes one `DependencyCycle`, listing its
member file paths. **Deliberately not** exhaustive simple-cycle
enumeration — a real codebase's cyclic cluster can contain an exponential
number of simple cycles between the same files; reporting the maximal SCC
once (as most architecture linters do — e.g. madge, dependency-cruiser)
keeps this tractable and correct, at the cost of not enumerating every
individual cycle path within a cluster. No threshold applies: any SCC of
size ≥ 2 is reported.

## 6. Fan-in / fan-out

`fanOut(file)` = size of `outgoingEdges[file]` (already deduplicated by
construction — a file including the same header via two separate
`#include` lines produces one edge, not two, since edges are graph
membership, not directive counts). `fanIn(file)` = count of files whose
`outgoingEdges` contains this file's index. Both are computed for every
file and always appear in `ProjectArchitectureModel::files`. Only fan-out
is compared against a threshold to produce a `FanOutFinding` — fan-in never
does (see §2).

## 7. Config

```cpp
// include/codeqa/config/ConfigModels.h
struct ArchitectureConfig
{
    bool enabled{true};
    long long fanOutWarning{10};
    long long fanOutHigh{20};
};

struct ProjectConfig
{
    // ...existing fields unchanged...
    ArchitectureConfig architecture;
};
```

`defaultConfig()` needs **no** new line — exactly like `DuplicationConfig
duplication;`, the member's own default initializers are the defaults;
`defaultConfig()` never explicitly assigns `config.duplication` either.
Starting thresholds (10/20) are an unvalidated starting guess, same
convention as the 8 original threshold rules' own initial numbers — marked
`experimental` in spirit, though `RULE-STABILITY.md`'s table is specific to
`codeqa::rules` rule ids and this isn't one; a parallel stability note for
`codeqa::architecture` is a documentation decision for the plan, not the
spec.

JSON parsing in `src/config/ConfigJson.cpp` mirrors `parseDuplication`
exactly: a new `parseArchitecture(const Json&)` helper, `"architecture"`
added to the top-level `requireKeysSubset` allowed-keys list (alongside
`"languages"`, `"rules"`, `"qualityGate"`, `"duplication"`), and a new
validation error `CONFIG013` for `fanOutWarning > fanOutHigh` (mirrors
`CONFIG008`'s existing rule-threshold check exactly). No positivity check
is needed (unlike `duplication.minNormalizedTokens`'s `CONFIG012`) — a
fan-out threshold of `0` is a legitimate (if aggressive) configuration,
not a nonsensical one. `toJson()` gains a matching `document["architecture"]
= {{"enabled", ...}, {"fanOutWarning", ...}, {"fanOutHigh", ...}}` block.

`CONFIG013` was confirmed free by grepping the whole repo for
`CONFIG007`/`CONFIG010`/`CONFIG011`/`CONFIG013`/`CONFIG014` before picking
it — `007`/`010`/`011` are already used elsewhere (application-layer config
codes, not `ConfigJson.cpp`'s own parsing codes), `013` is not used
anywhere in the codebase.

## 8. Wiring into the analysis pipeline

`RunAnalysis::execute` (`src/application/RunAnalysis.cpp`) gains one line,
directly after the existing `detectDuplicates` call:

```cpp
result.architecture = codeqa::architecture::analyzeArchitecture(result.model, request.config.architecture);
```

`RunAnalysisResult` (`include/codeqa/application/RunAnalysis.h`) gains
`codeqa::architecture::ProjectArchitectureModel architecture;` — same
pattern as the existing `duplicates` field, added the same way.

## 9. Report integration

**JSON** (`toReportJson`, new parameter mirroring `duplicates` exactly):

```json
"architecture": {
  "files": [
    {"path": "src/core/Models.h", "fanIn": 12, "fanOut": 3}
  ],
  "cycles": [
    {"files": ["src/a.h", "src/b.h"]}
  ],
  "fanOutFindings": [
    {"path": "src/legacy/GodHeader.h", "fanOut": 25, "severity": "error"}
  ],
  "externalOrUnresolvedIncludes": 4,
  "ambiguousIncludes": 1
}
```

**Text report** (`TextReport.cpp`): a new `writeArchitecture`-style
section, rendered only when there is something to show (at least one cycle
or fan-out finding) — mirrors `writeDuplicates`'s existing "empty groups →
skip the section entirely" behavior. The full per-file fan-in/fan-out
table is JSON-only (would be excessive noise in a human-read text report
for a large project); the text section surfaces only the cycles and the
flagged fan-out findings.

**Markdown report: out of scope, corrected during planning.** Verified by
reading `MarkdownReport.cpp` directly: `renderMarkdown` doesn't even take
a `duplicates` parameter — duplication was never added to it either. It's
a per-finding checklist renderer for the desktop GUI's "export as
checklist" feature (`AppController::exportMarkdown`), one checkbox item
per `Finding`/`Diagnostic` inside a per-file section. Cycles and fan-out
findings don't have that shape (a cycle spans multiple files; a fan-out
finding's natural unit is a whole file, not a checklist line), and
extending `renderMarkdown` isn't required to match the existing
duplication precedent, which also skipped it. No change to
`MarkdownReport.cpp`/`renderMarkdown`'s signature in this slice.

**Desktop GUI / `ReportDocumentFactory`:** confirmed by reading
`ReportDocumentFactory.cpp` directly — its JSON parsing is field-by-field
pull-based (`requireField`/`document.contains(...)`), not a
reject-unknown-keys schema validator like `ConfigJson.cpp`'s
`requireKeysSubset`. An unrecognized top-level `"architecture"` key is
silently ignored by the existing parser. No change needed there for this
slice to ship safely — consistent with "no GUI surface" being an explicit
scope decision, not an oversight.

## 10. Testing and verification strategy

- `IncludeGraph`/`resolveIncludeGraph`: unit tests on hand-built
  `ProjectModel` fixtures — resolves a simple local include; skips a
  system include entirely; an unresolved local include increments
  `externalOrUnresolvedIncludes` and adds no edge; an ambiguous local
  include (two files sharing a path suffix) increments `ambiguousIncludes`
  and resolves deterministically to the first match; a self-include adds
  no edge.
- Cycle detection: unit tests on hand-built `IncludeGraph`s — no cycle (0
  results); a 2-file mutual cycle; a 3-file cycle; two independent cycles
  in the same graph (2 separate `DependencyCycle` entries, not merged); a
  larger connected-but-acyclic diamond (A→B, A→C, B→D, C→D — no cycle,
  common false-positive risk for a naive implementation).
- Fan-in/fan-out: unit tests on a small hand-built graph with known
  in/out-degrees per node, including a node with fan-out above/at/below
  both thresholds (severity boundary behavior, same convention as the
  existing threshold rules' own boundary tests).
- `analyzeArchitecture`: integration test wiring the three pieces together
  with `config.architecture.enabled = false` producing an empty
  `ProjectArchitectureModel` (mirrors `duplication.enabled` gating).
- `RunAnalysis`/`toReportJson`/text report: at least one test proving the
  new report section appears with real data and is absent when
  `ProjectArchitectureModel` is empty. Whether this needs a new
  `tests/corpus/` fixture with a genuine include cycle (none exists today,
  confirmed by the absence of any prior architecture-shaped fixture) is a
  plan-level decision, not resolved here — hand-built `ProjectModel`
  fixtures may be sufficient, matching how Task 1 of the
  `security.unsafe-string-api` track relied entirely on hand-built models
  rather than a new corpus fixture.

## 11. Acceptance criteria

- Given a project with `a.h` including `b.h` and `b.h` including `a.h`
  (directly, via quoted local includes), `codeqa analyze` reports one
  cycle containing both files.
- Given a file whose resolved local-include fan-out is at or above
  `config.architecture.fanOutWarning`, one `FanOutFinding` appears with the
  correct severity (`warning` at/above `fanOutWarning`, `error` at/above
  `fanOutHigh`).
- A file with high fan-in and low fan-out (a plausible foundational header)
  never produces a finding, regardless of how high its fan-in is.
- `<system>` includes never appear as graph edges or affect any file's
  fan-in/fan-out count.
- Setting `architecture.enabled = false` in `codeqa.json` produces an
  empty `ProjectArchitectureModel` and an absent/empty report section,
  with zero behavior change to any other part of the report.
- No change to any of the 9 existing rules, `Rule`/`RuleContext`, the
  quality gate, or the suppression mechanism.
- Existing findings/duplication/quality-gate/history/suppressed-findings
  tests all still pass unmodified.
