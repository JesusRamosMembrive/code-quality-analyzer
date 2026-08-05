# Fase 3 — Detección inicial del lenguaje Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Assign each file discovered by Fase 2 a resolved `Language` (`C`, `Cpp`, `Unknown`) with a confidence value and an explainable reason, giving explicit `--lang-override <pattern>=<lang>` overrides priority over extension-based detection, and print the per-file result as part of `codeqa discover`'s output.

**Architecture:** A new `codeqa::core::Language`/`LanguageDetectionResult` pair of pure value types, plus a `codeqa::application::DetectFileLanguage` use case (mirrors `GetApplicationInfo`/`DiscoverProjectSources`: a class with `execute()`) that maps a `DiscoveredFile`'s `ExtensionCategory` (Fase 2, unchanged) to a `Language`, checked first against a list of `LanguageOverride`s using the same glob matcher Fase 2 already built for `--exclude`. `RunApplication` parses the new flag, calls the use case once per discovered file, and appends a per-file listing after the existing summary.

**Tech Stack:** C++17, reuses `codeqa::infrastructure::matchesExcludePattern` (Fase 2) — no new external dependencies.

## Global Constraints

Carried forward from Fase 1 and Fase 2 (still binding):

- C++17 standard, no compiler-specific extensions.
- `codeqa_core` must not depend on Qt in any form. Standard library only.
- No new external dependencies of any kind.
- All project code lives inside `namespace codeqa { ... }`, subnamespaces `codeqa::core`, `codeqa::application`, `codeqa::infrastructure`, `codeqa::cli`. Never `using namespace std;`.
- Naming: classes/structs/enums `PascalCase`; functions/methods `camelCase`; namespaces lowercase; files `PascalCase.h`/`.cpp`.
- Use `${PROJECT_SOURCE_DIR}` / `${PROJECT_BINARY_DIR}` in CMake, not `CMAKE_SOURCE_DIR` / `CMAKE_BINARY_DIR`.
- This phase still does not read file content — detection is extension- and path-based only.

New, from `docs/superpowers/specs/2026-08-05-fase-3-language-detection-design.md` (read it if a task feels ambiguous):

- **No content heuristics for ambiguous `.h` files.** Without a matching override, `.h` always resolves to `Language::Unknown`, confidence `0.5`, reason `"ambiguous header extension"`. Never invent certainty.
- **Confidence is a fixed step, not a computed statistic:** `1.0` for unambiguous extension or override, `0.5` for ambiguous-without-override. No other values exist in this phase.
- **Overrides win even over an unambiguous extension.** An override always takes priority over `ExtensionCategory`, regardless of how unambiguous the extension was.
- **When multiple overrides match the same file, the last one specified on the command line wins.**
- **`Language`/`LanguageDetectionResult` live in `codeqa::core`** (pure value types, no I/O) — `ExtensionCategory` (Fase 2, `codeqa::infrastructure`) is unchanged and stays the raw per-extension signal; `Language` is the new resolved decision built on top of it.
- **Detection logic lives in a new, separate `codeqa::application::DetectFileLanguage` use case** — `DiscoverProjectSources` (Fase 2) is not modified.
- **Overrides reuse `codeqa::infrastructure::matchesExcludePattern`** (Fase 2's glob matcher) with its existing semantics unchanged: a pattern containing `/` matches the whole relative path; a pattern without `/` matches the filename at any depth; `*` never crosses `/`.
- **`<lang>` in `--lang-override <pattern>=<lang>` is case-sensitive:** only the literal strings `c` and `cpp` are valid.
- **The `<pattern>=<lang>` value splits on the first `=`** encountered.
- **Per-file output line format:** `<ruta-relativa-generic_string>` + two literal spaces + `<nombre-lenguaje>`, and — only when the detection reason is not `"unambiguous extension"` — followed by ` (confidence X.XX) - <reason>` (confidence formatted with exactly 2 decimal places). This is a simplification of the design spec's illustrative (visually column-aligned) example: exact spacing is not part of the spec's acceptance criteria, so this plan fixes it at a literal two-space separator, no dynamic column alignment.

---

## File Structure

```text
include/codeqa/core/
├── Language.h                      (enum class: C, Cpp, Unknown)
└── LanguageDetectionResult.h       (struct)

include/codeqa/application/
└── DetectFileLanguage.h            (LanguageOverride struct + DetectFileLanguage class)

src/application/
└── DetectFileLanguage.cpp

src/cli/
└── RunApplication.cpp              (modified: --lang-override parsing, per-file listing)

tests/unit/
├── DetectFileLanguageTest.cpp
└── RunApplicationTest.cpp          (modified: new --lang-override / listing cases)

tests/CMakeLists.txt                (modified: new test file, 2 new integration tests)
README.md                           (modified: documents language detection + --lang-override)
CMakeLists.txt                      (modified: adds DetectFileLanguage.cpp to codeqa_core)
```

No new fixture files are needed — the existing `tests/integration/fixtures/discover-sample/` (Fase 2) already contains an ambiguous `.h`, which is exactly what this phase needs to exercise end-to-end.

---

### Task 1: `Language` core types and the `DetectFileLanguage` use case

**Files:**
- Create: `include/codeqa/core/Language.h`
- Create: `include/codeqa/core/LanguageDetectionResult.h`
- Create: `include/codeqa/application/DetectFileLanguage.h`
- Create: `src/application/DetectFileLanguage.cpp`
- Create: `tests/unit/DetectFileLanguageTest.cpp`
- Modify: `CMakeLists.txt` (add `DetectFileLanguage.cpp` to `codeqa_core`)
- Modify: `tests/CMakeLists.txt` (add the new test file)

**Interfaces:**
- Consumes: `codeqa::infrastructure::DiscoveredFile{path, category, sizeBytes}`, `codeqa::infrastructure::ExtensionCategory{C, Cpp, AmbiguousHeader}` (Fase 2, `include/codeqa/infrastructure/DiscoveredFile.h`); `codeqa::infrastructure::matchesExcludePattern(const std::string&, const std::filesystem::path&) -> bool` (Fase 2, `include/codeqa/infrastructure/GlobMatcher.h`).
- Produces: `codeqa::core::Language{C, Cpp, Unknown}`; `codeqa::core::LanguageDetectionResult{language, confidence, reason}`; `codeqa::application::LanguageOverride{pattern, language}`; `codeqa::application::DetectFileLanguage` with `[[nodiscard]] codeqa::core::LanguageDetectionResult execute(const codeqa::infrastructure::DiscoveredFile& file, const std::vector<LanguageOverride>& overrides) const`. Task 2 (CLI) calls this exact signature once per discovered file.

- [ ] **Step 1: Write the failing test**

`tests/unit/DetectFileLanguageTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/application/DetectFileLanguage.h"

#include "codeqa/infrastructure/DiscoveredFile.h"
#include "codeqa/infrastructure/ExtensionCategory.h"

#include <filesystem>
#include <vector>

namespace
{

codeqa::infrastructure::DiscoveredFile makeFile(
    const std::filesystem::path& path,
    codeqa::infrastructure::ExtensionCategory category)
{
    return codeqa::infrastructure::DiscoveredFile{path, category, 0};
}

} // namespace

TEST_CASE("an unambiguous C extension resolves to Language::C with full confidence")
{
    const codeqa::application::DetectFileLanguage detect;
    const codeqa::infrastructure::DiscoveredFile file =
        makeFile("main.c", codeqa::infrastructure::ExtensionCategory::C);

    const codeqa::core::LanguageDetectionResult result = detect.execute(file, {});

    CHECK(result.language == codeqa::core::Language::C);
    CHECK(result.confidence == 1.0);
    CHECK(result.reason == "unambiguous extension");
}

TEST_CASE("an unambiguous C++ extension resolves to Language::Cpp with full confidence")
{
    const codeqa::application::DetectFileLanguage detect;
    const codeqa::infrastructure::DiscoveredFile file =
        makeFile("util.cpp", codeqa::infrastructure::ExtensionCategory::Cpp);

    const codeqa::core::LanguageDetectionResult result = detect.execute(file, {});

    CHECK(result.language == codeqa::core::Language::Cpp);
    CHECK(result.confidence == 1.0);
    CHECK(result.reason == "unambiguous extension");
}

TEST_CASE("an ambiguous header without an override resolves to Unknown with partial confidence")
{
    const codeqa::application::DetectFileLanguage detect;
    const codeqa::infrastructure::DiscoveredFile file =
        makeFile("util.h", codeqa::infrastructure::ExtensionCategory::AmbiguousHeader);

    const codeqa::core::LanguageDetectionResult result = detect.execute(file, {});

    CHECK(result.language == codeqa::core::Language::Unknown);
    CHECK(result.confidence == 0.5);
    CHECK(result.reason == "ambiguous header extension");
}

TEST_CASE("a filename override resolves an ambiguous header")
{
    const codeqa::application::DetectFileLanguage detect;
    const codeqa::infrastructure::DiscoveredFile file =
        makeFile("util.h", codeqa::infrastructure::ExtensionCategory::AmbiguousHeader);
    const std::vector<codeqa::application::LanguageOverride> overrides{
        {"util.h", codeqa::core::Language::Cpp}};

    const codeqa::core::LanguageDetectionResult result = detect.execute(file, overrides);

    CHECK(result.language == codeqa::core::Language::Cpp);
    CHECK(result.confidence == 1.0);
    CHECK(result.reason == "path override");
}

TEST_CASE("a full-path override resolves a nested ambiguous header")
{
    const codeqa::application::DetectFileLanguage detect;
    const codeqa::infrastructure::DiscoveredFile file = makeFile(
        "include/network/socket.h", codeqa::infrastructure::ExtensionCategory::AmbiguousHeader);
    const std::vector<codeqa::application::LanguageOverride> overrides{
        {"include/network/socket.h", codeqa::core::Language::C}};

    const codeqa::core::LanguageDetectionResult result = detect.execute(file, overrides);

    CHECK(result.language == codeqa::core::Language::C);
    CHECK(result.reason == "path override");
}

TEST_CASE("an override wins even over an unambiguous extension")
{
    const codeqa::application::DetectFileLanguage detect;
    const codeqa::infrastructure::DiscoveredFile file =
        makeFile("legacy.c", codeqa::infrastructure::ExtensionCategory::C);
    const std::vector<codeqa::application::LanguageOverride> overrides{
        {"legacy.c", codeqa::core::Language::Cpp}};

    const codeqa::core::LanguageDetectionResult result = detect.execute(file, overrides);

    CHECK(result.language == codeqa::core::Language::Cpp);
    CHECK(result.confidence == 1.0);
    CHECK(result.reason == "path override");
}

TEST_CASE("when multiple overrides match the same file, the last one wins")
{
    const codeqa::application::DetectFileLanguage detect;
    const codeqa::infrastructure::DiscoveredFile file =
        makeFile("ambiguous.h", codeqa::infrastructure::ExtensionCategory::AmbiguousHeader);
    const std::vector<codeqa::application::LanguageOverride> overrides{
        {"ambiguous.h", codeqa::core::Language::C},
        {"ambiguous.h", codeqa::core::Language::Cpp}};

    const codeqa::core::LanguageDetectionResult result = detect.execute(file, overrides);

    CHECK(result.language == codeqa::core::Language::Cpp);
}

TEST_CASE("an override that does not match the file is ignored")
{
    const codeqa::application::DetectFileLanguage detect;
    const codeqa::infrastructure::DiscoveredFile file =
        makeFile("main.c", codeqa::infrastructure::ExtensionCategory::C);
    const std::vector<codeqa::application::LanguageOverride> overrides{
        {"other.c", codeqa::core::Language::Cpp}};

    const codeqa::core::LanguageDetectionResult result = detect.execute(file, overrides);

    CHECK(result.language == codeqa::core::Language::C);
    CHECK(result.reason == "unambiguous extension");
}
```

Add it to `tests/CMakeLists.txt`'s `add_executable(codeqa_tests ...)` source list.

- [ ] **Step 2: Run it to verify it fails**

```bash
cmake -S . -B build
cmake --build build
```

Expected: FAIL — `codeqa/application/DetectFileLanguage.h: No such file or directory`.

- [ ] **Step 3: Implement the types and the use case**

`include/codeqa/core/Language.h`:

```cpp
#pragma once

namespace codeqa::core
{

enum class Language
{
    C,
    Cpp,
    Unknown
};

} // namespace codeqa::core
```

`include/codeqa/core/LanguageDetectionResult.h`:

```cpp
#pragma once

#include "codeqa/core/Language.h"

#include <string>

namespace codeqa::core
{

struct LanguageDetectionResult
{
    Language language{Language::Unknown};
    double confidence{0.0};
    std::string reason;
};

} // namespace codeqa::core
```

`include/codeqa/application/DetectFileLanguage.h`:

```cpp
#pragma once

#include "codeqa/core/Language.h"
#include "codeqa/core/LanguageDetectionResult.h"
#include "codeqa/infrastructure/DiscoveredFile.h"

#include <string>
#include <vector>

namespace codeqa::application
{

struct LanguageOverride
{
    std::string pattern;
    codeqa::core::Language language;
};

class DetectFileLanguage
{
public:
    [[nodiscard]] codeqa::core::LanguageDetectionResult execute(
        const codeqa::infrastructure::DiscoveredFile& file,
        const std::vector<LanguageOverride>& overrides) const;
};

} // namespace codeqa::application
```

`src/application/DetectFileLanguage.cpp`:

```cpp
#include "codeqa/application/DetectFileLanguage.h"

#include "codeqa/infrastructure/ExtensionCategory.h"
#include "codeqa/infrastructure/GlobMatcher.h"

namespace codeqa::application
{

codeqa::core::LanguageDetectionResult DetectFileLanguage::execute(
    const codeqa::infrastructure::DiscoveredFile& file,
    const std::vector<LanguageOverride>& overrides) const
{
    for (auto it = overrides.rbegin(); it != overrides.rend(); ++it)
    {
        if (codeqa::infrastructure::matchesExcludePattern(it->pattern, file.path))
        {
            return codeqa::core::LanguageDetectionResult{it->language, 1.0, "path override"};
        }
    }

    switch (file.category)
    {
    case codeqa::infrastructure::ExtensionCategory::C:
        return codeqa::core::LanguageDetectionResult{
            codeqa::core::Language::C, 1.0, "unambiguous extension"};
    case codeqa::infrastructure::ExtensionCategory::Cpp:
        return codeqa::core::LanguageDetectionResult{
            codeqa::core::Language::Cpp, 1.0, "unambiguous extension"};
    case codeqa::infrastructure::ExtensionCategory::AmbiguousHeader:
        return codeqa::core::LanguageDetectionResult{
            codeqa::core::Language::Unknown, 0.5, "ambiguous header extension"};
    }

    return codeqa::core::LanguageDetectionResult{};
}

} // namespace codeqa::application
```

The trailing `return codeqa::core::LanguageDetectionResult{};` after the switch is unreachable (the switch covers all three `ExtensionCategory` enumerators) but required so MSVC doesn't warn "not all control paths return a value" (`/W4` is enabled on `codeqa_core`).

Iterating overrides in reverse (`rbegin()`/`rend()`) and returning on the first match implements "the last matching override wins" without a second pass.

Add `src/application/DetectFileLanguage.cpp` to `CMakeLists.txt`'s `codeqa_core` sources.

- [ ] **Step 4: Run the tests to verify they pass**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: build succeeds, all tests pass including the new `DetectFileLanguageTest` cases.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/core/Language.h include/codeqa/core/LanguageDetectionResult.h include/codeqa/application/DetectFileLanguage.h src/application/DetectFileLanguage.cpp tests/unit/DetectFileLanguageTest.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add Language detection types and the DetectFileLanguage use case"
```

---

### Task 2: `--lang-override` CLI flag and per-file language listing

**Files:**
- Modify: `src/cli/RunApplication.cpp`
- Modify: `tests/unit/RunApplicationTest.cpp`

**Interfaces:**
- Consumes: `codeqa::application::DetectFileLanguage`, `codeqa::application::LanguageOverride` (Task 1); `codeqa::core::Language`, `codeqa::core::LanguageDetectionResult` (Task 1); `codeqa::infrastructure::DiscoveredFile` (Fase 2, already used by `RunApplication.cpp`).
- Produces: no new public interface — `runApplication`'s signature is unchanged. This task only changes `runDiscover`'s behavior and output.

**Exact behavior to implement:**
- `--lang-override <pattern>=<lang>` is parsed in the same argument loop that already handles `--exclude`, appending to a `std::vector<codeqa::application::LanguageOverride>`.
- `--lang-override` with no following token → `Error [CLI006]: Missing value for '--lang-override'.\n`, `ExitCode::InvalidArguments`.
- `--lang-override` value with no `=`, an empty pattern (`=` at position 0), or an empty language (`=` at the last character) → `Error [CLI007]: Invalid --lang-override value '<value>'. Expected <pattern>=c|cpp.\n`, `ExitCode::InvalidArguments`.
- `--lang-override` value whose language part is anything other than the literal strings `c` or `cpp` → same `CLI007` error.
- After a successful discovery, for every file in `result.files` (in the existing deterministic order), call `DetectFileLanguage::execute(file, overrides)` and print one line to `output`.
- Line format: `<file.path.generic_string()>` + `"  "` (two spaces) + language name (`"C"`, `"C++"`, or `"Unknown"`). If `detection.reason != "unambiguous extension"`, append `" (confidence "` + confidence formatted to exactly 2 decimal places + `") - "` + `detection.reason`. Always end with `'\n'`.
- The per-file listing is preceded by one blank line, printed only if `result.files` is non-empty.
- `--help` documents `--lang-override`.

- [ ] **Step 1: Write the failing tests**

Append to `tests/unit/RunApplicationTest.cpp` (existing `#include`s, `RunResult`/`run()`/`makeTempDiscoverRoot()` helpers stay as-is; the existing `TEST_CASE`s from Fase 2 must keep passing unmodified):

```cpp
TEST_CASE("discover with a missing --lang-override value returns CLI006")
{
    const std::filesystem::path root = makeTempDiscoverRoot();

    const RunResult result = run({"discover", root.string(), "--lang-override"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI006") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("discover with a malformed --lang-override value returns CLI007")
{
    const std::filesystem::path root = makeTempDiscoverRoot();

    const RunResult resultNoEquals = run({"discover", root.string(), "--lang-override", "util.h"});
    CHECK(resultNoEquals.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(resultNoEquals.errorOutput.find("CLI007") != std::string::npos);

    const RunResult resultBadLanguage =
        run({"discover", root.string(), "--lang-override", "util.h=python"});
    CHECK(resultBadLanguage.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(resultBadLanguage.errorOutput.find("CLI007") != std::string::npos);

    const RunResult resultEmptyPattern =
        run({"discover", root.string(), "--lang-override", "=c"});
    CHECK(resultEmptyPattern.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(resultEmptyPattern.errorOutput.find("CLI007") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("discover prints a per-file language listing")
{
    const std::filesystem::path root = makeTempDiscoverRoot();

    const RunResult result = run({"discover", root.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("main.c  C\n") != std::string::npos);
    CHECK(result.output.find("util.cpp  C++\n") != std::string::npos);
    CHECK(result.output.find("util.h  Unknown (confidence 0.50) - ambiguous header extension\n")
        != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("--lang-override forces the language of a matching file")
{
    const std::filesystem::path root = makeTempDiscoverRoot();

    const RunResult result = run({"discover", root.string(), "--lang-override", "util.h=cpp"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("util.h  C++ (confidence 1.00) - path override\n") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("--help documents --lang-override")
{
    const RunResult result = run({"--help"});

    CHECK(result.output.find("--lang-override") != std::string::npos);
}
```

- [ ] **Step 2: Run it to verify it fails**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: FAIL — `--lang-override` is currently treated as an unrecognized second positional argument (`CLI005`), and there is no per-file listing in the current output, and `--help` doesn't mention `--lang-override`.

- [ ] **Step 3: Implement the CLI changes**

Replace the full contents of `src/cli/RunApplication.cpp`:

```cpp
#include "codeqa/cli/RunApplication.h"

#include "codeqa/application/ApplicationInfo.h"
#include "codeqa/application/DetectFileLanguage.h"
#include "codeqa/application/DiscoverProjectSources.h"
#include "codeqa/application/GetApplicationInfo.h"
#include "codeqa/cli/ExitCode.h"
#include "codeqa/core/Language.h"
#include "codeqa/core/LanguageDetectionResult.h"
#include "codeqa/infrastructure/DiscoveredFile.h"
#include "codeqa/infrastructure/DiscoveryOptions.h"
#include "codeqa/infrastructure/DiscoveryResult.h"
#include "codeqa/infrastructure/ExtensionCategory.h"

#include <filesystem>
#include <iomanip>
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
              "  codeqa discover <path> [--exclude <glob>]... [--lang-override <pattern>=<lang>]...\n"
              "\n"
              "Options:\n"
              "  -h, --help       Show this help.\n"
              "  -v, --version    Show application version.\n"
              "\n"
              "Commands:\n"
              "  discover <path>                    Discover C/C++ source files under <path>.\n"
              "      --exclude <glob>                Exclude files/directories matching a glob\n"
              "                                       pattern (repeatable).\n"
              "      --lang-override <pattern>=<lang> Force the language of files matching\n"
              "                                       <pattern> (<lang> is 'c' or 'cpp';\n"
              "                                       repeatable, last match wins).\n";
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

const char* languageName(codeqa::core::Language language)
{
    switch (language)
    {
    case codeqa::core::Language::C:
        return "C";
    case codeqa::core::Language::Cpp:
        return "C++";
    case codeqa::core::Language::Unknown:
        return "Unknown";
    }

    return "Unknown";
}

void printLanguageListing(
    std::ostream& output,
    const codeqa::infrastructure::DiscoveryResult& result,
    const std::vector<codeqa::application::LanguageOverride>& overrides)
{
    if (result.files.empty())
    {
        return;
    }

    output << '\n';

    const codeqa::application::DetectFileLanguage detectFileLanguage;

    for (const codeqa::infrastructure::DiscoveredFile& file : result.files)
    {
        const codeqa::core::LanguageDetectionResult detection =
            detectFileLanguage.execute(file, overrides);

        output << file.path.generic_string() << "  " << languageName(detection.language);

        if (detection.reason != "unambiguous extension")
        {
            output << " (confidence " << std::fixed << std::setprecision(2) << detection.confidence
                   << ") - " << detection.reason;
        }

        output << '\n';
    }
}

std::optional<codeqa::core::Language> parseLanguageName(const std::string& languageText)
{
    if (languageText == "c")
    {
        return codeqa::core::Language::C;
    }

    if (languageText == "cpp")
    {
        return codeqa::core::Language::Cpp;
    }

    return std::nullopt;
}

int runDiscover(const std::vector<std::string>& arguments, std::ostream& output, std::ostream& errorOutput)
{
    std::optional<std::filesystem::path> path;
    std::vector<std::string> excludePatterns;
    std::vector<codeqa::application::LanguageOverride> languageOverrides;

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

        if (token == "--lang-override")
        {
            if (index + 1 >= arguments.size())
            {
                errorOutput << "Error [CLI006]: Missing value for '--lang-override'.\n";
                return static_cast<int>(ExitCode::InvalidArguments);
            }

            const std::string& value = arguments[index + 1];
            const std::size_t equalsPosition = value.find('=');

            if (equalsPosition == std::string::npos || equalsPosition == 0
                || equalsPosition == value.size() - 1)
            {
                errorOutput << "Error [CLI007]: Invalid --lang-override value '" << value
                               << "'. Expected <pattern>=c|cpp.\n";
                return static_cast<int>(ExitCode::InvalidArguments);
            }

            const std::string pattern = value.substr(0, equalsPosition);
            const std::string languageText = value.substr(equalsPosition + 1);
            const std::optional<codeqa::core::Language> language = parseLanguageName(languageText);

            if (!language.has_value())
            {
                errorOutput << "Error [CLI007]: Invalid --lang-override value '" << value
                               << "'. Expected <pattern>=c|cpp.\n";
                return static_cast<int>(ExitCode::InvalidArguments);
            }

            languageOverrides.push_back(codeqa::application::LanguageOverride{pattern, *language});
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
    printLanguageListing(output, result, languageOverrides);
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

No `CMakeLists.txt` change is needed for this task — `RunApplication.cpp` is already part of `codeqa_core`, and `codeqa_core` already builds `src/application/DetectFileLanguage.cpp` from Task 1.

- [ ] **Step 4: Run the tests to verify they pass**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: all tests pass, including every new case and every pre-existing `RunApplicationTest` case (`--help`, `-h`, `--version`, no-args, `CLI001`, stdout/stderr separation, all Fase 2 `discover` cases) unmodified.

- [ ] **Step 5: Commit**

```bash
git add src/cli/RunApplication.cpp tests/unit/RunApplicationTest.cpp
git commit -m "feat: add --lang-override flag and per-file language listing to discover"
```

---

### Task 3: Integration tests and README

**Files:**
- Modify: `tests/CMakeLists.txt` (add two integration tests)
- Modify: `README.md` (documents language detection and `--lang-override`)

**Interfaces:**
- Consumes: the real `codeqa` executable (Task 2), the existing `tests/integration/fixtures/discover-sample/` fixture and `tests/integration/CheckCliOutput.cmake` script (both unchanged from Fase 2 — no new fixture files needed).
- Produces: nothing consumed by later tasks — this is the last task in the plan.

- [ ] **Step 1: Add the two integration tests**

Append to `tests/CMakeLists.txt` (after the existing `codeqa_integration_discover_excludes_build` test):

```cmake
add_test(
    NAME codeqa_integration_discover_language
    COMMAND ${CMAKE_COMMAND}
        -DCODEQA_EXE=$<TARGET_FILE:codeqa>
        "-DCODEQA_ARGS=discover$<SEMICOLON>${CMAKE_CURRENT_SOURCE_DIR}/integration/fixtures/discover-sample"
        "-DCODEQA_EXPECTED_OUTPUT=util.h  Unknown (confidence 0.50) - ambiguous header extension"
        -DCODEQA_EXPECTED_EXIT_CODE=0
        -P ${CMAKE_CURRENT_SOURCE_DIR}/integration/CheckCliOutput.cmake
)

add_test(
    NAME codeqa_integration_lang_override
    COMMAND ${CMAKE_COMMAND}
        -DCODEQA_EXE=$<TARGET_FILE:codeqa>
        "-DCODEQA_ARGS=discover$<SEMICOLON>${CMAKE_CURRENT_SOURCE_DIR}/integration/fixtures/discover-sample$<SEMICOLON>--lang-override$<SEMICOLON>util.h=cpp"
        "-DCODEQA_EXPECTED_OUTPUT=util.h  C++ (confidence 1.00) - path override"
        -DCODEQA_EXPECTED_EXIT_CODE=0
        -P ${CMAKE_CURRENT_SOURCE_DIR}/integration/CheckCliOutput.cmake
)
```

- [ ] **Step 2: Reconfigure, build, and run the full suite to verify it passes**

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: all tests pass, including the two new integration tests. `codeqa_integration_lang_override` chains three `$<SEMICOLON>` generator expressions to pass four separate arguments (`discover`, the fixture path, `--lang-override`, `util.h=cpp`) through the single `CODEQA_ARGS` `-D` value — the same mechanism `codeqa_integration_discover` already uses once, just repeated. If it doesn't split into four arguments as expected, `CheckCliOutput.cmake`'s failure output shows the actual command and output it ran — inspect that rather than guessing.

- [ ] **Step 3: Update the README**

In `README.md`, extend the existing "Descubrimiento de archivos" section (do not create a new section — add to the end of it, after the existing "Reglas de coincidencia de `--exclude`" paragraph):

````markdown
### Detección de lenguaje

```bash
./build/codeqa discover ./ruta/al/proyecto --lang-override "include/network/*.h=c"
```

Cada archivo descubierto se lista con su lenguaje detectado (`C`, `C++` o `Unknown`). La detección se basa solo en la extensión: `.c` es siempre `C`, `.cc`/`.cpp`/`.cxx`/`.hpp`/`.hh`/`.hxx` son siempre `C++`, y un `.h` sin override queda como `Unknown` con confianza `0.50` — la herramienta no inventa certeza cuando no la hay, y esta fase sigue sin leer el contenido de los archivos.

`--lang-override <pattern>=<lang>` (repetible, `<lang>` es `c` o `cpp`) fuerza el lenguaje de los archivos cuya ruta coincide con `<pattern>` — mismas reglas de coincidencia que `--exclude` (ver arriba) — con prioridad sobre la detección automática, incluso si la extensión era inequívoca. Si varios overrides coinciden con el mismo archivo, gana el último especificado.
````

- [ ] **Step 4: Commit**

```bash
git add tests/CMakeLists.txt README.md
git commit -m "test: add discover language-detection CTest checks and document --lang-override"
```

---

## Self-Review Notes

- **Spec coverage:** every item in the design spec's section 2 (Alcance) maps to a task — extension→language resolution and override priority (Task 1), `--lang-override` CLI flag and desempate por orden (Task 1 + Task 2), listado por archivo en `discover` (Task 2), prueba end-to-end sin fixtures nuevas (Task 3).
- **Type consistency:** `Language`, `LanguageDetectionResult`, `LanguageOverride`, and `DetectFileLanguage::execute`'s exact signature are identical across Task 1 (definition) and Task 2 (call site). `matchesExcludePattern`'s signature matches its Fase 2 declaration exactly.
- **No placeholders:** every step has literal code, exact CLI error codes/messages, and exact expected test/CLI output — no "add appropriate error handling" language anywhere in the tasks.
