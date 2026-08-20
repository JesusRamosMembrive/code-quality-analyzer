# Code Quality Analyzer — Roadmap v2 (post-MVP)

## 1. Propósito de este documento

El roadmap del MVP (`docs/01-cpp-mvp-roadmap.md`, Fases 0-12) está completo y shippeado, junto con dos tracks post-MVP fuera de esa numeración: la GUI de escritorio Qt6/QML y la detección de duplicación de código (Type-1/Type-2).

Este documento fija el backlog de tracks candidatos para seguir extendiendo el analizador, y el orden en que se abordarán. No diseña ningún track en detalle — cada uno, cuando le toque, sigue el mismo ciclo `brainstorming` → `writing-plans` → `subagent-driven-development` que ya se usó para duplicación, con su propia spec en `docs/superpowers/specs/` y su propio plan en `docs/superpowers/plans/`.

---

## 2. Criterio de orden

Los tracks están ordenados por **coste ascendente**: se empieza por lo más barato/rápido y se deja lo más grande (soporte de un lenguaje nuevo, un frontend semántico real) para el final. El orden no es una promesa de fechas — es la cola de trabajo por defecto; nada impide reordenar si cambian las prioridades.

---

## 3. Backlog

| # | Track | Qué añade | Punto de partida | Coste |
|---|-------|-----------|-------------------|-------|
| 0 | ✅ README al día | Reescribir la apertura y la sección "Estado" para reflejar las Fases 6-12, duplicación y la GUI Qt6/QML, todas ya shippeadas | Todo el contenido real ya está documentado en el README; solo el framing inicial y el cierre siguen diciendo "fase de fundación... todavía no analiza" | XS |
| 1 | ✅ GUI: gestión de supresiones | Superficie de solo lectura en la GUI de escritorio para ver supresiones de reglas (crear/borrar sigue siendo manual, en el código fuente) | El mecanismo `// codeqa-disable-next-line <rule-id>` ya existe en `ApplyRules.cpp`; cero superficie hoy en `src/desktop/` | S |
| 2 | ✅ GUI: comparación de snapshots | Selección de dos snapshots arbitrarios en `HistoryPage` + contadores agregados (nuevos/existentes/resueltos) — deliberadamente sin vista de diff por finding, ver nota abajo | `compareSnapshots(current, previous)` ya diffea dos snapshots cualquiera; solo falta UI para elegirlos (hoy solo se usa internamente, current vs. anterior, para el quality gate) | S |
| 3 | 🟡 Reglas avanzadas de C (parcial) | ✅ API insegura + ✅ punteros locales sin inicializar + ✅ arrays en parámetros que decaen (`memory.array-parameter-decay`) — solo macros sigue pendiente, ver nota abajo | Mismo motor de reglas (`Rule::evaluate(RuleContext)`); la tercera vuelta requirió extender `ParameterPassing` (definido dos veces: `codeqa::frontend` y `codeqa::model`) con un valor `Array` nuevo | M |
| 4 | 🟡 Arquitectura (grafo de includes, parcial) | ✅ Grafo + ciclos + fan-out shippeados (`codeqa::architecture`, nueva capa hermana de `codeqa::duplication`) — capas y dependencias prohibidas siguen pendientes, ver nota abajo | `DependencyModel::target` era solo texto crudo de `#include`, sin resolver contra archivos reales — la resolución del grafo fue trabajo nuevo, no solo "conectar un dato que ya existía" | M |
| 5 | Reglas avanzadas de C++ | Ownership/RAII, Rule of Five, destructores virtuales, `override`, `std::move`, smart pointers | `CallableModel` no tiene hoy ningún campo relacionado con RAII (destructor, punteros crudos, special members) — requiere extender el modelo antes de poder escribir las reglas | M/L |
| 6 | Histórico avanzado | Tendencias multi-snapshot (series temporales, no solo pares) | Extiende `codeqa::history`; hoy solo hay diff pairwise current-vs-previous y cero agregación de series (la comparación arbitraria par-a-par la cubre el track 2) | L |
| 7 | Frontend Clang | AST real, resolución de tipos, análisis semántico | Habilita ownership real más allá de heurísticas de nombres/patrones; varias fases en sí mismo, no un track único | XL |
| 8 | Soporte Python | Lenguaje nuevo de cero | Totalmente independiente del track C/C++; el mayor esfuerzo del backlog | XL |

---

## 4. Notas de alcance

- Los tracks 1 y 2 (GUI) están separados aunque comparten página/capa porque su coste real es independiente entre sí: ambos dependen de lógica de motor que ya existe, así que cada uno es solo trabajo de UI.
- El track 2, tal como se shippeó, muestra solo contadores agregados (`newCount`/`existingCount`/`resolvedCount`), no qué findings concretos cambiaron — decisión explícita durante su brainstorm para mantenerlo GUI-only sin tocar `codeqa::history`. Ver qué findings exactos son nuevos/resueltos requeriría extender `SnapshotComparison` con identidad por finding; queda como posible iteración futura, no prometida aquí.
- El track 3, tal como se shippeó su primera vuelta, cubre solo "API insegura" (5 funciones de string/IO de C sin uso seguro posible: `gets`, `strcpy`, `strcat`, `sprintf`, `vsprintf`) — decisión explícita durante su brainstorm para acotar el track heterogéneo original a la pieza más barata primero. Deliberadamente fuera de esa vuelta, no olvidado: `scanf`/`fscanf`/`sscanf`/`strtok` (necesitarían leer el contenido del format string, que `CallModel` no guarda hoy — solo `argumentCount`); punteros/arrays (solo alcanzables hoy vía heurística de texto sobre `declaredType`, sin AST real); macros (`FileModel` no modela `#define` en absoluto, requeriría extender el frontend). Cada uno queda como posible sub-slice futuro del mismo track #3.
- La segunda vuelta del track 3 shippeó `memory.uninitialized-pointer` (variable local puntero cruda declarada sin inicializador). Solo variables locales: `LocalVariableModel::declaredType` ya incluye el sufijo del declarador (`"int*"`, `"int[10]"`), pero `ParameterModel::declaredType` no (`parameterDeclaredType()` corta el texto antes del declarador para no tragarse un valor por defecto) — así que arrays en parámetros siguen sin ser detectables sin extender el frontend, deliberadamente fuera de esta vuelta también. Sin análisis de flujo de datos: una variable asignada en una sentencia posterior a su declaración se sigue marcando como sin inicializar (limitación conocida y documentada, no un bug).
- La tercera vuelta del track 3 shippeó `memory.array-parameter-decay` (parámetro declarado con sintaxis de array, que decae a puntero perdiendo el tamaño). Requirió una extensión real de frontend: `ParameterPassing` está definido dos veces (`codeqa::frontend` y `codeqa::model`, unidos por dos switches exhaustivos sin `default:`) — se añadió un valor `Array` a ambas definiciones, detectado vía `array_declarator`/`abstract_array_declarator` en el AST, no heurística de texto. La revisión final encontró y corrigió un falso positivo real antes de mergear: `int (&arr)[N]` (referencia a array) e `int (*p)[N]` (puntero a array) no decaen, pero la detección inicial los clasificaba como `Array` igualmente — corregido comprobando que el `array_declarator` encontrado no esté envuelto en un `parenthesized_declarator`. Limitación residual conocida, no bloqueante: un puntero a array multidimensional (`int (*m)[3][4]`) todavía se clasifica mal (pre-existente, no introducida por esta corrección). Macros sigue siendo el único sub-slice pendiente del track 3: `FileModel` no modela `#define` en absoluto.
- El track 4, tal como se shippeó su primera vuelta, cubre solo ciclos (componentes fuertemente conexas, sin enumeración exhaustiva de ciclos simples) y fan-out con umbral (`fanOutWarning`/`fanOutHigh`) — fan-in se calcula y reporta pero deliberadamente nunca genera finding (un fan-in alto suele ser buen diseño, no un olor). Deliberadamente fuera de esa vuelta: capas y dependencias prohibidas, que necesitarían que el usuario defina un esquema de configuración nuevo (qué es una capa, qué pares están prohibidos) — coste mayor que la pieza ya shippeada. No incluye visualización en la GUI por defecto — eso, si se quiere, es una extensión del track o un track propio posterior. Tampoco participa en el quality gate ni tiene supresión por comentario (un ciclo cruza varios archivos, no tiene una línea natural).
- El track 5 (reglas C++) tiene una dependencia explícita: no se puede escribir la primera regla sin antes decidir qué campos añadir a `CallableModel`. Ese es el primer punto a resolver en su brainstorm.
- El track 6 (histórico avanzado) es deliberadamente más estrecho que en la lista original: la parte de "comparar dos snapshots arbitrarios" se movió al track 2 porque la lógica de backend ya existe; a este track le queda solo la parte que sí es trabajo nuevo (series/tendencias).
- Los tracks 7 y 8 son inversiones grandes, no "una fase más" — cuando les toque, probablemente necesiten su propia descomposición en sub-tracks antes de una spec única.

---

## 5. Siguientes pasos

Tracks #0, #1 y #2 ya están shippeados. El track #3 tiene tres vueltas shippeadas (API insegura, punteros locales sin inicializar, arrays en parámetros) y el track #4 tiene su primera vuelta shippeada (grafo + ciclos + fan-out) (ver [[project-fase-sequencing]] en memoria para las fechas/commits exactos). Pendiente decidir: cerrar el track #3 con macros (el único sub-slice que queda), continuar el track #4 (capas o dependencias prohibidas), o saltar al **#5 (Reglas avanzadas de C++)**. Cada slice/track sigue su propio ciclo `superpowers:brainstorming` → spec/plan → implementación, igual que duplicación.

Nota recurrente, ahora confirmada tres veces: cada vuelta de reglas ha tocado 4-6 listas hardcodeadas en paralelo (id de regla en `Rules.cpp`, `DefaultConfig.cpp`, `ConfigJson.cpp::knownRuleIds()`, `ReportModels.cpp::orderedRuleIds()`, doc comment de `Rules.h`, más un recuento en `ReportJsonTest.cpp`) sin registro central. La tercera vuelta fue la primera en cubrir las 5 conocidas sin que el implementador encontrara una sexta — pero la revisión final del branch volvió a señalar el coste acumulado. Un registro central de reglas sigue siendo candidato para cuando se retome este track o se haga un refactor dedicado.

Nota recurrente a resolver en algún momento: esta es la segunda vuelta consecutiva que edita 4-6 listas hardcodeadas en paralelo (id de regla en `Rules.cpp`, `DefaultConfig.cpp`, `ConfigJson.cpp::knownRuleIds()`, `ReportModels.cpp::orderedRuleIds()`, doc comment de `Rules.h`, más un recuento en `ReportJsonTest.cpp` que ninguna de las dos vueltas anteveía) — sin registro central de reglas. La revisión final de esta vuelta lo señaló explícitamente como coste demostrado dos veces; no es trabajo de esta rama, pero cualquier slice futuro de reglas debería considerar un registro central antes de seguir apilando vueltas manuales.
