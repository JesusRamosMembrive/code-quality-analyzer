#include "doctest/doctest.h"

#include "codeqa/cli/ExitCode.h"
#include "codeqa/cli/RunApplication.h"
#include "codeqa/infrastructure/DiscoveryOptions.h"

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
