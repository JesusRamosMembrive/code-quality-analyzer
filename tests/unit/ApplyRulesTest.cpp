#include "doctest/doctest.h"

#include "codeqa/application/AnalyzeProject.h"
#include "codeqa/application/ApplyRules.h"
#include "codeqa/application/LoadProjectConfig.h"
#include "codeqa/application/MeasureProject.h"
#include "codeqa/config/DefaultConfig.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{

class TempProject
{
public:
    TempProject()
        : root_(std::filesystem::temp_directory_path() / "codeqa_apply_rules_test")
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

bool hasFinding(const std::vector<codeqa::rules::Finding>& findings, const std::string& ruleId)
{
    return std::any_of(
        findings.begin(), findings.end(),
        [&ruleId](const codeqa::rules::Finding& finding) { return finding.ruleId == ruleId; });
}

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

} // namespace

TEST_CASE("ApplyRules generates findings for a function that crosses several thresholds")
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

    const codeqa::rules::ProjectFindingsModel findings = analyzeAndApplyRules(fixture.root());

    REQUIRE(findings.files.size() == 1);
    const std::vector<codeqa::rules::Finding>& fileFindings = findings.files.front().findings;
    CHECK(hasFinding(fileFindings, "signature.parameter-count"));
    CHECK(hasFinding(fileFindings, "complexity.returns"));
}

TEST_CASE("ApplyRules produces no findings for a function whose range overlaps a parse error")
{
    const TempProject fixture;
    fixture.addFile(
        "broken.cpp",
        "int recovered() { int stable = 1; return stable; }\n"
        "int broken(int a, int b, int c, int d, int e) { int value = ; }\n");

    const codeqa::rules::ProjectFindingsModel findings = analyzeAndApplyRules(fixture.root());

    REQUIRE(findings.files.size() == 1);
    CHECK(findings.files.front().findings.empty());
}

TEST_CASE("ApplyRules produces no findings for a function whose range overlaps a PARSE004 diagnostic")
{
    const std::filesystem::path rootPath{"project"};
    const std::filesystem::path filePath = rootPath / "corrupted.c";

    codeqa::core::SourceRange range;
    range.begin = codeqa::core::SourceLocation{filePath, 1, 1, 0};
    range.end = codeqa::core::SourceLocation{filePath, 20, 2, 500};

    codeqa::model::ProjectModel project;
    project.rootPath = rootPath;

    codeqa::model::FileModel file;
    file.path = filePath;
    file.language = codeqa::core::Language::C;
    file.state = codeqa::model::AnalysisState::Partial;
    file.diagnostics.push_back(codeqa::core::Diagnostic{
        "PARSE004", codeqa::core::DiagnosticSeverity::Error, "Function or type range appears corrupted.",
        range.begin});

    codeqa::model::CallableModel callable;
    callable.symbol = codeqa::model::SymbolIdentity{"broken", "broken", range};
    callable.kind = codeqa::model::CallableKind::FreeFunction;
    file.callables.push_back(callable);
    project.files.push_back(file);

    codeqa::metrics::ProjectMetricsModel metrics;
    metrics.rootPath = rootPath;
    codeqa::metrics::FileMetricsModel fileMetrics;
    fileMetrics.path = filePath;
    codeqa::metrics::FunctionMetricsModel functionMetrics;
    functionMetrics.symbol = callable.symbol;
    functionMetrics.signature.parameterCount = 10; // guarantees signature.parameter-count would
                                                     // fire (default warning=4) if this callable
                                                     // were NOT filtered -- proves the emptiness
                                                     // below is really due to the PARSE004 overlap
                                                     // check, not a metrics-lookup miss.
    fileMetrics.functions.push_back(functionMetrics);
    metrics.files.push_back(fileMetrics);

    const codeqa::rules::ProjectFindingsModel findings =
        codeqa::application::ApplyRules{}.execute(project, metrics, codeqa::config::defaultConfig());

    REQUIRE(findings.files.size() == 1);
    CHECK(findings.files.front().findings.empty());
}

TEST_CASE("ApplyRules suppresses a finding with a matching inline disable comment")
{
    const TempProject fixture;
    fixture.addFile(
        "suppressed.c",
        "// codeqa-disable-next-line signature.parameter-count\n"
        "int manyParams(int a, int b, int c, int d, int e)\n"
        "{\n"
        "    return a;\n"
        "}\n");

    const codeqa::rules::ProjectFindingsModel findings = analyzeAndApplyRules(fixture.root());

    REQUIRE(findings.files.size() == 1);
    CHECK_FALSE(hasFinding(findings.files.front().findings, "signature.parameter-count"));
    CHECK(hasFinding(findings.files.front().suppressedFindings, "signature.parameter-count"));
}

TEST_CASE("ApplyRules does not suppress a finding when the comment names a different rule")
{
    const TempProject fixture;
    fixture.addFile(
        "not-suppressed.c",
        "// codeqa-disable-next-line complexity.cyclomatic\n"
        "int manyParams(int a, int b, int c, int d, int e)\n"
        "{\n"
        "    return a;\n"
        "}\n");

    const codeqa::rules::ProjectFindingsModel findings = analyzeAndApplyRules(fixture.root());

    REQUIRE(findings.files.size() == 1);
    CHECK(hasFinding(findings.files.front().findings, "signature.parameter-count"));
    CHECK(findings.files.front().suppressedFindings.empty());
}

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

TEST_CASE("LoadProjectConfig output flows end-to-end into ApplyRules")
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
    fixture.addFile(
        "codeqa.json",
        R"({ "rules": { "signature.parameter-count": { "enabled": false, "params": { "warning": 4, "high": 7 } } } })");

    const codeqa::application::LoadProjectConfigResult loaded =
        codeqa::application::LoadProjectConfig{}.execute(fixture.root() / "codeqa.json");
    REQUIRE(loaded.config.has_value());
    CHECK_FALSE(loaded.error.has_value());

    const codeqa::rules::ProjectFindingsModel findings = analyzeAndApplyRules(fixture.root(), *loaded.config);

    REQUIRE(findings.files.size() == 1);
    CHECK_FALSE(hasFinding(findings.files.front().findings, "signature.parameter-count"));
    CHECK(hasFinding(findings.files.front().findings, "complexity.returns"));
}
