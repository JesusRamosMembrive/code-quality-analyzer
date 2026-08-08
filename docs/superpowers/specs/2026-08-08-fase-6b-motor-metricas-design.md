# Fase 6b — Motor de métricas: diseño

Fecha: 2026-08-08
Estado: pendiente de revisión

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
- fixtures nuevos en `tests/fixtures/complexity/`, `tests/fixtures/nesting/`
  y `tests/fixtures/parameters/` con resultados esperados.

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
        const std::vector<std::string>& sourceLines);

`sourceLines` son las líneas del archivo que contiene `callable`, ya leídas
por el llamador. Esta separación hace que todos los casos límite de
complejidad (anidamiento, operadores booleanos, mezcla de comentario y
código en una misma línea) sean verificables con arrays de líneas
construidos a mano, sin fixtures de archivo ni E/S en los tests unitarios.

`codeqa::application::MeasureProject` es el único punto que toca disco:
recorre `model::ProjectModel`, relee cada archivo una vez completo (nunca
por función), divide el contenido en líneas, y llama a
`calculateFunctionMetrics` por cada `CallableModel` de cada `FileModel`.
Reutiliza la misma ruta ya validada por el descubrimiento (Fase 2); no
vuelve a comprobar accesibilidad ni tamaño del archivo, porque ya se
comprobó al construir el `ProjectModel`.

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
    └── functions: vector<FunctionMetricsModel>

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

- **physicalLines**: `range.end.line - range.begin.line + 1`.
- **commentLines**: líneas del rango que intersectan con alguna entrada de
  `FileModel.comments`.
- **blankLines**: líneas del rango cuyo contenido, tras recortar espacios en
  ambos extremos, es una cadena vacía.
- **codeLines**: `physicalLines - blankLines - commentLines`. Una línea con
  código seguido de un comentario (`x = 1; // nota`) cuenta como línea con
  código, no como línea de comentario: el comentario no ocupa la línea
  completa.
- **statementCount**: `callable.statements.size()`.

### Firma

- **parameterCount**: `callable.parameters.size()`.
- **signatureLength**: número de caracteres del texto fuente entre `(` y `)`
  de la lista de parámetros — no incluye tipo de retorno ni nombre de la
  función.
- **booleanParameterCount**: parámetros cuyo `declaredType`, tras quitar
  `const`/`volatile` y espacios sobrantes, es exactamente `bool` (C++) o
  `bool`/`_Bool` (C, vía `stdbool.h`). No se resuelven typedefs.

### Complejidad

- **branchCount**: número de entradas en `callable.controlFlow` cuyo `kind`
  es `If`, `For`, `While`, `DoWhile`, `Case` o `Conditional`. `Default`,
  `Return`, `Break` y `Continue` no cuentan como rama.
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
(ya disponible vía `sourceLines`) evitando los que aparezcan dentro de un
literal de cadena o carácter contenido en el propio rango. No hace falta
volver a consultar tree-sitter: `conditionRange` ya delimita con precisión
dónde empieza y termina la condición.

## 6. Flujo de datos

    ProjectModel (Fase 5, ya construido)
        │
        ▼
    MeasureProject.execute()
        │  relee cada archivo una vez, por FileModel.path
        ▼
    por cada CallableModel del archivo:
        calculateFunctionMetrics(callable, sourceLines)
        │
        ▼
    ProjectMetricsModel

`MeasureProject` no vuelve a invocar el frontend ni tree-sitter: parte de un
`ProjectModel` ya construido por `AnalyzeProject`/`ProjectModelBuilder`.

## 7. Errores y determinismo

Un archivo que ya forma parte de `ProjectModel` se asume legible (ya se leyó
con éxito en Fase 2/4 para llegar hasta ahí). Si la relectura fallara de
todos modos (archivo borrado entre el análisis y la medición), `Measure
Project` no aborta el proyecto completo: registra un `Diagnostic` para ese
archivo y continúa con los demás, igual que el resto del pipeline nunca deja
que un archivo problemático bloquee a los demás.

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

Pruebas de integración/corpus: fixtures nuevos en
`tests/fixtures/complexity/`, `tests/fixtures/nesting/` y
`tests/fixtures/parameters/`, cada uno con su `sample.expected.json`
describiendo las métricas esperadas por función, verificados ejecutando
`MeasureProject` sobre el `ProjectModel` del fixture.

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
- No existe ningún comando CLI nuevo asociado a esta fase.

## 10. Dependencias

Fase 6a.
