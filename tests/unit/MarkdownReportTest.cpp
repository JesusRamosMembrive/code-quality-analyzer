#include "doctest/doctest.h"

#include "codeqa/report/MarkdownReport.h"

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

TEST_CASE("renderMarkdown with no diagnostics or findings reports a zero summary and omits file sections")
{
    codeqa::model::ProjectModel model;
    model.rootPath = "project";
    codeqa::model::FileModel file;
    file.path = model.rootPath / "src" / "clean.cpp";
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
    codeqa::report::renderMarkdown(output, model, metrics, findings);
    const std::string text = output.str();

    CHECK(text.find("# Code Quality Analysis Report") != std::string::npos);
    CHECK(text.find("| Files analyzed | 1 (C: 0, C++: 1, unavailable: 0) |") != std::string::npos);
    CHECK(text.find("| Functions measured | 0 |") != std::string::npos);
    CHECK(text.find("| Diagnostics | 0 (errors: 0, warnings: 0, information: 0) |") != std::string::npos);
    CHECK(text.find("| Findings | 0 (errors: 0, warnings: 0) |") != std::string::npos);
    CHECK(text.find("Result: no findings.") != std::string::npos);
    CHECK(text.find("## src/clean.cpp") == std::string::npos);
}

TEST_CASE("renderMarkdown lists findings as checkboxes grouped by file, in file order")
{
    codeqa::model::ProjectModel model;
    model.rootPath = "project";
    codeqa::model::FileModel fileA;
    fileA.path = model.rootPath / "src" / "a.cpp";
    fileA.language = codeqa::core::Language::Cpp;
    codeqa::model::FileModel fileB;
    fileB.path = model.rootPath / "src" / "b.cpp";
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
    codeqa::report::renderMarkdown(output, model, metrics, findings);
    const std::string text = output.str();

    const std::size_t sectionA = text.find("## src/a.cpp");
    const std::size_t sectionB = text.find("## src/b.cpp");
    REQUIRE(sectionA != std::string::npos);
    REQUIRE(sectionB != std::string::npos);
    CHECK(sectionA < sectionB);

    CHECK(
        text.find(
            "- [ ] **[WARNING]** `size.function-length` \xE2\x80\x94 `warnFn` \xE2\x80\x94 10:3 \xE2\x80\x94 "
            "Something is off. (observed=10, limit=5)")
        != std::string::npos);
    CHECK(
        text.find(
            "- [ ] **[ERROR]** `complexity.cyclomatic` \xE2\x80\x94 `errorFn` \xE2\x80\x94 20:3 \xE2\x80\x94 "
            "Something is off. (observed=10, limit=5)")
        != std::string::npos);
}

TEST_CASE("renderMarkdown lists diagnostics as checkboxes within the file section")
{
    codeqa::model::ProjectModel model;
    model.rootPath = "project";
    codeqa::model::FileModel file;
    file.path = model.rootPath / "src" / "broken.cpp";
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
    codeqa::report::renderMarkdown(output, model, metrics, findings);
    const std::string text = output.str();

    CHECK(text.find("## src/broken.cpp") != std::string::npos);
    CHECK(
        text.find("- [ ] **[ERROR]** `PARSE001` \xE2\x80\x94 8:1 \xE2\x80\x94 unexpected token")
        != std::string::npos);
}
