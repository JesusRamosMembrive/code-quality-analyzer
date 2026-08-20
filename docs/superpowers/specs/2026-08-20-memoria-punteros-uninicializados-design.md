# Reglas avanzadas de C: punteros locales sin inicializar — Design

**Date:** 2026-08-20
**Status:** approved for specification review
**Track:** post-MVP, roadmap v2 track #3 (continuación) — see `docs/02-roadmap-v2.md`.

## 1. Goal

Add a second pattern-based rule to the rule engine: `memory.uninitialized-pointer`,
which flags local pointer variables declared without an initializer — a
well-known source of undefined behavior when the pointer is dereferenced or
assigned-through before it is ever set. This is the second slice of roadmap
track #3 ("Reglas avanzadas de C": memoria, punteros, arrays, macros), after
`security.unsafe-string-api`. Scoped during brainstorming to local variables
only — the only part of "punteros/arrays" buildable today with zero frontend
changes, once a real nuance was found and confirmed by reading
`StructuralExtractor.cpp`: `LocalVariableModel::declaredType` already
includes the declarator suffix (`"int*"`, `"int[10]"`), but
`ParameterModel::declaredType` does not (`parameterDeclaredType()` stops at
the declarator's start byte, deliberately, to avoid swallowing a default
value) — so array/pointer detection on parameters is not the same free text
heuristic it is on locals, and is out of scope here.

## 2. Scope and boundaries

### In scope

- One new `Rule` implementation, `UninitializedPointerRule`, registered in
  `codeqa::rules::allRules`, alongside the existing 9 rules.
- One new config entry, `memory.uninitialized-pointer`, in
  `codeqa::config::defaultConfig()` — `enabled` by default, `warningLimit`/
  `highLimit` present but unused, same as `security.unsafe-string-api`.
- Desktop GUI: hide the warning/high limit input fields on `AnalyzePage.qml`'s
  rule row for this rule id too.
- A new `experimental` row in `tests/corpus/validation/RULE-STABILITY.md`.

### Explicitly out of scope (deferred, not forgotten)

- **Array parameters decaying to pointers.** `ParameterModel::declaredType`
  does not capture the array declarator suffix today (see Goal above) —
  detecting this needs either extending `parameterDeclaredType()` to include
  the suffix (mirroring `declarationType()`, used for locals/globals) or a
  new `ParameterPassing::Array` enumerator populated from the AST. Real but
  small frontend work, deliberately not bundled into this rule-only slice.
- **Uninitialized pointer parameters or global variables.** `RuleContext`
  only exposes `callable` + `metrics` — no access to `FileModel::globalVariables`
  from a per-function rule. Parameters are almost always "initialized" by the
  call site by construction (the caller supplies the argument), so this
  category doesn't apply to them the way it does to locals.
- **Uninitialized function-pointer locals.** `declaredType` for a function
  pointer ends in `)`, not `*` (e.g. `"int(*)(int)"`), so the `ends_with('*')`
  heuristic below naturally excludes them. A dedicated rule for this is a
  separate, not-yet-designed decision.
- **Dataflow-aware initialization tracking.** A pointer declared on one line
  and assigned on the next (`int* p; p = &x;`) is still flagged — there is no
  control-flow/data-flow analysis available (same class of limitation
  already accepted for `size.*`/`complexity.*` threshold rules and for
  track #4's fan-in). Documented as a known limitation, not treated as a bug.
- Macros, `malloc`/`free` pairing, `scanf`-family format-string parsing — all
  already out of scope per the first slice's spec, unchanged here.
- Any change to `Rule::evaluate`'s signature, `RuleContext`, or any of the 9
  existing rules' behavior.

## 3. Architecture

```text
CallableModel::localVariables (already populated by StructuralExtractor for
every function/method body — LocalVariableModel{symbol, declaredType,
hasInitializer, lexicalDepth})
        |
        v
UninitializedPointerRule::evaluate(RuleContext)   (new, src/rules/Rules.cpp)
        |  filters: declaredType (trimmed) ends with '*'
        |           AND hasInitializer == false
        v
std::vector<Finding>  — one per matching local variable, ruleId =
                         "memory.uninitialized-pointer", severity = Warning,
                         observedValue/limitValue = std::nullopt
```

No other layer changes — same `Finding.observedValue`/`limitValue` optional
path already exercised by `security.unsafe-string-api`, already verified
end-to-end (`ReportJson.cpp`, `ReportDocumentFactory.cpp`, `TextReport.cpp`/
`MarkdownReport.cpp`, `ResultsPage.qml`).

## 4. Rule implementation

```cpp
// src/rules/Rules.cpp, alongside the existing rule classes

bool looksLikeRawPointer(std::string_view declaredType)
{
    return !declaredType.empty() && declaredType.back() == '*';
}

class UninitializedPointerRule : public Rule
{
public:
    [[nodiscard]] std::string id() const override { return "memory.uninitialized-pointer"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        std::vector<Finding> findings;

        for (const codeqa::model::LocalVariableModel& local : context.callable.localVariables)
        {
            if (local.hasInitializer || !looksLikeRawPointer(local.declaredType))
            {
                continue;
            }

            Finding finding;
            finding.ruleId = id();
            finding.severity = codeqa::core::DiagnosticSeverity::Warning;
            finding.description =
                "Local pointer variable '" + local.symbol.name + "' is declared without being initialized.";
            finding.recommendation =
                "Initialize the pointer at declaration (e.g., to nullptr or a valid address) "
                "to avoid undefined behavior from using it before assignment.";
            finding.location = local.symbol.range.begin;
            finding.symbolName = context.callable.symbol.qualifiedName;
            findings.push_back(std::move(finding));
        }

        return findings;
    }
};
```

Registered in `allRules`:

```cpp
if (const codeqa::config::RuleConfig& c = config.rules.at("memory.uninitialized-pointer"); c.enabled)
{
    rules.push_back(std::make_unique<UninitializedPointerRule>());
}
```

`declaredType` is already trimmed by the frontend (`declarationType()`
returns `trim(base) + trim(prefix) + trim(suffix)` concatenated with no
added whitespace — verified against existing test assertions:
`"const char*"`, `"const auto&"`, `"int(*)(int)"`), so `ends_with('*')`
needs no additional trimming. It naturally excludes arrays (`"int[10]"`,
ends in `]`), smart pointers (`"std::unique_ptr<int>"`, ends in `>`), and
function pointers (`"int(*)(int)"`, ends in `)`).

## 5. Config

```cpp
// src/config/DefaultConfig.cpp
config.rules["memory.uninitialized-pointer"] = thresholds(0, 0);
```

Same pattern as `security.unsafe-string-api` — `thresholds()` reused purely
for `enabled = true`; `0, 0` never read by `evaluate`. Doc comment on
`allRules` in `include/codeqa/rules/Rules.h` updated from "9 known rule ids"
to "10".

## 6. Registration surface

Same 6 touch points found the last time a rule was added (there is no
central rule registry — confirmed again by reading each file):

1. `src/config/DefaultConfig.cpp` — new `config.rules[...]` line (§5).
2. `src/config/ConfigJson.cpp::knownRuleIds()` (line ~34) — append
   `"memory.uninitialized-pointer"` to the static vector; without this,
   `codeqa.json` configs mentioning the new rule id are rejected with
   `CONFIG003`.
3. `src/desktop/ReportModels.cpp::orderedRuleIds()` (line ~28) — append the
   id, same list shape (`QStringLiteral`); gates the desktop
   `RuleConfigModel`'s row count and order.
4. `include/codeqa/rules/Rules.h` doc comment — 9 → 10.
5. `src/desktop/qml/pages/AnalyzePage.qml` — extend the existing `visible`
   condition on `warningLimitField`/`highLimitField` to also exclude
   `"memory.uninitialized-pointer"`:
   `visible: ruleRow.model.id !== "security.unsafe-string-api" && ruleRow.model.id !== "memory.uninitialized-pointer"`.
6. `tests/desktop/AppControllerTest.cpp` — two `rowCount()` assertions
   (lines ~219, ~262) 9 → 10, plus the `expectedRuleIds` list (line ~220)
   gets the new id appended in the same position as `knownRuleIds()`/
   `orderedRuleIds()`.

## 7. Testing and verification strategy

- `tests/unit/RulesTest.cpp`: new test case for `memory.uninitialized-pointer`
  covering, on `CallableModel::localVariables` entries in one `evaluate`
  call:
  - `int* p;` (raw pointer, no initializer) → 1 finding, `ruleId ==
    "memory.uninitialized-pointer"`, `severity == Warning`, description
    names the variable, `observedValue`/`limitValue` both `std::nullopt`.
  - `int* p = nullptr;` (raw pointer, initialized) → no finding.
  - `int arr[10];` (array, no initializer) → no finding (doesn't end in `*`).
  - `std::unique_ptr<int> p;` (smart pointer, no initializer) → no finding.
  - `int (*fp)(int);` (function pointer, no initializer) → no finding.
  - `int x;` (non-pointer, no initializer) → no finding (existing
    `size.local-variables` territory, untouched).
  - A callable with zero local variables → empty result.
- `tests/unit/DefaultConfigTest.cpp`: `rules.size()` assertion 9 → 10, plus
  `checkRule(config, "memory.uninitialized-pointer", 0, 0)`.
- `tests/unit/CorpusReportTest.cpp`: grep `tests/corpus/` fixtures for any
  uninitialized raw-pointer local matching this pattern before assuming no
  change is needed — if found, update the affected fixture's expected
  finding count; otherwise no code change. Confirmed by actually running the
  full test suite at the end, not solely by the grep.
- `tests/corpus/validation/RULE-STABILITY.md`: new row,
  `memory.uninitialized-pointer | experimental | — | —`.

## 8. Acceptance criteria

- A function declaring a raw-pointer local variable without an initializer
  produces one `Warning`-severity finding per such variable, naming the
  variable in the description.
- A pointer local initialized at declaration, an array local, a smart
  pointer local, a function-pointer local, or a non-pointer local all
  produce no finding from this rule.
- The rule is enabled by default and appears in the desktop GUI's rule list
  without a functioning-but-meaningless pair of limit fields.
- No change to any of the 9 existing rules' behavior, to `Rule::evaluate`'s
  signature, or to any CLI command's output shape beyond the new rule's own
  findings appearing when triggered.
- Full existing test suite (unit + desktop) still passes.
