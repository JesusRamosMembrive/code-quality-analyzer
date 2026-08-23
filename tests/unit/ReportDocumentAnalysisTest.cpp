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

TEST_CASE("toAnalysisResult carries the duplicates model through unchanged")
{
    codeqa::application::ReportDocument document;
    document.metadata.rootPath = "project";

    codeqa::duplication::DuplicateGroup group;
    group.members.push_back(codeqa::duplication::DuplicateMember{"a::run", codeqa::core::SourceRange{}});
    group.members.push_back(codeqa::duplication::DuplicateMember{"b::run", codeqa::core::SourceRange{}});
    document.duplicates.groups.push_back(group);

    const codeqa::application::RunAnalysisResult result = codeqa::application::toAnalysisResult(document);

    REQUIRE(result.duplicates.groups.size() == 1);
    CHECK(result.duplicates.groups.front().members.size() == 2);
}

TEST_CASE("toAnalysisResult absolutizes architecture paths read from a document")
{
    codeqa::application::ReportDocument document;
    document.metadata.rootPath = "C:/project";
    document.architecture.files.push_back(
        codeqa::architecture::FileDependencyMetrics{std::filesystem::path{"src"} / "a.h", 1, 0});
    document.architecture.cycles.push_back(codeqa::architecture::DependencyCycle{
        {std::filesystem::path{"src"} / "a.h", std::filesystem::path{"src"} / "b.h"}});
    document.architecture.fanOutFindings.push_back(codeqa::architecture::FanOutFinding{
        std::filesystem::path{"src"} / "a.h", 5, codeqa::core::DiagnosticSeverity::Warning});
    document.architecture.forbiddenDependencyViolations.push_back(codeqa::architecture::ForbiddenDependencyViolation{
        std::filesystem::path{"src"} / "core" / "a.h", "core", std::filesystem::path{"src"} / "desktop" / "b.h",
        "desktop"});

    const codeqa::application::RunAnalysisResult analysis = codeqa::application::toAnalysisResult(document);

    REQUIRE(analysis.architecture.files.size() == 1);
    CHECK(analysis.architecture.files[0].path == std::filesystem::path{"C:/project"} / "src" / "a.h");
    REQUIRE(analysis.architecture.cycles.size() == 1);
    CHECK(analysis.architecture.cycles[0].files[0] == std::filesystem::path{"C:/project"} / "src" / "a.h");
    REQUIRE(analysis.architecture.fanOutFindings.size() == 1);
    CHECK(analysis.architecture.fanOutFindings[0].path == std::filesystem::path{"C:/project"} / "src" / "a.h");
    REQUIRE(analysis.architecture.forbiddenDependencyViolations.size() == 1);
    CHECK(
        analysis.architecture.forbiddenDependencyViolations[0].fromPath
        == std::filesystem::path{"C:/project"} / "src" / "core" / "a.h");
    CHECK(analysis.architecture.forbiddenDependencyViolations[0].fromLayer == "core");
    CHECK(
        analysis.architecture.forbiddenDependencyViolations[0].toPath
        == std::filesystem::path{"C:/project"} / "src" / "desktop" / "b.h");
    CHECK(analysis.architecture.forbiddenDependencyViolations[0].toLayer == "desktop");
}
