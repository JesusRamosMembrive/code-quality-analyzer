# Fase 10 — Histórico mínimo: diseño

Fecha: 2026-08-12
Estado: revisado (brainstorming 2026-08-12); pendiente de aprobación final

## 1. Objetivo

Que `codeqa analyze` guarde automáticamente un snapshot de cada corrida y lo
compare contra el snapshot anterior más reciente, clasificando cada finding
como nuevo, existente o resuelto — para poder responder "¿este cambio
introdujo problemas nuevos?" sin que la deuda técnica heredada tape la
señal. Más un comando `codeqa snapshot list|show` para listar/consultar los
snapshots guardados.

## 2. Alcance y límites

Esta fase incluye:

- una capa nueva `codeqa::history` (pura, sin I/O) que extrae claves
  comparables de findings y clasifica dos conjuntos de claves en
  nuevo/existente/resuelto;
- tres clases nuevas en `codeqa::application` (`SaveSnapshot`,
  `ListSnapshots`, `LoadSnapshot`) — las únicas piezas que tocan
  `.codeqa/snapshots/` en disco, mismo patrón que `LoadProjectConfig`
  (Fase 8);
- `codeqa analyze <path>` guarda un snapshot y compara contra el anterior
  **automáticamente, sin flag nuevo** — el comando objetivo de Fase 12
  (`codeqa analyze ./project --config codeqa.json --format json --output
  report.json`) no lleva ningún flag de snapshot y ya lista "snapshot
  guardado y comparación" como algo que `analyze` simplemente produce;
- el subcomando nuevo `codeqa snapshot list <path>` / `codeqa snapshot show
  <path> <timestamp>`.

Esta fase **no** incluye, y por qué:

- **Exit code afectado por la comparación.** El quality gate que depende de
  "hallazgos nuevos con severidad ≥ X" es explícitamente Fase 12 en el
  roadmap (que declara Fase 10 como su propia dependencia, no al revés).
  `analyze` sigue sin devolver no-cero por findings esta fase.
- **Lista de *cuáles* findings son nuevos/existentes/resueltos**, más allá
  de los conteos. El "Resultado esperado" del roadmap es una sola línea de
  conteos (`Findings: 3 new, 12 existing, 1 resolved`); nada en esta fase
  consume la lista detallada. Fase 12 sí la va a necesitar (para chequear
  severidad de cada nuevo), pero diseñar esa forma ahora sería adivinar sin
  la brainstorming propia de esa fase — `compareSnapshots` puede extenderse
  después sin romper nada, porque ya calcula las tres particiones
  internamente, solo no las expone todavía.
- **Comparación entre dos snapshots arbitrarios** ni **tendencias
  multi-snapshot** — excluidos explícitamente por el roadmap.
- **Un segundo formato de snapshot.** El archivo guardado es exactamente
  `codeqa::report::toReportJson(...)` — mismo `schemaVersion`, sin campos
  agregados. El resultado de la comparación es efímero (solo de la corrida
  actual) y nunca se persiste dentro del snapshot.
- **`.gitignore` para `.codeqa/`.** No es trabajo de `codeqa` modificar
  archivos del repositorio del usuario; queda a criterio de quien lo use.

## 3. Arquitectura

```
CLI (analyze <path> …)
        │
        ├─ codeqa::application::RunAnalysis::execute(...)      (Fase 9, sin cambios)
        │       -> RunAnalysisResult { model, metrics, findings }
        │
        ├─ reportJson = codeqa::report::toReportJson(...)      (Fase 9, sin cambios;
        │                                                        ahora se calcula siempre,
        │                                                        también con --format text)
        │
        ├─ codeqa::application::ListSnapshots::execute(path) -> vector<timestamp>
        │       │
        │       └─ si hay anterior: codeqa::application::LoadSnapshot::execute(path, ts)
        │               -> optional<json>
        │               │
        │               └─ si parsea: codeqa::history::extractFindingKeys(...) x2
        │                              -> codeqa::history::compareSnapshots(...)
        │                              -> SnapshotComparison { newCount, existingCount, resolvedCount }
        │
        ├─ render (texto o JSON) + línea/campo "comparison" si hubo comparación
        │       -> destino (stdout o --output)
        │
        └─ codeqa::application::SaveSnapshot::execute(path, reportJson)   (best-effort)

CLI (snapshot list|show <path> [timestamp])
        ├─ ListSnapshots::execute(path)   -> list
        └─ LoadSnapshot::execute(path, ts) -> show
```

`codeqa::history` es una capa nueva, hermana de `codeqa::report`: depende
solo de `codeqa::core`, `codeqa::rules` (por `ProjectFindingsModel`/
`Finding`) y `nlohmann::json` — nunca de `codeqa::application`, misma
disciplina que ya documentaron Fase 8 y Fase 9. Solo sabe comparar
*conjuntos de claves*, no sabe qué es un snapshot en disco ni cómo se
guarda.

## 4. `codeqa::history` — comparación pura

Clave de un finding: `(ruleId, symbolName, path relativo del archivo)` —
deliberadamente **sin** línea/columna, para que mover código sin tocar una
función no la haga parecer "resuelta + nueva". `symbolName` en `Finding` ya
guarda `qualifiedName` (namespace/clase incluidos, ver `Rules.cpp`), así que
la clave ya es razonablemente única sin necesitar más campos.

```cpp
// include/codeqa/history/SnapshotComparison.h
#pragma once
#include <cstddef>

namespace codeqa::history
{

struct SnapshotComparison
{
    std::size_t newCount{0};
    std::size_t existingCount{0};
    std::size_t resolvedCount{0};
};

} // namespace codeqa::history
```

```cpp
// include/codeqa/history/CompareSnapshots.h
#pragma once
#include "codeqa/history/SnapshotComparison.h"
#include "codeqa/rules/RuleModels.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <set>
#include <string>
#include <tuple>

namespace codeqa::history
{

/// (ruleId, symbolName, file path relative to rootPath).
using FindingKey = std::tuple<std::string, std::string, std::string>;
using FindingKeySet = std::set<FindingKey>;

/// For the just-computed current run (findings still in memory).
[[nodiscard]] FindingKeySet extractFindingKeys(
    const codeqa::rules::ProjectFindingsModel& findings, const std::filesystem::path& rootPath);

/// For a previous run loaded back from a snapshot file. Paths inside snapshotReport are
/// already relative (toReportJson relativizes them at save time).
[[nodiscard]] FindingKeySet extractFindingKeys(const nlohmann::json& snapshotReport);

[[nodiscard]] SnapshotComparison compareSnapshots(
    const FindingKeySet& current, const FindingKeySet& previous);

} // namespace codeqa::history
```

`compareSnapshots` es un set-diff: presente en ambos → `existingCount`;
solo en `current` → `newCount`; solo en `previous` → `resolvedCount`. No
hace falta un tipo `FindingKey` con nombre propio expuesto más allá del
`using` — es una tupla ordenable de 3 strings, y un `std::set` alcanza para
el único uso que tiene (membership + diff).

## 5. `codeqa::application` — snapshots en disco

Mismo patrón que `LoadProjectConfig` (Fase 8): son las únicas piezas de
esta fase que tocan el filesystem.

```cpp
// include/codeqa/application/SaveSnapshot.h
#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>

namespace codeqa::application
{

class SaveSnapshot
{
public:
    /// Writes reportJson to <rootPath>/.codeqa/snapshots/<timestamp>.json, creating the
    /// directory if needed. Returns the written path, or nullopt if the write failed
    /// (disk full, permissions) -- the caller decides whether/how to warn.
    [[nodiscard]] std::optional<std::filesystem::path> execute(
        const std::filesystem::path& rootPath, const nlohmann::json& reportJson) const;
};

} // namespace codeqa::application
```

```cpp
// include/codeqa/application/ListSnapshots.h
#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace codeqa::application
{

class ListSnapshots
{
public:
    /// Timestamps (filename stems) of every *.json file under
    /// <rootPath>/.codeqa/snapshots/, ascending. Empty if the directory doesn't exist --
    /// not an error (first run has no history yet).
    [[nodiscard]] std::vector<std::string> execute(const std::filesystem::path& rootPath) const;
};

} // namespace codeqa::application
```

```cpp
// include/codeqa/application/LoadSnapshot.h
#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace codeqa::application
{

class LoadSnapshot
{
public:
    /// nullopt if the file doesn't exist OR isn't valid JSON -- both cases are "can't use
    /// this snapshot" for every caller this phase has (analyze's comparison treats them
    /// identically; snapshot show reports them with the same message). Distinguishing
    /// "missing" from "corrupt" isn't needed until a caller actually needs to tell them
    /// apart.
    [[nodiscard]] std::optional<nlohmann::json> execute(
        const std::filesystem::path& rootPath, const std::string& timestamp) const;
};

} // namespace codeqa::application
```

Timestamp: UTC, formato ordenable lexicográficamente y válido como nombre
de archivo en cualquier plataforma — `YYYYMMDDTHHMMSSZ` (ej.
`20260812T153045Z`), vía `std::chrono`/`std::gmtime`. "El anterior más
reciente" es simplemente `ListSnapshots(path).back()` antes de guardar el
nuevo (orden ascendente, ya sorteado).

Límite aceptado, no resuelto: dos `analyze` dentro del mismo segundo
pisarían el mismo archivo. Igual que Fase 9 dejó sin resolver los fallos de
escritura de `--output`, no vale la pena una resolución a milisegundos o un
contador para un caso que no bloquea el uso real de la herramienta.

## 6. CLI: `analyze` guarda y compara automáticamente

Cambios sobre el `runAnalyze` de Fase 9 — sin flags nuevos, sin cambios en
`parseAnalyzeArguments`:

1. Igual que hoy: `RunAnalysis::execute(request) -> result`.
2. `reportJson = toReportJson(result.model, result.metrics, result.findings, config, toolVersion)`
   — se calcula siempre ahora (antes solo pasaba con `--format json`), porque
   hace falta para guardar el snapshot sin importar el formato de salida
   elegido.
3. `timestamps = ListSnapshots{}.execute(path)`. Si no está vacío:
   `previous = LoadSnapshot{}.execute(path, timestamps.back())`.
   - Si `previous` tiene valor: `comparison = compareSnapshots(extractFindingKeys(result.findings, path), extractFindingKeys(*previous))`.
   - Si no (`nullopt`, snapshot corrupto o no legible): warning a
     `errorOutput` (`"Warning: previous snapshot '<ts>' could not be read; skipping comparison.\n"`)
     y se sigue exactamente como si no hubiera anterior.
   - Si `timestamps` está vacío (primera corrida): no hay warning, la
     comparación simplemente se omite — así lo pide el criterio de
     aceptación del roadmap.
4. Render: si `comparison` tiene valor,
   - texto: se agrega una línea final `Findings: N new, M existing, P resolved.`
     después de la línea `Result: ...` que ya emite `renderText`;
   - JSON: se agrega `comparison` como campo top-level **a la copia que se
     imprime**, no a `reportJson` (el que se persiste en el paso 5 sigue
     siendo el `toReportJson` canónico, sin este campo — el snapshot en
     disco nunca lleva su propia comparación embebida):
     ```json
     "comparison": { "previousTimestamp": "20260811T090000Z", "new": 3, "existing": 12, "resolved": 1 }
     ```
     `previousTimestamp` es el que devolvió `ListSnapshots(path).back()` en
     el paso 3 — sin él, quien lea el JSON no tiene forma de saber contra
     qué snapshot se comparó. Las claves `new`/`existing`/`resolved` usan
     las mismas palabras que la línea de texto, no los nombres de campo de
     `SnapshotComparison` (`newCount`/...) — esos son C++ interno, esto es
     el contrato JSON público.
   Si no hubo comparación, no se agrega nada (ni línea ni campo) — mismo
   criterio de "sección vacía se omite" que ya usa `renderText`.
5. `SaveSnapshot{}.execute(path, reportJson)` (el canónico, sin
   `comparison`). Si devuelve `nullopt`: warning a `errorOutput`
   (`"Warning: could not save snapshot.\n"`), pero `analyze` igual termina
   con `ExitCode::Success` — mismo criterio best-effort que Fase 9 ya
   documentó para fallos silenciosos de `--output`.
6. Escribe el reporte renderizado en el destino de siempre (stdout o
   `--output`), sin cambios respecto a Fase 9.

## 7. CLI: comando `snapshot`

```
codeqa snapshot list <path>
codeqa snapshot show <path> <timestamp>
```

- `list`: un timestamp por línea, orden ascendente (el mismo que devuelve
  `ListSnapshots`). Directorio sin snapshots → sin líneas, sin error.
- `show`: `LoadSnapshot{}.execute(path, timestamp)`; si tiene valor, se
  imprime `value.dump(2)`; si no, error.

Códigos de error nuevos, continuando la numeración `CLI001`-`CLI012` ya
existente:

| código | causa |
|---|---|
| `CLI013` | falta el subcomando (`list`/`show`) para `snapshot` |
| `CLI014` | subcomando de `snapshot` desconocido (ni `list` ni `show`) |
| `CLI015` | falta `<timestamp>` para `snapshot show` |
| `CLI016` | el snapshot `<timestamp>` no existe o no se pudo leer para `<path>` |

`<path>` reutiliza tal cual `CLI002` (falta) / `CLI003` (no existe o no es
directorio) — misma condición que ya cubren `discover`/`parse`/`analyze`,
sin duplicar el código de error.

## 8. Manejo de errores y exit codes

- Argumentos de CLI inválidos (`CLI002`, `CLI003`, `CLI013`-`CLI016`) →
  `ExitCode::InvalidArguments`.
- Snapshot anterior corrupto/no legible → warning a stderr, **no** afecta
  el exit code de `analyze` (sigue en `Success` si el resto del análisis
  terminó bien).
- Fallo al guardar el snapshot actual → warning a stderr, tampoco afecta el
  exit code.
- `analyze` sigue sin devolver no-cero por findings — sin cambios respecto
  a Fase 9; el quality gate es Fase 12.

## 9. Verificación

- **`codeqa::history`** (`HistoryTest.cpp`): ambas sobrecargas de
  `extractFindingKeys` con fixtures armados a mano (`ProjectFindingsModel`
  en memoria, y un `nlohmann::json` con la forma de `toReportJson`);
  `compareSnapshots` con casos de conjuntos disjuntos, solapados,
  idénticos, y ambos vacíos (primera corrida hipotética). Todo sin tocar
  disco.
- **`SaveSnapshot`/`ListSnapshots`/`LoadSnapshot`**: directorio temporal
  real (mismo patrón que `LoadProjectConfigTest.cpp`) — guardar y releer un
  snapshot, listar un directorio vacío/inexistente (vector vacío, no
  excepción), leer un timestamp inexistente y un archivo corrupto (ambos
  `nullopt`), verificar que el timestamp generado por `SaveSnapshot`
  aparece en `ListSnapshots` después.
- **Integración `analyze` (`RunApplicationTest.cpp`)**: primera corrida
  sobre un proyecto temporal (sin snapshot previo, sin línea/campo de
  comparación, `.codeqa/snapshots/` queda con un archivo); segunda corrida
  sin cambios en el código (todo "existing"); segunda corrida con un cambio
  real que agrega un finding y resuelve otro (conteos correctos de
  new/existing/resolved en texto y en JSON); corrida con un snapshot previo
  corrupto a mano (warning en stderr, análisis sigue en `Success`).
- **CLI `snapshot` (`RunApplicationTest.cpp`)**: `list`/`show` sin `<path>`
  (`CLI002`); `<path>` inválido (`CLI003`); sin subcomando (`CLI013`);
  subcomando desconocido (`CLI014`); `show` sin `<timestamp>` (`CLI015`);
  `show` con timestamp inexistente (`CLI016`); caso feliz de `list` sobre
  un directorio con 2 snapshots guardados por `analyze`; caso feliz de
  `show` que reproduce el JSON exacto de un snapshot guardado.

## 10. Criterios de aceptación

Mapeados a los del roadmap:

- cada ejecución de `analyze` puede guardar un snapshot — §6 paso 5,
  automático, best-effort;
- un snapshot es un JSON versionado, reutilizando el esquema de Fase 9 —
  §6 paso 2/5, `toReportJson` sin modificar, sin campo `comparison`
  persistido;
- comparar dos snapshots consecutivos clasifica correctamente
  nuevo/existente/resuelto — §4, §9 (caso de integración con cambio real);
- sin snapshot anterior, la comparación se omite sin error — §6 paso 3,
  cubierto también cuando el anterior existe pero está corrupto (mismo
  camino, con warning).

## 11. Dependencias

Fase 9.
