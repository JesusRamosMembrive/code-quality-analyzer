# Fase 11 — Validación sistemática: diseño

Fecha: 2026-08-12
Estado: revisado (brainstorming 2026-08-12); pendiente de aprobación final

## 1. Objetivo

Medir la exactitud y utilidad del analizador (roadmap, Fase 11). El roadmap
mezcla dos tipos de trabajo bajo un mismo objetivo: disciplina de tests ya
en marcha desde Fase 0 (Grupo A) y un programa de validación de precisión
contra proyectos reales que no existe todavía (Grupo B). Esta fase entrega
ambos como un solo spec/plan, porque A es pequeño y se apoya en el mismo
corpus que B usa.

## 2. Alcance y límites

Esta fase incluye:

- **Grupo A** — auditoría de cobertura de fixtures por métrica; política
  escrita de "todo bug fix agrega un test de regresión"; metadata
  `experimental`/`stable` por regla.
- **Grupo B** — un ledger versionado de hallazgos revisados contra SQLite
  3.46.0 y fmt 10.2.1 (las versiones que `tests/corpus/REAL-PROJECTS.md` ya
  fija), un script que agrega el ledger en el informe por regla que pide el
  roadmap, un documento de metodología, y una muestra chica (10-20
  hallazgos reales, no sintéticos) que prueba el flujo completo de punta a
  punta — incluyendo un agente proponiendo veredicto y un humano
  confirmándolo, tal como pide el criterio de aceptación.

Esta fase **no** incluye, y por qué:

- **La revisión completa de ~100 ejemplos por regla.** Es trabajo humano
  genuino (leer cada hallazgo contra el código real y juzgarlo), no algo
  que un ciclo de brainstorm→plan→subagentes pueda completar por sí mismo.
  Esta fase entrega la infraestructura y la prueba con una muestra chica;
  la revisión a gran escala es trabajo humano futuro que reutiliza esa
  infraestructura sin volver a diseñarla.
- **Promoción de ninguna regla a `stable`.** Con una muestra de 10-20
  hallazgos repartidos entre 8 reglas, ningún denominador por regla se
  acerca a un tamaño que justifique la decisión — todas quedan
  `experimental` al cierre de esta fase, como se espera.
- **Un umbral numérico fijo de precisión para `stable`.** Decisión humana
  informada por el informe, sin número inventado — mismo principio que
  `REAL-PROJECTS.md` ya aplicó a la tasa de regiones `ERROR` ("no existe
  ningún dato que lo respalde, e inventarlo sería... el tipo de certeza
  falsa que este proyecto rechaza por principio").
- **`stability` como campo de `codeqa.json`/`ProjectConfig`.** El roadmap lo
  pide como criterio de *validación* (Fase 11), no como feature de producto
  — no aparece en la lista "el MVP deberá producir" de Fase 12. Meterlo en
  `RuleConfig` complicaría `mergeConfig` (reemplaza el `RuleConfig`
  completo por id; si `stability` no viene en el JSON de usuario, cualquier
  override de un rule-id perdería su stability real) y obligaría a tocar
  todos los fixtures de config existentes. Queda fuera de `codeqa_core` por
  completo — ver §3.
- **Auditoría retroactiva de fixes pasados sin test de regresión.** Cada
  fase anterior ya siguió TDD estrictamente (evidencia RED/GREEN en cada
  task de cada plan); la política que esta fase escribe es hacia adelante,
  no arqueología del historial.
- **Bajar el mínimo de CMake del proyecto.** Se mantiene
  `cmake_minimum_required(VERSION 3.16)` intacto — ver §5.3.

## 3. Grupo A — disciplina de tests y metadata de reglas

**Auditoría de fixtures por métrica:** cada `<nombre>.metrics.json` del
corpus ya agrupa `size`/`signature`/`complexity` por función (ver
`tests/corpus/README.md` §Métricas) — la tarea es confirmar que las 8
reglas (`docs/superpowers/specs/2026-08-09-fase-7-motor-reglas-design.md`
§6) tienen al menos un fixture que las ejercita con un `findings.json`
sibling, y agregar el que falte. Bajo riesgo: no se espera encontrar un
hueco real dado que Fase 7 ya construyó dos fixtures (`nesting/`,
`parameters/`) específicamente para esto.

**Política de test de regresión:** un párrafo en `docs/03-testing-policy.md`
(doc nuevo, siguiendo la numeración ya usada por `docs/00-project-overview.md`,
`docs/01-cpp-mvp-roadmap.md`, `docs/02-phase-01-project-foundation.md`; no
`AGENTS.md`, que está sin trackear en este repo y se deja intacto) indicando
que todo bug corregido en `codeqa_core` añade el test que lo habría
atrapado, en el mismo commit o el inmediatamente siguiente.

**`experimental`/`stable`:** una tabla en `tests/corpus/validation/RULE-STABILITY.md`
(doc nuevo, no código — el ejemplo de "Resultado esperado" del roadmap para
el informe por regla no incluye un campo de stability, así que no hace
falta que `GenerateValidationReport.cmake` la lea; basta con que exista y
sea consultable). Fuera de `codeqa_core`/`codeqa.json` por completo (ver
§2). Las 8 reglas arrancan `experimental` (ninguna validada todavía, per
Fase 7); la promoción a `stable` es una edición manual de esa tabla,
documentada con la fecha y el informe que la respalda, después de que la
revisión a gran escala (fuera de esta fase) acumule suficientes ejemplos.

## 4. Grupo B — arquitectura

```
tests/corpus/REAL-PROJECTS.md (Fase 0, sin cambios)
        │ versión fijada de SQLite/fmt
        ▼
codeqa analyze <path real> --format json --output <tmp>.json   (Fase 9, sin cambios)
        │ hallazgos reales
        ▼
[selección de una muestra + revisión humana asistida por agente]
        │
        ▼
tests/corpus/validation/{sqlite,fmt}.ledger.json   (nuevo, fuera de la suite automatizada)
        │
        ▼
tests/corpus/GenerateValidationReport.cmake   (nuevo, cmake -P, CMake ≥3.19)
        │
        ▼
Informe por regla (formato exacto del roadmap, impreso a stdout)
```

Nada de esto toca `codeqa_core` ni el binario `codeqa` — `analyze
--format json` ya existe (Fase 9) y es exactamente lo que este flujo
necesita para producir hallazgos sobre un proyecto real.

### 4.1 Ledger

Un archivo por proyecto real, `tests/corpus/validation/<proyecto>.ledger.json`:

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

- `ruleId`/`symbolName`/`file` reusan la misma noción de identidad que
  `codeqa::history` ya usa para snapshots (Fase 10) — no se inventa una
  segunda forma de identificar un finding.
- `location`/`severity`/`observedValue`/`limitValue` son contexto para que
  quien revise no tenga que volver a correr `analyze` — se copian tal cual
  del JSON que produjo `toReportJson`.
- `proposal` y `decision` están separados a propósito: `proposal` es lo que
  el agente sugiere (`by: "agent"`), `decision` es lo único que cuenta para
  el informe (`by` es un identificador humano). Un finding revisado sin
  ayuda de agente tiene `proposal.by == "human"` y `proposal`/`decision`
  coinciden.
- `verdict`: `"accepted"` = el hallazgo es legítimo (verdadero positivo);
  `"rejected"` = falso positivo, la regla disparó pero el código está bien.
- No forma parte de la suite automatizada (ni `ctest`, ni `codeqa_tests`) —
  mismo principio que `REAL-PROJECTS.md` ya declara para todo lo que toca
  proyectos reales: requiere juicio humano, no debería bloquear un build.

### 4.2 Informe de precisión — por qué CMake y por qué no como el resto del proyecto

El precedente obvio era imitar `tests/corpus/CheckCorpusIntegrity.cmake`
(mismo lenguaje, mismo patrón `cmake -P`). Pero ese archivo evita a
propósito `string(JSON ...)` — su propio comentario dice: "A simple
key-presence regex is all CMake 3.16 offers (no JSON parsing until Fase
4)" — porque el proyecto declara `cmake_minimum_required(VERSION 3.16)` y
`string(JSON ...)` recién existe desde CMake 3.19. Agrupar por `ruleId` y
sumar `accepted`/`rejected` con solo regex sobre un ledger con objetos
anidados sería mucho más frágil que con un parser real.

La resolución: este script **no participa del build** — nadie lo invoca
desde `add_test`/`ctest` con datos reales (sí tiene su propio test con un
ledger sintético, ver §6), lo corre un mantenedor a mano de vez en cuando.
No está atado al piso de 3.16 que protege la *compilación* del proyecto.
Puede declarar su propia necesidad de CMake ≥3.19 y usar `string(JSON
...)` de verdad, documentado con un comentario en el propio archivo (mismo
estilo que `CheckCorpusIntegrity.cmake` documenta su propia restricción).
El CMake instalado en este entorno es 3.31.6, así que no es una fricción
práctica. `cmake_minimum_required(VERSION 3.16)` en el `CMakeLists.txt`
raíz **no cambia**.

`tests/corpus/GenerateValidationReport.cmake`, invocado con la misma
convención `-D` que ya usa `CheckCorpusIntegrity.cmake` — una lista
separada por `;` de rutas de ledger en una sola variable (mismo patrón que
`tests/CMakeLists.txt` ya usa para pasar listas vía `$<SEMICOLON>` a otros
scripts):

```
cmake "-DCODEQA_LEDGER_FILES=tests/corpus/validation/sqlite.ledger.json;tests/corpus/validation/fmt.ledger.json" -P tests/corpus/GenerateValidationReport.cmake
```

Rutas explícitas, no un `file(GLOB_RECURSE ...)` sobre
`tests/corpus/validation/` — ese directorio también aloja el fixture
sintético del propio script en `fixtures/` (§6); un glob recursivo mezclaría
ese ledger de prueba con los ledgers reales en cualquier corrida del
informe real. El script lee cada ledger de `CODEQA_LEDGER_FILES`, agrupa
todos los `findings[]` combinados por `ruleId` (across ledgers — el informe
no se separa por proyecto, tal como muestra el ejemplo del roadmap), cuenta
`decision.verdict` (nunca `proposal.verdict` — la decisión humana es lo
único que cuenta), e imprime un bloque por regla, ordenado por `ruleId`:

```text
Rule: complexity.cyclomatic
Examples reviewed: 100
Accepted: 98
Rejected: 2
Precision: 98 %
```

### 4.3 Metodología

`tests/corpus/VALIDATION-METHODOLOGY.md`, documento nuevo: describe el
flujo de §4 en prosa para quien retome la revisión a gran escala después de
esta fase — bajar la versión fijada (sin commitear código, mismo principio
que `REAL-PROJECTS.md`), correr `analyze --format json`, cómo un agente
propone veredicto+razón, cómo un humano confirma o corrige, cómo se agrega
al ledger, cómo se corre el informe. Es el artefacto que hace este proceso
repetible por alguien que no vivió esta sesión — incluido el propio
usuario, más adelante, contra el codebase confidencial que no puede
aparecer en este repositorio.

## 5. Flujo de la muestra chica (ejecución de esta fase, no código que se ship-ea)

1. Bajar SQLite 3.46.0 (amalgamación) y fmt 10.2.1 a un directorio fuera
   del repo (confirmado alcanzable: ambas URLs fijadas en
   `REAL-PROJECTS.md` responden `200`/`302` hoy).
2. Correr `codeqa analyze <path> --format json --output <tmp>.json` sobre
   cada uno, con la config por defecto.
3. Elegir 10-20 hallazgos combinados, cubriendo varias reglas distintas
   donde sea posible (no todas las 8 van a aparecer necesariamente en una
   muestra chica — eso es aceptable, el objetivo es probar el mecanismo,
   no la cobertura).
4. Un subagente (`general-purpose`) lee cada hallazgo más el código real
   alrededor y propone `accepted`/`rejected` + razón — esta es la
   "revisión auxiliar mediante agentes" del roadmap, ejecutada de verdad,
   no solo documentada en la metodología.
5. El humano confirma o corrige cada propuesta.
6. Se escriben las entradas en `tests/corpus/validation/{sqlite,fmt}.ledger.json`.
7. Se corre `GenerateValidationReport.cmake` sobre ambos ledgers y se
   confirma que el informe agregado coincide con lo que un conteo manual
   del ledger predice.

## 6. Manejo de errores y verificación

- `GenerateValidationReport.cmake` sobre un ledger inexistente o JSON
  inválido: `FATAL_ERROR` con la ruta del archivo — mismo estilo que
  `CheckCorpusIntegrity.cmake`. No hay modo "silencioso" para este tooling:
  es de uso manual, un mantenedor corriéndolo con un typo en la ruta debe
  enterarse inmediato, no recibir un informe vacío.
- **El script tiene su propio test determinístico**, distinto de los
  ledgers reales: un fixture sintético
  `tests/corpus/validation/fixtures/sample.ledger.json` con conteos
  conocidos a mano (ej. 3 `accepted`/1 `rejected` en una regla, 1/1 en
  otra), más un `add_test` nuevo en `tests/CMakeLists.txt` que sigue el
  mismo patrón que `tests/integration/CheckCliOutput.cmake` ya establece
  (`execute_process` + comparación de substring en stdout) — un
  `CheckValidationReportOutput.cmake` que corre el script contra el
  fixture sintético y verifica el bloque de salida esperado. Esto **sí**
  entra en el `ctest` normal, a diferencia de los ledgers reales de
  SQLite/fmt: se está probando la corrección del script, no juzgando
  hallazgos reales.
- La muestra chica de §5 no tiene test automatizado — es una demostración
  ejecutada una vez durante esta fase, documentada en el reporte del task
  que la ejecute, verificable a mano contra el ledger que produce.

## 7. Criterios de aceptación

Mapeados a los del roadmap:

- toda métrica dispone de fixtures — §3, auditoría confirmada o cerrada;
- todo bug corregido añade un test de regresión — §3, política escrita;
- los falsos positivos se registran — §4.1, `decision.verdict == "rejected"`
  en el ledger;
- una regla experimental se distingue de una estable — §3, tabla estática,
  las 8 arrancan `experimental`;
- el agente no se considera fuente de verdad — §4.1, `proposal` vs.
  `decision` separados, el informe (§4.2) solo cuenta `decision`;
- las decisiones finales de validación son revisables — el ledger es texto
  plano versionable en git, cada entrada lleva `by`/`date`/`notes`.

## 8. Dependencias

Todas las fases anteriores (según el roadmap). En la práctica: Fase 9
(`analyze --format json`) es el único componente de código que este flujo
consume; Fase 10 no es una dependencia funcional, solo presta la noción de
identidad de finding que el ledger reutiliza.
