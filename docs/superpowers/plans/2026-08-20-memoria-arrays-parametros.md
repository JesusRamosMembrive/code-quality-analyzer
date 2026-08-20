# Array Parameter Decay Rule Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `memory.array-parameter-decay`, a new pattern-based (non-threshold) rule that flags a function parameter declared with array syntax (`int arr[]`, `int arr[10]`) — such a parameter always decays to a bare pointer, silently discarding whatever size the declarator seems to promise. This requires a small, real frontend extension first: a new `ParameterPassing::Array` value, since today neither `ParameterModel::declaredType` nor `ParameterModel::passing` can distinguish an array parameter from a plain one.

**Architecture:** `ParameterPassing` is defined **twice** in this codebase — the frontend's parse-time model (`codeqa::frontend`) and the common intermediate model rules actually see (`codeqa::model`) — bridged by one translating switch (`ProjectModelBuilder.cpp`) and serialized to JSON by a second switch (`FileJson.cpp`), both exhaustive with no `default:` case. Task 1 adds `Array` to both enums and both switches (the build will not compile otherwise) plus the detection logic in the frontend extractor. Task 2 adds the rule itself on top of that new signal, using the exact same `allRules`/`defaultConfig` registration machinery as the two prior non-threshold rules. Task 3 wires the new rule id into the desktop GUI. No changes to `Rule::evaluate`'s signature or to any of the 10 existing rules.

**Tech Stack:** C++17, doctest (existing `codeqa_tests` executable), Qt6/QML (`AnalyzePage.qml`), CMake (MSVC `build/` for `codeqa_tests`, MinGW `buildQt/` for the desktop app and its tests).

**Spec:** `docs/superpowers/specs/2026-08-20-memoria-arrays-parametros-design.md`

## Global Constraints

- New enumerator, added identically to **both** `enum class ParameterPassing` definitions (`include/codeqa/frontend/StructuralModels.h` and `include/codeqa/model/Models.h`): `Array`, appended after `FunctionPointer`.
- Detection priority order in `parametersFor()`: `FunctionPointer` (checked first, unchanged) → **`Array`** (new, checked second) → `Reference` → `Pointer` → `Value`. An array-of-pointers (`int* arr[5]`) must classify as `Array`, not `Pointer`.
- Rule id, exact string: `memory.array-parameter-decay`.
- One `Finding` per array-shaped parameter (not aggregated per function).
- Finding description, exact format: `"Parameter '<name>' is declared as an array, which decays to a pointer and loses its size information."` for a named parameter; `"an unnamed parameter is declared as an array, which decays to a pointer and loses its size information."` for an unnamed one (note lowercase "an" — it's the sentence's grammatical subject).
- Finding recommendation, exact: `"Pass the array's size explicitly as a separate parameter, or use a type that carries its own size (e.g. std::span, std::array&, or a fixed-size struct)."`
- Severity is always `codeqa::core::DiagnosticSeverity::Warning`.
- `finding.location`: `context.callable.parameterListRange->begin` if set, else `context.callable.symbol.range.begin` — same fallback pattern as `signature.parameter-count`/`signature.consecutive-booleans`.
- `finding.symbolName = context.callable.symbol.qualifiedName`.
- `Finding::observedValue`/`limitValue` stay `std::nullopt`.
- `appliesTo(codeqa::core::Language)` returns `true` unconditionally.
- Default config entry: `enabled = true`, `warningLimit = 0`, `highLimit = 0` (via `thresholds()`).
- Desktop GUI: hide `warningLimitField`/`highLimitField` for `"memory.array-parameter-decay"` too, alongside the two existing exclusions — extend the condition, do not replace it.
- Out of scope, do not add in this plan: macros; array locals/globals (they don't decay, no bug to flag); a `SourceRange` field on `ParameterModel`; any change to `parameterDeclaredType()`'s text-extraction logic; any change to `Rule::evaluate`'s signature or to any of the 10 existing rules.

---

### Task 1: Extend `ParameterPassing` with an `Array` value end-to-end

**Files:**
- Modify: `include/codeqa/frontend/StructuralModels.h:24-30` (add enumerator)
- Modify: `include/codeqa/model/Models.h:41-47` (add enumerator)
- Modify: `src/frontend/StructuralExtractor.cpp:582-592` (detection logic)
- Modify: `src/model/ProjectModelBuilder.cpp:37-52` (translating switch)
- Modify: `src/report/FileJson.cpp:66-80` (JSON-serializing switch)
- Modify: `tests/unit/StructuralExtractorTest.cpp` (new test case)
- Modify: `tests/unit/ProjectModelBuilderTest.cpp` (new test case)
- Modify: `tests/unit/FileJsonTest.cpp` (new test case)

**Interfaces:**
- Consumes: `codeqa::frontend::ParameterPassing`, `codeqa::model::ParameterPassing`, `codeqa::model::ProjectModelBuilder`, `codeqa::report::fileJson` — all pre-existing.
- Produces: `codeqa::model::ParameterPassing::Array`, reachable on any `codeqa::model::ParameterModel::passing` produced by `ProjectModelBuilder` from a frontend-extracted array-shaped parameter. Task 2's rule consumes this exact enumerator.

- [ ] **Step 1: Write the failing frontend test**

Open `tests/unit/StructuralExtractorTest.cpp`. Immediately after the existing test case `"derives C++ parameter passing from the declarator only"` (it ends at line 158 with a closing `}`, followed by a blank line and then `TEST_CASE("extracts the declared type of each parameter, excluding declarator markers")`), insert a new test case:

```cpp
TEST_CASE("derives Array parameter passing from array declarators, including arrays of pointers")
{
    const TempSourceFile file{
        "array-parameters.cpp",
        "void classify(int unsized[], int sized[10], int* pointers[5], int plain, int matrix[3][4]);\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 1);
    REQUIRE(result.functions[0].parameters.size() == 5);
    CHECK(
        result.functions[0].parameters[0].passing
        == codeqa::frontend::ParameterPassing::Array);
    CHECK(
        result.functions[0].parameters[1].passing
        == codeqa::frontend::ParameterPassing::Array);
    CHECK(
        result.functions[0].parameters[2].passing
        == codeqa::frontend::ParameterPassing::Array);
    CHECK(
        result.functions[0].parameters[3].passing
        == codeqa::frontend::ParameterPassing::Value);
    CHECK(
        result.functions[0].parameters[4].passing
        == codeqa::frontend::ParameterPassing::Array);
}

TEST_CASE("derives Array parameter passing from an unnamed array parameter")
{
    const TempSourceFile file{
        "unnamed-array-parameter.cpp",
        "void classify(int[]);\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 1);
    REQUIRE(result.functions[0].parameters.size() == 1);
    CHECK(
        result.functions[0].parameters[0].passing
        == codeqa::frontend::ParameterPassing::Array);
    CHECK(result.functions[0].parameters[0].name.empty());
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="*Array parameter passing*"
```
Expected: FAIL to compile — `codeqa::frontend::ParameterPassing::Array` does not exist yet.

- [ ] **Step 3: Add the `Array` enumerator to both `ParameterPassing` definitions**

In `include/codeqa/frontend/StructuralModels.h`, the enum currently reads (lines 24-30):

```cpp
enum class ParameterPassing
{
    Value,
    Pointer,
    Reference,
    FunctionPointer
};
```

Change to:

```cpp
enum class ParameterPassing
{
    Value,
    Pointer,
    Reference,
    FunctionPointer,
    Array
};
```

In `include/codeqa/model/Models.h`, the enum currently reads (lines 41-47):

```cpp
enum class ParameterPassing
{
    Value,
    Pointer,
    Reference,
    FunctionPointer
};
```

Change to:

```cpp
enum class ParameterPassing
{
    Value,
    Pointer,
    Reference,
    FunctionPointer,
    Array
};
```

- [ ] **Step 4: Add the detection logic in the frontend extractor**

In `src/frontend/StructuralExtractor.cpp`, inside `parametersFor()`, the `model.passing` assignment currently reads (lines 582-592):

```cpp
        model.passing = isFunctionPointerDeclarator(declarator)
            ? ParameterPassing::FunctionPointer
            : (!ts_node_is_null(findDescendantOfType(declarator, "reference_declarator"))
                      || !ts_node_is_null(
                          findDescendantOfType(declarator, "abstract_reference_declarator"))
                    ? ParameterPassing::Reference
                    : (!ts_node_is_null(findDescendantOfType(declarator, "pointer_declarator"))
                              || !ts_node_is_null(
                                  findDescendantOfType(declarator, "abstract_pointer_declarator"))
                            ? ParameterPassing::Pointer
                            : ParameterPassing::Value));
```

Change to (inserting the `Array` check between `FunctionPointer` and `Reference`):

```cpp
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
                                      || !ts_node_is_null(findDescendantOfType(
                                          declarator, "abstract_pointer_declarator"))
                                    ? ParameterPassing::Pointer
                                    : ParameterPassing::Value)));
```

- [ ] **Step 5: Add the translating case in `ProjectModelBuilder.cpp`**

In `src/model/ProjectModelBuilder.cpp`, `parameterPassingFor()` currently reads (lines 37-52):

```cpp
ParameterPassing parameterPassingFor(frontend::ParameterPassing passing)
{
    switch (passing)
    {
    case frontend::ParameterPassing::Value:
        return ParameterPassing::Value;
    case frontend::ParameterPassing::Pointer:
        return ParameterPassing::Pointer;
    case frontend::ParameterPassing::Reference:
        return ParameterPassing::Reference;
    case frontend::ParameterPassing::FunctionPointer:
        return ParameterPassing::FunctionPointer;
    }

    throw std::logic_error("Unknown frontend parameter passing.");
}
```

Add a case right before the closing brace of the switch:

```cpp
ParameterPassing parameterPassingFor(frontend::ParameterPassing passing)
{
    switch (passing)
    {
    case frontend::ParameterPassing::Value:
        return ParameterPassing::Value;
    case frontend::ParameterPassing::Pointer:
        return ParameterPassing::Pointer;
    case frontend::ParameterPassing::Reference:
        return ParameterPassing::Reference;
    case frontend::ParameterPassing::FunctionPointer:
        return ParameterPassing::FunctionPointer;
    case frontend::ParameterPassing::Array:
        return ParameterPassing::Array;
    }

    throw std::logic_error("Unknown frontend parameter passing.");
}
```

Without this case, the build fails to compile under this project's warnings-as-errors setting (`-Wswitch` on an exhaustive enum switch with no `default:`) — this is expected and confirms the case is required, not optional.

- [ ] **Step 6: Add the serializing case in `FileJson.cpp`**

In `src/report/FileJson.cpp`, `parameterPassingToJsonString()` currently reads (lines 66-80):

```cpp
std::string parameterPassingToJsonString(codeqa::model::ParameterPassing passing)
{
    switch (passing)
    {
    case codeqa::model::ParameterPassing::Value:
        return "value";
    case codeqa::model::ParameterPassing::Pointer:
        return "pointer";
    case codeqa::model::ParameterPassing::Reference:
        return "reference";
    case codeqa::model::ParameterPassing::FunctionPointer:
        return "functionPointer";
    }
    return "value";
}
```

Add a case:

```cpp
std::string parameterPassingToJsonString(codeqa::model::ParameterPassing passing)
{
    switch (passing)
    {
    case codeqa::model::ParameterPassing::Value:
        return "value";
    case codeqa::model::ParameterPassing::Pointer:
        return "pointer";
    case codeqa::model::ParameterPassing::Reference:
        return "reference";
    case codeqa::model::ParameterPassing::FunctionPointer:
        return "functionPointer";
    case codeqa::model::ParameterPassing::Array:
        return "array";
    }
    return "value";
}
```

- [ ] **Step 7: Run the frontend tests to verify they pass**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="*Array parameter passing*"
```
Expected: PASS (2 test cases, 0 failures).

- [ ] **Step 8: Add a builder-mapping test**

Open `tests/unit/ProjectModelBuilderTest.cpp`. Near the top of the file is a helper `rangeFor(path, beginOffset, endOffset)` returning a `codeqa::core::SourceRange` (used throughout the file — reuse it, do not redefine it). At the end of the file (after the last `TEST_CASE`, `"two Free functions with non-overlapping ranges never get a PARSE004 diagnostic"`), append:

```cpp
TEST_CASE("ProjectModelBuilder maps a frontend Array parameter to the model's Array passing")
{
    const std::filesystem::path rootPath{"project"};
    const std::filesystem::path sourcePath{"project/src/sample.cpp"};

    codeqa::frontend::ParsedFile sourceFile;
    sourceFile.path = sourcePath;
    sourceFile.language = codeqa::core::Language::Cpp;
    sourceFile.grammar = codeqa::frontend::Grammar::Cpp;
    sourceFile.parsed = true;
    sourceFile.functions.push_back(codeqa::frontend::FunctionModel{
        "classify",
        codeqa::frontend::FunctionKind::Free,
        rangeFor(sourcePath, 0, 30),
        {codeqa::frontend::ParameterModel{
            "items", codeqa::frontend::ParameterPassing::Array, false, "int"}},
        false,
        false});

    const codeqa::model::ProjectModel result = codeqa::model::ProjectModelBuilder{}.build(
        rootPath, std::vector<codeqa::frontend::ParsedFile>{sourceFile});

    REQUIRE(result.files.size() == 1);
    REQUIRE(result.files[0].callables.size() == 1);
    REQUIRE(result.files[0].callables[0].parameters.size() == 1);
    CHECK(
        result.files[0].callables[0].parameters[0].passing
        == codeqa::model::ParameterPassing::Array);
}
```

This mirrors the exact `FunctionModel{name, kind, range, parameters, isStatic, isVariadic}` and `ParameterModel{name, passing, hasDefaultValue, declaredType}` aggregate-initialization shapes already used elsewhere in this same file (e.g. the `"value"`/`"target"` fixture earlier in the file).

- [ ] **Step 9: Add a JSON-serialization test**

Open `tests/unit/FileJsonTest.cpp` (ends at line 135 with the closing brace of `"fileJson omits cppDetails when a type has none"`). Append:

```cpp
TEST_CASE("fileJson serializes Array parameter passing as \"array\"")
{
    codeqa::model::FileModel file;
    file.path = "src/sample.cpp";
    file.language = codeqa::core::Language::Cpp;

    codeqa::model::CallableModel callable;
    callable.symbol.name = "classify";
    callable.symbol.qualifiedName = "classify";

    codeqa::model::ParameterModel parameter;
    parameter.name = "items";
    parameter.passing = codeqa::model::ParameterPassing::Array;
    parameter.declaredType = "int";
    callable.parameters.push_back(parameter);

    file.callables.push_back(callable);

    const nlohmann::json document = codeqa::report::fileJson(file);

    REQUIRE(document.at("callables").size() == 1);
    const nlohmann::json& callableJson = document.at("callables")[0];
    REQUIRE(callableJson.at("parameters").size() == 1);
    CHECK(callableJson.at("parameters")[0].at("passing").get<std::string>() == "array");
}
```

- [ ] **Step 10: Run the full test suite**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe
```
Expected: PASS, all test cases (0 failures) — confirms the build compiles cleanly (both switches handle `Array`) and no existing test asserting `ParameterPassing::Value`/`Pointer`/`Reference`/`FunctionPointer` regressed.

- [ ] **Step 11: Commit**

```bash
git add include/codeqa/frontend/StructuralModels.h include/codeqa/model/Models.h src/frontend/StructuralExtractor.cpp src/model/ProjectModelBuilder.cpp src/report/FileJson.cpp tests/unit/StructuralExtractorTest.cpp tests/unit/ProjectModelBuilderTest.cpp tests/unit/FileJsonTest.cpp
git commit -m "feat: add ParameterPassing::Array, detected from array declarators"
```

---

### Task 2: Implement, register, and config-wire the `memory.array-parameter-decay` rule

**Files:**
- Modify: `src/rules/Rules.cpp` (new rule class + registration)
- Modify: `include/codeqa/rules/Rules.h:12` (doc comment: 10 → 11)
- Modify: `src/config/DefaultConfig.cpp` (new default config entry)
- Modify: `src/config/ConfigJson.cpp` (`knownRuleIds()` — append the id)
- Modify: `tests/unit/RulesTest.cpp` (new test cases + 2 existing-count updates)
- Modify: `tests/unit/DefaultConfigTest.cpp` (count + new rule assertion)
- Modify: `tests/unit/ReportJsonTest.cpp:52` (rule-count assertion: 10 → 11)
- Modify: `tests/corpus/validation/RULE-STABILITY.md` (new table row)

**Interfaces:**
- Consumes: `codeqa::model::ParameterModel{name, passing, hasDefaultValue, declaredType}` and `codeqa::model::ParameterPassing::Array` (produced by Task 1); `codeqa::rules::RuleContext{metrics, callable}` and `codeqa::rules::Finding` (`include/codeqa/rules/Rule.h`, `RuleModels.h`); `codeqa::config::RuleConfig` and the local `thresholds(warningLimit, highLimit)` helper in `src/config/DefaultConfig.cpp`.
- Produces: rule id string `"memory.array-parameter-decay"`, registered in `codeqa::rules::allRules(...)` — Task 3's GUI change matches against this exact string.

- [ ] **Step 1: Write the failing tests**

`tests/unit/RulesTest.cpp` already has a `makeCallable()` helper (returns a `CallableModel` with `symbol.name = "sample"`, `symbol.qualifiedName = "Sample::sample"`, `symbol.range.begin.line = 10`, `symbol.range.begin.column = 1`) in its anonymous namespace near the top. Append 5 new `TEST_CASE`s at the end of the file (after the existing `"memory.uninitialized-pointer produces one finding per matching local, in declaration order"` test case, which currently ends the file at line 686):

```cpp
TEST_CASE("memory.array-parameter-decay flags a named array parameter")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules =
        codeqa::rules::allRules(codeqa::config::defaultConfig());
    const codeqa::rules::Rule& rule = ruleNamed(rules, "memory.array-parameter-decay");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;

    callable.parameters = {
        codeqa::model::ParameterModel{"items", codeqa::model::ParameterPassing::Array, false, "int"}};

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});

    REQUIRE(findings.size() == 1);
    CHECK(findings[0].ruleId == "memory.array-parameter-decay");
    CHECK(findings[0].severity == codeqa::core::DiagnosticSeverity::Warning);
    CHECK(
        findings[0].description
        == "Parameter 'items' is declared as an array, which decays to a pointer and loses its size information.");
    CHECK(
        findings[0].recommendation
        == "Pass the array's size explicitly as a separate parameter, or use a type "
           "that carries its own size (e.g. std::span, std::array&, or a fixed-size struct).");
    CHECK_FALSE(findings[0].observedValue.has_value());
    CHECK_FALSE(findings[0].limitValue.has_value());
    CHECK(findings[0].symbolName == "Sample::sample");
    CHECK(findings[0].location.line == 10);
}

TEST_CASE("memory.array-parameter-decay flags an unnamed array parameter with the unnamed phrasing")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules =
        codeqa::rules::allRules(codeqa::config::defaultConfig());
    const codeqa::rules::Rule& rule = ruleNamed(rules, "memory.array-parameter-decay");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;

    callable.parameters = {
        codeqa::model::ParameterModel{"", codeqa::model::ParameterPassing::Array, false, "int"}};

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});

    REQUIRE(findings.size() == 1);
    CHECK(
        findings[0].description
        == "an unnamed parameter is declared as an array, which decays to a pointer and loses its size information.");
}

TEST_CASE("memory.array-parameter-decay ignores non-array parameters")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules =
        codeqa::rules::allRules(codeqa::config::defaultConfig());
    const codeqa::rules::Rule& rule = ruleNamed(rules, "memory.array-parameter-decay");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;

    callable.parameters = {
        codeqa::model::ParameterModel{"a", codeqa::model::ParameterPassing::Value, false, "int"},
        codeqa::model::ParameterModel{"b", codeqa::model::ParameterPassing::Pointer, false, "int"},
        codeqa::model::ParameterModel{"c", codeqa::model::ParameterPassing::Reference, false, "int"},
        codeqa::model::ParameterModel{"d", codeqa::model::ParameterPassing::FunctionPointer, false, "int"}};

    CHECK(rule.evaluate(codeqa::rules::RuleContext{metrics, callable}).empty());
}

TEST_CASE("memory.array-parameter-decay produces one finding per array parameter, at the shared parameter-list location")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules =
        codeqa::rules::allRules(codeqa::config::defaultConfig());
    const codeqa::rules::Rule& rule = ruleNamed(rules, "memory.array-parameter-decay");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::core::SourceRange parameterListRange;
    parameterListRange.begin.line = 10;
    parameterListRange.begin.column = 13;
    callable.parameterListRange = parameterListRange;
    codeqa::metrics::FunctionMetricsModel metrics;

    callable.parameters = {
        codeqa::model::ParameterModel{"a", codeqa::model::ParameterPassing::Array, false, "int"},
        codeqa::model::ParameterModel{"b", codeqa::model::ParameterPassing::Value, false, "int"},
        codeqa::model::ParameterModel{"c", codeqa::model::ParameterPassing::Array, false, "int"}};

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});

    REQUIRE(findings.size() == 2);
    CHECK(findings[0].description.find("'a'") != std::string::npos);
    CHECK(findings[0].location.line == 10);
    CHECK(findings[0].location.column == 13);
    CHECK(findings[1].description.find("'c'") != std::string::npos);
    CHECK(findings[1].location.line == 10);
    CHECK(findings[1].location.column == 13);
}

TEST_CASE("memory.array-parameter-decay falls back to the function start when there is no parameter list")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules =
        codeqa::rules::allRules(codeqa::config::defaultConfig());
    const codeqa::rules::Rule& rule = ruleNamed(rules, "memory.array-parameter-decay");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;

    callable.parameters = {
        codeqa::model::ParameterModel{"a", codeqa::model::ParameterPassing::Array, false, "int"}};

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});

    REQUIRE(findings.size() == 1);
    CHECK(findings[0].location.line == 10);
    CHECK(findings[0].location.column == 1);
}
```

Then update the 2 existing test cases that hardcode the total rule count:

```cpp
TEST_CASE("all ten rules are registered and apply to both C and C++")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(codeqa::config::defaultConfig());
    REQUIRE(rules.size() == 10);
```

becomes:

```cpp
TEST_CASE("all eleven rules are registered and apply to both C and C++")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(codeqa::config::defaultConfig());
    REQUIRE(rules.size() == 11);
```

and:

```cpp
TEST_CASE("allRules omits a rule disabled via config")
{
    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.rules.at("complexity.cyclomatic").enabled = false;

    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(config);

    REQUIRE(rules.size() == 9);
```

becomes:

```cpp
TEST_CASE("allRules omits a rule disabled via config")
{
    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.rules.at("complexity.cyclomatic").enabled = false;

    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(config);

    REQUIRE(rules.size() == 10);
```

- [ ] **Step 2: Run tests to verify they fail**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="memory.array-parameter-decay*"
```
Expected: FAIL — `ruleNamed` throws `std::runtime_error("Expected rule was not registered: memory.array-parameter-decay")`.

- [ ] **Step 3: Implement the rule**

In `src/rules/Rules.cpp`, inside the anonymous namespace, directly after the `UninitializedPointerRule` class definition and before the namespace's closing `} // namespace`, add:

```cpp
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

Then, in `allRules(...)`, add this block right before the function's final `return rules;` (right after the existing `memory.uninitialized-pointer` block):

```cpp
    if (const codeqa::config::RuleConfig& c = config.rules.at("memory.array-parameter-decay"); c.enabled)
    {
        rules.push_back(std::make_unique<ArrayParameterDecayRule>());
    }

```

- [ ] **Step 4: Wire the default config, `knownRuleIds()`, and the rule-count doc comment**

In `src/config/DefaultConfig.cpp`, add this line right before `return config;` in `defaultConfig()`, right after the existing `memory.uninitialized-pointer` line:

```cpp
    config.rules["memory.array-parameter-decay"] = thresholds(0, 0);
```

In `src/config/ConfigJson.cpp`, `knownRuleIds()` currently ends:

```cpp
        "security.unsafe-string-api",
        "memory.uninitialized-pointer"};
    return ids;
```

Change it to:

```cpp
        "security.unsafe-string-api",
        "memory.uninitialized-pointer",
        "memory.array-parameter-decay"};
    return ids;
```

In `include/codeqa/rules/Rules.h`, the doc comment above `allRules` currently reads:

```cpp
/// Requires config.rules to contain an entry for each of the 10 known rule ids --
```

Change `10` to `11`:

```cpp
/// Requires config.rules to contain an entry for each of the 11 known rule ids --
```

- [ ] **Step 5: Run the new tests to verify they pass**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="memory.array-parameter-decay*"
```
Expected: PASS (5 test cases, 0 failures).

- [ ] **Step 6: Update `DefaultConfigTest.cpp` and verify**

`tests/unit/DefaultConfigTest.cpp` currently ends with:

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

Replace it with:

```cpp
TEST_CASE("defaultConfig ships the eight Fase 7 thresholds plus the unsafe-string-api, uninitialized-pointer, and array-parameter-decay rules, all enabled, no header overrides")
{
    const codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();

    CHECK(config.headerOverrides.empty());
    REQUIRE(config.rules.size() == 11);

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
    checkRule(config, "memory.array-parameter-decay", 0, 0);
}
```

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="defaultConfig*"
```
Expected: PASS.

- [ ] **Step 7: Update the `ReportJsonTest.cpp` rule-count assertion**

In `tests/unit/ReportJsonTest.cpp:52`, this line:

```cpp
    CHECK(document.at("config").at("rules").size() == 10);
```

becomes:

```cpp
    CHECK(document.at("config").at("rules").size() == 11);
```

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="*ReportJson*"
```
Expected: PASS.

- [ ] **Step 8: Add the corpus stability row**

In `tests/corpus/validation/RULE-STABILITY.md`, add a new row at the end of the table (after the `memory.uninitialized-pointer` row):

```markdown
| `memory.array-parameter-decay` | `experimental` | — | — |
```

- [ ] **Step 9: Run the full MSVC test suite**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe
```
Expected: PASS, all test cases (0 failures) — in particular confirms `CorpusReportTest.cpp` needs no changes: grepping `tests/corpus/` for any array-shaped parameter (`grep -rn "\[\]" tests/corpus` and `grep -rn "\[[0-9]\+\]" tests/corpus` over `.c`/`.cpp` fixtures) finds none among function parameters, so no fixture's expected finding count changes. If the grep finds a real array parameter in a fixture, update that fixture's expected finding count instead of skipping this check.

- [ ] **Step 10: Commit**

```bash
git add src/rules/Rules.cpp include/codeqa/rules/Rules.h src/config/DefaultConfig.cpp src/config/ConfigJson.cpp tests/unit/RulesTest.cpp tests/unit/DefaultConfigTest.cpp tests/unit/ReportJsonTest.cpp tests/corpus/validation/RULE-STABILITY.md
git commit -m "feat: add memory.array-parameter-decay rule for array parameters that decay to pointers"
```

---

### Task 3: Wire the rule into the desktop GUI (rule list, limit-field visibility, tests)

**Files:**
- Modify: `src/desktop/ReportModels.cpp:17-30` (`orderedRuleIds()` — append the new id)
- Modify: `src/desktop/qml/pages/AnalyzePage.qml:177-213` (extend both `visible` conditions)
- Modify: `tests/desktop/AppControllerTest.cpp:219,220-230,263` (rowCount + expected id list, both call sites)

**Interfaces:**
- Consumes: rule id string `"memory.array-parameter-decay"` produced by Task 2 (must match exactly); the existing `ruleRow.model.id`, `warningLimitField`, `highLimitField` QML elements; the existing `orderedRuleIds()` static list shape in `ReportModels.cpp`.
- Produces: nothing consumed by a later task — this is the final task in the plan.

- [ ] **Step 1: Write the failing desktop test expectations**

`tests/desktop/AppControllerTest.cpp` has two assertions to update. First, around line 219:

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

becomes:

```cpp
    QCOMPARE(controller.rules()->rowCount(), 11);
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
        "memory.uninitialized-pointer",
        "memory.array-parameter-decay"};
```

Second, around line 263 (in `missingConfigurationUsesUnsavedDefaultsWithoutWriting`):

```cpp
    QCOMPARE(controller.rules()->rowCount(), 10);
```

becomes:

```cpp
    QCOMPARE(controller.rules()->rowCount(), 11);
```

- [ ] **Step 2: Run the desktop tests to verify they fail**

Run (MinGW/MSVC `buildQt` tree — required for anything under `src/desktop/` or `tests/desktop/`; all executables land directly at `buildQt\<name>.exe`, no `tests\` subdirectory):
```
cmake --build buildQt --target codeqa_desktop_tests
buildQt\codeqa_desktop_tests.exe
```
If this executable fails to start with no output (a known gap in a fresh worktree missing the Qt platform plugin — not a code issue), instead run it with the platform forced to offscreen and Qt's `bin` on `PATH`:
```
set QT_QPA_PLATFORM=offscreen
set PATH=C:\Qt\6.11.0\msvc2022_64\bin;%PATH%
buildQt\codeqa_desktop_tests.exe
```
Expected: FAIL on the two updated `rowCount()` assertions (still 10 in the model, since `orderedRuleIds()` hasn't been updated yet).

- [ ] **Step 3: Update `orderedRuleIds()`**

`src/desktop/ReportModels.cpp`'s `orderedRuleIds()` currently ends:

```cpp
        QStringLiteral("security.unsafe-string-api"),
        QStringLiteral("memory.uninitialized-pointer")};
    return ids;
```

Change it to:

```cpp
        QStringLiteral("security.unsafe-string-api"),
        QStringLiteral("memory.uninitialized-pointer"),
        QStringLiteral("memory.array-parameter-decay")};
    return ids;
```

- [ ] **Step 4: Hide the unused limit fields for this rule too**

In `src/desktop/qml/pages/AnalyzePage.qml`, both `TextField`s currently carry a two-line `visible` binding:

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

Extend both with a third clause (still one `visible:` property each, just a longer expression):

```qml
                        TextField {
                            id: warningLimitField
                            visible: ruleRow.model.id !== "security.unsafe-string-api"
                                && ruleRow.model.id !== "memory.uninitialized-pointer"
                                && ruleRow.model.id !== "memory.array-parameter-decay"
```

and

```qml
                        TextField {
                            id: highLimitField
                            visible: ruleRow.model.id !== "security.unsafe-string-api"
                                && ruleRow.model.id !== "memory.uninitialized-pointer"
                                && ruleRow.model.id !== "memory.array-parameter-decay"
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
(Use the same `QT_QPA_PLATFORM=offscreen` + `PATH` workaround from Step 2 if the executables produce no output.)
Expected: all three builds succeed, `codeqa_desktop_tests.exe` reports 0 failures, `codeqa_desktop_qml_smoke.exe` passes (confirms `AnalyzePage.qml` still parses and loads with no binding errors).

- [ ] **Step 6: Manual visual check (recommended, not build-blocking)**

Run `buildQt\codeqa_desktop.exe`, open any project, go to the Analyze page, and confirm: the `size.function-length` row still shows both limit fields; the `memory.array-parameter-decay` row (enabled by default) shows no limit fields, just its id and the enabled checkbox — same as the two prior pattern-based rules.

- [ ] **Step 7: Commit**

```bash
git add src/desktop/ReportModels.cpp src/desktop/qml/pages/AnalyzePage.qml tests/desktop/AppControllerTest.cpp
git commit -m "feat: wire memory.array-parameter-decay into the desktop rule list and hide its limit fields"
```

---

## Self-Review Notes

- **Spec coverage:** §4 (frontend change, both enums + both switches) → Task 1. §5 (rule implementation) → Task 2 Steps 1-5. §6 (config + full registration surface, all 12 points) → Task 1 Steps 3-6 (enum-extension surfaces 9-12) and Task 2 Steps 4, 6, 7 (rule-id-list surfaces 1-5) plus Task 3 Steps 3, 4 (surfaces 6-7) — every numbered surface from spec §6 has a corresponding step. §7 (testing) → Task 1 Steps 1, 8, 9; Task 2 Steps 1, 6, 7, 8, 9. §8 (acceptance criteria) → covered by the combination of Task 1's frontend tests (array/pointer/reference/function-pointer/value classification), Task 2's rule tests (named/unnamed/multiple/no-parameter-list), and Task 3 (GUI field hiding, desktop rule list row count). No gaps found.
- **Placeholder scan:** none found — every step carries the literal code or exact file content to write.
- **Type/name consistency:** `ParameterPassing::Array` is spelled identically across Task 1's two enum definitions, the detection logic, both translating/serializing switches, and all three of Task 1's tests. Rule id string `"memory.array-parameter-decay"` is identical across Task 2 (`id()`, `allRules`, `defaultConfig`, `knownRuleIds`, both test files, `ReportJsonTest.cpp`) and Task 3 (`orderedRuleIds`, both QML `visible` bindings, `AppControllerTest.cpp`). `ArrayParameterDecayRule` is defined once (Task 2 Step 3) and not renamed elsewhere. Confirmed against the actual current file contents (not assumed) by reading every touched file in full before writing this plan — including a real correction to the spec itself mid-planning (§4/§6 originally missed that `ParameterPassing` is defined twice with two translating switches; the spec file was corrected and recommitted before this plan was written, so plan and spec now agree).
