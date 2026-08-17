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
| 0 | README al día | Reescribir la apertura y la sección "Estado" para reflejar las Fases 6-12, duplicación y la GUI Qt6/QML, todas ya shippeadas | Todo el contenido real ya está documentado en el README; solo el framing inicial y el cierre siguen diciendo "fase de fundación... todavía no analiza" | XS |
| 1 | GUI: gestión de supresiones | Superficie en la GUI de escritorio para ver/gestionar supresiones de reglas | El mecanismo `// codeqa-disable-next-line <rule-id>` ya existe en `ApplyRules.cpp`; cero superficie hoy en `src/desktop/` | S |
| 2 | GUI: comparación de snapshots | Selección de dos snapshots arbitrarios en `HistoryPage` + vista de diff | `compareSnapshots(current, previous)` ya diffea dos snapshots cualquiera; solo falta UI para elegirlos (hoy solo se usa internamente, current vs. anterior, para el quality gate) | S |
| 3 | Reglas avanzadas de C | Memoria, punteros, arrays, macros, API insegura (`strcpy`, `gets`, etc.) | Mismo motor de reglas (`Rule::evaluate(RuleContext)`, ya soporta patrones no solo umbrales); sin precedente de regla no-umbral todavía | M |
| 4 | Arquitectura (grafo de includes) | Ciclos, dependencias prohibidas, capas, fan-in/fan-out | `DependencyModel` ya fluye a `codeqa::model` y al JSON del reporte (`FileModel::dependencies`), cubierto por tests; falta construir la capa de grafo y las reglas sobre él | M |
| 5 | Reglas avanzadas de C++ | Ownership/RAII, Rule of Five, destructores virtuales, `override`, `std::move`, smart pointers | `CallableModel` no tiene hoy ningún campo relacionado con RAII (destructor, punteros crudos, special members) — requiere extender el modelo antes de poder escribir las reglas | M/L |
| 6 | Histórico avanzado | Tendencias multi-snapshot (series temporales, no solo pares) | Extiende `codeqa::history`; hoy solo hay diff pairwise current-vs-previous y cero agregación de series (la comparación arbitraria par-a-par la cubre el track 2) | L |
| 7 | Frontend Clang | AST real, resolución de tipos, análisis semántico | Habilita ownership real más allá de heurísticas de nombres/patrones; varias fases en sí mismo, no un track único | XL |
| 8 | Soporte Python | Lenguaje nuevo de cero | Totalmente independiente del track C/C++; el mayor esfuerzo del backlog | XL |

---

## 4. Notas de alcance

- Los tracks 1 y 2 (GUI) están separados aunque comparten página/capa porque su coste real es independiente entre sí: ambos dependen de lógica de motor que ya existe, así que cada uno es solo trabajo de UI.
- El track 4 (arquitectura) no incluye visualización en la GUI por defecto — eso, si se quiere, es una extensión del track o un track propio posterior.
- El track 5 (reglas C++) tiene una dependencia explícita: no se puede escribir la primera regla sin antes decidir qué campos añadir a `CallableModel`. Ese es el primer punto a resolver en su brainstorm.
- El track 6 (histórico avanzado) es deliberadamente más estrecho que en la lista original: la parte de "comparar dos snapshots arbitrarios" se movió al track 2 porque la lógica de backend ya existe; a este track le queda solo la parte que sí es trabajo nuevo (series/tendencias).
- Los tracks 7 y 8 son inversiones grandes, no "una fase más" — cuando les toque, probablemente necesiten su propia descomposición en sub-tracks antes de una spec única.

---

## 5. Siguientes pasos

Al retomar este roadmap, el siguiente track a brainstormear es el **#0 (README al día)**, seguido de los tracks **#1/#2 (GUI)**. Cada uno sigue su propio ciclo `superpowers:brainstorming` → spec/plan → implementación, igual que duplicación.
