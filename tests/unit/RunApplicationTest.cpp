#include "doctest/doctest.h"

#include "codeqa/cli/ExitCode.h"
#include "codeqa/cli/RunApplication.h"
#include "codeqa/infrastructure/DiscoveryOptions.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace
{

struct RunResult
{
    int exitCode{};
    std::string output;
    std::string errorOutput;
};

RunResult run(const std::vector<std::string>& arguments)
{
    std::ostringstream output;
    std::ostringstream errorOutput;

    const int exitCode = codeqa::cli::runApplication(arguments, output, errorOutput);

    return RunResult{exitCode, output.str(), errorOutput.str()};
}

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

std::filesystem::path makeTempDiscoverRoot()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "codeqa_run_application_discover_test";

    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::ofstream(root / "main.c") << "";
    std::ofstream(root / "util.cpp") << "";
    std::ofstream(root / "util.h") << "";

    return root;
}

std::filesystem::path makeTempAnalyzeRoot()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "codeqa_run_application_analyze_test";

    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::ofstream(root / "sample.c")
        << "int classify(int a, int b, int c, int d, int e)\n"
           "{\n"
           "    if (a == 0) { return 0; }\n"
           "    if (a == 1) { return 1; }\n"
           "    if (a == 2) { return 2; }\n"
           "    if (a == 3) { return 3; }\n"
           "    return -1;\n"
           "}\n";
    std::ofstream(root / "util.h") << "";

    return root;
}

std::filesystem::path makeTempSnapshotRoot()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "codeqa_run_application_snapshot_test";

    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::ofstream(root / "sample.c")
        << "int classify(int a, int b, int c, int d, int e)\n"
           "{\n"
           "    return a + b + c + d + e;\n"
           "}\n";

    return root;
}

void rewriteSnapshotSample(const std::filesystem::path& root)
{
    std::ofstream(root / "sample.c")
        << "int classify(int a, int b)\n"
           "{\n"
           "    if (a == 0) { return 0; }\n"
           "    if (a == 1) { return 1; }\n"
           "    if (a == 2) { return 2; }\n"
           "    return b;\n"
           "}\n";
}

std::string currentSnapshotTimestamp()
{
    const std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
    const std::tm* utc = std::gmtime(&time);
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    std::ostringstream timestamp;
    timestamp << std::put_time(utc, "%Y%m%dT%H%M%SZ");
    return timestamp.str();
}

} // namespace

TEST_CASE("--help prints usage and returns success")
{
    const RunResult result = run({"--help"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("Usage:") != std::string::npos);
    CHECK(result.errorOutput.empty());
}

TEST_CASE("-h prints usage and returns success")
{
    const RunResult result = run({"-h"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("Usage:") != std::string::npos);
}

TEST_CASE("--version prints the application name and version and returns success")
{
    const RunResult result = run({"--version"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output == "Code Quality Analyzer 0.1.0\n");
}

TEST_CASE("no arguments prints usage and returns success")
{
    const RunResult result = run({});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("Usage:") != std::string::npos);
}

TEST_CASE("an unknown option prints a CLI001 error to stderr and returns InvalidArguments")
{
    const RunResult result = run({"--unknown"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI001") != std::string::npos);
    CHECK(result.errorOutput.find("--unknown") != std::string::npos);
    CHECK(result.output.empty());
}

TEST_CASE("stdout and stderr stay separate")
{
    const RunResult helpResult = run({"--help"});
    CHECK(helpResult.errorOutput.empty());

    const RunResult errorResult = run({"--bogus"});
    CHECK(errorResult.output.empty());
}

TEST_CASE("discover without a path returns CLI002")
{
    const RunResult result = run({"discover"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI002") != std::string::npos);
}

TEST_CASE("discover with a nonexistent path returns CLI003")
{
    const RunResult result = run({"discover", "this/path/does/not/exist"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI003") != std::string::npos);
}

TEST_CASE("discover with a missing --exclude value returns CLI004")
{
    const std::filesystem::path root = makeTempDiscoverRoot();

    const RunResult result = run({"discover", root.string(), "--exclude"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI004") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("discover with an unexpected extra argument returns CLI005")
{
    const std::filesystem::path root = makeTempDiscoverRoot();

    const RunResult result = run({"discover", root.string(), "extra"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI005") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("discover on a real directory prints the summary and returns Success")
{
    const std::filesystem::path root = makeTempDiscoverRoot();

    const RunResult result = run({"discover", root.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("Files discovered: 3") != std::string::npos);
    CHECK(result.output.find("C files: 1") != std::string::npos);
    CHECK(result.output.find("C++ files: 1") != std::string::npos);
    CHECK(result.output.find("Ambiguous headers: 1") != std::string::npos);
    CHECK(result.output.find("Excluded files: 0") != std::string::npos);
    CHECK(result.output.find("Read errors: 0") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("discover respects a repeated --exclude flag")
{
    const std::filesystem::path root = makeTempDiscoverRoot();

    const RunResult result = run({"discover", root.string(), "--exclude", "util.cpp", "--exclude", "util.h"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("Files discovered: 1") != std::string::npos);
    CHECK(result.output.find("Excluded files: 2") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("--help documents the discover subcommand")
{
    const RunResult result = run({"--help"});

    CHECK(result.output.find("discover") != std::string::npos);
    CHECK(result.output.find("--exclude") != std::string::npos);
}

TEST_CASE("discover with a missing --lang-override value returns CLI006")
{
    const std::filesystem::path root = makeTempDiscoverRoot();

    const RunResult result = run({"discover", root.string(), "--lang-override"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI006") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("discover with a malformed --lang-override value returns CLI007")
{
    const std::filesystem::path root = makeTempDiscoverRoot();

    const RunResult resultNoEquals = run({"discover", root.string(), "--lang-override", "util.h"});
    CHECK(resultNoEquals.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(resultNoEquals.errorOutput.find("CLI007") != std::string::npos);

    const RunResult resultBadLanguage =
        run({"discover", root.string(), "--lang-override", "util.h=python"});
    CHECK(resultBadLanguage.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(resultBadLanguage.errorOutput.find("CLI007") != std::string::npos);

    const RunResult resultEmptyPattern =
        run({"discover", root.string(), "--lang-override", "=c"});
    CHECK(resultEmptyPattern.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(resultEmptyPattern.errorOutput.find("CLI007") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("discover prints a per-file language listing")
{
    const std::filesystem::path root = makeTempDiscoverRoot();

    const RunResult result = run({"discover", root.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("main.c  C\n") != std::string::npos);
    CHECK(result.output.find("util.cpp  C++\n") != std::string::npos);
    CHECK(result.output.find("util.h  Unknown (confidence 0.50) - ambiguous header extension\n")
        != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("--lang-override forces the language of a matching file")
{
    const std::filesystem::path root = makeTempDiscoverRoot();

    const RunResult result = run({"discover", root.string(), "--lang-override", "util.h=cpp"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("util.h  C++ (confidence 1.00) - path override\n") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("--help documents --lang-override")
{
    const RunResult result = run({"--help"});

    CHECK(result.output.find("--lang-override") != std::string::npos);
}

TEST_CASE("discover does not leave the output stream's formatting state mutated")
{
    const std::filesystem::path root = makeTempDiscoverRoot();

    std::ostringstream output;
    std::ostringstream errorOutput;

    const int exitCode = codeqa::cli::runApplication({"discover", root.string()}, output, errorOutput);
    CHECK(exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));

    // The listing above must have printed an ambiguous header's confidence (0.50), which is
    // where std::fixed/std::setprecision(2) would previously leak into the stream's persistent
    // formatting state. Writing an unformatted double to the same stream afterwards reveals the
    // leak: with the bug, this would render as "3.00" instead of the default "3".
    output << 3.0;

    CHECK(output.str().find("util.h  Unknown (confidence 0.50) - ambiguous header extension\n")
        != std::string::npos);
    CHECK(output.str().substr(output.str().size() - 1) == "3");

    std::filesystem::remove_all(root);
}

TEST_CASE("discover's per-file listing carries a Detected languages header")
{
    const std::filesystem::path root = makeTempDiscoverRoot();

    const RunResult result = run({"discover", root.string()});

    CHECK(result.output.find("\nDetected languages:\nmain.c") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("parse without a path returns CLI002")
{
    const RunResult result = run({"parse"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI002") != std::string::npos);
}

TEST_CASE("parse with a nonexistent path returns CLI003")
{
    const RunResult result = run({"parse", "this/path/does/not/exist"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI003") != std::string::npos);
}

TEST_CASE("parse on a clean directory reports no parse errors and returns Success")
{
    const std::filesystem::path root = makeTempDiscoverRoot();

    const RunResult result = run({"parse", root.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("Files parsed: 3") != std::string::npos);
    CHECK(result.output.find("Parse errors: 0") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("parse reports a broken file and still returns Success")
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "codeqa_parse_broken_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::ofstream(root / "good.c") << "int main(void) { return 0; }\n";
    std::ofstream(root / "bad.c") << "int broken( {\n";

    const RunResult result = run({"parse", root.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("Parse errors: 1") != std::string::npos);
    CHECK(result.output.find("bad.c") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("parse honours --lang-override")
{
    const std::filesystem::path root = makeTempDiscoverRoot();

    const RunResult result = run({"parse", root.string(), "--lang-override", "util.h=c"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("C files: 2") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("--help documents the parse subcommand")
{
    const RunResult result = run({"--help"});

    CHECK(result.output.find("parse <path>") != std::string::npos);
}

TEST_CASE("analyze without a path returns CLI002")
{
    const RunResult result = run({"analyze"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI002") != std::string::npos);
}

TEST_CASE("analyze with a missing --config value returns CLI008")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const RunResult result = run({"analyze", root.string(), "--config"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI008") != std::string::npos);
}

TEST_CASE("analyze with a missing --format value returns CLI009")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const RunResult result = run({"analyze", root.string(), "--format"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI009") != std::string::npos);
}

TEST_CASE("analyze with an invalid --format value returns CLI010")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const RunResult result = run({"analyze", root.string(), "--format", "xml"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI010") != std::string::npos);
}

TEST_CASE("analyze with a missing --output value returns CLI011")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const RunResult result = run({"analyze", root.string(), "--output"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI011") != std::string::npos);
}

TEST_CASE("analyze with an unwritable --output path returns CLI012")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const std::filesystem::path unwritable = root / "no-such-directory" / "report.json";
    const RunResult result = run({"analyze", root.string(), "--output", unwritable.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI012") != std::string::npos);
    CHECK_FALSE(std::filesystem::exists(root / ".codeqa" / "snapshots"));
}

TEST_CASE("analyze opens a valid output before snapshot persistence effects")
{
    const std::filesystem::path root = makeTempSnapshotRoot();
    const std::filesystem::path outputPath = root / ".codeqa";

    const RunResult result = run({"analyze", root.string(), "--output", outputPath.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::QualityGateFailed));
    CHECK(result.output.empty());
    CHECK(result.errorOutput == "Warning: could not save snapshot.\n");
    CHECK(std::filesystem::is_regular_file(outputPath));
    std::ifstream report(outputPath);
    std::ostringstream text;
    text << report.rdbuf();
    CHECK(text.str().find("Code Quality Analysis Report") != std::string::npos);
}

TEST_CASE("analyze with an invalid --config file forwards the CONFIG error and returns ConfigurationError")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const std::filesystem::path configPath = root / "codeqa.json";
    std::ofstream(configPath) << "{ not json";

    const RunResult result = run({"analyze", root.string(), "--config", configPath.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::ConfigurationError));
    CHECK(result.errorOutput.find("CONFIG001") != std::string::npos);
}

TEST_CASE("analyze with an invalid qualityGate.minSeverity value returns ConfigurationError")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const std::filesystem::path configPath = root / "codeqa.json";
    std::ofstream(configPath) << R"({ "qualityGate": { "minSeverity": "critical" } })";

    const RunResult result = run({"analyze", root.string(), "--config", configPath.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::ConfigurationError));
    CHECK(result.errorOutput.find("CONFIG009") != std::string::npos);
}

TEST_CASE("analyze with default text format prints a report to stdout")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const RunResult result = run({"analyze", root.string()});

    // sample.c's 5-parameter classify() trips signature.parameter-count at Warning, and there's
    // no previous snapshot yet, so the default (minSeverity: warning) gate fails in absolute mode.
    // The report is still printed regardless of the gate outcome.
    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::QualityGateFailed));
    CHECK(result.output.find("Code Quality Analysis Report") != std::string::npos);
    CHECK(result.output.find("signature.parameter-count") != std::string::npos);
}

TEST_CASE("analyze --format json --output writes a valid JSON report to a file")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const std::filesystem::path outputPath = root / "report.json";

    const RunResult result = run({"analyze", root.string(), "--format", "json", "--output", outputPath.string()});

    // Same gate-triggering fixture as above; the report file is still written regardless.
    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::QualityGateFailed));
    CHECK(result.output.empty());
    REQUIRE(std::filesystem::exists(outputPath));

    std::ifstream reportFile(outputPath);
    const nlohmann::json report = nlohmann::json::parse(reportFile);
    CHECK(report.at("schemaVersion").get<int>() == 1);
    CHECK(report.at("files").size() == 2);
}

TEST_CASE("analyze --format markdown --output writes a checklist report to a file")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const std::filesystem::path outputPath = root / "report.md";

    const RunResult result =
        run({"analyze", root.string(), "--format", "markdown", "--output", outputPath.string()});

    // Same gate-triggering fixture as above; the report file is still written regardless.
    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::QualityGateFailed));
    CHECK(result.output.empty());
    REQUIRE(std::filesystem::exists(outputPath));

    std::ifstream reportFile(outputPath);
    std::ostringstream reportContent;
    reportContent << reportFile.rdbuf();
    const std::string report = reportContent.str();

    CHECK(report.find("# Code Quality Analysis Report") != std::string::npos);
    CHECK(report.find("- [ ] **[") != std::string::npos);
    CHECK(report.find("signature.parameter-count") != std::string::npos);
}

TEST_CASE("analyze rejects an output path that is an analysis source without changing it")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const std::filesystem::path sourceOutputPath = root / "sample.c";
    std::ifstream sourceBeforeStream(sourceOutputPath);
    std::ostringstream sourceBefore;
    sourceBefore << sourceBeforeStream.rdbuf();

    const RunResult result = run({"analyze", root.string(), "--output", sourceOutputPath.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.output.empty());
    CHECK(result.errorOutput.find("CLI017") != std::string::npos);
    std::ifstream outputFile(sourceOutputPath);
    std::ostringstream outputText;
    outputText << outputFile.rdbuf();
    CHECK(outputText.str() == sourceBefore.str());
    CHECK_FALSE(std::filesystem::exists(root / ".codeqa" / "snapshots"));
}

TEST_CASE("analyze rejects an output path that is the explicit config without changing it")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const std::filesystem::path configPath = root / "custom.json";
    const std::string configText = R"({ "qualityGate": { "minSeverity": "error" } })";
    std::ofstream(configPath) << configText;

    const RunResult result = run(
        {"analyze", root.string(), "--config", configPath.string(), "--output", configPath.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI017") != std::string::npos);
    std::ifstream configFile(configPath);
    std::ostringstream preservedConfig;
    preservedConfig << configFile.rdbuf();
    CHECK(preservedConfig.str() == configText);
    CHECK_FALSE(std::filesystem::exists(root / ".codeqa" / "snapshots"));
}

TEST_CASE("analyze rejects an output path that targets the newest existing snapshot")
{
    const std::filesystem::path root = makeTempSnapshotRoot();
    run({"analyze", root.string()});
    const RunResult list = run({"snapshot", "list", root.string()});
    const std::string timestamp = list.output.substr(0, list.output.find('\n'));
    const std::filesystem::path snapshotPath = root / ".codeqa" / "snapshots" / (timestamp + ".json");
    std::ifstream beforeStream(snapshotPath);
    const nlohmann::json before = nlohmann::json::parse(beforeStream);

    const RunResult result = run({"analyze", root.string(), "--output", snapshotPath.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI017") != std::string::npos);
    std::ifstream afterStream(snapshotPath);
    CHECK(nlohmann::json::parse(afterStream) == before);
}

TEST_CASE("analyze rejects an output path that targets the current managed snapshot location")
{
    const std::filesystem::path root = makeTempSnapshotRoot();
    const std::filesystem::path snapshots = root / ".codeqa" / "snapshots";
    std::filesystem::create_directories(snapshots);
    const std::filesystem::path outputPath = snapshots / (currentSnapshotTimestamp() + ".json");

    const RunResult result = run({"analyze", root.string(), "--format", "json", "--output", outputPath.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI017") != std::string::npos);
    CHECK_FALSE(std::filesystem::exists(outputPath));
    CHECK(std::filesystem::is_empty(snapshots));
}

TEST_CASE("analyze: a --lang-override wins over the same pattern's --config headerOverride")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const std::filesystem::path configPath = root / "codeqa.json";
    std::ofstream(configPath)
        << R"({ "languages": { "headerOverrides": [ { "pattern": "util.h", "language": "c" } ] } })";

    const RunResult result = run(
        {"analyze", root.string(), "--config", configPath.string(), "--lang-override", "util.h=cpp", "--format",
         "json"});

    // Same gate-triggering fixture as above; this test is about header-override precedence.
    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::QualityGateFailed));

    const nlohmann::json report = nlohmann::json::parse(result.output);
    bool found = false;
    for (const nlohmann::json& file : report.at("files"))
    {
        if (file.at("path").get<std::string>() == "util.h")
        {
            found = true;
            CHECK(file.at("language").get<std::string>() == "cpp");
        }
    }
    CHECK(found);
    REQUIRE(report.at("config").at("languages").at("headerOverrides").size() == 1);
    CHECK(report.at("config").at("languages").at("headerOverrides").at(0).at("language").get<std::string>()
          == "c");
}

TEST_CASE("analyze with qualityGate.minSeverity raised to error passes despite a Warning finding")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const std::filesystem::path configPath = root / "codeqa.json";
    std::ofstream(configPath) << R"({ "qualityGate": { "minSeverity": "error" } })";

    const RunResult result = run({"analyze", root.string(), "--config", configPath.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("signature.parameter-count") != std::string::npos);
}

TEST_CASE("analyze's first run has no previous snapshot to compare against")
{
    const std::filesystem::path root = makeTempSnapshotRoot();

    const RunResult result = run({"analyze", root.string()});

    // No previous snapshot means absolute mode; sample.c's finding is Warning-severity, so the
    // default gate (minSeverity: warning) fails -- exactly the "no snapshot leaves no protection
    // gap" behavior the roadmap's absolute mode exists for.
    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::QualityGateFailed));
    CHECK(result.output.find(" new, ") == std::string::npos);
    CHECK(result.errorOutput.empty());

    std::filesystem::remove_all(root);
}

TEST_CASE("analyze saves a snapshot under <path>/.codeqa/snapshots")
{
    const std::filesystem::path root = makeTempSnapshotRoot();

    const RunResult result = run({"analyze", root.string()});
    // First run, absolute-mode gate fails (see the test above); the snapshot is still saved.
    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::QualityGateFailed));

    const std::filesystem::path snapshotsDir = root / ".codeqa" / "snapshots";
    REQUIRE(std::filesystem::is_directory(snapshotsDir));

    std::size_t fileCount = 0;
    std::filesystem::path savedFile;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(snapshotsDir))
    {
        ++fileCount;
        savedFile = entry.path();
    }
    CHECK(fileCount == 1);

    std::ifstream savedStream(savedFile);
    const nlohmann::json saved = nlohmann::json::parse(savedStream);
    CHECK(saved.at("schemaVersion").get<int>() == 1);
    savedStream.close(); // Windows can't remove_all() over a still-open file handle.

    std::filesystem::remove_all(root);
}

TEST_CASE("analyze's second run against an unchanged project reports the finding as existing")
{
    const std::filesystem::path root = makeTempSnapshotRoot();

    run({"analyze", root.string()});
    const RunResult second = run({"analyze", root.string()});

    CHECK(second.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(second.output.find("Findings: 0 new, 1 existing, 0 resolved.") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("analyze's second run after a real code change reports one new and one resolved finding")
{
    const std::filesystem::path root = makeTempSnapshotRoot();

    run({"analyze", root.string()});
    rewriteSnapshotSample(root);
    const RunResult second = run({"analyze", root.string(), "--format", "json"});

    // One new Warning-or-higher finding is exactly what the gate exists to catch.
    CHECK(second.exitCode == static_cast<int>(codeqa::cli::ExitCode::QualityGateFailed));
    const nlohmann::json report = nlohmann::json::parse(second.output);
    REQUIRE(report.contains("comparison"));
    CHECK(report.at("comparison").at("new").get<int>() == 1);
    CHECK(report.at("comparison").at("existing").get<int>() == 0);
    CHECK(report.at("comparison").at("resolved").get<int>() == 1);
    CHECK_FALSE(report.at("comparison").at("previousTimestamp").get<std::string>().empty());

    std::filesystem::remove_all(root);
}

TEST_CASE("analyze warns and skips comparison when the previous snapshot is corrupt")
{
    const std::filesystem::path root = makeTempSnapshotRoot();

    run({"analyze", root.string()});

    const std::filesystem::path snapshotsDir = root / ".codeqa" / "snapshots";
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(snapshotsDir))
    {
        std::ofstream(entry.path()) << "{ not json";
    }

    const RunResult second = run({"analyze", root.string()});

    // An unreadable previous snapshot falls back to absolute mode, same as no snapshot at all.
    CHECK(second.exitCode == static_cast<int>(codeqa::cli::ExitCode::QualityGateFailed));
    CHECK(second.errorOutput.find("could not be read") != std::string::npos);
    CHECK(second.output.find(" new, ") == std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("--help documents that analyze saves and compares snapshots automatically")
{
    const RunResult result = run({"--help"});

    CHECK(result.output.find(".codeqa/snapshots") != std::string::npos);
}

TEST_CASE("snapshot without a subcommand returns CLI013")
{
    const RunResult result = run({"snapshot"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI013") != std::string::npos);
}

TEST_CASE("snapshot with an unknown subcommand returns CLI014")
{
    const RunResult result = run({"snapshot", "delete"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI014") != std::string::npos);
}

TEST_CASE("snapshot list without a path returns CLI002")
{
    const RunResult result = run({"snapshot", "list"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI002") != std::string::npos);
}

TEST_CASE("snapshot list with a nonexistent path returns CLI003")
{
    const RunResult result = run({"snapshot", "list", "this/path/does/not/exist"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI003") != std::string::npos);
}

TEST_CASE("snapshot show without a timestamp returns CLI015")
{
    const std::filesystem::path root = makeTempSnapshotRoot();

    const RunResult result = run({"snapshot", "show", root.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI015") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("snapshot show with an unknown timestamp returns CLI016")
{
    const std::filesystem::path root = makeTempSnapshotRoot();

    const RunResult result = run({"snapshot", "show", root.string(), "20000101T000000Z"});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CLI016") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("snapshot list prints the timestamps saved by analyze")
{
    const std::filesystem::path root = makeTempSnapshotRoot();
    run({"analyze", root.string()});

    const RunResult result = run({"snapshot", "list", root.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    const std::string firstLine = result.output.substr(0, result.output.find('\n'));
    CHECK(firstLine.size() == 16); // YYYYMMDDTHHMMSSZ
    CHECK(result.output.find('\n') == firstLine.size());

    std::filesystem::remove_all(root);
}

TEST_CASE("snapshot show prints the exact JSON of a saved snapshot")
{
    const std::filesystem::path root = makeTempSnapshotRoot();
    run({"analyze", root.string()});

    const RunResult listResult = run({"snapshot", "list", root.string()});
    const std::string timestamp = listResult.output.substr(0, listResult.output.find('\n'));

    const RunResult showResult = run({"snapshot", "show", root.string(), timestamp});

    CHECK(showResult.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    const nlohmann::json snapshot = nlohmann::json::parse(showResult.output);
    CHECK(snapshot.at("schemaVersion").get<int>() == 1);

    std::filesystem::remove_all(root);
}

TEST_CASE("snapshot show rejects a snapshot directory link that resolves outside the project")
{
    const std::filesystem::path root = makeTempSnapshotRoot();
    const std::filesystem::path snapshots = root / ".codeqa" / "snapshots";
    const std::filesystem::path outside =
        std::filesystem::temp_directory_path() / "codeqa_run_application_snapshot_outside";
    const std::string timestamp = "20260814T101112Z";
    std::filesystem::remove_all(outside);
    std::filesystem::create_directories(root / ".codeqa");
    std::filesystem::create_directories(outside);
    std::ofstream(outside / (timestamp + ".json")) << R"({"external":true})";

    if (!createDirectoryLink(outside, snapshots))
    {
        WARN("Directory links are unavailable; production policy rejects resolved paths outside the project.");
        std::filesystem::remove_all(root);
        std::filesystem::remove_all(outside);
        return;
    }

    const RunResult result = run({"snapshot", "show", root.string(), timestamp});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.output.empty());
    CHECK(result.errorOutput.find("CLI016") != std::string::npos);
    CHECK(std::filesystem::exists(outside / (timestamp + ".json")));

    std::error_code ignored;
    std::filesystem::remove(snapshots, ignored);
    std::filesystem::remove_all(root, ignored);
    std::filesystem::remove_all(outside, ignored);
}

TEST_CASE("--help documents the snapshot subcommand")
{
    const RunResult result = run({"--help"});

    CHECK(result.output.find("codeqa snapshot list") != std::string::npos);
    CHECK(result.output.find("codeqa snapshot show") != std::string::npos);
}

TEST_CASE("analyze reports a duplicated function pair in the default text report")
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "codeqa_run_application_duplication_test";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);
    std::ofstream(root / "a.c") << "int addFirst(int alpha, int beta)\n"
                                   "{\n"
                                   "    int total = alpha + beta;\n"
                                   "    return total;\n"
                                   "}\n";
    std::ofstream(root / "b.c") << "int addSecond(int first, int second)\n"
                                   "{\n"
                                   "    int sum = first + second;\n"
                                   "    return sum;\n"
                                   "}\n";
    const std::filesystem::path configPath = root / "codeqa.json";
    std::ofstream(configPath) << R"({ "duplication": { "minNormalizedTokens": 10 }, )"
                                  R"("qualityGate": { "minSeverity": "error" } })";

    const RunResult result = run({"analyze", root.string(), "--config", configPath.string()});

    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("Duplicated code") != std::string::npos);
    CHECK(result.output.find("addFirst") != std::string::npos);
    CHECK(result.output.find("addSecond") != std::string::npos);
}
