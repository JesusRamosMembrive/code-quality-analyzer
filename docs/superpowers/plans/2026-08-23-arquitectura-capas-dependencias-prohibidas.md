# Architecture Layers and Forbidden Dependencies Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close roadmap v2 track #4 by adding user-declared architecture layers (named glob-pattern groups) and an explicit list of forbidden layer-to-layer dependency pairs. A direct, resolved `#include` edge whose two endpoints fall in a forbidden pair is reported as a `ForbiddenDependencyViolation` in the existing `architecture` report section — same place as cycles and fan-out, no `Rule`/`FileRule` involvement, no suppression comment, no quality-gate participation.

**Architecture:** `ArchitectureConfig` gains `layers` (`std::map<std::string, std::vector<std::string>>`) and `forbiddenDependencies` (`std::vector<LayerDependency>`), both optional and validated in `ConfigJson.cpp` (Task 1). A new `computeForbiddenDependencies(graph, rootPath, config)` (Task 2) reuses the `IncludeGraph` already built for cycles/fan-out — no new graph resolution — classifies each file into a layer via the existing glob engine (`codeqa::infrastructure::matchesExcludePattern`), and flags any direct edge crossing a declared forbidden pair. Results flow into `ProjectArchitectureModel::forbiddenDependencyViolations`, then into the JSON and text reports (Task 3) via the same rendering functions cycles/fan-out already use. Task 4 verifies two things the *first* slice of this track got wrong in code that looked identical to this (a real, historical `MergeConfig.cpp` bug): that the new config fields survive `mergeConfig` and a desktop load→save round trip untouched, even though nothing edits them from the GUI.

**Tech Stack:** C++17, doctest (`codeqa_tests`), Qt6 (`codeqa_desktop_tests`), CMake (MSVC `build/` for `codeqa_tests`, MSVC+Qt `buildQt/` for the desktop app and its tests).

**Spec:** `docs/superpowers/specs/2026-08-23-arquitectura-capas-dependencias-prohibidas-design.md`

## Global Constraints

- New config struct `codeqa::config::LayerDependency{from, to}`, both `std::string`. New `ArchitectureConfig` fields: `std::map<std::string, std::vector<std::string>> layers;` and `std::vector<LayerDependency> forbiddenDependencies;` — both default-empty ("no restriction").
- Validation error codes, exact: `CONFIG014` when `forbiddenDependencies[i].from` or `.to` references a layer name not present in `layers` (one throw per offending field, reusing the existing message style); `CONFIG015` when `from == to` in one forbidden-dependency entry. `layers`/`forbiddenDependencies` themselves being malformed (not an object/array, non-string entries) reuse the existing generic `CONFIG005` ("must be an object"/"must be an array"/"must be a string").
- `layers` is parsed and populated **before** `forbiddenDependencies` in `parseArchitecture` — the `from`/`to` validation needs `config.layers` already built.
- Layer-membership tie-break when a file matches more than one layer's patterns: **alphabetically-first layer name wins** — a direct, intentional consequence of `layers` being a `std::map` (iteration order == key order), not a new ordering mechanism.
- A file matching no layer's pattern is never classified — it can never be the origin or destination of a violation.
- Only **direct** resolved edges are checked (the same `IncludeGraph::outgoingEdges` cycles/fan-out already use) — no transitive closure.
- `computeForbiddenDependencies` computes each file's project-relative path via `path.lexically_relative(rootPath)` — **not** `std::filesystem::relative`. This is a deliberate refinement over the spec's pseudocode (which wrote `std::filesystem::relative`): `lexically_relative` is pure/lexical (no disk I/O, no dependency on `current_path()`), matches `codeqa::infrastructure::DiscoverSourceFiles.cpp`'s own established convention for this exact "make a rootPath-prefixed path glob-matchable" operation, and avoids a real determinism pitfall the report layer's `std::filesystem::relative` calls only get away with because they always relativize genuinely-joined paths. Note this refinement in the Task 2 commit.
- `ForbiddenDependencyViolation{fromPath, fromLayer, toPath, toLayer}` carries **no severity field** — it is a binary rule, not a graduated threshold. The text report renders it via the existing `severityTag(codeqa::core::DiagnosticSeverity::Error)` helper (fixed at the call site, not stored on the struct).
- `computeForbiddenDependencies` takes `const codeqa::config::ArchitectureConfig&` directly (a deliberate, documented departure from `findCycles`/`computeFanInOut`'s config-free signatures — see spec §5's design note) — `IncludeGraph.h` gains a new `#include "codeqa/config/ConfigModels.h"`. This does not introduce a dependency cycle (`codeqa::config` never depends on `codeqa::architecture`).
- No changes to `Rule`, `RuleContext`, `FileRule`, `FileRuleContext`, or any of the 12 existing rule ids. No changes to `findCycles`/`computeFanInOut`'s existing behavior or signatures.
- No desktop GUI form fields for `layers`/`forbiddenDependencies` — `codeqa.json` only. The desktop app must still round-trip these fields losslessly through its existing whole-struct `ArchitectureConfig` handling (verified by a test, not assumed).
- No suppression comment, no quality-gate participation for forbidden-dependency violations — matches the existing cycles/fan-out precedent exactly.

---

### Task 1: Config model, parsing, validation, and serialization

**Files:**
- Modify: `include/codeqa/config/ConfigModels.h:37-43` (new `LayerDependency` struct + `ArchitectureConfig` fields)
- Modify: `src/config/ConfigJson.cpp:254-283` (`parseArchitecture()` extended), `:385-388` (`toJson()` extended)
- Modify: `tests/unit/ConfigJsonTest.cpp` (8 new test cases)
- Modify: `tests/unit/MergeConfigTest.cpp` (1 new test case)

**Interfaces:**
- Consumes: existing `ParseFailure`, `requireObject`, `requireKeysSubset`, `requireString` helpers already in `ConfigJson.cpp`'s anonymous namespace.
- Produces: `codeqa::config::LayerDependency{from, to}` and `ArchitectureConfig{enabled, fanOutWarning, fanOutHigh, layers, forbiddenDependencies}` — consumed by Task 2's `computeForbiddenDependencies` and Task 4's desktop round-trip test.

- [ ] **Step 1: Write the failing config tests**

Open `tests/unit/ConfigJsonTest.cpp`. Append at the true end of the file (after the last `TEST_CASE`, which currently ends at line 420):

```cpp

TEST_CASE("parseConfig accepts architecture layers and forbiddenDependencies")
{
    const std::string text = R"({
        "architecture": {
            "layers": {"core": ["src/core/**"], "desktop": ["src/desktop/**"]},
            "forbiddenDependencies": [{"from": "core", "to": "desktop"}]
        }
    })";
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.config.has_value());
    REQUIRE(result.config->architecture.layers.count("core") == 1);
    CHECK(result.config->architecture.layers.at("core") == std::vector<std::string>{"src/core/**"});
    REQUIRE(result.config->architecture.forbiddenDependencies.size() == 1);
    CHECK(result.config->architecture.forbiddenDependencies[0].from == "core");
    CHECK(result.config->architecture.forbiddenDependencies[0].to == "desktop");
}

TEST_CASE("parseConfig reports CONFIG005 when a layer's pattern list is not an array")
{
    const std::string text = R"({ "architecture": { "layers": {"core": "src/core/**"} } })";
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE_FALSE(result.config.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG005") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG005 when a layer pattern entry is not a string")
{
    const std::string text = R"({ "architecture": { "layers": {"core": [123]} } })";
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE_FALSE(result.config.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG005") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG005 when forbiddenDependencies is not an array")
{
    const std::string text = R"({ "architecture": { "forbiddenDependencies": {} } })";
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE_FALSE(result.config.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG005") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG014 when forbiddenDependencies references an undeclared 'from' layer")
{
    const std::string text = R"({
        "architecture": {
            "layers": {"desktop": ["src/desktop/**"]},
            "forbiddenDependencies": [{"from": "core", "to": "desktop"}]
        }
    })";
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE_FALSE(result.config.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG014") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG014 when forbiddenDependencies references an undeclared 'to' layer")
{
    const std::string text = R"({
        "architecture": {
            "layers": {"core": ["src/core/**"]},
            "forbiddenDependencies": [{"from": "core", "to": "desktop"}]
        }
    })";
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE_FALSE(result.config.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG014") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG015 when a forbidden pair names the same layer twice")
{
    const std::string text = R"({
        "architecture": {
            "layers": {"core": ["src/core/**"]},
            "forbiddenDependencies": [{"from": "core", "to": "core"}]
        }
    })";
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE_FALSE(result.config.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG015") != std::string::npos);
}

TEST_CASE("toJson round-trips architecture layers and forbiddenDependencies through parseConfig")
{
    codeqa::config::ProjectConfig original = codeqa::config::defaultConfig();
    original.architecture.layers = {{"core", {"src/core/**"}}, {"desktop", {"src/desktop/**"}}};
    original.architecture.forbiddenDependencies = {{"core", "desktop"}};

    const codeqa::config::ParseConfigResult result =
        codeqa::config::parseConfig(codeqa::config::toJson(original).dump());

    REQUIRE(result.config.has_value());
    REQUIRE(result.config->architecture.layers.count("core") == 1);
    CHECK(result.config->architecture.layers.at("core") == std::vector<std::string>{"src/core/**"});
    REQUIRE(result.config->architecture.forbiddenDependencies.size() == 1);
    CHECK(result.config->architecture.forbiddenDependencies[0].from == "core");
    CHECK(result.config->architecture.forbiddenDependencies[0].to == "desktop");
}
```

Open `tests/unit/MergeConfigTest.cpp`. Append at the true end of the file (after the last `TEST_CASE`, which currently ends at line 117):

```cpp

TEST_CASE("mergeConfig preserves architecture layers and forbiddenDependencies through a wholesale replace")
{
    codeqa::config::ProjectConfig defaults;

    codeqa::config::ProjectConfig user;
    user.architecture.layers = {{"core", {"src/core/**"}}, {"desktop", {"src/desktop/**"}}};
    user.architecture.forbiddenDependencies = {{"core", "desktop"}};

    const codeqa::config::ProjectConfig merged = codeqa::config::mergeConfig(defaults, user);

    REQUIRE(merged.architecture.layers.count("core") == 1);
    CHECK(merged.architecture.layers.at("core") == std::vector<std::string>{"src/core/**"});
    REQUIRE(merged.architecture.forbiddenDependencies.size() == 1);
    CHECK(merged.architecture.forbiddenDependencies[0].from == "core");
    CHECK(merged.architecture.forbiddenDependencies[0].to == "desktop");
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="*architecture*layers*,*forbidden*,*Forbidden*"
```
Expected: FAIL to compile — `codeqa::config::LayerDependency` and `ArchitectureConfig::layers`/`forbiddenDependencies` do not exist yet.

- [ ] **Step 3: Add `LayerDependency` and the two new `ArchitectureConfig` fields**

In `include/codeqa/config/ConfigModels.h`, this block currently reads (lines 37-43):

```cpp
struct ArchitectureConfig
{
    bool enabled{true};
    long long fanOutWarning{10};
    long long fanOutHigh{20};
};
```

Change to:

```cpp
struct LayerDependency
{
    std::string from;
    std::string to;
};

struct ArchitectureConfig
{
    bool enabled{true};
    long long fanOutWarning{10};
    long long fanOutHigh{20};
    std::map<std::string, std::vector<std::string>> layers;
    std::vector<LayerDependency> forbiddenDependencies;
};
```

(`<map>`/`<string>`/`<vector>` are already included at the top of this file — no new includes needed.)

- [ ] **Step 4: Extend `parseArchitecture()`**

In `src/config/ConfigJson.cpp`, `parseArchitecture()` currently reads (lines 254-283):

```cpp
ArchitectureConfig parseArchitecture(const Json& architecture)
{
    requireKeysSubset(architecture, {"enabled", "fanOutWarning", "fanOutHigh"}, "architecture");

    ArchitectureConfig config;

    if (architecture.contains("enabled"))
    {
        config.enabled = requireBool(architecture, "enabled", "architecture");
    }

    if (architecture.contains("fanOutWarning"))
    {
        config.fanOutWarning = requireInteger(architecture, "fanOutWarning", "architecture");
    }

    if (architecture.contains("fanOutHigh"))
    {
        config.fanOutHigh = requireInteger(architecture, "fanOutHigh", "architecture");
    }

    if (config.fanOutWarning > config.fanOutHigh)
    {
        throw ParseFailure(
            "Error [CONFIG013]: 'architecture' has fanOutWarning (" + std::to_string(config.fanOutWarning)
            + ") greater than fanOutHigh (" + std::to_string(config.fanOutHigh) + ").");
    }

    return config;
}
```

Change to:

```cpp
ArchitectureConfig parseArchitecture(const Json& architecture)
{
    requireKeysSubset(
        architecture, {"enabled", "fanOutWarning", "fanOutHigh", "layers", "forbiddenDependencies"},
        "architecture");

    ArchitectureConfig config;

    if (architecture.contains("enabled"))
    {
        config.enabled = requireBool(architecture, "enabled", "architecture");
    }

    if (architecture.contains("fanOutWarning"))
    {
        config.fanOutWarning = requireInteger(architecture, "fanOutWarning", "architecture");
    }

    if (architecture.contains("fanOutHigh"))
    {
        config.fanOutHigh = requireInteger(architecture, "fanOutHigh", "architecture");
    }

    if (architecture.contains("layers"))
    {
        const Json& layers = architecture.at("layers");
        requireObject(layers, "architecture.layers");

        for (const auto& [layerName, patternsJson] : layers.items())
        {
            const std::string path = "architecture.layers." + layerName;

            if (!patternsJson.is_array())
            {
                throw ParseFailure("Error [CONFIG005]: '" + path + "' must be an array.");
            }

            std::vector<std::string> patterns;
            for (const Json& patternJson : patternsJson)
            {
                if (!patternJson.is_string())
                {
                    throw ParseFailure("Error [CONFIG005]: '" + path + "' entries must be strings.");
                }
                patterns.push_back(patternJson.get<std::string>());
            }

            config.layers[layerName] = std::move(patterns);
        }
    }

    if (architecture.contains("forbiddenDependencies"))
    {
        const Json& forbidden = architecture.at("forbiddenDependencies");

        if (!forbidden.is_array())
        {
            throw ParseFailure("Error [CONFIG005]: 'architecture.forbiddenDependencies' must be an array.");
        }

        for (std::size_t index = 0; index < forbidden.size(); ++index)
        {
            const std::string entryPath = "architecture.forbiddenDependencies[" + std::to_string(index) + "]";
            const Json& entry = forbidden.at(index);
            requireObject(entry, entryPath);
            requireKeysSubset(entry, {"from", "to"}, entryPath);

            LayerDependency dependency;
            dependency.from = requireString(entry, "from", entryPath);
            dependency.to = requireString(entry, "to", entryPath);

            if (config.layers.find(dependency.from) == config.layers.end())
            {
                throw ParseFailure(
                    "Error [CONFIG014]: '" + entryPath + ".from' references undeclared layer '" + dependency.from
                    + "'.");
            }

            if (config.layers.find(dependency.to) == config.layers.end())
            {
                throw ParseFailure(
                    "Error [CONFIG014]: '" + entryPath + ".to' references undeclared layer '" + dependency.to
                    + "'.");
            }

            if (dependency.from == dependency.to)
            {
                throw ParseFailure(
                    "Error [CONFIG015]: '" + entryPath + "' has the same layer ('" + dependency.from
                    + "') as both 'from' and 'to'.");
            }

            config.forbiddenDependencies.push_back(std::move(dependency));
        }
    }

    if (config.fanOutWarning > config.fanOutHigh)
    {
        throw ParseFailure(
            "Error [CONFIG013]: 'architecture' has fanOutWarning (" + std::to_string(config.fanOutWarning)
            + ") greater than fanOutHigh (" + std::to_string(config.fanOutHigh) + ").");
    }

    return config;
}
```

`std::size_t` is already available in this file (used elsewhere, e.g. `parseHeaderOverrides`).

- [ ] **Step 5: Extend `toJson()`**

In `src/config/ConfigJson.cpp`, this block currently reads (lines 385-388):

```cpp
    document["architecture"] = {
        {"enabled", config.architecture.enabled},
        {"fanOutWarning", config.architecture.fanOutWarning},
        {"fanOutHigh", config.architecture.fanOutHigh}};
```

Change to:

```cpp
    document["architecture"] = {
        {"enabled", config.architecture.enabled},
        {"fanOutWarning", config.architecture.fanOutWarning},
        {"fanOutHigh", config.architecture.fanOutHigh}};

    document["architecture"]["layers"] = Json::object();
    for (const auto& [layerName, patterns] : config.architecture.layers)
    {
        document["architecture"]["layers"][layerName] = patterns;
    }

    document["architecture"]["forbiddenDependencies"] = Json::array();
    for (const codeqa::config::LayerDependency& dependency : config.architecture.forbiddenDependencies)
    {
        document["architecture"]["forbiddenDependencies"].push_back(
            {{"from", dependency.from}, {"to", dependency.to}});
    }
```

- [ ] **Step 6: Run the tests to verify they pass**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="*architecture*layers*,*forbidden*,*Forbidden*"
```
Expected: PASS (9 test cases, 0 failures: 8 in `ConfigJsonTest.cpp`, 1 in `MergeConfigTest.cpp`).

- [ ] **Step 7: Run the full test suite**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe
```
Expected: PASS, all test cases, 0 failures.

- [ ] **Step 8: Commit**

```bash
git add include/codeqa/config/ConfigModels.h src/config/ConfigJson.cpp tests/unit/ConfigJsonTest.cpp tests/unit/MergeConfigTest.cpp
git commit -m "feat: add architecture layers and forbiddenDependencies to config"
```

---

### Task 2: Detection algorithm and wiring

**Files:**
- Modify: `include/codeqa/architecture/ArchitectureModels.h:1-41` (new `ForbiddenDependencyViolation` struct + `ProjectArchitectureModel` field)
- Modify: `include/codeqa/architecture/IncludeGraph.h:1-27` (new include + new declaration)
- Create: `src/architecture/ComputeForbiddenDependencies.cpp`
- Modify: `src/architecture/AnalyzeArchitecture.cpp:8-22` (wire the new call)
- Modify: `CMakeLists.txt:119-122` (register the new source file)
- Create: `tests/unit/ComputeForbiddenDependenciesTest.cpp`
- Modify: `tests/CMakeLists.txt:36-38` (register the new test file)
- Modify: `tests/unit/AnalyzeArchitectureTest.cpp` (2 new test cases + 1-line extension to the existing "disabled" test)

**Interfaces:**
- Consumes: `codeqa::config::ArchitectureConfig::layers`/`forbiddenDependencies` (Task 1); the existing `IncludeGraph{filePaths, outgoingEdges}` and `codeqa::infrastructure::matchesExcludePattern(pattern, relativePath)`.
- Produces: `codeqa::architecture::ForbiddenDependencyViolation{fromPath, fromLayer, toPath, toLayer}`, `computeForbiddenDependencies(graph, rootPath, config)`, and `ProjectArchitectureModel::forbiddenDependencyViolations` — consumed by Task 3's report bridging.

- [ ] **Step 1: Write the failing tests**

Create `tests/unit/ComputeForbiddenDependenciesTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/architecture/IncludeGraph.h"

namespace
{

codeqa::config::ArchitectureConfig makeConfig(
    std::map<std::string, std::vector<std::string>> layers,
    std::vector<codeqa::config::LayerDependency> forbiddenDependencies)
{
    codeqa::config::ArchitectureConfig config;
    config.layers = std::move(layers);
    config.forbiddenDependencies = std::move(forbiddenDependencies);
    return config;
}

} // namespace

TEST_CASE("computeForbiddenDependencies flags an edge crossing a forbidden layer pair")
{
    codeqa::architecture::IncludeGraph graph;
    graph.filePaths = {"/project/src/core/a.h", "/project/src/desktop/b.h"};
    graph.outgoingEdges = {{1}, {}};

    const codeqa::config::ArchitectureConfig config =
        makeConfig({{"core", {"src/core/**"}}, {"desktop", {"src/desktop/**"}}}, {{"core", "desktop"}});

    const std::vector<codeqa::architecture::ForbiddenDependencyViolation> violations =
        codeqa::architecture::computeForbiddenDependencies(graph, "/project", config);

    REQUIRE(violations.size() == 1);
    CHECK(violations[0].fromPath == "/project/src/core/a.h");
    CHECK(violations[0].fromLayer == "core");
    CHECK(violations[0].toPath == "/project/src/desktop/b.h");
    CHECK(violations[0].toLayer == "desktop");
}

TEST_CASE("computeForbiddenDependencies ignores an edge between layers with no forbidden pair")
{
    codeqa::architecture::IncludeGraph graph;
    graph.filePaths = {"/project/src/core/a.h", "/project/src/desktop/b.h"};
    graph.outgoingEdges = {{1}, {}};

    const codeqa::config::ArchitectureConfig config =
        makeConfig({{"core", {"src/core/**"}}, {"desktop", {"src/desktop/**"}}}, {});

    CHECK(codeqa::architecture::computeForbiddenDependencies(graph, "/project", config).empty());
}

TEST_CASE("computeForbiddenDependencies never flags a file that matches no declared layer")
{
    codeqa::architecture::IncludeGraph graph;
    graph.filePaths = {"/project/src/core/a.h", "/project/src/vendor/b.h"};
    graph.outgoingEdges = {{1}, {}};

    // "vendor" is referenced by forbiddenDependencies but never declared in `layers` -- impossible
    // via real config parsing (rejected at CONFIG014), but this low-level test exercises the
    // algorithm directly and must still behave safely: "src/vendor/b.h" matches no *declared*
    // layer pattern, so layerOf() returns nullopt for it and the edge is skipped.
    const codeqa::config::ArchitectureConfig config = makeConfig({{"core", {"src/core/**"}}}, {{"core", "vendor"}});

    CHECK(codeqa::architecture::computeForbiddenDependencies(graph, "/project", config).empty());
}

TEST_CASE("computeForbiddenDependencies does not flag the reverse direction of a declared pair")
{
    codeqa::architecture::IncludeGraph graph;
    graph.filePaths = {"/project/src/core/a.h", "/project/src/desktop/b.h"};
    graph.outgoingEdges = {{}, {0}}; // desktop -> core; only core -> desktop is forbidden

    const codeqa::config::ArchitectureConfig config =
        makeConfig({{"core", {"src/core/**"}}, {"desktop", {"src/desktop/**"}}}, {{"core", "desktop"}});

    CHECK(codeqa::architecture::computeForbiddenDependencies(graph, "/project", config).empty());
}

TEST_CASE("computeForbiddenDependencies breaks a multi-layer match by alphabetically-first layer name")
{
    codeqa::architecture::IncludeGraph graph;
    graph.filePaths = {"/project/src/shared/a.h", "/project/src/other/b.h"};
    graph.outgoingEdges = {{1}, {}};

    // "src/shared/a.h" matches both "alpha" and "zeta"'s patterns -- "alpha" wins (std::map
    // iterates layers alphabetically by name). Only {alpha -> other} is forbidden, so if "zeta"
    // won instead, this would produce zero violations instead of one.
    const codeqa::config::ArchitectureConfig config = makeConfig(
        {{"alpha", {"src/shared/**"}}, {"zeta", {"src/shared/**"}}, {"other", {"src/other/**"}}},
        {{"alpha", "other"}});

    const std::vector<codeqa::architecture::ForbiddenDependencyViolation> violations =
        codeqa::architecture::computeForbiddenDependencies(graph, "/project", config);

    REQUIRE(violations.size() == 1);
    CHECK(violations[0].fromLayer == "alpha");
}

TEST_CASE("computeForbiddenDependencies returns immediately when layers or forbiddenDependencies is empty")
{
    codeqa::architecture::IncludeGraph graph;
    graph.filePaths = {"/project/src/core/a.h", "/project/src/desktop/b.h"};
    graph.outgoingEdges = {{1}, {}};

    CHECK(codeqa::architecture::computeForbiddenDependencies(graph, "/project", codeqa::config::ArchitectureConfig{})
              .empty());
}
```

In `tests/CMakeLists.txt`, immediately after `unit/ComputeFanInOutTest.cpp` (line 37), add:

```cmake
    unit/ComputeForbiddenDependenciesTest.cpp
```

Open `tests/unit/AnalyzeArchitectureTest.cpp`. Append at the true end of the file (after the last `TEST_CASE`, which currently ends at line 138):

```cpp

TEST_CASE("analyzeArchitecture reports a forbidden-dependency violation end-to-end")
{
    codeqa::model::ProjectModel project;
    project.rootPath = "project";
    project.files.push_back(makeFile((project.rootPath / "src" / "core" / "a.h").generic_string()));
    project.files.push_back(makeFile((project.rootPath / "src" / "desktop" / "b.h").generic_string()));
    project.files[0].dependencies.push_back(makeDependency("b.h"));

    codeqa::config::ArchitectureConfig config = codeqa::config::defaultConfig().architecture;
    config.layers = {{"core", {"src/core/**"}}, {"desktop", {"src/desktop/**"}}};
    config.forbiddenDependencies = {{"core", "desktop"}};

    const codeqa::architecture::ProjectArchitectureModel result =
        codeqa::architecture::analyzeArchitecture(project, config);

    REQUIRE(result.forbiddenDependencyViolations.size() == 1);
    CHECK(result.forbiddenDependencyViolations[0].fromLayer == "core");
    CHECK(result.forbiddenDependencyViolations[0].toLayer == "desktop");
}

TEST_CASE("analyzeArchitecture reports no violation when the project has no layer configuration")
{
    codeqa::model::ProjectModel project;
    project.rootPath = "project";
    project.files.push_back(makeFile((project.rootPath / "src" / "core" / "a.h").generic_string()));
    project.files.push_back(makeFile((project.rootPath / "src" / "desktop" / "b.h").generic_string()));
    project.files[0].dependencies.push_back(makeDependency("b.h"));

    const codeqa::config::ArchitectureConfig config = codeqa::config::defaultConfig().architecture;

    const codeqa::architecture::ProjectArchitectureModel result =
        codeqa::architecture::analyzeArchitecture(project, config);

    CHECK(result.forbiddenDependencyViolations.empty());
}
```

Then, in the same file, the existing `"analyzeArchitecture returns an empty model when disabled"` test currently ends:

```cpp
    CHECK(result.files.empty());
    CHECK(result.cycles.empty());
    CHECK(result.fanOutFindings.empty());
}
```

Change to:

```cpp
    CHECK(result.files.empty());
    CHECK(result.cycles.empty());
    CHECK(result.fanOutFindings.empty());
    CHECK(result.forbiddenDependencyViolations.empty());
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Adding a new test *file* changes `tests/CMakeLists.txt`'s source list, so re-run configure, not just build:
```
cmake -S . -B build
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="*orbiddenDepend*"
```
Expected: FAIL to compile — `ForbiddenDependencyViolation`, `computeForbiddenDependencies`, and `ProjectArchitectureModel::forbiddenDependencyViolations` do not exist yet.

- [ ] **Step 3: Add `ForbiddenDependencyViolation` and the `ProjectArchitectureModel` field**

In `include/codeqa/architecture/ArchitectureModels.h`, the includes currently read (lines 1-7):

```cpp
#pragma once

#include "codeqa/core/DiagnosticSeverity.h"

#include <cstddef>
#include <filesystem>
#include <vector>
```

Add `<string>`:

```cpp
#pragma once

#include "codeqa/core/DiagnosticSeverity.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>
```

The `FanOutFinding`/`ProjectArchitectureModel` block currently reads (lines 24-38):

```cpp
struct FanOutFinding
{
    std::filesystem::path path;
    std::size_t fanOut{0};
    codeqa::core::DiagnosticSeverity severity{codeqa::core::DiagnosticSeverity::Warning};
};

struct ProjectArchitectureModel
{
    std::vector<FileDependencyMetrics> files;
    std::vector<DependencyCycle> cycles;
    std::vector<FanOutFinding> fanOutFindings;
    std::size_t externalOrUnresolvedIncludes{0};
    std::size_t ambiguousIncludes{0};
};
```

Change to:

```cpp
struct FanOutFinding
{
    std::filesystem::path path;
    std::size_t fanOut{0};
    codeqa::core::DiagnosticSeverity severity{codeqa::core::DiagnosticSeverity::Warning};
};

struct ForbiddenDependencyViolation
{
    std::filesystem::path fromPath;
    std::string fromLayer;
    std::filesystem::path toPath;
    std::string toLayer;
};

struct ProjectArchitectureModel
{
    std::vector<FileDependencyMetrics> files;
    std::vector<DependencyCycle> cycles;
    std::vector<FanOutFinding> fanOutFindings;
    std::vector<ForbiddenDependencyViolation> forbiddenDependencyViolations;
    std::size_t externalOrUnresolvedIncludes{0};
    std::size_t ambiguousIncludes{0};
};
```

- [ ] **Step 4: Declare `computeForbiddenDependencies()` in `IncludeGraph.h`**

`include/codeqa/architecture/IncludeGraph.h` currently reads in full:

```cpp
#pragma once

#include "codeqa/architecture/ArchitectureModels.h"
#include "codeqa/model/Models.h"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace codeqa::architecture
{

struct IncludeGraph
{
    std::vector<std::filesystem::path> filePaths;
    std::vector<std::vector<std::size_t>> outgoingEdges;
    std::size_t externalOrUnresolvedIncludes{0};
    std::size_t ambiguousIncludes{0};
};

[[nodiscard]] IncludeGraph resolveIncludeGraph(const codeqa::model::ProjectModel& project);

[[nodiscard]] std::vector<DependencyCycle> findCycles(const IncludeGraph& graph);
[[nodiscard]] std::vector<FileDependencyMetrics> computeFanInOut(const IncludeGraph& graph);

} // namespace codeqa::architecture
```

Replace it with:

```cpp
#pragma once

#include "codeqa/architecture/ArchitectureModels.h"
#include "codeqa/config/ConfigModels.h"
#include "codeqa/model/Models.h"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace codeqa::architecture
{

struct IncludeGraph
{
    std::vector<std::filesystem::path> filePaths;
    std::vector<std::vector<std::size_t>> outgoingEdges;
    std::size_t externalOrUnresolvedIncludes{0};
    std::size_t ambiguousIncludes{0};
};

[[nodiscard]] IncludeGraph resolveIncludeGraph(const codeqa::model::ProjectModel& project);

[[nodiscard]] std::vector<DependencyCycle> findCycles(const IncludeGraph& graph);
[[nodiscard]] std::vector<FileDependencyMetrics> computeFanInOut(const IncludeGraph& graph);
[[nodiscard]] std::vector<ForbiddenDependencyViolation> computeForbiddenDependencies(
    const IncludeGraph& graph,
    const std::filesystem::path& rootPath,
    const codeqa::config::ArchitectureConfig& config);

} // namespace codeqa::architecture
```

- [ ] **Step 5: Implement `computeForbiddenDependencies()`**

Create `src/architecture/ComputeForbiddenDependencies.cpp`:

```cpp
#include "codeqa/architecture/IncludeGraph.h"
#include "codeqa/infrastructure/GlobMatcher.h"

#include <optional>

namespace codeqa::architecture
{

namespace
{

// Layers is a std::map, so this iterates layer names in alphabetical order -- the
// alphabetically-first layer whose pattern matches wins when a file matches more than one.
std::optional<std::string> layerOf(
    const std::filesystem::path& relativePath, const std::map<std::string, std::vector<std::string>>& layers)
{
    for (const auto& [layerName, patterns] : layers)
    {
        for (const std::string& pattern : patterns)
        {
            if (codeqa::infrastructure::matchesExcludePattern(pattern, relativePath))
            {
                return layerName;
            }
        }
    }

    return std::nullopt;
}

bool isForbiddenPair(
    const std::vector<codeqa::config::LayerDependency>& forbiddenDependencies, const std::string& from,
    const std::string& to)
{
    for (const codeqa::config::LayerDependency& pair : forbiddenDependencies)
    {
        if (pair.from == from && pair.to == to)
        {
            return true;
        }
    }

    return false;
}

} // namespace

std::vector<ForbiddenDependencyViolation> computeForbiddenDependencies(
    const IncludeGraph& graph, const std::filesystem::path& rootPath, const codeqa::config::ArchitectureConfig& config)
{
    std::vector<ForbiddenDependencyViolation> violations;

    if (config.layers.empty() || config.forbiddenDependencies.empty())
    {
        return violations;
    }

    std::vector<std::optional<std::string>> layerPerFile;
    layerPerFile.reserve(graph.filePaths.size());
    for (const std::filesystem::path& path : graph.filePaths)
    {
        layerPerFile.push_back(layerOf(path.lexically_relative(rootPath), config.layers));
    }

    for (std::size_t fromIndex = 0; fromIndex < graph.filePaths.size(); ++fromIndex)
    {
        if (!layerPerFile[fromIndex].has_value())
        {
            continue;
        }

        for (const std::size_t toIndex : graph.outgoingEdges[fromIndex])
        {
            if (!layerPerFile[toIndex].has_value())
            {
                continue;
            }

            const std::string& fromLayer = *layerPerFile[fromIndex];
            const std::string& toLayer = *layerPerFile[toIndex];

            if (isForbiddenPair(config.forbiddenDependencies, fromLayer, toLayer))
            {
                violations.push_back(ForbiddenDependencyViolation{
                    graph.filePaths[fromIndex], fromLayer, graph.filePaths[toIndex], toLayer});
            }
        }
    }

    return violations;
}

} // namespace codeqa::architecture
```

- [ ] **Step 6: Wire the call in `AnalyzeArchitecture.cpp`**

`src/architecture/AnalyzeArchitecture.cpp` currently reads in full:

```cpp
#include "codeqa/architecture/AnalyzeArchitecture.h"

#include "codeqa/architecture/IncludeGraph.h"

namespace codeqa::architecture
{

ProjectArchitectureModel analyzeArchitecture(
    const codeqa::model::ProjectModel& project, const codeqa::config::ArchitectureConfig& config)
{
    ProjectArchitectureModel result;

    if (!config.enabled)
    {
        return result;
    }

    const IncludeGraph graph = resolveIncludeGraph(project);
    result.files = computeFanInOut(graph);
    result.cycles = findCycles(graph);
    result.externalOrUnresolvedIncludes = graph.externalOrUnresolvedIncludes;
    result.ambiguousIncludes = graph.ambiguousIncludes;

    for (const FileDependencyMetrics& file : result.files)
    {
        if (static_cast<long long>(file.fanOut) < config.fanOutWarning)
        {
            continue;
        }

        FanOutFinding finding;
        finding.path = file.path;
        finding.fanOut = file.fanOut;
        finding.severity = static_cast<long long>(file.fanOut) >= config.fanOutHigh
            ? codeqa::core::DiagnosticSeverity::Error
            : codeqa::core::DiagnosticSeverity::Warning;
        result.fanOutFindings.push_back(finding);
    }

    return result;
}

} // namespace codeqa::architecture
```

Change the line right after `result.cycles = findCycles(graph);`:

```cpp
    const IncludeGraph graph = resolveIncludeGraph(project);
    result.files = computeFanInOut(graph);
    result.cycles = findCycles(graph);
    result.forbiddenDependencyViolations = computeForbiddenDependencies(graph, project.rootPath, config);
    result.externalOrUnresolvedIncludes = graph.externalOrUnresolvedIncludes;
    result.ambiguousIncludes = graph.ambiguousIncludes;
```

- [ ] **Step 7: Register the new source file in CMake**

In `CMakeLists.txt`, the architecture source list currently reads (lines 119-122):

```cmake
    src/architecture/ResolveIncludeGraph.cpp
    src/architecture/FindCycles.cpp
    src/architecture/ComputeFanInOut.cpp
    src/architecture/AnalyzeArchitecture.cpp
```

Change to:

```cmake
    src/architecture/ResolveIncludeGraph.cpp
    src/architecture/FindCycles.cpp
    src/architecture/ComputeFanInOut.cpp
    src/architecture/ComputeForbiddenDependencies.cpp
    src/architecture/AnalyzeArchitecture.cpp
```

- [ ] **Step 8: Run the tests to verify they pass**

Adding a new source file also requires reconfiguring:
```
cmake -S . -B build
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="*orbiddenDepend*"
```
Expected: PASS (8 test cases, 0 failures: 6 in `ComputeForbiddenDependenciesTest.cpp`, 2 in `AnalyzeArchitectureTest.cpp`).

- [ ] **Step 9: Run the full test suite**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe
```
Expected: PASS, all test cases, 0 failures — confirms the existing "disabled" test's new assertion line passes and `findCycles`/`computeFanInOut` are unaffected.

- [ ] **Step 10: Commit**

```bash
git add include/codeqa/architecture/ArchitectureModels.h include/codeqa/architecture/IncludeGraph.h src/architecture/ComputeForbiddenDependencies.cpp src/architecture/AnalyzeArchitecture.cpp CMakeLists.txt tests/unit/ComputeForbiddenDependenciesTest.cpp tests/CMakeLists.txt tests/unit/AnalyzeArchitectureTest.cpp
git commit -m "feat: detect forbidden layer-to-layer dependencies over the include graph"
```

---

### Task 3: Report bridging (JSON and text)

**Files:**
- Modify: `src/report/ReportJson.cpp:76-115` (new `forbiddenDependencyJson()` + `architectureJson()` extended)
- Modify: `src/report/TextReport.cpp:163-189` (`writeArchitecture()` extended)
- Modify: `tests/unit/ReportJsonTest.cpp:163-210` (extend both architecture test cases)
- Modify: `tests/unit/TextReportTest.cpp` (1 new test case)

**Interfaces:**
- Consumes: `codeqa::architecture::ForbiddenDependencyViolation`, `ProjectArchitectureModel::forbiddenDependencyViolations` (Task 2).
- Produces: nothing consumed by a later task in this plan — Task 4 only verifies config round-tripping, unrelated to report rendering.

- [ ] **Step 1: Write the failing tests**

In `tests/unit/ReportJsonTest.cpp`, the test `"toReportJson includes an architecture section with relativized paths"` currently reads (lines 163-193):

```cpp
TEST_CASE("toReportJson includes an architecture section with relativized paths")
{
    codeqa::model::ProjectModel model;
    model.rootPath = std::filesystem::path{"project"};
    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::rules::ProjectFindingsModel findings;
    const codeqa::duplication::ProjectDuplicationModel duplicates;

    codeqa::architecture::ProjectArchitectureModel architecture;
    architecture.files.push_back(codeqa::architecture::FileDependencyMetrics{
        model.rootPath / "src" / "a.h", 2, 1});
    architecture.cycles.push_back(codeqa::architecture::DependencyCycle{
        {model.rootPath / "src" / "a.h", model.rootPath / "src" / "b.h"}});
    architecture.fanOutFindings.push_back(codeqa::architecture::FanOutFinding{
        model.rootPath / "src" / "a.h", 12, codeqa::core::DiagnosticSeverity::Warning});
    architecture.externalOrUnresolvedIncludes = 3;
    architecture.ambiguousIncludes = 1;

    const nlohmann::json document = codeqa::report::toReportJson(
        model, metrics, findings, duplicates, architecture, codeqa::config::defaultConfig(), "9.9.9");

    REQUIRE(document.at("architecture").at("files").size() == 1);
    CHECK(document.at("architecture").at("files")[0].at("path").get<std::string>() == "src/a.h");
    CHECK(document.at("architecture").at("files")[0].at("fanIn").get<int>() == 2);
    REQUIRE(document.at("architecture").at("cycles").size() == 1);
    CHECK(document.at("architecture").at("cycles")[0].at("files")[0].get<std::string>() == "src/a.h");
    REQUIRE(document.at("architecture").at("fanOutFindings").size() == 1);
    CHECK(document.at("architecture").at("fanOutFindings")[0].at("severity").get<std::string>() == "warning");
    CHECK(document.at("architecture").at("externalOrUnresolvedIncludes").get<int>() == 3);
    CHECK(document.at("architecture").at("ambiguousIncludes").get<int>() == 1);
}
```

Change to:

```cpp
TEST_CASE("toReportJson includes an architecture section with relativized paths")
{
    codeqa::model::ProjectModel model;
    model.rootPath = std::filesystem::path{"project"};
    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::rules::ProjectFindingsModel findings;
    const codeqa::duplication::ProjectDuplicationModel duplicates;

    codeqa::architecture::ProjectArchitectureModel architecture;
    architecture.files.push_back(codeqa::architecture::FileDependencyMetrics{
        model.rootPath / "src" / "a.h", 2, 1});
    architecture.cycles.push_back(codeqa::architecture::DependencyCycle{
        {model.rootPath / "src" / "a.h", model.rootPath / "src" / "b.h"}});
    architecture.fanOutFindings.push_back(codeqa::architecture::FanOutFinding{
        model.rootPath / "src" / "a.h", 12, codeqa::core::DiagnosticSeverity::Warning});
    architecture.forbiddenDependencyViolations.push_back(codeqa::architecture::ForbiddenDependencyViolation{
        model.rootPath / "src" / "core" / "a.h", "core", model.rootPath / "src" / "desktop" / "b.h", "desktop"});
    architecture.externalOrUnresolvedIncludes = 3;
    architecture.ambiguousIncludes = 1;

    const nlohmann::json document = codeqa::report::toReportJson(
        model, metrics, findings, duplicates, architecture, codeqa::config::defaultConfig(), "9.9.9");

    REQUIRE(document.at("architecture").at("files").size() == 1);
    CHECK(document.at("architecture").at("files")[0].at("path").get<std::string>() == "src/a.h");
    CHECK(document.at("architecture").at("files")[0].at("fanIn").get<int>() == 2);
    REQUIRE(document.at("architecture").at("cycles").size() == 1);
    CHECK(document.at("architecture").at("cycles")[0].at("files")[0].get<std::string>() == "src/a.h");
    REQUIRE(document.at("architecture").at("fanOutFindings").size() == 1);
    CHECK(document.at("architecture").at("fanOutFindings")[0].at("severity").get<std::string>() == "warning");
    REQUIRE(document.at("architecture").at("forbiddenDependencyViolations").size() == 1);
    CHECK(
        document.at("architecture").at("forbiddenDependencyViolations")[0].at("from").at("path").get<std::string>()
        == "src/core/a.h");
    CHECK(
        document.at("architecture").at("forbiddenDependencyViolations")[0].at("from").at("layer").get<std::string>()
        == "core");
    CHECK(
        document.at("architecture").at("forbiddenDependencyViolations")[0].at("to").at("path").get<std::string>()
        == "src/desktop/b.h");
    CHECK(
        document.at("architecture").at("forbiddenDependencyViolations")[0].at("to").at("layer").get<std::string>()
        == "desktop");
    CHECK(document.at("architecture").at("externalOrUnresolvedIncludes").get<int>() == 3);
    CHECK(document.at("architecture").at("ambiguousIncludes").get<int>() == 1);
}
```

Immediately after it, `"toReportJson emits empty architecture arrays when there is nothing to report"` currently ends (lines 207-210):

```cpp
    CHECK(document.at("architecture").at("files").empty());
    CHECK(document.at("architecture").at("cycles").empty());
    CHECK(document.at("architecture").at("fanOutFindings").empty());
}
```

Change to:

```cpp
    CHECK(document.at("architecture").at("files").empty());
    CHECK(document.at("architecture").at("cycles").empty());
    CHECK(document.at("architecture").at("fanOutFindings").empty());
    CHECK(document.at("architecture").at("forbiddenDependencyViolations").empty());
}
```

Open `tests/unit/TextReportTest.cpp`. Immediately after the `"renderText lists a cycle and a fan-out finding"` test case, append:

```cpp

TEST_CASE("renderText lists a forbidden dependency violation")
{
    codeqa::model::ProjectModel model;
    model.rootPath = "project";
    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::rules::ProjectFindingsModel findings;
    const codeqa::duplication::ProjectDuplicationModel duplicates;

    codeqa::architecture::ProjectArchitectureModel architecture;
    architecture.forbiddenDependencyViolations.push_back(codeqa::architecture::ForbiddenDependencyViolation{
        model.rootPath / "src" / "core" / "a.h", "core", model.rootPath / "src" / "desktop" / "b.h", "desktop"});

    std::ostringstream output;
    codeqa::report::renderText(output, model, metrics, findings, duplicates, architecture);
    const std::string text = output.str();

    CHECK(text.find("Architecture") != std::string::npos);
    CHECK(text.find("core") != std::string::npos);
    CHECK(text.find("desktop") != std::string::npos);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="*architecture*,*Architecture*"
```
Expected: FAIL — `document.at("architecture").at("forbiddenDependencyViolations")` throws (key does not exist yet); the new `TextReportTest.cpp` case fails to find "core"/"desktop" in the output.

- [ ] **Step 3: Add `forbiddenDependencyJson()` and extend `architectureJson()`**

In `src/report/ReportJson.cpp`, immediately after `duplicateGroupJson()` (ends at line 75 with `}`) and before `architectureJson()` (line 77), insert:

```cpp
Json forbiddenDependencyJson(
    const codeqa::architecture::ForbiddenDependencyViolation& violation, const std::filesystem::path& rootPath)
{
    return Json{
        {"from",
         Json{
             {"path", std::filesystem::relative(violation.fromPath, rootPath).generic_string()},
             {"layer", violation.fromLayer}}},
        {"to",
         Json{
             {"path", std::filesystem::relative(violation.toPath, rootPath).generic_string()},
             {"layer", violation.toLayer}}}};
}
```

`architectureJson()` currently reads (lines 77-115):

```cpp
Json architectureJson(
    const codeqa::architecture::ProjectArchitectureModel& architecture, const std::filesystem::path& rootPath)
{
    Json files = Json::array();
    for (const codeqa::architecture::FileDependencyMetrics& file : architecture.files)
    {
        files.push_back(Json{
            {"path", std::filesystem::relative(file.path, rootPath).generic_string()},
            {"fanIn", file.fanIn},
            {"fanOut", file.fanOut}});
    }

    Json cycles = Json::array();
    for (const codeqa::architecture::DependencyCycle& cycle : architecture.cycles)
    {
        Json cycleFiles = Json::array();
        for (const std::filesystem::path& path : cycle.files)
        {
            cycleFiles.push_back(std::filesystem::relative(path, rootPath).generic_string());
        }
        cycles.push_back(Json{{"files", cycleFiles}});
    }

    Json fanOutFindings = Json::array();
    for (const codeqa::architecture::FanOutFinding& finding : architecture.fanOutFindings)
    {
        fanOutFindings.push_back(Json{
            {"path", std::filesystem::relative(finding.path, rootPath).generic_string()},
            {"fanOut", finding.fanOut},
            {"severity", codeqa::core::diagnosticSeverityToJsonString(finding.severity)}});
    }

    return Json{
        {"files", files},
        {"cycles", cycles},
        {"fanOutFindings", fanOutFindings},
        {"externalOrUnresolvedIncludes", architecture.externalOrUnresolvedIncludes},
        {"ambiguousIncludes", architecture.ambiguousIncludes}};
}
```

Change to:

```cpp
Json architectureJson(
    const codeqa::architecture::ProjectArchitectureModel& architecture, const std::filesystem::path& rootPath)
{
    Json files = Json::array();
    for (const codeqa::architecture::FileDependencyMetrics& file : architecture.files)
    {
        files.push_back(Json{
            {"path", std::filesystem::relative(file.path, rootPath).generic_string()},
            {"fanIn", file.fanIn},
            {"fanOut", file.fanOut}});
    }

    Json cycles = Json::array();
    for (const codeqa::architecture::DependencyCycle& cycle : architecture.cycles)
    {
        Json cycleFiles = Json::array();
        for (const std::filesystem::path& path : cycle.files)
        {
            cycleFiles.push_back(std::filesystem::relative(path, rootPath).generic_string());
        }
        cycles.push_back(Json{{"files", cycleFiles}});
    }

    Json fanOutFindings = Json::array();
    for (const codeqa::architecture::FanOutFinding& finding : architecture.fanOutFindings)
    {
        fanOutFindings.push_back(Json{
            {"path", std::filesystem::relative(finding.path, rootPath).generic_string()},
            {"fanOut", finding.fanOut},
            {"severity", codeqa::core::diagnosticSeverityToJsonString(finding.severity)}});
    }

    Json forbiddenDependencyViolations = Json::array();
    for (const codeqa::architecture::ForbiddenDependencyViolation& violation :
         architecture.forbiddenDependencyViolations)
    {
        forbiddenDependencyViolations.push_back(forbiddenDependencyJson(violation, rootPath));
    }

    return Json{
        {"files", files},
        {"cycles", cycles},
        {"fanOutFindings", fanOutFindings},
        {"forbiddenDependencyViolations", forbiddenDependencyViolations},
        {"externalOrUnresolvedIncludes", architecture.externalOrUnresolvedIncludes},
        {"ambiguousIncludes", architecture.ambiguousIncludes}};
}
```

- [ ] **Step 4: Extend `writeArchitecture()`**

In `src/report/TextReport.cpp`, this function currently reads (lines 163-189):

```cpp
void writeArchitecture(
    std::ostream& output, const codeqa::architecture::ProjectArchitectureModel& architecture,
    const std::filesystem::path& rootPath)
{
    if (architecture.cycles.empty() && architecture.fanOutFindings.empty())
    {
        return;
    }

    output << "\nArchitecture\n------------\n";

    for (const codeqa::architecture::DependencyCycle& cycle : architecture.cycles)
    {
        output << "Cycle:";
        for (const std::filesystem::path& path : cycle.files)
        {
            output << ' ' << relativeDisplayPath(path, rootPath);
        }
        output << '\n';
    }

    for (const codeqa::architecture::FanOutFinding& finding : architecture.fanOutFindings)
    {
        output << "  [" << severityTag(finding.severity) << "] " << relativeDisplayPath(finding.path, rootPath)
               << "  fan-out=" << finding.fanOut << '\n';
    }
}
```

Change to:

```cpp
void writeArchitecture(
    std::ostream& output, const codeqa::architecture::ProjectArchitectureModel& architecture,
    const std::filesystem::path& rootPath)
{
    if (architecture.cycles.empty() && architecture.fanOutFindings.empty()
        && architecture.forbiddenDependencyViolations.empty())
    {
        return;
    }

    output << "\nArchitecture\n------------\n";

    for (const codeqa::architecture::DependencyCycle& cycle : architecture.cycles)
    {
        output << "Cycle:";
        for (const std::filesystem::path& path : cycle.files)
        {
            output << ' ' << relativeDisplayPath(path, rootPath);
        }
        output << '\n';
    }

    for (const codeqa::architecture::FanOutFinding& finding : architecture.fanOutFindings)
    {
        output << "  [" << severityTag(finding.severity) << "] " << relativeDisplayPath(finding.path, rootPath)
               << "  fan-out=" << finding.fanOut << '\n';
    }

    for (const codeqa::architecture::ForbiddenDependencyViolation& violation :
         architecture.forbiddenDependencyViolations)
    {
        output << "  [" << severityTag(codeqa::core::DiagnosticSeverity::Error) << "] "
               << relativeDisplayPath(violation.fromPath, rootPath) << " (" << violation.fromLayer << ") -> "
               << relativeDisplayPath(violation.toPath, rootPath) << " (" << violation.toLayer << ")\n";
    }
}
```

- [ ] **Step 5: Run the tests to verify they pass**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="*architecture*,*Architecture*"
```
Expected: PASS, all matching test cases, 0 failures.

- [ ] **Step 6: Run the full test suite**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe
```
Expected: PASS, all test cases, 0 failures.

- [ ] **Step 7: Commit**

```bash
git add src/report/ReportJson.cpp src/report/TextReport.cpp tests/unit/ReportJsonTest.cpp tests/unit/TextReportTest.cpp
git commit -m "feat: render forbidden-dependency violations in the JSON and text reports"
```

---

### Task 4: Desktop round-trip verification and final regression

**Files:**
- Modify: `tests/desktop/AppControllerTest.cpp` (1 new declaration + 1 new test)

**Interfaces:**
- Consumes: `codeqa::config::ArchitectureConfig::layers`/`forbiddenDependencies` (Task 1); `AppController`'s existing whole-struct `architecture_` round-trip (`currentFormConfiguration()`/`applyConfiguration()`), unmodified by this plan.
- Produces: nothing consumed by a later task — this is the final task in the plan.

- [ ] **Step 1: Write the failing desktop test**

In `tests/desktop/AppControllerTest.cpp`, the `private slots:` list currently includes (line 161):

```cpp
    void saveConfigurationWritesOnlyExplicitValidatedForm();
```

Add a new declaration immediately after it:

```cpp
    void saveConfigurationWritesOnlyExplicitValidatedForm();
    void savingConfigurationPreservesArchitectureLayersItDoesNotEdit();
```

The implementation of `saveConfigurationWritesOnlyExplicitValidatedForm()` currently ends (line 325) right before `invalidFormDoesNotOverwriteExistingConfiguration()` (line 327) begins. Insert the new test's implementation between them:

```cpp
void AppControllerTest::savingConfigurationPreservesArchitectureLayersItDoesNotEdit()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.architecture.layers["core"] = {"src/core/**"};
    config.architecture.layers["desktop"] = {"src/desktop/**"};
    config.architecture.forbiddenDependencies.push_back({"core", "desktop"});
    writeTextFile(
        directory.filePath("codeqa.json"),
        QByteArray::fromStdString(codeqa::config::toJson(config).dump(2)));
    AppController controller(settingsPath(directory));

    controller.selectProject(QUrl::fromLocalFile(directory.path()));
    QVERIFY(controller.configurationAvailable());
    controller.saveConfiguration();

    const codeqa::application::ProjectConfigLoadResult loaded =
        codeqa::application::ApplicationFacade{}.loadProjectConfig(
            std::filesystem::path(directory.path().toStdWString()));
    QVERIFY(loaded.config.has_value());
    QVERIFY(!loaded.error.hasError());
    QCOMPARE(loaded.config->architecture.layers.count("core"), std::size_t{1});
    QCOMPARE(
        QString::fromStdString(loaded.config->architecture.layers.at("core").front()), QString("src/core/**"));
    QCOMPARE(loaded.config->architecture.forbiddenDependencies.size(), std::size_t{1});
    QCOMPARE(
        QString::fromStdString(loaded.config->architecture.forbiddenDependencies.front().from), QString("core"));
    QCOMPARE(
        QString::fromStdString(loaded.config->architecture.forbiddenDependencies.front().to), QString("desktop"));
}
```

- [ ] **Step 2: Run the desktop tests**

Unlike the previous three tasks, this step is not a RED→GREEN cycle: `AppController`'s `architecture_` round trip (`currentFormConfiguration()`/`applyConfiguration()`, both already whole-struct copies of `ArchitectureConfig` — see spec §2 and the Global Constraints) is pre-existing, generic code that never needed to change for this feature. This test is a **regression guard confirming that reasoning is actually true**, not a new behavior being implemented — expect it to pass the first time it runs. Still run it before committing, per this plan's "verify, don't assume" discipline (the same discipline that caught a real bug in exactly this area during this track's *first* slice).

Run:
```
cmake --build buildQt --target codeqa_desktop_tests
ctest --test-dir buildQt -C Debug -R codeqa_desktop_tests --output-on-failure -V
```
(If `cmake`/`ctest` are not on `PATH`, use the full path to the Visual Studio-bundled CMake install, e.g. `"C:\Program Files\Microsoft Visual Studio\<version>\<edition>\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"`/`ctest.exe`. Do **not** invoke `codeqa_desktop_tests.exe` directly from the repo root — in some environments it silently produces zero output; always run it through `ctest`, which sets the correct working directory.)
Expected: PASS, 1/1 test binary reporting success (all Qt Test cases inside it passing, including the new one). If it fails instead, that is a real, newly-discovered round-trip bug — stop and investigate rather than adjusting the test to match.

- [ ] **Step 3: Verify no corpus fixture is affected**

Run:
```
grep -rn "forbiddenDependencies" tests/corpus
```
Expected: no matches outside this plan's own new test files (none of which live under `tests/corpus/`) — confirms `tests/unit/CorpusReportTest.cpp` needs no changes, checked directly rather than assumed.

- [ ] **Step 4: Run the complete CLI test suite one final time**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe
```
Expected: PASS, all test cases, 0 failures. This is the final regression check across all 4 tasks (config, detection algorithm, report bridging, desktop round trip) — confirms nothing regressed any of the 12 existing rules, `findCycles`/`computeFanInOut`'s existing behavior, or any other test file.

- [ ] **Step 5: Commit**

```bash
git add tests/desktop/AppControllerTest.cpp
git commit -m "test: verify architecture layers and forbiddenDependencies survive a desktop config round trip"
```

---

## Self-Review Notes

- **Spec coverage:** §4 (config model + validation + serialization) → Task 1. §5 (detection algorithm, including the documented `lexically_relative` refinement and the `ArchitectureConfig`-dependency design note) → Task 2. §6 (report bridging) → Task 3. The two "verified, not assumed" `MergeConfig`/desktop round-trip checks → Task 1 Step 1 (`MergeConfigTest.cpp`) and Task 4. §7's full test list is covered: `ConfigJsonTest.cpp` (Task 1), `ComputeForbiddenDependenciesTest.cpp` + `AnalyzeArchitectureTest.cpp` (Task 2, including the alphabetical-tiebreak and directional-asymmetry cases — the latter replacing the spec's self-review-flagged, slightly-confused "self-include" bullet with a cleaner, equally-meaningful check), `ReportJsonTest.cpp` + `TextReportTest.cpp` (Task 3), `MergeConfigTest.cpp` + `AppControllerTest.cpp` + the `CorpusReportTest.cpp` grep check (Task 4). §8 acceptance criteria are covered by the combination of all four tasks' tests. No gaps found.
- **Placeholder scan:** none found — every step carries the literal code or exact file content to write.
- **Type/name consistency:** `codeqa::config::LayerDependency{from, to}` (Task 1) is used with these exact field names in Task 2's `ComputeForbiddenDependencies.cpp`, Task 2's test file, and Task 4's desktop test. `codeqa::architecture::ForbiddenDependencyViolation{fromPath, fromLayer, toPath, toLayer}` (Task 2) is used with this exact field order/names in Task 3's `ReportJson.cpp`/`TextReport.cpp` and both of Task 3's test files. `computeForbiddenDependencies(graph, rootPath, config)`'s signature is declared once (Task 2 Step 4) and called with matching argument order everywhere it's used (`AnalyzeArchitecture.cpp`, both test files). All CONFIG014/CONFIG015 references are consistent between Task 1's implementation and its own tests. Confirmed against the actual current file contents (not assumed) by reading every touched file in full, including exact current line numbers, before writing this plan — including tracing the real production data flow (`DiscoverSourceFiles.cpp`'s `lexically_relative` + `AnalyzeProject.cpp`'s `rootPath / file.path` join) that justified the `lexically_relative`-over-`std::filesystem::relative` refinement documented in the Global Constraints.
