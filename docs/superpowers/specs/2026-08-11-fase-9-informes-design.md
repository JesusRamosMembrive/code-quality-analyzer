# Fase 9 — Informes de texto y JSON: diseño

Fecha: 2026-08-11
Estado: revisado (brainstorming 2026-08-11); pendiente de aprobación final

## 1. Objetivo

Un comando `codeqa analyze <path>` que ejecute el pipeline completo
(descubrimiento → detección de lenguaje → parseo → modelo común → métricas →
reglas) y produzca el resultado en dos formatos: texto legible por personas
y JSON determinista consumible por otras herramientas (incluida, más
adelante, la GUI). Esta es también la primera vez que `--config` se conecta
a un comando real, cerrando el pendiente que Fase 8 dejó explícitamente para
esta fase.

## 2. Alcance y límites

Esta fase incluye:

- un orquestador nuevo `codeqa::application::RunAnalysis` que encadena
  `AnalyzeProject` → `MeasureProject` → `ApplyRules` a partir de una
  `ProjectConfig` ya resuelta;
- el subcomando `codeqa analyze <path>`, con `--exclude`/`--lang-override`
  (ya existentes, reutilizados), más `--config`, `--format text|json` y
  `--output <file>` (nuevos);
- wiring de `--config`: lee `codeqa.json` con `LoadProjectConfig`, y sus
  `headerOverrides` se combinan con `--lang-override` con la CLI ganando en
  caso de conflicto;
- una capa nueva `codeqa::report` con el formateador de texto y el
  serializador JSON, ambos construidos a partir de `ProjectModel` +
  `ProjectMetricsModel` + `ProjectFindingsModel` (+ `ProjectConfig` para el
  JSON).

Esta fase **no** incluye, y por qué:

- **Exit code distinto de 0 por hallazgos.** Decisión explícita del usuario
  en el brainstorming: gating por severidad (`--fail-on`) es una política de
  CI que no está en el alcance del roadmap para esta fase; se añade el día
  que un caller concreto lo necesite. `analyze` solo devuelve no-cero por
  errores de argumentos/config o fallos internos, igual que `discover`/`parse`.
- **`excludePatterns` vía `codeqa.json`.** Sigue sin haber un ejemplo
  concreto en el roadmap ni una necesidad real: `--exclude` ya cubre el
  caso, y añadirlo a config sin consumidor sería la misma sobre-ingeniería
  que Fase 8 evitó explícitamente.
- **Snapshots / histórico.** Es Fase 10 completa; esta fase solo deja el
  esquema JSON versionado que Fase 10 reutilizará tal cual (su propio
  roadmap lo exige: "no se define un segundo formato en paralelo").
- **Un cliente GUI real.** El criterio de aceptación pide que el JSON
  *pueda* usarse desde la GUI, no que la GUI exista.

## 3. Arquitectura

```
CLI (analyze <path> --exclude … --lang-override … --config … --format … --output …)
        │
        │ resuelve ProjectConfig (defaultConfig() o merge vía LoadProjectConfig)
        │ combina headerOverrides (config) ++ --lang-override (CLI) — CLI gana
        ▼
codeqa::application::RunAnalysis::execute(RunAnalysisRequest)
        │
        ├─ AnalyzeProject::execute(...)   -> ProjectModel      (ya existe, Fase 5)
        ├─ MeasureProject::execute(model) -> ProjectMetricsModel (ya existe, Fase 6b)
        └─ ApplyRules::execute(model, metrics, config) -> ProjectFindingsModel (ya existe, Fase 7/8)
        │
        ▼
RunAnalysisResult { model, metrics, findings }
        │
        ├─ codeqa::report::renderText(ostream, model, metrics, findings)
        └─ codeqa::report::toReportJson(model, metrics, findings, config, toolVersion) -> nlohmann::json
```

`codeqa::report` es una capa nueva, hermana de `codeqa::rules`/`codeqa::config`:
depende de `model`, `metrics`, `rules`, `config`, `core` — nunca de
`application` (misma disciplina de layering que ya documentó Fase 8). Por
eso `toReportJson` recibe `toolVersion` como `std::string` plano en vez de
`codeqa::application::ApplicationInfo`: la capa de reporte no necesita saber
que ese tipo existe.

`RunAnalysis` reutiliza `AnalyzeProjectRequest` tal cual (no duplica sus
campos):

```cpp
// include/codeqa/application/RunAnalysis.h
namespace codeqa::application
{

struct RunAnalysisRequest
{
    AnalyzeProjectRequest analysis;     // discovery + languageOverrides ya combinados (config ++ CLI)
    codeqa::config::ProjectConfig config; // ya resuelta: defaultConfig() o merge de LoadProjectConfig
};

struct RunAnalysisResult
{
    codeqa::model::ProjectModel model;
    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::rules::ProjectFindingsModel findings;
};

class RunAnalysis
{
public:
    [[nodiscard]] RunAnalysisResult execute(const RunAnalysisRequest& request) const;
};

} // namespace codeqa::application
```

Por qué un orquestador nuevo y no ampliar `AnalyzeProject`: `AnalyzeProject`
ya tiene un contrato probado y acotado ("dame el `ProjectModel`"); ampliarlo
para que también dependa de `config`/`metrics`/`rules` invertiría su
layering actual sin necesidad. `RunAnalysis` no es una abstracción
especulativa: Fase 10 (snapshots) es un segundo consumidor real y ya
documentado en el roadmap, no una necesidad hipotética.

## 4. CLI: comando `analyze`

```
codeqa analyze <path> [--exclude <glob>]... [--lang-override <pattern>=<lang>]...
                       [--config <path>] [--format text|json] [--output <file>]
```

- `--format` por defecto `text`; el único otro valor válido es `json`.
- `--output` por defecto stdout (el `output` que ya recibe `runApplication`);
  si se indica, se abre ese archivo para escritura.
- `--config` opcional; sin él, `RunAnalysisRequest.config = codeqa::config::defaultConfig()`.
- Precedencia de overrides de lenguaje: `languageOverrides` combinado se
  arma como `convert(config.headerOverrides) ++ cliLangOverrides` (config
  primero, CLI al final) — porque `DetectFileLanguage::execute` recorre el
  vector con `rbegin()/rend()` y devuelve el primer match, es decir, el
  **último** elemento del vector que matchea gana. Poner los overrides de
  CLI al final los hace ganar, tal como se decidió en el brainstorming. La
  conversión `HeaderOverrideConfig{pattern, language} → LanguageOverride{pattern, language}`
  es una función local trivial en `RunApplication.cpp` (Fase 8 ya documentó
  que estos dos structs se mantienen deliberadamente separados por capa).

`analyze` necesita su propio bucle de parseo de argumentos (no reutiliza
`parseDiscoverArguments` de `discover`/`parse` tal cual, porque esa función
no reconoce `--config`/`--format`/`--output` y los rechazaría como
argumento inesperado). Duplica las ramas de `--exclude`/`--lang-override`
— es la segunda aparición de ese bloque concreto (la primera ya vive
compartida entre `discover` y `parse`), así que sigue bajo el umbral de
"regla de tres" que el proyecto ya usa para decidir cuándo extraer.

Códigos de error nuevos (mismo estilo que `CLI001`-`CLI007`):

| código | causa |
|---|---|
| `CLI008` | falta el valor de `--config` |
| `CLI009` | falta el valor de `--format` |
| `CLI010` | `--format` tiene un valor que no es `text` ni `json` |
| `CLI011` | falta el valor de `--output` |
| `CLI012` | no se pudo abrir el archivo de `--output` para escritura |

Si `--config` apunta a un archivo inválido, `analyze` no genera un código
`CLIxxx` nuevo: imprime tal cual el `error` que ya devuelve
`LoadProjectConfig::execute` (`"Error [CONFIGxxx]: ..."`) y devuelve
`ExitCode::InvalidArguments` — el mismo texto que ya es correcto, sin
reenvolverlo.

## 5. Reporte de texto

Cubre las 7 líneas del alcance del roadmap (resumen, diagnósticos,
métricas, hallazgos, agrupación por severidad, agrupación por archivo,
resultado final). "Métricas" se pliega en el resumen como un conteo — un
volcado función-por-función en texto plano no sería "legible" (criterio de
aceptación); el detalle completo por función vive en el JSON.

```
Code Quality Analysis Report
=============================

Summary
-------
Root: <rootPath>
Files analyzed: N (C: x, C++: y, unavailable: z)
Functions measured: N
Diagnostics: N (errors: a, warnings: b, information: c)
Findings: N (errors: a, warnings: b)

Findings by severity
---------------------
[ERROR] complexity.cyclomatic  src/foo.cpp:42:5  Foo::bar
    cyclomatic complexity 18 exceeds limit 15
[WARNING] size.function-length  src/baz.cpp:10:1  doThing
    function length 95 exceeds limit 90

Findings by file
-----------------
src/foo.cpp
  [ERROR] complexity.cyclomatic  42:5  Foo::bar - cyclomatic complexity 18 exceeds limit 15
src/baz.cpp
  [WARNING] size.function-length  10:1  doThing - function length 95 exceeds limit 90

Diagnostics
-----------
src/broken.cpp
  line 8: error [PARSE001] unexpected token

Result: 2 findings (1 error, 1 warning) across 2 files.
```

Reglas de omisión: sin findings, se omiten "Findings by severity" y
"Findings by file"; sin diagnósticos, se omite "Diagnostics" — mismo patrón
que `printLanguageListing` ya usa (omitir una sección entera si no hay
datos, no imprimirla vacía). Dentro de "Findings by severity", error antes
que warning; dentro de cada severidad, orden de aparición en el modelo
(estable, sin criterio de ordenamiento adicional). "Findings by file" sigue
el orden de `ProjectFindingsModel.files` (ya en el orden de descubrimiento).

`codeqa::report::renderText(std::ostream&, const ProjectModel&, const ProjectMetricsModel&, const ProjectFindingsModel&)`
— sin `ProjectConfig`: el alcance de texto del roadmap no menciona
configuración, solo el JSON lo pide explícitamente.

## 6. Reporte JSON

Arrays paralelos por archivo — volcado directo de los tres modelos
existentes, sin inventar una estructura anidada por función (decisión del
brainstorming: un join por `symbolName`/rango sería frágil y `Finding` no
tiene una `SymbolIdentity` completa, solo un `symbolName` de texto suelto).

```json
{
  "schemaVersion": 1,
  "metadata": { "rootPath": "...", "toolName": "codeqa", "toolVersion": "..." },
  "config": { "languages": { "headerOverrides": [...] }, "rules": { "size.function-length": {...}, ... } },
  "files": [
    {
      "path": "src/foo.cpp", "language": "cpp", "state": "complete",
      "diagnostics": [ { "code": "PARSE001", "severity": "error", "message": "...", "location": {"line":8,"column":1} } ],
      "dependencies": [ { "target": "...", "kind": "local", "range": {...} } ],
      "namespaces": [ { "name": "...", "qualifiedName": "...", "range": {...} } ],
      "types": [ { "name": "...", "qualifiedName": "...", "kind": "class", "range": {...}, "cppDetails": {...} } ],
      "globalVariables": [ {...} ],
      "typeAliases": [ {...} ],
      "comments": [ { "range": {...} } ],
      "callables": [
        {
          "name": "bar", "qualifiedName": "Foo::bar", "kind": "method", "range": {...},
          "isStatic": false, "isVariadic": false,
          "parameters": [ { "name": "x", "passing": "value", "hasDefaultValue": false, "declaredType": "int" } ],
          "parameterListRange": {...},
          "localVariables": [...], "controlFlow": [...], "calls": [...]
        }
      ],
      "metrics": [
        { "symbol": {"name":"bar","qualifiedName":"Foo::bar","range":{...}}, "size": {...}, "signature": {...}, "complexity": {...} }
      ],
      "findings": [
        {
          "ruleId": "complexity.cyclomatic", "severity": "error",
          "description": "...", "recommendation": "...",
          "observedValue": 18, "limitValue": 15,
          "location": {"line":42,"column":5}, "symbolName": "Foo::bar"
        }
      ]
    }
  ]
}
```

Notas de diseño:

- `"config"` no reinventa serialización: es directamente
  `codeqa::config::toJson(effectiveConfig)`, la función que Fase 8 ya
  construyó para exactamente este propósito ("la configuración efectiva
  puede mostrarse").
- `location`/`range` se serializan como `{"line":..,"column":..}` (y
  `{"begin":{...},"end":{...}}` para rangos) — se omiten `filePath` (ya es
  la clave `"path"` del archivo contenedor) y `offset` (nada lo consume
  hoy), siguiendo el mismo formato que ya usa `CorpusFindingsTest.cpp` para
  comparar hallazgos.
- Lenguaje como string: se extrae un helper compartido
  `codeqa::core::languageToJsonString`/`languageFromJsonString` (`"c"`/`"cpp"`),
  porque esta sería la tercera aparición real de ese mapping (ya duplicado
  entre `RunApplication.cpp` y `ConfigJson.cpp`) — cumple la regla de tres
  que el proyecto ya sigue. Vive en `codeqa::core` porque es lo único que
  `config` y `report` tienen en común por debajo.
- `state` (`AnalysisState`) se serializa como `"complete"`/`"partial"`/`"unavailable"`.
- Determinismo: el orden de `files[]` es el de `ProjectModel.files` (orden
  de descubrimiento, ya estable); dentro de cada archivo, cada colección
  sigue el orden del modelo de origen — no se introduce ningún
  reordenamiento nuevo.

`codeqa::report::toReportJson(const ProjectModel&, const ProjectMetricsModel&, const ProjectFindingsModel&, const ProjectConfig&, const std::string& toolVersion) -> nlohmann::json`.

## 7. Campos variables (golden tests)

El criterio de aceptación del roadmap pide que "los campos variables puedan
excluirse de golden tests". Los únicos dos campos no deterministas de este
esquema son `metadata.rootPath` (depende del path de invocación, p.ej. un
directorio temporal en tests) y `metadata.toolVersion` (cambia entre
releases). Todo lo demás — incluyendo cada `path` dentro de `files[]`, que
ya es relativo a `rootPath` — es determinista para un mismo árbol de
fuentes y una misma config. Los golden tests de Fase 9 comparan el JSON
completo excepto esos dos campos (los ignoran o los sobrescriben con un
valor fijo antes de comparar, como ya hace el resto de la suite con paths
absolutos).

## 8. Manejo de errores y exit codes

- Argumentos de CLI inválidos (incluyendo `CLI008`-`CLI012`) y configuración
  inválida (`CONFIGxxx`, reenviado tal cual) → `ExitCode::InvalidArguments`.
- Excepción no capturada → `ExitCode::InternalError` (manejador top-level
  ya existente, sin cambios).
- `analyze` **nunca** devuelve no-cero por hallazgos, sin importar su
  severidad — decisión explícita del brainstorming; queda fuera de esta
  fase cualquier flag de gating.

## 9. Verificación

- **`RunAnalysis`**: test de integración con un proyecto temporal pequeño
  (patrón `TempProject` ya usado en `ApplyRulesTest.cpp`/`MeasureProjectTest.cpp`)
  que verifica que `model`/`metrics`/`findings` resultantes son consistentes
  entre sí (mismos archivos, mismos símbolos).
- **`renderText`**: casos con modelos armados a mano — sin findings ni
  diagnósticos (secciones omitidas), con findings de ambas severidades
  (orden error-antes-que-warning), con diagnósticos, comparando la salida
  exacta contra un string esperado.
- **`toReportJson`**: casos con modelos armados a mano verificando claves y
  valores exactos (incluyendo `schemaVersion`, `state`, y que `"config"`
  coincide con `codeqa::config::toJson` del mismo `ProjectConfig`); un
  round-trip liviano (`nlohmann::json::parse(toReportJson(...).dump())`
  reproduce la misma estructura).
- **Golden test de corpus**: extiende el patrón de `CorpusFindingsTest.cpp`
  — corre `RunAnalysis` sobre 1-2 fixtures del corpus con findings
  conocidos y compara el JSON resultante contra un fixture esperado,
  ignorando `metadata.rootPath`/`metadata.toolVersion` (ver §7).
- **`languageToJsonString`/`languageFromJsonString`**: casos para `C`,
  `Cpp`, y el caso de error para `Unknown`/texto no reconocido (mismo
  comportamiento que ya prueban `RunApplicationTest.cpp`/`ConfigJsonTest.cpp`
  por separado hoy).
- **CLI (`RunApplicationTest.cpp`)**: `analyze` sin path (`CLI002`,
  reutilizado); `--config`/`--format`/`--output` sin valor (`CLI008`/`CLI009`/`CLI011`);
  `--format` con valor inválido (`CLI010`); `--output` a una ruta no
  escribible (`CLI012`); `--config` a un archivo inválido (mensaje
  `CONFIGxxx` reenviado, `InvalidArguments`); un caso feliz con `--format json --output <file>`
  verificando que el archivo se escribió y contiene JSON válido; un caso
  feliz con `--lang-override` pisando un `headerOverride` del mismo patrón
  en `--config` (precedencia de §4).

## 10. Criterios de aceptación

Mapeados a los del roadmap:

- el informe de texto es legible — §5, secciones se omiten si están vacías,
  sin volcado función-por-función de métricas en texto;
- el JSON es determinista — §6, orden de modelo preservado, sin
  reordenamientos nuevos;
- el esquema incluye versión — `schemaVersion` en `metadata`;
- el informe no depende de Qt — `codeqa::report` depende solo de
  `model`/`metrics`/`rules`/`config`/`core`, igual que el resto del núcleo;
- los campos variables pueden excluirse de golden tests — §7, exactamente
  dos campos (`metadata.rootPath`, `metadata.toolVersion`);
- el JSON puede utilizarse posteriormente por la GUI — §6, modelo completo
  (símbolos + métricas + hallazgos + diagnósticos + config efectiva) por
  archivo, no un subconjunto reducido.

## 11. Dependencias

Fase 8.
