# Fase 4b — extracción estructural y validación del corpus Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** Extraer estructura C/C++ del CST de tree-sitter en modelos tipados y verificar el resultado completo contra el corpus declarativo.

**Architecture:** ParsedFile seguirá siendo el resultado del frontend y adquirirá colecciones de modelos estructurales. StructuralExtractor, dentro de codeqa::frontend, compilará queries .scm embebidas y las convertirá en modelos mientras el SyntaxTree aún está vivo; el runner de pruebas leerá las expectativas JSON y comparará el resultado sin que JSON ni tree-sitter salgan a otras capas.

**Tech Stack:** C++17, CMake 3.16+, tree-sitter C API vendorizada, queries tree-sitter, doctest, CTest y nlohmann/json v3.11.3 vendorizada como header-only para tests.

## Global Constraints

- Todo código del proyecto usa C++17 sin extensiones de compilador y vive en namespace codeqa.
- codeqa::frontend es la única capa que incluye tree_sitter/api.h o conoce tipos TS*.
- Los handles de tree-sitter son RAII, no copiables, movibles cuando proceda y no se exponen como punteros crudos.
- Las dependencias externas se vendorizarán como fuente en third_party; no se usa FetchContent ni submódulos.
- CMake usa PROJECT_SOURCE_DIR y PROJECT_BINARY_DIR; codeqa_enable_warnings se aplica solo al código propio.
- Un .h ambiguo mantiene la selección C++; PARSE001, PARSE002 y PARSE003 conservan su significado y parse devuelve éxito ante diagnósticos.
- Los rangos del modelo convierten filas y columnas de tree-sitter de 0-based a 1-based.
- La fase no modela variables globales ni typedef; C-DECL-01 y C-TYPE-04 permanecen pendientes de Fase 5.
- No se añade resolución de tipos, semántica de herencia, macros, control de flujo, llamadas, métricas, reglas ni salida estructural de CLI.
- Antes de descargar nlohmann/json se debe solicitar permiso explícito al propietario del proyecto.

---

## File Structure

    cmake/EmbeddedQueries.h.in                         Generated-header template for query text
    include/codeqa/frontend/StructuralModels.h         Typed frontend structural models
    include/codeqa/frontend/StructuralExtractor.h      RAII query owner and extractor interface
    include/codeqa/frontend/ParsedFile.h               Adds structural collections to ParsedFile
    include/codeqa/frontend/TreeSitterParser.h         Keeps SyntaxTree handle private
    src/frontend/StructuralExtractor.cpp               Query compilation, execution and adapters
    src/frontend/ParseSourceFile.cpp                   Invokes extraction while SyntaxTree is alive
    src/frontend/queries/c/functions.scm               C function candidates
    src/frontend/queries/c/types.scm                   C struct/union/enum candidates
    src/frontend/queries/c/includes.scm                C include candidates
    src/frontend/queries/cpp/functions.scm             C++ function candidates
    src/frontend/queries/cpp/types.scm                 C++ type and namespace candidates
    src/frontend/queries/cpp/includes.scm              C++ include candidates
    tests/unit/StructuralModelsTest.cpp                Model defaults and value semantics
    tests/unit/StructuralExtractorTest.cpp             Extraction behaviour for C and C++
    tests/unit/EmbeddedQueriesTest.cpp                 Every embedded query compiles
    tests/unit/CorpusStructureTest.cpp                 JSON-backed whole-corpus runner
    third_party/nlohmann/json.hpp                      Vendored JSON header, v3.11.3
    third_party/nlohmann/LICENSE.MIT                   Vendored license
    third_party/README.md                              Provenance entry
    CMakeLists.txt                                     Embed query assets and declare JSON target
    tests/CMakeLists.txt                               Test sources and JSON test-only dependency
    tests/corpus/*.parsing.json                        Typed structural expectations
    tests/corpus/README.md                             Schema, comparison and Phase 5 limits
    tests/corpus/CheckCorpusIntegrity.cmake            Require the includes expectation field

---

### Task 1: Add typed structural models and close the SyntaxTree handle escape

**Files:**
- Create: include/codeqa/frontend/StructuralModels.h
- Create: tests/unit/StructuralModelsTest.cpp
- Modify: include/codeqa/frontend/ParsedFile.h
- Modify: include/codeqa/frontend/TreeSitterParser.h
- Modify: src/frontend/TreeSitterParser.cpp
- Modify: tests/CMakeLists.txt

**Interfaces:**
- Produces FunctionKind { Free, Method, Constructor, Destructor, Operator, Lambda, Prototype }, ParameterPassing { Value, Pointer, Reference, FunctionPointer }, ClassKind { Class, Struct, Union, Enum, Namespace }, IncludeDelimiter { Local, System }, and the four model structs.
- Produces a private SyntaxTree::handle() accessible only to future StructuralExtractor.
- ParsedFile gains std::vector<FunctionModel> functions, std::vector<ClassModel> classes and std::vector<IncludeDependency> includes.

- [ ] **Step 1: Write the failing model-contract test**

Create tests/unit/StructuralModelsTest.cpp with these assertions:

    #include "doctest/doctest.h"
    #include "codeqa/frontend/StructuralModels.h"

    TEST_CASE("structural models retain the attributes required by the corpus")
    {
        codeqa::frontend::ParameterModel parameter{
            "callback",
            codeqa::frontend::ParameterPassing::FunctionPointer,
            true};
        codeqa::frontend::FunctionModel function;
        function.name = "run";
        function.kind = codeqa::frontend::FunctionKind::Free;
        function.parameters = {parameter};
        function.isStatic = true;
        function.isVariadic = true;

        CHECK(function.parameters.front().name == "callback");
        CHECK(function.parameters.front().passing
            == codeqa::frontend::ParameterPassing::FunctionPointer);
        CHECK(function.parameters.front().hasDefaultValue);
        CHECK(function.isStatic);
        CHECK(function.isVariadic);
    }

    TEST_CASE("class and include models default to typed, non-generic values")
    {
        codeqa::frontend::ClassModel type;
        codeqa::frontend::IncludeDependency include;

        CHECK(type.kind == codeqa::frontend::ClassKind::Class);
        CHECK_FALSE(type.isTemplate);
        CHECK_FALSE(type.isScopedEnum);
        CHECK(include.delimiter == codeqa::frontend::IncludeDelimiter::Local);
    }

Add this source to codeqa_tests in tests/CMakeLists.txt.

- [ ] **Step 2: Run the test to verify it fails**

Run:

    cmake --build build --config Debug --target codeqa_tests
    build/tests/Debug/codeqa_tests.exe --test-case="structural models*"

Expected: compilation fails because StructuralModels.h does not exist.

- [ ] **Step 3: Implement the models and encapsulation**

Create StructuralModels.h. Give every model a SourceRange named range; use explicit enum values rather than strings:

    struct ParameterModel
    {
        std::string name;
        ParameterPassing passing{ParameterPassing::Value};
        bool hasDefaultValue{false};
    };

    struct FunctionModel
    {
        std::string name;
        FunctionKind kind{FunctionKind::Free};
        core::SourceRange range;
        std::vector<ParameterModel> parameters;
        bool isStatic{false};
        bool isVariadic{false};
    };

    struct ClassModel
    {
        std::string name;
        ClassKind kind{ClassKind::Class};
        core::SourceRange range;
        bool isTemplate{false};
        bool isScopedEnum{false};
        std::vector<std::string> baseSpecifiers;
    };

    struct IncludeDependency
    {
        std::string target;
        IncludeDelimiter delimiter{IncludeDelimiter::Local};
        core::SourceRange range;
    };

Include this header from ParsedFile.h and add the three vectors named in the Interfaces block. In TreeSitterParser.h forward-declare class StructuralExtractor, move handle() below private:, and declare friend class StructuralExtractor. Define the private method in TreeSitterParser.cpp. Do not change rootNodeType() or rootHasError().

- [ ] **Step 4: Run focused and regression tests**

Run:

    cmake --build build --config Debug
    build/tests/Debug/codeqa_tests.exe --test-case="structural models*,the parser*"
    ctest --test-dir build -C Debug --output-on-failure

Expected: the new tests and all existing tests pass.

- [ ] **Step 5: Commit**

    git add include/codeqa/frontend/StructuralModels.h include/codeqa/frontend/ParsedFile.h include/codeqa/frontend/TreeSitterParser.h src/frontend/TreeSitterParser.cpp tests/unit/StructuralModelsTest.cpp tests/CMakeLists.txt
    git commit -m "feat: add typed frontend structural models"

---

### Task 2: Embed and validate the C and C++ query assets

**Files:**
- Create: cmake/EmbeddedQueries.h.in
- Create: include/codeqa/frontend/StructuralExtractor.h
- Create: src/frontend/StructuralExtractor.cpp
- Create: six files below src/frontend/queries/c and src/frontend/queries/cpp
- Create: tests/unit/EmbeddedQueriesTest.cpp
- Modify: CMakeLists.txt
- Modify: tests/CMakeLists.txt

**Interfaces:**
- Produces StructuralExtractor as a non-copyable, movable RAII owner with constructor StructuralExtractor() and method void extract(const SyntaxTree&, Grammar, std::string_view, const std::filesystem::path&, ParsedFile&) const.
- Construction compiles every embedded query against its matching TSLanguage and throws std::runtime_error with the asset name and byte offset if a project query is invalid.
- This task compiles and owns queries; extract initially performs no model adaptation.

- [ ] **Step 1: Write the failing query-compilation test**

Create tests/unit/EmbeddedQueriesTest.cpp:

    #include "doctest/doctest.h"
    #include "codeqa/frontend/StructuralExtractor.h"

    TEST_CASE("all embedded structural queries compile")
    {
        CHECK_NOTHROW(codeqa::frontend::StructuralExtractor{});
    }

- [ ] **Step 2: Run it to verify it fails**

Run:

    cmake --build build --config Debug --target codeqa_tests

Expected: compilation fails because StructuralExtractor.h and the generated query header do not exist.

- [ ] **Step 3: Add the exact query assets and CMake embedding pipeline**

Create these query files. Their captures intentionally select candidates; C++ adapters in Tasks 3 and 4 validate the declarator and normalize details.

    src/frontend/queries/c/functions.scm
    (function_definition) @function
    (declaration declarator: (function_declarator)) @function

    src/frontend/queries/c/types.scm
    [(struct_specifier) (union_specifier) (enum_specifier)] @type

    src/frontend/queries/c/includes.scm
    (preproc_include) @include

    src/frontend/queries/cpp/functions.scm
    [(function_definition) (declaration declarator: (function_declarator))] @function

    src/frontend/queries/cpp/types.scm
    [(class_specifier) (struct_specifier) (union_specifier) (enum_specifier)
     (namespace_definition)] @type

    src/frontend/queries/cpp/includes.scm
    (preproc_include) @include

Create cmake/EmbeddedQueries.h.in with six inline constexpr string_view values, one per asset, each surrounded by a distinct raw-string delimiter. In the top-level CMakeLists.txt, list all six absolute source paths in CODEQA_QUERY_FILES, register them with CMAKE_CONFIGURE_DEPENDS, file(READ) each into a named variable, and configure the template to:

    ${PROJECT_BINARY_DIR}/generated/codeqa/frontend/EmbeddedQueries.h

StructuralExtractor.cpp includes tree_sitter/api.h and the generated header. Its Impl owns TSQuery pointers in std::unique_ptr with ts_query_delete deleters, obtains tree_sitter_c() and tree_sitter_cpp(), and calls ts_query_new for every asset during construction. Keep all TSQuery and TSLanguage references in this .cpp file.

- [ ] **Step 4: Implement the public extractor boundary**

Create StructuralExtractor.h with this complete interface:

    class StructuralExtractor
    {
    public:
        StructuralExtractor();
        ~StructuralExtractor();
        StructuralExtractor(const StructuralExtractor&) = delete;
        StructuralExtractor& operator=(const StructuralExtractor&) = delete;
        StructuralExtractor(StructuralExtractor&&) noexcept;
        StructuralExtractor& operator=(StructuralExtractor&&) noexcept;

        void extract(
            const SyntaxTree& tree,
            Grammar grammar,
            std::string_view source,
            const std::filesystem::path& path,
            ParsedFile& result) const;

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };

Implement extract as an intentionally empty, tested boundary in this task; its only
observable responsibility is preserving a valid RAII query owner without mutating
ParsedFile. Do not make SyntaxTree::handle() public again.

- [ ] **Step 5: Run focused and regression tests**

Run:

    cmake -S . -B build
    cmake --build build --config Debug
    build/tests/Debug/codeqa_tests.exe --test-case="all embedded structural queries compile"
    ctest --test-dir build -C Debug --output-on-failure

Expected: all six assets compile and the full suite passes.

- [ ] **Step 6: Commit**

    git add CMakeLists.txt cmake/EmbeddedQueries.h.in include/codeqa/frontend/StructuralExtractor.h src/frontend/StructuralExtractor.cpp src/frontend/queries tests/unit/EmbeddedQueriesTest.cpp tests/CMakeLists.txt
    git commit -m "build: embed and validate frontend tree-sitter queries"

---

### Task 3: Extract functions and typed parameters through the query adapter

**Files:**
- Modify: src/frontend/StructuralExtractor.cpp
- Modify: src/frontend/ParseSourceFile.cpp
- Modify: include/codeqa/frontend/ParseSourceFile.h
- Modify: src/cli/RunApplication.cpp
- Modify: tests/unit/ParseSourceFileTest.cpp
- Modify: tests/unit/CorpusParsingTest.cpp
- Create: tests/unit/StructuralExtractorTest.cpp
- Modify: tests/CMakeLists.txt

**Interfaces:**
- parseSourceFile gains a final StructuralExtractor& extractor parameter.
- After parsing and collecting diagnostics, it calls extractor.extract(*tree, grammar, source, path, result).
- runParse constructs one StructuralExtractor next to its reusable TreeSitterParser and passes it for every discovered file.
- Produces functions in source order with 1-based SourceRange, typed parameters, static and variadic attributes.

- [ ] **Step 1: Write failing behaviour tests**

Add tests/unit/StructuralExtractorTest.cpp using a temporary source file and parseSourceFile. Test this C input:

    static int apply(int value, int (*operation)(int))
    {
        return operation(value);
    }

    int sum(int count, ...)
    {
        return count;
    }

Assert two functions, names apply and sum, FunctionKind::Free, apply.isStatic, sum.isVariadic, and apply.parameters[1].passing == FunctionPointer.

Add a C++ case:

    int update(int& target, const int* source, int optional = 7)
    {
        target += *source;
        return optional;
    }

Assert parameter passings Reference, Pointer and Value, and hasDefaultValue only for optional. Update every existing parseSourceFile caller: tests/unit/ParseSourceFileTest.cpp, tests/unit/CorpusParsingTest.cpp and src/cli/RunApplication.cpp must construct or receive one reusable StructuralExtractor and pass it to every call.

- [ ] **Step 2: Run the focused test to verify it fails**

Run:

    cmake --build build --config Debug --target codeqa_tests
    build/tests/Debug/codeqa_tests.exe --test-case="extracts functions*,extracts typed parameters*"

Expected: the test fails because extract does not populate ParsedFile.functions.

- [ ] **Step 3: Implement function and parameter adaptation**

In StructuralExtractor.cpp, execute the selected function query with TSQueryCursor over tree.handle(). For each candidate:

1. Reject a declaration whose declarator subtree contains no function_declarator.
2. Obtain the declaration or definition node text from source using start_byte/end_byte.
3. Recursively find the innermost declarator name. Preserve a qualified C++ name such as Counter::value; use <lambda> for lambda_expression.
4. Set FunctionKind from the candidate node and its declarator: definition without class qualification is Free or Method according to its parent class; constructor, destructor, operator and lambda use their dedicated kinds; declaration without a body is Prototype.
5. Read parameter_declaration children in source order. An absent identifier becomes an empty name. Set FunctionPointer when its declarator contains function_declarator beneath pointer_declarator; otherwise Reference when text contains &, Pointer when text contains *, and Value otherwise. Set hasDefaultValue when a default_value child exists.
6. Set isStatic only when the declaration specifier text contains the standalone token static. Set isVariadic when a variadic_parameter node or ... text is present.
7. Convert node start/end points and bytes to SourceRange. End line is the line containing the final syntax token; use ts_node_end_point and ts_node_end_byte.
8. Sort by range.begin.offset then range.end.offset and remove duplicate functions with identical kind and range.

In ParseSourceFile.cpp pass the source contents to extract after collectDiagnostics. If reading or parsing fails, do not call the extractor.

- [ ] **Step 4: Run focused and regression tests**

Run:

    cmake --build build --config Debug
    build/tests/Debug/codeqa_tests.exe --test-case="extracts functions*,extracts typed parameters*,a well-formed*"
    ctest --test-dir build -C Debug --output-on-failure

Expected: both new cases pass, existing parser diagnostics remain unchanged, and parse CLI output is byte-for-byte unchanged.

- [ ] **Step 5: Commit**

    git add include/codeqa/frontend/ParseSourceFile.h src/frontend/ParseSourceFile.cpp src/frontend/StructuralExtractor.cpp src/cli/RunApplication.cpp tests/unit/ParseSourceFileTest.cpp tests/unit/CorpusParsingTest.cpp tests/unit/StructuralExtractorTest.cpp tests/CMakeLists.txt
    git commit -m "feat: extract functions and typed parameters"

---

### Task 4: Extract types, namespaces and include dependencies

**Files:**
- Modify: src/frontend/StructuralExtractor.cpp
- Modify: tests/unit/StructuralExtractorTest.cpp

**Interfaces:**
- Produces classes in source order with ClassKind, name, range, isTemplate, isScopedEnum and baseSpecifiers.
- Produces includes in source order with target, IncludeDelimiter and range.
- Keeps recovered structural nodes from malformed trees when their candidate node itself is neither ERROR nor MISSING.

- [ ] **Step 1: Write failing extraction tests**

Append these source snippets to StructuralExtractorTest.cpp.

For C++ types:

    namespace outer {
    namespace {
    class Base {};
    class Derived : public Base {};
    template <typename T> class Box {};
    enum class Mode { Fast };
    }

Assert namespaces outer and empty name, Derived with ClassKind::Class and baseSpecifiers == {"Base"}, Box.isTemplate, and Mode.isScopedEnum.

For includes:

    #include <vector>
    #include "local.h"

Assert target vector with IncludeDelimiter::System and target local.h with IncludeDelimiter::Local.

- [ ] **Step 2: Run the tests to verify they fail**

Run:

    cmake --build build --config Debug --target codeqa_tests
    build/tests/Debug/codeqa_tests.exe --test-case="extracts types*,extracts include dependencies*"

Expected: the tests fail because ParsedFile.classes and ParsedFile.includes are empty.

- [ ] **Step 3: Implement type, namespace and include adaptation**

Execute the selected types and includes queries. For each captured type candidate:

1. Ignore a candidate that is ERROR or MISSING.
2. Map tree-sitter node type class_specifier, struct_specifier, union_specifier, enum_specifier and namespace_definition to ClassKind.
3. Read the direct name child; anonymous namespaces use an empty name.
4. Mark isTemplate when an ancestor template_declaration directly wraps the candidate.
5. Mark isScopedEnum only when the enum node text starts with enum class or enum struct.
6. For a class or struct with a base_class_clause, collect each direct base name text after access modifiers and commas, trimming whitespace without resolving aliases or types.
7. Build the 1-based SourceRange and deduplicate by ClassKind plus range.

For every preproc_include candidate, obtain its path child. Strip only the surrounding angle brackets or quotes into target; set System for angle brackets and Local for quotes. Preserve the include node range.

Sort classes and includes by begin offset. Do not change FunctionModel behaviour.

- [ ] **Step 4: Run focused and regression tests**

Run:

    cmake --build build --config Debug
    build/tests/Debug/codeqa_tests.exe --test-case="extracts types*,extracts include dependencies*,extracts functions*"
    ctest --test-dir build -C Debug --output-on-failure

Expected: the new C++ and include tests pass and every pre-existing test remains green.

- [ ] **Step 5: Commit**

    git add src/frontend/StructuralExtractor.cpp tests/unit/StructuralExtractorTest.cpp
    git commit -m "feat: extract types namespaces and includes"

---

### Task 5: Vendor JSON, migrate corpus expectations and add the complete structural runner

**Files:**
- Create: third_party/nlohmann/json.hpp
- Create: third_party/nlohmann/LICENSE.MIT
- Create: tests/unit/CorpusStructureTest.cpp
- Modify: CMakeLists.txt
- Modify: tests/CMakeLists.txt
- Modify: third_party/README.md
- Modify: tests/corpus/CheckCorpusIntegrity.cmake
- Modify: tests/corpus/README.md
- Modify: every existing tests/corpus/*.parsing.json file

**Interfaces:**
- nlohmann_json is an INTERFACE target linked only by codeqa_tests.
- CorpusStructureTest loads each source expectation, invokes parseSourceFile with one reusable TreeSitterParser and StructuralExtractor, and applies exact or minimum comparison according to the malformed directory.
- Every expectation has top-level constructs, functions, classes, includes and diagnostics.

- [ ] **Step 1: Request dependency permission and write the failing runner test**

Ask the project owner for explicit permission to download nlohmann/json. Do not download or vendor anything until the response is affirmative.

After permission, create CorpusStructureTest.cpp with a test named every corpus fixture matches its structural expectation. Its fixture collector must accept only .c, .cpp, .h and .hpp files, find the sibling .parsing.json file, and classify malformed/ by the first relative path segment.

Start the test with:

    #include "doctest/doctest.h"
    #include "codeqa/frontend/ParseSourceFile.h"
    #include "codeqa/frontend/SelectGrammar.h"
    #include "codeqa/frontend/StructuralExtractor.h"
    #include <nlohmann/json.hpp>

    TEST_CASE("every corpus fixture matches its structural expectation")
    {
        codeqa::frontend::TreeSitterParser parser;
        codeqa::frontend::StructuralExtractor extractor;
        // Iterate all fixture pairs and compare them as specified below.
    }

- [ ] **Step 2: Run it to verify it fails**

Run:

    cmake --build build --config Debug --target codeqa_tests

Expected: compilation fails because the vendored nlohmann/json header, CMake target and runner source do not exist.

- [ ] **Step 3: Vendor and configure the pinned dependency**

Download release v3.11.3 from https://github.com/nlohmann/json/releases/tag/v3.11.3. Copy exactly single_include/nlohmann/json.hpp to third_party/nlohmann/json.hpp and LICENSE.MIT to third_party/nlohmann/LICENSE.MIT. Do not copy examples, tests, CMake files or the repository history.

Add this exact target to CMakeLists.txt:

    add_library(nlohmann_json INTERFACE)
    target_include_directories(nlohmann_json INTERFACE
        ${PROJECT_SOURCE_DIR}/third_party/nlohmann
    )

Link nlohmann_json only to tests with:

    target_link_libraries(codeqa_tests PRIVATE
        codeqa_core tree_sitter tree_sitter_c tree_sitter_cpp nlohmann_json
    )

Add its version, URL, MIT license and the two copied files to third_party/README.md.

- [ ] **Step 4: Migrate every corpus expectation to the typed schema**

Require includes in CheckCorpusIntegrity.cmake alongside functions, classes and diagnostics. In every parsing expectation:

1. Add includes as an explicit array. Each entry has the shape { "target": "local.h", "delimiter": "local", "startLine": 1, "endLine": 1 } or uses delimiter "system" for an angle-bracket include.
2. Replace each function parameters string array with objects of shape { "name": "...", "passing": "value" | "pointer" | "reference" | "function-pointer", "hasDefaultValue": true | false }.
3. Add function kind with one of free, method, constructor, destructor, operator, lambda or prototype, plus isStatic and isVariadic booleans.
4. Add class isTemplate and isScopedEnum booleans; add bases as an array of source names.
5. Preserve constructs, startLine, endLine and diagnostics. Add a diagnostic column only when asserting it is valuable; the runner must accept its absence.
6. Keep C-DECL-01 and C-TYPE-04 cited by their existing fixtures, but update tests/corpus/README.md to state that Fase 5 is the first phase that can prove them structurally.

Use the following parameter expectation shape everywhere, including unnamed parameters:

    {
      "name": "",
      "passing": "value",
      "hasDefaultValue": false
    }

- [ ] **Step 5: Implement exact and minimum comparison**

In CorpusStructureTest.cpp deserialize expectations into small test-only structs, not production model types. Compare functions, classes and includes in source order.

For a well-formed fixture, require equal sequence lengths and compare every declared field. For malformed fixtures, for each expected function and class require one actual entry with all declared fields equal; permit actual extras. In all fixtures, require diagnostics to have equal sequence length, line and severity, and compare column only when the JSON object contains column.

Use selectGrammar for every fixture extension exactly as CorpusParsingTest.cpp does. Assert result.parsed before any structural comparison. Retain the existing CorpusParsingTest.cpp as the parser-only smoke check.

- [ ] **Step 6: Run the corpus and full suites**

Run:

    cmake -S . -B build
    cmake --build build --config Debug
    build/tests/Debug/codeqa_tests.exe --test-case="every corpus fixture matches its structural expectation"
    ctest --test-dir build -C Debug --output-on-failure
    build/Debug/codeqa.exe parse tests/corpus

Expected: the structural runner passes all fixtures; CTest stays green; the CLI still reports Files parsed: 13, C files: 4, C++ files: 9 and Parse errors: 2.

- [ ] **Step 7: Commit**

    git add CMakeLists.txt tests/CMakeLists.txt third_party/nlohmann third_party/README.md tests/unit/CorpusStructureTest.cpp tests/corpus
    git commit -m "test: validate structural extraction against the corpus"

---

## Final Verification

- [ ] Re-read docs/superpowers/specs/2026-08-06-fase-4b-extraccion-estructural-design.md and verify Tasks 1–5 cover models, embedded queries, error behaviour, deterministic ordering, JSON provenance, runner semantics, corpus limits and acceptance criteria.
- [ ] Run git diff --check master~5..HEAD, excluding only untouched vendored tree-sitter files. Expected: no whitespace errors in project changes.
- [ ] Run cmake -S . -B build, cmake --build build --config Debug and ctest --test-dir build -C Debug --output-on-failure. Expected: every test passes.
- [ ] Run build/Debug/codeqa.exe parse tests/corpus. Expected: the Phase 4a CLI output remains unchanged.
