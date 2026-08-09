#include "doctest/doctest.h"

#include "codeqa/application/AnalyzeProject.h"
#include "codeqa/application/ApplyRules.h"
#include "codeqa/application/MeasureProject.h"

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

codeqa::rules::ProjectFindingsModel analyzeAndApplyRules(const std::filesystem::path& root)
{
    codeqa::application::AnalyzeProjectRequest request;
    request.discovery.rootPath = root;
    const codeqa::model::ProjectModel project = codeqa::application::AnalyzeProject{}.execute(request);
    const codeqa::metrics::ProjectMetricsModel metrics = codeqa::application::MeasureProject{}.execute(project);
    return codeqa::application::ApplyRules{}.execute(project, metrics);
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
}
