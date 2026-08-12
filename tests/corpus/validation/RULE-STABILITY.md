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
