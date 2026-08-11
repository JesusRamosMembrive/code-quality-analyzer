# Fase 9 — Informes de texto y JSON Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Un comando `codeqa analyze <path>` corre el pipeline completo (descubrimiento → detección de lenguaje → parseo → modelo común → métricas → reglas) y produce un reporte de texto legible o JSON determinista (`--format text|json`, `--output <file>`), con `--config` conectado por primera vez a un comando real (`--lang-override` de CLI gana sobre `headerOverrides` de `codeqa.json`). El JSON usa el esquema versionado que Fase 10 reutilizará tal cual para sus snapshots.

**Architecture:** Orquestador nuevo `codeqa::application::RunAnalysis` encadena `AnalyzeProject` → `MeasureProject` → `ApplyRules` (todos ya existentes) a partir de una `ProjectConfig` ya resuelta. Capa nueva `codeqa::report` (hermana de `rules`/`config`, sin depender de `application`) con un formateador de texto (`renderText`) y un serializador JSON (`toReportJson`, construido sobre `fileJson` por archivo). La CLI (`RunApplication.cpp`) resuelve `--config`, combina overrides de lenguaje, llama a `RunAnalysis`, y despacha al formateador según `--format`.

**Tech Stack:** C++17, CMake, doctest, nlohmann::json (ya vendorizado y consumido desde Fase 8).

## Global Constraints

- `analyze` **nunca** devuelve un exit code distinto de 0 por hallazgos, sin importar su severidad — solo por argumentos/config inválidos (`InvalidArguments`) o una excepción interna (`InternalError`, manejador ya existente). Decisión explícita del brainstorming de esta fase; cualquier gating de CI (`--fail-on`) queda fuera de esta fase.
- Precedencia de overrides de lenguaje: `--lang-override` (CLI) siempre gana sobre `headerOverrides` de `codeqa.json` para el mismo patrón. Se logra construyendo el vector combinado como `convert(config.headerOverrides) ++ cliLangOverrides` (config primero, CLI al final), porque `DetectFileLanguage::execute` (sin cambios en esta fase) recorre con `rbegin()/rend()` y devuelve el primer match — es decir, el **último** elemento del vector gana.
- `codeqa::report` depende solo de `model`/`metrics`/`rules`/`config`/`core` — nunca de `application`, para no invertir el layering (`cli → application → {rules, report, config, ...} → core`). Por eso `toReportJson` recibe `toolVersion` como `std::string` plano, no un `codeqa::application::ApplicationInfo`.
- `ProjectModel.files`, `ProjectMetricsModel.files` y `ProjectFindingsModel.files` están garantizados index-aligned: `MeasureProject::execute` y `ApplyRules::execute` (ambos sin cambios en esta fase) iteran `project.files` y hacen `push_back` de exactamente una entrada por archivo de entrada, en el mismo orden — verificado leyendo ambos `.cpp`. `toReportJson` explota este invariante con un bucle por índice; no se reemplaza por una búsqueda por path (no hace falta, y sería más lento sin ganar nada).
- `codeqa::model::FileModel.path` (heredado de Fase 4/5, sin cambios en esta fase) es la ruta **completa** (`rootPath / rutaRelativa`), no relativa — se confirmó leyendo `AnalyzeProject.cpp`/`ParseSourceFile.cpp`. Para que el JSON sea determinista (criterio de aceptación de esta fase), `toReportJson` relativiza cada `files[].path` contra `model.rootPath` antes de emitirlo; `fileJson` (una capa más abajo) no lo hace — simplemente serializa el `FileModel` que recibe tal cual, así que en aislamiento su `"path"` es lo que sea que el `FileModel` traiga.
- Los únicos dos campos no deterministas del JSON son `metadata.rootPath` (depende del path de invocación) y `metadata.toolVersion` (cambia entre releases) — todo lo demás, incluyendo cada `files[].path` ya relativizado, es determinista para un mismo árbol de fuentes y una misma config.
- Códigos de error CLI nuevos (`CLI008`-`CLI012`) solo para los tres flags nuevos de `analyze` (`--config`, `--format`, `--output`); `--exclude`/`--lang-override`/`<path>` reutilizan los códigos ya existentes de `discover`/`parse` (`CLI002`-`CLI007`) porque son exactamente el mismo modo de fallo.
- `analyze` necesita su propio bucle de parseo de argumentos (`parseAnalyzeArguments`), no reutiliza `parseDiscoverArguments` de `discover`/`parse` tal cual — esa función no reconoce `--config`/`--format`/`--output` y los rechazaría como argumento inesperado. Duplica las ramas de `--exclude`/`--lang-override` (segunda aparición de ese bloque concreto — la primera ya vive compartida entre `discover` y `parse` — sigue bajo el umbral de "regla de tres" que este proyecto ya usa).

## File Structure

- `include/codeqa/core/EnumJson.h` + `src/core/EnumJson.cpp` (nuevo): `languageToJsonString`/`languageFromJsonString`/`diagnosticSeverityToJsonString` — únicas fuentes de verdad para estas conversiones; también corrige de paso un hallazgo diferido de Fase 8 (`ConfigJson::toJson` mapeaba silenciosamente `Language::Unknown` a `"cpp"`).
- `include/codeqa/application/RunAnalysis.h` + `src/application/RunAnalysis.cpp` (nuevo): encadena `AnalyzeProject`/`MeasureProject`/`ApplyRules`.
- `include/codeqa/report/TextReport.h` + `src/report/TextReport.cpp` (nuevo): `renderText`.
- `include/codeqa/report/FileJson.h` + `src/report/FileJson.cpp` (nuevo): `fileJson`, serialización completa de un `FileModel` (símbolos, diagnósticos, dependencias).
- `include/codeqa/report/ReportJson.h` + `src/report/ReportJson.cpp` (nuevo): `toReportJson`, envuelve `fileJson` con métricas/hallazgos/config/metadata.
- `src/config/ConfigJson.cpp` (modificado): usa el helper de `EnumJson` en vez de su propio mapeo ad-hoc.
- `src/cli/RunApplication.cpp` (modificado): elimina su `severityName` local (usa el helper compartido), añade el subcomando `analyze`.
- `tests/unit/{EnumJson,RunAnalysis,TextReport,FileJson,ReportJson,CorpusReport}Test.cpp` (nuevos); `tests/unit/RunApplicationTest.cpp` (modificado).
- `CMakeLists.txt`, `tests/CMakeLists.txt` (modificados en varios tasks).

---

### Task 1: `codeqa::core` enum↔JSON string helpers

**Files:**
- Create: `include/codeqa/core/EnumJson.h`
- Create: `src/core/EnumJson.cpp`
- Create: `tests/unit/EnumJsonTest.cpp`
- Modify: `src/config/ConfigJson.cpp`
- Modify: `src/cli/RunApplication.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `codeqa::core::Language`, `codeqa::core::DiagnosticSeverity` (ya existentes, sin cambios).
- Produces: `codeqa::core::languageToJsonString(Language) -> std::string` (`"c"`/`"cpp"`/`"unknown"`), `codeqa::core::languageFromJsonString(const std::string&) -> std::optional<Language>` (`"c"`/`"cpp"` únicamente; cualquier otro texto, incluido `"unknown"`, es `std::nullopt`), `codeqa::core::diagnosticSeverityToJsonString(DiagnosticSeverity) -> std::string` (`"error"`/`"warning"`/`"information"`). Usados por Task 4, Task 5, y por el `ConfigJson.cpp`/`RunApplication.cpp` ya existentes.

- [ ] **Step 1: Write the failing test**

Create `tests/unit/EnumJsonTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/core/EnumJson.h"

TEST_CASE("languageToJsonString maps every Language value")
{
    CHECK(codeqa::core::languageToJsonString(codeqa::core::Language::C) == "c");
    CHECK(codeqa::core::languageToJsonString(codeqa::core::Language::Cpp) == "cpp");
    CHECK(codeqa::core::languageToJsonString(codeqa::core::Language::Unknown) == "unknown");
}

TEST_CASE("languageFromJsonString accepts only c and cpp")
{
    const std::optional<codeqa::core::Language> c = codeqa::core::languageFromJsonString("c");
    REQUIRE(c.has_value());
    CHECK(*c == codeqa::core::Language::C);

    const std::optional<codeqa::core::Language> cpp = codeqa::core::languageFromJsonString("cpp");
    REQUIRE(cpp.has_value());
    CHECK(*cpp == codeqa::core::Language::Cpp);

    CHECK_FALSE(codeqa::core::languageFromJsonString("unknown").has_value());
    CHECK_FALSE(codeqa::core::languageFromJsonString("rust").has_value());
}

TEST_CASE("diagnosticSeverityToJsonString maps every DiagnosticSeverity value")
{
    CHECK(codeqa::core::diagnosticSeverityToJsonString(codeqa::core::DiagnosticSeverity::Error) == "error");
    CHECK(codeqa::core::diagnosticSeverityToJsonString(codeqa::core::DiagnosticSeverity::Warning) == "warning");
    CHECK(
        codeqa::core::diagnosticSeverityToJsonString(codeqa::core::DiagnosticSeverity::Information)
        == "information");
}
```

Add `unit/EnumJsonTest.cpp` to `tests/CMakeLists.txt`, right after `unit/LoadProjectConfigTest.cpp` (the last entry).

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails — `codeqa/core/EnumJson.h` does not exist.

- [ ] **Step 3: Create the header and implementation**

Create `include/codeqa/core/EnumJson.h`:

```cpp
#pragma once

#include "codeqa/core/DiagnosticSeverity.h"
#include "codeqa/core/Language.h"

#include <optional>
#include <string>

namespace codeqa::core
{

[[nodiscard]] std::string languageToJsonString(Language language);
[[nodiscard]] std::optional<Language> languageFromJsonString(const std::string& text);
[[nodiscard]] std::string diagnosticSeverityToJsonString(DiagnosticSeverity severity);

} // namespace codeqa::core
```

Create `src/core/EnumJson.cpp`:

```cpp
#include "codeqa/core/EnumJson.h"

namespace codeqa::core
{

std::string languageToJsonString(Language language)
{
    switch (language)
    {
    case Language::C:
        return "c";
    case Language::Cpp:
        return "cpp";
    case Language::Unknown:
        return "unknown";
    }

    return "unknown";
}

std::optional<Language> languageFromJsonString(const std::string& text)
{
    if (text == "c")
    {
        return Language::C;
    }

    if (text == "cpp")
    {
        return Language::Cpp;
    }

    return std::nullopt;
}

std::string diagnosticSeverityToJsonString(DiagnosticSeverity severity)
{
    switch (severity)
    {
    case DiagnosticSeverity::Error:
        return "error";
    case DiagnosticSeverity::Warning:
        return "warning";
    case DiagnosticSeverity::Information:
        return "information";
    }

    return "error";
}

} // namespace codeqa::core
```

In `CMakeLists.txt`, add `src/core/EnumJson.cpp` to `add_library(codeqa_core STATIC ...)`, right after `src/config/ConfigJson.cpp` (the last entry).

- [ ] **Step 4: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass, including the 3 new ones.

- [ ] **Step 5: Wire the helper into `ConfigJson.cpp`**

In `src/config/ConfigJson.cpp`, add `#include "codeqa/core/EnumJson.h"` to the includes.

Replace the `parseLanguage` function:

```cpp
codeqa::core::Language parseLanguage(const std::string& text, const std::string& path)
{
    const std::optional<codeqa::core::Language> language = codeqa::core::languageFromJsonString(text);

    if (!language.has_value())
    {
        throw ParseFailure("Error [CONFIG006]: '" + path + "' must be 'c' or 'cpp', got '" + text + "'.");
    }

    return *language;
}
```

Replace the header-override serialization line inside `toJson`:

```cpp
    for (const HeaderOverrideConfig& headerOverride : config.headerOverrides)
    {
        document["languages"]["headerOverrides"].push_back(
            {{"pattern", headerOverride.pattern},
             {"language", codeqa::core::languageToJsonString(headerOverride.language)}});
    }
```

(This also fixes the Fase 8 deferred finding: a `HeaderOverrideConfig` with `Language::Unknown` used to silently serialize as `"cpp"`; it now serializes as `"unknown"`.)

- [ ] **Step 6: Wire the helper into `RunApplication.cpp`**

In `src/cli/RunApplication.cpp`, add `#include "codeqa/core/EnumJson.h"` to the includes. Delete the local `severityName` function (the `const char* severityName(codeqa::core::DiagnosticSeverity severity) { ... }` block, right before `runParse`). In `runParse`, replace the diagnostic-printing line:

```cpp
                output << "  line " << diagnostic.location.line << ": " << severityWord << '\n';
```

is not literal — the exact existing line is:

```cpp
                output << "  line " << diagnostic.location.line << ": " << severityName(diagnostic.severity)
                       << '\n';
```

Replace it with:

```cpp
                output << "  line " << diagnostic.location.line << ": "
                       << codeqa::core::diagnosticSeverityToJsonString(diagnostic.severity) << '\n';
```

- [ ] **Step 7: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass — in particular every existing `ConfigJsonTest.cpp` and `RunApplicationTest.cpp` case, unchanged behavior (same output strings, just produced by the shared helper now).

- [ ] **Step 8: Commit**

```bash
git add include/codeqa/core/EnumJson.h src/core/EnumJson.cpp tests/unit/EnumJsonTest.cpp src/config/ConfigJson.cpp src/cli/RunApplication.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add core Language/severity JSON string helpers"
```

---

### Task 2: `codeqa::application::RunAnalysis`

**Files:**
- Create: `include/codeqa/application/RunAnalysis.h`
- Create: `src/application/RunAnalysis.cpp`
- Create: `tests/unit/RunAnalysisTest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `codeqa::application::AnalyzeProjectRequest`/`AnalyzeProject` (Fase 5), `codeqa::application::MeasureProject` (Fase 6b), `codeqa::application::ApplyRules` (Fase 7/8), `codeqa::config::ProjectConfig`/`defaultConfig()` (Fase 8) — all unchanged.
- Produces: `codeqa::application::RunAnalysisRequest{AnalyzeProjectRequest analysis; codeqa::config::ProjectConfig config;}`, `codeqa::application::RunAnalysisResult{ProjectModel model; ProjectMetricsModel metrics; ProjectFindingsModel findings;}`, `codeqa::application::RunAnalysis::execute(const RunAnalysisRequest&) const -> RunAnalysisResult`. Used by Task 6 (CLI) and Task 7 (corpus test).

- [ ] **Step 1: Write the failing test**

Create `tests/unit/RunAnalysisTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/application/RunAnalysis.h"
#include "codeqa/config/DefaultConfig.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace
{

class TempProject
{
public:
    TempProject() : root_(std::filesystem::temp_directory_path() / "codeqa_run_analysis_test")
    {
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(root_);
    }

    ~TempProject()
    {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    TempProject(const TempProject&) = delete;
    TempProject& operator=(const TempProject&) = delete;

    void addFile(const std::filesystem::path& relativePath, const std::string& content) const
    {
        const std::filesystem::path path = root_ / relativePath;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path) << content;
    }

    [[nodiscard]] const std::filesystem::path& root() const { return root_; }

private:
    std::filesystem::path root_;
};

} // namespace

TEST_CASE("RunAnalysis chains discovery, metrics and rules into one consistent result")
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

    codeqa::application::RunAnalysisRequest request;
    request.analysis.discovery.rootPath = fixture.root();
    request.config = codeqa::config::defaultConfig();

    const codeqa::application::RunAnalysisResult result = codeqa::application::RunAnalysis{}.execute(request);

    REQUIRE(result.model.files.size() == 1);
    REQUIRE(result.metrics.files.size() == 1);
    REQUIRE(result.findings.files.size() == 1);
    CHECK(result.model.files.front().path == result.metrics.files.front().path);
    CHECK(result.model.files.front().path == result.findings.files.front().path);

    REQUIRE(result.metrics.files.front().functions.size() == 1);
    CHECK(result.metrics.files.front().functions.front().signature.parameterCount == 5);

    const std::vector<codeqa::rules::Finding>& findings = result.findings.files.front().findings;
    const bool hasParameterCountFinding = std::any_of(
        findings.begin(), findings.end(),
        [](const codeqa::rules::Finding& finding) { return finding.ruleId == "signature.parameter-count"; });
    CHECK(hasParameterCountFinding);
}

TEST_CASE("RunAnalysis with a config that disables a rule produces no finding for it")
{
    const TempProject fixture;
    fixture.addFile(
        "sample.c",
        "int classify(int a, int b, int c, int d, int e)\n"
        "{\n"
        "    return a + b + c + d + e;\n"
        "}\n");

    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.rules.at("signature.parameter-count").enabled = false;

    codeqa::application::RunAnalysisRequest request;
    request.analysis.discovery.rootPath = fixture.root();
    request.config = config;

    const codeqa::application::RunAnalysisResult result = codeqa::application::RunAnalysis{}.execute(request);

    REQUIRE(result.findings.files.size() == 1);
    const bool hasParameterCountFinding = std::any_of(
        result.findings.files.front().findings.begin(), result.findings.files.front().findings.end(),
        [](const codeqa::rules::Finding& finding) { return finding.ruleId == "signature.parameter-count"; });
    CHECK_FALSE(hasParameterCountFinding);
}
```

Add `unit/RunAnalysisTest.cpp` to `tests/CMakeLists.txt`, right after `unit/EnumJsonTest.cpp`.

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails — `codeqa/application/RunAnalysis.h` does not exist.

- [ ] **Step 3: Create the header and implementation**

Create `include/codeqa/application/RunAnalysis.h`:

```cpp
#pragma once

#include "codeqa/application/AnalyzeProject.h"
#include "codeqa/config/ConfigModels.h"
#include "codeqa/metrics/MetricsModels.h"
#include "codeqa/model/Models.h"
#include "codeqa/rules/RuleModels.h"

namespace codeqa::application
{

struct RunAnalysisRequest
{
    AnalyzeProjectRequest analysis;
    codeqa::config::ProjectConfig config;
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

Create `src/application/RunAnalysis.cpp`:

```cpp
#include "codeqa/application/RunAnalysis.h"

#include "codeqa/application/ApplyRules.h"
#include "codeqa/application/MeasureProject.h"

namespace codeqa::application
{

RunAnalysisResult RunAnalysis::execute(const RunAnalysisRequest& request) const
{
    RunAnalysisResult result;
    result.model = AnalyzeProject{}.execute(request.analysis);
    result.metrics = MeasureProject{}.execute(result.model);
    result.findings = ApplyRules{}.execute(result.model, result.metrics, request.config);
    return result;
}

} // namespace codeqa::application
```

In `CMakeLists.txt`, add `src/application/RunAnalysis.cpp` to `add_library(codeqa_core STATIC ...)`, right after `src/core/EnumJson.cpp`.

- [ ] **Step 4: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass, including the 2 new ones.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/application/RunAnalysis.h src/application/RunAnalysis.cpp tests/unit/RunAnalysisTest.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add RunAnalysis to chain metrics and rules over ProjectConfig"
```

---

### Task 3: `codeqa::report::renderText`

**Files:**
- Create: `include/codeqa/report/TextReport.h`
- Create: `src/report/TextReport.cpp`
- Create: `tests/unit/TextReportTest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `codeqa::model::ProjectModel`/`FileModel` (Fase 5), `codeqa::metrics::ProjectMetricsModel`/`FileMetricsModel` (Fase 6b), `codeqa::rules::ProjectFindingsModel`/`FileFindingsModel`/`Finding` (Fase 7), `codeqa::core::diagnosticSeverityToJsonString` (Task 1, for the lowercase word in the Diagnostics section).
- Produces: `codeqa::report::renderText(std::ostream&, const ProjectModel&, const ProjectMetricsModel&, const ProjectFindingsModel&)`. Used by Task 6 (CLI, `--format text`, the default).

- [ ] **Step 1: Write the failing test**

Create `tests/unit/TextReportTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/report/TextReport.h"

#include <sstream>
#include <string>

namespace
{

codeqa::rules::Finding makeFinding(
    const std::string& ruleId, codeqa::core::DiagnosticSeverity severity, const std::string& symbolName,
    std::size_t line)
{
    codeqa::rules::Finding finding;
    finding.ruleId = ruleId;
    finding.severity = severity;
    finding.description = "Something is off.";
    finding.recommendation = "Fix it.";
    finding.observedValue = 10;
    finding.limitValue = 5;
    finding.location.line = line;
    finding.location.column = 3;
    finding.symbolName = symbolName;
    return finding;
}

} // namespace

TEST_CASE("renderText with no diagnostics or findings reports zero and omits those sections")
{
    codeqa::model::ProjectModel model;
    model.rootPath = "project";
    codeqa::model::FileModel file;
    file.path = "src/clean.cpp";
    file.language = codeqa::core::Language::Cpp;
    file.state = codeqa::model::AnalysisState::Complete;
    model.files.push_back(file);

    codeqa::metrics::ProjectMetricsModel metrics;
    metrics.files.push_back(codeqa::metrics::FileMetricsModel{file.path, {}, {}});

    codeqa::rules::ProjectFindingsModel findings;
    codeqa::rules::FileFindingsModel fileFindings;
    fileFindings.path = file.path;
    findings.files.push_back(fileFindings);

    std::ostringstream output;
    codeqa::report::renderText(output, model, metrics, findings);
    const std::string text = output.str();

    CHECK(text.find("Code Quality Analysis Report") != std::string::npos);
    CHECK(text.find("Files analyzed: 1 (C: 0, C++: 1, unavailable: 0)") != std::string::npos);
    CHECK(text.find("Functions measured: 0") != std::string::npos);
    CHECK(text.find("Diagnostics: 0 (errors: 0, warnings: 0, information: 0)") != std::string::npos);
    CHECK(text.find("Findings: 0 (errors: 0, warnings: 0)") != std::string::npos);
    CHECK(text.find("Findings by severity") == std::string::npos);
    CHECK(text.find("Findings by file") == std::string::npos);
    CHECK(text.find("Diagnostics\n-----------") == std::string::npos);
    CHECK(text.find("Result: no findings.") != std::string::npos);
}

TEST_CASE("renderText lists findings by severity with errors before warnings, then groups by file")
{
    codeqa::model::ProjectModel model;
    model.rootPath = "project";
    codeqa::model::FileModel fileA;
    fileA.path = "src/a.cpp";
    fileA.language = codeqa::core::Language::Cpp;
    codeqa::model::FileModel fileB;
    fileB.path = "src/b.cpp";
    fileB.language = codeqa::core::Language::Cpp;
    model.files.push_back(fileA);
    model.files.push_back(fileB);

    codeqa::metrics::ProjectMetricsModel metrics;
    metrics.files.push_back(codeqa::metrics::FileMetricsModel{fileA.path, {}, {}});
    metrics.files.push_back(codeqa::metrics::FileMetricsModel{fileB.path, {}, {}});

    codeqa::rules::FileFindingsModel findingsA;
    findingsA.path = fileA.path;
    findingsA.findings.push_back(
        makeFinding("size.function-length", codeqa::core::DiagnosticSeverity::Warning, "warnFn", 10));
    codeqa::rules::FileFindingsModel findingsB;
    findingsB.path = fileB.path;
    findingsB.findings.push_back(
        makeFinding("complexity.cyclomatic", codeqa::core::DiagnosticSeverity::Error, "errorFn", 20));

    codeqa::rules::ProjectFindingsModel findings;
    findings.files.push_back(findingsA);
    findings.files.push_back(findingsB);

    std::ostringstream output;
    codeqa::report::renderText(output, model, metrics, findings);
    const std::string text = output.str();

    const std::size_t severitySection = text.find("Findings by severity");
    const std::size_t errorLine = text.find("[ERROR] complexity.cyclomatic");
    const std::size_t warningLine = text.find("[WARNING] size.function-length");
    REQUIRE(severitySection != std::string::npos);
    REQUIRE(errorLine != std::string::npos);
    REQUIRE(warningLine != std::string::npos);
    CHECK(errorLine < warningLine);

    const std::size_t fileSection = text.find("Findings by file");
    REQUIRE(fileSection != std::string::npos);
    const std::size_t fileALine = text.find("src/a.cpp", fileSection);
    const std::size_t fileBLine = text.find("src/b.cpp", fileSection);
    REQUIRE(fileALine != std::string::npos);
    REQUIRE(fileBLine != std::string::npos);
    CHECK(fileALine < fileBLine);

    CHECK(text.find("Result: 2 findings (1 error, 1 warning) across 2 files.") != std::string::npos);
}

TEST_CASE("renderText lists diagnostics grouped by file")
{
    codeqa::model::ProjectModel model;
    model.rootPath = "project";
    codeqa::model::FileModel file;
    file.path = "src/broken.cpp";
    file.language = codeqa::core::Language::Cpp;
    codeqa::core::Diagnostic diagnostic;
    diagnostic.code = "PARSE001";
    diagnostic.severity = codeqa::core::DiagnosticSeverity::Error;
    diagnostic.message = "unexpected token";
    diagnostic.location.line = 8;
    file.diagnostics.push_back(diagnostic);
    model.files.push_back(file);

    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::rules::ProjectFindingsModel findings;

    std::ostringstream output;
    codeqa::report::renderText(output, model, metrics, findings);
    const std::string text = output.str();

    CHECK(text.find("Diagnostics\n-----------") != std::string::npos);
    CHECK(text.find("src/broken.cpp") != std::string::npos);
    CHECK(text.find("  line 8: error [PARSE001] unexpected token") != std::string::npos);
}
```

Add `unit/TextReportTest.cpp` to `tests/CMakeLists.txt`, right after `unit/RunAnalysisTest.cpp`.

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails — `codeqa/report/TextReport.h` does not exist.

- [ ] **Step 3: Create the header and implementation**

Create `include/codeqa/report/TextReport.h`:

```cpp
#pragma once

#include "codeqa/metrics/MetricsModels.h"
#include "codeqa/model/Models.h"
#include "codeqa/rules/RuleModels.h"

#include <ostream>

namespace codeqa::report
{

void renderText(
    std::ostream& output,
    const codeqa::model::ProjectModel& model,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::rules::ProjectFindingsModel& findings);

} // namespace codeqa::report
```

Create `src/report/TextReport.cpp`:

```cpp
#include "codeqa/report/TextReport.h"

#include "codeqa/core/EnumJson.h"

#include <cstddef>
#include <vector>

namespace codeqa::report
{

namespace
{

const char* severityTag(codeqa::core::DiagnosticSeverity severity)
{
    switch (severity)
    {
    case codeqa::core::DiagnosticSeverity::Error:
        return "ERROR";
    case codeqa::core::DiagnosticSeverity::Warning:
        return "WARNING";
    case codeqa::core::DiagnosticSeverity::Information:
        return "INFORMATION";
    }

    return "ERROR";
}

struct SeverityCounts
{
    std::size_t errors{0};
    std::size_t warnings{0};
    std::size_t information{0};
};

void countSeverity(SeverityCounts& counts, codeqa::core::DiagnosticSeverity severity)
{
    switch (severity)
    {
    case codeqa::core::DiagnosticSeverity::Error:
        ++counts.errors;
        break;
    case codeqa::core::DiagnosticSeverity::Warning:
        ++counts.warnings;
        break;
    case codeqa::core::DiagnosticSeverity::Information:
        ++counts.information;
        break;
    }
}

struct LocatedFinding
{
    std::filesystem::path filePath;
    const codeqa::rules::Finding* finding{nullptr};
};

void writeSummary(
    std::ostream& output,
    const codeqa::model::ProjectModel& model,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::rules::ProjectFindingsModel& findings)
{
    std::size_t cCount = 0;
    std::size_t cppCount = 0;
    std::size_t unavailableCount = 0;

    for (const codeqa::model::FileModel& file : model.files)
    {
        if (file.language == codeqa::core::Language::C)
        {
            ++cCount;
        }
        else if (file.language == codeqa::core::Language::Cpp)
        {
            ++cppCount;
        }

        if (file.state == codeqa::model::AnalysisState::Unavailable)
        {
            ++unavailableCount;
        }
    }

    std::size_t functionCount = 0;
    for (const codeqa::metrics::FileMetricsModel& file : metrics.files)
    {
        functionCount += file.functions.size();
    }

    SeverityCounts diagnosticCounts;
    for (const codeqa::model::FileModel& file : model.files)
    {
        for (const codeqa::core::Diagnostic& diagnostic : file.diagnostics)
        {
            countSeverity(diagnosticCounts, diagnostic.severity);
        }
    }

    SeverityCounts findingCounts;
    for (const codeqa::rules::FileFindingsModel& file : findings.files)
    {
        for (const codeqa::rules::Finding& finding : file.findings)
        {
            countSeverity(findingCounts, finding.severity);
        }
    }

    output << "Summary\n"
              "-------\n"
              "Root: "
           << model.rootPath.generic_string() << "\n"
                                                  "Files analyzed: "
           << model.files.size() << " (C: " << cCount << ", C++: " << cppCount << ", unavailable: " << unavailableCount
           << ")\n"
              "Functions measured: "
           << functionCount << "\n"
                                "Diagnostics: "
           << (diagnosticCounts.errors + diagnosticCounts.warnings + diagnosticCounts.information)
           << " (errors: " << diagnosticCounts.errors << ", warnings: " << diagnosticCounts.warnings
           << ", information: " << diagnosticCounts.information
           << ")\n"
              "Findings: "
           << (findingCounts.errors + findingCounts.warnings) << " (errors: " << findingCounts.errors
           << ", warnings: " << findingCounts.warnings << ")\n";
}

std::vector<LocatedFinding> collectFindingsBySeverity(
    const codeqa::rules::ProjectFindingsModel& findings, codeqa::core::DiagnosticSeverity severity)
{
    std::vector<LocatedFinding> located;

    for (const codeqa::rules::FileFindingsModel& file : findings.files)
    {
        for (const codeqa::rules::Finding& finding : file.findings)
        {
            if (finding.severity == severity)
            {
                located.push_back(LocatedFinding{file.path, &finding});
            }
        }
    }

    return located;
}

void writeFindingsBySeverity(std::ostream& output, const codeqa::rules::ProjectFindingsModel& findings)
{
    std::vector<LocatedFinding> ordered =
        collectFindingsBySeverity(findings, codeqa::core::DiagnosticSeverity::Error);
    const std::vector<LocatedFinding> warnings =
        collectFindingsBySeverity(findings, codeqa::core::DiagnosticSeverity::Warning);
    ordered.insert(ordered.end(), warnings.begin(), warnings.end());

    if (ordered.empty())
    {
        return;
    }

    output << "\nFindings by severity\n---------------------\n";

    for (const LocatedFinding& located : ordered)
    {
        const codeqa::rules::Finding& finding = *located.finding;
        output << '[' << severityTag(finding.severity) << "] " << finding.ruleId << "  "
               << located.filePath.generic_string() << ':' << finding.location.line << ':' << finding.location.column
               << "  " << finding.symbolName << "\n    " << finding.description
               << " (observed=" << (finding.observedValue.has_value() ? *finding.observedValue : 0)
               << ", limit=" << (finding.limitValue.has_value() ? *finding.limitValue : 0) << ")\n";
    }
}

void writeFindingsByFile(std::ostream& output, const codeqa::rules::ProjectFindingsModel& findings)
{
    bool any = false;
    for (const codeqa::rules::FileFindingsModel& file : findings.files)
    {
        if (!file.findings.empty())
        {
            any = true;
            break;
        }
    }

    if (!any)
    {
        return;
    }

    output << "\nFindings by file\n-----------------\n";

    for (const codeqa::rules::FileFindingsModel& file : findings.files)
    {
        if (file.findings.empty())
        {
            continue;
        }

        output << file.path.generic_string() << '\n';

        for (const codeqa::rules::Finding& finding : file.findings)
        {
            output << "  [" << severityTag(finding.severity) << "] " << finding.ruleId << "  " << finding.location.line
                   << ':' << finding.location.column << "  " << finding.symbolName << " - " << finding.description
                   << '\n';
        }
    }
}

void writeDiagnostics(std::ostream& output, const codeqa::model::ProjectModel& model)
{
    bool any = false;
    for (const codeqa::model::FileModel& file : model.files)
    {
        if (!file.diagnostics.empty())
        {
            any = true;
            break;
        }
    }

    if (!any)
    {
        return;
    }

    output << "\nDiagnostics\n-----------\n";

    for (const codeqa::model::FileModel& file : model.files)
    {
        if (file.diagnostics.empty())
        {
            continue;
        }

        output << file.path.generic_string() << '\n';

        for (const codeqa::core::Diagnostic& diagnostic : file.diagnostics)
        {
            output << "  line " << diagnostic.location.line << ": "
                   << codeqa::core::diagnosticSeverityToJsonString(diagnostic.severity) << " [" << diagnostic.code
                   << "] " << diagnostic.message << '\n';
        }
    }
}

void writeResult(std::ostream& output, const codeqa::rules::ProjectFindingsModel& findings)
{
    SeverityCounts findingCounts;
    std::size_t filesWithFindings = 0;

    for (const codeqa::rules::FileFindingsModel& file : findings.files)
    {
        if (!file.findings.empty())
        {
            ++filesWithFindings;
        }

        for (const codeqa::rules::Finding& finding : file.findings)
        {
            countSeverity(findingCounts, finding.severity);
        }
    }

    const std::size_t total = findingCounts.errors + findingCounts.warnings;

    output << "\nResult: ";

    if (total == 0)
    {
        output << "no findings.\n";
        return;
    }

    output << total << " finding" << (total == 1 ? "" : "s") << " (" << findingCounts.errors << " error"
           << (findingCounts.errors == 1 ? "" : "s") << ", " << findingCounts.warnings << " warning"
           << (findingCounts.warnings == 1 ? "" : "s") << ") across " << filesWithFindings << " file"
           << (filesWithFindings == 1 ? "" : "s") << ".\n";
}

} // namespace

void renderText(
    std::ostream& output,
    const codeqa::model::ProjectModel& model,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::rules::ProjectFindingsModel& findings)
{
    output << "Code Quality Analysis Report\n=============================\n\n";
    writeSummary(output, model, metrics, findings);
    writeFindingsBySeverity(output, findings);
    writeFindingsByFile(output, findings);
    writeDiagnostics(output, model);
    writeResult(output, findings);
}

} // namespace codeqa::report
```

Note: `LocatedFinding` uses `std::filesystem::path`, so add `#include <filesystem>` to the top of `src/report/TextReport.cpp` alongside `<cstddef>`/`<vector>`.

In `CMakeLists.txt`, add `src/report/TextReport.cpp` to `add_library(codeqa_core STATIC ...)`, right after `src/application/RunAnalysis.cpp`.

- [ ] **Step 4: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass, including the 3 new ones.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/report/TextReport.h src/report/TextReport.cpp tests/unit/TextReportTest.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add codeqa::report text formatter"
```

---

### Task 4: `codeqa::report::fileJson`

**Files:**
- Create: `include/codeqa/report/FileJson.h`
- Create: `src/report/FileJson.cpp`
- Create: `tests/unit/FileJsonTest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `codeqa::model::FileModel` and everything it nests (`CallableModel`, `TypeModel`, `NamespaceModel`, `GlobalVariableModel`, `TypeAliasModel`, `DependencyModel`, `ParameterModel`, `LocalVariableModel`, `ControlFlowModel`, `CallModel` — all Fase 5/6a, unchanged); `codeqa::core::languageToJsonString`/`diagnosticSeverityToJsonString` (Task 1).
- Produces: `codeqa::report::fileJson(const codeqa::model::FileModel&) -> nlohmann::json`. Used by Task 5 (`toReportJson`).

- [ ] **Step 1: Write the failing test**

Create `tests/unit/FileJsonTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/report/FileJson.h"

TEST_CASE("fileJson serializes a file's language, state, diagnostics and structural symbols")
{
    codeqa::model::FileModel file;
    file.path = "src/sample.cpp";
    file.language = codeqa::core::Language::Cpp;
    file.state = codeqa::model::AnalysisState::Partial;

    codeqa::core::Diagnostic diagnostic;
    diagnostic.code = "PARSE001";
    diagnostic.severity = codeqa::core::DiagnosticSeverity::Warning;
    diagnostic.message = "recovered from an error";
    diagnostic.location.line = 4;
    diagnostic.location.column = 2;
    file.diagnostics.push_back(diagnostic);

    codeqa::model::DependencyModel dependency;
    dependency.target = "vector";
    dependency.kind = codeqa::model::IncludeKind::System;
    file.dependencies.push_back(dependency);

    codeqa::model::NamespaceModel namespaceModel;
    namespaceModel.symbol.name = "app";
    namespaceModel.symbol.qualifiedName = "app";
    file.namespaces.push_back(namespaceModel);

    codeqa::model::TypeModel type;
    type.symbol.name = "Widget";
    type.symbol.qualifiedName = "app::Widget";
    type.kind = codeqa::model::TypeKind::Struct;
    codeqa::model::CppTypeDetails details;
    details.isTemplate = true;
    details.baseSpecifiers.push_back("Base");
    type.cppDetails = details;
    file.types.push_back(type);

    codeqa::model::GlobalVariableModel global;
    global.symbol.name = "counter";
    global.symbol.qualifiedName = "app::counter";
    global.declaredType = "int";
    global.isStatic = true;
    file.globalVariables.push_back(global);

    codeqa::model::TypeAliasModel alias;
    alias.symbol.name = "WidgetPtr";
    alias.symbol.qualifiedName = "app::WidgetPtr";
    alias.aliasedType = "Widget*";
    file.typeAliases.push_back(alias);

    file.comments.push_back(codeqa::core::SourceRange{});

    codeqa::model::CallableModel callable;
    callable.symbol.name = "run";
    callable.symbol.qualifiedName = "app::Widget::run";
    callable.kind = codeqa::model::CallableKind::Method;

    codeqa::model::ParameterModel parameter;
    parameter.name = "value";
    parameter.passing = codeqa::model::ParameterPassing::Reference;
    parameter.declaredType = "int&";
    callable.parameters.push_back(parameter);

    codeqa::model::ControlFlowModel controlFlow;
    controlFlow.kind = codeqa::model::ControlFlowKind::If;
    controlFlow.nestingDepth = 1;
    callable.controlFlow.push_back(controlFlow);

    codeqa::model::CallModel call;
    call.calleeText = "helper";
    call.kind = codeqa::model::CallKind::Member;
    call.argumentCount = 1;
    callable.calls.push_back(call);

    file.callables.push_back(callable);

    const nlohmann::json document = codeqa::report::fileJson(file);

    CHECK(document.at("path").get<std::string>() == "src/sample.cpp");
    CHECK(document.at("language").get<std::string>() == "cpp");
    CHECK(document.at("state").get<std::string>() == "partial");

    REQUIRE(document.at("diagnostics").size() == 1);
    CHECK(document.at("diagnostics")[0].at("severity").get<std::string>() == "warning");
    CHECK(document.at("diagnostics")[0].at("location").at("line").get<int>() == 4);

    REQUIRE(document.at("dependencies").size() == 1);
    CHECK(document.at("dependencies")[0].at("kind").get<std::string>() == "system");

    REQUIRE(document.at("namespaces").size() == 1);
    CHECK(document.at("namespaces")[0].at("qualifiedName").get<std::string>() == "app");

    REQUIRE(document.at("types").size() == 1);
    CHECK(document.at("types")[0].at("kind").get<std::string>() == "struct");
    CHECK(document.at("types")[0].at("cppDetails").at("isTemplate").get<bool>());
    CHECK(document.at("types")[0].at("cppDetails").at("baseSpecifiers")[0].get<std::string>() == "Base");

    REQUIRE(document.at("globalVariables").size() == 1);
    CHECK(document.at("globalVariables")[0].at("isStatic").get<bool>());

    REQUIRE(document.at("typeAliases").size() == 1);
    CHECK(document.at("typeAliases")[0].at("aliasedType").get<std::string>() == "Widget*");

    REQUIRE(document.at("comments").size() == 1);

    REQUIRE(document.at("callables").size() == 1);
    const nlohmann::json& callableJson = document.at("callables")[0];
    CHECK(callableJson.at("kind").get<std::string>() == "method");
    REQUIRE(callableJson.at("parameters").size() == 1);
    CHECK(callableJson.at("parameters")[0].at("passing").get<std::string>() == "reference");
    REQUIRE(callableJson.at("controlFlow").size() == 1);
    CHECK(callableJson.at("controlFlow")[0].at("kind").get<std::string>() == "if");
    REQUIRE(callableJson.at("calls").size() == 1);
    CHECK(callableJson.at("calls")[0].at("kind").get<std::string>() == "member");
}

TEST_CASE("fileJson omits cppDetails when a type has none")
{
    codeqa::model::FileModel file;
    file.path = "src/plain.c";
    file.language = codeqa::core::Language::C;

    codeqa::model::TypeModel type;
    type.symbol.name = "Point";
    type.symbol.qualifiedName = "Point";
    type.kind = codeqa::model::TypeKind::Struct;
    file.types.push_back(type);

    const nlohmann::json document = codeqa::report::fileJson(file);

    CHECK(document.at("language").get<std::string>() == "c");
    CHECK_FALSE(document.at("types")[0].contains("cppDetails"));
}
```

Add `unit/FileJsonTest.cpp` to `tests/CMakeLists.txt`, right after `unit/TextReportTest.cpp`.

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails — `codeqa/report/FileJson.h` does not exist.

- [ ] **Step 3: Create the header and implementation**

Create `include/codeqa/report/FileJson.h`:

```cpp
#pragma once

#include "codeqa/model/Models.h"

#include <nlohmann/json.hpp>

namespace codeqa::report
{

[[nodiscard]] nlohmann::json fileJson(const codeqa::model::FileModel& file);

} // namespace codeqa::report
```

Create `src/report/FileJson.cpp`:

```cpp
#include "codeqa/report/FileJson.h"

#include "codeqa/core/EnumJson.h"

namespace codeqa::report
{

namespace
{

using Json = nlohmann::json;

Json locationJson(const codeqa::core::SourceLocation& location)
{
    return Json{{"line", location.line}, {"column", location.column}};
}

Json rangeJson(const codeqa::core::SourceRange& range)
{
    return Json{{"begin", locationJson(range.begin)}, {"end", locationJson(range.end)}};
}

std::string includeKindToJsonString(codeqa::model::IncludeKind kind)
{
    return kind == codeqa::model::IncludeKind::Local ? "local" : "system";
}

std::string typeKindToJsonString(codeqa::model::TypeKind kind)
{
    switch (kind)
    {
    case codeqa::model::TypeKind::Class:
        return "class";
    case codeqa::model::TypeKind::Struct:
        return "struct";
    case codeqa::model::TypeKind::Union:
        return "union";
    case codeqa::model::TypeKind::Enum:
        return "enum";
    }
    return "class";
}

std::string callableKindToJsonString(codeqa::model::CallableKind kind)
{
    switch (kind)
    {
    case codeqa::model::CallableKind::FreeFunction:
        return "freeFunction";
    case codeqa::model::CallableKind::Method:
        return "method";
    case codeqa::model::CallableKind::Constructor:
        return "constructor";
    case codeqa::model::CallableKind::Destructor:
        return "destructor";
    case codeqa::model::CallableKind::Operator:
        return "operator";
    case codeqa::model::CallableKind::Lambda:
        return "lambda";
    case codeqa::model::CallableKind::Prototype:
        return "prototype";
    }
    return "freeFunction";
}

std::string parameterPassingToJsonString(codeqa::model::ParameterPassing passing)
{
    switch (passing)
    {
    case codeqa::model::ParameterPassing::Value:
        return "value";
    case codeqa::model::ParameterPassing::Pointer:
        return "pointer";
    case codeqa::model::ParameterPassing::Reference:
        return "reference";
    case codeqa::model::ParameterPassing::FunctionPointer:
        return "functionPointer";
    }
    return "value";
}

std::string controlFlowKindToJsonString(codeqa::model::ControlFlowKind kind)
{
    switch (kind)
    {
    case codeqa::model::ControlFlowKind::If:
        return "if";
    case codeqa::model::ControlFlowKind::Switch:
        return "switch";
    case codeqa::model::ControlFlowKind::For:
        return "for";
    case codeqa::model::ControlFlowKind::While:
        return "while";
    case codeqa::model::ControlFlowKind::DoWhile:
        return "doWhile";
    case codeqa::model::ControlFlowKind::Case:
        return "case";
    case codeqa::model::ControlFlowKind::Default:
        return "default";
    case codeqa::model::ControlFlowKind::Conditional:
        return "conditional";
    case codeqa::model::ControlFlowKind::Return:
        return "return";
    case codeqa::model::ControlFlowKind::Break:
        return "break";
    case codeqa::model::ControlFlowKind::Continue:
        return "continue";
    }
    return "if";
}

std::string callKindToJsonString(codeqa::model::CallKind kind)
{
    switch (kind)
    {
    case codeqa::model::CallKind::Named:
        return "named";
    case codeqa::model::CallKind::Member:
        return "member";
    case codeqa::model::CallKind::Indirect:
        return "indirect";
    }
    return "named";
}

std::string stateToJsonString(codeqa::model::AnalysisState state)
{
    switch (state)
    {
    case codeqa::model::AnalysisState::Complete:
        return "complete";
    case codeqa::model::AnalysisState::Partial:
        return "partial";
    case codeqa::model::AnalysisState::Unavailable:
        return "unavailable";
    }
    return "unavailable";
}

Json symbolIdentityJson(const codeqa::model::SymbolIdentity& symbol)
{
    return Json{{"name", symbol.name}, {"qualifiedName", symbol.qualifiedName}, {"range", rangeJson(symbol.range)}};
}

Json diagnosticJson(const codeqa::core::Diagnostic& diagnostic)
{
    return Json{
        {"code", diagnostic.code},
        {"severity", codeqa::core::diagnosticSeverityToJsonString(diagnostic.severity)},
        {"message", diagnostic.message},
        {"location", locationJson(diagnostic.location)}};
}

Json dependencyJson(const codeqa::model::DependencyModel& dependency)
{
    return Json{
        {"target", dependency.target},
        {"kind", includeKindToJsonString(dependency.kind)},
        {"range", rangeJson(dependency.range)}};
}

Json typeJson(const codeqa::model::TypeModel& type)
{
    Json document = symbolIdentityJson(type.symbol);
    document["kind"] = typeKindToJsonString(type.kind);

    if (type.cppDetails.has_value())
    {
        document["cppDetails"] = Json{
            {"isTemplate", type.cppDetails->isTemplate},
            {"isScopedEnum", type.cppDetails->isScopedEnum},
            {"baseSpecifiers", type.cppDetails->baseSpecifiers}};
    }

    return document;
}

Json globalVariableJson(const codeqa::model::GlobalVariableModel& global)
{
    Json document = symbolIdentityJson(global.symbol);
    document["declaredType"] = global.declaredType;
    document["isStatic"] = global.isStatic;
    document["isExtern"] = global.isExtern;
    document["hasInitializer"] = global.hasInitializer;
    return document;
}

Json typeAliasJson(const codeqa::model::TypeAliasModel& alias)
{
    Json document = symbolIdentityJson(alias.symbol);
    document["aliasedType"] = alias.aliasedType;
    return document;
}

Json parameterJson(const codeqa::model::ParameterModel& parameter)
{
    return Json{
        {"name", parameter.name},
        {"passing", parameterPassingToJsonString(parameter.passing)},
        {"hasDefaultValue", parameter.hasDefaultValue},
        {"declaredType", parameter.declaredType}};
}

Json localVariableJson(const codeqa::model::LocalVariableModel& local)
{
    Json document = symbolIdentityJson(local.symbol);
    document["declaredType"] = local.declaredType;
    document["hasInitializer"] = local.hasInitializer;
    document["lexicalDepth"] = local.lexicalDepth;
    return document;
}

Json controlFlowJson(const codeqa::model::ControlFlowModel& controlFlow)
{
    Json document{
        {"kind", controlFlowKindToJsonString(controlFlow.kind)},
        {"range", rangeJson(controlFlow.range)},
        {"nestingDepth", controlFlow.nestingDepth}};

    if (controlFlow.conditionRange.has_value())
    {
        document["conditionRange"] = rangeJson(*controlFlow.conditionRange);
    }

    return document;
}

Json callJson(const codeqa::model::CallModel& call)
{
    return Json{
        {"calleeText", call.calleeText},
        {"kind", callKindToJsonString(call.kind)},
        {"argumentCount", call.argumentCount},
        {"range", rangeJson(call.range)}};
}

Json callableJson(const codeqa::model::CallableModel& callable)
{
    Json document = symbolIdentityJson(callable.symbol);
    document["kind"] = callableKindToJsonString(callable.kind);
    document["isStatic"] = callable.isStatic;
    document["isVariadic"] = callable.isVariadic;

    document["parameters"] = Json::array();
    for (const codeqa::model::ParameterModel& parameter : callable.parameters)
    {
        document["parameters"].push_back(parameterJson(parameter));
    }

    if (callable.parameterListRange.has_value())
    {
        document["parameterListRange"] = rangeJson(*callable.parameterListRange);
    }

    document["localVariables"] = Json::array();
    for (const codeqa::model::LocalVariableModel& local : callable.localVariables)
    {
        document["localVariables"].push_back(localVariableJson(local));
    }

    document["controlFlow"] = Json::array();
    for (const codeqa::model::ControlFlowModel& controlFlow : callable.controlFlow)
    {
        document["controlFlow"].push_back(controlFlowJson(controlFlow));
    }

    document["calls"] = Json::array();
    for (const codeqa::model::CallModel& call : callable.calls)
    {
        document["calls"].push_back(callJson(call));
    }

    return document;
}

} // namespace

nlohmann::json fileJson(const codeqa::model::FileModel& file)
{
    Json document;
    document["path"] = file.path.generic_string();
    document["language"] = codeqa::core::languageToJsonString(file.language);
    document["state"] = stateToJsonString(file.state);

    document["diagnostics"] = Json::array();
    for (const codeqa::core::Diagnostic& diagnostic : file.diagnostics)
    {
        document["diagnostics"].push_back(diagnosticJson(diagnostic));
    }

    document["dependencies"] = Json::array();
    for (const codeqa::model::DependencyModel& dependency : file.dependencies)
    {
        document["dependencies"].push_back(dependencyJson(dependency));
    }

    document["namespaces"] = Json::array();
    for (const codeqa::model::NamespaceModel& namespaceModel : file.namespaces)
    {
        document["namespaces"].push_back(symbolIdentityJson(namespaceModel.symbol));
    }

    document["types"] = Json::array();
    for (const codeqa::model::TypeModel& type : file.types)
    {
        document["types"].push_back(typeJson(type));
    }

    document["globalVariables"] = Json::array();
    for (const codeqa::model::GlobalVariableModel& global : file.globalVariables)
    {
        document["globalVariables"].push_back(globalVariableJson(global));
    }

    document["typeAliases"] = Json::array();
    for (const codeqa::model::TypeAliasModel& alias : file.typeAliases)
    {
        document["typeAliases"].push_back(typeAliasJson(alias));
    }

    document["comments"] = Json::array();
    for (const codeqa::core::SourceRange& comment : file.comments)
    {
        document["comments"].push_back(Json{{"range", rangeJson(comment)}});
    }

    document["callables"] = Json::array();
    for (const codeqa::model::CallableModel& callable : file.callables)
    {
        document["callables"].push_back(callableJson(callable));
    }

    return document;
}

} // namespace codeqa::report
```

In `CMakeLists.txt`, add `src/report/FileJson.cpp` to `add_library(codeqa_core STATIC ...)`, right after `src/report/TextReport.cpp`.

- [ ] **Step 4: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass, including the 2 new ones.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/report/FileJson.h src/report/FileJson.cpp tests/unit/FileJsonTest.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add codeqa::report per-file JSON symbol serialization"
```

---

### Task 5: `codeqa::report::toReportJson`

**Files:**
- Create: `include/codeqa/report/ReportJson.h`
- Create: `src/report/ReportJson.cpp`
- Create: `tests/unit/ReportJsonTest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `codeqa::report::fileJson` (Task 4); `codeqa::core::diagnosticSeverityToJsonString` (Task 1); `codeqa::config::toJson` (Fase 8, unchanged); `codeqa::metrics::FunctionMetricsModel`/`ProjectMetricsModel` (Fase 6b); `codeqa::rules::Finding`/`ProjectFindingsModel` (Fase 7).
- Produces: `codeqa::report::toReportJson(const ProjectModel&, const ProjectMetricsModel&, const ProjectFindingsModel&, const ProjectConfig&, const std::string& toolVersion) -> nlohmann::json`. Used by Task 6 (CLI, `--format json`) and Task 7 (corpus test).

- [ ] **Step 1: Write the failing test**

Create `tests/unit/ReportJsonTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/config/DefaultConfig.h"
#include "codeqa/report/ReportJson.h"

#include <filesystem>

TEST_CASE("toReportJson wraps each file's symbols with its metrics and findings, plus schema metadata")
{
    codeqa::model::ProjectModel model;
    model.rootPath = std::filesystem::path{"project"};
    codeqa::model::FileModel file;
    file.path = model.rootPath / "src" / "sample.cpp";
    file.language = codeqa::core::Language::Cpp;
    codeqa::model::CallableModel callable;
    callable.symbol.name = "run";
    callable.symbol.qualifiedName = "run";
    file.callables.push_back(callable);
    model.files.push_back(file);

    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::metrics::FileMetricsModel fileMetrics;
    fileMetrics.path = file.path;
    codeqa::metrics::FunctionMetricsModel functionMetrics;
    functionMetrics.symbol = callable.symbol;
    functionMetrics.complexity.cyclomaticComplexity = 12;
    fileMetrics.functions.push_back(functionMetrics);
    metrics.files.push_back(fileMetrics);

    codeqa::rules::ProjectFindingsModel findings;
    codeqa::rules::FileFindingsModel fileFindings;
    fileFindings.path = file.path;
    codeqa::rules::Finding finding;
    finding.ruleId = "complexity.cyclomatic";
    finding.severity = codeqa::core::DiagnosticSeverity::Warning;
    finding.description = "too complex";
    finding.recommendation = "simplify";
    finding.observedValue = 12;
    finding.limitValue = 10;
    finding.symbolName = "run";
    fileFindings.findings.push_back(finding);
    findings.files.push_back(fileFindings);

    const codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    const nlohmann::json document = codeqa::report::toReportJson(model, metrics, findings, config, "9.9.9");

    CHECK(document.at("schemaVersion").get<int>() == 1);
    CHECK(document.at("metadata").at("rootPath").get<std::string>() == model.rootPath.generic_string());
    CHECK(document.at("metadata").at("toolVersion").get<std::string>() == "9.9.9");
    CHECK(document.at("config").at("rules").size() == 8);

    REQUIRE(document.at("files").size() == 1);
    const nlohmann::json& fileDocument = document.at("files")[0];
    CHECK(fileDocument.at("path").get<std::string>() == "src/sample.cpp");
    REQUIRE(fileDocument.at("callables").size() == 1);

    REQUIRE(fileDocument.at("metrics").size() == 1);
    CHECK(fileDocument.at("metrics")[0].at("complexity").at("cyclomaticComplexity").get<int>() == 12);

    REQUIRE(fileDocument.at("findings").size() == 1);
    CHECK(fileDocument.at("findings")[0].at("ruleId").get<std::string>() == "complexity.cyclomatic");
    CHECK(fileDocument.at("findings")[0].at("observedValue").get<int>() == 12);
}

TEST_CASE("toReportJson serializes an absent observedValue/limitValue as null")
{
    codeqa::model::ProjectModel model;
    model.rootPath = std::filesystem::path{"project"};
    codeqa::model::FileModel file;
    file.path = model.rootPath / "src" / "sample.cpp";
    model.files.push_back(file);

    codeqa::metrics::ProjectMetricsModel metrics;
    metrics.files.push_back(codeqa::metrics::FileMetricsModel{file.path, {}, {}});

    codeqa::rules::ProjectFindingsModel findings;
    codeqa::rules::FileFindingsModel fileFindings;
    fileFindings.path = file.path;
    codeqa::rules::Finding finding;
    finding.ruleId = "custom.rule";
    finding.symbolName = "x";
    fileFindings.findings.push_back(finding);
    findings.files.push_back(fileFindings);

    const nlohmann::json document =
        codeqa::report::toReportJson(model, metrics, findings, codeqa::config::defaultConfig(), "0.0.0");

    CHECK(document.at("files")[0].at("findings")[0].at("observedValue").is_null());
    CHECK(document.at("files")[0].at("findings")[0].at("limitValue").is_null());
}
```

Add `unit/ReportJsonTest.cpp` to `tests/CMakeLists.txt`, right after `unit/FileJsonTest.cpp`.

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails — `codeqa/report/ReportJson.h` does not exist.

- [ ] **Step 3: Create the header and implementation**

Create `include/codeqa/report/ReportJson.h`:

```cpp
#pragma once

#include "codeqa/config/ConfigModels.h"
#include "codeqa/metrics/MetricsModels.h"
#include "codeqa/model/Models.h"
#include "codeqa/rules/RuleModels.h"

#include <nlohmann/json.hpp>

#include <string>

namespace codeqa::report
{

[[nodiscard]] nlohmann::json toReportJson(
    const codeqa::model::ProjectModel& model,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::rules::ProjectFindingsModel& findings,
    const codeqa::config::ProjectConfig& config,
    const std::string& toolVersion);

} // namespace codeqa::report
```

Create `src/report/ReportJson.cpp`:

```cpp
#include "codeqa/report/ReportJson.h"

#include "codeqa/config/ConfigJson.h"
#include "codeqa/core/EnumJson.h"
#include "codeqa/report/FileJson.h"

#include <cstddef>
#include <filesystem>

namespace codeqa::report
{

namespace
{

using Json = nlohmann::json;

Json locationJson(const codeqa::core::SourceLocation& location)
{
    return Json{{"line", location.line}, {"column", location.column}};
}

Json functionMetricsJson(const codeqa::metrics::FunctionMetricsModel& function)
{
    return Json{
        {"symbol",
         {{"name", function.symbol.name},
          {"qualifiedName", function.symbol.qualifiedName},
          {"range",
           {{"begin", locationJson(function.symbol.range.begin)},
            {"end", locationJson(function.symbol.range.end)}}}}},
        {"size",
         {{"physicalLines", function.size.physicalLines},
          {"codeLines", function.size.codeLines},
          {"blankLines", function.size.blankLines},
          {"commentLines", function.size.commentLines},
          {"statementCount", function.size.statementCount}}},
        {"signature",
         {{"parameterCount", function.signature.parameterCount},
          {"signatureLength", function.signature.signatureLength},
          {"booleanParameterCount", function.signature.booleanParameterCount}}},
        {"complexity",
         {{"cyclomaticComplexity", function.complexity.cyclomaticComplexity},
          {"maxConditionComplexity", function.complexity.maxConditionComplexity},
          {"maxNestingDepth", function.complexity.maxNestingDepth},
          {"branchCount", function.complexity.branchCount},
          {"returnCount", function.complexity.returnCount}}}};
}

Json findingJson(const codeqa::rules::Finding& finding)
{
    Json document{
        {"ruleId", finding.ruleId},
        {"severity", codeqa::core::diagnosticSeverityToJsonString(finding.severity)},
        {"description", finding.description},
        {"recommendation", finding.recommendation},
        {"location", locationJson(finding.location)},
        {"symbolName", finding.symbolName}};

    document["observedValue"] = finding.observedValue.has_value() ? Json(*finding.observedValue) : Json(nullptr);
    document["limitValue"] = finding.limitValue.has_value() ? Json(*finding.limitValue) : Json(nullptr);

    return document;
}

} // namespace

nlohmann::json toReportJson(
    const codeqa::model::ProjectModel& model,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::rules::ProjectFindingsModel& findings,
    const codeqa::config::ProjectConfig& config,
    const std::string& toolVersion)
{
    Json document;
    document["schemaVersion"] = 1;
    document["metadata"] =
        Json{{"rootPath", model.rootPath.generic_string()}, {"toolName", "codeqa"}, {"toolVersion", toolVersion}};
    document["config"] = codeqa::config::toJson(config);

    document["files"] = Json::array();

    // Precondition: model.files, metrics.files and findings.files are index-aligned --
    // MeasureProject::execute and ApplyRules::execute both iterate project.files and push
    // exactly one entry per input file, in the same order (verified in both source files).
    for (std::size_t index = 0; index < model.files.size(); ++index)
    {
        Json fileDocument = fileJson(model.files[index]);

        // FileModel.path is rootPath-prefixed (see AnalyzeProject.cpp/ParseSourceFile.cpp), not
        // relative -- relativize here so the JSON stays portable across machines/invocations.
        fileDocument["path"] = std::filesystem::relative(model.files[index].path, model.rootPath).generic_string();

        fileDocument["metrics"] = Json::array();
        for (const codeqa::metrics::FunctionMetricsModel& function : metrics.files[index].functions)
        {
            fileDocument["metrics"].push_back(functionMetricsJson(function));
        }

        fileDocument["findings"] = Json::array();
        for (const codeqa::rules::Finding& finding : findings.files[index].findings)
        {
            fileDocument["findings"].push_back(findingJson(finding));
        }

        document["files"].push_back(std::move(fileDocument));
    }

    return document;
}

} // namespace codeqa::report
```

In `CMakeLists.txt`, add `src/report/ReportJson.cpp` to `add_library(codeqa_core STATIC ...)`, right after `src/report/FileJson.cpp`.

- [ ] **Step 4: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass, including the 2 new ones.

- [ ] **Step 5: Commit**

```bash
git add include/codeqa/report/ReportJson.h src/report/ReportJson.cpp tests/unit/ReportJsonTest.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add codeqa::report JSON schema wrapper (toReportJson)"
```

---

### Task 6: `codeqa analyze` CLI command

**Files:**
- Modify: `src/cli/RunApplication.cpp`
- Modify: `tests/unit/RunApplicationTest.cpp`

**Interfaces:**
- Consumes: `codeqa::application::RunAnalysis`/`RunAnalysisRequest`/`RunAnalysisResult` (Task 2), `codeqa::report::renderText` (Task 3), `codeqa::report::toReportJson` (Task 5), `codeqa::application::LoadProjectConfig`/`LoadProjectConfigResult` (Fase 8), `codeqa::config::defaultConfig()`/`ProjectConfig`/`HeaderOverrideConfig` (Fase 8), `codeqa::application::GetApplicationInfo` (existing), `codeqa::application::LanguageOverride` (existing).
- Produces: the `analyze` subcommand wired into `runApplication` — no new public C++ API (`RunApplication.h`'s `runApplication` signature is unchanged).

- [ ] **Step 1: Write the failing tests**

In `tests/unit/RunApplicationTest.cpp`, add `#include <nlohmann/json.hpp>` to the includes at the top of the file.

Add this helper right after `makeTempDiscoverRoot()` (still inside the anonymous namespace):

```cpp
std::filesystem::path makeTempAnalyzeRoot()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "codeqa_run_application_analyze_test";

    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::ofstream(root / "sample.c")
        << "int classify(int a, int b, int c, int d, int e)\n"
           "{\n"
           "    if (a == 0) { return 0; }\n"
           "    if (a == 1) { return 1; }\n"
           "    if (a == 2) { return 2; }\n"
           "    if (a == 3) { return 3; }\n"
           "    return -1;\n"
           "}\n";
    std::ofstream(root / "util.h") << "";

    return root;
}
```

Append these `TEST_CASE`s at the end of the file:

```cpp
TEST_CASE("analyze without a path returns CLI002")
{
    const RunResult result = run({"analyze"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI002") != std::string::npos);
}

TEST_CASE("analyze with a missing --config value returns CLI008")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const RunResult result = run({"analyze", root.string(), "--config"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI008") != std::string::npos);
}

TEST_CASE("analyze with a missing --format value returns CLI009")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const RunResult result = run({"analyze", root.string(), "--format"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI009") != std::string::npos);
}

TEST_CASE("analyze with an invalid --format value returns CLI010")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const RunResult result = run({"analyze", root.string(), "--format", "xml"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI010") != std::string::npos);
}

TEST_CASE("analyze with a missing --output value returns CLI011")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const RunResult result = run({"analyze", root.string(), "--output"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI011") != std::string::npos);
}

TEST_CASE("analyze with an unwritable --output path returns CLI012")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const std::filesystem::path unwritable = root / "no-such-directory" / "report.json";
    const RunResult result = run({"analyze", root.string(), "--output", unwritable.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI012") != std::string::npos);
}

TEST_CASE("analyze with an invalid --config file forwards the CONFIG error and returns InvalidArguments")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const std::filesystem::path configPath = root / "codeqa.json";
    std::ofstream(configPath) << "{ not json";

    const RunResult result = run({"analyze", root.string(), "--config", configPath.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CONFIG001") != std::string::npos);
}

TEST_CASE("analyze with default text format prints a report to stdout")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const RunResult result = run({"analyze", root.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("Code Quality Analysis Report") != std::string::npos);
    CHECK(result.output.find("signature.parameter-count") != std::string::npos);
}

TEST_CASE("analyze --format json --output writes a valid JSON report to a file")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const std::filesystem::path outputPath = root / "report.json";

    const RunResult result = run({"analyze", root.string(), "--format", "json", "--output", outputPath.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.empty());
    REQUIRE(std::filesystem::exists(outputPath));

    std::ifstream reportFile(outputPath);
    const nlohmann::json report = nlohmann::json::parse(reportFile);
    CHECK(report.at("schemaVersion").get<int>() == 1);
    CHECK(report.at("files").size() == 2);
}

TEST_CASE("analyze: a --lang-override wins over the same pattern's --config headerOverride")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const std::filesystem::path configPath = root / "codeqa.json";
    std::ofstream(configPath)
        << R"({ "languages": { "headerOverrides": [ { "pattern": "util.h", "language": "c" } ] } })";

    const RunResult result = run(
        {"analyze", root.string(), "--config", configPath.string(), "--lang-override", "util.h=cpp", "--format",
         "json"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));

    const nlohmann::json report = nlohmann::json::parse(result.output);
    bool found = false;
    for (const nlohmann::json& file : report.at("files"))
    {
        if (file.at("path").get<std::string>() == "util.h")
        {
            found = true;
            CHECK(file.at("language").get<std::string>() == "cpp");
        }
    }
    CHECK(found);
}
```

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails to compile or fails at runtime — `analyze` is not a recognized subcommand yet (falls through to `CLI001: Unknown option`).

- [ ] **Step 3: Add the `analyze` command**

In `src/cli/RunApplication.cpp`, add these includes (alongside the existing ones):

```cpp
#include "codeqa/application/GetApplicationInfo.h"
#include "codeqa/application/LoadProjectConfig.h"
#include "codeqa/application/RunAnalysis.h"
#include "codeqa/config/ConfigModels.h"
#include "codeqa/config/DefaultConfig.h"
#include "codeqa/core/EnumJson.h"
#include "codeqa/report/ReportJson.h"
#include "codeqa/report/TextReport.h"

#include <nlohmann/json.hpp>
#include <fstream>
```

(`codeqa/application/GetApplicationInfo.h` and `codeqa/core/EnumJson.h` are already included from Task 1 — do not duplicate the `#include` line if it is already there.)

In `printHelp`, extend the usage text. Replace:

```cpp
              "Usage:\n"
              "  codeqa [options]\n"
              "  codeqa discover <path> [--exclude <glob>]... [--lang-override <pattern>=<lang>]...\n"
              "  codeqa parse <path> [--exclude <glob>]... [--lang-override <pattern>=<lang>]...\n"
              "\n"
```

with:

```cpp
              "Usage:\n"
              "  codeqa [options]\n"
              "  codeqa discover <path> [--exclude <glob>]... [--lang-override <pattern>=<lang>]...\n"
              "  codeqa parse <path> [--exclude <glob>]... [--lang-override <pattern>=<lang>]...\n"
              "  codeqa analyze <path> [--exclude <glob>]... [--lang-override <pattern>=<lang>]...\n"
              "                        [--config <path>] [--format text|json] [--output <file>]\n"
              "\n"
```

And, right after the `--lang-override` line inside the "Commands:" section, add:

```cpp
              "  analyze <path>                     Run the full analysis pipeline (metrics +\n"
              "                                      rules) and print a report.\n"
              "      --config <path>                 Merge rule/language overrides from a\n"
              "                                       codeqa.json file over the defaults.\n"
              "      --format text|json               Report format (default: text).\n"
              "      --output <file>                  Write the report to a file instead of\n"
              "                                        stdout.\n"
```

Add the following right after `runParse` and before the closing `} // namespace` that ends the file's anonymous namespace (i.e., right before the line `} // namespace` that precedes `int runApplication(...)`):

```cpp
struct AnalyzeArguments
{
    std::filesystem::path path;
    std::vector<std::string> excludePatterns;
    std::vector<codeqa::application::LanguageOverride> languageOverrides;
    std::optional<std::filesystem::path> configPath;
    std::string format{"text"};
    std::optional<std::filesystem::path> outputPath;
};

struct AnalyzeArgumentsOutcome
{
    std::optional<AnalyzeArguments> arguments;
    int exitCode{static_cast<int>(ExitCode::Success)};
};

AnalyzeArgumentsOutcome parseAnalyzeArguments(const std::vector<std::string>& arguments, std::ostream& errorOutput)
{
    std::optional<std::filesystem::path> path;
    std::vector<std::string> excludePatterns;
    std::vector<codeqa::application::LanguageOverride> languageOverrides;
    std::optional<std::filesystem::path> configPath;
    std::string format = "text";
    std::optional<std::filesystem::path> outputPath;

    for (std::size_t index = 1; index < arguments.size(); ++index)
    {
        const std::string& token = arguments[index];

        if (token == "--exclude")
        {
            if (index + 1 >= arguments.size())
            {
                errorOutput << "Error [CLI004]: Missing value for '--exclude'.\n";
                return AnalyzeArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
            }

            excludePatterns.push_back(arguments[index + 1]);
            ++index;
            continue;
        }

        if (token == "--lang-override")
        {
            if (index + 1 >= arguments.size())
            {
                errorOutput << "Error [CLI006]: Missing value for '--lang-override'.\n";
                return AnalyzeArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
            }

            const std::string& value = arguments[index + 1];
            const std::size_t equalsPosition = value.find('=');

            if (equalsPosition == std::string::npos || equalsPosition == 0
                || equalsPosition == value.size() - 1)
            {
                errorOutput << "Error [CLI007]: Invalid --lang-override value '" << value
                               << "'. Expected <pattern>=c|cpp.\n";
                return AnalyzeArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
            }

            const std::string pattern = value.substr(0, equalsPosition);
            const std::string languageText = value.substr(equalsPosition + 1);
            const std::optional<codeqa::core::Language> language = parseLanguageName(languageText);

            if (!language.has_value())
            {
                errorOutput << "Error [CLI007]: Invalid --lang-override value '" << value
                               << "'. Expected <pattern>=c|cpp.\n";
                return AnalyzeArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
            }

            languageOverrides.push_back(codeqa::application::LanguageOverride{pattern, *language});
            ++index;
            continue;
        }

        if (token == "--config")
        {
            if (index + 1 >= arguments.size())
            {
                errorOutput << "Error [CLI008]: Missing value for '--config'.\n";
                return AnalyzeArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
            }

            configPath = std::filesystem::path{arguments[index + 1]};
            ++index;
            continue;
        }

        if (token == "--format")
        {
            if (index + 1 >= arguments.size())
            {
                errorOutput << "Error [CLI009]: Missing value for '--format'.\n";
                return AnalyzeArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
            }

            const std::string& value = arguments[index + 1];

            if (value != "text" && value != "json")
            {
                errorOutput << "Error [CLI010]: Invalid --format value '" << value
                               << "'. Expected 'text' or 'json'.\n";
                return AnalyzeArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
            }

            format = value;
            ++index;
            continue;
        }

        if (token == "--output")
        {
            if (index + 1 >= arguments.size())
            {
                errorOutput << "Error [CLI011]: Missing value for '--output'.\n";
                return AnalyzeArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
            }

            outputPath = std::filesystem::path{arguments[index + 1]};
            ++index;
            continue;
        }

        if (!path.has_value())
        {
            path = std::filesystem::path{token};
            continue;
        }

        errorOutput << "Error [CLI005]: Unexpected argument '" << token << "'.\n";
        return AnalyzeArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
    }

    if (!path.has_value())
    {
        errorOutput << "Error [CLI002]: Missing required argument <path> for 'analyze'.\n"
                       "Use 'codeqa --help' for usage.\n";
        return AnalyzeArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
    }

    std::error_code existsError;
    const bool isDirectory = std::filesystem::is_directory(*path, existsError);

    if (!isDirectory)
    {
        errorOutput << "Error [CLI003]: Path '" << path->string() << "' does not exist or is not a directory.\n";
        return AnalyzeArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
    }

    return AnalyzeArgumentsOutcome{
        AnalyzeArguments{*path, excludePatterns, languageOverrides, configPath, format, outputPath},
        static_cast<int>(ExitCode::Success)};
}

codeqa::application::LanguageOverride toLanguageOverride(const codeqa::config::HeaderOverrideConfig& headerOverride)
{
    return codeqa::application::LanguageOverride{headerOverride.pattern, headerOverride.language};
}

int runAnalyze(const std::vector<std::string>& arguments, std::ostream& output, std::ostream& errorOutput)
{
    const AnalyzeArgumentsOutcome outcome = parseAnalyzeArguments(arguments, errorOutput);

    if (!outcome.arguments.has_value())
    {
        return outcome.exitCode;
    }

    const AnalyzeArguments& parsedArguments = *outcome.arguments;

    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();

    if (parsedArguments.configPath.has_value())
    {
        const codeqa::application::LoadProjectConfigResult configResult =
            codeqa::application::LoadProjectConfig{}.execute(*parsedArguments.configPath);

        if (!configResult.config.has_value())
        {
            errorOutput << *configResult.error << '\n';
            return static_cast<int>(ExitCode::InvalidArguments);
        }

        config = *configResult.config;
    }

    codeqa::application::RunAnalysisRequest request;
    request.analysis.discovery.rootPath = parsedArguments.path;
    request.analysis.discovery.excludePatterns = parsedArguments.excludePatterns;

    for (const codeqa::config::HeaderOverrideConfig& headerOverride : config.headerOverrides)
    {
        request.analysis.languageOverrides.push_back(toLanguageOverride(headerOverride));
    }

    for (const codeqa::application::LanguageOverride& languageOverride : parsedArguments.languageOverrides)
    {
        request.analysis.languageOverrides.push_back(languageOverride);
    }

    request.config = config;

    const codeqa::application::RunAnalysisResult result = codeqa::application::RunAnalysis{}.execute(request);

    std::ofstream outputFile;
    std::ostream* destination = &output;

    if (parsedArguments.outputPath.has_value())
    {
        outputFile.open(*parsedArguments.outputPath);

        if (!outputFile)
        {
            errorOutput << "Error [CLI012]: Could not open '" << parsedArguments.outputPath->string()
                           << "' for writing.\n";
            return static_cast<int>(ExitCode::InvalidArguments);
        }

        destination = &outputFile;
    }

    if (parsedArguments.format == "json")
    {
        const codeqa::application::GetApplicationInfo getApplicationInfo;
        const std::string toolVersion = getApplicationInfo.execute().version;
        const nlohmann::json report =
            codeqa::report::toReportJson(result.model, result.metrics, result.findings, config, toolVersion);
        *destination << report.dump(2) << '\n';
    }
    else
    {
        codeqa::report::renderText(*destination, result.model, result.metrics, result.findings);
    }

    return static_cast<int>(ExitCode::Success);
}
```

Finally, add the dispatch branch in `runApplication`, right after the `parse` block:

```cpp
    if (firstArgument == "parse")
    {
        return runParse(arguments, output, errorOutput);
    }

    if (firstArgument == "analyze")
    {
        return runAnalyze(arguments, output, errorOutput);
    }
```

- [ ] **Step 4: Run to verify GREEN**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass, including the 9 new ones — and every pre-existing `RunApplicationTest.cpp` case (in particular `--help`'s output, since it now mentions `analyze`; the existing assertions only check for `"Usage:"`, so they still pass unchanged).

- [ ] **Step 5: Commit**

```bash
git add src/cli/RunApplication.cpp tests/unit/RunApplicationTest.cpp
git commit -m "feat: add codeqa analyze CLI command"
```

---

### Task 7: corpus integration test for `toReportJson`

**Files:**
- Create: `tests/unit/CorpusReportTest.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `codeqa::application::RunAnalysis` (Task 2), `codeqa::report::toReportJson` (Task 5), `codeqa::config::defaultConfig()` (Fase 8), the real corpus at `CODEQA_CORPUS_DIR` (already used by `CorpusFindingsTest.cpp`/`CorpusMetricsTest.cpp`).
- Produces: no new public API — this is a pure verification task, following the same golden-fixture spirit as `CorpusFindingsTest.cpp`, but asserting values directly instead of introducing a fourth per-fixture JSON file format (the existing `.findings.json`/`.metrics.json` already carry the exact values a hand-authored `.report.json` would just repeat).

- [ ] **Step 1: Write the failing test**

Create `tests/unit/CorpusReportTest.cpp`. The expected values below come directly from `tests/corpus/nesting/deep-nesting.findings.json` and `tests/corpus/nesting/deep-nesting.metrics.json` (both already part of the corpus, unchanged by this phase):

```cpp
#include "doctest/doctest.h"

#include "codeqa/application/RunAnalysis.h"
#include "codeqa/config/DefaultConfig.h"
#include "codeqa/report/ReportJson.h"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace
{

const nlohmann::json& fileEntryNamed(const nlohmann::json& document, const std::string& path)
{
    for (const nlohmann::json& file : document.at("files"))
    {
        if (file.at("path").get<std::string>() == path)
        {
            return file;
        }
    }

    throw std::runtime_error("Expected report file entry was not produced: " + path);
}

} // namespace

TEST_CASE("toReportJson over the real corpus reproduces the deep-nesting fixture's known finding and metric")
{
    codeqa::application::RunAnalysisRequest request;
    request.analysis.discovery.rootPath = std::filesystem::path{CODEQA_CORPUS_DIR};
    request.config = codeqa::config::defaultConfig();

    const codeqa::application::RunAnalysisResult result = codeqa::application::RunAnalysis{}.execute(request);
    const nlohmann::json document =
        codeqa::report::toReportJson(result.model, result.metrics, result.findings, request.config, "test");

    CHECK(document.at("schemaVersion").get<int>() == 1);

    const nlohmann::json& file = fileEntryNamed(document, "nesting/deep-nesting.cpp");
    CHECK(file.at("language").get<std::string>() == "cpp");

    bool foundMetric = false;
    for (const nlohmann::json& metric : file.at("metrics"))
    {
        if (metric.at("symbol").at("name").get<std::string>() == "fourLevels")
        {
            foundMetric = true;
            CHECK(metric.at("complexity").at("maxNestingDepth").get<int>() == 3);
        }
    }
    CHECK(foundMetric);

    REQUIRE(file.at("findings").size() == 1);
    const nlohmann::json& finding = file.at("findings")[0];
    CHECK(finding.at("ruleId").get<std::string>() == "complexity.nesting");
    CHECK(finding.at("severity").get<std::string>() == "warning");
    CHECK(finding.at("symbolName").get<std::string>() == "fourLevels");
    CHECK(finding.at("observedValue").get<int>() == 3);
    CHECK(finding.at("limitValue").get<int>() == 3);
    CHECK(finding.at("location").at("line").get<int>() == 26);
    CHECK(finding.at("location").at("column").get<int>() == 17);
}
```

Add `unit/CorpusReportTest.cpp` to `tests/CMakeLists.txt`, right after `unit/CorpusFindingsTest.cpp` (grouping it with the other corpus tests rather than at the very end of the list).

- [ ] **Step 2: Run to verify RED**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
```

Expected: fails — `tests/unit/CorpusReportTest.cpp` is not yet registered / does not exist before this step, so the build fails until it is added and the CMake file is regenerated.

Actually run it after adding the file and the CMake entry:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe --test-case="toReportJson over the real corpus*"
```

Expected: PASS immediately — this task adds no new production code, only a test exercising Tasks 2 and 5 together over real fixtures. If it fails, the bug is in Task 2 or Task 5, not here; re-check those tasks' implementations against this task's assertions before changing this test.

- [ ] **Step 3: Run the full suite to verify no regressions**

Run:

```text
cmake --build build --config Debug --target codeqa_tests
build/tests/Debug/codeqa_tests.exe
```

Expected: all tests pass.

- [ ] **Step 4: Commit**

```bash
git add tests/unit/CorpusReportTest.cpp tests/CMakeLists.txt
git commit -m "test: verify toReportJson against the corpus's deep-nesting fixture"
```
