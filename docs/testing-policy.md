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
