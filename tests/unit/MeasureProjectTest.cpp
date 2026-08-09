#include "doctest/doctest.h"

#include "codeqa/application/AnalyzeProject.h"
#include "codeqa/application/MeasureProject.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace
{

class TempProject
{
public:
    TempProject()
        : root_(std::filesystem::temp_directory_path() / "codeqa_measure_project_test")
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

const codeqa::metrics::FileMetricsModel& fileMetricsNamed(
    const codeqa::metrics::ProjectMetricsModel& project, const std::filesystem::path& path)
{
    for (const codeqa::metrics::FileMetricsModel& file : project.files)
    {
        if (file.path == path)
        {
            return file;
        }
    }

    throw std::runtime_error("Expected metrics file was not produced.");
}

} // namespace

TEST_CASE("MeasureProject computes metrics for every non-prototype function")
{
    const TempProject fixture;
    fixture.addFile(
        "sample.c",
        "int add(int a, int b);\n"
        "\n"
        "int add(int a, int b)\n"
        "{\n"
        "    return a + b;\n"
        "}\n");

    codeqa::application::AnalyzeProjectRequest request;
    request.discovery.rootPath = fixture.root();
    const codeqa::model::ProjectModel project = codeqa::application::AnalyzeProject{}.execute(request);
    const codeqa::metrics::ProjectMetricsModel metrics = codeqa::application::MeasureProject{}.execute(project);

    CHECK(metrics.rootPath == fixture.root());
    REQUIRE(metrics.files.size() == 1);
    const codeqa::metrics::FileMetricsModel& file = metrics.files.front();
    CHECK(file.path == fixture.root() / "sample.c");
    CHECK(file.diagnostics.empty());
    REQUIRE(file.functions.size() == 1);
    CHECK(file.functions[0].symbol.name == "add");
    CHECK(file.functions[0].size.statementCount == 1);
}

TEST_CASE("MeasureProject computes metrics for every function of a multi-function file")
{
    const TempProject fixture;
    fixture.addFile(
        "multi.c",
        "int first(void) { return 1; }\n"
        "int second(void) { return 2; }\n");

    codeqa::application::AnalyzeProjectRequest request;
    request.discovery.rootPath = fixture.root();
    const codeqa::model::ProjectModel project = codeqa::application::AnalyzeProject{}.execute(request);
    const codeqa::metrics::ProjectMetricsModel metrics = codeqa::application::MeasureProject{}.execute(project);

    const codeqa::metrics::FileMetricsModel& file = fileMetricsNamed(metrics, fixture.root() / "multi.c");
    REQUIRE(file.functions.size() == 2);
    CHECK(file.functions[0].symbol.name == "first");
    CHECK(file.functions[1].symbol.name == "second");
    CHECK(file.functions[0].size.physicalLines == 1);
    CHECK(file.functions[1].size.physicalLines == 1);
}

TEST_CASE("MeasureProject reports a diagnostic, not an abort, when a file disappears before measurement")
{
    const TempProject fixture;
    fixture.addFile("stable.c", "int stable(void) { return 1; }\n");
    fixture.addFile("vanishing.c", "int vanishing(void) { return 2; }\n");

    codeqa::application::AnalyzeProjectRequest request;
    request.discovery.rootPath = fixture.root();
    const codeqa::model::ProjectModel project = codeqa::application::AnalyzeProject{}.execute(request);

    std::filesystem::remove(fixture.root() / "vanishing.c");

    const codeqa::metrics::ProjectMetricsModel metrics = codeqa::application::MeasureProject{}.execute(project);

    REQUIRE(metrics.files.size() == 2);
    const codeqa::metrics::FileMetricsModel& vanished =
        fileMetricsNamed(metrics, fixture.root() / "vanishing.c");
    CHECK(vanished.functions.empty());
    REQUIRE(vanished.diagnostics.size() == 1);
    CHECK(vanished.diagnostics[0].code == "METRICS001");
    CHECK(vanished.diagnostics[0].severity == codeqa::core::DiagnosticSeverity::Error);

    const codeqa::metrics::FileMetricsModel& stable = fileMetricsNamed(metrics, fixture.root() / "stable.c");
    CHECK(stable.diagnostics.empty());
    REQUIRE(stable.functions.size() == 1);
    CHECK(stable.functions[0].symbol.name == "stable");
}
