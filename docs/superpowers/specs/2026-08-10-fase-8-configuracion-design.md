# Fase 8 — Configuración: diseño

Fecha: 2026-08-10
Estado: revisado (brainstorming 2026-08-10); pendiente de aprobación final

## 1. Objetivo

Permitir que un `codeqa.json` opcional ajuste, por rule-id, si una regla está
activa y sus umbrales de warning/high, y que un patrón de override de
lenguaje pueda expresar "todo lo que cuelga de este directorio" — sin tocar
ninguna superficie de CLI todavía, siguiendo el mismo patrón que Fase 6b y
Fase 7 (librería primero, wiring a CLI cuando exista un comando real que lo
consuma, que es Fase 9).

## 2. Alcance y límites

Esta fase incluye:

- una capa nueva `codeqa::config` con el modelo de configuración, la config
  por defecto embebida, el merge por rule-id, y el parseo/serialización JSON;
- un orquestador nuevo `codeqa::application::LoadProjectConfig` que lee
  `codeqa.json` de disco, parsea, valida y hace merge contra los valores por
  defecto — el único punto de esta fase que toca disco, igual que
  `MeasureProject`/`ApplyRules`;
- el cambio necesario en `codeqa::rules::Rules.cpp` para que los 8 umbrales
  dejen de ser `static constexpr` y se puedan construir a partir de un
  `ProjectConfig`, incluyendo que una regla deshabilitada no se instancie;
- soporte de `**` (globstar) en `codeqa::infrastructure::GlobMatcher`, para
  que `"include/c_api/**"` (el ejemplo del propio roadmap) y el hallazgo
  diferido de Fase 3 (`vendor/**` como override de lenguaje) sean
  expresables.

Esta fase **no** incluye, y por qué:

- **Ninguna superficie CLI nueva.** `--config` no se conecta a `discover` ni
  a `parse`, y no se añade un subcomando `codeqa config`. Decisión explícita
  del usuario en el brainstorming de esta fase (2026-08-10), coherente con
  el precedente de Fase 6b/7: cada fase se queda en su capa, y `--config`
  llega con `codeqa analyze` en Fase 9.
- **`excludePatterns` vía `codeqa.json`.** El Alcance del roadmap lo
  menciona como término suelto ("rutas excluidas"), pero el único ejemplo
  JSON concreto que da el roadmap para Fase 8 no incluye ese campo — solo
  `languages.headerOverrides` y `rules`. Sin CLI que lo consuma y sin forma
  concreta que implementar, se deja fuera; se añade el día que algo lo
  necesite (probablemente Fase 9, cuando `analyze --config` exista).
- **"Listas permitidas" y "patrones ignorados".** Mismos términos sueltos
  del Alcance del roadmap, sin ejemplo ni consumidor. La supresión inline
  por comentario (Fase 7) y las exclusiones por `--exclude` (Fase 2) ya
  cubren los dos mecanismos de "ignorar algo" que el proyecto tiene hoy;
  no se inventa un tercero sin un caso concreto.
- **"Formato de salida".** Pertenece conceptualmente a Fase 9 (reporting),
  que todavía no existe — no hay nada que un campo de config pudiera
  seleccionar todavía.
- **Un campo `severity` independiente por regla.** Las 8 reglas actuales
  siempre derivan la severidad de qué umbral cruzan (`Warning` en
  `warning`, `Error` en `high`) — eso ya es "configurable" a través de
  `warningLimit`/`highLimit`. El ejemplo del roadmap muestra `severity`
  solo para una regla hipotética (`missing-override`) sin `params`, que no
  existe en el código; no se generaliza el esquema sin un segundo caso real
  (mismo criterio que usó Fase 7 para no crear `ThresholdRule`/`PatternRule`).
- **Relleno parcial de parámetros.** El roadmap dice explícitamente "no hay
  merge profundo dentro de los parámetros de una regla individual" — así
  que si un rule-id aparece en `codeqa.json`, su entrada debe venir completa
  (`enabled` + `params.warning` + `params.high`); no hay una versión
  "hereda `high` del default pero cambia `warning`".

## 3. Arquitectura

```
codeqa.json (texto)
        │
        ▼
LoadProjectConfig::execute(path)      -- codeqa::application, toca disco
        │  lee con codeqa::infrastructure::readFileContents
        ▼
codeqa::config::parseConfig(text) -> ParseConfigResult
        │  nlohmann::json::parse + validación manual de claves conocidas
        ▼
codeqa::config::mergeConfig(defaultConfig(), parsed) -> ProjectConfig
        │
        ▼
ProjectConfig completo (8 rule-ids siempre presentes)
        │
        ▼
codeqa::rules::allRules(config) -> vector<unique_ptr<Rule>>
        │  una regla con enabled=false no se instancia
        ▼
codeqa::application::ApplyRules::execute(project, metrics, config)
```

Dependencias entre capas (todas hacia abajo, ninguna nueva inversión):

```
cli
  ↓
application (AnalyzeProject, MeasureProject, ApplyRules, LoadProjectConfig)
  ↓
rules  ←  config  (Rules.cpp consume codeqa::config::ProjectConfig)
  ↓        ↓
metrics  core
  ↓
model
  ↓
frontend → infrastructure (GlobMatcher, readFileContents)
  ↓
core
```

`codeqa::config` depende solo de `codeqa::core` (para `Language`) — no de
`codeqa::application`, para no invertir el sentido de la dependencia. Por
eso `HeaderOverrideConfig{pattern, language}` es un tipo propio de
`codeqa::config`, distinto de
`codeqa::application::LanguageOverride{pattern, language}` que ya existe
para `--lang-override`: son estructuralmente idénticos pero vivir en capas
distintas — duplicar un struct de dos campos es más barato que invertir el
layering. Quien conecte `codeqa.json` a la CLI (Fase 9) hace la conversión
trivial entre ambos.

## 4. Modelo público

```cpp
// include/codeqa/config/ConfigModels.h
namespace codeqa::config
{

struct RuleConfig
{
    bool enabled{true};
    long long warningLimit{0};
    long long highLimit{0};
};

struct HeaderOverrideConfig
{
    std::string pattern;
    codeqa::core::Language language{codeqa::core::Language::Unknown};
};

struct ProjectConfig
{
    std::vector<HeaderOverrideConfig> headerOverrides;
    std::map<std::string, RuleConfig> rules;
};

} // namespace codeqa::config
```

```cpp
// include/codeqa/config/DefaultConfig.h
namespace codeqa::config
{
[[nodiscard]] ProjectConfig defaultConfig();
} // namespace codeqa::config
```

`defaultConfig()` es la única fuente de verdad de los 8 pares
warning/high ya shippeados en Fase 7 (`size.function-length` 50/90,
`size.local-variables` 8/12, `signature.parameter-count` 4/7,
`signature.consecutive-booleans` 2/3, `complexity.cyclomatic` 10/15,
`complexity.condition` 3/5, `complexity.nesting` 3/5,
`complexity.returns` 4/6), todas `enabled=true`. Los `static constexpr`
que hoy viven en cada clase de `Rules.cpp` se eliminan — quedarían como una
segunda fuente de verdad de los mismos números, que es exactamente lo que
esta fase existe para evitar.

```cpp
// include/codeqa/config/MergeConfig.h
namespace codeqa::config
{
[[nodiscard]] ProjectConfig mergeConfig(const ProjectConfig& defaults, const ProjectConfig& user);
} // namespace codeqa::config
```

Semántica (función pura, sin I/O, para que los tests puedan construir
`ProjectConfig` aisladas en memoria):

- `result.rules` empieza como copia de `defaults.rules`; por cada
  `[id, cfg]` en `user.rules`, `result.rules[id] = cfg` (reemplazo total de
  esa entrada). `user.rules` solo contiene, por construcción vía
  `parseConfig`, los ids que el usuario mencionó explícitamente — nunca una
  entrada parcial.
- `result.headerOverrides = user.headerOverrides` — reemplazo directo, sin
  merge por clave (no hay concepto de "clave" en una lista). No existe hoy
  ningún override por defecto que perder: `defaultConfig().headerOverrides`
  siempre está vacío.

```cpp
// include/codeqa/config/ConfigJson.h
namespace codeqa::config
{

struct ParseConfigResult
{
    std::optional<ProjectConfig> config;  // solo lo presente en el JSON: subconjunto de rules, headerOverrides tal cual
    std::optional<std::string> error;     // "Error [CONFIGxxx]: <detalle con ruta json>"
};

[[nodiscard]] ParseConfigResult parseConfig(const std::string& jsonText);
[[nodiscard]] nlohmann::json toJson(const ProjectConfig& config);

} // namespace codeqa::config
```

```cpp
// include/codeqa/application/LoadProjectConfig.h
namespace codeqa::application
{

struct LoadProjectConfigResult
{
    std::optional<codeqa::config::ProjectConfig> config;  // ya mergeado con los defaults; nullopt solo si error
    std::optional<std::string> error;
};

class LoadProjectConfig
{
public:
    [[nodiscard]] LoadProjectConfigResult execute(const std::filesystem::path& path) const;
};

} // namespace codeqa::application
```

`LoadProjectConfig::execute`: lee el archivo con
`codeqa::infrastructure::readFileContents` (CONFIG007 si falla o no
existe) → `config::parseConfig` (propaga su error si falla) →
`config::mergeConfig(config::defaultConfig(), *parsed.config)` → resultado.

## 5. Parseo y validación JSON

`parseConfig` no usa las macros `NLOHMANN_DEFINE_TYPE_*`: esas ignoran en
silencio cualquier clave que el struct de destino no declare, lo cual viola
directamente el criterio de aceptación del roadmap "los valores desconocidos
no se ignoran silenciosamente". En su lugar, recorre el árbol JSON a mano
con `j.items()` y rechaza cualquier clave fuera del esquema conocido en
cada nivel.

Esquema aceptado (todo lo demás es error):

```text
{
  "languages"?: {
    "headerOverrides"?: [
      { "pattern": string, "language": "c" | "cpp" }, ...
    ]
  },
  "rules"?: {
    "<uno de los 8 rule-ids de Fase 7>": {
      "enabled": bool,
      "params": { "warning": integer, "high": integer }
    },
    ...
  }
}
```

Códigos de error (mismo estilo `PREFIJONNN` que `PARSE001`/`PARSE002`
(Fase 4) y `DISC001` (Fase 2), pero como cadena de texto formateada — igual
que `RunApplication.cpp` ya hace con `CLI001`-`CLI007` — no como
`core::Diagnostic`, porque no hay una ubicación de código fuente que
señalar, solo una ruta dentro del propio JSON):

| código | causa |
|---|---|
| `CONFIG001` | el texto no es JSON válido (`nlohmann::json::parse` lanza `parse_error`) |
| `CONFIG002` | una clave desconocida en cualquier nivel del objeto (el mensaje incluye la ruta, p.ej. `rules.complexity.cyclomatic.limit`) |
| `CONFIG003` | una clave bajo `rules` que no es uno de los 8 rule-ids conocidos |
| `CONFIG004` | falta un campo requerido (p.ej. `rules.<id>` sin `params`, o `params` sin `high`) |
| `CONFIG005` | un campo tiene el tipo incorrecto (p.ej. `enabled` no booleano, `warning` no entero) |
| `CONFIG006` | `language` no es `"c"` ni `"cpp"` |
| `CONFIG007` | el archivo de configuración no existe o no se puede leer |
| `CONFIG008` | `params.warning > params.high` para una regla |

`CONFIG008` existe porque `evaluateThreshold` (Fase 7) asume
`warning <= high`: si se invierte, los valores entre `high` y `warning` no
disparan ningún `Finding` (fallan el corte `< warningLimit`) y todo lo que
sí dispara es directamente `Error`, nunca `Warning` — un rango de
severidad silenciosamente roto, no un valor simplemente "creativo". Un
`warningLimit` igual a `highLimit`, negativo, o cero sí son válidos (p.ej.
`warning=0` para forzar que la regla dispare siempre es una configuración
deliberada razonable) — solo se rechaza el orden invertido.

La validación falla en el primer error encontrado (fail-fast, recorrido en
orden de aparición en el JSON) — es "un error claro" tal como pide el
criterio de aceptación; no hace falta acumular todos los errores posibles
en una sola pasada para satisfacerlo, y acumular añadiría una estructura de
resultado más compleja sin un requisito que la pida.

`toJson(const ProjectConfig&)` es la serialización inversa (construida a
mano con el mismo esquema, no macros) — da soporte directo al criterio "la
configuración efectiva puede mostrarse" sin necesitar ninguna superficie
CLI en esta fase: cualquier consumidor futuro (Fase 9) puede volcar
`toJson(mergedConfig).dump(4)`.

## 6. Wiring a `codeqa::rules` y `ApplyRules`

`Rule` y `RuleContext` (`include/codeqa/rules/Rule.h`) **no cambian** — la
interfaz que Fase 7 dejó deliberadamente estrecha sigue igual. El cambio
es en cómo se construyen las instancias:

- cada una de las 8 clases en `src/rules/Rules.cpp` gana un constructor
  `(long long warningLimit, long long highLimit)` que rellena lo que hoy
  son miembros `static constexpr`; `evaluate()` pasa a leer
  `warningLimit_`/`highLimit_` (miembros de instancia) en vez de la
  constante de clase — el resto de cada `evaluate()` no cambia una línea;
- `allRules()` gana un parámetro:

```cpp
// include/codeqa/rules/Rules.h
[[nodiscard]] std::vector<std::unique_ptr<Rule>> allRules(const codeqa::config::ProjectConfig& config);
```

  Internamente, por cada uno de los 8 rule-ids conocidos: si
  `config.rules.at(id).enabled` es `false`, no se añade esa instancia al
  vector devuelto — así "una regla puede desactivarse" (criterio del
  roadmap) pasa de ser una decisión de código (Fase 7) a una decisión de
  usuario final. `allRules` confía en el invariante de que
  `mergeConfig`/`defaultConfig` siempre dejan las 8 claves presentes en
  `config.rules` — `.at()` lanzaría `std::out_of_range` solo si ese
  invariante se rompe, lo cual sería un bug de esta misma fase, no una
  entrada de usuario inválida (esa ya se validó en `parseConfig`).

- `ApplyRules::execute` gana el mismo parámetro y se lo reenvía a
  `allRules(config)`; nada más en `ApplyRules.cpp` cambia (el filtro de
  confianza ante errores de parseo y la supresión inline son ajenos a la
  configuración).

```cpp
// include/codeqa/application/ApplyRules.h
[[nodiscard]] codeqa::rules::ProjectFindingsModel execute(
    const codeqa::model::ProjectModel& project,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::config::ProjectConfig& config) const;
```

Los tests existentes que llaman a `ApplyRules{}.execute(project, metrics)`
(`ApplyRulesTest.cpp`, y cualquier corpus test que lo invoque) pasan a
llamar con `codeqa::config::defaultConfig()` como tercer argumento — el
comportamiento observable no cambia para ninguno de ellos.

## 7. `GlobMatcher`: soporte para `**`

`translateGlobToRegex` (`src/infrastructure/GlobMatcher.cpp`) hoy traduce
carácter a carácter; `*` se convierte siempre en `[^/]*` (no cruza `/`).
Dos asteriscos consecutivos hoy producen `[^/]*[^/]*`, que es equivalente a
un único `[^/]*` — es decir, `**` no significa nada distinto de `*` hoy.

El cambio: al recorrer el patrón, dos o más `*` consecutivos se tratan como
un único token "globstar" y se traducen a `.*` (cruza `/`, incluyendo
cadena vacía). Un único `*` sigue traduciéndose a `[^/]*` como hoy. Esto
requiere cambiar el bucle de `for (char : pattern)` a un recorrido por
índice para poder mirar el carácter siguiente y consumir la racha completa
de asteriscos de una vez.

No se toca `matchWholePath` (decide si comparar contra la ruta relativa
completa o solo contra el nombre de archivo, según si el patrón contiene
`/`): un patrón como `"include/c_api/**"` ya contiene `/` en otra parte,
así que activa correctamente la comparación contra la ruta completa sin
ningún cambio adicional. Tampoco se toca `DiscoverSourceFiles.cpp` (poda de
directorios durante el recorrido): el hallazgo diferido de Fase 3 era
específicamente que `DetectFileLanguage` compara contra la ruta completa
del archivo (nunca contra una entrada de directorio) — con `**` cruzando
`/`, `"vendor/**"` ya matchea `"vendor/foo/bar.cpp"` directamente, sin
necesitar poda.

Casos nuevos para `tests/unit/GlobMatcherTest.cpp`: `"vendor/**"` matchea
`"vendor/foo.cpp"` y `"vendor/nested/foo.cpp"` pero no `"other/foo.cpp"`;
`"include/c_api/**"` (el ejemplo literal del roadmap) matchea
`"include/c_api/socket.h"`; un `*` simple sigue sin cruzar `/` (regresión
de los casos ya existentes).

## 8. Verificación

- **`mergeConfig`**: aislado, sin disco — casos: rule-id mencionado
  reemplaza por completo su entrada; rule-id no mencionado conserva el
  default; `headerOverrides` del usuario reemplaza al default (vacío).
- **`parseConfig`**: un caso válido con `languages` y `rules` completos; un
  caso válido con solo `rules` (sin `languages`); JSON malformado
  (`CONFIG001`); clave desconocida en cada uno de los tres niveles
  (top-level, dentro de un override, dentro de una entrada de regla)
  (`CONFIG002`); rule-id desconocido (`CONFIG003`); campo requerido
  ausente, en al menos dos posiciones distintas (`CONFIG004`); tipo
  incorrecto para `enabled`, `warning` y `language`
  (`CONFIG005`/`CONFIG006`); `warning > high` para una regla (`CONFIG008`).
- **`toJson`**: round-trip — `parseConfig(toJson(defaultConfig()).dump())`
  reproduce los mismos valores (con las 8 entradas completas, ya que
  `toJson` serializa el `ProjectConfig` ya mergeado, no un subconjunto).
- **`LoadProjectConfig`**: siguiendo el patrón `TempProject` ya usado en
  `ApplyRulesTest.cpp`/`MeasureProjectTest.cpp` — escribe un `codeqa.json`
  real a un directorio temporal y verifica el resultado end-to-end,
  incluyendo archivo inexistente (`CONFIG007`).
- **`Rules.cpp`/`RulesTest.cpp`/`RuleTest.cpp`**: cada una de las 8 reglas
  se prueba también con un umbral rebajado vía config (un caso que antes no
  disparaba warning ahora sí, con el mismo `RuleContext`); `allRules`
  probado con una regla deshabilitada (no aparece en el vector devuelto) y
  con un umbral no-default aplicado correctamente al `Finding` resultante.
- **`ApplyRulesTest.cpp`**: se actualizan las llamadas existentes con
  `codeqa::config::defaultConfig()`; se añade un caso end-to-end con una
  regla deshabilitada por config (ya no genera `Finding` aunque el código
  cruce su umbral por defecto) y otro con un umbral bajado por config (una
  función que con los defaults no generaría hallazgo, con config sí).
- **`GlobMatcherTest.cpp`**: casos de §7.

## 9. Criterios de aceptación

Mapeados directamente a los del roadmap para esta fase:

- existe una configuración por defecto embebida en el binario —
  `defaultConfig()`;
- una regla no mencionada en `codeqa.json` conserva su valor por defecto —
  `mergeConfig`;
- una regla mencionada en `codeqa.json` reemplaza completamente su entrada
  por defecto — `mergeConfig` + `parseConfig` (sin relleno parcial);
- sin `--config`, no se busca ningún archivo de configuración en el
  sistema de archivos — no aplica todavía en esta fase (no hay `--config`
  en ninguna CLI); se satisface trivialmente y se revalida en Fase 9
  cuando exista el flag;
- una configuración inválida genera errores claros — códigos `CONFIG001`-
  `CONFIG007` con mensaje descriptivo y ruta dentro del JSON;
- los valores desconocidos no se ignoran silenciosamente — validación
  manual de claves conocidas en cada nivel, sin macros de nlohmann;
- la configuración efectiva puede mostrarse — `toJson(ProjectConfig)`;
- los tests pueden proporcionar configuraciones aisladas —
  `mergeConfig`/`ProjectConfig` son construibles en memoria, sin I/O.

## 10. Dependencias

Fase 7.
