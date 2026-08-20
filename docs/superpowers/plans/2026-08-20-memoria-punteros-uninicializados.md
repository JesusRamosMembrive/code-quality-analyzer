# Uninitialized Pointer Rule Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `memory.uninitialized-pointer`, a new pattern-based (non-threshold) rule that flags local raw-pointer variables declared without an initializer, and wire it through every registration surface: config defaults, config-JSON validation, the desktop rule list, and the desktop GUI's meaningless limit fields.

**Architecture:** One new `Rule` subclass in `src/rules/Rules.cpp` scans each callable's already-extracted `LocalVariableModel` list for entries whose (already-trimmed) `declaredType` ends in `'*'` and whose `hasInitializer` is `false`, producing one `Finding` per hit. Registered through the existing `allRules`/`defaultConfig` machinery exactly like `security.unsafe-string-api` — no new abstractions, no changes to `Rule::evaluate`'s signature or to any of the 9 existing rules. There is no central rule-id registry in this codebase: the id string must be added independently to 4 separate hardcoded lists (`DefaultConfig.cpp`, `ConfigJson.cpp::knownRuleIds()`, `ReportModels.cpp::orderedRuleIds()`, and the doc comment in `Rules.h`), confirmed by grepping the whole repo for every file that currently mentions `security.unsafe-string-api` (the same shape of addition, one track slice ago) — Task 1 and Task 2 below cover all of them up front, rather than discovering them as a fix round the way the previous slice did.

**Tech Stack:** C++17, doctest (existing `codeqa_tests` executable), Qt6/QML (`AnalyzePage.qml`), CMake (MSVC `build/` for `codeqa_tests`, MinGW `buildQt/` for the desktop app, `AppControllerTest`, and the QML smoke test).

**Spec:** `docs/superpowers/specs/2026-08-20-memoria-punteros-uninicializados-design.md`

## Global Constraints

- Rule id, exact string: `memory.uninitialized-pointer`.
- Detection: a `codeqa::model::LocalVariableModel` matches when `local.declaredType` is non-empty and its last character is `'*'`, AND `local.hasInitializer == false`. No other condition.
- Finding description format, exact: `"Local pointer variable '<name>' is declared without being initialized."` where `<name>` is `local.symbol.name`.
- Finding recommendation, exact: `"Initialize the pointer at declaration (e.g., to nullptr or a valid address) to avoid undefined behavior from using it before assignment."`
- Severity is always `codeqa::core::DiagnosticSeverity::Warning` — no High tier.
- `finding.location = local.symbol.range.begin`; `finding.symbolName = context.callable.symbol.qualifiedName` (the containing function, not the local variable itself).
- `Finding::observedValue` and `Finding::limitValue` stay `std::nullopt` — this rule has no numeric threshold.
- `appliesTo(codeqa::core::Language)` returns `true` unconditionally (applies to C and C++).
- Default config entry: `enabled = true`, `warningLimit = 0`, `highLimit = 0` (via the existing `thresholds()` helper) — both limit values exist only because `RuleConfig` requires them; the rule never reads them.
- Desktop GUI: hide `warningLimitField`/`highLimitField` for `"memory.uninitialized-pointer"` too, in addition to the existing `"security.unsafe-string-api"` exclusion already on those fields — do not remove or restructure the existing condition.
- Out of scope, do not add in this plan: array parameters, uninitialized function-pointer locals, uninitialized pointer parameters/globals, any dataflow tracking of a pointer assigned after its declaration, any change to `Rule::evaluate`'s signature or to any of the 9 existing rules.

---

### Task 1: Implement, register, and config-wire the `memory.uninitialized-pointer` rule

**Files:**
- Modify: `src/rules/Rules.cpp` (new rule class + registration)
- Modify: `include/codeqa/rules/Rules.h:12` (doc comment: 9 → 10 known rule ids)
- Modify: `src/config/DefaultConfig.cpp` (new default config entry)
- Modify: `src/config/ConfigJson.cpp:23-34` (`knownRuleIds()` — append the new id)
- Modify: `tests/unit/RulesTest.cpp` (new test cases + 2 existing-count updates)
- Modify: `tests/unit/DefaultConfigTest.cpp` (count + new rule assertion)
- Modify: `tests/corpus/validation/RULE-STABILITY.md` (new table row)

**Interfaces:**
- Consumes: `codeqa::model::LocalVariableModel{symbol, declaredType, hasInitializer, lexicalDepth}` (`include/codeqa/model/Models.h`, already visible via `codeqa/rules/Rule.h`); `codeqa::rules::RuleContext{metrics, callable}` and `codeqa::rules::Finding` (`include/codeqa/rules/Rule.h`, `RuleModels.h`); `codeqa::config::RuleConfig` and the local `thresholds(warningLimit, highLimit)` helper already defined in `src/config/DefaultConfig.cpp`.
- Produces: rule id string `"memory.uninitialized-pointer"`, registered in `codeqa::rules::allRules(...)`, present in `codeqa::config::defaultConfig()` and in `codeqa::config::knownRuleIds()` — Task 2's `ReportModels.cpp`/QML/`AppControllerTest.cpp` changes match against this exact string.

- [ ] **Step 1: Write the failing tests**

`tests/unit/RulesTest.cpp` already has a `makeCallable()` helper (returns a `CallableModel` with `symbol.name = "sample"`, `symbol.qualifiedName = "Sample::sample"`, `symbol.range.begin.line = 10`, `symbol.range.begin.column = 1`) and a `makeCall(calleeText, kind)` helper, both in the anonymous namespace near the top of the file. Append 4 new `TEST_CASE`s at the end of the file (after the existing `"security.unsafe-string-api recommends a specific safe alternative for each blocklisted function"` test case):

```cpp
TEST_CASE("memory.uninitialized-pointer flags a raw pointer local with no initializer")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules =
        codeqa::rules::allRules(codeqa::config::defaultConfig());
    const codeqa::rules::Rule& rule = ruleNamed(rules, "memory.uninitialized-pointer");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;

    codeqa::model::LocalVariableModel local;
    local.symbol.name = "p";
    local.symbol.qualifiedName = "Sample::sample::p";
    local.symbol.range.begin.line = 12;
    local.symbol.range.begin.column = 5;
    local.declaredType = "int*";
    local.hasInitializer = false;
    callable.localVariables.push_back(local);

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});

    REQUIRE(findings.size() == 1);
    CHECK(findings[0].ruleId == "memory.uninitialized-pointer");
    CHECK(findings[0].severity == codeqa::core::DiagnosticSeverity::Warning);
    CHECK(findings[0].description == "Local pointer variable 'p' is declared without being initialized.");
    CHECK(
        findings[0].recommendation
        == "Initialize the pointer at declaration (e.g., to nullptr or a valid address) "
           "to avoid undefined behavior from using it before assignment.");
    CHECK_FALSE(findings[0].observedValue.has_value());
    CHECK_FALSE(findings[0].limitValue.has_value());
    CHECK(findings[0].symbolName == "Sample::sample");
    CHECK(findings[0].location.line == 12);
    CHECK(findings[0].location.column == 5);
}

TEST_CASE("memory.uninitialized-pointer ignores an initialized raw pointer local")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules =
        codeqa::rules::allRules(codeqa::config::defaultConfig());
    const codeqa::rules::Rule& rule = ruleNamed(rules, "memory.uninitialized-pointer");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;

    codeqa::model::LocalVariableModel local;
    local.symbol.name = "p";
    local.declaredType = "int*";
    local.hasInitializer = true;
    callable.localVariables.push_back(local);

    CHECK(rule.evaluate(codeqa::rules::RuleContext{metrics, callable}).empty());
}

TEST_CASE("memory.uninitialized-pointer ignores non-pointer, array, smart-pointer, and function-pointer locals")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules =
        codeqa::rules::allRules(codeqa::config::defaultConfig());
    const codeqa::rules::Rule& rule = ruleNamed(rules, "memory.uninitialized-pointer");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;

    codeqa::model::LocalVariableModel plainInt;
    plainInt.symbol.name = "x";
    plainInt.declaredType = "int";
    plainInt.hasInitializer = false;

    codeqa::model::LocalVariableModel array;
    array.symbol.name = "arr";
    array.declaredType = "int[10]";
    array.hasInitializer = false;

    codeqa::model::LocalVariableModel smartPointer;
    smartPointer.symbol.name = "sp";
    smartPointer.declaredType = "std::unique_ptr<int>";
    smartPointer.hasInitializer = false;

    codeqa::model::LocalVariableModel functionPointer;
    functionPointer.symbol.name = "fp";
    functionPointer.declaredType = "int(*)(int)";
    functionPointer.hasInitializer = false;

    callable.localVariables = {plainInt, array, smartPointer, functionPointer};

    CHECK(rule.evaluate(codeqa::rules::RuleContext{metrics, callable}).empty());
}

TEST_CASE("memory.uninitialized-pointer produces one finding per matching local, in declaration order")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules =
        codeqa::rules::allRules(codeqa::config::defaultConfig());
    const codeqa::rules::Rule& rule = ruleNamed(rules, "memory.uninitialized-pointer");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;

    codeqa::model::LocalVariableModel first;
    first.symbol.name = "a";
    first.declaredType = "char*";
    first.hasInitializer = false;

    codeqa::model::LocalVariableModel initialized;
    initialized.symbol.name = "b";
    initialized.declaredType = "char*";
    initialized.hasInitializer = true;

    codeqa::model::LocalVariableModel second;
    second.symbol.name = "c";
    second.declaredType = "char**";
    second.hasInitializer = false;

    callable.localVariables = {first, initialized, second};

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});

    REQUIRE(findings.size() == 2);
    CHECK(findings[0].description == "Local pointer variable 'a' is declared without being initialized.");
    CHECK(findings[1].description == "Local pointer variable 'c' is declared without being initialized.");
}
```

Then update the 2 existing test cases that hardcode the total rule count:

```cpp
TEST_CASE("all nine rules are registered and apply to both C and C++")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(codeqa::config::defaultConfig());
    REQUIRE(rules.size() == 9);
```

becomes:

```cpp
TEST_CASE("all ten rules are registered and apply to both C and C++")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(codeqa::config::defaultConfig());
    REQUIRE(rules.size() == 10);
```

and:

```cpp
TEST_CASE("allRules omits a rule disabled via config")
{
    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.rules.at("complexity.cyclomatic").enabled = false;

    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(config);

    REQUIRE(rules.size() == 8);
```

becomes:

```cpp
TEST_CASE("allRules omits a rule disabled via config")
{
    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.rules.at("complexity.cyclomatic").enabled = false;

    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(config);

    REQUIRE(rules.size() == 9);
```

- [ ] **Step 2: Run tests to verify they fail**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="memory.uninitialized-pointer*"
```
Expected: FAIL — `ruleNamed` throws `std::runtime_error("Expected rule was not registered: memory.uninitialized-pointer")`, because `allRules` does not produce this rule yet.

- [ ] **Step 3: Implement the rule**

In `src/rules/Rules.cpp`, inside the anonymous namespace, directly after the `UnsafeStringApiRule` class definition and before the namespace's closing `} // namespace`, add:

```cpp
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

Then, in `allRules(...)`, add this block right before the function's final `return rules;` (right after the existing `security.unsafe-string-api` block):

```cpp
    if (const codeqa::config::RuleConfig& c = config.rules.at("memory.uninitialized-pointer"); c.enabled)
    {
        rules.push_back(std::make_unique<UninitializedPointerRule>());
    }

```

(Note this rule's constructor takes no arguments, unlike the threshold rules — `c` is read only for `.enabled`, same as `UnsafeStringApiRule`.)

- [ ] **Step 4: Wire the default config, `knownRuleIds()`, and the rule-count doc comment**

In `src/config/DefaultConfig.cpp`, add this line right before `return config;` in `defaultConfig()`, right after the existing `security.unsafe-string-api` line:

```cpp
    config.rules["memory.uninitialized-pointer"] = thresholds(0, 0);
```

In `src/config/ConfigJson.cpp`, `knownRuleIds()` currently ends:

```cpp
        "complexity.nesting",
        "security.unsafe-string-api"};
    return ids;
```

Change it to:

```cpp
        "complexity.nesting",
        "security.unsafe-string-api",
        "memory.uninitialized-pointer"};
    return ids;
```

(Without this, a `codeqa.json` mentioning `"memory.uninitialized-pointer"` in its `rules` object is rejected with `Error [CONFIG003]: unknown rule id`.)

In `include/codeqa/rules/Rules.h`, the doc comment above `allRules` currently reads:

```cpp
/// Requires config.rules to contain an entry for each of the 9 known rule ids --
```

Change `9` to `10`:

```cpp
/// Requires config.rules to contain an entry for each of the 10 known rule ids --
```

- [ ] **Step 5: Run the new tests to verify they pass**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="memory.uninitialized-pointer*"
```
Expected: PASS (4 test cases, 0 failures).

- [ ] **Step 6: Update `DefaultConfigTest.cpp` and verify**

`tests/unit/DefaultConfigTest.cpp` currently ends with:

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

Replace it with:

```cpp
TEST_CASE("defaultConfig ships the eight Fase 7 thresholds plus the unsafe-string-api and uninitialized-pointer rules, all enabled, no header overrides")
{
    const codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();

    CHECK(config.headerOverrides.empty());
    REQUIRE(config.rules.size() == 10);

    checkRule(config, "size.function-length", 50, 90);
    checkRule(config, "size.local-variables", 8, 12);
    checkRule(config, "signature.parameter-count", 4, 7);
    checkRule(config, "signature.consecutive-booleans", 2, 3);
    checkRule(config, "complexity.cyclomatic", 10, 15);
    checkRule(config, "complexity.returns", 4, 6);
    checkRule(config, "complexity.condition", 3, 5);
    checkRule(config, "complexity.nesting", 3, 5);
    checkRule(config, "security.unsafe-string-api", 0, 0);
    checkRule(config, "memory.uninitialized-pointer", 0, 0);
}
```

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="defaultConfig*"
```
Expected: PASS.

- [ ] **Step 7: Add the corpus stability row**

In `tests/corpus/validation/RULE-STABILITY.md`, add a new row at the end of the table (after the `security.unsafe-string-api` row):

```markdown
| `memory.uninitialized-pointer` | `experimental` | — | — |
```

Do not touch the introductory paragraph — it already stayed unedited through the previous rule addition and remains accurate as written.

- [ ] **Step 8: Run the full MSVC test suite**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe
```
Expected: PASS, all test cases (0 failures) — in particular confirms `CorpusReportTest.cpp` needs no changes: grepping `tests/corpus/` for any local pointer declaration (`grep -rn "\*\s*\w+\s*;" tests/corpus`) found none, only pointer *parameters* (untouched by this rule), so no fixture's expected finding count changes.

- [ ] **Step 9: Commit**

```bash
git add src/rules/Rules.cpp include/codeqa/rules/Rules.h src/config/DefaultConfig.cpp src/config/ConfigJson.cpp tests/unit/RulesTest.cpp tests/unit/DefaultConfigTest.cpp tests/corpus/validation/RULE-STABILITY.md
git commit -m "feat: add memory.uninitialized-pointer rule for uninitialized raw pointer locals"
```

---

### Task 2: Wire the rule into the desktop GUI (rule list, limit-field visibility, tests)

**Files:**
- Modify: `src/desktop/ReportModels.cpp:17-30` (`orderedRuleIds()` — append the new id)
- Modify: `src/desktop/qml/pages/AnalyzePage.qml:177-209` (extend the existing `visible` conditions)
- Modify: `tests/desktop/AppControllerTest.cpp:219,220-229,262` (rowCount + expected id list, both call sites)

**Interfaces:**
- Consumes: rule id string `"memory.uninitialized-pointer"` produced by Task 1 (must match exactly); the existing `ruleRow.model.id`, `warningLimitField`, `highLimitField` QML elements; the existing `orderedRuleIds()` static list shape in `ReportModels.cpp`.
- Produces: nothing consumed by a later task — this is the final task in the plan.

- [ ] **Step 1: Write the failing desktop test expectations**

`tests/desktop/AppControllerTest.cpp` has two assertions to update. First, around line 219:

```cpp
    QCOMPARE(controller.rules()->rowCount(), 9);
    const QStringList expectedRuleIds = {
        "size.function-length",
        "size.local-variables",
        "signature.parameter-count",
        "signature.consecutive-booleans",
        "complexity.cyclomatic",
        "complexity.returns",
        "complexity.condition",
        "complexity.nesting",
        "security.unsafe-string-api"};
```

becomes:

```cpp
    QCOMPARE(controller.rules()->rowCount(), 10);
    const QStringList expectedRuleIds = {
        "size.function-length",
        "size.local-variables",
        "signature.parameter-count",
        "signature.consecutive-booleans",
        "complexity.cyclomatic",
        "complexity.returns",
        "complexity.condition",
        "complexity.nesting",
        "security.unsafe-string-api",
        "memory.uninitialized-pointer"};
```

Second, around line 262 (in `missingConfigurationUsesUnsavedDefaultsWithoutWriting`):

```cpp
    QCOMPARE(controller.rules()->rowCount(), 9);
```

becomes:

```cpp
    QCOMPARE(controller.rules()->rowCount(), 10);
```

- [ ] **Step 2: Run the desktop tests to verify they fail**

Run (MinGW `buildQt` tree — required for anything under `src/desktop/` or `tests/desktop/`):
```
cmake --build buildQt --target codeqa_desktop_tests
buildQt\codeqa_desktop_tests.exe
```
Expected: FAIL on the two updated `rowCount()` assertions (still 9 in the model, since `orderedRuleIds()` hasn't been updated yet).

- [ ] **Step 3: Update `orderedRuleIds()`**

`src/desktop/ReportModels.cpp`'s `orderedRuleIds()` currently ends:

```cpp
        QStringLiteral("complexity.nesting"),
        QStringLiteral("security.unsafe-string-api")};
    return ids;
```

Change it to:

```cpp
        QStringLiteral("complexity.nesting"),
        QStringLiteral("security.unsafe-string-api"),
        QStringLiteral("memory.uninitialized-pointer")};
    return ids;
```

- [ ] **Step 4: Hide the unused limit fields for this rule too**

In `src/desktop/qml/pages/AnalyzePage.qml`, both `TextField`s already carry a `visible` binding from the previous rule slice:

```qml
                        TextField {
                            id: warningLimitField
                            visible: ruleRow.model.id !== "security.unsafe-string-api"
```

and

```qml
                        TextField {
                            id: highLimitField
                            visible: ruleRow.model.id !== "security.unsafe-string-api"
```

Extend both conditions (do not add a second `visible:` line — QML only keeps the last one):

```qml
                        TextField {
                            id: warningLimitField
                            visible: ruleRow.model.id !== "security.unsafe-string-api"
                                && ruleRow.model.id !== "memory.uninitialized-pointer"
```

and

```qml
                        TextField {
                            id: highLimitField
                            visible: ruleRow.model.id !== "security.unsafe-string-api"
                                && ruleRow.model.id !== "memory.uninitialized-pointer"
```

- [ ] **Step 5: Run the desktop tests and QML smoke test to verify they pass**

Run:
```
cmake --build buildQt --target codeqa_desktop
cmake --build buildQt --target codeqa_desktop_tests
cmake --build buildQt --target codeqa_desktop_qml_smoke
buildQt\codeqa_desktop_tests.exe
buildQt\codeqa_desktop_qml_smoke.exe
```
Expected: all three succeed, `codeqa_desktop_tests.exe` reports 0 failures, `codeqa_desktop_qml_smoke.exe` passes (confirms `AnalyzePage.qml` still parses and loads with no binding errors).

- [ ] **Step 6: Manual visual check (recommended, not build-blocking)**

Run `buildQt\codeqa_desktop.exe`, open any project, go to the Analyze page, and confirm: the `size.function-length` row still shows both limit fields; the `memory.uninitialized-pointer` row (enabled by default, so it will be present) shows no limit fields, just its id and the enabled checkbox — same as the `security.unsafe-string-api` row already does.

- [ ] **Step 7: Commit**

```bash
git add src/desktop/ReportModels.cpp src/desktop/qml/pages/AnalyzePage.qml tests/desktop/AppControllerTest.cpp
git commit -m "feat: wire memory.uninitialized-pointer into the desktop rule list and hide its limit fields"
```

---

## Self-Review Notes

- **Spec coverage:** §4 (rule implementation) → Task 1 Steps 1-5. §5 (config) → Task 1 Step 4. §6 (registration surface, all 6 points) → Task 1 Steps 4, 6, 7 (points 1-4) and Task 2 Steps 3, 4, 5 (points 5-6). §7 (testing) → Task 1 Steps 1, 6, 7, 8. §8 (acceptance criteria) → covered by the combination of Task 1's tests (matching/non-matching declaredType shapes, per-local findings, default-enabled) and Task 2 (GUI field hiding, desktop rule list row count). No gaps found.
- **Placeholder scan:** none found — every step carries the literal code or exact file content to write.
- **Type/name consistency:** rule id string `"memory.uninitialized-pointer"` is identical across Task 1 (`id()`, `allRules`, `defaultConfig`, `knownRuleIds`, both test files) and Task 2 (`orderedRuleIds`, QML `visible` binding, `AppControllerTest.cpp`). `UninitializedPointerRule`, `looksLikeRawPointer` are each defined once (Task 1 Step 3) and not renamed elsewhere. Confirmed against the actual current file contents (not assumed) by reading `RulesTest.cpp`, `DefaultConfigTest.cpp`, `ConfigJson.cpp`, `ReportModels.cpp`, `AnalyzePage.qml`, and `AppControllerTest.cpp` in full before writing this plan — this closes the exact gap (registration surface incomplete in the plan) that the previous slice's implementer/reviewer had to catch mid-flight.
