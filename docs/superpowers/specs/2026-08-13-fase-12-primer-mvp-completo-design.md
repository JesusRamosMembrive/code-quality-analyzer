# Fase 12 — Primer MVP completo: diseño

Fecha: 2026-08-13
Estado: revisado (brainstorming 2026-08-13); pendiente de aprobación final

## 1. Objetivo

Consolidar Fases 0-11 en una versión utilizable (roadmap, Fase 12): comando
único `codeqa analyze <path> --config codeqa.json --format json --output
report.json`, con quality gate y código de salida coherente. La mayoría de
lo que el roadmap pide como "el MVP deberá producir" ya existe desde Fase
9/10; lo que falta es exactamente dos cosas nuevas — el quality gate
(código de salida 1) y la taxonomía completa de códigos de salida (0-4) —
más dos bugs reales de `codeqa_core` que Fase 11 encontró y dejó sin
corregir, cuya corrección se vuelve necesaria ahora porque uno de ellos
puede hacer fallar el gate sobre datos corruptos.

## 2. Alcance y límites

Incluye:

- Campo de configuración `qualityGate` en `codeqa.json`.
- Evaluación del gate (diferencial con snapshot anterior, absoluto sin él).
- Taxonomía completa de códigos de salida (0/1/2/3/4).
- Corrección de los dos bugs de Fase 11 (rango corrompido bajo
  preprocesador; fuga de scope en `symbolName`).
- Fixtures y tests nuevos para ambos bugs y para el gate.
- Verificación end-to-end contra un proyecto real (SQLite, corpus ya usado
  en Fase 11).

No incluye, y por qué:

- **Ninguna regla nueva.** El roadmap no lo pide para Fase 12 — "reglas
  posteriores" (duplicación, arquitectura, etc.) quedan en fases futuras
  explícitamente marcadas como posteriores al MVP.
- **Corrección del número de métrica crudo mostrado para funciones con
  rango corrompido.** `physicalLines` seguiría mostrando un valor grande en
  el reporte aunque el finding esté suprimido — esto ya es el
  comportamiento establecido para `PARSE001`/`PARSE002` desde Fase 6b/7
  (`MeasureProject` nunca filtra por diagnóstico, solo `ApplyRules` lo
  hace). Extender ese filtro también a `MeasureProject`, para los tres
  códigos simétricamente, es un cambio más grande y no lo exige ningún
  criterio de aceptación del roadmap (que exige que el *gate* no falle
  sobre basura, no que cada métrica cruda esté blindada individualmente) —
  ver §7.2.
- **Modelar semántica de preprocesador** (evaluar `#if`/`#ifdef` real).
  Declarado fuera de alcance del MVP desde Fase 0
  (`tests/corpus/SUPPORTED-CONSTRUCTS.md`, `OUT-03`). La corrección de
  ambos bugs de esta fase evita este problema por diseño: no interpreta
  ninguna rama de preprocesador, solo detecta cuándo un rango es
  geométricamente imposible.
- **Promoción de reglas a `stable`.** El roadmap no lo condiciona para Fase
  12 (ver spec de Fase 11 §2 / `RULE-STABILITY.md`).
- **Un flag de CLI para el gate** (p.ej. `--fail-on`). Fase 8 ya estableció
  el patrón de que los umbrales configurables viven en `codeqa.json`, no en
  flags de una sola corrida; el gate sigue esa misma convención (ver §4).

## 3. Estado actual — qué ya existe (para no reabrir Fases 9-10)

- El comando objetivo del roadmap ya funciona hoy tal cual, salvo el gate:
  `codeqa analyze ./project --config codeqa.json --format json --output
  report.json` (`--config`/`--format`/`--output` de Fase 9,
  `RunApplication.cpp:580-711`).
- archivos analizados / lenguajes / funciones / diagnósticos / métricas /
  hallazgos / informe texto+JSON: Fase 9 (`codeqa::report`).
- snapshot guardado y comparación contra el anterior: Fase 10
  (`codeqa::history`, `SaveSnapshot`/`ListSnapshots`/`LoadSnapshot`).
- resumen por severidad: ya en `renderText`/`toReportJson` (Fase 9).
- Lo único que falta de la lista del roadmap: el código de salida coherente
  con el quality gate.

## 4. Quality gate — configuración

Nuevo campo de nivel superior en `codeqa.json`, junto a `languages`/`rules`
(`ConfigJson.cpp:215`, `requireKeysSubset`):

```json
{
  "qualityGate": { "minSeverity": "warning" }
}
```

- Nuevo `QualityGateConfig { codeqa::core::DiagnosticSeverity minSeverity =
  DiagnosticSeverity::Warning; }`, campo `qualityGate` en `ProjectConfig`
  (`ConfigModels.h`).
- `defaultConfig()` (`DefaultConfig.cpp`) no necesita tocarse — el
  inicializador por defecto del struct ya deja `minSeverity = Warning`,
  coherente con que las 8 reglas de hoy solo emiten `Warning`/`Error`
  (nunca `Information`, deferred finding de Fase 9).
- `parseConfig()`/`toJson()` (`ConfigJson.cpp`): `qualityGate` se agrega a
  los keys permitidos de nivel superior; adentro, `minSeverity` es un
  string validado contra los tres valores de `DiagnosticSeverity` vía un
  nuevo `diagnosticSeverityFromJsonString` (`EnumJson.h`/`.cpp`, simétrico
  al ya existente `languageFromJsonString`) — nuevo código `CONFIG009` si
  el string no matchea ("Error [CONFIG009]: 'qualityGate.minSeverity' must
  be 'information', 'warning', or 'error', got '<valor>'.").
- El objeto `qualityGate` completo es opcional (como `languages`/`rules` ya
  lo son) — su ausencia deja el default. Si `qualityGate` está presente,
  `minSeverity` adentro también es opcional (a diferencia de
  `rules.<id>.enabled`/`params`, que sí son obligatorios una vez que el id
  aparece) — es el único campo del objeto, y forzar a repetirlo siempre que
  se quiera tocar cualquier otra futura opción del gate no aporta nada hoy;
  su ausencia dentro de un `qualityGate` presente también deja `Warning`.

## 5. Quality gate — detección y evaluación

Roadmap (líneas 852-855): con snapshot anterior, el gate falla solo por
hallazgos **nuevos** con severidad ≥ configurada; sin snapshot anterior
(primera corrida), falla en modo absoluto (cualquier hallazgo ≥
configurada).

Ambos modos los resuelve una sola función nueva junto a `compareSnapshots`
(`include/codeqa/history/CompareSnapshots.h`,
`src/history/CompareSnapshots.cpp`):

```cpp
[[nodiscard]] bool hasNewFindingAtOrAboveSeverity(
    const codeqa::rules::ProjectFindingsModel& current,
    const FindingKeySet& previousKeys,
    codeqa::core::DiagnosticSeverity minSeverity);
```

Recorre `current.files[].findings[]` en orden, consumiendo una copia local
de los contadores de `previousKeys` por `FindingKey` — exactamente la misma
noción de identidad y la misma lógica de "diff tipo multiset" que
`compareSnapshots` ya usa desde Fase 10 (mismo `FindingKey` =
`(ruleId, symbolName, path)`, sin línea/columna). Un finding cuya clave ya
no tiene ocurrencias previas para consumir es "nuevo"; si su severidad es ≥
`minSeverity`, la función retorna `true` de inmediato.

- **Modo diferencial**: `previousKeys = extractFindingKeys(*previousReport)`
  (ya existe, Fase 10).
- **Modo absoluto** (primera corrida): se llama con `previousKeys` vacío —
  como ningún key tiene ocurrencias previas, *todo* finding actual cuenta
  como nuevo, sin ninguna rama especial. Un mapa vacío ya representa "no
  hay snapshot anterior" con exactitud, sin duplicar la lógica de diff en
  dos caminos.

Se elige esta forma (una función nueva, sin tocar `FindingKeySet`/
`SnapshotComparison`) en vez de extender esos tipos ya shippeados en Fase
10 para cargar severidad por clave: misma corrección observable, superficie
menor, cero riesgo sobre código ya testeado (decisión validada con el
usuario durante el brainstorming, §5 de la conversación).

`runAnalyze` (`RunApplication.cpp`) llama a esta función después de
imprimir el reporte y guardar el snapshot — el gate es un resultado del
análisis completo, no un motivo para saltarse ninguno de los dos — y
retorna `ExitCode::QualityGateFailed` si es `true`, `ExitCode::Success` si
no.

## 6. Códigos de salida

`ExitCode.h` pasa de 3 a 5 valores, siguiendo la tabla exacta del roadmap:

```cpp
enum class ExitCode : int
{
    Success = 0,
    QualityGateFailed = 1,
    ConfigurationError = 2,
    InvalidArguments = 3,
    InternalError = 4
};
```

- `ConfigurationError` (nuevo) reemplaza a `InvalidArguments` únicamente en
  el único call site donde `runAnalyze` falla al cargar `--config`
  (`RunApplication.cpp:593-602`, hoy retorna `InvalidArguments` genérico).
  Es el único lugar del código que hoy distingue "el archivo de config está
  mal" de "los argumentos de CLI están mal".
- Todo el resto de usos de `InvalidArguments` (flags desconocidos, `<path>`
  faltante o inválido, `--lang-override` malformado, `--output` no
  escribible, subcomando desconocido) no cambia de código en el archivo —
  solo cambia su valor numérico, de 2 a 3, automáticamente vía el enum.
- `InternalError` (4) no cambia — sigue siendo el catch-all de `main.cpp`
  para excepciones no manejadas (fuera del alcance de esta fase).

## 7. Corrección de los dos bugs de Fase 11

Ambos bugs comparten una sola causa raíz, confirmada leyendo el código
actual: `sourceRangeForCandidate()` (`StructuralExtractor.cpp:444-466`)
confía ciegamente en el rango que tree-sitter calcula para un nodo
`function_definition`/`struct_specifier`. Bajo un patrón `#if X { ... #else
{ ... #endif` (llaves de apertura textualmente desbalanceadas en cada
rama), tree-sitter-c/cpp no evalúa el preprocesador — su recuperación de
error puede producir un árbol sin nodos `ERROR`/`MISSING` (por lo que el
filtro `subtreeContainsErrorOrMissing` no lo detecta) pero con el punto
final del nodo empujado cientos o miles de líneas más allá, hasta un `}` no
relacionado. Confirmado contra los dos casos reales del ledger de Fase 11:
`winWrite` (`sqlite3.c:49451`, 105 líneas reales reportadas como 19703) y
`abort_fuzzing_if` (fmt `format.h:279`, 6 líneas reales reportadas como
574) — ambos con `decision.category: "analyzer-defect"`.

Se corrigen como dos síntomas independientes, cada uno con el arreglo más
chico que resuelve exactamente lo observado — ninguno intenta interpretar
preprocesador (fuera de alcance, ver §2):

### 7.1 Fuga de scope (`write_int_data::` en símbolos libres)

`qualifiedNameFor` (`src/model/ProjectModelBuilder.cpp:155-231`) decide la
clase que encierra una función escaneando `file.classes` y comprobando
contención geométrica de bytes (`encloses()`, líneas 142-148) contra
`ClassModel.range` — que puede estar corrompido por la misma causa raíz de
arriba, que infla el rango de un `struct_specifier`. Mientras tanto, la
clasificación `FunctionKind` (Method vs. Free) ya se calcula correctamente
en `StructuralExtractor.cpp` (`hasClassAncestor`/`enclosingClassName`,
líneas 348-401) caminando el árbol real de ancestros — el propio ledger de
Fase 11 confirma que `write_significand`/`write_int`/`format_float` están
correctamente clasificados `Free` a pesar de mostrar el prefijo erróneo en
`symbolName`.

Arreglo: `qualifiedNameFor` no debe calificar con ningún nombre de clase a
una función cuya extracción ya determinó `FunctionKind::Free` — sin
importar qué rango geométrico la contenga. Esto resuelve la fuga sin
necesitar detectar la corrupción del rango de la clase en absoluto.

### 7.2 Longitud de función fabricada

`CalculateFunctionMetrics.cpp:92` (`calculateSizeMetrics`) calcula
`physicalLines` por aritmética pura sobre `callable.symbol.range`, sin
ninguna verificación de plausibilidad. Arreglo: un nuevo paso de validación
al final de `StructuralExtractor::extract` (después de que
`result.functions`/`result.classes` estén completos, ya que un candidato
"tragado" puede aparecer más adelante en el recorrido) — para cada rango de
función/clase de nivel superior, comprobar si el punto de inicio de algún
otro candidato de nivel superior cae dentro de ese rango sin ser su
descendiente real en el AST (mismo dato de ancestros real ya usado para
`hasClassAncestor`). Si se encuentra uno, el rango es geométricamente
imposible: en vez de intentar repararlo con una heurística numérica (p.ej.
"descartar si es +100x el límite") — inventar un umbral sin datos que lo
respalde es el mismo tipo de certeza falsa que `REAL-PROJECTS.md` ya
rechazó para otra decisión —, se agrega un diagnóstico nuevo `PARSE004`
("Function or type range appears corrupted.", `DiagnosticSeverity::Error`
— misma severidad que `PARSE001`/`PARSE002`/`PARSE003`) a
`result.diagnostics`, el mismo vector de `ParsedFile` que
`ParseSourceFile.cpp` ya llena (el extractor ya recibe ese `ParsedFile&`
por referencia — no hace falta ningún cambio de interfaz).

`ApplyRules::overlapsParseError` (`ApplyRules.cpp:81-98`) ya filtra
cualquier función que se solape con `PARSE001`/`PARSE002` antes de
evaluarla contra ninguna regla — se le agrega `PARSE004` a esa misma lista
de códigos. El finding fabricado de `size.function-length` deja de
generarse por completo, sin tocar ninguna regla ni el motor de reglas.

**Deliberadamente no se toca** (documentado, no un olvido): `MeasureProject.cpp`
nunca filtró por diagnóstico desde Fase 6b — la métrica cruda
(`physicalLines`) seguirá mostrando un valor grande para una función con
`PARSE004` en el reporte JSON/texto, exactamente igual a como ya se
comporta hoy para `PARSE001`/`PARSE002`. Ver §2 para el porqué de no
extender ese filtro también a las métricas crudas en esta fase.

## 8. Manejo de errores y verificación

- `hasNewFindingAtOrAboveSeverity`: tests unitarios cubriendo modo absoluto
  (mapa previo vacío), modo diferencial (con y sin findings nuevos), y el
  límite exacto en `minSeverity` (un finding con severidad igual al umbral
  configurado debe contar, no solo por encima).
- Parsing de `qualityGate` en `ConfigJson.cpp`: tests para el valor por
  defecto (ausencia del campo), los tres valores válidos, y `CONFIG009`
  sobre un string inválido — mismo patrón que los tests ya existentes de
  `CONFIG006` para `language`.
- Dos fixtures nuevas en el corpus, reproduciendo cada patrón a escala
  mínima (no los archivos reales de SQLite/fmt, que no se commitean —
  mismo principio que `REAL-PROJECTS.md` ya aplica):
  - una función C envuelta en `#if ... { #else { #endif` para
    `StructuralExtractorTest`/`CalculateFunctionMetricsTest` — el nuevo
    `PARSE004` debe aparecer, y `size.function-length` no debe generar
    finding para esa función tras `ApplyRules`.
  - un struct C++ con el mismo patrón en su cuerpo seguido de una función
    libre, para `ProjectModelBuilderTest` — la función libre debe quedar
    con `symbolName` sin calificar, sin importar el estado del rango del
    struct.
- Verificación end-to-end (criterio de aceptación del roadmap "el MVP puede
  analizar al menos un proyecto real completo"): re-correr `codeqa analyze
  --format json` contra el mismo SQLite 3.46.0 usado en Fase 11 (versión ya
  fijada en `tests/corpus/REAL-PROJECTS.md`) y confirmar en el reporte
  resultante que `winWrite` ya no aparece con un `size.function-length`
  fabricado. No es parte de la suite automatizada (mismo motivo que el
  corpus de validación de Fase 11: depende de una descarga externa
  versionada, no de un fixture committeado) — se documenta el resultado en
  el reporte del task que lo ejecute.

## 9. Criterios de aceptación

Mapeados a los del roadmap (líneas 867-875):

- el análisis completo es reproducible — sin cambios de esta fase, ya lo
  era desde Fase 9.
- los resultados coinciden con el corpus esperado — §8, fixtures nuevas más
  las ya existentes.
- la herramienta no se bloquea ante archivos problemáticos — §7.2, el
  rango corrompido ahora produce un diagnóstico en vez de un finding
  fabricado (ni crasheaba ni bloqueaba nada antes tampoco — este criterio
  ya estaba cubierto; el fix mejora la *exactitud*, no la robustez).
- los hallazgos son accionables — §7.1, `symbolName` deja de mentir sobre
  qué símbolo violó la regla.
- el motor no depende de una interfaz gráfica — sin cambios, ya era CLI
  puro.
- el quality gate es diferencial cuando hay snapshot anterior, y absoluto
  en la primera ejecución — §5.
- el MVP puede analizar al menos un proyecto real completo — §8,
  verificación end-to-end contra SQLite.

## 10. Dependencias

Fases 0-11 (roadmap). En la práctica: Fase 9 (`--config`/`--format`/
`--output`, `codeqa::report`) y Fase 10 (`codeqa::history`, snapshots) son
los dos componentes de código que esta fase extiende directamente; Fase 8
(`codeqa::config`) es el que recibe el nuevo campo `qualityGate`; Fase 7
(`ApplyRules`, `PARSE001`/`PARSE002`) es el mecanismo que se reutiliza para
`PARSE004`.
