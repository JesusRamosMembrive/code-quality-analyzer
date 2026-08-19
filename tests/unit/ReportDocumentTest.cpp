#include "doctest/doctest.h"

#include "codeqa/application/ReportDocumentAnalysis.h"
#include "codeqa/application/ReportDocumentFactory.h"
#include "codeqa/architecture/ArchitectureModels.h"
#include "codeqa/config/DefaultConfig.h"
#include "codeqa/duplication/DuplicationModels.h"
#include "codeqa/report/ReportJson.h"

#include <filesystem>
#include <fstream>

namespace
{

nlohmann::json makeCompleteReportJson()
{
    codeqa::model::ProjectModel model;
    model.rootPath = "project";

    codeqa::model::FileModel file;
    file.path = model.rootPath / "src" / "sample.cpp";
    file.language = codeqa::core::Language::Cpp;

    codeqa::core::Diagnostic diagnostic;
    diagnostic.code = "PARSE001";
    diagnostic.severity = codeqa::core::DiagnosticSeverity::Warning;
    diagnostic.message = "recovered parse error";
    diagnostic.location.line = 3;
    diagnostic.location.column = 5;
    file.diagnostics.push_back(diagnostic);

    codeqa::model::CallableModel callable;
    callable.symbol.name = "run";
    callable.symbol.qualifiedName = "sample::run";
    callable.symbol.range.begin.line = 2;
    callable.symbol.range.begin.column = 1;
    callable.symbol.range.end.line = 8;
    callable.symbol.range.end.column = 1;
    file.callables.push_back(callable);
    model.files.push_back(file);

    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::metrics::FileMetricsModel fileMetrics;
    fileMetrics.path = file.path;
    codeqa::metrics::FunctionMetricsModel functionMetrics;
    functionMetrics.symbol = callable.symbol;
    functionMetrics.size.physicalLines = 7;
    functionMetrics.signature.parameterCount = 2;
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
    finding.location.line = 2;
    finding.location.column = 1;
    finding.symbolName = "run";
    fileFindings.findings.push_back(finding);
    findings.files.push_back(fileFindings);

    return codeqa::report::toReportJson(
        model, metrics, findings, codeqa::duplication::ProjectDuplicationModel{}, codeqa::architecture::ProjectArchitectureModel{}, codeqa::config::defaultConfig(),
        "9.9.9");
}

std::filesystem::path temporaryReportPath()
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "codeqa-report-document-test.json";
    std::error_code error;
    std::filesystem::remove(path, error);
    return path;
}

} // namespace

TEST_CASE("parseReportDocument accepts a schema-version-1 CodeQA report")
{
    nlohmann::json report = makeCompleteReportJson();
    report["futureVersionOneField"] = "tolerated";

    const auto result = codeqa::application::parseReportDocument(report);

    REQUIRE(result.document.has_value());
    CHECK(result.document->metadata.toolVersion == "9.9.9");
    CHECK(result.document->files.front().path.generic_string() == "src/sample.cpp");
    CHECK(result.document->files.front().diagnostics.front().code == "PARSE001");
    CHECK(result.document->files.front().metrics.front().complexity.cyclomaticComplexity == 12);
    CHECK(result.document->files.front().findings.front().ruleId == "complexity.cyclomatic");
    CHECK(result.document->files.front().suppressedFindings.empty());
    CHECK(result.document->qualityGateOutcome == codeqa::application::QualityGateOutcome::NotEvaluated);
}

TEST_CASE("parseReportDocument rejects an unsupported schema version")
{
    nlohmann::json report = makeCompleteReportJson();
    report["schemaVersion"] = 2;

    const auto result = codeqa::application::parseReportDocument(report);

    CHECK_FALSE(result.document.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("REPORT003") != std::string::npos);
    CHECK(result.error->find("schemaVersion 2") != std::string::npos);
}

TEST_CASE("parseReportDocument rejects a missing required report field")
{
    nlohmann::json report = makeCompleteReportJson();
    report["files"][0].erase("findings");

    const auto result = codeqa::application::parseReportDocument(report);

    CHECK_FALSE(result.document.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("REPORT004") != std::string::npos);
}

TEST_CASE("parseReportDocument rejects a wrongly typed report field")
{
    nlohmann::json report = makeCompleteReportJson();
    report["files"][0]["path"] = 42;

    const auto result = codeqa::application::parseReportDocument(report);

    CHECK_FALSE(result.document.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("REPORT004") != std::string::npos);
}

TEST_CASE("parseReportDocument rejects an invalid embedded config")
{
    nlohmann::json report = makeCompleteReportJson();
    report["config"]["unknown"] = true;

    const auto result = codeqa::application::parseReportDocument(report);

    CHECK_FALSE(result.document.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("REPORT004") != std::string::npos);
}

TEST_CASE("loadReportDocument distinguishes unreadable and malformed reports")
{
    const auto unreadable = codeqa::application::loadReportDocument(temporaryReportPath());
    CHECK_FALSE(unreadable.document.has_value());
    REQUIRE(unreadable.error.has_value());
    CHECK(unreadable.error->find("REPORT001") != std::string::npos);

    const std::filesystem::path path = temporaryReportPath();
    {
        std::ofstream output(path);
        output << "{ invalid";
    }

    const auto malformed = codeqa::application::loadReportDocument(path);
    std::error_code error;
    std::filesystem::remove(path, error);

    CHECK_FALSE(malformed.document.has_value());
    REQUIRE(malformed.error.has_value());
    CHECK(malformed.error->find("REPORT002") != std::string::npos);
}

TEST_CASE("makeReportDocument maps fresh analysis without a JSON round trip")
{
    codeqa::application::RunAnalysisResult analysis;
    analysis.model.rootPath = "project";
    codeqa::model::FileModel file;
    file.path = analysis.model.rootPath / "src" / "sample.cpp";
    file.language = codeqa::core::Language::Cpp;

    codeqa::core::Diagnostic diagnostic;
    diagnostic.code = "PARSE001";
    diagnostic.location.line = 3;
    file.diagnostics.push_back(diagnostic);
    analysis.model.files.push_back(file);

    codeqa::metrics::FileMetricsModel metrics;
    metrics.path = file.path;
    codeqa::metrics::FunctionMetricsModel functionMetrics;
    functionMetrics.symbol.name = "run";
    functionMetrics.complexity.cyclomaticComplexity = 12;
    metrics.functions.push_back(functionMetrics);
    analysis.metrics.files.push_back(metrics);

    codeqa::rules::FileFindingsModel findings;
    findings.path = file.path;
    codeqa::rules::Finding finding;
    finding.ruleId = "complexity.cyclomatic";
    finding.observedValue = 12;
    findings.findings.push_back(finding);
    codeqa::rules::Finding suppressed;
    suppressed.ruleId = "signature.parameter-count";
    suppressed.symbolName = "run";
    suppressed.location.line = 4;
    findings.suppressedFindings.push_back(suppressed);
    analysis.findings.files.push_back(findings);

    const codeqa::application::ReportComparison comparison{"20260814T101112Z", 3, 4, 5};
    const auto document = codeqa::application::makeReportDocument(
        analysis,
        codeqa::config::defaultConfig(),
        "9.9.9",
        codeqa::application::QualityGateOutcome::Passed,
        comparison);

    CHECK(document.metadata.rootPath == "project");
    CHECK(document.files.front().path.generic_string() == "src/sample.cpp");
    CHECK(document.files.front().diagnostics.front().location.filePath.generic_string() == "src/sample.cpp");
    CHECK(document.files.front().metrics.front().complexity.cyclomaticComplexity == 12);
    CHECK(document.files.front().findings.front().ruleId == "complexity.cyclomatic");
    CHECK(document.files.front().suppressedFindings.front().ruleId == "signature.parameter-count");
    CHECK(document.files.front().suppressedFindings.front().location.filePath.generic_string() == "src/sample.cpp");
    CHECK(document.comparison->newCount == 3);
    CHECK(document.qualityGateOutcome == codeqa::application::QualityGateOutcome::Passed);
}

TEST_CASE("parseReportDocument round-trips a duplicate group with its members")
{
    nlohmann::json report = makeCompleteReportJson();

    nlohmann::json member1;
    member1["qualifiedName"] = "sample::run";
    member1["path"] = "src/sample.cpp";
    member1["range"]["begin"] = {{"line", 2}, {"column", 1}};
    member1["range"]["end"] = {{"line", 8}, {"column", 1}};

    nlohmann::json member2;
    member2["qualifiedName"] = "sample::runAgain";
    member2["path"] = "src/other.cpp";
    member2["range"]["begin"] = {{"line", 4}, {"column", 1}};
    member2["range"]["end"] = {{"line", 10}, {"column", 1}};

    nlohmann::json group;
    group["members"] = nlohmann::json::array({member1, member2});

    report["duplicates"] = nlohmann::json::array({group});

    const auto result = codeqa::application::parseReportDocument(report);

    REQUIRE(result.document.has_value());
    REQUIRE(result.document->duplicates.groups.size() == 1);
    REQUIRE(result.document->duplicates.groups.front().members.size() == 2);
    CHECK(result.document->duplicates.groups.front().members[0].qualifiedName == "sample::run");
    CHECK(result.document->duplicates.groups.front().members[1].qualifiedName == "sample::runAgain");

    const codeqa::application::RunAnalysisResult analysis =
        codeqa::application::toAnalysisResult(*result.document);

    REQUIRE(analysis.duplicates.groups.size() == 1);
    REQUIRE(analysis.duplicates.groups.front().members.size() == 2);
    const std::filesystem::path expectedPath = std::filesystem::path{"project"} / "src" / "sample.cpp";
    CHECK(analysis.duplicates.groups.front().members[0].range.begin.filePath == expectedPath);
    CHECK(analysis.duplicates.groups.front().members[0].range.begin.line == 2);
}

TEST_CASE("parseReportDocument defaults to no duplicate groups when the field is absent (pre-existing reports)")
{
    nlohmann::json report = makeCompleteReportJson();
    report.erase("duplicates");

    const auto result = codeqa::application::parseReportDocument(report);

    REQUIRE(result.document.has_value());
    CHECK(result.document->duplicates.groups.empty());
}

TEST_CASE("makeReportDocument and parseReportDocument round-trip architecture data")
{
    codeqa::application::RunAnalysisResult analysis;
    analysis.model.rootPath = std::filesystem::path{"project"};

    analysis.architecture.files.push_back(codeqa::architecture::FileDependencyMetrics{
        analysis.model.rootPath / "src" / "a.h", 2, 1});
    analysis.architecture.cycles.push_back(codeqa::architecture::DependencyCycle{
        {analysis.model.rootPath / "src" / "a.h", analysis.model.rootPath / "src" / "b.h"}});
    analysis.architecture.fanOutFindings.push_back(codeqa::architecture::FanOutFinding{
        analysis.model.rootPath / "src" / "a.h", 12, codeqa::core::DiagnosticSeverity::Warning});
    analysis.architecture.externalOrUnresolvedIncludes = 2;
    analysis.architecture.ambiguousIncludes = 1;

    const codeqa::application::ReportDocument document = codeqa::application::makeReportDocument(
        analysis, codeqa::config::defaultConfig(), "9.9.9", codeqa::application::QualityGateOutcome::Passed,
        std::nullopt);

    REQUIRE(document.architecture.files.size() == 1);
    CHECK(document.architecture.files[0].path == analysis.model.rootPath / "src" / "a.h");
    REQUIRE(document.architecture.cycles.size() == 1);
    CHECK(document.architecture.fanOutFindings.size() == 1);
    CHECK(document.architecture.externalOrUnresolvedIncludes == 2);
    CHECK(document.architecture.ambiguousIncludes == 1);

    const nlohmann::json json = codeqa::report::toReportJson(
        analysis.model, analysis.metrics, analysis.findings, analysis.duplicates, analysis.architecture,
        codeqa::config::defaultConfig(), "9.9.9");

    const codeqa::application::ReportDocumentResult parsed = codeqa::application::parseReportDocument(json);

    REQUIRE(parsed.document.has_value());
    REQUIRE(parsed.document->architecture.files.size() == 1);
    CHECK(parsed.document->architecture.files[0].path == std::filesystem::path{"src"} / "a.h");
    REQUIRE(parsed.document->architecture.cycles.size() == 1);
    REQUIRE(parsed.document->architecture.fanOutFindings.size() == 1);
    CHECK(parsed.document->architecture.fanOutFindings[0].severity == codeqa::core::DiagnosticSeverity::Warning);
    CHECK(parsed.document->architecture.externalOrUnresolvedIncludes == 2);
    CHECK(parsed.document->architecture.ambiguousIncludes == 1);
}
