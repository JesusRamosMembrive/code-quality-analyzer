# Fase 6a — Extensiones del modelo para métricas: diseño

Fecha: 2026-08-08
Estado: pendiente de revisión

## 1. Objetivo

Cerrar los huecos del modelo intermedio común (Fase 5) que la Fase 6b necesita
para calcular métricas, sin calcular ninguna métrica todavía. El resultado
sigue siendo puramente estructural: tipado, determinista e independiente de
tree-sitter fuera de `codeqa::frontend`.

## 2. Alcance y límites

Esta fase incluye:

- tipo declarado por parámetro (`ParameterModel.declaredType`), en frontend y
  modelo común;
- rangos de comentario a nivel de archivo (`FileModel.comments`);
- rangos de sentencia por función (`CallableModel.statements`);
- cierre de `CPP-PARAM-01`, `CPP-PARAM-02` y `CPP-TYPE-03` en el corpus de
  validación, con aserciones exactas en vez de solo presencia;
- documentación explícita en `tests/corpus/README.md` de los IDs que quedan
  sin aserción exacta y por qué.

No incluye: ningún cálculo de métrica (tamaño, firma o complejidad); la
clasificación de tipos de sentencia (solo se captura su rango, no su
naturaleza); resolución de tipos más allá de comparación textual; `goto`,
`throw`, `try`/`catch`, macros, evaluación del preprocesador — igual que en
Fase 5 capa 2.

## 3. Modelo público

`ParameterModel` (frontend y modelo común) gana `declaredType: std::string`,
el texto del tipo tal como aparece en la firma, sin normalizar más allá de lo
que ya hace la extracción existente para determinar `ParameterPassing`.

`FileModel` gana `comments: std::vector<core::SourceRange>`. Vive a nivel de
archivo, no de función: un comentario no pertenece sintácticamente a ninguna
función en el CST, y una función puede contener comentarios en su rango sin
que estos sean hijos de su subárbol de cuerpo en todas las gramáticas. La
Fase 6b intersecta estos rangos con el rango de cada función al calcular
líneas de comentario.

`CallableModel` gana `statements: std::vector<core::SourceRange>`. Cada
entrada es el rango de una sentencia de nivel de cuerpo de función
(`expression_statement`, `declaration`, `return_statement`, y equivalentes ya
reconocidos por las queries de control de flujo y llamadas). No se clasifica
el tipo de sentencia ni se distingue de las colecciones ya existentes
(`localVariables`, `controlFlow`, `calls`); esas construcciones seguirán
siendo accesibles también como entradas de `statements`, porque el objetivo
de esta colección es solo contarlas y ubicarlas, no evitar solapamiento
conceptual con las demás.

Los prototipos siguen sin tener estas colecciones pobladas, igual que en
Fase 5 capa 2. Un cuerpo de lambda atribuye sus propias sentencias solo a su
callable, nunca a la función envolvente.

## 4. Queries y extracción

`src/frontend/queries/{c,cpp}/functions.scm` amplían la captura de parámetros
para incluir el nodo de tipo ya visible en el CST (el mismo que hoy se usa
para derivar `ParameterPassing`), sin añadir una query nueva.

Se añaden `src/frontend/queries/{c,cpp}/comments.scm`, que capturan nodos
`(comment)` en cualquier posición del archivo, y
`src/frontend/queries/{c,cpp}/statements.scm`, que capturan nodos de
sentencia dentro del cuerpo de cada función/método/constructor/
destructor/operador/lambda con cuerpo.

`StructuralExtractor.cpp` (1440 líneas antes de esta fase) recibe dos
funciones libres nuevas y autocontenidas para comentarios y sentencias, cada
una en su propia sección con su propio bloque de comentario de sección, en
vez de mezclarse con la extracción de funciones existente. Si durante la
implementación el archivo resulta difícil de navegar, se divide en un
`StructuralExtractor` (funciones, tipos, includes — ya existente) y un
archivo nuevo para las extracciones a nivel de archivo (comentarios); esa
decisión se toma en el plan de implementación, no aquí.

`ProjectModelBuilder.cpp` convierte las tres colecciones nuevas de frontend a
modelo común mediante conversiones explícitas, igual que ya hace con
`localVariables`, `controlFlow` y `calls`.

## 5. Corpus de validación

`CPP-PARAM-01` (valores por defecto) y `CPP-PARAM-02` (parámetros por
referencia y por puntero) pasan de cobertura por presencia a aserción exacta
de `declaredType` en `tests/corpus/functions/cpp-functions.parsing.json`,
`tests/corpus/parameters/cpp-parameters.parsing.json` y
`tests/corpus/mixed/mixed-constructs.parsing.json`. `CPP-TYPE-03`
(`enum`/`enum class`) se cierra en el mismo trabajo porque su verificación
exacta depende de la misma ampliación de esquema JSON de parámetros.

`C-PP-01`, `C-DECL-01`, `C-TYPE-04`, `C-FUNC-03`, `C-FUNC-04` y `C-FUNC-05`
quedan sin aserción exacta: no tienen relación con parámetros, comentarios ni
sentencias, y su cierre (typedefs, variables globales, storage class,
elipsis variádica) no aporta nada a las métricas de la Fase 6b. Se documentan
como abiertos en `tests/corpus/README.md`, junto con la razón, para que no se
asuman resueltos por error en una fase futura.

## 6. Errores y determinismo

Un comentario o sentencia dentro de un subárbol `ERROR`/`MISSING` se descarta
igual que ya se descartan los candidatos ambiguos de Fase 5 capa 2: no se
inventa estructura, y el diagnóstico de parsing correspondiente se conserva.
Las tres colecciones nuevas se ordenan por inicio y fin de rango, con empates
estables, siguiendo la convención ya establecida por `localVariables`,
`controlFlow` y `calls`.

## 7. Verificación

`StructuralModelsTest.cpp` y `StructuralExtractorTest.cpp` cubren C y C++ con
tipos de parámetro variados (`bool`, `const bool`, `bool*`, `bool&`, tipos no
booleanos), comentarios de línea y de bloque en distintas posiciones
(dentro/fuera de funciones, junto a código), y sentencias simples anidadas.
`ProjectModelBuilderTest.cpp` comprueba el mapeo tipado de las tres
colecciones nuevas. `CorpusModelTest.cpp`/`CorpusStructureTest.cpp` verifican
las nuevas aserciones exactas de parámetros. `EmbeddedQueriesTest.cpp` se
amplía para compilar `comments.scm` y `statements.scm`.

La validación final incluye compilación Debug, pruebas focalizadas, CTest,
`codeqa parse tests/corpus` con el mismo resumen `13/4/9/2` (esta fase no
cambia comportamiento visible del comando `parse`) y `git diff --check`.

## 8. Criterios de aceptación

- `ParameterModel.declaredType` se puebla en C y C++ y coincide con el texto
  fuente del tipo.
- `FileModel.comments` cubre comentarios de línea y de bloque en C y C++, sin
  falsos positivos por `//` o `/*` dentro de literales de cadena o carácter.
- `CallableModel.statements` cubre sentencias de nivel de cuerpo en funciones,
  métodos, constructores, destructores, operadores y lambdas con cuerpo; los
  prototipos no tienen entradas.
- `CPP-PARAM-01`, `CPP-PARAM-02` y `CPP-TYPE-03` tienen aserción exacta en el
  corpus.
- `tests/corpus/README.md` documenta explícitamente los IDs que siguen sin
  aserción exacta y la razón.
- `codeqa parse tests/corpus` no cambia su salida visible.

## 9. Dependencias

Fase 5 (capa 1 y capa 2).
