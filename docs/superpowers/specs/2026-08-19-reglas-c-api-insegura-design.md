# Reglas avanzadas de C: API insegura — Design

**Date:** 2026-08-19
**Status:** approved for specification review
**Track:** post-MVP, roadmap v2 track #3 — see `docs/02-roadmap-v2.md`.

## 1. Goal

Add the first pattern-based (non-threshold) rule to the rule engine:
`security.unsafe-string-api`, which flags calls to a small, well-known set of
C standard library string/I/O functions that have no safe usage pattern —
`gets`, `strcpy`, `strcat`, `sprintf`, `vsprintf` — and recommends a
bounds-checked replacement for each. This is a deliberately narrow first
slice of roadmap track #3 ("Reglas avanzadas de C": memoria, punteros,
arrays, macros, API insegura), scoped down during brainstorming to just the
unsafe-API piece, the only one buildable today with zero model changes.

## 2. Scope and boundaries

### In scope

- One new `Rule` implementation, `UnsafeStringApiRule`, registered in
  `codeqa::rules::allRules`, alongside the existing 8 threshold rules.
- One new config entry, `security.unsafe-string-api`, in
  `codeqa::config::defaultConfig()` — `enabled` by default like every other
  rule, `warningLimit`/`highLimit` present (the `RuleConfig` struct requires
  all three fields) but unused by this rule's logic.
- Desktop GUI: hide the (meaningless, for this rule) warning/high limit
  input fields on `AnalyzePage.qml`'s rule row for this one rule id.
- A new `experimental` row in `tests/corpus/validation/RULE-STABILITY.md`,
  matching how all 8 existing rules started.

### Explicitly out of scope (deferred to a possible future track #3
sub-slice, not forgotten)

- **`scanf`/`fscanf`/`sscanf`/`strtok`.** `CallModel` records
  `argumentCount` but not argument text, so there is no way to tell
  `scanf("%d", &x)` (safe) from `scanf("%s", buf)` (unsafe) without parsing
  the format-string literal — a real piece of new frontend work, not a
  one-line addition to a name list. `strtok`'s issue (global mutable state,
  not memory-unsafe) is a different category from the buffer-overflow class
  the other 5 functions share, and mixing categories in one rule's copy
  would blur the "unsafe = buffer overflow risk" framing this rule commits
  to.
- **Punteros/arrays.** Only reachable today via string matching on
  `ParameterModel::declaredType`/`LocalVariableModel::declaredType` (the
  frontend has no real AST — that's roadmap track #7, Clang frontend) — a
  separate design decision with a real false-positive/negative tradeoff,
  deliberately not bundled into this slice.
- **Macros.** `FileModel` does not model `#define` at all today; adding
  that is frontend work comparable in size to track #5's `CallableModel`
  extension, not something that fits alongside a pure rule addition.
- **Memoria** (malloc/free pairing, use-after-free heuristics). Would need
  either real dataflow analysis (out of reach without a real AST/frontend)
  or a crude same-function malloc-without-free heuristic prone to false
  positives across early returns and helper functions — not attempted here.
- Any change to `Rule::evaluate`'s signature, `RuleContext`, or any of the
  8 existing threshold rules.
- CLI surface changes: the rule flows through the existing `codeqa analyze`
  pipeline automatically, like every other rule — no new flags, no new
  commands.

## 3. Architecture

```text
CallableModel::calls (already populated by StructuralExtractor for every
function/method body — CallModel{calleeText, kind, argumentCount, range})
        |
        v
UnsafeStringApiRule::evaluate(RuleContext)      (new, src/rules/Rules.cpp)
        |  filters: kind == CallKind::Named
        |  matches: text after the last "::" in calleeText, exact-compared
        |           against {"gets","strcpy","strcat","sprintf","vsprintf"}
        v
std::vector<Finding>  — one per matching call site, ruleId =
                         "security.unsafe-string-api", severity = Warning,
                         observedValue/limitValue = std::nullopt
```

No other layer changes. `Finding.observedValue`/`Finding.limitValue` are
already `std::optional<long long>` and every consumer already treats them
as optional end-to-end — verified directly, not assumed:
`ReportJson.cpp` writes JSON `null` when unset, `ReportDocumentFactory.cpp`
reads `null` back to `std::nullopt`, `TextReport.cpp`/`MarkdownReport.cpp`
fall back to printing `0`, and `ResultsPage.qml`'s detail view already
handles an absent value. This rule is the first to actually exercise that
path, but it requires no changes to any of it.

## 4. Rule implementation

```cpp
// src/rules/Rules.cpp, alongside the existing rule classes

struct UnsafeApiEntry
{
    std::string_view name;
    std::string_view recommendation;
};

constexpr std::array<UnsafeApiEntry, 5> kUnsafeStringApis{{
    {"gets", "gets was removed from C11; use fgets(buf, size, stream) instead."},
    {"strcpy", "Use strncpy or strlcpy (bounds-checked) instead."},
    {"strcat", "Use strncat or strlcat (bounds-checked) instead."},
    {"sprintf", "Use snprintf instead, which takes a buffer size."},
    {"vsprintf", "Use vsnprintf instead, which takes a buffer size."},
}};

std::string_view lastQualifiedSegment(std::string_view calleeText)
{
    const std::size_t separator = calleeText.rfind("::");
    return separator == std::string_view::npos
        ? calleeText
        : calleeText.substr(separator + 2);
}

class UnsafeStringApiRule : public Rule
{
public:
    [[nodiscard]] std::string id() const override { return "security.unsafe-string-api"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        std::vector<Finding> findings;

        for (const codeqa::model::CallModel& call : context.callable.calls)
        {
            if (call.kind != codeqa::model::CallKind::Named)
            {
                continue;
            }

            const std::string_view calleeName = lastQualifiedSegment(call.calleeText);
            const auto match = std::find_if(
                kUnsafeStringApis.begin(), kUnsafeStringApis.end(),
                [calleeName](const UnsafeApiEntry& entry) { return entry.name == calleeName; });

            if (match == kUnsafeStringApis.end())
            {
                continue;
            }

            Finding finding;
            finding.ruleId = id();
            finding.severity = codeqa::core::DiagnosticSeverity::Warning;
            finding.description = "Call to unsafe function '" + std::string(calleeName) + "'.";
            finding.recommendation = std::string(match->recommendation);
            finding.location = call.range.begin;
            finding.symbolName = context.callable.symbol.qualifiedName;
            findings.push_back(std::move(finding));
        }

        return findings;
    }
};
```

Registered in `allRules`:

```cpp
if (const codeqa::config::RuleConfig& c = config.rules.at("security.unsafe-string-api"); c.enabled)
{
    rules.push_back(std::make_unique<UnsafeStringApiRule>());
}
```

Note this constructor takes no limits, unlike every other rule's
`(warningLimit, highLimit)` constructor — `c.warningLimit`/`c.highLimit`
are read from config only to check `.enabled`; the rule itself never reads
them, since a call site either matches the blocklist or it doesn't.

`CallKind::Named` is exactly `identifier` or `qualified_identifier` nodes
in `StructuralExtractor.cpp` (`callKind()`) — this is what makes
`strcpy(...)` and `std::strcpy(...)` both match while `obj.strcpy(...)`
(`CallKind::Member`, structurally impossible for these functions but
excluded on principle) and calls through a function pointer
(`CallKind::Indirect`) do not.

## 5. Config

```cpp
// src/config/DefaultConfig.cpp
config.rules["security.unsafe-string-api"] = thresholds(0, 0);
```

Reuses the existing `thresholds()` helper purely to get `enabled = true`
with minimal code — the `0, 0` values are never read by
`UnsafeStringApiRule::evaluate`. The doc comment on `allRules` in
`include/codeqa/rules/Rules.h` ("Requires config.rules to contain an entry
for each of the 8 known rule ids") is updated to say 9.

## 6. Desktop GUI

`AnalyzePage.qml`'s per-rule row currently renders `warningLimitField`/
`highLimitField` unconditionally for every rule. Both gain:

```qml
visible: ruleRow.model.id !== "security.unsafe-string-api"
```

No other GUI change. `ResultsPage.qml` already renders any `Finding`
generically (ruleId, severity, description, recommendation, location,
symbolName, optional observed/limit) — findings from this rule show up in
the existing findings list, filters, and suppressed-findings tab with no
code change, the same way every other rule's findings already do.

## 7. Testing and verification strategy

- `tests/unit/RulesTest.cpp`: new test case for `security.unsafe-string-api`
  covering, on a single `CallableModel` with several `calls` entries in one
  evaluate call:
  - `strcpy(...)` (Named) → 1 finding, `ruleId == "security.unsafe-string-api"`,
    `severity == Warning`, `recommendation` mentions `strncpy`,
    `observedValue`/`limitValue` both `std::nullopt`.
  - `std::strcpy(...)` (Named, qualified) → matches too.
  - `obj.strcpy(...)` (Member) → does not match.
  - `strncpy(...)` (different name entirely) → does not match.
  - A callable with zero unsafe calls → empty result.
- `tests/unit/DefaultConfigTest.cpp`: `rules.size()` assertion 8 → 9, plus
  one new `checkRule(config, "security.unsafe-string-api", 0, 0)` line.
- `tests/unit/CorpusReportTest.cpp`: no code change expected — confirmed by
  grep that no fixture under `tests/corpus/` calls any of the 5 flagged
  functions, so the existing `findings().size() == 1` assertion for
  `nesting/deep-nesting.cpp` stays valid. Verified by actually running the
  full test suite at the end, not solely by the grep.
- `tests/corpus/validation/RULE-STABILITY.md`: new row,
  `security.unsafe-string-api | experimental | — | —`, matching the
  existing 8.

## 8. Acceptance criteria

- A function calling `strcpy`, `strcat`, `sprintf`, `vsprintf`, or `gets`
  (bare or `std::`-qualified) produces one `Warning`-severity finding per
  call site, with a function-specific recommendation.
- A function calling a same-named member function (`obj.strcpy(...)`) or an
  unrelated function (`strncpy`, `my_strcpy`) produces no finding.
- The rule is enabled by default and appears in the desktop GUI's rule list
  without a functioning-but-meaningless pair of limit fields.
- No change to any of the 8 existing rules' behavior, to `Rule::evaluate`'s
  signature, or to any CLI command's output shape beyond the new rule's own
  findings appearing when triggered.
- Full existing test suite (unit + desktop) still passes.
