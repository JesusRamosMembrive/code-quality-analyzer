# Fase 11 — Validación sistemática Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. **Task 5 is not a normal autonomous task** — see its own instructions before dispatching it.

**Goal:** Close the corpus's rule-coverage gap (6 of 8 rules currently have no `findings.json` fixture), write down the project's regression-test policy and rule-stability tracking, and stand up the real-project precision-validation pipeline (ledger schema, aggregation script, methodology doc), proven end-to-end with a real 10-20-finding sample against SQLite 3.46.0 and fmt 10.2.1.

**Architecture:** Two independent groups. Group A (Task 1-2) only touches the existing corpus and adds two docs — no new subsystem. Group B (Task 3-5) adds a new, deliberately-non-shipped validation pipeline: a ledger JSON format for reviewed findings (`proposal` vs. authoritative `decision`), a standalone `cmake -P` script that aggregates a ledger into the roadmap's exact per-rule precision report (this script alone needs CMake ≥3.19 for `string(JSON ...)`, unlike the rest of the project's 3.16 floor — it's never invoked by the build), and a one-time real-project run that produces the first two ledgers by hand, with an agent proposing verdicts and a human confirming them.

**Tech Stack:** C++17/doctest/CMake (Group A, matches the existing project), CMake script mode + `string(JSON ...)` (Group B's report script only), `codeqa analyze --format json` (already shipped, Fase 9) as the only way real findings enter this pipeline.

## Global Constraints

- `tests/corpus/GenerateValidationReport.cmake` requires CMake ≥3.19 (`string(JSON ...)`) — this is a **deliberate, scoped exception**, documented in the script itself. It does not change `cmake_minimum_required(VERSION 3.16)` in the root `CMakeLists.txt`, because this script is never invoked by the configure/build — only manually by a maintainer, or by its own synthetic-fixture `ctest` entry (Task 4).
- `experimental`/`stable` rule metadata lives **only** in `tests/corpus/validation/RULE-STABILITY.md` — never in `codeqa_core`, `ProjectConfig`, `RuleConfig`, or `codeqa.json`'s schema. No task in this plan touches `include/codeqa/config/`, `src/config/`, or `src/rules/`.
- A ledger entry's `decision` object (not `proposal`) is the only thing `GenerateValidationReport.cmake` counts. `proposal.verdict` is never read by the report script — it exists purely as an audit trail of what the agent suggested.
- Real project source (SQLite, fmt) is **never** committed to this repository — matches `tests/corpus/REAL-PROJECTS.md`'s existing rule. Task 5 downloads them to a scratch directory outside the repo.
- No numeric precision threshold decides `stable` vs. `experimental` — it's a human judgment call recorded with a date and the report that backed it (see `RULE-STABILITY.md` in Task 2). No rule is promoted to `stable` in this plan — the 10-20-finding sample in Task 5 is nowhere near the ~100-example scale the roadmap's own report example implies is needed for that call.
- `tests/corpus/validation/*.ledger.json` files (both the real ones from Task 5 and the synthetic one from Task 4) are **not** part of the automated `codeqa_tests` binary — no C++ test reads them. Only `GenerateValidationReport.cmake`, invoked directly or via its own `ctest` entries, ever reads a ledger.
- Every new corpus fixture (Task 1) still needs an exhaustive `.parsing.json` per the existing rule in `tests/corpus/README.md` — `constructs`, `functions`, `classes: []`, `includes: []`, `diagnostics: []` all required, even when empty.

## File Structure

- `tests/corpus/size/large-functions.cpp` + `.parsing.json` + `.findings.json` (new) — closes `size.function-length`/`size.local-variables`.
- `tests/corpus/parameters/consecutive-booleans.cpp` + `.parsing.json` + `.findings.json` (new) — closes `signature.consecutive-booleans`.
- `tests/corpus/complexity/thresholds.cpp` + `.parsing.json` + `.findings.json` (new) — closes `complexity.cyclomatic`/`complexity.returns`/`complexity.condition`.
- `tests/unit/CorpusFindingsTest.cpp` (modified) — `fixturesWithFindings()` gains the 3 new source paths.
- `docs/03-testing-policy.md` (new) — regression-test policy paragraph.
- `tests/corpus/validation/RULE-STABILITY.md` (new) — the experimental/stable table.
- `tests/corpus/VALIDATION-METHODOLOGY.md` (new) — the end-to-end process doc, including the ledger JSON schema.
- `tests/corpus/GenerateValidationReport.cmake` (new) — the aggregation script.
- `tests/corpus/validation/fixtures/sample.ledger.json` (new) — synthetic fixture with known counts, for the script's own test.
- `tests/integration/CheckValidationReportOutput.cmake` (new) — runs the report script and checks its output, mirrors `CheckCliOutput.cmake`.
- `tests/CMakeLists.txt` (modified) — two new `add_test` entries for the report script's own test.
- `tests/corpus/validation/sqlite.ledger.json`, `tests/corpus/validation/fmt.ledger.json` (new, produced by Task 5's execution, not hand-authored here).

---

### Task 1: Close the corpus's rule-coverage gap

**Files:**
- Create: `tests/corpus/size/large-functions.cpp`, `tests/corpus/size/large-functions.parsing.json`, `tests/corpus/size/large-functions.findings.json`
- Create: `tests/corpus/parameters/consecutive-booleans.cpp`, `tests/corpus/parameters/consecutive-booleans.parsing.json`, `tests/corpus/parameters/consecutive-booleans.findings.json`
- Create: `tests/corpus/complexity/thresholds.cpp`, `tests/corpus/complexity/thresholds.parsing.json`, `tests/corpus/complexity/thresholds.findings.json`
- Modify: `tests/unit/CorpusFindingsTest.cpp`

**Interfaces:**
- Consumes: `codeqa::rules::allRules` (Fase 7/8, unchanged), `codeqa::metrics::calculateFunctionMetrics` (Fase 6b, unchanged) — this task only adds *data* (fixtures), no production code changes.
- Produces: nothing new for later tasks — this task is self-contained; Task 2-5 don't depend on it.

**Why these three files, precisely:** `tests/corpus/README.md` already states every `<name>.metrics.json` bundles size+signature+complexity per function, so "toda métrica dispone de fixtures" is already true. But `tests/unit/CorpusFindingsTest.cpp`'s `fixturesWithFindings()` only lists two fixtures today (`nesting/deep-nesting.cpp` for `complexity.nesting`, `parameters/cpp-parameters.cpp` for `signature.parameter-count`) — verified by reading every existing `.metrics.json` in the corpus: none of their functions cross any threshold for `size.function-length` (max 21 physical lines vs. warning 50), `size.local-variables` (no fixture declares 8+ locals), `signature.consecutive-booleans` (no fixture has 2+ consecutive `bool` value parameters), `complexity.cyclomatic` (max 5 vs. warning 10), `complexity.returns` (max 3 vs. warning 4), or `complexity.condition` (max 0 vs. warning 3 — `complexity/branching.cpp`'s conditions never use `&&`/`||`). These 3 new fixtures close all 6 gaps.

- [ ] **Step 1: Create the fixture sources and their expectation files**

Create `tests/corpus/size/large-functions.cpp` — `longFunction` has 1 local variable and 53 physical lines (over the `size.function-length` warning of 50, under the high of 90); `manyLocals` declares exactly 8 locals (matching the `size.local-variables` warning of 8 exactly) in a 12-line body (nowhere near 50, so it can't also trip `size.function-length`):

```cpp
int longFunction(int seed)
{
    int total = seed;
    total += 1;
    total += 2;
    total += 3;
    total += 4;
    total += 5;
    total += 6;
    total += 7;
    total += 8;
    total += 9;
    total += 10;
    total += 11;
    total += 12;
    total += 13;
    total += 14;
    total += 15;
    total += 16;
    total += 17;
    total += 18;
    total += 19;
    total += 20;
    total += 21;
    total += 22;
    total += 23;
    total += 24;
    total += 25;
    total += 26;
    total += 27;
    total += 28;
    total += 29;
    total += 30;
    total += 31;
    total += 32;
    total += 33;
    total += 34;
    total += 35;
    total += 36;
    total += 37;
    total += 38;
    total += 39;
    total += 40;
    total += 41;
    total += 42;
    total += 43;
    total += 44;
    total += 45;
    total += 46;
    total += 47;
    total += 48;
    return total;
}

int manyLocals()
{
    int a = 1;
    int b = 2;
    int c = 3;
    int d = 4;
    int e = 5;
    int f = 6;
    int g = 7;
    int h = 8;
    return a + b + c + d + e + f + g + h;
}
```

Line count check (both symbol ranges are `beginLine` to `endLine` inclusive): `longFunction` occupies lines 1-53 (signature, brace, 1 declaration, 48 `total += N;` lines, `return`, closing brace = 1+1+1+48+1+1 = 53) → `physicalLines = 53`. `manyLocals` starts after a blank separator line (line 54) at line 55 and ends at line 66 (signature, brace, 8 declarations, return, closing brace = 1+1+8+1+1 = 12 lines) → `physicalLines = 12`, `localVariables.size() = 8`.

Create `tests/corpus/size/large-functions.parsing.json`:

```json
{
  "constructs": ["CPP-FUNC-01", "CTRL-08"],
  "functions": [
    {"name": "longFunction", "kind": "free", "startLine": 1, "endLine": 53, "parameters": [{"name": "seed", "passing": "value", "hasDefaultValue": false, "type": "int"}], "isStatic": false, "isVariadic": false},
    {"name": "manyLocals", "kind": "free", "startLine": 55, "endLine": 66, "parameters": [], "isStatic": false, "isVariadic": false}
  ],
  "classes": [],
  "includes": [],
  "diagnostics": []
}
```

Create `tests/corpus/size/large-functions.findings.json`:

```json
{
  "findings": [
    {
      "symbolName": "longFunction",
      "ruleId": "size.function-length",
      "severity": "warning",
      "observedValue": 53,
      "limitValue": 50,
      "location": {"line": 1, "column": 1}
    },
    {
      "symbolName": "manyLocals",
      "ruleId": "size.local-variables",
      "severity": "warning",
      "observedValue": 8,
      "limitValue": 8,
      "location": {"line": 55, "column": 1}
    }
  ]
}
```

Create `tests/corpus/parameters/consecutive-booleans.cpp` — two consecutive `bool` *value* parameters (`verbose`, `strict`) followed by a non-bool one, matching `signature.consecutive-booleans`'s warning of 2 exactly; only 3 parameters total, safely under `signature.parameter-count`'s warning of 4:

```cpp
void enableFeatures(bool verbose, bool strict, int retries)
{
}
```

Create `tests/corpus/parameters/consecutive-booleans.parsing.json`:

```json
{
  "constructs": ["CPP-FUNC-01"],
  "functions": [
    {"name": "enableFeatures", "kind": "free", "startLine": 1, "endLine": 3, "parameters": [{"name": "verbose", "passing": "value", "hasDefaultValue": false, "type": "bool"}, {"name": "strict", "passing": "value", "hasDefaultValue": false, "type": "bool"}, {"name": "retries", "passing": "value", "hasDefaultValue": false, "type": "int"}], "isStatic": false, "isVariadic": false}
  ],
  "classes": [],
  "includes": [],
  "diagnostics": []
}
```

Create `tests/corpus/parameters/consecutive-booleans.findings.json` — `signature.consecutive-booleans` uses `parameterListRange->begin`, which (verified against the existing `manyParameters` fixture in `tests/corpus/parameters/cpp-parameters.findings.json`, whose `(` sits at column 19 for `int manyParameters(`) points at the opening `(` itself. Counting `void enableFeatures(` character by character (`void` = 4, space = 1, `enableFeatures` = 14 letters) puts `(` at column 20:

```json
{
  "findings": [
    {
      "symbolName": "enableFeatures",
      "ruleId": "signature.consecutive-booleans",
      "severity": "warning",
      "observedValue": 2,
      "limitValue": 2,
      "location": {"line": 1, "column": 20}
    }
  ]
}
```

Create `tests/corpus/complexity/thresholds.cpp` — three functions, each isolated to trip exactly one rule. `manyBranches` has 9 sequential (non-nested) `if`s, no boolean operators in any condition → `cyclomaticComplexity = 1 + 9 + 0 = 10` (matches the warning exactly), `returnCount = 1` and `maxNestingDepth ≈ 1`, both nowhere near their own rules' warnings (4 and 3). `earlyReturns` has 3 `if`s each with a single unconditional `return`, plus a trailing `return`, for `returnCount = 4` (matches the warning exactly) with `cyclomaticComplexity = 1 + 3 + 0 = 4`, safely under 10. `inRange` has one `if` whose condition chains four clauses with three `&&` operators (`maxConditionComplexity = 3`, matching the warning exactly) while reusing only 3 parameters (`value`, `lower`, `upper` — all plain `int`, so no `signature.*` rule can trip) and keeping `cyclomaticComplexity = 1 + 1 + 3 = 5`, safely under 10:

```cpp
int manyBranches(int value)
{
    int total = 0;

    if (value == 0)
    {
        total += 1;
    }
    if (value == 1)
    {
        total += 1;
    }
    if (value == 2)
    {
        total += 1;
    }
    if (value == 3)
    {
        total += 1;
    }
    if (value == 4)
    {
        total += 1;
    }
    if (value == 5)
    {
        total += 1;
    }
    if (value == 6)
    {
        total += 1;
    }
    if (value == 7)
    {
        total += 1;
    }
    if (value == 8)
    {
        total += 1;
    }

    return total;
}

int earlyReturns(int value)
{
    if (value == 0)
    {
        return 0;
    }

    if (value == 1)
    {
        return 1;
    }

    if (value == 2)
    {
        return 2;
    }

    return -1;
}

bool inRange(int value, int lower, int upper)
{
    if (value >= lower && value <= upper && value != 0 && upper != lower)
    {
        return true;
    }

    return false;
}
```

Line count check: `manyBranches` is lines 1-43 (signature, brace, declaration, blank, 9×(if-line, brace, statement, brace) = 9×4=36, blank, return, closing brace = 1+1+1+1+36+1+1+1=43). `earlyReturns` starts at line 45 (one blank separator) and ends at line 63 (signature, brace, 3×(if, brace, return, brace, blank)=3×5=15 minus the last blank which is instead the trailing return's own blank, i.e. signature+brace+ 3 if-blocks of 4 lines each with a blank after each + final return + closing brace — counted directly against the source above: lines 45-63). `inRange` starts at line 65 (one blank separator) and ends at line 73.

Create `tests/corpus/complexity/thresholds.parsing.json`:

```json
{
  "constructs": ["CPP-FUNC-01", "CTRL-01", "CTRL-07", "CTRL-08"],
  "functions": [
    {"name": "manyBranches", "kind": "free", "startLine": 1, "endLine": 43, "parameters": [{"name": "value", "passing": "value", "hasDefaultValue": false, "type": "int"}], "isStatic": false, "isVariadic": false},
    {"name": "earlyReturns", "kind": "free", "startLine": 45, "endLine": 63, "parameters": [{"name": "value", "passing": "value", "hasDefaultValue": false, "type": "int"}], "isStatic": false, "isVariadic": false},
    {"name": "inRange", "kind": "free", "startLine": 65, "endLine": 73, "parameters": [{"name": "value", "passing": "value", "hasDefaultValue": false, "type": "int"}, {"name": "lower", "passing": "value", "hasDefaultValue": false, "type": "int"}, {"name": "upper", "passing": "value", "hasDefaultValue": false, "type": "int"}], "isStatic": false, "isVariadic": false}
  ],
  "classes": [],
  "includes": [],
  "diagnostics": []
}
```

Create `tests/corpus/complexity/thresholds.findings.json` — `complexity.condition`'s location is `maxConditionRange->begin`, the start of the winning condition's own text (right after `if (`), not the function start. On line 67 (`    if (value >= lower && value <= upper && value != 0 && upper != lower)`), 4 spaces of indent + `if (` (4 chars) puts the condition's first character (`v` of `value`) at column 9:

```json
{
  "findings": [
    {
      "symbolName": "manyBranches",
      "ruleId": "complexity.cyclomatic",
      "severity": "warning",
      "observedValue": 10,
      "limitValue": 10,
      "location": {"line": 1, "column": 1}
    },
    {
      "symbolName": "earlyReturns",
      "ruleId": "complexity.returns",
      "severity": "warning",
      "observedValue": 4,
      "limitValue": 4,
      "location": {"line": 45, "column": 1}
    },
    {
      "symbolName": "inRange",
      "ruleId": "complexity.condition",
      "severity": "warning",
      "observedValue": 3,
      "limitValue": 3,
      "location": {"line": 67, "column": 9}
    }
  ]
}
```

- [ ] **Step 2: Register the new fixtures with `CorpusFindingsTest.cpp`**

In `tests/unit/CorpusFindingsTest.cpp`, replace `fixturesWithFindings()`:

```cpp
std::vector<std::filesystem::path> fixturesWithFindings(const std::filesystem::path& corpusRoot)
{
    return {
        corpusRoot / "nesting" / "deep-nesting.cpp",
        corpusRoot / "parameters" / "cpp-parameters.cpp",
        corpusRoot / "size" / "large-functions.cpp",
        corpusRoot / "parameters" / "consecutive-booleans.cpp",
        corpusRoot / "complexity" / "thresholds.cpp"};
}
```

- [ ] **Step 3: Run the corpus checks and correct any mismatch against real output**

Run (from the repository root, using whichever `cmake` executable this environment resolves — check `build/CMakeCache.txt`'s `CMAKE_COMMAND` line if `cmake` isn't on `PATH`):

```text
cmake --build build --config Debug --target codeqa_tests
ctest --test-dir build --output-on-failure -R "codeqa_tests|codeqa_corpus_integrity"
```

Expected: this is the corpus's own established fixture-authoring workflow (see `tests/corpus/README.md`) — expectations are written by hand and then checked against the real tool, not the other way around. The line numbers, `observedValue`/`limitValue`/`severity` values above were derived directly from `src/metrics/CalculateFunctionMetrics.cpp` and `src/rules/Rules.cpp`'s exact algorithms and should match. The `location.column` values for `size.*`/`complexity.cyclomatic`/`complexity.returns` (all using `symbol.range.begin`) are the one place with residual uncertainty — if `CorpusFindingsTest`'s failure output shows a different column than `1`, or `CorpusParsingTest`/`CorpusStructureTest`/`CorpusModelTest` show a different value anywhere in the three new `.parsing.json` files, copy the actual value from the test's own diff output into the expectation file and re-run. Do not guess a second time — use what the tool actually reports.

Also run the CTest corpus-integrity check specifically (it validates the `constructs` IDs and required-field shape independent of the C++ runner):

```text
ctest --test-dir build --output-on-failure -R codeqa_corpus_integrity
```

- [ ] **Step 4: Commit**

```bash
git add tests/corpus/size tests/corpus/parameters/consecutive-booleans.cpp tests/corpus/parameters/consecutive-booleans.parsing.json tests/corpus/parameters/consecutive-booleans.findings.json tests/corpus/complexity/thresholds.cpp tests/corpus/complexity/thresholds.parsing.json tests/corpus/complexity/thresholds.findings.json tests/unit/CorpusFindingsTest.cpp
git commit -m "test: add corpus fixtures closing the findings coverage gap for 6 rules"
```

---

### Task 2: Regression-test policy and rule-stability docs

**Files:**
- Create: `docs/03-testing-policy.md`
- Create: `tests/corpus/validation/RULE-STABILITY.md`

**Interfaces:**
- Consumes: nothing.
- Produces: `tests/corpus/validation/RULE-STABILITY.md`'s table format is referenced by Task 3's methodology doc (as "where stability gets recorded"), but no later task edits this file's content — this task closes the roadmap's "una regla experimental se distingue de una estable" criterion completely on its own.

- [ ] **Step 1: Write the regression-test policy doc**

Create `docs/03-testing-policy.md`:

```markdown
# Política de testing

## Test de regresión por bug

Todo bug corregido en `codeqa_core` (`include/codeqa/`, `src/`) añade el
test automatizado que lo habría atrapado — en el mismo commit que corrige
el bug, o en el commit inmediatamente siguiente si el fix y el test no
pueden ir juntos por algún motivo concreto (poco común). El test vive donde
corresponda según qué se rompió: `tests/unit/` para lógica de una unidad,
`tests/corpus/` con su fixture hermano si el bug es de extracción
estructural o de una regla sobre una construcción de código real,
`tests/integration/` si solo es observable desde el comportamiento del CLI.

Esta política es hacia adelante: no se audita retroactivamente el
historial de fixes anteriores a esta fase. Cada fase de este proyecto ya
siguió TDD estrictamente (evidencia RED/GREEN documentada en cada task de
cada plan de implementación en `docs/superpowers/plans/`), así que no hay
arqueología pendiente.
```

- [ ] **Step 2: Write the rule-stability table**

Create `tests/corpus/validation/RULE-STABILITY.md`:

```markdown
# Estabilidad de reglas

Ninguna de las 8 reglas fue validada contra proyectos reales antes de la
Fase 11 (ver `docs/superpowers/specs/2026-08-09-fase-7-motor-reglas-design.md`
§6) — todas arrancan `experimental`. La promoción a `stable` es una
decisión humana respaldada por un informe real de
`GenerateValidationReport.cmake` con una muestra representativa (el roadmap
usa el orden de magnitud de 100 ejemplos revisados por regla) — nunca un
umbral numérico automático. Cada fila que cambia de `experimental` a
`stable` agrega la fecha y qué informe la respalda.

| Regla | Estado | Desde | Informe que respalda la decisión |
|---|---|---|---|
| `size.function-length` | `experimental` | — | — |
| `size.local-variables` | `experimental` | — | — |
| `signature.parameter-count` | `experimental` | — | — |
| `signature.consecutive-booleans` | `experimental` | — | — |
| `complexity.cyclomatic` | `experimental` | — | — |
| `complexity.returns` | `experimental` | — | — |
| `complexity.condition` | `experimental` | — | — |
| `complexity.nesting` | `experimental` | — | — |
```

- [ ] **Step 3: Commit**

```bash
git add docs/03-testing-policy.md tests/corpus/validation/RULE-STABILITY.md
git commit -m "docs: add regression-test policy and rule-stability tracking"
```

---

### Task 3: Ledger schema and validation methodology doc

**Files:**
- Create: `tests/corpus/VALIDATION-METHODOLOGY.md`

**Interfaces:**
- Consumes: `tests/corpus/REAL-PROJECTS.md` (Fase 0, unchanged — the pinned SQLite/fmt versions), `codeqa analyze --format json` (Fase 9, unchanged).
- Produces: the ledger JSON schema (documented here, not code) that Task 4's synthetic fixture and Task 5's real ledgers both instantiate: a top-level `{project, projectVersion, findings: [...]}` object; each finding has `ruleId`/`file`/`symbolName`/`location{line,column}`/`severity`/`observedValue`/`limitValue` (copied from `analyze --format json`'s own finding shape) plus `proposal{verdict, by, rationale}` and `decision{verdict, by, date, notes}`, where `verdict` is `"accepted"` or `"rejected"`.

- [ ] **Step 1: Write the methodology doc**

Create `tests/corpus/VALIDATION-METHODOLOGY.md`:

```markdown
# Metodología de validación contra proyectos reales

Este documento describe cómo revisar hallazgos de `codeqa` contra un
proyecto real (hoy: SQLite y fmt, fijados en `REAL-PROJECTS.md`) y producir
el informe de precisión por regla que pide la Fase 11 del roadmap. Está
pensado para que alguien que no vivió la sesión original pueda repetir el
proceso — incluido correrlo más adelante contra un codebase que no puede
aparecer en este repositorio.

## 1. Obtener el proyecto real

Bajar exactamente la versión fijada en `REAL-PROJECTS.md` a un directorio
fuera del repositorio (nunca se commitea código de terceros aquí). Por
ejemplo, para SQLite 3.46.0:

```
https://www.sqlite.org/2024/sqlite-amalgamation-3460000.zip
```

y para fmt 10.2.1:

```
https://github.com/fmtlib/fmt/archive/refs/tags/10.2.1.tar.gz
```

## 2. Correr el análisis

```
codeqa analyze <path-al-proyecto-real> --format json --output <tmp>.json
```

## 3. Elegir una muestra y revisarla

Elegir hallazgos del JSON resultante — cuantos más, mejor, pero incluso una
muestra chica prueba el mecanismo. Para cada uno:

1. Un agente lee el hallazgo (`ruleId`, `symbolName`, `location`,
   `observedValue`, `limitValue`) junto con el código real alrededor de esa
   ubicación, y propone un veredicto: `"accepted"` (el hallazgo es
   legítimo, verdadero positivo) o `"rejected"` (falso positivo — la regla
   disparó pero el código está bien), con una razón breve.
2. Un humano confirma el veredicto propuesto o lo corrige. Solo la decisión
   humana cuenta para el informe — el agente propone, nunca decide.

## 4. Registrar en el ledger

Cada hallazgo revisado se agrega a `tests/corpus/validation/<proyecto>.ledger.json`:

```json
{
  "project": "sqlite",
  "projectVersion": "3.46.0",
  "findings": [
    {
      "ruleId": "complexity.cyclomatic",
      "file": "sqlite3.c",
      "symbolName": "sqlite3VdbeExec",
      "location": { "line": 12345, "column": 1 },
      "severity": "error",
      "observedValue": 340,
      "limitValue": 15,
      "proposal": {
        "verdict": "accepted",
        "by": "agent",
        "rationale": "Función de ~2000 líneas con dispatch de opcodes; la complejidad es real, no un artefacto del analizador."
      },
      "decision": {
        "verdict": "accepted",
        "by": "human",
        "date": "2026-08-12",
        "notes": ""
      }
    }
  ]
}
```

Un hallazgo revisado sin ayuda de agente tiene `proposal.by == "human"` y
`proposal`/`decision` coinciden. `tests/corpus/validation/*.ledger.json` no
forma parte de la suite automatizada (`ctest`/`codeqa_tests`) — requiere
juicio humano, no debería bloquear un build.

## 5. Generar el informe

```
cmake "-DCODEQA_LEDGER_FILES=tests/corpus/validation/sqlite.ledger.json;tests/corpus/validation/fmt.ledger.json" -P tests/corpus/GenerateValidationReport.cmake
```

Imprime un bloque por regla (`Rule`, `Examples reviewed`, `Accepted`,
`Rejected`, `Precision`), agregando todos los ledgers dados. Solo cuenta
`decision.verdict`, nunca `proposal.verdict`.

## 6. Promover una regla a estable

Cuando el informe acumule una muestra representativa para una regla (el
roadmap usa el orden de magnitud de 100 ejemplos), un humano decide si
promoverla a `stable` en `tests/corpus/validation/RULE-STABILITY.md`,
anotando la fecha y qué informe respalda la decisión. No hay un umbral de
precisión fijo — es juicio informado, no un cálculo automático.
```

- [ ] **Step 2: Commit**

```bash
git add tests/corpus/VALIDATION-METHODOLOGY.md
git commit -m "docs: add real-project validation methodology and ledger schema"
```

---

### Task 4: `GenerateValidationReport.cmake` and its own test

**Files:**
- Create: `tests/corpus/GenerateValidationReport.cmake`
- Create: `tests/corpus/validation/fixtures/sample.ledger.json`
- Create: `tests/integration/CheckValidationReportOutput.cmake`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: the ledger JSON schema from Task 3 (`{project, projectVersion, findings: [{ruleId, ..., decision: {verdict, ...}}]}`).
- Produces: `tests/corpus/GenerateValidationReport.cmake`, invoked as `cmake "-DCODEQA_LEDGER_FILES=<path1>[;<path2>...]" -P tests/corpus/GenerateValidationReport.cmake`, printing one block per `ruleId` (sorted) in the roadmap's exact format. Used by Task 5's real run.

- [ ] **Step 1: Write the synthetic fixture ledger with known counts**

Create `tests/corpus/validation/fixtures/sample.ledger.json` — 4 `complexity.cyclomatic` findings (3 accepted, 1 rejected → 75% precision) and 2 `signature.parameter-count` findings (1 accepted, 1 rejected → 50% precision), both fractions chosen to divide evenly so there's no integer-rounding ambiguity to reason about:

```json
{
  "project": "sample",
  "projectVersion": "0.0.0",
  "findings": [
    {"ruleId": "complexity.cyclomatic", "file": "a.cpp", "symbolName": "f1", "location": {"line": 1, "column": 1}, "severity": "warning", "observedValue": 12, "limitValue": 10, "proposal": {"verdict": "accepted", "by": "agent", "rationale": "real complexity"}, "decision": {"verdict": "accepted", "by": "human", "date": "2026-01-01", "notes": ""}},
    {"ruleId": "complexity.cyclomatic", "file": "a.cpp", "symbolName": "f2", "location": {"line": 10, "column": 1}, "severity": "warning", "observedValue": 11, "limitValue": 10, "proposal": {"verdict": "accepted", "by": "agent", "rationale": "real complexity"}, "decision": {"verdict": "accepted", "by": "human", "date": "2026-01-01", "notes": ""}},
    {"ruleId": "complexity.cyclomatic", "file": "a.cpp", "symbolName": "f3", "location": {"line": 20, "column": 1}, "severity": "error", "observedValue": 16, "limitValue": 15, "proposal": {"verdict": "accepted", "by": "agent", "rationale": "real complexity"}, "decision": {"verdict": "accepted", "by": "human", "date": "2026-01-01", "notes": ""}},
    {"ruleId": "complexity.cyclomatic", "file": "a.cpp", "symbolName": "f4", "location": {"line": 30, "column": 1}, "severity": "warning", "observedValue": 10, "limitValue": 10, "proposal": {"verdict": "rejected", "by": "agent", "rationale": "table-driven dispatch"}, "decision": {"verdict": "rejected", "by": "human", "date": "2026-01-01", "notes": "confirmed: generated dispatch table, not organic complexity"}},
    {"ruleId": "signature.parameter-count", "file": "b.cpp", "symbolName": "g1", "location": {"line": 1, "column": 10}, "severity": "warning", "observedValue": 5, "limitValue": 4, "proposal": {"verdict": "accepted", "by": "human", "rationale": ""}, "decision": {"verdict": "accepted", "by": "human", "date": "2026-01-01", "notes": ""}},
    {"ruleId": "signature.parameter-count", "file": "b.cpp", "symbolName": "g2", "location": {"line": 10, "column": 10}, "severity": "warning", "observedValue": 4, "limitValue": 4, "proposal": {"verdict": "rejected", "by": "human", "rationale": ""}, "decision": {"verdict": "rejected", "by": "human", "date": "2026-01-01", "notes": "builder-pattern constructor, each param independently meaningful"}}
  ]
}
```

- [ ] **Step 2: Write the report script**

Create `tests/corpus/GenerateValidationReport.cmake`:

```cmake
# Aggregates one or more validation ledger JSON files (see
# tests/corpus/VALIDATION-METHODOLOGY.md for the schema) into a per-rule
# precision report. Not part of the build or of any ctest run against real
# data -- a maintainer runs this by hand against
# tests/corpus/validation/*.ledger.json (its own synthetic-fixture test is
# in tests/integration/CheckValidationReportOutput.cmake).
#
# Deliberately requires CMake >= 3.19 for string(JSON ...), unlike the rest
# of this project's cmake_minimum_required(VERSION 3.16) floor -- this
# script is never invoked by the configure/build step, only manually or by
# its own test, so the higher floor doesn't affect anyone building codeqa.
#
# Required variable:
#   CODEQA_LEDGER_FILES - semicolon-separated list of ledger JSON file paths

if(NOT DEFINED CODEQA_LEDGER_FILES)
    message(FATAL_ERROR "CODEQA_LEDGER_FILES must be defined.")
endif()

# CMake has no map type. Collect every (ruleId, verdict) pair across all
# ledgers as two parallel flat lists first, then aggregate per rule in a
# second pass -- simpler and less error-prone than maintaining running
# counts indexed into parallel lists.
set(allRuleIds "")
set(allVerdicts "")

foreach(ledgerFile IN LISTS CODEQA_LEDGER_FILES)
    if(NOT EXISTS "${ledgerFile}")
        message(FATAL_ERROR "Ledger file '${ledgerFile}' does not exist.")
    endif()

    file(READ "${ledgerFile}" ledgerText)

    string(JSON findingsCount ERROR_VARIABLE lengthError LENGTH "${ledgerText}" "findings")
    if(lengthError)
        message(FATAL_ERROR "Ledger '${ledgerFile}' is not valid JSON or has no 'findings' array: ${lengthError}")
    endif()

    if(findingsCount GREATER 0)
        math(EXPR lastIndex "${findingsCount} - 1")

        foreach(index RANGE ${lastIndex})
            string(JSON ruleId GET "${ledgerText}" "findings" ${index} "ruleId")
            string(JSON verdict GET "${ledgerText}" "findings" ${index} "decision" "verdict")

            if(NOT verdict STREQUAL "accepted" AND NOT verdict STREQUAL "rejected")
                message(FATAL_ERROR
                    "Ledger '${ledgerFile}' finding ${index} has an unknown decision.verdict "
                    "'${verdict}' (expected 'accepted' or 'rejected').")
            endif()

            list(APPEND allRuleIds "${ruleId}")
            list(APPEND allVerdicts "${verdict}")
        endforeach()
    endif()
endforeach()

list(LENGTH allRuleIds totalFindings)

if(totalFindings EQUAL 0)
    message(STATUS "No findings in the given ledger(s).")
    return()
endif()

math(EXPR lastFindingIndex "${totalFindings} - 1")

set(uniqueRuleIds ${allRuleIds})
list(REMOVE_DUPLICATES uniqueRuleIds)
list(SORT uniqueRuleIds)

foreach(ruleId IN LISTS uniqueRuleIds)
    set(accepted 0)
    set(rejected 0)

    foreach(index RANGE ${lastFindingIndex})
        list(GET allRuleIds ${index} currentRuleId)

        if(currentRuleId STREQUAL ruleId)
            list(GET allVerdicts ${index} currentVerdict)

            if(currentVerdict STREQUAL "accepted")
                math(EXPR accepted "${accepted} + 1")
            else()
                math(EXPR rejected "${rejected} + 1")
            endif()
        endif()
    endforeach()

    math(EXPR reviewed "${accepted} + ${rejected}")
    math(EXPR precisionPercent "(${accepted} * 100) / ${reviewed}")

    message(STATUS "Rule: ${ruleId}")
    message(STATUS "Examples reviewed: ${reviewed}")
    message(STATUS "Accepted: ${accepted}")
    message(STATUS "Rejected: ${rejected}")
    message(STATUS "Precision: ${precisionPercent} %")
    message(STATUS "")
endforeach()
```

- [ ] **Step 3: Write the test that runs the script against the synthetic fixture**

Create `tests/integration/CheckValidationReportOutput.cmake`:

```cmake
# Runs GenerateValidationReport.cmake against a fixed ledger and checks its output for an
# expected substring. Mirrors CheckCliOutput.cmake's pattern for the codeqa binary itself.
# Required variables (passed via -D on the command line):
#   CODEQA_CMAKE            - path to a CMake executable able to run GenerateValidationReport.cmake
#   CODEQA_REPORT_SCRIPT     - path to GenerateValidationReport.cmake
#   CODEQA_LEDGER_FILE       - path to the ledger to run it against
#   CODEQA_EXPECTED_OUTPUT   - substring that must appear in the script's output

execute_process(
    COMMAND ${CODEQA_CMAKE} "-DCODEQA_LEDGER_FILES=${CODEQA_LEDGER_FILE}" -P ${CODEQA_REPORT_SCRIPT}
    OUTPUT_VARIABLE actualOutput
    ERROR_VARIABLE actualError
    RESULT_VARIABLE actualExitCode
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT actualExitCode EQUAL 0)
    message(FATAL_ERROR
        "Expected exit code 0 but got ${actualExitCode}.\nstdout:\n${actualOutput}\nstderr:\n${actualError}")
endif()

# message(STATUS ...) is documented to print to stdout, but this checks both streams --
# cheap insurance against relying on an exact channel for a message() mode.
set(combinedOutput "${actualOutput}${actualError}")
string(FIND "${combinedOutput}" "${CODEQA_EXPECTED_OUTPUT}" matchPosition)

if(matchPosition EQUAL -1)
    message(FATAL_ERROR
        "Expected output to contain:\n${CODEQA_EXPECTED_OUTPUT}\n"
        "Actual stdout was:\n${actualOutput}\nActual stderr was:\n${actualError}")
endif()

message(STATUS "Validation report output check passed.")
```

In `tests/CMakeLists.txt`, add these two `add_test` entries, right after `codeqa_integration_parse_corpus_unclosed_brace` (the last entry):

```cmake
add_test(
    NAME codeqa_validation_report_sample_rule
    COMMAND ${CMAKE_COMMAND}
        "-DCODEQA_CMAKE=${CMAKE_COMMAND}"
        "-DCODEQA_REPORT_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/corpus/GenerateValidationReport.cmake"
        "-DCODEQA_LEDGER_FILE=${CMAKE_CURRENT_SOURCE_DIR}/corpus/validation/fixtures/sample.ledger.json"
        "-DCODEQA_EXPECTED_OUTPUT=Rule: complexity.cyclomatic"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/integration/CheckValidationReportOutput.cmake
)

add_test(
    NAME codeqa_validation_report_sample_precision
    COMMAND ${CMAKE_COMMAND}
        "-DCODEQA_CMAKE=${CMAKE_COMMAND}"
        "-DCODEQA_REPORT_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/corpus/GenerateValidationReport.cmake"
        "-DCODEQA_LEDGER_FILE=${CMAKE_CURRENT_SOURCE_DIR}/corpus/validation/fixtures/sample.ledger.json"
        "-DCODEQA_EXPECTED_OUTPUT=Precision: 75 %"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/integration/CheckValidationReportOutput.cmake
)
```

- [ ] **Step 4: Run to verify RED, then GREEN**

Run (the CMake used to configure `build/` might be older than 3.19 — check `build/CMakeCache.txt`'s `CMAKE_COMMAND` line; if that binary's own `--version` is below 3.19, this task still needs *a* CMake ≥3.19 available on the machine to run the script itself, per the Global Constraint — the `build/`'s own configure does not need to change):

```text
cmake --build build --config Debug --target codeqa_tests
ctest --test-dir build --output-on-failure -R codeqa_validation_report_sample
```

Expected on the first attempt: likely FAIL — this is genuinely new CMake script code with no prior compile-time check, so treat any `string(JSON ...)` syntax error or list-manipulation mistake in `GenerateValidationReport.cmake` as normal RED-phase feedback. Fix based on the actual error message (CMake reports the exact line and command that failed) and re-run until both new tests pass. Also manually eyeball a full run's output once:

```text
cmake "-DCODEQA_LEDGER_FILES=tests/corpus/validation/fixtures/sample.ledger.json" -P tests/corpus/GenerateValidationReport.cmake
```

Expected: two full blocks, `complexity.cyclomatic` (reviewed 4, accepted 3, rejected 1, precision 75 %) before `signature.parameter-count` (reviewed 2, accepted 1, rejected 1, precision 50 %) — alphabetical order.

- [ ] **Step 5: Commit**

```bash
git add tests/corpus/GenerateValidationReport.cmake tests/corpus/validation/fixtures/sample.ledger.json tests/integration/CheckValidationReportOutput.cmake tests/CMakeLists.txt
git commit -m "feat: add GenerateValidationReport.cmake to aggregate validation ledgers into a per-rule precision report"
```

---

### Task 5: Prove the pipeline end-to-end against real SQLite/fmt findings

**This task is not a normal autonomous implementer task.** It has a mandatory human-in-the-loop step (a real person must confirm or override each proposed verdict — "el agente no se considera fuente de verdad" is a hard constraint, not a suggestion). Structure the dispatch in two phases using the existing subagent-driven-development `NEEDS_CONTEXT` mechanism — do not try to get the implementer to guess or auto-approve its own proposals.

**Files:**
- Create: `tests/corpus/validation/sqlite.ledger.json`, `tests/corpus/validation/fmt.ledger.json`

**Interfaces:**
- Consumes: `codeqa analyze --format json` (Fase 9, unchanged), `tests/corpus/GenerateValidationReport.cmake` (Task 4), the ledger schema (Task 3).
- Produces: nothing later tasks depend on — this is the last task in the plan.

- [ ] **Step 1 (agent phase): download, analyze, and propose**

Dispatch an implementer with these instructions (adapt the brief, don't paste this plan file into the dispatch):

1. Download SQLite 3.46.0 amalgamation from `https://www.sqlite.org/2024/sqlite-amalgamation-3460000.zip` and fmt 10.2.1 from `https://github.com/fmtlib/fmt/archive/refs/tags/10.2.1.tar.gz`, into a scratch directory outside the repository (both URLs were confirmed reachable — `curl -sI` returned `200`/`302` respectively — during this plan's brainstorming).
2. Run `codeqa analyze <path> --format json --output <tmp>/sqlite.json` and the same for fmt's `include/fmt/`+`src/` directories (per `REAL-PROJECTS.md`'s "directorios de interés").
3. Pick 10-20 findings combined across both projects, covering as many distinct `ruleId`s as the sample naturally contains (no need to force coverage of all 8 — a small sample won't hit everything, and that's expected per this plan's Global Constraints).
4. For each chosen finding, read the real source around its `location` and write a proposed `verdict` (`"accepted"`/`"rejected"`) with a one-to-two-sentence `rationale`, in the shape Task 3's ledger schema expects (`ruleId`, `file`, `symbolName`, `location`, `severity`, `observedValue`, `limitValue`, `proposal: {verdict, by: "agent", rationale}` — no `decision` yet).
5. Write these as two draft files (`sqlite.proposals.json`, `fmt.proposals.json`, same shape as the final ledgers but without `decision`) to the task's own report-file directory (not `tests/corpus/validation/` — these are drafts, not the final ledgers).
6. Report status `NEEDS_CONTEXT`: "Proposals ready for human review at `<path>`; need the human's accept/override decision on each before I can write the final ledgers."

- [ ] **Step 2 (controller + human phase): collect decisions**

Read the draft proposal files. Present each finding's `ruleId`/`symbolName`/`file`/`location`/agent-proposed-verdict/rationale to the human (a table is more scannable than one question per finding for 10-20 items) and collect, for each: confirm or override, plus optional notes. Do not finalize any entry without an explicit human response for it.

- [ ] **Step 3 (agent phase, resumed): write the final ledgers and prove the report**

Resume the same implementer with the human's decisions. It should:

1. Write `tests/corpus/validation/sqlite.ledger.json` and `tests/corpus/validation/fmt.ledger.json`, each finding now carrying both `proposal` (unchanged from Step 1) and `decision` (`verdict` = the human's final call, `by` = a human identifier, `date` = today, `notes` = whatever the human added, empty string if nothing).
2. Run the report script over both:

```text
cmake "-DCODEQA_LEDGER_FILES=tests/corpus/validation/sqlite.ledger.json;tests/corpus/validation/fmt.ledger.json" -P tests/corpus/GenerateValidationReport.cmake
```

3. Verify by hand that the printed `Accepted`/`Rejected`/`Precision` for each rule matches a manual count of that rule's entries in the two ledger files — this is the task's actual "test," since there's no C++ assertion covering real, human-judged data (matches this plan's Global Constraint that real ledgers stay outside `ctest`).
4. Report back the full report output and the manual-count cross-check in its report file.

- [ ] **Step 4: Commit**

```bash
git add tests/corpus/validation/sqlite.ledger.json tests/corpus/validation/fmt.ledger.json
git commit -m "test: record a real-project validation sample against SQLite 3.46.0 and fmt 10.2.1"
```

---

## Post-plan check

After Task 5, re-read `docs/superpowers/specs/2026-08-12-fase-11-validacion-sistematica-design.md` §7 (Criterios de aceptación) against the finished result: every metric has fixtures (already true, confirmed in Task 1's own investigation); every rule now has at least one `findings.json` fixture (Task 1); the regression-test policy is written down (Task 2); false positives are recorded — every `decision.verdict == "rejected"` entry in the two real ledgers (Task 5) is exactly that; experimental/stable is distinguished and all 8 rules correctly stay `experimental` (Task 2, confirmed unchanged after Task 5's small sample); the agent's proposal is never authoritative — `GenerateValidationReport.cmake` only ever reads `decision` (Task 4); and every decision is revisable — the ledgers are plain versioned JSON with `by`/`date`/`notes` on every entry (Task 3 schema, Task 5 data).
