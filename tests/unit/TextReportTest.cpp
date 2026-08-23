#include "doctest/doctest.h"

#include "codeqa/report/TextReport.h"
#include "codeqa/duplication/DuplicationModels.h"

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
    codeqa::report::renderText(output, model, metrics, findings, codeqa::duplication::ProjectDuplicationModel{}, codeqa::architecture::ProjectArchitectureModel{});
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
    codeqa::report::renderText(output, model, metrics, findings, codeqa::duplication::ProjectDuplicationModel{}, codeqa::architecture::ProjectArchitectureModel{});
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
    codeqa::report::renderText(output, model, metrics, findings, codeqa::duplication::ProjectDuplicationModel{}, codeqa::architecture::ProjectArchitectureModel{});
    const std::string text = output.str();

    CHECK(text.find("Diagnostics\n-----------") != std::string::npos);
    CHECK(text.find("src/broken.cpp") != std::string::npos);
    CHECK(text.find("  line 8: error [PARSE001] unexpected token") != std::string::npos);
}

TEST_CASE("renderText omits the duplicated code section when there are no groups")
{
    codeqa::model::ProjectModel model;
    model.rootPath = "project";
    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::rules::ProjectFindingsModel findings;
    const codeqa::duplication::ProjectDuplicationModel duplicates;

    std::ostringstream output;
    codeqa::report::renderText(output, model, metrics, findings, duplicates, codeqa::architecture::ProjectArchitectureModel{});

    CHECK(output.str().find("Duplicated code") == std::string::npos);
}

TEST_CASE("renderText lists each duplicate group with its members")
{
    codeqa::model::ProjectModel model;
    model.rootPath = "project";
    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::rules::ProjectFindingsModel findings;

    codeqa::duplication::ProjectDuplicationModel duplicates;
    codeqa::duplication::DuplicateGroup group;
    group.members.push_back(codeqa::duplication::DuplicateMember{
        "first",
        codeqa::core::SourceRange{
            codeqa::core::SourceLocation{model.rootPath / "src" / "a.cpp", 3, 1, 20},
            codeqa::core::SourceLocation{model.rootPath / "src" / "a.cpp", 6, 1, 60}}});
    group.members.push_back(codeqa::duplication::DuplicateMember{
        "second",
        codeqa::core::SourceRange{
            codeqa::core::SourceLocation{model.rootPath / "src" / "b.cpp", 8, 1, 100},
            codeqa::core::SourceLocation{model.rootPath / "src" / "b.cpp", 11, 1, 140}}});
    duplicates.groups.push_back(group);

    std::ostringstream output;
    codeqa::report::renderText(output, model, metrics, findings, duplicates, codeqa::architecture::ProjectArchitectureModel{});
    const std::string text = output.str();

    CHECK(text.find("Duplicated code") != std::string::npos);
    CHECK(text.find("first") != std::string::npos);
    CHECK(text.find("second") != std::string::npos);
    CHECK(text.find("src/a.cpp:3") != std::string::npos);
    CHECK(text.find("src/b.cpp:8") != std::string::npos);
}

TEST_CASE("renderText omits the architecture section when there is nothing to report")
{
    codeqa::model::ProjectModel model;
    model.rootPath = "project";
    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::rules::ProjectFindingsModel findings;
    const codeqa::duplication::ProjectDuplicationModel duplicates;
    const codeqa::architecture::ProjectArchitectureModel architecture;

    std::ostringstream output;
    codeqa::report::renderText(output, model, metrics, findings, duplicates, architecture);

    CHECK(output.str().find("Architecture") == std::string::npos);
}

TEST_CASE("renderText lists a cycle and a fan-out finding")
{
    codeqa::model::ProjectModel model;
    model.rootPath = "project";
    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::rules::ProjectFindingsModel findings;
    const codeqa::duplication::ProjectDuplicationModel duplicates;

    codeqa::architecture::ProjectArchitectureModel architecture;
    architecture.cycles.push_back(codeqa::architecture::DependencyCycle{
        {model.rootPath / "src" / "a.h", model.rootPath / "src" / "b.h"}});
    architecture.fanOutFindings.push_back(codeqa::architecture::FanOutFinding{
        model.rootPath / "src" / "hub.h", 15, codeqa::core::DiagnosticSeverity::Warning});

    std::ostringstream output;
    codeqa::report::renderText(output, model, metrics, findings, duplicates, architecture);
    const std::string text = output.str();

    CHECK(text.find("Architecture") != std::string::npos);
    CHECK(text.find("hub.h") != std::string::npos);
    CHECK(text.find("fan-out=15") != std::string::npos);
}

TEST_CASE("renderText lists a forbidden dependency violation")
{
    codeqa::model::ProjectModel model;
    model.rootPath = "project";
    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::rules::ProjectFindingsModel findings;
    const codeqa::duplication::ProjectDuplicationModel duplicates;

    codeqa::architecture::ProjectArchitectureModel architecture;
    architecture.forbiddenDependencyViolations.push_back(codeqa::architecture::ForbiddenDependencyViolation{
        model.rootPath / "src" / "core" / "a.h", "core", model.rootPath / "src" / "desktop" / "b.h", "desktop"});

    std::ostringstream output;
    codeqa::report::renderText(output, model, metrics, findings, duplicates, architecture);
    const std::string text = output.str();

    CHECK(text.find("Architecture") != std::string::npos);
    CHECK(text.find("core") != std::string::npos);
    CHECK(text.find("desktop") != std::string::npos);
}
