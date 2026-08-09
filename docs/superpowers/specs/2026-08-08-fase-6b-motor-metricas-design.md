# Fase 6b — Motor de métricas: diseño

Fecha: 2026-08-08
Estado: revisado (segunda pasada 2026-08-08); pendiente de aprobación final

## 1. Objetivo

Calcular las métricas de tamaño, firma y complejidad definidas en
`01-cpp-mvp-roadmap.md` (Fase 6) para cada función del modelo intermedio
común, extendido en la Fase 6a. Las métricas son valores objetivos y
verificables; esta fase no genera hallazgos ni aplica umbrales — eso
pertenece a la Fase 7.

## 2. Alcance y límites

Esta fase incluye:

- una capa nueva `codeqa::metrics`, separada del modelo intermedio, con los
  tipos de métricas y el cálculo puro por función;
- un orquestador `codeqa::application::MeasureProject` que relee el texto
  fuente y produce las métricas de un `ProjectModel` completo;
- las fórmulas exactas de tamaño, firma y complejidad descritas en la
  sección 5;
- expectativas de métricas nuevas en el corpus existente
  (`tests/corpus/complexity/`, `tests/corpus/nesting/` y
  `tests/corpus/parameters/`), como archivos `*.metrics.json` hermanos de
  las fuentes ya presentes. El roadmap de Fase 0 esbozaba `tests/fixtures/`,
  pero el corpus real del proyecto vive en `tests/corpus/` y esos tres
  directorios ya existen con fuentes y expectativas `*.parsing.json`.

No incluye: ningún comando CLI nuevo (las métricas quedan como capa de
librería verificada por tests, igual que la Fase 5; la exposición al usuario
final es responsabilidad de la Fase 9); reglas, umbrales ni hallazgos (Fase
7); supresión de métricas en regiones `ERROR`/`MISSING` — se calculan
igual, para diagnóstico, según el criterio de aceptación explícito del
roadmap.

## 3. Arquitectura

`codeqa::metrics` no depende de E/S ni de `codeqa::frontend`. Su punto de
entrada es una función pura:

    FunctionMetricsModel calculateFunctionMetrics(
        const model::CallableModel& callable,
        const std::vector<core::SourceRange>& fileComments,
        const std::vector<std::string>& sourceLines);

`sourceLines` son las líneas del archivo que contiene `callable`, ya leídas
por el llamador. `fileComments` es `FileModel.comments` del archivo (Fase
6a): es imprescindible como parámetro porque tanto `commentLines` como la
exclusión de operadores booleanos dentro de comentarios dependen de esos
rangos, y `CallableModel` no los contiene. Esta separación hace que todos
los casos límite de complejidad (anidamiento, operadores booleanos, mezcla
de comentario y código en una misma línea) sean verificables con arrays de
líneas y rangos construidos a mano, sin fixtures de archivo ni E/S en los
tests unitarios.

`codeqa::application::MeasureProject` es el único punto que toca disco:
recorre `model::ProjectModel`, relee cada archivo una vez completo (nunca
por función), divide el contenido en líneas, y llama a
`calculateFunctionMetrics` por cada `CallableModel` de cada `FileModel` —
**excepto los de `CallableKind::Prototype`**. Un prototipo no tiene cuerpo:
sus métricas serían ruido (complejidad 1, cero sentencias) y duplicarían la
entrada de la definición real cuando esta existe en otro archivo. Reutiliza
la misma ruta ya validada por el descubrimiento (Fase 2); no vuelve a
comprobar accesibilidad ni tamaño del archivo, porque ya se comprobó al
construir el `ProjectModel`.

Se descartan dos alternativas: embeber las métricas directamente en
`CallableModel` (mezclaría la capa de modelo, pensada para ser reutilizable
entre frontends, con cálculos específicos del motor de análisis, y
contradice el principio de "métricas separadas" del roadmap); y calcular
sobre `frontend::FunctionModel` en vez de `model::CallableModel` (rompería
el propósito explícito de la Fase 5 de que el motor de métricas no consulte
tokens ni tipos de tree-sitter).

## 4. Modelo de métricas

    FunctionSizeMetrics
    ├── physicalLines
    ├── codeLines
    ├── blankLines
    ├── commentLines
    └── statementCount

    FunctionSignatureMetrics
    ├── parameterCount
    ├── signatureLength
    └── booleanParameterCount

    FunctionComplexityMetrics
    ├── cyclomaticComplexity
    ├── maxConditionComplexity
    ├── maxNestingDepth
    ├── branchCount
    └── returnCount

    FunctionMetricsModel
    ├── symbol (SymbolIdentity de la función medida)
    ├── size: FunctionSizeMetrics
    ├── signature: FunctionSignatureMetrics
    └── complexity: FunctionComplexityMetrics

    FileMetricsModel
    ├── path
    ├── functions: vector<FunctionMetricsModel>
    └── diagnostics: vector<core::Diagnostic>

    ProjectMetricsModel
    ├── rootPath
    └── files: vector<FileMetricsModel>

`FileMetricsModel`/`ProjectMetricsModel` reflejan la forma de
`FileModel`/`ProjectModel` un nivel más arriba, para que consumirlos desde
Fase 7/9 sea familiar. No se mezclan con `model::ProjectModel`: son un árbol
paralelo, producido por `MeasureProject`, nunca escrito de vuelta en el
modelo intermedio.

## 5. Definiciones de las métricas

### Tamaño

Todas se calculan sobre las líneas cubiertas por `callable.symbol.range`.
Cada línea del rango se clasifica en exactamente una categoría, evaluando
las reglas en este orden:

1. **Vacía**: la línea no contiene ningún carácter distinto de espacio en
   blanco. Esto incluye una línea vacía en medio de un comentario de bloque:
   la precedencia resuelve el empate a favor de vacía.
2. **Comentario**: todos los caracteres no blancos de la línea caen dentro
   de algún rango de `fileComments`. Nótese que la regla no es "la línea
   intersecta un comentario": una línea con código seguido de comentario
   (`x = 1; // nota`) tiene caracteres no blancos fuera del comentario y
   por tanto no es línea de comentario.
3. **Código**: cualquier otra línea.

- **physicalLines**: `range.end.line - range.begin.line + 1`.
- **blankLines**: líneas clasificadas como vacías.
- **commentLines**: líneas clasificadas como comentario.
- **codeLines**: líneas clasificadas como código. Por construcción,
  `physicalLines == codeLines + blankLines + commentLines`.
- **statementCount**: `callable.statements.size()`.

### Firma

- **parameterCount**: `callable.parameters.size()`.
- **signatureLength**: número de caracteres del texto fuente dentro de
  `callable.parameterListRange` (Fase 6a), excluyendo los propios
  paréntesis — no incluye tipo de retorno ni nombre de la función. Toda
  secuencia de espacio en blanco (espacios, tabuladores, saltos de línea) se
  cuenta como un único espacio, para que una firma partida en varias líneas
  no mida distinto que la misma firma en una línea. Si `parameterListRange`
  está vacío (lambda sin lista de parámetros), vale 0.
- **booleanParameterCount**: parámetros que cumplen **ambas** condiciones:
  su `declaredType`, tras quitar `const`/`volatile` y espacios sobrantes, es
  exactamente `bool` (C++) o `bool`/`_Bool` (C, vía `stdbool.h`), **y** su
  `passing` es `Value`. La segunda condición es necesaria porque
  `declaredType` no incluye los marcadores del declarador (Fase 6a): sin
  ella, `bool* flags` contaría como parámetro booleano. Un `bool&`/
  `const bool&` tampoco cuenta — la métrica apunta al "boolean trap" de
  flags por valor; la exclusión es deliberada y queda documentada aquí. No
  se resuelven typedefs.

### Complejidad

- **branchCount**: número de entradas en `callable.controlFlow` cuyo `kind`
  es `If`, `For`, `While`, `DoWhile`, `Case` o `Conditional`. `Switch`,
  `Default`, `Return`, `Break` y `Continue` no cuentan como rama. `Switch` en
  particular queda excluido a propósito (no solo por omisión): cada `case`
  ya aporta su propia entrada `Case`, así que contar también la entrada
  `Switch` duplicaría la misma decisión — la convención McCabe estándar
  cuenta las etiquetas `case`, no la cabecera `switch`.
- **cyclomaticComplexity**: `1 + branchCount + booleanOperatorCount`, donde
  `booleanOperatorCount` es el número total de operadores `&&`/`||`
  encontrados en el texto de todos los `conditionRange` de la función.
- **maxConditionComplexity**: el máximo, no la suma, de operadores `&&`/`||`
  encontrados en un único `conditionRange` de la función. Identifica la
  condición individual más difícil de leer.
- **maxNestingDepth**: el máximo de `nestingDepth` entre las entradas de
  `callable.controlFlow`. Si la función no tiene entradas, es `0`.
- **returnCount**: número de entradas en `callable.controlFlow` con
  `kind == Return`.

Contar `&&`/`||` dentro de un `conditionRange` escanea el texto de ese rango
(ya disponible vía `sourceLines`; una condición puede abarcar varias líneas)
excluyendo los operadores que aparezcan dentro de un literal de cadena o
carácter contenido en el propio rango, **y también los que caigan dentro de
un rango de `fileComments`** — `if (a /* && b */ && c)` tiene un solo
operador, no dos. No hace falta volver a consultar tree-sitter:
`conditionRange` delimita la condición y `fileComments` delimita los
comentarios.

## 6. Flujo de datos

    ProjectModel (Fase 5, ya construido)
        │
        ▼
    MeasureProject.execute()
        │  relee cada archivo una vez, por FileModel.path
        ▼
    por cada CallableModel del archivo (excepto prototipos):
        calculateFunctionMetrics(callable, fileComments, sourceLines)
        │
        ▼
    ProjectMetricsModel

`MeasureProject` no vuelve a invocar el frontend ni tree-sitter: parte de un
`ProjectModel` ya construido por `AnalyzeProject`/`ProjectModelBuilder`.

## 7. Errores y determinismo

Un archivo que ya forma parte de `ProjectModel` se asume legible (ya se leyó
con éxito en Fase 2/4 para llegar hasta ahí). Si la relectura fallara de
todos modos (archivo borrado entre el análisis y la medición), `Measure
Project` no aborta el proyecto completo: emite el `FileMetricsModel` de ese
archivo con `functions` vacío y un `Diagnostic` en su campo `diagnostics`, y
continúa con los demás, igual que el resto del pipeline nunca deja que un
archivo problemático bloquee a los demás.

La relectura se hace en modo binario y la división en líneas tolera `\r\n`,
de modo que la numeración 1-based de líneas y columnas coincide con las
posiciones que tree-sitter registró en los rangos del modelo durante la
Fase 4; sin esa correspondencia, la extracción de texto de `conditionRange`
y `parameterListRange` se desplazaría.

Las métricas se calculan igual en funciones cuyo rango se solape con un nodo
`ERROR`/`MISSING`: no se excluyen aquí. La decisión de si esas funciones
generan hallazgos pertenece a la Fase 7, tal como fija el criterio de
aceptación del roadmap.

Repetir el cálculo sobre el mismo `ProjectModel` y el mismo contenido de
archivo produce siempre los mismos valores.

## 8. Verificación

Pruebas unitarias de `codeqa::metrics` (sin E/S): `CallableModel` y arrays de
líneas construidos a mano para cada fórmula — anidamiento variable, mezcla
de `&&`/`||`, `switch` con varios `case` y un `default`, líneas con
código+comentario en la misma línea, comentario ocupando una línea completa,
firma vacía, parámetros `bool`/`const bool`/tipos no booleanos.

Pruebas unitarias de `MeasureProject`: relectura de un archivo temporal
real, verificando que se lee una sola vez por archivo y que un archivo
borrado tras el análisis produce un diagnóstico sin abortar el resto.

Pruebas de integración/corpus: expectativas de métricas como archivos
`<nombre>.metrics.json` hermanos de las fuentes existentes en
`tests/corpus/complexity/`, `tests/corpus/nesting/` y
`tests/corpus/parameters/` (p. ej. `branching.metrics.json` junto a
`branching.cpp`), describiendo las métricas esperadas por función. Un runner
de corpus de métricas construye el `ProjectModel` del fixture, ejecuta
`MeasureProject` y compara con igualdad exacta. `CheckCorpusIntegrity.cmake`
se amplía para verificar que cada `*.metrics.json` tenga fuente hermana; la
expectativa de métricas es opcional por fixture (solo los tres directorios
citados la incorporan en esta fase), pero donde exista se valida completa.

La validación final incluye compilación Debug, pruebas focalizadas, CTest y
`git diff --check`.

## 9. Criterios de aceptación

- Las métricas de tamaño, firma y complejidad coinciden con los fixtures
  para C y C++.
- Cada métrica tiene su definición documentada en este archivo (sección 5).
- Las métricas no generan hallazgos por sí mismas.
- Los cálculos son reproducibles: misma entrada, mismo resultado.
- Las métricas se calculan igualmente en funciones que se solapan con
  regiones `ERROR`/`MISSING`.
- Los prototipos no aparecen en `ProjectMetricsModel`.
- Un archivo cuya relectura falla produce un `FileMetricsModel` con
  diagnóstico y sin funciones, sin abortar el resto del proyecto.
- No existe ningún comando CLI nuevo asociado a esta fase.

## 10. Dependencias

Fase 6a.
