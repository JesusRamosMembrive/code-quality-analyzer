# Fase 7 — Motor de reglas y hallazgos: diseño

Fecha: 2026-08-09
Estado: revisado (brainstorming 2026-08-09); pendiente de aprobación final

## 1. Objetivo

Evaluar las métricas producidas por la Fase 6b (y algunos datos crudos del
modelo común que ninguna métrica captura todavía) mediante un conjunto fijo
de reglas, y producir hallazgos (`Finding`) localizables y explicables, sin
generar todavía ningún informe ni depender de ninguna configuración externa.

## 2. Alcance y límites

Esta fase incluye:

- una capa nueva `codeqa::rules`, separada de `codeqa::metrics`, con el
  modelo de hallazgos y la interfaz única de regla que define
  `01-cpp-mvp-roadmap.md` (Fase 7);
- un orquestador nuevo `codeqa::application::ApplyRules` que aplica las
  reglas sobre un `ProjectModel` + `ProjectMetricsModel` ya construidos;
- las 8 reglas iniciales que propone el roadmap, todas comunes a C y C++;
- la regla de confianza ante `ERROR`/`MISSING` y la supresión inline por
  comentario, ambas ya decididas en el roadmap;
- dos cambios quirúrgicos sobre fases ya fusionadas (Fase 4 y Fase 6b, ver
  sección 7) — necesarios para que los hallazgos sean precisos sin duplicar
  lógica de escaneo de texto.

No incluye: ningún comando CLI nuevo (Fase 9, igual que con las métricas);
lectura de `codeqa.json` ni ningún otro mecanismo de configuración en
tiempo de ejecución (Fase 8); reglas específicas de C o de C++ (las 8
iniciales son todas comunes); supresión por configuración, solo inline;
distinción entre regla "experimental" y "estable" (criterio de aceptación
explícito de la Fase 11, no de esta); resolución de tipos o análisis
semántico más allá de lo que el modelo común ya expone.

## 3. Arquitectura

```
model::ProjectModel (Fase 5)  +  metrics::ProjectMetricsModel (Fase 6b)
        │
        ▼
ApplyRules::execute(project, metrics)
        │  por archivo: relee una vez con
        │  codeqa::infrastructure::readFileContents (nuevo, ver §7)
        ▼
por cada CallableModel no-prototipo cuyo rango NO se solape con un
diagnóstico PARSE001/PARSE002 del archivo:
        │
        ▼
    para cada Rule de allRules() cuyo appliesTo(language) sea true:
        rule.evaluate(RuleContext{metrics, callable}) -> vector<Finding>
        │
        ▼
    filtro de supresión inline (comentario en la línea anterior a
    finding.location, ver §8)
        │
        ▼
ProjectFindingsModel
```

Decisiones de diseño clave:

- **`RuleContext` es deliberadamente estrecho**: `FunctionMetricsModel`
  (Fase 6b) + `model::CallableModel` (para datos crudos que las métricas no
  capturan, como el recuento de variables locales). Ninguna regla ve texto
  fuente, comentarios ni diagnósticos — eso es responsabilidad exclusiva
  del orquestador. Esto es intencionalmente distinto de cómo
  `calculateFunctionMetrics` recibe `sourceLines`: las reglas nunca
  necesitan escanear texto, solo leer campos ya calculados o ya
  estructurados.
- **La regla de confianza se aplica *antes* de invocar ninguna regla**, no
  como filtro posterior: una función solapada con un diagnóstico de parseo
  ni siquiera construye un `RuleContext`.
- **La supresión se aplica *después*, sobre los `Finding` ya generados** —
  es el único paso que necesita releer texto fuente, porque
  `FileModel.comments` solo guarda rangos de bytes, nunca el contenido.
- **`codeqa::rules` no depende de E/S ni de `codeqa::frontend`**, mismo
  principio que `codeqa::metrics`. Solo `ApplyRules` (en
  `codeqa::application`) toca disco, igual que `MeasureProject`.
- **Sin jerarquía de tipos entre reglas de umbral y de patrón** (decisión
  ya fijada por el roadmap): las 8 reglas implementan la misma interfaz
  `Rule`. Internamente, y solo como detalle de implementación privado (no
  parte de la API pública), comparten un helper `evaluateThreshold` para no
  repetir 8 veces la misma lógica de comparación con dos umbrales — esto
  no es una jerarquía de tipos, es una función libre reutilizada.
- **Sin mecanismo de registro dinámico ni de plugins**: `allRules()` es una
  función libre que devuelve una lista fija de instancias. No existe
  configuración que active o desactive una regla en tiempo de ejecución
  todavía — "una regla puede desactivarse" (criterio del roadmap) se
  satisface en esta fase a nivel de código (no registrarla en `allRules()`
  la desactiva), no a nivel de usuario final; eso es explícitamente trabajo
  de la Fase 8.

## 4. Modelo público

```cpp
namespace codeqa::rules
{

struct Finding
{
    std::string ruleId;
    codeqa::core::DiagnosticSeverity severity;
    std::string description;
    std::string recommendation;
    std::optional<long long> observedValue;
    std::optional<long long> limitValue;
    codeqa::core::SourceLocation location;
    std::string symbolName;
};

struct FileFindingsModel
{
    std::filesystem::path path;
    std::vector<Finding> findings;
};

struct ProjectFindingsModel
{
    std::filesystem::path rootPath;
    std::vector<FileFindingsModel> files;
};

struct RuleContext
{
    const codeqa::metrics::FunctionMetricsModel& metrics;
    const codeqa::model::CallableModel& callable;
};

class Rule
{
public:
    virtual ~Rule() = default;
    [[nodiscard]] virtual std::string id() const = 0;
    [[nodiscard]] virtual bool appliesTo(codeqa::core::Language language) const = 0;
    [[nodiscard]] virtual std::vector<Finding> evaluate(const RuleContext& context) const = 0;
};

[[nodiscard]] std::vector<std::unique_ptr<Rule>> allRules();

} // namespace codeqa::rules
```

```cpp
namespace codeqa::application
{

class ApplyRules
{
public:
    [[nodiscard]] codeqa::rules::ProjectFindingsModel execute(
        const codeqa::model::ProjectModel& project,
        const codeqa::metrics::ProjectMetricsModel& metrics) const;
};

} // namespace codeqa::application
```

Notas sobre los tipos:

- `severity` reutiliza `codeqa::core::DiagnosticSeverity` (Information/
  Warning/Error) — no se introduce un enum nuevo solo para hallazgos.
- `observedValue`/`limitValue` son `std::optional<long long>`, no
  obligatorios: las 8 reglas iniciales siempre los rellenan, pero el propio
  roadmap escribe "cuando aplique" en el Alcance de la Fase 7, anticipando
  reglas futuras sin valor numérico (p.ej. una futura regla de patrón puro
  como "ausencia de `override`"). No es una abstracción anticipada: es
  implementar literalmente lo que el roadmap ya especifica.
- `location` es `codeqa::core::SourceLocation` (un punto, no un rango),
  mismo tipo que ya usa `Diagnostic.location` — consistente con el patrón
  existente, suficiente para el ejemplo de salida que da el roadmap
  (`archivo:línea`).
- **Los identificadores de regla usan notación con puntos, agrupados por
  familia de métrica** (`size.*`, `signature.*`, `complexity.*`) — el
  ejemplo de salida de la Fase 7 en el roadmap usa `complexity.cyclomatic`,
  pero el ejemplo de configuración de la Fase 8 usa
  `"cyclomatic-complexity"` (con guion, y literalmente otro nombre). Es una
  inconsistencia real del propio roadmap entre dos fases; se resuelve aquí
  a favor de la notación con puntos, y la Fase 8 deberá usar estos mismos
  identificadores cuando lea `codeqa.json`.
- `appliesTo(Language)` existe y se ejercita (con una regla de prueba que
  devuelve `false` para un lenguaje, ver §9), pero las 8 reglas de esta
  fase devuelven `true` para C y C++ por igual — ninguna es específica de
  lenguaje todavía.

## 5. Las 8 reglas

Cada regla es una clase pequeña que extrae su valor observado de
`RuleContext` y delega la decisión de severidad a un helper privado
`evaluateThreshold(ruleId, description, recommendation, observedValue,
warningLimit, highLimit, location, symbolName) -> std::optional<Finding>`:
por debajo de `warning` no genera hallazgo; entre `warning` y `high`
genera `Warning`; en `high` o por encima, `Error` — y `limitValue` reporta
el umbral que realmente determinó la severidad obtenida (no siempre el de
`warning`).

| id | valor observado | ubicación |
|---|---|---|
| `size.function-length` | `metrics.size.physicalLines` | inicio de función |
| `size.local-variables` | `callable.localVariables.size()` | inicio de función |
| `signature.parameter-count` | `metrics.signature.parameterCount` | inicio de `callable.parameterListRange` |
| `signature.consecutive-booleans` | racha más larga de parámetros `bool`/`_Bool` por valor consecutivos en `callable.parameters` (mismo criterio que `isBooleanValueParameter` de Fase 6b: `const`/`volatile` se ignoran, `passing != Value` rompe la racha) | inicio de `callable.parameterListRange` |
| `complexity.cyclomatic` | `metrics.complexity.cyclomaticComplexity` | inicio de función |
| `complexity.condition` | `metrics.complexity.maxConditionComplexity` | `metrics.complexity.maxConditionRange` (campo nuevo, ver §7) |
| `complexity.nesting` | `metrics.complexity.maxNestingDepth` | `range` de la entrada de `callable.controlFlow` cuyo `nestingDepth` sea igual al máximo (empate → la primera en orden de aparición) |
| `complexity.returns` | `metrics.complexity.returnCount` | inicio de función |

Para `signature.parameter-count` y `signature.consecutive-booleans`: si
`callable.parameterListRange` es `std::nullopt` (lambda sin lista de
parámetros), `callable.parameters` está necesariamente vacío también — la
regla nunca cruza ningún umbral en ese caso, así que la ausencia de
ubicación no se llega a ejercitar en la práctica; aun así, la
implementación debe caer de vuelta al inicio de función de forma
defensiva, sin asumir que `parameterListRange` siempre tiene valor cuando
la regla dispara.

## 6. Umbrales por defecto: procedencia y estado de validación

**Ninguno de estos 8 umbrales está validado contra proyectos reales.** Son
valores de partida razonables, no resultados de medición. La Fase 11
(validación sistemática) es la que debe medir precisión/recall de cada uno
contra proyectos reales antes de que se consideren fiables por defecto —
la propia Fase 11 fija como criterio de aceptación distinguir "regla
experimental" de "regla estable", precisamente para esto. Esta tabla existe
para que la Fase 11 no tenga que reconstruir esta información: sabe, desde
el primer momento, que las 8 reglas empiezan como no válidadas, no solo
las que "parezcan" dudosas.

| id | warning | high | procedencia |
|---|---|---|---|
| `size.function-length` | 50 | 90 | roadmap (ejemplo de configuración, Fase 8) |
| `size.local-variables` | 8 | 12 | estimado, sin precedente en el roadmap |
| `signature.parameter-count` | 4 | 7 | estimado, sin precedente en el roadmap |
| `signature.consecutive-booleans` | 2 | 3 | estimado, sin precedente en el roadmap |
| `complexity.cyclomatic` | 10 | 15 | roadmap (ejemplo de configuración, Fase 8) |
| `complexity.condition` | 3 | 5 | estimado, sin precedente en el roadmap |
| `complexity.nesting` | 3 | 5 | estimado, sin precedente en el roadmap |
| `complexity.returns` | 4 | 6 | estimado, sin precedente en el roadmap |

`complexity.nesting` merece una advertencia propia: hereda la semántica no
intuitiva de `maxNestingDepth` documentada en
`docs/superpowers/specs/2026-08-08-fase-6b-motor-metricas-design.md` §5 —
una cadena `if`/`else if` cuenta cada rama como anidada, y un `return`
suelto puede superar a cuatro niveles de llaves visuales cuyo contenido más
interno no sea `Return`/`Break`/`Continue`. Es razonablemente probable que
esta regla concreta tenga una tasa de falsos positivos más alta que las
demás hasta que la Fase 11 la mida.

Todos los umbrales son constantes privadas en el código de cada regla, sin
ningún mecanismo de configuración expuesto — cambiarlos hoy significa
editar una constante y recompilar. La Fase 8 los hace ajustables vía
`codeqa.json`.

## 7. Cambios sobre fases ya fusionadas

Esta fase toca código ya revisado y fusionado en dos puntos concretos,
ambos quirúrgicos y justificados por evitar duplicar lógica no trivial:

**Fase 6b — `FunctionComplexityMetrics` gana un campo al final:**

```cpp
struct FunctionComplexityMetrics
{
    std::size_t cyclomaticComplexity{0};
    std::size_t maxConditionComplexity{0};
    std::size_t maxNestingDepth{0};
    std::size_t branchCount{0};
    std::size_t returnCount{0};
    std::optional<codeqa::core::SourceRange> maxConditionRange;
};
```

Se rellena en `calculateComplexityMetrics`
(`src/metrics/CalculateFunctionMetrics.cpp`), en el mismo punto donde ya se
actualiza `maxConditionComplexity`: hoy usa `std::max`, que no conserva cuál
`ControlFlowModel` produjo el máximo; pasa a una comparación explícita que
también captura el `conditionRange` de esa entrada (empate → la primera en
orden de aparición, mismo criterio determinista que el resto del proyecto).
Sin este campo, `complexity.condition` no podría apuntar a la condición
exacta sin volver a invocar la lógica de escaneo de texto
(`countBooleanOperators`, con manejo de literales de cadena, comentarios y
separadores de dígitos) — que ya tuvo un bug real durante la Fase 6b y no
se debe duplicar.

**Fase 4 y Fase 6b — extracción de `codeqa::infrastructure::readFileContents`:**

```cpp
namespace codeqa::infrastructure
{

[[nodiscard]] std::optional<std::string> readFileContents(const std::filesystem::path& path);

} // namespace codeqa::infrastructure
```

`ParseSourceFile.cpp` (Fase 4) y `MeasureProject.cpp` (Fase 6b) tienen cada
uno su propia copia casi idéntica de esta función (apertura binaria,
`file_size`, lectura completa, gestión de error). `ApplyRules` necesita la
misma lectura por un tercer motivo (extraer el texto de los comentarios
para la supresión inline) — la propia revisión final de la Fase 6b ya
señaló esto como la condición que justificaría centralizar: "revisit when
a third reader appears". Se extrae ahora, y los dos puntos existentes se
migran a la función compartida en el mismo cambio. Introduce una
dependencia real nueva: `codeqa::frontend` pasa a depender de
`codeqa::infrastructure` (antes no dependía de esa capa en absoluto).

**Duplicaciones deliberadas: cuatro helpers, no dos.** Aparte de esos dos
cambios sobre código ya fusionado, esta fase copia cuatro funciones
auxiliares pequeñas de fases anteriores en vez de exportarlas a una
ubicación compartida (no modifica su origen, solo lo duplica). Este es el
recuento completo:

| helper | origen | copia nueva en esta fase | para qué |
|---|---|---|---|
| `positionInRange` | `src/metrics/CalculateFunctionMetrics.cpp` | `src/application/ApplyRules.cpp` | filtro de confianza: ¿cae un diagnóstico de parseo dentro del rango de la función? (§8) |
| `isBooleanValueParameter` | `src/metrics/CalculateFunctionMetrics.cpp` | `src/rules/Rules.cpp` | criterio de "booleano por valor" para `signature.consecutive-booleans` |
| `extractRangeText` | `src/metrics/CalculateFunctionMetrics.cpp` | `src/application/ApplyRules.cpp` | leer el texto de un comentario para buscar la directiva de supresión (§8) |
| `splitLines` | `src/application/MeasureProject.cpp` | `src/application/ApplyRules.cpp` | trocear el contenido del archivo en líneas (con normalización CRLF) |

Los cuatro siguen por debajo del umbral de la "regla de tres" que ya se
aplicó en la revisión final de la Fase 6b para no extraer utilidades
compartidas prematuramente: cada uno está exactamente en su **segunda**
aparición, ninguno en la tercera, y ninguno pasa de ~25 líneas. Las cuatro
copias quedan anotadas en el propio código con un comentario que apunta a su
origen y a este criterio, para que la duplicación se lea como deliberada y
no como descuido.

Este recuento (cuatro, no dos) es el dato que debe consultar quien planifique
la siguiente fase al decidir si alguno merece ya extraerse a una ubicación
compartida — la recomendación de la revisión final de esta fase es
precisamente que esa decisión se tome de una vez y con los cuatro a la vista,
en lugar de dejar que cada uno llegue a su tercera copia por separado. Esta
fase no extrae ninguno; solo deja el inventario exacto.

## 8. Confianza ante errores de parsing y supresión inline

**Filtro de confianza** (antes de evaluar cualquier regla, por función): si
algún diagnóstico de `model::FileModel.diagnostics` con código `PARSE001`
o `PARSE002` tiene su `location` dentro de `callable.symbol.range`, la
función se descarta por completo — ninguna regla se invoca sobre ella, y
solo queda el diagnóstico de parseo correspondiente.

**Supresión inline** (después de generar los `Finding`, antes de incluirlos
en el resultado): para cada `Finding`, se busca un comentario en
`FileModel.comments` cuyo rango cubra exactamente la línea
`finding.location.line - 1` — la línea inmediatamente anterior a donde
apunta el hallazgo, sin buscar más atrás. Se extrae su texto (usando
`sourceLines`, ya leído una vez por archivo) y se busca dentro de él —no se
exige que el comentario completo sea únicamente la directiva, puede llevar
texto adicional antes o después (p.ej. una explicación)— el patrón
`codeqa-disable-next-line\s+([A-Za-z0-9_.\-]+)`; si el identificador
capturado coincide exactamente con `finding.ruleId`, el hallazgo se
descarta. La clase de caracteres del identificador excluye deliberadamente
puntuación de frase (comas, puntos finales de oración, paréntesis) para que
un comentario como `// codeqa-disable-next-line complexity.cyclomatic,
revisado por X` no arrastre la coma dentro del id capturado.

Como `location` es ahora específica por regla (§5) y no siempre el inicio
de función, la supresión resulta más granular de lo que parecía al
principio del diseño: el comentario va justo encima de la construcción
concreta (la condición, la lista de parámetros), no necesariamente encima
de toda la función.

Un identificador de regla mal escrito en el comentario de supresión falla
en silencio — no hay forma de avisar al usuario todavía porque no existe
capa de reporting. Se acepta como limitación del MVP; el roadmap tampoco
pide detección de errores de supresión.

## 9. Verificación

- **Reglas individuales**: cada una de las 8 se prueba con un `RuleContext`
  construido a mano — casos justo por debajo de `warning`, exactamente en
  `warning`, entre `warning` y `high`, exactamente en `high`, y por encima.
- **`evaluateThreshold`**: se prueba una vez, de forma aislada, la tabla de
  fronteras de severidad — evita repetirla 8 veces.
- **`ApplyRules`**: pruebas de integración con archivos reales (patrón
  `TempProject`, como en `MeasureProjectTest.cpp`) cubriendo el filtro de
  confianza, la supresión (con id correcto, con id incorrecto, sin
  comentario) y al menos un caso multi-regla/multi-función end-to-end sobre
  el pipeline real `AnalyzeProject → MeasureProject → ApplyRules`.
- **Alcance por lenguaje**: una regla de prueba (solo en el test, nunca
  registrada en `allRules()`) que devuelve `false` para un lenguaje,
  verificando que la interfaz `Rule` despacha `appliesTo()`/`evaluate()`
  correctamente de forma aislada. Como las 8 reglas reales devuelven `true`
  para ambos lenguajes, ninguna ejercita la rama negativa del `if` de
  `ApplyRules` que la consulta — verificar eso a nivel de orquestador
  exigiría hacer inyectable la lista de reglas solo para alcanzar una
  comprobación de una línea; se acepta como hueco de cobertura documentado
  en vez de ampliar la API pública de `ApplyRules` sin una segunda regla
  real que lo necesite.
- **`readFileContents`**: cubierta indirectamente por la suite ya existente
  de `ParseSourceFileTest.cpp` y `MeasureProjectTest.cpp` (que no cambian
  de comportamiento tras la migración) más un test dedicado a la nueva
  función en su ubicación de infraestructura.
- **Corpus**: mismo procedimiento de "bootstrap verificado" que en la Fase
  6b — fixtures `*.findings.json` junto a fuentes existentes que ya
  contengan construcciones por encima de algún umbral (p.ej.
  `nesting/deep-nesting.cpp`).

## 10. Criterios de aceptación

- Métricas y reglas están separadas: ninguna regla recalcula una métrica,
  solo lee `RuleContext`.
- Cada hallazgo tiene una ubicación específica de su regla (no siempre el
  inicio de función, ver §5).
- Cada hallazgo identifica su regla (`ruleId`) y su función (`symbolName`).
- Los umbrales de cada regla son ajustables a nivel de código (constantes
  con nombre) — la configurabilidad en tiempo de ejecución es explícitamente
  trabajo de la Fase 8, no de esta.
- Una regla no registrada en `allRules()` no se evalúa nunca — la
  desactivación en esta fase es a nivel de código, no de usuario final.
- Una regla cuyo `appliesTo(language)` es `false` no se ejecuta sobre
  archivos de ese lenguaje (verificado con una regla de prueba).
- Una función cuyo rango se solapa con un diagnóstico `PARSE001`/`PARSE002`
  no genera ningún hallazgo, solo se conserva el diagnóstico de parseo.
- Un hallazgo suprimido mediante comentario inline (`// codeqa-disable-next-line
  <rule-id>` en la línea anterior a `location`) no aparece en
  `ProjectFindingsModel`.
- Los 8 umbrales de partida están documentados como no validados (§6), no
  presentados como resultados de medición.
- `codeqa parse` y `codeqa` (sin nueva superficie CLI) no cambian su
  comportamiento visible.

## 11. Dependencias

Fase 6b.
