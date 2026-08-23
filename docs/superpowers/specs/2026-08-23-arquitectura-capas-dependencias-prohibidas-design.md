# Arquitectura: capas y dependencias prohibidas — Design

**Date:** 2026-08-23
**Status:** approved for specification review
**Track:** post-MVP, roadmap v2 track #4 (segunda y última vuelta) — see `docs/02-roadmap-v2.md`.

## 1. Goal

Cierra el track #4 añadiendo la pieza que su primera vuelta dejó deliberadamente fuera: capas de arquitectura definidas por el usuario y una lista explícita de pares de capas cuya dependencia está prohibida. Un archivo de una capa `A` que incluye (directamente, vía el `IncludeGraph` ya resuelto) un archivo de una capa `B` con el par `{from: A, to: B}` declarado como prohibido genera una violación reportada en la sección `architecture` del informe — igual que ciclos y fan-out, sin pasar por el motor de `Rule`/`FileRule`, sin supresión por comentario, sin participar en el quality gate.

## 2. Scope and boundaries

### In scope

- **Config:** dos campos nuevos en `codeqa::config::ArchitectureConfig`: `layers` (mapa nombre de capa → lista de patrones glob sobre rutas relativas al proyecto, reutilizando el motor de glob ya existente en `codeqa::infrastructure::matchesExcludePattern`) y `forbiddenDependencies` (lista de pares `{from, to}` que referencian nombres de capa declarados en `layers`). Ambos opcionales — un `codeqa.json` sin ellos se comporta exactamente igual que hoy. Nuevo struct `codeqa::config::LayerDependency{from, to}`.
- **Validación en `ConfigJson.cpp`:** `layers` debe ser un objeto de string→array de strings; `forbiddenDependencies` debe ser un array de objetos `{from, to}`; `from`/`to` deben referenciar una capa ya declarada en `layers` (nuevo `CONFIG014`); `from == to` es inválido (nuevo `CONFIG015`, una capa no puede tener prohibida la dependencia consigo misma). Serialización (`toJson`) simétrica, para que `codeqa config` (guardado desde CLI o desktop) nunca pierda estos campos.
- **Detección (`codeqa::architecture`):** nueva función `computeForbiddenDependencies(graph, rootPath, config)`, junto a `findCycles`/`computeFanInOut` en `IncludeGraph.h`/nuevo `ComputeForbiddenDependencies.cpp`. Reutiliza el `IncludeGraph` ya calculado — no se recalcula el grafo de includes. Para cada archivo, determina su capa (si la tiene) evaluando los patrones de `layers` en **orden alfabético del nombre de capa** (consecuencia directa de que `layers` es un `std::map`, igual que `ProjectConfig::rules` — no se introduce un mecanismo de orden nuevo); el primer patrón que matchea gana. Un archivo que no matchea ningún patrón de ninguna capa queda sin clasificar y nunca participa en el chequeo (ni como origen ni como destino). Para cada arista directa resuelta del grafo, si ambos extremos tienen capa y el par `(capaOrigen, capaDestino)` está en `forbiddenDependencies`, se registra una violación. Solo aristas directas — igual que ciclos y fan-out, no se persigue una cadena transitiva A→B→C.
- **Modelo de resultado:** nuevo struct `codeqa::architecture::ForbiddenDependencyViolation{fromPath, fromLayer, toPath, toLayer}` (sin campo de severidad — es una regla binaria, no un umbral que graduar, a diferencia de `FanOutFinding`). Nuevo campo `ProjectArchitectureModel::forbiddenDependencyViolations`.
- **Reporte:** JSON (`ReportJson.cpp`) añade `architecture.forbiddenDependencyViolations[]`, cada entrada con `from`/`to` como `{path, layer}` (rutas relativizadas contra `rootPath`, igual que el resto de la sección). Texto (`TextReport.cpp`) extiende `writeArchitecture()` con una línea por violación, etiquetada `[error]` (severidad fija en el texto, no en el struct).
- **`MergeConfig.cpp` — verificación explícita, no asumida.** `merged.architecture = user.architecture;` ya hace una sustitución completa del struct (no campo a campo) — el mismo patrón que ya cubre correctamente `qualityGate`/`duplication`. Como `ArchitectureConfig{}` tiene `layers`/`forbiddenDependencies` vacíos por defecto (los valores por defecto correctos: "sin restricción"), este código **no necesita cambios** — pero dado que la primera vuelta de este mismo track tuvo un bug real y silencioso exactamente aquí (`MergeConfig.cpp` no mencionaba `architecture` en absoluto), esta vuelta añade un test explícito que lo confirma en vez de asumirlo.
- **Desktop — verificación explícita, no asumida.** `AppController` guarda/carga `codeqa::config::ArchitectureConfig` como struct completo (`config.architecture = architecture_;` / `architecture_ = config.architecture;`), no campo a campo — así que `layers`/`forbiddenDependencies` sobreviven un ciclo cargar→guardar desde el escritorio aunque la GUI no tenga ningún campo para editarlos. Esta vuelta añade un test que lo confirma, en vez de asumirlo por lectura de código.

### Explicitly out of scope (deferred, not forgotten)

- **Sin superficie en la GUI de escritorio** para editar `layers`/`forbiddenDependencies` — solo `codeqa.json` en esta vuelta, igual que la primera vuelta del track dejó fuera cualquier visualización. Un formulario para definir capas/pares es una extensión de GUI futura, no parte de este slice.
- **Sin supresión por comentario ni participación en el quality gate** — decisión explícita (ver §2 de la conversación de brainstorming): a diferencia de un ciclo, una violación de dependencia prohibida sí tiene una línea natural (el `#include` concreto), pero se mantiene consistente con el precedente ya shippeado de esta misma capa de arquitectura (ciclos/fan-out tampoco participan).
- **Sin inferencia automática de capas** a partir de la estructura de directorios — siempre patrones declarados explícitamente por el usuario.
- **Sin validar que una capa declarada matchee al menos un archivo real** del proyect — una capa que nunca matchea nada simplemente nunca genera violaciones; no es un error de configuración en esta vuelta (posible mejora futura si demuestra ser un problema real).
- **Sin dependencias transitivas** (A→B prohibido no implica perseguir A→C si C llega a B indirectamente) — coherente con que `IncludeGraph` solo modela aristas directas para ciclos/fan-out también.
- **Cualquier cambio a `Rule`/`RuleContext`/`FileRule`/`FileRuleContext`** — esta vuelta no toca el motor de reglas del track #3 en absoluto.

## 3. Architecture

```text
codeqa.json:
  "architecture": {
    "layers": { "core": ["include/codeqa/core/**", "src/core/**"], "desktop": [...] },
    "forbiddenDependencies": [{"from": "core", "to": "desktop"}]
  }
        |
        v
ConfigJson.cpp::parseArchitecture()  -- nueva validación: layers es objeto de
  arrays de strings; forbiddenDependencies referencia capas declaradas;
  from != to. Nuevos códigos CONFIG014/CONFIG015.
        |
        v
codeqa::config::ArchitectureConfig{enabled, fanOutWarning, fanOutHigh,
  layers, forbiddenDependencies}
        |
        v  (ya fluye por AnalyzeArchitecture.h -> AnalyzeArchitecture.cpp,
        |   sin cambios en esa firma)
AnalyzeArchitecture.cpp:
  const IncludeGraph graph = resolveIncludeGraph(project);   // sin cambios
  result.cycles = findCycles(graph);                          // sin cambios
  result.forbiddenDependencyViolations =
      computeForbiddenDependencies(graph, project.rootPath, config);  // NUEVO
  ... fan-out loop sin cambios ...
        |
        v
ComputeForbiddenDependencies.cpp (nuevo, junto a FindCycles.cpp/ComputeFanInOut.cpp):
  layerOf(relativePath, config.layers) -> optional<string>
    (evalúa patrones en orden alfabético de nombre de capa; primer match gana;
     reutiliza codeqa::infrastructure::matchesExcludePattern)
  para cada arista directa (fromFile -> toFile) del grafo:
    si layerOf(from) y layerOf(to) existen y (capaFrom, capaTo) está en
    config.forbiddenDependencies -> ForbiddenDependencyViolation
        |
        v
ProjectArchitectureModel::forbiddenDependencyViolations
        |                                          \
        v                                           v
ReportJson.cpp: architecture.forbiddenDependencyViolations[]
  (from/to como {path relativizado, layer})    TextReport.cpp: línea "[error]
                                                 <from> (<capa>) -> <to> (<capa>)"
                                                 dentro de la sección Architecture
                                                 ya existente
```

## 4. Config model and validation

```cpp
// include/codeqa/config/ConfigModels.h
struct LayerDependency
{
    std::string from;
    std::string to;
};

struct ArchitectureConfig
{
    bool enabled{true};
    long long fanOutWarning{10};
    long long fanOutHigh{20};
    std::map<std::string, std::vector<std::string>> layers;
    std::vector<LayerDependency> forbiddenDependencies;
};
```

```cpp
// src/config/ConfigJson.cpp -- parseArchitecture() extendido
ArchitectureConfig parseArchitecture(const Json& architecture)
{
    requireKeysSubset(
        architecture, {"enabled", "fanOutWarning", "fanOutHigh", "layers", "forbiddenDependencies"},
        "architecture");

    ArchitectureConfig config;

    // ... enabled / fanOutWarning / fanOutHigh sin cambios ...

    if (architecture.contains("layers"))
    {
        const Json& layers = architecture.at("layers");
        requireObject(layers, "architecture.layers");

        for (const auto& [layerName, patternsJson] : layers.items())
        {
            const std::string path = "architecture.layers." + layerName;

            if (!patternsJson.is_array())
            {
                throw ParseFailure("Error [CONFIG005]: '" + path + "' must be an array.");
            }

            std::vector<std::string> patterns;
            for (const Json& patternJson : patternsJson)
            {
                if (!patternJson.is_string())
                {
                    throw ParseFailure("Error [CONFIG005]: '" + path + "' entries must be strings.");
                }
                patterns.push_back(patternJson.get<std::string>());
            }

            config.layers[layerName] = std::move(patterns);
        }
    }

    if (architecture.contains("forbiddenDependencies"))
    {
        const Json& forbidden = architecture.at("forbiddenDependencies");

        if (!forbidden.is_array())
        {
            throw ParseFailure("Error [CONFIG005]: 'architecture.forbiddenDependencies' must be an array.");
        }

        for (std::size_t index = 0; index < forbidden.size(); ++index)
        {
            const std::string entryPath = "architecture.forbiddenDependencies[" + std::to_string(index) + "]";
            const Json& entry = forbidden.at(index);
            requireObject(entry, entryPath);
            requireKeysSubset(entry, {"from", "to"}, entryPath);

            LayerDependency dependency;
            dependency.from = requireString(entry, "from", entryPath);
            dependency.to = requireString(entry, "to", entryPath);

            if (config.layers.find(dependency.from) == config.layers.end())
            {
                throw ParseFailure(
                    "Error [CONFIG014]: '" + entryPath + ".from' references undeclared layer '" + dependency.from
                    + "'.");
            }

            if (config.layers.find(dependency.to) == config.layers.end())
            {
                throw ParseFailure(
                    "Error [CONFIG014]: '" + entryPath + ".to' references undeclared layer '" + dependency.to
                    + "'.");
            }

            if (dependency.from == dependency.to)
            {
                throw ParseFailure(
                    "Error [CONFIG015]: '" + entryPath + "' has the same layer ('" + dependency.from
                    + "') as both 'from' and 'to'.");
            }

            config.forbiddenDependencies.push_back(std::move(dependency));
        }
    }

    // ... fanOutWarning > fanOutHigh check (CONFIG013) sin cambios, al final ...

    return config;
}
```

`layers` se valida y puebla **antes** que `forbiddenDependencies` en la misma función, precisamente porque la validación de `from`/`to` necesita `config.layers` ya construido.

```cpp
// src/config/ConfigJson.cpp -- toJson(), dentro del bloque document["architecture"]
document["architecture"]["layers"] = Json::object();
for (const auto& [layerName, patterns] : config.architecture.layers)
{
    document["architecture"]["layers"][layerName] = patterns;
}

document["architecture"]["forbiddenDependencies"] = Json::array();
for (const codeqa::config::LayerDependency& dependency : config.architecture.forbiddenDependencies)
{
    document["architecture"]["forbiddenDependencies"].push_back(
        {{"from", dependency.from}, {"to", dependency.to}});
}
```

## 5. Detection algorithm

```cpp
// include/codeqa/architecture/ArchitectureModels.h -- nuevo struct
struct ForbiddenDependencyViolation
{
    std::filesystem::path fromPath;
    std::string fromLayer;
    std::filesystem::path toPath;
    std::string toLayer;
};

struct ProjectArchitectureModel
{
    std::vector<FileDependencyMetrics> files;
    std::vector<DependencyCycle> cycles;
    std::vector<FanOutFinding> fanOutFindings;
    std::vector<ForbiddenDependencyViolation> forbiddenDependencyViolations;  // NUEVO
    std::size_t externalOrUnresolvedIncludes{0};
    std::size_t ambiguousIncludes{0};
};
```

```cpp
// include/codeqa/architecture/IncludeGraph.h -- nueva declaración
// (nuevo #include "codeqa/config/ConfigModels.h" -- ver nota de diseño abajo)
[[nodiscard]] std::vector<ForbiddenDependencyViolation> computeForbiddenDependencies(
    const IncludeGraph& graph,
    const std::filesystem::path& rootPath,
    const codeqa::config::ArchitectureConfig& config);
```

**Nota de diseño — desviación deliberada del patrón `findCycles`/`computeFanInOut`:** esas dos funciones no dependen de `codeqa::config` en absoluto (reciben solo el grafo; `AnalyzeArchitecture.cpp` aplica el umbral de fan-out *después*, como una política separada de post-proceso sobre un número ya calculado). `computeForbiddenDependencies` sí recibe `ArchitectureConfig` directamente porque la pertenencia a una capa **es** parte del cálculo en sí (no hay un "número crudo" que calcular primero y filtrar después) — separar esto en un tipo paralelo sin dependencia de `codeqa::config` añadiría una capa de indirección sin beneficio real, y no crea ningún ciclo de dependencias (`codeqa::config` nunca depende de `codeqa::architecture`). `AnalyzeArchitecture.h` ya importa `codeqa::config::ArchitectureConfig` al mismo nivel conceptual, así que esto no introduce una dirección de dependencia nueva en el proyecto — solo la mueve un archivo más abajo, a `IncludeGraph.h`.

```cpp
// src/architecture/ComputeForbiddenDependencies.cpp -- nuevo archivo
#include "codeqa/architecture/IncludeGraph.h"
#include "codeqa/infrastructure/GlobMatcher.h"

#include <optional>

namespace codeqa::architecture
{

namespace
{

std::optional<std::string> layerOf(
    const std::filesystem::path& relativePath, const std::map<std::string, std::vector<std::string>>& layers)
{
    for (const auto& [layerName, patterns] : layers)  // std::map -> orden alfabético de layerName
    {
        for (const std::string& pattern : patterns)
        {
            if (codeqa::infrastructure::matchesExcludePattern(pattern, relativePath))
            {
                return layerName;
            }
        }
    }

    return std::nullopt;
}

bool isForbiddenPair(
    const std::vector<codeqa::config::LayerDependency>& forbiddenDependencies, const std::string& from,
    const std::string& to)
{
    for (const codeqa::config::LayerDependency& pair : forbiddenDependencies)
    {
        if (pair.from == from && pair.to == to)
        {
            return true;
        }
    }

    return false;
}

} // namespace

std::vector<ForbiddenDependencyViolation> computeForbiddenDependencies(
    const IncludeGraph& graph, const std::filesystem::path& rootPath, const codeqa::config::ArchitectureConfig& config)
{
    std::vector<ForbiddenDependencyViolation> violations;

    if (config.layers.empty() || config.forbiddenDependencies.empty())
    {
        return violations;
    }

    std::vector<std::optional<std::string>> layerPerFile;
    layerPerFile.reserve(graph.filePaths.size());
    for (const std::filesystem::path& path : graph.filePaths)
    {
        layerPerFile.push_back(layerOf(std::filesystem::relative(path, rootPath), config.layers));
    }

    for (std::size_t fromIndex = 0; fromIndex < graph.filePaths.size(); ++fromIndex)
    {
        if (!layerPerFile[fromIndex].has_value())
        {
            continue;
        }

        for (const std::size_t toIndex : graph.outgoingEdges[fromIndex])
        {
            if (!layerPerFile[toIndex].has_value())
            {
                continue;
            }

            const std::string& fromLayer = *layerPerFile[fromIndex];
            const std::string& toLayer = *layerPerFile[toIndex];

            if (isForbiddenPair(config.forbiddenDependencies, fromLayer, toLayer))
            {
                violations.push_back(ForbiddenDependencyViolation{
                    graph.filePaths[fromIndex], fromLayer, graph.filePaths[toIndex], toLayer});
            }
        }
    }

    return violations;
}

} // namespace codeqa::architecture
```

```cpp
// src/architecture/AnalyzeArchitecture.cpp -- una línea nueva, junto a cycles
result.cycles = findCycles(graph);
result.forbiddenDependencyViolations = computeForbiddenDependencies(graph, project.rootPath, config);
```

`CMakeLists.txt` gana una línea nueva en la lista explícita de fuentes de `codeqa_core` (junto a `ResolveIncludeGraph.cpp`/`FindCycles.cpp`/`ComputeFanInOut.cpp`/`AnalyzeArchitecture.cpp`): `src/architecture/ComputeForbiddenDependencies.cpp`.

El resultado sale ya en un orden determinista (orden de `project.files`, luego orden de `outgoingEdges[fromIndex]`) sin necesidad de un `sort` explícito — mismo criterio que el resto de esta capa.

## 6. Report bridging

```cpp
// src/report/ReportJson.cpp -- nueva función, junto a las otras *Json() de esta capa
Json forbiddenDependencyJson(
    const codeqa::architecture::ForbiddenDependencyViolation& violation, const std::filesystem::path& rootPath)
{
    return Json{
        {"from",
         Json{
             {"path", std::filesystem::relative(violation.fromPath, rootPath).generic_string()},
             {"layer", violation.fromLayer}}},
        {"to",
         Json{
             {"path", std::filesystem::relative(violation.toPath, rootPath).generic_string()},
             {"layer", violation.toLayer}}}};
}
```

```cpp
// src/report/ReportJson.cpp -- dentro de architectureJson(), junto al array de cycles
Json forbiddenDependencyViolations = Json::array();
for (const codeqa::architecture::ForbiddenDependencyViolation& violation : architecture.forbiddenDependencyViolations)
{
    forbiddenDependencyViolations.push_back(forbiddenDependencyJson(violation, rootPath));
}
// ... y se añade al documento devuelto junto a "files"/"cycles"/"fanOutFindings":
// document["forbiddenDependencyViolations"] = forbiddenDependencyViolations;
```

```cpp
// src/report/TextReport.cpp -- writeArchitecture() extendido
void writeArchitecture(
    std::ostream& output, const codeqa::architecture::ProjectArchitectureModel& architecture,
    const std::filesystem::path& rootPath)
{
    if (architecture.cycles.empty() && architecture.fanOutFindings.empty()
        && architecture.forbiddenDependencyViolations.empty())
    {
        return;
    }

    output << "\nArchitecture\n------------\n";

    // ... cycles loop sin cambios ...
    // ... fanOutFindings loop sin cambios ...

    for (const codeqa::architecture::ForbiddenDependencyViolation& violation :
         architecture.forbiddenDependencyViolations)
    {
        output << "  [" << severityTag(codeqa::core::DiagnosticSeverity::Error) << "] "
               << relativeDisplayPath(violation.fromPath, rootPath) << " (" << violation.fromLayer << ") -> "
               << relativeDisplayPath(violation.toPath, rootPath) << " (" << violation.toLayer << ")\n";
    }
}
```

Reutiliza el `severityTag()` ya existente en este archivo (el mismo que renderiza `fanOutFindings`) en vez de un literal `"error"` — la severidad de una violación de dependencia prohibida es conceptualmente fija (`DiagnosticSeverity::Error`), pero se renderiza con la misma función que cualquier otra severidad, no con una cadena mágica aparte.

## 7. Testing and verification strategy

- `tests/unit/ConfigJsonTest.cpp`: capas + pares válidos parseados correctamente; `layers` no-objeto rechazado (CONFIG005); una entrada de patrones no-array rechazada (CONFIG005); `forbiddenDependencies` no-array rechazado (CONFIG005); `from`/`to` no declarados como capa rechazados (CONFIG014, un caso por cada uno); `from == to` rechazado (CONFIG015); round-trip `parseConfig` → `toJson` → `parseConfig` produce el mismo `ArchitectureConfig` (incluyendo `layers`/`forbiddenDependencies`).
- Nuevo `tests/unit/ComputeForbiddenDependenciesTest.cpp` (mismo patrón que `ComputeFanInOutTest.cpp`/`FindCyclesTest.cpp`: `IncludeGraph` construido a mano, sin pasar por el parser real):
  - Una arista que cruza un par prohibido produce exactamente una violación con los campos correctos.
  - Una arista entre capas no listadas en `forbiddenDependencies` no produce violación.
  - Un archivo que no matchea ningún patrón de ninguna capa nunca aparece como origen ni destino de una violación.
  - Dos patrones de capas distintas que matchean el mismo archivo: gana la capa cuyo nombre es alfabéticamente primero (verifica el criterio de desempate documentado en §5).
  - `layers` vacío o `forbiddenDependencies` vacío (config por defecto, sin configuración de capas) produce cero violaciones sin recorrer el grafo — no hay coste ni comportamiento sorpresa para un `codeqa.json` que no usa esta función.
  - Un par prohibido declarado en una sola dirección (`{from: A, to: B}`) no prohíbe implícitamente la dirección contraria — una arista `B -> A` en el grafo no produce violación a menos que `{from: B, to: A}` esté declarado por separado. Confirma que `isForbiddenPair` compara el par exacto, no un par no ordenado.
- `tests/unit/AnalyzeArchitectureTest.cpp`: caso end-to-end confirmando que `analyzeArchitecture` con una `ArchitectureConfig` real (capas + par prohibido) puebla `forbiddenDependencyViolations`; caso confirmando que `config.enabled = false` sigue devolviendo un `ProjectArchitectureModel{}` vacío (incluyendo este campo nuevo) sin evaluar nada.
- `tests/unit/ReportJsonTest.cpp`: extender el caso de arquitectura existente con una violación y verificar la forma `{from: {path, layer}, to: {path, layer}}`.
- `tests/unit/TextReportTest.cpp`: extender el caso de arquitectura existente con una violación y verificar la línea `[error] ... -> ...` en el texto.
- `tests/unit/MergeConfigTest.cpp`: **nuevo caso, regresión directa del bug real de la primera vuelta de este track** — un `user.architecture` con `layers`/`forbiddenDependencies` poblados sobrevive `mergeConfig(defaults, user)` intactos.
- `tests/desktop/AppControllerTest.cpp`: **nuevo caso** — un `codeqa.json` con `layers`/`forbiddenDependencies` poblados, cargado vía `selectProject`, y luego guardado (sin tocar ningún campo de arquitectura desde la GUI), produce un archivo de salida donde `layers`/`forbiddenDependencies` siguen presentes e intactos.
- `tests/unit/CorpusReportTest.cpp`: verificar (`grep` de `"layers"`/`forbiddenDependencies` en `tests/corpus/`) que ninguna fixture existente define esta sección — no se espera ningún cambio de conteo, pero se confirma en vez de asumirse, siguiendo la disciplina ya establecida en las vueltas anteriores de reglas.

## 8. Acceptance criteria

- Un `codeqa.json` sin `layers`/`forbiddenDependencies` se comporta exactamente igual que antes de esta vuelta (cero violaciones, cero coste añadido más allá de dos comprobaciones de "está vacío").
- Un archivo de la capa `A` que incluye directamente un archivo de la capa `B`, con `{from: A, to: B}` declarado, produce una `ForbiddenDependencyViolation` visible tanto en el JSON (`architecture.forbiddenDependencyViolations`) como en el texto del informe.
- Una configuración con `from`/`to` no declarados como capa, o con `from == to`, se rechaza en la carga de config con un mensaje de error claro (`CONFIG014`/`CONFIG015`), nunca falla en tiempo de análisis.
- `mergeConfig` y el ciclo cargar/guardar del `AppController` de escritorio preservan `layers`/`forbiddenDependencies` sin pérdidas, verificado con un test explícito en cada caso (no asumido por lectura de código, dado el precedente real de bug en esta misma área).
- Ningún cambio a `Rule`/`RuleContext`/`FileRule`/`FileRuleContext`, a los 12 rule ids existentes, ni a `findCycles`/`computeFanInOut`'s comportamiento ya shippeado.
- Suite completa de tests (unit + desktop) sigue pasando.
