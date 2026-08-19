# Dependency Graph (Cycles + Fan-out) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a project-wide `#include` dependency graph and two analyses on top of it — circular-dependency detection and fan-out (efferent coupling) findings — as a new `codeqa::architecture` layer, sibling to `codeqa::duplication`.

**Architecture:** Resolve raw `#include` text against discovered project files into an `IncludeGraph` (adjacency list of file indices); run Tarjan's SCC algorithm for cycles and a degree count for fan-in/fan-out on top of it; wire the result into `RunAnalysis`, the JSON report, the text report, and the `ReportDocument` persistence round-trip, mirroring `codeqa::duplication`'s existing integration into each of those four places exactly.

**Tech Stack:** C++17, doctest (`codeqa_tests`), nlohmann::json.

**Spec:** `docs/superpowers/specs/2026-08-19-arquitectura-grafo-includes-design.md`

## Global Constraints

- New rule id / config namespace: `"architecture"` (config JSON top-level key; not a `codeqa::rules` rule id — this layer never touches `codeqa::rules`, `Rule`, or `RuleContext`).
- `ArchitectureConfig{enabled=true, fanOutWarning=10, fanOutHigh=20}` — unvalidated starting guess, same convention as the original 8 threshold rules.
- New config error code: `CONFIG013` for `fanOutWarning > fanOutHigh` (confirmed free — `CONFIG007`/`CONFIG010`/`CONFIG011` are used elsewhere in the codebase, outside `ConfigJson.cpp`; `CONFIG013`/`CONFIG014` are not used anywhere).
- `System` (`<...>`) includes never become graph edges or affect fan-in/fan-out. Self-includes never become edges. A `Local` (`"..."`) include with zero matching project files increments `externalOrUnresolvedIncludes` and adds no edge (expected — vendored/external headers included with quotes, not an error). A `Local` include matching more than one project file increments `ambiguousIncludes`, resolves to the first match in `project.files` order, and still adds exactly one edge.
- Cycles are reported as strongly connected components of size ≥ 2 (Tarjan's algorithm) — never exhaustive simple-cycle enumeration.
- Fan-in never produces a finding, at any value. Only fan-out is compared against `fanOutWarning`/`fanOutHigh` (severity `Warning` at/above `fanOutWarning`, `Error` at/above `fanOutHigh`, matching `evaluateThreshold`'s existing `>=` convention in `Rules.cpp`).
- `renderMarkdown`/`MarkdownReport.cpp` is **not** touched in this plan — duplication was never added to it either, confirmed by reading the file directly.
- No suppression mechanism, no quality-gate integration, no desktop GUI surface — all explicitly out of scope per the spec.
- The `ReportDocument` round-trip (`makeReportDocument`/`parseReportDocument`/`toAnalysisResult`) must carry `architecture` data through both directions — `codeqa analyze --format text` reconstructs its `RunAnalysisResult` from `result.document`, not the freshly-computed `analysis` object, so skipping this would silently make text output always show an empty architecture section.

---

### Task 1: Model types + include resolution

**Files:**
- Create: `include/codeqa/architecture/ArchitectureModels.h`
- Create: `include/codeqa/architecture/IncludeGraph.h`
- Create: `src/architecture/ResolveIncludeGraph.cpp`
- Modify: `CMakeLists.txt` (add `src/architecture/ResolveIncludeGraph.cpp` to `codeqa_core`'s source list)
- Test: `tests/unit/ResolveIncludeGraphTest.cpp`
- Modify: `tests/CMakeLists.txt` (add the new test file)

**Interfaces:**
- Consumes: `codeqa::model::ProjectModel`, `codeqa::model::FileModel{path, dependencies}`, `codeqa::model::DependencyModel{target, kind, range}`, `codeqa::model::IncludeKind::{Local, System}` — all already exist in `include/codeqa/model/Models.h`.
- Produces: `codeqa::architecture::FileDependencyMetrics{path, fanIn, fanOut}`, `codeqa::architecture::DependencyCycle{files}`, `codeqa::architecture::FanOutFinding{path, fanOut, severity}`, `codeqa::architecture::ProjectArchitectureModel{files, cycles, fanOutFindings, externalOrUnresolvedIncludes, ambiguousIncludes}` (Task 2, 4, 5, 6 all consume these exact names), `codeqa::architecture::IncludeGraph{filePaths, outgoingEdges, externalOrUnresolvedIncludes, ambiguousIncludes}` and `resolveIncludeGraph(const codeqa::model::ProjectModel&) -> IncludeGraph` (Task 2 and 4 consume this exact signature).

- [ ] **Step 1: Write the failing tests**

Create `tests/unit/ResolveIncludeGraphTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/architecture/IncludeGraph.h"

namespace
{

codeqa::model::FileModel makeFile(std::string path)
{
    codeqa::model::FileModel file;
    file.path = std::move(path);
    return file;
}

codeqa::model::DependencyModel makeDependency(std::string target, codeqa::model::IncludeKind kind)
{
    codeqa::model::DependencyModel dependency;
    dependency.target = std::move(target);
    dependency.kind = kind;
    return dependency;
}

} // namespace

TEST_CASE("resolveIncludeGraph resolves a local include to the matching project file")
{
    codeqa::model::ProjectModel project;
    project.files.push_back(makeFile("src/a.h"));
    project.files.push_back(makeFile("src/b.h"));
    project.files[0].dependencies.push_back(makeDependency("b.h", codeqa::model::IncludeKind::Local));

    const codeqa::architecture::IncludeGraph graph = codeqa::architecture::resolveIncludeGraph(project);

    REQUIRE(graph.filePaths.size() == 2);
    REQUIRE(graph.outgoingEdges.size() == 2);
    REQUIRE(graph.outgoingEdges[0].size() == 1);
    CHECK(graph.outgoingEdges[0][0] == 1);
    CHECK(graph.outgoingEdges[1].empty());
    CHECK(graph.externalOrUnresolvedIncludes == 0);
    CHECK(graph.ambiguousIncludes == 0);
}

TEST_CASE("resolveIncludeGraph never adds an edge for a system include")
{
    codeqa::model::ProjectModel project;
    project.files.push_back(makeFile("src/a.cpp"));
    project.files.push_back(makeFile("vector"));
    project.files[0].dependencies.push_back(makeDependency("vector", codeqa::model::IncludeKind::System));

    const codeqa::architecture::IncludeGraph graph = codeqa::architecture::resolveIncludeGraph(project);

    CHECK(graph.outgoingEdges[0].empty());
    CHECK(graph.externalOrUnresolvedIncludes == 0);
}

TEST_CASE("resolveIncludeGraph counts an unresolved local include without adding an edge")
{
    codeqa::model::ProjectModel project;
    project.files.push_back(makeFile("src/a.cpp"));
    project.files[0].dependencies.push_back(
        makeDependency("nlohmann/json.hpp", codeqa::model::IncludeKind::Local));

    const codeqa::architecture::IncludeGraph graph = codeqa::architecture::resolveIncludeGraph(project);

    CHECK(graph.outgoingEdges[0].empty());
    CHECK(graph.externalOrUnresolvedIncludes == 1);
    CHECK(graph.ambiguousIncludes == 0);
}

TEST_CASE("resolveIncludeGraph counts an ambiguous local include and resolves to the first match")
{
    codeqa::model::ProjectModel project;
    project.files.push_back(makeFile("src/a.cpp"));
    project.files.push_back(makeFile("src/one/utils.h"));
    project.files.push_back(makeFile("src/two/utils.h"));
    project.files[0].dependencies.push_back(makeDependency("utils.h", codeqa::model::IncludeKind::Local));

    const codeqa::architecture::IncludeGraph graph = codeqa::architecture::resolveIncludeGraph(project);

    REQUIRE(graph.outgoingEdges[0].size() == 1);
    CHECK(graph.outgoingEdges[0][0] == 1);
    CHECK(graph.ambiguousIncludes == 1);
    CHECK(graph.externalOrUnresolvedIncludes == 0);
}

TEST_CASE("resolveIncludeGraph drops a self-include")
{
    codeqa::model::ProjectModel project;
    project.files.push_back(makeFile("src/a.h"));
    project.files[0].dependencies.push_back(makeDependency("a.h", codeqa::model::IncludeKind::Local));

    const codeqa::architecture::IncludeGraph graph = codeqa::architecture::resolveIncludeGraph(project);

    CHECK(graph.outgoingEdges[0].empty());
    CHECK(graph.externalOrUnresolvedIncludes == 0);
}

TEST_CASE("resolveIncludeGraph deduplicates two includes of the same target into one edge")
{
    codeqa::model::ProjectModel project;
    project.files.push_back(makeFile("src/a.cpp"));
    project.files.push_back(makeFile("src/b.h"));
    project.files[0].dependencies.push_back(makeDependency("b.h", codeqa::model::IncludeKind::Local));
    project.files[0].dependencies.push_back(makeDependency("b.h", codeqa::model::IncludeKind::Local));

    const codeqa::architecture::IncludeGraph graph = codeqa::architecture::resolveIncludeGraph(project);

    REQUIRE(graph.outgoingEdges[0].size() == 1);
    CHECK(graph.outgoingEdges[0][0] == 1);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run:
```
cmake --build build --config Debug --target codeqa_tests
```
Expected: FAIL to compile — `codeqa/architecture/IncludeGraph.h` does not exist yet.

- [ ] **Step 3: Create the model types header**

Create `include/codeqa/architecture/ArchitectureModels.h`:

```cpp
#pragma once

#include "codeqa/core/DiagnosticSeverity.h"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace codeqa::architecture
{

struct FileDependencyMetrics
{
    std::filesystem::path path;
    std::size_t fanIn{0};
    std::size_t fanOut{0};
};

struct DependencyCycle
{
    std::vector<std::filesystem::path> files;
};

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

} // namespace codeqa::architecture
```

- [ ] **Step 4: Create the include-graph header**

Create `include/codeqa/architecture/IncludeGraph.h`:

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

} // namespace codeqa::architecture
```

- [ ] **Step 5: Implement resolution**

Create `src/architecture/ResolveIncludeGraph.cpp`:

```cpp
#include "codeqa/architecture/IncludeGraph.h"

#include <algorithm>

namespace codeqa::architecture
{

namespace
{

bool endsWithPathSegment(const std::string& path, const std::string& suffix)
{
    if (suffix.empty() || suffix.size() > path.size())
    {
        return false;
    }

    if (path.size() == suffix.size())
    {
        return path == suffix;
    }

    const std::size_t offset = path.size() - suffix.size();
    return path.compare(offset, suffix.size(), suffix) == 0 && path[offset - 1] == '/';
}

} // namespace

IncludeGraph resolveIncludeGraph(const codeqa::model::ProjectModel& project)
{
    IncludeGraph graph;
    graph.filePaths.reserve(project.files.size());
    for (const codeqa::model::FileModel& file : project.files)
    {
        graph.filePaths.push_back(file.path);
    }
    graph.outgoingEdges.resize(project.files.size());

    for (std::size_t fromIndex = 0; fromIndex < project.files.size(); ++fromIndex)
    {
        std::vector<std::size_t> resolved;

        for (const codeqa::model::DependencyModel& dependency : project.files[fromIndex].dependencies)
        {
            if (dependency.kind == codeqa::model::IncludeKind::System)
            {
                continue;
            }

            const std::string target = std::filesystem::path(dependency.target).generic_string();
            std::vector<std::size_t> matches;

            for (std::size_t toIndex = 0; toIndex < project.files.size(); ++toIndex)
            {
                if (toIndex == fromIndex)
                {
                    continue;
                }

                if (endsWithPathSegment(project.files[toIndex].path.generic_string(), target))
                {
                    matches.push_back(toIndex);
                }
            }

            if (matches.empty())
            {
                ++graph.externalOrUnresolvedIncludes;
                continue;
            }

            if (matches.size() > 1)
            {
                ++graph.ambiguousIncludes;
            }

            const std::size_t toIndex = matches.front();
            if (std::find(resolved.begin(), resolved.end(), toIndex) == resolved.end())
            {
                resolved.push_back(toIndex);
            }
        }

        graph.outgoingEdges[fromIndex] = std::move(resolved);
    }

    return graph;
}

} // namespace codeqa::architecture
```

- [ ] **Step 6: Register the new source file**

In `CMakeLists.txt`, add a line to `add_library(codeqa_core STATIC ...)`'s source list, right after `src/duplication/DetectDuplicates.cpp`:

```cmake
    src/architecture/ResolveIncludeGraph.cpp
```

In `tests/CMakeLists.txt`, add `unit/ResolveIncludeGraphTest.cpp` to `add_executable(codeqa_tests ...)`'s source list, right after `unit/DetectDuplicatesTest.cpp`.

- [ ] **Step 7: Run tests to verify they pass**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\Debug\codeqa_tests.exe --test-case="resolveIncludeGraph*"
```
Expected: PASS (6 test cases, 0 failures).

- [ ] **Step 8: Commit**

```bash
git add include/codeqa/architecture/ArchitectureModels.h include/codeqa/architecture/IncludeGraph.h src/architecture/ResolveIncludeGraph.cpp CMakeLists.txt tests/CMakeLists.txt tests/unit/ResolveIncludeGraphTest.cpp
git commit -m "feat: add codeqa::architecture model types and include resolution"
```

---

### Task 2: Graph algorithms (cycle detection + fan-in/fan-out)

**Files:**
- Modify: `include/codeqa/architecture/IncludeGraph.h` (add 2 function declarations)
- Create: `src/architecture/FindCycles.cpp`
- Create: `src/architecture/ComputeFanInOut.cpp`
- Modify: `CMakeLists.txt` (add both new source files)
- Test: `tests/unit/FindCyclesTest.cpp`
- Test: `tests/unit/ComputeFanInOutTest.cpp`
- Modify: `tests/CMakeLists.txt` (add both new test files)

**Interfaces:**
- Consumes: `codeqa::architecture::IncludeGraph{filePaths, outgoingEdges}` (Task 1), `codeqa::architecture::DependencyCycle{files}`, `codeqa::architecture::FileDependencyMetrics{path, fanIn, fanOut}` (Task 1).
- Produces: `findCycles(const IncludeGraph&) -> std::vector<DependencyCycle>` and `computeFanInOut(const IncludeGraph&) -> std::vector<FileDependencyMetrics>` (Task 4 consumes both exact signatures).

- [ ] **Step 1: Write the failing tests**

Create `tests/unit/FindCyclesTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/architecture/IncludeGraph.h"

TEST_CASE("findCycles returns nothing for an acyclic graph")
{
    codeqa::architecture::IncludeGraph graph;
    graph.filePaths = {"a.h", "b.h", "c.h", "d.h"};
    graph.outgoingEdges = {{1, 2}, {3}, {3}, {}};

    CHECK(codeqa::architecture::findCycles(graph).empty());
}

TEST_CASE("findCycles reports a 2-file mutual cycle")
{
    codeqa::architecture::IncludeGraph graph;
    graph.filePaths = {"a.h", "b.h"};
    graph.outgoingEdges = {{1}, {0}};

    const std::vector<codeqa::architecture::DependencyCycle> cycles = codeqa::architecture::findCycles(graph);

    REQUIRE(cycles.size() == 1);
    CHECK(cycles[0].files.size() == 2);
}

TEST_CASE("findCycles reports a 3-file cycle")
{
    codeqa::architecture::IncludeGraph graph;
    graph.filePaths = {"a.h", "b.h", "c.h"};
    graph.outgoingEdges = {{1}, {2}, {0}};

    const std::vector<codeqa::architecture::DependencyCycle> cycles = codeqa::architecture::findCycles(graph);

    REQUIRE(cycles.size() == 1);
    CHECK(cycles[0].files.size() == 3);
}

TEST_CASE("findCycles reports two independent cycles as two separate entries")
{
    codeqa::architecture::IncludeGraph graph;
    graph.filePaths = {"a.h", "b.h", "c.h", "d.h"};
    graph.outgoingEdges = {{1}, {0}, {3}, {2}};

    const std::vector<codeqa::architecture::DependencyCycle> cycles = codeqa::architecture::findCycles(graph);

    REQUIRE(cycles.size() == 2);
    CHECK(cycles[0].files.size() == 2);
    CHECK(cycles[1].files.size() == 2);
}
```

Create `tests/unit/ComputeFanInOutTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/architecture/IncludeGraph.h"

TEST_CASE("computeFanInOut counts in-degree and out-degree per file")
{
    codeqa::architecture::IncludeGraph graph;
    graph.filePaths = {"a.h", "b.h", "c.h"};
    graph.outgoingEdges = {{1, 2}, {2}, {}};

    const std::vector<codeqa::architecture::FileDependencyMetrics> metrics =
        codeqa::architecture::computeFanInOut(graph);

    REQUIRE(metrics.size() == 3);
    CHECK(metrics[0].path == "a.h");
    CHECK(metrics[0].fanOut == 2);
    CHECK(metrics[0].fanIn == 0);
    CHECK(metrics[1].fanOut == 1);
    CHECK(metrics[1].fanIn == 1);
    CHECK(metrics[2].fanOut == 0);
    CHECK(metrics[2].fanIn == 2);
}

TEST_CASE("computeFanInOut returns zero for every file in a graph with no edges")
{
    codeqa::architecture::IncludeGraph graph;
    graph.filePaths = {"a.h", "b.h"};
    graph.outgoingEdges = {{}, {}};

    const std::vector<codeqa::architecture::FileDependencyMetrics> metrics =
        codeqa::architecture::computeFanInOut(graph);

    REQUIRE(metrics.size() == 2);
    CHECK(metrics[0].fanIn == 0);
    CHECK(metrics[0].fanOut == 0);
    CHECK(metrics[1].fanIn == 0);
    CHECK(metrics[1].fanOut == 0);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run:
```
cmake --build build --config Debug --target codeqa_tests
```
Expected: FAIL to compile — `findCycles`/`computeFanInOut` are not declared yet.

- [ ] **Step 3: Add the declarations**

In `include/codeqa/architecture/IncludeGraph.h`, add these two lines right after the `resolveIncludeGraph` declaration:

```cpp
[[nodiscard]] std::vector<DependencyCycle> findCycles(const IncludeGraph& graph);
[[nodiscard]] std::vector<FileDependencyMetrics> computeFanInOut(const IncludeGraph& graph);
```

- [ ] **Step 4: Implement cycle detection**

Create `src/architecture/FindCycles.cpp`:

```cpp
#include "codeqa/architecture/IncludeGraph.h"

#include <algorithm>

namespace codeqa::architecture
{

namespace
{

struct TarjanState
{
    std::vector<long long> index;
    std::vector<long long> lowLink;
    std::vector<bool> onStack;
    std::vector<std::size_t> stack;
    long long nextIndex{0};
    std::vector<DependencyCycle> cycles;
};

void strongConnect(const IncludeGraph& graph, std::size_t vertex, TarjanState& state)
{
    state.index[vertex] = state.nextIndex;
    state.lowLink[vertex] = state.nextIndex;
    ++state.nextIndex;
    state.stack.push_back(vertex);
    state.onStack[vertex] = true;

    for (const std::size_t neighbor : graph.outgoingEdges[vertex])
    {
        if (state.index[neighbor] == -1)
        {
            strongConnect(graph, neighbor, state);
            state.lowLink[vertex] = std::min(state.lowLink[vertex], state.lowLink[neighbor]);
        }
        else if (state.onStack[neighbor])
        {
            state.lowLink[vertex] = std::min(state.lowLink[vertex], state.index[neighbor]);
        }
    }

    if (state.lowLink[vertex] == state.index[vertex])
    {
        DependencyCycle component;
        std::size_t member = 0;
        do
        {
            member = state.stack.back();
            state.stack.pop_back();
            state.onStack[member] = false;
            component.files.push_back(graph.filePaths[member]);
        } while (member != vertex);

        if (component.files.size() > 1)
        {
            state.cycles.push_back(std::move(component));
        }
    }
}

} // namespace

std::vector<DependencyCycle> findCycles(const IncludeGraph& graph)
{
    TarjanState state;
    const std::size_t fileCount = graph.filePaths.size();
    state.index.assign(fileCount, -1);
    state.lowLink.assign(fileCount, -1);
    state.onStack.assign(fileCount, false);

    for (std::size_t vertex = 0; vertex < fileCount; ++vertex)
    {
        if (state.index[vertex] == -1)
        {
            strongConnect(graph, vertex, state);
        }
    }

    return std::move(state.cycles);
}

} // namespace codeqa::architecture
```

(Self-includes never appear in `outgoingEdges` — Task 1's resolution drops them — so a size-1 strongly connected component can only ever be an isolated, acyclic node; the `component.files.size() > 1` check is therefore a correct and sufficient "real cycle" filter, never mistakenly excluding a genuine self-loop.)

- [ ] **Step 5: Implement fan-in/fan-out**

Create `src/architecture/ComputeFanInOut.cpp`:

```cpp
#include "codeqa/architecture/IncludeGraph.h"

namespace codeqa::architecture
{

std::vector<FileDependencyMetrics> computeFanInOut(const IncludeGraph& graph)
{
    std::vector<std::size_t> fanIn(graph.filePaths.size(), 0);
    for (const std::vector<std::size_t>& edges : graph.outgoingEdges)
    {
        for (const std::size_t target : edges)
        {
            ++fanIn[target];
        }
    }

    std::vector<FileDependencyMetrics> result;
    result.reserve(graph.filePaths.size());
    for (std::size_t index = 0; index < graph.filePaths.size(); ++index)
    {
        result.push_back(FileDependencyMetrics{
            graph.filePaths[index], fanIn[index], graph.outgoingEdges[index].size()});
    }

    return result;
}

} // namespace codeqa::architecture
```

- [ ] **Step 6: Register the new source files**

In `CMakeLists.txt`, add these two lines right after `src/architecture/ResolveIncludeGraph.cpp`:

```cmake
    src/architecture/FindCycles.cpp
    src/architecture/ComputeFanInOut.cpp
```

In `tests/CMakeLists.txt`, add `unit/FindCyclesTest.cpp` and `unit/ComputeFanInOutTest.cpp` right after `unit/ResolveIncludeGraphTest.cpp`.

- [ ] **Step 7: Run tests to verify they pass**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\Debug\codeqa_tests.exe --test-case="findCycles*,computeFanInOut*"
```
Expected: PASS (6 test cases, 0 failures).

- [ ] **Step 8: Commit**

```bash
git add include/codeqa/architecture/IncludeGraph.h src/architecture/FindCycles.cpp src/architecture/ComputeFanInOut.cpp CMakeLists.txt tests/CMakeLists.txt tests/unit/FindCyclesTest.cpp tests/unit/ComputeFanInOutTest.cpp
git commit -m "feat: add cycle detection and fan-in/fan-out graph algorithms"
```

---

### Task 3: Config (ArchitectureConfig)

**Files:**
- Modify: `include/codeqa/config/ConfigModels.h`
- Modify: `src/config/ConfigJson.cpp`
- Test: `tests/unit/ConfigJsonTest.cpp`

**Interfaces:**
- Consumes: nothing from Tasks 1-2 — this task is independent config plumbing, mirroring `DuplicationConfig`/`parseDuplication` exactly.
- Produces: `codeqa::config::ArchitectureConfig{enabled, fanOutWarning, fanOutHigh}` as `codeqa::config::ProjectConfig::architecture` (Task 4 and 6 consume this exact field name and type).

- [ ] **Step 1: Write the failing tests**

Append to `tests/unit/ConfigJsonTest.cpp`, after the existing `TEST_CASE("toJson round-trips duplication through parseConfig")` (the file's last test case):

```cpp

TEST_CASE("parseConfig accepts a document with architecture")
{
    const std::string text = R"({ "architecture": { "enabled": false, "fanOutWarning": 5, "fanOutHigh": 8 } })";
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.config.has_value());
    CHECK_FALSE(result.config->architecture.enabled);
    CHECK(result.config->architecture.fanOutWarning == 5);
    CHECK(result.config->architecture.fanOutHigh == 8);
}

TEST_CASE("parseConfig defaults architecture fields when the object is present but empty")
{
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(R"({ "architecture": {} })");

    REQUIRE(result.config.has_value());
    CHECK(result.config->architecture.enabled);
    CHECK(result.config->architecture.fanOutWarning == 10);
    CHECK(result.config->architecture.fanOutHigh == 20);
}

TEST_CASE("parseConfig defaults architecture fields when architecture is absent")
{
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig("{}");

    REQUIRE(result.config.has_value());
    CHECK(result.config->architecture.enabled);
    CHECK(result.config->architecture.fanOutWarning == 10);
    CHECK(result.config->architecture.fanOutHigh == 20);
}

TEST_CASE("parseConfig reports CONFIG013 when fanOutWarning is greater than fanOutHigh")
{
    const std::string text = R"({ "architecture": { "fanOutWarning": 30, "fanOutHigh": 10 } })";
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE_FALSE(result.config.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG013") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG002 for an unknown key inside architecture")
{
    const std::string text = R"({ "architecture": { "enabled": true, "note": "x" } })";
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE_FALSE(result.config.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG002") != std::string::npos);
}

TEST_CASE("toJson round-trips architecture through parseConfig")
{
    codeqa::config::ProjectConfig original = codeqa::config::defaultConfig();
    original.architecture.enabled = false;
    original.architecture.fanOutWarning = 7;
    original.architecture.fanOutHigh = 14;

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(codeqa::config::toJson(original).dump());

    REQUIRE(result.config.has_value());
    CHECK_FALSE(result.config->architecture.enabled);
    CHECK(result.config->architecture.fanOutWarning == 7);
    CHECK(result.config->architecture.fanOutHigh == 14);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run:
```
cmake --build build --config Debug --target codeqa_tests
```
Expected: FAIL to compile — `ProjectConfig` has no `architecture` member yet.

- [ ] **Step 3: Add the config struct**

In `include/codeqa/config/ConfigModels.h`, add right after `struct DuplicationConfig { ... };`:

```cpp
struct ArchitectureConfig
{
    bool enabled{true};
    long long fanOutWarning{10};
    long long fanOutHigh{20};
};
```

Add a matching field to `ProjectConfig`, right after `DuplicationConfig duplication;`:

```cpp
    ArchitectureConfig architecture;
```

- [ ] **Step 4: Implement JSON parsing and serialization**

In `src/config/ConfigJson.cpp`, add a new function right after `parseDuplication` (before its closing `} // namespace`):

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

In `parseConfig`, change the top-level allowed-keys list:

```cpp
        requireKeysSubset(document, {"languages", "rules", "qualityGate", "duplication", "architecture"}, "config");
```

Add parsing right after the existing `duplication` block (after its closing `}`):

```cpp

        if (document.contains("architecture"))
        {
            const Json& architecture = document.at("architecture");
            requireObject(architecture, "architecture");
            config.architecture = parseArchitecture(architecture);
        }
```

In `toJson`, add right after the existing `document["duplication"] = {...};` block:

```cpp

    document["architecture"] = {
        {"enabled", config.architecture.enabled},
        {"fanOutWarning", config.architecture.fanOutWarning},
        {"fanOutHigh", config.architecture.fanOutHigh}};
```

- [ ] **Step 5: Run tests to verify they pass**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\Debug\codeqa_tests.exe --test-case="*architecture*"
```
Expected: PASS (6 test cases, 0 failures).

- [ ] **Step 6: Run the full suite**

Run:
```
build\Debug\codeqa_tests.exe
```
Expected: PASS, all cases — in particular confirms the new top-level `"architecture"` key doesn't break `"toJson round-trips the default configuration through parseConfig"` or any other existing `parseConfig`/`toJson` test.

- [ ] **Step 7: Commit**

```bash
git add include/codeqa/config/ConfigModels.h src/config/ConfigJson.cpp tests/unit/ConfigJsonTest.cpp
git commit -m "feat: add architecture config (fan-out thresholds) to codeqa.json schema"
```

---

### Task 4: Orchestrator + RunAnalysis wiring

**Files:**
- Create: `include/codeqa/architecture/AnalyzeArchitecture.h`
- Create: `src/architecture/AnalyzeArchitecture.cpp`
- Modify: `CMakeLists.txt` (add the new source file)
- Modify: `include/codeqa/application/RunAnalysis.h`
- Modify: `src/application/RunAnalysis.cpp`
- Test: `tests/unit/AnalyzeArchitectureTest.cpp`
- Modify: `tests/unit/RunAnalysisTest.cpp`
- Modify: `tests/CMakeLists.txt` (add the new test file)

**Interfaces:**
- Consumes: `resolveIncludeGraph`, `findCycles`, `computeFanInOut` (Task 1-2, exact signatures above); `codeqa::config::ArchitectureConfig{enabled, fanOutWarning, fanOutHigh}` (Task 3).
- Produces: `analyzeArchitecture(const codeqa::model::ProjectModel&, const codeqa::config::ArchitectureConfig&) -> codeqa::architecture::ProjectArchitectureModel` (Task 5 and 6 consume this exact signature); `RunAnalysisResult::architecture` field (Task 5 and 6 consume this exact field name).

- [ ] **Step 1: Write the failing tests**

Create `tests/unit/AnalyzeArchitectureTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/architecture/AnalyzeArchitecture.h"
#include "codeqa/config/DefaultConfig.h"

namespace
{

codeqa::model::FileModel makeFile(std::string path)
{
    codeqa::model::FileModel file;
    file.path = std::move(path);
    return file;
}

codeqa::model::DependencyModel makeDependency(std::string target)
{
    codeqa::model::DependencyModel dependency;
    dependency.target = std::move(target);
    dependency.kind = codeqa::model::IncludeKind::Local;
    return dependency;
}

} // namespace

TEST_CASE("analyzeArchitecture reports a cycle end-to-end from raw includes")
{
    codeqa::model::ProjectModel project;
    project.files.push_back(makeFile("a.h"));
    project.files.push_back(makeFile("b.h"));
    project.files[0].dependencies.push_back(makeDependency("b.h"));
    project.files[1].dependencies.push_back(makeDependency("a.h"));

    const codeqa::config::ArchitectureConfig config = codeqa::config::defaultConfig().architecture;
    const codeqa::architecture::ProjectArchitectureModel result =
        codeqa::architecture::analyzeArchitecture(project, config);

    REQUIRE(result.cycles.size() == 1);
    CHECK(result.cycles[0].files.size() == 2);
    CHECK(result.files.size() == 2);
}

TEST_CASE("analyzeArchitecture flags a file whose fan-out is at or above the warning threshold")
{
    codeqa::model::ProjectModel project;
    for (int i = 0; i < 11; ++i)
    {
        project.files.push_back(makeFile("dep" + std::to_string(i) + ".h"));
    }
    project.files.push_back(makeFile("hub.h"));
    for (int i = 0; i < 11; ++i)
    {
        project.files.back().dependencies.push_back(makeDependency("dep" + std::to_string(i) + ".h"));
    }

    codeqa::config::ArchitectureConfig config;
    config.fanOutWarning = 10;
    config.fanOutHigh = 20;

    const codeqa::architecture::ProjectArchitectureModel result =
        codeqa::architecture::analyzeArchitecture(project, config);

    REQUIRE(result.fanOutFindings.size() == 1);
    CHECK(result.fanOutFindings[0].path == "hub.h");
    CHECK(result.fanOutFindings[0].fanOut == 11);
    CHECK(result.fanOutFindings[0].severity == codeqa::core::DiagnosticSeverity::Warning);
}

TEST_CASE("analyzeArchitecture uses Error severity at or above the high threshold")
{
    codeqa::model::ProjectModel project;
    for (int i = 0; i < 20; ++i)
    {
        project.files.push_back(makeFile("dep" + std::to_string(i) + ".h"));
    }
    project.files.push_back(makeFile("hub.h"));
    for (int i = 0; i < 20; ++i)
    {
        project.files.back().dependencies.push_back(makeDependency("dep" + std::to_string(i) + ".h"));
    }

    codeqa::config::ArchitectureConfig config;
    config.fanOutWarning = 10;
    config.fanOutHigh = 20;

    const codeqa::architecture::ProjectArchitectureModel result =
        codeqa::architecture::analyzeArchitecture(project, config);

    REQUIRE(result.fanOutFindings.size() == 1);
    CHECK(result.fanOutFindings[0].severity == codeqa::core::DiagnosticSeverity::Error);
}

TEST_CASE("analyzeArchitecture never flags fan-in, regardless of value")
{
    codeqa::model::ProjectModel project;
    project.files.push_back(makeFile("core.h"));
    for (int i = 0; i < 30; ++i)
    {
        project.files.push_back(makeFile("user" + std::to_string(i) + ".h"));
        project.files.back().dependencies.push_back(makeDependency("core.h"));
    }

    const codeqa::config::ArchitectureConfig config = codeqa::config::defaultConfig().architecture;
    const codeqa::architecture::ProjectArchitectureModel result =
        codeqa::architecture::analyzeArchitecture(project, config);

    CHECK(result.fanOutFindings.empty());

    bool foundCore = false;
    for (const codeqa::architecture::FileDependencyMetrics& file : result.files)
    {
        if (file.path == "core.h")
        {
            foundCore = true;
            CHECK(file.fanIn == 30);
        }
    }
    CHECK(foundCore);
}

TEST_CASE("analyzeArchitecture returns an empty model when disabled")
{
    codeqa::model::ProjectModel project;
    project.files.push_back(makeFile("a.h"));
    project.files.push_back(makeFile("b.h"));
    project.files[0].dependencies.push_back(makeDependency("b.h"));
    project.files[1].dependencies.push_back(makeDependency("a.h"));

    codeqa::config::ArchitectureConfig config;
    config.enabled = false;

    const codeqa::architecture::ProjectArchitectureModel result =
        codeqa::architecture::analyzeArchitecture(project, config);

    CHECK(result.files.empty());
    CHECK(result.cycles.empty());
    CHECK(result.fanOutFindings.empty());
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run:
```
cmake --build build --config Debug --target codeqa_tests
```
Expected: FAIL to compile — `codeqa/architecture/AnalyzeArchitecture.h` does not exist yet.

- [ ] **Step 3: Implement the orchestrator**

Create `include/codeqa/architecture/AnalyzeArchitecture.h`:

```cpp
#pragma once

#include "codeqa/architecture/ArchitectureModels.h"
#include "codeqa/config/ConfigModels.h"
#include "codeqa/model/Models.h"

namespace codeqa::architecture
{

[[nodiscard]] ProjectArchitectureModel analyzeArchitecture(
    const codeqa::model::ProjectModel& project, const codeqa::config::ArchitectureConfig& config);

} // namespace codeqa::architecture
```

Create `src/architecture/AnalyzeArchitecture.cpp`:

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

- [ ] **Step 4: Wire into RunAnalysis**

In `include/codeqa/application/RunAnalysis.h`, add `#include "codeqa/architecture/ArchitectureModels.h"` to the includes, and add a field to `RunAnalysisResult` right after `codeqa::duplication::ProjectDuplicationModel duplicates;`:

```cpp
    codeqa::architecture::ProjectArchitectureModel architecture;
```

In `src/application/RunAnalysis.cpp`, add `#include "codeqa/architecture/AnalyzeArchitecture.h"` to the includes, and add this line right after the existing `result.duplicates = codeqa::duplication::detectDuplicates(result.model, request.config.duplication);`:

```cpp
    result.architecture = codeqa::architecture::analyzeArchitecture(result.model, request.config.architecture);
```

- [ ] **Step 5: Add a RunAnalysis integration test**

Append to `tests/unit/RunAnalysisTest.cpp`, after the existing `TEST_CASE("RunAnalysis detects a duplicated function pair across two files")`:

```cpp

TEST_CASE("RunAnalysis detects a real 2-file include cycle end-to-end")
{
    const TempProject fixture;
    fixture.addFile("a.h", "#include \"b.h\"\n");
    fixture.addFile("b.h", "#include \"a.h\"\n");

    codeqa::application::RunAnalysisRequest request;
    request.analysis.discovery.rootPath = fixture.root();
    request.config = codeqa::config::defaultConfig();

    const codeqa::application::RunAnalysisResult result = codeqa::application::RunAnalysis{}.execute(request);

    REQUIRE(result.architecture.cycles.size() == 1);
    CHECK(result.architecture.cycles.front().files.size() == 2);
}
```

- [ ] **Step 6: Register the new source and test files**

In `CMakeLists.txt`, add right after `src/architecture/ComputeFanInOut.cpp`:

```cmake
    src/architecture/AnalyzeArchitecture.cpp
```

In `tests/CMakeLists.txt`, add `unit/AnalyzeArchitectureTest.cpp` right after `unit/ComputeFanInOutTest.cpp`.

- [ ] **Step 7: Run tests to verify they pass**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\Debug\codeqa_tests.exe --test-case="analyzeArchitecture*,RunAnalysis*"
```
Expected: PASS.

- [ ] **Step 8: Run the full suite**

Run:
```
build\Debug\codeqa_tests.exe
```
Expected: PASS, all cases.

- [ ] **Step 9: Commit**

```bash
git add include/codeqa/architecture/AnalyzeArchitecture.h src/architecture/AnalyzeArchitecture.cpp include/codeqa/application/RunAnalysis.h src/application/RunAnalysis.cpp CMakeLists.txt tests/CMakeLists.txt tests/unit/AnalyzeArchitectureTest.cpp tests/unit/RunAnalysisTest.cpp
git commit -m "feat: wire architecture analysis into RunAnalysis"
```

---

### Task 5: Report rendering (JSON + text)

**Files:**
- Modify: `include/codeqa/report/ReportJson.h`
- Modify: `src/report/ReportJson.cpp`
- Modify: `include/codeqa/report/TextReport.h`
- Modify: `src/report/TextReport.cpp`
- Modify: `src/application/ApplicationFacade.cpp`
- Modify: `src/cli/RunApplication.cpp`
- Test: `tests/unit/ReportJsonTest.cpp`
- Test: `tests/unit/TextReportTest.cpp`

**Interfaces:**
- Consumes: `codeqa::architecture::ProjectArchitectureModel` (Task 1), `RunAnalysisResult::architecture` (Task 4).
- Produces: `toReportJson(..., const codeqa::architecture::ProjectArchitectureModel&, ...)` and `renderText(..., const codeqa::architecture::ProjectArchitectureModel&)` — both gain one new parameter, positioned immediately after their existing `duplicates` parameter. Task 6 does not call these directly but must match the same parameter shape conceptually when it touches the two call sites' siblings.

- [ ] **Step 1: Write the failing tests**

Append to `tests/unit/ReportJsonTest.cpp`, after the existing `TEST_CASE("toReportJson emits an empty duplicates array when there are no groups")`:

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

TEST_CASE("toReportJson emits empty architecture arrays when there is nothing to report")
{
    codeqa::model::ProjectModel model;
    model.rootPath = std::filesystem::path{"project"};
    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::rules::ProjectFindingsModel findings;
    const codeqa::duplication::ProjectDuplicationModel duplicates;
    const codeqa::architecture::ProjectArchitectureModel architecture;

    const nlohmann::json document = codeqa::report::toReportJson(
        model, metrics, findings, duplicates, architecture, codeqa::config::defaultConfig(), "9.9.9");

    CHECK(document.at("architecture").at("files").empty());
    CHECK(document.at("architecture").at("cycles").empty());
    CHECK(document.at("architecture").at("fanOutFindings").empty());
}
```

Three earlier calls in this same file need the same new argument inserted. Each is a distinct literal string (different trailing arguments) — replace each exactly:

Replace:
```cpp
    const nlohmann::json document = codeqa::report::toReportJson(model, metrics, findings, codeqa::duplication::ProjectDuplicationModel{}, config, "9.9.9");
```
with:
```cpp
    const nlohmann::json document = codeqa::report::toReportJson(model, metrics, findings, codeqa::duplication::ProjectDuplicationModel{}, codeqa::architecture::ProjectArchitectureModel{}, config, "9.9.9");
```

Replace both occurrences (they are identical) of:
```cpp
        codeqa::report::toReportJson(model, metrics, findings, codeqa::duplication::ProjectDuplicationModel{}, codeqa::config::defaultConfig(), "0.0.0");
```
with:
```cpp
        codeqa::report::toReportJson(model, metrics, findings, codeqa::duplication::ProjectDuplicationModel{}, codeqa::architecture::ProjectArchitectureModel{}, codeqa::config::defaultConfig(), "0.0.0");
```
(one is prefixed with `const nlohmann::json document =`, the other with `(void)` — keep each call's own prefix, only the argument list changes).

Append to `tests/unit/TextReportTest.cpp`, after the existing `TEST_CASE("renderText lists each duplicate group with its members")`:

```cpp

TEST_CASE("renderText omits the architecture section when there is nothing to report")
{
    codeqa::model::ProjectModel model;
    model.rootPath = "project";
    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::rules::ProjectFindingsModel findings;
    const codeqa::duplication::ProjectDuplicationModel duplicates;
    const codeqa::architecture::ProjectArchitectureModel architecture;

    std::ostringstream output;
    codeqa::report::renderText(output, model, metrics, findings, duplicates, architecture);

    CHECK(output.str().find("Architecture") == std::string::npos);
}

TEST_CASE("renderText lists a cycle and a fan-out finding")
{
    codeqa::model::ProjectModel model;
    model.rootPath = "project";
    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::rules::ProjectFindingsModel findings;
    const codeqa::duplication::ProjectDuplicationModel duplicates;

    codeqa::architecture::ProjectArchitectureModel architecture;
    architecture.cycles.push_back(codeqa::architecture::DependencyCycle{
        {model.rootPath / "src" / "a.h", model.rootPath / "src" / "b.h"}});
    architecture.fanOutFindings.push_back(codeqa::architecture::FanOutFinding{
        model.rootPath / "src" / "hub.h", 15, codeqa::core::DiagnosticSeverity::Warning});

    std::ostringstream output;
    codeqa::report::renderText(output, model, metrics, findings, duplicates, architecture);
    const std::string text = output.str();

    CHECK(text.find("Architecture") != std::string::npos);
    CHECK(text.find("hub.h") != std::string::npos);
    CHECK(text.find("fan-out=15") != std::string::npos);
}
```

Five earlier calls in this same file need the same new argument inserted — two distinct literal strings, each appearing multiple times. Replace all 3 occurrences of:
```cpp
    codeqa::report::renderText(output, model, metrics, findings, codeqa::duplication::ProjectDuplicationModel{});
```
with:
```cpp
    codeqa::report::renderText(output, model, metrics, findings, codeqa::duplication::ProjectDuplicationModel{}, codeqa::architecture::ProjectArchitectureModel{});
```

Replace all 2 occurrences of:
```cpp
    codeqa::report::renderText(output, model, metrics, findings, duplicates);
```
with:
```cpp
    codeqa::report::renderText(output, model, metrics, findings, duplicates, codeqa::architecture::ProjectArchitectureModel{});
```

- [ ] **Step 2: Run tests to verify they fail**

Run:
```
cmake --build build --config Debug --target codeqa_tests
```
Expected: FAIL to compile — `toReportJson`/`renderText` don't accept an `architecture` argument yet.

- [ ] **Step 3: Update ReportJson**

In `include/codeqa/report/ReportJson.h`, add `#include "codeqa/architecture/ArchitectureModels.h"` and a new parameter to `toReportJson`, right after `const codeqa::duplication::ProjectDuplicationModel& duplicates,`:

```cpp
    const codeqa::architecture::ProjectArchitectureModel& architecture,
```

In `src/report/ReportJson.cpp`, add a new function right after `duplicateGroupJson` (before its closing `} // namespace`):

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

In `toReportJson`'s own parameter list, add the new parameter matching the header, and add this line right after the existing `document["duplicates"] = ...` loop, before `return document;`:

```cpp
    document["architecture"] = architectureJson(architecture, model.rootPath);
```

- [ ] **Step 4: Update TextReport**

In `include/codeqa/report/TextReport.h`, add `#include "codeqa/architecture/ArchitectureModels.h"` and a new parameter to `renderText`, right after `const codeqa::duplication::ProjectDuplicationModel& duplicates`:

```cpp
    const codeqa::architecture::ProjectArchitectureModel& architecture
```

In `src/report/TextReport.cpp`, add a new function right after `writeDuplicates` (before `writeDiagnostics`):

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

Update `renderText`'s parameter list to match the header, and add this line right after `writeDuplicates(output, duplicates, model.rootPath);`:

```cpp
    writeArchitecture(output, architecture, model.rootPath);
```

- [ ] **Step 5: Update the two call sites**

In `src/application/ApplicationFacade.cpp`, the existing call:

```cpp
        result.reportJson = codeqa::report::toReportJson(
            analysis.model, analysis.metrics, analysis.findings, analysis.duplicates,
            request.config, toolVersion);
```

becomes:

```cpp
        result.reportJson = codeqa::report::toReportJson(
            analysis.model, analysis.metrics, analysis.findings, analysis.duplicates, analysis.architecture,
            request.config, toolVersion);
```

In `src/cli/RunApplication.cpp`, the existing call:

```cpp
        codeqa::report::renderText(
            *destination, textAnalysis.model, textAnalysis.metrics, textAnalysis.findings,
            textAnalysis.duplicates);
```

becomes:

```cpp
        codeqa::report::renderText(
            *destination, textAnalysis.model, textAnalysis.metrics, textAnalysis.findings,
            textAnalysis.duplicates, textAnalysis.architecture);
```

(`textAnalysis.architecture` will be empty until Task 6 wires the `ReportDocument` round-trip — that's expected and fixed by the next task, not a bug in this one. This task's own tests exercise `toReportJson`/`renderText` directly, not through the CLI end-to-end, so they pass regardless.)

- [ ] **Step 6: Run tests to verify they pass**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\Debug\codeqa_tests.exe --test-case="*architecture*,*Duplicates*"
```
Expected: PASS.

- [ ] **Step 7: Run the full suite**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\Debug\codeqa_tests.exe
```
Expected: PASS, all cases (this also compiles `codeqa` and `codeqa_desktop`'s other callers indirectly through `codeqa_core`, catching any missed call site).

- [ ] **Step 8: Commit**

```bash
git add include/codeqa/report/ReportJson.h src/report/ReportJson.cpp include/codeqa/report/TextReport.h src/report/TextReport.cpp src/application/ApplicationFacade.cpp src/cli/RunApplication.cpp tests/unit/ReportJsonTest.cpp tests/unit/TextReportTest.cpp
git commit -m "feat: render architecture cycles and fan-out findings in JSON and text reports"
```

---

### Task 6: ReportDocument persistence round-trip

**Files:**
- Modify: `include/codeqa/application/ReportDocument.h`
- Modify: `src/application/ReportDocumentFactory.cpp`
- Modify: `src/application/ReportDocumentAnalysis.cpp`
- Test: `tests/unit/ReportDocumentTest.cpp`
- Test: `tests/unit/ReportDocumentAnalysisTest.cpp`

**Interfaces:**
- Consumes: `codeqa::architecture::ProjectArchitectureModel` and its member types (Task 1), `RunAnalysisResult::architecture` (Task 4).
- Produces: `ReportDocument::architecture` field, consumed by nothing later in this plan — this is the final task.

- [ ] **Step 1: Write the failing tests**

Add `#include "codeqa/architecture/ArchitectureModels.h"` to `tests/unit/ReportDocumentTest.cpp`'s includes, right after the existing `#include "codeqa/duplication/DuplicationModels.h"` — matches this file's existing style of explicitly including every type it directly names, even though it would also be reachable transitively through `ReportDocument.h` after this task's Step 3.

Append to `tests/unit/ReportDocumentTest.cpp` (find the existing test(s) that build a `RunAnalysisResult` and call `makeReportDocument`, then parse it back — mirror that exact fixture pattern for this new case):

```cpp

TEST_CASE("makeReportDocument and parseReportDocument round-trip architecture data")
{
    codeqa::application::RunAnalysisResult analysis;
    analysis.model.rootPath = std::filesystem::path{"project"};

    analysis.architecture.files.push_back(codeqa::architecture::FileDependencyMetrics{
        analysis.model.rootPath / "src" / "a.h", 2, 1});
    analysis.architecture.cycles.push_back(codeqa::architecture::DependencyCycle{
        {analysis.model.rootPath / "src" / "a.h", analysis.model.rootPath / "src" / "b.h"}});
    analysis.architecture.fanOutFindings.push_back(codeqa::architecture::FanOutFinding{
        analysis.model.rootPath / "src" / "a.h", 12, codeqa::core::DiagnosticSeverity::Warning});
    analysis.architecture.externalOrUnresolvedIncludes = 2;
    analysis.architecture.ambiguousIncludes = 1;

    const codeqa::application::ReportDocument document = codeqa::application::makeReportDocument(
        analysis, codeqa::config::defaultConfig(), "9.9.9", codeqa::application::QualityGateOutcome::Passed,
        std::nullopt);

    REQUIRE(document.architecture.files.size() == 1);
    CHECK(document.architecture.files[0].path == analysis.model.rootPath / "src" / "a.h");
    REQUIRE(document.architecture.cycles.size() == 1);
    CHECK(document.architecture.fanOutFindings.size() == 1);
    CHECK(document.architecture.externalOrUnresolvedIncludes == 2);
    CHECK(document.architecture.ambiguousIncludes == 1);

    const nlohmann::json json = codeqa::report::toReportJson(
        analysis.model, analysis.metrics, analysis.findings, analysis.duplicates, analysis.architecture,
        codeqa::config::defaultConfig(), "9.9.9");

    const codeqa::application::ReportDocumentResult parsed = codeqa::application::parseReportDocument(json);

    REQUIRE(parsed.document.has_value());
    REQUIRE(parsed.document->architecture.files.size() == 1);
    CHECK(parsed.document->architecture.files[0].path == std::filesystem::path{"src"} / "a.h");
    REQUIRE(parsed.document->architecture.cycles.size() == 1);
    REQUIRE(parsed.document->architecture.fanOutFindings.size() == 1);
    CHECK(parsed.document->architecture.fanOutFindings[0].severity == codeqa::core::DiagnosticSeverity::Warning);
    CHECK(parsed.document->architecture.externalOrUnresolvedIncludes == 2);
    CHECK(parsed.document->architecture.ambiguousIncludes == 1);
}
```

Append to `tests/unit/ReportDocumentAnalysisTest.cpp`:

```cpp

TEST_CASE("toAnalysisResult absolutizes architecture paths read from a document")
{
    codeqa::application::ReportDocument document;
    document.metadata.rootPath = "C:/project";
    document.architecture.files.push_back(
        codeqa::architecture::FileDependencyMetrics{std::filesystem::path{"src"} / "a.h", 1, 0});
    document.architecture.cycles.push_back(codeqa::architecture::DependencyCycle{
        {std::filesystem::path{"src"} / "a.h", std::filesystem::path{"src"} / "b.h"}});
    document.architecture.fanOutFindings.push_back(codeqa::architecture::FanOutFinding{
        std::filesystem::path{"src"} / "a.h", 5, codeqa::core::DiagnosticSeverity::Warning});

    const codeqa::application::RunAnalysisResult analysis = codeqa::application::toAnalysisResult(document);

    REQUIRE(analysis.architecture.files.size() == 1);
    CHECK(analysis.architecture.files[0].path == std::filesystem::path{"C:/project"} / "src" / "a.h");
    REQUIRE(analysis.architecture.cycles.size() == 1);
    CHECK(analysis.architecture.cycles[0].files[0] == std::filesystem::path{"C:/project"} / "src" / "a.h");
    REQUIRE(analysis.architecture.fanOutFindings.size() == 1);
    CHECK(analysis.architecture.fanOutFindings[0].path == std::filesystem::path{"C:/project"} / "src" / "a.h");
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run:
```
cmake --build build --config Debug --target codeqa_tests
```
Expected: FAIL to compile — `ReportDocument` has no `architecture` member yet.

- [ ] **Step 3: Add the field to ReportDocument**

In `include/codeqa/application/ReportDocument.h`, add `#include "codeqa/architecture/ArchitectureModels.h"` to the includes, and add a field to `ReportDocument` right after `codeqa::duplication::ProjectDuplicationModel duplicates;`:

```cpp
    codeqa::architecture::ProjectArchitectureModel architecture;
```

- [ ] **Step 4: Write and read it in ReportDocumentFactory**

In `src/application/ReportDocumentFactory.cpp`, in `makeReportDocument`, add this line right after the existing `document.duplicates = analysis.duplicates;`:

```cpp
    document.architecture = analysis.architecture;
```

Add three new parse helper functions right after `parseDuplicateGroup` (before its closing `} // namespace`):

```cpp
codeqa::architecture::FileDependencyMetrics parseArchitectureFile(const Json& document, const std::string& path)
{
    if (!document.is_object())
    {
        failRequired("'" + path + "' must be an object.");
    }

    codeqa::architecture::FileDependencyMetrics file;
    file.path = requireString(document, "path", path);
    file.fanIn = requireSize(document, "fanIn", path);
    file.fanOut = requireSize(document, "fanOut", path);
    return file;
}

codeqa::architecture::DependencyCycle parseDependencyCycle(const Json& document, const std::string& path)
{
    if (!document.is_object())
    {
        failRequired("'" + path + "' must be an object.");
    }

    codeqa::architecture::DependencyCycle cycle;
    const Json& files = requireArray(document, "files", path);
    for (std::size_t index = 0; index < files.size(); ++index)
    {
        const Json& fileEntry = files.at(index);
        if (!fileEntry.is_string())
        {
            failRequired("'" + path + ".files[" + std::to_string(index) + "]' must be a string.");
        }
        cycle.files.push_back(fileEntry.get<std::string>());
    }
    return cycle;
}

codeqa::architecture::FanOutFinding parseFanOutFinding(const Json& document, const std::string& path)
{
    if (!document.is_object())
    {
        failRequired("'" + path + "' must be an object.");
    }

    codeqa::architecture::FanOutFinding finding;
    finding.path = requireString(document, "path", path);
    finding.fanOut = requireSize(document, "fanOut", path);
    finding.severity = parseSeverity(requireString(document, "severity", path), path);
    return finding;
}
```

In `parseReportDocument`, add this block right after the existing `if (document.contains("duplicates")) { ... }` block:

```cpp

        if (document.contains("architecture"))
        {
            const Json& architecture = requireObject(document, "architecture", "report");

            const Json& files = requireArray(architecture, "files", "report.architecture");
            for (std::size_t index = 0; index < files.size(); ++index)
            {
                result.architecture.files.push_back(parseArchitectureFile(
                    files.at(index), "report.architecture.files[" + std::to_string(index) + "]"));
            }

            const Json& cycles = requireArray(architecture, "cycles", "report.architecture");
            for (std::size_t index = 0; index < cycles.size(); ++index)
            {
                result.architecture.cycles.push_back(parseDependencyCycle(
                    cycles.at(index), "report.architecture.cycles[" + std::to_string(index) + "]"));
            }

            const Json& fanOutFindings = requireArray(architecture, "fanOutFindings", "report.architecture");
            for (std::size_t index = 0; index < fanOutFindings.size(); ++index)
            {
                result.architecture.fanOutFindings.push_back(parseFanOutFinding(
                    fanOutFindings.at(index),
                    "report.architecture.fanOutFindings[" + std::to_string(index) + "]"));
            }

            result.architecture.externalOrUnresolvedIncludes =
                requireSize(architecture, "externalOrUnresolvedIncludes", "report.architecture");
            result.architecture.ambiguousIncludes =
                requireSize(architecture, "ambiguousIncludes", "report.architecture");
        }
```

(`requireSize`, `requireArray`, `requireObject`, `requireString`, `failRequired`, and `parseSeverity` all already exist in this file, used by the finding/duplicate parsing right above — no new helpers needed beyond the three functions above.)

- [ ] **Step 5: Absolutize on the way back out**

In `src/application/ReportDocumentAnalysis.cpp`, add a new function right after `absolutizeDuplicateMembers` (before its closing `} // namespace`):

```cpp
/// Mirrors absolutizeDuplicateMembers above: rewrites every architecture path from its
/// stored (relative) form to the rootPath-prefixed absolute form. Safe to call even if a
/// path were already absolute -- std::filesystem::path::operator/ with an absolute
/// right-hand side discards the left side per the standard, making this idempotent.
void absolutizeArchitecturePaths(
    codeqa::architecture::ProjectArchitectureModel& architecture, const std::filesystem::path& rootPath)
{
    for (codeqa::architecture::FileDependencyMetrics& file : architecture.files)
    {
        file.path = rootPath / file.path;
    }

    for (codeqa::architecture::DependencyCycle& cycle : architecture.cycles)
    {
        for (std::filesystem::path& path : cycle.files)
        {
            path = rootPath / path;
        }
    }

    for (codeqa::architecture::FanOutFinding& finding : architecture.fanOutFindings)
    {
        finding.path = rootPath / finding.path;
    }
}
```

In `toAnalysisResult`, add these two lines right after the existing `analysis.duplicates = document.duplicates; absolutizeDuplicateMembers(analysis.duplicates, analysis.model.rootPath);`:

```cpp
    analysis.architecture = document.architecture;
    absolutizeArchitecturePaths(analysis.architecture, analysis.model.rootPath);
```

- [ ] **Step 6: Run tests to verify they pass**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\Debug\codeqa_tests.exe --test-case="*architecture*"
```
Expected: PASS.

- [ ] **Step 7: Run the full suite**

Run:
```
build\Debug\codeqa_tests.exe
```
Expected: PASS, all cases, 0 failures.

- [ ] **Step 8: Commit**

```bash
git add include/codeqa/application/ReportDocument.h src/application/ReportDocumentFactory.cpp src/application/ReportDocumentAnalysis.cpp tests/unit/ReportDocumentTest.cpp tests/unit/ReportDocumentAnalysisTest.cpp
git commit -m "feat: carry architecture data through the ReportDocument persistence round-trip"
```

---

## Self-Review Notes

- **Spec coverage:** §4 (data model) → Task 1. §5 (cycles) → Task 2. §6 (fan-in/out) → Task 2, Task 4's threshold logic. §7 (config) → Task 3. §8 (wiring) → Task 4. §9 (report integration, including the two corrections made during planning: `IncludeGraph`'s real header location, and the `ReportDocument` round-trip) → Task 5 (JSON/text) and Task 6 (persistence round-trip). §10 (testing strategy) → covered by every task's own test step; no separate `tests/corpus/` fixture was added, matching the spec's own note that hand-built `ProjectModel` fixtures may suffice — Task 4's `RunAnalysisTest` addition uses real files on disk via `TempProject`, which is the closer-to-real-world check the spec asked for. §11 (acceptance criteria) — each criterion maps to a specific test: the 2-file cycle criterion to Task 4's `RunAnalysisTest` case, the fan-out severity criteria to Task 4's `AnalyzeArchitectureTest` cases, the "fan-in never flags" criterion to Task 4's dedicated test, the "system includes never count" criterion to Task 1's test, the "disabled produces empty model" criterion to Task 4's test, "no change to existing rules/gate/suppression" is structural (no task touches `codeqa::rules`, `ApplyRules`, or the quality gate at all).
- **Placeholder scan:** none found — every step carries literal code.
- **Type/name consistency:** `ProjectArchitectureModel{files, cycles, fanOutFindings, externalOrUnresolvedIncludes, ambiguousIncludes}` (Task 1) is used with these exact field names in Tasks 4, 5, and 6. `IncludeGraph{filePaths, outgoingEdges, externalOrUnresolvedIncludes, ambiguousIncludes}` (Task 1) is consumed identically in Task 2 and Task 4. `analyzeArchitecture`'s signature (Task 4) matches its declaration and its two call-adjacent tests exactly. `ArchitectureConfig{enabled, fanOutWarning, fanOutHigh}` (Task 3) matches `AnalyzeArchitecture.cpp`'s usage (Task 4) and the JSON parse/serialize code (Task 3) exactly.
