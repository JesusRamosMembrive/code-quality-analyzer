# Fase 4a — tree-sitter vendorizado y parseo Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Vendor the tree-sitter runtime and its C/C++ grammars, compile them without network access, parse every discovered source file into a CST, and turn the regions the grammar cannot interpret into `Diagnostic`s — exposed as a new `codeqa parse <path>` subcommand and verified against the Fase 0 corpus.

**Architecture:** A new `codeqa::frontend` layer is the only code that knows tree-sitter exists. `TreeSitterParser` is a RAII wrapper over the C API (`TSParser*`/`TSTree*` need explicit deletion); `SelectGrammar` maps Fase 3's `Language` onto a grammar, defaulting ambiguous headers to C++; `ParseSourceFile` reads a file, parses it, walks the tree, and emits one `Diagnostic` per top-level `ERROR`/`MISSING` node without descending into an already-reported region. No tree-sitter type escapes the frontend — `ParsedFile` carries diagnostics and metadata, not the tree.

**Tech Stack:** C++17, tree-sitter runtime + `tree-sitter-c` + `tree-sitter-cpp` (vendored C sources), CMake, doctest, CTest.

## Global Constraints

Carried forward from Fases 1–3 (still binding):

- C++17, no compiler-specific extensions, for all **project** code.
- `codeqa_core` must not depend on Qt in any form.
- All external dependencies vendored as source in `third_party/` — never `FetchContent`, never git submodules.
- All project code lives inside `namespace codeqa { ... }`. Never `using namespace std;`.
- Naming: types `PascalCase`; functions `camelCase`; namespaces lowercase; files `PascalCase.h`/`.cpp`.
- Use `${PROJECT_SOURCE_DIR}` / `${PROJECT_BINARY_DIR}` in CMake, not `CMAKE_SOURCE_DIR` / `CMAKE_BINARY_DIR`.
- CMake minimum 3.16.

New, from `docs/superpowers/specs/2026-08-05-fase-4a-treesitter-parsing-design.md`:

- **`codeqa::frontend` is the only namespace that may include tree-sitter headers.** No `TSParser`, `TSTree`, `TSNode` or any other tree-sitter type appears in `codeqa::core`, `codeqa::application`, `codeqa::infrastructure`, or `codeqa::cli`.
- **All tree-sitter handles are RAII-owned.** `TSParser*` and `TSTree*` require `ts_parser_delete`/`ts_tree_delete`; the owning types are non-copyable and movable, and no raw handle escapes the wrapper.
- **A file whose detected `Language` is `Unknown` is parsed with the C++ grammar**, and the result records that this is why.
- **One `Diagnostic` per top-level `ERROR`/`MISSING` node; never descend into a node already reported.**
- **Diagnostic codes:** `PARSE001` (ERROR node), `PARSE002` (MISSING node), `PARSE003` (file unreadable). All severity `Error`.
- **tree-sitter row/column are 0-based; `codeqa::core::SourceLocation` is 1-based.** Add 1 to each at the boundary.
- **Vendored third-party libraries are excluded from `codeqa_enable_warnings`** — generated third-party C would not survive `-Werror`. Same rule already applied to doctest.
- **`parse` returns `ExitCode::Success` even when files have diagnostics.** Parse errors are information, not a failed run; the quality gate is Fase 12.
- **No `.scm` queries, no structural extraction, no `nlohmann/json`, no corpus runner** — all Fase 4b.

---

## File Structure

```text
third_party/
├── README.md                              (new: records every vendored dependency)
├── doctest/doctest.h                       (existing)
├── tree-sitter/lib/                         (new: runtime)
├── tree-sitter-c/src/                       (new: C grammar)
└── tree-sitter-cpp/src/                     (new: C++ grammar)

include/codeqa/frontend/
├── Grammar.h                  enum class Grammar { C, Cpp }
├── SelectGrammar.h            GrammarSelection selectGrammar(core::Language)
├── ParsedFile.h               struct ParsedFile
├── TreeSitterParser.h         RAII wrapper (the only tree-sitter-aware header)
└── ParseSourceFile.h          ParsedFile parseSourceFile(path, Grammar, reason)

src/frontend/
├── SelectGrammar.cpp
├── TreeSitterParser.cpp
└── ParseSourceFile.cpp

src/cli/RunApplication.cpp     (modified: `parse` subcommand, Detected languages: header)

tests/unit/
├── SelectGrammarTest.cpp       (new)
├── TreeSitterParserTest.cpp    (new)
├── ParseSourceFileTest.cpp     (new)
├── CorpusParsingTest.cpp       (new: the corpus-wide assertion)
└── RunApplicationTest.cpp      (modified)

CMakeLists.txt                  (modified: LANGUAGES C CXX, three third-party libs, frontend sources)
tests/CMakeLists.txt            (modified: new test files, corpus path definition)
README.md                       (modified: documents `parse`)
```

---

### Task 1: Vendor tree-sitter and both grammars, wire CMake, prove it parses

**Files:**
- Create: `third_party/README.md`
- Create: `third_party/tree-sitter/` (downloaded runtime sources)
- Create: `third_party/tree-sitter-c/src/` (downloaded)
- Create: `third_party/tree-sitter-cpp/src/` (downloaded)
- Modify: `CMakeLists.txt`
- Create: `tests/unit/TreeSitterSmokeTest.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing from earlier phases.
- Produces: CMake targets `tree_sitter`, `tree_sitter_c`, `tree_sitter_cpp`, linkable by `codeqa_core`. The two grammar entry points `const TSLanguage *tree_sitter_c(void)` and `const TSLanguage *tree_sitter_cpp(void)` — Tasks 2 and 3 call these. The include path `third_party/tree-sitter/lib/include` provides `<tree_sitter/api.h>`.

**This task requires downloading from the network.** The project owner must grant explicit permission before any download, exactly as was done for doctest in Fase 1. If you have not been told the permission is granted, stop and ask.

- [ ] **Step 1: Download the runtime and both grammars**

Three repositories are needed. Take the **latest stable release tag** of each, not `master`, and record exactly what you took.

```bash
mkdir -p third_party/tree-sitter third_party/tree-sitter-c third_party/tree-sitter-cpp
```

From `https://github.com/tree-sitter/tree-sitter` (MIT), copy only:
- `lib/src/` — the whole directory
- `lib/include/` — the whole directory

From `https://github.com/tree-sitter/tree-sitter-c` (MIT), copy only:
- `src/parser.c`
- `src/tree_sitter/parser.h`

From `https://github.com/tree-sitter/tree-sitter-cpp` (MIT), copy only:
- `src/parser.c`
- `src/scanner.c`
- `src/tree_sitter/parser.h`

Do **not** copy: `bindings/`, `grammar.js`, `test/`, `.github/`, `Cargo.toml`, `package.json`, or anything else. Do not edit any copied file.

Note: `tree-sitter-cpp` has an external scanner (`scanner.c`); `tree-sitter-c` does not. If `tree-sitter-c` turns out to ship a `scanner.c` in the version you take, copy it too and note it in your report.

- [ ] **Step 2: Verify grammar/runtime ABI compatibility before writing any CMake**

A grammar generated for a newer tree-sitter than the runtime you vendored will compile and then fail at runtime. Check the numbers rather than assuming:

```bash
grep -n "LANGUAGE_VERSION" third_party/tree-sitter-c/src/parser.c | head -3
grep -n "LANGUAGE_VERSION" third_party/tree-sitter-cpp/src/parser.c | head -3
grep -rn "TREE_SITTER_LANGUAGE_VERSION" third_party/tree-sitter/lib/include/tree_sitter/api.h
```

Each `parser.c` defines `LANGUAGE_VERSION`. `api.h` defines `TREE_SITTER_LANGUAGE_VERSION` and `TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION`. Both grammars' `LANGUAGE_VERSION` must fall within the runtime's `[MIN_COMPATIBLE, LANGUAGE_VERSION]` range, inclusive.

If they do not, take a different release of whichever side is out of range and re-check. Record the three version numbers in your report.

- [ ] **Step 3: Record the provenance**

`third_party/README.md`:

```markdown
# Dependencias vendorizadas

Todas las dependencias externas se copian como fuente dentro de este directorio.
No se usa `FetchContent` ni submodules: la compilación debe ser reproducible sin
acceso a red.

Ningún archivo de este directorio se edita a mano. Para actualizar una
dependencia se sustituyen sus archivos por los de la nueva versión y se
actualiza su entrada aquí.

## doctest

| Campo | Valor |
|---|---|
| Versión | 2.4.11 |
| Origen | https://github.com/doctest/doctest |
| Licencia | MIT |
| Archivos | `doctest/doctest.h` |
| Para qué | Framework de tests unitarios |

## tree-sitter (runtime)

| Campo | Valor |
|---|---|
| Versión | <la que descargaste> |
| Origen | https://github.com/tree-sitter/tree-sitter |
| Licencia | MIT |
| Archivos | `tree-sitter/lib/src/`, `tree-sitter/lib/include/` |
| Para qué | Motor de parseo incremental y tolerante a errores |

## tree-sitter-c

| Campo | Valor |
|---|---|
| Versión | <la que descargaste> |
| Origen | https://github.com/tree-sitter/tree-sitter-c |
| Licencia | MIT |
| Archivos | `tree-sitter-c/src/parser.c`, `tree-sitter-c/src/tree_sitter/parser.h` |
| LANGUAGE_VERSION | <el número que verificaste> |
| Para qué | Gramática de C |

## tree-sitter-cpp

| Campo | Valor |
|---|---|
| Versión | <la que descargaste> |
| Origen | https://github.com/tree-sitter/tree-sitter-cpp |
| Licencia | MIT |
| Archivos | `tree-sitter-cpp/src/parser.c`, `tree-sitter-cpp/src/scanner.c`, `tree-sitter-cpp/src/tree_sitter/parser.h` |
| LANGUAGE_VERSION | <el número que verificaste> |
| Para qué | Gramática de C++ |

El runtime vendorizado admite `LANGUAGE_VERSION` en el rango
`[<MIN_COMPATIBLE>, <LANGUAGE_VERSION>]`; ambas gramáticas están dentro.
```

Replace every `<...>` with the real value. Leaving a placeholder in a committed file is a defect.

- [ ] **Step 4: Wire CMake**

In `CMakeLists.txt`, change the `project()` line to enable C:

```cmake
project(
    CodeQualityAnalyzer
    VERSION 0.1.0
    LANGUAGES C CXX
)
```

Then add the three libraries, before `add_library(codeqa_core ...)`:

```cmake
# Vendored third-party C libraries. Deliberately excluded from
# codeqa_enable_warnings: parser.c files are generated code and would not
# survive -Werror. Same rule already applied to doctest.
#
# The runtime builds from lib.c alone: it #includes every other .c file in
# lib/src, which is tree-sitter's documented single-translation-unit build.
add_library(tree_sitter STATIC
    third_party/tree-sitter/lib/src/lib.c
)
target_include_directories(tree_sitter
    PUBLIC ${PROJECT_SOURCE_DIR}/third_party/tree-sitter/lib/include
    PRIVATE ${PROJECT_SOURCE_DIR}/third_party/tree-sitter/lib/src
)

add_library(tree_sitter_c STATIC
    third_party/tree-sitter-c/src/parser.c
)
target_include_directories(tree_sitter_c PRIVATE
    ${PROJECT_SOURCE_DIR}/third_party/tree-sitter-c/src
)

add_library(tree_sitter_cpp STATIC
    third_party/tree-sitter-cpp/src/parser.c
    third_party/tree-sitter-cpp/src/scanner.c
)
target_include_directories(tree_sitter_cpp PRIVATE
    ${PROJECT_SOURCE_DIR}/third_party/tree-sitter-cpp/src
)
```

If `third_party/tree-sitter/lib/src/lib.c` does not exist in the version you took, the runtime must instead list its `.c` files individually — use `file(GLOB ...)` only as a last resort and say so in your report, since globbed sources do not re-run CMake when files change.

- [ ] **Step 5: Write the smoke test**

`tests/unit/TreeSitterSmokeTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include <tree_sitter/api.h>

#include <cstring>
#include <string>

extern "C" const TSLanguage* tree_sitter_c(void);
extern "C" const TSLanguage* tree_sitter_cpp(void);

namespace
{

std::string parseAndGetRootType(const TSLanguage* language, const std::string& source)
{
    TSParser* parser = ts_parser_new();
    REQUIRE(parser != nullptr);
    REQUIRE(ts_parser_set_language(parser, language));

    TSTree* tree = ts_parser_parse_string(
        parser, nullptr, source.c_str(), static_cast<uint32_t>(source.size()));
    REQUIRE(tree != nullptr);

    const TSNode root = ts_tree_root_node(tree);
    const std::string rootType = ts_node_type(root);
    const bool hasError = ts_node_has_error(root);

    ts_tree_delete(tree);
    ts_parser_delete(parser);

    CHECK_FALSE(hasError);
    return rootType;
}

} // namespace

TEST_CASE("the vendored C grammar parses a trivial translation unit")
{
    CHECK(parseAndGetRootType(tree_sitter_c(), "int main(void) { return 0; }")
        == "translation_unit");
}

TEST_CASE("the vendored C++ grammar parses a trivial translation unit")
{
    CHECK(parseAndGetRootType(tree_sitter_cpp(), "int main() { return 0; }")
        == "translation_unit");
}

TEST_CASE("the vendored C++ grammar parses a class with a method")
{
    CHECK(parseAndGetRootType(tree_sitter_cpp(), "class Foo { public: int bar() { return 1; } };")
        == "translation_unit");
}
```

Add it to `tests/CMakeLists.txt`'s `add_executable(codeqa_tests ...)` source list, and link the three new libraries plus give the test target the include path:

```cmake
target_link_libraries(codeqa_tests PRIVATE codeqa_core tree_sitter tree_sitter_c tree_sitter_cpp)
```

- [ ] **Step 6: Build and run**

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: everything compiles (the generated `parser.c` files are large; the C++ one may take a while) and all tests pass, including the three new smoke cases.

If `ts_node_has_error` does not exist in the runtime version you vendored, substitute the portable equivalent: compare `ts_node_type(root)` against `"ERROR"` and check `ts_node_is_missing`. Note any such substitution in your report.

- [ ] **Step 7: Commit**

```bash
git add third_party CMakeLists.txt tests/CMakeLists.txt tests/unit/TreeSitterSmokeTest.cpp
git commit -m "build: vendor tree-sitter runtime and C/C++ grammars"
```

---

### Task 2: `Grammar`, `SelectGrammar`, and the RAII parser wrapper

**Files:**
- Create: `include/codeqa/frontend/Grammar.h`
- Create: `include/codeqa/frontend/SelectGrammar.h`
- Create: `src/frontend/SelectGrammar.cpp`
- Create: `include/codeqa/frontend/TreeSitterParser.h`
- Create: `src/frontend/TreeSitterParser.cpp`
- Create: `tests/unit/SelectGrammarTest.cpp`
- Create: `tests/unit/TreeSitterParserTest.cpp`
- Modify: `CMakeLists.txt` (add the two `.cpp` files to `codeqa_core`, link the grammars)
- Modify: `tests/CMakeLists.txt` (add the two test files)

**Interfaces:**
- Consumes: `codeqa::core::Language{C, Cpp, Unknown}` from `include/codeqa/core/Language.h` (Fase 3); the CMake targets and grammar entry points from Task 1.
- Produces: `codeqa::frontend::Grammar{C, Cpp}`; `codeqa::frontend::GrammarSelection{grammar, reason}`; `codeqa::frontend::selectGrammar(codeqa::core::Language) -> GrammarSelection`; `codeqa::frontend::TreeSitterParser` with `[[nodiscard]] std::optional<SyntaxTree> parse(Grammar, std::string_view source)`, and `codeqa::frontend::SyntaxTree` — an RAII owner exposing `rootNode()`. Task 3 uses all of these.

- [ ] **Step 1: Write the failing tests**

`tests/unit/SelectGrammarTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/frontend/SelectGrammar.h"

TEST_CASE("a C file selects the C grammar")
{
    const codeqa::frontend::GrammarSelection selection =
        codeqa::frontend::selectGrammar(codeqa::core::Language::C);

    CHECK(selection.grammar == codeqa::frontend::Grammar::C);
    CHECK(selection.reason == "detected language");
}

TEST_CASE("a C++ file selects the C++ grammar")
{
    const codeqa::frontend::GrammarSelection selection =
        codeqa::frontend::selectGrammar(codeqa::core::Language::Cpp);

    CHECK(selection.grammar == codeqa::frontend::Grammar::Cpp);
    CHECK(selection.reason == "detected language");
}

TEST_CASE("an unknown-language file falls back to the C++ grammar and says so")
{
    const codeqa::frontend::GrammarSelection selection =
        codeqa::frontend::selectGrammar(codeqa::core::Language::Unknown);

    CHECK(selection.grammar == codeqa::frontend::Grammar::Cpp);
    CHECK(selection.reason == "ambiguous header, defaulted to C++");
}
```

`tests/unit/TreeSitterParserTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/frontend/TreeSitterParser.h"

#include <optional>
#include <string>

TEST_CASE("the parser produces a tree for valid C")
{
    codeqa::frontend::TreeSitterParser parser;
    std::optional<codeqa::frontend::SyntaxTree> tree =
        parser.parse(codeqa::frontend::Grammar::C, "int main(void) { return 0; }");

    REQUIRE(tree.has_value());
    CHECK(std::string{tree->rootNodeType()} == "translation_unit");
    CHECK_FALSE(tree->rootHasError());
}

TEST_CASE("the parser produces a tree for valid C++")
{
    codeqa::frontend::TreeSitterParser parser;
    std::optional<codeqa::frontend::SyntaxTree> tree =
        parser.parse(codeqa::frontend::Grammar::Cpp, "class Foo { int bar; };");

    REQUIRE(tree.has_value());
    CHECK(std::string{tree->rootNodeType()} == "translation_unit");
    CHECK_FALSE(tree->rootHasError());
}

TEST_CASE("the parser reports an error tree for broken source")
{
    codeqa::frontend::TreeSitterParser parser;
    std::optional<codeqa::frontend::SyntaxTree> tree =
        parser.parse(codeqa::frontend::Grammar::Cpp, "int broken( {");

    REQUIRE(tree.has_value());
    CHECK(tree->rootHasError());
}

TEST_CASE("an empty source still yields a tree")
{
    codeqa::frontend::TreeSitterParser parser;
    std::optional<codeqa::frontend::SyntaxTree> tree =
        parser.parse(codeqa::frontend::Grammar::C, "");

    REQUIRE(tree.has_value());
    CHECK(std::string{tree->rootNodeType()} == "translation_unit");
}

TEST_CASE("a parser can be reused for several files")
{
    codeqa::frontend::TreeSitterParser parser;

    const std::optional<codeqa::frontend::SyntaxTree> first =
        parser.parse(codeqa::frontend::Grammar::C, "int a(void) { return 1; }");
    const std::optional<codeqa::frontend::SyntaxTree> second =
        parser.parse(codeqa::frontend::Grammar::Cpp, "int b() { return 2; }");

    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    CHECK_FALSE(first->rootHasError());
    CHECK_FALSE(second->rootHasError());
}

TEST_CASE("a syntax tree can be moved without double-freeing")
{
    codeqa::frontend::TreeSitterParser parser;
    std::optional<codeqa::frontend::SyntaxTree> original =
        parser.parse(codeqa::frontend::Grammar::C, "int a(void) { return 1; }");

    REQUIRE(original.has_value());

    const codeqa::frontend::SyntaxTree moved = std::move(*original);

    CHECK(std::string{moved.rootNodeType()} == "translation_unit");
}
```

Add both files to `tests/CMakeLists.txt`.

- [ ] **Step 2: Run to verify they fail**

```bash
cmake -S . -B build
cmake --build build
```

Expected: FAIL — `codeqa/frontend/SelectGrammar.h: No such file or directory`.

- [ ] **Step 3: Implement**

`include/codeqa/frontend/Grammar.h`:

```cpp
#pragma once

namespace codeqa::frontend
{

enum class Grammar
{
    C,
    Cpp
};

} // namespace codeqa::frontend
```

`include/codeqa/frontend/SelectGrammar.h`:

```cpp
#pragma once

#include "codeqa/core/Language.h"
#include "codeqa/frontend/Grammar.h"

#include <string>

namespace codeqa::frontend
{

struct GrammarSelection
{
    Grammar grammar{Grammar::Cpp};
    std::string reason;
};

[[nodiscard]] GrammarSelection selectGrammar(codeqa::core::Language language);

} // namespace codeqa::frontend
```

`src/frontend/SelectGrammar.cpp`:

```cpp
#include "codeqa/frontend/SelectGrammar.h"

namespace codeqa::frontend
{

GrammarSelection selectGrammar(codeqa::core::Language language)
{
    switch (language)
    {
    case codeqa::core::Language::C:
        return GrammarSelection{Grammar::C, "detected language"};
    case codeqa::core::Language::Cpp:
        return GrammarSelection{Grammar::Cpp, "detected language"};
    case codeqa::core::Language::Unknown:
        return GrammarSelection{Grammar::Cpp, "ambiguous header, defaulted to C++"};
    }

    return GrammarSelection{Grammar::Cpp, "ambiguous header, defaulted to C++"};
}

} // namespace codeqa::frontend
```

The trailing return after the switch is unreachable but keeps MSVC `/W4` from warning that not all paths return a value — the same pattern already used in `DetectFileLanguage.cpp`.

`include/codeqa/frontend/TreeSitterParser.h`:

```cpp
#pragma once

#include "codeqa/frontend/Grammar.h"

#include <cstddef>
#include <optional>
#include <string_view>

struct TSParser;
struct TSTree;

namespace codeqa::frontend
{

/// Owns a parsed syntax tree. Non-copyable, movable: the underlying TSTree
/// must be deleted exactly once.
class SyntaxTree
{
public:
    explicit SyntaxTree(TSTree* tree);
    ~SyntaxTree();

    SyntaxTree(const SyntaxTree&) = delete;
    SyntaxTree& operator=(const SyntaxTree&) = delete;

    SyntaxTree(SyntaxTree&& other) noexcept;
    SyntaxTree& operator=(SyntaxTree&& other) noexcept;

    [[nodiscard]] const char* rootNodeType() const;
    [[nodiscard]] bool rootHasError() const;

    /// Only the frontend's own translation units use this.
    [[nodiscard]] TSTree* handle() const { return tree_; }

private:
    TSTree* tree_{nullptr};
};

/// Owns a reusable TSParser. Non-copyable, movable.
class TreeSitterParser
{
public:
    TreeSitterParser();
    ~TreeSitterParser();

    TreeSitterParser(const TreeSitterParser&) = delete;
    TreeSitterParser& operator=(const TreeSitterParser&) = delete;

    TreeSitterParser(TreeSitterParser&& other) noexcept;
    TreeSitterParser& operator=(TreeSitterParser&& other) noexcept;

    [[nodiscard]] std::optional<SyntaxTree> parse(Grammar grammar, std::string_view source);

private:
    TSParser* parser_{nullptr};
};

} // namespace codeqa::frontend
```

Note the forward declarations of `TSParser`/`TSTree`: this header does **not** include `<tree_sitter/api.h>`, so nothing that includes it picks up tree-sitter's headers transitively. Only the `.cpp` files do.

`src/frontend/TreeSitterParser.cpp`:

```cpp
#include "codeqa/frontend/TreeSitterParser.h"

#include <tree_sitter/api.h>

#include <utility>

extern "C" const TSLanguage* tree_sitter_c(void);
extern "C" const TSLanguage* tree_sitter_cpp(void);

namespace codeqa::frontend
{

namespace
{

const TSLanguage* languageFor(Grammar grammar)
{
    switch (grammar)
    {
    case Grammar::C:
        return tree_sitter_c();
    case Grammar::Cpp:
        return tree_sitter_cpp();
    }

    return tree_sitter_cpp();
}

} // namespace

SyntaxTree::SyntaxTree(TSTree* tree)
    : tree_(tree)
{
}

SyntaxTree::~SyntaxTree()
{
    if (tree_ != nullptr)
    {
        ts_tree_delete(tree_);
    }
}

SyntaxTree::SyntaxTree(SyntaxTree&& other) noexcept
    : tree_(std::exchange(other.tree_, nullptr))
{
}

SyntaxTree& SyntaxTree::operator=(SyntaxTree&& other) noexcept
{
    if (this != &other)
    {
        if (tree_ != nullptr)
        {
            ts_tree_delete(tree_);
        }

        tree_ = std::exchange(other.tree_, nullptr);
    }

    return *this;
}

const char* SyntaxTree::rootNodeType() const
{
    return ts_node_type(ts_tree_root_node(tree_));
}

bool SyntaxTree::rootHasError() const
{
    return ts_node_has_error(ts_tree_root_node(tree_));
}

TreeSitterParser::TreeSitterParser()
    : parser_(ts_parser_new())
{
}

TreeSitterParser::~TreeSitterParser()
{
    if (parser_ != nullptr)
    {
        ts_parser_delete(parser_);
    }
}

TreeSitterParser::TreeSitterParser(TreeSitterParser&& other) noexcept
    : parser_(std::exchange(other.parser_, nullptr))
{
}

TreeSitterParser& TreeSitterParser::operator=(TreeSitterParser&& other) noexcept
{
    if (this != &other)
    {
        if (parser_ != nullptr)
        {
            ts_parser_delete(parser_);
        }

        parser_ = std::exchange(other.parser_, nullptr);
    }

    return *this;
}

std::optional<SyntaxTree> TreeSitterParser::parse(Grammar grammar, std::string_view source)
{
    if (parser_ == nullptr)
    {
        return std::nullopt;
    }

    if (!ts_parser_set_language(parser_, languageFor(grammar)))
    {
        return std::nullopt;
    }

    TSTree* tree = ts_parser_parse_string(
        parser_, nullptr, source.data(), static_cast<uint32_t>(source.size()));

    if (tree == nullptr)
    {
        return std::nullopt;
    }

    return SyntaxTree{tree};
}

} // namespace codeqa::frontend
```

`ts_parser_set_language` returning `false` means the grammar's `LANGUAGE_VERSION` is outside the runtime's compatible range — the failure mode Task 1 Step 2 checks for. Returning `nullopt` rather than crashing keeps it diagnosable.

**Caution on `std::string_view::data()`:** it is not guaranteed null-terminated, but `ts_parser_parse_string` takes an explicit length and does not require termination. Every call site in this plan passes a view over a `std::string` that owns its buffer.

Add the two new `.cpp` files to `CMakeLists.txt`'s `codeqa_core` sources, and link the grammars into `codeqa_core`:

```cmake
target_link_libraries(codeqa_core PUBLIC tree_sitter tree_sitter_c tree_sitter_cpp)
```

- [ ] **Step 4: Run to verify they pass**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: all tests pass, including the new `SelectGrammarTest` and `TreeSitterParserTest` cases.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/frontend src/frontend tests/unit/SelectGrammarTest.cpp tests/unit/TreeSitterParserTest.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add grammar selection and the RAII tree-sitter parser wrapper"
```

---

### Task 3: `ParseSourceFile` — read, parse, and diagnose

**Files:**
- Create: `include/codeqa/frontend/ParsedFile.h`
- Create: `include/codeqa/frontend/ParseSourceFile.h`
- Create: `src/frontend/ParseSourceFile.cpp`
- Create: `tests/unit/ParseSourceFileTest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `codeqa::frontend::{Grammar, TreeSitterParser, SyntaxTree}` (Task 2); `codeqa::core::{Diagnostic, DiagnosticSeverity, SourceLocation}` (Fase 1).
- Produces: `codeqa::frontend::ParsedFile{path, grammar, grammarReason, diagnostics, parsed}` and `codeqa::frontend::parseSourceFile(const std::filesystem::path&, Grammar, const std::string& grammarReason, TreeSitterParser&) -> ParsedFile`. Tasks 4 and 5 call this exact signature.

**The tree walk — the substantive logic of this task.** Walk the tree depth-first. At each node:
- if the node is an `ERROR` node, emit `PARSE001` at its start point and **do not descend into it**;
- else if the node is `MISSING`, emit `PARSE002` at its start point and do not descend (a MISSING node has no children anyway);
- else, descend into its children.

Detecting an `ERROR` node: compare `ts_node_type(node)` against the literal `"ERROR"`. Newer runtimes also offer `ts_node_is_error`; use it if the vendored version has it, and note which you used. `MISSING` is `ts_node_is_missing(node)`.

An important optimisation and correctness point: `ts_node_has_error(node)` is true when a node's *subtree* contains an error. Use it to skip descending into clean subtrees entirely — a file with one broken function does not need its other 200 functions walked.

- [ ] **Step 1: Write the failing tests**

`tests/unit/ParseSourceFileTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/frontend/ParseSourceFile.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace
{

class TempSourceFile
{
public:
    TempSourceFile(const std::string& name, const std::string& content)
        : path_(std::filesystem::temp_directory_path() / "codeqa_parse_source_file_test" / name)
    {
        std::filesystem::create_directories(path_.parent_path());
        std::ofstream(path_) << content;
    }

    ~TempSourceFile()
    {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    TempSourceFile(const TempSourceFile&) = delete;
    TempSourceFile& operator=(const TempSourceFile&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("a well-formed C file parses with no diagnostics")
{
    const TempSourceFile file{"clean.c", "int main(void)\n{\n    return 0;\n}\n"};
    codeqa::frontend::TreeSitterParser parser;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::C, "detected language", parser);

    CHECK(result.parsed);
    CHECK(result.diagnostics.empty());
    CHECK(result.grammar == codeqa::frontend::Grammar::C);
    CHECK(result.grammarReason == "detected language");
}

TEST_CASE("a well-formed C++ file parses with no diagnostics")
{
    const TempSourceFile file{
        "clean.cpp", "class Foo\n{\npublic:\n    int bar() { return 1; }\n};\n"};
    codeqa::frontend::TreeSitterParser parser;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser);

    CHECK(result.parsed);
    CHECK(result.diagnostics.empty());
}

TEST_CASE("a broken file produces at least one diagnostic with a plausible line")
{
    const TempSourceFile file{
        "broken.cpp", "int fine(void)\n{\n    return 0;\n}\n\nint broken( {\n"};
    codeqa::frontend::TreeSitterParser parser;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser);

    CHECK(result.parsed);
    REQUIRE_FALSE(result.diagnostics.empty());
    CHECK(result.diagnostics.front().severity == codeqa::core::DiagnosticSeverity::Error);
    CHECK(result.diagnostics.front().location.line >= 6);
    CHECK(result.diagnostics.front().location.filePath == file.path());
}

TEST_CASE("a cascading error does not produce a diagnostic per nested node")
{
    const TempSourceFile file{
        "cascade.cpp",
        "void a()\n{\n    if (x) {\n        if (y) {\n            if (z) {\n"};
    codeqa::frontend::TreeSitterParser parser;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser);

    CHECK(result.parsed);
    REQUIRE_FALSE(result.diagnostics.empty());
    CHECK(result.diagnostics.size() <= 3);
}

TEST_CASE("a missing file produces PARSE003 and is not marked parsed")
{
    codeqa::frontend::TreeSitterParser parser;
    const std::filesystem::path missing =
        std::filesystem::temp_directory_path() / "codeqa_does_not_exist_12345.c";

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        missing, codeqa::frontend::Grammar::C, "detected language", parser);

    CHECK_FALSE(result.parsed);
    REQUIRE(result.diagnostics.size() == 1);
    CHECK(result.diagnostics.front().code == "PARSE003");
}

TEST_CASE("an empty file parses cleanly")
{
    const TempSourceFile file{"empty.c", ""};
    codeqa::frontend::TreeSitterParser parser;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::C, "detected language", parser);

    CHECK(result.parsed);
    CHECK(result.diagnostics.empty());
}

TEST_CASE("parsing the same file twice produces identical diagnostics")
{
    const TempSourceFile file{"repeat.cpp", "int broken( {\n"};
    codeqa::frontend::TreeSitterParser parser;

    const codeqa::frontend::ParsedFile first = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser);
    const codeqa::frontend::ParsedFile second = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser);

    REQUIRE(first.diagnostics.size() == second.diagnostics.size());

    for (std::size_t index = 0; index < first.diagnostics.size(); ++index)
    {
        CHECK(first.diagnostics[index].code == second.diagnostics[index].code);
        CHECK(first.diagnostics[index].location.line == second.diagnostics[index].location.line);
        CHECK(first.diagnostics[index].location.column == second.diagnostics[index].location.column);
    }
}
```

Add it to `tests/CMakeLists.txt`.

- [ ] **Step 2: Run to verify they fail**

```bash
cmake -S . -B build
cmake --build build
```

Expected: FAIL — `codeqa/frontend/ParseSourceFile.h: No such file or directory`.

- [ ] **Step 3: Implement**

`include/codeqa/frontend/ParsedFile.h`:

```cpp
#pragma once

#include "codeqa/core/Diagnostic.h"
#include "codeqa/frontend/Grammar.h"

#include <filesystem>
#include <string>
#include <vector>

namespace codeqa::frontend
{

struct ParsedFile
{
    std::filesystem::path path;
    Grammar grammar{Grammar::Cpp};
    std::string grammarReason;
    std::vector<codeqa::core::Diagnostic> diagnostics;
    bool parsed{false};
};

} // namespace codeqa::frontend
```

`include/codeqa/frontend/ParseSourceFile.h`:

```cpp
#pragma once

#include "codeqa/frontend/Grammar.h"
#include "codeqa/frontend/ParsedFile.h"
#include "codeqa/frontend/TreeSitterParser.h"

#include <filesystem>
#include <string>

namespace codeqa::frontend
{

[[nodiscard]] ParsedFile parseSourceFile(
    const std::filesystem::path& path,
    Grammar grammar,
    const std::string& grammarReason,
    TreeSitterParser& parser);

} // namespace codeqa::frontend
```

`src/frontend/ParseSourceFile.cpp`:

```cpp
#include "codeqa/frontend/ParseSourceFile.h"

#include "codeqa/core/DiagnosticSeverity.h"

#include <tree_sitter/api.h>

#include <cstring>
#include <fstream>
#include <sstream>

namespace codeqa::frontend
{

namespace
{

codeqa::core::Diagnostic makeDiagnostic(
    const std::string& code,
    const std::string& message,
    const std::filesystem::path& path,
    TSPoint point)
{
    codeqa::core::Diagnostic diagnostic;
    diagnostic.code = code;
    diagnostic.severity = codeqa::core::DiagnosticSeverity::Error;
    diagnostic.message = message;
    diagnostic.location.filePath = path;
    // tree-sitter points are 0-based; SourceLocation is 1-based.
    diagnostic.location.line = static_cast<std::size_t>(point.row) + 1;
    diagnostic.location.column = static_cast<std::size_t>(point.column) + 1;

    return diagnostic;
}

bool isErrorNode(TSNode node)
{
    return std::strcmp(ts_node_type(node), "ERROR") == 0;
}

void collectDiagnostics(
    TSNode node,
    const std::filesystem::path& path,
    std::vector<codeqa::core::Diagnostic>& diagnostics)
{
    if (ts_node_is_missing(node))
    {
        diagnostics.push_back(makeDiagnostic(
            "PARSE002", "Expected token is missing.", path, ts_node_start_point(node)));
        return;
    }

    if (isErrorNode(node))
    {
        diagnostics.push_back(makeDiagnostic(
            "PARSE001", "Could not parse this region.", path, ts_node_start_point(node)));
        return;
    }

    // A clean subtree cannot contain an ERROR or MISSING node, so skip it whole.
    if (!ts_node_has_error(node))
    {
        return;
    }

    const uint32_t childCount = ts_node_child_count(node);

    for (uint32_t index = 0; index < childCount; ++index)
    {
        collectDiagnostics(ts_node_child(node, index), path, diagnostics);
    }
}

std::optional<std::string> readFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);

    if (!input)
    {
        return std::nullopt;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    return buffer.str();
}

} // namespace

ParsedFile parseSourceFile(
    const std::filesystem::path& path,
    Grammar grammar,
    const std::string& grammarReason,
    TreeSitterParser& parser)
{
    ParsedFile result;
    result.path = path;
    result.grammar = grammar;
    result.grammarReason = grammarReason;

    const std::optional<std::string> source = readFile(path);

    if (!source.has_value())
    {
        result.diagnostics.push_back(makeDiagnostic(
            "PARSE003", "Could not read the file.", path, TSPoint{0, 0}));
        return result;
    }

    std::optional<SyntaxTree> tree = parser.parse(grammar, *source);

    if (!tree.has_value())
    {
        result.diagnostics.push_back(makeDiagnostic(
            "PARSE003", "The parser could not process the file.", path, TSPoint{0, 0}));
        return result;
    }

    result.parsed = true;
    collectDiagnostics(
        ts_tree_root_node(tree->handle()), path, result.diagnostics);

    return result;
}

} // namespace codeqa::frontend
```

`ParseSourceFile.cpp` needs `<optional>` — add it to the include list if the compiler complains; `TreeSitterParser.h` already pulls it in transitively, but relying on that is fragile.

Add `src/frontend/ParseSourceFile.cpp` to `CMakeLists.txt`'s `codeqa_core` sources.

- [ ] **Step 4: Run to verify they pass**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: all tests pass. If the cascading-error test's `<= 3` bound turns out wrong for the vendored grammar, **investigate before changing it**: the point of the test is that a cascade does not produce one diagnostic per nested node. If the real number is 4 or 5 the bound can move; if it is 40, the pruning logic is broken.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/frontend src/frontend/ParseSourceFile.cpp tests/unit/ParseSourceFileTest.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: parse source files and turn ERROR/MISSING nodes into diagnostics"
```

---

### Task 4: Corpus-wide verification, and fixing the provisional expectations

**Files:**
- Create: `tests/unit/CorpusParsingTest.cpp`
- Modify: `tests/CMakeLists.txt` (add the file and define the corpus path)
- Modify: `tests/corpus/malformed/unclosed-brace.parsing.json` (only if the observed line differs)
- Modify: `tests/corpus/malformed/truncated.parsing.json` (only if the observed line differs)

**Interfaces:**
- Consumes: `codeqa::frontend::{parseSourceFile, TreeSitterParser, Grammar}` (Tasks 2-3); the corpus at `tests/corpus/` (Fase 0).
- Produces: nothing consumed by later tasks.

**This is the task where the phase earns its keep.** Every fixture in `tests/corpus/` outside `malformed/` was written to exercise a construct that `SUPPORTED-CONSTRUCTS.md` declares supported. If the vendored grammar produces a diagnostic on any of them, that is a **real finding** — either the construct is not actually supported, or the fixture is wrong. Do not relax the assertion to make it pass; investigate and report.

- [ ] **Step 1: Pass the corpus path to the test target**

In `tests/CMakeLists.txt`, after the `add_executable(codeqa_tests ...)` block:

```cmake
target_compile_definitions(codeqa_tests PRIVATE
    CODEQA_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus"
)
```

- [ ] **Step 2: Write the corpus test**

`tests/unit/CorpusParsingTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/frontend/ParseSourceFile.h"
#include "codeqa/frontend/SelectGrammar.h"

#include <filesystem>
#include <string>
#include <vector>

namespace
{

struct CorpusFixture
{
    std::filesystem::path path;
    bool isMalformed{false};
};

std::vector<CorpusFixture> collectCorpusFixtures()
{
    std::vector<CorpusFixture> fixtures;
    const std::filesystem::path corpusRoot{CODEQA_CORPUS_DIR};

    for (const std::filesystem::directory_entry& entry :
        std::filesystem::recursive_directory_iterator(corpusRoot))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        const std::string extension = entry.path().extension().string();

        if (extension != ".c" && extension != ".cpp" && extension != ".h" && extension != ".hpp")
        {
            continue;
        }

        const std::string relative =
            entry.path().lexically_relative(corpusRoot).generic_string();

        fixtures.push_back(
            CorpusFixture{entry.path(), relative.rfind("malformed/", 0) == 0});
    }

    return fixtures;
}

codeqa::frontend::Grammar grammarForExtension(const std::filesystem::path& path)
{
    // Mirrors Fase 3: .c is C; .cpp/.hpp are C++; .h is ambiguous and
    // defaults to C++ via selectGrammar.
    const std::string extension = path.extension().string();

    if (extension == ".c")
    {
        return codeqa::frontend::selectGrammar(codeqa::core::Language::C).grammar;
    }

    if (extension == ".h")
    {
        return codeqa::frontend::selectGrammar(codeqa::core::Language::Unknown).grammar;
    }

    return codeqa::frontend::selectGrammar(codeqa::core::Language::Cpp).grammar;
}

} // namespace

TEST_CASE("the corpus contains fixtures to check")
{
    CHECK(collectCorpusFixtures().size() >= 13);
}

TEST_CASE("every well-formed corpus fixture parses with zero diagnostics")
{
    codeqa::frontend::TreeSitterParser parser;

    for (const CorpusFixture& fixture : collectCorpusFixtures())
    {
        if (fixture.isMalformed)
        {
            continue;
        }

        const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
            fixture.path, grammarForExtension(fixture.path), "corpus check", parser);

        INFO("fixture: " << fixture.path.generic_string());
        CHECK(result.parsed);
        CHECK(result.diagnostics.empty());
    }
}

TEST_CASE("every malformed corpus fixture produces at least one diagnostic")
{
    codeqa::frontend::TreeSitterParser parser;
    std::size_t malformedCount = 0;

    for (const CorpusFixture& fixture : collectCorpusFixtures())
    {
        if (!fixture.isMalformed)
        {
            continue;
        }

        ++malformedCount;

        const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
            fixture.path, grammarForExtension(fixture.path), "corpus check", parser);

        INFO("fixture: " << fixture.path.generic_string());
        CHECK(result.parsed);
        CHECK_FALSE(result.diagnostics.empty());
    }

    CHECK(malformedCount == 2);
}
```

- [ ] **Step 3: Run it and read the result carefully**

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Two possible outcomes, and they are handled very differently:

**All green.** Good — the vendored grammars handle every construct the corpus declares supported. Proceed to Step 4.

**A well-formed fixture produces a diagnostic.** This is a genuine finding, not a test to be relaxed. The `INFO` line names the fixture. Determine which of these it is:
- the fixture uses a construct the grammar genuinely does not support → report it; the construct's entry in `SUPPORTED-CONSTRUCTS.md` may need to move to the out-of-scope table, which is a decision for the project owner, not for you;
- the fixture has a real syntax error nobody noticed → fix the fixture and re-verify its line numbers in the `.parsing.json` with `grep -n ""`;
- the wrong grammar is being selected for it → fix `grammarForExtension`.

Report which, with evidence. **Do not** add an exception list to the test to route around a failing fixture.

- [ ] **Step 4: Observe and fix the provisional malformed line numbers**

Fase 0 marked the `diagnostics` line numbers of the two `malformed/` fixtures as provisional, because no parser existed to confirm what error recovery actually produces. Now one does.

Print what the parser really reports:

```bash
./build/Debug/codeqa.exe --version
```

(The `parse` subcommand does not exist until Task 5, so use a scratch program or a temporary `TEST_CASE` with `MESSAGE(...)` to print each malformed fixture's diagnostics — line, column, and code. Remove any temporary test before committing.)

Then, for each of `tests/corpus/malformed/unclosed-brace.parsing.json` and `truncated.parsing.json`:
- If the observed line matches what the file already says, leave it and record that you verified it.
- If it differs, **check that the observed line is defensible before adopting it** — it should point at the defect or at its first detectable manifestation. Recall from Fase 0 that `truncated.c` has only 8 lines, so any reported line above 8 is not adoptable and indicates a problem to report rather than a value to copy.
- Update the JSON only after that judgement. Copying parser output blindly would make the expectation assert that the code does what it does.

Then update `tests/corpus/README.md`: the paragraph stating that the `malformed/` fixtures' `diagnostics` lines are "provisionales pendientes de la salida real de tree-sitter en la Fase 4" is now satisfied for the diagnostics half. Reword it so it still flags the `functions`/`classes` arrays as provisional for Fase 4b, but no longer claims the diagnostic lines are unverified.

- [ ] **Step 5: Commit**

```bash
git add tests/unit/CorpusParsingTest.cpp tests/CMakeLists.txt tests/corpus
git commit -m "test: verify the whole corpus parses as declared and pin malformed diagnostics"
```

---

### Task 5: The `parse` subcommand, the `Detected languages:` header, and the README

**Files:**
- Modify: `src/cli/RunApplication.cpp`
- Modify: `tests/unit/RunApplicationTest.cpp`
- Modify: `README.md`

**Interfaces:**
- Consumes: `codeqa::frontend::{parseSourceFile, selectGrammar, TreeSitterParser, ParsedFile, Grammar}` (Tasks 2-3); `codeqa::application::{DiscoverProjectSources, DetectFileLanguage, LanguageOverride}` and `codeqa::infrastructure::{DiscoveryOptions, DiscoveryResult, DiscoveredFile}` (Fases 2-3, unchanged).
- Produces: no new public interface — `runApplication`'s signature does not change.

**Exact behaviour:**
- `codeqa parse <path> [--exclude <glob>]... [--lang-override <pattern>=<lang>]...` — the same argument parsing as `discover`, with the same `CLI002`–`CLI007` errors. Extract the shared parsing rather than duplicating it: the existing `runDiscover` already contains that loop, so lift it into a helper both subcommands call.
- After discovery and language detection, parse each discovered file. The grammar comes from `selectGrammar(detection.language)`; the reason is that selection's `reason`.
- Returns `ExitCode::Success` regardless of how many diagnostics were found.
- `--help` documents `parse`.

**Output format:**

```text
Files parsed: 13
C files: 4
C++ files: 9
Parse errors: 2

malformed/truncated.c
  line 8: error
malformed/unclosed-brace.cpp
  line 13: error
```

- `Files parsed` counts files with `parsed == true`.
- `C files` / `C++ files` count by the grammar actually used, so an ambiguous `.h` counts as C++.
- `Parse errors` is the number of files with at least one diagnostic (not the number of diagnostics).
- The per-file detail lists only files with diagnostics, each followed by one indented `  line <N>: <severity>` per diagnostic. A clean run prints nothing after the summary.
- A file with `PARSE003` is not counted in `Files parsed` or in either grammar count, but does appear in `Parse errors` and in the detail.

**And the deferred Fase 3 item:** `discover`'s per-file listing gets a `Detected languages:` header. The blank line that currently separates summary from listing stays, the header goes immediately after it, and the rows follow with no blank line between. It is printed under the same condition as the listing itself (at least one discovered file).

- [ ] **Step 1: Write the failing tests**

Append to `tests/unit/RunApplicationTest.cpp` (keep every existing test and helper unchanged):

```cpp
TEST_CASE("discover's per-file listing carries a Detected languages header")
{
    const std::filesystem::path root = makeTempDiscoverRoot();

    const RunResult result = run({"discover", root.string()});

    CHECK(result.output.find("\nDetected languages:\nmain.c") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("parse without a path returns CLI002")
{
    const RunResult result = run({"parse"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI002") != std::string::npos);
}

TEST_CASE("parse with a nonexistent path returns CLI003")
{
    const RunResult result = run({"parse", "this/path/does/not/exist"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI003") != std::string::npos);
}

TEST_CASE("parse on a clean directory reports no parse errors and returns Success")
{
    const std::filesystem::path root = makeTempDiscoverRoot();

    const RunResult result = run({"parse", root.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("Files parsed: 3") != std::string::npos);
    CHECK(result.output.find("Parse errors: 0") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("parse reports a broken file and still returns Success")
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "codeqa_parse_broken_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::ofstream(root / "good.c") << "int main(void) { return 0; }\n";
    std::ofstream(root / "bad.c") << "int broken( {\n";

    const RunResult result = run({"parse", root.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("Parse errors: 1") != std::string::npos);
    CHECK(result.output.find("bad.c") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("parse honours --lang-override")
{
    const std::filesystem::path root = makeTempDiscoverRoot();

    const RunResult result = run({"parse", root.string(), "--lang-override", "util.h=c"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("C files: 2") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("--help documents the parse subcommand")
{
    const RunResult result = run({"--help"});

    CHECK(result.output.find("parse <path>") != std::string::npos);
}
```

Note on the `--lang-override` case: `makeTempDiscoverRoot()` creates `main.c`, `util.cpp`, and `util.h`. Without the override, `util.h` is ambiguous and parses as C++ (so 1 C file, 2 C++). With `util.h=c`, it parses as C, giving 2 C files and 1 C++ file.

- [ ] **Step 2: Run to verify they fail**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: FAIL — `parse` is not recognised, so `run({"parse", ...})` currently returns `CLI001`; and `discover`'s output has no `Detected languages:` header.

- [ ] **Step 3: Implement**

In `src/cli/RunApplication.cpp`:

1. Add the frontend includes (`codeqa/frontend/ParseSourceFile.h`, `codeqa/frontend/SelectGrammar.h`).
2. Extract the argument-parsing loop currently inside `runDiscover` into a helper — for example a `DiscoverArguments` struct holding `path`, `excludePatterns`, and `languageOverrides`, and a function that fills it and returns either success or the exit code to propagate. Both `runDiscover` and the new `runParse` call it. Do not duplicate the loop: the `CLI002`–`CLI007` behaviour must stay identical between the two subcommands by construction, not by coincidence.
3. Add `printLanguageListing`'s header: immediately after the existing `output << '\n';` that precedes the listing, emit `output << "Detected languages:\n";`.
4. Add `runParse`, and dispatch `"parse"` to it in `runApplication` alongside the existing `"discover"` branch.
5. Extend `printHelp` to document `parse`.

`runParse` builds the discovery options, runs discovery, then for each discovered file: detects its language, selects the grammar, and calls `parseSourceFile` with a single `TreeSitterParser` reused across all files (constructing one per file wastes the parser's internal buffers).

- [ ] **Step 4: Run to verify they pass**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: all tests pass — the new ones and every pre-existing `RunApplicationTest` case unchanged.

- [ ] **Step 5: Verify by hand against the real corpus**

```bash
./build/Debug/codeqa.exe parse tests/corpus
```

Expected: `Files parsed: 13`, `C files: 4`, `C++ files: 9`, `Parse errors: 2`, and the two `malformed/` fixtures listed with their diagnostic lines. Paste the real output into your report.

- [ ] **Step 6: Update the README**

Add a `## Parseo` section after `## Descubrimiento de archivos`:

````markdown
## Parseo

```bash
./build/codeqa parse ./ruta/al/proyecto
./build/codeqa parse ./ruta/al/proyecto --exclude vendor --lang-override "legacy/*.h=c"
```

Descubre los archivos fuente (mismas reglas que `discover`), detecta su lenguaje
y los parsea con tree-sitter, reportando las regiones que la gramática no ha
podido interpretar.

Acepta los mismos flags que `discover`. Una cabecera `.h` sin `--lang-override`
se parsea con la gramática de C++, que acepta prácticamente todo C válido y
minimiza los errores falsos.

Los errores de parseo son información, no un fallo: `parse` devuelve 0 aunque
encuentre archivos rotos. El código de salida solo reflejará la calidad del
código cuando exista el quality gate (Fase 12).
````

Also update the intro and `## Estado` sections: the tool now discovers, detects language, and parses — but still does not analyse. Read the current wording first and adjust it minimally.

- [ ] **Step 7: Commit**

```bash
git add src/cli/RunApplication.cpp tests/unit/RunApplicationTest.cpp README.md
git commit -m "feat: add the parse subcommand and the discover language-listing header"
```

---

## Self-Review Notes

- **Spec coverage:** every item of the spec's section 3 maps to a task — vendoring and CMake (Task 1), grammar selection and RAII wrapper (Task 2), reading + parsing + diagnostics (Task 3), corpus verification and the provisional-line fix (Task 4), the `parse` subcommand and the `Detected languages:` header (Task 5).
- **Deferrals restated where an implementer might otherwise act:** no `.scm` queries, no structural models, no `nlohmann/json`, no corpus runner reading JSON, no validation of the eight source-presence-only IDs.
- **Type consistency:** `Grammar`, `GrammarSelection{grammar, reason}`, `ParsedFile{path, grammar, grammarReason, diagnostics, parsed}`, `SyntaxTree`, `TreeSitterParser::parse`, and `parseSourceFile`'s four-argument signature are identical across their defining task and every later call site.
- **Where this plan is least certain, and how each is handled rather than assumed:**
  - The exact tree-sitter release and its API surface cannot be known at planning time. Task 1 Step 2 makes ABI compatibility an explicit verification with a documented failure route; Task 1 Step 6 and Task 3 name the specific API calls that might differ (`ts_node_has_error`, `ts_node_is_error`) and give the portable fallback.
  - Whether `lib/src/lib.c` exists in the taken version — Task 1 Step 4 states the alternative.
  - The cascading-diagnostic count in Task 3's test is a bound, not an exact value, with explicit instructions on when moving it is legitimate and when it signals broken pruning.
  - Whether the grammars accept every construct the corpus declares supported is genuinely unknown until Task 4 runs. That task treats a failure as a finding to report — with three named diagnoses and an explicit prohibition on routing around it — rather than as a test to adjust.
