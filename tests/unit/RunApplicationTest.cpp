#include "doctest/doctest.h"

#include "codeqa/cli/ExitCode.h"
#include "codeqa/cli/RunApplication.h"
#include "codeqa/infrastructure/DiscoveryOptions.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
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
}

TEST_CASE("analyze with an invalid --config file forwards the CONFIG error and returns InvalidArguments")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const std::filesystem::path configPath = root / "codeqa.json";
    std::ofstream(configPath) << "{ not json";

    const RunResult result = run({"analyze", root.string(), "--config", configPath.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::InvalidArguments));
    CHECK(result.errorOutput.find("CONFIG001") != std::string::npos);
}

TEST_CASE("analyze with default text format prints a report to stdout")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const RunResult result = run({"analyze", root.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.find("Code Quality Analysis Report") != std::string::npos);
    CHECK(result.output.find("signature.parameter-count") != std::string::npos);
}

TEST_CASE("analyze --format json --output writes a valid JSON report to a file")
{
    const std::filesystem::path root = makeTempAnalyzeRoot();
    const std::filesystem::path outputPath = root / "report.json";

    const RunResult result = run({"analyze", root.string(), "--format", "json", "--output", outputPath.string()});

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));
    CHECK(result.output.empty());
    REQUIRE(std::filesystem::exists(outputPath));

    std::ifstream reportFile(outputPath);
    const nlohmann::json report = nlohmann::json::parse(reportFile);
    CHECK(report.at("schemaVersion").get<int>() == 1);
    CHECK(report.at("files").size() == 2);
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

    CHECK(result.exitCode == static_cast<int>(codeqa::cli::ExitCode::Success));

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
}
