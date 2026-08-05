# Fase 0 — Corpus y criterios de validación Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the validation corpus (`tests/corpus/`) — an explicit, ID-addressed list of the C/C++ constructs the MVP supports, fixture sources covering all of them, per-fixture parsing expectations in JSON, documented public validation projects, and a CMake-based integrity check that keeps the corpus self-consistent.

**Architecture:** Pure data and documentation — no production C++ code is written or modified in this phase. Each fixture is a source file plus a sibling `<name>.parsing.json` declaring which construct IDs it exercises and what the Fase 4 frontend must extract from it. A standalone CMake script (`tests/corpus/CheckCorpusIntegrity.cmake`), wired as a CTest test, cross-references construct IDs between `SUPPORTED-CONSTRUCTS.md` and every `.parsing.json`, and verifies every fixture has its expectation file. Metric values (`*.metrics.json`) are deliberately absent — they arrive in Fase 6.

**Tech Stack:** Markdown, JSON (data only — no parser is written or vendored in this phase), CMake scripting, CTest.

## Global Constraints

Carried forward from Fases 1–3 (still binding):

- No new external dependencies of any kind. **This phase writes no C++ code and vendors nothing** — the JSON files are data; the library that reads them (`nlohmann/json`) is vendored in Fase 4, not here.
- Use `${PROJECT_SOURCE_DIR}` / `${PROJECT_BINARY_DIR}` in CMake, not `CMAKE_SOURCE_DIR` / `CMAKE_BINARY_DIR`.
- CMake minimum version 3.16 — the integrity script must not use features newer than that (notably: **do not use `string(JSON ...)`**, which requires 3.19).

New, from `docs/superpowers/specs/2026-08-05-fase-0-corpus-validacion-design.md` (read it if a task feels ambiguous):

- **No production code changes.** `src/`, `include/`, and `tests/unit/` are not touched. `tests/integration/fixtures/discover-sample/` is not touched.
- **The corpus lives in `tests/corpus/`**, deliberately separate from `tests/integration/fixtures/`.
- **Every fixture source has a sibling `<name>.parsing.json`.** No orphans in either direction.
- **`constructs` is mandatory and non-empty** in every `.parsing.json`, and every ID it names must exist in `SUPPORTED-CONSTRUCTS.md`.
- **Lines are 1-based.** `startLine` is the line where the declaration begins (the return type, or the `template<...>` line if present), **not** the opening brace. `endLine` is the line of the closing brace, inclusive. For a prototype with no body, `startLine == endLine`.
- **Unnamed parameters** (`void f(int)`) are represented as the empty string `""`, preserving position. A function with no parameters has `"parameters": []`.
- **A fixture with no errors carries an explicit `"diagnostics": []`** — never an absent field.
- **`diagnostics` entries carry only `line` and `severity`, never the message text** (the text comes from tree-sitter and may change between versions).
- **No `*.metrics.json` files are created in this phase.** Writing metric values before Fase 6 defines the formulas would be inventing the answer before the rule.
- **`OUT-*` IDs are exclusions**, documented so they can be referenced. They are never listed in a fixture's `constructs`, and the coverage check must skip them.

Two additional format rules this plan fixes, because the spec's examples do not cover them:

- **The ellipsis of a variadic function is not a parameter.** `int sumAll(int count, ...)` has `"parameters": ["count"]`.
- **A lambda is recorded in `functions` with the name `"<lambda>"`**, since lambdas are unnamed. If a fixture contains more than one, they are distinguished by their line ranges.

---

## File Structure

```text
tests/corpus/
├── SUPPORTED-CONSTRUCTS.md          The contract: supported + out-of-scope IDs
├── REAL-PROJECTS.md                 Public validation projects (SQLite, fmt)
├── README.md                        How the corpus works; expectation format
├── CheckCorpusIntegrity.cmake       Integrity + coverage checker (CTest script)
├── parsing/
│   ├── minimal.c            + minimal.parsing.json
│   └── minimal-cpp.cpp      + minimal-cpp.parsing.json
├── functions/
│   ├── c-functions.c        + c-functions.parsing.json
│   └── cpp-functions.cpp    + cpp-functions.parsing.json
├── classes/
│   ├── c-types.c            + c-types.parsing.json
│   └── cpp-types.cpp        + cpp-types.parsing.json
├── parameters/
│   └── cpp-parameters.cpp   + cpp-parameters.parsing.json
├── complexity/
│   └── branching.cpp        + branching.parsing.json
├── nesting/
│   └── deep-nesting.cpp     + deep-nesting.parsing.json
├── malformed/
│   ├── unclosed-brace.cpp   + unclosed-brace.parsing.json
│   └── truncated.c          + truncated.parsing.json
└── mixed/
    └── mixed-constructs.cpp + mixed-constructs.parsing.json

tests/CMakeLists.txt                 (modified: register the corpus integrity test)
```

**Note on file naming:** the expectation file replaces the source's extension — `minimal.c` pairs with `minimal.parsing.json`, `cpp-functions.cpp` with `cpp-functions.parsing.json`. Because `parsing/` holds both a `.c` and a `.cpp` fixture that would otherwise collide on the same stem, the C++ one is named `minimal-cpp.cpp`.

---

### Task 1: Corpus scaffolding, construct list, and the integrity checker

**Files:**
- Create: `tests/corpus/SUPPORTED-CONSTRUCTS.md`
- Create: `tests/corpus/README.md`
- Create: `tests/corpus/CheckCorpusIntegrity.cmake`
- Create: `tests/corpus/parsing/minimal.c`
- Create: `tests/corpus/parsing/minimal.parsing.json`
- Create: `tests/corpus/parsing/minimal-cpp.cpp`
- Create: `tests/corpus/parsing/minimal-cpp.parsing.json`
- Modify: `tests/CMakeLists.txt` (register `codeqa_corpus_integrity`)

**Interfaces:**
- Consumes: nothing from earlier phases.
- Produces: `tests/corpus/SUPPORTED-CONSTRUCTS.md` as the single source of truth for construct IDs (Tasks 2–6 reference IDs from it and must not invent new ones); `CheckCorpusIntegrity.cmake` as the gate every later task runs; the two `parsing/` fixtures as the worked example of the file format.

- [ ] **Step 1: Write the construct list**

`tests/corpus/SUPPORTED-CONSTRUCTS.md`:

````markdown
# Construcciones soportadas por el MVP

Este documento es el contrato que define "terminado" para el frontend (Fase 4).
Cada construcción tiene un identificador estable. Cada fixture del corpus declara,
en el campo `constructs` de su `.parsing.json`, qué identificadores ejercita.

Si el soporte de una construcción cambia, `grep` de su ID indica exactamente qué
fixtures hay que revisar.

## C

| ID | Construcción |
|---|---|
| `C-FUNC-01` | Definición de función libre |
| `C-FUNC-02` | Declaración de función (prototipo sin cuerpo) |
| `C-FUNC-03` | Función `static` |
| `C-FUNC-04` | Función variádica (`...`) |
| `C-FUNC-05` | Puntero a función como parámetro |
| `C-TYPE-01` | `struct` |
| `C-TYPE-02` | `union` |
| `C-TYPE-03` | `enum` |
| `C-TYPE-04` | `typedef` |
| `C-DECL-01` | Variable global |
| `C-PP-01` | `#include` (con `<>` y con `""`) |

## C++ (además de todo lo anterior)

| ID | Construcción |
|---|---|
| `CPP-FUNC-01` | Función libre |
| `CPP-FUNC-02` | Método definido dentro de la clase |
| `CPP-FUNC-03` | Método definido fuera de la clase (`Foo::bar()`) |
| `CPP-FUNC-04` | Constructor |
| `CPP-FUNC-05` | Destructor |
| `CPP-FUNC-06` | Sobrecarga de operador |
| `CPP-FUNC-07` | Lambda con cuerpo |
| `CPP-FUNC-08` | Plantilla de función (declarada, no instanciada) |
| `CPP-TYPE-01` | `class` |
| `CPP-TYPE-02` | `struct` |
| `CPP-TYPE-03` | `enum` / `enum class` |
| `CPP-TYPE-04` | Plantilla de clase (declarada, no instanciada) |
| `CPP-TYPE-05` | Clase anidada |
| `CPP-NS-01` | `namespace` |
| `CPP-NS-02` | `namespace` anónimo |
| `CPP-PARAM-01` | Valores de parámetro por defecto |
| `CPP-PARAM-02` | Parámetros por referencia y por puntero |

## Control de flujo (común a C y C++)

Estas construcciones no las usa la Fase 4 para extraer estructura, pero son
exactamente lo que mide la Fase 6 (complejidad ciclomática, ramas, anidamiento,
retornos). Se identifican para que los fixtures de `complexity/` y `nesting/`
puedan declarar qué ejercitan.

Que un `CTRL-*` esté listado significa que el corpus lo cubre y que la Fase 6
debe decidir explícitamente cómo lo cuenta. **No** implica que sume a la
complejidad ciclomática: esa decisión pertenece a la Fase 6.

| ID | Construcción |
|---|---|
| `CTRL-01` | `if` / `else if` / `else` |
| `CTRL-02` | `while` |
| `CTRL-03` | `for` (y `for` basado en rango, solo C++) |
| `CTRL-04` | `do`-`while` |
| `CTRL-05` | `switch` / `case` |
| `CTRL-06` | Operador ternario (`?:`) |
| `CTRL-07` | Operadores de cortocircuito (`&&`, `\|\|`) |
| `CTRL-08` | `return` (incluido el caso de retornos múltiples) |
| `CTRL-09` | `break` / `continue` |
| `CTRL-10` | `try` / `catch` (solo C++) |
| `CTRL-11` | `goto` |

## Fuera de alcance

Estas construcciones están explícitamente **excluidas** del MVP. Se identifican
para poder referenciarlas: que el frontend no las reconozca es una decisión
documentada, no un defecto.

Los IDs `OUT-*` nunca aparecen en el campo `constructs` de un fixture.

| ID | Construcción excluida |
|---|---|
| `OUT-01` | Instanciación de plantillas |
| `OUT-02` | Overload resolution |
| `OUT-03` | Evaluación del preprocesador (`#ifdef` no se razona; el texto se parsea tal cual) |
| `OUT-04` | Macros que generan funciones |
| `OUT-05` | Semántica de herencia |
| `OUT-06` | Deducción y resolución de tipos |
| `OUT-07` | Declaraciones estilo K&R |
````

- [ ] **Step 2: Write the corpus README**

`tests/corpus/README.md`:

````markdown
# Corpus de validación

Conjunto de datos de prueba del analizador: archivos fuente C/C++ con expectativas
declaradas, usados para verificar que el frontend (Fase 4) y las métricas (Fase 6)
producen los resultados correctos.

No confundir con `tests/integration/fixtures/`, que contiene fixtures de tests
concretos de la CLI. El corpus es el conjunto de validación del analizador.

## Estructura

| Directorio | Contenido |
|---|---|
| `parsing/` | Estructura básica reconocible (casos mínimos) |
| `functions/` | Funciones libres, métodos, constructores, destructores, lambdas, operadores |
| `classes/` | Clases, structs, enums, namespaces, plantillas declaradas |
| `parameters/` | Firmas: cantidad, valores por defecto, referencias, punteros |
| `complexity/` | Fuentes con ramificación conocida |
| `nesting/` | Fuentes con anidamiento conocido |
| `malformed/` | Código incompleto o roto: debe producir diagnósticos |
| `mixed/` | Archivos que combinan varias categorías |

## Formato de expectativas

Cada fixture es un par de archivos:

```text
functions/cpp-functions.cpp
functions/cpp-functions.parsing.json
```

En la Fase 6 se añadirá un tercer archivo por fixture, `<nombre>.metrics.json`,
con los valores esperados de las métricas. **No existe todavía**: escribir hoy
"complejidad ciclomática = 7" exigiría haber decidido antes si `&&` cuenta como
rama o si cada `case` suma, y esas decisiones pertenecen a la Fase 6.

### `<nombre>.parsing.json`

```json
{
  "constructs": ["CPP-FUNC-01", "CPP-FUNC-06"],
  "functions": [
    {
      "name": "add",
      "startLine": 3,
      "endLine": 6,
      "parameters": ["a", "b"]
    }
  ],
  "classes": [],
  "diagnostics": []
}
```

| Campo | Significado |
|---|---|
| `constructs` | IDs de `SUPPORTED-CONSTRUCTS.md` que este fixture ejercita. Obligatorio y no vacío. |
| `functions` | Funciones que el frontend debe encontrar. |
| `classes` | Clases, structs, enums y namespaces que debe encontrar: `name`, `kind`, `startLine`, `endLine`. |
| `diagnostics` | Diagnósticos esperados: solo `line` y `severity`. |

### Reglas

- Las líneas son **1-based**, consistentes con `codeqa::core::SourceLocation`.
- `startLine` es la línea donde empieza la declaración (el tipo de retorno, o la
  línea `template<...>` si la lleva), **no** la de la llave de apertura.
- `endLine` es la línea de la llave de cierre, incluida. Para un prototipo sin
  cuerpo, `startLine == endLine`.
- Un parámetro sin nombre (`void f(int)`) se representa con la cadena vacía `""`,
  preservando su posición. Una función sin parámetros lleva `"parameters": []`.
- El `...` de una función variádica **no** es un parámetro: `int f(int n, ...)`
  lleva `"parameters": ["n"]`.
- Una lambda se registra en `functions` con el nombre `"<lambda>"`. Si hay varias
  en un mismo fixture, se distinguen por su rango de líneas.
- `diagnostics` **nunca** incluye el texto del mensaje: proviene de tree-sitter y
  puede cambiar entre versiones. La línea es lo que debe permanecer estable.
- Un fixture sin errores lleva `"diagnostics": []` explícito. La ausencia de
  diagnósticos es una afirmación deliberada, no un descuido.
- Los valores de `kind` en `classes` son: `class`, `struct`, `union`, `enum`,
  `namespace`.

## Verificación

`CheckCorpusIntegrity.cmake` se ejecuta como test de CTest (`codeqa_corpus_integrity`)
y comprueba, sin necesidad del analizador:

1. Todo fixture fuente tiene su `.parsing.json`, y viceversa.
2. Todo ID citado en un `constructs` existe en `SUPPORTED-CONSTRUCTS.md`.
3. Ningún fixture cita un ID `OUT-*`.
4. Todo ID no-`OUT-*` de `SUPPORTED-CONSTRUCTS.md` está cubierto por al menos un fixture.

La verificación de que el analizador produce realmente estos resultados llega en
la Fase 4, con el runner que lee estos archivos.
````

- [ ] **Step 3: Write the integrity checker (the deliberately-failing version first)**

Write `tests/corpus/CheckCorpusIntegrity.cmake` with checks 1–3 only. Check 4 (full coverage) is added in Task 6, once all fixtures exist — enabling it now would make every intermediate task fail.

```cmake
# Verifies the corpus is internally consistent, without needing the analyzer.
# Run as a CTest test via `cmake -P`. Required variable:
#   CODEQA_CORPUS_DIR - absolute path to tests/corpus
#
# Checks:
#   1. Every fixture source has a sibling .parsing.json, and vice versa.
#   2. Every construct ID cited by a fixture exists in SUPPORTED-CONSTRUCTS.md.
#   3. No fixture cites an OUT-* ID (those are exclusions, never coverage).
# Check 4 (every documented ID is covered by a fixture) is added in Task 6.

if(NOT DEFINED CODEQA_CORPUS_DIR)
    message(FATAL_ERROR "CODEQA_CORPUS_DIR must be defined.")
endif()

# --- Collect the documented IDs -------------------------------------------

# The ID regex must accept both three-segment IDs (C-FUNC-01, CPP-PARAM-02) and
# two-segment ones (CTRL-01, OUT-03). A regex requiring three segments would make
# every CTRL-* and OUT-* ID invisible to this checker on BOTH sides — documented
# and cited — leaving the control-flow constructs silently unverified.
set(CODEQA_ID_PATTERN "[A-Z][A-Z-]*-[0-9][0-9]")

file(READ "${CODEQA_CORPUS_DIR}/SUPPORTED-CONSTRUCTS.md" constructsDocument)
string(REGEX MATCHALL "`${CODEQA_ID_PATTERN}`" documentedIdTokens "${constructsDocument}")

set(documentedIds "")
foreach(token IN LISTS documentedIdTokens)
    string(REPLACE "`" "" identifier "${token}")
    list(APPEND documentedIds "${identifier}")
endforeach()
list(REMOVE_DUPLICATES documentedIds)

list(LENGTH documentedIds documentedIdCount)
if(documentedIdCount EQUAL 0)
    message(FATAL_ERROR "No construct IDs found in SUPPORTED-CONSTRUCTS.md.")
endif()

# --- Check 1: fixture sources and expectation files pair up ---------------

file(GLOB_RECURSE fixtureSources
    "${CODEQA_CORPUS_DIR}/*.c"
    "${CODEQA_CORPUS_DIR}/*.cpp")
file(GLOB_RECURSE expectationFiles "${CODEQA_CORPUS_DIR}/*.parsing.json")

set(failures "")

foreach(source IN LISTS fixtureSources)
    get_filename_component(sourceDirectory "${source}" DIRECTORY)
    get_filename_component(sourceStem "${source}" NAME_WE)
    set(expected "${sourceDirectory}/${sourceStem}.parsing.json")

    if(NOT EXISTS "${expected}")
        list(APPEND failures "Fixture '${source}' has no '${sourceStem}.parsing.json'.")
    endif()
endforeach()

foreach(expectation IN LISTS expectationFiles)
    get_filename_component(expectationDirectory "${expectation}" DIRECTORY)
    get_filename_component(expectationName "${expectation}" NAME)
    string(REPLACE ".parsing.json" "" expectationStem "${expectationName}")

    if(NOT EXISTS "${expectationDirectory}/${expectationStem}.c"
        AND NOT EXISTS "${expectationDirectory}/${expectationStem}.cpp")
        list(APPEND failures "Expectation '${expectation}' has no matching .c or .cpp fixture.")
    endif()
endforeach()

# --- Checks 2 and 3: cited IDs exist, and are never OUT-* -----------------

set(citedIds "")

foreach(expectation IN LISTS expectationFiles)
    file(READ "${expectation}" expectationText)

    # The constructs array is the first [...] block after the "constructs" key.
    string(REGEX MATCH "\"constructs\"[ \t\r\n]*:[ \t\r\n]*\\[([^]]*)\\]"
        constructsMatch "${expectationText}")

    if(NOT constructsMatch)
        list(APPEND failures "Expectation '${expectation}' has no 'constructs' field.")
        continue()
    endif()

    string(REGEX MATCHALL "${CODEQA_ID_PATTERN}" fixtureIds "${CMAKE_MATCH_1}")

    if(NOT fixtureIds)
        list(APPEND failures "Expectation '${expectation}' has an empty 'constructs' field.")
        continue()
    endif()

    foreach(identifier IN LISTS fixtureIds)
        if(identifier MATCHES "^OUT-")
            list(APPEND failures
                "Expectation '${expectation}' cites '${identifier}', but OUT-* IDs are exclusions.")
        elseif(NOT identifier IN_LIST documentedIds)
            list(APPEND failures
                "Expectation '${expectation}' cites '${identifier}', which is not in SUPPORTED-CONSTRUCTS.md.")
        endif()

        list(APPEND citedIds "${identifier}")
    endforeach()
endforeach()

list(REMOVE_DUPLICATES citedIds)

# --- Report ---------------------------------------------------------------

if(failures)
    list(JOIN failures "\n  " failureText)
    message(FATAL_ERROR "Corpus integrity check failed:\n  ${failureText}")
endif()

list(LENGTH fixtureSources fixtureCount)
list(LENGTH citedIds citedIdCount)
message(STATUS
    "Corpus integrity check passed: ${fixtureCount} fixture(s), "
    "${citedIdCount} construct ID(s) cited, ${documentedIdCount} documented "
    "(the documented total includes OUT-* exclusions, which are never cited).")
```

- [ ] **Step 4: Register the check with CTest**

Append to `tests/CMakeLists.txt`, after the existing `codeqa_integration_lang_override` test:

```cmake
add_test(
    NAME codeqa_corpus_integrity
    COMMAND ${CMAKE_COMMAND}
        -DCODEQA_CORPUS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/corpus
        -P ${CMAKE_CURRENT_SOURCE_DIR}/corpus/CheckCorpusIntegrity.cmake
)
```

- [ ] **Step 5: Run it to verify it FAILS (no fixtures exist yet)**

```bash
cmake -S . -B build
ctest --test-dir build --output-on-failure -C Debug -R codeqa_corpus_integrity
```

Expected: the test runs. With `SUPPORTED-CONSTRUCTS.md` present but no fixtures yet, `fixtureSources` and `expectationFiles` are both empty, so checks 1–3 find no failures and the test **passes**, reporting `0 fixture(s), 0 construct ID(s) cited, 46 documented`. That is the correct behavior at this point — an empty corpus is consistent, just empty. If the documented count is anything other than 46, the ID regex is not matching every table row: check that two-segment IDs (`CTRL-01`, `OUT-03`) are being picked up alongside three-segment ones.

To confirm the checker actually detects breakage, temporarily create an orphan file and re-run:

```bash
echo "int main(void) { return 0; }" > tests/corpus/parsing/orphan.c
ctest --test-dir build --output-on-failure -C Debug -R codeqa_corpus_integrity
```

Expected: **FAIL** with `Fixture '.../orphan.c' has no 'orphan.parsing.json'.` Then delete it:

```bash
rm tests/corpus/parsing/orphan.c
```

- [ ] **Step 6: Write the two `parsing/` fixtures**

`tests/corpus/parsing/minimal.c`:

```c
#include <stdio.h>

int main(void)
{
    return 0;
}
```

`tests/corpus/parsing/minimal.parsing.json`:

```json
{
  "constructs": ["C-PP-01", "C-FUNC-01", "CTRL-08"],
  "functions": [
    {
      "name": "main",
      "startLine": 3,
      "endLine": 6,
      "parameters": []
    }
  ],
  "classes": [],
  "diagnostics": []
}
```

`tests/corpus/parsing/minimal-cpp.cpp`:

```cpp
#include <string>

namespace demo
{

int identity(int value)
{
    return value;
}

} // namespace demo
```

`tests/corpus/parsing/minimal-cpp.parsing.json`:

```json
{
  "constructs": ["C-PP-01", "CPP-NS-01", "CPP-FUNC-01", "CTRL-08"],
  "functions": [
    {
      "name": "identity",
      "startLine": 6,
      "endLine": 9,
      "parameters": ["value"]
    }
  ],
  "classes": [
    {
      "name": "demo",
      "kind": "namespace",
      "startLine": 3,
      "endLine": 11
    }
  ],
  "diagnostics": []
}
```

- [ ] **Step 7: Verify the line numbers against the real files**

Do not trust the numbers transcribed above — confirm them against the files you just wrote:

```bash
grep -n "" tests/corpus/parsing/minimal.c
grep -n "" tests/corpus/parsing/minimal-cpp.cpp
```

Check each `startLine`/`endLine` in the two `.parsing.json` files against that numbered output. `main` must start on the line with `int main(void)` and end on the line with its closing `}`. Same for `identity`, and `demo`'s range must span from `namespace demo` to its closing `}`. If any number disagrees, **fix the JSON, not the source** — then note the correction in your report.

- [ ] **Step 8: Run the integrity check to verify it passes**

```bash
cmake -S . -B build
ctest --test-dir build --output-on-failure -C Debug
```

Expected: all tests pass, including `codeqa_corpus_integrity`, which now reports `2 fixture(s), 5 construct ID(s) cited, 46 documented`.

- [ ] **Step 9: Commit**

```bash
git add tests/corpus tests/CMakeLists.txt
git commit -m "test: add corpus scaffolding, construct list, and integrity checker"
```

---

### Task 2: `functions/` fixtures

**Files:**
- Create: `tests/corpus/functions/c-functions.c`
- Create: `tests/corpus/functions/c-functions.parsing.json`
- Create: `tests/corpus/functions/cpp-functions.cpp`
- Create: `tests/corpus/functions/cpp-functions.parsing.json`

**Interfaces:**
- Consumes: the construct IDs defined in `tests/corpus/SUPPORTED-CONSTRUCTS.md` (Task 1) and the expectation format documented in `tests/corpus/README.md` (Task 1). Do not invent IDs — every one you cite must already exist in that document.
- Produces: coverage for `C-FUNC-01..05`, `C-PP-01`, `CPP-FUNC-01..08`, `CTRL-11`. Task 6's coverage check depends on these IDs being cited here.

- [ ] **Step 1: Write the C functions fixture**

`tests/corpus/functions/c-functions.c`:

```c
#include <stdarg.h>

int declaredOnly(int value);

static int helper(int value)
{
    return value * 2;
}

int sumAll(int count, ...)
{
    va_list args;
    va_start(args, count);

    int total = 0;

    for (int index = 0; index < count; ++index)
    {
        total += va_arg(args, int);
    }

    va_end(args);
    return total;
}

int applyTwice(int value, int (*operation)(int))
{
    return operation(operation(value));
}

int findFirstNegative(const int* values, int count)
{
    int index = 0;

search:
    if (index >= count)
    {
        return -1;
    }

    if (values[index] < 0)
    {
        return index;
    }

    ++index;
    goto search;
}
```

`tests/corpus/functions/c-functions.parsing.json`:

```json
{
  "constructs": [
    "C-PP-01",
    "C-FUNC-01",
    "C-FUNC-02",
    "C-FUNC-03",
    "C-FUNC-04",
    "C-FUNC-05",
    "CTRL-01",
    "CTRL-03",
    "CTRL-08",
    "CTRL-11"
  ],
  "functions": [
    {
      "name": "declaredOnly",
      "startLine": 3,
      "endLine": 3,
      "parameters": ["value"]
    },
    {
      "name": "helper",
      "startLine": 5,
      "endLine": 8,
      "parameters": ["value"]
    },
    {
      "name": "sumAll",
      "startLine": 10,
      "endLine": 24,
      "parameters": ["count"]
    },
    {
      "name": "applyTwice",
      "startLine": 26,
      "endLine": 29,
      "parameters": ["value", "operation"]
    },
    {
      "name": "findFirstNegative",
      "startLine": 31,
      "endLine": 48,
      "parameters": ["values", "count"]
    }
  ],
  "classes": [],
  "diagnostics": []
}
```

- [ ] **Step 2: Write the C++ functions fixture**

`tests/corpus/functions/cpp-functions.cpp`:

```cpp
#include <algorithm>
#include <vector>

class Counter
{
public:
    Counter()
        : value_(0)
    {
    }

    ~Counter()
    {
        value_ = 0;
    }

    void increment()
    {
        ++value_;
    }

    int value() const;

    bool operator==(const Counter& other) const
    {
        return value_ == other.value_;
    }

private:
    int value_;
};

int Counter::value() const
{
    return value_;
}

int countPositive(const std::vector<int>& values)
{
    return static_cast<int>(std::count_if(values.begin(), values.end(), [](int value)
    {
        return value > 0;
    }));
}

template <typename T>
T largest(T first, T second)
{
    return first < second ? second : first;
}
```

`tests/corpus/functions/cpp-functions.parsing.json`:

```json
{
  "constructs": [
    "C-PP-01",
    "CPP-FUNC-01",
    "CPP-FUNC-02",
    "CPP-FUNC-03",
    "CPP-FUNC-04",
    "CPP-FUNC-05",
    "CPP-FUNC-06",
    "CPP-FUNC-07",
    "CPP-FUNC-08",
    "CPP-TYPE-01",
    "CPP-PARAM-02",
    "CTRL-06",
    "CTRL-08"
  ],
  "functions": [
    {
      "name": "Counter",
      "startLine": 7,
      "endLine": 10,
      "parameters": []
    },
    {
      "name": "~Counter",
      "startLine": 12,
      "endLine": 15,
      "parameters": []
    },
    {
      "name": "increment",
      "startLine": 17,
      "endLine": 20,
      "parameters": []
    },
    {
      "name": "value",
      "startLine": 22,
      "endLine": 22,
      "parameters": []
    },
    {
      "name": "operator==",
      "startLine": 24,
      "endLine": 27,
      "parameters": ["other"]
    },
    {
      "name": "Counter::value",
      "startLine": 33,
      "endLine": 36,
      "parameters": []
    },
    {
      "name": "countPositive",
      "startLine": 38,
      "endLine": 44,
      "parameters": ["values"]
    },
    {
      "name": "<lambda>",
      "startLine": 40,
      "endLine": 43,
      "parameters": ["value"]
    },
    {
      "name": "largest",
      "startLine": 46,
      "endLine": 50,
      "parameters": ["first", "second"]
    }
  ],
  "classes": [
    {
      "name": "Counter",
      "kind": "class",
      "startLine": 4,
      "endLine": 30
    }
  ],
  "diagnostics": []
}
```

Note on `Counter::value`: the out-of-class definition is recorded with its
qualified name, distinguishing it from the in-class declaration on line 22.
This is the convention the Fase 4 adapter must follow.

- [ ] **Step 3: Verify the line numbers against the real files**

```bash
grep -n "" tests/corpus/functions/c-functions.c
grep -n "" tests/corpus/functions/cpp-functions.cpp
```

Check every `startLine`/`endLine` against that numbered output. The lambda's range
is the `[](int value)` line through the line with its closing `}`. `largest`'s
`startLine` is the `template <typename T>` line, not the `T largest(...)` line —
per the format rule in `README.md`. If any number disagrees, fix the JSON and note
the correction in your report.

- [ ] **Step 4: Run the integrity check**

```bash
cmake -S . -B build
ctest --test-dir build --output-on-failure -C Debug
```

Expected: all tests pass; `codeqa_corpus_integrity` reports 4 fixtures and a higher cited-ID count than after Task 1.

- [ ] **Step 5: Commit**

```bash
git add tests/corpus/functions
git commit -m "test: add C and C++ function corpus fixtures"
```

---

### Task 3: `classes/` and `parameters/` fixtures

**Files:**
- Create: `tests/corpus/classes/c-types.c`
- Create: `tests/corpus/classes/c-types.parsing.json`
- Create: `tests/corpus/classes/cpp-types.cpp`
- Create: `tests/corpus/classes/cpp-types.parsing.json`
- Create: `tests/corpus/parameters/cpp-parameters.cpp`
- Create: `tests/corpus/parameters/cpp-parameters.parsing.json`

**Interfaces:**
- Consumes: construct IDs from `tests/corpus/SUPPORTED-CONSTRUCTS.md` and the format documented in `tests/corpus/README.md`.
- Produces: coverage for `C-TYPE-01..04`, `C-DECL-01`, `CPP-TYPE-01..05`, `CPP-NS-01..02`, `CPP-PARAM-01..02`.

- [ ] **Step 1: Write the C types fixture**

`tests/corpus/classes/c-types.c`:

```c
#include <stddef.h>

int globalCounter = 0;

struct Point
{
    int x;
    int y;
};

union Value
{
    int asInteger;
    float asFloat;
};

enum Status
{
    StatusIdle,
    StatusRunning,
    StatusStopped
};

typedef struct Point PointAlias;

size_t pointSize(void)
{
    return sizeof(PointAlias);
}
```

`tests/corpus/classes/c-types.parsing.json`:

```json
{
  "constructs": [
    "C-PP-01",
    "C-TYPE-01",
    "C-TYPE-02",
    "C-TYPE-03",
    "C-TYPE-04",
    "C-DECL-01",
    "C-FUNC-01",
    "CTRL-08"
  ],
  "functions": [
    {
      "name": "pointSize",
      "startLine": 26,
      "endLine": 29,
      "parameters": []
    }
  ],
  "classes": [
    {
      "name": "Point",
      "kind": "struct",
      "startLine": 5,
      "endLine": 9
    },
    {
      "name": "Value",
      "kind": "union",
      "startLine": 11,
      "endLine": 15
    },
    {
      "name": "Status",
      "kind": "enum",
      "startLine": 17,
      "endLine": 22
    }
  ],
  "diagnostics": []
}
```

- [ ] **Step 2: Write the C++ types fixture**

`tests/corpus/classes/cpp-types.cpp`:

```cpp
#include <cstddef>

namespace outer
{

namespace
{

int internalHelper()
{
    return 1;
}

} // namespace

enum class Mode
{
    Fast,
    Slow
};

struct Config
{
    int retries;
};

class Container
{
public:
    class Iterator
    {
    public:
        int position() const
        {
            return position_;
        }

    private:
        int position_;
    };

    std::size_t size() const
    {
        return size_;
    }

private:
    std::size_t size_;
};

template <typename T>
class Box
{
public:
    explicit Box(T content)
        : content_(content)
    {
    }

private:
    T content_;
};

} // namespace outer
```

`tests/corpus/classes/cpp-types.parsing.json`:

```json
{
  "constructs": [
    "C-PP-01",
    "CPP-TYPE-01",
    "CPP-TYPE-02",
    "CPP-TYPE-03",
    "CPP-TYPE-04",
    "CPP-TYPE-05",
    "CPP-NS-01",
    "CPP-NS-02",
    "CPP-FUNC-01",
    "CPP-FUNC-02",
    "CPP-FUNC-04",
    "CTRL-08"
  ],
  "functions": [
    {
      "name": "internalHelper",
      "startLine": 9,
      "endLine": 12,
      "parameters": []
    },
    {
      "name": "position",
      "startLine": 32,
      "endLine": 35,
      "parameters": []
    },
    {
      "name": "size",
      "startLine": 41,
      "endLine": 44,
      "parameters": []
    },
    {
      "name": "Box",
      "startLine": 54,
      "endLine": 57,
      "parameters": ["content"]
    }
  ],
  "classes": [
    {
      "name": "outer",
      "kind": "namespace",
      "startLine": 3,
      "endLine": 62
    },
    {
      "name": "",
      "kind": "namespace",
      "startLine": 6,
      "endLine": 14
    },
    {
      "name": "Mode",
      "kind": "enum",
      "startLine": 16,
      "endLine": 20
    },
    {
      "name": "Config",
      "kind": "struct",
      "startLine": 22,
      "endLine": 25
    },
    {
      "name": "Container",
      "kind": "class",
      "startLine": 27,
      "endLine": 48
    },
    {
      "name": "Iterator",
      "kind": "class",
      "startLine": 30,
      "endLine": 39
    },
    {
      "name": "Box",
      "kind": "class",
      "startLine": 50,
      "endLine": 60
    }
  ],
  "diagnostics": []
}
```

An anonymous namespace is recorded with an empty `name`, matching the
unnamed-parameter convention in `README.md`. `Box`'s `startLine` is the
`template <typename T>` line.

- [ ] **Step 3: Write the parameters fixture**

`tests/corpus/parameters/cpp-parameters.cpp`:

```cpp
#include <string>

int withDefaults(int required, int optional = 5, bool verbose = false)
{
    return verbose ? required + optional : required;
}

void byReference(std::string& target, const std::string& source)
{
    target = source;
}

void byPointer(int* output, const int* input)
{
    *output = *input;
}

int ignoresArgument(int)
{
    return 0;
}

int manyParameters(int first, int second, int third, int fourth, int fifth)
{
    return first + second + third + fourth + fifth;
}
```

`tests/corpus/parameters/cpp-parameters.parsing.json`:

```json
{
  "constructs": [
    "C-PP-01",
    "CPP-FUNC-01",
    "CPP-PARAM-01",
    "CPP-PARAM-02",
    "CTRL-06",
    "CTRL-08"
  ],
  "functions": [
    {
      "name": "withDefaults",
      "startLine": 3,
      "endLine": 6,
      "parameters": ["required", "optional", "verbose"]
    },
    {
      "name": "byReference",
      "startLine": 8,
      "endLine": 11,
      "parameters": ["target", "source"]
    },
    {
      "name": "byPointer",
      "startLine": 13,
      "endLine": 16,
      "parameters": ["output", "input"]
    },
    {
      "name": "ignoresArgument",
      "startLine": 18,
      "endLine": 21,
      "parameters": [""]
    },
    {
      "name": "manyParameters",
      "startLine": 23,
      "endLine": 26,
      "parameters": ["first", "second", "third", "fourth", "fifth"]
    }
  ],
  "classes": [],
  "diagnostics": []
}
```

`ignoresArgument` is the worked example of the unnamed-parameter rule: one
positional parameter, represented as `""`.

- [ ] **Step 4: Verify the line numbers against the real files**

```bash
grep -n "" tests/corpus/classes/c-types.c
grep -n "" tests/corpus/classes/cpp-types.cpp
grep -n "" tests/corpus/parameters/cpp-parameters.cpp
```

Check every `startLine`/`endLine`. Pay particular attention to the nested ranges in
`cpp-types.cpp`: `outer` must span the whole namespace, `Container` must contain
`Iterator`'s range entirely. Fix the JSON where it disagrees and note it in your report.

- [ ] **Step 5: Run the integrity check**

```bash
cmake -S . -B build
ctest --test-dir build --output-on-failure -C Debug
```

Expected: all tests pass; `codeqa_corpus_integrity` reports 7 fixtures.

- [ ] **Step 6: Commit**

```bash
git add tests/corpus/classes tests/corpus/parameters
git commit -m "test: add type, namespace, and parameter corpus fixtures"
```

---

### Task 4: `complexity/` and `nesting/` fixtures

**Files:**
- Create: `tests/corpus/complexity/branching.cpp`
- Create: `tests/corpus/complexity/branching.parsing.json`
- Create: `tests/corpus/nesting/deep-nesting.cpp`
- Create: `tests/corpus/nesting/deep-nesting.parsing.json`

**Interfaces:**
- Consumes: construct IDs from `tests/corpus/SUPPORTED-CONSTRUCTS.md`; format from `tests/corpus/README.md`.
- Produces: coverage for `CTRL-01..10`. These two fixtures are the ones Fase 6 will later attach `*.metrics.json` files to.

**Important:** these fixtures get a `.parsing.json` like every other (their structure is knowable today), but **no `.metrics.json`**. Do not write metric values — the formulas are a Fase 6 decision. The sources are written now precisely because they are stable: a `for` inside an `if` inside a `while` stays exactly that regardless of how complexity is later defined.

- [ ] **Step 1: Write the branching fixture**

`tests/corpus/complexity/branching.cpp`:

```cpp
#include <stdexcept>

int classify(int value, bool strict)
{
    if (value < 0 && strict)
    {
        return -1;
    }
    else if (value == 0 || !strict)
    {
        return 0;
    }

    return 1;
}

int scan(const int* values, int count)
{
    int found = 0;

    for (int index = 0; index < count; ++index)
    {
        if (values[index] == 0)
        {
            continue;
        }

        if (values[index] < 0)
        {
            break;
        }

        ++found;
    }

    return found;
}

int describe(int code)
{
    switch (code)
    {
    case 0:
        return 100;
    case 1:
        return 200;
    default:
        return 900;
    }
}

int drain(int remaining)
{
    do
    {
        --remaining;
    } while (remaining > 0);

    return remaining;
}

int guarded(int value)
{
    try
    {
        if (value < 0)
        {
            throw std::runtime_error("negative");
        }

        return value;
    }
    catch (const std::runtime_error&)
    {
        return -1;
    }
}
```

`tests/corpus/complexity/branching.parsing.json`:

```json
{
  "constructs": [
    "C-PP-01",
    "CPP-FUNC-01",
    "CTRL-01",
    "CTRL-03",
    "CTRL-04",
    "CTRL-05",
    "CTRL-07",
    "CTRL-08",
    "CTRL-09",
    "CTRL-10"
  ],
  "functions": [
    {
      "name": "classify",
      "startLine": 3,
      "endLine": 15,
      "parameters": ["value", "strict"]
    },
    {
      "name": "scan",
      "startLine": 17,
      "endLine": 37,
      "parameters": ["values", "count"]
    },
    {
      "name": "describe",
      "startLine": 39,
      "endLine": 50,
      "parameters": ["code"]
    },
    {
      "name": "drain",
      "startLine": 52,
      "endLine": 60,
      "parameters": ["remaining"]
    },
    {
      "name": "guarded",
      "startLine": 62,
      "endLine": 77,
      "parameters": ["value"]
    }
  ],
  "classes": [],
  "diagnostics": []
}
```

- [ ] **Step 2: Write the nesting fixture**

`tests/corpus/nesting/deep-nesting.cpp`:

```cpp
int flat(int value)
{
    return value;
}

int oneLevel(int value)
{
    if (value > 0)
    {
        return 1;
    }

    return 0;
}

int fourLevels(const int* values, int count)
{
    int total = 0;

    while (count > 0)
    {
        for (int index = 0; index < count; ++index)
        {
            if (values[index] > 0)
            {
                if (values[index] % 2 == 0)
                {
                    total += values[index];
                }
            }
        }

        --count;
    }

    return total;
}
```

`tests/corpus/nesting/deep-nesting.parsing.json`:

```json
{
  "constructs": [
    "CPP-FUNC-01",
    "CTRL-01",
    "CTRL-02",
    "CTRL-03",
    "CTRL-08"
  ],
  "functions": [
    {
      "name": "flat",
      "startLine": 1,
      "endLine": 4,
      "parameters": ["value"]
    },
    {
      "name": "oneLevel",
      "startLine": 6,
      "endLine": 14,
      "parameters": ["value"]
    },
    {
      "name": "fourLevels",
      "startLine": 16,
      "endLine": 38,
      "parameters": ["values", "count"]
    }
  ],
  "classes": [],
  "diagnostics": []
}
```

The three functions are deliberately graded — zero, one, and four levels of
nesting — so Fase 6's maximum-nesting metric has an unambiguous gradient to
verify against.

- [ ] **Step 3: Verify the line numbers against the real files**

```bash
grep -n "" tests/corpus/complexity/branching.cpp
grep -n "" tests/corpus/nesting/deep-nesting.cpp
```

Check every `startLine`/`endLine`. Fix the JSON where it disagrees and note it in your report.

- [ ] **Step 4: Confirm no metrics files were created**

```bash
ls tests/corpus/complexity/
ls tests/corpus/nesting/
```

Expected: each directory contains exactly one `.cpp` and one `.parsing.json`. If a `.metrics.json` exists, delete it — those belong to Fase 6.

- [ ] **Step 5: Run the integrity check**

```bash
cmake -S . -B build
ctest --test-dir build --output-on-failure -C Debug
```

Expected: all tests pass; `codeqa_corpus_integrity` reports 9 fixtures.

- [ ] **Step 6: Commit**

```bash
git add tests/corpus/complexity tests/corpus/nesting
git commit -m "test: add branching and nesting corpus fixtures"
```

---

### Task 5: `malformed/` and `mixed/` fixtures

**Files:**
- Create: `tests/corpus/malformed/unclosed-brace.cpp`
- Create: `tests/corpus/malformed/unclosed-brace.parsing.json`
- Create: `tests/corpus/malformed/truncated.c`
- Create: `tests/corpus/malformed/truncated.parsing.json`
- Create: `tests/corpus/mixed/mixed-constructs.cpp`
- Create: `tests/corpus/mixed/mixed-constructs.parsing.json`

**Interfaces:**
- Consumes: construct IDs from `tests/corpus/SUPPORTED-CONSTRUCTS.md`; format from `tests/corpus/README.md`.
- Produces: the only fixtures with non-empty `diagnostics`, plus coverage for `C-TYPE-03` and `CPP-TYPE-03` in a combined context.

**Important:** the `malformed/` fixtures are deliberately broken source. They must **not** compile. Their `diagnostics` entries carry only `line` and `severity` — never the message text, which comes from tree-sitter and may change between versions. The line numbers given below are where the defect becomes detectable; if tree-sitter later reports a neighbouring line, that is a legitimate expectation update in Fase 4, not a fixture defect.

- [ ] **Step 1: Write the unclosed-brace fixture**

`tests/corpus/malformed/unclosed-brace.cpp`:

```cpp
int wellFormed(int value)
{
    return value;
}

int missingClosingBrace(int value)
{
    if (value > 0)
    {
        return 1;

    return 0;
}
```

`tests/corpus/malformed/unclosed-brace.parsing.json`:

```json
{
  "constructs": ["CPP-FUNC-01", "CTRL-01", "CTRL-08"],
  "functions": [
    {
      "name": "wellFormed",
      "startLine": 1,
      "endLine": 4,
      "parameters": ["value"]
    }
  ],
  "classes": [],
  "diagnostics": [
    {
      "line": 13,
      "severity": "error"
    }
  ]
}
```

`wellFormed` is present and correct **before** the broken region: this fixture's
purpose is to prove that a defect later in the file does not destroy the analysis
of what came before it.

- [ ] **Step 2: Write the truncated fixture**

`tests/corpus/malformed/truncated.c`:

```c
#include <stddef.h>

int completeFunction(int value)
{
    return value + 1;
}

int truncatedFunction(int value
```

`tests/corpus/malformed/truncated.parsing.json`:

```json
{
  "constructs": ["C-PP-01", "C-FUNC-01", "CTRL-08"],
  "functions": [
    {
      "name": "completeFunction",
      "startLine": 3,
      "endLine": 6,
      "parameters": ["value"]
    }
  ],
  "classes": [],
  "diagnostics": [
    {
      "line": 8,
      "severity": "error"
    }
  ]
}
```

- [ ] **Step 3: Write the mixed fixture**

`tests/corpus/mixed/mixed-constructs.cpp`:

```cpp
#include <string>
#include <vector>

namespace app
{

enum class Level
{
    Low,
    High
};

struct Settings
{
    int retries;
    bool verbose;
};

class Engine
{
public:
    explicit Engine(Settings settings)
        : settings_(settings)
    {
    }

    ~Engine() = default;

    bool run(const std::vector<int>& inputs);

    bool operator!() const
    {
        return settings_.retries == 0;
    }

private:
    Settings settings_;
};

bool Engine::run(const std::vector<int>& inputs)
{
    if (inputs.empty())
    {
        return false;
    }

    for (int input : inputs)
    {
        if (input < 0 && settings_.verbose)
        {
            return false;
        }
    }

    return true;
}

template <typename T>
bool isEmpty(const T& container)
{
    return container.size() == 0;
}

} // namespace app
```

`tests/corpus/mixed/mixed-constructs.parsing.json`:

```json
{
  "constructs": [
    "C-PP-01",
    "CPP-NS-01",
    "CPP-TYPE-01",
    "CPP-TYPE-02",
    "CPP-TYPE-03",
    "CPP-FUNC-02",
    "CPP-FUNC-03",
    "CPP-FUNC-04",
    "CPP-FUNC-05",
    "CPP-FUNC-06",
    "CPP-FUNC-08",
    "CPP-PARAM-02",
    "CTRL-01",
    "CTRL-03",
    "CTRL-07",
    "CTRL-08"
  ],
  "functions": [
    {
      "name": "Engine",
      "startLine": 22,
      "endLine": 25,
      "parameters": ["settings"]
    },
    {
      "name": "~Engine",
      "startLine": 27,
      "endLine": 27,
      "parameters": []
    },
    {
      "name": "run",
      "startLine": 29,
      "endLine": 29,
      "parameters": ["inputs"]
    },
    {
      "name": "operator!",
      "startLine": 31,
      "endLine": 34,
      "parameters": []
    },
    {
      "name": "Engine::run",
      "startLine": 40,
      "endLine": 56,
      "parameters": ["inputs"]
    },
    {
      "name": "isEmpty",
      "startLine": 58,
      "endLine": 62,
      "parameters": ["container"]
    }
  ],
  "classes": [
    {
      "name": "app",
      "kind": "namespace",
      "startLine": 4,
      "endLine": 64
    },
    {
      "name": "Level",
      "kind": "enum",
      "startLine": 7,
      "endLine": 11
    },
    {
      "name": "Settings",
      "kind": "struct",
      "startLine": 13,
      "endLine": 17
    },
    {
      "name": "Engine",
      "kind": "class",
      "startLine": 19,
      "endLine": 37
    }
  ],
  "diagnostics": []
}
```

`~Engine() = default;` is a defaulted destructor with no body:
`startLine == endLine`, following the prototype rule.

- [ ] **Step 4: Verify the line numbers against the real files**

```bash
grep -n "" tests/corpus/malformed/unclosed-brace.cpp
grep -n "" tests/corpus/malformed/truncated.c
grep -n "" tests/corpus/mixed/mixed-constructs.cpp
```

Check every `startLine`/`endLine`, and confirm each `diagnostics` line points at
where the defect actually is. Fix the JSON where it disagrees and note it in your report.

- [ ] **Step 5: Confirm the malformed fixtures really are malformed**

The point of `malformed/` is that the source is broken. Verify this rather than
assuming it — a fixture that turns out to be valid C++ silently tests nothing:

```bash
"/c/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64/cl.exe" /c /nologo /Fo:NUL tests/corpus/malformed/unclosed-brace.cpp
```

Expected: the compiler **fails** with a syntax error. Repeat for `truncated.c`.
If either compiles successfully, the fixture is not malformed — fix the source so
it genuinely breaks, then re-check its expectation file.

- [ ] **Step 6: Run the integrity check**

```bash
cmake -S . -B build
ctest --test-dir build --output-on-failure -C Debug
```

Expected: all tests pass; `codeqa_corpus_integrity` reports 12 fixtures.

- [ ] **Step 7: Commit**

```bash
git add tests/corpus/malformed tests/corpus/mixed
git commit -m "test: add malformed and mixed corpus fixtures"
```

---

### Task 6: Real projects document and full coverage check

**Files:**
- Create: `tests/corpus/REAL-PROJECTS.md`
- Modify: `tests/corpus/CheckCorpusIntegrity.cmake` (add check 4: full coverage)

**Interfaces:**
- Consumes: every `.parsing.json` written in Tasks 1–5, and the ID tables in `tests/corpus/SUPPORTED-CONSTRUCTS.md`.
- Produces: nothing consumed by later tasks — this is the last task in the plan.

- [ ] **Step 1: Write the real projects document**

`tests/corpus/REAL-PROJECTS.md`:

```markdown
# Proyectos reales de validación

Proyectos públicos usados para validación exploratoria manual del analizador.
Su código **no se copia** a este repositorio: se listan aquí con su procedencia
exacta para que cualquiera pueda obtener la misma versión y reproducir los
resultados.

No forman parte de la suite automatizada. Los casos diseñados a propósito viven
en el corpus (`tests/corpus/`); estos proyectos aportan lo que ningún fixture
puede aportar: código que nadie escribió pensando en este analizador.

## SQLite (amalgamación)

| Campo | Valor |
|---|---|
| Lenguaje | C |
| Licencia | Dominio público |
| Origen | https://www.sqlite.org/download.html (`sqlite-amalgamation-*.zip`) |
| Versión fijada | 3.46.0 |
| Archivo de interés | `sqlite3.c` |

**Por qué:** la amalgamación es un único archivo de aproximadamente 250 000
líneas. Es la prueba de estrés del descubrimiento y del parsing, con código C
real de altísima calidad y una licencia sin fricción alguna.

**Qué debe ejercitar:** volumen extremo en un solo archivo, funciones `static`
en cantidad, macros densas, y el comportamiento del analizador cuando un archivo
es mucho mayor que cualquier fixture.

## fmt

| Campo | Valor |
|---|---|
| Lenguaje | C++ |
| Licencia | MIT |
| Origen | https://github.com/fmtlib/fmt |
| Versión fijada | 10.2.1 |
| Directorios de interés | `include/fmt/`, `src/` |

**Por qué:** C++ moderno cargado de plantillas, lambdas y sobrecarga de
operadores — exactamente las tres construcciones de la zona gris que el MVP
declara soportadas (`CPP-FUNC-06`, `CPP-FUNC-07`, `CPP-FUNC-08`, `CPP-TYPE-04`).

**Qué debe ejercitar:** plantillas anidadas, `constexpr` intensivo, cabeceras
grandes, y la distinción entre declaración e instanciación (que el MVP
explícitamente no hace, `OUT-01`).

## Criterios de éxito sobre estos proyectos

- El análisis completa sin caídas ni cuelgues sobre ambos.
- La tasa de regiones `ERROR` se **mide y se documenta** la primera vez en la
  Fase 4, y ese valor se trata como línea base a no empeorar.

No se fija hoy un umbral numérico: no existe ningún dato que lo respalde, e
inventarlo sería precisamente el tipo de certeza falsa que este proyecto rechaza
por principio.
```

- [ ] **Step 2: Add the coverage check to the integrity script**

In `tests/corpus/CheckCorpusIntegrity.cmake`, replace the `# --- Report` section
header comment block and insert check 4 immediately before it:

```cmake
# --- Check 4: every documented non-OUT ID is covered by a fixture ---------

foreach(identifier IN LISTS documentedIds)
    if(identifier MATCHES "^OUT-")
        continue()
    endif()

    if(NOT identifier IN_LIST citedIds)
        list(APPEND failures "Construct '${identifier}' is documented but no fixture covers it.")
    endif()
endforeach()

# --- Report ---------------------------------------------------------------
```

Also update the file's header comment to describe check 4 as active rather than
deferred: replace the line

```cmake
# Check 4 (every documented ID is covered by a fixture) is added in Task 6.
```

with

```cmake
#   4. Every documented non-OUT-* ID is covered by at least one fixture.
```

- [ ] **Step 3: Run the full suite and resolve any coverage gaps**

```bash
cmake -S . -B build
ctest --test-dir build --output-on-failure -C Debug
```

Expected: all tests pass. If `codeqa_corpus_integrity` now reports documented IDs
with no covering fixture, that is a real gap in the corpus — **do not** delete the
ID from `SUPPORTED-CONSTRUCTS.md` to silence the check. Instead, add the missing
construct to the most appropriate existing fixture and extend that fixture's
`constructs` array and expectations accordingly, then re-verify its line numbers
with `grep -n ""` as in the earlier tasks. Report which IDs were missing and how
you covered them.

- [ ] **Step 4: Verify the checker still detects breakage**

A passing check proves nothing unless it can fail. Temporarily add a bogus ID to
one fixture and confirm the check catches it:

```bash
sed -i 's/"constructs": \["CPP-FUNC-01", "CTRL-01", "CTRL-08"\]/"constructs": ["CPP-FUNC-01", "CTRL-01", "CTRL-08", "CPP-BOGUS-99"]/' tests/corpus/malformed/unclosed-brace.parsing.json
ctest --test-dir build --output-on-failure -C Debug -R codeqa_corpus_integrity
```

Expected: **FAIL** with `cites 'CPP-BOGUS-99', which is not in SUPPORTED-CONSTRUCTS.md`.
Then restore the file:

```bash
git checkout tests/corpus/malformed/unclosed-brace.parsing.json
ctest --test-dir build --output-on-failure -C Debug -R codeqa_corpus_integrity
```

Expected: PASS again.

- [ ] **Step 5: Commit**

```bash
git add tests/corpus/REAL-PROJECTS.md tests/corpus/CheckCorpusIntegrity.cmake
git commit -m "test: document real validation projects and enforce full corpus coverage"
```

---

## Self-Review Notes

- **Spec coverage:** every item in the design spec's section 3 (Alcance) maps to a task — construct list with IDs and out-of-scope table (Task 1), fixture sources for all categories (Tasks 1–5), parsing expectations (Tasks 1–5), format and usage documentation (Task 1), public validation projects (Task 6), verifiable success criteria (Task 1's integrity checker plus Task 6's coverage check).
- **Deliberate deferrals, restated where an implementer might otherwise act:** no `*.metrics.json` (Task 4 Step 4 explicitly verifies none were created), no JSON library vendored, no runner built, no production code touched.
- **Type/format consistency:** the four `.parsing.json` fields (`constructs`, `functions`, `classes`, `diagnostics`) and their sub-fields (`name`, `startLine`, `endLine`, `parameters`; `name`, `kind`, `startLine`, `endLine`; `line`, `severity`) are identical across the README's specification in Task 1 and all twelve fixtures in Tasks 1–5. The `kind` vocabulary (`class`, `struct`, `union`, `enum`, `namespace`) is fixed in the README and used consistently.
- **Line-number risk, and how it is handled:** the line numbers in this plan were counted by hand and are the most error-prone content here. Every fixture task therefore carries an explicit verification step (`grep -n ""`) instructing the implementer to check the numbers against the real file and fix the JSON rather than transcribe blindly. The integrity checker cannot catch a wrong line number — only Fase 4's runner will — which is exactly why the manual check is mandatory rather than optional.
- **The checker is itself tested:** Task 1 Step 5 and Task 6 Step 4 both force a deliberate failure and confirm it is detected, so `codeqa_corpus_integrity` cannot silently degrade into a test that always passes.
