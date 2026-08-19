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
| 3 | 🟡 Reglas avanzadas de C (parcial) | ✅ API insegura shippeada (`security.unsafe-string-api`: `gets`, `strcpy`, `strcat`, `sprintf`, `vsprintf`) — memoria, punteros, arrays y macros siguen pendientes, ver nota abajo | Mismo motor de reglas (`Rule::evaluate(RuleContext)`); la regla shippeada fue la primera no-umbral del motor | M |
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
- El track 4, tal como se shippeó su primera vuelta, cubre solo ciclos (componentes fuertemente conexas, sin enumeración exhaustiva de ciclos simples) y fan-out con umbral (`fanOutWarning`/`fanOutHigh`) — fan-in se calcula y reporta pero deliberadamente nunca genera finding (un fan-in alto suele ser buen diseño, no un olor). Deliberadamente fuera de esa vuelta: capas y dependencias prohibidas, que necesitarían que el usuario defina un esquema de configuración nuevo (qué es una capa, qué pares están prohibidos) — coste mayor que la pieza ya shippeada. No incluye visualización en la GUI por defecto — eso, si se quiere, es una extensión del track o un track propio posterior. Tampoco participa en el quality gate ni tiene supresión por comentario (un ciclo cruza varios archivos, no tiene una línea natural).
- El track 5 (reglas C++) tiene una dependencia explícita: no se puede escribir la primera regla sin antes decidir qué campos añadir a `CallableModel`. Ese es el primer punto a resolver en su brainstorm.
- El track 6 (histórico avanzado) es deliberadamente más estrecho que en la lista original: la parte de "comparar dos snapshots arbitrarios" se movió al track 2 porque la lógica de backend ya existe; a este track le queda solo la parte que sí es trabajo nuevo (series/tendencias).
- Los tracks 7 y 8 son inversiones grandes, no "una fase más" — cuando les toque, probablemente necesiten su propia descomposición en sub-tracks antes de una spec única.

---

## 5. Siguientes pasos

Tracks #0, #1 y #2 ya están shippeados, y los tracks #3 (API insegura) y #4 (grafo + ciclos + fan-out) tienen su primera vuelta shippeada (ver [[project-fase-sequencing]] en memoria para las fechas/commits exactos). Pendiente decidir: continuar el track #3 (punteros/arrays o macros), continuar el track #4 (capas o dependencias prohibidas), o saltar al **#5 (Reglas avanzadas de C++)**. Cada slice/track sigue su propio ciclo `superpowers:brainstorming` → spec/plan → implementación, igual que duplicación.
