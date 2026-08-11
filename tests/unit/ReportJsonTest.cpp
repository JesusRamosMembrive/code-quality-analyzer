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
