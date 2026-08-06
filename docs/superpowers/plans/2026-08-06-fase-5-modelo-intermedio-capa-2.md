# Fase 5, capa 2 — cuerpos de función Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Completar el modelo común con variables locales, control de flujo y llamadas sintácticas por función, sin resolver símbolos ni cambiar el CLI.

**Architecture:** `StructuralExtractor` amplía cada `frontend::FunctionModel` que tenga cuerpo fiable; el `ProjectModelBuilder` adapta esos datos a colecciones tipadas de `model::CallableModel`. La extracción usa los propietarios ya identificados por las queries de funciones y recorre su cuerpo CST, excluyendo cuerpos de lambdas anidadas para no duplicar datos.

**Tech Stack:** C++17, CMake, doctest y tree-sitter C/C++ vendorizado.

## Global Constraints

- Solo `codeqa::frontend` puede conocer tree-sitter; los headers públicos de `model` no incluyen ni declaran sus tipos.
- Reutilizar `core::SourceRange`, `core::Diagnostic` y `core::Language`; C++17, sin extensiones ni dependencias nuevas.
- La información es sintáctica: no hay resolución de símbolos o tipos, CFG, AST genérico, texto de cuerpos, métricas, reglas ni informes.
- Los parámetros siguen exclusivamente en `CallableModel::parameters`; no se duplican como locales.
- Las locales tienen nombre, tipo textual, rango, inicializador y profundidad léxica; `for` con declaración introduce un nivel léxico adicional.
- El flujo se limita a `If`, `Switch`, `For`, `While`, `DoWhile`, `Case`, `Default`, `Conditional`, `Return`, `Break` y `Continue`; `goto`, `throw`, `try`/`catch`, corutinas, macros y preprocesado quedan fuera.
- Las llamadas son `Named`, `Member` o `Indirect`. Una llamada `callback()` es `Named`; sin semántica no se conoce que sea un callback. Solo una expresión invocable sin nombre, como la llamada exterior de `factory()()`, es `Indirect`.
- Cada colección de cuerpo se ordena por rango inicio/fin con empates estables. Un candidato cuyo subárbol contenga `ERROR` o `MISSING` se omite; el archivo conserva diagnóstico y estado parcial.
- El cuerpo de una lambda pertenece solo a su callable lambda, nunca a la función envolvente.
- `codeqa parse` conserva su salida y el corpus sigue resumiendo `13/4/9/2`.

## File Structure

- `include/codeqa/frontend/StructuralModels.h`: contratos sintácticos de cuerpo y vectores de `FunctionModel`.
- `include/codeqa/model/Models.h`: contratos públicos comunes y vectores de `CallableModel`.
- `src/frontend/StructuralExtractor.cpp`: recorrido CST por cuerpo, clasificación y tolerancia a errores.
- `src/model/ProjectModelBuilder.cpp`: adaptación total y ordenada hacia el modelo común.
- `tests/unit/StructuralModelsTest.cpp`, `ModelTypesTest.cpp`: contratos de datos.
- `tests/unit/ModelPublicApiTest.cpp`: límite que impide filtrar tipos de
  tree-sitter por headers públicos de `model`.
- `tests/unit/StructuralExtractorTest.cpp`, `ProjectModelBuilderTest.cpp`: extracción y adaptación aisladas.
- `tests/unit/AnalyzeProjectTest.cpp`, `CorpusModelTest.cpp`: flujo real y regresión de corpus.
- `tests/corpus/README.md`, `tests/corpus/SUPPORTED-CONSTRUCTS.md`: frontera documentada de la capa 2.

---

### Task 1: Define typed body-model contracts

**Files:**
- Modify: `include/codeqa/frontend/StructuralModels.h`
- Modify: `include/codeqa/model/Models.h`
- Modify: `tests/unit/StructuralModelsTest.cpp`
- Modify: `tests/unit/ModelTypesTest.cpp`
- Create: `tests/unit/ModelPublicApiTest.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**

```cpp
enum class ControlFlowKind
{
    If, Switch, For, While, DoWhile, Case, Default, Conditional, Return, Break, Continue
};

enum class CallKind { Named, Member, Indirect };

struct LocalVariableModel
{
    std::string name; // model version uses SymbolIdentity symbol instead
    std::string declaredType;
    core::SourceRange range;
    bool hasInitializer{false};
    std::size_t lexicalDepth{0};
};

struct ControlFlowModel
{
    ControlFlowKind kind{ControlFlowKind::If};
    core::SourceRange range;
    std::size_t nestingDepth{0};
    std::optional<core::SourceRange> conditionRange;
};

struct CallModel
{
    std::string calleeText;
    CallKind kind{CallKind::Named};
    std::size_t argumentCount{0};
    core::SourceRange range;
};
```

`frontend::FunctionModel` and `model::CallableModel` each own vectors named
`localVariables`, `controlFlow`, and `calls`. The common local model replaces
`name` and `range` with `SymbolIdentity symbol`; all remaining fields retain
the names above. Add `<cstddef>` and `<optional>` where the declarations need
them.

- [ ] **Step 1: Write failing frontend and common-model contract tests**

Extend `StructuralModelsTest.cpp` and `ModelTypesTest.cpp` with tests that
construct each new aggregate and append it to a callable. Assert default
`CallKind::Named`, empty optional condition range, vector ownership and that
the common local identity survives:

```cpp
codeqa::model::CallableModel callable;
callable.localVariables.push_back(codeqa::model::LocalVariableModel{
    {"index", "index", {}}, "int", true, 1});
callable.controlFlow.push_back(codeqa::model::ControlFlowModel{
    codeqa::model::ControlFlowKind::For, {}, 0, {}});
callable.calls.push_back(codeqa::model::CallModel{"callback", codeqa::model::CallKind::Named, 1, {}});

CHECK(callable.localVariables[0].symbol.name == "index");
CHECK_FALSE(callable.controlFlow[0].conditionRange.has_value());
CHECK(callable.calls[0].kind == codeqa::model::CallKind::Named);
```

In `ModelPublicApiTest.cpp`, declare incompatible stand-ins before including
the public model header, then compile a trivial model value:

```cpp
using TSNode = int;
using TSParser = int;
using TSTree = int;

#include "doctest/doctest.h"
#include "codeqa/model/Models.h"
#include "codeqa/model/ProjectModelBuilder.h"

TEST_CASE("the public model API is independent of tree-sitter type names")
{
    codeqa::model::CallableModel callable;
    codeqa::model::ProjectModelBuilder builder;
    CHECK(callable.localVariables.empty());
    CHECK(sizeof(builder) > 0);
}
```

- [ ] **Step 2: Run the focused tests to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe --test-case="structural models retain the attributes required by the corpus,common model preserves typed file aggregates"
```

Expected: compilation fails because the body-model types and callable vectors
do not yet exist.

- [ ] **Step 3: Add the frontend and common contracts**

Define the enums and aggregates above in their respective namespaces. Add
three vectors at the end of both function/callable aggregates so existing
aggregate initializers retain their current field order. Use `SymbolIdentity`
for `model::LocalVariableModel`; do not add a new generic symbol hierarchy.
Register `ModelPublicApiTest.cpp` in the existing `codeqa_tests` source list.

- [ ] **Step 4: Verify the contracts**

Run:

```text
cmake --build build --config Debug
build/tests/Debug/codeqa_tests.exe --test-case="structural models retain the attributes required by the corpus,common model preserves typed file aggregates"
ctest --test-dir build -C Debug --output-on-failure
```

- [ ] **Step 5: Commit the contract layer**

```text
git add include/codeqa/frontend/StructuralModels.h include/codeqa/model/Models.h tests/CMakeLists.txt tests/unit/StructuralModelsTest.cpp tests/unit/ModelTypesTest.cpp tests/unit/ModelPublicApiTest.cpp
git commit -m "feat: add typed function body model contracts"
```

---

### Task 2: Extract local variables and control flow per reliable body

**Files:**
- Modify: `src/frontend/StructuralExtractor.cpp`
- Modify: `tests/unit/StructuralExtractorTest.cpp`

**Interfaces:**
- Consumes the new `frontend::FunctionModel::localVariables` and
  `controlFlow` fields from Task 1.
- Produces source-ordered values for every function definition and lambda with
  a body; prototypes retain empty body collections.

- [ ] **Step 1: Write failing extraction tests**

Add a C fixture with this body and assert the locals and all eleven flow kinds:

```c
int helper(int value) { return value; }
int scan(int count) {
    int total = 0;
    for (int index = 0; index < count; ++index) {
        if (index ? 1 : 0) {
            while (total < 10) { do { ++total; } while (total < 5); break; }
            continue;
        }
    }
    switch (total) { case 0: return helper(total); default: return total; }
}
```

Require `total` at lexical depth 0, `index` at depth 1, `For` before `If`,
an engaged condition range for `If`, and no condition range for `Return`.

Add a C++ lambda fixture that declares `outer` in the enclosing function and
`inner` inside a lambda. Require `inner` and the lambda's flow nodes only on
the lambda callable. Add a malformed file with one valid function and one body
containing `int value = ;`; require recovered data on the valid function,
empty body collections for the unreliable candidate, and preserved parsing
diagnostics.

- [ ] **Step 2: Run the focused extraction tests to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe --test-case="extracts function body locals and control flow*,keeps lambda body data out of the enclosing callable*,recovers reliable body data from partial files*"
```

Expected: the new vectors are empty because no body traversal exists.

- [ ] **Step 3: Implement reliable body traversal**

Use the existing function-query matches as owners; obtain each definition's
`body` field with `childForField`, and obtain the lambda compound body in the
same way. Do not add a project-wide body query or a second parser.

Add internal helpers with these responsibilities:

```cpp
bool subtreeContainsErrorOrMissing(TSNode node);
void extractBodyStructure(
    TSNode body, std::string_view source, const std::filesystem::path& path,
    FunctionModel& function);
```

`subtreeContainsErrorOrMissing` recursively checks the candidate and all its
children with `ts_node_is_error` and `ts_node_is_missing`. The traversal skips
a nested `lambda_expression` subtree after the enclosing declaration has been
recorded, because the existing lambda collection adapts that body separately.

Reuse `declarationDeclarators`, `declarationType`, `declaratorName`,
`hasInitializer`, and `sourceRange` for non-typedef declarations beneath a
function body. A callable body's direct block is depth 0; a nested compound
block increments depth; a declaration in a `for` initializer uses the current
block depth plus one. Ignore function declarations and candidates with an
error/missing descendant.

Map these CST types to `ControlFlowKind`:

```text
if_statement -> If                 switch_statement -> Switch
for_statement/range_based_for_statement -> For
while_statement -> While           do_statement -> DoWhile
case_statement -> Case             default_statement -> Default
conditional_expression -> Conditional
return_statement -> Return         break_statement -> Break
continue_statement -> Continue
```

Capture an optional range from the node's `condition` field when it exists.
Set `nestingDepth` to the number of enclosing supported flow nodes in the same
callable. Skip `try`, `catch`, `throw`, `goto` and all unrelated node kinds.
Stable-sort both produced vectors by range after traversal.

- [ ] **Step 4: Verify extraction, query construction and regression suite**

Run:

```text
cmake -S . -B build
cmake --build build --config Debug
build/tests/Debug/codeqa_tests.exe --test-case="all embedded structural queries compile,extracts function body locals and control flow*,keeps lambda body data out of the enclosing callable*,recovers reliable body data from partial files*,extracts functions*"
ctest --test-dir build -C Debug --output-on-failure
```

- [ ] **Step 5: Commit local and flow extraction**

```text
git add src/frontend/StructuralExtractor.cpp tests/unit/StructuralExtractorTest.cpp
git commit -m "feat: extract locals and control flow per function"
```

---

### Task 3: Extract syntactic calls without symbol resolution

**Files:**
- Modify: `src/frontend/StructuralExtractor.cpp`
- Modify: `tests/unit/StructuralExtractorTest.cpp`

**Interfaces:**
- Consumes `frontend::CallKind` and `FunctionModel::calls` from Task 1 and
  extends the body traversal from Task 2.
- Produces source-ordered calls with callee spelling, syntactic form, argument
  count and range, owned by exactly one callable.

- [ ] **Step 1: Write failing call and ownership tests**

Add a C++ fixture containing a named helper, a member call, a function-pointer
parameter named `callback`, and an outer indirect call:

```cpp
struct Sink { int emit(int value); };
int helper(int value) { return value; }
Sink factory();
using Callback = int (*)(int);
Callback callbackFactory();
int run(Sink* sink, int (*callback)(int), int value) {
    int first = helper(value);
    int second = sink->emit(value);
    int third = callback(value);
    int fourth = callbackFactory()(value);
    return factory().emit(first + second + third) + fourth;
}
```

Require `helper` and `callback` to be `Named`, `sink->emit` and
`factory().emit` to be `Member`, and the nested `factory()` call to be
`Named`. Require `callbackFactory()` to be `Named` and its outer invocation to
be `Indirect`. Check argument counts and range order.

Extend the lambda fixture so `helper` is invoked both inside and outside the
lambda; require each callable to own only its own call list.

- [ ] **Step 2: Run the focused tests to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe --test-case="extracts syntactic calls without resolving symbols*,keeps lambda calls out of the enclosing callable*"
```

Expected: call collections are empty.

- [ ] **Step 3: Extend the body traversal for calls**

For each reliable `call_expression` outside a nested lambda, read its
`function` and `arguments` fields. Preserve `nodeText(function, source)` as
`calleeText`, use the call expression itself for the range, and count named
children in `argument_list` for `argumentCount`.

Classify only by CST shape:

```text
identifier or qualified_identifier -> Named
field_expression                -> Member
every other function expression -> Indirect
```

Never inspect parameter types or file-wide declarations to alter that result.
Ignore a call expression with an error/missing descendant. Stable-sort
`FunctionModel::calls` by range after extraction.

- [ ] **Step 4: Verify call extraction and full suite**

Run:

```text
cmake --build build --config Debug
build/tests/Debug/codeqa_tests.exe --test-case="extracts syntactic calls without resolving symbols*,keeps lambda calls out of the enclosing callable*,extracts function body locals and control flow*"
ctest --test-dir build -C Debug --output-on-failure
```

- [ ] **Step 5: Commit call extraction**

```text
git add src/frontend/StructuralExtractor.cpp tests/unit/StructuralExtractorTest.cpp
git commit -m "feat: extract syntactic calls per function"
```

---

### Task 4: Adapt function body data into the common model

**Files:**
- Modify: `src/model/ProjectModelBuilder.cpp`
- Modify: `tests/unit/ProjectModelBuilderTest.cpp`

**Interfaces:**
- Consumes all frontend body contracts from Tasks 1–3.
- Produces `CallableModel::localVariables`, `controlFlow` and `calls` using
  explicit enum mapping and stable range ordering.

- [ ] **Step 1: Write failing builder adaptation tests**

Extend the parsed C++ fixture in `ProjectModelBuilderTest.cpp` with deliberately
reverse-ordered local, flow and call values, including an equal-range pair.
Assert their mapped common kinds, `SymbolIdentity` for the local, optional
condition range, `Named` call kind and stable order:

```cpp
sourceFile.functions[0].localVariables = {
    {"late", "int", rangeFor(sourcePath, 90, 91), false, 1},
    {"first", "int", rangeFor(sourcePath, 10, 11), true, 0},
    {"second", "int", rangeFor(sourcePath, 10, 11), false, 0}};
sourceFile.functions[0].calls = {
    {"lateCall", codeqa::frontend::CallKind::Named, 0, rangeFor(sourcePath, 90, 91)},
    {"firstCall", codeqa::frontend::CallKind::Member, 2, rangeFor(sourcePath, 10, 11)},
    {"secondCall", codeqa::frontend::CallKind::Indirect, 1, rangeFor(sourcePath, 10, 11)}};
```

Require the mapped callable to order `first`, `second`, `late` and to preserve
the equal-range insertion order.

- [ ] **Step 2: Run the focused builder tests to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe --test-case="builds deterministic common project model from parsed frontend files,sorts every model collection by source range with stable ties"
```

Expected: compilation fails because the frontend/common body contracts are not
yet mapped, or the new callable collections remain empty.

- [ ] **Step 3: Implement explicit mappings and nested ordering**

Add total `switch` functions for `frontend::ControlFlowKind` and
`frontend::CallKind`. In the existing function loop, append model values using:

```cpp
LocalVariableModel{identityFor(local.name, local.range, file.classes),
                   local.declaredType, local.hasInitializer, local.lexicalDepth}
ControlFlowModel{controlFlowKindFor(flow.kind), flow.range,
                 flow.nestingDepth, flow.conditionRange}
CallModel{call.calleeText, callKindFor(call.kind), call.argumentCount, call.range}
```

Use `sortByRange` for each nested vector after mapping. Preserve the existing
file state logic: unavailable files still expose no callables or body data;
partial parsed files retain recovered callable body data.

- [ ] **Step 4: Verify builder behavior and all tests**

Run:

```text
cmake --build build --config Debug
build/tests/Debug/codeqa_tests.exe --test-case="builds deterministic common project model from parsed frontend files,sorts every model collection by source range with stable ties,preserves qualified callable names*"
ctest --test-dir build -C Debug --output-on-failure
```

- [ ] **Step 5: Commit common-model adaptation**

```text
git add src/model/ProjectModelBuilder.cpp tests/unit/ProjectModelBuilderTest.cpp
git commit -m "feat: adapt function body data to common model"
```

---

### Task 5: Validate the full service and document the completed Fase 5 boundary

**Files:**
- Modify: `tests/unit/AnalyzeProjectTest.cpp`
- Modify: `tests/unit/CorpusModelTest.cpp`
- Modify: `tests/corpus/README.md`
- Modify: `tests/corpus/SUPPORTED-CONSTRUCTS.md`

**Interfaces:**
- Consumes `AnalyzeProject::execute`, which remains unchanged and now returns
  callables populated by Task 4.
- Produces end-to-end evidence that body data survives discovery, parsing and
  adaptation while the existing CLI output stays unchanged.

- [ ] **Step 1: Write failing integration and corpus assertions**

Add a temporary-project test with one clean C++ source containing a local,
`if`, member call and lambda, plus one malformed source with a recoverable
valid function. Execute `AnalyzeProject` and require body data only on reliable
callables, `Partial` plus diagnostics for the malformed file, and no symbol
resolution claim for `callback()`.

Extend `CorpusModelTest.cpp` to inspect `complexity/branching.cpp` and
`nesting/deep-nesting.cpp`. Require `scan` to expose `For`, `If`, `Continue`
and `Break`; require `describe` to expose `Switch`, `Case` and `Default`; and
require `fourLevels` to contain four nested supported flow nodes with depths
0, 1, 2 and 3. Run the service twice and compare each callable's local, flow
and call sequences for deterministic order.

- [ ] **Step 2: Run the focused tests to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe --test-case="analyzes a project into the common model,corpus adapts to the common model"
```

Expected: the new body-model assertions fail until Tasks 2–4 are complete.

- [ ] **Step 3: Document the boundary without changing CLI expectations**

In `tests/corpus/README.md`, replace the statement that locals, control flow
and calls remain in layer 2 with a statement that Fase 5 is complete through
typed function-body data. Explicitly retain the exclusions: no call resolution,
CFG, metrics, rules or reports.

In `SUPPORTED-CONSTRUCTS.md`, add a Fase 5 note: `CTRL-01` through `CTRL-09`
are represented by the typed body model; `CTRL-07` is available through a
condition range rather than its own flow node; `CTRL-10` and `CTRL-11` remain
outside this layer. Do not alter any `.parsing.json` expectation.

- [ ] **Step 4: Run end-to-end validation**

Run:

```text
cmake -S . -B build
cmake --build build --config Debug
build/tests/Debug/codeqa_tests.exe --test-case="analyzes a project into the common model,corpus adapts to the common model"
ctest --test-dir build -C Debug --output-on-failure
build/Debug/codeqa.exe parse tests/corpus
git diff --check
```

Expected CLI summary:

```text
Files parsed: 13
C files: 4
C++ files: 9
Parse errors: 2
```

- [ ] **Step 5: Commit end-to-end coverage and documentation**

```text
git add tests/unit/AnalyzeProjectTest.cpp tests/unit/CorpusModelTest.cpp tests/corpus/README.md tests/corpus/SUPPORTED-CONSTRUCTS.md
git commit -m "test: validate Fase 5 function body model"
```
