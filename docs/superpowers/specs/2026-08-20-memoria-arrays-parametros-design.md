# Reglas avanzadas de C: arrays en parámetros — Design

**Date:** 2026-08-20
**Status:** approved for specification review
**Track:** post-MVP, roadmap v2 track #3 (tercera vuelta) — see `docs/02-roadmap-v2.md`.

## 1. Goal

Add a third rule to the rule engine: `memory.array-parameter-decay`, which
flags a function parameter declared with array syntax (`int arr[]`,
`int arr[10]`) — in C/C++, such a parameter always decays to a bare pointer
at the call boundary, silently discarding whatever size the declarator
appears to promise. This is the third slice of roadmap track #3 ("Reglas
avanzadas de C"), after `security.unsafe-string-api` and
`memory.uninitialized-pointer`. Unlike the previous slice, this one requires
a small, real frontend extension: today neither `ParameterModel::declaredType`
nor `ParameterModel::passing` carries any signal that a parameter is
array-shaped — `parameterDeclaredType()` deliberately stops before the
declarator (to avoid swallowing a default value), and `ParameterPassing`'s
existing values (`Value`/`Pointer`/`Reference`/`FunctionPointer`) are derived
from a declarator-shape check that never looks for `array_declarator`.

## 2. Scope and boundaries

### In scope

- **Frontend:** a new `ParameterPassing::Array` enumerator
  (`include/codeqa/model/Models.h`), populated in
  `parametersFor()` (`src/frontend/StructuralExtractor.cpp`) by checking the
  parameter's declarator for an `array_declarator`/`abstract_array_declarator`
  descendant, inserted into the existing priority chain right after the
  `FunctionPointer` check and before `Reference`/`Pointer` — so `int* arr[5]`
  (an array of pointers) is classified `Array`, not `Pointer`, since it still
  decays and still loses its element count. `parameterDeclaredType()` is
  **not** changed — parameter `declaredType` keeps meaning "base type only,"
  exactly as it already does for pointers and references today.
- **Rule:** one new `Rule` implementation, `ArrayParameterDecayRule`,
  registered in `codeqa::rules::allRules`, alongside the existing 10 rules.
  One finding per array-shaped parameter (not aggregated per function) —
  decided explicitly during brainstorming, matching
  `security.unsafe-string-api`'s "one finding per occurrence" shape rather
  than `signature.consecutive-booleans`'s aggregate-into-one shape.
- One new config entry, `memory.array-parameter-decay`, in
  `codeqa::config::defaultConfig()` — same `thresholds(0, 0)` pattern as the
  two prior non-threshold rules.
- Desktop GUI: hide the warning/high limit fields for this rule id too.
- All 5 known hardcoded rule-id/count surfaces updated up front (not
  discovered mid-implementation, unlike the previous two slices — see §6).

### Explicitly out of scope (deferred, not forgotten)

- **Macros.** `FileModel` still does not model `#define` at all — unchanged
  from the previous two slices' scope notes.
- **Local or global array variables.** An array *local*/*global* does not
  decay (it only decays when passed as a function argument or otherwise
  used in a pointer context) — there is no equivalent bug to flag by mere
  declaration, unlike `memory.uninitialized-pointer`'s local-pointer case.
  Not attempted here.
- **Per-parameter source location.** `ParameterModel` has no `SourceRange`
  of its own — no parameter-level rule has one today
  (`signature.parameter-count`, `signature.consecutive-booleans` both use
  `CallableModel::parameterListRange` as a shared fallback location). This
  rule follows the same existing convention rather than extending the model
  to add per-parameter ranges, which is a larger, unrelated change.
- **Multi-dimensional arrays, arrays-of-function-pointers.** These are
  structurally handled by the same `array_declarator` check (tree-sitter
  nests declarators, and `findDescendantOfType` finds the outermost match in
  a preorder walk) without any special-casing, but are not separately
  tested beyond one representative case — same precision level as the
  existing pointer/reference/function-pointer classification, which has
  never enumerated every nesting shape either.
- Any change to `Rule::evaluate`'s signature, `RuleContext`, or any of the
  10 existing rules' behavior. Any change to `parameterDeclaredType()`'s
  text-extraction logic.

## 3. Architecture

```text
Frontend (src/frontend/StructuralExtractor.cpp, parametersFor):
  parameter's declarator
        |
        v
  isFunctionPointerDeclarator(declarator)            -> FunctionPointer
  else declarator contains array_declarator /
       abstract_array_declarator                     -> Array   (NEW)
  else declarator contains reference_declarator /
       abstract_reference_declarator                 -> Reference
  else declarator contains pointer_declarator /
       abstract_pointer_declarator                    -> Pointer
  else                                                -> Value
        |
        v
ParameterModel::passing == ParameterPassing::Array
        |
        v
ArrayParameterDecayRule::evaluate(RuleContext)   (new, src/rules/Rules.cpp)
        |  for each context.callable.parameters entry with
        |  passing == ParameterPassing::Array
        v
std::vector<Finding>  — one per matching parameter, ruleId =
                         "memory.array-parameter-decay", severity = Warning,
                         location = parameterListRange->begin (or the
                         function's own start if absent), observedValue/
                         limitValue = std::nullopt
```

No other layer changes — same optional `observedValue`/`limitValue` path
already exercised twice (`security.unsafe-string-api`,
`memory.uninitialized-pointer`).

## 4. Frontend change

```cpp
// include/codeqa/model/Models.h
enum class ParameterPassing
{
    Value,
    Pointer,
    Reference,
    FunctionPointer,
    Array
};
```

```cpp
// src/frontend/StructuralExtractor.cpp, parametersFor()
model.passing = isFunctionPointerDeclarator(declarator)
    ? ParameterPassing::FunctionPointer
    : (!ts_node_is_null(findDescendantOfType(declarator, "array_declarator"))
              || !ts_node_is_null(
                  findDescendantOfType(declarator, "abstract_array_declarator"))
            ? ParameterPassing::Array
            : (!ts_node_is_null(findDescendantOfType(declarator, "reference_declarator"))
                      || !ts_node_is_null(
                          findDescendantOfType(declarator, "abstract_reference_declarator"))
                    ? ParameterPassing::Reference
                    : (!ts_node_is_null(findDescendantOfType(declarator, "pointer_declarator"))
                              || !ts_node_is_null(
                                  findDescendantOfType(declarator, "abstract_pointer_declarator"))
                            ? ParameterPassing::Pointer
                            : ParameterPassing::Value)));
```

`array_declarator`/`abstract_array_declarator` are existing node types in
the vendored `tree-sitter-c`/`tree-sitter-cpp` grammars (confirmed present
in both `third_party/tree-sitter-c/src/parser.c` and
`third_party/tree-sitter-cpp/src/parser.c`'s generated tables, and
`array_declarator` is already referenced elsewhere in this same file's
`isDirectDeclarator` helper) — this is a new *detection*, not a new grammar
node.

Both a sized (`int arr[10]`) and unsized (`int arr[]`) array declarator
produce an `array_declarator` node with an empty or present `size` field
respectively — both decay identically in C/C++ semantics, so both match
with no size-based branching needed.

## 5. Rule implementation

```cpp
// src/rules/Rules.cpp, alongside the existing rule classes

class ArrayParameterDecayRule : public Rule
{
public:
    [[nodiscard]] std::string id() const override { return "memory.array-parameter-decay"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        std::vector<Finding> findings;

        const codeqa::core::SourceLocation location = context.callable.parameterListRange.has_value()
            ? context.callable.parameterListRange->begin
            : context.callable.symbol.range.begin;

        for (const codeqa::model::ParameterModel& parameter : context.callable.parameters)
        {
            if (parameter.passing != codeqa::model::ParameterPassing::Array)
            {
                continue;
            }

            const std::string subject = parameter.name.empty()
                ? "an unnamed parameter"
                : "Parameter '" + parameter.name + "'";

            Finding finding;
            finding.ruleId = id();
            finding.severity = codeqa::core::DiagnosticSeverity::Warning;
            finding.description = subject
                + " is declared as an array, which decays to a pointer and loses its size information.";
            finding.recommendation =
                "Pass the array's size explicitly as a separate parameter, or use a type "
                "that carries its own size (e.g. std::span, std::array&, or a fixed-size struct).";
            finding.location = location;
            finding.symbolName = context.callable.symbol.qualifiedName;
            findings.push_back(std::move(finding));
        }

        return findings;
    }
};
```

Note the unnamed-parameter branch produces the sentence "an unnamed
parameter is declared as an array..." (lowercase "an", since it's the
grammatical subject) versus "Parameter 'arr' is declared as an array..."
for a named one — both read as complete sentences.

Registered in `allRules`:

```cpp
if (const codeqa::config::RuleConfig& c = config.rules.at("memory.array-parameter-decay"); c.enabled)
{
    rules.push_back(std::make_unique<ArrayParameterDecayRule>());
}
```

## 6. Config and registration surface

```cpp
// src/config/DefaultConfig.cpp
config.rules["memory.array-parameter-decay"] = thresholds(0, 0);
```

The full, now-known set of touch points (5 backend + 3 desktop, all found
by direct repo inspection before writing the plan, not discovered
mid-implementation this time):

1. `src/config/DefaultConfig.cpp` — new `config.rules[...]` line.
2. `src/config/ConfigJson.cpp::knownRuleIds()` — append the id.
3. `src/desktop/ReportModels.cpp::orderedRuleIds()` — append the id.
4. `include/codeqa/rules/Rules.h` doc comment — 10 → 11.
5. `tests/unit/ReportJsonTest.cpp` — the rule-count assertion this exact
   class of change has needed twice now (9→10 for slice 1, 10→11 here);
   found only during slice 2's implementation, now known up front.
6. `src/desktop/qml/pages/AnalyzePage.qml` — extend the `visible` condition
   on both limit fields with a third exclusion.
7. `tests/desktop/AppControllerTest.cpp` — both `rowCount()` call sites,
   plus `expectedRuleIds`.
8. `tests/unit/DefaultConfigTest.cpp` — count + new `checkRule` line.

## 7. Testing and verification strategy

- `tests/unit/StructuralExtractorTest.cpp`: extend the existing
  `"derives C++ parameter passing from the declarator only"` test (or add a
  sibling test) with array parameter cases: `int arr[]` → `Array`;
  `int arr[10]` → `Array`; `int* arr[5]` (array of pointers) → `Array`, not
  `Pointer`; a plain `int` parameter stays `Value` (regression guard).
- `tests/unit/RulesTest.cpp`: new test cases for `memory.array-parameter-decay`
  covering, on `CallableModel::parameters` entries in one `evaluate` call:
  - A named array parameter → 1 finding naming the parameter, `ruleId ==
    "memory.array-parameter-decay"`, `severity == Warning`,
    `observedValue`/`limitValue` both `std::nullopt`.
  - An unnamed array parameter → 1 finding using the "an unnamed parameter"
    phrasing.
  - A `Pointer`/`Reference`/`FunctionPointer`/`Value` parameter → no finding.
  - Two array parameters in the same function → 2 findings, both at the
    same `location` (the shared `parameterListRange`), each naming its own
    parameter.
  - No `parameterListRange` set → location falls back to the function's own
    `symbol.range.begin` (mirrors `signature.parameter-count`'s existing
    fallback test).
- `tests/unit/DefaultConfigTest.cpp`: `rules.size()` 10 → 11, plus
  `checkRule(config, "memory.array-parameter-decay", 0, 0)`.
- `tests/unit/CorpusReportTest.cpp`: grep `tests/corpus/` fixtures for any
  array-shaped parameter before assuming no change is needed; update the
  affected fixture's expected finding count if found. Confirmed by running
  the full test suite at the end either way.
- `tests/corpus/validation/RULE-STABILITY.md`: new row,
  `memory.array-parameter-decay | experimental | — | —`.

## 8. Acceptance criteria

- A function with an array-shaped parameter (sized, unsized, or an array of
  pointers) produces one `Warning`-severity finding per such parameter,
  naming the parameter (or noting it's unnamed) in the description.
- A pointer, reference, function-pointer, or plain-value parameter produces
  no finding from this rule.
- The rule is enabled by default and appears in the desktop GUI's rule list
  without a functioning-but-meaningless pair of limit fields.
- No change to any of the 10 existing rules' behavior, to `Rule::evaluate`'s
  signature, to `parameterDeclaredType()`'s output, or to any CLI command's
  output shape beyond the new rule's own findings appearing when triggered.
- Full existing test suite (unit + desktop) still passes.
