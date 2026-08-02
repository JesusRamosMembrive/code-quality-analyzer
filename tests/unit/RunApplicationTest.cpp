#include "doctest/doctest.h"

#include "codeqa/cli/ExitCode.h"
#include "codeqa/cli/RunApplication.h"

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
