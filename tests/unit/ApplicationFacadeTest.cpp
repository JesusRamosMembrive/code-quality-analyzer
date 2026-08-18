#include "doctest/doctest.h"

#include "codeqa/application/ApplicationFacade.h"
#include "codeqa/config/DefaultConfig.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace
{

bool createDirectoryLink(const std::filesystem::path& target, const std::filesystem::path& link)
{
    std::error_code linkError;
    std::filesystem::create_directory_symlink(target, link, linkError);
    if (!linkError)
    {
        return true;
    }

#ifdef _WIN32
    const std::string command =
        "cmd.exe /d /c mklink /J \"" + link.string() + "\" \"" + target.string() + "\" >NUL";
    return std::system(command.c_str()) == 0;
#else
    return false;
#endif
}

class TempProject
{
public:
    explicit TempProject(const std::string& source = {})
        : root_(std::filesystem::temp_directory_path() / "codeqa_application_facade_test"),
          outsideSentinel_(root_.parent_path() / "codeqa_application_facade_outside.json"),
          outsideSnapshots_(root_.parent_path() / "codeqa_application_facade_outside_snapshots")
    {
        std::error_code ignored;
        std::filesystem::remove(root_ / ".codeqa" / "snapshots", ignored);
        std::filesystem::remove_all(root_, ignored);
        std::filesystem::remove(outsideSentinel_, ignored);
        std::filesystem::remove_all(outsideSnapshots_, ignored);
        std::filesystem::create_directories(root_);

        if (!source.empty())
        {
            std::ofstream(root_ / "sample.c") << source;
        }

        std::ofstream(outsideSentinel_) << "keep";
    }

    ~TempProject()
    {
        std::error_code ignored;
        std::filesystem::remove(root_ / ".codeqa" / "snapshots", ignored);
        std::filesystem::remove_all(root_, ignored);
        std::filesystem::remove(outsideSentinel_, ignored);
        std::filesystem::remove_all(outsideSnapshots_, ignored);
    }

    TempProject(const TempProject&) = delete;
    TempProject& operator=(const TempProject&) = delete;

    void writeSnapshot(const std::string& timestamp, const std::string& contents) const
    {
        const std::filesystem::path directory = root_ / ".codeqa" / "snapshots";
        std::filesystem::create_directories(directory);
        std::ofstream(directory / (timestamp + ".json")) << contents;
    }

    void writeOutsideReport(const nlohmann::json& report) const
    {
        std::ofstream(outsideSentinel_) << report.dump(2);
    }

    bool redirectSnapshotsOutside(const std::string& timestamp, const nlohmann::json& report) const
    {
        const std::filesystem::path snapshots = root_ / ".codeqa" / "snapshots";
        std::error_code ignored;
        std::filesystem::remove_all(snapshots, ignored);
        std::filesystem::create_directories(outsideSnapshots_);
        std::ofstream(outsideSnapshots_ / (timestamp + ".json")) << report.dump(2);
        return createDirectoryLink(outsideSnapshots_, snapshots);
    }

    [[nodiscard]] std::size_t snapshotCount() const
    {
        const std::filesystem::path directory = root_ / ".codeqa" / "snapshots";
        std::error_code error;
        std::filesystem::directory_iterator it(directory, error);
        if (error)
        {
            return 0;
        }

        std::size_t count = 0;
        for (const std::filesystem::directory_entry& entry : it)
        {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
            {
                ++count;
            }
        }
        return count;
    }

    [[nodiscard]] const std::filesystem::path& root() const { return root_; }
    [[nodiscard]] const std::filesystem::path& outsideSentinel() const { return outsideSentinel_; }

private:
    std::filesystem::path root_;
    std::filesystem::path outsideSentinel_;
    std::filesystem::path outsideSnapshots_;
};

std::string sourceWithWarningFinding()
{
    return "int classify(int a, int b, int c, int d, int e)\n"
           "{\n"
           "    return a + b + c + d + e;\n"
           "}\n";
}

} // namespace

TEST_CASE("ApplicationFacade completes an analysis, saves a snapshot, and reports a failed quality gate")
{
    const TempProject project{sourceWithWarningFinding()};
    std::vector<codeqa::application::AnalysisPhase> phases;

    const auto result = codeqa::application::ApplicationFacade{}.analyzeProject(
        {project.root(), codeqa::config::defaultConfig(), {}},
        [&phases](const codeqa::application::AnalysisPhase phase) { phases.push_back(phase); });

    REQUIRE(result.document.has_value());
    REQUIRE(result.reportJson.has_value());
    REQUIRE(result.savedSnapshotPath.has_value());
    CHECK_FALSE(result.error.hasError());
    CHECK(result.qualityGateOutcome == codeqa::application::QualityGateOutcome::Failed);
    CHECK(result.document->qualityGateOutcome == codeqa::application::QualityGateOutcome::Failed);
    CHECK(std::filesystem::exists(*result.savedSnapshotPath));
    CHECK(result.reportJson->at("schemaVersion").get<int>() == 1);

    const std::vector<codeqa::application::AnalysisPhase> expectedPhases = {
        codeqa::application::AnalysisPhase::DiscoveringFiles,
        codeqa::application::AnalysisPhase::ParsingSourceFiles,
        codeqa::application::AnalysisPhase::MeasuringFunctions,
        codeqa::application::AnalysisPhase::ApplyingRules,
        codeqa::application::AnalysisPhase::PreparingReport};
    CHECK(phases == expectedPhases);
}

TEST_CASE("ApplicationFacade rejects an invalid in-memory config before progress or output effects")
{
    const TempProject project{sourceWithWarningFinding()};
    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.rules.at("complexity.cyclomatic").warningLimit = 20;
    config.rules.at("complexity.cyclomatic").highLimit = 10;
    std::vector<codeqa::application::AnalysisPhase> phases;

    const auto result = codeqa::application::ApplicationFacade{}.analyzeProject(
        {project.root(), config, {}},
        [&phases](const codeqa::application::AnalysisPhase phase) { phases.push_back(phase); });

    CHECK_FALSE(result.document.has_value());
    CHECK_FALSE(result.reportJson.has_value());
    CHECK_FALSE(result.savedSnapshotPath.has_value());
    CHECK(result.error.hasError());
    REQUIRE(result.error.message.has_value());
    CHECK(result.error.message->find("CONFIG008") != std::string::npos);
    CHECK(phases.empty());
    CHECK_FALSE(std::filesystem::exists(project.root() / ".codeqa"));
}

TEST_CASE("ApplicationFacade rejects a vanished project root before progress or filesystem effects")
{
    const TempProject project{sourceWithWarningFinding()};
    std::error_code removeError;
    std::filesystem::remove_all(project.root(), removeError);
    REQUIRE_FALSE(removeError);
    std::vector<codeqa::application::AnalysisPhase> phases;

    const auto result = codeqa::application::ApplicationFacade{}.analyzeProject(
        {project.root(), codeqa::config::defaultConfig(), {}},
        [&phases](const codeqa::application::AnalysisPhase phase) { phases.push_back(phase); });

    CHECK_FALSE(result.document.has_value());
    CHECK_FALSE(result.reportJson.has_value());
    CHECK_FALSE(result.savedSnapshotPath.has_value());
    CHECK(result.error.hasError());
    CHECK(phases.empty());
    CHECK_FALSE(std::filesystem::exists(project.root()));
}

TEST_CASE("ApplicationFacade rejects a non-directory project root before progress or output effects")
{
    const TempProject project{sourceWithWarningFinding()};
    std::error_code removeError;
    std::filesystem::remove_all(project.root(), removeError);
    REQUIRE_FALSE(removeError);
    std::ofstream(project.root()) << "not a directory";
    REQUIRE(std::filesystem::is_regular_file(project.root()));
    std::vector<codeqa::application::AnalysisPhase> phases;

    const auto result = codeqa::application::ApplicationFacade{}.analyzeProject(
        {project.root(), codeqa::config::defaultConfig(), {}},
        [&phases](const codeqa::application::AnalysisPhase phase) { phases.push_back(phase); });

    CHECK_FALSE(result.document.has_value());
    CHECK_FALSE(result.reportJson.has_value());
    CHECK_FALSE(result.savedSnapshotPath.has_value());
    CHECK(result.error.hasError());
    CHECK(phases.empty());
    CHECK(std::filesystem::is_regular_file(project.root()));
}

TEST_CASE("ApplicationFacade returns defaults when a project has no codeqa.json")
{
    const TempProject project;

    const auto result = codeqa::application::ApplicationFacade{}.loadProjectConfig(project.root());

    REQUIRE(result.config.has_value());
    CHECK_FALSE(result.fileExists);
    CHECK_FALSE(result.error.hasError());
    CHECK(result.config->rules.at("complexity.cyclomatic").warningLimit == 10);
}

TEST_CASE("ApplicationFacade treats an existing invalid codeqa.json as an error")
{
    const TempProject project;
    std::ofstream(project.root() / "codeqa.json") << "{ not json";

    const auto result = codeqa::application::ApplicationFacade{}.loadProjectConfig(project.root());

    CHECK_FALSE(result.config.has_value());
    CHECK(result.fileExists);
    CHECK(result.error.hasError());
    REQUIRE(result.error.message.has_value());
    CHECK(result.error.message->find("CONFIG001") != std::string::npos);
}

TEST_CASE("ApplicationFacade lists snapshots newest first with their full paths")
{
    const TempProject project;
    project.writeSnapshot("20260101T000000Z", "{}");
    project.writeSnapshot("20260301T000000Z", "{}");
    project.writeSnapshot("20260201T000000Z", "{}");

    const auto result = codeqa::application::ApplicationFacade{}.listSnapshots(project.root());

    CHECK_FALSE(result.error.hasError());
    REQUIRE(result.snapshots.size() == 3);
    CHECK(result.snapshots[0].timestamp == "20260301T000000Z");
    CHECK(result.snapshots[1].timestamp == "20260201T000000Z");
    CHECK(result.snapshots[2].timestamp == "20260101T000000Z");
    CHECK(result.snapshots[0].path == project.root() / ".codeqa" / "snapshots" / "20260301T000000Z.json");
}

TEST_CASE("ApplicationFacade rejects a path traversal snapshot deletion")
{
    const TempProject project;

    const auto result = codeqa::application::ApplicationFacade{}.deleteSnapshot(
        project.root(), "../../../codeqa_application_facade_outside");

    CHECK(result.hasError());
    CHECK(std::filesystem::exists(project.outsideSentinel()));
}

TEST_CASE("ApplicationFacade opens a snapshot without creating another snapshot")
{
    const TempProject project{sourceWithWarningFinding()};
    const auto analysis = codeqa::application::ApplicationFacade{}.analyzeProject(
        {project.root(), codeqa::config::defaultConfig(), {}});
    REQUIRE(analysis.savedSnapshotPath.has_value());
    const std::string timestamp = analysis.savedSnapshotPath->stem().string();
    const std::size_t before = project.snapshotCount();

    const auto result = codeqa::application::ApplicationFacade{}.openSnapshot(project.root(), timestamp);

    REQUIRE(result.document.has_value());
    CHECK_FALSE(result.error.has_value());
    CHECK(result.document->qualityGateOutcome == codeqa::application::QualityGateOutcome::NotEvaluated);
    CHECK(project.snapshotCount() == before);
}

TEST_CASE("ApplicationFacade rejects a path traversal snapshot open")
{
    const TempProject project{sourceWithWarningFinding()};
    const codeqa::application::ApplicationFacade facade;
    const auto analysis = facade.analyzeProject({project.root(), codeqa::config::defaultConfig(), {}});
    REQUIRE(analysis.reportJson.has_value());
    project.writeOutsideReport(*analysis.reportJson);

    const auto result = facade.openSnapshot(
        project.root(), "../../../codeqa_application_facade_outside");

    CHECK_FALSE(result.document.has_value());
    CHECK(result.error.has_value());
    CHECK(std::filesystem::exists(project.outsideSentinel()));
}

TEST_CASE("ApplicationFacade rejects a snapshot directory link that resolves outside the project")
{
    const TempProject project{sourceWithWarningFinding()};
    const codeqa::application::ApplicationFacade facade;
    const auto analysis = facade.analyzeProject({project.root(), codeqa::config::defaultConfig(), {}});
    REQUIRE(analysis.reportJson.has_value());
    const std::string timestamp = "20260814T101112Z";
    if (!project.redirectSnapshotsOutside(timestamp, *analysis.reportJson))
    {
        WARN("Directory links are unavailable; production policy rejects resolved paths outside the project.");
        return;
    }

    const auto result = facade.openSnapshot(project.root(), timestamp);

    CHECK_FALSE(result.document.has_value());
    CHECK(result.error.has_value());
}

TEST_CASE("ApplicationFacade warns and uses absolute quality-gate mode for an invalid previous snapshot")
{
    const TempProject project{sourceWithWarningFinding()};
    project.writeSnapshot("20260101T000000Z", "{ not json");

    const auto result = codeqa::application::ApplicationFacade{}.analyzeProject(
        {project.root(), codeqa::config::defaultConfig(), {}});

    REQUIRE(result.document.has_value());
    CHECK(result.qualityGateOutcome == codeqa::application::QualityGateOutcome::Failed);
    CHECK_FALSE(result.comparison.has_value());
    REQUIRE(result.warning.has_value());
    CHECK(*result.warning
          == "Warning: previous snapshot '20260101T000000Z' could not be read; skipping comparison.");
}

TEST_CASE("ApplicationFacade compares against the newest snapshot without changing raw report JSON")
{
    const TempProject project{sourceWithWarningFinding()};
    const codeqa::application::ApplicationFacade facade;
    const auto first = facade.analyzeProject({project.root(), codeqa::config::defaultConfig(), {}});
    REQUIRE(first.savedSnapshotPath.has_value());

    const auto second = facade.analyzeProject({project.root(), codeqa::config::defaultConfig(), {}});

    REQUIRE(second.document.has_value());
    REQUIRE(second.comparison.has_value());
    REQUIRE(second.reportJson.has_value());
    CHECK(second.qualityGateOutcome == codeqa::application::QualityGateOutcome::Passed);
    CHECK(second.comparison->newCount == 0);
    CHECK(second.comparison->existingCount == 1);
    CHECK(second.comparison->resolvedCount == 0);
    REQUIRE(second.document->comparison.has_value());
    CHECK(second.document->comparison->previousTimestamp == second.comparison->previousTimestamp);
    CHECK_FALSE(second.reportJson->contains("comparison"));
}

TEST_CASE("ApplicationFacade compares two arbitrary snapshots by counting added, shared, and removed findings")
{
    const TempProject project;
    project.writeSnapshot(
        "20260101T000000Z",
        R"({"files": [{"path": "sample.c", "findings": [)"
        R"({"ruleId": "complexity.cyclomatic", "symbolName": "oldOnly"}, )"
        R"({"ruleId": "complexity.cyclomatic", "symbolName": "shared"}]}]})");
    project.writeSnapshot(
        "20260102T000000Z",
        R"({"files": [{"path": "sample.c", "findings": [)"
        R"({"ruleId": "complexity.cyclomatic", "symbolName": "shared"}, )"
        R"({"ruleId": "complexity.cyclomatic", "symbolName": "newOnly"}]}]})");

    const codeqa::application::ApplicationFacade facade;
    const codeqa::application::SnapshotComparisonResult result =
        facade.compareSnapshots(project.root(), "20260101T000000Z", "20260102T000000Z");

    REQUIRE(result.comparison.has_value());
    CHECK_FALSE(result.error.hasError());
    CHECK(result.comparison->newCount == 1);
    CHECK(result.comparison->existingCount == 1);
    CHECK(result.comparison->resolvedCount == 1);
}

TEST_CASE("ApplicationFacade reports APPLICATION003 when a compared snapshot does not exist")
{
    const TempProject project;
    project.writeSnapshot("20260101T000000Z", R"({"files": []})");

    const codeqa::application::ApplicationFacade facade;
    const codeqa::application::SnapshotComparisonResult result =
        facade.compareSnapshots(project.root(), "20260101T000000Z", "does-not-exist");

    CHECK_FALSE(result.comparison.has_value());
    REQUIRE(result.error.hasError());
    CHECK(result.error.message->find("APPLICATION003") != std::string::npos);
}

TEST_CASE("ApplicationFacade reports APPLICATION004 when a compared snapshot has an unexpected shape")
{
    const TempProject project;
    project.writeSnapshot("20260101T000000Z", R"({"files": []})");
    project.writeSnapshot("20260102T000000Z", R"({"files": [{"path": "sample.c"}]})");

    const codeqa::application::ApplicationFacade facade;
    const codeqa::application::SnapshotComparisonResult result =
        facade.compareSnapshots(project.root(), "20260101T000000Z", "20260102T000000Z");

    CHECK_FALSE(result.comparison.has_value());
    REQUIRE(result.error.hasError());
    CHECK(result.error.message->find("APPLICATION004") != std::string::npos);
}

TEST_CASE("ApplicationFacade keeps an in-memory report when snapshot saving fails")
{
    const TempProject project{sourceWithWarningFinding()};
    std::ofstream(project.root() / ".codeqa") << "blocks snapshot directory";

    const auto result = codeqa::application::ApplicationFacade{}.analyzeProject(
        {project.root(), codeqa::config::defaultConfig(), {}});

    REQUIRE(result.document.has_value());
    REQUIRE(result.reportJson.has_value());
    CHECK_FALSE(result.savedSnapshotPath.has_value());
    REQUIRE(result.warning.has_value());
    CHECK(*result.warning == "Warning: could not save snapshot.");
}

TEST_CASE("ApplicationFacade reports a duplicated function pair in the JSON report and the ReportDocument")
{
    const TempProject project;
    std::ofstream(project.root() / "a.c")
        << "int addFirst(int alpha, int beta)\n{\n    int total = alpha + beta;\n    return total;\n}\n";
    std::ofstream(project.root() / "b.c")
        << "int addSecond(int first, int second)\n{\n    int sum = first + second;\n    return sum;\n}\n";

    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.duplication.minNormalizedTokens = 10;
    config.qualityGate.minSeverity = codeqa::core::DiagnosticSeverity::Error;

    const codeqa::application::AnalysisFacadeResult result =
        codeqa::application::ApplicationFacade{}.analyzeProject({project.root(), config, {}, {}});

    REQUIRE_FALSE(result.error.hasError());
    REQUIRE(result.reportJson.has_value());
    CHECK(result.reportJson->at("duplicates").size() == 1);

    REQUIRE(result.document.has_value());
    REQUIRE(result.document->duplicates.groups.size() == 1);
    CHECK(result.document->duplicates.groups.front().members.size() == 2);
}
