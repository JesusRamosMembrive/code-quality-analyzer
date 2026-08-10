# Fase 8 — Configuración Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Un `codeqa.json` opcional puede activar/desactivar cada una de las 8 reglas de Fase 7 y ajustar sus umbrales warning/high, con merge por rule-id sobre una configuración por defecto embebida, validación estricta (ningún valor desconocido se ignora en silencio), y soporte `**` en `GlobMatcher` para expresar overrides de lenguaje recursivos — todo sin tocar ninguna superficie de CLI (eso es Fase 9).

**Architecture:** Capa nueva `codeqa::config` (modelo `ProjectConfig`, `defaultConfig()`, `mergeConfig()`, `parseConfig()`/`toJson()` sobre nlohmann::json con validación manual de claves conocidas) + orquestador nuevo `codeqa::application::LoadProjectConfig` (el único punto que toca disco). `codeqa::rules::allRules()` gana un parámetro `ProjectConfig`: cada regla pasa de `static constexpr` a construcción por parámetro, y una regla `enabled=false` simplemente no se instancia. `ApplyRules::execute` reenvía el mismo `ProjectConfig`. Cambio aparte y sin relación: `GlobMatcher` gana soporte de `**` (globstar).

**Tech Stack:** C++17, CMake, doctest, nlohmann::json (ya vendorizado en `third_party/nlohmann/json.hpp`, hasta ahora sin consumidor real).

## Global Constraints

- Ninguna superficie CLI nueva en esta fase — `--config` no se conecta a `discover`/`parse`, ni se añade un subcomando nuevo. Decisión explícita del brainstorming de esta fase (2026-08-10); llega con `codeqa analyze` en Fase 9.
- `Rule` y `RuleContext` (`include/codeqa/rules/Rule.h`) no cambian de firma — la interfaz estrecha que fijó Fase 7 se mantiene intacta.
- Los 8 rule-ids (`size.function-length`, `size.local-variables`, `signature.parameter-count`, `signature.consecutive-booleans`, `complexity.cyclomatic`, `complexity.returns`, `complexity.condition`, `complexity.nesting`) son literales de texto conocidos que aparecen, deliberadamente duplicados, en tres sitios: `id()` de cada clase en `src/rules/Rules.cpp` (Fase 7, sin tocar), la lista de claves de `defaultConfig()` (Task 2) y la lista de ids permitidos de `parseConfig` (Task 5). No existe un registro dinámico de reglas — introducir uno para evitar esta triplicación de 8 strings sería la abstracción anticipada que este proyecto ya ha evitado en decisiones equivalentes (ver Fase 7 §4, decisión de no crear `ThresholdRule`/`PatternRule`).
- Merge por regla sin relleno parcial: si un rule-id aparece en `codeqa.json`, su entrada debe traer `enabled` + `params.warning` + `params.high` completos — no hay merge profundo dentro de una regla individual (decisión ya fijada por el roadmap, Fase 8 §"Decisiones de diseño").
- `codeqa::config` depende solo de `codeqa::core` (para `Language`) — nunca de `codeqa::application` ni de `codeqa::rules`, para no invertir el sentido de las dependencias (`cli → application → {rules, config, ...} → infrastructure → core`).
- Todos los errores de validación de config son cadenas de texto con un código `CONFIGxxx` embebido (mismo estilo que `CLI001`-`CLI007` en `RunApplication.cpp`), no `codeqa::core::Diagnostic` — no hay una ubicación de código fuente que señalar, solo una ruta dentro del propio JSON.
- La validación de `parseConfig` falla en el primer error encontrado (fail-fast) — no se acumulan varios errores en una sola pasada.
- `warningLimit > highLimit` para cualquier regla es un error de validación (`CONFIG008`), no un valor "raro pero legal": `evaluateThreshold` (Fase 7) asume ese orden, e invertirlo produce un rango de severidad roto en silencio.

## File Structure

- `include/codeqa/config/ConfigModels.h` (nuevo): `RuleConfig`, `HeaderOverrideConfig`, `ProjectConfig` — sin lógica.
- `include/codeqa/config/DefaultConfig.h` + `src/config/DefaultConfig.cpp` (nuevo): `defaultConfig()`.
- `include/codeqa/config/MergeConfig.h` + `src/config/MergeConfig.cpp` (nuevo): `mergeConfig()`, pura, sin I/O.
- `include/codeqa/config/ConfigJson.h` + `src/config/ConfigJson.cpp` (nuevo): `parseConfig()` (validación manual, sin macros de nlohmann) y `toJson()`.
- `include/codeqa/application/LoadProjectConfig.h` + `src/application/LoadProjectConfig.cpp` (nuevo): único punto que toca disco, lee + parsea + mergea.
- `src/infrastructure/GlobMatcher.cpp` (modificado): soporte `**`.
- `include/codeqa/rules/Rules.h`, `src/rules/Rules.cpp` (modificados): construcción por parámetro, `allRules(config)`.
- `include/codeqa/application/ApplyRules.h`, `src/application/ApplyRules.cpp` (modificados): `execute` gana el parámetro `config`.
- `tests/unit/{ConfigModels,DefaultConfig,MergeConfig,ConfigJson,LoadProjectConfig}Test.cpp` (nuevos); `tests/unit/GlobMatcherTest.cpp`, `tests/unit/RulesTest.cpp`, `tests/unit/ApplyRulesTest.cpp`, `tests/unit/CorpusFindingsTest.cpp` (modificados).
- `CMakeLists.txt`, `tests/CMakeLists.txt` (modificados en varios tasks).

---

### Task 1: `codeqa::config` model types

**Files:**
- Create: `include/codeqa/config/ConfigModels.h`
- Create: `tests/unit/ConfigModelsTest.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `codeqa::config::RuleConfig{bool enabled; long long warningLimit; long long highLimit;}`, `codeqa::config::HeaderOverrideConfig{std::string pattern; codeqa::core::Language language;}`, `codeqa::config::ProjectConfig{std::vector<HeaderOverrideConfig> headerOverrides; std::map<std::string, RuleConfig> rules;}` — usados por todas las tareas siguientes.

- [ ] **Step 1: Write the failing test**

Create `tests/unit/ConfigModelsTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/config/ConfigModels.h"

TEST_CASE("RuleConfig defaults to enabled with zeroed thresholds")
{
    codeqa::config::RuleConfig config;
    CHECK(config.enabled);
    CHECK(config.warningLimit == 0);
    CHECK(config.highLimit == 0);
}

TEST_CASE("HeaderOverrideConfig defaults to an empty pattern and Unknown language")
{
    codeqa::config::HeaderOverrideConfig override;
    CHECK(override.pattern.empty());
    CHECK(override.language == codeqa::core::Language::Unknown);
}

TEST_CASE("ProjectConfig defaults to empty overrides and rules")
{
    codeqa::config::ProjectConfig config;
    CHECK(config.headerOverrides.empty());
    CHECK(config.rules.empty());
}

TEST_CASE("RuleConfig is aggregate-initializable in id order")
{
    const codeqa::config::RuleConfig config{false, 2, 4};
    CHECK_FALSE(config.enabled);
    CHECK(config.warningLimit == 2);
    CHECK(config.highLimit == 4);
}
```

Add `unit/ConfigModelsTest.cpp` to `tests/CMakeLists.txt`, inside the `add_executable(codeqa_tests ...)` list, right after `unit/CorpusFindingsTest.cpp` (the last entry).

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails — `codeqa/config/ConfigModels.h` does not exist.

- [ ] **Step 3: Create the header**

Create `include/codeqa/config/ConfigModels.h`:

```cpp
#pragma once

#include "codeqa/core/Language.h"

#include <map>
#include <string>
#include <vector>

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

- [ ] **Step 4: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass, including the 4 new ones.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/config/ConfigModels.h tests/unit/ConfigModelsTest.cpp tests/CMakeLists.txt
git commit -m "feat: add codeqa::config model types"
```

---

### Task 2: `defaultConfig()`

**Files:**
- Create: `include/codeqa/config/DefaultConfig.h`
- Create: `src/config/DefaultConfig.cpp`
- Create: `tests/unit/DefaultConfigTest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `codeqa::config::ProjectConfig`, `codeqa::config::RuleConfig` (Task 1).
- Produces: `codeqa::config::defaultConfig() -> ProjectConfig` — las 8 rule-ids de Fase 7 con sus umbrales shippeados, todas `enabled=true`, `headerOverrides` vacío. Usado por prácticamente todas las tareas siguientes.

- [ ] **Step 1: Write the failing test**

Create `tests/unit/DefaultConfigTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/config/DefaultConfig.h"

#include <string>

namespace
{

void checkRule(
    const codeqa::config::ProjectConfig& config, const std::string& id, long long warning, long long high)
{
    REQUIRE(config.rules.count(id) == 1);
    const codeqa::config::RuleConfig& rule = config.rules.at(id);
    CHECK(rule.enabled);
    CHECK(rule.warningLimit == warning);
    CHECK(rule.highLimit == high);
}

} // namespace

TEST_CASE("defaultConfig ships the eight Fase 7 thresholds, all enabled, no header overrides")
{
    const codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();

    CHECK(config.headerOverrides.empty());
    REQUIRE(config.rules.size() == 8);

    checkRule(config, "size.function-length", 50, 90);
    checkRule(config, "size.local-variables", 8, 12);
    checkRule(config, "signature.parameter-count", 4, 7);
    checkRule(config, "signature.consecutive-booleans", 2, 3);
    checkRule(config, "complexity.cyclomatic", 10, 15);
    checkRule(config, "complexity.returns", 4, 6);
    checkRule(config, "complexity.condition", 3, 5);
    checkRule(config, "complexity.nesting", 3, 5);
}
```

Add `unit/DefaultConfigTest.cpp` to `tests/CMakeLists.txt`, right after `unit/ConfigModelsTest.cpp`.

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails — `codeqa/config/DefaultConfig.h` does not exist.

- [ ] **Step 3: Create the header and implementation**

Create `include/codeqa/config/DefaultConfig.h`:

```cpp
#pragma once

#include "codeqa/config/ConfigModels.h"

namespace codeqa::config
{

[[nodiscard]] ProjectConfig defaultConfig();

} // namespace codeqa::config
```

Create `src/config/DefaultConfig.cpp`:

```cpp
#include "codeqa/config/DefaultConfig.h"

namespace codeqa::config
{

namespace
{

RuleConfig thresholds(long long warningLimit, long long highLimit)
{
    RuleConfig config;
    config.enabled = true;
    config.warningLimit = warningLimit;
    config.highLimit = highLimit;
    return config;
}

} // namespace

ProjectConfig defaultConfig()
{
    ProjectConfig config;
    config.rules["size.function-length"] = thresholds(50, 90);
    config.rules["size.local-variables"] = thresholds(8, 12);
    config.rules["signature.parameter-count"] = thresholds(4, 7);
    config.rules["signature.consecutive-booleans"] = thresholds(2, 3);
    config.rules["complexity.cyclomatic"] = thresholds(10, 15);
    config.rules["complexity.returns"] = thresholds(4, 6);
    config.rules["complexity.condition"] = thresholds(3, 5);
    config.rules["complexity.nesting"] = thresholds(3, 5);
    return config;
}

} // namespace codeqa::config
```

In `CMakeLists.txt`, add `src/config/DefaultConfig.cpp` to the `add_library(codeqa_core STATIC ...)` list, right after `src/rules/Rules.cpp` (the last entry).

- [ ] **Step 4: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass, including the new one.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/config/DefaultConfig.h src/config/DefaultConfig.cpp tests/unit/DefaultConfigTest.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add defaultConfig with the eight Fase 7 thresholds"
```

---

### Task 3: `mergeConfig()`

**Files:**
- Create: `include/codeqa/config/MergeConfig.h`
- Create: `src/config/MergeConfig.cpp`
- Create: `tests/unit/MergeConfigTest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `codeqa::config::ProjectConfig` (Task 1).
- Produces: `codeqa::config::mergeConfig(const ProjectConfig& defaults, const ProjectConfig& user) -> ProjectConfig` — función pura, sin I/O. Usada por `LoadProjectConfig` (Task 6).

- [ ] **Step 1: Write the failing test**

Create `tests/unit/MergeConfigTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/config/MergeConfig.h"

TEST_CASE("mergeConfig replaces a rule mentioned by the user and keeps the rest at their default")
{
    codeqa::config::ProjectConfig defaults;
    defaults.rules["complexity.cyclomatic"] = codeqa::config::RuleConfig{true, 10, 15};
    defaults.rules["size.function-length"] = codeqa::config::RuleConfig{true, 50, 90};

    codeqa::config::ProjectConfig user;
    user.rules["complexity.cyclomatic"] = codeqa::config::RuleConfig{false, 2, 4};

    const codeqa::config::ProjectConfig merged = codeqa::config::mergeConfig(defaults, user);

    REQUIRE(merged.rules.count("complexity.cyclomatic") == 1);
    CHECK_FALSE(merged.rules.at("complexity.cyclomatic").enabled);
    CHECK(merged.rules.at("complexity.cyclomatic").warningLimit == 2);
    CHECK(merged.rules.at("complexity.cyclomatic").highLimit == 4);

    REQUIRE(merged.rules.count("size.function-length") == 1);
    CHECK(merged.rules.at("size.function-length").enabled);
    CHECK(merged.rules.at("size.function-length").warningLimit == 50);
}

TEST_CASE("mergeConfig replaces headerOverrides wholesale with the user's list")
{
    codeqa::config::ProjectConfig defaults;
    defaults.headerOverrides.push_back(codeqa::config::HeaderOverrideConfig{"a/**", codeqa::core::Language::C});

    codeqa::config::ProjectConfig user;
    user.headerOverrides.push_back(codeqa::config::HeaderOverrideConfig{"b/**", codeqa::core::Language::Cpp});

    const codeqa::config::ProjectConfig merged = codeqa::config::mergeConfig(defaults, user);

    REQUIRE(merged.headerOverrides.size() == 1);
    CHECK(merged.headerOverrides[0].pattern == "b/**");
    CHECK(merged.headerOverrides[0].language == codeqa::core::Language::Cpp);
}

TEST_CASE("mergeConfig with an empty user config reproduces the defaults exactly")
{
    codeqa::config::ProjectConfig defaults;
    defaults.rules["complexity.cyclomatic"] = codeqa::config::RuleConfig{true, 10, 15};
    defaults.headerOverrides.push_back(codeqa::config::HeaderOverrideConfig{"a/**", codeqa::core::Language::C});

    const codeqa::config::ProjectConfig merged =
        codeqa::config::mergeConfig(defaults, codeqa::config::ProjectConfig{});

    REQUIRE(merged.rules.count("complexity.cyclomatic") == 1);
    CHECK(merged.rules.at("complexity.cyclomatic").warningLimit == 10);
    CHECK(merged.headerOverrides.empty());
}
```

Add `unit/MergeConfigTest.cpp` to `tests/CMakeLists.txt`, right after `unit/DefaultConfigTest.cpp`.

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails — `codeqa/config/MergeConfig.h` does not exist.

- [ ] **Step 3: Create the header and implementation**

Create `include/codeqa/config/MergeConfig.h`:

```cpp
#pragma once

#include "codeqa/config/ConfigModels.h"

namespace codeqa::config
{

[[nodiscard]] ProjectConfig mergeConfig(const ProjectConfig& defaults, const ProjectConfig& user);

} // namespace codeqa::config
```

Create `src/config/MergeConfig.cpp`:

```cpp
#include "codeqa/config/MergeConfig.h"

namespace codeqa::config
{

ProjectConfig mergeConfig(const ProjectConfig& defaults, const ProjectConfig& user)
{
    ProjectConfig merged = defaults;

    for (const auto& [id, ruleConfig] : user.rules)
    {
        merged.rules[id] = ruleConfig;
    }

    merged.headerOverrides = user.headerOverrides;

    return merged;
}

} // namespace codeqa::config
```

In `CMakeLists.txt`, add `src/config/MergeConfig.cpp` to `add_library(codeqa_core STATIC ...)`, right after `src/config/DefaultConfig.cpp`.

- [ ] **Step 4: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass, including the 3 new ones.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/config/MergeConfig.h src/config/MergeConfig.cpp tests/unit/MergeConfigTest.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add mergeConfig for rule-id-scoped config merging"
```

---

### Task 4: `**` (globstar) support in `GlobMatcher`

**Files:**
- Modify: `src/infrastructure/GlobMatcher.cpp`
- Modify: `tests/unit/GlobMatcherTest.cpp`

**Interfaces:**
- Consumes/Produces: no new public interface — `codeqa::infrastructure::matchesExcludePattern` (existing signature, unchanged) gains the ability to match `**` across `/`.

- [ ] **Step 1: Write the failing test**

In `tests/unit/GlobMatcherTest.cpp`, append these `TEST_CASE`s at the end of the file (after the existing "pathological pattern" test case):

```cpp
TEST_CASE("a double-asterisk pattern crosses directory separators")
{
    CHECK(matchesExcludePattern("vendor/**", "vendor/foo.cpp"));
    CHECK(matchesExcludePattern("vendor/**", "vendor/nested/foo.cpp"));
    CHECK_FALSE(matchesExcludePattern("vendor/**", "other/foo.cpp"));
}

TEST_CASE("a double-asterisk pattern matches the roadmap's own Fase 8 example shape")
{
    CHECK(matchesExcludePattern("include/c_api/**", "include/c_api/socket.h"));
    CHECK(matchesExcludePattern("include/c_api/**", "include/c_api/nested/socket.h"));
    CHECK_FALSE(matchesExcludePattern("include/c_api/**", "include/other/socket.h"));
}

TEST_CASE("a single asterisk still does not cross a directory separator next to a double-asterisk pattern")
{
    CHECK_FALSE(matchesExcludePattern("vendor/*", "vendor/nested/foo.cpp"));
    CHECK(matchesExcludePattern("vendor/*", "vendor/foo.cpp"));
}
```

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: builds fine, but the 3 new test cases fail (`vendor/**` does not match `vendor/nested/foo.cpp` today — `**` currently behaves exactly like a single `*`, which does not cross `/`).

- [ ] **Step 3: Implement globstar support**

In `src/infrastructure/GlobMatcher.cpp`, replace the `translateGlobToRegex` function:

```cpp
std::string translateGlobToRegex(const std::string& pattern)
{
    static const std::string kRegexSpecialChars = R"(\.^$|()[]{}+)";

    std::string regexPattern;
    regexPattern.reserve(pattern.size() * 2);

    std::size_t index = 0;

    while (index < pattern.size())
    {
        const char character = pattern[index];

        if (character == '*')
        {
            std::size_t runLength = 0;

            while (index < pattern.size() && pattern[index] == '*')
            {
                ++runLength;
                ++index;
            }

            regexPattern += (runLength >= 2) ? ".*" : "[^/]*";
            continue;
        }

        if (character == '?')
        {
            regexPattern += "[^/]";
        }
        else if (kRegexSpecialChars.find(character) != std::string::npos)
        {
            regexPattern += '\\';
            regexPattern += character;
        }
        else
        {
            regexPattern += character;
        }

        ++index;
    }

    return regexPattern;
}
```

This is the only change in the file: two or more consecutive `*` are now collapsed into a single `.*` token (crosses `/`, matches the empty string too); a lone `*` still becomes `[^/]*` exactly as before. Everything else in `GlobMatcher.cpp` (`normalizePattern`, `matchesExcludePattern`, the `matchWholePath` decision) is untouched.

- [ ] **Step 4: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass, including the 3 new ones and every pre-existing `GlobMatcherTest.cpp` case (in particular the pathological-pattern regression test — a repeated `*x` run must still terminate cleanly).

- [ ] **Step 5: Commit**

```bash
git add src/infrastructure/GlobMatcher.cpp tests/unit/GlobMatcherTest.cpp
git commit -m "feat: add ** (globstar) support to GlobMatcher"
```

---

### Task 5: `parseConfig()` and `toJson()`

**Files:**
- Create: `include/codeqa/config/ConfigJson.h`
- Create: `src/config/ConfigJson.cpp`
- Create: `tests/unit/ConfigJsonTest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `codeqa::config::ProjectConfig`, `RuleConfig`, `HeaderOverrideConfig` (Task 1); `codeqa::config::defaultConfig()` (Task 2, only for the round-trip test).
- Produces: `codeqa::config::ParseConfigResult{std::optional<ProjectConfig> config; std::optional<std::string> error;}`, `codeqa::config::parseConfig(const std::string& jsonText) -> ParseConfigResult`, `codeqa::config::toJson(const ProjectConfig& config) -> nlohmann::json`. Used by `LoadProjectConfig` (Task 6).

This task also fixes `nlohmann_json`'s include directory: it is currently set to `third_party/nlohmann` (the folder containing `json.hpp` itself), so `#include <nlohmann/json.hpp>` cannot resolve through that target — `tests/CMakeLists.txt` currently works around this with its own separate `target_include_directories(codeqa_tests PRIVATE ${PROJECT_SOURCE_DIR}/third_party)`. Every other vendored library in this project (e.g. `tree_sitter`, include dir `third_party/tree-sitter/lib/include`, consumed as `#include <tree_sitter/api.h>`) points its include dir at the *parent* of the library's own folder. This task aligns `nlohmann_json` with that convention so `codeqa_core` can consume it too.

- [ ] **Step 1: Write the failing test**

Create `tests/unit/ConfigJsonTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/config/ConfigJson.h"
#include "codeqa/config/DefaultConfig.h"

#include <string>

TEST_CASE("parseConfig accepts a document with languages and rules")
{
    const std::string text = R"({
        "languages": { "headerOverrides": [ { "pattern": "include/c_api/**", "language": "c" } ] },
        "rules": {
            "complexity.cyclomatic": { "enabled": false, "params": { "warning": 8, "high": 12 } }
        }
    })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.config.has_value());
    CHECK_FALSE(result.error.has_value());
    REQUIRE(result.config->headerOverrides.size() == 1);
    CHECK(result.config->headerOverrides[0].pattern == "include/c_api/**");
    CHECK(result.config->headerOverrides[0].language == codeqa::core::Language::C);
    REQUIRE(result.config->rules.count("complexity.cyclomatic") == 1);
    CHECK_FALSE(result.config->rules.at("complexity.cyclomatic").enabled);
    CHECK(result.config->rules.at("complexity.cyclomatic").warningLimit == 8);
    CHECK(result.config->rules.at("complexity.cyclomatic").highLimit == 12);
}

TEST_CASE("parseConfig accepts a document with only rules")
{
    const std::string text = R"({
        "rules": { "size.function-length": { "enabled": true, "params": { "warning": 40, "high": 80 } } }
    })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.config.has_value());
    CHECK(result.config->headerOverrides.empty());
    REQUIRE(result.config->rules.size() == 1);
}

TEST_CASE("parseConfig accepts an empty document")
{
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig("{}");

    REQUIRE(result.config.has_value());
    CHECK(result.config->headerOverrides.empty());
    CHECK(result.config->rules.empty());
}

TEST_CASE("parseConfig reports CONFIG001 for malformed JSON")
{
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig("{ not json");

    CHECK_FALSE(result.config.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG001") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG002 for an unknown top-level key")
{
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(R"({ "outputFormat": "json" })");

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG002") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG002 for an unknown key inside a header override")
{
    const std::string text =
        R"({ "languages": { "headerOverrides": [ { "pattern": "a/**", "language": "c", "note": "x" } ] } })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG002") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG002 for an unknown key inside a rule entry")
{
    const std::string text = R"({
        "rules": {
            "complexity.cyclomatic": {
                "enabled": true, "params": { "warning": 8, "high": 12 }, "severity": "error"
            }
        }
    })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG002") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG003 for an unknown rule id")
{
    const std::string text =
        R"({ "rules": { "missing-override": { "enabled": true, "params": { "warning": 1, "high": 2 } } } })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG003") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG004 for a rule entry missing params")
{
    const std::string text = R"({ "rules": { "complexity.cyclomatic": { "enabled": true } } })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG004") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG004 for params missing high")
{
    const std::string text =
        R"({ "rules": { "complexity.cyclomatic": { "enabled": true, "params": { "warning": 8 } } } })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG004") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG005 for a non-boolean enabled field")
{
    const std::string text = R"({
        "rules": { "complexity.cyclomatic": { "enabled": "yes", "params": { "warning": 8, "high": 12 } } }
    })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG005") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG006 for an invalid language value")
{
    const std::string text =
        R"({ "languages": { "headerOverrides": [ { "pattern": "a/**", "language": "rust" } ] } })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG006") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG008 when warning is greater than high")
{
    const std::string text =
        R"({ "rules": { "complexity.cyclomatic": { "enabled": true, "params": { "warning": 20, "high": 10 } } } })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG008") != std::string::npos);
}

TEST_CASE("toJson round-trips the default configuration through parseConfig")
{
    const codeqa::config::ProjectConfig original = codeqa::config::defaultConfig();
    const std::string serialized = codeqa::config::toJson(original).dump();

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(serialized);

    REQUIRE(result.config.has_value());
    CHECK(result.config->rules.size() == original.rules.size());

    for (const auto& [id, ruleConfig] : original.rules)
    {
        REQUIRE(result.config->rules.count(id) == 1);
        CHECK(result.config->rules.at(id).enabled == ruleConfig.enabled);
        CHECK(result.config->rules.at(id).warningLimit == ruleConfig.warningLimit);
        CHECK(result.config->rules.at(id).highLimit == ruleConfig.highLimit);
    }
}
```

Add `unit/ConfigJsonTest.cpp` to `tests/CMakeLists.txt`, right after `unit/MergeConfigTest.cpp`.

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails — `codeqa/config/ConfigJson.h` does not exist.

- [ ] **Step 3: Fix the nlohmann_json include directory and link it into codeqa_core**

In `CMakeLists.txt`, change:

```cmake
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE
    ${PROJECT_SOURCE_DIR}/third_party/nlohmann
)
```

to:

```cmake
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE
    ${PROJECT_SOURCE_DIR}/third_party
)
```

And change:

```cmake
target_link_libraries(codeqa_core PUBLIC tree_sitter tree_sitter_c tree_sitter_cpp)
```

to:

```cmake
target_link_libraries(codeqa_core PUBLIC tree_sitter tree_sitter_c tree_sitter_cpp nlohmann_json)
```

`nlohmann_json` is `PUBLIC` because `ConfigJson.h`'s `toJson` returns `nlohmann::json` — a type that leaks into `codeqa_core`'s public API, so consumers (including `codeqa_tests`, which already separately adds `third_party` as an include directory — that line in `tests/CMakeLists.txt` can stay, it is now redundant but harmless) need it too.

- [ ] **Step 4: Create the header and implementation**

Create `include/codeqa/config/ConfigJson.h`:

```cpp
#pragma once

#include "codeqa/config/ConfigModels.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace codeqa::config
{

struct ParseConfigResult
{
    std::optional<ProjectConfig> config;
    std::optional<std::string> error;
};

[[nodiscard]] ParseConfigResult parseConfig(const std::string& jsonText);
[[nodiscard]] nlohmann::json toJson(const ProjectConfig& config);

} // namespace codeqa::config
```

Create `src/config/ConfigJson.cpp`:

```cpp
#include "codeqa/config/ConfigJson.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace codeqa::config
{

namespace
{

using Json = nlohmann::json;

struct ParseFailure : std::runtime_error
{
    explicit ParseFailure(const std::string& message) : std::runtime_error(message) {}
};

const std::vector<std::string>& knownRuleIds()
{
    static const std::vector<std::string> ids = {
        "size.function-length",
        "size.local-variables",
        "signature.parameter-count",
        "signature.consecutive-booleans",
        "complexity.cyclomatic",
        "complexity.returns",
        "complexity.condition",
        "complexity.nesting"};
    return ids;
}

void requireObject(const Json& value, const std::string& path)
{
    if (!value.is_object())
    {
        throw ParseFailure("Error [CONFIG005]: '" + path + "' must be an object.");
    }
}

void requireKeysSubset(const Json& object, const std::vector<std::string>& allowed, const std::string& path)
{
    for (const auto& item : object.items())
    {
        if (std::find(allowed.begin(), allowed.end(), item.key()) == allowed.end())
        {
            throw ParseFailure("Error [CONFIG002]: unknown key '" + path + "." + item.key() + "'.");
        }
    }
}

std::string requireString(const Json& object, const std::string& field, const std::string& path)
{
    if (!object.contains(field))
    {
        throw ParseFailure("Error [CONFIG004]: '" + path + "' is missing required field '" + field + "'.");
    }
    if (!object.at(field).is_string())
    {
        throw ParseFailure("Error [CONFIG005]: '" + path + "." + field + "' must be a string.");
    }
    return object.at(field).get<std::string>();
}

bool requireBool(const Json& object, const std::string& field, const std::string& path)
{
    if (!object.contains(field))
    {
        throw ParseFailure("Error [CONFIG004]: '" + path + "' is missing required field '" + field + "'.");
    }
    if (!object.at(field).is_boolean())
    {
        throw ParseFailure("Error [CONFIG005]: '" + path + "." + field + "' must be a boolean.");
    }
    return object.at(field).get<bool>();
}

long long requireInteger(const Json& object, const std::string& field, const std::string& path)
{
    if (!object.contains(field))
    {
        throw ParseFailure("Error [CONFIG004]: '" + path + "' is missing required field '" + field + "'.");
    }
    if (!object.at(field).is_number_integer())
    {
        throw ParseFailure("Error [CONFIG005]: '" + path + "." + field + "' must be an integer.");
    }
    return object.at(field).get<long long>();
}

codeqa::core::Language parseLanguage(const std::string& text, const std::string& path)
{
    if (text == "c")
    {
        return codeqa::core::Language::C;
    }
    if (text == "cpp")
    {
        return codeqa::core::Language::Cpp;
    }
    throw ParseFailure("Error [CONFIG006]: '" + path + "' must be 'c' or 'cpp', got '" + text + "'.");
}

std::vector<HeaderOverrideConfig> parseHeaderOverrides(const Json& languages)
{
    requireKeysSubset(languages, {"headerOverrides"}, "languages");

    std::vector<HeaderOverrideConfig> overrides;

    if (!languages.contains("headerOverrides"))
    {
        return overrides;
    }

    const Json& list = languages.at("headerOverrides");

    if (!list.is_array())
    {
        throw ParseFailure("Error [CONFIG005]: 'languages.headerOverrides' must be an array.");
    }

    for (std::size_t index = 0; index < list.size(); ++index)
    {
        const std::string entryPath = "languages.headerOverrides[" + std::to_string(index) + "]";
        const Json& entry = list.at(index);
        requireObject(entry, entryPath);
        requireKeysSubset(entry, {"pattern", "language"}, entryPath);

        HeaderOverrideConfig parsedOverride;
        parsedOverride.pattern = requireString(entry, "pattern", entryPath);
        parsedOverride.language =
            parseLanguage(requireString(entry, "language", entryPath), entryPath + ".language");
        overrides.push_back(std::move(parsedOverride));
    }

    return overrides;
}

std::map<std::string, RuleConfig> parseRules(const Json& rules)
{
    std::map<std::string, RuleConfig> parsed;

    for (const auto& item : rules.items())
    {
        const std::string& id = item.key();

        if (std::find(knownRuleIds().begin(), knownRuleIds().end(), id) == knownRuleIds().end())
        {
            throw ParseFailure("Error [CONFIG003]: unknown rule id 'rules." + id + "'.");
        }

        const std::string path = "rules." + id;
        const Json& entry = item.value();
        requireObject(entry, path);
        requireKeysSubset(entry, {"enabled", "params"}, path);

        RuleConfig config;
        config.enabled = requireBool(entry, "enabled", path);

        if (!entry.contains("params"))
        {
            throw ParseFailure("Error [CONFIG004]: '" + path + "' is missing required field 'params'.");
        }

        const Json& params = entry.at("params");
        requireObject(params, path + ".params");
        requireKeysSubset(params, {"warning", "high"}, path + ".params");
        config.warningLimit = requireInteger(params, "warning", path + ".params");
        config.highLimit = requireInteger(params, "high", path + ".params");

        if (config.warningLimit > config.highLimit)
        {
            throw ParseFailure(
                "Error [CONFIG008]: '" + path + ".params' has warning (" + std::to_string(config.warningLimit)
                + ") greater than high (" + std::to_string(config.highLimit) + ").");
        }

        parsed[id] = config;
    }

    return parsed;
}

} // namespace

ParseConfigResult parseConfig(const std::string& jsonText)
{
    Json document;

    try
    {
        document = Json::parse(jsonText);
    }
    catch (const Json::parse_error& error)
    {
        return ParseConfigResult{std::nullopt, std::string("Error [CONFIG001]: invalid JSON: ") + error.what()};
    }

    try
    {
        requireObject(document, "config");
        requireKeysSubset(document, {"languages", "rules"}, "config");

        ProjectConfig config;

        if (document.contains("languages"))
        {
            const Json& languages = document.at("languages");
            requireObject(languages, "languages");
            config.headerOverrides = parseHeaderOverrides(languages);
        }

        if (document.contains("rules"))
        {
            const Json& rules = document.at("rules");
            requireObject(rules, "rules");
            config.rules = parseRules(rules);
        }

        return ParseConfigResult{config, std::nullopt};
    }
    catch (const ParseFailure& failure)
    {
        return ParseConfigResult{std::nullopt, std::string(failure.what())};
    }
    catch (const Json::exception& error)
    {
        // Safety net: any nlohmann-internal exception this validation code did not
        // anticipate (e.g. a numeric value too large for long long) still turns into
        // a ParseConfigResult error instead of propagating out of parseConfig.
        return ParseConfigResult{std::nullopt, std::string("Error [CONFIG005]: malformed value: ") + error.what()};
    }
}

nlohmann::json toJson(const ProjectConfig& config)
{
    Json document;
    document["languages"]["headerOverrides"] = Json::array();

    for (const HeaderOverrideConfig& headerOverride : config.headerOverrides)
    {
        document["languages"]["headerOverrides"].push_back(
            {{"pattern", headerOverride.pattern},
             {"language", headerOverride.language == codeqa::core::Language::C ? "c" : "cpp"}});
    }

    document["rules"] = Json::object();

    for (const auto& [id, ruleConfig] : config.rules)
    {
        document["rules"][id] = {
            {"enabled", ruleConfig.enabled},
            {"params", {{"warning", ruleConfig.warningLimit}, {"high", ruleConfig.highLimit}}}};
    }

    return document;
}

} // namespace codeqa::config
```

In `CMakeLists.txt`, add `src/config/ConfigJson.cpp` to `add_library(codeqa_core STATIC ...)`, right after `src/config/MergeConfig.cpp`.

- [ ] **Step 5: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass, including the 13 new ones.

- [ ] **Step 6: Commit**

```bash
git add include/codeqa/config/ConfigJson.h src/config/ConfigJson.cpp tests/unit/ConfigJsonTest.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add strict JSON parsing and serialization for ProjectConfig"
```

---

### Task 6: `LoadProjectConfig`

**Files:**
- Create: `include/codeqa/application/LoadProjectConfig.h`
- Create: `src/application/LoadProjectConfig.cpp`
- Create: `tests/unit/LoadProjectConfigTest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `codeqa::config::ProjectConfig` (Task 1), `codeqa::config::defaultConfig()` (Task 2), `codeqa::config::mergeConfig()` (Task 3), `codeqa::config::parseConfig()` (Task 5), `codeqa::infrastructure::readFileContents(const std::filesystem::path&) -> std::optional<std::string>` (Fase 7, unchanged).
- Produces: `codeqa::application::LoadProjectConfigResult{std::optional<codeqa::config::ProjectConfig> config; std::optional<std::string> error;}`, `codeqa::application::LoadProjectConfig::execute(const std::filesystem::path& path) const -> LoadProjectConfigResult`. This is the only piece of this fase that touches disk.

- [ ] **Step 1: Write the failing test**

Create `tests/unit/LoadProjectConfigTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/application/LoadProjectConfig.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace
{

class TempConfigFile
{
public:
    TempConfigFile(const std::string& name, const std::string& content)
        : path_(std::filesystem::temp_directory_path() / "codeqa_load_project_config_test" / name)
    {
        std::filesystem::create_directories(path_.parent_path());
        std::ofstream(path_) << content;
    }

    ~TempConfigFile()
    {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    TempConfigFile(const TempConfigFile&) = delete;
    TempConfigFile& operator=(const TempConfigFile&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("LoadProjectConfig merges a valid file over the embedded defaults")
{
    const TempConfigFile file{
        "codeqa.json",
        R"({ "rules": { "complexity.cyclomatic": { "enabled": false, "params": { "warning": 2, "high": 4 } } } })"};

    const codeqa::application::LoadProjectConfigResult result =
        codeqa::application::LoadProjectConfig{}.execute(file.path());

    REQUIRE(result.config.has_value());
    CHECK_FALSE(result.error.has_value());
    CHECK_FALSE(result.config->rules.at("complexity.cyclomatic").enabled);
    CHECK(result.config->rules.at("complexity.cyclomatic").warningLimit == 2);
    REQUIRE(result.config->rules.count("size.function-length") == 1);
    CHECK(result.config->rules.at("size.function-length").enabled);
    CHECK(result.config->rules.at("size.function-length").warningLimit == 50);
}

TEST_CASE("LoadProjectConfig reports CONFIG007 for a missing file")
{
    const std::filesystem::path missing =
        std::filesystem::temp_directory_path() / "codeqa_load_project_config_test" / "does-not-exist.json";

    const codeqa::application::LoadProjectConfigResult result =
        codeqa::application::LoadProjectConfig{}.execute(missing);

    CHECK_FALSE(result.config.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG007") != std::string::npos);
}

TEST_CASE("LoadProjectConfig propagates a parseConfig error for invalid JSON")
{
    const TempConfigFile file{"broken.json", "{ not json"};

    const codeqa::application::LoadProjectConfigResult result =
        codeqa::application::LoadProjectConfig{}.execute(file.path());

    CHECK_FALSE(result.config.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG001") != std::string::npos);
}
```

Add `unit/LoadProjectConfigTest.cpp` to `tests/CMakeLists.txt`, right after `unit/ConfigJsonTest.cpp`.

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails — `codeqa/application/LoadProjectConfig.h` does not exist.

- [ ] **Step 3: Create the header and implementation**

Create `include/codeqa/application/LoadProjectConfig.h`:

```cpp
#pragma once

#include "codeqa/config/ConfigModels.h"

#include <filesystem>
#include <optional>
#include <string>

namespace codeqa::application
{

struct LoadProjectConfigResult
{
    std::optional<codeqa::config::ProjectConfig> config;
    std::optional<std::string> error;
};

class LoadProjectConfig
{
public:
    [[nodiscard]] LoadProjectConfigResult execute(const std::filesystem::path& path) const;
};

} // namespace codeqa::application
```

Create `src/application/LoadProjectConfig.cpp`:

```cpp
#include "codeqa/application/LoadProjectConfig.h"

#include "codeqa/config/ConfigJson.h"
#include "codeqa/config/DefaultConfig.h"
#include "codeqa/config/MergeConfig.h"
#include "codeqa/infrastructure/ReadFileContents.h"

namespace codeqa::application
{

LoadProjectConfigResult LoadProjectConfig::execute(const std::filesystem::path& path) const
{
    const std::optional<std::string> content = codeqa::infrastructure::readFileContents(path);

    if (!content.has_value())
    {
        return LoadProjectConfigResult{
            std::nullopt, "Error [CONFIG007]: could not read configuration file '" + path.string() + "'."};
    }

    const codeqa::config::ParseConfigResult parsed = codeqa::config::parseConfig(*content);

    if (parsed.error.has_value())
    {
        return LoadProjectConfigResult{std::nullopt, parsed.error};
    }

    return LoadProjectConfigResult{
        codeqa::config::mergeConfig(codeqa::config::defaultConfig(), *parsed.config), std::nullopt};
}

} // namespace codeqa::application
```

In `CMakeLists.txt`, add `src/application/LoadProjectConfig.cpp` to `add_library(codeqa_core STATIC ...)`, right after `src/application/ApplyRules.cpp`.

- [ ] **Step 4: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass, including the 3 new ones.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/application/LoadProjectConfig.h src/application/LoadProjectConfig.cpp tests/unit/LoadProjectConfigTest.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add LoadProjectConfig to read and merge codeqa.json"
```

---

### Task 7: wire `codeqa::rules` thresholds to `ProjectConfig`

**Files:**
- Modify: `include/codeqa/rules/Rules.h`
- Modify: `src/rules/Rules.cpp`
- Modify: `tests/unit/RulesTest.cpp`

**Interfaces:**
- Consumes: `codeqa::config::ProjectConfig`, `RuleConfig` (Task 1); `codeqa::config::defaultConfig()` (Task 2, in tests only).
- Produces: `codeqa::rules::allRules(const codeqa::config::ProjectConfig& config) -> std::vector<std::unique_ptr<Rule>>` (signature change — was `allRules()`). `Rule`/`RuleContext` (`include/codeqa/rules/Rule.h`) are untouched. Consumed by `ApplyRules` (Task 8).

- [ ] **Step 1: Write the failing test**

In `tests/unit/RulesTest.cpp`:

1. Add `#include "codeqa/config/DefaultConfig.h"` to the includes at the top of the file.
2. Replace every one of the 13 occurrences of `codeqa::rules::allRules()` in the file with `codeqa::rules::allRules(codeqa::config::defaultConfig())`. Every occurrence has the exact same shape:

```cpp
const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules();
```

becomes:

```cpp
const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(codeqa::config::defaultConfig());
```

This applies inside every existing `TEST_CASE` in the file (the 8 per-rule tests, plus `"all eight rules are registered and apply to both C and C++"`). No other line in any of those test bodies changes.

3. Append these two new `TEST_CASE`s at the end of the file:

```cpp
TEST_CASE("allRules omits a rule disabled via config")
{
    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.rules.at("complexity.cyclomatic").enabled = false;

    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(config);

    REQUIRE(rules.size() == 7);
    for (const auto& rule : rules)
    {
        CHECK(rule->id() != "complexity.cyclomatic");
    }
}

TEST_CASE("allRules applies a threshold overridden via config")
{
    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.rules.at("complexity.cyclomatic").warningLimit = 2;
    config.rules.at("complexity.cyclomatic").highLimit = 4;

    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(config);
    const codeqa::rules::Rule& rule = ruleNamed(rules, "complexity.cyclomatic");

    codeqa::model::CallableModel callable;
    callable.symbol.name = "sample";
    callable.symbol.qualifiedName = "Sample::sample";
    codeqa::metrics::FunctionMetricsModel metrics;
    metrics.complexity.cyclomaticComplexity = 3;

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(findings.size() == 1);
    CHECK(findings[0].severity == codeqa::core::DiagnosticSeverity::Warning);
    REQUIRE(findings[0].limitValue.has_value());
    CHECK(*findings[0].limitValue == 2);
}
```

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails to compile — `codeqa::rules::allRules` does not accept an argument yet, and `config.rules.at(...)` cannot resolve.

- [ ] **Step 3: Update `Rules.h` and `Rules.cpp`**

Replace `include/codeqa/rules/Rules.h` in full:

```cpp
#pragma once

#include "codeqa/config/ConfigModels.h"
#include "codeqa/rules/Rule.h"

#include <memory>
#include <vector>

namespace codeqa::rules
{

[[nodiscard]] std::vector<std::unique_ptr<Rule>> allRules(const codeqa::config::ProjectConfig& config);

} // namespace codeqa::rules
```

In `src/rules/Rules.cpp`, every one of the 8 rule classes gains a constructor and switches its two `static constexpr` members to instance members read from that constructor. For example, `FunctionLengthRule` changes from:

```cpp
class FunctionLengthRule : public Rule
{
public:
    static constexpr long long warningLimit = 50;
    static constexpr long long highLimit = 90;

    [[nodiscard]] std::string id() const override { return "size.function-length"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        return singleOrEmpty(evaluateThreshold(
            id(),
            "Function is too long.",
            "Consider splitting this function into smaller ones.",
            static_cast<long long>(context.metrics.size.physicalLines),
            warningLimit,
            highLimit,
            context.callable.symbol.range.begin,
            context.callable.symbol.qualifiedName));
    }
};
```

to:

```cpp
class FunctionLengthRule : public Rule
{
public:
    FunctionLengthRule(long long warningLimit, long long highLimit)
        : warningLimit_(warningLimit), highLimit_(highLimit)
    {
    }

    [[nodiscard]] std::string id() const override { return "size.function-length"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        return singleOrEmpty(evaluateThreshold(
            id(),
            "Function is too long.",
            "Consider splitting this function into smaller ones.",
            static_cast<long long>(context.metrics.size.physicalLines),
            warningLimit_,
            highLimit_,
            context.callable.symbol.range.begin,
            context.callable.symbol.qualifiedName));
    }

private:
    long long warningLimit_;
    long long highLimit_;
};
```

Apply the same transformation to the other 7 classes too. To remove all ambiguity, replace the full content of `src/rules/Rules.cpp` with:

```cpp
#include "codeqa/rules/Rules.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace codeqa::rules
{

namespace
{

std::optional<Finding> evaluateThreshold(
    std::string ruleId,
    std::string description,
    std::string recommendation,
    long long observedValue,
    long long warningLimit,
    long long highLimit,
    codeqa::core::SourceLocation location,
    std::string symbolName)
{
    if (observedValue < warningLimit)
    {
        return std::nullopt;
    }

    Finding finding;
    finding.ruleId = std::move(ruleId);
    finding.severity = observedValue >= highLimit
        ? codeqa::core::DiagnosticSeverity::Error
        : codeqa::core::DiagnosticSeverity::Warning;
    finding.description = std::move(description);
    finding.recommendation = std::move(recommendation);
    finding.observedValue = observedValue;
    finding.limitValue = observedValue >= highLimit ? highLimit : warningLimit;
    finding.location = std::move(location);
    finding.symbolName = std::move(symbolName);
    return finding;
}

// Duplicated from CalculateFunctionMetrics.cpp deliberately -- this is its
// second occurrence, still under the "rule of three" this project already
// uses to decide when small predicates get shared vs. copied (see the Fase
// 6b final review and this phase's design doc, section 7).
bool isBooleanValueParameter(const codeqa::model::ParameterModel& parameter)
{
    if (parameter.passing != codeqa::model::ParameterPassing::Value)
    {
        return false;
    }

    std::istringstream stream(parameter.declaredType);
    std::vector<std::string> tokens;
    std::string token;

    while (stream >> token)
    {
        if (token != "const" && token != "volatile")
        {
            tokens.push_back(token);
        }
    }

    return tokens.size() == 1 && (tokens[0] == "bool" || tokens[0] == "_Bool");
}

std::vector<Finding> singleOrEmpty(const std::optional<Finding>& finding)
{
    return finding.has_value() ? std::vector<Finding>{*finding} : std::vector<Finding>{};
}

class FunctionLengthRule : public Rule
{
public:
    FunctionLengthRule(long long warningLimit, long long highLimit)
        : warningLimit_(warningLimit), highLimit_(highLimit)
    {
    }

    [[nodiscard]] std::string id() const override { return "size.function-length"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        return singleOrEmpty(evaluateThreshold(
            id(),
            "Function is too long.",
            "Consider splitting this function into smaller ones.",
            static_cast<long long>(context.metrics.size.physicalLines),
            warningLimit_,
            highLimit_,
            context.callable.symbol.range.begin,
            context.callable.symbol.qualifiedName));
    }

private:
    long long warningLimit_;
    long long highLimit_;
};

class LocalVariableCountRule : public Rule
{
public:
    LocalVariableCountRule(long long warningLimit, long long highLimit)
        : warningLimit_(warningLimit), highLimit_(highLimit)
    {
    }

    [[nodiscard]] std::string id() const override { return "size.local-variables"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        return singleOrEmpty(evaluateThreshold(
            id(),
            "Function declares too many local variables.",
            "Consider extracting part of this function's logic into helper functions.",
            static_cast<long long>(context.callable.localVariables.size()),
            warningLimit_,
            highLimit_,
            context.callable.symbol.range.begin,
            context.callable.symbol.qualifiedName));
    }

private:
    long long warningLimit_;
    long long highLimit_;
};

class ParameterCountRule : public Rule
{
public:
    ParameterCountRule(long long warningLimit, long long highLimit)
        : warningLimit_(warningLimit), highLimit_(highLimit)
    {
    }

    [[nodiscard]] std::string id() const override { return "signature.parameter-count"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        const codeqa::core::SourceLocation location = context.callable.parameterListRange.has_value()
            ? context.callable.parameterListRange->begin
            : context.callable.symbol.range.begin;
        return singleOrEmpty(evaluateThreshold(
            id(),
            "Function has too many parameters.",
            "Consider grouping related parameters into a struct.",
            static_cast<long long>(context.metrics.signature.parameterCount),
            warningLimit_,
            highLimit_,
            location,
            context.callable.symbol.qualifiedName));
    }

private:
    long long warningLimit_;
    long long highLimit_;
};

class ConsecutiveBooleanParametersRule : public Rule
{
public:
    ConsecutiveBooleanParametersRule(long long warningLimit, long long highLimit)
        : warningLimit_(warningLimit), highLimit_(highLimit)
    {
    }

    [[nodiscard]] std::string id() const override { return "signature.consecutive-booleans"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        std::size_t longestRun = 0;
        std::size_t currentRun = 0;

        for (const codeqa::model::ParameterModel& parameter : context.callable.parameters)
        {
            if (isBooleanValueParameter(parameter))
            {
                ++currentRun;
                longestRun = std::max(longestRun, currentRun);
            }
            else
            {
                currentRun = 0;
            }
        }

        const codeqa::core::SourceLocation location = context.callable.parameterListRange.has_value()
            ? context.callable.parameterListRange->begin
            : context.callable.symbol.range.begin;
        return singleOrEmpty(evaluateThreshold(
            id(),
            "Function has consecutive boolean parameters.",
            "Consider replacing consecutive boolean parameters with an enum or a struct to "
            "avoid the \"boolean trap\" at call sites.",
            static_cast<long long>(longestRun),
            warningLimit_,
            highLimit_,
            location,
            context.callable.symbol.qualifiedName));
    }

private:
    long long warningLimit_;
    long long highLimit_;
};

class CyclomaticComplexityRule : public Rule
{
public:
    CyclomaticComplexityRule(long long warningLimit, long long highLimit)
        : warningLimit_(warningLimit), highLimit_(highLimit)
    {
    }

    [[nodiscard]] std::string id() const override { return "complexity.cyclomatic"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        return singleOrEmpty(evaluateThreshold(
            id(),
            "Cyclomatic complexity is too high.",
            "Consider splitting this function to reduce the number of independent paths through it.",
            static_cast<long long>(context.metrics.complexity.cyclomaticComplexity),
            warningLimit_,
            highLimit_,
            context.callable.symbol.range.begin,
            context.callable.symbol.qualifiedName));
    }

private:
    long long warningLimit_;
    long long highLimit_;
};

class ReturnCountRule : public Rule
{
public:
    ReturnCountRule(long long warningLimit, long long highLimit)
        : warningLimit_(warningLimit), highLimit_(highLimit)
    {
    }

    [[nodiscard]] std::string id() const override { return "complexity.returns"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        return singleOrEmpty(evaluateThreshold(
            id(),
            "Function has too many return points.",
            "Consider consolidating exit points to make control flow easier to follow.",
            static_cast<long long>(context.metrics.complexity.returnCount),
            warningLimit_,
            highLimit_,
            context.callable.symbol.range.begin,
            context.callable.symbol.qualifiedName));
    }

private:
    long long warningLimit_;
    long long highLimit_;
};

class ConditionComplexityRule : public Rule
{
public:
    ConditionComplexityRule(long long warningLimit, long long highLimit)
        : warningLimit_(warningLimit), highLimit_(highLimit)
    {
    }

    [[nodiscard]] std::string id() const override { return "complexity.condition"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        if (!context.metrics.complexity.maxConditionRange.has_value())
        {
            return {};
        }

        return singleOrEmpty(evaluateThreshold(
            id(),
            "A condition in this function is too complex.",
            "Consider extracting this condition into a well-named helper function or variable.",
            static_cast<long long>(context.metrics.complexity.maxConditionComplexity),
            warningLimit_,
            highLimit_,
            context.metrics.complexity.maxConditionRange->begin,
            context.callable.symbol.qualifiedName));
    }

private:
    long long warningLimit_;
    long long highLimit_;
};

class NestingDepthRule : public Rule
{
public:
    NestingDepthRule(long long warningLimit, long long highLimit)
        : warningLimit_(warningLimit), highLimit_(highLimit)
    {
    }

    [[nodiscard]] std::string id() const override { return "complexity.nesting"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        codeqa::core::SourceLocation location = context.callable.symbol.range.begin;

        for (const codeqa::model::ControlFlowModel& flow : context.callable.controlFlow)
        {
            if (flow.nestingDepth == context.metrics.complexity.maxNestingDepth)
            {
                location = flow.range.begin;
                break;
            }
        }

        return singleOrEmpty(evaluateThreshold(
            id(),
            "This function has excessive nesting.",
            "Consider using early returns or extracting nested blocks into helper functions.",
            static_cast<long long>(context.metrics.complexity.maxNestingDepth),
            warningLimit_,
            highLimit_,
            location,
            context.callable.symbol.qualifiedName));
    }

private:
    long long warningLimit_;
    long long highLimit_;
};

} // namespace

std::vector<std::unique_ptr<Rule>> allRules(const codeqa::config::ProjectConfig& config)
{
    std::vector<std::unique_ptr<Rule>> rules;

    if (const codeqa::config::RuleConfig& c = config.rules.at("size.function-length"); c.enabled)
    {
        rules.push_back(std::make_unique<FunctionLengthRule>(c.warningLimit, c.highLimit));
    }

    if (const codeqa::config::RuleConfig& c = config.rules.at("size.local-variables"); c.enabled)
    {
        rules.push_back(std::make_unique<LocalVariableCountRule>(c.warningLimit, c.highLimit));
    }

    if (const codeqa::config::RuleConfig& c = config.rules.at("signature.parameter-count"); c.enabled)
    {
        rules.push_back(std::make_unique<ParameterCountRule>(c.warningLimit, c.highLimit));
    }

    if (const codeqa::config::RuleConfig& c = config.rules.at("signature.consecutive-booleans"); c.enabled)
    {
        rules.push_back(std::make_unique<ConsecutiveBooleanParametersRule>(c.warningLimit, c.highLimit));
    }

    if (const codeqa::config::RuleConfig& c = config.rules.at("complexity.cyclomatic"); c.enabled)
    {
        rules.push_back(std::make_unique<CyclomaticComplexityRule>(c.warningLimit, c.highLimit));
    }

    if (const codeqa::config::RuleConfig& c = config.rules.at("complexity.returns"); c.enabled)
    {
        rules.push_back(std::make_unique<ReturnCountRule>(c.warningLimit, c.highLimit));
    }

    if (const codeqa::config::RuleConfig& c = config.rules.at("complexity.condition"); c.enabled)
    {
        rules.push_back(std::make_unique<ConditionComplexityRule>(c.warningLimit, c.highLimit));
    }

    if (const codeqa::config::RuleConfig& c = config.rules.at("complexity.nesting"); c.enabled)
    {
        rules.push_back(std::make_unique<NestingDepthRule>(c.warningLimit, c.highLimit));
    }

    return rules;
}

} // namespace codeqa::rules
```

`config.rules.at(id)` relies on the invariant that `config.rules` always contains all 8 known ids — guaranteed by `defaultConfig()` (Task 2) and preserved by `mergeConfig()` (Task 3, which starts from a copy of the defaults). `.at()` throwing `std::out_of_range` here would mean that invariant was violated by a bug elsewhere in this fase, not by user input (user input is validated by `parseConfig`, Task 5, before it ever reaches `mergeConfig`).

- [ ] **Step 4: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass, including the 2 new ones. Every pre-existing `RulesTest.cpp` assertion (thresholds, locations, severities) behaves identically because `defaultConfig()` reproduces the exact same 8 threshold pairs the `static constexpr` members used to hardcode.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/rules/Rules.h src/rules/Rules.cpp tests/unit/RulesTest.cpp
git commit -m "feat: wire codeqa::rules thresholds to ProjectConfig"
```

---

### Task 8: wire `ApplyRules` to `ProjectConfig`

**Files:**
- Modify: `include/codeqa/application/ApplyRules.h`
- Modify: `src/application/ApplyRules.cpp`
- Modify: `tests/unit/ApplyRulesTest.cpp`
- Modify: `tests/unit/CorpusFindingsTest.cpp`

**Interfaces:**
- Consumes: `codeqa::rules::allRules(const codeqa::config::ProjectConfig&)` (Task 7); `codeqa::config::defaultConfig()` (Task 2).
- Produces: `codeqa::application::ApplyRules::execute(const codeqa::model::ProjectModel& project, const codeqa::metrics::ProjectMetricsModel& metrics, const codeqa::config::ProjectConfig& config) const -> codeqa::rules::ProjectFindingsModel` (signature change — gains the `config` parameter as the third argument).

- [ ] **Step 1: Write the failing test**

In `tests/unit/ApplyRulesTest.cpp`:

1. Add `#include "codeqa/config/DefaultConfig.h"` to the includes at the top of the file.
2. Replace the `analyzeAndApplyRules` helper:

```cpp
codeqa::rules::ProjectFindingsModel analyzeAndApplyRules(const std::filesystem::path& root)
{
    codeqa::application::AnalyzeProjectRequest request;
    request.discovery.rootPath = root;
    const codeqa::model::ProjectModel project = codeqa::application::AnalyzeProject{}.execute(request);
    const codeqa::metrics::ProjectMetricsModel metrics = codeqa::application::MeasureProject{}.execute(project);
    return codeqa::application::ApplyRules{}.execute(project, metrics);
}
```

with:

```cpp
codeqa::rules::ProjectFindingsModel analyzeAndApplyRules(
    const std::filesystem::path& root,
    const codeqa::config::ProjectConfig& config = codeqa::config::defaultConfig())
{
    codeqa::application::AnalyzeProjectRequest request;
    request.discovery.rootPath = root;
    const codeqa::model::ProjectModel project = codeqa::application::AnalyzeProject{}.execute(request);
    const codeqa::metrics::ProjectMetricsModel metrics = codeqa::application::MeasureProject{}.execute(project);
    return codeqa::application::ApplyRules{}.execute(project, metrics, config);
}
```

The default argument means the 4 existing `TEST_CASE`s in the file, which all call `analyzeAndApplyRules(fixture.root())` with a single argument, need no other change.

3. Append these two new `TEST_CASE`s at the end of the file:

```cpp
TEST_CASE("ApplyRules omits findings for a rule disabled via config")
{
    const TempProject fixture;
    fixture.addFile(
        "sample.c",
        "int classify(int a, int b, int c, int d, int e)\n"
        "{\n"
        "    if (a == 0) { return 0; }\n"
        "    if (a == 1) { return 1; }\n"
        "    if (a == 2) { return 2; }\n"
        "    if (a == 3) { return 3; }\n"
        "    return -1;\n"
        "}\n");

    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.rules.at("signature.parameter-count").enabled = false;

    const codeqa::rules::ProjectFindingsModel findings = analyzeAndApplyRules(fixture.root(), config);

    REQUIRE(findings.files.size() == 1);
    CHECK_FALSE(hasFinding(findings.files.front().findings, "signature.parameter-count"));
    CHECK(hasFinding(findings.files.front().findings, "complexity.returns"));
}

TEST_CASE("ApplyRules fires a finding only after its threshold is lowered via config")
{
    const TempProject fixture;
    fixture.addFile(
        "sample.c",
        "int add(int a, int b, int c)\n"
        "{\n"
        "    return a + b + c;\n"
        "}\n");

    const codeqa::rules::ProjectFindingsModel withDefaults = analyzeAndApplyRules(fixture.root());
    REQUIRE(withDefaults.files.size() == 1);
    CHECK_FALSE(hasFinding(withDefaults.files.front().findings, "signature.parameter-count"));

    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.rules.at("signature.parameter-count").warningLimit = 2;
    config.rules.at("signature.parameter-count").highLimit = 3;

    const codeqa::rules::ProjectFindingsModel withLoweredThreshold = analyzeAndApplyRules(fixture.root(), config);
    REQUIRE(withLoweredThreshold.files.size() == 1);
    CHECK(hasFinding(withLoweredThreshold.files.front().findings, "signature.parameter-count"));
}
```

In `tests/unit/CorpusFindingsTest.cpp`, add `#include "codeqa/config/DefaultConfig.h"` to the includes, and replace:

```cpp
    const codeqa::rules::ProjectFindingsModel findings =
        codeqa::application::ApplyRules{}.execute(project, metrics);
```

with:

```cpp
    const codeqa::rules::ProjectFindingsModel findings =
        codeqa::application::ApplyRules{}.execute(project, metrics, codeqa::config::defaultConfig());
```

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails to compile — `ApplyRules::execute` does not accept a third argument yet.

- [ ] **Step 3: Update `ApplyRules.h` and `ApplyRules.cpp`**

Replace `include/codeqa/application/ApplyRules.h` in full:

```cpp
#pragma once

#include "codeqa/config/ConfigModels.h"
#include "codeqa/metrics/MetricsModels.h"
#include "codeqa/model/Models.h"
#include "codeqa/rules/RuleModels.h"

namespace codeqa::application
{

class ApplyRules
{
public:
    [[nodiscard]] codeqa::rules::ProjectFindingsModel execute(
        const codeqa::model::ProjectModel& project,
        const codeqa::metrics::ProjectMetricsModel& metrics,
        const codeqa::config::ProjectConfig& config) const;
};

} // namespace codeqa::application
```

In `src/application/ApplyRules.cpp`, change the `execute` signature:

```cpp
codeqa::rules::ProjectFindingsModel ApplyRules::execute(
    const codeqa::model::ProjectModel& project, const codeqa::metrics::ProjectMetricsModel& metrics) const
{
```

to:

```cpp
codeqa::rules::ProjectFindingsModel ApplyRules::execute(
    const codeqa::model::ProjectModel& project,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::config::ProjectConfig& config) const
{
```

and the one line inside it that builds the rule list:

```cpp
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules();
```

to:

```cpp
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(config);
```

Nothing else in `ApplyRules.cpp` changes — the confidence filter (`overlapsParseError`) and the inline-suppression logic (`isSuppressed`) are unrelated to configuration.

- [ ] **Step 4: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass, including the 2 new `ApplyRulesTest.cpp` cases. `CorpusFindingsTest.cpp`'s existing expectations (`tests/corpus/nesting/deep-nesting.findings.json`, `tests/corpus/parameters/cpp-parameters.findings.json`) are unchanged, since `defaultConfig()` reproduces the same 8 threshold pairs the corpus fixtures were already tuned against.

- [ ] **Step 5: Run the full test suite one last time**

Run:

```text
ctest --test-dir build --output-on-failure
```

Expected: every registered test (`codeqa_tests`, all `codeqa_integration_*` cases, `codeqa_corpus_integrity`) passes — this also exercises the two `codeqa parse` integration tests, confirming the config wiring did not disturb `codeqa`'s existing CLI-visible behavior (this fase deliberately does not add or change any CLI surface).

- [ ] **Step 6: Commit**

```bash
git add include/codeqa/application/ApplyRules.h src/application/ApplyRules.cpp tests/unit/ApplyRulesTest.cpp tests/unit/CorpusFindingsTest.cpp
git commit -m "feat: wire ApplyRules to ProjectConfig"
```

---

## Post-plan note for whoever plans Fase 9

`codeqa.json` still is not reachable from any `codeqa` command — `LoadProjectConfig` and `codeqa::rules::allRules(config)` are library-only until `codeqa analyze --config <path>` exists. `excludePatterns` via config, "listas permitidas", "patrones ignorados", and output format were deliberately left out of this fase's `ProjectConfig` (see `docs/superpowers/specs/2026-08-10-fase-8-configuracion-design.md` §2) — add them to `ConfigModels.h`/`ConfigJson.cpp` only once Fase 9 has a concrete consumer for each, following the same strict-validation pattern this fase established (`requireKeysSubset` at every level, `CONFIG00x` codes, fail-fast).
