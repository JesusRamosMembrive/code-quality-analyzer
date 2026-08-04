# Fase 2 — Descubrimiento de archivos fuente Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a `codeqa::infrastructure` layer that recursively discovers C/C++ source files under a root directory (extension classification, fixed default exclusions, `--exclude` glob patterns, deterministic order, per-file access diagnostics), wrapped by a thin `codeqa::application` use case and exposed as a real CLI subcommand: `codeqa discover <path> [--exclude <glob>]...`.

**Architecture:** `codeqa::infrastructure::discoverSourceFiles(DiscoveryOptions) -> DiscoveryResult` walks the filesystem with `std::filesystem::recursive_directory_iterator`, pruning excluded directories instead of descending into them. `codeqa::application::DiscoverProjectSources` wraps it with zero added logic (same pattern as `GetApplicationInfo`). `RunApplication` validates the root path and parses `--exclude` flags before calling the use case, then formats the text summary. No file content is read in this phase — only paths and metadata.

**Tech Stack:** C++17, `std::filesystem`, `std::regex` (minimal glob→regex translation), doctest, CTest — no new external dependencies.

## Global Constraints

These carry forward from Fase 1 (`docs/superpowers/plans/2026-08-02-fase-1-fundacion-tecnica.md`) and remain binding:

- C++17 standard, no compiler-specific extensions.
- `codeqa_core` must not depend on Qt in any form. Standard library only.
- No new external dependencies of any kind (no `FetchContent`, no submodules, no new vendored libraries) — this phase uses only `std::filesystem` and `std::regex`.
- All project code lives inside `namespace codeqa { ... }`, with subnamespaces `codeqa::core`, `codeqa::application`, `codeqa::cli`, and now `codeqa::infrastructure`. Never `using namespace std;`.
- Naming: classes/structs `PascalCase`; functions/methods `camelCase`; local variables `camelCase`; namespaces lowercase; class files `PascalCase.h`/`.cpp`.
- Use `${PROJECT_SOURCE_DIR}` / `${PROJECT_BINARY_DIR}` in CMake, not `CMAKE_SOURCE_DIR` / `CMAKE_BINARY_DIR`.

These are new, from `docs/superpowers/specs/2026-08-04-fase-2-source-discovery-design.md` (the approved design for this phase — read it if any task feels ambiguous):

- **Discovery only, no content loading.** `discoverSourceFiles` never opens a file for reading its contents — only path, extension, and size.
- **Fixed default directory exclusions**, hardcoded, no config file lookup: any directory name starting with `.`, the literal name `build`, and any name starting with `cmake-build-`.
- **`--exclude <glob>` is a repeatable CLI flag** in this phase (not deferred to a future config phase).
- **`discover` is a real, working CLI subcommand** by the end of this phase, not just a library.
- **Symlinked directories are never descended into.** This falls out of using `recursive_directory_iterator` without `directory_options::follow_directory_symlink` — do not add extra symlink-detection code for this.
- **No file size limit, empty files included normally.**
- **Per-file access errors become `codeqa::core::Diagnostic`s** appended to the result; they never abort the walk.
- **`excludedCount` counts entries at the point of exclusion** — an excluded directory counts once, not once per file it would have contained (this is a deliberate simplification of the pruning strategy; do not try to recursively count files inside excluded directories).
- **A nonexistent or non-directory root path is a CLI-level error** (`ExitCode::InvalidArguments`), validated in `RunApplication` *before* the use case is called — `discoverSourceFiles` itself assumes its `rootPath` argument already exists and is a directory.

---

## File Structure

```text
include/codeqa/infrastructure/
├── ExtensionCategory.h        (enum class: C, Cpp, AmbiguousHeader)
├── ClassifyExtension.h        (pure function: extension string -> ExtensionCategory)
├── DefaultExclusions.h        (pure function: directory name -> excluded by default?)
├── DiscoveredFile.h           (struct)
├── DiscoveryOptions.h         (struct)
├── DiscoveryResult.h          (struct)
├── GlobMatcher.h               (glob pattern <-> path matching, for --exclude)
└── DiscoverSourceFiles.h      (the traversal engine)

src/infrastructure/
├── ClassifyExtension.cpp
├── DefaultExclusions.cpp
├── GlobMatcher.cpp
└── DiscoverSourceFiles.cpp

include/codeqa/application/
└── DiscoverProjectSources.h   (use case: delegates to infrastructure, no logic of its own)

src/application/
└── DiscoverProjectSources.cpp

src/cli/
└── RunApplication.cpp         (modified: adds the `discover` subcommand)

tests/unit/
├── InfrastructureModelsTest.cpp   (classifyExtension, isDefaultExcludedDirectoryName, struct defaults)
├── GlobMatcherTest.cpp
├── DiscoverSourceFilesTest.cpp
├── DiscoverProjectSourcesTest.cpp
└── RunApplicationTest.cpp         (modified: adds discover CLI cases)

tests/integration/
└── fixtures/
    └── discover-sample/
        ├── main.c
        ├── util.cpp
        ├── util.h
        └── build/
            └── generated.cpp

CMakeLists.txt          (modified: new codeqa_core sources)
tests/CMakeLists.txt    (modified: new test sources, new integration test)
README.md               (modified: documents `discover`)
```

---

### Task 1: Infrastructure data model and extension/exclusion rules

**Files:**
- Create: `include/codeqa/infrastructure/ExtensionCategory.h`
- Create: `include/codeqa/infrastructure/ClassifyExtension.h`
- Create: `src/infrastructure/ClassifyExtension.cpp`
- Create: `include/codeqa/infrastructure/DefaultExclusions.h`
- Create: `src/infrastructure/DefaultExclusions.cpp`
- Create: `include/codeqa/infrastructure/DiscoveredFile.h`
- Create: `include/codeqa/infrastructure/DiscoveryOptions.h`
- Create: `include/codeqa/infrastructure/DiscoveryResult.h`
- Create: `tests/unit/InfrastructureModelsTest.cpp`
- Modify: `CMakeLists.txt` (add the two new `.cpp` files to `codeqa_core`)
- Modify: `tests/CMakeLists.txt` (add the new test file)

**Interfaces:**
- Consumes: `codeqa::core::Diagnostic` (from Fase 1, `include/codeqa/core/Diagnostic.h`).
- Produces: `codeqa::infrastructure::ExtensionCategory{C, Cpp, AmbiguousHeader}`, `classifyExtension(const std::string&) -> std::optional<ExtensionCategory>`, `isDefaultExcludedDirectoryName(const std::string&) -> bool`, `DiscoveredFile{path, category, sizeBytes}`, `DiscoveryOptions{rootPath, excludePatterns}`, `DiscoveryResult{files, diagnostics, excludedCount}`. Tasks 2–5 build directly on these exact names.

- [ ] **Step 1: Write the failing test**

`tests/unit/InfrastructureModelsTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/infrastructure/ClassifyExtension.h"
#include "codeqa/infrastructure/DefaultExclusions.h"
#include "codeqa/infrastructure/DiscoveryResult.h"

#include <string>

TEST_CASE("classifyExtension recognizes the C extension")
{
    const auto category = codeqa::infrastructure::classifyExtension(".c");

    REQUIRE(category.has_value());
    CHECK(*category == codeqa::infrastructure::ExtensionCategory::C);
}

TEST_CASE("classifyExtension recognizes C++ extensions")
{
    for (const std::string& extension : {".cc", ".cpp", ".cxx", ".hpp", ".hh", ".hxx"})
    {
        const auto category = codeqa::infrastructure::classifyExtension(extension);

        REQUIRE(category.has_value());
        CHECK(*category == codeqa::infrastructure::ExtensionCategory::Cpp);
    }
}

TEST_CASE("classifyExtension treats .h as an ambiguous header")
{
    const auto category = codeqa::infrastructure::classifyExtension(".h");

    REQUIRE(category.has_value());
    CHECK(*category == codeqa::infrastructure::ExtensionCategory::AmbiguousHeader);
}

TEST_CASE("classifyExtension returns nullopt for unsupported extensions")
{
    CHECK_FALSE(codeqa::infrastructure::classifyExtension(".py").has_value());
    CHECK_FALSE(codeqa::infrastructure::classifyExtension(".md").has_value());
    CHECK_FALSE(codeqa::infrastructure::classifyExtension("").has_value());
}

TEST_CASE("isDefaultExcludedDirectoryName excludes hidden directories")
{
    CHECK(codeqa::infrastructure::isDefaultExcludedDirectoryName(".git"));
    CHECK(codeqa::infrastructure::isDefaultExcludedDirectoryName(".worktrees"));
    CHECK(codeqa::infrastructure::isDefaultExcludedDirectoryName(".anything"));
}

TEST_CASE("isDefaultExcludedDirectoryName excludes build directories")
{
    CHECK(codeqa::infrastructure::isDefaultExcludedDirectoryName("build"));
    CHECK(codeqa::infrastructure::isDefaultExcludedDirectoryName("cmake-build-debug"));
    CHECK(codeqa::infrastructure::isDefaultExcludedDirectoryName("cmake-build-release"));
}

TEST_CASE("isDefaultExcludedDirectoryName allows ordinary directories")
{
    CHECK_FALSE(codeqa::infrastructure::isDefaultExcludedDirectoryName("src"));
    CHECK_FALSE(codeqa::infrastructure::isDefaultExcludedDirectoryName("include"));
    CHECK_FALSE(codeqa::infrastructure::isDefaultExcludedDirectoryName("builder"));
}

TEST_CASE("DiscoveryResult default-constructs empty")
{
    const codeqa::infrastructure::DiscoveryResult result;

    CHECK(result.files.empty());
    CHECK(result.diagnostics.empty());
    CHECK(result.excludedCount == 0);
}
```

Add it to `tests/CMakeLists.txt` (append to the existing `add_executable(codeqa_tests ...)` source list):

```cmake
add_executable(codeqa_tests
    main.cpp
    unit/SmokeTest.cpp
    unit/CoreModelsTest.cpp
    unit/GetApplicationInfoTest.cpp
    unit/RunApplicationTest.cpp
    unit/InfrastructureModelsTest.cpp
)
```

- [ ] **Step 2: Run it to verify it fails**

```bash
cmake -S . -B build
cmake --build build
```

Expected: FAIL — `codeqa/infrastructure/ClassifyExtension.h: No such file or directory` (none of the headers exist yet).

- [ ] **Step 3: Implement the headers and sources**

`include/codeqa/infrastructure/ExtensionCategory.h`:

```cpp
#pragma once

namespace codeqa::infrastructure
{

enum class ExtensionCategory
{
    C,
    Cpp,
    AmbiguousHeader
};

} // namespace codeqa::infrastructure
```

`include/codeqa/infrastructure/ClassifyExtension.h`:

```cpp
#pragma once

#include "codeqa/infrastructure/ExtensionCategory.h"

#include <optional>
#include <string>

namespace codeqa::infrastructure
{

[[nodiscard]] std::optional<ExtensionCategory> classifyExtension(const std::string& extension);

} // namespace codeqa::infrastructure
```

`src/infrastructure/ClassifyExtension.cpp`:

```cpp
#include "codeqa/infrastructure/ClassifyExtension.h"

namespace codeqa::infrastructure
{

std::optional<ExtensionCategory> classifyExtension(const std::string& extension)
{
    if (extension == ".c")
    {
        return ExtensionCategory::C;
    }

    if (extension == ".cc" || extension == ".cpp" || extension == ".cxx"
        || extension == ".hpp" || extension == ".hh" || extension == ".hxx")
    {
        return ExtensionCategory::Cpp;
    }

    if (extension == ".h")
    {
        return ExtensionCategory::AmbiguousHeader;
    }

    return std::nullopt;
}

} // namespace codeqa::infrastructure
```

`include/codeqa/infrastructure/DefaultExclusions.h`:

```cpp
#pragma once

#include <string>

namespace codeqa::infrastructure
{

[[nodiscard]] bool isDefaultExcludedDirectoryName(const std::string& directoryName);

} // namespace codeqa::infrastructure
```

`src/infrastructure/DefaultExclusions.cpp`:

```cpp
#include "codeqa/infrastructure/DefaultExclusions.h"

namespace codeqa::infrastructure
{

bool isDefaultExcludedDirectoryName(const std::string& directoryName)
{
    if (!directoryName.empty() && directoryName.front() == '.')
    {
        return true;
    }

    if (directoryName == "build")
    {
        return true;
    }

    if (directoryName.rfind("cmake-build-", 0) == 0)
    {
        return true;
    }

    return false;
}

} // namespace codeqa::infrastructure
```

`include/codeqa/infrastructure/DiscoveredFile.h`:

```cpp
#pragma once

#include "codeqa/infrastructure/ExtensionCategory.h"

#include <cstdint>
#include <filesystem>

namespace codeqa::infrastructure
{

struct DiscoveredFile
{
    std::filesystem::path path;
    ExtensionCategory category{ExtensionCategory::C};
    std::uintmax_t sizeBytes{0};
};

} // namespace codeqa::infrastructure
```

`include/codeqa/infrastructure/DiscoveryOptions.h`:

```cpp
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace codeqa::infrastructure
{

struct DiscoveryOptions
{
    std::filesystem::path rootPath;
    std::vector<std::string> excludePatterns;
};

} // namespace codeqa::infrastructure
```

`include/codeqa/infrastructure/DiscoveryResult.h`:

```cpp
#pragma once

#include "codeqa/core/Diagnostic.h"
#include "codeqa/infrastructure/DiscoveredFile.h"

#include <cstddef>
#include <vector>

namespace codeqa::infrastructure
{

struct DiscoveryResult
{
    std::vector<DiscoveredFile> files;
    std::vector<codeqa::core::Diagnostic> diagnostics;
    std::size_t excludedCount{0};
};

} // namespace codeqa::infrastructure
```

Add the two new `.cpp` files to `CMakeLists.txt`'s `codeqa_core` sources:

```cmake
add_library(codeqa_core STATIC
    src/application/GetApplicationInfo.cpp
    src/cli/RunApplication.cpp
    src/infrastructure/ClassifyExtension.cpp
    src/infrastructure/DefaultExclusions.cpp
)
```

- [ ] **Step 4: Run the tests to verify they pass**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: build succeeds, all tests pass including the new `InfrastructureModelsTest` cases.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/infrastructure/ExtensionCategory.h include/codeqa/infrastructure/ClassifyExtension.h src/infrastructure/ClassifyExtension.cpp include/codeqa/infrastructure/DefaultExclusions.h src/infrastructure/DefaultExclusions.cpp include/codeqa/infrastructure/DiscoveredFile.h include/codeqa/infrastructure/DiscoveryOptions.h include/codeqa/infrastructure/DiscoveryResult.h tests/unit/InfrastructureModelsTest.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add infrastructure data model and extension/exclusion rules"
```

---

### Task 2: Glob pattern matcher for `--exclude`

**Files:**
- Create: `include/codeqa/infrastructure/GlobMatcher.h`
- Create: `src/infrastructure/GlobMatcher.cpp`
- Create: `tests/unit/GlobMatcherTest.cpp`
- Modify: `CMakeLists.txt` (add `GlobMatcher.cpp`)
- Modify: `tests/CMakeLists.txt` (add the new test file)

**Interfaces:**
- Consumes: nothing from Task 1.
- Produces: `codeqa::infrastructure::matchesExcludePattern(const std::string& pattern, const std::filesystem::path& relativePath) -> bool`. Task 3 calls this once per candidate path against every pattern in `DiscoveryOptions::excludePatterns`.

**Matching semantics this task must implement exactly** (this is the "minimalist glob→regex translator" decision from the spec):
- `*` matches any run of characters except `/` (translates to regex `[^/]*`).
- `?` matches exactly one character except `/` (translates to regex `[^/]`).
- All other characters are literal; regex metacharacters (`. ^ $ | ( ) [ ] { } +`) are escaped so they match themselves.
- If the pattern contains a `/`, it is matched against the **whole relative path** (`relativePath.generic_string()`, forward slashes, no leading slash).
- If the pattern contains no `/`, it is matched against the **filename only** (`relativePath.filename().generic_string()`), so it applies at any depth.
- The match is a full match (anchored at both ends), not a substring search.

- [ ] **Step 1: Write the failing test**

`tests/unit/GlobMatcherTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/infrastructure/GlobMatcher.h"

using codeqa::infrastructure::matchesExcludePattern;

TEST_CASE("a pattern without a slash matches the filename at any depth")
{
    CHECK(matchesExcludePattern("*_generated.cpp", "foo_generated.cpp"));
    CHECK(matchesExcludePattern("*_generated.cpp", "src/nested/foo_generated.cpp"));
    CHECK_FALSE(matchesExcludePattern("*_generated.cpp", "foo_generated.hpp"));
}

TEST_CASE("a pattern with a slash matches the full relative path")
{
    CHECK(matchesExcludePattern("*/vendor/*", "src/vendor/foo.cpp"));
    CHECK_FALSE(matchesExcludePattern("*/vendor/*", "vendor/foo.cpp"));
    CHECK_FALSE(matchesExcludePattern("*/vendor/*", "src/vendor/nested/foo.cpp"));
}

TEST_CASE("? matches exactly one character")
{
    CHECK(matchesExcludePattern("file?.cpp", "file1.cpp"));
    CHECK_FALSE(matchesExcludePattern("file?.cpp", "file12.cpp"));
    CHECK_FALSE(matchesExcludePattern("file?.cpp", "file.cpp"));
}

TEST_CASE("literal characters match exactly, including regex-special ones")
{
    CHECK(matchesExcludePattern("a.b.cpp", "a.b.cpp"));
    CHECK_FALSE(matchesExcludePattern("a.b.cpp", "aXbXcpp"));
}
```

Add it to `tests/CMakeLists.txt`'s source list (same pattern as Task 1's addition).

- [ ] **Step 2: Run it to verify it fails**

```bash
cmake -S . -B build
cmake --build build
```

Expected: FAIL — `codeqa/infrastructure/GlobMatcher.h: No such file or directory`.

- [ ] **Step 3: Implement the matcher**

`include/codeqa/infrastructure/GlobMatcher.h`:

```cpp
#pragma once

#include <filesystem>
#include <string>

namespace codeqa::infrastructure
{

[[nodiscard]] bool matchesExcludePattern(const std::string& pattern, const std::filesystem::path& relativePath);

} // namespace codeqa::infrastructure
```

`src/infrastructure/GlobMatcher.cpp`:

```cpp
#include "codeqa/infrastructure/GlobMatcher.h"

#include <regex>

namespace codeqa::infrastructure
{

namespace
{

std::string translateGlobToRegex(const std::string& pattern)
{
    static const std::string kRegexSpecialChars = R"(\.^$|()[]{}+)";

    std::string regexPattern;
    regexPattern.reserve(pattern.size() * 2);

    for (const char character : pattern)
    {
        if (character == '*')
        {
            regexPattern += "[^/]*";
        }
        else if (character == '?')
        {
            regexPattern += "[^/]";
        }
        else if (kRegexSpecialChars.find(character) != std::string::npos)
        {
            regexPattern += '\\';
            regexPattern += character;
        }
        else
        {
            regexPattern += character;
        }
    }

    return regexPattern;
}

} // namespace

bool matchesExcludePattern(const std::string& pattern, const std::filesystem::path& relativePath)
{
    const bool matchWholePath = pattern.find('/') != std::string::npos;
    const std::string target = matchWholePath
        ? relativePath.generic_string()
        : relativePath.filename().generic_string();

    const std::regex compiledPattern("^" + translateGlobToRegex(pattern) + "$");

    return std::regex_match(target, compiledPattern);
}

} // namespace codeqa::infrastructure
```

Add `src/infrastructure/GlobMatcher.cpp` to `CMakeLists.txt`'s `codeqa_core` sources.

- [ ] **Step 4: Run the tests to verify they pass**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: all tests pass, including the new `GlobMatcherTest` cases.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/infrastructure/GlobMatcher.h src/infrastructure/GlobMatcher.cpp tests/unit/GlobMatcherTest.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add minimalist glob matcher for --exclude patterns"
```

---

### Task 3: Source file discovery engine

**Files:**
- Create: `include/codeqa/infrastructure/DiscoverSourceFiles.h`
- Create: `src/infrastructure/DiscoverSourceFiles.cpp`
- Create: `tests/unit/DiscoverSourceFilesTest.cpp`
- Modify: `CMakeLists.txt` (add `DiscoverSourceFiles.cpp`)
- Modify: `tests/CMakeLists.txt` (add the new test file)

**Interfaces:**
- Consumes: `ExtensionCategory`, `classifyExtension`, `isDefaultExcludedDirectoryName`, `DiscoveredFile`, `DiscoveryOptions`, `DiscoveryResult` (Task 1); `matchesExcludePattern` (Task 2); `codeqa::core::Diagnostic`, `codeqa::core::DiagnosticSeverity`, `codeqa::core::SourceLocation` (Fase 1).
- Produces: `codeqa::infrastructure::discoverSourceFiles(const DiscoveryOptions& options) -> DiscoveryResult`. **Precondition:** `options.rootPath` already exists and is a directory — the caller (Task 5's `RunApplication`) is responsible for that check. Task 4's `DiscoverProjectSources` calls this directly with no added logic.

**A note on testing access errors:** simulating a genuine OS-level "permission denied" file portably inside an automated test is unreliable across platforms (Windows ACLs vs. POSIX permission bits, and CI users are frequently unprivileged either way). This task's tests therefore verify the *behavior the code guarantees by construction* — every `std::filesystem` call in the engine that can fail is guarded with an `std::error_code` overload and turned into a `Diagnostic` instead of an exception, so a failure at any single entry cannot abort the walk. Do not attempt to force a real permission-denied file in the test; if you want extra confidence, step through the error-handling branches in a debugger or add a temporary throwing stub during development, but remove it before committing.

**A note on the symlink test:** creating a directory symlink on Windows can fail with "a required privilege is not held by the client" unless Developer Mode is enabled. The test below must detect this and skip gracefully (via `MESSAGE` + early `return`) rather than fail the suite — do not weaken this into skipping silently on all platforms, only on the specific privilege error.

- [ ] **Step 1: Write the failing test**

`tests/unit/DiscoverSourceFilesTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/infrastructure/DiscoverSourceFiles.h"

#include <filesystem>
#include <fstream>
#include <set>
#include <vector>

namespace
{

class TempDirectory
{
public:
    TempDirectory()
        : path_(std::filesystem::temp_directory_path() / "codeqa_discover_source_files_test")
    {
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }

    ~TempDirectory()
    {
        std::filesystem::remove_all(path_);
    }

    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const
    {
        return path_;
    }

    void createFile(const std::filesystem::path& relativePath, const std::string& content = "") const
    {
        const std::filesystem::path fullPath = path_ / relativePath;
        std::filesystem::create_directories(fullPath.parent_path());
        std::ofstream(fullPath) << content;
    }

private:
    std::filesystem::path path_;
};

std::set<std::filesystem::path> discoveredPaths(const codeqa::infrastructure::DiscoveryResult& result)
{
    std::set<std::filesystem::path> paths;

    for (const codeqa::infrastructure::DiscoveredFile& file : result.files)
    {
        paths.insert(file.path);
    }

    return paths;
}

} // namespace

TEST_CASE("discoverSourceFiles finds supported files and classifies them by extension")
{
    TempDirectory root;
    root.createFile("main.c");
    root.createFile("util.cpp");
    root.createFile("util.h");
    root.createFile("notes.txt");

    codeqa::infrastructure::DiscoveryOptions options;
    options.rootPath = root.path();

    const codeqa::infrastructure::DiscoveryResult result = codeqa::infrastructure::discoverSourceFiles(options);

    REQUIRE(result.files.size() == 3);
    CHECK(discoveredPaths(result)
        == std::set<std::filesystem::path>{
            std::filesystem::path{"main.c"},
            std::filesystem::path{"util.cpp"},
            std::filesystem::path{"util.h"}});
    CHECK(result.diagnostics.empty());
}

TEST_CASE("discoverSourceFiles excludes default directories without descending into them")
{
    TempDirectory root;
    root.createFile("main.c");
    root.createFile(".git/HEAD");
    root.createFile("build/generated.cpp");
    root.createFile("cmake-build-debug/generated.cpp");

    codeqa::infrastructure::DiscoveryOptions options;
    options.rootPath = root.path();

    const codeqa::infrastructure::DiscoveryResult result = codeqa::infrastructure::discoverSourceFiles(options);

    REQUIRE(result.files.size() == 1);
    CHECK(result.files.front().path == std::filesystem::path{"main.c"});
    CHECK(result.excludedCount == 3);
}

TEST_CASE("discoverSourceFiles applies --exclude glob patterns")
{
    TempDirectory root;
    root.createFile("main.cpp");
    root.createFile("vendor/lib.cpp");
    root.createFile("main_generated.cpp");

    codeqa::infrastructure::DiscoveryOptions options;
    options.rootPath = root.path();
    options.excludePatterns = {"*/vendor/*", "*_generated.cpp"};

    const codeqa::infrastructure::DiscoveryResult result = codeqa::infrastructure::discoverSourceFiles(options);

    REQUIRE(result.files.size() == 1);
    CHECK(result.files.front().path == std::filesystem::path{"main.cpp"});
    CHECK(result.excludedCount == 2);
}

TEST_CASE("discoverSourceFiles returns files in deterministic order")
{
    TempDirectory root;
    root.createFile("z.c");
    root.createFile("a.c");
    root.createFile("m.c");

    codeqa::infrastructure::DiscoveryOptions options;
    options.rootPath = root.path();

    const codeqa::infrastructure::DiscoveryResult firstRun = codeqa::infrastructure::discoverSourceFiles(options);
    const codeqa::infrastructure::DiscoveryResult secondRun = codeqa::infrastructure::discoverSourceFiles(options);

    std::vector<std::filesystem::path> firstPaths;
    for (const codeqa::infrastructure::DiscoveredFile& file : firstRun.files)
    {
        firstPaths.push_back(file.path);
    }

    CHECK(firstPaths
        == std::vector<std::filesystem::path>{
            std::filesystem::path{"a.c"}, std::filesystem::path{"m.c"}, std::filesystem::path{"z.c"}});

    std::vector<std::filesystem::path> secondPaths;
    for (const codeqa::infrastructure::DiscoveredFile& file : secondRun.files)
    {
        secondPaths.push_back(file.path);
    }

    CHECK(firstPaths == secondPaths);
}

TEST_CASE("discoverSourceFiles includes empty files with no size limit")
{
    TempDirectory root;
    root.createFile("empty.c", "");

    codeqa::infrastructure::DiscoveryOptions options;
    options.rootPath = root.path();

    const codeqa::infrastructure::DiscoveryResult result = codeqa::infrastructure::discoverSourceFiles(options);

    REQUIRE(result.files.size() == 1);
    CHECK(result.files.front().sizeBytes == 0);
}

TEST_CASE("discoverSourceFiles does not descend into a symlinked directory")
{
    TempDirectory root;
    root.createFile("main.c");
    root.createFile("real/inner.cpp");

    std::error_code symlinkError;
    std::filesystem::create_directory_symlink(root.path() / "real", root.path() / "link", symlinkError);

    if (symlinkError)
    {
        MESSAGE("Skipping: cannot create a directory symlink in this environment ("
            << symlinkError.message() << ").");
        return;
    }

    codeqa::infrastructure::DiscoveryOptions options;
    options.rootPath = root.path();

    const codeqa::infrastructure::DiscoveryResult result = codeqa::infrastructure::discoverSourceFiles(options);
    const std::set<std::filesystem::path> paths = discoveredPaths(result);

    CHECK(paths.count(std::filesystem::path{"real/inner.cpp"}) == 1);
    CHECK(paths.count(std::filesystem::path{"link/inner.cpp"}) == 0);
}
```

Add it to `tests/CMakeLists.txt`'s source list.

- [ ] **Step 2: Run it to verify it fails**

```bash
cmake -S . -B build
cmake --build build
```

Expected: FAIL — `codeqa/infrastructure/DiscoverSourceFiles.h: No such file or directory`.

- [ ] **Step 3: Implement the engine**

`include/codeqa/infrastructure/DiscoverSourceFiles.h`:

```cpp
#pragma once

#include "codeqa/infrastructure/DiscoveryOptions.h"
#include "codeqa/infrastructure/DiscoveryResult.h"

namespace codeqa::infrastructure
{

[[nodiscard]] DiscoveryResult discoverSourceFiles(const DiscoveryOptions& options);

} // namespace codeqa::infrastructure
```

`src/infrastructure/DiscoverSourceFiles.cpp`:

```cpp
#include "codeqa/infrastructure/DiscoverSourceFiles.h"

#include "codeqa/core/DiagnosticSeverity.h"
#include "codeqa/infrastructure/ClassifyExtension.h"
#include "codeqa/infrastructure/DefaultExclusions.h"
#include "codeqa/infrastructure/GlobMatcher.h"

#include <algorithm>
#include <system_error>

namespace codeqa::infrastructure
{

namespace
{

bool matchesAnyExcludePattern(const std::vector<std::string>& patterns, const std::filesystem::path& relativePath)
{
    return std::any_of(patterns.begin(), patterns.end(), [&relativePath](const std::string& pattern)
    {
        return matchesExcludePattern(pattern, relativePath);
    });
}

void recordAccessDiagnostic(DiscoveryResult& result, const std::filesystem::path& path, const std::error_code& error)
{
    codeqa::core::Diagnostic diagnostic;
    diagnostic.code = "DISC001";
    diagnostic.severity = codeqa::core::DiagnosticSeverity::Warning;
    diagnostic.message = "Could not access '" + path.string() + "': " + error.message();
    diagnostic.location.filePath = path;

    result.diagnostics.push_back(diagnostic);
}

void advance(
    std::filesystem::recursive_directory_iterator& it,
    const std::filesystem::path& currentPath,
    DiscoveryResult& result)
{
    std::error_code incrementError;
    it.increment(incrementError);

    if (incrementError)
    {
        recordAccessDiagnostic(result, currentPath, incrementError);
    }
}

} // namespace

DiscoveryResult discoverSourceFiles(const DiscoveryOptions& options)
{
    DiscoveryResult result;

    std::error_code constructionError;
    std::filesystem::recursive_directory_iterator it(
        options.rootPath,
        std::filesystem::directory_options::skip_permission_denied,
        constructionError);
    const std::filesystem::recursive_directory_iterator end;

    if (constructionError)
    {
        recordAccessDiagnostic(result, options.rootPath, constructionError);
        return result;
    }

    while (it != end)
    {
        const std::filesystem::directory_entry entry = *it;
        const std::filesystem::path relativePath = std::filesystem::relative(entry.path(), options.rootPath);

        std::error_code typeError;
        const bool isDirectory = entry.is_directory(typeError);

        if (typeError)
        {
            recordAccessDiagnostic(result, entry.path(), typeError);
            advance(it, entry.path(), result);
            continue;
        }

        if (isDirectory)
        {
            const std::string directoryName = entry.path().filename().string();
            const bool excluded = isDefaultExcludedDirectoryName(directoryName)
                || matchesAnyExcludePattern(options.excludePatterns, relativePath);

            if (excluded)
            {
                result.excludedCount += 1;
                it.disable_recursion_pending();
            }

            advance(it, entry.path(), result);
            continue;
        }

        std::error_code fileTypeError;
        const bool isRegularFile = entry.is_regular_file(fileTypeError);

        if (fileTypeError)
        {
            recordAccessDiagnostic(result, entry.path(), fileTypeError);
            advance(it, entry.path(), result);
            continue;
        }

        if (!isRegularFile)
        {
            advance(it, entry.path(), result);
            continue;
        }

        const std::optional<ExtensionCategory> category = classifyExtension(entry.path().extension().string());

        if (!category.has_value())
        {
            advance(it, entry.path(), result);
            continue;
        }

        if (matchesAnyExcludePattern(options.excludePatterns, relativePath))
        {
            result.excludedCount += 1;
            advance(it, entry.path(), result);
            continue;
        }

        std::error_code sizeError;
        const std::uintmax_t sizeBytes = entry.file_size(sizeError);

        if (sizeError)
        {
            recordAccessDiagnostic(result, entry.path(), sizeError);
            advance(it, entry.path(), result);
            continue;
        }

        result.files.push_back(DiscoveredFile{relativePath, *category, sizeBytes});
        advance(it, entry.path(), result);
    }

    std::sort(result.files.begin(), result.files.end(), [](const DiscoveredFile& lhs, const DiscoveredFile& rhs)
    {
        return lhs.path < rhs.path;
    });

    return result;
}

} // namespace codeqa::infrastructure
```

Add `src/infrastructure/DiscoverSourceFiles.cpp` to `CMakeLists.txt`'s `codeqa_core` sources.

- [ ] **Step 4: Run the tests to verify they pass**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: all tests pass, including the new `DiscoverSourceFilesTest` cases. If the symlink test prints a skip message via `MESSAGE`, that is an expected pass, not a failure — confirm the message names a privilege error, not an assertion failure.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/infrastructure/DiscoverSourceFiles.h src/infrastructure/DiscoverSourceFiles.cpp tests/unit/DiscoverSourceFilesTest.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add the source file discovery traversal engine"
```

---

### Task 4: `DiscoverProjectSources` application use case

**Files:**
- Create: `include/codeqa/application/DiscoverProjectSources.h`
- Create: `src/application/DiscoverProjectSources.cpp`
- Create: `tests/unit/DiscoverProjectSourcesTest.cpp`
- Modify: `CMakeLists.txt` (add `DiscoverProjectSources.cpp`)
- Modify: `tests/CMakeLists.txt` (add the new test file)

**Interfaces:**
- Consumes: `codeqa::infrastructure::discoverSourceFiles`, `DiscoveryOptions`, `DiscoveryResult` (Task 3).
- Produces: `codeqa::application::DiscoverProjectSources` with `[[nodiscard]] DiscoveryResult execute(const DiscoveryOptions& options) const`. Task 5's `RunApplication` calls this directly.

- [ ] **Step 1: Write the failing test**

`tests/unit/DiscoverProjectSourcesTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/application/DiscoverProjectSources.h"

#include <filesystem>
#include <fstream>

namespace
{

std::filesystem::path makeTempRoot()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "codeqa_discover_project_sources_test";

    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    return root;
}

} // namespace

TEST_CASE("DiscoverProjectSources delegates to the discovery engine")
{
    const std::filesystem::path root = makeTempRoot();
    std::ofstream(root / "main.c") << "";
    std::ofstream(root / "notes.txt") << "";

    codeqa::infrastructure::DiscoveryOptions options;
    options.rootPath = root;

    const codeqa::application::DiscoverProjectSources useCase;
    const codeqa::infrastructure::DiscoveryResult result = useCase.execute(options);

    CHECK(result.files.size() == 1);
    CHECK(result.files.front().path == std::filesystem::path{"main.c"});

    std::filesystem::remove_all(root);
}

TEST_CASE("DiscoverProjectSources forwards exclude patterns to the engine")
{
    const std::filesystem::path root = makeTempRoot();
    std::ofstream(root / "keep.cpp") << "";
    std::ofstream(root / "generated.cpp") << "";

    codeqa::infrastructure::DiscoveryOptions options;
    options.rootPath = root;
    options.excludePatterns = {"generated.cpp"};

    const codeqa::application::DiscoverProjectSources useCase;
    const codeqa::infrastructure::DiscoveryResult result = useCase.execute(options);

    REQUIRE(result.files.size() == 1);
    CHECK(result.files.front().path == std::filesystem::path{"keep.cpp"});
    CHECK(result.excludedCount == 1);

    std::filesystem::remove_all(root);
}
```

Add it to `tests/CMakeLists.txt`'s source list.

- [ ] **Step 2: Run it to verify it fails**

```bash
cmake -S . -B build
cmake --build build
```

Expected: FAIL — `codeqa/application/DiscoverProjectSources.h: No such file or directory`.

- [ ] **Step 3: Implement the use case**

`include/codeqa/application/DiscoverProjectSources.h`:

```cpp
#pragma once

#include "codeqa/infrastructure/DiscoveryOptions.h"
#include "codeqa/infrastructure/DiscoveryResult.h"

namespace codeqa::application
{

class DiscoverProjectSources
{
public:
    [[nodiscard]] codeqa::infrastructure::DiscoveryResult execute(
        const codeqa::infrastructure::DiscoveryOptions& options) const;
};

} // namespace codeqa::application
```

`src/application/DiscoverProjectSources.cpp`:

```cpp
#include "codeqa/application/DiscoverProjectSources.h"

#include "codeqa/infrastructure/DiscoverSourceFiles.h"

namespace codeqa::application
{

codeqa::infrastructure::DiscoveryResult DiscoverProjectSources::execute(
    const codeqa::infrastructure::DiscoveryOptions& options) const
{
    return codeqa::infrastructure::discoverSourceFiles(options);
}

} // namespace codeqa::application
```

Add `src/application/DiscoverProjectSources.cpp` to `CMakeLists.txt`'s `codeqa_core` sources.

- [ ] **Step 4: Run the tests to verify they pass**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: all tests pass, including the new `DiscoverProjectSourcesTest` cases.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/application/DiscoverProjectSources.h src/application/DiscoverProjectSources.cpp tests/unit/DiscoverProjectSourcesTest.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add the DiscoverProjectSources application use case"
```

---

### Task 5: `discover` CLI subcommand

**Files:**
- Modify: `src/cli/RunApplication.cpp`
- Modify: `tests/unit/RunApplicationTest.cpp`

**Interfaces:**
- Consumes: `codeqa::application::DiscoverProjectSources` (Task 4); `codeqa::infrastructure::{DiscoveryOptions, DiscoveryResult, DiscoveredFile, ExtensionCategory}` (Tasks 1, 4); `codeqa::cli::ExitCode` (Fase 1, unchanged).
- Produces: no new public interface — this task only changes `runApplication`'s behavior. `runApplication`'s signature (`const std::vector<std::string>&, std::ostream&, std::ostream&) -> int`) does not change.

**Exact CLI behavior to implement:**
- `codeqa discover <path>` — runs discovery, prints the text summary, returns `ExitCode::Success` (even if there are per-file diagnostics).
- `codeqa discover <path> --exclude <glob>` — `--exclude` is repeatable; each occurrence adds one pattern.
- `codeqa discover` (no path) — prints `Error [CLI002]: Missing required argument <path> for 'discover'.` to `errorOutput`, returns `ExitCode::InvalidArguments`.
- `codeqa discover <path>` where `<path>` does not exist or is not a directory — prints `Error [CLI003]: Path '<path>' does not exist or is not a directory.` to `errorOutput`, returns `ExitCode::InvalidArguments`. This check happens **before** the use case is called.
- `codeqa discover <path> --exclude` (no value after `--exclude`) — prints `Error [CLI004]: Missing value for '--exclude'.` to `errorOutput`, returns `ExitCode::InvalidArguments`.
- `codeqa discover <path> <extra>` (a second positional argument) — prints `Error [CLI005]: Unexpected argument '<extra>'.` to `errorOutput`, returns `ExitCode::InvalidArguments`.
- `--help` output documents the new subcommand.

**Text output format** (written to `output`, not `errorOutput`):

```text
Files discovered: <files.size()>
C files: <count where category == C>
C++ files: <count where category == Cpp>
Ambiguous headers: <count where category == AmbiguousHeader>
Excluded files: <excludedCount>
Read errors: <diagnostics.size()>
```

- [ ] **Step 1: Write the failing tests**

Append these `TEST_CASE`s to `tests/unit/RunApplicationTest.cpp` (keep the existing includes and the existing `run()` helper; add the includes and helper shown below):

```cpp
// Add to the existing #include block at the top of the file:
#include "codeqa/infrastructure/DiscoveryOptions.h"

#include <filesystem>
#include <fstream>

// Add to the existing anonymous namespace, alongside RunResult/run():

std::filesystem::path makeTempDiscoverRoot()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "codeqa_run_application_discover_test";

    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::ofstream(root / "main.c") << "";
    std::ofstream(root / "util.cpp") << "";
    std::ofstream(root / "util.h") << "";

    return root;
}

// New TEST_CASEs, appended at the end of the file:

TEST_CASE("discover without a path returns CLI002")
{
    const RunResult result = run({"discover"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI002") != std::string::npos);
}

TEST_CASE("discover with a nonexistent path returns CLI003")
{
    const RunResult result = run({"discover", "this/path/does/not/exist"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI003") != std::string::npos);
}

TEST_CASE("discover with a missing --exclude value returns CLI004")
{
    const std::filesystem::path root = makeTempDiscoverRoot();

    const RunResult result = run({"discover", root.string(), "--exclude"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI004") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("discover with an unexpected extra argument returns CLI005")
{
    const std::filesystem::path root = makeTempDiscoverRoot();

    const RunResult result = run({"discover", root.string(), "extra"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI005") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("discover on a real directory prints the summary and returns Success")
{
    const std::filesystem::path root = makeTempDiscoverRoot();

    const RunResult result = run({"discover", root.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("Files discovered: 3") != std::string::npos);
    CHECK(result.output.find("C files: 1") != std::string::npos);
    CHECK(result.output.find("C++ files: 1") != std::string::npos);
    CHECK(result.output.find("Ambiguous headers: 1") != std::string::npos);
    CHECK(result.output.find("Excluded files: 0") != std::string::npos);
    CHECK(result.output.find("Read errors: 0") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("discover respects a repeated --exclude flag")
{
    const std::filesystem::path root = makeTempDiscoverRoot();

    const RunResult result = run({"discover", root.string(), "--exclude", "util.cpp", "--exclude", "util.h"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("Files discovered: 1") != std::string::npos);
    CHECK(result.output.find("Excluded files: 2") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("--help documents the discover subcommand")
{
    const RunResult result = run({"--help"});

    CHECK(result.output.find("discover") != std::string::npos);
    CHECK(result.output.find("--exclude") != std::string::npos);
}
```

- [ ] **Step 2: Run it to verify it fails**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: FAIL — `discover` is not a recognized subcommand yet, so `run({"discover", ...})` currently returns `ExitCode::InvalidArguments` with a `CLI001` message instead of the new behavior; the `--help` case fails because the current help text has no `discover` mention.

- [ ] **Step 3: Implement the subcommand**

Replace the full contents of `src/cli/RunApplication.cpp`:

```cpp
#include "codeqa/cli/RunApplication.h"

#include "codeqa/application/ApplicationInfo.h"
#include "codeqa/application/DiscoverProjectSources.h"
#include "codeqa/application/GetApplicationInfo.h"
#include "codeqa/cli/ExitCode.h"
#include "codeqa/infrastructure/DiscoveredFile.h"
#include "codeqa/infrastructure/DiscoveryOptions.h"
#include "codeqa/infrastructure/DiscoveryResult.h"
#include "codeqa/infrastructure/ExtensionCategory.h"

#include <filesystem>
#include <optional>
#include <system_error>

namespace codeqa::cli
{

namespace
{

void printHelp(std::ostream& output)
{
    output << "Code Quality Analyzer\n"
              "\n"
              "Usage:\n"
              "  codeqa [options]\n"
              "  codeqa discover <path> [--exclude <glob>]...\n"
              "\n"
              "Options:\n"
              "  -h, --help       Show this help.\n"
              "  -v, --version    Show application version.\n"
              "\n"
              "Commands:\n"
              "  discover <path>       Discover C/C++ source files under <path>.\n"
              "      --exclude <glob>  Exclude files/directories matching a glob pattern\n"
              "                        (repeatable).\n";
}

void printVersion(std::ostream& output)
{
    const codeqa::application::GetApplicationInfo getApplicationInfo;
    const codeqa::application::ApplicationInfo info = getApplicationInfo.execute();

    output << info.name << ' ' << info.version << '\n';
}

void printDiscoveryResult(std::ostream& output, const codeqa::infrastructure::DiscoveryResult& result)
{
    std::size_t cCount = 0;
    std::size_t cppCount = 0;
    std::size_t ambiguousCount = 0;

    for (const codeqa::infrastructure::DiscoveredFile& file : result.files)
    {
        switch (file.category)
        {
        case codeqa::infrastructure::ExtensionCategory::C:
            ++cCount;
            break;
        case codeqa::infrastructure::ExtensionCategory::Cpp:
            ++cppCount;
            break;
        case codeqa::infrastructure::ExtensionCategory::AmbiguousHeader:
            ++ambiguousCount;
            break;
        }
    }

    output << "Files discovered: " << result.files.size() << '\n'
           << "C files: " << cCount << '\n'
           << "C++ files: " << cppCount << '\n'
           << "Ambiguous headers: " << ambiguousCount << '\n'
           << "Excluded files: " << result.excludedCount << '\n'
           << "Read errors: " << result.diagnostics.size() << '\n';
}

int runDiscover(const std::vector<std::string>& arguments, std::ostream& output, std::ostream& errorOutput)
{
    std::optional<std::filesystem::path> path;
    std::vector<std::string> excludePatterns;

    for (std::size_t index = 1; index < arguments.size(); ++index)
    {
        const std::string& token = arguments[index];

        if (token == "--exclude")
        {
            if (index + 1 >= arguments.size())
            {
                errorOutput << "Error [CLI004]: Missing value for '--exclude'.\n";
                return static_cast<int>(ExitCode::InvalidArguments);
            }

            excludePatterns.push_back(arguments[index + 1]);
            ++index;
            continue;
        }

        if (!path.has_value())
        {
            path = std::filesystem::path{token};
            continue;
        }

        errorOutput << "Error [CLI005]: Unexpected argument '" << token << "'.\n";
        return static_cast<int>(ExitCode::InvalidArguments);
    }

    if (!path.has_value())
    {
        errorOutput << "Error [CLI002]: Missing required argument <path> for 'discover'.\n"
                       "Use 'codeqa --help' for usage.\n";
        return static_cast<int>(ExitCode::InvalidArguments);
    }

    std::error_code existsError;
    const bool isDirectory = std::filesystem::is_directory(*path, existsError);

    if (!isDirectory)
    {
        errorOutput << "Error [CLI003]: Path '" << path->string() << "' does not exist or is not a directory.\n";
        return static_cast<int>(ExitCode::InvalidArguments);
    }

    codeqa::infrastructure::DiscoveryOptions options;
    options.rootPath = *path;
    options.excludePatterns = excludePatterns;

    const codeqa::application::DiscoverProjectSources discoverProjectSources;
    const codeqa::infrastructure::DiscoveryResult result = discoverProjectSources.execute(options);

    printDiscoveryResult(output, result);
    return static_cast<int>(ExitCode::Success);
}

} // namespace

int runApplication(
    const std::vector<std::string>& arguments,
    std::ostream& output,
    std::ostream& errorOutput)
{
    if (arguments.empty())
    {
        printHelp(output);
        return static_cast<int>(ExitCode::Success);
    }

    const std::string& firstArgument = arguments.front();

    if (firstArgument == "-h" || firstArgument == "--help")
    {
        printHelp(output);
        return static_cast<int>(ExitCode::Success);
    }

    if (firstArgument == "-v" || firstArgument == "--version")
    {
        printVersion(output);
        return static_cast<int>(ExitCode::Success);
    }

    if (firstArgument == "discover")
    {
        return runDiscover(arguments, output, errorOutput);
    }

    errorOutput << "Error [CLI001]: Unknown option '" << firstArgument << "'.\n"
                   "Use 'codeqa --help' for usage.\n";
    return static_cast<int>(ExitCode::InvalidArguments);
}

} // namespace codeqa::cli
```

Add `src/infrastructure/*.cpp` link/include dependencies: no `CMakeLists.txt` change is needed here — `RunApplication.cpp` was already part of `codeqa_core`, and `codeqa_core` already builds every `src/infrastructure/*.cpp` file added in Tasks 1–3, so the new `#include`s just need those object files, which are already linked into the same static library.

- [ ] **Step 4: Run the tests to verify they pass**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: all tests pass, including every new `RunApplicationTest` case and the pre-existing ones (`--help`, `--version`, unknown option, etc. must still pass unmodified).

- [ ] **Step 5: Commit**

```bash
git add src/cli/RunApplication.cpp tests/unit/RunApplicationTest.cpp
git commit -m "feat: add the discover CLI subcommand"
```

---

### Task 6: Integration test fixture, CTest wiring, and README

**Files:**
- Create: `tests/integration/fixtures/discover-sample/main.c`
- Create: `tests/integration/fixtures/discover-sample/util.cpp`
- Create: `tests/integration/fixtures/discover-sample/util.h`
- Create: `tests/integration/fixtures/discover-sample/build/generated.cpp`
- Modify: `tests/CMakeLists.txt` (add the `codeqa_integration_discover` test)
- Modify: `README.md` (document `discover`)

**Interfaces:**
- Consumes: the real `codeqa` executable (built by Task 5), via the existing `tests/integration/CheckCliOutput.cmake` script (unchanged).
- Produces: nothing consumed by later tasks — this is the last task in the plan.

**Expected fixture counts:** `main.c` (C), `util.cpp` (C++), `util.h` (ambiguous header) = 3 discovered files; `build/` is excluded by the fixed default list, counted once (per the `excludedCount` semantics from Task 3) = 1 excluded entry; no access errors.

- [ ] **Step 1: Create the fixture files**

`tests/integration/fixtures/discover-sample/main.c`:

```c
int main(void)
{
    return 0;
}
```

`tests/integration/fixtures/discover-sample/util.cpp`:

```cpp
int add(int a, int b)
{
    return a + b;
}
```

`tests/integration/fixtures/discover-sample/util.h`:

```c
#pragma once

int add(int a, int b);
```

`tests/integration/fixtures/discover-sample/build/generated.cpp`:

```cpp
// Placeholder file used only to prove `build/` is excluded by discovery.
int placeholder;
```

- [ ] **Step 2: Add the CTest integration test**

Append to `tests/CMakeLists.txt` (after the existing `codeqa_integration_help` test):

```cmake
add_test(
    NAME codeqa_integration_discover
    COMMAND ${CMAKE_COMMAND}
        -DCODEQA_EXE=$<TARGET_FILE:codeqa>
        "-DCODEQA_ARGS=discover$<SEMICOLON>${CMAKE_CURRENT_SOURCE_DIR}/integration/fixtures/discover-sample"
        "-DCODEQA_EXPECTED_OUTPUT=Files discovered: 3"
        -DCODEQA_EXPECTED_EXIT_CODE=0
        -P ${CMAKE_CURRENT_SOURCE_DIR}/integration/CheckCliOutput.cmake
)
```

- [ ] **Step 3: Run the full test suite to verify it passes**

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: all tests pass, including the new `codeqa_integration_discover` test. If `$<SEMICOLON>` does not produce two separate arguments on this CMake version (check with `cmake --version`; the project's floor is 3.16), inspect the failure output from `CheckCliOutput.cmake` — it prints the actual command output on mismatch — and adjust by passing `CODEQA_ARGS` as an explicit CMake list (`-DCODEQA_ARGS=discover\;<path>` with the semicolon escaped for the shell) instead.

- [ ] **Step 4: Update the README**

In `README.md`, add a new section after `## Ejecución` (before `## Códigos de salida`):

````markdown
## Descubrimiento de archivos

```bash
./build/codeqa discover ./ruta/al/proyecto
./build/codeqa discover ./ruta/al/proyecto --exclude "*/vendor/*"
```

Recorre recursivamente el directorio indicado y localiza los archivos fuente C/C++ soportados (`.c`, `.cc`, `.cpp`, `.cxx`, `.hpp`, `.hh`, `.hxx`; `.h` se cuenta como ambiguo). Excluye por defecto `.git`, `build/`, `cmake-build-*/` y cualquier directorio oculto; `--exclude <glob>` (repetible) añade exclusiones adicionales. No lee el contenido de los archivos — solo enumera rutas y metadatos.
````

Also update the line in `README.md`'s introduction that currently reads "no analiza ningún código C o C++ todavía" / the `## Estado` section, if it exists, to reflect that source discovery now works — check the current wording first and adjust it minimally (e.g. "descubre archivos fuente C/C++, pero todavía no los analiza").

- [ ] **Step 5: Commit**

```bash
git add tests/integration/fixtures/discover-sample tests/CMakeLists.txt README.md
git commit -m "test: add discover CTest integration check and document the subcommand"
```

---

## Self-Review Notes

- **Spec coverage:** every item in the design spec's section 2 (Alcance) maps to a task — recursive traversal + pruning (Task 3), extension classification (Task 1), fixed directory exclusions (Task 1/3), `--exclude` patterns (Task 2/3), deterministic order (Task 3), per-file access diagnostics (Task 3), nonexistent-root CLI error (Task 5), the `discover` subcommand itself (Task 5), fixture-backed end-to-end proof (Task 6).
- **Type consistency:** `DiscoveryOptions`, `DiscoveryResult`, `DiscoveredFile`, `ExtensionCategory` field names are identical across Tasks 1, 3, 4, and 5 (`rootPath`, `excludePatterns`, `files`, `diagnostics`, `excludedCount`, `path`, `category`, `sizeBytes`). `matchesExcludePattern`'s signature is identical in Task 2's declaration and Task 3's call site.
- **No placeholders:** every step has literal code, exact CLI error codes/messages, and exact expected test output — no "add appropriate error handling" language anywhere in the tasks.
