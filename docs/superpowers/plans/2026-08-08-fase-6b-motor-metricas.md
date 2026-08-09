# Fase 6b — Motor de métricas Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Calcular las métricas de tamaño, firma y complejidad por función definidas en `docs/superpowers/specs/2026-08-08-fase-6b-motor-metricas-design.md`, sobre el modelo común que la Fase 6a extendió, sin generar hallazgos ni tocar la CLI.

**Architecture:** Una capa nueva `codeqa::metrics` (pura, sin E/S ni dependencia de `codeqa::frontend`) expone `calculateFunctionMetrics(callable, fileComments, sourceLines)`. Un orquestador nuevo `codeqa::application::MeasureProject` es el único punto que relee disco: recorre un `model::ProjectModel` ya construido, relee cada archivo una vez, y llama a `calculateFunctionMetrics` por cada `CallableModel` no-prototipo. Ambas piezas son código nuevo — no se modifica ningún archivo existente de Fase 0-6a salvo `CMakeLists.txt` (nuevas fuentes), `tests/CMakeLists.txt` (nuevos tests) y `tests/corpus/{CheckCorpusIntegrity.cmake,README.md}` (soporte para `*.metrics.json`).

**Tech Stack:** C++17, CMake, doctest, nlohmann/json (solo tests del corpus).

## Global Constraints

- `codeqa::metrics` no incluye ningún header de `codeqa::frontend` ni hace E/S — solo `codeqa/model/Models.h`, `codeqa/core/*` y la STL. `codeqa::application::MeasureProject` es la única pieza nueva que toca disco.
- Ningún archivo de Fase 0-6a cambia de comportamiento ni de firma pública. Todo lo de esta fase es aditivo: dos headers nuevos, dos `.cpp` nuevos, cuatro tests nuevos, tres fixtures `*.metrics.json` nuevas.
- `core::SourceLocation.line`/`.column` son 1-based (`SourceLocation.h`). `sourceLines` (el `std::vector<std::string>` que separa el contenido del archivo en líneas) es 0-indexed: la línea `L` (1-based) es `sourceLines[L - 1]`. Cada línea se guarda **sin** su terminador; si el archivo usa `\r\n`, el `\r` final se recorta al partir en líneas, para que los índices de columna coincidan con los que tree-sitter registró (ver diseño §7).
- `core::SourceRange` no tiene `operator==` en ningún sitio del código — si una comparación de rangos hiciera falta, usar `.begin.offset`/`.end.offset` o `.begin.line`/`.begin.column`, nunca `==` directo sobre el struct. En la práctica, esta fase no necesita comparar `SourceRange`s: todas las fórmulas producen valores escalares (`std::size_t`).
- `branchCount` cuenta `ControlFlowKind::{If, For, While, DoWhile, Case, Conditional}`. `Switch`, `Default`, `Return`, `Break` y `Continue` **no** cuentan — `Switch` queda excluido a propósito (cada `case` ya aporta su propia entrada `Case`; contar también `Switch` duplicaría la misma decisión). Este es el único punto donde el diseño original tenía una laguna (no mencionaba `Switch`), corregida en `docs/superpowers/specs/2026-08-08-fase-6b-motor-metricas-design.md` antes de escribir este plan.
- `MeasureProject` excluye `CallableModel`s con `kind == codeqa::model::CallableKind::Prototype` (no `codeqa::frontend::FunctionKind` — `MeasureProject` opera sobre `model::ProjectModel`, la capa común, nunca sobre `frontend::ParsedFile`).
- Las tres fixtures `*.metrics.json` de Task 6 se generan con un procedimiento de "golden master verificado", no escribiendo números a mano en este plan: Task 6 incluye cuatro funciones (`classify`, `describe`, `fourLevels`, `withDefaults`) cuyos valores están derivados y verificados a mano dos veces en este documento (una de ellas cruzada contra aserciones ya existentes en `tests/unit/CorpusModelTest.cpp`). Esas cuatro son la puerta de verificación obligatoria antes de aceptar el resto de la salida transcrita del binario real.
- Nuevas fuentes en `CMakeLists.txt` (raíz): `src/application/MeasureProject.cpp` junto a los demás `src/application/*.cpp`; `src/metrics/CalculateFunctionMetrics.cpp` en un bloque nuevo junto a `src/model/ProjectModelBuilder.cpp`. Nuevos tests en `tests/CMakeLists.txt`: añadidos al final de la lista de `add_executable(codeqa_tests ...)`, sin reordenar los existentes.

## File Structure

- `include/codeqa/metrics/MetricsModels.h` (nuevo): `FunctionSizeMetrics`, `FunctionSignatureMetrics`, `FunctionComplexityMetrics`, `FunctionMetricsModel`, `FileMetricsModel`, `ProjectMetricsModel`.
- `include/codeqa/metrics/CalculateFunctionMetrics.h` (nuevo): declara `calculateFunctionMetrics`.
- `src/metrics/CalculateFunctionMetrics.cpp` (nuevo): cálculo puro de las tres familias de métricas.
- `include/codeqa/application/MeasureProject.h` (nuevo): declara la clase `MeasureProject`.
- `src/application/MeasureProject.cpp` (nuevo): relectura de archivo + orquestación.
- `tests/unit/MetricsModelsTest.cpp`, `CalculateFunctionMetricsTest.cpp`, `MeasureProjectTest.cpp`, `CorpusMetricsTest.cpp` (nuevos).
- `tests/corpus/{complexity/branching,nesting/deep-nesting,parameters/cpp-parameters}.metrics.json` (nuevos).
- `tests/corpus/CheckCorpusIntegrity.cmake`, `tests/corpus/README.md`: soporte para el nuevo esquema opcional.
- `CMakeLists.txt`, `tests/CMakeLists.txt`: registran las fuentes y tests nuevos.

---

### Task 1: `codeqa::metrics` — tipos del modelo de métricas

**Files:**
- Create: `include/codeqa/metrics/MetricsModels.h`
- Create: `tests/unit/MetricsModelsTest.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `codeqa::metrics::FunctionSizeMetrics`, `FunctionSignatureMetrics`, `FunctionComplexityMetrics`, `FunctionMetricsModel{symbol, size, signature, complexity}`, `FileMetricsModel{path, functions, diagnostics}`, `ProjectMetricsModel{rootPath, files}`. Todo campo numérico por defecto `0`.

- [ ] **Step 1: Write the failing test**

Create `tests/unit/MetricsModelsTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/metrics/MetricsModels.h"

TEST_CASE("metrics models default to zero-valued, empty state")
{
    codeqa::metrics::FunctionMetricsModel function;
    CHECK(function.symbol.name.empty());
    CHECK(function.size.physicalLines == 0);
    CHECK(function.size.codeLines == 0);
    CHECK(function.size.blankLines == 0);
    CHECK(function.size.commentLines == 0);
    CHECK(function.size.statementCount == 0);
    CHECK(function.signature.parameterCount == 0);
    CHECK(function.signature.signatureLength == 0);
    CHECK(function.signature.booleanParameterCount == 0);
    CHECK(function.complexity.cyclomaticComplexity == 0);
    CHECK(function.complexity.maxConditionComplexity == 0);
    CHECK(function.complexity.maxNestingDepth == 0);
    CHECK(function.complexity.branchCount == 0);
    CHECK(function.complexity.returnCount == 0);

    codeqa::metrics::FileMetricsModel file;
    CHECK(file.path.empty());
    CHECK(file.functions.empty());
    CHECK(file.diagnostics.empty());

    codeqa::metrics::ProjectMetricsModel project;
    CHECK(project.rootPath.empty());
    CHECK(project.files.empty());
}
```

Add `unit/MetricsModelsTest.cpp` to the `add_executable(codeqa_tests ...)` list in `tests/CMakeLists.txt`, right after `unit/CorpusModelTest.cpp`.

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails — `codeqa/metrics/MetricsModels.h` does not exist.

- [ ] **Step 3: Create the header**

Create `include/codeqa/metrics/MetricsModels.h`:

```cpp
#pragma once

#include "codeqa/core/Diagnostic.h"
#include "codeqa/model/Models.h"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace codeqa::metrics
{

struct FunctionSizeMetrics
{
    std::size_t physicalLines{0};
    std::size_t codeLines{0};
    std::size_t blankLines{0};
    std::size_t commentLines{0};
    std::size_t statementCount{0};
};

struct FunctionSignatureMetrics
{
    std::size_t parameterCount{0};
    std::size_t signatureLength{0};
    std::size_t booleanParameterCount{0};
};

struct FunctionComplexityMetrics
{
    std::size_t cyclomaticComplexity{0};
    std::size_t maxConditionComplexity{0};
    std::size_t maxNestingDepth{0};
    std::size_t branchCount{0};
    std::size_t returnCount{0};
};

struct FunctionMetricsModel
{
    codeqa::model::SymbolIdentity symbol;
    FunctionSizeMetrics size;
    FunctionSignatureMetrics signature;
    FunctionComplexityMetrics complexity;
};

struct FileMetricsModel
{
    std::filesystem::path path;
    std::vector<FunctionMetricsModel> functions;
    std::vector<codeqa::core::Diagnostic> diagnostics;
};

struct ProjectMetricsModel
{
    std::filesystem::path rootPath;
    std::vector<FileMetricsModel> files;
};

} // namespace codeqa::metrics
```

- [ ] **Step 4: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe --test-case="metrics models default to zero-valued, empty state"
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/metrics/MetricsModels.h tests/unit/MetricsModelsTest.cpp tests/CMakeLists.txt
git commit -m "feat: add the codeqa::metrics model types"
```

---

### Task 2: `calculateFunctionMetrics` — métricas de tamaño

**Files:**
- Create: `include/codeqa/metrics/CalculateFunctionMetrics.h`
- Create: `src/metrics/CalculateFunctionMetrics.cpp`
- Create: `tests/unit/CalculateFunctionMetricsTest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `codeqa::model::CallableModel` (`symbol.range`, `statements`), `codeqa::core::SourceRange` (`FileModel.comments`), from Fase 6a. `codeqa::metrics::FunctionMetricsModel` from Task 1.
- Produces: `codeqa::metrics::calculateFunctionMetrics(const model::CallableModel&, const std::vector<core::SourceRange>& fileComments, const std::vector<std::string>& sourceLines) -> FunctionMetricsModel`. En este task, `.size` queda completo; `.signature`/`.complexity` quedan por defecto (Tasks 3/4 los completan).

- [ ] **Step 1: Write the failing tests**

Create `tests/unit/CalculateFunctionMetricsTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/metrics/CalculateFunctionMetrics.h"

#include <string>
#include <vector>

namespace
{

codeqa::model::CallableModel makeCallable(std::size_t beginLine, std::size_t endLine, std::size_t statementCount)
{
    codeqa::model::CallableModel callable;
    callable.symbol.name = "sample";
    callable.symbol.range.begin.line = beginLine;
    callable.symbol.range.end.line = endLine;
    callable.statements.resize(statementCount);
    return callable;
}

codeqa::core::SourceRange lineSpan(
    std::size_t beginLine, std::size_t beginColumn, std::size_t endLine, std::size_t endColumn)
{
    codeqa::core::SourceRange range;
    range.begin.line = beginLine;
    range.begin.column = beginColumn;
    range.end.line = endLine;
    range.end.column = endColumn;
    return range;
}

} // namespace

TEST_CASE("size metrics classify blank, comment and code lines")
{
    const std::vector<std::string> sourceLines{
        "int add(int a, int b)",
        "{",
        "    // sum the values",
        "",
        "    return a + b;",
        "}"};
    const std::vector<codeqa::core::SourceRange> fileComments{lineSpan(3, 5, 3, 22)};
    const codeqa::model::CallableModel callable = makeCallable(1, 6, 1);

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, fileComments, sourceLines);

    CHECK(metrics.size.physicalLines == 6);
    CHECK(metrics.size.blankLines == 1);
    CHECK(metrics.size.commentLines == 1);
    CHECK(metrics.size.codeLines == 4);
    CHECK(metrics.size.statementCount == 1);
}

TEST_CASE("a blank line inside a block comment counts as blank, not comment")
{
    const std::vector<std::string> sourceLines{
        "int f()",
        "{",
        "    /* line one",
        "",
        "       line two */",
        "    return 0;",
        "}"};
    const std::vector<codeqa::core::SourceRange> fileComments{lineSpan(3, 5, 5, 19)};
    const codeqa::model::CallableModel callable = makeCallable(1, 7, 1);

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, fileComments, sourceLines);

    CHECK(metrics.size.physicalLines == 7);
    CHECK(metrics.size.blankLines == 1);
    CHECK(metrics.size.commentLines == 2);
    CHECK(metrics.size.codeLines == 4);
}

TEST_CASE("a line with code followed by a trailing comment is a code line, not a comment line")
{
    const std::vector<std::string> sourceLines{
        "int g()",
        "{",
        "    int x = 1; // note",
        "    return x;",
        "}"};
    const std::vector<codeqa::core::SourceRange> fileComments{lineSpan(3, 16, 3, 23)};
    const codeqa::model::CallableModel callable = makeCallable(1, 5, 2);

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, fileComments, sourceLines);

    CHECK(metrics.size.physicalLines == 5);
    CHECK(metrics.size.blankLines == 0);
    CHECK(metrics.size.commentLines == 0);
    CHECK(metrics.size.codeLines == 5);
    CHECK(metrics.size.statementCount == 2);
}
```

Add `unit/CalculateFunctionMetricsTest.cpp` to `tests/CMakeLists.txt`, after `unit/MetricsModelsTest.cpp`.

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails — `codeqa/metrics/CalculateFunctionMetrics.h` does not exist.

- [ ] **Step 3: Create the header**

Create `include/codeqa/metrics/CalculateFunctionMetrics.h`:

```cpp
#pragma once

#include "codeqa/core/SourceRange.h"
#include "codeqa/metrics/MetricsModels.h"
#include "codeqa/model/Models.h"

#include <string>
#include <vector>

namespace codeqa::metrics
{

[[nodiscard]] FunctionMetricsModel calculateFunctionMetrics(
    const codeqa::model::CallableModel& callable,
    const std::vector<codeqa::core::SourceRange>& fileComments,
    const std::vector<std::string>& sourceLines);

} // namespace codeqa::metrics
```

- [ ] **Step 4: Implement size metrics**

Create `src/metrics/CalculateFunctionMetrics.cpp`:

```cpp
#include "codeqa/metrics/CalculateFunctionMetrics.h"

#include <algorithm>
#include <cstddef>

namespace codeqa::metrics
{

namespace
{

enum class LineClassification
{
    Blank,
    Comment,
    Code
};

bool isHorizontalWhitespace(char character)
{
    return character == ' ' || character == '\t' || character == '\f' || character == '\v';
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

LineClassification classifyLine(
    std::size_t line,
    const std::string& text,
    const std::vector<codeqa::core::SourceRange>& fileComments)
{
    bool sawNonBlank = false;
    bool allCommentCovered = true;

    for (std::size_t index = 0; index < text.size(); ++index)
    {
        if (isHorizontalWhitespace(text[index]))
        {
            continue;
        }

        sawNonBlank = true;
        const std::size_t column = index + 1;
        const bool covered = std::any_of(
            fileComments.begin(), fileComments.end(), [line, column](const codeqa::core::SourceRange& comment) {
                return positionInRange(line, column, comment);
            });

        if (!covered)
        {
            allCommentCovered = false;
            break;
        }
    }

    if (!sawNonBlank)
    {
        return LineClassification::Blank;
    }

    return allCommentCovered ? LineClassification::Comment : LineClassification::Code;
}

FunctionSizeMetrics calculateSizeMetrics(
    const codeqa::model::CallableModel& callable,
    const std::vector<codeqa::core::SourceRange>& fileComments,
    const std::vector<std::string>& sourceLines)
{
    FunctionSizeMetrics metrics;
    const std::size_t beginLine = callable.symbol.range.begin.line;
    const std::size_t endLine = callable.symbol.range.end.line;
    metrics.physicalLines = endLine - beginLine + 1;

    static const std::string emptyLine;

    for (std::size_t line = beginLine; line <= endLine; ++line)
    {
        const std::string& text = (line - 1) < sourceLines.size() ? sourceLines[line - 1] : emptyLine;

        switch (classifyLine(line, text, fileComments))
        {
            case LineClassification::Blank:
                ++metrics.blankLines;
                break;
            case LineClassification::Comment:
                ++metrics.commentLines;
                break;
            case LineClassification::Code:
                break;
        }
    }

    metrics.codeLines = metrics.physicalLines - metrics.blankLines - metrics.commentLines;
    metrics.statementCount = callable.statements.size();
    return metrics;
}

} // namespace

FunctionMetricsModel calculateFunctionMetrics(
    const codeqa::model::CallableModel& callable,
    const std::vector<codeqa::core::SourceRange>& fileComments,
    const std::vector<std::string>& sourceLines)
{
    FunctionMetricsModel metrics;
    metrics.symbol = callable.symbol;
    metrics.size = calculateSizeMetrics(callable, fileComments, sourceLines);
    return metrics;
}

} // namespace codeqa::metrics
```

- [ ] **Step 5: Register the new sources**

In `CMakeLists.txt`, add `src/metrics/CalculateFunctionMetrics.cpp` to `add_library(codeqa_core STATIC ...)`, right after `src/model/ProjectModelBuilder.cpp`.

- [ ] **Step 6: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe --test-case="size metrics classify blank, comment and code lines,a blank line inside a block comment counts as blank, not comment,a line with code followed by a trailing comment is a code line, not a comment line"
```

Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add include/codeqa/metrics/CalculateFunctionMetrics.h src/metrics/CalculateFunctionMetrics.cpp tests/unit/CalculateFunctionMetricsTest.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: calculate function size metrics"
```

---

### Task 3: `calculateFunctionMetrics` — métricas de firma

**Files:**
- Modify: `src/metrics/CalculateFunctionMetrics.cpp`
- Modify: `tests/unit/CalculateFunctionMetricsTest.cpp`

**Interfaces:**
- Consumes: `callable.parameters` (`ParameterModel{name, passing, hasDefaultValue, declaredType}`), `callable.parameterListRange` (`std::optional<core::SourceRange>`), both from Fase 6a.
- Produces: `.signature` queda completo en `calculateFunctionMetrics`. `.complexity` sigue por defecto hasta Task 4.

- [ ] **Step 1: Write the failing tests**

Append to `tests/unit/CalculateFunctionMetricsTest.cpp`:

```cpp
TEST_CASE("signature length counts the parameter list text, excluding parentheses")
{
    const std::vector<std::string> sourceLines{"int classify(int value, bool strict)"};
    codeqa::model::CallableModel callable = makeCallable(1, 1, 0);
    callable.parameters = {
        codeqa::model::ParameterModel{"value", codeqa::model::ParameterPassing::Value, false, "int"},
        codeqa::model::ParameterModel{"strict", codeqa::model::ParameterPassing::Value, false, "bool"}};
    callable.parameterListRange = lineSpan(1, 13, 1, 37);

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, {}, sourceLines);

    CHECK(metrics.signature.parameterCount == 2);
    CHECK(metrics.signature.signatureLength == 22);
    CHECK(metrics.signature.booleanParameterCount == 1);
}

TEST_CASE("signature length collapses whitespace so a multi-line signature measures the same as one line")
{
    const std::vector<std::string> multiLine{"int build(", "    int width,", "    int height)"};
    codeqa::model::CallableModel multiLineCallable = makeCallable(1, 3, 0);
    multiLineCallable.parameters = {
        codeqa::model::ParameterModel{"width", codeqa::model::ParameterPassing::Value, false, "int"},
        codeqa::model::ParameterModel{"height", codeqa::model::ParameterPassing::Value, false, "int"}};
    multiLineCallable.parameterListRange = lineSpan(1, 10, 3, 16);

    const codeqa::metrics::FunctionMetricsModel multiLineMetrics =
        codeqa::metrics::calculateFunctionMetrics(multiLineCallable, {}, multiLine);

    const std::vector<std::string> singleLine{"int build(int width, int height)"};
    codeqa::model::CallableModel singleLineCallable = makeCallable(1, 1, 0);
    singleLineCallable.parameters = multiLineCallable.parameters;
    singleLineCallable.parameterListRange = lineSpan(1, 10, 1, 33);

    const codeqa::metrics::FunctionMetricsModel singleLineMetrics =
        codeqa::metrics::calculateFunctionMetrics(singleLineCallable, {}, singleLine);

    CHECK(multiLineMetrics.signature.signatureLength == singleLineMetrics.signature.signatureLength);
    CHECK(multiLineMetrics.signature.signatureLength == 21);
}

TEST_CASE("an empty parameter list has signature length zero")
{
    const std::vector<std::string> sourceLines{"void noop()"};
    codeqa::model::CallableModel callable = makeCallable(1, 1, 0);
    callable.parameterListRange = lineSpan(1, 10, 1, 12);

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, {}, sourceLines);

    CHECK(metrics.signature.parameterCount == 0);
    CHECK(metrics.signature.signatureLength == 0);
}

TEST_CASE("a callable with no parameter list has signature length zero")
{
    const std::vector<std::string> sourceLines{"auto lambda = []{ return 1; };"};
    codeqa::model::CallableModel callable = makeCallable(1, 1, 0);

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, {}, sourceLines);

    CHECK(metrics.signature.parameterCount == 0);
    CHECK(metrics.signature.signatureLength == 0);
}

TEST_CASE("boolean parameter count requires both a bool-family type and pass-by-value")
{
    const std::vector<std::string> sourceLines{"void f(bool a, bool b, bool* c, bool& d, int e)"};
    codeqa::model::CallableModel callable = makeCallable(1, 1, 0);
    callable.parameters = {
        codeqa::model::ParameterModel{"a", codeqa::model::ParameterPassing::Value, false, "bool"},
        codeqa::model::ParameterModel{"b", codeqa::model::ParameterPassing::Value, false, "const bool"},
        codeqa::model::ParameterModel{"c", codeqa::model::ParameterPassing::Pointer, false, "bool"},
        codeqa::model::ParameterModel{"d", codeqa::model::ParameterPassing::Reference, false, "bool"},
        codeqa::model::ParameterModel{"e", codeqa::model::ParameterPassing::Value, false, "int"}};
    callable.parameterListRange = lineSpan(1, 7, 1, 48);

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, {}, sourceLines);

    CHECK(metrics.signature.parameterCount == 5);
    CHECK(metrics.signature.booleanParameterCount == 2);
}
```

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: compiles (the tests only read `.signature`, which exists and is zero-valued) but the new `CHECK`s fail because `.signature` is still all-zero.

- [ ] **Step 3: Implement signature metrics**

In `src/metrics/CalculateFunctionMetrics.cpp`, inside the anonymous namespace, add (after `calculateSizeMetrics`):

```cpp
std::string trim(const std::string& text)
{
    const std::size_t begin = text.find_first_not_of(" \t\r\n\f\v");

    if (begin == std::string::npos)
    {
        return std::string{};
    }

    const std::size_t end = text.find_last_not_of(" \t\r\n\f\v");
    return text.substr(begin, end - begin + 1);
}

std::size_t collapsedLength(const std::string& text)
{
    std::size_t length = 0;
    bool inWhitespaceRun = false;

    for (const char character : text)
    {
        const bool isWhitespace = character == ' ' || character == '\t' || character == '\n'
            || character == '\r' || character == '\f' || character == '\v';

        if (isWhitespace)
        {
            if (!inWhitespaceRun)
            {
                ++length;
                inWhitespaceRun = true;
            }

            continue;
        }

        inWhitespaceRun = false;
        ++length;
    }

    return length;
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

FunctionSignatureMetrics calculateSignatureMetrics(
    const codeqa::model::CallableModel& callable, const std::vector<std::string>& sourceLines)
{
    FunctionSignatureMetrics metrics;
    metrics.parameterCount = callable.parameters.size();

    if (callable.parameterListRange.has_value())
    {
        const std::string text = extractRangeText(*callable.parameterListRange, sourceLines);
        const std::string inner = text.size() >= 2 ? text.substr(1, text.size() - 2) : std::string{};
        metrics.signatureLength = collapsedLength(trim(inner));
    }

    for (const codeqa::model::ParameterModel& parameter : callable.parameters)
    {
        if (isBooleanValueParameter(parameter))
        {
            ++metrics.booleanParameterCount;
        }
    }

    return metrics;
}
```

Add `#include <sstream>` and `#include <string>` to the top of the file next to the other standard headers.

Replace the body of the public `calculateFunctionMetrics` to also fill `.signature`:

```cpp
FunctionMetricsModel calculateFunctionMetrics(
    const codeqa::model::CallableModel& callable,
    const std::vector<codeqa::core::SourceRange>& fileComments,
    const std::vector<std::string>& sourceLines)
{
    FunctionMetricsModel metrics;
    metrics.symbol = callable.symbol;
    metrics.size = calculateSizeMetrics(callable, fileComments, sourceLines);
    metrics.signature = calculateSignatureMetrics(callable, sourceLines);
    return metrics;
}
```

- [ ] **Step 4: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe --test-case="signature length counts the parameter list text*,signature length collapses whitespace*,an empty parameter list*,a callable with no parameter list*,boolean parameter count*"
```

Expected: PASS, and the Task 2 size tests still pass.

- [ ] **Step 5: Commit**

```bash
git add src/metrics/CalculateFunctionMetrics.cpp tests/unit/CalculateFunctionMetricsTest.cpp
git commit -m "feat: calculate function signature metrics"
```

---

### Task 4: `calculateFunctionMetrics` — métricas de complejidad

**Files:**
- Modify: `src/metrics/CalculateFunctionMetrics.cpp`
- Modify: `tests/unit/CalculateFunctionMetricsTest.cpp`

**Interfaces:**
- Consumes: `callable.controlFlow` (`ControlFlowModel{kind, range, nestingDepth, conditionRange}`), `codeqa::model::ControlFlowKind`, both from Fase 5/6a. Reuses `positionInRange` from Task 2.
- Produces: `.complexity` queda completo. `calculateFunctionMetrics` queda con su forma final.

- [ ] **Step 1: Write the failing tests**

Append to `tests/unit/CalculateFunctionMetricsTest.cpp`:

```cpp
namespace
{

codeqa::model::ControlFlowModel makeFlow(
    codeqa::model::ControlFlowKind kind, std::size_t nestingDepth)
{
    codeqa::model::ControlFlowModel flow;
    flow.kind = kind;
    flow.nestingDepth = nestingDepth;
    return flow;
}

} // namespace

TEST_CASE("complexity metrics for an if/else-if chain with && and ||, matching classify() in the corpus")
{
    const std::vector<std::string> sourceLines{
        "int classify(int value, bool strict)",
        "{",
        "    if (value < 0 && strict)",
        "    {",
        "        return -1;",
        "    }",
        "    else if (value == 0 || !strict)",
        "    {",
        "        return 0;",
        "    }",
        "",
        "    return 1;",
        "}"};
    codeqa::model::CallableModel callable = makeCallable(1, 13, 5);

    codeqa::model::ControlFlowModel outerIf = makeFlow(codeqa::model::ControlFlowKind::If, 0);
    outerIf.conditionRange = lineSpan(3, 9, 3, 28);
    codeqa::model::ControlFlowModel innerIf = makeFlow(codeqa::model::ControlFlowKind::If, 1);
    innerIf.conditionRange = lineSpan(7, 14, 7, 35);
    callable.controlFlow = {
        outerIf, makeFlow(codeqa::model::ControlFlowKind::Return, 1), innerIf,
        makeFlow(codeqa::model::ControlFlowKind::Return, 2), makeFlow(codeqa::model::ControlFlowKind::Return, 0)};

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, {}, sourceLines);

    CHECK(metrics.complexity.branchCount == 2);
    CHECK(metrics.complexity.cyclomaticComplexity == 5);
    CHECK(metrics.complexity.maxConditionComplexity == 1);
    CHECK(metrics.complexity.maxNestingDepth == 2);
    CHECK(metrics.complexity.returnCount == 3);
}

TEST_CASE("switch/case/default: only Case counts as a branch, matching describe() in the corpus")
{
    codeqa::model::CallableModel callable = makeCallable(1, 12, 7);
    callable.controlFlow = {
        makeFlow(codeqa::model::ControlFlowKind::Switch, 0),
        makeFlow(codeqa::model::ControlFlowKind::Case, 1),
        makeFlow(codeqa::model::ControlFlowKind::Return, 2),
        makeFlow(codeqa::model::ControlFlowKind::Case, 1),
        makeFlow(codeqa::model::ControlFlowKind::Return, 2),
        makeFlow(codeqa::model::ControlFlowKind::Default, 1),
        makeFlow(codeqa::model::ControlFlowKind::Return, 2)};

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, {}, {});

    CHECK(metrics.complexity.branchCount == 2);
    CHECK(metrics.complexity.cyclomaticComplexity == 3);
    CHECK(metrics.complexity.maxConditionComplexity == 0);
    CHECK(metrics.complexity.maxNestingDepth == 2);
    CHECK(metrics.complexity.returnCount == 3);
}

TEST_CASE("a function with no control flow has cyclomatic complexity 1 and depth 0")
{
    codeqa::model::CallableModel callable = makeCallable(1, 3, 1);

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, {}, {});

    CHECK(metrics.complexity.branchCount == 0);
    CHECK(metrics.complexity.cyclomaticComplexity == 1);
    CHECK(metrics.complexity.maxConditionComplexity == 0);
    CHECK(metrics.complexity.maxNestingDepth == 0);
    CHECK(metrics.complexity.returnCount == 0);
}

TEST_CASE("a boolean operator inside a comment inside a condition is not counted")
{
    const std::vector<std::string> sourceLines{"if (a /* && b */ && c) { }"};
    codeqa::model::CallableModel callable = makeCallable(1, 1, 0);
    codeqa::model::ControlFlowModel flow = makeFlow(codeqa::model::ControlFlowKind::If, 0);
    flow.conditionRange = lineSpan(1, 5, 1, 22);
    callable.controlFlow = {flow};
    const std::vector<codeqa::core::SourceRange> fileComments{lineSpan(1, 7, 1, 17)};

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, fileComments, sourceLines);

    CHECK(metrics.complexity.maxConditionComplexity == 1);
    CHECK(metrics.complexity.cyclomaticComplexity == 3);
}

TEST_CASE("a boolean operator inside a string literal inside a condition is not counted")
{
    const std::vector<std::string> sourceLines{"if (strcmp(a, \"x && y\") && ok) { }"};
    codeqa::model::CallableModel callable = makeCallable(1, 1, 0);
    codeqa::model::ControlFlowModel flow = makeFlow(codeqa::model::ControlFlowKind::If, 0);
    flow.conditionRange = lineSpan(1, 5, 1, 30);
    callable.controlFlow = {flow};

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, {}, sourceLines);

    CHECK(metrics.complexity.maxConditionComplexity == 1);
}
```

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: compiles, but the new `CHECK`s fail (`.complexity` still all-zero).

- [ ] **Step 3: Implement complexity metrics**

In `src/metrics/CalculateFunctionMetrics.cpp`, inside the anonymous namespace, add (after `calculateSignatureMetrics`):

```cpp
bool countsAsBranch(codeqa::model::ControlFlowKind kind)
{
    switch (kind)
    {
        case codeqa::model::ControlFlowKind::If:
        case codeqa::model::ControlFlowKind::For:
        case codeqa::model::ControlFlowKind::While:
        case codeqa::model::ControlFlowKind::DoWhile:
        case codeqa::model::ControlFlowKind::Case:
        case codeqa::model::ControlFlowKind::Conditional:
            return true;
        default:
            return false;
    }
}

std::size_t countBooleanOperators(
    const codeqa::core::SourceRange& range,
    const std::vector<std::string>& sourceLines,
    const std::vector<codeqa::core::SourceRange>& fileComments)
{
    static const std::string emptyLine;
    std::size_t count = 0;
    bool inString = false;
    bool inChar = false;

    for (std::size_t line = range.begin.line; line <= range.end.line; ++line)
    {
        const std::string& text = (line - 1) < sourceLines.size() ? sourceLines[line - 1] : emptyLine;
        const std::size_t startIndex = (line == range.begin.line) ? range.begin.column - 1 : 0;
        const std::size_t endIndex =
            (line == range.end.line) ? std::min(text.size(), range.end.column - 1) : text.size();

        std::size_t index = startIndex;

        while (index < endIndex)
        {
            const char character = text[index];

            if (inString || inChar)
            {
                if (character == '\\')
                {
                    index += 2;
                    continue;
                }

                if ((inString && character == '"') || (inChar && character == '\''))
                {
                    inString = false;
                    inChar = false;
                }

                ++index;
                continue;
            }

            if (character == '"')
            {
                inString = true;
                ++index;
                continue;
            }

            if (character == '\'')
            {
                inChar = true;
                ++index;
                continue;
            }

            if (index + 1 < text.size()
                && ((character == '&' && text[index + 1] == '&')
                    || (character == '|' && text[index + 1] == '|')))
            {
                const std::size_t column = index + 1;
                const bool covered = std::any_of(
                    fileComments.begin(), fileComments.end(),
                    [line, column](const codeqa::core::SourceRange& comment) {
                        return positionInRange(line, column, comment);
                    });

                if (!covered)
                {
                    ++count;
                }

                index += 2;
                continue;
            }

            ++index;
        }
    }

    return count;
}

FunctionComplexityMetrics calculateComplexityMetrics(
    const codeqa::model::CallableModel& callable,
    const std::vector<std::string>& sourceLines,
    const std::vector<codeqa::core::SourceRange>& fileComments)
{
    FunctionComplexityMetrics metrics;
    std::size_t booleanOperatorCount = 0;

    for (const codeqa::model::ControlFlowModel& flow : callable.controlFlow)
    {
        if (countsAsBranch(flow.kind))
        {
            ++metrics.branchCount;
        }

        if (flow.kind == codeqa::model::ControlFlowKind::Return)
        {
            ++metrics.returnCount;
        }

        metrics.maxNestingDepth = std::max(metrics.maxNestingDepth, flow.nestingDepth);

        if (flow.conditionRange.has_value())
        {
            const std::size_t operators = countBooleanOperators(*flow.conditionRange, sourceLines, fileComments);
            metrics.maxConditionComplexity = std::max(metrics.maxConditionComplexity, operators);
            booleanOperatorCount += operators;
        }
    }

    metrics.cyclomaticComplexity = 1 + metrics.branchCount + booleanOperatorCount;
    return metrics;
}
```

Replace the body of the public `calculateFunctionMetrics` one last time:

```cpp
FunctionMetricsModel calculateFunctionMetrics(
    const codeqa::model::CallableModel& callable,
    const std::vector<codeqa::core::SourceRange>& fileComments,
    const std::vector<std::string>& sourceLines)
{
    FunctionMetricsModel metrics;
    metrics.symbol = callable.symbol;
    metrics.size = calculateSizeMetrics(callable, fileComments, sourceLines);
    metrics.signature = calculateSignatureMetrics(callable, sourceLines);
    metrics.complexity = calculateComplexityMetrics(callable, sourceLines, fileComments);
    return metrics;
}
```

- [ ] **Step 4: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe --test-case="complexity metrics for an if/else-if chain*,switch/case/default*,a function with no control flow*,a boolean operator inside a comment*,a boolean operator inside a string literal*"
```

Expected: PASS, and every earlier test in this file still passes.

- [ ] **Step 5: Commit**

```bash
git add src/metrics/CalculateFunctionMetrics.cpp tests/unit/CalculateFunctionMetricsTest.cpp
git commit -m "feat: calculate function complexity metrics"
```

---

### Task 5: `codeqa::application::MeasureProject`

**Files:**
- Create: `include/codeqa/application/MeasureProject.h`
- Create: `src/application/MeasureProject.cpp`
- Create: `tests/unit/MeasureProjectTest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `codeqa::model::ProjectModel`/`FileModel`/`CallableModel`/`CallableKind::Prototype` (Fase 5/6a), `codeqa::metrics::calculateFunctionMetrics` (Task 4), `codeqa::core::Diagnostic`/`DiagnosticSeverity`.
- Produces: `class codeqa::application::MeasureProject { ProjectMetricsModel execute(const model::ProjectModel&) const; }`.

- [ ] **Step 1: Write the failing tests**

Create `tests/unit/MeasureProjectTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/application/AnalyzeProject.h"
#include "codeqa/application/MeasureProject.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace
{

class TempProject
{
public:
    TempProject()
        : root_(std::filesystem::temp_directory_path() / "codeqa_measure_project_test")
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

const codeqa::metrics::FileMetricsModel& fileMetricsNamed(
    const codeqa::metrics::ProjectMetricsModel& project, const std::filesystem::path& path)
{
    for (const codeqa::metrics::FileMetricsModel& file : project.files)
    {
        if (file.path == path)
        {
            return file;
        }
    }

    throw std::runtime_error("Expected metrics file was not produced.");
}

} // namespace

TEST_CASE("MeasureProject computes metrics for every non-prototype function")
{
    const TempProject fixture;
    fixture.addFile(
        "sample.c",
        "int add(int a, int b);\n"
        "\n"
        "int add(int a, int b)\n"
        "{\n"
        "    return a + b;\n"
        "}\n");

    codeqa::application::AnalyzeProjectRequest request;
    request.discovery.rootPath = fixture.root();
    const codeqa::model::ProjectModel project = codeqa::application::AnalyzeProject{}.execute(request);
    const codeqa::metrics::ProjectMetricsModel metrics = codeqa::application::MeasureProject{}.execute(project);

    CHECK(metrics.rootPath == fixture.root());
    REQUIRE(metrics.files.size() == 1);
    const codeqa::metrics::FileMetricsModel& file = metrics.files.front();
    CHECK(file.path == fixture.root() / "sample.c");
    CHECK(file.diagnostics.empty());
    REQUIRE(file.functions.size() == 1);
    CHECK(file.functions[0].symbol.name == "add");
    CHECK(file.functions[0].size.statementCount == 1);
}

TEST_CASE("MeasureProject computes metrics for every function of a multi-function file")
{
    const TempProject fixture;
    fixture.addFile(
        "multi.c",
        "int first(void) { return 1; }\n"
        "int second(void) { return 2; }\n");

    codeqa::application::AnalyzeProjectRequest request;
    request.discovery.rootPath = fixture.root();
    const codeqa::model::ProjectModel project = codeqa::application::AnalyzeProject{}.execute(request);
    const codeqa::metrics::ProjectMetricsModel metrics = codeqa::application::MeasureProject{}.execute(project);

    const codeqa::metrics::FileMetricsModel& file = fileMetricsNamed(metrics, fixture.root() / "multi.c");
    REQUIRE(file.functions.size() == 2);
    CHECK(file.functions[0].symbol.name == "first");
    CHECK(file.functions[1].symbol.name == "second");
    CHECK(file.functions[0].size.physicalLines == 1);
    CHECK(file.functions[1].size.physicalLines == 1);
}

TEST_CASE("MeasureProject reports a diagnostic, not an abort, when a file disappears before measurement")
{
    const TempProject fixture;
    fixture.addFile("stable.c", "int stable(void) { return 1; }\n");
    fixture.addFile("vanishing.c", "int vanishing(void) { return 2; }\n");

    codeqa::application::AnalyzeProjectRequest request;
    request.discovery.rootPath = fixture.root();
    const codeqa::model::ProjectModel project = codeqa::application::AnalyzeProject{}.execute(request);

    std::filesystem::remove(fixture.root() / "vanishing.c");

    const codeqa::metrics::ProjectMetricsModel metrics = codeqa::application::MeasureProject{}.execute(project);

    REQUIRE(metrics.files.size() == 2);
    const codeqa::metrics::FileMetricsModel& vanished =
        fileMetricsNamed(metrics, fixture.root() / "vanishing.c");
    CHECK(vanished.functions.empty());
    REQUIRE(vanished.diagnostics.size() == 1);
    CHECK(vanished.diagnostics[0].code == "METRICS001");
    CHECK(vanished.diagnostics[0].severity == codeqa::core::DiagnosticSeverity::Error);

    const codeqa::metrics::FileMetricsModel& stable = fileMetricsNamed(metrics, fixture.root() / "stable.c");
    CHECK(stable.diagnostics.empty());
    REQUIRE(stable.functions.size() == 1);
    CHECK(stable.functions[0].symbol.name == "stable");
}
```

Add `unit/MeasureProjectTest.cpp` to `tests/CMakeLists.txt`, after `unit/CalculateFunctionMetricsTest.cpp`.

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails — `codeqa/application/MeasureProject.h` does not exist.

- [ ] **Step 3: Create the header**

Create `include/codeqa/application/MeasureProject.h`:

```cpp
#pragma once

#include "codeqa/metrics/MetricsModels.h"
#include "codeqa/model/Models.h"

namespace codeqa::application
{

class MeasureProject
{
public:
    [[nodiscard]] codeqa::metrics::ProjectMetricsModel execute(const codeqa::model::ProjectModel& project) const;
};

} // namespace codeqa::application
```

- [ ] **Step 4: Implement the orchestrator**

Create `src/application/MeasureProject.cpp`:

```cpp
#include "codeqa/application/MeasureProject.h"

#include "codeqa/core/DiagnosticSeverity.h"
#include "codeqa/metrics/CalculateFunctionMetrics.h"

#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace codeqa::application
{

namespace
{

// Mirrors the binary-read helper in ParseSourceFile.cpp: same contract (raw
// bytes, no line-ending translation), different layer (application, not
// frontend), so it is not shared across the frontend/application boundary.
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

        const std::optional<std::string> content = readFileContents(file.path);

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

Note for the reviewer: the file is read exactly once per `FileModel`, via the single `readFileContents` call that sits *outside* the `for (const CallableModel& callable : file.callables)` loop — this is verifiable by inspection of the call site, not by a runtime read-count assertion.

- [ ] **Step 5: Register the new sources**

In `CMakeLists.txt`, add `src/application/MeasureProject.cpp` to `add_library(codeqa_core STATIC ...)`, right after `src/application/AnalyzeProject.cpp`.

- [ ] **Step 6: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe --test-case="MeasureProject computes metrics for every non-prototype function,MeasureProject computes metrics for every function of a multi-function file,MeasureProject reports a diagnostic, not an abort, when a file disappears before measurement"
```

Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add include/codeqa/application/MeasureProject.h src/application/MeasureProject.cpp tests/unit/MeasureProjectTest.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add the MeasureProject orchestrator"
```

---

### Task 6: Corpus de métricas

Esta tarea genera las expectativas `*.metrics.json` a partir de la salida real
del binario ya implementado (Tasks 1-5), verificada primero a mano contra
cuatro funciones — no se transcriben números sin verificarlos primero.

**Valores verificados a mano (usar como puerta de aceptación en el Paso 5):**

| Función | Fixture | size (physicalLines, codeLines, blankLines, commentLines, statementCount) | signature (parameterCount, signatureLength, booleanParameterCount) | complexity (cyclomaticComplexity, maxConditionComplexity, maxNestingDepth, branchCount, returnCount) |
|---|---|---|---|---|
| `classify` | `complexity/branching.cpp` (líneas 3-15) | (13, 12, 1, 0, 5) | (2, 22, 1) | (5, 1, 2, 2, 3) |
| `describe` | `complexity/branching.cpp` (líneas 39-50) | (12, 12, 0, 0, 7) | (1, 8, 0) | (3, 0, 2, 2, 3) |
| `fourLevels` | `nesting/deep-nesting.cpp` (líneas 16-37) | (22, 19, 3, 0, 8) | (2, 28, 0) | (5, 0, 3, 4, 1) |
| `withDefaults` | `parameters/cpp-parameters.cpp` (líneas 3-6) | (4, 4, 0, 0, 1) | (3, 52, 1) | (2, 0, 1, 1, 1) |

`describe`'s `controlFlow` (Switch, Case, Return, Case, Return, Default,
Return) y `fourLevels`'s primeras cuatro entradas (While profundidad 0, For
profundidad 1, If profundidad 2, If profundidad 3) ya están fijadas
independientemente en `tests/unit/CorpusModelTest.cpp` (líneas ~296-314) —
si el binario no reproduce esos mismos `kind`/`nestingDepth`, ese test ya
falla antes de llegar a esta tarea.

**Files:**
- Create: `tests/corpus/complexity/branching.metrics.json`
- Create: `tests/corpus/nesting/deep-nesting.metrics.json`
- Create: `tests/corpus/parameters/cpp-parameters.metrics.json`
- Create: `tests/unit/CorpusMetricsTest.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/corpus/CheckCorpusIntegrity.cmake`
- Modify: `tests/corpus/README.md`

**Interfaces:**
- Consumes: `codeqa::application::AnalyzeProject` + `MeasureProject` (Task 5), existing corpus sources in `tests/corpus/{complexity,nesting,parameters}/`.
- Produces: three `*.metrics.json` fixtures, an exact-match corpus runner, an integrity check, and README documentation.

- [ ] **Step 1: Create placeholder expectation files**

Create all three with the same placeholder content (they will be overwritten in Step 6):

```json
{
  "functions": []
}
```

at `tests/corpus/complexity/branching.metrics.json`,
`tests/corpus/nesting/deep-nesting.metrics.json`, and
`tests/corpus/parameters/cpp-parameters.metrics.json`.

- [ ] **Step 2: Write the corpus metrics runner (real test + temporary bootstrap dump)**

Create `tests/unit/CorpusMetricsTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/application/AnalyzeProject.h"
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

struct FunctionMetricsExpectation
{
    std::string name;
    std::size_t physicalLines{0};
    std::size_t codeLines{0};
    std::size_t blankLines{0};
    std::size_t commentLines{0};
    std::size_t statementCount{0};
    std::size_t parameterCount{0};
    std::size_t signatureLength{0};
    std::size_t booleanParameterCount{0};
    std::size_t cyclomaticComplexity{0};
    std::size_t maxConditionComplexity{0};
    std::size_t maxNestingDepth{0};
    std::size_t branchCount{0};
    std::size_t returnCount{0};
};

std::vector<FunctionMetricsExpectation> readExpectation(const std::filesystem::path& path)
{
    std::ifstream input(path);

    if (!input)
    {
        throw std::runtime_error("Could not open metrics expectation '" + path.generic_string() + "'.");
    }

    const Json document = Json::parse(input);
    std::vector<FunctionMetricsExpectation> expectations;

    for (const Json& function : document.at("functions"))
    {
        FunctionMetricsExpectation expected;
        expected.name = function.at("name").get<std::string>();
        const Json& size = function.at("size");
        expected.physicalLines = size.at("physicalLines").get<std::size_t>();
        expected.codeLines = size.at("codeLines").get<std::size_t>();
        expected.blankLines = size.at("blankLines").get<std::size_t>();
        expected.commentLines = size.at("commentLines").get<std::size_t>();
        expected.statementCount = size.at("statementCount").get<std::size_t>();
        const Json& signature = function.at("signature");
        expected.parameterCount = signature.at("parameterCount").get<std::size_t>();
        expected.signatureLength = signature.at("signatureLength").get<std::size_t>();
        expected.booleanParameterCount = signature.at("booleanParameterCount").get<std::size_t>();
        const Json& complexity = function.at("complexity");
        expected.cyclomaticComplexity = complexity.at("cyclomaticComplexity").get<std::size_t>();
        expected.maxConditionComplexity = complexity.at("maxConditionComplexity").get<std::size_t>();
        expected.maxNestingDepth = complexity.at("maxNestingDepth").get<std::size_t>();
        expected.branchCount = complexity.at("branchCount").get<std::size_t>();
        expected.returnCount = complexity.at("returnCount").get<std::size_t>();
        expectations.push_back(std::move(expected));
    }

    return expectations;
}

std::string describe(const codeqa::metrics::FunctionMetricsModel& value)
{
    std::ostringstream output;
    output << "{\"name\": \"" << value.symbol.name << "\", "
           << "\"size\": {\"physicalLines\": " << value.size.physicalLines
           << ", \"codeLines\": " << value.size.codeLines << ", \"blankLines\": " << value.size.blankLines
           << ", \"commentLines\": " << value.size.commentLines
           << ", \"statementCount\": " << value.size.statementCount << "}, "
           << "\"signature\": {\"parameterCount\": " << value.signature.parameterCount
           << ", \"signatureLength\": " << value.signature.signatureLength
           << ", \"booleanParameterCount\": " << value.signature.booleanParameterCount << "}, "
           << "\"complexity\": {\"cyclomaticComplexity\": " << value.complexity.cyclomaticComplexity
           << ", \"maxConditionComplexity\": " << value.complexity.maxConditionComplexity
           << ", \"maxNestingDepth\": " << value.complexity.maxNestingDepth
           << ", \"branchCount\": " << value.complexity.branchCount
           << ", \"returnCount\": " << value.complexity.returnCount << "}}";
    return output.str();
}

bool matches(const codeqa::metrics::FunctionMetricsModel& actual, const FunctionMetricsExpectation& expected)
{
    return actual.symbol.name == expected.name && actual.size.physicalLines == expected.physicalLines
        && actual.size.codeLines == expected.codeLines && actual.size.blankLines == expected.blankLines
        && actual.size.commentLines == expected.commentLines
        && actual.size.statementCount == expected.statementCount
        && actual.signature.parameterCount == expected.parameterCount
        && actual.signature.signatureLength == expected.signatureLength
        && actual.signature.booleanParameterCount == expected.booleanParameterCount
        && actual.complexity.cyclomaticComplexity == expected.cyclomaticComplexity
        && actual.complexity.maxConditionComplexity == expected.maxConditionComplexity
        && actual.complexity.maxNestingDepth == expected.maxNestingDepth
        && actual.complexity.branchCount == expected.branchCount
        && actual.complexity.returnCount == expected.returnCount;
}

const codeqa::metrics::FileMetricsModel& fileMetricsNamed(
    const codeqa::metrics::ProjectMetricsModel& project, const std::filesystem::path& path)
{
    for (const codeqa::metrics::FileMetricsModel& file : project.files)
    {
        if (file.path == path)
        {
            return file;
        }
    }

    throw std::runtime_error("Expected metrics file was not produced: " + path.generic_string());
}

std::vector<std::filesystem::path> fixturesWithMetrics(const std::filesystem::path& corpusRoot)
{
    return {
        corpusRoot / "complexity" / "branching.cpp",
        corpusRoot / "nesting" / "deep-nesting.cpp",
        corpusRoot / "parameters" / "cpp-parameters.cpp"};
}

} // namespace

TEST_CASE("corpus function metrics match their expectations")
{
    const std::filesystem::path corpusRoot{CODEQA_CORPUS_DIR};
    codeqa::application::AnalyzeProjectRequest request;
    request.discovery.rootPath = corpusRoot;
    const codeqa::model::ProjectModel project = codeqa::application::AnalyzeProject{}.execute(request);
    const codeqa::metrics::ProjectMetricsModel metrics = codeqa::application::MeasureProject{}.execute(project);

    for (const std::filesystem::path& sourcePath : fixturesWithMetrics(corpusRoot))
    {
        INFO("fixture: " << sourcePath.generic_string());
        const std::filesystem::path expectationPath =
            sourcePath.parent_path() / (sourcePath.stem().string() + ".metrics.json");
        const std::vector<FunctionMetricsExpectation> expectation = readExpectation(expectationPath);
        const codeqa::metrics::FileMetricsModel& file = fileMetricsNamed(metrics, sourcePath);

        REQUIRE(file.diagnostics.empty());
        REQUIRE_MESSAGE(
            file.functions.size() == expectation.size(),
            "actual has " << file.functions.size() << " function(s), expected " << expectation.size());

        for (std::size_t index = 0; index < expectation.size(); ++index)
        {
            CAPTURE(index);
            CHECK_MESSAGE(
                matches(file.functions[index], expectation[index]),
                "actual: " << describe(file.functions[index]) << "; expected function '"
                           << expectation[index].name << "'");
        }
    }
}

// TEMPORARY — bootstrap scaffolding for this task's Step 4. Prints every
// actual computed FunctionMetricsModel for the three metrics-bearing
// fixtures so their values can be hand-verified and transcribed into the
// *.metrics.json files. Delete this TEST_CASE in Step 7, before committing.
TEST_CASE("bootstrap: dump corpus function metrics")
{
    const std::filesystem::path corpusRoot{CODEQA_CORPUS_DIR};
    codeqa::application::AnalyzeProjectRequest request;
    request.discovery.rootPath = corpusRoot;
    const codeqa::model::ProjectModel project = codeqa::application::AnalyzeProject{}.execute(request);
    const codeqa::metrics::ProjectMetricsModel metrics = codeqa::application::MeasureProject{}.execute(project);

    for (const std::filesystem::path& sourcePath : fixturesWithMetrics(corpusRoot))
    {
        const codeqa::metrics::FileMetricsModel& file = fileMetricsNamed(metrics, sourcePath);
        MESSAGE("fixture: " << sourcePath.generic_string());

        for (const codeqa::metrics::FunctionMetricsModel& function : file.functions)
        {
            MESSAGE(describe(function));
        }
    }
}
```

Add `unit/CorpusMetricsTest.cpp` to `tests/CMakeLists.txt`, after `unit/MeasureProjectTest.cpp`.

- [ ] **Step 3: Build**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: builds clean (the real test will fail at runtime against the
placeholder `{"functions": []}` fixtures — that is expected at this point).

- [ ] **Step 4: Dump the actual computed metrics**

Run:

```text
build/tests/Debug/codeqa_tests.exe --test-case="bootstrap: dump corpus function metrics" --success
```

Capture the full output: one `describe()`-formatted line per function, per
fixture.

- [ ] **Step 5: Verify the four gate functions by hand before trusting the rest**

From the dumped output, find `classify`, `describe`, `fourLevels`, and
`withDefaults`. Compare each field against the table at the top of this
task. **All four must match exactly.** If any of them differs:

- Stop. Do not transcribe anything.
- This means a defect in Task 2, 3, or 4's implementation (not a fixture
  problem) — re-read `calculateSizeMetrics`/`calculateSignatureMetrics`/
  `calculateComplexityMetrics` against the design doc's section 5 formulas
  and the worked derivation in this table, find the divergence, fix it, and
  restart from Step 3.

If all four match, the calculator is trustworthy for the remaining nine
functions (`scan`, `drain`, `guarded` in `branching.cpp`; `flat`,
`oneLevel` in `deep-nesting.cpp`; `byReference`, `byPointer`,
`ignoresArgument`, `manyParameters` in `cpp-parameters.cpp`).

- [ ] **Step 6: Transcribe the dumped output into the real fixtures**

For each of the three fixtures, write the corresponding
`tests/corpus/<dir>/<name>.metrics.json` using this shape, with one entry
per function **in the order the dump printed them** (this is already the
`callables` order, sorted by source range — see `ProjectModelBuilderTest.cpp`
range-order checks):

```json
{
  "functions": [
    {
      "name": "classify",
      "size": {"physicalLines": 13, "codeLines": 12, "blankLines": 1, "commentLines": 0, "statementCount": 5},
      "signature": {"parameterCount": 2, "signatureLength": 22, "booleanParameterCount": 1},
      "complexity": {"cyclomaticComplexity": 5, "maxConditionComplexity": 1, "maxNestingDepth": 2, "branchCount": 2, "returnCount": 3}
    }
  ]
}
```

Fill every function's `size`/`signature`/`complexity` block with the values
read verbatim from the Step 4 dump (already hand-verified for `classify`,
`describe`, `fourLevels`, `withDefaults` in Step 5).

- [ ] **Step 7: Delete the bootstrap scaffolding and confirm GREEN**

Delete the entire `TEST_CASE("bootstrap: dump corpus function metrics")`
block (and its preceding comment) from `tests/unit/CorpusMetricsTest.cpp` —
it was scaffolding for Step 4, not part of the permanent suite.

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe --test-case="corpus function metrics match their expectations"
```

Expected: PASS.

- [ ] **Step 8: Extend the corpus integrity check**

In `tests/corpus/CheckCorpusIntegrity.cmake`, add a new check after "Check
2b" (before "Checks 3 and 4"):

```cmake
# --- Check 2c: every *.metrics.json has a sibling source ------------------
# Unlike *.parsing.json, *.metrics.json is optional per fixture (Fase 6b
# only adds it to complexity/, nesting/ and parameters/) -- so only the
# metrics-to-source direction is checked, never the reverse.

file(GLOB_RECURSE metricsFiles "${CODEQA_CORPUS_DIR}/*.metrics.json")

foreach(metricsFile IN LISTS metricsFiles)
    get_filename_component(metricsDirectory "${metricsFile}" DIRECTORY)
    get_filename_component(metricsName "${metricsFile}" NAME)
    string(REPLACE ".metrics.json" "" metricsStem "${metricsName}")

    if(NOT EXISTS "${metricsDirectory}/${metricsStem}.c"
        AND NOT EXISTS "${metricsDirectory}/${metricsStem}.cpp")
        list(APPEND failures
            "Metrics expectation '${metricsFile}' has no matching .c or .cpp fixture.")
    endif()
endforeach()
```

- [ ] **Step 9: Document the schema in the corpus README**

Append to `tests/corpus/README.md`, after the "## Límites de Fase 6a"
section:

```markdown
## Métricas (Fase 6b)

Los directorios `complexity/`, `nesting/` y `parameters/` tienen además un
archivo hermano `<nombre>.metrics.json`, opcional por fixture (solo estos
tres lo declaran). Cada uno describe, por función, las métricas de tamaño,
firma y complejidad definidas en
`docs/superpowers/specs/2026-08-08-fase-6b-motor-metricas-design.md`:

```json
{
  "functions": [
    {
      "name": "classify",
      "size": {"physicalLines": 13, "codeLines": 12, "blankLines": 1, "commentLines": 0, "statementCount": 5},
      "signature": {"parameterCount": 2, "signatureLength": 22, "booleanParameterCount": 1},
      "complexity": {"cyclomaticComplexity": 5, "maxConditionComplexity": 1, "maxNestingDepth": 2, "branchCount": 2, "returnCount": 3}
    }
  ]
}
```

`functions` es una secuencia exhaustiva y ordenada, igual que en
`*.parsing.json` — un `name` por entrada, sin duplicar `kind` ni rangos (ya
cubiertos por el `.parsing.json` hermano). Los prototipos no tienen entrada:
`MeasureProject` los excluye porque no tienen cuerpo. El runner
(`tests/unit/CorpusMetricsTest.cpp`) compara con igualdad exacta.
```

- [ ] **Step 10: Full corpus + integrity sweep**

Run:

```text
ctest --test-dir build -C Debug --output-on-failure -R "codeqa_tests|codeqa_corpus_integrity"
```

Expected: all green.

- [ ] **Step 11: Commit**

```bash
git add tests/corpus/complexity/branching.metrics.json tests/corpus/nesting/deep-nesting.metrics.json tests/corpus/parameters/cpp-parameters.metrics.json tests/unit/CorpusMetricsTest.cpp tests/CMakeLists.txt tests/corpus/CheckCorpusIntegrity.cmake tests/corpus/README.md
git commit -m "test: add corpus metrics fixtures and the metrics corpus runner"
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

Expected: every test passes, including the corpus metrics runner and the
integrity check.

- [ ] **Step 3: Confirm `codeqa parse` and the CLI surface are unchanged**

Run:

```text
build/Debug/codeqa.exe parse tests/corpus
build/Debug/codeqa.exe --help
```

Expected: `parse` output identical to before this phase (still `13/4/9/2`);
`--help` shows no new subcommand — Fase 6b adds no CLI surface, per the
design's explicit exclusion.

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
