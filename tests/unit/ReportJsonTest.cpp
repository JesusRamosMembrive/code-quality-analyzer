#include "doctest/doctest.h"

#include "codeqa/config/DefaultConfig.h"
#include "codeqa/duplication/DuplicationModels.h"
#include "codeqa/report/ReportJson.h"

#include <filesystem>
#include <stdexcept>

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
    const nlohmann::json document = codeqa::report::toReportJson(model, metrics, findings, codeqa::duplication::ProjectDuplicationModel{}, codeqa::architecture::ProjectArchitectureModel{}, config, "9.9.9");

    CHECK(document.at("schemaVersion").get<int>() == 1);
    CHECK(document.at("metadata").at("rootPath").get<std::string>() == model.rootPath.generic_string());
    CHECK(document.at("metadata").at("toolVersion").get<std::string>() == "9.9.9");
    CHECK(document.at("config").at("rules").size() == 12);

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
        codeqa::report::toReportJson(model, metrics, findings, codeqa::duplication::ProjectDuplicationModel{}, codeqa::architecture::ProjectArchitectureModel{}, codeqa::config::defaultConfig(), "0.0.0");

    CHECK(document.at("files")[0].at("findings")[0].at("observedValue").is_null());
    CHECK(document.at("files")[0].at("findings")[0].at("limitValue").is_null());
}

TEST_CASE("toReportJson throws when metrics or findings are not index-aligned with model.files")
{
    codeqa::model::ProjectModel model;
    model.rootPath = std::filesystem::path{"project"};
    codeqa::model::FileModel file;
    file.path = model.rootPath / "src" / "sample.cpp";
    model.files.push_back(file);

    // metrics.files is left empty, so its size (0) mismatches model.files.size() (1).
    codeqa::metrics::ProjectMetricsModel metrics;

    codeqa::rules::ProjectFindingsModel findings;
    codeqa::rules::FileFindingsModel fileFindings;
    fileFindings.path = file.path;
    findings.files.push_back(fileFindings);

    auto callToReportJson = [&]() {
        (void)codeqa::report::toReportJson(model, metrics, findings, codeqa::duplication::ProjectDuplicationModel{}, codeqa::architecture::ProjectArchitectureModel{}, codeqa::config::defaultConfig(), "0.0.0");
    };
    CHECK_THROWS_AS(callToReportJson(), std::invalid_argument);
}

TEST_CASE("toReportJson includes a duplicates section with relativized paths")
{
    codeqa::model::ProjectModel model;
    model.rootPath = std::filesystem::path{"project"};
    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::rules::ProjectFindingsModel findings;

    codeqa::duplication::ProjectDuplicationModel duplicates;
    codeqa::duplication::DuplicateGroup group;
    group.members.push_back(codeqa::duplication::DuplicateMember{
        "first",
        codeqa::core::SourceRange{
            codeqa::core::SourceLocation{model.rootPath / "src" / "a.cpp", 1, 1, 0},
            codeqa::core::SourceLocation{model.rootPath / "src" / "a.cpp", 4, 1, 40}}});
    group.members.push_back(codeqa::duplication::DuplicateMember{
        "second",
        codeqa::core::SourceRange{
            codeqa::core::SourceLocation{model.rootPath / "src" / "b.cpp", 1, 1, 0},
            codeqa::core::SourceLocation{model.rootPath / "src" / "b.cpp", 4, 1, 40}}});
    duplicates.groups.push_back(group);

    const codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    const nlohmann::json document =
        codeqa::report::toReportJson(model, metrics, findings, duplicates, codeqa::architecture::ProjectArchitectureModel{}, config, "9.9.9");

    REQUIRE(document.at("duplicates").size() == 1);
    const nlohmann::json& groupDocument = document.at("duplicates")[0];
    REQUIRE(groupDocument.at("members").size() == 2);
    CHECK(groupDocument.at("members")[0].at("qualifiedName").get<std::string>() == "first");
    CHECK(groupDocument.at("members")[0].at("path").get<std::string>() == "src/a.cpp");
    CHECK(groupDocument.at("members")[1].at("path").get<std::string>() == "src/b.cpp");
}

TEST_CASE("toReportJson emits an empty duplicates array when there are no groups")
{
    codeqa::model::ProjectModel model;
    model.rootPath = std::filesystem::path{"project"};
    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::rules::ProjectFindingsModel findings;
    const codeqa::duplication::ProjectDuplicationModel duplicates;

    const nlohmann::json document = codeqa::report::toReportJson(
        model, metrics, findings, duplicates, codeqa::architecture::ProjectArchitectureModel{}, codeqa::config::defaultConfig(), "9.9.9");

    CHECK(document.at("duplicates").empty());
}

TEST_CASE("toReportJson includes an architecture section with relativized paths")
{
    codeqa::model::ProjectModel model;
    model.rootPath = std::filesystem::path{"project"};
    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::rules::ProjectFindingsModel findings;
    const codeqa::duplication::ProjectDuplicationModel duplicates;

    codeqa::architecture::ProjectArchitectureModel architecture;
    architecture.files.push_back(codeqa::architecture::FileDependencyMetrics{
        model.rootPath / "src" / "a.h", 2, 1});
    architecture.cycles.push_back(codeqa::architecture::DependencyCycle{
        {model.rootPath / "src" / "a.h", model.rootPath / "src" / "b.h"}});
    architecture.fanOutFindings.push_back(codeqa::architecture::FanOutFinding{
        model.rootPath / "src" / "a.h", 12, codeqa::core::DiagnosticSeverity::Warning});
    architecture.externalOrUnresolvedIncludes = 3;
    architecture.ambiguousIncludes = 1;

    const nlohmann::json document = codeqa::report::toReportJson(
        model, metrics, findings, duplicates, architecture, codeqa::config::defaultConfig(), "9.9.9");

    REQUIRE(document.at("architecture").at("files").size() == 1);
    CHECK(document.at("architecture").at("files")[0].at("path").get<std::string>() == "src/a.h");
    CHECK(document.at("architecture").at("files")[0].at("fanIn").get<int>() == 2);
    REQUIRE(document.at("architecture").at("cycles").size() == 1);
    CHECK(document.at("architecture").at("cycles")[0].at("files")[0].get<std::string>() == "src/a.h");
    REQUIRE(document.at("architecture").at("fanOutFindings").size() == 1);
    CHECK(document.at("architecture").at("fanOutFindings")[0].at("severity").get<std::string>() == "warning");
    CHECK(document.at("architecture").at("externalOrUnresolvedIncludes").get<int>() == 3);
    CHECK(document.at("architecture").at("ambiguousIncludes").get<int>() == 1);
}

TEST_CASE("toReportJson emits empty architecture arrays when there is nothing to report")
{
    codeqa::model::ProjectModel model;
    model.rootPath = std::filesystem::path{"project"};
    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::rules::ProjectFindingsModel findings;
    const codeqa::duplication::ProjectDuplicationModel duplicates;
    const codeqa::architecture::ProjectArchitectureModel architecture;

    const nlohmann::json document = codeqa::report::toReportJson(
        model, metrics, findings, duplicates, architecture, codeqa::config::defaultConfig(), "9.9.9");

    CHECK(document.at("architecture").at("files").empty());
    CHECK(document.at("architecture").at("cycles").empty());
    CHECK(document.at("architecture").at("fanOutFindings").empty());
}
