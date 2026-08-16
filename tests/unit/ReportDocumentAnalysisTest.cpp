#include "doctest/doctest.h"

#include "codeqa/application/ReportDocumentAnalysis.h"

TEST_CASE("toAnalysisResult converts a ReportDocument into a parallel model/metrics/findings triplet")
{
    codeqa::application::ReportDocument document;
    document.metadata.rootPath = "project";

    codeqa::application::ReportFile file;
    file.path = "src/a.cpp";
    file.language = codeqa::core::Language::Cpp;
    file.state = codeqa::model::AnalysisState::Complete;

    codeqa::core::Diagnostic diagnostic;
    diagnostic.code = "PARSE001";
    diagnostic.severity = codeqa::core::DiagnosticSeverity::Error;
    diagnostic.message = "unexpected token";
    file.diagnostics.push_back(diagnostic);

    codeqa::metrics::FunctionMetricsModel metric;
    metric.symbol.name = "run";
    file.metrics.push_back(metric);

    codeqa::rules::Finding finding;
    finding.ruleId = "complexity.cyclomatic";
    finding.severity = codeqa::core::DiagnosticSeverity::Warning;
    finding.symbolName = "run";
    file.findings.push_back(finding);

    document.files.push_back(file);

    const codeqa::application::RunAnalysisResult result = codeqa::application::toAnalysisResult(document);

    REQUIRE(result.model.files.size() == 1);
    REQUIRE(result.metrics.files.size() == 1);
    REQUIRE(result.findings.files.size() == 1);

    const std::filesystem::path expectedPath = std::filesystem::path{"project"} / "src" / "a.cpp";
    CHECK(result.model.rootPath == "project");
    CHECK(result.model.files.front().path == expectedPath);
    CHECK(result.model.files.front().language == codeqa::core::Language::Cpp);
    CHECK(result.model.files.front().state == codeqa::model::AnalysisState::Complete);
    CHECK(result.model.files.front().diagnostics.size() == 1);
    CHECK(result.model.files.front().diagnostics.front().code == "PARSE001");

    CHECK(result.metrics.files.front().path == expectedPath);
    CHECK(result.metrics.files.front().functions.size() == 1);
    CHECK(result.metrics.files.front().functions.front().symbol.name == "run");

    CHECK(result.findings.files.front().path == expectedPath);
    CHECK(result.findings.files.front().findings.size() == 1);
    CHECK(result.findings.files.front().findings.front().ruleId == "complexity.cyclomatic");
}
