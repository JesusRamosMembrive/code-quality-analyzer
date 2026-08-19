# Unsafe C String API Rule Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `security.unsafe-string-api`, a new pattern-based (non-threshold) rule that flags calls to `gets`/`strcpy`/`strcat`/`sprintf`/`vsprintf`, and hide the meaningless warning/high-limit fields this rule leaves in the desktop GUI's rule list.

**Architecture:** One new `Rule` subclass in `src/rules/Rules.cpp` scans each callable's already-extracted `CallModel` list for named calls matching a 5-entry blocklist, producing one `Finding` per hit with a function-specific recommendation. Registered through the existing `allRules`/`defaultConfig` machinery exactly like the 8 threshold rules already there — no new abstractions, no changes to `Rule::evaluate`'s signature or to any other rule.

**Tech Stack:** C++17, doctest (existing `codeqa_tests` executable), Qt6/QML (`AnalyzePage.qml`), CMake (MSVC `build/` for `codeqa_tests`, MinGW `buildQt/` for the desktop app and its QML smoke test).

**Spec:** `docs/superpowers/specs/2026-08-19-reglas-c-api-insegura-design.md`

## Global Constraints

- Rule id, exact string: `security.unsafe-string-api`.
- Blocklist and exact recommendation text (verbatim from the spec):
  - `gets` → `"gets was removed from C11; use fgets(buf, size, stream) instead."`
  - `strcpy` → `"Use strncpy or strlcpy (bounds-checked) instead."`
  - `strcat` → `"Use strncat or strlcat (bounds-checked) instead."`
  - `sprintf` → `"Use snprintf instead, which takes a buffer size."`
  - `vsprintf` → `"Use vsnprintf instead, which takes a buffer size."`
- Finding description format, exact: `"Call to unsafe function '<name>'."` where `<name>` is the text after the last `"::"` in `CallModel::calleeText` (so `std::strcpy` reports `strcpy`).
- Severity is always `codeqa::core::DiagnosticSeverity::Warning` — no High tier, no severity computed from any limit.
- Only calls with `kind == codeqa::model::CallKind::Named` are considered (excludes `CallKind::Member` and `CallKind::Indirect`).
- `Finding::observedValue` and `Finding::limitValue` stay `std::nullopt` — this rule has no numeric threshold.
- `appliesTo(codeqa::core::Language)` returns `true` unconditionally (applies to C and C++).
- Default config entry: `enabled = true`, `warningLimit = 0`, `highLimit = 0` — both limit values exist only because `RuleConfig` requires them; the rule never reads them.
- Desktop GUI: hide `warningLimitField` and `highLimitField` specifically when `ruleRow.model.id === "security.unsafe-string-api"` — no other row's fields change.
- Out of scope, do not add in this plan: `scanf`/`fscanf`/`sscanf`/`strtok`, any pointer/array/macro detection, any memory-leak heuristic, any change to `Rule::evaluate`'s signature or to any of the 8 existing rules.

---

### Task 1: Implement and register the `security.unsafe-string-api` rule

**Files:**
- Modify: `src/rules/Rules.cpp` (new rule class + registration)
- Modify: `include/codeqa/rules/Rules.h:12-16` (doc comment: 8 → 9 known rule ids)
- Modify: `src/config/DefaultConfig.cpp` (new default config entry)
- Modify: `tests/unit/RulesTest.cpp` (two new test cases)
- Modify: `tests/unit/DefaultConfigTest.cpp` (count + new rule assertion)
- Modify: `tests/corpus/validation/RULE-STABILITY.md` (new table row)

**Interfaces:**
- Consumes: `codeqa::model::CallModel{calleeText, kind, argumentCount, range}`, `codeqa::model::CallKind::Named` (`include/codeqa/model/Models.h`, already visible via `codeqa/rules/Rule.h`); `codeqa::rules::RuleContext{metrics, callable}` and `codeqa::rules::Finding` (`include/codeqa/rules/Rule.h`, `RuleModels.h`); `codeqa::config::RuleConfig` and the local `thresholds(warningLimit, highLimit)` helper already defined in `src/config/DefaultConfig.cpp`.
- Produces: rule id string `"security.unsafe-string-api"`, registered in `codeqa::rules::allRules(...)` — Task 2's GUI change matches against this exact string.

- [ ] **Step 1: Write the failing tests**

Open `tests/unit/RulesTest.cpp`. It currently ends at line 90 with the closing brace of the `size.local-variables` test case, right after this helper already present near the top of the file:

```cpp
codeqa::model::CallableModel makeCallable()
{
    codeqa::model::CallableModel callable;
    callable.symbol.name = "sample";
    callable.symbol.qualifiedName = "Sample::sample";
    callable.symbol.range.begin.line = 10;
    callable.symbol.range.begin.column = 1;
    return callable;
}
```

Add a second helper right after `makeCallable()` (still inside the anonymous namespace, before its closing `}`), and append two new `TEST_CASE`s at the end of the file:

```cpp
codeqa::model::CallModel makeCall(std::string calleeText, codeqa::model::CallKind kind)
{
    codeqa::model::CallModel call;
    call.calleeText = std::move(calleeText);
    call.kind = kind;
    return call;
}
```

```cpp
TEST_CASE("security.unsafe-string-api flags named calls to banned functions, including std:: qualified ones")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules =
        codeqa::rules::allRules(codeqa::config::defaultConfig());
    const codeqa::rules::Rule& rule = ruleNamed(rules, "security.unsafe-string-api");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;

    callable.calls.push_back(makeCall("strcpy", codeqa::model::CallKind::Named));
    callable.calls.push_back(makeCall("std::strcpy", codeqa::model::CallKind::Named));
    callable.calls.push_back(makeCall("strcpy", codeqa::model::CallKind::Member));
    callable.calls.push_back(makeCall("strncpy", codeqa::model::CallKind::Named));

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});

    REQUIRE(findings.size() == 2);
    CHECK(findings[0].ruleId == "security.unsafe-string-api");
    CHECK(findings[0].severity == codeqa::core::DiagnosticSeverity::Warning);
    CHECK(findings[0].description == "Call to unsafe function 'strcpy'.");
    CHECK(findings[0].recommendation == "Use strncpy or strlcpy (bounds-checked) instead.");
    CHECK_FALSE(findings[0].observedValue.has_value());
    CHECK_FALSE(findings[0].limitValue.has_value());
    CHECK(findings[0].symbolName == "Sample::sample");
    CHECK(findings[1].description == "Call to unsafe function 'strcpy'.");
}

TEST_CASE("security.unsafe-string-api produces no findings for calls outside its blocklist")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules =
        codeqa::rules::allRules(codeqa::config::defaultConfig());
    const codeqa::rules::Rule& rule = ruleNamed(rules, "security.unsafe-string-api");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;

    callable.calls.push_back(makeCall("strncpy", codeqa::model::CallKind::Named));
    callable.calls.push_back(makeCall("my_strcpy", codeqa::model::CallKind::Named));

    CHECK(rule.evaluate(codeqa::rules::RuleContext{metrics, callable}).empty());
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="security.unsafe-string-api*"
```
Expected: FAIL — `ruleNamed` throws `std::runtime_error("Expected rule was not registered: security.unsafe-string-api")`, because `allRules` does not produce this rule yet.

- [ ] **Step 3: Implement the rule**

In `src/rules/Rules.cpp`, add `<array>` and `<string_view>` to the includes at the top (the file currently has `<algorithm>`, `<memory>`, `<optional>`, `<string>`, `<utility>`, `<vector>` — add the two new ones alongside them, keeping alphabetical order):

```cpp
#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
```

Then, inside the anonymous namespace, after the existing `NestingDepthRule` class definition and before the namespace's closing `}` (i.e., directly above the line `} // namespace` that currently precedes `std::vector<std::unique_ptr<Rule>> allRules(...)`), add:

```cpp
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
    return separator == std::string_view::npos ? calleeText : calleeText.substr(separator + 2);
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

In `allRules(...)`, add this block right before the function's final `return rules;`:

```cpp
    if (const codeqa::config::RuleConfig& c = config.rules.at("security.unsafe-string-api"); c.enabled)
    {
        rules.push_back(std::make_unique<UnsafeStringApiRule>());
    }

```

(Note this rule's constructor takes no arguments, unlike the 8 threshold rules — `c` is read only for `.enabled`.)

- [ ] **Step 4: Wire the default config and update the rule-count doc comment**

In `src/config/DefaultConfig.cpp`, add this line right before `return config;` in `defaultConfig()`:

```cpp
    config.rules["security.unsafe-string-api"] = thresholds(0, 0);
```

In `include/codeqa/rules/Rules.h`, the doc comment above `allRules` currently reads:

```cpp
/// Requires config.rules to contain an entry for each of the 8 known rule ids --
```

Change `8` to `9`:

```cpp
/// Requires config.rules to contain an entry for each of the 9 known rule ids --
```

- [ ] **Step 5: Run the new tests to verify they pass**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="security.unsafe-string-api*"
```
Expected: PASS (2 test cases, 0 failures).

- [ ] **Step 6: Update `DefaultConfigTest.cpp` and verify**

`tests/unit/DefaultConfigTest.cpp` currently reads, in full:

```cpp
#include "doctest/doctest.h"

#include "codeqa/config/DefaultConfig.h"

#include <string>

namespace
{

void checkRule(
    const codeqa::config::ProjectConfig& config, const std::string& id, long long warning, long long high)
{
    REQUIRE(config.rules.count(id) == 1);
    const codeqa::config::RuleConfig& rule = config.rules.at(id);
    CHECK(rule.enabled);
    CHECK(rule.warningLimit == warning);
    CHECK(rule.highLimit == high);
}

} // namespace

TEST_CASE("defaultConfig ships the eight Fase 7 thresholds, all enabled, no header overrides")
{
    const codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();

    CHECK(config.headerOverrides.empty());
    REQUIRE(config.rules.size() == 8);

    checkRule(config, "size.function-length", 50, 90);
    checkRule(config, "size.local-variables", 8, 12);
    checkRule(config, "signature.parameter-count", 4, 7);
    checkRule(config, "signature.consecutive-booleans", 2, 3);
    checkRule(config, "complexity.cyclomatic", 10, 15);
    checkRule(config, "complexity.returns", 4, 6);
    checkRule(config, "complexity.condition", 3, 5);
    checkRule(config, "complexity.nesting", 3, 5);
}
```

Replace the `TEST_CASE` line and its final two lines (the closing `checkRule` call and the closing brace) so the file ends with:

```cpp
TEST_CASE("defaultConfig ships the eight Fase 7 thresholds plus the unsafe-string-api rule, all enabled, no header overrides")
{
    const codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();

    CHECK(config.headerOverrides.empty());
    REQUIRE(config.rules.size() == 9);

    checkRule(config, "size.function-length", 50, 90);
    checkRule(config, "size.local-variables", 8, 12);
    checkRule(config, "signature.parameter-count", 4, 7);
    checkRule(config, "signature.consecutive-booleans", 2, 3);
    checkRule(config, "complexity.cyclomatic", 10, 15);
    checkRule(config, "complexity.returns", 4, 6);
    checkRule(config, "complexity.condition", 3, 5);
    checkRule(config, "complexity.nesting", 3, 5);
    checkRule(config, "security.unsafe-string-api", 0, 0);
}
```

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="defaultConfig*"
```
Expected: PASS.

- [ ] **Step 7: Add the corpus stability row**

In `tests/corpus/validation/RULE-STABILITY.md`, add a new row at the end of the table (after the `complexity.nesting` row). Do not touch the introductory paragraph's "8 reglas" sentence — that sentence describes the historical set of rules that existed before Fase 11 and stays accurate as written; only the table gains a row:

```markdown
| `security.unsafe-string-api` | `experimental` | — | — |
```

- [ ] **Step 8: Run the full test suite**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe
```
Expected: PASS, all test cases (0 failures) — in particular confirms `CorpusReportTest.cpp`'s `findings().size() == 1` assertion for `nesting/deep-nesting.cpp` still holds, since no fixture under `tests/corpus/` calls any of the 5 newly-blocked functions.

- [ ] **Step 9: Commit**

```bash
git add src/rules/Rules.cpp include/codeqa/rules/Rules.h src/config/DefaultConfig.cpp tests/unit/RulesTest.cpp tests/unit/DefaultConfigTest.cpp tests/corpus/validation/RULE-STABILITY.md
git commit -m "feat: add security.unsafe-string-api rule for unsafe C string/IO calls"
```

---

### Task 2: Hide the unused limit fields for this rule in the desktop GUI

**Files:**
- Modify: `src/desktop/qml/pages/AnalyzePage.qml:177-209`

**Interfaces:**
- Consumes: rule id string `"security.unsafe-string-api"` produced by Task 1 (must match exactly); the existing `ruleRow.model.id`, `warningLimitField`, `highLimitField` QML elements already in this file.
- Produces: nothing consumed by a later task — this is the final task in the plan.

- [ ] **Step 1: Edit the QML**

In `src/desktop/qml/pages/AnalyzePage.qml`, the rule row currently defines (lines 177-209):

```qml
                        TextField {
                            id: warningLimitField
                            Layout.preferredWidth: 140
                            text: ruleRow.model.warningLimit === undefined
                                ? ""
                                : ruleRow.model.warningLimit.toString()
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            validator: IntValidator {}
                            onEditingFinished: {
                                if (acceptableInput && highLimitField.acceptableInput
                                        && root.controller && ruleRow.model.id !== undefined) {
                                    root.controller.setRuleLimits(
                                        ruleRow.model.id, Number(text), Number(highLimitField.text))
                                }
                            }
                        }

                        TextField {
                            id: highLimitField
                            Layout.preferredWidth: 140
                            text: ruleRow.model.highLimit === undefined
                                ? ""
                                : ruleRow.model.highLimit.toString()
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            validator: IntValidator {}
                            onEditingFinished: {
                                if (acceptableInput && warningLimitField.acceptableInput
                                        && root.controller && ruleRow.model.id !== undefined) {
                                    root.controller.setRuleLimits(
                                        ruleRow.model.id, Number(warningLimitField.text), Number(text))
                                }
                            }
                        }
```

Add a `visible` binding to each `TextField` (as the first property, right after `id:`), so the block becomes:

```qml
                        TextField {
                            id: warningLimitField
                            visible: ruleRow.model.id !== "security.unsafe-string-api"
                            Layout.preferredWidth: 140
                            text: ruleRow.model.warningLimit === undefined
                                ? ""
                                : ruleRow.model.warningLimit.toString()
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            validator: IntValidator {}
                            onEditingFinished: {
                                if (acceptableInput && highLimitField.acceptableInput
                                        && root.controller && ruleRow.model.id !== undefined) {
                                    root.controller.setRuleLimits(
                                        ruleRow.model.id, Number(text), Number(highLimitField.text))
                                }
                            }
                        }

                        TextField {
                            id: highLimitField
                            visible: ruleRow.model.id !== "security.unsafe-string-api"
                            Layout.preferredWidth: 140
                            text: ruleRow.model.highLimit === undefined
                                ? ""
                                : ruleRow.model.highLimit.toString()
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            validator: IntValidator {}
                            onEditingFinished: {
                                if (acceptableInput && warningLimitField.acceptableInput
                                        && root.controller && ruleRow.model.id !== undefined) {
                                    root.controller.setRuleLimits(
                                        ruleRow.model.id, Number(warningLimitField.text), Number(text))
                                }
                            }
                        }
```

- [ ] **Step 2: Build the desktop app and run the QML smoke test**

Run (MinGW `buildQt` tree — required for anything under `src/desktop/`; `build/` is MSVC and is not used for this step):
```
cmake --build buildQt --target codeqa_desktop
cmake --build buildQt --target codeqa_desktop_qml_smoke
buildQt\tests\codeqa_desktop_qml_smoke.exe
```
Expected: both builds succeed and the smoke test passes (0 failures) — this confirms `AnalyzePage.qml` still parses and loads with no binding errors. It does not, by itself, verify the visibility logic (no existing QML test currently drives `AnalyzePage`'s rule list at all — a pre-existing gap, not something this task introduces or is scoped to fix).

- [ ] **Step 3: Manual visual check (recommended, not build-blocking)**

Run `buildQt\Debug\codeqa_desktop.exe` (or the equivalent output path for this build tree), open any project, go to the Analyze page, and confirm: the `size.function-length` row (or any other pre-existing rule) still shows both limit fields; once the `security.unsafe-string-api` row appears in the list (it will, since it is enabled by default), it shows no limit fields, just its id and the enabled checkbox.

- [ ] **Step 4: Commit**

```bash
git add src/desktop/qml/pages/AnalyzePage.qml
git commit -m "feat: hide unused threshold fields for the unsafe-string-api rule in AnalyzePage"
```

---

## Self-Review Notes

- **Spec coverage:** §4 (rule implementation) → Task 1 Steps 1-5. §5 (config) → Task 1 Step 4. §6 (GUI) → Task 2. §7 (testing) → Task 1 Steps 1, 6, 7, 8. §8 (acceptance criteria) → covered by the combination of Task 1's tests (blocklist behavior, no-match behavior, default-enabled) and Task 2 (GUI field hiding). No gaps found.
- **Placeholder scan:** none found — every step carries the literal code or exact file content to write.
- **Type/name consistency:** rule id string `"security.unsafe-string-api"` is identical across Task 1 (`id()`, `allRules`, `defaultConfig`, both test files) and Task 2 (QML `visible` binding). `UnsafeStringApiRule`, `UnsafeApiEntry`, `kUnsafeStringApis`, `lastQualifiedSegment` are each defined once (Task 1 Step 3) and not renamed elsewhere.
