# Fase 6a — Extensiones del modelo para métricas Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Cerrar los huecos del modelo intermedio común que la Fase 6b necesita para calcular métricas — tipo de parámetro, rango de la lista de parámetros, comentarios y sentencias — sin calcular ninguna métrica, y cerrar `CPP-PARAM-01`, `CPP-PARAM-02` y `CPP-TYPE-03` en el corpus de validación.

**Architecture:** Todo lo nuevo se apoya en mecanismos que ya existen en `StructuralExtractor.cpp`: el tipo de parámetro y el rango de su lista se derivan por aritmética de bytes sobre nodos que el extractor ya localiza (`findDeclarator`, `findParameterList`); los comentarios se recogen con el mismo recorrido de árbol completo que ya usa `collectNodesOfType` para lambdas; las sentencias se cuentan ampliando el único recorrido recursivo que ya alimenta `localVariables`/`controlFlow`/`calls` dentro de `extractBodyStructure`. No se añaden queries `.scm` ni archivos nuevos. `ProjectModelBuilder` copia los campos basados en `core::SourceRange` directamente (no están duplicados por espacio de nombres) y convierte `declaredType` como un campo más de `ParameterModel`.

**Tech Stack:** C++17, CMake, doctest, tree-sitter C/C++ vendorizado, nlohmann/json (solo tests del corpus).

## Global Constraints

- Todo campo nuevo se añade **al final** de su struct (`ParameterModel`, `FunctionModel`/`CallableModel`, `ParsedFile`/`FileModel`), nunca insertado entre campos existentes — varios sitios del código y de los tests construyen estos structs con inicialización posicional (`ParameterModel{"target", Passing::Reference, true}`, `FunctionModel{name, kind, range, parameters, isStatic, isVariadic}`), y desplazar un campo existente rompería esas llamadas en silencio o con un error de compilación confuso.
- No se crean archivos `.cpp`/`.h`/`.scm` nuevos y no se toca `CMakeLists.txt` — todo el trabajo modifica archivos ya existentes.
- `core::SourceRange` es el mismo tipo en `codeqa::frontend` y `codeqa::model` (no se duplica por espacio de nombres como sí ocurre con `ParameterModel`/`ControlFlowModel`/`CallModel`). Los campos `comments`, `parameterListRange` y `statements` se copian directamente en `ProjectModelBuilder`, sin función de conversión.
- `ParameterModel.declaredType` es el texto fuente del parámetro desde su inicio hasta el comienzo del declarador (o el texto completo si no hay declarador), recortado de espacios. Excluye marcadores del declarador (`*`, `&`) porque `ParameterPassing` ya los identifica.
- `CallableModel.statements`/`FunctionModel.statements` cuentan nodos con nombre de estos tipos exactos: `expression_statement`, `declaration`, `return_statement`, `if_statement`, `for_statement`, `for_range_loop`, `while_statement`, `do_statement`, `switch_statement`, `case_statement`, `default_statement`, `break_statement`, `continue_statement`. Una `declaration` que es la cláusula de inicialización de un `for_statement` no cuenta aparte — el `for_statement` ya cuenta una vez. `compound_statement` nunca cuenta.
- Un comentario o sentencia dentro de un subárbol `ERROR`/`MISSING` se descarta, igual que el resto de la extracción de cuerpo de Fase 5 capa 2.
- El campo `type` en el JSON del corpus pasa a ser obligatorio en **todo** parámetro de **toda** expectativa que declare parámetros — no solo en los fixtures que cierran IDs — porque el runner del corpus (`CorpusStructureTest.cpp`) compara `FunctionModel` por igualdad exhaustiva; si `type` faltara en un fixture, esa comparación fallaría para ese fixture en cuanto `declaredType` entre en la comparación, así que no hay forma de dejarlo "opcional" sin además dejar de comparar `declaredType` en general.
- `codeqa parse tests/corpus` no cambia su salida visible; el resumen sigue siendo `13/4/9/2`.

## File Structure

- `include/codeqa/frontend/StructuralModels.h`: añade `declaredType` a `ParameterModel`, `parameterListRange` y `statements` a `FunctionModel`.
- `include/codeqa/frontend/ParsedFile.h`: añade `comments`.
- `include/codeqa/model/Models.h`: los mismos cuatro campos en `ParameterModel`/`CallableModel`/`FileModel`.
- `src/frontend/StructuralExtractor.cpp`: extracción real de los cuatro campos.
- `src/model/ProjectModelBuilder.cpp`: conversión al modelo común.
- `tests/unit/StructuralModelsTest.cpp`, `StructuralExtractorTest.cpp`, `ProjectModelBuilderTest.cpp`: cobertura de contrato y extracción.
- `tests/unit/CorpusStructureTest.cpp`: soporte de `type` en el esquema del corpus.
- `tests/corpus/CheckCorpusIntegrity.cmake`: exige `type` en cada parámetro.
- `tests/corpus/**/*.parsing.json` (13 archivos): añaden `type` a cada parámetro.
- `tests/corpus/README.md`, `tests/corpus/SUPPORTED-CONSTRUCTS.md`: documentan el cierre parcial y los IDs que siguen abiertos.

---

### Task 1: `ParameterModel.declaredType`

**Files:**
- Modify: `include/codeqa/frontend/StructuralModels.h`
- Modify: `src/frontend/StructuralExtractor.cpp`
- Modify: `tests/unit/StructuralExtractorTest.cpp`

**Interfaces:**

```cpp
// StructuralModels.h — ParameterModel gains a trailing field:
struct ParameterModel
{
    std::string name;
    ParameterPassing passing{ParameterPassing::Value};
    bool hasDefaultValue{false};
    std::string declaredType;
};
```

- [ ] **Step 1: Write the failing tests**

Add to `tests/unit/StructuralExtractorTest.cpp`, after the existing `TEST_CASE("derives C++ parameter passing from the declarator only")`:

```cpp
TEST_CASE("extracts the declared type of each parameter, excluding declarator markers")
{
    const TempSourceFile file{
        "parameter-types.cpp",
        "void classify(bool flag, const bool constFlag, bool* flagPointer,\n"
        "    bool& flagReference, int value, void (*callback)(int))\n"
        "{\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 1);
    REQUIRE(result.functions[0].parameters.size() == 6);
    CHECK(result.functions[0].parameters[0].declaredType == "bool");
    CHECK(result.functions[0].parameters[1].declaredType == "const bool");
    CHECK(result.functions[0].parameters[2].declaredType == "bool");
    CHECK(result.functions[0].parameters[3].declaredType == "bool");
    CHECK(result.functions[0].parameters[4].declaredType == "int");
    CHECK(result.functions[0].parameters[5].declaredType == "void");
}

TEST_CASE("extracts the declared type of unnamed and void C parameters")
{
    const TempSourceFile file{
        "unnamed-parameter-types.c",
        "int ignoresArgument(int)\n"
        "{\n"
        "    return 0;\n"
        "}\n"
        "\n"
        "int noArguments(void)\n"
        "{\n"
        "    return 0;\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::C, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 2);
    REQUIRE(result.functions[0].parameters.size() == 1);
    CHECK(result.functions[0].parameters[0].declaredType == "int");
    REQUIRE(result.functions[1].parameters.size() == 1);
    CHECK(result.functions[1].parameters[0].declaredType == "void");
}
```

- [ ] **Step 2: Run the focused tests to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe --test-case="extracts the declared type of each parameter, excluding declarator markers,extracts the declared type of unnamed and void C parameters"
```

Expected: compilation fails — `ParameterModel` has no member `declaredType`.

- [ ] **Step 3: Add the field and the extraction**

In `include/codeqa/frontend/StructuralModels.h`, add `std::string declaredType;` as the last member of `ParameterModel` (after `hasDefaultValue`).

In `src/frontend/StructuralExtractor.cpp`, add a helper right before `parametersFor` (around line 463):

```cpp
std::string parameterDeclaredType(TSNode parameter, TSNode declarator, std::string_view source)
{
    const std::size_t parameterStart = ts_node_start_byte(parameter);
    const std::size_t typeEnd = ts_node_is_null(declarator)
        ? ts_node_end_byte(parameter)
        : ts_node_start_byte(declarator);

    if (typeEnd < parameterStart || typeEnd > source.size())
    {
        return trim(nodeText(parameter, source));
    }

    return trim(std::string(source.substr(parameterStart, typeEnd - parameterStart)));
}
```

Inside `parametersFor`, right after `model.hasDefaultValue = ...;` (currently the last assignment before `parameters.push_back(std::move(model));`), add:

```cpp
        model.declaredType = parameterDeclaredType(parameter, declarator, source);
```

- [ ] **Step 4: Verify the tests pass**

Run:

```text
cmake --build build --config Debug
build/tests/Debug/codeqa_tests.exe --test-case="extracts the declared type of each parameter, excluding declarator markers,extracts the declared type of unnamed and void C parameters"
ctest --test-dir build -C Debug --output-on-failure
```

- [ ] **Step 5: Commit**

```text
git add include/codeqa/frontend/StructuralModels.h src/frontend/StructuralExtractor.cpp tests/unit/StructuralExtractorTest.cpp
git commit -m "feat: extract declared type per parameter"
```

---

### Task 2: `CallableModel.parameterListRange`

**Files:**
- Modify: `include/codeqa/frontend/StructuralModels.h`
- Modify: `src/frontend/StructuralExtractor.cpp`
- Modify: `tests/unit/StructuralExtractorTest.cpp`

**Interfaces:**

```cpp
// StructuralModels.h — FunctionModel gains a trailing field (in addition to
// Task 4's `statements`, added later in this same struct):
struct FunctionModel
{
    // ...existing fields unchanged...
    std::optional<core::SourceRange> parameterListRange;
};
```

**Consumes:** `findParameterList(TSNode functionDeclarator)`, already defined in `StructuralExtractor.cpp` (used today by `parametersFor` and `isVariadic`); `sourceRange(TSNode, const std::filesystem::path&)`.

- [ ] **Step 1: Write the failing tests**

Add to `tests/unit/StructuralExtractorTest.cpp`:

```cpp
TEST_CASE("captures the parameter list range for functions and prototypes")
{
    const TempSourceFile file{
        "parameter-list-range.cpp",
        "int add(int left, int right);\n"
        "\n"
        "int add(int left, int right)\n"
        "{\n"
        "    return left + right;\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 2);

    for (const codeqa::frontend::FunctionModel& function : result.functions)
    {
        REQUIRE(function.parameterListRange.has_value());
        CHECK(function.parameterListRange->begin.line == function.range.begin.line);
    }
}

TEST_CASE("leaves the parameter list range empty for a parameterless lambda")
{
    const TempSourceFile file{
        "lambda-no-parameters.cpp",
        "void run()\n"
        "{\n"
        "    auto action = []{ return 1; };\n"
        "    action();\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    const auto lambda = std::find_if(
        result.functions.begin(),
        result.functions.end(),
        [](const codeqa::frontend::FunctionModel& function) {
            return function.kind == codeqa::frontend::FunctionKind::Lambda;
        });

    REQUIRE(lambda != result.functions.end());
    CHECK_FALSE(lambda->parameterListRange.has_value());
}
```

This file already includes `<vector>`; add `#include <algorithm>` to its include block for `std::find_if`.

- [ ] **Step 2: Run the focused tests to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe --test-case="captures the parameter list range for functions and prototypes,leaves the parameter list range empty for a parameterless lambda"
```

Expected: compilation fails — `FunctionModel` has no member `parameterListRange`.

- [ ] **Step 3: Add the field and the extraction**

In `include/codeqa/frontend/StructuralModels.h`, add `std::optional<core::SourceRange> parameterListRange;` as a new trailing member of `FunctionModel` (after `calls`; Task 4 adds `statements` right after this one, in the order the two tasks land).

In `src/frontend/StructuralExtractor.cpp`, inside `adaptFunction` (around line 550, right after `model.parameters = parametersFor(functionDeclarator, source);`), add:

```cpp
    const TSNode parameterList = findParameterList(functionDeclarator);
    model.parameterListRange = ts_node_is_null(parameterList)
        ? std::nullopt
        : std::optional<core::SourceRange>(sourceRange(parameterList, path));
```

- [ ] **Step 4: Verify the tests pass**

Run:

```text
cmake --build build --config Debug
build/tests/Debug/codeqa_tests.exe --test-case="captures the parameter list range for functions and prototypes,leaves the parameter list range empty for a parameterless lambda"
ctest --test-dir build -C Debug --output-on-failure
```

- [ ] **Step 5: Commit**

```text
git add include/codeqa/frontend/StructuralModels.h src/frontend/StructuralExtractor.cpp tests/unit/StructuralExtractorTest.cpp
git commit -m "feat: capture parameter list range per callable"
```

---

### Task 3: `FileModel.comments`

**Files:**
- Modify: `include/codeqa/frontend/ParsedFile.h`
- Modify: `src/frontend/StructuralExtractor.cpp`
- Modify: `tests/unit/StructuralExtractorTest.cpp`

**Interfaces:**

```cpp
// ParsedFile.h
#include "codeqa/core/SourceRange.h"
// ...
struct ParsedFile
{
    // ...existing fields unchanged...
    std::vector<core::SourceRange> comments;
};
```

**Consumes:** `collectNodesOfType(TSNode node, const char* type, std::vector<TSNode>& nodes)`, already defined in `StructuralExtractor.cpp` (used today to collect `lambda_expression` nodes).

- [ ] **Step 1: Write the failing test**

Add to `tests/unit/StructuralExtractorTest.cpp`:

```cpp
TEST_CASE("collects line and block comments anywhere in the file")
{
    const TempSourceFile file{
        "comments.cpp",
        "// leading comment\n"
        "int add(int left, int right)\n"
        "{\n"
        "    /* block\n"
        "       comment */\n"
        "    return left + right; // trailing comment\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.comments.size() == 3);
    CHECK(result.comments[0].begin.line == 1);
    CHECK(result.comments[1].begin.line == 4);
    CHECK(result.comments[1].end.line == 5);
    CHECK(result.comments[2].begin.line == 6);
}
```

- [ ] **Step 2: Run the focused test to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe --test-case="collects line and block comments anywhere in the file"
```

Expected: compilation fails — `ParsedFile` has no member `comments`.

- [ ] **Step 3: Add the field and the extraction**

In `include/codeqa/frontend/ParsedFile.h`, add `#include "codeqa/core/SourceRange.h"` to the include block, and add `std::vector<core::SourceRange> comments;` as a new trailing member of `ParsedFile` (after `typeAliases`, before `parsed`; `parsed` stays last since every call site sets it by name, not positionally, so this is safe either way, but keeping `parsed` last matches its role as the final "is this usable" flag).

In `src/frontend/StructuralExtractor.cpp`, inside `StructuralExtractor::extract` (right after `const TSNode root = tree.impl_->rootNode();`, before the functions query cursor is executed), add:

```cpp
    std::vector<TSNode> comments;
    collectNodesOfType(root, "comment", comments);

    for (const TSNode comment : comments)
    {
        result.comments.push_back(sourceRange(comment, path));
    }

    std::sort(
        result.comments.begin(),
        result.comments.end(),
        [](const core::SourceRange& left, const core::SourceRange& right) {
            if (left.begin.offset != right.begin.offset)
            {
                return left.begin.offset < right.begin.offset;
            }

            return left.end.offset < right.end.offset;
        });
```

- [ ] **Step 4: Verify the test passes**

Run:

```text
cmake --build build --config Debug
build/tests/Debug/codeqa_tests.exe --test-case="collects line and block comments anywhere in the file"
ctest --test-dir build -C Debug --output-on-failure
```

- [ ] **Step 5: Commit**

```text
git add include/codeqa/frontend/ParsedFile.h src/frontend/StructuralExtractor.cpp tests/unit/StructuralExtractorTest.cpp
git commit -m "feat: collect file-level comment ranges"
```

---

### Task 4: `CallableModel.statements`

**Files:**
- Modify: `include/codeqa/frontend/StructuralModels.h`
- Modify: `src/frontend/StructuralExtractor.cpp`
- Modify: `tests/unit/StructuralExtractorTest.cpp`

**Interfaces:**

```cpp
// StructuralModels.h — FunctionModel's final trailing field:
struct FunctionModel
{
    // ...existing fields, then Task 2's parameterListRange...
    std::vector<core::SourceRange> statements;
};
```

- [ ] **Step 1: Write the failing tests**

Add to `tests/unit/StructuralExtractorTest.cpp`:

```cpp
TEST_CASE("counts statements at any nesting depth, excluding compound blocks")
{
    const TempSourceFile file{
        "statement-count.cpp",
        "int classify(int value)\n"
        "{\n"
        "    int result = 0;\n"
        "\n"
        "    if (value > 0)\n"
        "    {\n"
        "        result = value;\n"
        "    }\n"
        "\n"
        "    return result;\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 1);
    // declaration (int result = 0;), if_statement, expression_statement
    // (result = value;), return_statement -- the compound_statement braces
    // around the if-body do not add an entry.
    CHECK(result.functions[0].statements.size() == 4);
}

TEST_CASE("counts a for-loop once and excludes its header clauses")
{
    const TempSourceFile file{
        "for-loop-statement-count.cpp",
        "int sum(int count)\n"
        "{\n"
        "    int total = 0;\n"
        "\n"
        "    for (int index = 0; index < count; ++index)\n"
        "    {\n"
        "        total += index;\n"
        "    }\n"
        "\n"
        "    return total;\n"
        "}\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser, extractor);

    REQUIRE(result.functions.size() == 1);
    // declaration (int total = 0;), for_statement (once -- its init clause
    // "int index = 0" does not add a second entry), expression_statement
    // (total += index;), return_statement.
    CHECK(result.functions[0].statements.size() == 4);
}
```

- [ ] **Step 2: Run the focused tests to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe --test-case="counts statements at any nesting depth, excluding compound blocks,counts a for-loop once and excludes its header clauses"
```

Expected: compilation fails — `FunctionModel` has no member `statements`.

- [ ] **Step 3: Add the field and the extraction**

In `include/codeqa/frontend/StructuralModels.h`, add `std::vector<core::SourceRange> statements;` as the new last member of `FunctionModel`, after `parameterListRange` from Task 2.

In `src/frontend/StructuralExtractor.cpp`, add a helper right before `extractBodyStructure`'s definition (around line 977, next to `isForStatement`):

```cpp
bool isStatementNode(TSNode node)
{
    if (isNodeType(node, "declaration") && isForStatement(ts_node_parent(node)))
    {
        // The for-loop's init clause ("int i = 0" in `for (int i = 0; ...)`)
        // is part of the for_statement itself, which already counts as one
        // statement; counting it again would break the "for counts once"
        // rule from the design.
        return false;
    }

    return isNodeType(node, "expression_statement") || isNodeType(node, "declaration")
        || isNodeType(node, "return_statement") || isNodeType(node, "if_statement")
        || isNodeType(node, "for_statement") || isNodeType(node, "for_range_loop")
        || isNodeType(node, "while_statement") || isNodeType(node, "do_statement")
        || isNodeType(node, "switch_statement") || isNodeType(node, "case_statement")
        || isNodeType(node, "default_statement") || isNodeType(node, "break_statement")
        || isNodeType(node, "continue_statement");
}
```

Inside `extractBodyStructure`'s `visit` lambda (around line 1038, right after the existing `ControlFlowModel` block that ends with `function.controlFlow.push_back(std::move(flow));` and its closing `}`), add:

```cpp
        if (isStatementNode(node) && !subtreeContainsErrorOrMissing(node))
        {
            function.statements.push_back(sourceRange(node, path));
        }
```

After the existing `calls` sort block at the end of `extractBodyStructure` (right after the `std::stable_sort` for `function.calls`, before the function's closing `}`), add:

```cpp
    std::stable_sort(
        function.statements.begin(),
        function.statements.end(),
        [](const core::SourceRange& left, const core::SourceRange& right) {
            if (left.begin.offset != right.begin.offset)
            {
                return left.begin.offset < right.begin.offset;
            }

            return left.end.offset < right.end.offset;
        });
```

- [ ] **Step 4: Verify the tests pass**

Run:

```text
cmake --build build --config Debug
build/tests/Debug/codeqa_tests.exe --test-case="counts statements at any nesting depth, excluding compound blocks,counts a for-loop once and excludes its header clauses"
ctest --test-dir build -C Debug --output-on-failure
```

- [ ] **Step 5: Commit**

```text
git add include/codeqa/frontend/StructuralModels.h src/frontend/StructuralExtractor.cpp tests/unit/StructuralExtractorTest.cpp
git commit -m "feat: count statements per callable"
```

---

### Task 5: Mirror the four fields into the common model

**Files:**
- Modify: `include/codeqa/model/Models.h`
- Modify: `src/model/ProjectModelBuilder.cpp`
- Modify: `tests/unit/ProjectModelBuilderTest.cpp`
- Modify: `tests/unit/StructuralModelsTest.cpp`

**Interfaces:**

```cpp
// Models.h
struct ParameterModel
{
    std::string name;
    ParameterPassing passing{ParameterPassing::Value};
    bool hasDefaultValue{false};
    std::string declaredType;
};

struct CallableModel
{
    // ...existing fields unchanged...
    std::optional<core::SourceRange> parameterListRange;
    std::vector<core::SourceRange> statements;
};

struct FileModel
{
    // ...existing fields unchanged...
    std::vector<core::SourceRange> comments;
};
```

**Consumes:** `frontend::FunctionModel::{parameterListRange, statements}`, `frontend::ParsedFile::comments`, `frontend::ParameterModel::declaredType` (all from Tasks 1-4).

- [ ] **Step 1: Write the failing tests**

Add to `tests/unit/StructuralModelsTest.cpp`, inside the existing `TEST_CASE("structural models retain the attributes required by the corpus")`, right after the existing parameter assertions (`CHECK(function.parameters.front().hasDefaultValue);`):

```cpp
    CHECK(function.parameters.front().declaredType.empty());
```

and right after the existing `CHECK(function.calls[0].kind == codeqa::frontend::CallKind::Named);`:

```cpp
    CHECK_FALSE(function.parameterListRange.has_value());
    CHECK(function.statements.empty());
```

Add to `tests/unit/ProjectModelBuilderTest.cpp` a new test:

```cpp
TEST_CASE("copies declared parameter types, parameter list ranges, comments and statements")
{
    const std::filesystem::path rootPath{"project"};
    const std::filesystem::path sourcePath{"project/src/greet.cpp"};

    codeqa::frontend::ParsedFile sourceFile;
    sourceFile.path = sourcePath;
    sourceFile.language = codeqa::core::Language::Cpp;
    sourceFile.grammar = codeqa::frontend::Grammar::Cpp;
    sourceFile.parsed = true;
    sourceFile.comments = {rangeFor(sourcePath, 0, 10)};
    sourceFile.functions.push_back(codeqa::frontend::FunctionModel{
        "greet",
        codeqa::frontend::FunctionKind::Free,
        rangeFor(sourcePath, 20, 60),
        {codeqa::frontend::ParameterModel{
            "loud", codeqa::frontend::ParameterPassing::Value, false, "bool"}},
        false,
        false});
    sourceFile.functions[0].parameterListRange = rangeFor(sourcePath, 25, 35);
    sourceFile.functions[0].statements = {rangeFor(sourcePath, 40, 50), rangeFor(sourcePath, 50, 59)};

    const codeqa::model::ProjectModel result = codeqa::model::ProjectModelBuilder{}.build(
        rootPath, std::vector<codeqa::frontend::ParsedFile>{sourceFile});

    REQUIRE(result.files.size() == 1);
    CHECK(result.files[0].comments == sourceFile.comments);
    REQUIRE(result.files[0].callables.size() == 1);
    const codeqa::model::CallableModel& callable = result.files[0].callables[0];
    REQUIRE(callable.parameters.size() == 1);
    CHECK(callable.parameters[0].declaredType == "bool");
    CHECK(callable.parameterListRange == sourceFile.functions[0].parameterListRange);
    CHECK(callable.statements == sourceFile.functions[0].statements);
}
```

This uses the `rangeFor` helper already defined in this file's anonymous namespace.

- [ ] **Step 2: Run the focused tests to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe --test-case="structural models retain the attributes required by the corpus,copies declared parameter types, parameter list ranges, comments and statements"
```

Expected: compilation fails — `model::ParameterModel`/`CallableModel`/`FileModel` have no matching members yet.

- [ ] **Step 3: Mirror the fields and wire the conversion**

In `include/codeqa/model/Models.h`:
- Add `std::string declaredType;` as the last member of `ParameterModel`.
- Add `std::optional<core::SourceRange> parameterListRange;` and `std::vector<core::SourceRange> statements;` as new trailing members of `CallableModel` (after `calls`).
- Add `std::vector<core::SourceRange> comments;` as a new trailing member of `FileModel` (after `typeAliases`).

In `src/model/ProjectModelBuilder.cpp`:
- In the parameter conversion loop inside `buildFileModel` (currently `callable.parameters.push_back(ParameterModel{ parameter.name, parameterPassingFor(parameter.passing), parameter.hasDefaultValue});`), add the fourth field:

```cpp
        for (const frontend::ParameterModel& parameter : function.parameters)
        {
            callable.parameters.push_back(ParameterModel{
                parameter.name,
                parameterPassingFor(parameter.passing),
                parameter.hasDefaultValue,
                parameter.declaredType});
        }
```

- Right after `callable.isVariadic = function.isVariadic;` (before the parameters loop), add:

```cpp
        callable.parameterListRange = function.parameterListRange;
        callable.statements = function.statements;
```

- In `buildFileModel`, right after `model.diagnostics = file.diagnostics;` and the `if (!file.parsed) { return model; }` guard, as the first line of the populated section (before the includes loop), add:

```cpp
    model.comments = file.comments;
```

- [ ] **Step 4: Verify the tests pass**

Run:

```text
cmake --build build --config Debug
build/tests/Debug/codeqa_tests.exe --test-case="structural models retain the attributes required by the corpus,copies declared parameter types, parameter list ranges, comments and statements"
ctest --test-dir build -C Debug --output-on-failure
```

- [ ] **Step 5: Commit**

```text
git add include/codeqa/model/Models.h src/model/ProjectModelBuilder.cpp tests/unit/ProjectModelBuilderTest.cpp tests/unit/StructuralModelsTest.cpp
git commit -m "feat: adapt parameter types, comments and statements to the common model"
```

---

### Task 6: Close `CPP-PARAM-01`, `CPP-PARAM-02` and `CPP-TYPE-03` in the corpus

**Files:**
- Modify: `tests/unit/CorpusStructureTest.cpp`
- Modify: `tests/corpus/CheckCorpusIntegrity.cmake`
- Modify: `tests/corpus/README.md`
- Modify: `tests/corpus/parsing/minimal.parsing.json`
- Modify: `tests/corpus/parsing/minimal-cpp.parsing.json`
- Modify: `tests/corpus/parsing/class-header.parsing.json`
- Modify: `tests/corpus/parameters/cpp-parameters.parsing.json`
- Modify: `tests/corpus/nesting/deep-nesting.parsing.json`
- Modify: `tests/corpus/mixed/mixed-constructs.parsing.json`
- Modify: `tests/corpus/malformed/unclosed-brace.parsing.json`
- Modify: `tests/corpus/malformed/truncated.parsing.json`
- Modify: `tests/corpus/functions/c-functions.parsing.json`
- Modify: `tests/corpus/functions/cpp-functions.parsing.json`
- Modify: `tests/corpus/complexity/branching.parsing.json`
- Modify: `tests/corpus/classes/c-types.parsing.json`
- Modify: `tests/corpus/classes/cpp-types.parsing.json`

This task has no separate "write a failing test" step in the usual sense: the
failing state is the corpus runner itself once it starts comparing `type`,
against fixtures that don't have it yet. Steps 1-2 make the runner compare
`type` (which will fail every corpus fixture with parameters, since none has
`type` yet); steps 3-4 fill in every fixture with the exact expected type so
the same run turns green.

- [ ] **Step 1: Make the corpus runner require and compare `type`**

In `tests/unit/CorpusStructureTest.cpp`:

Add `std::string type;` as the last member of `ParameterExpectation` (around line 37-42):

```cpp
struct ParameterExpectation
{
    std::string name;
    codeqa::frontend::ParameterPassing passing{codeqa::frontend::ParameterPassing::Value};
    bool hasDefaultValue{false};
    std::string type;
};
```

In the parameter-parsing loop (around line 234-240), read the new field:

```cpp
        for (const Json& parameter : function.at("parameters"))
        {
            expected.parameters.push_back(ParameterExpectation{
                parameter.at("name").get<std::string>(),
                parameterPassingFrom(parameter.at("passing").get<std::string>()),
                parameter.at("hasDefaultValue").get<bool>(),
                parameter.at("type").get<std::string>()});
        }
```

In `matches(const FunctionModel&, const FunctionExpectation&)` (around line 292-293), compare it:

```cpp
        if (parameter.name != expectedParameter.name || parameter.passing != expectedParameter.passing
            || parameter.hasDefaultValue != expectedParameter.hasDefaultValue
            || parameter.declaredType != expectedParameter.type)
        {
            return false;
        }
```

- [ ] **Step 2: Run the corpus test to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe --test-case="every corpus fixture matches its structural expectation"
```

Expected: compilation fails first (`parameter.at("type")` throws `json::out_of_range` at runtime once it compiles, since `.at()` requires the key to exist) — every fixture with parameters starts failing because none has `type` yet. This confirms the comparison is live before fixtures are updated.

- [ ] **Step 3: Add `type` to every corpus parameter**

Update each file's `parameters` arrays with the `type` values below (all other fields in each file are unchanged).

`tests/corpus/parsing/minimal.parsing.json` — `main(void)`:
```json
"parameters": [{"name": "", "passing": "value", "hasDefaultValue": false, "type": "void"}]
```

`tests/corpus/parsing/minimal-cpp.parsing.json` — `identity(int value)`:
```json
"parameters": [{"name": "value", "passing": "value", "hasDefaultValue": false, "type": "int"}]
```

`tests/corpus/parsing/class-header.parsing.json` — `scale(double factor)`:
```json
"parameters": [{"name": "factor", "passing": "value", "hasDefaultValue": false, "type": "double"}]
```

`tests/corpus/parameters/cpp-parameters.parsing.json`:
```json
{"name": "withDefaults", "kind": "free", "startLine": 3, "endLine": 6, "parameters": [{"name": "required", "passing": "value", "hasDefaultValue": false, "type": "int"}, {"name": "optional", "passing": "value", "hasDefaultValue": true, "type": "int"}, {"name": "verbose", "passing": "value", "hasDefaultValue": true, "type": "bool"}], "isStatic": false, "isVariadic": false},
{"name": "byReference", "kind": "free", "startLine": 8, "endLine": 11, "parameters": [{"name": "target", "passing": "reference", "hasDefaultValue": false, "type": "std::string"}, {"name": "source", "passing": "reference", "hasDefaultValue": false, "type": "const std::string"}], "isStatic": false, "isVariadic": false},
{"name": "byPointer", "kind": "free", "startLine": 13, "endLine": 16, "parameters": [{"name": "output", "passing": "pointer", "hasDefaultValue": false, "type": "int"}, {"name": "input", "passing": "pointer", "hasDefaultValue": false, "type": "const int"}], "isStatic": false, "isVariadic": false},
{"name": "ignoresArgument", "kind": "free", "startLine": 18, "endLine": 21, "parameters": [{"name": "", "passing": "value", "hasDefaultValue": false, "type": "int"}], "isStatic": false, "isVariadic": false},
{"name": "manyParameters", "kind": "free", "startLine": 23, "endLine": 26, "parameters": [{"name": "first", "passing": "value", "hasDefaultValue": false, "type": "int"}, {"name": "second", "passing": "value", "hasDefaultValue": false, "type": "int"}, {"name": "third", "passing": "value", "hasDefaultValue": false, "type": "int"}, {"name": "fourth", "passing": "value", "hasDefaultValue": false, "type": "int"}, {"name": "fifth", "passing": "value", "hasDefaultValue": false, "type": "int"}], "isStatic": false, "isVariadic": false}
```

`tests/corpus/nesting/deep-nesting.parsing.json`:
```json
{"name": "flat", "kind": "free", "startLine": 1, "endLine": 4, "parameters": [{"name": "value", "passing": "value", "hasDefaultValue": false, "type": "int"}], "isStatic": false, "isVariadic": false},
{"name": "oneLevel", "kind": "free", "startLine": 6, "endLine": 14, "parameters": [{"name": "value", "passing": "value", "hasDefaultValue": false, "type": "int"}], "isStatic": false, "isVariadic": false},
{"name": "fourLevels", "kind": "free", "startLine": 16, "endLine": 37, "parameters": [{"name": "values", "passing": "pointer", "hasDefaultValue": false, "type": "const int"}, {"name": "count", "passing": "value", "hasDefaultValue": false, "type": "int"}], "isStatic": false, "isVariadic": false}
```

`tests/corpus/mixed/mixed-constructs.parsing.json`:
```json
{"name": "Engine", "kind": "constructor", "startLine": 22, "endLine": 25, "parameters": [{"name": "settings", "passing": "value", "hasDefaultValue": false, "type": "Settings"}], "isStatic": false, "isVariadic": false},
{"name": "~Engine", "kind": "destructor", "startLine": 27, "endLine": 27, "parameters": [], "isStatic": false, "isVariadic": false},
{"name": "run", "kind": "prototype", "startLine": 29, "endLine": 29, "parameters": [{"name": "inputs", "passing": "reference", "hasDefaultValue": false, "type": "const std::vector<int>"}], "isStatic": false, "isVariadic": false},
{"name": "operator!", "kind": "operator", "startLine": 31, "endLine": 34, "parameters": [], "isStatic": false, "isVariadic": false},
{"name": "Engine::run", "kind": "method", "startLine": 40, "endLine": 56, "parameters": [{"name": "inputs", "passing": "reference", "hasDefaultValue": false, "type": "const std::vector<int>"}], "isStatic": false, "isVariadic": false},
{"name": "isEmpty", "kind": "free", "startLine": 58, "endLine": 62, "parameters": [{"name": "container", "passing": "reference", "hasDefaultValue": false, "type": "const T"}], "isStatic": false, "isVariadic": false}
```

`tests/corpus/malformed/unclosed-brace.parsing.json` — `wellFormed(int value)`:
```json
"functions": [{"name": "wellFormed", "kind": "free", "startLine": 1, "endLine": 4, "parameters": [{"name": "value", "passing": "value", "hasDefaultValue": false, "type": "int"}], "isStatic": false, "isVariadic": false}],
```

`tests/corpus/malformed/truncated.parsing.json` — `completeFunction(int value)`:
```json
"functions": [{"name": "completeFunction", "kind": "free", "startLine": 3, "endLine": 6, "parameters": [{"name": "value", "passing": "value", "hasDefaultValue": false, "type": "int"}], "isStatic": false, "isVariadic": false}],
```

`tests/corpus/functions/c-functions.parsing.json`:
```json
{"name": "declaredOnly", "kind": "prototype", "startLine": 3, "endLine": 3, "parameters": [{"name": "value", "passing": "value", "hasDefaultValue": false, "type": "int"}], "isStatic": false, "isVariadic": false},
{"name": "helper", "kind": "free", "startLine": 5, "endLine": 8, "parameters": [{"name": "value", "passing": "value", "hasDefaultValue": false, "type": "int"}], "isStatic": true, "isVariadic": false},
{"name": "sumAll", "kind": "free", "startLine": 10, "endLine": 24, "parameters": [{"name": "count", "passing": "value", "hasDefaultValue": false, "type": "int"}], "isStatic": false, "isVariadic": true},
{"name": "applyTwice", "kind": "free", "startLine": 26, "endLine": 29, "parameters": [{"name": "value", "passing": "value", "hasDefaultValue": false, "type": "int"}, {"name": "operation", "passing": "function-pointer", "hasDefaultValue": false, "type": "int"}], "isStatic": false, "isVariadic": false},
{"name": "findFirstNegative", "kind": "free", "startLine": 31, "endLine": 48, "parameters": [{"name": "values", "passing": "pointer", "hasDefaultValue": false, "type": "const int"}, {"name": "count", "passing": "value", "hasDefaultValue": false, "type": "int"}], "isStatic": false, "isVariadic": false}
```

`tests/corpus/functions/cpp-functions.parsing.json`:
```json
{"name": "Counter", "kind": "constructor", "startLine": 7, "endLine": 10, "parameters": [], "isStatic": false, "isVariadic": false},
{"name": "~Counter", "kind": "destructor", "startLine": 12, "endLine": 15, "parameters": [], "isStatic": false, "isVariadic": false},
{"name": "increment", "kind": "method", "startLine": 17, "endLine": 20, "parameters": [], "isStatic": false, "isVariadic": false},
{"name": "value", "kind": "prototype", "startLine": 22, "endLine": 22, "parameters": [], "isStatic": false, "isVariadic": false},
{"name": "operator==", "kind": "operator", "startLine": 24, "endLine": 27, "parameters": [{"name": "other", "passing": "reference", "hasDefaultValue": false, "type": "const Counter"}], "isStatic": false, "isVariadic": false},
{"name": "Counter::value", "kind": "method", "startLine": 33, "endLine": 36, "parameters": [], "isStatic": false, "isVariadic": false},
{"name": "countPositive", "kind": "free", "startLine": 38, "endLine": 44, "parameters": [{"name": "values", "passing": "reference", "hasDefaultValue": false, "type": "const std::vector<int>"}], "isStatic": false, "isVariadic": false},
{"name": "<lambda>", "kind": "lambda", "startLine": 40, "endLine": 43, "parameters": [{"name": "value", "passing": "value", "hasDefaultValue": false, "type": "int"}], "isStatic": false, "isVariadic": false},
{"name": "largest", "kind": "free", "startLine": 46, "endLine": 50, "parameters": [{"name": "first", "passing": "value", "hasDefaultValue": false, "type": "T"}, {"name": "second", "passing": "value", "hasDefaultValue": false, "type": "T"}], "isStatic": false, "isVariadic": false}
```

`tests/corpus/complexity/branching.parsing.json`:
```json
{"name": "classify", "kind": "free", "startLine": 3, "endLine": 15, "parameters": [{"name": "value", "passing": "value", "hasDefaultValue": false, "type": "int"}, {"name": "strict", "passing": "value", "hasDefaultValue": false, "type": "bool"}], "isStatic": false, "isVariadic": false},
{"name": "scan", "kind": "free", "startLine": 17, "endLine": 37, "parameters": [{"name": "values", "passing": "pointer", "hasDefaultValue": false, "type": "const int"}, {"name": "count", "passing": "value", "hasDefaultValue": false, "type": "int"}], "isStatic": false, "isVariadic": false},
{"name": "describe", "kind": "free", "startLine": 39, "endLine": 50, "parameters": [{"name": "code", "passing": "value", "hasDefaultValue": false, "type": "int"}], "isStatic": false, "isVariadic": false},
{"name": "drain", "kind": "free", "startLine": 52, "endLine": 60, "parameters": [{"name": "remaining", "passing": "value", "hasDefaultValue": false, "type": "int"}], "isStatic": false, "isVariadic": false},
{"name": "guarded", "kind": "free", "startLine": 62, "endLine": 77, "parameters": [{"name": "value", "passing": "value", "hasDefaultValue": false, "type": "int"}], "isStatic": false, "isVariadic": false}
```

`tests/corpus/classes/c-types.parsing.json` — `pointSize(void)`:
```json
"functions": [
    {"name": "pointSize", "kind": "free", "startLine": 26, "endLine": 29, "parameters": [{"name": "", "passing": "value", "hasDefaultValue": false, "type": "void"}], "isStatic": false, "isVariadic": false}
  ],
```

`tests/corpus/classes/cpp-types.parsing.json`:
```json
{"name": "internalHelper", "kind": "free", "startLine": 9, "endLine": 12, "parameters": [], "isStatic": false, "isVariadic": false},
{"name": "position", "kind": "method", "startLine": 33, "endLine": 36, "parameters": [], "isStatic": false, "isVariadic": false},
{"name": "size", "kind": "method", "startLine": 42, "endLine": 45, "parameters": [], "isStatic": false, "isVariadic": false},
{"name": "Box", "kind": "constructor", "startLine": 55, "endLine": 58, "parameters": [{"name": "content", "passing": "value", "hasDefaultValue": false, "type": "T"}], "isStatic": false, "isVariadic": false}
```

- [ ] **Step 4: Verify the corpus is green again**

Run:

```text
cmake --build build --config Debug
build/tests/Debug/codeqa_tests.exe --test-case="every corpus fixture matches its structural expectation"
ctest --test-dir build -C Debug --output-on-failure
```

- [ ] **Step 5: Require `type` in `CheckCorpusIntegrity.cmake`**

In `tests/corpus/CheckCorpusIntegrity.cmake`, after the existing "Check 2: required top-level fields are present" block (after its closing `endforeach()`, around line 94), add a new check that every `"passing"` occurrence has a matching `"type"` occurrence — cheap enough for the existing regex-only approach (no JSON parsing available at this level), and precise enough because every parameter object has exactly one `"passing"` key:

```cmake
# --- Check 2b: every parameter that declares "passing" also declares "type" -
# Fase 6a requires an exact "type" (declaredType) per parameter so the C++
# runner can compare it exhaustively; a regex-only presence count is enough
# to catch a fixture where a parameter object was extended with "passing" but
# never got "type" added alongside it.

foreach(expectation IN LISTS expectationFiles)
    file(READ "${expectation}" expectationText)

    string(REGEX MATCHALL "\"passing\"[ \t\r\n]*:" passingMatches "${expectationText}")
    string(REGEX MATCHALL "\"type\"[ \t\r\n]*:" typeMatches "${expectationText}")
    list(LENGTH passingMatches passingCount)
    list(LENGTH typeMatches typeCount)

    if(NOT passingCount EQUAL typeCount)
        list(APPEND failures
            "Expectation '${expectation}' has ${passingCount} parameter(s) with 'passing' but ${typeCount} with 'type'.")
    endif()
endforeach()
```

- [ ] **Step 6: Update `tests/corpus/README.md`**

In the schema example (around line 34-43), add `"type": "int"` to the sample parameter object:

```json
      "parameters": [
        {
          "name": "left",
          "passing": "value",
          "hasDefaultValue": false,
          "type": "int"
        }
      ],
```

In the prose right after it (around line 62-64, "Cada parámetro conserva su posición..."), append a sentence:

```markdown
Cada parámetro también declara `type`, el texto fuente de su tipo declarado
sin los marcadores del declarador (`*`, `&`), que `passing` ya identifica.
```

Add a new section right after "## Límites de Fase 5" (after its last paragraph, before "## Integridad del corpus"):

```markdown
## Límites de Fase 6a

`CPP-PARAM-01` y `CPP-PARAM-02` tienen ahora aserción exacta de `type` en
`functions/cpp-functions.parsing.json`, `parameters/cpp-parameters.parsing.json`
y `mixed/mixed-constructs.parsing.json`. `CPP-TYPE-03` se cierra en el mismo
trabajo porque dependía de la misma ampliación de esquema.

`C-PP-01`, `C-DECL-01`, `C-TYPE-04`, `C-FUNC-03`, `C-FUNC-04` y `C-FUNC-05`
siguen sin aserción exacta: cubren typedefs, variables globales, storage
class, la función `static` y la elipsis variádica, ninguno de los cuales
tiene relación con parámetros, comentarios o sentencias. Cerrarlos no aporta
nada a las métricas de la Fase 6b, así que quedan deliberadamente abiertos.
```

- [ ] **Step 7: Full verification**

Run:

```text
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
build/Debug/codeqa.exe parse tests/corpus
```

Expected: `ctest` all-green; `codeqa parse tests/corpus` still prints the `13/4/9/2` summary from before this phase.

- [ ] **Step 8: Commit**

```text
git add tests/unit/CorpusStructureTest.cpp tests/corpus/CheckCorpusIntegrity.cmake tests/corpus/README.md tests/corpus/parsing/minimal.parsing.json tests/corpus/parsing/minimal-cpp.parsing.json tests/corpus/parsing/class-header.parsing.json tests/corpus/parameters/cpp-parameters.parsing.json tests/corpus/nesting/deep-nesting.parsing.json tests/corpus/mixed/mixed-constructs.parsing.json tests/corpus/malformed/unclosed-brace.parsing.json tests/corpus/malformed/truncated.parsing.json tests/corpus/functions/c-functions.parsing.json tests/corpus/functions/cpp-functions.parsing.json tests/corpus/complexity/branching.parsing.json tests/corpus/classes/c-types.parsing.json tests/corpus/classes/cpp-types.parsing.json
git commit -m "test: assert exact parameter types across the corpus"
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

Expected: every test passes, including the corpus suite and the integrity check.

- [ ] **Step 3: Confirm `codeqa parse` behavior is unchanged**

Run:

```text
build/Debug/codeqa.exe parse tests/corpus
```

Expected: identical output to before this phase (files parsed / C / C++ / parse errors counts unchanged; no new CLI output was added by this phase).

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

Expected: no output (Tasks 1-6 already committed everything).
