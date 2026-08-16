# Minimal Code Duplication Detection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Detect Type-1/Type-2 whole-function code clones across a CodeQA project and report them as a new `duplicates` section in both the text and JSON reports, without touching the `Rule`/`Finding` machinery.

**Architecture:** Tree-sitter leaf tokens inside each function's own range are normalized (identifiers/literals → placeholders) during structural extraction and stored on `CallableModel`. A new pure `codeqa::duplication` layer (sibling to `codeqa::metrics`/`codeqa::rules`/`codeqa::history`) groups callables project-wide by exact normalized-sequence equality. The result flows through the same pipeline as `model`/`metrics`/`findings` (`RunAnalysis` → `ReportDocument` → `toReportJson`/`renderText`) as a fourth, parallel, top-level field — never through `Rule`/`Finding`, since a duplicate is inherently a relationship between two locations, not a single function's verdict.

**Tech Stack:** C++17, tree-sitter (already vendored), nlohmann::json (already vendored), doctest (already vendored). No new dependencies.

**Spec:** `docs/superpowers/specs/2026-08-16-duplicacion-minima-design.md`

## Global Constraints

- C++17, builds under `CODEQA_WARNINGS_AS_ERRORS=ON` (GCC, Clang, and MSVC all must compile clean) — never expose `std::hash<...>`'s raw output in anything serialized, persisted, or asserted in a golden fixture; it has no cross-implementation stability guarantee. `std::hash` may only be used as an ephemeral, same-process, never-compared bucketing key (see Task 3).
- No new third-party dependencies. Everything in this plan uses the C++ standard library plus tree-sitter/nlohmann::json APIs already vendored in this repo.
- Match existing style exactly: Allman braces (opening brace on its own line for every function/class/namespace/control block), 4-space indentation, `[[nodiscard]]` on value-returning public functions, `} // namespace x::y` closing comments, free functions in an anonymous `namespace { }` for translation-unit-local helpers.
- Every new/changed public type or function goes in `include/codeqa/...`; implementation in the matching `src/...` path. Register every new `.cpp` in the exact `add_library(codeqa_core STATIC ...)` list in the top-level `CMakeLists.txt`, and every new test `.cpp` in `tests/CMakeLists.txt`'s `add_executable(codeqa_tests ...)` list — both are explicit file lists, not globs.
- Explicitly out of scope for every task below (per the spec, §2): Type-3/near-miss clones, sub-function/block-level duplication, quality-gate participation, snapshot/history new-vs-existing-vs-resolved diffing for duplicate groups, `codeqa::report::renderMarkdown` (Markdown output does not gain a duplicates section this phase), and any Qt/QML (`src/desktop/`) change. Do not touch `AnalysisPhase` (`include/codeqa/application/AnalysisProgress.h`) or any switch over it — `detectDuplicates` runs without its own progress phase, folded silently between "measuring functions" and "preparing report", specifically to avoid breaking the exact-phase-sequence test in `ApplicationFacadeTest.cpp` and any exhaustive `AnalysisPhase` switch in `src/desktop/`.
- Every task must leave the project fully building and every existing test passing — `RunAnalysisResult`, `ReportDocument`, `toReportJson`, and `renderText` are consumed by multiple call sites (both `src/` and `tests/`); a task that changes one of these types/signatures must update **every** existing call site in the same task, even where the "correct" data isn't available yet (an empty `codeqa::duplication::ProjectDuplicationModel{}` is a complete, valid, compiling value — not a placeholder). A later task then swaps the empty literal for the real value once it's available. This happens twice by design: `src/application/ApplicationFacade.cpp`'s `toReportJson` call (Task 4 adds the argument as `{}`, Task 8 swaps it for `analysis.duplicates`) and `src/cli/RunApplication.cpp`'s `renderText` call (Task 5 adds it as `{}`, Task 9 swaps it for `textAnalysis.duplicates`).

---

### Task 1: Normalized token data on `FunctionModel`/`CallableModel`

**Files:**
- Modify: `include/codeqa/frontend/StructuralModels.h:104-117` (`FunctionModel` struct)
- Modify: `include/codeqa/model/Models.h:117-129` (`CallableModel` struct)
- Modify: `src/frontend/StructuralExtractor.cpp` (insert helpers after line 161, modify `adaptFunction` at lines 564-607)
- Modify: `src/model/ProjectModelBuilder.cpp:336-346` (carry the two fields through)
- Test: `tests/unit/StructuralExtractorTest.cpp`
- Test: `tests/unit/ProjectModelBuilderTest.cpp`

**Interfaces:**
- Consumes: nothing new (tree-sitter `TSNode` API and `isNodeType`/`nodeText`/`ts_node_child`/`ts_node_child_count`, already defined earlier in `StructuralExtractor.cpp`).
- Produces: `codeqa::frontend::FunctionModel::normalizedTokenSequence` (`std::string`), `::normalizedTokenCount` (`std::size_t`); `codeqa::model::CallableModel::normalizedTokenSequence` (`std::string`), `::normalizedTokenCount` (`std::size_t`). Task 3 consumes both `model::CallableModel` fields.

- [ ] **Step 1: Write the failing tests in `StructuralExtractorTest.cpp`**

Add at the end of the file (after the existing `TEST_CASE`s, following the file's existing `TempSourceFile` + `parseSourceFile` pattern):

```cpp
TEST_CASE("normalizes identifiers into placeholders so a renamed clone compares equal")
{
    const TempSourceFile file{
        "normalize-identifiers.cpp",
        "int addValues(int first, int second)\n"
        "{\n"
        "    return first + second;\n"
        "}\n"
        "\n"
        "int addNumbers(int alpha, int beta)\n"
        "{\n"
        "    return alpha + beta;\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 2);
    CHECK(result.functions[0].normalizedTokenSequence == result.functions[1].normalizedTokenSequence);
    CHECK(result.functions[0].normalizedTokenCount > 0);
    CHECK(result.functions[0].normalizedTokenCount == result.functions[1].normalizedTokenCount);
}

TEST_CASE("normalizes numeric literals into the same placeholder regardless of value")
{
    const TempSourceFile file{
        "normalize-literals.cpp",
        "int returnOne()\n"
        "{\n"
        "    return 1;\n"
        "}\n"
        "\n"
        "int returnFortyTwo()\n"
        "{\n"
        "    return 42;\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 2);
    CHECK(result.functions[0].normalizedTokenSequence == result.functions[1].normalizedTokenSequence);
}

TEST_CASE("normalization is sensitive to a real structural change")
{
    const TempSourceFile file{
        "normalize-structural.cpp",
        "int addValues(int first, int second)\n"
        "{\n"
        "    return first + second;\n"
        "}\n"
        "\n"
        "int subtractValues(int first, int second)\n"
        "{\n"
        "    return first - second;\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 2);
    CHECK(result.functions[0].normalizedTokenSequence != result.functions[1].normalizedTokenSequence);
}

TEST_CASE("normalization ignores comments and whitespace-only differences")
{
    const TempSourceFile file{
        "normalize-comments.cpp",
        "int plainAdd(int first, int second)\n"
        "{\n"
        "    return first + second;\n"
        "}\n"
        "\n"
        "int /* commented */ commentedAdd( int   first , int second )\n"
        "{\n"
        "    return first + second; // inline comment\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 2);
    CHECK(result.functions[0].normalizedTokenSequence == result.functions[1].normalizedTokenSequence);
}
```

- [ ] **Step 2: Run the tests to verify they fail to compile**

Run: `cmake --build build --target codeqa_tests` (or the PowerShell equivalent from the project memory: `& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build --target codeqa_tests` if that's the configured generator).
Expected: compile error — `FunctionModel` has no member `normalizedTokenSequence`/`normalizedTokenCount`.

- [ ] **Step 3: Add the two fields to `FunctionModel`**

In `include/codeqa/frontend/StructuralModels.h`, replace:

```cpp
struct FunctionModel
{
    std::string name;
    FunctionKind kind{FunctionKind::Free};
    core::SourceRange range;
    std::vector<ParameterModel> parameters;
    bool isStatic{false};
    bool isVariadic{false};
    std::vector<LocalVariableModel> localVariables;
    std::vector<ControlFlowModel> controlFlow;
    std::vector<CallModel> calls;
    std::optional<core::SourceRange> parameterListRange;
    std::vector<core::SourceRange> statements;
};
```

with:

```cpp
struct FunctionModel
{
    std::string name;
    FunctionKind kind{FunctionKind::Free};
    core::SourceRange range;
    std::vector<ParameterModel> parameters;
    bool isStatic{false};
    bool isVariadic{false};
    std::vector<LocalVariableModel> localVariables;
    std::vector<ControlFlowModel> controlFlow;
    std::vector<CallModel> calls;
    std::optional<core::SourceRange> parameterListRange;
    std::vector<core::SourceRange> statements;
    std::string normalizedTokenSequence;
    std::size_t normalizedTokenCount{0};
};
```

(`<cstddef>` and `<string>` are already included at the top of this header.)

- [ ] **Step 4: Implement the normalization walk in `StructuralExtractor.cpp`**

Insert immediately after the `nodeText` function (ends at line 161, right before `std::string trim(std::string text)` at line 163):

```cpp
struct NormalizedTokens
{
    std::string sequence;
    std::size_t count{0};
};

// ponytail: walks `candidate`'s own subtree only -- a leading storage-class
// specifier like `static` that tree-sitter parks in a sibling/parent node
// (see sourceRangeForCandidate) is not included. Acceptable for Type-1/Type-2
// clone comparison, which cares about body structure, not storage class.
void appendNormalizedTokens(TSNode node, std::string_view source, NormalizedTokens& tokens)
{
    const uint32_t childCount = ts_node_child_count(node);

    if (childCount == 0)
    {
        if (isNodeType(node, "comment"))
        {
            return;
        }

        if (!tokens.sequence.empty())
        {
            tokens.sequence += ' ';
        }

        if (isNodeType(node, "identifier") || isNodeType(node, "field_identifier")
            || isNodeType(node, "type_identifier"))
        {
            tokens.sequence += "ID";
        }
        else if (isNodeType(node, "number_literal") || isNodeType(node, "string_literal")
            || isNodeType(node, "char_literal"))
        {
            tokens.sequence += "LIT";
        }
        else
        {
            tokens.sequence += nodeText(node, source);
        }

        ++tokens.count;
        return;
    }

    for (uint32_t index = 0; index < childCount; ++index)
    {
        appendNormalizedTokens(ts_node_child(node, index), source, tokens);
    }
}

NormalizedTokens normalizedTokensFor(TSNode node, std::string_view source)
{
    NormalizedTokens tokens;
    appendNormalizedTokens(node, source, tokens);
    return tokens;
}
```

Then in `adaptFunction` (currently lines 564-607), change the ending from:

```cpp
    const TSNode body = childForField(candidate, "body");

    if (!ts_node_is_null(body) && !subtreeContainsErrorOrMissing(body))
    {
        extractBodyStructure(body, source, path, model);
    }

    return model;
}
```

to:

```cpp
    const TSNode body = childForField(candidate, "body");

    if (!ts_node_is_null(body) && !subtreeContainsErrorOrMissing(body))
    {
        extractBodyStructure(body, source, path, model);
    }

    const NormalizedTokens normalized = normalizedTokensFor(candidate, source);
    model.normalizedTokenSequence = normalized.sequence;
    model.normalizedTokenCount = normalized.count;

    return model;
}
```

Note this runs unconditionally, even when `body` is null/unparseable — unlike `extractBodyStructure`. This is deliberate (spec §8): a duplicate is a structural comparison, not a per-symbol correctness judgment, so parse-damaged functions still get compared instead of silently disappearing from duplicate detection.

- [ ] **Step 5: Run the tests to verify they pass**

Run: `cmake --build build --target codeqa_tests` then run the built test binary filtering by name, e.g. `build/Debug/codeqa_tests.exe --test-case="normaliz*"` (adjust path per the local build directory).
Expected: all 4 new `TEST_CASE`s PASS.

- [ ] **Step 6: Write the failing test in `ProjectModelBuilderTest.cpp`**

Add after the existing `TEST_CASE("copies declared parameter types, parameter list ranges, comments and statements")` (which already builds a `frontend::FunctionModel` via aggregate init and asserts fields carried onto `CallableModel`):

```cpp
TEST_CASE("copies the normalized token sequence and count onto CallableModel")
{
    const std::filesystem::path rootPath{"project"};
    const std::filesystem::path sourcePath{"project/src/greet.cpp"};

    codeqa::frontend::ParsedFile sourceFile;
    sourceFile.path = sourcePath;
    sourceFile.language = codeqa::core::Language::Cpp;
    sourceFile.grammar = codeqa::frontend::Grammar::Cpp;
    sourceFile.parsed = true;
    sourceFile.functions.push_back(codeqa::frontend::FunctionModel{
        "greet",
        codeqa::frontend::FunctionKind::Free,
        rangeFor(sourcePath, 20, 60),
        {},
        false,
        false});
    sourceFile.functions[0].normalizedTokenSequence = "int ID ( ) { return LIT ; }";
    sourceFile.functions[0].normalizedTokenCount = 9;

    const codeqa::model::ProjectModel result = codeqa::model::ProjectModelBuilder{}.build(
        rootPath, std::vector<codeqa::frontend::ParsedFile>{sourceFile});

    REQUIRE(result.files.size() == 1);
    REQUIRE(result.files[0].callables.size() == 1);
    CHECK(result.files[0].callables[0].normalizedTokenSequence == "int ID ( ) { return LIT ; }");
    CHECK(result.files[0].callables[0].normalizedTokenCount == 9);
}
```

(Reuses the file's existing local `rangeFor` helper.)

- [ ] **Step 7: Run the test to verify it fails to compile**

Run: `cmake --build build --target codeqa_tests`.
Expected: compile error — `CallableModel` has no member `normalizedTokenSequence`/`normalizedTokenCount`.

- [ ] **Step 8: Add the fields to `CallableModel` and carry them through**

In `include/codeqa/model/Models.h`, replace:

```cpp
struct CallableModel
{
    SymbolIdentity symbol;
    CallableKind kind{CallableKind::FreeFunction};
    std::vector<ParameterModel> parameters;
    bool isStatic{false};
    bool isVariadic{false};
    std::vector<LocalVariableModel> localVariables;
    std::vector<ControlFlowModel> controlFlow;
    std::vector<CallModel> calls;
    std::optional<core::SourceRange> parameterListRange;
    std::vector<core::SourceRange> statements;
};
```

with:

```cpp
struct CallableModel
{
    SymbolIdentity symbol;
    CallableKind kind{CallableKind::FreeFunction};
    std::vector<ParameterModel> parameters;
    bool isStatic{false};
    bool isVariadic{false};
    std::vector<LocalVariableModel> localVariables;
    std::vector<ControlFlowModel> controlFlow;
    std::vector<CallModel> calls;
    std::optional<core::SourceRange> parameterListRange;
    std::vector<core::SourceRange> statements;
    std::string normalizedTokenSequence;
    std::size_t normalizedTokenCount{0};
};
```

In `src/model/ProjectModelBuilder.cpp`, change:

```cpp
        callable.parameterListRange = function.parameterListRange;
        callable.statements = function.statements;
```

to:

```cpp
        callable.parameterListRange = function.parameterListRange;
        callable.statements = function.statements;
        callable.normalizedTokenSequence = function.normalizedTokenSequence;
        callable.normalizedTokenCount = function.normalizedTokenCount;
```

- [ ] **Step 9: Run the tests to verify they pass**

Run: `cmake --build build --target codeqa_tests` then run the full suite: `ctest --test-dir build --output-on-failure`.
Expected: all tests PASS, including the new ones.

- [ ] **Step 10: Commit**

```bash
git add include/codeqa/frontend/StructuralModels.h include/codeqa/model/Models.h src/frontend/StructuralExtractor.cpp src/model/ProjectModelBuilder.cpp tests/unit/StructuralExtractorTest.cpp tests/unit/ProjectModelBuilderTest.cpp
git commit -m "feat: normalize function tokens for duplicate detection"
```

---

### Task 2: `DuplicationConfig` in the config layer

**Files:**
- Modify: `include/codeqa/config/ConfigModels.h`
- Modify: `src/config/MergeConfig.cpp`
- Modify: `src/config/ConfigJson.cpp`
- Test: `tests/unit/ConfigModelsTest.cpp`
- Test: `tests/unit/MergeConfigTest.cpp`
- Test: `tests/unit/ConfigJsonTest.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: `codeqa::config::DuplicationConfig{bool enabled{true}; std::size_t minNormalizedTokens{30};}`; `codeqa::config::ProjectConfig::duplication` (`DuplicationConfig`). Task 3's `detectDuplicates` and Task 6's `RunAnalysis::execute` consume `ProjectConfig::duplication`.

- [ ] **Step 1: Write the failing tests in `ConfigModelsTest.cpp`**

Add at the end of the file:

```cpp
TEST_CASE("DuplicationConfig defaults to enabled with a 30-token floor")
{
    codeqa::config::DuplicationConfig config;
    CHECK(config.enabled);
    CHECK(config.minNormalizedTokens == 30);
}

TEST_CASE("ProjectConfig defaults duplication to enabled with a 30-token floor")
{
    codeqa::config::ProjectConfig config;
    CHECK(config.duplication.enabled);
    CHECK(config.duplication.minNormalizedTokens == 30);
}
```

- [ ] **Step 2: Run the tests to verify they fail to compile**

Run: `cmake --build build --target codeqa_tests`.
Expected: compile error — no member `DuplicationConfig` in namespace `codeqa::config`.

- [ ] **Step 3: Add `DuplicationConfig` and wire it into `ProjectConfig`**

In `include/codeqa/config/ConfigModels.h`, add after the `QualityGateConfig` struct (before `ProjectConfig`):

```cpp
struct DuplicationConfig
{
    bool enabled{true};
    std::size_t minNormalizedTokens{30};
};
```

Add `#include <cstddef>` to the top of the file (currently only `<map>`, `<string>`, `<vector>` plus the two `codeqa/core` headers — `std::size_t` needs it explicitly). Then change:

```cpp
struct ProjectConfig
{
    std::vector<HeaderOverrideConfig> headerOverrides;
    std::map<std::string, RuleConfig> rules;
    QualityGateConfig qualityGate;
};
```

to:

```cpp
struct ProjectConfig
{
    std::vector<HeaderOverrideConfig> headerOverrides;
    std::map<std::string, RuleConfig> rules;
    QualityGateConfig qualityGate;
    DuplicationConfig duplication;
};
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cmake --build build --target codeqa_tests` then run the new cases.
Expected: both new `TEST_CASE`s PASS. (`defaultConfig()` needs no change — exactly like `qualityGate`, it never touches `.duplication`, relying on `DuplicationConfig`'s own in-class defaults, which is the established, documented convention for non-rule-id config blocks in this codebase.)

- [ ] **Step 5: Write the failing test in `MergeConfigTest.cpp`**

Add at the end of the file:

```cpp
TEST_CASE("mergeConfig replaces duplication wholesale with the user's value")
{
    codeqa::config::ProjectConfig defaults;
    defaults.duplication.enabled = true;
    defaults.duplication.minNormalizedTokens = 30;

    codeqa::config::ProjectConfig user;
    user.duplication.enabled = false;
    user.duplication.minNormalizedTokens = 60;

    const codeqa::config::ProjectConfig merged = codeqa::config::mergeConfig(defaults, user);

    CHECK_FALSE(merged.duplication.enabled);
    CHECK(merged.duplication.minNormalizedTokens == 60);
}
```

- [ ] **Step 6: Run the test to verify it fails**

Run: `cmake --build build --target codeqa_tests` then run the new case.
Expected: FAIL — `merged.duplication.enabled` is still `true` (defaults win because `mergeConfig` doesn't touch `.duplication` yet).

- [ ] **Step 7: Wire the merge**

In `src/config/MergeConfig.cpp`, change:

```cpp
    merged.headerOverrides = user.headerOverrides;
    merged.qualityGate = user.qualityGate;

    return merged;
```

to:

```cpp
    merged.headerOverrides = user.headerOverrides;
    merged.qualityGate = user.qualityGate;
    merged.duplication = user.duplication;

    return merged;
```

- [ ] **Step 8: Run the test to verify it passes**

Run: `cmake --build build --target codeqa_tests` then run the new case.
Expected: PASS.

- [ ] **Step 9: Write the failing tests in `ConfigJsonTest.cpp`**

Add at the end of the file (mirroring the existing `qualityGate` test group exactly):

```cpp
TEST_CASE("parseConfig accepts a document with duplication")
{
    const std::string text = R"({ "duplication": { "enabled": false, "minNormalizedTokens": 50 } })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.config.has_value());
    CHECK_FALSE(result.config->duplication.enabled);
    CHECK(result.config->duplication.minNormalizedTokens == 50);
}

TEST_CASE("parseConfig defaults duplication fields when the object is present but empty")
{
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(R"({ "duplication": {} })");

    REQUIRE(result.config.has_value());
    CHECK(result.config->duplication.enabled);
    CHECK(result.config->duplication.minNormalizedTokens == 30);
}

TEST_CASE("parseConfig defaults duplication fields when duplication is absent")
{
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig("{}");

    REQUIRE(result.config.has_value());
    CHECK(result.config->duplication.enabled);
    CHECK(result.config->duplication.minNormalizedTokens == 30);
}

TEST_CASE("parseConfig reports CONFIG011 for a non-positive duplication.minNormalizedTokens")
{
    const std::string text = R"({ "duplication": { "minNormalizedTokens": 0 } })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG011") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG002 for an unknown key inside duplication")
{
    const std::string text = R"({ "duplication": { "enabled": true, "note": "x" } })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG002") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG002 for an unknown top-level key next to duplication")
{
    const std::string text = R"({ "duplication": { "enabled": true }, "outputFormat": "json" })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG002") != std::string::npos);
}

TEST_CASE("toJson round-trips duplication through parseConfig")
{
    codeqa::config::ProjectConfig original;
    original.duplication.enabled = false;
    original.duplication.minNormalizedTokens = 45;

    const std::string serialized = codeqa::config::toJson(original).dump();
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(serialized);

    REQUIRE(result.config.has_value());
    CHECK_FALSE(result.config->duplication.enabled);
    CHECK(result.config->duplication.minNormalizedTokens == 45);
}
```

- [ ] **Step 10: Run the tests to verify they fail**

Run: `cmake --build build --target codeqa_tests` then run the new cases.
Expected: the first two/general-shape tests FAIL with a CONFIG002 "unknown top-level key 'config.duplication'" error (since `parseConfig` doesn't recognize the key yet); the CONFIG011 test FAILS because that error can never be reached yet.

- [ ] **Step 11: Wire parsing and serialization in `ConfigJson.cpp`**

Change the top-level key allowlist from:

```cpp
        requireKeysSubset(document, {"languages", "rules", "qualityGate"}, "config");
```

to:

```cpp
        requireKeysSubset(document, {"languages", "rules", "qualityGate", "duplication"}, "config");
```

Add a new `parseDuplication` helper right after `parseQualityGate` (still inside the anonymous namespace, before the closing `} // namespace`):

```cpp
DuplicationConfig parseDuplication(const Json& duplication)
{
    requireKeysSubset(duplication, {"enabled", "minNormalizedTokens"}, "duplication");

    DuplicationConfig config;

    if (duplication.contains("enabled"))
    {
        config.enabled = requireBool(duplication, "enabled", "duplication");
    }

    if (duplication.contains("minNormalizedTokens"))
    {
        const long long value = requireInteger(duplication, "minNormalizedTokens", "duplication");

        if (value <= 0)
        {
            throw ParseFailure(
                "Error [CONFIG011]: 'duplication.minNormalizedTokens' must be a positive integer, got "
                + std::to_string(value) + ".");
        }

        config.minNormalizedTokens = static_cast<std::size_t>(value);
    }

    return config;
}
```

In `parseConfig`, add after the `qualityGate` block (after its closing `}`, before `return ParseConfigResult{config, std::nullopt};`):

```cpp
        if (document.contains("duplication"))
        {
            const Json& duplication = document.at("duplication");
            requireObject(duplication, "duplication");
            config.duplication = parseDuplication(duplication);
        }
```

In `toJson`, add after the `qualityGate` assignment:

```cpp
    document["duplication"] = {
        {"enabled", config.duplication.enabled},
        {"minNormalizedTokens", config.duplication.minNormalizedTokens}};
```

- [ ] **Step 12: Run the tests to verify they pass**

Run: `cmake --build build --target codeqa_tests` then run the full suite: `ctest --test-dir build --output-on-failure`.
Expected: all tests PASS.

- [ ] **Step 13: Commit**

```bash
git add include/codeqa/config/ConfigModels.h src/config/MergeConfig.cpp src/config/ConfigJson.cpp tests/unit/ConfigModelsTest.cpp tests/unit/MergeConfigTest.cpp tests/unit/ConfigJsonTest.cpp
git commit -m "feat: add duplication config block"
```

---

### Task 3: `codeqa::duplication` engine

**Files:**
- Create: `include/codeqa/duplication/DuplicationModels.h`
- Create: `include/codeqa/duplication/DetectDuplicates.h`
- Create: `src/duplication/DetectDuplicates.cpp`
- Test: `tests/unit/DetectDuplicatesTest.cpp`
- Modify: `CMakeLists.txt` (register the new `.cpp`)
- Modify: `tests/CMakeLists.txt` (register the new test `.cpp`)

**Interfaces:**
- Consumes: `codeqa::model::ProjectModel`/`FileModel`/`CallableModel` (Task 1's `normalizedTokenSequence`/`normalizedTokenCount`), `codeqa::config::DuplicationConfig` (Task 2).
- Produces: `codeqa::duplication::DuplicateMember{std::string qualifiedName; codeqa::core::SourceRange range;}`; `codeqa::duplication::DuplicateGroup{std::vector<DuplicateMember> members;}` (`size() >= 2` always); `codeqa::duplication::ProjectDuplicationModel{std::vector<DuplicateGroup> groups;}`; free function `codeqa::duplication::detectDuplicates(const codeqa::model::ProjectModel&, const codeqa::config::DuplicationConfig&) -> ProjectDuplicationModel`. Tasks 4, 5, 6, 10 all consume these.

- [ ] **Step 1: Create the model header**

Create `include/codeqa/duplication/DuplicationModels.h`:

```cpp
#pragma once

#include "codeqa/core/SourceRange.h"

#include <string>
#include <vector>

namespace codeqa::duplication
{

struct DuplicateMember
{
    std::string qualifiedName;
    codeqa::core::SourceRange range;
};

struct DuplicateGroup
{
    std::vector<DuplicateMember> members;
};

struct ProjectDuplicationModel
{
    std::vector<DuplicateGroup> groups;
};

} // namespace codeqa::duplication
```

(No test needed for this step in isolation — it's a plain aggregate header, exercised by every test in the steps below.)

- [ ] **Step 2: Create the function header**

Create `include/codeqa/duplication/DetectDuplicates.h`:

```cpp
#pragma once

#include "codeqa/config/ConfigModels.h"
#include "codeqa/duplication/DuplicationModels.h"
#include "codeqa/model/Models.h"

namespace codeqa::duplication
{

[[nodiscard]] ProjectDuplicationModel detectDuplicates(
    const codeqa::model::ProjectModel& project, const codeqa::config::DuplicationConfig& config);

} // namespace codeqa::duplication
```

- [ ] **Step 3: Register the new files in CMake before writing the implementation**

In `CMakeLists.txt`, add `src/duplication/DetectDuplicates.cpp` to `add_library(codeqa_core STATIC ...)`, right after `src/rules/Rules.cpp`:

```cmake
    src/rules/Rules.cpp
    src/duplication/DetectDuplicates.cpp
```

In `tests/CMakeLists.txt`, add `unit/DetectDuplicatesTest.cpp` to `add_executable(codeqa_tests ...)`, right after `unit/RulesTest.cpp` (or any existing entry — exact position among the alphabetically-unordered list doesn't matter, keep it near other `unit/*Test.cpp` entries for readability).

- [ ] **Step 4: Write the failing test file**

Create `tests/unit/DetectDuplicatesTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/duplication/DetectDuplicates.h"

#include <filesystem>
#include <string>

namespace
{

codeqa::core::SourceRange rangeAt(const std::filesystem::path& path, std::size_t offset)
{
    return codeqa::core::SourceRange{
        codeqa::core::SourceLocation{path, 1, offset + 1, offset},
        codeqa::core::SourceLocation{path, 1, offset + 41, offset + 40}};
}

codeqa::model::CallableModel makeCallable(
    const std::string& qualifiedName, const std::filesystem::path& path, std::size_t offset,
    const std::string& normalizedTokenSequence, std::size_t normalizedTokenCount)
{
    codeqa::model::CallableModel callable;
    callable.symbol.name = qualifiedName;
    callable.symbol.qualifiedName = qualifiedName;
    callable.symbol.range = rangeAt(path, offset);
    callable.normalizedTokenSequence = normalizedTokenSequence;
    callable.normalizedTokenCount = normalizedTokenCount;
    return callable;
}

} // namespace

TEST_CASE("detectDuplicates groups two callables with an identical normalized sequence")
{
    codeqa::model::ProjectModel project;
    codeqa::model::FileModel file;
    file.path = "project/src/a.cpp";
    file.callables.push_back(makeCallable("first", file.path, 0, "int ID ( int ID ) { return ID ; }", 9));
    file.callables.push_back(makeCallable("second", file.path, 100, "int ID ( int ID ) { return ID ; }", 9));
    project.files.push_back(file);

    codeqa::config::DuplicationConfig config;
    config.minNormalizedTokens = 5;

    const codeqa::duplication::ProjectDuplicationModel result =
        codeqa::duplication::detectDuplicates(project, config);

    REQUIRE(result.groups.size() == 1);
    REQUIRE(result.groups.front().members.size() == 2);
    CHECK(result.groups.front().members[0].qualifiedName == "first");
    CHECK(result.groups.front().members[1].qualifiedName == "second");
}

TEST_CASE("detectDuplicates ignores a callable below the minimum token floor")
{
    codeqa::model::ProjectModel project;
    codeqa::model::FileModel file;
    file.path = "project/src/a.cpp";
    file.callables.push_back(makeCallable("tiny", file.path, 0, "int ID ( ) { return LIT ; }", 7));
    project.files.push_back(file);

    codeqa::config::DuplicationConfig config;
    config.minNormalizedTokens = 30;

    const codeqa::duplication::ProjectDuplicationModel result =
        codeqa::duplication::detectDuplicates(project, config);

    CHECK(result.groups.empty());
}

TEST_CASE("detectDuplicates does not group two callables with different normalized sequences")
{
    codeqa::model::ProjectModel project;
    codeqa::model::FileModel file;
    file.path = "project/src/a.cpp";
    file.callables.push_back(makeCallable("add", file.path, 0, "int ID ( int ID ) { return ID + ID ; }", 10));
    file.callables.push_back(makeCallable("sub", file.path, 100, "int ID ( int ID ) { return ID - ID ; }", 10));
    project.files.push_back(file);

    codeqa::config::DuplicationConfig config;
    config.minNormalizedTokens = 5;

    const codeqa::duplication::ProjectDuplicationModel result =
        codeqa::duplication::detectDuplicates(project, config);

    CHECK(result.groups.empty());
}

TEST_CASE("detectDuplicates finds duplicates across two different files")
{
    codeqa::model::ProjectModel project;
    codeqa::model::FileModel fileA;
    fileA.path = "project/src/a.cpp";
    fileA.callables.push_back(makeCallable("a::run", fileA.path, 0, "int ID ( ) { return LIT ; }", 7));
    codeqa::model::FileModel fileB;
    fileB.path = "project/src/b.cpp";
    fileB.callables.push_back(makeCallable("b::run", fileB.path, 0, "int ID ( ) { return LIT ; }", 7));
    project.files.push_back(fileA);
    project.files.push_back(fileB);

    codeqa::config::DuplicationConfig config;
    config.minNormalizedTokens = 5;

    const codeqa::duplication::ProjectDuplicationModel result =
        codeqa::duplication::detectDuplicates(project, config);

    REQUIRE(result.groups.size() == 1);
    CHECK(result.groups.front().members.size() == 2);
}

TEST_CASE("detectDuplicates groups three callables that all share the same sequence")
{
    codeqa::model::ProjectModel project;
    codeqa::model::FileModel file;
    file.path = "project/src/a.cpp";
    file.callables.push_back(makeCallable("one", file.path, 0, "int ID ( ) { return LIT ; }", 7));
    file.callables.push_back(makeCallable("two", file.path, 100, "int ID ( ) { return LIT ; }", 7));
    file.callables.push_back(makeCallable("three", file.path, 200, "int ID ( ) { return LIT ; }", 7));
    project.files.push_back(file);

    codeqa::config::DuplicationConfig config;
    config.minNormalizedTokens = 5;

    const codeqa::duplication::ProjectDuplicationModel result =
        codeqa::duplication::detectDuplicates(project, config);

    REQUIRE(result.groups.size() == 1);
    CHECK(result.groups.front().members.size() == 3);
}

TEST_CASE("detectDuplicates returns nothing when disabled")
{
    codeqa::model::ProjectModel project;
    codeqa::model::FileModel file;
    file.path = "project/src/a.cpp";
    file.callables.push_back(makeCallable("first", file.path, 0, "int ID ( ) { return LIT ; }", 7));
    file.callables.push_back(makeCallable("second", file.path, 100, "int ID ( ) { return LIT ; }", 7));
    project.files.push_back(file);

    codeqa::config::DuplicationConfig config;
    config.enabled = false;
    config.minNormalizedTokens = 1;

    const codeqa::duplication::ProjectDuplicationModel result =
        codeqa::duplication::detectDuplicates(project, config);

    CHECK(result.groups.empty());
}
```

- [ ] **Step 5: Run the tests to verify they fail to link**

Run: `cmake --build build --target codeqa_tests`.
Expected: link error — `codeqa::duplication::detectDuplicates` is declared but not defined.

- [ ] **Step 6: Implement `detectDuplicates`**

Create `src/duplication/DetectDuplicates.cpp`:

```cpp
#include "codeqa/duplication/DetectDuplicates.h"

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <utility>

namespace codeqa::duplication
{

namespace
{

struct Candidate
{
    std::string qualifiedName;
    codeqa::core::SourceRange range;
    const std::string* normalizedTokenSequence{nullptr};
};

bool byLocation(const DuplicateMember& left, const DuplicateMember& right)
{
    if (left.range.begin.filePath != right.range.begin.filePath)
    {
        return left.range.begin.filePath < right.range.begin.filePath;
    }

    return left.range.begin.offset < right.range.begin.offset;
}

} // namespace

ProjectDuplicationModel detectDuplicates(
    const codeqa::model::ProjectModel& project, const codeqa::config::DuplicationConfig& config)
{
    ProjectDuplicationModel result;

    if (!config.enabled)
    {
        return result;
    }

    std::vector<Candidate> candidates;

    for (const codeqa::model::FileModel& file : project.files)
    {
        for (const codeqa::model::CallableModel& callable : file.callables)
        {
            if (callable.normalizedTokenCount < config.minNormalizedTokens)
            {
                continue;
            }

            candidates.push_back(Candidate{
                callable.symbol.qualifiedName, callable.symbol.range, &callable.normalizedTokenSequence});
        }
    }

    // Bucket by an in-process hash purely as a fast pre-filter -- std::hash has no
    // cross-implementation stability guarantee, so it is never serialized or compared
    // across runs. Membership within a bucket is always re-verified below by full
    // string equality, so a hash collision cannot fabricate a false duplicate group.
    std::unordered_map<std::size_t, std::vector<std::size_t>> buckets;

    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        buckets[std::hash<std::string>{}(*candidates[index].normalizedTokenSequence)].push_back(index);
    }

    for (const auto& [hash, bucket] : buckets)
    {
        std::vector<bool> grouped(bucket.size(), false);

        for (std::size_t i = 0; i < bucket.size(); ++i)
        {
            if (grouped[i])
            {
                continue;
            }

            const Candidate& base = candidates[bucket[i]];
            DuplicateGroup group;
            group.members.push_back(DuplicateMember{base.qualifiedName, base.range});
            grouped[i] = true;

            for (std::size_t j = i + 1; j < bucket.size(); ++j)
            {
                const Candidate& other = candidates[bucket[j]];
                if (!grouped[j] && *other.normalizedTokenSequence == *base.normalizedTokenSequence)
                {
                    group.members.push_back(DuplicateMember{other.qualifiedName, other.range});
                    grouped[j] = true;
                }
            }

            if (group.members.size() >= 2)
            {
                std::sort(group.members.begin(), group.members.end(), byLocation);
                result.groups.push_back(std::move(group));
            }
        }
    }

    std::sort(result.groups.begin(), result.groups.end(), [](const DuplicateGroup& left, const DuplicateGroup& right) {
        return byLocation(left.members.front(), right.members.front());
    });

    return result;
}

} // namespace codeqa::duplication
```

- [ ] **Step 7: Run the tests to verify they pass**

Run: `cmake --build build --target codeqa_tests` then run the full suite: `ctest --test-dir build --output-on-failure`.
Expected: all tests PASS, including all 6 new `DetectDuplicatesTest.cpp` cases.

- [ ] **Step 8: Commit**

```bash
git add CMakeLists.txt tests/CMakeLists.txt include/codeqa/duplication src/duplication tests/unit/DetectDuplicatesTest.cpp
git commit -m "feat: add codeqa::duplication engine"
```

---

### Task 4: JSON report — `duplicates` section

**Files:**
- Modify: `include/codeqa/report/ReportJson.h`
- Modify: `src/report/ReportJson.cpp`
- Modify: `src/application/ApplicationFacade.cpp:192-193` (update the one production call site)
- Modify: `tests/unit/CorpusReportTest.cpp:36-37`
- Modify: `tests/unit/ReportDocumentTest.cpp:67`
- Modify: `tests/unit/ReportJsonTest.cpp`

**Interfaces:**
- Consumes: `codeqa::duplication::ProjectDuplicationModel` (Task 3).
- Produces: `toReportJson`'s new 4th parameter (`const codeqa::duplication::ProjectDuplicationModel& duplicates`, inserted between `findings` and `config`) and its `document["duplicates"]` JSON array. Task 8 supplies the real value at the one production call site (this task uses an empty placeholder there, per Global Constraints).

- [ ] **Step 1: Write the failing tests in `ReportJsonTest.cpp`**

Add at the end of the file:

```cpp
TEST_CASE("toReportJson includes a duplicates section with relativized paths")
{
    codeqa::model::ProjectModel model;
    model.rootPath = std::filesystem::path{"project"};
    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::rules::ProjectFindingsModel findings;

    codeqa::duplication::ProjectDuplicationModel duplicates;
    codeqa::duplication::DuplicateGroup group;
    group.members.push_back(codeqa::duplication::DuplicateMember{
        "first",
        codeqa::core::SourceRange{
            codeqa::core::SourceLocation{model.rootPath / "src" / "a.cpp", 1, 1, 0},
            codeqa::core::SourceLocation{model.rootPath / "src" / "a.cpp", 4, 1, 40}}});
    group.members.push_back(codeqa::duplication::DuplicateMember{
        "second",
        codeqa::core::SourceRange{
            codeqa::core::SourceLocation{model.rootPath / "src" / "b.cpp", 1, 1, 0},
            codeqa::core::SourceLocation{model.rootPath / "src" / "b.cpp", 4, 1, 40}}});
    duplicates.groups.push_back(group);

    const codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    const nlohmann::json document =
        codeqa::report::toReportJson(model, metrics, findings, duplicates, config, "9.9.9");

    REQUIRE(document.at("duplicates").size() == 1);
    const nlohmann::json& groupDocument = document.at("duplicates")[0];
    REQUIRE(groupDocument.at("members").size() == 2);
    CHECK(groupDocument.at("members")[0].at("qualifiedName").get<std::string>() == "first");
    CHECK(groupDocument.at("members")[0].at("path").get<std::string>() == "src/a.cpp");
    CHECK(groupDocument.at("members")[1].at("path").get<std::string>() == "src/b.cpp");
}

TEST_CASE("toReportJson emits an empty duplicates array when there are no groups")
{
    codeqa::model::ProjectModel model;
    model.rootPath = std::filesystem::path{"project"};
    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::rules::ProjectFindingsModel findings;
    const codeqa::duplication::ProjectDuplicationModel duplicates;

    const nlohmann::json document = codeqa::report::toReportJson(
        model, metrics, findings, duplicates, codeqa::config::defaultConfig(), "9.9.9");

    CHECK(document.at("duplicates").empty());
}
```

Then update the file's 3 existing `toReportJson` call sites to insert `codeqa::duplication::ProjectDuplicationModel{}` as the 4th argument. They are not all identical text, so this is two separate edits:

1. At (currently) line 46, replace `findings, config, "9.9.9");` with `findings, codeqa::duplication::ProjectDuplicationModel{}, config, "9.9.9");` (this exact text, with the `"9.9.9"` version literal, appears only once in the file).
2. At (currently) lines 87 and 110, both calls end in the identical substring `findings, codeqa::config::defaultConfig(), "0.0.0")` — replace it, with `replace_all`, with `findings, codeqa::duplication::ProjectDuplicationModel{}, codeqa::config::defaultConfig(), "0.0.0")`. This substring is specific enough (the `"0.0.0"` version literal plus the exact call shape) that it cannot match unrelated code; verify it matches exactly 2 occurrences before applying.

- [ ] **Step 2: Run the tests to verify they fail to compile**

Run: `cmake --build build --target codeqa_tests`.
Expected: compile error — `toReportJson` called/declared with the wrong number of arguments.

- [ ] **Step 3: Extend `toReportJson`'s signature and implementation**

In `include/codeqa/report/ReportJson.h`, add `#include "codeqa/duplication/DuplicationModels.h"` to the includes, and change:

```cpp
[[nodiscard]] nlohmann::json toReportJson(
    const codeqa::model::ProjectModel& model,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::rules::ProjectFindingsModel& findings,
    const codeqa::config::ProjectConfig& config,
    const std::string& toolVersion);
```

to:

```cpp
[[nodiscard]] nlohmann::json toReportJson(
    const codeqa::model::ProjectModel& model,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::rules::ProjectFindingsModel& findings,
    const codeqa::duplication::ProjectDuplicationModel& duplicates,
    const codeqa::config::ProjectConfig& config,
    const std::string& toolVersion);
```

In `src/report/ReportJson.cpp`, add a new helper right after `findingJson` (before the closing `} // namespace`):

```cpp
Json duplicateGroupJson(const codeqa::duplication::DuplicateGroup& group, const std::filesystem::path& rootPath)
{
    Json members = Json::array();

    for (const codeqa::duplication::DuplicateMember& member : group.members)
    {
        members.push_back(Json{
            {"qualifiedName", member.qualifiedName},
            {"path", std::filesystem::relative(member.range.begin.filePath, rootPath).generic_string()},
            {"range", {{"begin", locationJson(member.range.begin)}, {"end", locationJson(member.range.end)}}}});
    }

    return Json{{"members", members}};
}
```

Change the function signature from:

```cpp
nlohmann::json toReportJson(
    const codeqa::model::ProjectModel& model,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::rules::ProjectFindingsModel& findings,
    const codeqa::config::ProjectConfig& config,
    const std::string& toolVersion)
{
```

to:

```cpp
nlohmann::json toReportJson(
    const codeqa::model::ProjectModel& model,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::rules::ProjectFindingsModel& findings,
    const codeqa::duplication::ProjectDuplicationModel& duplicates,
    const codeqa::config::ProjectConfig& config,
    const std::string& toolVersion)
{
```

and add, right after the closing `}` of the `for (std::size_t index = 0; index < model.files.size(); ++index)` loop (after `document["files"].push_back(std::move(fileDocument));` and its enclosing loop's `}`), before `return document;`:

```cpp
    document["duplicates"] = Json::array();
    for (const codeqa::duplication::DuplicateGroup& group : duplicates.groups)
    {
        document["duplicates"].push_back(duplicateGroupJson(group, model.rootPath));
    }
```

- [ ] **Step 4: Update the remaining call sites**

In `src/application/ApplicationFacade.cpp`, change:

```cpp
        result.reportJson = codeqa::report::toReportJson(
            analysis.model, analysis.metrics, analysis.findings, request.config, toolVersion);
```

to:

```cpp
        result.reportJson = codeqa::report::toReportJson(
            analysis.model, analysis.metrics, analysis.findings, codeqa::duplication::ProjectDuplicationModel{},
            request.config, toolVersion);
```

(Task 8 replaces the empty placeholder with `analysis.duplicates` once it exists.) Add `#include "codeqa/duplication/DuplicationModels.h"` to this file's includes if not already pulled in transitively (check first — `ApplicationFacade.h` already includes `ReportDocumentFactory.h` → `ReportDocument.h`, which after Task 7 will include it transitively; for this task, add the include explicitly since Task 7 hasn't run yet).

In `tests/unit/CorpusReportTest.cpp`, change:

```cpp
    const nlohmann::json document =
        codeqa::report::toReportJson(result.model, result.metrics, result.findings, request.config, "test");
```

to:

```cpp
    const nlohmann::json document = codeqa::report::toReportJson(
        result.model, result.metrics, result.findings, codeqa::duplication::ProjectDuplicationModel{},
        request.config, "test");
```

In `tests/unit/ReportDocumentTest.cpp`, change:

```cpp
    return codeqa::report::toReportJson(model, metrics, findings, codeqa::config::defaultConfig(), "9.9.9");
```

to:

```cpp
    return codeqa::report::toReportJson(
        model, metrics, findings, codeqa::duplication::ProjectDuplicationModel{}, codeqa::config::defaultConfig(),
        "9.9.9");
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `cmake --build build --target codeqa_tests` then run the full suite: `ctest --test-dir build --output-on-failure`.
Expected: all tests PASS.

- [ ] **Step 6: Commit**

```bash
git add include/codeqa/report/ReportJson.h src/report/ReportJson.cpp src/application/ApplicationFacade.cpp tests/unit/CorpusReportTest.cpp tests/unit/ReportDocumentTest.cpp tests/unit/ReportJsonTest.cpp
git commit -m "feat: add duplicates section to the JSON report"
```

---

### Task 5: Text report — "Duplicated code" section

**Files:**
- Modify: `include/codeqa/report/TextReport.h`
- Modify: `src/report/TextReport.cpp`
- Modify: `src/cli/RunApplication.cpp:753-756` (update the one production call site)
- Modify: `tests/unit/TextReportTest.cpp`

**Interfaces:**
- Consumes: `codeqa::duplication::ProjectDuplicationModel` (Task 3).
- Produces: `renderText`'s new 5th parameter (`const codeqa::duplication::ProjectDuplicationModel& duplicates`). Task 9 supplies the real value at the one production call site (this task uses an empty placeholder there).

- [ ] **Step 1: Write the failing tests in `TextReportTest.cpp`**

Add at the end of the file:

```cpp
TEST_CASE("renderText omits the duplicated code section when there are no groups")
{
    codeqa::model::ProjectModel model;
    model.rootPath = "project";
    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::rules::ProjectFindingsModel findings;
    const codeqa::duplication::ProjectDuplicationModel duplicates;

    std::ostringstream output;
    codeqa::report::renderText(output, model, metrics, findings, duplicates);

    CHECK(output.str().find("Duplicated code") == std::string::npos);
}

TEST_CASE("renderText lists each duplicate group with its members")
{
    codeqa::model::ProjectModel model;
    model.rootPath = "project";
    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::rules::ProjectFindingsModel findings;

    codeqa::duplication::ProjectDuplicationModel duplicates;
    codeqa::duplication::DuplicateGroup group;
    group.members.push_back(codeqa::duplication::DuplicateMember{
        "first",
        codeqa::core::SourceRange{
            codeqa::core::SourceLocation{model.rootPath / "src" / "a.cpp", 3, 1, 20},
            codeqa::core::SourceLocation{model.rootPath / "src" / "a.cpp", 6, 1, 60}}});
    group.members.push_back(codeqa::duplication::DuplicateMember{
        "second",
        codeqa::core::SourceRange{
            codeqa::core::SourceLocation{model.rootPath / "src" / "b.cpp", 8, 1, 100},
            codeqa::core::SourceLocation{model.rootPath / "src" / "b.cpp", 11, 1, 140}}});
    duplicates.groups.push_back(group);

    std::ostringstream output;
    codeqa::report::renderText(output, model, metrics, findings, duplicates);
    const std::string text = output.str();

    CHECK(text.find("Duplicated code") != std::string::npos);
    CHECK(text.find("first") != std::string::npos);
    CHECK(text.find("second") != std::string::npos);
    CHECK(text.find("src/a.cpp:3") != std::string::npos);
    CHECK(text.find("src/b.cpp:8") != std::string::npos);
}
```

Then update the file's 3 existing calls to `codeqa::report::renderText(output, model, metrics, findings);` (currently identical text at 3 locations) with one `replace_all` edit: replace `codeqa::report::renderText(output, model, metrics, findings);` with `codeqa::report::renderText(output, model, metrics, findings, codeqa::duplication::ProjectDuplicationModel{});`.

- [ ] **Step 2: Run the tests to verify they fail to compile**

Run: `cmake --build build --target codeqa_tests`.
Expected: compile error — `renderText` called/declared with the wrong number of arguments.

- [ ] **Step 3: Extend `renderText`'s signature and implementation**

In `include/codeqa/report/TextReport.h`, add `#include "codeqa/duplication/DuplicationModels.h"`, and change:

```cpp
void renderText(
    std::ostream& output,
    const codeqa::model::ProjectModel& model,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::rules::ProjectFindingsModel& findings);
```

to:

```cpp
void renderText(
    std::ostream& output,
    const codeqa::model::ProjectModel& model,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::rules::ProjectFindingsModel& findings,
    const codeqa::duplication::ProjectDuplicationModel& duplicates);
```

In `src/report/TextReport.cpp`, add a new helper right after `writeFindingsByFile` (before `writeDiagnostics`):

```cpp
void writeDuplicates(
    std::ostream& output, const codeqa::duplication::ProjectDuplicationModel& duplicates,
    const std::filesystem::path& rootPath)
{
    if (duplicates.groups.empty())
    {
        return;
    }

    output << "\nDuplicated code\n----------------\n";

    std::size_t groupNumber = 1;
    for (const codeqa::duplication::DuplicateGroup& group : duplicates.groups)
    {
        output << "Group " << groupNumber << " (" << group.members.size() << " copies)\n";

        for (const codeqa::duplication::DuplicateMember& member : group.members)
        {
            output << "  " << relativeDisplayPath(member.range.begin.filePath, rootPath) << ':'
                   << member.range.begin.line << "  " << member.qualifiedName << '\n';
        }

        ++groupNumber;
    }
}
```

Change `renderText`'s signature and body from:

```cpp
void renderText(
    std::ostream& output,
    const codeqa::model::ProjectModel& model,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::rules::ProjectFindingsModel& findings)
{
    output << "Code Quality Analysis Report\n=============================\n\n";
    writeSummary(output, model, metrics, findings);
    writeFindingsBySeverity(output, findings, model.rootPath);
    writeFindingsByFile(output, findings, model.rootPath);
    writeDiagnostics(output, model, model.rootPath);
    writeResult(output, findings);
}
```

to:

```cpp
void renderText(
    std::ostream& output,
    const codeqa::model::ProjectModel& model,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::rules::ProjectFindingsModel& findings,
    const codeqa::duplication::ProjectDuplicationModel& duplicates)
{
    output << "Code Quality Analysis Report\n=============================\n\n";
    writeSummary(output, model, metrics, findings);
    writeFindingsBySeverity(output, findings, model.rootPath);
    writeFindingsByFile(output, findings, model.rootPath);
    writeDuplicates(output, duplicates, model.rootPath);
    writeDiagnostics(output, model, model.rootPath);
    writeResult(output, findings);
}
```

- [ ] **Step 4: Update the production call site**

In `src/cli/RunApplication.cpp`, change:

```cpp
        codeqa::report::renderText(
            *destination, textAnalysis.model, textAnalysis.metrics, textAnalysis.findings);
```

to:

```cpp
        codeqa::report::renderText(
            *destination, textAnalysis.model, textAnalysis.metrics, textAnalysis.findings,
            codeqa::duplication::ProjectDuplicationModel{});
```

(Task 9 replaces the empty placeholder with `textAnalysis.duplicates` once it exists.) Add `#include "codeqa/duplication/DuplicationModels.h"` to this file's includes.

- [ ] **Step 5: Run the tests to verify they pass**

Run: `cmake --build build --target codeqa_tests` then run the full suite: `ctest --test-dir build --output-on-failure`.
Expected: all tests PASS.

- [ ] **Step 6: Commit**

```bash
git add include/codeqa/report/TextReport.h src/report/TextReport.cpp src/cli/RunApplication.cpp tests/unit/TextReportTest.cpp
git commit -m "feat: add duplicated code section to the text report"
```

---

### Task 6: `RunAnalysis` computes duplicates

**Files:**
- Modify: `include/codeqa/application/RunAnalysis.h`
- Modify: `src/application/RunAnalysis.cpp`
- Test: `tests/unit/RunAnalysisTest.cpp`

**Interfaces:**
- Consumes: `codeqa::duplication::detectDuplicates` (Task 3), `codeqa::config::ProjectConfig::duplication` (Task 2).
- Produces: `codeqa::application::RunAnalysisResult::duplicates` (`codeqa::duplication::ProjectDuplicationModel`). Task 7 consumes this.

- [ ] **Step 1: Write the failing test**

Add at the end of `tests/unit/RunAnalysisTest.cpp`:

```cpp
TEST_CASE("RunAnalysis detects a duplicated function pair across two files")
{
    const TempProject fixture;
    fixture.addFile(
        "a.c",
        "int addFirst(int alpha, int beta)\n"
        "{\n"
        "    int total = alpha + beta;\n"
        "    return total;\n"
        "}\n");
    fixture.addFile(
        "b.c",
        "int addSecond(int first, int second)\n"
        "{\n"
        "    int sum = first + second;\n"
        "    return sum;\n"
        "}\n");

    codeqa::application::RunAnalysisRequest request;
    request.analysis.discovery.rootPath = fixture.root();
    request.config = codeqa::config::defaultConfig();
    request.config.duplication.minNormalizedTokens = 10;

    const codeqa::application::RunAnalysisResult result = codeqa::application::RunAnalysis{}.execute(request);

    REQUIRE(result.duplicates.groups.size() == 1);
    CHECK(result.duplicates.groups.front().members.size() == 2);
}
```

- [ ] **Step 2: Run the test to verify it fails to compile**

Run: `cmake --build build --target codeqa_tests`.
Expected: compile error — `RunAnalysisResult` has no member `duplicates`.

- [ ] **Step 3: Add `duplicates` to `RunAnalysisResult` and compute it**

In `include/codeqa/application/RunAnalysis.h`, add `#include "codeqa/duplication/DuplicationModels.h"`, and change:

```cpp
struct RunAnalysisResult
{
    codeqa::model::ProjectModel model;
    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::rules::ProjectFindingsModel findings;
};
```

to:

```cpp
struct RunAnalysisResult
{
    codeqa::model::ProjectModel model;
    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::rules::ProjectFindingsModel findings;
    codeqa::duplication::ProjectDuplicationModel duplicates;
};
```

In `src/application/RunAnalysis.cpp`, add `#include "codeqa/duplication/DetectDuplicates.h"`, and change:

```cpp
    result.findings = ApplyRules{}.execute(result.model, result.metrics, request.config);
    return result;
```

to:

```cpp
    result.findings = ApplyRules{}.execute(result.model, result.metrics, request.config);
    result.duplicates = codeqa::duplication::detectDuplicates(result.model, request.config.duplication);
    return result;
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake --build build --target codeqa_tests` then run the full suite: `ctest --test-dir build --output-on-failure`.
Expected: all tests PASS.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/application/RunAnalysis.h src/application/RunAnalysis.cpp tests/unit/RunAnalysisTest.cpp
git commit -m "feat: compute duplicates in RunAnalysis"
```

---

### Task 7: `ReportDocument` carries `duplicates` through

**Files:**
- Modify: `include/codeqa/application/ReportDocument.h`
- Modify: `src/application/ReportDocumentFactory.cpp`
- Modify: `src/application/ReportDocumentAnalysis.cpp`
- Test: `tests/unit/ReportDocumentTest.cpp`
- Test: `tests/unit/ReportDocumentAnalysisTest.cpp`

**Interfaces:**
- Consumes: `codeqa::application::RunAnalysisResult::duplicates` (Task 6).
- Produces: `codeqa::application::ReportDocument::duplicates` (`codeqa::duplication::ProjectDuplicationModel`), populated by `makeReportDocument` and by `parseReportDocument` (when the source JSON has a `duplicates` key — optional, for backward compatibility with reports/snapshots saved before this feature existed), and read back out by `toAnalysisResult`. Task 8 and Task 9 consume `ReportDocument::duplicates` (via `analysis.duplicates` and `textAnalysis.duplicates` respectively).

- [ ] **Step 1: Write the failing tests**

Add at the end of `tests/unit/ReportDocumentTest.cpp`:

```cpp
TEST_CASE("parseReportDocument round-trips a duplicate group with its members")
{
    nlohmann::json report = makeCompleteReportJson();

    nlohmann::json member1;
    member1["qualifiedName"] = "sample::run";
    member1["path"] = "src/sample.cpp";
    member1["range"]["begin"] = {{"line", 2}, {"column", 1}};
    member1["range"]["end"] = {{"line", 8}, {"column", 1}};

    nlohmann::json member2;
    member2["qualifiedName"] = "sample::runAgain";
    member2["path"] = "src/other.cpp";
    member2["range"]["begin"] = {{"line", 4}, {"column", 1}};
    member2["range"]["end"] = {{"line", 10}, {"column", 1}};

    nlohmann::json group;
    group["members"] = nlohmann::json::array({member1, member2});

    report["duplicates"] = nlohmann::json::array({group});

    const auto result = codeqa::application::parseReportDocument(report);

    REQUIRE(result.document.has_value());
    REQUIRE(result.document->duplicates.groups.size() == 1);
    REQUIRE(result.document->duplicates.groups.front().members.size() == 2);
    CHECK(result.document->duplicates.groups.front().members[0].qualifiedName == "sample::run");
    CHECK(result.document->duplicates.groups.front().members[1].qualifiedName == "sample::runAgain");
}

TEST_CASE("parseReportDocument defaults to no duplicate groups when the field is absent (pre-existing reports)")
{
    nlohmann::json report = makeCompleteReportJson();
    report.erase("duplicates");

    const auto result = codeqa::application::parseReportDocument(report);

    REQUIRE(result.document.has_value());
    CHECK(result.document->duplicates.groups.empty());
}
```

Add at the end of `tests/unit/ReportDocumentAnalysisTest.cpp`:

```cpp
TEST_CASE("toAnalysisResult carries the duplicates model through unchanged")
{
    codeqa::application::ReportDocument document;
    document.metadata.rootPath = "project";

    codeqa::duplication::DuplicateGroup group;
    group.members.push_back(codeqa::duplication::DuplicateMember{"a::run", codeqa::core::SourceRange{}});
    group.members.push_back(codeqa::duplication::DuplicateMember{"b::run", codeqa::core::SourceRange{}});
    document.duplicates.groups.push_back(group);

    const codeqa::application::RunAnalysisResult result = codeqa::application::toAnalysisResult(document);

    REQUIRE(result.duplicates.groups.size() == 1);
    CHECK(result.duplicates.groups.front().members.size() == 2);
}
```

- [ ] **Step 2: Run the tests to verify they fail to compile**

Run: `cmake --build build --target codeqa_tests`.
Expected: compile error — `ReportDocument`/`RunAnalysisResult` have no member `duplicates` reachable the way these tests use it (or `document.duplicates` is silently always empty and the round-trip test's `REQUIRE` fails at runtime instead — either way, this step must show red before Step 3).

- [ ] **Step 3: Add `duplicates` to `ReportDocument` and wire `makeReportDocument`**

In `include/codeqa/application/ReportDocument.h`, add `#include "codeqa/duplication/DuplicationModels.h"`, and change:

```cpp
struct ReportDocument
{
    ReportMetadata metadata;
    codeqa::config::ProjectConfig config;
    std::vector<ReportFile> files;
    std::optional<ReportComparison> comparison;
    QualityGateOutcome qualityGateOutcome{QualityGateOutcome::NotEvaluated};
};
```

to:

```cpp
struct ReportDocument
{
    ReportMetadata metadata;
    codeqa::config::ProjectConfig config;
    std::vector<ReportFile> files;
    std::optional<ReportComparison> comparison;
    QualityGateOutcome qualityGateOutcome{QualityGateOutcome::NotEvaluated};
    codeqa::duplication::ProjectDuplicationModel duplicates;
};
```

In `src/application/ReportDocumentFactory.cpp`, in `makeReportDocument`, change:

```cpp
    document.config = config;
    document.comparison = comparison;
    document.qualityGateOutcome = qualityGateOutcome;
```

to:

```cpp
    document.config = config;
    document.comparison = comparison;
    document.qualityGateOutcome = qualityGateOutcome;
    document.duplicates = analysis.duplicates;
```

- [ ] **Step 4: Add optional parsing of `duplicates` in `parseReportDocument`**

Still in `src/application/ReportDocumentFactory.cpp`, add two new helpers in the anonymous namespace, right after `parseFile` (before the closing `} // namespace`):

```cpp
codeqa::duplication::DuplicateMember parseDuplicateMember(const Json& document, const std::string& path)
{
    if (!document.is_object())
    {
        failRequired("'" + path + "' must be an object.");
    }

    codeqa::duplication::DuplicateMember member;
    member.qualifiedName = requireString(document, "qualifiedName", path);
    const std::string filePath = requireString(document, "path", path);
    member.range = parseRange(requireObject(document, "range", path), path + ".range");
    member.range.begin.filePath = filePath;
    member.range.end.filePath = filePath;
    return member;
}

codeqa::duplication::DuplicateGroup parseDuplicateGroup(const Json& document, const std::string& path)
{
    if (!document.is_object())
    {
        failRequired("'" + path + "' must be an object.");
    }

    codeqa::duplication::DuplicateGroup group;
    const Json& members = requireArray(document, "members", path);
    for (std::size_t index = 0; index < members.size(); ++index)
    {
        group.members.push_back(
            parseDuplicateMember(members.at(index), path + ".members[" + std::to_string(index) + "]"));
    }
    return group;
}
```

In `parseReportDocument`, add after the `files` loop, before `return ReportDocumentResult{std::move(result), std::nullopt};`:

```cpp
        if (document.contains("duplicates"))
        {
            const Json& duplicates = requireArray(document, "duplicates", "report");
            for (std::size_t index = 0; index < duplicates.size(); ++index)
            {
                result.duplicates.groups.push_back(parseDuplicateGroup(
                    duplicates.at(index), "report.duplicates[" + std::to_string(index) + "]"));
            }
        }
```

This is deliberately optional (`if (document.contains(...))`, not `requireArray` unconditionally): a report or snapshot saved before this feature existed has no `duplicates` key at all, and must still load successfully with an empty `duplicates.groups` rather than failing to parse.

- [ ] **Step 5: Wire `toAnalysisResult`**

In `src/application/ReportDocumentAnalysis.cpp`, change:

```cpp
    RunAnalysisResult analysis;
    analysis.model.rootPath = document.metadata.rootPath;
    analysis.metrics.rootPath = analysis.model.rootPath;
    analysis.findings.rootPath = analysis.model.rootPath;
```

to:

```cpp
    RunAnalysisResult analysis;
    analysis.model.rootPath = document.metadata.rootPath;
    analysis.metrics.rootPath = analysis.model.rootPath;
    analysis.findings.rootPath = analysis.model.rootPath;
    analysis.duplicates = document.duplicates;
```

- [ ] **Step 6: Run the tests to verify they pass**

Run: `cmake --build build --target codeqa_tests` then run the full suite: `ctest --test-dir build --output-on-failure`.
Expected: all tests PASS.

- [ ] **Step 7: Commit**

```bash
git add include/codeqa/application/ReportDocument.h src/application/ReportDocumentFactory.cpp src/application/ReportDocumentAnalysis.cpp tests/unit/ReportDocumentTest.cpp tests/unit/ReportDocumentAnalysisTest.cpp
git commit -m "feat: carry duplicates through ReportDocument"
```

---

### Task 8: `ApplicationFacade` wires the real duplicates into the JSON report

**Files:**
- Modify: `src/application/ApplicationFacade.cpp` (swap Task 4's placeholder)
- Test: `tests/unit/ApplicationFacadeTest.cpp`

**Interfaces:**
- Consumes: `analysis.duplicates` (Task 6, already flowing since `analysis` is a `RunAnalysisResult`).
- Produces: nothing new — closes the loop so `AnalysisFacadeResult::reportJson`/`::document` both carry real duplicate data end-to-end.

- [ ] **Step 1: Write the failing test**

Add at the end of `tests/unit/ApplicationFacadeTest.cpp`:

```cpp
TEST_CASE("ApplicationFacade reports a duplicated function pair in the JSON report and the ReportDocument")
{
    const TempProject project;
    std::ofstream(project.root() / "a.c")
        << "int addFirst(int alpha, int beta)\n{\n    int total = alpha + beta;\n    return total;\n}\n";
    std::ofstream(project.root() / "b.c")
        << "int addSecond(int first, int second)\n{\n    int sum = first + second;\n    return sum;\n}\n";

    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.duplication.minNormalizedTokens = 10;
    config.qualityGate.minSeverity = codeqa::core::DiagnosticSeverity::Error;

    const codeqa::application::AnalysisFacadeResult result =
        codeqa::application::ApplicationFacade{}.analyzeProject({project.root(), config, {}, {}});

    REQUIRE_FALSE(result.error.hasError());
    REQUIRE(result.reportJson.has_value());
    CHECK(result.reportJson->at("duplicates").size() == 1);

    REQUIRE(result.document.has_value());
    REQUIRE(result.document->duplicates.groups.size() == 1);
    CHECK(result.document->duplicates.groups.front().members.size() == 2);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build build --target codeqa_tests` then run the new case.
Expected: FAIL — `result.reportJson->at("duplicates")` is an empty array (Task 4's placeholder), size 0 not 1.

- [ ] **Step 3: Swap the placeholder for the real value**

In `src/application/ApplicationFacade.cpp`, change:

```cpp
        result.reportJson = codeqa::report::toReportJson(
            analysis.model, analysis.metrics, analysis.findings, codeqa::duplication::ProjectDuplicationModel{},
            request.config, toolVersion);
```

to:

```cpp
        result.reportJson = codeqa::report::toReportJson(
            analysis.model, analysis.metrics, analysis.findings, analysis.duplicates, request.config, toolVersion);
```

(`result.document = makeReportDocument(analysis, ...)` on the next line already reads `analysis.duplicates` internally since Task 7 — no change needed there.)

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake --build build --target codeqa_tests` then run the full suite: `ctest --test-dir build --output-on-failure`.
Expected: all tests PASS.

- [ ] **Step 5: Commit**

```bash
git add src/application/ApplicationFacade.cpp tests/unit/ApplicationFacadeTest.cpp
git commit -m "feat: wire real duplicates into ApplicationFacade's JSON report"
```

---

### Task 9: CLI text output shows the real duplicates

**Files:**
- Modify: `src/cli/RunApplication.cpp` (swap Task 5's placeholder)
- Test: `tests/unit/RunApplicationTest.cpp`

**Interfaces:**
- Consumes: `textAnalysis.duplicates` (Task 7, via `toAnalysisResult`).
- Produces: nothing new — closes the loop so `codeqa analyze` (default text format) prints real duplicate data.

- [ ] **Step 1: Write the failing test**

Add at the end of `tests/unit/RunApplicationTest.cpp`:

```cpp
TEST_CASE("analyze reports a duplicated function pair in the default text report")
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "codeqa_run_application_duplication_test";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);
    std::ofstream(root / "a.c") << "int addFirst(int alpha, int beta)\n"
                                   "{\n"
                                   "    int total = alpha + beta;\n"
                                   "    return total;\n"
                                   "}\n";
    std::ofstream(root / "b.c") << "int addSecond(int first, int second)\n"
                                   "{\n"
                                   "    int sum = first + second;\n"
                                   "    return sum;\n"
                                   "}\n";
    const std::filesystem::path configPath = root / "codeqa.json";
    std::ofstream(configPath) << R"({ "duplication": { "minNormalizedTokens": 10 }, )"
                                  R"("qualityGate": { "minSeverity": "error" } })";

    const RunResult result = run({"analyze", root.string(), "--config", configPath.string()});

    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("Duplicated code") != std::string::npos);
    CHECK(result.output.find("addFirst") != std::string::npos);
    CHECK(result.output.find("addSecond") != std::string::npos);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build build --target codeqa_tests` then run the new case.
Expected: FAIL — `result.output` never contains "Duplicated code" (Task 5's placeholder always renders an empty duplicates model).

- [ ] **Step 3: Swap the placeholder for the real value**

In `src/cli/RunApplication.cpp`, change:

```cpp
        codeqa::report::renderText(
            *destination, textAnalysis.model, textAnalysis.metrics, textAnalysis.findings,
            codeqa::duplication::ProjectDuplicationModel{});
```

to:

```cpp
        codeqa::report::renderText(
            *destination, textAnalysis.model, textAnalysis.metrics, textAnalysis.findings, textAnalysis.duplicates);
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake --build build --target codeqa_tests` then run the full suite: `ctest --test-dir build --output-on-failure`.
Expected: all tests PASS.

- [ ] **Step 5: Commit**

```bash
git add src/cli/RunApplication.cpp tests/unit/RunApplicationTest.cpp
git commit -m "feat: show real duplicates in the CLI text report"
```

---

### Task 10: Corpus fixtures and end-to-end verification

**Files:**
- Create: `tests/corpus/duplication/exact-clone.cpp`
- Create: `tests/corpus/duplication/renamed-clone.cpp`
- Create: `tests/corpus/duplication/below-threshold.cpp`
- Create: `tests/corpus/duplication/trivial-getters.cpp`
- Create: `tests/unit/CorpusDuplicationTest.cpp`
- Modify: `tests/CMakeLists.txt` (register the new test `.cpp`)

**Interfaces:**
- Consumes: `codeqa::application::AnalyzeProject`, `codeqa::duplication::detectDuplicates`, `codeqa::config::defaultConfig` (all from prior tasks).
- Produces: nothing new — this task is pure verification against the real corpus tree, matching the spec's §9 testing strategy.

- [ ] **Step 1: Create the fixture files**

Create `tests/corpus/duplication/exact-clone.cpp`:

```cpp
int computeChecksumA(unsigned char value, unsigned char seed)
{
    unsigned char result = value ^ seed;
    result = static_cast<unsigned char>(result + 1);
    return result;
}

int computeChecksumB(unsigned char value, unsigned char seed)
{
    unsigned char result = value ^ seed;
    result = static_cast<unsigned char>(result + 1);
    return result;
}
```

Create `tests/corpus/duplication/renamed-clone.cpp`:

```cpp
int mergeBuffersLeft(int firstLength, int secondLength)
{
    int combinedLength = firstLength + secondLength;
    if (combinedLength > 256)
    {
        combinedLength = 256;
    }
    return combinedLength;
}

int mergeBuffersRight(int leftSize, int rightSize)
{
    int totalSize = leftSize + rightSize;
    if (totalSize > 512)
    {
        totalSize = 512;
    }
    return totalSize;
}
```

Create `tests/corpus/duplication/below-threshold.cpp`:

```cpp
int identityValue(int value)
{
    return value;
}

int passThroughValue(int value)
{
    return value;
}
```

Create `tests/corpus/duplication/trivial-getters.cpp`:

```cpp
// codeqa: documented "borderline" fixture -- these two validators are
// structurally identical because there is essentially one reasonable way to
// write a range check this small, not because of copy-paste. Included to make
// the minNormalizedTokens threshold's false-positive trade-off visible in the
// test suite instead of only in prose (see the duplication design spec, S9).
bool isValidPercentage(int value)
{
    if (value < 0)
    {
        return false;
    }
    if (value > 100)
    {
        return false;
    }
    return true;
}

bool isValidByteValue(int value)
{
    if (value < 0)
    {
        return false;
    }
    if (value > 100)
    {
        return false;
    }
    return true;
}
```

- [ ] **Step 2: Register the new test file in CMake**

In `tests/CMakeLists.txt`, add `unit/CorpusDuplicationTest.cpp` to `add_executable(codeqa_tests ...)`, near the other `unit/Corpus*Test.cpp` entries.

- [ ] **Step 3: Write the failing test**

Create `tests/unit/CorpusDuplicationTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/application/AnalyzeProject.h"
#include "codeqa/config/DefaultConfig.h"
#include "codeqa/duplication/DetectDuplicates.h"

#include <algorithm>
#include <filesystem>
#include <string>

namespace
{

bool groupContains(
    const codeqa::duplication::DuplicateGroup& group, const std::string& qualifiedNameA,
    const std::string& qualifiedNameB)
{
    const bool hasA = std::any_of(
        group.members.begin(), group.members.end(),
        [&](const codeqa::duplication::DuplicateMember& member) { return member.qualifiedName == qualifiedNameA; });
    const bool hasB = std::any_of(
        group.members.begin(), group.members.end(),
        [&](const codeqa::duplication::DuplicateMember& member) { return member.qualifiedName == qualifiedNameB; });
    return hasA && hasB;
}

bool anyGroupContains(
    const codeqa::duplication::ProjectDuplicationModel& duplicates, const std::string& qualifiedNameA,
    const std::string& qualifiedNameB)
{
    return std::any_of(
        duplicates.groups.begin(), duplicates.groups.end(), [&](const codeqa::duplication::DuplicateGroup& group) {
            return groupContains(group, qualifiedNameA, qualifiedNameB);
        });
}

} // namespace

TEST_CASE("corpus duplication fixtures are detected against the whole real corpus")
{
    const std::filesystem::path corpusRoot{CODEQA_CORPUS_DIR};
    codeqa::application::AnalyzeProjectRequest request;
    request.discovery.rootPath = corpusRoot;
    const codeqa::model::ProjectModel project = codeqa::application::AnalyzeProject{}.execute(request);

    const codeqa::duplication::ProjectDuplicationModel duplicates =
        codeqa::duplication::detectDuplicates(project, codeqa::config::defaultConfig().duplication);

    CHECK(anyGroupContains(duplicates, "computeChecksumA", "computeChecksumB"));
    CHECK(anyGroupContains(duplicates, "mergeBuffersLeft", "mergeBuffersRight"));
    CHECK(anyGroupContains(duplicates, "isValidPercentage", "isValidByteValue"));
    CHECK_FALSE(anyGroupContains(duplicates, "identityValue", "passThroughValue"));
}
```

- [ ] **Step 4: Run the test to verify it fails to link**

Run: `cmake --build build --target codeqa_tests`.
Expected: link error (new `.cpp` not yet registered) or, once registered, a runtime FAIL if any fixture file is missing or a token-count assumption was wrong (double check by hand: `renamed-clone.cpp`'s `mergeBuffersLeft` normalizes to 33 tokens, `trivial-getters.cpp`'s `isValidPercentage` to 33 tokens — both clear the default `minNormalizedTokens: 30`; `below-threshold.cpp`'s `identityValue` normalizes to 11 tokens, well under it).

- [ ] **Step 5: Run the test to verify it passes**

Run: `cmake --build build --target codeqa_tests` then run the full suite: `ctest --test-dir build --output-on-failure`.
Expected: all tests PASS, including the new corpus case.

- [ ] **Step 6: Manual sanity check against a real external project**

This step has no automated assertion — it mirrors how Fase 4b/6b validated new capability against real code before closing the phase (`tests/corpus/REAL-PROJECTS.md` documents the pinned SQLite 3.46.0 amalgamation already used by other corpus tests). Build the CLI (`cmake --build build --target codeqa`) and run:

```bash
./build/Debug/codeqa.exe analyze <path-to-the-pinned-sqlite-amalgamation-directory> --config <(echo '{"duplication":{"minNormalizedTokens":40}}')
```

(On Windows/PowerShell, write the JSON to a temp file first and pass its path via `--config` instead of process substitution.) Inspect the `Duplicated code` section of the output. Record in the task's completion notes: how many groups were found, whether any look like true positives (genuine copy-paste), and whether any look like false positives worth a future threshold adjustment — do not change `DuplicationConfig`'s default based on this single run (the spec explicitly defers formal precision validation), just record the observation.

- [ ] **Step 7: Commit**

```bash
git add tests/corpus/duplication tests/unit/CorpusDuplicationTest.cpp tests/CMakeLists.txt
git commit -m "test: add duplication corpus fixtures and end-to-end coverage"
```
