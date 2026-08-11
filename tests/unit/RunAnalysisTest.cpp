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
