# Fase 10 — Histórico mínimo Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `codeqa analyze <path>` automatically saves a snapshot of every run under `<path>/.codeqa/snapshots/<timestamp>.json` and compares it against the most recent previous snapshot, classifying each finding as new/existing/resolved — no new flag required. A new `codeqa snapshot list|show` command lists/inspects saved snapshots.

**Architecture:** A new pure layer `codeqa::history` extracts a `(ruleId, symbolName, relativeFilePath)` key set from findings (either the live `ProjectFindingsModel` of the current run, or a `nlohmann::json` snapshot loaded from disk) and diffs two key sets into new/existing/resolved counts. Three new `codeqa::application` classes (`SaveSnapshot`/`ListSnapshots`/`LoadSnapshot`) are the only pieces touching `.codeqa/snapshots/` on disk, mirroring `LoadProjectConfig`'s pattern from Fase 8. `RunApplication.cpp`'s `runAnalyze` wires these together after `RunAnalysis` (Fase 9, unchanged) produces its result; a new `runSnapshot` dispatches `list`/`show`.

**Tech Stack:** C++17, CMake, doctest, nlohmann::json (already vendored, used since Fase 8).

## Global Constraints

- `analyze` saves a snapshot and compares automatically on every run — **no new CLI flag**. Fase 12's target command (`codeqa analyze ./project --config codeqa.json --format json --output report.json`) carries none, and lists "snapshot saved + comparison" as something `analyze` simply produces.
- Finding identity for comparison: `(ruleId, symbolName, file path relative to rootPath)` — deliberately **no** line/column, so moving unrelated code doesn't make a still-live finding look resolved+new. `Finding.symbolName` already holds `qualifiedName` (see `src/rules/Rules.cpp`), so it's reasonably unique per file already.
- `.codeqa/snapshots/` lives under the analyzed `<path>`, never under the CLI's cwd.
- A saved snapshot file is `codeqa::report::toReportJson(...)`'s output **verbatim** — same `schemaVersion`, no extra fields. The comparison result (`comparison`) is ephemeral: added only to the JSON that gets printed for `--format json`, never to the JSON that gets persisted via `SaveSnapshot`.
- A previous snapshot that's missing, isn't valid JSON, or is valid JSON with the wrong shape (missing `files`/`findings`/`ruleId`/etc. keys) is all treated the same way: warn to `errorOutput`, skip the comparison, `analyze` still returns `ExitCode::Success`. No previous snapshot at all (first run) skips silently, no warning — that's not an error case, per the roadmap's acceptance criteria.
- Failing to *save* the current snapshot (disk full, permissions) is the same best-effort story Fase 9 already established for `--output`: warn to `errorOutput`, `analyze` still returns `ExitCode::Success`.
- `analyze`'s exit code is **unaffected** by findings or by the comparison — unchanged from Fase 9. The quality gate (`--fail-on`-style severity gating using "new since last snapshot") is Fase 12, not this phase.
- Timestamp format: UTC, `YYYYMMDDTHHMMSSZ` (`std::gmtime`, no platform-specific `_r`/`_s` variants — this is a single-threaded CLI, thread-safety of `std::gmtime` isn't a concern here), lexicographically sortable. Two `analyze` runs within the same second would overwrite the same snapshot file — an accepted limitation, not engineered around (same spirit as Fase 9 leaving `--output` write failures unchecked).
- New CLI error codes continue the existing sequence: `CLI013` (missing `snapshot` subcommand), `CLI014` (unknown `snapshot` subcommand), `CLI015` (missing `<timestamp>` for `snapshot show`), `CLI016` (snapshot not found/unreadable). `snapshot`'s `<path>` argument reuses `CLI002`/`CLI003` verbatim (same failure mode as `discover`/`parse`/`analyze`); an unexpected trailing argument reuses `CLI005`.

## File Structure

- `include/codeqa/history/SnapshotComparison.h` (new): `SnapshotComparison{newCount, existingCount, resolvedCount}`.
- `include/codeqa/history/CompareSnapshots.h` + `src/history/CompareSnapshots.cpp` (new): `FindingKey`/`FindingKeySet` aliases, both `extractFindingKeys` overloads, `compareSnapshots`.
- `include/codeqa/application/SaveSnapshot.h` + `src/application/SaveSnapshot.cpp` (new).
- `include/codeqa/application/ListSnapshots.h` + `src/application/ListSnapshots.cpp` (new).
- `include/codeqa/application/LoadSnapshot.h` + `src/application/LoadSnapshot.cpp` (new).
- `src/cli/RunApplication.cpp` (modified twice): `runAnalyze` gains automatic snapshot save+compare (Task 5); a new `runSnapshot`/`parseSnapshotArguments` and dispatch entry add the `snapshot` subcommand (Task 6). `printHelp` gains text for both.
- `tests/unit/{HistoryTest,SaveSnapshotTest,ListSnapshotsTest,LoadSnapshotTest}.cpp` (new); `tests/unit/RunApplicationTest.cpp` (modified twice, Tasks 5 and 6).
- `CMakeLists.txt`, `tests/CMakeLists.txt` (modified across Tasks 1-4).

---

### Task 1: `codeqa::history` — pure finding-key comparison

**Files:**
- Create: `include/codeqa/history/SnapshotComparison.h`
- Create: `include/codeqa/history/CompareSnapshots.h`
- Create: `src/history/CompareSnapshots.cpp`
- Create: `tests/unit/HistoryTest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `codeqa::rules::ProjectFindingsModel`/`FileFindingsModel`/`Finding` (Fase 7, unchanged — `ProjectFindingsModel.rootPath` is set by `ApplyRules::execute` from `ProjectModel.rootPath`, confirmed in `src/application/ApplyRules.cpp`); `nlohmann::json` shaped like `codeqa::report::toReportJson`'s output (Fase 9, unchanged).
- Produces: `codeqa::history::SnapshotComparison{std::size_t newCount; std::size_t existingCount; std::size_t resolvedCount;}`; `codeqa::history::FindingKey` (`= std::tuple<std::string, std::string, std::string>`, meaning `(ruleId, symbolName, relativeFilePath)`); `codeqa::history::FindingKeySet` (`= std::set<FindingKey>`); `codeqa::history::extractFindingKeys(const codeqa::rules::ProjectFindingsModel&) -> FindingKeySet`; `codeqa::history::extractFindingKeys(const nlohmann::json&) -> FindingKeySet` (throws `nlohmann::json::exception` if the document doesn't have the expected shape); `codeqa::history::compareSnapshots(const FindingKeySet& current, const FindingKeySet& previous) -> SnapshotComparison`. Used by Task 5.

- [ ] **Step 1: Write the failing tests**

Create `tests/unit/HistoryTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/history/CompareSnapshots.h"

#include <filesystem>

TEST_CASE("extractFindingKeys from a ProjectFindingsModel relativizes each file's path against rootPath")
{
    codeqa::rules::ProjectFindingsModel findings;
    findings.rootPath = std::filesystem::path{"project"};

    codeqa::rules::FileFindingsModel fileFindings;
    fileFindings.path = findings.rootPath / "src" / "foo.cpp";
    codeqa::rules::Finding finding;
    finding.ruleId = "complexity.cyclomatic";
    finding.symbolName = "Foo::bar";
    fileFindings.findings.push_back(finding);
    findings.files.push_back(fileFindings);

    const codeqa::history::FindingKeySet keys = codeqa::history::extractFindingKeys(findings);

    REQUIRE(keys.size() == 1);
    CHECK(*keys.begin() == codeqa::history::FindingKey{"complexity.cyclomatic", "Foo::bar", "src/foo.cpp"});
}

TEST_CASE("extractFindingKeys from JSON reads ruleId/symbolName/path from a toReportJson-shaped document")
{
    const nlohmann::json snapshot = nlohmann::json::parse(R"({
        "files": [
            {
                "path": "src/foo.cpp",
                "findings": [
                    { "ruleId": "complexity.cyclomatic", "symbolName": "Foo::bar" }
                ]
            }
        ]
    })");

    const codeqa::history::FindingKeySet keys = codeqa::history::extractFindingKeys(snapshot);

    REQUIRE(keys.size() == 1);
    CHECK(*keys.begin() == codeqa::history::FindingKey{"complexity.cyclomatic", "Foo::bar", "src/foo.cpp"});
}

TEST_CASE("extractFindingKeys from JSON throws on a snapshot missing the files array")
{
    const nlohmann::json malformed = nlohmann::json::object();

    CHECK_THROWS_AS(codeqa::history::extractFindingKeys(malformed), nlohmann::json::exception);
}

TEST_CASE(
    "compareSnapshots classifies keys only in current as new, only in previous as resolved, "
    "and shared keys as existing")
{
    const codeqa::history::FindingKey shared{"complexity.cyclomatic", "Foo::bar", "src/foo.cpp"};
    const codeqa::history::FindingKey added{"complexity.returns", "Foo::baz", "src/foo.cpp"};
    const codeqa::history::FindingKey removed{"signature.parameter-count", "Foo::qux", "src/foo.cpp"};

    const codeqa::history::FindingKeySet current{shared, added};
    const codeqa::history::FindingKeySet previous{shared, removed};

    const codeqa::history::SnapshotComparison comparison = codeqa::history::compareSnapshots(current, previous);

    CHECK(comparison.newCount == 1);
    CHECK(comparison.existingCount == 1);
    CHECK(comparison.resolvedCount == 1);
}

TEST_CASE("compareSnapshots on two empty sets reports all zero counts")
{
    const codeqa::history::SnapshotComparison comparison =
        codeqa::history::compareSnapshots(codeqa::history::FindingKeySet{}, codeqa::history::FindingKeySet{});

    CHECK(comparison.newCount == 0);
    CHECK(comparison.existingCount == 0);
    CHECK(comparison.resolvedCount == 0);
}
```

Add `unit/HistoryTest.cpp` to `tests/CMakeLists.txt`, right after `unit/ReportJsonTest.cpp` (the last entry).

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails — `codeqa/history/CompareSnapshots.h` does not exist.

- [ ] **Step 3: Create the headers and implementation**

Create `include/codeqa/history/SnapshotComparison.h`:

```cpp
#pragma once

#include <cstddef>

namespace codeqa::history
{

struct SnapshotComparison
{
    std::size_t newCount{0};
    std::size_t existingCount{0};
    std::size_t resolvedCount{0};
};

} // namespace codeqa::history
```

Create `include/codeqa/history/CompareSnapshots.h`:

```cpp
#pragma once

#include "codeqa/history/SnapshotComparison.h"
#include "codeqa/rules/RuleModels.h"

#include <nlohmann/json.hpp>

#include <set>
#include <string>
#include <tuple>

namespace codeqa::history
{

/// (ruleId, symbolName, file path relative to rootPath). No line/column: moving unrelated
/// code shouldn't make a still-live finding look resolved+new.
using FindingKey = std::tuple<std::string, std::string, std::string>;
using FindingKeySet = std::set<FindingKey>;

/// For the just-computed current run (findings still in memory). Relativizes each
/// FileFindingsModel::path against findings.rootPath (ProjectFindingsModel already carries
/// it -- ApplyRules::execute sets it from ProjectModel::rootPath -- no separate rootPath
/// parameter needed).
[[nodiscard]] FindingKeySet extractFindingKeys(const codeqa::rules::ProjectFindingsModel& findings);

/// For a previous run loaded back from a snapshot file. Paths inside snapshotReport are
/// already relative (toReportJson relativizes them at save time). Throws
/// nlohmann::json::exception if the document doesn't have the expected files[].findings[]
/// shape -- the caller (runAnalyze) treats that the same as an unreadable snapshot.
[[nodiscard]] FindingKeySet extractFindingKeys(const nlohmann::json& snapshotReport);

[[nodiscard]] SnapshotComparison compareSnapshots(const FindingKeySet& current, const FindingKeySet& previous);

} // namespace codeqa::history
```

Create `src/history/CompareSnapshots.cpp`:

```cpp
#include "codeqa/history/CompareSnapshots.h"

#include <filesystem>

namespace codeqa::history
{

FindingKeySet extractFindingKeys(const codeqa::rules::ProjectFindingsModel& findings)
{
    FindingKeySet keys;

    for (const codeqa::rules::FileFindingsModel& file : findings.files)
    {
        const std::string relativePath =
            std::filesystem::relative(file.path, findings.rootPath).generic_string();

        for (const codeqa::rules::Finding& finding : file.findings)
        {
            keys.emplace(finding.ruleId, finding.symbolName, relativePath);
        }
    }

    return keys;
}

FindingKeySet extractFindingKeys(const nlohmann::json& snapshotReport)
{
    FindingKeySet keys;

    for (const nlohmann::json& file : snapshotReport.at("files"))
    {
        const std::string path = file.at("path").get<std::string>();

        for (const nlohmann::json& finding : file.at("findings"))
        {
            keys.emplace(
                finding.at("ruleId").get<std::string>(), finding.at("symbolName").get<std::string>(), path);
        }
    }

    return keys;
}

SnapshotComparison compareSnapshots(const FindingKeySet& current, const FindingKeySet& previous)
{
    SnapshotComparison result;

    for (const FindingKey& key : current)
    {
        if (previous.count(key) > 0)
        {
            ++result.existingCount;
        }
        else
        {
            ++result.newCount;
        }
    }

    for (const FindingKey& key : previous)
    {
        if (current.count(key) == 0)
        {
            ++result.resolvedCount;
        }
    }

    return result;
}

} // namespace codeqa::history
```

In `CMakeLists.txt`, add `src/history/CompareSnapshots.cpp` to `add_library(codeqa_core STATIC ...)`, right after `src/report/ReportJson.cpp` (the last entry).

- [ ] **Step 4: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass, including the 5 new ones.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/history/SnapshotComparison.h include/codeqa/history/CompareSnapshots.h src/history/CompareSnapshots.cpp tests/unit/HistoryTest.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add codeqa::history finding-key comparison"
```

---

### Task 2: `codeqa::application::SaveSnapshot`

**Files:**
- Create: `include/codeqa/application/SaveSnapshot.h`
- Create: `src/application/SaveSnapshot.cpp`
- Create: `tests/unit/SaveSnapshotTest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing beyond the standard library and `nlohmann::json`.
- Produces: `codeqa::application::SaveSnapshot::execute(const std::filesystem::path& rootPath, const nlohmann::json& reportJson) const -> std::optional<std::filesystem::path>` — writes `<rootPath>/.codeqa/snapshots/<timestamp>.json` (creating directories as needed) and returns the written path, or `std::nullopt` on any write failure. Used by Task 5.

- [ ] **Step 1: Write the failing tests**

Create `tests/unit/SaveSnapshotTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/application/SaveSnapshot.h"

#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <system_error>

namespace
{

class TempProject
{
public:
    TempProject() : root_(std::filesystem::temp_directory_path() / "codeqa_save_snapshot_test")
    {
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(root_);
    }

    ~TempProject()
    {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    TempProject(const TempProject&) = delete;
    TempProject& operator=(const TempProject&) = delete;

    [[nodiscard]] const std::filesystem::path& root() const { return root_; }

private:
    std::filesystem::path root_;
};

} // namespace

TEST_CASE("SaveSnapshot writes the report JSON under .codeqa/snapshots and returns the path")
{
    const TempProject fixture;
    const nlohmann::json report{{"schemaVersion", 1}, {"metadata", {{"toolName", "codeqa"}}}};

    const std::optional<std::filesystem::path> result =
        codeqa::application::SaveSnapshot{}.execute(fixture.root(), report);

    REQUIRE(result.has_value());
    CHECK(result->parent_path().filename() == "snapshots");
    CHECK(result->parent_path().parent_path().filename() == ".codeqa");
    CHECK(std::regex_match(result->stem().string(), std::regex(R"(\d{8}T\d{6}Z)")));

    std::ifstream savedStream(*result);
    const nlohmann::json saved = nlohmann::json::parse(savedStream);
    CHECK(saved == report);
}

TEST_CASE("SaveSnapshot returns nullopt when the snapshots directory can't be created")
{
    const TempProject fixture;
    std::ofstream(fixture.root() / ".codeqa") << ""; // a file, not a directory -- blocks .codeqa/snapshots

    const std::optional<std::filesystem::path> result =
        codeqa::application::SaveSnapshot{}.execute(fixture.root(), nlohmann::json{{"schemaVersion", 1}});

    CHECK_FALSE(result.has_value());
}
```

Add `unit/SaveSnapshotTest.cpp` to `tests/CMakeLists.txt`, right after `unit/HistoryTest.cpp`.

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails — `codeqa/application/SaveSnapshot.h` does not exist.

- [ ] **Step 3: Create the header and implementation**

Create `include/codeqa/application/SaveSnapshot.h`:

```cpp
#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>

namespace codeqa::application
{

class SaveSnapshot
{
public:
    /// Writes reportJson to <rootPath>/.codeqa/snapshots/<timestamp>.json, creating the
    /// directory if needed. Returns the written path, or nullopt if the write failed (disk
    /// full, permissions) -- the caller decides whether/how to warn.
    [[nodiscard]] std::optional<std::filesystem::path> execute(
        const std::filesystem::path& rootPath, const nlohmann::json& reportJson) const;
};

} // namespace codeqa::application
```

Create `src/application/SaveSnapshot.cpp`:

```cpp
#include "codeqa/application/SaveSnapshot.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace codeqa::application
{

namespace
{

std::string formatTimestamp(std::chrono::system_clock::time_point now)
{
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    const std::tm* utc = std::gmtime(&time);

    std::ostringstream stream;
    stream << std::put_time(utc, "%Y%m%dT%H%M%SZ");
    return stream.str();
}

} // namespace

std::optional<std::filesystem::path> SaveSnapshot::execute(
    const std::filesystem::path& rootPath, const nlohmann::json& reportJson) const
{
    const std::filesystem::path snapshotsDirectory = rootPath / ".codeqa" / "snapshots";

    std::error_code createError;
    std::filesystem::create_directories(snapshotsDirectory, createError);

    if (createError)
    {
        return std::nullopt;
    }

    const std::filesystem::path snapshotPath =
        snapshotsDirectory / (formatTimestamp(std::chrono::system_clock::now()) + ".json");

    std::ofstream snapshotFile(snapshotPath);

    if (!snapshotFile)
    {
        return std::nullopt;
    }

    snapshotFile << reportJson.dump(2);

    if (!snapshotFile)
    {
        return std::nullopt;
    }

    return snapshotPath;
}

} // namespace codeqa::application
```

In `CMakeLists.txt`, add `src/application/SaveSnapshot.cpp` to `add_library(codeqa_core STATIC ...)`, right after `src/history/CompareSnapshots.cpp`.

- [ ] **Step 4: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass, including the 2 new ones.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/application/SaveSnapshot.h src/application/SaveSnapshot.cpp tests/unit/SaveSnapshotTest.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add SaveSnapshot to persist analyze reports under .codeqa/snapshots"
```

---

### Task 3: `codeqa::application::ListSnapshots`

**Files:**
- Create: `include/codeqa/application/ListSnapshots.h`
- Create: `src/application/ListSnapshots.cpp`
- Create: `tests/unit/ListSnapshotsTest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing beyond the standard library.
- Produces: `codeqa::application::ListSnapshots::execute(const std::filesystem::path& rootPath) const -> std::vector<std::string>` — timestamps (filename stems) of every `*.json` file under `<rootPath>/.codeqa/snapshots/`, ascending; an empty vector if the directory doesn't exist. Used by Task 5, Task 6.

- [ ] **Step 1: Write the failing tests**

Create `tests/unit/ListSnapshotsTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/application/ListSnapshots.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace
{

class TempProject
{
public:
    TempProject() : root_(std::filesystem::temp_directory_path() / "codeqa_list_snapshots_test")
    {
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(root_);
    }

    ~TempProject()
    {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    TempProject(const TempProject&) = delete;
    TempProject& operator=(const TempProject&) = delete;

    void writeSnapshotFile(const std::string& name) const
    {
        const std::filesystem::path path = root_ / ".codeqa" / "snapshots" / name;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path) << "{}";
    }

    [[nodiscard]] const std::filesystem::path& root() const { return root_; }

private:
    std::filesystem::path root_;
};

} // namespace

TEST_CASE("ListSnapshots returns an empty vector when .codeqa/snapshots doesn't exist")
{
    const TempProject fixture;

    const std::vector<std::string> timestamps = codeqa::application::ListSnapshots{}.execute(fixture.root());

    CHECK(timestamps.empty());
}

TEST_CASE("ListSnapshots returns timestamps sorted ascending, ignoring non-JSON files")
{
    const TempProject fixture;
    fixture.writeSnapshotFile("20260301T000000Z.json");
    fixture.writeSnapshotFile("20260101T000000Z.json");
    fixture.writeSnapshotFile("20260201T000000Z.json");
    std::ofstream(fixture.root() / ".codeqa" / "snapshots" / "notes.txt") << "not a snapshot";

    const std::vector<std::string> timestamps = codeqa::application::ListSnapshots{}.execute(fixture.root());

    REQUIRE(timestamps.size() == 3);
    CHECK(timestamps[0] == "20260101T000000Z");
    CHECK(timestamps[1] == "20260201T000000Z");
    CHECK(timestamps[2] == "20260301T000000Z");
}
```

Add `unit/ListSnapshotsTest.cpp` to `tests/CMakeLists.txt`, right after `unit/SaveSnapshotTest.cpp`.

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails — `codeqa/application/ListSnapshots.h` does not exist.

- [ ] **Step 3: Create the header and implementation**

Create `include/codeqa/application/ListSnapshots.h`:

```cpp
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace codeqa::application
{

class ListSnapshots
{
public:
    /// Timestamps (filename stems) of every *.json file under
    /// <rootPath>/.codeqa/snapshots/, ascending. Empty if the directory doesn't exist -- not
    /// an error (first run has no history yet).
    [[nodiscard]] std::vector<std::string> execute(const std::filesystem::path& rootPath) const;
};

} // namespace codeqa::application
```

Create `src/application/ListSnapshots.cpp`:

```cpp
#include "codeqa/application/ListSnapshots.h"

#include <algorithm>
#include <system_error>

namespace codeqa::application
{

std::vector<std::string> ListSnapshots::execute(const std::filesystem::path& rootPath) const
{
    const std::filesystem::path snapshotsDirectory = rootPath / ".codeqa" / "snapshots";
    std::vector<std::string> timestamps;

    std::error_code existsError;
    if (!std::filesystem::is_directory(snapshotsDirectory, existsError))
    {
        return timestamps;
    }

    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(snapshotsDirectory))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
        {
            timestamps.push_back(entry.path().stem().string());
        }
    }

    std::sort(timestamps.begin(), timestamps.end());
    return timestamps;
}

} // namespace codeqa::application
```

In `CMakeLists.txt`, add `src/application/ListSnapshots.cpp` to `add_library(codeqa_core STATIC ...)`, right after `src/application/SaveSnapshot.cpp`.

- [ ] **Step 4: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass, including the 2 new ones.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/application/ListSnapshots.h src/application/ListSnapshots.cpp tests/unit/ListSnapshotsTest.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add ListSnapshots to enumerate saved snapshot timestamps"
```

---

### Task 4: `codeqa::application::LoadSnapshot`

**Files:**
- Create: `include/codeqa/application/LoadSnapshot.h`
- Create: `src/application/LoadSnapshot.cpp`
- Create: `tests/unit/LoadSnapshotTest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `codeqa::infrastructure::readFileContents(const std::filesystem::path&) -> std::optional<std::string>` (already exists, `include/codeqa/infrastructure/ReadFileContents.h`, unchanged).
- Produces: `codeqa::application::LoadSnapshot::execute(const std::filesystem::path& rootPath, const std::string& timestamp) const -> std::optional<nlohmann::json>` — reads and parses `<rootPath>/.codeqa/snapshots/<timestamp>.json`; `std::nullopt` if the file doesn't exist or isn't valid JSON (both are "can't use this snapshot" for every caller this phase has). Used by Task 5, Task 6.

- [ ] **Step 1: Write the failing tests**

Create `tests/unit/LoadSnapshotTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/application/LoadSnapshot.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>

namespace
{

class TempProject
{
public:
    TempProject() : root_(std::filesystem::temp_directory_path() / "codeqa_load_snapshot_test")
    {
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(root_ / ".codeqa" / "snapshots");
    }

    ~TempProject()
    {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    TempProject(const TempProject&) = delete;
    TempProject& operator=(const TempProject&) = delete;

    [[nodiscard]] const std::filesystem::path& root() const { return root_; }

private:
    std::filesystem::path root_;
};

} // namespace

TEST_CASE("LoadSnapshot returns the parsed JSON for an existing snapshot")
{
    const TempProject fixture;
    std::ofstream(fixture.root() / ".codeqa" / "snapshots" / "20260101T000000Z.json")
        << R"({ "schemaVersion": 1 })";

    const std::optional<nlohmann::json> result =
        codeqa::application::LoadSnapshot{}.execute(fixture.root(), "20260101T000000Z");

    REQUIRE(result.has_value());
    CHECK(result->at("schemaVersion").get<int>() == 1);
}

TEST_CASE("LoadSnapshot returns nullopt for a timestamp that doesn't exist")
{
    const TempProject fixture;

    const std::optional<nlohmann::json> result =
        codeqa::application::LoadSnapshot{}.execute(fixture.root(), "does-not-exist");

    CHECK_FALSE(result.has_value());
}

TEST_CASE("LoadSnapshot returns nullopt for a file that isn't valid JSON")
{
    const TempProject fixture;
    std::ofstream(fixture.root() / ".codeqa" / "snapshots" / "broken.json") << "{ not json";

    const std::optional<nlohmann::json> result =
        codeqa::application::LoadSnapshot{}.execute(fixture.root(), "broken");

    CHECK_FALSE(result.has_value());
}
```

Add `unit/LoadSnapshotTest.cpp` to `tests/CMakeLists.txt`, right after `unit/ListSnapshotsTest.cpp`.

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails — `codeqa/application/LoadSnapshot.h` does not exist.

- [ ] **Step 3: Create the header and implementation**

Create `include/codeqa/application/LoadSnapshot.h`:

```cpp
#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace codeqa::application
{

class LoadSnapshot
{
public:
    /// nullopt if <rootPath>/.codeqa/snapshots/<timestamp>.json doesn't exist or isn't valid
    /// JSON -- both are "can't use this snapshot" for every caller this phase has.
    [[nodiscard]] std::optional<nlohmann::json> execute(
        const std::filesystem::path& rootPath, const std::string& timestamp) const;
};

} // namespace codeqa::application
```

Create `src/application/LoadSnapshot.cpp`:

```cpp
#include "codeqa/application/LoadSnapshot.h"

#include "codeqa/infrastructure/ReadFileContents.h"

namespace codeqa::application
{

std::optional<nlohmann::json> LoadSnapshot::execute(
    const std::filesystem::path& rootPath, const std::string& timestamp) const
{
    const std::filesystem::path snapshotPath = rootPath / ".codeqa" / "snapshots" / (timestamp + ".json");
    const std::optional<std::string> content = codeqa::infrastructure::readFileContents(snapshotPath);

    if (!content.has_value())
    {
        return std::nullopt;
    }

    try
    {
        return nlohmann::json::parse(*content);
    }
    catch (const nlohmann::json::parse_error&)
    {
        return std::nullopt;
    }
}

} // namespace codeqa::application
```

In `CMakeLists.txt`, add `src/application/LoadSnapshot.cpp` to `add_library(codeqa_core STATIC ...)`, right after `src/application/ListSnapshots.cpp`.

- [ ] **Step 4: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass, including the 3 new ones.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/application/LoadSnapshot.h src/application/LoadSnapshot.cpp tests/unit/LoadSnapshotTest.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add LoadSnapshot to read back a saved snapshot"
```

---

### Task 5: Wire automatic snapshot save + compare into `codeqa analyze`

**Files:**
- Modify: `src/cli/RunApplication.cpp`
- Modify: `tests/unit/RunApplicationTest.cpp`

**Interfaces:**
- Consumes: `codeqa::history::extractFindingKeys`/`compareSnapshots`/`SnapshotComparison` (Task 1), `codeqa::application::SaveSnapshot`/`ListSnapshots`/`LoadSnapshot` (Tasks 2-4), `codeqa::report::toReportJson` (Fase 9, unchanged), `codeqa::application::RunAnalysis`/`RunAnalysisResult` (Fase 9, unchanged).
- Produces: no new public interface — `runAnalyze`'s observable behavior changes: (1) it now always writes a snapshot under `<path>/.codeqa/snapshots/`; (2) when a readable previous snapshot exists, `--format text` output gains a trailing line `"Findings: N new, M existing, P resolved.\n"` and `--format json` output gains a top-level `"comparison": {"previousTimestamp": ..., "new": N, "existing": M, "resolved": P}` field (not persisted to the snapshot file); (3) an unreadable/malformed previous snapshot or a failed snapshot save prints a `"Warning: ..."` line to `errorOutput` but never changes the exit code.

- [ ] **Step 1: Write the failing tests**

In `tests/unit/RunApplicationTest.cpp`, add these two helpers to the existing anonymous namespace (right after `makeTempAnalyzeRoot`):

```cpp
std::filesystem::path makeTempSnapshotRoot()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "codeqa_run_application_snapshot_test";

    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::ofstream(root / "sample.c")
        << "int classify(int a, int b, int c, int d, int e)\n"
           "{\n"
           "    return a + b + c + d + e;\n"
           "}\n";

    return root;
}

void rewriteSnapshotSample(const std::filesystem::path& root)
{
    std::ofstream(root / "sample.c")
        << "int classify(int a, int b)\n"
           "{\n"
           "    if (a == 0) { return 0; }\n"
           "    if (a == 1) { return 1; }\n"
           "    if (a == 2) { return 2; }\n"
           "    return b;\n"
           "}\n";
}
```

`makeTempSnapshotRoot`'s function has 5 int parameters and a trivial body: only `signature.parameter-count` (5 >= warning 4) fires — no branches (cyclomatic stays at 1), one return (under the `complexity.returns` warning of 4), well under every size/nesting/condition threshold. `rewriteSnapshotSample`'s version has 2 parameters (no `signature.parameter-count`) and 4 returns via three `if`s (>= the `complexity.returns` warning of 4; cyclomatic only reaches 4, under its warning of 10) — so going from one file to the other resolves exactly one finding and introduces exactly one different one.

Then add these test cases at the end of the file:

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

    const std::filesystem::path snapshotsDir = root / ".codeqa" / "snapshots";
    REQUIRE(std::filesystem::is_directory(snapshotsDir));

    std::size_t fileCount = 0;
    std::filesystem::path savedFile;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(snapshotsDir))
    {
        ++fileCount;
        savedFile = entry.path();
    }
    CHECK(fileCount == 1);

    std::ifstream savedStream(savedFile);
    const nlohmann::json saved = nlohmann::json::parse(savedStream);
    CHECK(saved.at("schemaVersion").get<int>() == 1);

    std::filesystem::remove_all(root);
}

TEST_CASE("analyze's second run against an unchanged project reports the finding as existing")
{
    const std::filesystem::path root = makeTempSnapshotRoot();

    run({"analyze", root.string()});
    const RunResult second = run({"analyze", root.string()});

    CHECK(second.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(second.output.find("Findings: 0 new, 1 existing, 0 resolved.") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("analyze's second run after a real code change reports one new and one resolved finding")
{
    const std::filesystem::path root = makeTempSnapshotRoot();

    run({"analyze", root.string()});
    rewriteSnapshotSample(root);
    const RunResult second = run({"analyze", root.string(), "--format", "json"});

    CHECK(second.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    const nlohmann::json report = nlohmann::json::parse(second.output);
    REQUIRE(report.contains("comparison"));
    CHECK(report.at("comparison").at("new").get<int>() == 1);
    CHECK(report.at("comparison").at("existing").get<int>() == 0);
    CHECK(report.at("comparison").at("resolved").get<int>() == 1);
    CHECK_FALSE(report.at("comparison").at("previousTimestamp").get<std::string>().empty());

    std::filesystem::remove_all(root);
}

TEST_CASE("analyze warns and skips comparison when the previous snapshot is corrupt")
{
    const std::filesystem::path root = makeTempSnapshotRoot();

    run({"analyze", root.string()});

    const std::filesystem::path snapshotsDir = root / ".codeqa" / "snapshots";
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(snapshotsDir))
    {
        std::ofstream(entry.path()) << "{ not json";
    }

    const RunResult second = run({"analyze", root.string()});

    CHECK(second.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(second.errorOutput.find("could not be read") != std::string::npos);
    CHECK(second.output.find(" new, ") == std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("--help documents that analyze saves and compares snapshots automatically")
{
    const RunResult result = run({"--help"});

    CHECK(result.output.find(".codeqa/snapshots") != std::string::npos);
}
```

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: compiles (all consumed symbols already exist from Tasks 1-4), but the 6 new tests fail — `analyze` doesn't create `.codeqa/snapshots/` or print any comparison text yet.

- [ ] **Step 3: Wire snapshot save + compare into `runAnalyze`**

In `src/cli/RunApplication.cpp`, add these includes right after `#include "codeqa/application/LoadProjectConfig.h"`:

```cpp
#include "codeqa/application/ListSnapshots.h"
#include "codeqa/application/LoadSnapshot.h"
#include "codeqa/application/SaveSnapshot.h"
```

And add this include right after `#include "codeqa/frontend/TreeSitterParser.h"`:

```cpp
#include "codeqa/history/CompareSnapshots.h"
```

Replace the entire `runAnalyze` function body:

```cpp
int runAnalyze(const std::vector<std::string>& arguments, std::ostream& output, std::ostream& errorOutput)
{
    const AnalyzeArgumentsOutcome outcome = parseAnalyzeArguments(arguments, errorOutput);

    if (!outcome.arguments.has_value())
    {
        return outcome.exitCode;
    }

    const AnalyzeArguments& parsedArguments = *outcome.arguments;

    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();

    if (parsedArguments.configPath.has_value())
    {
        const codeqa::application::LoadProjectConfigResult configResult =
            codeqa::application::LoadProjectConfig{}.execute(*parsedArguments.configPath);

        if (!configResult.config.has_value())
        {
            errorOutput << *configResult.error << '\n';
            return static_cast<int>(ExitCode::InvalidArguments);
        }

        config = *configResult.config;
    }

    codeqa::application::RunAnalysisRequest request;
    request.analysis.discovery.rootPath = parsedArguments.path;
    request.analysis.discovery.excludePatterns = parsedArguments.excludePatterns;

    for (const codeqa::config::HeaderOverrideConfig& headerOverride : config.headerOverrides)
    {
        request.analysis.languageOverrides.push_back(toLanguageOverride(headerOverride));
    }

    for (const codeqa::application::LanguageOverride& languageOverride : parsedArguments.languageOverrides)
    {
        request.analysis.languageOverrides.push_back(languageOverride);
    }

    request.config = config;

    const codeqa::application::RunAnalysisResult result = codeqa::application::RunAnalysis{}.execute(request);

    const codeqa::application::GetApplicationInfo getApplicationInfo;
    const std::string toolVersion = getApplicationInfo.execute().version;
    const nlohmann::json reportJson =
        codeqa::report::toReportJson(result.model, result.metrics, result.findings, config, toolVersion);

    std::ofstream outputFile;
    std::ostream* destination = &output;

    if (parsedArguments.outputPath.has_value())
    {
        outputFile.open(*parsedArguments.outputPath);

        if (!outputFile)
        {
            errorOutput << "Error [CLI012]: Could not open '" << parsedArguments.outputPath->string()
                           << "' for writing.\n";
            return static_cast<int>(ExitCode::InvalidArguments);
        }

        destination = &outputFile;
    }

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

    if (parsedArguments.format == "json")
    {
        nlohmann::json displayJson = reportJson;

        if (comparison.has_value())
        {
            displayJson["comparison"] = {
                {"previousTimestamp", previousTimestamp},
                {"new", comparison->newCount},
                {"existing", comparison->existingCount},
                {"resolved", comparison->resolvedCount}};
        }

        *destination << displayJson.dump(2) << '\n';
    }
    else
    {
        codeqa::report::renderText(*destination, result.model, result.metrics, result.findings);

        if (comparison.has_value())
        {
            *destination << "\nFindings: " << comparison->newCount << " new, " << comparison->existingCount
                         << " existing, " << comparison->resolvedCount << " resolved.\n";
        }
    }

    if (!codeqa::application::SaveSnapshot{}.execute(parsedArguments.path, reportJson).has_value())
    {
        errorOutput << "Warning: could not save snapshot.\n";
    }

    return static_cast<int>(ExitCode::Success);
}
```

This removes the old `if (parsedArguments.format == "json") { const GetApplicationInfo...; const nlohmann::json report = toReportJson(...); *destination << report.dump(2) << '\n'; } else { renderText(...); }` block (now unconditional `reportJson` computation up front, and the two branches only add the `comparison` bits) — everything else in the function keeps its existing order (`--config` handling, `RunAnalysisRequest` construction, `--output` opening and its `CLI012` check).

Now update `printHelp` to document the new automatic behavior. Replace:

```cpp
              "  analyze <path>                     Run the full analysis pipeline (metrics +\n"
              "                                      rules) and print a report.\n"
              "      --config <path>                 Merge rule/language overrides from a\n"
              "                                       codeqa.json file over the defaults.\n"
              "      --format text|json               Report format (default: text).\n"
              "      --output <file>                  Write the report to a file instead of\n"
              "                                        stdout.\n";
```

with:

```cpp
              "  analyze <path>                     Run the full analysis pipeline (metrics +\n"
              "                                      rules) and print a report. Also saves a\n"
              "                                      snapshot under <path>/.codeqa/snapshots/\n"
              "                                      and compares it against the previous one\n"
              "                                      automatically.\n"
              "      --config <path>                 Merge rule/language overrides from a\n"
              "                                       codeqa.json file over the defaults.\n"
              "      --format text|json               Report format (default: text).\n"
              "      --output <file>                  Write the report to a file instead of\n"
              "                                        stdout.\n";
```

- [ ] **Step 4: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass, including the 6 new ones and every pre-existing `analyze` test (their assertions are unaffected — `.codeqa/snapshots/` writes are a new side effect they don't check for).

- [ ] **Step 5: Commit**

```bash
git add src/cli/RunApplication.cpp tests/unit/RunApplicationTest.cpp
git commit -m "feat: analyze saves and compares snapshots automatically"
```

---

### Task 6: `codeqa snapshot list|show` command

**Files:**
- Modify: `src/cli/RunApplication.cpp`
- Modify: `tests/unit/RunApplicationTest.cpp`

**Interfaces:**
- Consumes: `codeqa::application::ListSnapshots`/`LoadSnapshot` (Tasks 3, 4).
- Produces: `codeqa snapshot list <path>` (one timestamp per line, ascending) and `codeqa snapshot show <path> <timestamp>` (the snapshot's JSON, pretty-printed) as new top-level CLI verbs. New error codes `CLI013`-`CLI016` (see Global Constraints).

- [ ] **Step 1: Write the failing tests**

Add these test cases to the end of `tests/unit/RunApplicationTest.cpp`:

```cpp
TEST_CASE("snapshot without a subcommand returns CLI013")
{
    const RunResult result = run({"snapshot"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI013") != std::string::npos);
}

TEST_CASE("snapshot with an unknown subcommand returns CLI014")
{
    const RunResult result = run({"snapshot", "delete"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI014") != std::string::npos);
}

TEST_CASE("snapshot list without a path returns CLI002")
{
    const RunResult result = run({"snapshot", "list"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI002") != std::string::npos);
}

TEST_CASE("snapshot list with a nonexistent path returns CLI003")
{
    const RunResult result = run({"snapshot", "list", "this/path/does/not/exist"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI003") != std::string::npos);
}

TEST_CASE("snapshot show without a timestamp returns CLI015")
{
    const std::filesystem::path root = makeTempSnapshotRoot();

    const RunResult result = run({"snapshot", "show", root.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI015") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("snapshot show with an unknown timestamp returns CLI016")
{
    const std::filesystem::path root = makeTempSnapshotRoot();

    const RunResult result = run({"snapshot", "show", root.string(), "20000101T000000Z"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI016") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("snapshot list prints the timestamps saved by analyze")
{
    const std::filesystem::path root = makeTempSnapshotRoot();
    run({"analyze", root.string()});

    const RunResult result = run({"snapshot", "list", root.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    const std::string firstLine = result.output.substr(0, result.output.find('\n'));
    CHECK(firstLine.size() == 16); // YYYYMMDDTHHMMSSZ
    CHECK(result.output.find('\n') == firstLine.size());

    std::filesystem::remove_all(root);
}

TEST_CASE("snapshot show prints the exact JSON of a saved snapshot")
{
    const std::filesystem::path root = makeTempSnapshotRoot();
    run({"analyze", root.string()});

    const RunResult listResult = run({"snapshot", "list", root.string()});
    const std::string timestamp = listResult.output.substr(0, listResult.output.find('\n'));

    const RunResult showResult = run({"snapshot", "show", root.string(), timestamp});

    CHECK(showResult.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    const nlohmann::json snapshot = nlohmann::json::parse(showResult.output);
    CHECK(snapshot.at("schemaVersion").get<int>() == 1);

    std::filesystem::remove_all(root);
}

TEST_CASE("--help documents the snapshot subcommand")
{
    const RunResult result = run({"--help"});

    CHECK(result.output.find("codeqa snapshot list") != std::string::npos);
    CHECK(result.output.find("codeqa snapshot show") != std::string::npos);
}
```

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails to compile / fails at runtime — `snapshot` isn't a recognized subcommand yet (falls through to the `CLI001` "Unknown option" branch instead of `CLI013`/etc.).

- [ ] **Step 3: Add the `snapshot` subcommand**

In `src/cli/RunApplication.cpp`, add this new code in the anonymous namespace, right after `runAnalyze` (before the closing `} // namespace`):

```cpp
struct SnapshotArguments
{
    std::string verb;
    std::filesystem::path path;
    std::optional<std::string> timestamp;
};

struct SnapshotArgumentsOutcome
{
    std::optional<SnapshotArguments> arguments;
    int exitCode{static_cast<int>(ExitCode::Success)};
};

SnapshotArgumentsOutcome parseSnapshotArguments(
    const std::vector<std::string>& arguments, std::ostream& errorOutput)
{
    if (arguments.size() < 2)
    {
        errorOutput << "Error [CLI013]: Missing subcommand ('list' or 'show') for 'snapshot'.\n"
                       "Use 'codeqa --help' for usage.\n";
        return SnapshotArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
    }

    const std::string& verb = arguments[1];

    if (verb != "list" && verb != "show")
    {
        errorOutput << "Error [CLI014]: Unknown snapshot subcommand '" << verb
                       << "'. Expected 'list' or 'show'.\n";
        return SnapshotArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
    }

    if (arguments.size() < 3)
    {
        errorOutput << "Error [CLI002]: Missing required argument <path> for 'snapshot " << verb << "'.\n"
                       "Use 'codeqa --help' for usage.\n";
        return SnapshotArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
    }

    const std::filesystem::path path{arguments[2]};

    std::error_code existsError;
    if (!std::filesystem::is_directory(path, existsError))
    {
        errorOutput << "Error [CLI003]: Path '" << path.string() << "' does not exist or is not a directory.\n";
        return SnapshotArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
    }

    std::optional<std::string> timestamp;

    if (verb == "show")
    {
        if (arguments.size() < 4)
        {
            errorOutput << "Error [CLI015]: Missing required argument <timestamp> for 'snapshot show'.\n";
            return SnapshotArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
        }

        timestamp = arguments[3];
    }

    const std::size_t expectedArgumentCount = (verb == "show") ? 4 : 3;

    if (arguments.size() > expectedArgumentCount)
    {
        errorOutput << "Error [CLI005]: Unexpected argument '" << arguments[expectedArgumentCount] << "'.\n";
        return SnapshotArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
    }

    return SnapshotArgumentsOutcome{
        SnapshotArguments{verb, path, timestamp}, static_cast<int>(ExitCode::Success)};
}

int runSnapshot(const std::vector<std::string>& arguments, std::ostream& output, std::ostream& errorOutput)
{
    const SnapshotArgumentsOutcome outcome = parseSnapshotArguments(arguments, errorOutput);

    if (!outcome.arguments.has_value())
    {
        return outcome.exitCode;
    }

    const SnapshotArguments& parsedArguments = *outcome.arguments;

    if (parsedArguments.verb == "list")
    {
        for (const std::string& timestamp : codeqa::application::ListSnapshots{}.execute(parsedArguments.path))
        {
            output << timestamp << '\n';
        }

        return static_cast<int>(ExitCode::Success);
    }

    const std::optional<nlohmann::json> snapshot =
        codeqa::application::LoadSnapshot{}.execute(parsedArguments.path, *parsedArguments.timestamp);

    if (!snapshot.has_value())
    {
        errorOutput << "Error [CLI016]: Snapshot '" << *parsedArguments.timestamp << "' not found for '"
                       << parsedArguments.path.string() << "'.\n";
        return static_cast<int>(ExitCode::InvalidArguments);
    }

    output << snapshot->dump(2) << '\n';
    return static_cast<int>(ExitCode::Success);
}
```

Wire the dispatch in `runApplication`, right after the `analyze` branch:

```cpp
    if (firstArgument == "snapshot")
    {
        return runSnapshot(arguments, output, errorOutput);
    }
```

Update `printHelp`'s usage block — replace:

```cpp
              "  codeqa analyze <path> [--exclude <glob>]... [--lang-override <pattern>=<lang>]...\n"
              "                        [--config <path>] [--format text|json] [--output <file>]\n"
              "\n"
```

with:

```cpp
              "  codeqa analyze <path> [--exclude <glob>]... [--lang-override <pattern>=<lang>]...\n"
              "                        [--config <path>] [--format text|json] [--output <file>]\n"
              "  codeqa snapshot list <path>\n"
              "  codeqa snapshot show <path> <timestamp>\n"
              "\n"
```

And extend the `Commands:` section — replace the closing:

```cpp
              "      --output <file>                  Write the report to a file instead of\n"
              "                                        stdout.\n";
```

with:

```cpp
              "      --output <file>                  Write the report to a file instead of\n"
              "                                        stdout.\n"
              "  snapshot list <path>               List saved snapshot timestamps for <path>,\n"
              "                                      oldest first.\n"
              "  snapshot show <path> <timestamp>   Print the JSON of one saved snapshot.\n";
```

(This is the same block Task 5 already edited to mention automatic snapshot saving — both edits land in the same `printHelp` function, this one appends after it.)

- [ ] **Step 4: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass, including the 9 new ones.

- [ ] **Step 5: Commit**

```bash
git add src/cli/RunApplication.cpp tests/unit/RunApplicationTest.cpp
git commit -m "feat: add codeqa snapshot list|show command"
```

---

## Post-plan check

After Task 6, re-read `docs/superpowers/specs/2026-08-12-fase-10-historico-minimo-design.md` §10 (Criterios de aceptación) against the finished code: every `analyze` run saves a snapshot (Task 5); the snapshot is `toReportJson`'s own schema, untouched (Task 2, `reportJson` passed to `SaveSnapshot` verbatim); comparing two consecutive snapshots classifies new/existing/resolved correctly (Task 5's "real code change" test); no previous snapshot skips the comparison without error (Task 5's "first run" test), and a corrupt one does too (Task 5's "corrupt" test) with a warning instead of a failure.
