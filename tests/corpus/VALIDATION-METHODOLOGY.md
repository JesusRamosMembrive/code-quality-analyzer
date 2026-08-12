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
