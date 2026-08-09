# Fase 7 — Motor de reglas y hallazgos Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Evaluar las métricas de Fase 6b (más algunos datos crudos del modelo común) mediante las 8 reglas que define el roadmap, produciendo hallazgos localizables con la regla de confianza ERROR/MISSING y la supresión inline ya decididas.

**Architecture:** Capa nueva `codeqa::rules` (pura, sin E/S) con la interfaz única `Rule` y un `RuleContext` estrecho (`FunctionMetricsModel` + `CallableModel`). Orquestador nuevo `codeqa::application::ApplyRules` aplica el filtro de confianza antes de invocar reglas y la supresión después. Dos cambios quirúrgicos sobre código ya fusionado: `codeqa::infrastructure::readFileContents` (extraído de Fase 4 y 6b, ahora con un tercer consumidor) y `FunctionComplexityMetrics.maxConditionRange` (Fase 6b, para que `complexity.condition` no tenga que re-escanear texto).

**Tech Stack:** C++17, CMake, doctest, nlohmann/json (tests del corpus), `<regex>` (nuevo — solo para el patrón de supresión, sobre texto de comentario ya aislado, no sobre código C++).

## Global Constraints

- `codeqa::rules` no depende de `codeqa::frontend` ni hace E/S — solo `codeqa/core/*`, `codeqa/model/Models.h`, `codeqa/metrics/MetricsModels.h` y la STL. Solo `ApplyRules` toca disco.
- `RuleContext` expone exactamente `{const metrics::FunctionMetricsModel& metrics; const model::CallableModel& callable;}` — ninguna regla ve texto fuente, comentarios ni diagnósticos.
- Los identificadores de regla usan notación con puntos, agrupados por familia: `size.*`, `signature.*`, `complexity.*` (ver `docs/superpowers/specs/2026-08-09-fase-7-motor-reglas-design.md` §4 para la razón: el roadmap tiene una inconsistencia real entre el ejemplo de la Fase 7 y el de la Fase 8, y esta es la resuelta).
- `Finding.severity` reutiliza `codeqa::core::DiagnosticSeverity` (Information/Warning/Error) — sin enum nuevo.
- Umbral de dos niveles por regla: `observedValue < warningLimit` → sin hallazgo; `[warningLimit, highLimit)` → `Warning`; `>= highLimit` → `Error`. `limitValue` reporta el umbral que determinó la severidad obtenida, no siempre `warningLimit`.
- El filtro de confianza (ERROR/MISSING) se aplica **antes** de invocar cualquier regla — una función excluida ni siquiera construye un `RuleContext`. La supresión se aplica **después**, sobre los `Finding` ya generados.
- `codeqa::infrastructure::readFileContents` es la única función de lectura de archivos compartida; `ParseSourceFile.cpp` (Fase 4) y `MeasureProject.cpp` (Fase 6b) se migran a usarla en el mismo cambio. `splitLines` NO se comparte (solo 2 ocurrencias tras esta fase — bajo el umbral de "regla de tres" ya establecido en este proyecto).
- `FunctionComplexityMetrics.maxConditionRange` se añade **al final** del struct (mismo patrón *append-only* que Fase 6a/6b) y se rellena con una comparación explícita (`>`, no `std::max`) para conservar qué entrada produjo el máximo — empate resuelto a favor de la primera en orden de aparición.
- Todos los umbrales de las 8 reglas son constantes privadas en el código — ningún acceso a `codeqa.json` ni a ningún mecanismo de configuración en esta fase.
- `core::SourceRange`/`SourceLocation` no tienen `operator==` — comparar campo a campo (`.begin.line`, `.begin.column`, etc.), nunca `==` directo sobre el struct.
- No se crea ningún comando CLI nuevo ni se cambia el comportamiento visible de `codeqa parse`/`codeqa --help`.

## File Structure

- `include/codeqa/infrastructure/ReadFileContents.h` + `src/infrastructure/ReadFileContents.cpp` (nuevo): lectura binaria compartida.
- `include/codeqa/metrics/MetricsModels.h` (modificado): `maxConditionRange`.
- `src/metrics/CalculateFunctionMetrics.cpp` (modificado): lo rellena.
- `include/codeqa/rules/RuleModels.h` (nuevo): `Finding`, `FileFindingsModel`, `ProjectFindingsModel`.
- `include/codeqa/rules/Rule.h` (nuevo): `RuleContext`, interfaz `Rule`.
- `include/codeqa/rules/Rules.h` + `src/rules/Rules.cpp` (nuevo): las 8 reglas concretas + `evaluateThreshold` + `allRules()`.
- `include/codeqa/application/ApplyRules.h` + `src/application/ApplyRules.cpp` (nuevo): orquestador.
- `src/frontend/ParseSourceFile.cpp`, `src/application/MeasureProject.cpp` (modificados): migran a `readFileContents` compartido.
- `tests/corpus/{nesting/deep-nesting,parameters/cpp-parameters}.findings.json` (nuevo), `tests/unit/CorpusFindingsTest.cpp` (nuevo).
- `CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/corpus/CheckCorpusIntegrity.cmake`, `tests/corpus/README.md` (modificados).

---

### Task 1: Extraer `codeqa::infrastructure::readFileContents`

**Files:**
- Create: `include/codeqa/infrastructure/ReadFileContents.h`
- Create: `src/infrastructure/ReadFileContents.cpp`
- Create: `tests/unit/ReadFileContentsTest.cpp`
- Modify: `src/frontend/ParseSourceFile.cpp`
- Modify: `src/application/MeasureProject.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `codeqa::infrastructure::readFileContents(const std::filesystem::path&) -> std::optional<std::string>` — lectura binaria completa, `std::nullopt` si el archivo no existe o no se puede leer.

- [ ] **Step 1: Write the failing test**

Create `tests/unit/ReadFileContentsTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/infrastructure/ReadFileContents.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace
{

class TempFile
{
public:
    TempFile(const std::string& name, const std::string& content)
        : path_(std::filesystem::temp_directory_path() / "codeqa_read_file_contents_test" / name)
    {
        std::filesystem::create_directories(path_.parent_path());
        std::ofstream(path_, std::ios::binary) << content;
    }

    ~TempFile()
    {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("readFileContents returns the exact bytes of an existing file")
{
    const TempFile file{"sample.txt", "line one\nline two\n"};

    const std::optional<std::string> content = codeqa::infrastructure::readFileContents(file.path());

    REQUIRE(content.has_value());
    CHECK(*content == "line one\nline two\n");
}

TEST_CASE("readFileContents returns an empty string for an empty file")
{
    const TempFile file{"empty.txt", ""};

    const std::optional<std::string> content = codeqa::infrastructure::readFileContents(file.path());

    REQUIRE(content.has_value());
    CHECK(content->empty());
}

TEST_CASE("readFileContents returns nullopt for a missing file")
{
    const std::filesystem::path missing =
        std::filesystem::temp_directory_path() / "codeqa_read_file_contents_test" / "does-not-exist.txt";

    const std::optional<std::string> content = codeqa::infrastructure::readFileContents(missing);

    CHECK_FALSE(content.has_value());
}
```

Add `unit/ReadFileContentsTest.cpp` to `tests/CMakeLists.txt`, at the end of the `add_executable(codeqa_tests ...)` list (after `unit/CorpusMetricsTest.cpp`).

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails — `codeqa/infrastructure/ReadFileContents.h` does not exist.

- [ ] **Step 3: Create the header and implementation**

Create `include/codeqa/infrastructure/ReadFileContents.h`:

```cpp
#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace codeqa::infrastructure
{

[[nodiscard]] std::optional<std::string> readFileContents(const std::filesystem::path& path);

} // namespace codeqa::infrastructure
```

Create `src/infrastructure/ReadFileContents.cpp`:

```cpp
#include "codeqa/infrastructure/ReadFileContents.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <system_error>

namespace codeqa::infrastructure
{

std::optional<std::string> readFileContents(const std::filesystem::path& path)
{
    std::error_code sizeError;
    const std::uintmax_t size = std::filesystem::file_size(path, sizeError);

    if (sizeError)
    {
        return std::nullopt;
    }

    std::ifstream input(path, std::ios::binary);

    if (!input)
    {
        return std::nullopt;
    }

    std::string content(static_cast<std::size_t>(size), '\0');

    if (size > 0)
    {
        input.read(content.data(), static_cast<std::streamsize>(size));

        if (!input)
        {
            return std::nullopt;
        }
    }

    return content;
}

} // namespace codeqa::infrastructure
```

Add `src/infrastructure/ReadFileContents.cpp` to `add_library(codeqa_core STATIC ...)` in `CMakeLists.txt`, right after `src/infrastructure/GlobMatcher.cpp`.

- [ ] **Step 4: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass, including the 3 new ones.

- [ ] **Step 5: Migrate `ParseSourceFile.cpp` to the shared function**

Replace the full content of `src/frontend/ParseSourceFile.cpp` with:

```cpp
#include "codeqa/frontend/ParseSourceFile.h"

#include "codeqa/core/DiagnosticSeverity.h"
#include "codeqa/frontend/StructuralExtractor.h"
#include "codeqa/infrastructure/ReadFileContents.h"

#include <cstring>
#include <filesystem>
#include <optional>

namespace codeqa::frontend
{

namespace
{

codeqa::core::Diagnostic makeDiagnostic(
    const std::string& code,
    const std::string& message,
    const std::filesystem::path& path,
    std::size_t line,
    std::size_t column)
{
    codeqa::core::Diagnostic diagnostic;
    diagnostic.code = code;
    diagnostic.severity = codeqa::core::DiagnosticSeverity::Error;
    diagnostic.message = message;
    diagnostic.location.filePath = path;
    diagnostic.location.line = line + 1;
    diagnostic.location.column = column + 1;

    return diagnostic;
}

} // namespace

ParsedFile parseSourceFile(
    const std::filesystem::path& path,
    Grammar grammar,
    const std::string& grammarReason,
    TreeSitterParser& parser,
    StructuralExtractor& extractor)
{
    ParsedFile result;
    result.path = path;
    result.language = grammar == Grammar::C
        ? codeqa::core::Language::C
        : codeqa::core::Language::Cpp;
    result.grammar = grammar;
    result.grammarReason = grammarReason;

    const std::optional<std::string> source = codeqa::infrastructure::readFileContents(path);

    if (!source.has_value())
    {
        result.diagnostics.push_back(makeDiagnostic(
            "PARSE003", "Could not read the file.", path, 0, 0));
        return result;
    }

    std::optional<SyntaxTree> tree = parser.parse(grammar, *source);

    if (!tree.has_value())
    {
        result.diagnostics.push_back(makeDiagnostic(
            "PARSE003", "The parser could not process the file.", path, 0, 0));
        return result;
    }

    result.parsed = true;
    for (const SyntaxError& error : tree->syntaxErrors())
    {
        result.diagnostics.push_back(makeDiagnostic(
            error.missing ? "PARSE002" : "PARSE001",
            error.missing ? "Expected token is missing." : "Could not parse this region.",
            path,
            error.line,
            error.column));
    }

    extractor.extract(*tree, grammar, *source, path, result);

    return result;
}

} // namespace codeqa::frontend
```

(Removed the private `readFile` helper and its now-unused `<fstream>`/`<system_error>` includes; everything else is unchanged.)

- [ ] **Step 6: Migrate `MeasureProject.cpp` to the shared function**

Replace the full content of `src/application/MeasureProject.cpp` with:

```cpp
#include "codeqa/application/MeasureProject.h"

#include "codeqa/core/DiagnosticSeverity.h"
#include "codeqa/infrastructure/ReadFileContents.h"
#include "codeqa/metrics/CalculateFunctionMetrics.h"

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace codeqa::application
{

namespace
{

std::vector<std::string> splitLines(const std::string& content)
{
    std::vector<std::string> lines;
    std::size_t start = 0;

    while (start <= content.size())
    {
        const std::size_t newline = content.find('\n', start);
        const std::size_t end = (newline == std::string::npos) ? content.size() : newline;
        std::size_t lineEnd = end;

        if (lineEnd > start && content[lineEnd - 1] == '\r')
        {
            --lineEnd;
        }

        lines.push_back(content.substr(start, lineEnd - start));

        if (newline == std::string::npos)
        {
            break;
        }

        start = newline + 1;
    }

    return lines;
}

codeqa::core::Diagnostic makeUnreadableDiagnostic(const std::filesystem::path& path)
{
    codeqa::core::Diagnostic diagnostic;
    diagnostic.code = "METRICS001";
    diagnostic.severity = codeqa::core::DiagnosticSeverity::Error;
    diagnostic.message = "Could not reread the file to compute metrics.";
    diagnostic.location.filePath = path;
    diagnostic.location.line = 1;
    diagnostic.location.column = 1;
    return diagnostic;
}

} // namespace

codeqa::metrics::ProjectMetricsModel MeasureProject::execute(const codeqa::model::ProjectModel& project) const
{
    codeqa::metrics::ProjectMetricsModel result;
    result.rootPath = project.rootPath;

    for (const codeqa::model::FileModel& file : project.files)
    {
        codeqa::metrics::FileMetricsModel fileMetrics;
        fileMetrics.path = file.path;

        const std::optional<std::string> content = codeqa::infrastructure::readFileContents(file.path);

        if (!content.has_value())
        {
            fileMetrics.diagnostics.push_back(makeUnreadableDiagnostic(file.path));
            result.files.push_back(std::move(fileMetrics));
            continue;
        }

        const std::vector<std::string> sourceLines = splitLines(*content);

        for (const codeqa::model::CallableModel& callable : file.callables)
        {
            if (callable.kind == codeqa::model::CallableKind::Prototype)
            {
                continue;
            }

            fileMetrics.functions.push_back(
                codeqa::metrics::calculateFunctionMetrics(callable, file.comments, sourceLines));
        }

        result.files.push_back(std::move(fileMetrics));
    }

    return result;
}

} // namespace codeqa::application
```

(Removed the private `readFileContents` helper and its comment explaining the old duplication; added a direct `<filesystem>` include, closing the IWYU gap the Fase 6b final review deferred; everything else unchanged.)

- [ ] **Step 7: Run the full suite to verify no regressions**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: every test passes, including the existing `ParseSourceFileTest.cpp` and `MeasureProjectTest.cpp` suites (both already cover the read-success and read-failure paths, so this is the regression safety net for the migration).

- [ ] **Step 8: Commit**

```bash
git add include/codeqa/infrastructure/ReadFileContents.h src/infrastructure/ReadFileContents.cpp tests/unit/ReadFileContentsTest.cpp src/frontend/ParseSourceFile.cpp src/application/MeasureProject.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "refactor: extract shared codeqa::infrastructure::readFileContents"
```

---

### Task 2: `FunctionComplexityMetrics.maxConditionRange`

**Files:**
- Modify: `include/codeqa/metrics/MetricsModels.h`
- Modify: `src/metrics/CalculateFunctionMetrics.cpp`
- Modify: `tests/unit/MetricsModelsTest.cpp`
- Modify: `tests/unit/CalculateFunctionMetricsTest.cpp`

**Interfaces:**
- Produces: `FunctionComplexityMetrics.maxConditionRange: std::optional<core::SourceRange>` — el rango de la entrada de `controlFlow` cuyo `conditionRange` produjo `maxConditionComplexity` (empate → la primera en orden de aparición). `std::nullopt` si ninguna entrada tiene `conditionRange`.

- [ ] **Step 1: Write the failing tests**

In `tests/unit/MetricsModelsTest.cpp`, add to the existing `TEST_CASE("metrics models default to zero-valued, empty state")`, right after the existing `complexity.*` checks:

```cpp
    CHECK_FALSE(function.complexity.maxConditionRange.has_value());
```

In `tests/unit/CalculateFunctionMetricsTest.cpp`, extend `TEST_CASE("complexity metrics for an if/else-if chain with && and ||, matching classify() in the corpus")` by adding these lines right after the existing `CHECK(metrics.complexity.returnCount == 3);`:

```cpp
    REQUIRE(metrics.complexity.maxConditionRange.has_value());
    CHECK(metrics.complexity.maxConditionRange->begin.line == 3);
    CHECK(metrics.complexity.maxConditionRange->begin.column == 9);
    CHECK(metrics.complexity.maxConditionRange->end.line == 3);
    CHECK(metrics.complexity.maxConditionRange->end.column == 28);
```

(This asserts the outer `if`'s `conditionRange`, not the inner `else if`'s — both produce exactly 1 boolean operator, a tie, and the outer one appears first in `callable.controlFlow`'s source-order, so it wins per the tie-break rule.)

Extend `TEST_CASE("a function with no control flow has cyclomatic complexity 1 and depth 0")` by adding:

```cpp
    CHECK_FALSE(metrics.complexity.maxConditionRange.has_value());
```

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails to compile — `FunctionComplexityMetrics` has no member `maxConditionRange`.

- [ ] **Step 3: Add the field**

In `include/codeqa/metrics/MetricsModels.h`, add `#include "codeqa/core/SourceRange.h"` and `#include <optional>` to the includes block, then add the trailing field to `FunctionComplexityMetrics`:

```cpp
struct FunctionComplexityMetrics
{
    std::size_t cyclomaticComplexity{0};
    std::size_t maxConditionComplexity{0};
    std::size_t maxNestingDepth{0};
    std::size_t branchCount{0};
    std::size_t returnCount{0};
    std::optional<codeqa::core::SourceRange> maxConditionRange;
};
```

- [ ] **Step 4: Populate the field**

In `src/metrics/CalculateFunctionMetrics.cpp`, inside `calculateComplexityMetrics`, replace:

```cpp
        if (flow.conditionRange.has_value())
        {
            const std::size_t operators = countBooleanOperators(*flow.conditionRange, sourceLines, fileComments);
            metrics.maxConditionComplexity = std::max(metrics.maxConditionComplexity, operators);
            booleanOperatorCount += operators;
        }
```

with:

```cpp
        if (flow.conditionRange.has_value())
        {
            const std::size_t operators = countBooleanOperators(*flow.conditionRange, sourceLines, fileComments);

            if (operators > metrics.maxConditionComplexity)
            {
                metrics.maxConditionComplexity = operators;
                metrics.maxConditionRange = flow.conditionRange;
            }

            booleanOperatorCount += operators;
        }
```

- [ ] **Step 5: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass.

- [ ] **Step 6: Commit**

```bash
git add include/codeqa/metrics/MetricsModels.h src/metrics/CalculateFunctionMetrics.cpp tests/unit/MetricsModelsTest.cpp tests/unit/CalculateFunctionMetricsTest.cpp
git commit -m "feat: track the source range of the condition with maximum boolean-operator complexity"
```

---

### Task 3: `codeqa::rules` model types and `Rule` interface

**Files:**
- Create: `include/codeqa/rules/RuleModels.h`
- Create: `include/codeqa/rules/Rule.h`
- Create: `tests/unit/RuleTest.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `codeqa::metrics::FunctionMetricsModel` (Fase 6b), `codeqa::model::CallableModel`/`SymbolIdentity` (Fase 5), `codeqa::core::DiagnosticSeverity`/`SourceLocation`/`Language`.
- Produces: `codeqa::rules::Finding`, `FileFindingsModel`, `ProjectFindingsModel`, `RuleContext{metrics, callable}`, abstract class `Rule` with `id()`, `appliesTo(Language)`, `evaluate(RuleContext) -> vector<Finding>`.

- [ ] **Step 1: Write the failing test**

Create `tests/unit/RuleTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/rules/Rule.h"

#include <string>
#include <vector>

TEST_CASE("rule models default to empty, informational state")
{
    codeqa::rules::Finding finding;
    CHECK(finding.ruleId.empty());
    CHECK(finding.severity == codeqa::core::DiagnosticSeverity::Information);
    CHECK(finding.description.empty());
    CHECK(finding.recommendation.empty());
    CHECK_FALSE(finding.observedValue.has_value());
    CHECK_FALSE(finding.limitValue.has_value());
    CHECK(finding.symbolName.empty());

    codeqa::rules::FileFindingsModel file;
    CHECK(file.path.empty());
    CHECK(file.findings.empty());

    codeqa::rules::ProjectFindingsModel project;
    CHECK(project.rootPath.empty());
    CHECK(project.files.empty());
}

namespace
{

class AlwaysFiresRule : public codeqa::rules::Rule
{
public:
    [[nodiscard]] std::string id() const override { return "test.always-fires"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language language) const override
    {
        return language == codeqa::core::Language::Cpp;
    }

    [[nodiscard]] std::vector<codeqa::rules::Finding> evaluate(
        const codeqa::rules::RuleContext& context) const override
    {
        codeqa::rules::Finding finding;
        finding.ruleId = id();
        finding.symbolName = context.callable.symbol.name;
        return {finding};
    }
};

} // namespace

TEST_CASE("a concrete Rule can be evaluated through the base interface")
{
    codeqa::model::CallableModel callable;
    callable.symbol.name = "sample";
    codeqa::metrics::FunctionMetricsModel metrics;
    const codeqa::rules::RuleContext context{metrics, callable};

    const AlwaysFiresRule rule;
    CHECK(rule.id() == "test.always-fires");
    CHECK(rule.appliesTo(codeqa::core::Language::Cpp));
    CHECK_FALSE(rule.appliesTo(codeqa::core::Language::C));

    const std::vector<codeqa::rules::Finding> findings = rule.evaluate(context);
    REQUIRE(findings.size() == 1);
    CHECK(findings[0].ruleId == "test.always-fires");
    CHECK(findings[0].symbolName == "sample");
}
```

Add `unit/RuleTest.cpp` to `tests/CMakeLists.txt`, after `unit/ReadFileContentsTest.cpp`.

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails — `codeqa/rules/Rule.h` does not exist.

- [ ] **Step 3: Create the headers**

Create `include/codeqa/rules/RuleModels.h`:

```cpp
#pragma once

#include "codeqa/core/DiagnosticSeverity.h"
#include "codeqa/core/SourceLocation.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace codeqa::rules
{

struct Finding
{
    std::string ruleId;
    codeqa::core::DiagnosticSeverity severity{codeqa::core::DiagnosticSeverity::Information};
    std::string description;
    std::string recommendation;
    std::optional<long long> observedValue;
    std::optional<long long> limitValue;
    codeqa::core::SourceLocation location;
    std::string symbolName;
};

struct FileFindingsModel
{
    std::filesystem::path path;
    std::vector<Finding> findings;
};

struct ProjectFindingsModel
{
    std::filesystem::path rootPath;
    std::vector<FileFindingsModel> files;
};

} // namespace codeqa::rules
```

Create `include/codeqa/rules/Rule.h`:

```cpp
#pragma once

#include "codeqa/core/Language.h"
#include "codeqa/metrics/MetricsModels.h"
#include "codeqa/model/Models.h"
#include "codeqa/rules/RuleModels.h"

#include <string>
#include <vector>

namespace codeqa::rules
{

struct RuleContext
{
    const codeqa::metrics::FunctionMetricsModel& metrics;
    const codeqa::model::CallableModel& callable;
};

class Rule
{
public:
    virtual ~Rule() = default;

    [[nodiscard]] virtual std::string id() const = 0;
    [[nodiscard]] virtual bool appliesTo(codeqa::core::Language language) const = 0;
    [[nodiscard]] virtual std::vector<Finding> evaluate(const RuleContext& context) const = 0;
};

} // namespace codeqa::rules
```

- [ ] **Step 4: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/rules/RuleModels.h include/codeqa/rules/Rule.h tests/unit/RuleTest.cpp tests/CMakeLists.txt
git commit -m "feat: add the codeqa::rules model types and the Rule interface"
```

---

### Task 4a: Seis reglas de umbral simple + `evaluateThreshold`

**Files:**
- Create: `include/codeqa/rules/Rules.h`
- Create: `src/rules/Rules.cpp`
- Create: `tests/unit/RulesTest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `codeqa::rules::Rule`/`RuleContext`/`Finding` (Task 3).
- Produces: `codeqa::rules::allRules() -> std::vector<std::unique_ptr<Rule>>`, devolviendo en esta tarea 6 reglas: `size.function-length`, `size.local-variables`, `signature.parameter-count`, `signature.consecutive-booleans`, `complexity.cyclomatic`, `complexity.returns`. Task 4b añade las 2 restantes al mismo `allRules()`.

- [ ] **Step 1: Write the failing tests**

Create `tests/unit/RulesTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/rules/Rules.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

const codeqa::rules::Rule& ruleNamed(
    const std::vector<std::unique_ptr<codeqa::rules::Rule>>& rules, const std::string& id)
{
    for (const auto& rule : rules)
    {
        if (rule->id() == id)
        {
            return *rule;
        }
    }

    throw std::runtime_error("Expected rule was not registered: " + id);
}

codeqa::model::CallableModel makeCallable()
{
    codeqa::model::CallableModel callable;
    callable.symbol.name = "sample";
    callable.symbol.qualifiedName = "Sample::sample";
    callable.symbol.range.begin.line = 10;
    callable.symbol.range.begin.column = 1;
    return callable;
}

} // namespace

TEST_CASE("size.function-length fires only at or above its warning threshold")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules();
    const codeqa::rules::Rule& rule = ruleNamed(rules, "size.function-length");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;

    metrics.size.physicalLines = 49;
    CHECK(rule.evaluate(codeqa::rules::RuleContext{metrics, callable}).empty());

    metrics.size.physicalLines = 50;
    const std::vector<codeqa::rules::Finding> atWarning =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(atWarning.size() == 1);
    CHECK(atWarning[0].ruleId == "size.function-length");
    CHECK(atWarning[0].severity == codeqa::core::DiagnosticSeverity::Warning);
    REQUIRE(atWarning[0].observedValue.has_value());
    CHECK(*atWarning[0].observedValue == 50);
    REQUIRE(atWarning[0].limitValue.has_value());
    CHECK(*atWarning[0].limitValue == 50);
    CHECK(atWarning[0].symbolName == "Sample::sample");
    CHECK(atWarning[0].location.line == 10);

    metrics.size.physicalLines = 90;
    const std::vector<codeqa::rules::Finding> atHigh =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(atHigh.size() == 1);
    CHECK(atHigh[0].severity == codeqa::core::DiagnosticSeverity::Error);
    REQUIRE(atHigh[0].limitValue.has_value());
    CHECK(*atHigh[0].limitValue == 90);
}

TEST_CASE("size.local-variables reads the callable's local variable count directly")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules();
    const codeqa::rules::Rule& rule = ruleNamed(rules, "size.local-variables");
    codeqa::model::CallableModel callable = makeCallable();
    callable.localVariables.resize(8);
    codeqa::metrics::FunctionMetricsModel metrics;

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(findings.size() == 1);
    REQUIRE(findings[0].observedValue.has_value());
    CHECK(*findings[0].observedValue == 8);
    CHECK(findings[0].severity == codeqa::core::DiagnosticSeverity::Warning);

    callable.localVariables.resize(7);
    CHECK(rule.evaluate(codeqa::rules::RuleContext{metrics, callable}).empty());
}

TEST_CASE("signature.parameter-count points at the parameter list, not the function start")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules();
    const codeqa::rules::Rule& rule = ruleNamed(rules, "signature.parameter-count");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::core::SourceRange parameterListRange;
    parameterListRange.begin.line = 10;
    parameterListRange.begin.column = 13;
    callable.parameterListRange = parameterListRange;
    codeqa::metrics::FunctionMetricsModel metrics;
    metrics.signature.parameterCount = 4;

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(findings.size() == 1);
    REQUIRE(findings[0].observedValue.has_value());
    CHECK(*findings[0].observedValue == 4);
    CHECK(findings[0].location.line == 10);
    CHECK(findings[0].location.column == 13);
}

TEST_CASE("signature.parameter-count falls back to the function start when there is no parameter list")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules();
    const codeqa::rules::Rule& rule = ruleNamed(rules, "signature.parameter-count");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;
    metrics.signature.parameterCount = 4;

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(findings.size() == 1);
    CHECK(findings[0].location.line == 10);
    CHECK(findings[0].location.column == 1);
}

TEST_CASE("signature.consecutive-booleans finds the longest run of by-value bool parameters")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules();
    const codeqa::rules::Rule& rule = ruleNamed(rules, "signature.consecutive-booleans");
    codeqa::model::CallableModel callable = makeCallable();
    callable.parameters = {
        codeqa::model::ParameterModel{"a", codeqa::model::ParameterPassing::Value, false, "bool"},
        codeqa::model::ParameterModel{"b", codeqa::model::ParameterPassing::Value, false, "int"},
        codeqa::model::ParameterModel{"c", codeqa::model::ParameterPassing::Value, false, "bool"},
        codeqa::model::ParameterModel{"d", codeqa::model::ParameterPassing::Value, false, "bool"},
        codeqa::model::ParameterModel{"e", codeqa::model::ParameterPassing::Pointer, false, "bool"}};
    codeqa::metrics::FunctionMetricsModel metrics;

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(findings.size() == 1);
    REQUIRE(findings[0].observedValue.has_value());
    CHECK(*findings[0].observedValue == 2);
    CHECK(findings[0].severity == codeqa::core::DiagnosticSeverity::Warning);
}

TEST_CASE("signature.consecutive-booleans produces no finding when no run reaches the threshold")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules();
    const codeqa::rules::Rule& rule = ruleNamed(rules, "signature.consecutive-booleans");
    codeqa::model::CallableModel callable = makeCallable();
    callable.parameters = {
        codeqa::model::ParameterModel{"a", codeqa::model::ParameterPassing::Value, false, "bool"},
        codeqa::model::ParameterModel{"b", codeqa::model::ParameterPassing::Value, false, "int"}};
    codeqa::metrics::FunctionMetricsModel metrics;

    CHECK(rule.evaluate(codeqa::rules::RuleContext{metrics, callable}).empty());
}

TEST_CASE("complexity.cyclomatic reads cyclomaticComplexity from the metrics")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules();
    const codeqa::rules::Rule& rule = ruleNamed(rules, "complexity.cyclomatic");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;

    metrics.complexity.cyclomaticComplexity = 9;
    CHECK(rule.evaluate(codeqa::rules::RuleContext{metrics, callable}).empty());

    metrics.complexity.cyclomaticComplexity = 15;
    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(findings.size() == 1);
    CHECK(findings[0].severity == codeqa::core::DiagnosticSeverity::Error);
}

TEST_CASE("complexity.returns reads returnCount from the metrics")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules();
    const codeqa::rules::Rule& rule = ruleNamed(rules, "complexity.returns");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;

    metrics.complexity.returnCount = 3;
    CHECK(rule.evaluate(codeqa::rules::RuleContext{metrics, callable}).empty());

    metrics.complexity.returnCount = 4;
    CHECK(rule.evaluate(codeqa::rules::RuleContext{metrics, callable}).size() == 1);
}

TEST_CASE("all six threshold rules apply to both C and C++")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules();
    REQUIRE(rules.size() == 6);

    for (const auto& rule : rules)
    {
        CHECK(rule->appliesTo(codeqa::core::Language::C));
        CHECK(rule->appliesTo(codeqa::core::Language::Cpp));
    }
}
```

Add `unit/RulesTest.cpp` to `tests/CMakeLists.txt`, after `unit/RuleTest.cpp`.

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails — `codeqa/rules/Rules.h` does not exist.

- [ ] **Step 3: Create the header**

Create `include/codeqa/rules/Rules.h`:

```cpp
#pragma once

#include "codeqa/rules/Rule.h"

#include <memory>
#include <vector>

namespace codeqa::rules
{

[[nodiscard]] std::vector<std::unique_ptr<Rule>> allRules();

} // namespace codeqa::rules
```

- [ ] **Step 4: Implement the six rules**

Create `src/rules/Rules.cpp`:

```cpp
#include "codeqa/rules/Rules.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace codeqa::rules
{

namespace
{

std::optional<Finding> evaluateThreshold(
    std::string ruleId,
    std::string description,
    std::string recommendation,
    long long observedValue,
    long long warningLimit,
    long long highLimit,
    codeqa::core::SourceLocation location,
    std::string symbolName)
{
    if (observedValue < warningLimit)
    {
        return std::nullopt;
    }

    Finding finding;
    finding.ruleId = std::move(ruleId);
    finding.severity = observedValue >= highLimit
        ? codeqa::core::DiagnosticSeverity::Error
        : codeqa::core::DiagnosticSeverity::Warning;
    finding.description = std::move(description);
    finding.recommendation = std::move(recommendation);
    finding.observedValue = observedValue;
    finding.limitValue = observedValue >= highLimit ? highLimit : warningLimit;
    finding.location = std::move(location);
    finding.symbolName = std::move(symbolName);
    return finding;
}

// Duplicated from CalculateFunctionMetrics.cpp deliberately -- this is its
// second occurrence, still under the "rule of three" this project already
// uses to decide when small predicates get shared vs. copied (see the Fase
// 6b final review and this phase's design doc, section 7).
bool isBooleanValueParameter(const codeqa::model::ParameterModel& parameter)
{
    if (parameter.passing != codeqa::model::ParameterPassing::Value)
    {
        return false;
    }

    std::istringstream stream(parameter.declaredType);
    std::vector<std::string> tokens;
    std::string token;

    while (stream >> token)
    {
        if (token != "const" && token != "volatile")
        {
            tokens.push_back(token);
        }
    }

    return tokens.size() == 1 && (tokens[0] == "bool" || tokens[0] == "_Bool");
}

std::vector<Finding> singleOrEmpty(const std::optional<Finding>& finding)
{
    return finding.has_value() ? std::vector<Finding>{*finding} : std::vector<Finding>{};
}

class FunctionLengthRule : public Rule
{
public:
    [[nodiscard]] std::string id() const override { return "size.function-length"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        return singleOrEmpty(evaluateThreshold(
            id(),
            "Function is too long.",
            "Consider splitting this function into smaller ones.",
            static_cast<long long>(context.metrics.size.physicalLines),
            50,
            90,
            context.callable.symbol.range.begin,
            context.callable.symbol.qualifiedName));
    }
};

class LocalVariableCountRule : public Rule
{
public:
    [[nodiscard]] std::string id() const override { return "size.local-variables"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        return singleOrEmpty(evaluateThreshold(
            id(),
            "Function declares too many local variables.",
            "Consider extracting part of this function's logic into helper functions.",
            static_cast<long long>(context.callable.localVariables.size()),
            8,
            12,
            context.callable.symbol.range.begin,
            context.callable.symbol.qualifiedName));
    }
};

class ParameterCountRule : public Rule
{
public:
    [[nodiscard]] std::string id() const override { return "signature.parameter-count"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        const codeqa::core::SourceLocation location = context.callable.parameterListRange.has_value()
            ? context.callable.parameterListRange->begin
            : context.callable.symbol.range.begin;
        return singleOrEmpty(evaluateThreshold(
            id(),
            "Function has too many parameters.",
            "Consider grouping related parameters into a struct.",
            static_cast<long long>(context.metrics.signature.parameterCount),
            4,
            7,
            location,
            context.callable.symbol.qualifiedName));
    }
};

class ConsecutiveBooleanParametersRule : public Rule
{
public:
    [[nodiscard]] std::string id() const override { return "signature.consecutive-booleans"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        std::size_t longestRun = 0;
        std::size_t currentRun = 0;

        for (const codeqa::model::ParameterModel& parameter : context.callable.parameters)
        {
            if (isBooleanValueParameter(parameter))
            {
                ++currentRun;
                longestRun = std::max(longestRun, currentRun);
            }
            else
            {
                currentRun = 0;
            }
        }

        const codeqa::core::SourceLocation location = context.callable.parameterListRange.has_value()
            ? context.callable.parameterListRange->begin
            : context.callable.symbol.range.begin;
        return singleOrEmpty(evaluateThreshold(
            id(),
            "Function has consecutive boolean parameters.",
            "Consider replacing consecutive boolean parameters with an enum or a struct to "
            "avoid the \"boolean trap\" at call sites.",
            static_cast<long long>(longestRun),
            2,
            3,
            location,
            context.callable.symbol.qualifiedName));
    }
};

class CyclomaticComplexityRule : public Rule
{
public:
    [[nodiscard]] std::string id() const override { return "complexity.cyclomatic"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        return singleOrEmpty(evaluateThreshold(
            id(),
            "Cyclomatic complexity is too high.",
            "Consider splitting this function to reduce the number of independent paths through it.",
            static_cast<long long>(context.metrics.complexity.cyclomaticComplexity),
            10,
            15,
            context.callable.symbol.range.begin,
            context.callable.symbol.qualifiedName));
    }
};

class ReturnCountRule : public Rule
{
public:
    [[nodiscard]] std::string id() const override { return "complexity.returns"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        return singleOrEmpty(evaluateThreshold(
            id(),
            "Function has too many return points.",
            "Consider consolidating exit points to make control flow easier to follow.",
            static_cast<long long>(context.metrics.complexity.returnCount),
            4,
            6,
            context.callable.symbol.range.begin,
            context.callable.symbol.qualifiedName));
    }
};

} // namespace

std::vector<std::unique_ptr<Rule>> allRules()
{
    std::vector<std::unique_ptr<Rule>> rules;
    rules.push_back(std::make_unique<FunctionLengthRule>());
    rules.push_back(std::make_unique<LocalVariableCountRule>());
    rules.push_back(std::make_unique<ParameterCountRule>());
    rules.push_back(std::make_unique<ConsecutiveBooleanParametersRule>());
    rules.push_back(std::make_unique<CyclomaticComplexityRule>());
    rules.push_back(std::make_unique<ReturnCountRule>());
    return rules;
}

} // namespace codeqa::rules
```

- [ ] **Step 5: Register the new sources**

In `CMakeLists.txt`, add `src/rules/Rules.cpp` to `add_library(codeqa_core STATIC ...)`, right after `src/metrics/CalculateFunctionMetrics.cpp`.

- [ ] **Step 6: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass.

- [ ] **Step 7: Commit**

```bash
git add include/codeqa/rules/Rules.h src/rules/Rules.cpp tests/unit/RulesTest.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: implement the six simple-location threshold rules"
```

---

### Task 4b: `complexity.condition` y `complexity.nesting`

**Files:**
- Modify: `src/rules/Rules.cpp`
- Modify: `tests/unit/RulesTest.cpp`

**Interfaces:**
- Consumes: `metrics.complexity.maxConditionRange` (Task 2), `callable.controlFlow` (`ControlFlowModel{kind, range, nestingDepth, conditionRange}`, Fase 5/6a).
- Produces: `allRules()` devuelve ahora las 8 reglas completas.

- [ ] **Step 1: Write the failing tests**

In `tests/unit/RulesTest.cpp`, replace the final test (`TEST_CASE("all six threshold rules apply to both C and C++")`) with:

```cpp
TEST_CASE("complexity.condition points at the exact condition that produced the maximum")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules();
    const codeqa::rules::Rule& rule = ruleNamed(rules, "complexity.condition");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;
    metrics.complexity.maxConditionComplexity = 3;
    codeqa::core::SourceRange conditionRange;
    conditionRange.begin.line = 42;
    conditionRange.begin.column = 9;
    metrics.complexity.maxConditionRange = conditionRange;

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(findings.size() == 1);
    REQUIRE(findings[0].observedValue.has_value());
    CHECK(*findings[0].observedValue == 3);
    CHECK(findings[0].location.line == 42);
    CHECK(findings[0].location.column == 9);
}

TEST_CASE("complexity.condition produces no finding when there is no condition to point at")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules();
    const codeqa::rules::Rule& rule = ruleNamed(rules, "complexity.condition");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;
    metrics.complexity.maxConditionComplexity = 10;

    CHECK(rule.evaluate(codeqa::rules::RuleContext{metrics, callable}).empty());
}

TEST_CASE("complexity.nesting points at the control-flow entry that reached the maximum depth")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules();
    const codeqa::rules::Rule& rule = ruleNamed(rules, "complexity.nesting");
    codeqa::model::CallableModel callable = makeCallable();

    codeqa::model::ControlFlowModel outer;
    outer.kind = codeqa::model::ControlFlowKind::If;
    outer.nestingDepth = 0;
    outer.range.begin.line = 20;
    outer.range.begin.column = 5;

    codeqa::model::ControlFlowModel inner;
    inner.kind = codeqa::model::ControlFlowKind::If;
    inner.nestingDepth = 3;
    inner.range.begin.line = 24;
    inner.range.begin.column = 9;

    callable.controlFlow = {outer, inner};

    codeqa::metrics::FunctionMetricsModel metrics;
    metrics.complexity.maxNestingDepth = 3;

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(findings.size() == 1);
    REQUIRE(findings[0].observedValue.has_value());
    CHECK(*findings[0].observedValue == 3);
    CHECK(findings[0].location.line == 24);
    CHECK(findings[0].location.column == 9);
}

TEST_CASE("all eight rules are registered and apply to both C and C++")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules();
    REQUIRE(rules.size() == 8);

    for (const auto& rule : rules)
    {
        CHECK(rule->appliesTo(codeqa::core::Language::C));
        CHECK(rule->appliesTo(codeqa::core::Language::Cpp));
    }
}
```

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: compiles (the rule ids don't exist in `allRules()` yet, so `ruleNamed` throws `std::runtime_error` at test time) and the new tests fail/error accordingly; the last test fails on `REQUIRE(rules.size() == 8)` (currently 6).

- [ ] **Step 3: Implement the two rules**

In `src/rules/Rules.cpp`, inside the anonymous namespace, add (after `ReturnCountRule`):

```cpp
class ConditionComplexityRule : public Rule
{
public:
    [[nodiscard]] std::string id() const override { return "complexity.condition"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        if (!context.metrics.complexity.maxConditionRange.has_value())
        {
            return {};
        }

        return singleOrEmpty(evaluateThreshold(
            id(),
            "A condition in this function is too complex.",
            "Consider extracting this condition into a well-named helper function or variable.",
            static_cast<long long>(context.metrics.complexity.maxConditionComplexity),
            3,
            5,
            context.metrics.complexity.maxConditionRange->begin,
            context.callable.symbol.qualifiedName));
    }
};

class NestingDepthRule : public Rule
{
public:
    [[nodiscard]] std::string id() const override { return "complexity.nesting"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        codeqa::core::SourceLocation location = context.callable.symbol.range.begin;

        for (const codeqa::model::ControlFlowModel& flow : context.callable.controlFlow)
        {
            if (flow.nestingDepth == context.metrics.complexity.maxNestingDepth)
            {
                location = flow.range.begin;
                break;
            }
        }

        return singleOrEmpty(evaluateThreshold(
            id(),
            "This function has excessive nesting.",
            "Consider using early returns or extracting nested blocks into helper functions.",
            static_cast<long long>(context.metrics.complexity.maxNestingDepth),
            3,
            5,
            location,
            context.callable.symbol.qualifiedName));
    }
};
```

Replace the body of `allRules()`:

```cpp
std::vector<std::unique_ptr<Rule>> allRules()
{
    std::vector<std::unique_ptr<Rule>> rules;
    rules.push_back(std::make_unique<FunctionLengthRule>());
    rules.push_back(std::make_unique<LocalVariableCountRule>());
    rules.push_back(std::make_unique<ParameterCountRule>());
    rules.push_back(std::make_unique<ConsecutiveBooleanParametersRule>());
    rules.push_back(std::make_unique<CyclomaticComplexityRule>());
    rules.push_back(std::make_unique<ReturnCountRule>());
    rules.push_back(std::make_unique<ConditionComplexityRule>());
    rules.push_back(std::make_unique<NestingDepthRule>());
    return rules;
}
```

- [ ] **Step 4: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/rules/Rules.cpp tests/unit/RulesTest.cpp
git commit -m "feat: implement the condition-complexity and nesting-depth rules"
```

---

### Task 5: `codeqa::application::ApplyRules`

**Files:**
- Create: `include/codeqa/application/ApplyRules.h`
- Create: `src/application/ApplyRules.cpp`
- Create: `tests/unit/ApplyRulesTest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `codeqa::model::ProjectModel` (Fase 5), `codeqa::metrics::ProjectMetricsModel` (Fase 6b), `codeqa::rules::allRules()` (Task 4b), `codeqa::infrastructure::readFileContents` (Task 1).
- Produces: `class ApplyRules { ProjectFindingsModel execute(const model::ProjectModel&, const metrics::ProjectMetricsModel&) const; }`.

- [ ] **Step 1: Write the failing tests**

Create `tests/unit/ApplyRulesTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/application/AnalyzeProject.h"
#include "codeqa/application/ApplyRules.h"
#include "codeqa/application/MeasureProject.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{

class TempProject
{
public:
    TempProject()
        : root_(std::filesystem::temp_directory_path() / "codeqa_apply_rules_test")
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

    void addFile(const std::filesystem::path& relativePath, const std::string& content) const
    {
        const std::filesystem::path path = root_ / relativePath;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path) << content;
    }

    [[nodiscard]] const std::filesystem::path& root() const { return root_; }

private:
    std::filesystem::path root_;
};

bool hasFinding(const std::vector<codeqa::rules::Finding>& findings, const std::string& ruleId)
{
    return std::any_of(
        findings.begin(), findings.end(),
        [&ruleId](const codeqa::rules::Finding& finding) { return finding.ruleId == ruleId; });
}

codeqa::rules::ProjectFindingsModel analyzeAndApplyRules(const std::filesystem::path& root)
{
    codeqa::application::AnalyzeProjectRequest request;
    request.discovery.rootPath = root;
    const codeqa::model::ProjectModel project = codeqa::application::AnalyzeProject{}.execute(request);
    const codeqa::metrics::ProjectMetricsModel metrics = codeqa::application::MeasureProject{}.execute(project);
    return codeqa::application::ApplyRules{}.execute(project, metrics);
}

} // namespace

TEST_CASE("ApplyRules generates findings for a function that crosses several thresholds")
{
    const TempProject fixture;
    fixture.addFile(
        "sample.c",
        "int classify(int a, int b, int c, int d, int e)\n"
        "{\n"
        "    if (a == 0) { return 0; }\n"
        "    if (a == 1) { return 1; }\n"
        "    if (a == 2) { return 2; }\n"
        "    if (a == 3) { return 3; }\n"
        "    return -1;\n"
        "}\n");

    const codeqa::rules::ProjectFindingsModel findings = analyzeAndApplyRules(fixture.root());

    REQUIRE(findings.files.size() == 1);
    const std::vector<codeqa::rules::Finding>& fileFindings = findings.files.front().findings;
    CHECK(hasFinding(fileFindings, "signature.parameter-count"));
    CHECK(hasFinding(fileFindings, "complexity.returns"));
}

TEST_CASE("ApplyRules produces no findings for a function whose range overlaps a parse error")
{
    const TempProject fixture;
    fixture.addFile(
        "broken.cpp",
        "int recovered() { int stable = 1; return stable; }\n"
        "int broken(int a, int b, int c, int d, int e) { int value = ; }\n");

    const codeqa::rules::ProjectFindingsModel findings = analyzeAndApplyRules(fixture.root());

    REQUIRE(findings.files.size() == 1);
    CHECK(findings.files.front().findings.empty());
}

TEST_CASE("ApplyRules suppresses a finding with a matching inline disable comment")
{
    const TempProject fixture;
    fixture.addFile(
        "suppressed.c",
        "// codeqa-disable-next-line signature.parameter-count\n"
        "int manyParams(int a, int b, int c, int d, int e)\n"
        "{\n"
        "    return a;\n"
        "}\n");

    const codeqa::rules::ProjectFindingsModel findings = analyzeAndApplyRules(fixture.root());

    REQUIRE(findings.files.size() == 1);
    CHECK_FALSE(hasFinding(findings.files.front().findings, "signature.parameter-count"));
}

TEST_CASE("ApplyRules does not suppress a finding when the comment names a different rule")
{
    const TempProject fixture;
    fixture.addFile(
        "not-suppressed.c",
        "// codeqa-disable-next-line complexity.cyclomatic\n"
        "int manyParams(int a, int b, int c, int d, int e)\n"
        "{\n"
        "    return a;\n"
        "}\n");

    const codeqa::rules::ProjectFindingsModel findings = analyzeAndApplyRules(fixture.root());

    REQUIRE(findings.files.size() == 1);
    CHECK(hasFinding(findings.files.front().findings, "signature.parameter-count"));
}
```

Add `unit/ApplyRulesTest.cpp` to `tests/CMakeLists.txt`, after `unit/RulesTest.cpp`.

**Known, accepted test-coverage gap:** `ApplyRules::execute` calls `codeqa::rules::allRules()` directly and is not
parameterized over the rule list, so nothing in this test file can prove that the orchestrator's
`if (!rule->appliesTo(file.language)) { continue; }` line actually skips a language-incompatible rule — all 8
registered rules return `true` for both languages, so that branch is never taken by any real rule. `Task 3`'s
`RuleTest.cpp` already proves the `Rule` interface contract itself dispatches `appliesTo()`/`evaluate()` correctly
in isolation; extending `ApplyRules`'s public API with an injectable rule list solely to reach one `if` branch that
no shipped rule exercises would be speculative generality for a single-line check. Leave this as a documented gap,
not a task to add.

**Note for Step 1's second test:** this reuses the exact `broken()` fixture shape already validated in `tests/unit/AnalyzeProjectTest.cpp` (a function whose body has `int value = ;` — a syntax error tree-sitter recovers from by producing an empty `localVariables`/`controlFlow`/`calls` for that callable), with 5 parameters added so the confidence filter has something concrete to suppress. If the RED run in Step 2 shows the parse-error diagnostic's location does **not** fall within `broken`'s `symbol.range` (i.e. the test doesn't behave as expected once real code exists), inspect the actual diagnostic location and callable range with a small ad-hoc print and adjust the fixture — do not adjust `ApplyRules`'s filter logic to fit an unexpected fixture shape without first confirming the discrepancy is in the fixture, not the implementation.

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails — `codeqa/application/ApplyRules.h` does not exist.

- [ ] **Step 3: Create the header**

Create `include/codeqa/application/ApplyRules.h`:

```cpp
#pragma once

#include "codeqa/metrics/MetricsModels.h"
#include "codeqa/model/Models.h"
#include "codeqa/rules/RuleModels.h"

namespace codeqa::application
{

class ApplyRules
{
public:
    [[nodiscard]] codeqa::rules::ProjectFindingsModel execute(
        const codeqa::model::ProjectModel& project,
        const codeqa::metrics::ProjectMetricsModel& metrics) const;
};

} // namespace codeqa::application
```

- [ ] **Step 4: Implement the orchestrator**

Create `src/application/ApplyRules.cpp`:

```cpp
#include "codeqa/application/ApplyRules.h"

#include "codeqa/core/Diagnostic.h"
#include "codeqa/core/SourceRange.h"
#include "codeqa/infrastructure/ReadFileContents.h"
#include "codeqa/rules/Rule.h"
#include "codeqa/rules/Rules.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace codeqa::application
{

namespace
{

std::vector<std::string> splitLines(const std::string& content)
{
    std::vector<std::string> lines;
    std::size_t start = 0;

    while (start <= content.size())
    {
        const std::size_t newline = content.find('\n', start);
        const std::size_t end = (newline == std::string::npos) ? content.size() : newline;
        std::size_t lineEnd = end;

        if (lineEnd > start && content[lineEnd - 1] == '\r')
        {
            --lineEnd;
        }

        lines.push_back(content.substr(start, lineEnd - start));

        if (newline == std::string::npos)
        {
            break;
        }

        start = newline + 1;
    }

    return lines;
}

bool positionInRange(std::size_t line, std::size_t column, const codeqa::core::SourceRange& range)
{
    if (line < range.begin.line || line > range.end.line)
    {
        return false;
    }

    if (line == range.begin.line && column < range.begin.column)
    {
        return false;
    }

    if (line == range.end.line && column >= range.end.column)
    {
        return false;
    }

    return true;
}

bool overlapsParseError(
    const codeqa::core::SourceRange& range, const std::vector<codeqa::core::Diagnostic>& diagnostics)
{
    for (const codeqa::core::Diagnostic& diagnostic : diagnostics)
    {
        if (diagnostic.code != "PARSE001" && diagnostic.code != "PARSE002")
        {
            continue;
        }

        if (positionInRange(diagnostic.location.line, diagnostic.location.column, range))
        {
            return true;
        }
    }

    return false;
}

const codeqa::metrics::FileMetricsModel* fileMetricsFor(
    const codeqa::metrics::ProjectMetricsModel& metrics, const std::filesystem::path& path)
{
    for (const codeqa::metrics::FileMetricsModel& file : metrics.files)
    {
        if (file.path == path)
        {
            return &file;
        }
    }

    return nullptr;
}

const codeqa::metrics::FunctionMetricsModel* functionMetricsFor(
    const codeqa::metrics::FileMetricsModel& file, const codeqa::model::SymbolIdentity& symbol)
{
    for (const codeqa::metrics::FunctionMetricsModel& function : file.functions)
    {
        if (function.symbol.range.begin.offset == symbol.range.begin.offset
            && function.symbol.range.end.offset == symbol.range.end.offset)
        {
            return &function;
        }
    }

    return nullptr;
}

std::string extractRangeText(const codeqa::core::SourceRange& range, const std::vector<std::string>& sourceLines)
{
    static const std::string emptyLine;
    std::string result;

    for (std::size_t line = range.begin.line; line <= range.end.line; ++line)
    {
        const std::string& text = (line - 1) < sourceLines.size() ? sourceLines[line - 1] : emptyLine;
        const std::size_t begin = (line == range.begin.line) ? range.begin.column - 1 : 0;
        const std::size_t end =
            (line == range.end.line) ? std::min(text.size(), range.end.column - 1) : text.size();

        if (begin < end)
        {
            result += text.substr(begin, end - begin);
        }

        if (line != range.end.line)
        {
            result += '\n';
        }
    }

    return result;
}

std::optional<std::string> suppressedRuleIdOnLine(
    std::size_t line,
    const std::vector<codeqa::core::SourceRange>& fileComments,
    const std::vector<std::string>& sourceLines)
{
    static const std::regex directive(R"(codeqa-disable-next-line\s+([A-Za-z0-9_.\-]+))");

    for (const codeqa::core::SourceRange& comment : fileComments)
    {
        if (comment.begin.line > line || comment.end.line < line)
        {
            continue;
        }

        const std::string text = extractRangeText(comment, sourceLines);
        std::smatch match;

        if (std::regex_search(text, match, directive))
        {
            return match[1].str();
        }
    }

    return std::nullopt;
}

bool isSuppressed(
    const codeqa::rules::Finding& finding,
    const std::vector<codeqa::core::SourceRange>& fileComments,
    const std::vector<std::string>& sourceLines)
{
    if (finding.location.line == 0)
    {
        return false;
    }

    const std::optional<std::string> suppressed =
        suppressedRuleIdOnLine(finding.location.line - 1, fileComments, sourceLines);
    return suppressed.has_value() && *suppressed == finding.ruleId;
}

} // namespace

codeqa::rules::ProjectFindingsModel ApplyRules::execute(
    const codeqa::model::ProjectModel& project, const codeqa::metrics::ProjectMetricsModel& metrics) const
{
    codeqa::rules::ProjectFindingsModel result;
    result.rootPath = project.rootPath;
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules();

    for (const codeqa::model::FileModel& file : project.files)
    {
        codeqa::rules::FileFindingsModel fileFindings;
        fileFindings.path = file.path;

        const codeqa::metrics::FileMetricsModel* fileMetrics = fileMetricsFor(metrics, file.path);

        if (fileMetrics == nullptr)
        {
            result.files.push_back(std::move(fileFindings));
            continue;
        }

        const std::optional<std::string> content = codeqa::infrastructure::readFileContents(file.path);
        const std::vector<std::string> sourceLines =
            content.has_value() ? splitLines(*content) : std::vector<std::string>{};

        for (const codeqa::model::CallableModel& callable : file.callables)
        {
            if (callable.kind == codeqa::model::CallableKind::Prototype)
            {
                continue;
            }

            if (overlapsParseError(callable.symbol.range, file.diagnostics))
            {
                continue;
            }

            const codeqa::metrics::FunctionMetricsModel* functionMetrics =
                functionMetricsFor(*fileMetrics, callable.symbol);

            if (functionMetrics == nullptr)
            {
                continue;
            }

            const codeqa::rules::RuleContext context{*functionMetrics, callable};

            for (const std::unique_ptr<codeqa::rules::Rule>& rule : rules)
            {
                if (!rule->appliesTo(file.language))
                {
                    continue;
                }

                for (codeqa::rules::Finding& finding : rule->evaluate(context))
                {
                    if (!isSuppressed(finding, file.comments, sourceLines))
                    {
                        fileFindings.findings.push_back(std::move(finding));
                    }
                }
            }
        }

        result.files.push_back(std::move(fileFindings));
    }

    return result;
}

} // namespace codeqa::application
```

- [ ] **Step 5: Register the new sources**

In `CMakeLists.txt`, add `src/application/ApplyRules.cpp` to `add_library(codeqa_core STATIC ...)`, right after `src/application/MeasureProject.cpp`.

- [ ] **Step 6: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass. If the second test (confidence filter) does not behave as predicted, follow the note under Step 1 before concluding the implementation is wrong.

- [ ] **Step 7: Commit**

```bash
git add include/codeqa/application/ApplyRules.h src/application/ApplyRules.cpp tests/unit/ApplyRulesTest.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add the ApplyRules orchestrator"
```

---

### Task 6: Corpus de hallazgos

Igual que en Fase 6b: las fixtures `*.findings.json` se generan a partir de la
salida real del binario ya implementado (Tasks 1-5), verificada primero a
mano contra dos funciones — no se transcriben números sin verificarlos.

**Valores verificados a mano (puerta de aceptación en el Paso 5):**

| Función | Fixture | ruleId | severity | observedValue | limitValue | línea esperada |
|---|---|---|---|---|---|---|
| `fourLevels` | `nesting/deep-nesting.cpp` | `complexity.nesting` | `warning` | 3 | 3 | 26 (el `if` interior, `values[index] % 2 == 0`) |
| `manyParameters` | `parameters/cpp-parameters.cpp` | `signature.parameter-count` | `warning` | 5 | 4 | 23 (la línea de la firma) |

Ninguna otra función de estos dos ficheros cruza ningún umbral con los
valores por defecto de esta fase (verificado a mano contra las métricas ya
conocidas de Fase 6b: `flat`/`oneLevel` en `deep-nesting.cpp`;
`withDefaults`/`byReference`/`byPointer`/`ignoresArgument` en
`cpp-parameters.cpp` — ninguna alcanza 4 parámetros, 3 de anidamiento, 3 de
condición, 10 de complejidad ciclomática, 4 de retornos, 8 de variables
locales, ni 2 booleanos consecutivos).

**Files:**
- Create: `tests/corpus/nesting/deep-nesting.findings.json`
- Create: `tests/corpus/parameters/cpp-parameters.findings.json`
- Create: `tests/unit/CorpusFindingsTest.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/corpus/CheckCorpusIntegrity.cmake`
- Modify: `tests/corpus/README.md`

**Interfaces:**
- Consumes: `codeqa::application::AnalyzeProject` + `MeasureProject` + `ApplyRules` (Task 5), fuentes de corpus existentes en `tests/corpus/{nesting,parameters}/`.

- [ ] **Step 1: Create placeholder expectation files**

Create both with the same placeholder content (overwritten in Step 6):

```json
{
  "findings": []
}
```

at `tests/corpus/nesting/deep-nesting.findings.json` and
`tests/corpus/parameters/cpp-parameters.findings.json`.

- [ ] **Step 2: Write the corpus findings runner (real test + temporary bootstrap dump)**

Create `tests/unit/CorpusFindingsTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/application/AnalyzeProject.h"
#include "codeqa/application/ApplyRules.h"
#include "codeqa/application/MeasureProject.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using Json = nlohmann::json;

struct FindingExpectation
{
    std::string symbolName;
    std::string ruleId;
    std::string severity;
    long long observedValue{0};
    long long limitValue{0};
    std::size_t line{0};
    std::size_t column{0};
};

std::string severityName(codeqa::core::DiagnosticSeverity severity)
{
    switch (severity)
    {
        case codeqa::core::DiagnosticSeverity::Information:
            return "information";
        case codeqa::core::DiagnosticSeverity::Warning:
            return "warning";
        case codeqa::core::DiagnosticSeverity::Error:
            return "error";
    }

    return "unknown";
}

std::vector<FindingExpectation> readExpectation(const std::filesystem::path& path)
{
    std::ifstream input(path);

    if (!input)
    {
        throw std::runtime_error("Could not open findings expectation '" + path.generic_string() + "'.");
    }

    const Json document = Json::parse(input);
    std::vector<FindingExpectation> expectations;

    for (const Json& finding : document.at("findings"))
    {
        FindingExpectation expected;
        expected.symbolName = finding.at("symbolName").get<std::string>();
        expected.ruleId = finding.at("ruleId").get<std::string>();
        expected.severity = finding.at("severity").get<std::string>();
        expected.observedValue = finding.at("observedValue").get<long long>();
        expected.limitValue = finding.at("limitValue").get<long long>();
        const Json& location = finding.at("location");
        expected.line = location.at("line").get<std::size_t>();
        expected.column = location.at("column").get<std::size_t>();
        expectations.push_back(std::move(expected));
    }

    return expectations;
}

std::string describe(const codeqa::rules::Finding& value)
{
    std::ostringstream output;
    output << "{\"symbolName\": \"" << value.symbolName << "\", \"ruleId\": \"" << value.ruleId
           << "\", \"severity\": \"" << severityName(value.severity)
           << "\", \"observedValue\": " << (value.observedValue.has_value() ? *value.observedValue : 0)
           << ", \"limitValue\": " << (value.limitValue.has_value() ? *value.limitValue : 0)
           << ", \"location\": {\"line\": " << value.location.line << ", \"column\": " << value.location.column
           << "}}";
    return output.str();
}

bool matches(const codeqa::rules::Finding& actual, const FindingExpectation& expected)
{
    return actual.symbolName == expected.symbolName && actual.ruleId == expected.ruleId
        && severityName(actual.severity) == expected.severity && actual.observedValue.has_value()
        && *actual.observedValue == expected.observedValue && actual.limitValue.has_value()
        && *actual.limitValue == expected.limitValue && actual.location.line == expected.line
        && actual.location.column == expected.column;
}

const codeqa::rules::FileFindingsModel& fileFindingsNamed(
    const codeqa::rules::ProjectFindingsModel& project, const std::filesystem::path& path)
{
    for (const codeqa::rules::FileFindingsModel& file : project.files)
    {
        if (file.path == path)
        {
            return file;
        }
    }

    throw std::runtime_error("Expected findings file was not produced: " + path.generic_string());
}

std::vector<std::filesystem::path> fixturesWithFindings(const std::filesystem::path& corpusRoot)
{
    return {corpusRoot / "nesting" / "deep-nesting.cpp", corpusRoot / "parameters" / "cpp-parameters.cpp"};
}

} // namespace

TEST_CASE("corpus findings match their expectations")
{
    const std::filesystem::path corpusRoot{CODEQA_CORPUS_DIR};
    codeqa::application::AnalyzeProjectRequest request;
    request.discovery.rootPath = corpusRoot;
    const codeqa::model::ProjectModel project = codeqa::application::AnalyzeProject{}.execute(request);
    const codeqa::metrics::ProjectMetricsModel metrics = codeqa::application::MeasureProject{}.execute(project);
    const codeqa::rules::ProjectFindingsModel findings =
        codeqa::application::ApplyRules{}.execute(project, metrics);

    for (const std::filesystem::path& sourcePath : fixturesWithFindings(corpusRoot))
    {
        INFO("fixture: " << sourcePath.generic_string());
        const std::filesystem::path expectationPath =
            sourcePath.parent_path() / (sourcePath.stem().string() + ".findings.json");
        const std::vector<FindingExpectation> expectation = readExpectation(expectationPath);
        const codeqa::rules::FileFindingsModel& file = fileFindingsNamed(findings, sourcePath);

        REQUIRE_MESSAGE(
            file.findings.size() == expectation.size(),
            "actual has " << file.findings.size() << " finding(s), expected " << expectation.size());

        for (std::size_t index = 0; index < expectation.size(); ++index)
        {
            CAPTURE(index);
            CHECK_MESSAGE(
                matches(file.findings[index], expectation[index]),
                "actual: " << describe(file.findings[index]) << "; expected symbol '"
                           << expectation[index].symbolName << "'");
        }
    }
}

// TEMPORARY -- bootstrap scaffolding for this task's Step 4. Prints every
// actual Finding for the two findings-bearing fixtures so their values can
// be hand-verified and transcribed into the *.findings.json files. Delete
// this TEST_CASE in Step 7, before committing.
TEST_CASE("bootstrap: dump corpus findings")
{
    const std::filesystem::path corpusRoot{CODEQA_CORPUS_DIR};
    codeqa::application::AnalyzeProjectRequest request;
    request.discovery.rootPath = corpusRoot;
    const codeqa::model::ProjectModel project = codeqa::application::AnalyzeProject{}.execute(request);
    const codeqa::metrics::ProjectMetricsModel metrics = codeqa::application::MeasureProject{}.execute(project);
    const codeqa::rules::ProjectFindingsModel findings =
        codeqa::application::ApplyRules{}.execute(project, metrics);

    for (const std::filesystem::path& sourcePath : fixturesWithFindings(corpusRoot))
    {
        const codeqa::rules::FileFindingsModel& file = fileFindingsNamed(findings, sourcePath);
        MESSAGE("fixture: " << sourcePath.generic_string());

        for (const codeqa::rules::Finding& finding : file.findings)
        {
            MESSAGE(describe(finding));
        }
    }
}
```

Add `unit/CorpusFindingsTest.cpp` to `tests/CMakeLists.txt`, after `unit/ApplyRulesTest.cpp`.

- [ ] **Step 3: Build**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: builds clean (the real test fails at runtime against the placeholder `{"findings": []}` fixtures — expected at this point).

- [ ] **Step 4: Dump the actual computed findings**

Run:

```text
build/tests/Debug/codeqa_tests.exe --test-case="bootstrap: dump corpus findings" --success
```

Capture the full output.

- [ ] **Step 5: Verify the two gate functions by hand before trusting the rest**

From the dumped output, find the finding(s) for `fourLevels` and
`manyParameters`. Compare `ruleId`/`severity`/`observedValue`/`limitValue`
against the table at the top of this task, and confirm `location.line`
matches the expected line. **Both must match exactly.** Also confirm the
dump shows **no other findings** for either fixture file — if it does,
that's new information (a threshold this plan didn't anticipate crossing),
not necessarily a bug; read the flagged function and confirm by hand
whether it genuinely crosses that rule's threshold before including it.

If `fourLevels` or `manyParameters` don't match: stop. Do not transcribe
anything. This means a defect survived Tasks 1-5's review — re-check
`ConditionComplexityRule`/`NestingDepthRule`/`ParameterCountRule` and the
confidence/suppression filters in `ApplyRules` against this task's table
and the design doc, fix it, and restart from Step 3.

- [ ] **Step 6: Transcribe the dumped output into the real fixtures**

For each of the two fixtures, write the corresponding
`tests/corpus/<dir>/<name>.findings.json` using this shape, with one entry
per finding **in the order the dump printed them**:

```json
{
  "findings": [
    {
      "symbolName": "fourLevels",
      "ruleId": "complexity.nesting",
      "severity": "warning",
      "observedValue": 3,
      "limitValue": 3,
      "location": {"line": 26, "column": 17}
    }
  ]
}
```

Fill every field with the values read verbatim from the Step 4 dump
(already hand-verified for `fourLevels` and `manyParameters` in Step 5).

- [ ] **Step 7: Delete the bootstrap scaffolding and confirm GREEN**

Delete the entire `TEST_CASE("bootstrap: dump corpus findings")` block (and
its preceding comment) from `tests/unit/CorpusFindingsTest.cpp`.

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe --test-case="corpus findings match their expectations"
```

Expected: PASS.

- [ ] **Step 8: Extend the corpus integrity check**

In `tests/corpus/CheckCorpusIntegrity.cmake`, add a new check after "Check
2c" (the `*.metrics.json` check Fase 6b added):

```cmake
# --- Check 2d: every *.findings.json has a sibling source -----------------
# Optional per fixture, same rule as *.metrics.json: only the
# findings-to-source direction is checked, never the reverse.

file(GLOB_RECURSE findingsFiles "${CODEQA_CORPUS_DIR}/*.findings.json")

foreach(findingsFile IN LISTS findingsFiles)
    get_filename_component(findingsDirectory "${findingsFile}" DIRECTORY)
    get_filename_component(findingsName "${findingsFile}" NAME)
    string(REPLACE ".findings.json" "" findingsStem "${findingsName}")

    if(NOT EXISTS "${findingsDirectory}/${findingsStem}.c"
        AND NOT EXISTS "${findingsDirectory}/${findingsStem}.cpp")
        list(APPEND failures
            "Findings expectation '${findingsFile}' has no matching .c or .cpp fixture.")
    endif()
endforeach()
```

- [ ] **Step 9: Document the schema in the corpus README**

Append to `tests/corpus/README.md`, after the "## Métricas (Fase 6b)"
section:

```markdown
## Hallazgos (Fase 7)

Los directorios `nesting/` y `parameters/` tienen además un archivo
hermano `<nombre>.findings.json`, opcional por fixture — solo se añade
donde alguna función realmente cruza un umbral por defecto. Cada uno
describe, en orden, los hallazgos esperados:

```json
{
  "findings": [
    {
      "symbolName": "fourLevels",
      "ruleId": "complexity.nesting",
      "severity": "warning",
      "observedValue": 3,
      "limitValue": 3,
      "location": {"line": 26, "column": 17}
    }
  ]
}
```

El runner (`tests/unit/CorpusFindingsTest.cpp`) compara con igualdad
exacta, incluida la ubicación. Los umbrales por defecto de las 8 reglas
están documentados en
`docs/superpowers/specs/2026-08-09-fase-7-motor-reglas-design.md` §6 —
ninguno está validado contra proyectos reales todavía (trabajo de la Fase
11).
```

- [ ] **Step 10: Full corpus + integrity sweep**

Run:

```text
ctest --test-dir build -C Debug --output-on-failure -R "codeqa_tests|codeqa_corpus_integrity"
```

Expected: all green.

- [ ] **Step 11: Commit**

```bash
git add tests/corpus/nesting/deep-nesting.findings.json tests/corpus/parameters/cpp-parameters.findings.json tests/unit/CorpusFindingsTest.cpp tests/CMakeLists.txt tests/corpus/CheckCorpusIntegrity.cmake tests/corpus/README.md
git commit -m "test: add corpus findings fixtures and the findings corpus runner"
```

---

### Task 7: Final verification

**Files:** none (verification only).

- [ ] **Step 1: Clean full build**

Run:

```text
cmake --build build --config Debug
```

Expected: no warnings or errors.

- [ ] **Step 2: Full test suite**

Run:

```text
ctest --test-dir build -C Debug --output-on-failure
```

Expected: every test passes, including the findings corpus runner and the
integrity check.

- [ ] **Step 3: Confirm `codeqa parse` and the CLI surface are unchanged**

Run:

```text
build/Debug/codeqa.exe parse tests/corpus
build/Debug/codeqa.exe --help
```

Expected: `parse` output identical to before this phase (still `13/4/9/2`);
`--help` shows no new subcommand — Fase 7 adds no CLI surface.

- [ ] **Step 4: Check for stray whitespace/formatting issues**

Run:

```text
git diff --check
```

Expected: no output.

- [ ] **Step 5: Confirm nothing was left uncommitted**

Run:

```text
git status --short
```

Expected: no output (or only pre-existing untracked files unrelated to this
phase).
