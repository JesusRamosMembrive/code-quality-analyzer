# Fase 5, capa 1 – Modelo intermedio común Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Producir un `codeqa::model::ProjectModel` tipado, independiente de tree-sitter, para funciones, tipos, dependencias, globales y `typedef`.

**Architecture:** El frontend conserva `ParsedFile` como frontera sintáctica y añade las dos extracciones pendientes. `ProjectModelBuilder` adapta una secuencia de `ParsedFile` a agregados tipados y deterministas; `AnalyzeProject` coordina descubrimiento, selección de lenguaje, parsing y adaptación sin cambiar el CLI.

**Tech Stack:** C++17, CMake, doctest, tree-sitter C/C++ vendorizado.

## Global Constraints

- Solo `codeqa::frontend` conoce tree-sitter; ningún header público de `model` lo incluye o lo declara.
- Reutilizar `core::Language`, `core::SourceRange` y `core::Diagnostic`; no duplicar sus contratos.
- C++17, sin extensiones de compilador ni dependencias nuevas.
- Conservar orden determinista por rango; archivos con errores generan modelos parciales, no se descartan.
- Esta capa incluye variables de ámbito de archivo y `typedef`; excluye locales, cuerpos, control de flujo, llamadas, resolución, métricas, reglas e informes.
- El comando `codeqa parse` mantiene su salida actual.

## File Structure

- `include/codeqa/model/Models.h`: valores y agregados públicos del modelo común.
- `include/codeqa/model/ProjectModelBuilder.h`, `src/model/ProjectModelBuilder.cpp`: adaptación determinista desde frontend.
- `include/codeqa/application/AnalyzeProject.h`, `src/application/AnalyzeProject.cpp`: orquestación de análisis sin CLI.
- `src/frontend/queries/{c,cpp}/declarations.scm`: candidatos a global y typedef.
- `include/codeqa/frontend/StructuralModels.h`, `ParsedFile.h`, `StructuralExtractor.cpp`: modelos y adaptación sintáctica pendientes.
- `tests/unit/*Model*Test.cpp`, `*BuilderTest.cpp`, `AnalyzeProjectTest.cpp`: contratos y flujo real.

---

### Task 1: Define the public common-model contracts

**Files:**
- Create: `include/codeqa/model/Models.h`
- Create: `tests/unit/ModelTypesTest.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- `AnalysisState { Complete, Partial, Unavailable }`.
- `SymbolIdentity { std::string name; std::string qualifiedName; core::SourceRange range; }`.
- Typed enums and aggregates: `CallableKind`, `ParameterPassing`, `TypeKind`, `IncludeKind`, `ParameterModel`, `CallableModel`, `TypeModel`, `NamespaceModel`, `GlobalVariableModel`, `TypeAliasModel`, `DependencyModel`, `FileModel`, `ProjectModel`.
- `FileModel` owns ordered vectors named `dependencies`, `callables`, `types`, `namespaces`, `globalVariables`, `typeAliases`, plus language, state and diagnostics; `ProjectModel` owns `rootPath` and `files`.

- [ ] **Step 1: Write the failing contract test**

Create `ModelTypesTest.cpp` and assert real default/value behavior:

```cpp
TEST_CASE("common model preserves typed file aggregates")
{
    codeqa::model::FileModel file;
    CHECK(file.state == codeqa::model::AnalysisState::Unavailable);
    CHECK(file.callables.empty());
    CHECK(file.globalVariables.empty());

    codeqa::model::CallableModel callable;
    callable.symbol.name = "sum";
    callable.kind = codeqa::model::CallableKind::FreeFunction;
    file.callables.push_back(callable);
    CHECK(file.callables[0].symbol.name == "sum");
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

    cmake --build build --config Debug --target codeqa_tests

Expected: compilation fails because `codeqa/model/Models.h` does not exist.

- [ ] **Step 3: Implement the header-only typed aggregates**

Create `Models.h`. Use `std::optional<CppTypeDetails>` only on `TypeModel`; it owns `isTemplate`, `isScopedEnum` and `baseSpecifiers`. Keep common callable flags and parameters outside that optional. Use `core::Language`, `core::SourceRange` and `core::Diagnostic` directly. Do not introduce a base `SymbolModel` collection, virtual methods or parser types.

- [ ] **Step 4: Verify the model contracts**

Run:

    cmake --build build --config Debug
    build/tests/Debug/codeqa_tests.exe --test-case="common model preserves typed file aggregates"
    ctest --test-dir build -C Debug --output-on-failure

- [ ] **Step 5: Commit**

    git add include/codeqa/model/Models.h tests/unit/ModelTypesTest.cpp tests/CMakeLists.txt
    git commit -m "feat: add common analysis model contracts"

---

### Task 2: Extract file-scope variables and typedef aliases in the frontend

**Files:**
- Create: `src/frontend/queries/c/declarations.scm`
- Create: `src/frontend/queries/cpp/declarations.scm`
- Modify: `CMakeLists.txt`, `cmake/EmbeddedQueries.h.in`
- Modify: `include/codeqa/frontend/StructuralModels.h`, `include/codeqa/frontend/ParsedFile.h`
- Modify: `src/frontend/StructuralExtractor.cpp`
- Modify: `tests/unit/EmbeddedQueriesTest.cpp`, `tests/unit/StructuralExtractorTest.cpp`

**Interfaces:**
- `GlobalVariableModel { name, declaredType, range, isStatic, isExtern, hasInitializer }`.
- `TypeAliasModel { name, aliasedType, range }`.
- `ParsedFile` gains source-ordered `globalVariables` and `typeAliases`.

- [ ] **Step 1: Write failing extraction tests**

Add C and C++ temporary-source cases. C input must contain:

```c
static int cache = 1;
extern const char* title;
typedef unsigned long Size;
int function(void) { int local = 0; return local; }
```

Assert `cache` and `title` only in globals with their flags/initializer state, `Size` in aliases with `unsigned long`, and no `local`. Add an equivalent C++ `typedef` and namespace-scope global case.

- [ ] **Step 2: Run focused tests to verify RED**

Run:

    cmake --build build --config Debug --target codeqa_tests
    build/tests/Debug/codeqa_tests.exe --test-case="extracts file scope globals*,extracts typedef aliases*"

Expected: the new `ParsedFile` collections are empty or headers are missing.

- [ ] **Step 3: Embed declaration queries and adapt candidates**

Add both assets to `CODEQA_QUERY_FILES`, read them in CMake and expose distinct generated string views. Compile each query in `StructuralExtractor::Impl`.

Capture declarations and use CST ancestry to accept only translation-unit or namespace-scope declarations, never function/class bodies. A declaration with `typedef` yields aliases, not globals; one declarator may yield several globals. Extract declaration type spelling and alias target spelling from source ranges, trim whitespace, set `static`, `extern` and initializer flags from direct CST/specifier structure, then sort/deduplicate by range. Keep malformed candidates only when they are neither ERROR nor MISSING.

- [ ] **Step 4: Verify query and extraction regressions**

Run:

    cmake -S . -B build
    cmake --build build --config Debug
    build/tests/Debug/codeqa_tests.exe --test-case="all embedded structural queries compile,extracts file scope globals*,extracts typedef aliases*,extracts functions*"
    ctest --test-dir build -C Debug --output-on-failure

- [ ] **Step 5: Commit**

    git add CMakeLists.txt cmake/EmbeddedQueries.h.in include/codeqa/frontend/StructuralModels.h include/codeqa/frontend/ParsedFile.h src/frontend/StructuralExtractor.cpp src/frontend/queries tests/unit/EmbeddedQueriesTest.cpp tests/unit/StructuralExtractorTest.cpp
    git commit -m "feat: extract globals and typedef aliases"

---

### Task 3: Adapt parsed files into a deterministic common project model

**Files:**
- Create: `include/codeqa/model/ProjectModelBuilder.h`
- Create: `src/model/ProjectModelBuilder.cpp`
- Create: `tests/unit/ProjectModelBuilderTest.cpp`
- Modify: `CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**

```cpp
class ProjectModelBuilder
{
public:
    [[nodiscard]] ProjectModel build(
        const std::filesystem::path& rootPath,
        const std::vector<codeqa::frontend::ParsedFile>& files) const;
};
```

- [ ] **Step 1: Write failing adaptation tests**

Construct two `ParsedFile` values in the test: a parsed C++ file with a namespace, class, method, include and diagnostic; and an unparsed file with a read diagnostic. Assert sorted file paths, `Complete` for the clean file, `Unavailable` for the read failure, preservation of diagnostics, and no symbols invented for the unavailable file.

Add a nested namespace/class range fixture and assert `outer::Box::value` as the method qualified name while its source name remains `value`.

- [ ] **Step 2: Run RED**

Run:

    cmake --build build --config Debug --target codeqa_tests

Expected: compilation fails because `ProjectModelBuilder` is absent.

- [ ] **Step 3: Implement explicit one-way mapping**

Map frontend enums to model enums with total `switch` statements. Build identity from each frontend name/range. Determine lexical qualification by finding enclosing namespace/type ranges in the same file, ignoring anonymous namespace name components; retain an already qualified callable name unchanged. Map parsed files with diagnostics to `Partial`, parsed files without diagnostics to `Complete`, and unparsed files to `Unavailable`. Copy source order, then sort files by generic path and every model collection by range begin/end with stable ties.

- [ ] **Step 4: Verify builder behavior**

Run:

    cmake --build build --config Debug
    build/tests/Debug/codeqa_tests.exe --test-case="builds deterministic common project model*"
    ctest --test-dir build -C Debug --output-on-failure

- [ ] **Step 5: Commit**

    git add include/codeqa/model/ProjectModelBuilder.h src/model/ProjectModelBuilder.cpp tests/unit/ProjectModelBuilderTest.cpp CMakeLists.txt tests/CMakeLists.txt
    git commit -m "feat: adapt parsed files to common model"

---

### Task 4: Provide an application service that produces ProjectModel

**Files:**
- Create: `include/codeqa/application/AnalyzeProject.h`
- Create: `src/application/AnalyzeProject.cpp`
- Create: `tests/unit/AnalyzeProjectTest.cpp`
- Create: `tests/unit/CorpusModelTest.cpp`
- Modify: `CMakeLists.txt`, `tests/CMakeLists.txt`
- Modify: `tests/corpus/README.md`

**Interfaces:**

```cpp
struct AnalyzeProjectRequest
{
    codeqa::infrastructure::DiscoveryOptions discovery;
    std::vector<LanguageOverride> languageOverrides;
};

class AnalyzeProject
{
public:
    [[nodiscard]] codeqa::model::ProjectModel execute(const AnalyzeProjectRequest& request) const;
};
```

- [ ] **Step 1: Write a failing end-to-end unit test**

Use a temporary directory containing one `.c` global/typedef fixture, one `.cpp` class fixture and one malformed `.cpp` fixture. Execute `AnalyzeProject`; assert all three files are present, clean files are complete, the malformed file is partial with diagnostics and recovered declarations, and collections contain no local variable.

Create `CorpusModelTest.cpp` in the same RED step. Its test calls the intended service for `tests/corpus` and asserts 13 file models, two partial malformed files, preserved diagnostics, C global/typedef structures, existing C++ callable/type/dependency structures, and deterministic file/collection order.

- [ ] **Step 2: Run RED**

Run:

    cmake --build build --config Debug --target codeqa_tests

Expected: compilation fails because `AnalyzeProject` is absent.

- [ ] **Step 3: Implement orchestration without changing CLI**

Reuse `DiscoverProjectSources`, `DetectFileLanguage`, `selectGrammar`, one reusable `TreeSitterParser` and one reusable `StructuralExtractor`. Parse every discovered file, pass all results to `ProjectModelBuilder`, and use the discovery root as `ProjectModel.rootPath`. Preserve discovery diagnostics by materializing unavailable `ParsedFile` entries with their path and diagnostics. Do not call this service from `RunApplication.cpp`.

- [ ] **Step 4: Verify end-to-end and CLI compatibility**

Run:

    cmake --build build --config Debug
    build/tests/Debug/codeqa_tests.exe --test-case="analyzes a project into the common model,corpus adapts to the common model"
    ctest --test-dir build -C Debug --output-on-failure
    build/Debug/codeqa.exe parse tests/corpus
    git diff --check

Expected CLI totals remain 13/4/9/2.

- [ ] **Step 5: Document the completed layer boundary**

Update `tests/corpus/README.md` to state that the corpus now validates the first common-model layer; control flow, calls and locals remain the second Fase 5 layer. Do not change JSON structural expectations unless a new assertion proves a real frontend gap.

- [ ] **Step 6: Commit**

    git add include/codeqa/application/AnalyzeProject.h src/application/AnalyzeProject.cpp tests/unit/AnalyzeProjectTest.cpp tests/unit/CorpusModelTest.cpp tests/corpus/README.md CMakeLists.txt tests/CMakeLists.txt
    git commit -m "feat: analyze projects into common model"
