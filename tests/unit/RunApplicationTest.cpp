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
