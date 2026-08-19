# Estabilidad de reglas

Ninguna de las 8 reglas fue validada contra proyectos reales antes de la
Fase 11 (ver `docs/superpowers/specs/2026-08-09-fase-7-motor-reglas-design.md`
§6) — todas arrancan `experimental`. La promoción a `stable` es una
decisión humana respaldada por un informe real de
`GenerateValidationReport.cmake` con una muestra representativa (el roadmap
usa el orden de magnitud de 100 ejemplos revisados por regla) — nunca un
umbral numérico automático. Cada fila que cambia de `experimental` a
`stable` agrega la fecha y qué informe la respalda.

Ya existen datos reales, aunque la muestra sea chica e informal: ver
`sqlite.ledger.json` y `fmt.ledger.json` en este mismo directorio, y correr
`GenerateValidationReport.cmake` sobre ambos para los números actuales
(ver `VALIDATION-METHODOLOGY.md` §5). `signature.consecutive-booleans` no
tiene todavía ningún ejemplo real en los ledgers — la regla no disparó en
ninguno de los dos proyectos muestreados hasta ahora.

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
| `security.unsafe-string-api` | `experimental` | — | — |
