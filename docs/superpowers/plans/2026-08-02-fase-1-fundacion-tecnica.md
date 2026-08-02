# Fase 1 — Fundación Técnica Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stand up a compilable, testable C++17 repository skeleton (CMake build, `codeqa_core` library, `codeqa` CLI executable, doctest-based test suite) with zero analysis logic — the foundation every later phase builds on.

**Architecture:** A thin `codeqa` CLI executable (`src/cli/main.cpp`) that only converts `argv`/streams and applies a try/catch barrier, backed by a `codeqa_core` static library holding all reusable logic (core value types, the `GetApplicationInfo` use case, and the testable `runApplication` orchestration function). `codeqa_core` has zero Qt dependency and depends only on the C++ standard library. A separate `codeqa_tests` executable links `codeqa_core` and doctest to run unit tests; CTest also drives small integration tests that invoke the real `codeqa` binary as a subprocess.

**Tech Stack:** C++17, CMake >= 3.16, doctest (vendored single header), CTest.

## Global Constraints

- C++17 standard, set explicitly in CMake (`CMAKE_CXX_STANDARD 17`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`). No compiler-specific extensions.
- CMake minimum version 3.16 (`cmake_minimum_required(VERSION 3.16)`).
- `codeqa_core` must not depend on Qt in any form (`QString`, `QFile`, `QDir`, `QObject`, `QVariant`, `QJsonDocument`, QML). Standard library only (`std::string`, `std::filesystem::path`, `std::vector`, `std::optional`, `std::variant`, `std::unique_ptr`, `std::string_view`).
- All external dependencies are vendored as source inside the repository (`third_party/`) — never `FetchContent`, never git submodules. This phase vendors doctest; tree-sitter is vendored the same way in a later phase.
- All project code lives inside `namespace codeqa { ... }`, with subnamespaces `codeqa::core`, `codeqa::application`, `codeqa::cli`. Never `using namespace std;`, in headers or at file scope.
- Naming: classes/structs `PascalCase`; functions/methods `camelCase`; local variables `camelCase`; private members `camelCase_` (trailing underscore); constants `camelCase` or `kPascalCase`; namespaces lowercase; class files `PascalCase.h`/`.cpp`.
- `main.cpp` contains no business logic — only argument collection, a call into `runApplication`, and the top-level try/catch barrier.
- No functionality from later phases (file discovery, language detection, parsing, metrics, rules, config, reporting, history) may be implemented now — see `docs/02-phase-01-project-foundation.md` section 4 ("Fuera de alcance") for the explicit exclusion list.
- Warnings-as-errors is off by default (`CODEQA_WARNINGS_AS_ERRORS` option, `OFF`), so it can be turned on later (e.g. in CI) without blocking local development.

---

## File Structure

```text
code-quality-analyzer/
├── CMakeLists.txt
├── cmake/
│   └── CompilerWarnings.cmake
├── third_party/
│   └── doctest/
│       └── doctest.h                          (vendored, not hand-written)
├── include/
│   └── codeqa/
│       ├── core/
│       │   ├── SourceLocation.h
│       │   ├── SourceRange.h
│       │   ├── DiagnosticSeverity.h
│       │   └── Diagnostic.h
│       ├── application/
│       │   ├── ApplicationInfo.h
│       │   ├── GetApplicationInfo.h
│       │   └── Version.h.in                    (configure_file template)
│       └── cli/
│           ├── ExitCode.h
│           └── RunApplication.h
├── src/
│   ├── application/
│   │   └── GetApplicationInfo.cpp
│   └── cli/
│       ├── RunApplication.cpp
│       └── main.cpp
├── tests/
│   ├── CMakeLists.txt
│   ├── main.cpp                                (doctest runner)
│   ├── unit/
│   │   ├── SmokeTest.cpp
│   │   ├── CoreModelsTest.cpp
│   │   ├── GetApplicationInfoTest.cpp
│   │   └── RunApplicationTest.cpp
│   └── integration/
│       └── CheckCliOutput.cmake
├── .gitignore
└── README.md
```

**Notes on deviations from the phase-1 spec document, and why:**

- **`include/codeqa/cli/` is a new subfolder** not shown in the spec's abbreviated repo tree (`docs/02-phase-01-project-foundation.md` section 6 only shows `core/` and `application/` as examples). It's required because `runApplication` must be declared somewhere both `main.cpp` and `tests/unit/RunApplicationTest.cpp` can include (spec section 17.2 requires it to be "a testable function").
- **`RunApplication.cpp` compiles into the `codeqa_core` library**, not a separate library. The spec (section 7) only names three concrete CMake targets (`codeqa_core`, the `codeqa` executable, `codeqa_tests`); there's no fourth "CLI library" target. Section 5.4 says reusable logic goes in "a library" (singular) — since `runApplication` must be linkable from both the `codeqa` executable and `codeqa_tests`, and `codeqa_core` is the only non-executable target defined, it goes there. `main.cpp` itself stays a thin wrapper with no logic, per the same section.
- **Version is generated from CMake via `configure_file`**, not hardcoded as a string literal in `GetApplicationInfo.cpp`. This directly addresses risk 26.7 in the spec ("Versiones no reproducibles" — corrección: "Definir versión estable desde CMake"): the version string exists in exactly one place (`project(... VERSION 0.1.0 ...)` in the root `CMakeLists.txt`).
- **`LICENSE` is intentionally not created in this phase.** It appears in the spec's illustrative repo tree, but no acceptance criterion in section 27 depends on it, and choosing a license is a decision for the project owner, not something to default silently.
- **Tasks 4 and 5 from the spec's own suggested breakdown (`--help`/`--version` vs. unknown-argument handling) are merged into one task (Task 4 below).** `runApplication` must return an `int` on every code path to compile without warnings — there's no correct intermediate state where the unknown-argument branch is simply missing. Splitting them would mean shipping code that doesn't compile cleanly partway through. Task 4 implements and tests all four argument-handling branches (`--help`, `--version`, no arguments, unknown argument) together, which is the smallest coherent, correct unit.

---

### Task 1: Project skeleton, CMake root config, doctest wired, smoke test

**Files:**
- Create: `CMakeLists.txt`
- Create: `tests/CMakeLists.txt`
- Create: `tests/main.cpp`
- Create: `tests/unit/SmokeTest.cpp`
- Create: `third_party/doctest/doctest.h` (vendored)
- Create: `.gitignore`

**Interfaces:**
- Consumes: nothing (first task).
- Produces: a buildable CMake project with a `codeqa_tests` executable and one passing doctest test, registered with CTest. Later tasks add sources to `codeqa_tests` via `tests/CMakeLists.txt` and add new `add_library`/`add_executable` targets to the root `CMakeLists.txt`.

- [ ] **Step 1: Create the directory skeleton and `.gitignore`**

Create empty directories `include/codeqa`, `src`, `cmake`, `third_party`, `tests/unit`, `tests/integration` (they'll be populated by this and later tasks).

Create `.gitignore`:

```text
build/
cmake-build-*/
.vscode/
.idea/
*.user
*.suo
*.VC.db
*.VC.opendb
Testing/
```

- [ ] **Step 2: Write the root `CMakeLists.txt` and `tests/CMakeLists.txt` (without doctest yet — this is the RED step)**

`CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16)

project(
    CodeQualityAnalyzer
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CODEQA_BUILD_TESTS "Build CodeQA tests" ON)
option(CODEQA_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

enable_testing()

if(CODEQA_BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

`tests/CMakeLists.txt`:

```cmake
add_executable(codeqa_tests
    main.cpp
    unit/SmokeTest.cpp
)

target_include_directories(codeqa_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party
)

add_test(NAME codeqa_tests COMMAND codeqa_tests)
```

`tests/main.cpp`:

```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
```

`tests/unit/SmokeTest.cpp`:

```cpp
#include "doctest/doctest.h"

TEST_CASE("doctest is wired correctly")
{
    CHECK(1 + 1 == 2);
}
```

- [ ] **Step 3: Try to configure and build to confirm it fails (doctest.h doesn't exist yet)**

Run:

```bash
cmake -S . -B build
cmake --build build
```

Expected: configure succeeds, but the build **fails** with a "doctest/doctest.h: No such file or directory" (or equivalent) compiler error from `tests/main.cpp` / `tests/unit/SmokeTest.cpp`.

- [ ] **Step 4: Vendor doctest**

This step downloads a file — ask the user for confirmation before fetching (per the download-permission policy), then:

Download `https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h` and save it to `third_party/doctest/doctest.h` exactly as downloaded (do not edit it).

Verify the file was saved correctly:

```bash
grep "DOCTEST_VERSION_STR" third_party/doctest/doctest.h
```

Expected: a line defining `DOCTEST_VERSION_STR` containing `"2.4.11"`.

- [ ] **Step 5: Build and run tests to verify the smoke test passes**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: build succeeds, `ctest` reports 1 test run, 1 passed (`codeqa_tests`).

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt tests/CMakeLists.txt tests/main.cpp tests/unit/SmokeTest.cpp third_party/doctest/doctest.h .gitignore
git commit -m "feat: project skeleton with CMake and doctest wired up"
```

---

### Task 2: Core value types (`SourceLocation`, `SourceRange`, `DiagnosticSeverity`, `Diagnostic`)

**Files:**
- Create: `include/codeqa/core/SourceLocation.h`
- Create: `include/codeqa/core/SourceRange.h`
- Create: `include/codeqa/core/DiagnosticSeverity.h`
- Create: `include/codeqa/core/Diagnostic.h`
- Create: `tests/unit/CoreModelsTest.cpp`
- Modify: `CMakeLists.txt` (add the `codeqa_core` INTERFACE library)
- Modify: `tests/CMakeLists.txt` (link `codeqa_core`, add the new test file)

**Interfaces:**
- Consumes: nothing new.
- Produces: `codeqa::core::SourceLocation{filePath, line, column, offset}`, `codeqa::core::SourceRange{begin, end}`, `codeqa::core::DiagnosticSeverity{Information, Warning, Error}`, `codeqa::core::Diagnostic{code, severity, message, location}`. Task 4's error-reporting code and all later phases build on these exact field names.

- [ ] **Step 1: Write the failing test**

`tests/unit/CoreModelsTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/core/Diagnostic.h"
#include "codeqa/core/DiagnosticSeverity.h"
#include "codeqa/core/SourceLocation.h"
#include "codeqa/core/SourceRange.h"

#include <filesystem>

TEST_CASE("SourceLocation has sensible defaults")
{
    const codeqa::core::SourceLocation location;

    CHECK(location.filePath.empty());
    CHECK(location.line == 1);
    CHECK(location.column == 1);
    CHECK(location.offset == 0);
}

TEST_CASE("SourceLocation stores the values it is constructed with")
{
    const codeqa::core::SourceLocation location{"src/main.cpp", 12, 5, 84};

    CHECK(location.filePath == std::filesystem::path{"src/main.cpp"});
    CHECK(location.line == 12);
    CHECK(location.column == 5);
    CHECK(location.offset == 84);
}

TEST_CASE("SourceRange stores begin and end locations")
{
    const codeqa::core::SourceLocation begin{"src/main.cpp", 1, 1, 0};
    const codeqa::core::SourceLocation end{"src/main.cpp", 3, 1, 40};

    const codeqa::core::SourceRange range{begin, end};

    CHECK(range.begin.line == 1);
    CHECK(range.end.line == 3);
}

TEST_CASE("Diagnostic defaults to Error severity")
{
    const codeqa::core::Diagnostic diagnostic;

    CHECK(diagnostic.severity == codeqa::core::DiagnosticSeverity::Error);
    CHECK(diagnostic.code.empty());
    CHECK(diagnostic.message.empty());
}

TEST_CASE("Diagnostic stores the values it is constructed with")
{
    const codeqa::core::Diagnostic diagnostic{
        "CLI001",
        codeqa::core::DiagnosticSeverity::Warning,
        "Unknown option '--unknown'.",
        codeqa::core::SourceLocation{}};

    CHECK(diagnostic.code == "CLI001");
    CHECK(diagnostic.severity == codeqa::core::DiagnosticSeverity::Warning);
    CHECK(diagnostic.message == "Unknown option '--unknown'.");
}
```

Add it to `tests/CMakeLists.txt`:

```cmake
add_executable(codeqa_tests
    main.cpp
    unit/SmokeTest.cpp
    unit/CoreModelsTest.cpp
)

target_include_directories(codeqa_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(codeqa_tests PRIVATE codeqa_core)

add_test(NAME codeqa_tests COMMAND codeqa_tests)
```

- [ ] **Step 2: Run it to verify it fails**

```bash
cmake -S . -B build
cmake --build build
```

Expected: FAIL — `codeqa/core/Diagnostic.h: No such file or directory` (the headers don't exist yet, and `codeqa_core` isn't even defined in the root `CMakeLists.txt` yet, so this will also fail with an unknown-target error until Step 3 adds it).

- [ ] **Step 3: Implement the four headers**

`include/codeqa/core/SourceLocation.h`:

```cpp
#pragma once

#include <cstddef>
#include <filesystem>

namespace codeqa::core
{

struct SourceLocation
{
    std::filesystem::path filePath;
    std::size_t line{1};
    std::size_t column{1};
    std::size_t offset{0};
};

} // namespace codeqa::core
```

`include/codeqa/core/SourceRange.h`:

```cpp
#pragma once

#include "codeqa/core/SourceLocation.h"

namespace codeqa::core
{

struct SourceRange
{
    SourceLocation begin;
    SourceLocation end;
};

} // namespace codeqa::core
```

`include/codeqa/core/DiagnosticSeverity.h`:

```cpp
#pragma once

namespace codeqa::core
{

enum class DiagnosticSeverity
{
    Information,
    Warning,
    Error
};

} // namespace codeqa::core
```

`include/codeqa/core/Diagnostic.h`:

```cpp
#pragma once

#include "codeqa/core/DiagnosticSeverity.h"
#include "codeqa/core/SourceLocation.h"

#include <string>

namespace codeqa::core
{

struct Diagnostic
{
    std::string code;
    DiagnosticSeverity severity{DiagnosticSeverity::Error};
    std::string message;
    SourceLocation location;
};

} // namespace codeqa::core
```

Add `codeqa_core` to the root `CMakeLists.txt` (insert after the `option(...)` lines, before `enable_testing()`):

```cmake
add_library(codeqa_core INTERFACE)
target_include_directories(codeqa_core INTERFACE ${CMAKE_SOURCE_DIR}/include)
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: PASS, `codeqa_tests` reports all test cases (smoke test + 5 new core-model cases) green.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/core tests/unit/CoreModelsTest.cpp tests/CMakeLists.txt CMakeLists.txt
git commit -m "feat: add core value types (SourceLocation, SourceRange, DiagnosticSeverity, Diagnostic)"
```

---

### Task 3: `ApplicationInfo` and `GetApplicationInfo` use case

**Files:**
- Create: `include/codeqa/application/ApplicationInfo.h`
- Create: `include/codeqa/application/GetApplicationInfo.h`
- Create: `include/codeqa/application/Version.h.in`
- Create: `src/application/GetApplicationInfo.cpp`
- Create: `tests/unit/GetApplicationInfoTest.cpp`
- Modify: `CMakeLists.txt` (convert `codeqa_core` from `INTERFACE` to `STATIC`, add `configure_file` for the version header)
- Modify: `tests/CMakeLists.txt` (add the new test file)

**Interfaces:**
- Consumes: nothing new (doesn't use `codeqa::core` types).
- Produces: `codeqa::application::ApplicationInfo{name, version}` (both `std::string`) and `codeqa::application::GetApplicationInfo::execute() const -> ApplicationInfo`. Task 4's `--version` handling calls this exact type and method.

- [ ] **Step 1: Write the failing test**

`tests/unit/GetApplicationInfoTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/application/GetApplicationInfo.h"

TEST_CASE("GetApplicationInfo returns the expected name and version")
{
    const codeqa::application::GetApplicationInfo useCase;

    const codeqa::application::ApplicationInfo info = useCase.execute();

    CHECK(info.name == "Code Quality Analyzer");
    CHECK(info.version == "0.1.0");
}
```

Add it to `tests/CMakeLists.txt`'s `add_executable(codeqa_tests ...)` source list (alongside the existing three files):

```cmake
add_executable(codeqa_tests
    main.cpp
    unit/SmokeTest.cpp
    unit/CoreModelsTest.cpp
    unit/GetApplicationInfoTest.cpp
)
```

- [ ] **Step 2: Run it to verify it fails**

```bash
cmake -S . -B build
cmake --build build
```

Expected: FAIL — `codeqa/application/GetApplicationInfo.h: No such file or directory`.

- [ ] **Step 3: Implement `ApplicationInfo`, the version header, and `GetApplicationInfo`**

`include/codeqa/application/ApplicationInfo.h`:

```cpp
#pragma once

#include <string>

namespace codeqa::application
{

struct ApplicationInfo
{
    std::string name;
    std::string version;
};

} // namespace codeqa::application
```

`include/codeqa/application/Version.h.in` (CMake template — `@PROJECT_VERSION@` is substituted by `configure_file`):

```cpp
#pragma once

#define CODEQA_VERSION "@PROJECT_VERSION@"
```

`include/codeqa/application/GetApplicationInfo.h`:

```cpp
#pragma once

#include "codeqa/application/ApplicationInfo.h"

namespace codeqa::application
{

class GetApplicationInfo
{
public:
    [[nodiscard]] ApplicationInfo execute() const;
};

} // namespace codeqa::application
```

`src/application/GetApplicationInfo.cpp`:

```cpp
#include "codeqa/application/GetApplicationInfo.h"

#include "codeqa/application/Version.h"

namespace codeqa::application
{

ApplicationInfo GetApplicationInfo::execute() const
{
    return ApplicationInfo{"Code Quality Analyzer", CODEQA_VERSION};
}

} // namespace codeqa::application
```

Replace the `codeqa_core` declaration in the root `CMakeLists.txt` (the two lines added in Task 2) with:

```cmake
configure_file(
    ${CMAKE_SOURCE_DIR}/include/codeqa/application/Version.h.in
    ${CMAKE_BINARY_DIR}/generated/codeqa/application/Version.h
    @ONLY
)

add_library(codeqa_core STATIC
    src/application/GetApplicationInfo.cpp
)
target_include_directories(codeqa_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/generated
)
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: PASS, all test cases green (smoke + core models + `GetApplicationInfo`).

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/application src/application tests/unit/GetApplicationInfoTest.cpp tests/CMakeLists.txt CMakeLists.txt
git commit -m "feat: add ApplicationInfo and GetApplicationInfo use case"
```

---

### Task 4: CLI `runApplication` (help, version, no-args, unknown-arg) and the `codeqa` executable

**Files:**
- Create: `include/codeqa/cli/ExitCode.h`
- Create: `include/codeqa/cli/RunApplication.h`
- Create: `src/cli/RunApplication.cpp`
- Create: `src/cli/main.cpp`
- Create: `tests/unit/RunApplicationTest.cpp`
- Modify: `CMakeLists.txt` (add `RunApplication.cpp` to `codeqa_core`, add the `codeqa` executable target)
- Modify: `tests/CMakeLists.txt` (add the new test file)

**Interfaces:**
- Consumes: `codeqa::application::GetApplicationInfo` and `codeqa::application::ApplicationInfo` (Task 3).
- Produces: `codeqa::cli::ExitCode{Success=0, InvalidArguments=2, InternalError=4}` and `codeqa::cli::runApplication(const std::vector<std::string>& arguments, std::ostream& output, std::ostream& errorOutput) -> int`. Task 6's integration tests invoke the compiled `codeqa` binary that wraps this function; no later task calls `runApplication` directly.

- [ ] **Step 1: Write the failing tests**

`tests/unit/RunApplicationTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/cli/ExitCode.h"
#include "codeqa/cli/RunApplication.h"

#include <sstream>
#include <string>
#include <vector>

namespace
{

struct RunResult
{
    int exitCode{};
    std::string output;
    std::string errorOutput;
};

RunResult run(const std::vector<std::string>& arguments)
{
    std::ostringstream output;
    std::ostringstream errorOutput;

    const int exitCode = codeqa::cli::runApplication(arguments, output, errorOutput);

    return RunResult{exitCode, output.str(), errorOutput.str()};
}

} // namespace

TEST_CASE("--help prints usage and returns success")
{
    const RunResult result = run({"--help"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("Usage:") != std::string::npos);
    CHECK(result.errorOutput.empty());
}

TEST_CASE("-h prints usage and returns success")
{
    const RunResult result = run({"-h"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("Usage:") != std::string::npos);
}

TEST_CASE("--version prints the application name and version and returns success")
{
    const RunResult result = run({"--version"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output == "Code Quality Analyzer 0.1.0\n");
}

TEST_CASE("no arguments prints usage and returns success")
{
    const RunResult result = run({});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("Usage:") != std::string::npos);
}

TEST_CASE("an unknown option prints a CLI001 error to stderr and returns InvalidArguments")
{
    const RunResult result = run({"--unknown"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI001") != std::string::npos);
    CHECK(result.errorOutput.find("--unknown") != std::string::npos);
    CHECK(result.output.empty());
}

TEST_CASE("stdout and stderr stay separate")
{
    const RunResult helpResult = run({"--help"});
    CHECK(helpResult.errorOutput.empty());

    const RunResult errorResult = run({"--bogus"});
    CHECK(errorResult.output.empty());
}
```

Add it to `tests/CMakeLists.txt`'s source list:

```cmake
add_executable(codeqa_tests
    main.cpp
    unit/SmokeTest.cpp
    unit/CoreModelsTest.cpp
    unit/GetApplicationInfoTest.cpp
    unit/RunApplicationTest.cpp
)
```

- [ ] **Step 2: Run it to verify it fails**

```bash
cmake -S . -B build
cmake --build build
```

Expected: FAIL — `codeqa/cli/RunApplication.h: No such file or directory`.

- [ ] **Step 3: Implement `ExitCode`, `RunApplication`, and `main.cpp`**

`include/codeqa/cli/ExitCode.h`:

```cpp
#pragma once

namespace codeqa::cli
{

enum class ExitCode : int
{
    Success = 0,
    InvalidArguments = 2,
    InternalError = 4
};

} // namespace codeqa::cli
```

`include/codeqa/cli/RunApplication.h`:

```cpp
#pragma once

#include <ostream>
#include <string>
#include <vector>

namespace codeqa::cli
{

int runApplication(
    const std::vector<std::string>& arguments,
    std::ostream& output,
    std::ostream& errorOutput);

} // namespace codeqa::cli
```

`src/cli/RunApplication.cpp`:

```cpp
#include "codeqa/cli/RunApplication.h"

#include "codeqa/application/ApplicationInfo.h"
#include "codeqa/application/GetApplicationInfo.h"
#include "codeqa/cli/ExitCode.h"

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
              "\n"
              "Options:\n"
              "  -h, --help       Show this help.\n"
              "  -v, --version    Show application version.\n";
}

void printVersion(std::ostream& output)
{
    const codeqa::application::GetApplicationInfo getApplicationInfo;
    const codeqa::application::ApplicationInfo info = getApplicationInfo.execute();

    output << info.name << ' ' << info.version << '\n';
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

    errorOutput << "Error [CLI001]: Unknown option '" << firstArgument << "'.\n"
                   "Use 'codeqa --help' for usage.\n";
    return static_cast<int>(ExitCode::InvalidArguments);
}

} // namespace codeqa::cli
```

`src/cli/main.cpp`:

```cpp
#include "codeqa/cli/RunApplication.h"

#include "codeqa/cli/ExitCode.h"

#include <exception>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    const std::vector<std::string> arguments(argv + 1, argv + argc);

    try
    {
        return codeqa::cli::runApplication(arguments, std::cout, std::cerr);
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Internal error: " << exception.what() << '\n';
        return static_cast<int>(codeqa::cli::ExitCode::InternalError);
    }
    catch (...)
    {
        std::cerr << "Internal error: unknown exception.\n";
        return static_cast<int>(codeqa::cli::ExitCode::InternalError);
    }
}
```

Update the root `CMakeLists.txt`: add `src/cli/RunApplication.cpp` to `codeqa_core`'s sources, and add the `codeqa` executable target right after it:

```cmake
add_library(codeqa_core STATIC
    src/application/GetApplicationInfo.cpp
    src/cli/RunApplication.cpp
)
target_include_directories(codeqa_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/generated
)

add_executable(codeqa src/cli/main.cpp)
target_link_libraries(codeqa PRIVATE codeqa_core)
```

(This replaces the `add_library(codeqa_core STATIC ...)` block from Task 3 and adds the two new lines after it — `configure_file(...)` above it stays unchanged.)

- [ ] **Step 4: Run tests to verify they pass, then check the real binary**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: PASS, all test cases green.

Then manually verify the real executable (informal check, not yet a CTest — that's Task 6):

```bash
./build/codeqa --version
./build/codeqa --help
```

Expected: `Code Quality Analyzer 0.1.0` and the usage text, respectively (path may be `build/Debug/codeqa.exe` or similar depending on generator/platform).

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/cli src/cli tests/unit/RunApplicationTest.cpp tests/CMakeLists.txt CMakeLists.txt
git commit -m "feat: add runApplication CLI logic and the codeqa executable"
```

---

### Task 5: Compiler warnings configuration

**Files:**
- Create: `cmake/CompilerWarnings.cmake`
- Modify: `CMakeLists.txt` (include the new file, apply `codeqa_enable_warnings` to `codeqa_core` and `codeqa`)

**Interfaces:**
- Consumes: the `CODEQA_WARNINGS_AS_ERRORS` option (already defined in Task 1).
- Produces: a `codeqa_enable_warnings(<target>)` CMake function. No later task needs to call it again — it's applied once per target here.

- [ ] **Step 1: Write `cmake/CompilerWarnings.cmake`**

```cmake
function(codeqa_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wshadow
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Woverloaded-virtual
        )
    endif()

    if(CODEQA_WARNINGS_AS_ERRORS)
        if(MSVC)
            target_compile_options(${target} PRIVATE /WX)
        else()
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()
```

- [ ] **Step 2: Wire it into the root `CMakeLists.txt`**

Add near the top, right after the `option(...)` declarations:

```cmake
include(cmake/CompilerWarnings.cmake)
```

Add right after `add_library(codeqa_core STATIC ...)` and its `target_include_directories` call:

```cmake
codeqa_enable_warnings(codeqa_core)
```

Add right after `add_executable(codeqa src/cli/main.cpp)` and its `target_link_libraries` call:

```cmake
codeqa_enable_warnings(codeqa)
```

- [ ] **Step 3: Rebuild from scratch and confirm no warnings**

```bash
rm -rf build
cmake -S . -B build
cmake --build build 2>&1 | tee build.log
```

Expected: build succeeds with zero warnings from `codeqa_core` or `codeqa` sources (warnings originating from the vendored `third_party/doctest/doctest.h` itself, if any, are acceptable and not something this project controls).

Then verify `-Werror`/`/WX` works when explicitly enabled:

```bash
rm -rf build
cmake -S . -B build -DCODEQA_WARNINGS_AS_ERRORS=ON
cmake --build build
```

Expected: still builds successfully (since there are no warnings to escalate).

- [ ] **Step 4: Run the full test suite once more**

```bash
ctest --test-dir build --output-on-failure
```

Expected: PASS, unaffected by the warnings change.

- [ ] **Step 5: Commit**

```bash
git add cmake/CompilerWarnings.cmake CMakeLists.txt
git commit -m "build: centralize compiler warnings configuration"
```

---

### Task 6: CTest integration tests against the real binary

**Files:**
- Create: `tests/integration/CheckCliOutput.cmake`
- Modify: `tests/CMakeLists.txt` (register two `add_test` entries that run the compiled `codeqa` binary)

**Interfaces:**
- Consumes: the `codeqa` executable target (Task 4), via the `$<TARGET_FILE:codeqa>` generator expression.
- Produces: two named CTest tests (`codeqa_integration_version`, `codeqa_integration_help`). Nothing later depends on these directly — they're a terminal verification.

- [ ] **Step 1: Write the check script (this *is* the test)**

`tests/integration/CheckCliOutput.cmake`:

```cmake
# Runs the real codeqa executable and checks stdout and exit code.
# Required variables (passed via -D on the command line):
#   CODEQA_EXE               - path to the codeqa executable
#   CODEQA_ARGS               - single argument to pass to it
#   CODEQA_EXPECTED_OUTPUT    - substring that must appear in stdout
#   CODEQA_EXPECTED_EXIT_CODE - expected process exit code

execute_process(
    COMMAND ${CODEQA_EXE} ${CODEQA_ARGS}
    OUTPUT_VARIABLE actualOutput
    RESULT_VARIABLE actualExitCode
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT actualExitCode EQUAL CODEQA_EXPECTED_EXIT_CODE)
    message(FATAL_ERROR
        "Expected exit code ${CODEQA_EXPECTED_EXIT_CODE} but got ${actualExitCode}. "
        "Output was:\n${actualOutput}")
endif()

string(FIND "${actualOutput}" "${CODEQA_EXPECTED_OUTPUT}" matchPosition)

if(matchPosition EQUAL -1)
    message(FATAL_ERROR
        "Expected output to contain:\n${CODEQA_EXPECTED_OUTPUT}\n"
        "Actual output was:\n${actualOutput}")
endif()

message(STATUS "CLI integration check passed.")
```

Register it in `tests/CMakeLists.txt` (append at the end, after the existing `add_test(NAME codeqa_tests ...)`):

```cmake
add_test(
    NAME codeqa_integration_version
    COMMAND ${CMAKE_COMMAND}
        -DCODEQA_EXE=$<TARGET_FILE:codeqa>
        -DCODEQA_ARGS=--version
        "-DCODEQA_EXPECTED_OUTPUT=Code Quality Analyzer 0.1.0"
        -DCODEQA_EXPECTED_EXIT_CODE=0
        -P ${CMAKE_CURRENT_SOURCE_DIR}/integration/CheckCliOutput.cmake
)

add_test(
    NAME codeqa_integration_help
    COMMAND ${CMAKE_COMMAND}
        -DCODEQA_EXE=$<TARGET_FILE:codeqa>
        -DCODEQA_ARGS=--help
        "-DCODEQA_EXPECTED_OUTPUT=Usage:"
        -DCODEQA_EXPECTED_EXIT_CODE=0
        -P ${CMAKE_CURRENT_SOURCE_DIR}/integration/CheckCliOutput.cmake
)
```

- [ ] **Step 2: Run it to verify it fails (before the script exists, or to confirm current CTest count)**

```bash
cmake -S . -B build
ctest --test-dir build -N
```

Expected before this task's script is saved: CMake configure error ("file ... CheckCliOutput.cmake does not exist"). Confirm this is the failure you see before Step 1's script file is actually written to disk (if you write the script and CMakeLists.txt together, skip straight to Step 3 — the point is not to register tests that reference a nonexistent script).

- [ ] **Step 3: Build and run the new tests**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: PASS — CTest now reports 3 tests total (`codeqa_tests`, `codeqa_integration_version`, `codeqa_integration_help`), all green.

- [ ] **Step 4: Commit**

```bash
git add tests/integration/CheckCliOutput.cmake tests/CMakeLists.txt
git commit -m "test: add CTest integration checks for the real codeqa binary"
```

---

### Task 7: README, `.gitignore` review, and clean-build verification

**Files:**
- Modify: `README.md` (create with final content — didn't exist before this task)
- Modify: `.gitignore` (review only, created in Task 1)

**Interfaces:**
- Consumes: nothing (documentation only).
- Produces: nothing consumed by other tasks — this is the phase's final deliverable check.

- [ ] **Step 1: Write `README.md`**

```markdown
# Code Quality Analyzer

Static analysis tool for measuring and improving the quality, maintainability, and evolution of C and C++ source projects. It targets issues that don't prevent compilation but progressively degrade design: excessively long functions, high cyclomatic complexity, deep nesting, poor naming, duplication, and more.

This repository is currently in the **foundation phase**: the project builds, runs, and tests itself, but does not yet analyze any C or C++ source code.

## Requisitos

- Compilador compatible con C++17 (GCC, Clang o MSVC).
- CMake >= 3.16.

## Compilación

\`\`\`bash
cmake -S . -B build
cmake --build build
\`\`\`

## Tests

\`\`\`bash
ctest --test-dir build --output-on-failure
\`\`\`

## Ejecución

\`\`\`bash
./build/codeqa --help
./build/codeqa --version
\`\`\`

En Windows, el ejecutable se genera normalmente en `build/Debug/codeqa.exe` o `build/Release/codeqa.exe`, según el generador y la configuración utilizados.

## Estado

Proyecto en fase de fundación. Todavía no analiza código C o C++.
```

(Write the actual file without the backslash-escaped triple backticks above — those are only escaped here so they don't terminate this plan document's own code fence.)

- [ ] **Step 2: Confirm `.gitignore` is complete**

Open `.gitignore` (written in Task 1) and confirm it still contains:

```text
build/
cmake-build-*/
.vscode/
.idea/
*.user
*.suo
*.VC.db
*.VC.opendb
Testing/
```

No changes expected — this step is a verification, not an edit.

- [ ] **Step 3: Verify the README instructions work from a clean build directory**

```bash
rm -rf build
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/codeqa --help
./build/codeqa --version
```

Expected: every command in the README succeeds exactly as written, using only the README's own instructions (no undocumented steps).

- [ ] **Step 4: Commit**

```bash
git add README.md
git commit -m "docs: complete README with verified build, test, and run instructions"
```

---

## Self-Review

**Spec coverage** (against `docs/02-phase-01-project-foundation.md`):

- Repo structure, CMake config, C++17 standard → Task 1.
- `codeqa_core` library → Tasks 2 (INTERFACE) and 3 (converted to STATIC).
- Core models (`SourceLocation`, `SourceRange`, `DiagnosticSeverity`, `Diagnostic`) → Task 2.
- `ApplicationInfo` / `GetApplicationInfo` → Task 3.
- CLI (`--help`, `--version`, no-args, unknown-arg, exit codes, `runApplication`, try/catch barrier in `main.cpp`) → Task 4.
- doctest integration → Task 1 (wired), Tasks 2–4 (tests added).
- Compiler warnings (GCC/Clang/MSVC sets, `CODEQA_WARNINGS_AS_ERRORS`) → Task 5.
- CTest integration tests against the real binary → Task 6.
- README and `.gitignore` → Task 1 (`.gitignore`) and Task 7 (README + final verification).
- All 16 acceptance checkboxes in spec section 27 are covered by the tasks above; none require functionality explicitly excluded in section 4.

**Placeholder scan:** no "TBD"/"TODO"/"implement later" markers; every code block is complete, compilable content; every test has real assertions.

**Type consistency:** `codeqa::core::SourceLocation{filePath, line, column, offset}` (Task 2) matches its use in `Diagnostic` (Task 2) and is not touched again. `codeqa::application::ApplicationInfo{name, version}` and `GetApplicationInfo::execute()` (Task 3) match their use in `RunApplication.cpp`'s `printVersion` (Task 4). `codeqa::cli::ExitCode` (Task 4) values (`Success=0`, `InvalidArguments=2`, `InternalError=4`) match both `RunApplication.cpp` and `main.cpp`. `runApplication`'s signature (Task 4 header) matches its call site in `main.cpp` and its test harness in `RunApplicationTest.cpp`.

---

**Plan complete and saved to `docs/superpowers/plans/2026-08-02-fase-1-fundacion-tecnica.md`. Two execution options:**

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

**Which approach?**
