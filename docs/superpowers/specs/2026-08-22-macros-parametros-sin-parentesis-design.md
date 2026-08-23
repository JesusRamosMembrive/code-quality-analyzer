# Reglas avanzadas de C: parámetros de macro sin paréntesis — Design

**Date:** 2026-08-22
**Status:** approved for specification review
**Track:** post-MVP, roadmap v2 track #3 (cuarta y última vuelta) — see `docs/02-roadmap-v2.md`.

## 1. Goal

Add a fourth and final rule to track #3: `preprocessor.unparenthesized-macro-parameter`,
which flags a function-like macro (`#define NAME(params) body`) that uses one
of its parameters in the body without fully parenthesizing that specific
occurrence — the classic C preprocessor bug where `#define SQUARE(x) x*x`
expands `SQUARE(a+b)` to `a+b*a+b` instead of `(a+b)*(a+b)`.

Unlike the previous three slices, this one cannot be built as a new `Rule`
at all: `Rule::evaluate(RuleContext{metrics, callable})` only ever sees one
function at a time, and a macro is not inside any function — `FileModel`
does not model `#define` in any form today. This slice therefore does two
things at once: it teaches the frontend about macros, and it adds a new,
smaller-scoped **file-level rule** mechanism (`FileRule`) so a macro finding
can participate in the exact same findings/suppression/report pipeline as
every other rule, rather than living in a side channel like track #4's
architecture layer does.

## 2. Scope and boundaries

### In scope

- **Frontend:** a new `frontend::MacroModel` (name, whether function-like,
  parameter names, whether variadic, raw body text, source range), extracted
  via two new tree-sitter queries (`queries/c/macros.scm`,
  `queries/cpp/macros.scm`) matching `preproc_def` (object-like,
  `#define NAME value`) and `preproc_function_def` (function-like,
  `#define NAME(params) value`). Both node types and their `name`/
  `parameters`/`value` fields are confirmed present in the vendored
  `tree-sitter-c`/`tree-sitter-cpp` grammars (`sym_preproc_def`,
  `sym_preproc_function_def`, `sym_preproc_params`, `field_name`,
  `field_parameters`, `field_value` all exist in both parsers' generated
  tables). The macro body (`value` field, a `preproc_arg` node) is an
  **opaque raw-text token** in this grammar — tree-sitter does not parse a
  macro's replacement list as C++ at all — so detection is necessarily a
  text scan over that string, the same style already used by
  `security.unsafe-string-api`'s call-text checks, not a new category of
  analysis for this codebase.
- **Model + report:** `model::MacroModel` (mirrors the frontend one, using
  `SymbolIdentity` like `GlobalVariableModel`/`TypeAliasModel` do), a
  `FileModel::macros` vector, bridging in `ProjectModelBuilder.cpp`, and
  JSON serialization in `FileJson.cpp` (`document["macros"]`), following the
  exact existing pattern for `globalVariables`/`typeAliases`.
- **New mechanism:** `codeqa::rules::FileRule` — a second rule interface,
  parallel to `Rule`, evaluated once per file against a `FileRuleContext{
  const FileModel& file}` instead of once per callable. `ApplyRules.cpp`
  gains a second, file-scoped evaluation loop alongside its existing
  per-callable one. File-rule findings flow into the same
  `FileFindingsModel::findings`/`suppressedFindings`, go through the exact
  same `// codeqa-disable-next-line <rule-id>` suppression check (a macro
  finding's location is a real, single line — the `#define` line — so
  suppression works exactly as it does for every other rule), and appear in
  the same JSON report and desktop rule list. This is a reusable mechanism,
  not a one-off hack scoped to this single rule.
- **Rule:** one `FileRule` implementation, `UnparenthesizedMacroParameterRule`,
  registered in a new `allFileRules(config)` factory (parallel to
  `allRules`). One finding per macro parameter that has **at least one**
  unsafely-used occurrence in the body — not per occurrence, not per macro —
  matching this track's established "one finding per at-risk item" shape.
  Only function-like macros are checked; an object-like macro
  (`#define NAME value`, no parameter list) has nothing to check and is
  skipped outright.
- **Precision-focused detection heuristic** (see §7): a parameter occurrence
  is *safe* when the nearest non-space character before it is one of
  `( [ ,` (or the start of the body) **and** the nearest non-space character
  after it is one of `) ] ,` (or the end of the body) — this treats a sole
  call argument (`LOG(x)`, where `x` sits directly inside `(...)`) and a
  sole subscript index (`buffer[x]`, where `x` sits directly inside
  `[...]`) as safe, not just a bare `(x)`, because both are extremely
  common, legitimate C idioms that a naive "must be immediately wrapped in
  its own parens" check would false-positive on. A parameter is flagged if
  it has at least one occurrence that is *not* safe by this test (e.g.
  `x*x`, `x+y`, `-x` — and, correctly, a parameter used as the array
  operand of a subscript, `arr[i]`, since `arr` is followed by `[` rather
  than enclosed by it: `AT(a+b, i)` expanding to `a+b[i]` really does
  parse wrong, because `[]` binds tighter than `+`).
- One new config entry, `preprocessor.unparenthesized-macro-parameter`, in
  `codeqa::config::defaultConfig()` — same `thresholds(0, 0)` pattern as the
  three prior non-threshold rules.
- Desktop GUI: hide the warning/high limit fields for this rule id too (a
  4th exclusion clause).
- All known registration-surface touch points updated up front (see §8) —
  this slice both extends the existing 5-6-surface "rule-id list" tax *and*
  introduces a second, smaller factory (`allFileRules`) that the same
  surfaces need to account for, since `config.rules` is a single flat map
  shared by both `Rule` and `FileRule` ids.

### Explicitly out of scope (deferred, not forgotten)

- **Whole-macro-body parenthesization.** `#define MIN(a,b) a<b?a:b` used as
  `2*MIN(a,b)` can also break, independent of whether each individual
  parameter is wrapped — this is a related but separate check (does the
  *entire* expansion need wrapping, not just each parameter), explicitly
  deferred during brainstorming in favor of shipping the narrower,
  well-understood parameter check first.
- **Multi-statement macros without a `do { ... } while (0)` guard.** A
  different bug category (statement-level, not expression-precedence) than
  what this rule checks. Not attempted here.
- **`__VA_ARGS__` / variadic parameters.** A macro's `...` parameter is
  recorded (`MacroModel::isVariadic`) but is not itself checked for
  unparenthesized use via `__VA_ARGS__` in the body — the rule only walks
  `MacroModel::parameters` (named parameters), which never includes `...`.
- **Occurrences inside string/char literals in the macro body.** The
  detection is a plain text/word-boundary scan over the raw body string; it
  does not track whether a matching identifier is actually inside a string
  or char literal token (e.g. `#define LOG(x) printf("x=%d\n", x)` — the
  literal `"x=%d\n"` happens to contain the letter sequence `x`, but at a
  word-boundary check it would only misfire if `x` appears there as an
  isolated word, which is rare in practice; still a known, undetected
  false-positive class, consistent with this track's precedent of
  documenting rather than eliminating every edge case for a first slice).
- **Macros used only as flags/constants** (`#define MAX_SIZE 128`) — object
  like macros have no parameters, so they are structurally exempt from this
  rule; a "prefer `constexpr`/`const` over object-like macro" style rule is
  a different, unrelated check, not part of this slice.
- **Any change to `Rule::evaluate`'s signature, `RuleContext`, or any of the
  11 existing rules' behavior.** `FileRule` is strictly additive.
- Any macro-related rule beyond this one. `FileModel::macros` is designed to
  be reused by a future slice (e.g. redefinition checks, unused-macro
  checks), but no second macro rule is built now — YAGNI.

## 3. Architecture

```text
Frontend (src/frontend/StructuralExtractor.cpp):
  New query: [(preproc_def) (preproc_function_def)] @macro
        |
        v
  adaptMacro(candidate, source, path)
        |  name        <- childForField(candidate, "name")
        |  isFunctionLike <- isNodeType(candidate, "preproc_function_def")
        |  parameters/isVariadic <- walk childForField(candidate, "parameters")
        |                           (only when isFunctionLike)
        |  body        <- childForField(candidate, "value") text, or "" if absent
        |  range       <- sourceRange(candidate, path)
        v
ParsedFile::macros  (new vector, sorted/deduped by range, same as includes)
        |
        v
ProjectModelBuilder.cpp: frontend::MacroModel -> model::MacroModel
  (flat SymbolIdentity{name, name, range} -- macros are never
   namespace/class-scoped, unlike globals/types, so no scope walk)
        |
        v
FileModel::macros  (new vector)
        |                                   \
        v                                    v
FileJson.cpp: document["macros"]      ApplyRules.cpp: new file-level loop
  (macroJson(), mirrors                 FileRuleContext{file}
   globalVariableJson())                      |
                                               v
                          UnparenthesizedMacroParameterRule::evaluate
                            (new, src/rules/Rules.cpp, implements
                             the new FileRule interface)
                                  |  for each function-like macro,
                                  |  for each parameter with >=1
                                  |  unsafe occurrence in macro.body
                                  v
                          std::vector<Finding> -- one per flagged
                            parameter, ruleId =
                            "preprocessor.unparenthesized-macro-parameter",
                            severity = Warning, location =
                            macro.symbol.range.begin, observedValue/
                            limitValue = std::nullopt
                                  |
                                  v
                     same FileFindingsModel, same suppression check,
                     same JSON report, same desktop rule list as every
                     other rule (Rule- or FileRule-sourced)
```

## 4. Frontend change

```cpp
// include/codeqa/frontend/StructuralModels.h
struct MacroModel
{
    std::string name;
    bool isFunctionLike{false};
    std::vector<std::string> parameters;
    bool isVariadic{false};
    // Raw, unparsed replacement-list text (tree-sitter never parses a
    // macro body as C++); empty if the macro has no value (`#define FLAG`).
    std::string body;
    core::SourceRange range;
};
```

```cpp
// include/codeqa/frontend/ParsedFile.h -- one new field
std::vector<MacroModel> macros;
```

```cpp
// src/frontend/queries/c/macros.scm  AND  src/frontend/queries/cpp/macros.scm
[(preproc_def) (preproc_function_def)] @macro
```

```cpp
// src/frontend/StructuralExtractor.cpp -- new adaptMacro(), alongside
// adaptInclude() and the other adapt* functions

MacroModel adaptMacro(TSNode candidate, std::string_view source, const std::filesystem::path& path)
{
    const TSNode nameNode = childForField(candidate, "name");

    MacroModel model;
    model.name = ts_node_is_null(nameNode) ? std::string{} : nodeText(nameNode, source);
    model.isFunctionLike = isNodeType(candidate, "preproc_function_def");

    if (model.isFunctionLike)
    {
        const TSNode parametersNode = childForField(candidate, "parameters");
        const uint32_t childCount = ts_node_child_count(parametersNode);

        for (uint32_t index = 0; index < childCount; ++index)
        {
            const TSNode child = ts_node_child(parametersNode, index);

            if (isNodeType(child, "identifier"))
            {
                model.parameters.push_back(nodeText(child, source));
            }
            else if (isNodeType(child, "..."))
            {
                model.isVariadic = true;
            }
        }
    }

    const TSNode valueNode = childForField(candidate, "value");
    model.body = ts_node_is_null(valueNode) ? std::string{} : trim(nodeText(valueNode, source));
    model.range = sourceRange(candidate, path);
    return model;
}
```

```cpp
// src/frontend/StructuralExtractor.cpp -- new pass in extract(), mirroring
// the includes pass exactly (new query accessor impl_->macrosFor(grammar),
// new cMacros_/cppMacros_ members in Impl, wired the same way as
// cIncludes_/cppIncludes_)

ts_query_cursor_exec(cursor.get(), impl_->macrosFor(grammar), root);

while (ts_query_cursor_next_match(cursor.get(), &match))
{
    for (uint16_t index = 0; index < match.capture_count; ++index)
    {
        result.macros.push_back(adaptMacro(match.captures[index].node, source, path));
    }
}

std::sort(
    result.macros.begin(), result.macros.end(),
    [](const MacroModel& left, const MacroModel& right) { return left.range < right.range; });
result.macros.erase(
    std::unique(
        result.macros.begin(), result.macros.end(),
        [](const MacroModel& left, const MacroModel& right) { return left.range == right.range; }),
    result.macros.end());
```

```cmake
# CMakeLists.txt -- 2 new entries in CODEQA_QUERY_FILES, 2 new file(READ ...)
${PROJECT_SOURCE_DIR}/src/frontend/queries/c/macros.scm
${PROJECT_SOURCE_DIR}/src/frontend/queries/cpp/macros.scm

file(READ ${PROJECT_SOURCE_DIR}/src/frontend/queries/c/macros.scm CODEQA_C_MACROS_QUERY)
file(READ ${PROJECT_SOURCE_DIR}/src/frontend/queries/cpp/macros.scm CODEQA_CPP_MACROS_QUERY)
```

```cpp
// cmake/EmbeddedQueries.h.in -- 2 new inline constexpr string_view blocks,
// same R"CQxx(...)CQxx" raw-string wrapping as the 8 existing ones
inline constexpr std::string_view cMacros = R"CQCM(
@CODEQA_C_MACROS_QUERY@
)CQCM";

inline constexpr std::string_view cppMacros = R"CQPM(
@CODEQA_CPP_MACROS_QUERY@
)CQPM";
```

A sized/unsized distinction (relevant to arrays) does not apply here; the
only structural branch is object-like vs. function-like, already captured
by `isNodeType(candidate, "preproc_function_def")`.

## 5. Model and report bridging

```cpp
// include/codeqa/model/Models.h
struct MacroModel
{
    SymbolIdentity symbol;
    bool isFunctionLike{false};
    std::vector<std::string> parameters;
    bool isVariadic{false};
    std::string body;
};
```

```cpp
// include/codeqa/model/Models.h -- FileModel gets one new field
std::vector<MacroModel> macros;
```

```cpp
// src/model/ProjectModelBuilder.cpp -- alongside the globalVariables/
// typeAliases bridging loops. Deliberately NOT using identityFor() (which
// walks enclosing namespace/class scopes): a macro is never
// namespace/class-scoped by the preprocessor, so qualifiedName == name
// always, unlike every other symbol kind bridged in this function.
for (const frontend::MacroModel& macro : file.macros)
{
    model.macros.push_back(MacroModel{
        SymbolIdentity{macro.name, macro.name, macro.range},
        macro.isFunctionLike,
        macro.parameters,
        macro.isVariadic,
        macro.body});
}
```

```cpp
// src/model/ProjectModelBuilder.cpp -- new sort, alongside the existing
// sortByRange calls for globalVariables/typeAliases
sortByRange(model.macros, [](const MacroModel& macro) -> const core::SourceRange& {
    return macro.symbol.range;
});
```

```cpp
// src/report/FileJson.cpp -- new macroJson(), mirrors globalVariableJson()
Json macroJson(const codeqa::model::MacroModel& macro)
{
    Json document = symbolIdentityJson(macro.symbol);
    document["isFunctionLike"] = macro.isFunctionLike;
    document["parameters"] = macro.parameters;
    document["isVariadic"] = macro.isVariadic;
    document["body"] = macro.body;
    return document;
}
```

```cpp
// src/report/FileJson.cpp -- new array in the per-file document, alongside
// document["globalVariables"] / document["typeAliases"]
document["macros"] = Json::array();
for (const codeqa::model::MacroModel& macro : file.macros)
{
    document["macros"].push_back(macroJson(macro));
}
```

## 6. New mechanism: file-level rules

```cpp
// include/codeqa/rules/FileRule.h -- new file, parallel to Rule.h
#pragma once

#include "codeqa/core/Language.h"
#include "codeqa/model/Models.h"
#include "codeqa/rules/RuleModels.h"

#include <string>
#include <vector>

namespace codeqa::rules
{

struct FileRuleContext
{
    const codeqa::model::FileModel& file;
};

class FileRule
{
public:
    virtual ~FileRule() = default;

    [[nodiscard]] virtual std::string id() const = 0;
    [[nodiscard]] virtual bool appliesTo(codeqa::core::Language language) const = 0;
    [[nodiscard]] virtual std::vector<Finding> evaluate(const FileRuleContext& context) const = 0;
};

} // namespace codeqa::rules
```

```cpp
// include/codeqa/rules/Rules.h -- new declaration, alongside allRules(),
// plus #include "codeqa/rules/FileRule.h"
[[nodiscard]] std::vector<std::unique_ptr<FileRule>> allFileRules(
    const codeqa::config::ProjectConfig& config);
```

```cpp
// src/application/ApplyRules.cpp -- new file-level loop inside the
// existing per-file loop, guarded by the same fileMetrics precondition the
// per-callable loop already requires (in practice always true: every file
// in project.files gets a FileMetricsModel entry unconditionally in
// MeasureProject.cpp, even a read failure only adds a diagnostic and still
// pushes one -- so this is not a new way for macro findings to go missing)

const std::vector<std::unique_ptr<codeqa::rules::FileRule>> fileRules =
    codeqa::rules::allFileRules(config);

// ... inside the existing `for (const codeqa::model::FileModel& file : project.files)`
// loop, after the fileMetrics null-check, before or after the per-callable loop:

const codeqa::rules::FileRuleContext fileContext{file};

for (const std::unique_ptr<codeqa::rules::FileRule>& fileRule : fileRules)
{
    if (!fileRule->appliesTo(file.language))
    {
        continue;
    }

    for (codeqa::rules::Finding& finding : fileRule->evaluate(fileContext))
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
}
```

`fileRules` is built once, outside the per-file loop, exactly like `rules`
already is (`codeqa::rules::allRules(config)` at the top of `execute()`).

## 7. Rule implementation

```cpp
// src/rules/Rules.cpp -- new helpers inside the existing anonymous
// namespace, alongside looksLikeRawPointer() and the other rule helpers

bool isWordBoundaryChar(char character)
{
    return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '_';
}

std::size_t findNextWholeWord(std::string_view text, std::string_view word, std::size_t from)
{
    for (std::size_t pos = text.find(word, from); pos != std::string_view::npos;
         pos = text.find(word, pos + 1))
    {
        const bool leftBoundary = pos == 0 || !isWordBoundaryChar(text[pos - 1]);
        const bool rightBoundary =
            pos + word.size() >= text.size() || !isWordBoundaryChar(text[pos + word.size()]);

        if (leftBoundary && rightBoundary)
        {
            return pos;
        }
    }

    return std::string_view::npos;
}

// Returns true as soon as any occurrence of `parameter` in `body` is not
// safely bracketed -- "safe" meaning the nearest non-space character before
// it is one of ( [ , (or body start) AND the nearest non-space character
// after it is one of ) ] , (or body end). Deliberately permissive: a sole
// call argument (LOG(x)) or sole subscript index (arr[x]) is safe, only a
// juxtaposition with an operator or another token is flagged.
bool hasUnwrappedOccurrence(std::string_view body, std::string_view parameter)
{
    for (std::size_t pos = findNextWholeWord(body, parameter, 0); pos != std::string_view::npos;
         pos = findNextWholeWord(body, parameter, pos + parameter.size()))
    {
        std::size_t before = pos;
        while (before > 0 && std::isspace(static_cast<unsigned char>(body[before - 1])) != 0)
        {
            --before;
        }

        std::size_t after = pos + parameter.size();
        while (after < body.size() && std::isspace(static_cast<unsigned char>(body[after])) != 0)
        {
            ++after;
        }

        const bool safeBefore =
            before == 0 || body[before - 1] == '(' || body[before - 1] == '[' || body[before - 1] == ',';
        const bool safeAfter =
            after >= body.size() || body[after] == ')' || body[after] == ']' || body[after] == ',';

        if (!safeBefore || !safeAfter)
        {
            return true;
        }
    }

    return false;
}
```

```cpp
// src/rules/Rules.cpp -- new rule class, implements FileRule not Rule
class UnparenthesizedMacroParameterRule : public FileRule
{
public:
    [[nodiscard]] std::string id() const override
    {
        return "preprocessor.unparenthesized-macro-parameter";
    }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const FileRuleContext& context) const override
    {
        std::vector<Finding> findings;

        for (const codeqa::model::MacroModel& macro : context.file.macros)
        {
            if (!macro.isFunctionLike)
            {
                continue;
            }

            for (const std::string& parameter : macro.parameters)
            {
                if (!hasUnwrappedOccurrence(macro.body, parameter))
                {
                    continue;
                }

                Finding finding;
                finding.ruleId = id();
                finding.severity = codeqa::core::DiagnosticSeverity::Warning;
                finding.description = "Parameter '" + parameter + "' of macro '" + macro.symbol.name
                    + "' is used without enclosing parentheses in the macro body.";
                finding.recommendation = "Wrap the parameter as (" + parameter
                    + ") everywhere it appears in the macro body to avoid operator-precedence "
                      "bugs at the call site.";
                finding.location = macro.symbol.range.begin;
                finding.symbolName = macro.symbol.qualifiedName;
                findings.push_back(std::move(finding));
            }
        }

        return findings;
    }
};
```

Registered in a new `allFileRules`, defined in `codeqa::rules` right after
`allRules`:

```cpp
std::vector<std::unique_ptr<FileRule>> allFileRules(const codeqa::config::ProjectConfig& config)
{
    std::vector<std::unique_ptr<FileRule>> rules;

    if (const codeqa::config::RuleConfig& c =
            config.rules.at("preprocessor.unparenthesized-macro-parameter");
        c.enabled)
    {
        rules.push_back(std::make_unique<UnparenthesizedMacroParameterRule>());
    }

    return rules;
}
```

## 8. Config and registration surface

```cpp
// src/config/DefaultConfig.cpp
config.rules["preprocessor.unparenthesized-macro-parameter"] = thresholds(0, 0);
```

Full touch-point list, found by direct repo inspection before writing the
plan (the same class of surfaces as every previous slice in this track,
now confirmed a 4th time):

**Rule-id-list surfaces:**
1. `src/config/DefaultConfig.cpp` — new `config.rules[...]` line.
2. `src/config/ConfigJson.cpp::knownRuleIds()` — append the id (12th entry).
3. `src/desktop/ReportModels.cpp::orderedRuleIds()` — append the id.
4. `include/codeqa/rules/Rules.h` doc comment — 11 → 12, and note that the
   count now spans both `allRules()` and `allFileRules()` ids together,
   since `config.rules` is one flat map for both.
5. `tests/unit/ReportJsonTest.cpp` — the rule-count assertion, 11 → 12.
6. `src/desktop/qml/pages/AnalyzePage.qml` — extend both `warningLimitField`/
   `highLimitField` `visible` conditions with a 4th exclusion clause.
7. `tests/desktop/AppControllerTest.cpp` — both `rowCount()` call sites
   (11 → 12), plus `expectedRuleIds`.
8. `tests/unit/DefaultConfigTest.cpp` — `rules.size()` 11 → 12, plus a new
   `checkRule(config, "preprocessor.unparenthesized-macro-parameter", 0, 0)`.

**New-mechanism surfaces (first time these exist):**
9. `include/codeqa/rules/FileRule.h` — new file, the `FileRule` interface.
10. `include/codeqa/rules/Rules.h` — `allFileRules()` declaration.
11. `src/rules/Rules.cpp` — `allFileRules()` implementation.
12. `src/application/ApplyRules.cpp` — the new file-level evaluation loop.

**New-model surfaces (first time `MacroModel` exists):**
13. `include/codeqa/frontend/StructuralModels.h` — `MacroModel` struct.
14. `include/codeqa/frontend/ParsedFile.h` — `macros` vector.
15. `include/codeqa/model/Models.h` — `MacroModel` struct + `FileModel::macros`.
16. `src/model/ProjectModelBuilder.cpp` — bridging loop + sort.
17. `src/report/FileJson.cpp` — `macroJson()` + `document["macros"]`.
18. `src/frontend/queries/{c,cpp}/macros.scm` — new query files.
19. `CMakeLists.txt` — `CODEQA_QUERY_FILES` + 2 `file(READ ...)` calls.
20. `cmake/EmbeddedQueries.h.in` — 2 new `inline constexpr` blocks.
21. `src/frontend/StructuralExtractor.cpp` — `adaptMacro()`, `Impl::macrosFor`,
    new `cMacros_`/`cppMacros_` members, new extraction pass in `extract()`.

## 9. Testing and verification strategy

- `tests/unit/StructuralExtractorTest.cpp`: new cases —
  - Object-like macro (`#define MAX_SIZE 128`) → one `MacroModel`,
    `isFunctionLike == false`, `parameters` empty, `body == "128"`.
  - Function-like macro with parameters
    (`#define SQUARE(x) x*x`) → `isFunctionLike == true`,
    `parameters == {"x"}`, `body == "x*x"`.
  - Variadic macro (`#define LOG(fmt, ...) printf(fmt, __VA_ARGS__)`) →
    `parameters == {"fmt"}`, `isVariadic == true`.
  - Macro with no value (`#define FLAG`) → `body` empty, no crash.
- `tests/unit/ProjectModelBuilderTest.cpp`: one new case confirming
  `frontend::MacroModel` → `model::MacroModel` bridging, and that
  `symbol.qualifiedName == symbol.name` (no scope prefix, unlike a global
  variable declared inside a namespace).
- `tests/unit/FileJsonTest.cpp`: one new case confirming a macro serializes
  to `document["macros"]` with the expected shape.
- New `tests/unit/FileRulesTest.cpp` (new file — no existing test file
  covers `FileRule`/`FileRuleContext`), covering
  `preprocessor.unparenthesized-macro-parameter` on hand-built `FileModel`
  instances:
  - `#define SQUARE(x) x*x` → 1 finding naming `x` and `SQUARE`.
  - `#define SQUARE(x) ((x)*(x))` → no finding (both occurrences safe).
  - `#define LOG(x) printf(x)` → no finding (sole call argument is safe).
  - `#define AT(i) buffer[i]` → no finding (sole subscript index is safe).
  - `#define AT(arr, i) arr[i]` → 1 finding, for `arr` only (`arr` is
    followed by `[` rather than enclosed by it, so it is not safe; `i` is
    enclosed by `[...]` and is safe) — the case that motivated writing the
    heuristic asymmetrically instead of just checking for any bracket on
    either side.
  - `#define ADD(a, b) a+b` → 2 findings, one per parameter.
  - `#define MAX_SIZE 128` (object-like) → no finding (not function-like).
  - `#define UNUSED(x) 0` (parameter never appears in body) → no finding.
  - Two macros in the same file, only one unsafe → exactly 1 finding total.
- `tests/unit/ApplyRulesTest.cpp`: extend to cover the new file-level loop
  end-to-end — a project with an unsafe macro produces a finding in
  `FileFindingsModel::findings`; a `// codeqa-disable-next-line
  preprocessor.unparenthesized-macro-parameter` comment immediately above
  the `#define` line suppresses it into `suppressedFindings`, mirroring the
  existing per-callable suppression test.
- `tests/unit/DefaultConfigTest.cpp`: `rules.size()` 11 → 12, plus the new
  `checkRule` line.
- `tests/unit/ReportJsonTest.cpp`: rule-count assertion 11 → 12.
- `tests/unit/CorpusReportTest.cpp`: grep `tests/corpus/` fixtures for any
  unsafe macro pattern before assuming no change is needed; update the
  affected fixture's expected finding count if found. Confirmed by running
  the full test suite at the end either way.
- `tests/corpus/validation/RULE-STABILITY.md`: new row,
  `preprocessor.unparenthesized-macro-parameter | experimental | — | —`.
- Desktop: `tests/desktop/AppControllerTest.cpp` rowCount 11 → 12,
  `expectedRuleIds` appended; manual/automated check that the new rule row
  hides its limit fields (matching the existing gap noted for all three
  prior pattern rules — no QML test exercises this binding for any of
  them, not newly introduced here).

## 10. Acceptance criteria

- A function-like macro with a parameter used without safe bracketing
  anywhere in its body produces one `Warning`-severity finding per such
  parameter, naming both the parameter and the macro.
- A function-like macro where every parameter occurrence is either fully
  parenthesized, a sole call argument, or a sole subscript index produces
  no finding.
- An object-like macro (no parameters) never produces a finding from this
  rule.
- The rule is enabled by default, appears in the desktop GUI's rule list
  without functioning-but-meaningless limit fields, and its findings can be
  suppressed with `// codeqa-disable-next-line
  preprocessor.unparenthesized-macro-parameter` exactly like any other
  rule's findings.
- `FileRule`/`allFileRules`/the new `ApplyRules.cpp` loop introduce no
  change to `Rule`, `RuleContext`, `allRules`, or any of the 11 existing
  rules' behavior.
- Full existing test suite (unit + desktop) still passes.
