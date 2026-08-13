# Fase 12 — Primer MVP completo Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship the quality gate (differential/absolute, exit code 1), the full exit-code taxonomy (0-4), and fixes for the two real `codeqa_core` bugs Fase 11 validation found and left unfixed — completing the roadmap's Fase 12 acceptance criteria.

**Architecture:** Additive changes across four already-established layers (`codeqa::core`, `codeqa::config`, `codeqa::history`, `codeqa::model`) plus the CLI (`codeqa::cli::runAnalyze`), with two independent, narrowly-scoped bug fixes in `codeqa::model::ProjectModelBuilder`. No new layer, no new binary, no new dependency.

**Tech Stack:** C++17, CMake 3.16+ (Visual Studio 17 2022 generator in this environment), doctest, nlohmann::json, tree-sitter (C/C++ grammars), ctest.

**Source of truth:** `docs/superpowers/specs/2026-08-13-fase-12-primer-mvp-completo-design.md` (approved spec). This plan refines two spec-level details based on investigation done while planning — both noted inline where they occur (§Task 9's detection logic moved from `StructuralExtractor` to `ProjectModelBuilder` for testability; the exact reproduction fixture for the corrupted-range bug, resolved empirically against real SQLite 3.46.0 rather than guessed).

## Global Constraints

- C++17, four-space indentation, Allman braces (matches all existing files — see any file under `src/`).
- `PascalCase` for types and files (`QualityGateConfig`, `ConfigModels.h`); `camelCase` for functions/variables (`hasNewFindingAtOrAboveSeverity`); trailing underscore for private data members (not applicable to this plan's free-function-heavy changes).
- Namespace per layer: `codeqa::core`, `codeqa::config`, `codeqa::history`, `codeqa::model`, `codeqa::rules`, `codeqa::application`, `codeqa::cli`. Declarations live in the matching public header (`include/codeqa/<layer>/...`) with the implementation in `src/<layer>/...`.
- No new third-party dependency. No new CMake target. Every new/changed source file already exists in `tests/CMakeLists.txt` or `CMakeLists.txt` — no registration changes needed anywhere in this plan.
- doctest `TEST_CASE`s, behavior-focused titles, matching the existing style in each touched test file exactly (see each task's "Files" section for the file to match).
- Diagnostic codes already taken: `CONFIG001`-`CONFIG008`, `PARSE001`-`PARSE003`, `CLI001`-`CLI016`, `METRICS001`. This plan adds `CONFIG009` and `PARSE004` — do not reuse or skip these numbers.
- `DiagnosticSeverity` JSON strings are exactly `"error"`, `"warning"`, `"information"` (`src/core/EnumJson.cpp`) — case-sensitive, no others.
- Build: `cmake --build build --config Debug` (Visual Studio 17 2022 generator already configured in `build/`). Test: `ctest --test-dir build -C Debug --output-on-failure`. Both must pass before any task is considered done.
- Never commit a real third-party source file (SQLite/fmt) into the repo — matches `tests/corpus/REAL-PROJECTS.md`'s existing convention. Task 10's real-SQLite verification is a manual, undocumented-in-ctest step, exactly like Fase 11's own validation flow.

---

## Task 1: `diagnosticSeverityFromJsonString`

**Files:**
- Modify: `include/codeqa/core/EnumJson.h`
- Modify: `src/core/EnumJson.cpp`
- Test: `tests/unit/EnumJsonTest.cpp`

**Interfaces:**
- Produces: `std::optional<codeqa::core::DiagnosticSeverity> codeqa::core::diagnosticSeverityFromJsonString(const std::string& text)` — accepts exactly `"error"`, `"warning"`, `"information"`; anything else returns `std::nullopt`. Consumed by Task 3.

- [ ] **Step 1: Write the failing test**

Add to the end of `tests/unit/EnumJsonTest.cpp`:

```cpp
TEST_CASE("diagnosticSeverityFromJsonString accepts information, warning, and error")
{
    const std::optional<codeqa::core::DiagnosticSeverity> information =
        codeqa::core::diagnosticSeverityFromJsonString("information");
    REQUIRE(information.has_value());
    CHECK(*information == codeqa::core::DiagnosticSeverity::Information);

    const std::optional<codeqa::core::DiagnosticSeverity> warning =
        codeqa::core::diagnosticSeverityFromJsonString("warning");
    REQUIRE(warning.has_value());
    CHECK(*warning == codeqa::core::DiagnosticSeverity::Warning);

    const std::optional<codeqa::core::DiagnosticSeverity> error =
        codeqa::core::diagnosticSeverityFromJsonString("error");
    REQUIRE(error.has_value());
    CHECK(*error == codeqa::core::DiagnosticSeverity::Error);
}

TEST_CASE("diagnosticSeverityFromJsonString rejects an unknown severity string")
{
    CHECK_FALSE(codeqa::core::diagnosticSeverityFromJsonString("critical").has_value());
    CHECK_FALSE(codeqa::core::diagnosticSeverityFromJsonString("").has_value());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --config Debug --target codeqa_tests`
Expected: FAIL to compile — `diagnosticSeverityFromJsonString` is not a member of `codeqa::core`.

- [ ] **Step 3: Implement**

In `include/codeqa/core/EnumJson.h`, add after the existing `diagnosticSeverityToJsonString` declaration:

```cpp
[[nodiscard]] std::optional<DiagnosticSeverity> diagnosticSeverityFromJsonString(const std::string& text);
```

In `src/core/EnumJson.cpp`, add after `diagnosticSeverityToJsonString`'s definition, inside `namespace codeqa::core { ... }`:

```cpp
std::optional<DiagnosticSeverity> diagnosticSeverityFromJsonString(const std::string& text)
{
    if (text == "error")
    {
        return DiagnosticSeverity::Error;
    }

    if (text == "warning")
    {
        return DiagnosticSeverity::Warning;
    }

    if (text == "information")
    {
        return DiagnosticSeverity::Information;
    }

    return std::nullopt;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --config Debug --target codeqa_tests && build\Debug\codeqa_tests.exe -tc="diagnosticSeverityFromJsonString*"`
Expected: PASS, both new `TEST_CASE`s green.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/core/EnumJson.h src/core/EnumJson.cpp tests/unit/EnumJsonTest.cpp
git commit -m "feat: add diagnosticSeverityFromJsonString"
```

---

## Task 2: `QualityGateConfig`

**Files:**
- Modify: `include/codeqa/config/ConfigModels.h`
- Test: `tests/unit/ConfigModelsTest.cpp`

**Interfaces:**
- Consumes: `codeqa::core::DiagnosticSeverity` (existing).
- Produces: `struct codeqa::config::QualityGateConfig { codeqa::core::DiagnosticSeverity minSeverity{codeqa::core::DiagnosticSeverity::Warning}; };` and a `QualityGateConfig qualityGate;` member on `ProjectConfig`. Consumed by Tasks 3, 4, 7.

- [ ] **Step 1: Write the failing test**

Add to the end of `tests/unit/ConfigModelsTest.cpp`:

```cpp
TEST_CASE("QualityGateConfig defaults minSeverity to Warning")
{
    codeqa::config::QualityGateConfig config;
    CHECK(config.minSeverity == codeqa::core::DiagnosticSeverity::Warning);
}

TEST_CASE("ProjectConfig defaults qualityGate.minSeverity to Warning")
{
    codeqa::config::ProjectConfig config;
    CHECK(config.qualityGate.minSeverity == codeqa::core::DiagnosticSeverity::Warning);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --config Debug --target codeqa_tests`
Expected: FAIL to compile — `codeqa::config::QualityGateConfig` does not exist, and `ProjectConfig` has no member `qualityGate`.

- [ ] **Step 3: Implement**

In `include/codeqa/config/ConfigModels.h`, add the include and the new struct/member:

```cpp
#pragma once

#include "codeqa/core/DiagnosticSeverity.h"
#include "codeqa/core/Language.h"

#include <map>
#include <string>
#include <vector>

namespace codeqa::config
{

struct RuleConfig
{
    bool enabled{true};
    long long warningLimit{0};
    long long highLimit{0};
};

struct HeaderOverrideConfig
{
    std::string pattern;
    codeqa::core::Language language{codeqa::core::Language::Unknown};
};

struct QualityGateConfig
{
    codeqa::core::DiagnosticSeverity minSeverity{codeqa::core::DiagnosticSeverity::Warning};
};

struct ProjectConfig
{
    std::vector<HeaderOverrideConfig> headerOverrides;
    std::map<std::string, RuleConfig> rules;
    QualityGateConfig qualityGate;
};

} // namespace codeqa::config
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --config Debug --target codeqa_tests && build\Debug\codeqa_tests.exe -tc="*QualityGate*,*qualityGate*"`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/config/ConfigModels.h tests/unit/ConfigModelsTest.cpp
git commit -m "feat: add QualityGateConfig to ProjectConfig"
```

---

## Task 3: Parse and serialize `qualityGate` in `codeqa.json`

**Files:**
- Modify: `src/config/ConfigJson.cpp`
- Test: `tests/unit/ConfigJsonTest.cpp`

**Interfaces:**
- Consumes: `codeqa::core::diagnosticSeverityFromJsonString`/`diagnosticSeverityToJsonString` (Task 1), `codeqa::config::QualityGateConfig` (Task 2).
- Produces: `parseConfig` accepts an optional top-level `"qualityGate": {"minSeverity": "warning"|"error"|"information"}` object; `toJson` emits it. `CONFIG009` for an invalid `minSeverity` string.

- [ ] **Step 1: Write the failing tests**

Add to the end of `tests/unit/ConfigJsonTest.cpp`:

```cpp
TEST_CASE("parseConfig accepts a document with qualityGate")
{
    const std::string text = R"({ "qualityGate": { "minSeverity": "error" } })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.config.has_value());
    CHECK(result.config->qualityGate.minSeverity == codeqa::core::DiagnosticSeverity::Error);
}

TEST_CASE("parseConfig defaults qualityGate.minSeverity to Warning when the object is present but empty")
{
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(R"({ "qualityGate": {} })");

    REQUIRE(result.config.has_value());
    CHECK(result.config->qualityGate.minSeverity == codeqa::core::DiagnosticSeverity::Warning);
}

TEST_CASE("parseConfig defaults qualityGate.minSeverity to Warning when qualityGate is absent")
{
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig("{}");

    REQUIRE(result.config.has_value());
    CHECK(result.config->qualityGate.minSeverity == codeqa::core::DiagnosticSeverity::Warning);
}

TEST_CASE("parseConfig reports CONFIG009 for an invalid qualityGate.minSeverity value")
{
    const std::string text = R"({ "qualityGate": { "minSeverity": "critical" } })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG009") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG002 for an unknown key inside qualityGate")
{
    const std::string text = R"({ "qualityGate": { "minSeverity": "error", "note": "x" } })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG002") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG002 for an unknown top-level key next to qualityGate")
{
    const std::string text = R"({ "qualityGate": { "minSeverity": "error" }, "outputFormat": "json" })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG002") != std::string::npos);
}

TEST_CASE("toJson round-trips qualityGate through parseConfig")
{
    codeqa::config::ProjectConfig original;
    original.qualityGate.minSeverity = codeqa::core::DiagnosticSeverity::Error;

    const std::string serialized = codeqa::config::toJson(original).dump();
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(serialized);

    REQUIRE(result.config.has_value());
    CHECK(result.config->qualityGate.minSeverity == codeqa::core::DiagnosticSeverity::Error);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --config Debug --target codeqa_tests && build\Debug\codeqa_tests.exe -tc="*qualityGate*,*QualityGate*"`
Expected: FAIL — the first test fails because `"qualityGate"` is currently an unknown top-level key (`CONFIG002`, not the expected parse), `toJson` doesn't emit `qualityGate` at all yet.

- [ ] **Step 3: Implement**

In `src/config/ConfigJson.cpp`, add `#include "codeqa/config/ConfigModels.h"` is already implicitly available via `ConfigJson.h`; no new include needed beyond what's already there (`EnumJson.h` is already included at line 3).

Change the top-level allowed-keys call (currently `requireKeysSubset(document, {"languages", "rules"}, "config");`) to:

```cpp
requireKeysSubset(document, {"languages", "rules", "qualityGate"}, "config");
```

Add a new parsing helper, next to `parseRules` in the anonymous namespace:

```cpp
QualityGateConfig parseQualityGate(const Json& qualityGate)
{
    requireKeysSubset(qualityGate, {"minSeverity"}, "qualityGate");

    QualityGateConfig config;

    if (qualityGate.contains("minSeverity"))
    {
        const std::string text = requireString(qualityGate, "minSeverity", "qualityGate");
        const std::optional<codeqa::core::DiagnosticSeverity> severity =
            codeqa::core::diagnosticSeverityFromJsonString(text);

        if (!severity.has_value())
        {
            throw ParseFailure(
                "Error [CONFIG009]: 'qualityGate.minSeverity' must be 'information', 'warning', or 'error', got '"
                + text + "'.");
        }

        config.minSeverity = *severity;
    }

    return config;
}
```

In `parseConfig`'s body, after the existing `if (document.contains("rules")) { ... }` block:

```cpp
        if (document.contains("qualityGate"))
        {
            const Json& qualityGate = document.at("qualityGate");
            requireObject(qualityGate, "qualityGate");
            config.qualityGate = parseQualityGate(qualityGate);
        }
```

In `toJson`, after the existing `document["rules"] = ...` loop, before `return document;`:

```cpp
    document["qualityGate"] = {
        {"minSeverity", codeqa::core::diagnosticSeverityToJsonString(config.qualityGate.minSeverity)}};
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build --config Debug --target codeqa_tests && build\Debug\codeqa_tests.exe -tc="*qualityGate*,*QualityGate*"`
Expected: PASS.

Run full config suite to check nothing else broke: `build\Debug\codeqa_tests.exe -tc="*Config*"`
Expected: PASS (the existing "toJson round-trips the default configuration" test doesn't check `qualityGate` but must still pass since it only asserts on `rules`).

- [ ] **Step 5: Commit**

```bash
git add src/config/ConfigJson.cpp tests/unit/ConfigJsonTest.cpp
git commit -m "feat: parse and serialize qualityGate in codeqa.json"
```

---

## Task 4: Merge `qualityGate`

**Files:**
- Modify: `src/config/MergeConfig.cpp`
- Test: `tests/unit/MergeConfigTest.cpp`

**Interfaces:**
- Consumes: `codeqa::config::QualityGateConfig` (Task 2).
- Produces: `mergeConfig` now also copies `user.qualityGate` onto the result, wholesale (same convention as `headerOverrides`).

- [ ] **Step 1: Write the failing tests**

Add to the end of `tests/unit/MergeConfigTest.cpp`:

```cpp
TEST_CASE("mergeConfig replaces qualityGate wholesale with the user's value")
{
    codeqa::config::ProjectConfig defaults;
    defaults.qualityGate.minSeverity = codeqa::core::DiagnosticSeverity::Warning;

    codeqa::config::ProjectConfig user;
    user.qualityGate.minSeverity = codeqa::core::DiagnosticSeverity::Error;

    const codeqa::config::ProjectConfig merged = codeqa::config::mergeConfig(defaults, user);

    CHECK(merged.qualityGate.minSeverity == codeqa::core::DiagnosticSeverity::Error);
}

TEST_CASE("mergeConfig with an empty user config keeps the default qualityGate")
{
    codeqa::config::ProjectConfig defaults;
    defaults.qualityGate.minSeverity = codeqa::core::DiagnosticSeverity::Error;

    const codeqa::config::ProjectConfig merged =
        codeqa::config::mergeConfig(defaults, codeqa::config::ProjectConfig{});

    CHECK(merged.qualityGate.minSeverity == codeqa::core::DiagnosticSeverity::Error);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --config Debug --target codeqa_tests && build\Debug\codeqa_tests.exe -tc="*qualityGate*"`
Expected: FAIL — the first test fails because `merged.qualityGate.minSeverity` is still `Warning` (the default), `mergeConfig` doesn't touch `qualityGate` yet.

- [ ] **Step 3: Implement**

In `src/config/MergeConfig.cpp`, add one line before `return merged;`:

```cpp
ProjectConfig mergeConfig(const ProjectConfig& defaults, const ProjectConfig& user)
{
    ProjectConfig merged = defaults;

    for (const auto& [id, ruleConfig] : user.rules)
    {
        merged.rules[id] = ruleConfig;
    }

    merged.headerOverrides = user.headerOverrides;
    merged.qualityGate = user.qualityGate;

    return merged;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build --config Debug --target codeqa_tests && build\Debug\codeqa_tests.exe -tc="*qualityGate*"`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/config/MergeConfig.cpp tests/unit/MergeConfigTest.cpp
git commit -m "feat: merge qualityGate from user config over defaults"
```

---

## Task 5: Exit-code taxonomy + `ConfigurationError`

**Files:**
- Modify: `include/codeqa/cli/ExitCode.h`
- Modify: `src/cli/RunApplication.cpp:598-602` (the `LoadProjectConfig` failure branch inside `runAnalyze`)
- Test: `tests/unit/RunApplicationTest.cpp`

**Interfaces:**
- Produces: `codeqa::cli::ExitCode::QualityGateFailed = 1`, `ConfigurationError = 2`, `InvalidArguments = 3` (renumbered from 2). Consumed by Task 7.

- [ ] **Step 1: Write the failing tests**

In `tests/unit/RunApplicationTest.cpp`, find this existing test:

```cpp
TEST_CASE("analyze with an invalid --config file forwards the CONFIG error and returns InvalidArguments")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const std::filesystem::path configPath = root / "codeqa.json";
    std::ofstream(configPath) << "{ not json";

    const RunResult result = run({"analyze", root.string(), "--config", configPath.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CONFIG001") != std::string::npos);
}
```

Replace it with:

```cpp
TEST_CASE("analyze with an invalid --config file forwards the CONFIG error and returns ConfigurationError")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const std::filesystem::path configPath = root / "codeqa.json";
    std::ofstream(configPath) << "{ not json";

    const RunResult result = run({"analyze", root.string(), "--config", configPath.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::ConfigurationError));
    CHECK(result.errorOutput.find("CONFIG001") != std::string::npos);
}

TEST_CASE("analyze with an invalid qualityGate.minSeverity value returns ConfigurationError")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const std::filesystem::path configPath = root / "codeqa.json";
    std::ofstream(configPath) << R"({ "qualityGate": { "minSeverity": "critical" } })";

    const RunResult result = run({"analyze", root.string(), "--config", configPath.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::ConfigurationError));
    CHECK(result.errorOutput.find("CONFIG009") != std::string::npos);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --config Debug --target codeqa_tests`
Expected: FAIL to compile — `codeqa::cli::ExitCode::ConfigurationError` does not exist yet.

- [ ] **Step 3: Implement**

Replace `include/codeqa/cli/ExitCode.h` in full:

```cpp
#pragma once

namespace codeqa::cli
{

enum class ExitCode : int
{
    /// Returned when the application ran to completion without a failed quality gate.
    Success = 0,
    /// Returned when analyze completed but the quality gate failed (new findings at/above the
    /// configured severity, or -- with no previous snapshot -- any qualifying finding at all).
    QualityGateFailed = 1,
    /// Returned when a --config file could not be read or failed to parse/validate.
    ConfigurationError = 2,
    /// Returned when the command-line arguments supplied by the user are invalid or unrecognized.
    InvalidArguments = 3,
    /// Returned when an unhandled exception (or other unexpected internal failure) is caught at the top level.
    InternalError = 4
};

} // namespace codeqa::cli
```

In `src/cli/RunApplication.cpp`, inside `runAnalyze`, change:

```cpp
        if (!configResult.config.has_value())
        {
            errorOutput << *configResult.error << '\n';
            return static_cast<int>(ExitCode::InvalidArguments);
        }
```

to:

```cpp
        if (!configResult.config.has_value())
        {
            errorOutput << *configResult.error << '\n';
            return static_cast<int>(ExitCode::ConfigurationError);
        }
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build --config Debug --target codeqa_tests && build\Debug\codeqa_tests.exe`
Expected: PASS — full suite, including every other `InvalidArguments`-asserting test (they now check against numeric value 3, automatically, with no source change needed since they use the symbolic enum).

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/cli/ExitCode.h src/cli/RunApplication.cpp tests/unit/RunApplicationTest.cpp
git commit -m "feat: add QualityGateFailed/ConfigurationError exit codes"
```

---

## Task 6: `hasNewFindingAtOrAboveSeverity`

**Files:**
- Modify: `include/codeqa/history/CompareSnapshots.h`
- Modify: `src/history/CompareSnapshots.cpp`
- Test: `tests/unit/HistoryTest.cpp`

**Interfaces:**
- Consumes: `codeqa::rules::ProjectFindingsModel`, `codeqa::history::FindingKeySet`, `codeqa::core::DiagnosticSeverity` (all existing).
- Produces: `bool codeqa::history::hasNewFindingAtOrAboveSeverity(const codeqa::rules::ProjectFindingsModel& current, const FindingKeySet& previousKeys, codeqa::core::DiagnosticSeverity minSeverity)`. Consumed by Task 7. Works for both differential mode (`previousKeys` populated) and absolute mode (`previousKeys` empty) — same function, no branching.

- [ ] **Step 1: Write the failing tests**

Add to the end of `tests/unit/HistoryTest.cpp`:

```cpp
codeqa::rules::ProjectFindingsModel makeSingleFinding(
    const std::string& ruleId,
    const std::string& symbolName,
    codeqa::core::DiagnosticSeverity severity)
{
    codeqa::rules::ProjectFindingsModel findings;
    findings.rootPath = std::filesystem::path{"project"};

    codeqa::rules::FileFindingsModel fileFindings;
    fileFindings.path = findings.rootPath / "src" / "foo.cpp";

    codeqa::rules::Finding finding;
    finding.ruleId = ruleId;
    finding.symbolName = symbolName;
    finding.severity = severity;
    fileFindings.findings.push_back(finding);

    findings.files.push_back(fileFindings);
    return findings;
}

TEST_CASE("hasNewFindingAtOrAboveSeverity in absolute mode: any qualifying finding counts as new")
{
    const codeqa::rules::ProjectFindingsModel current =
        makeSingleFinding("complexity.cyclomatic", "Foo::bar", codeqa::core::DiagnosticSeverity::Warning);

    CHECK(codeqa::history::hasNewFindingAtOrAboveSeverity(
        current, codeqa::history::FindingKeySet{}, codeqa::core::DiagnosticSeverity::Warning));
}

TEST_CASE("hasNewFindingAtOrAboveSeverity in absolute mode: a finding below minSeverity does not count")
{
    const codeqa::rules::ProjectFindingsModel current =
        makeSingleFinding("complexity.cyclomatic", "Foo::bar", codeqa::core::DiagnosticSeverity::Warning);

    CHECK_FALSE(codeqa::history::hasNewFindingAtOrAboveSeverity(
        current, codeqa::history::FindingKeySet{}, codeqa::core::DiagnosticSeverity::Error));
}

TEST_CASE("hasNewFindingAtOrAboveSeverity: a finding exactly at minSeverity counts")
{
    const codeqa::rules::ProjectFindingsModel current =
        makeSingleFinding("complexity.cyclomatic", "Foo::bar", codeqa::core::DiagnosticSeverity::Warning);

    CHECK(codeqa::history::hasNewFindingAtOrAboveSeverity(
        current, codeqa::history::FindingKeySet{}, codeqa::core::DiagnosticSeverity::Warning));
}

TEST_CASE("hasNewFindingAtOrAboveSeverity in differential mode: an existing finding is not new")
{
    const codeqa::rules::ProjectFindingsModel current =
        makeSingleFinding("complexity.cyclomatic", "Foo::bar", codeqa::core::DiagnosticSeverity::Error);
    const codeqa::history::FindingKey key{"complexity.cyclomatic", "Foo::bar", "src/foo.cpp"};
    const codeqa::history::FindingKeySet previousKeys{{key, 1}};

    CHECK_FALSE(codeqa::history::hasNewFindingAtOrAboveSeverity(
        current, previousKeys, codeqa::core::DiagnosticSeverity::Warning));
}

TEST_CASE("hasNewFindingAtOrAboveSeverity in differential mode: a genuinely new finding counts")
{
    const codeqa::rules::ProjectFindingsModel current =
        makeSingleFinding("complexity.returns", "Foo::baz", codeqa::core::DiagnosticSeverity::Warning);
    const codeqa::history::FindingKey unrelatedKey{"complexity.cyclomatic", "Foo::bar", "src/foo.cpp"};
    const codeqa::history::FindingKeySet previousKeys{{unrelatedKey, 1}};

    CHECK(codeqa::history::hasNewFindingAtOrAboveSeverity(
        current, previousKeys, codeqa::core::DiagnosticSeverity::Warning));
}

TEST_CASE("hasNewFindingAtOrAboveSeverity on no findings at all returns false in either mode")
{
    const codeqa::rules::ProjectFindingsModel empty;

    CHECK_FALSE(codeqa::history::hasNewFindingAtOrAboveSeverity(
        empty, codeqa::history::FindingKeySet{}, codeqa::core::DiagnosticSeverity::Warning));
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --config Debug --target codeqa_tests`
Expected: FAIL to compile — `hasNewFindingAtOrAboveSeverity` is not declared.

- [ ] **Step 3: Implement**

In `include/codeqa/history/CompareSnapshots.h`, add after the `compareSnapshots` declaration:

```cpp
/// True if any finding in `current` is "new" relative to `previousKeys` (the same multiset-diff
/// identity compareSnapshots uses: a finding is new once its FindingKey has no remaining
/// previousKeys occurrence left to consume) and its severity is >= minSeverity. An empty
/// previousKeys represents "no previous snapshot" -- every finding is then new, which is exactly
/// absolute mode; no separate branch is needed for it.
[[nodiscard]] bool hasNewFindingAtOrAboveSeverity(
    const codeqa::rules::ProjectFindingsModel& current,
    const FindingKeySet& previousKeys,
    codeqa::core::DiagnosticSeverity minSeverity);
```

Also add `#include "codeqa/core/DiagnosticSeverity.h"` to the header's includes (it's currently pulled in transitively via `RuleModels.h`, but include it explicitly since this new declaration names the type directly):

```cpp
#include "codeqa/core/DiagnosticSeverity.h"
#include "codeqa/history/SnapshotComparison.h"
#include "codeqa/rules/RuleModels.h"
```

In `src/history/CompareSnapshots.cpp`, add after `compareSnapshots`'s definition:

```cpp
bool hasNewFindingAtOrAboveSeverity(
    const codeqa::rules::ProjectFindingsModel& current,
    const FindingKeySet& previousKeys,
    codeqa::core::DiagnosticSeverity minSeverity)
{
    FindingKeySet remainingPreviousKeys = previousKeys;

    for (const codeqa::rules::FileFindingsModel& file : current.files)
    {
        const std::string relativePath =
            std::filesystem::relative(file.path, current.rootPath).generic_string();

        for (const codeqa::rules::Finding& finding : file.findings)
        {
            const FindingKey key{finding.ruleId, finding.symbolName, relativePath};
            const auto remaining = remainingPreviousKeys.find(key);

            if (remaining != remainingPreviousKeys.end() && remaining->second > 0)
            {
                --remaining->second;
                continue;
            }

            if (finding.severity >= minSeverity)
            {
                return true;
            }
        }
    }

    return false;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build --config Debug --target codeqa_tests && build\Debug\codeqa_tests.exe -tc="*hasNewFindingAtOrAboveSeverity*"`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/history/CompareSnapshots.h src/history/CompareSnapshots.cpp tests/unit/HistoryTest.cpp
git commit -m "feat: add hasNewFindingAtOrAboveSeverity for the quality gate"
```

---

## Task 7: Wire the gate into `analyze`

**Files:**
- Modify: `src/cli/RunApplication.cpp:647-711` (inside `runAnalyze`)
- Test: `tests/unit/RunApplicationTest.cpp`

**Interfaces:**
- Consumes: `codeqa::history::hasNewFindingAtOrAboveSeverity` (Task 6), `config.qualityGate.minSeverity` (Tasks 2-4), `codeqa::cli::ExitCode::QualityGateFailed` (Task 5).

**Context:** `makeTempAnalyzeRoot()` and `makeTempSnapshotRoot()` (both already in `tests/unit/RunApplicationTest.cpp`) write a `sample.c` with a 5-parameter `classify` function, which trips `signature.parameter-count` at **Warning** severity under the default config. Once the gate defaults to `minSeverity: warning`, every existing test that runs `analyze` against one of these roots with no prior *usable* snapshot now correctly returns `QualityGateFailed` instead of `Success` — this is the gate doing exactly its documented job, not a regression. Eight existing tests need their expected exit code updated to match; two need a new test added.

- [ ] **Step 1: Update the existing tests to expect the gate's new, correct behavior**

In `tests/unit/RunApplicationTest.cpp`, apply these five replacements (exact `old_string` → `new_string`, matched by each test's unique title):

Replace:
```cpp
TEST_CASE("analyze with default text format prints a report to stdout")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const RunResult result = run({"analyze", root.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("Code Quality Analysis Report") != std::string::npos);
    CHECK(result.output.find("signature.parameter-count") != std::string::npos);
}
```
with:
```cpp
TEST_CASE("analyze with default text format prints a report to stdout")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const RunResult result = run({"analyze", root.string()});

    // sample.c's 5-parameter classify() trips signature.parameter-count at Warning, and there's
    // no previous snapshot yet, so the default (minSeverity: warning) gate fails in absolute mode.
    // The report is still printed regardless of the gate outcome.
    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::QualityGateFailed));
    CHECK(result.output.find("Code Quality Analysis Report") != std::string::npos);
    CHECK(result.output.find("signature.parameter-count") != std::string::npos);
}
```

Replace:
```cpp
TEST_CASE("analyze --format json --output writes a valid JSON report to a file")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const std::filesystem::path outputPath = root / "report.json";

    const RunResult result = run({"analyze", root.string(), "--format", "json", "--output", outputPath.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.empty());
```
with:
```cpp
TEST_CASE("analyze --format json --output writes a valid JSON report to a file")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const std::filesystem::path outputPath = root / "report.json";

    const RunResult result = run({"analyze", root.string(), "--format", "json", "--output", outputPath.string()});

    // Same gate-triggering fixture as above; the report file is still written regardless.
    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::QualityGateFailed));
    CHECK(result.output.empty());
```

Replace:
```cpp
    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));

    const nlohmann::json report = nlohmann::json::parse(result.output);
    bool found = false;
    for (const nlohmann::json& file : report.at("files"))
    {
        if (file.at("path").get<std::string>() == "util.h")
        {
            found = true;
            CHECK(file.at("language").get<std::string>() == "cpp");
        }
    }
    CHECK(found);
}
```
with:
```cpp
    // Same gate-triggering fixture as above; this test is about header-override precedence.
    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::QualityGateFailed));

    const nlohmann::json report = nlohmann::json::parse(result.output);
    bool found = false;
    for (const nlohmann::json& file : report.at("files"))
    {
        if (file.at("path").get<std::string>() == "util.h")
        {
            found = true;
            CHECK(file.at("language").get<std::string>() == "cpp");
        }
    }
    CHECK(found);
}
```

Replace:
```cpp
TEST_CASE("analyze's first run has no previous snapshot to compare against")
{
    const std::filesystem::path root = makeTempSnapshotRoot();

    const RunResult result = run({"analyze", root.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find(" new, ") == std::string::npos);
    CHECK(result.errorOutput.empty());

    std::filesystem::remove_all(root);
}

TEST_CASE("analyze saves a snapshot under <path>/.codeqa/snapshots")
{
    const std::filesystem::path root = makeTempSnapshotRoot();

    const RunResult result = run({"analyze", root.string()});
    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
```
with:
```cpp
TEST_CASE("analyze's first run has no previous snapshot to compare against")
{
    const std::filesystem::path root = makeTempSnapshotRoot();

    const RunResult result = run({"analyze", root.string()});

    // No previous snapshot means absolute mode; sample.c's finding is Warning-severity, so the
    // default gate (minSeverity: warning) fails -- exactly the "no snapshot leaves no protection
    // gap" behavior the roadmap's absolute mode exists for.
    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::QualityGateFailed));
    CHECK(result.output.find(" new, ") == std::string::npos);
    CHECK(result.errorOutput.empty());

    std::filesystem::remove_all(root);
}

TEST_CASE("analyze saves a snapshot under <path>/.codeqa/snapshots")
{
    const std::filesystem::path root = makeTempSnapshotRoot();

    const RunResult result = run({"analyze", root.string()});
    // First run, absolute-mode gate fails (see the test above); the snapshot is still saved.
    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::QualityGateFailed));
```

Replace:
```cpp
TEST_CASE("analyze's second run after a real code change reports one new and one resolved finding")
{
    const std::filesystem::path root = makeTempSnapshotRoot();

    run({"analyze", root.string()});
    rewriteSnapshotSample(root);
    const RunResult second = run({"analyze", root.string(), "--format", "json"});

    CHECK(second.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    const nlohmann::json report = nlohmann::json::parse(second.output);
```
with:
```cpp
TEST_CASE("analyze's second run after a real code change reports one new and one resolved finding")
{
    const std::filesystem::path root = makeTempSnapshotRoot();

    run({"analyze", root.string()});
    rewriteSnapshotSample(root);
    const RunResult second = run({"analyze", root.string(), "--format", "json"});

    // One new Warning-or-higher finding is exactly what the gate exists to catch.
    CHECK(second.exitCode == static_cast<int>(codeqa::cli::ExitCode::QualityGateFailed));
    const nlohmann::json report = nlohmann::json::parse(second.output);
```

Replace:
```cpp
    const RunResult second = run({"analyze", root.string()});

    CHECK(second.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(second.errorOutput.find("could not be read") != std::string::npos);
    CHECK(second.output.find(" new, ") == std::string::npos);

    std::filesystem::remove_all(root);
}
```
with:
```cpp
    const RunResult second = run({"analyze", root.string()});

    // An unreadable previous snapshot falls back to absolute mode, same as no snapshot at all.
    CHECK(second.exitCode == static_cast<int>(codeqa::cli::ExitCode::QualityGateFailed));
    CHECK(second.errorOutput.find("could not be read") != std::string::npos);
    CHECK(second.output.find(" new, ") == std::string::npos);

    std::filesystem::remove_all(root);
}
```

Note: `"analyze's second run against an unchanged project reports the finding as existing"` (asserts `second.exitCode == Success`) needs **no change** — its finding is already-existing (0 new), so the gate correctly passes as-is.

Then add one new test, anywhere after `makeTempAnalyzeRoot`'s other `--config`-based tests:

```cpp
TEST_CASE("analyze with qualityGate.minSeverity raised to error passes despite a Warning finding")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const std::filesystem::path configPath = root / "codeqa.json";
    std::ofstream(configPath) << R"({ "qualityGate": { "minSeverity": "error" } })";

    const RunResult result = run({"analyze", root.string(), "--config", configPath.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("signature.parameter-count") != std::string::npos);
}
```

- [ ] **Step 2: Run tests to verify they fail as expected**

Run: `cmake --build build --config Debug --target codeqa_tests && build\Debug\codeqa_tests.exe -tc="*analyze*"`
Expected: the 7 updated tests FAIL with `actual: 0, expected: 1` (production code still always returns `Success`); the new `minSeverity` test FAILS too, for a different reason: `minSeverity: error` isn't wired to skip anything yet, so it currently unconditionally passes anyway (`Success == Success`) -- confirm this one is already green even before Step 3, which is fine, it becomes a real regression guard once Step 3 lands.

- [ ] **Step 3: Implement**

In `src/cli/RunApplication.cpp`, inside `runAnalyze`, change:

```cpp
    const std::vector<std::string> previousSnapshots =
        codeqa::application::ListSnapshots{}.execute(parsedArguments.path);
    std::optional<codeqa::history::SnapshotComparison> comparison;
    std::string previousTimestamp;

    if (!previousSnapshots.empty())
    {
        previousTimestamp = previousSnapshots.back();
        const std::optional<nlohmann::json> previousReport =
            codeqa::application::LoadSnapshot{}.execute(parsedArguments.path, previousTimestamp);

        if (!previousReport.has_value())
        {
            errorOutput << "Warning: previous snapshot '" << previousTimestamp
                           << "' could not be read; skipping comparison.\n";
        }
        else
        {
            try
            {
                comparison = codeqa::history::compareSnapshots(
                    codeqa::history::extractFindingKeys(result.findings),
                    codeqa::history::extractFindingKeys(*previousReport));
            }
            catch (const nlohmann::json::exception&)
            {
                errorOutput << "Warning: previous snapshot '" << previousTimestamp
                               << "' has an unexpected shape; skipping comparison.\n";
            }
        }
    }
```

to:

```cpp
    const std::vector<std::string> previousSnapshots =
        codeqa::application::ListSnapshots{}.execute(parsedArguments.path);
    std::optional<codeqa::history::SnapshotComparison> comparison;
    codeqa::history::FindingKeySet previousKeys;
    std::string previousTimestamp;

    if (!previousSnapshots.empty())
    {
        previousTimestamp = previousSnapshots.back();
        const std::optional<nlohmann::json> previousReport =
            codeqa::application::LoadSnapshot{}.execute(parsedArguments.path, previousTimestamp);

        if (!previousReport.has_value())
        {
            errorOutput << "Warning: previous snapshot '" << previousTimestamp
                           << "' could not be read; skipping comparison.\n";
        }
        else
        {
            try
            {
                previousKeys = codeqa::history::extractFindingKeys(*previousReport);
                comparison = codeqa::history::compareSnapshots(
                    codeqa::history::extractFindingKeys(result.findings), previousKeys);
            }
            catch (const nlohmann::json::exception&)
            {
                errorOutput << "Warning: previous snapshot '" << previousTimestamp
                               << "' has an unexpected shape; skipping comparison.\n";
            }
        }
    }
```

(`previousKeys` naturally stays empty -- absolute mode -- whenever there are no previous snapshots, the snapshot can't be read, or it throws while being parsed, since a failed assignment leaves the left-hand side at its prior, default-constructed value.)

Then change the function's final lines, from:

```cpp
    if (!codeqa::application::SaveSnapshot{}.execute(parsedArguments.path, reportJson).has_value())
    {
        errorOutput << "Warning: could not save snapshot.\n";
    }

    return static_cast<int>(ExitCode::Success);
}
```

to:

```cpp
    if (!codeqa::application::SaveSnapshot{}.execute(parsedArguments.path, reportJson).has_value())
    {
        errorOutput << "Warning: could not save snapshot.\n";
    }

    const bool qualityGateFailed = codeqa::history::hasNewFindingAtOrAboveSeverity(
        result.findings, previousKeys, config.qualityGate.minSeverity);

    return static_cast<int>(qualityGateFailed ? ExitCode::QualityGateFailed : ExitCode::Success);
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build --config Debug --target codeqa_tests && build\Debug\codeqa_tests.exe`
Expected: PASS, full suite (this exercises every test in the codebase, not just `RunApplicationTest.cpp` -- confirm nothing outside this file broke, since `RunAnalysis`/`ApplyRules`/`AnalyzeProject` tests never go through `runApplication` and are unaffected).

- [ ] **Step 5: Commit**

```bash
git add src/cli/RunApplication.cpp tests/unit/RunApplicationTest.cpp
git commit -m "feat: wire the quality gate into analyze's exit code"
```

---

## Task 8: Fix the `symbolName` scope leak

**Files:**
- Modify: `src/model/ProjectModelBuilder.cpp:150-240` (`qualifiedNameFor`/`identityFor`, and the one function call site at line 282)
- Test: `tests/unit/ProjectModelBuilderTest.cpp`

**Context (root cause, confirmed by reading the code):** `qualifiedNameFor` decides a function's enclosing scopes purely by geometric byte-range containment against every `frontend::ClassModel` in the file (`encloses(candidate.range, range)`, line 170). This is correct for **namespaces** (a free function genuinely nested in `namespace foo { ... }` should be qualified `foo::`) but wrong for **class/struct/union scopes** applied to a function extraction already determined to be `FunctionKind::Free`: a C++ free function can never be a lexical member of a class/struct/union body (only `Method`/`Constructor`/`Destructor`/`Operator` can), so if a corrupted class range happens to geometrically swallow a free function's start (as it does for fmt's `write_int_data` bug), that class scope must never be applied to it -- **independent of whether the class's own range is trustworthy**. `FunctionKind` is already computed correctly at extraction time via real AST-ancestor walking (`hasClassAncestor`, `StructuralExtractor.cpp:348-359`), so this fix needs no new detection logic at all.

**Interfaces:**
- Consumes: `frontend::FunctionKind` (existing), `frontend::ClassKind` (existing, distinguishes `Namespace` from `Class`/`Struct`/`Union`/`Enum`).
- Produces: `qualifiedNameFor`/`identityFor` gain a new parameter restricting which enclosing scopes apply for `FunctionKind::Free` extractions.

- [ ] **Step 1: Write the failing test**

Add to `tests/unit/ProjectModelBuilderTest.cpp` (reuse the file's existing `rangeFor` helper, already defined at the top of the file):

```cpp
TEST_CASE("a Free-kind function is never class-qualified, even when a corrupted class range geometrically contains it")
{
    const std::filesystem::path rootPath{"project"};
    const std::filesystem::path sourcePath{"project/src/format.h"};

    codeqa::frontend::ParsedFile sourceFile;
    sourceFile.path = sourcePath;
    sourceFile.language = codeqa::core::Language::Cpp;
    sourceFile.grammar = codeqa::frontend::Grammar::Cpp;
    sourceFile.parsed = true;

    // Simulates the fmt write_int_data bug: a struct whose extracted range is corrupted large
    // enough to geometrically swallow a later, unrelated free function's start.
    sourceFile.classes.push_back(codeqa::frontend::ClassModel{
        "write_int_data", codeqa::frontend::ClassKind::Struct, rangeFor(sourcePath, 0, 5000)});

    sourceFile.functions.push_back(codeqa::frontend::FunctionModel{
        "write_significand",
        codeqa::frontend::FunctionKind::Free,
        rangeFor(sourcePath, 2000, 2100),
        {},
        false,
        false});

    const codeqa::model::ProjectModel result = codeqa::model::ProjectModelBuilder{}.build(
        rootPath, std::vector<codeqa::frontend::ParsedFile>{sourceFile});

    REQUIRE(result.files.size() == 1);
    REQUIRE(result.files[0].callables.size() == 1);
    CHECK(result.files[0].callables[0].symbol.qualifiedName == "write_significand");
}

TEST_CASE("a Free-kind function nested in a namespace is still namespace-qualified")
{
    const std::filesystem::path rootPath{"project"};
    const std::filesystem::path sourcePath{"project/src/detail.cpp"};

    codeqa::frontend::ParsedFile sourceFile;
    sourceFile.path = sourcePath;
    sourceFile.language = codeqa::core::Language::Cpp;
    sourceFile.grammar = codeqa::frontend::Grammar::Cpp;
    sourceFile.parsed = true;

    sourceFile.classes.push_back(codeqa::frontend::ClassModel{
        "detail", codeqa::frontend::ClassKind::Namespace, rangeFor(sourcePath, 0, 200)});

    sourceFile.functions.push_back(codeqa::frontend::FunctionModel{
        "helper",
        codeqa::frontend::FunctionKind::Free,
        rangeFor(sourcePath, 20, 60),
        {},
        false,
        false});

    const codeqa::model::ProjectModel result = codeqa::model::ProjectModelBuilder{}.build(
        rootPath, std::vector<codeqa::frontend::ParsedFile>{sourceFile});

    REQUIRE(result.files.size() == 1);
    REQUIRE(result.files[0].callables.size() == 1);
    CHECK(result.files[0].callables[0].symbol.qualifiedName == "detail::helper");
}

TEST_CASE("a Method-kind function is still class-qualified")
{
    const std::filesystem::path rootPath{"project"};
    const std::filesystem::path sourcePath{"project/src/box.cpp"};

    codeqa::frontend::ParsedFile sourceFile;
    sourceFile.path = sourcePath;
    sourceFile.language = codeqa::core::Language::Cpp;
    sourceFile.grammar = codeqa::frontend::Grammar::Cpp;
    sourceFile.parsed = true;

    sourceFile.classes.push_back(codeqa::frontend::ClassModel{
        "Box", codeqa::frontend::ClassKind::Class, rangeFor(sourcePath, 0, 200)});

    sourceFile.functions.push_back(codeqa::frontend::FunctionModel{
        "open",
        codeqa::frontend::FunctionKind::Method,
        rangeFor(sourcePath, 20, 60),
        {},
        false,
        false});

    const codeqa::model::ProjectModel result = codeqa::model::ProjectModelBuilder{}.build(
        rootPath, std::vector<codeqa::frontend::ParsedFile>{sourceFile});

    REQUIRE(result.files.size() == 1);
    REQUIRE(result.files[0].callables.size() == 1);
    CHECK(result.files[0].callables[0].symbol.qualifiedName == "Box::open");
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --config Debug --target codeqa_tests && build\Debug\codeqa_tests.exe -tc="*class-qualified*,*namespace-qualified*"`
Expected: the first test FAILS with `qualifiedName == "write_int_data::write_significand"` (the current bug, reproduced deterministically without needing real tree-sitter corruption); the other two PASS already (they're regression guards for behavior that must not change).

- [ ] **Step 3: Implement**

In `src/model/ProjectModelBuilder.cpp`, change `qualifiedNameFor`'s signature and scope-collection loop:

```cpp
std::string qualifiedNameFor(
    const std::string& name,
    const core::SourceRange& range,
    const std::vector<frontend::ClassModel>& classes,
    bool preserveQualifiedName,
    bool restrictToNamespaceScopes)
{
    if (name.empty())
    {
        return {};
    }

    std::vector<const frontend::ClassModel*> enclosingScopes;

    for (const frontend::ClassModel& candidate : classes)
    {
        if (candidate.name.empty() || !encloses(candidate.range, range))
        {
            continue;
        }

        // A FunctionKind::Free extraction can never be a real lexical member of a
        // class/struct/union body (only Method/Constructor/Destructor/Operator can) --
        // independent of whether this candidate's own range is trustworthy. Namespaces are
        // orthogonal to class membership, so they still apply to a free function.
        if (restrictToNamespaceScopes && candidate.kind != frontend::ClassKind::Namespace)
        {
            continue;
        }

        enclosingScopes.push_back(&candidate);
    }

    std::stable_sort(
```

(the rest of the function body is unchanged).

Change `identityFor`:

```cpp
SymbolIdentity identityFor(
    const std::string& name,
    const core::SourceRange& range,
    const std::vector<frontend::ClassModel>& classes,
    bool preserveQualifiedName = false,
    bool restrictToNamespaceScopes = false)
{
    return SymbolIdentity{
        name, qualifiedNameFor(name, range, classes, preserveQualifiedName, restrictToNamespaceScopes), range};
}
```

Change the one call site that needs the new, non-default argument (inside the `for (const frontend::FunctionModel& function : file.functions)` loop):

```cpp
        CallableModel callable;
        callable.symbol = identityFor(
            function.name, function.range, file.classes, true,
            function.kind == frontend::FunctionKind::Free);
```

(every other `identityFor` call site -- `LocalVariableModel`, `NamespaceModel`, `TypeModel`, `GlobalVariableModel`, `TypeAliasModel` -- keeps using the 3- or 4-argument overload via the new parameter's default, unaffected.)

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build --config Debug --target codeqa_tests && build\Debug\codeqa_tests.exe -tc="*ProjectModelBuilder*"`
Expected: PASS, including the two pre-existing large tests in this file (confirm they still pass unchanged -- they exercise `Method`/`Constructor` kinds and out-of-line qualified definitions, none of which this change touches).

- [ ] **Step 5: Commit**

```bash
git add src/model/ProjectModelBuilder.cpp tests/unit/ProjectModelBuilderTest.cpp
git commit -m "fix: never class-qualify a Free-kind function's symbolName"
```

---

## Task 9: Detect and suppress the fabricated function-length finding

**Files:**
- Modify: `src/model/ProjectModelBuilder.cpp` (new helper + one call site in `buildFileModel`)
- Modify: `src/application/ApplyRules.cpp:86`
- Test: `tests/unit/ProjectModelBuilderTest.cpp`

**Deviation from the spec, and why:** the spec (§7.2) places this detection inside `StructuralExtractor::extract()`. Empirical testing during planning (see below) showed the exact real-world reproduction (`winWrite` in SQLite 3.46.0, confirmed live: `physicalLines: 19703`, matching the Fase 11 ledger exactly) depends on far more surrounding real-file context than any small, hand-written or excerpted fixture reproduces -- several `#if`/`#else`/`#endif` shapes were tried against the real, current `codeqa` binary and each was cleanly caught as `PARSE001` (dropped, not silently corrupted) rather than reproducing the bug. `ProjectModelBuilder::build()`, unlike `StructuralExtractor::extract()`, takes already-extracted `frontend::FunctionModel` values as direct input (exactly what `tests/unit/ProjectModelBuilderTest.cpp` already hand-constructs for its other tests, see Task 8) -- so placing the *detection* logic here makes it deterministically unit-testable without needing tree-sitter to cooperate at all. The real end-to-end proof that this catches the actual bug is Task 10's live re-run against SQLite 3.46.0, which this plan has already confirmed will demonstrate the fix (see Task 10).

**Root cause invariant (mathematically sound, no false positives possible in valid C/C++):** a `FunctionKind::Free` function is, by definition, a real top-level `function_definition` (never a nested declaration or a lambda) with no class ancestor. Two distinct `Free` functions can never be lexically nested inside one another in valid C/C++ -- there is no such thing as a nested free-function *definition*. So if one Free function's range contains another Free function's start offset, the containing range is provably corrupted.

**Interfaces:**
- Consumes: `frontend::FunctionModel`, `frontend::FunctionKind::Free`, `core::Diagnostic` (all existing).
- Produces: a `PARSE004` diagnostic on the corrupted function's own location, appended to `FileModel.diagnostics`. `ApplyRules::overlapsParseError` now also skips any callable overlapping `PARSE004`, exactly like `PARSE001`/`PARSE002` today.

- [ ] **Step 1: Write the failing test**

Add to `tests/unit/ProjectModelBuilderTest.cpp`:

```cpp
TEST_CASE("a Free function whose range swallows another Free function's start gets a PARSE004 diagnostic")
{
    const std::filesystem::path rootPath{"project"};
    const std::filesystem::path sourcePath{"project/src/os_win.c"};

    codeqa::frontend::ParsedFile sourceFile;
    sourceFile.path = sourcePath;
    sourceFile.language = codeqa::core::Language::C;
    sourceFile.grammar = codeqa::frontend::Grammar::C;
    sourceFile.parsed = true;

    // Simulates the real winWrite bug: a corrupted range (0-19703) that swallows the start of
    // the next real, correctly-ranged sibling function (winTruncate-equivalent, at 100-176).
    sourceFile.functions.push_back(codeqa::frontend::FunctionModel{
        "winWrite", codeqa::frontend::FunctionKind::Free, rangeFor(sourcePath, 0, 19703), {}, false, false});
    sourceFile.functions.push_back(codeqa::frontend::FunctionModel{
        "winTruncate", codeqa::frontend::FunctionKind::Free, rangeFor(sourcePath, 100, 176), {}, false, false});

    const codeqa::model::ProjectModel result = codeqa::model::ProjectModelBuilder{}.build(
        rootPath, std::vector<codeqa::frontend::ParsedFile>{sourceFile});

    REQUIRE(result.files.size() == 1);
    const std::vector<codeqa::core::Diagnostic>& diagnostics = result.files[0].diagnostics;
    const bool hasParse004 = std::any_of(
        diagnostics.begin(), diagnostics.end(),
        [](const codeqa::core::Diagnostic& diagnostic) { return diagnostic.code == "PARSE004"; });
    CHECK(hasParse004);
}

TEST_CASE("two Free functions with non-overlapping ranges never get a PARSE004 diagnostic")
{
    const std::filesystem::path rootPath{"project"};
    const std::filesystem::path sourcePath{"project/src/clean.c"};

    codeqa::frontend::ParsedFile sourceFile;
    sourceFile.path = sourcePath;
    sourceFile.language = codeqa::core::Language::C;
    sourceFile.grammar = codeqa::frontend::Grammar::C;
    sourceFile.parsed = true;

    sourceFile.functions.push_back(codeqa::frontend::FunctionModel{
        "first", codeqa::frontend::FunctionKind::Free, rangeFor(sourcePath, 0, 50), {}, false, false});
    sourceFile.functions.push_back(codeqa::frontend::FunctionModel{
        "second", codeqa::frontend::FunctionKind::Free, rangeFor(sourcePath, 50, 100), {}, false, false});

    const codeqa::model::ProjectModel result = codeqa::model::ProjectModelBuilder{}.build(
        rootPath, std::vector<codeqa::frontend::ParsedFile>{sourceFile});

    REQUIRE(result.files.size() == 1);
    CHECK(result.files[0].diagnostics.empty());
}
```

Add `#include <algorithm>` to `tests/unit/ProjectModelBuilderTest.cpp` if not already present (check the file's existing includes first; it already includes `<vector>` and `<string>`).

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --config Debug --target codeqa_tests && build\Debug\codeqa_tests.exe -tc="*PARSE004*"`
Expected: the first test FAILS (`hasParse004` is `false` -- nothing emits this diagnostic yet); the second PASSES already (there's nothing to regress yet, but it locks in "no false positives on adjacent non-overlapping functions" going forward).

- [ ] **Step 3: Implement**

In `src/model/ProjectModelBuilder.cpp`, add a new helper in the anonymous namespace, near `qualifiedNameFor`:

```cpp
// A FunctionKind::Free extraction is always a real, top-level function_definition -- C/C++ has
// no such thing as a nested free-function definition. So if one Free function's range contains
// another Free function's start offset, the containing range is provably corrupted (this is
// exactly how the real winWrite bug -- SQLite 3.46.0, see the Fase 11 validation ledger --
// manifests: a preprocessor-conditional brace pattern inflates one function's tree-sitter range
// until it swallows a later sibling's start). No preprocessor semantics are modeled here; this
// only compares already-extracted ranges.
void flagCorruptedFunctionRanges(
    const std::vector<frontend::FunctionModel>& functions, std::vector<core::Diagnostic>& diagnostics)
{
    for (const frontend::FunctionModel& outer : functions)
    {
        if (outer.kind != frontend::FunctionKind::Free)
        {
            continue;
        }

        for (const frontend::FunctionModel& inner : functions)
        {
            if (&inner == &outer || inner.kind != frontend::FunctionKind::Free)
            {
                continue;
            }

            if (inner.range.begin.offset > outer.range.begin.offset
                && inner.range.begin.offset < outer.range.end.offset)
            {
                core::Diagnostic diagnostic;
                diagnostic.code = "PARSE004";
                diagnostic.severity = core::DiagnosticSeverity::Error;
                diagnostic.message = "Function or type range appears corrupted.";
                diagnostic.location = outer.range.begin;
                diagnostics.push_back(diagnostic);
                break;
            }
        }
    }
}
```

In `buildFileModel`, call it right after `model.diagnostics = file.diagnostics;` and the `if (!file.parsed) { return model; }` early-return (i.e., after line "model.comments = file.comments;"):

```cpp
    model.comments = file.comments;
    flagCorruptedFunctionRanges(file.functions, model.diagnostics);
```

In `src/application/ApplyRules.cpp`, change `overlapsParseError`'s check (line 86) from:

```cpp
        if (diagnostic.code != "PARSE001" && diagnostic.code != "PARSE002")
```

to:

```cpp
        if (diagnostic.code != "PARSE001" && diagnostic.code != "PARSE002" && diagnostic.code != "PARSE004")
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build --config Debug --target codeqa_tests && build\Debug\codeqa_tests.exe -tc="*PARSE004*,*ProjectModelBuilder*,*ApplyRules*"`
Expected: PASS.

Run the full suite once: `build\Debug\codeqa_tests.exe`
Expected: PASS (confirms `flagCorruptedFunctionRanges` doesn't introduce false positives against every existing corpus fixture with multiple free functions).

- [ ] **Step 5: Commit**

```bash
git add src/model/ProjectModelBuilder.cpp src/application/ApplyRules.cpp tests/unit/ProjectModelBuilderTest.cpp
git commit -m "fix: flag corrupted function ranges with PARSE004, suppress their findings"
```

---

## Task 10: End-to-end verification against real SQLite 3.46.0

**Files:** none changed -- this is a manual verification step, documented in the task's own report, matching how Fase 11's own real-project validation was never added to `ctest` (see `tests/corpus/REAL-PROJECTS.md`'s existing convention: real third-party source is downloaded transiently, never committed).

**Context:** this plan already confirmed empirically (during planning, reused here rather than redone from scratch) that against the real, pinned SQLite 3.46.0 amalgamation, `winWrite` currently reports a fabricated `size.function-length` finding (`observedValue: 19703`, `severity: error`, exactly matching the Fase 11 ledger's `sqlite3.c:49451` entry) and no other diagnostic explains it away. This task re-runs the exact same real-world check against the `codeqa` binary built from this plan's finished code, to confirm Tasks 8-9's fix actually closes the real bug, not just the synthetic regression tests.

- [ ] **Step 1: Build the finished binary**

Run: `cmake --build build --config Debug --target codeqa`
Expected: builds cleanly with no new warnings under the project's normal (non-`-DCODEQA_WARNINGS_AS_ERRORS`) configuration.

- [ ] **Step 2: Fetch the pinned SQLite 3.46.0 amalgamation (external, not committed)**

Run (PowerShell or bash, into a scratch directory outside the repo):

```bash
curl -sL -o sqlite-amal.zip https://www.sqlite.org/2024/sqlite-amalgamation-3460000.zip
mkdir sqlite-check && cd sqlite-check
unzip -o -q ../sqlite-amal.zip
```

Expected: `sqlite-amalgamation-3460000/sqlite3.c` (~257,673 lines) extracted. This is the exact URL/version already pinned by `tests/corpus/REAL-PROJECTS.md` for Fase 11 (verified reachable and correct during this plan's own investigation).

- [ ] **Step 3: Run `codeqa analyze` and confirm the fabricated finding is gone**

Run:

```bash
codeqa.exe analyze <path-to>/sqlite-amalgamation-3460000 --format json --output sqlite-report.json
```

Then inspect `sqlite-report.json`:
- `winWrite` must **not** appear in any file's `findings` array under `ruleId: "size.function-length"`.
- `winWrite`'s entry in the `metrics` array is still present, and its `size.physicalLines` may still read a large, wrong number (the spec's §7.2 explicitly leaves the raw metric display untouched -- only the fabricated *finding* is suppressed, matching the existing `PARSE001`/`PARSE002` precedent).
- The corresponding file's `diagnostics` array now contains one entry with `"code": "PARSE004"` at `winWrite`'s location (line 49451).
- `winTruncate` (the real sibling function winWrite's corrupted range used to swallow) is unaffected: it still reports its own correct `physicalLines` (76) and is still eligible for its own findings.

- [ ] **Step 4: Document the result**

No code or test changes result from this task. Record the before/after `winWrite` finding-count comparison (1 fabricated finding before, 0 after) in the task's own completion report, per this project's established pattern of documenting real-project verification outcomes in-session rather than as a committed artifact (`tests/corpus/VALIDATION-METHODOLOGY.md` follows the same convention for Fase 11).

- [ ] **Step 5: No commit** -- this task changes no tracked files.

---

## Self-Review Notes

- **Spec coverage:** §4-5 (gate config + evaluation) → Tasks 2, 3, 4, 6, 7. §6 (exit codes) → Task 5. §7.1 (scope leak) → Task 8. §7.2 (fabricated length) → Task 9, with one documented, justified deviation in *where* the detection lives (testability), not in *what* it does. §8 (verification) → Task 10 (real SQLite), all new unit tests across Tasks 1-9 (config parsing boundary/limit/round-trip, gate absolute/differential/boundary, scope-leak regression, corrupted-range regression).
- **Existing-test ripple:** audited every `TEST_CASE` in `tests/unit/RunApplicationTest.cpp` that calls `run({"analyze", ...})` and asserts an exit code; the 8 affected by the gate defaulting to `minSeverity: warning` are enumerated exactly in Tasks 5 and 7 (no "audit and fix as needed" placeholder) -- confirmed by reading the fixtures both `makeTempAnalyzeRoot()` and `makeTempSnapshotRoot()` build (a 5-parameter `classify` function, `signature.parameter-count` at Warning under the default config).
- **Type consistency:** `hasNewFindingAtOrAboveSeverity`'s signature (Task 6) matches its call site in `runAnalyze` (Task 7) exactly; `flagCorruptedFunctionRanges`'s parameters (Task 9) match `buildFileModel`'s locally-available `file.functions`/`model.diagnostics`; `qualifiedNameFor`/`identityFor`'s new `restrictToNamespaceScopes` parameter (Task 8) is threaded through consistently with a default of `false` so every other call site is unaffected.
