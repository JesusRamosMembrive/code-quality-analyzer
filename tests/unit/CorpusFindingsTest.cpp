#include "doctest/doctest.h"

#include "codeqa/application/AnalyzeProject.h"
#include "codeqa/application/ApplyRules.h"
#include "codeqa/application/MeasureProject.h"
#include "codeqa/config/DefaultConfig.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using Json = nlohmann::json;

struct FindingExpectation
{
    std::string symbolName;
    std::string ruleId;
    std::string severity;
    long long observedValue{0};
    long long limitValue{0};
    std::size_t line{0};
    std::size_t column{0};
};

std::string severityName(codeqa::core::DiagnosticSeverity severity)
{
    switch (severity)
    {
        case codeqa::core::DiagnosticSeverity::Information:
            return "information";
        case codeqa::core::DiagnosticSeverity::Warning:
            return "warning";
        case codeqa::core::DiagnosticSeverity::Error:
            return "error";
    }

    return "unknown";
}

std::vector<FindingExpectation> readExpectation(const std::filesystem::path& path)
{
    std::ifstream input(path);

    if (!input)
    {
        throw std::runtime_error("Could not open findings expectation '" + path.generic_string() + "'.");
    }

    const Json document = Json::parse(input);
    std::vector<FindingExpectation> expectations;

    for (const Json& finding : document.at("findings"))
    {
        FindingExpectation expected;
        expected.symbolName = finding.at("symbolName").get<std::string>();
        expected.ruleId = finding.at("ruleId").get<std::string>();
        expected.severity = finding.at("severity").get<std::string>();
        expected.observedValue = finding.at("observedValue").get<long long>();
        expected.limitValue = finding.at("limitValue").get<long long>();
        const Json& location = finding.at("location");
        expected.line = location.at("line").get<std::size_t>();
        expected.column = location.at("column").get<std::size_t>();
        expectations.push_back(std::move(expected));
    }

    return expectations;
}

std::string describe(const codeqa::rules::Finding& value)
{
    std::ostringstream output;
    output << "{\"symbolName\": \"" << value.symbolName << "\", \"ruleId\": \"" << value.ruleId
           << "\", \"severity\": \"" << severityName(value.severity)
           << "\", \"observedValue\": " << (value.observedValue.has_value() ? *value.observedValue : 0)
           << ", \"limitValue\": " << (value.limitValue.has_value() ? *value.limitValue : 0)
           << ", \"location\": {\"line\": " << value.location.line << ", \"column\": " << value.location.column
           << "}}";
    return output.str();
}

bool matches(const codeqa::rules::Finding& actual, const FindingExpectation& expected)
{
    return actual.symbolName == expected.symbolName && actual.ruleId == expected.ruleId
        && severityName(actual.severity) == expected.severity && actual.observedValue.has_value()
        && *actual.observedValue == expected.observedValue && actual.limitValue.has_value()
        && *actual.limitValue == expected.limitValue && actual.location.line == expected.line
        && actual.location.column == expected.column;
}

const codeqa::rules::FileFindingsModel& fileFindingsNamed(
    const codeqa::rules::ProjectFindingsModel& project, const std::filesystem::path& path)
{
    for (const codeqa::rules::FileFindingsModel& file : project.files)
    {
        if (file.path == path)
        {
            return file;
        }
    }

    throw std::runtime_error("Expected findings file was not produced: " + path.generic_string());
}

std::vector<std::filesystem::path> fixturesWithFindings(const std::filesystem::path& corpusRoot)
{
    return {
        corpusRoot / "nesting" / "deep-nesting.cpp",
        corpusRoot / "parameters" / "cpp-parameters.cpp",
        corpusRoot / "size" / "large-functions.cpp",
        corpusRoot / "parameters" / "consecutive-booleans.cpp",
        corpusRoot / "complexity" / "thresholds.cpp"};
}

} // namespace

TEST_CASE("corpus findings match their expectations")
{
    const std::filesystem::path corpusRoot{CODEQA_CORPUS_DIR};
    codeqa::application::AnalyzeProjectRequest request;
    request.discovery.rootPath = corpusRoot;
    const codeqa::model::ProjectModel project = codeqa::application::AnalyzeProject{}.execute(request);
    const codeqa::metrics::ProjectMetricsModel metrics = codeqa::application::MeasureProject{}.execute(project);
    const codeqa::rules::ProjectFindingsModel findings =
        codeqa::application::ApplyRules{}.execute(project, metrics, codeqa::config::defaultConfig());

    for (const std::filesystem::path& sourcePath : fixturesWithFindings(corpusRoot))
    {
        INFO("fixture: " << sourcePath.generic_string());
        const std::filesystem::path expectationPath =
            sourcePath.parent_path() / (sourcePath.stem().string() + ".findings.json");
        const std::vector<FindingExpectation> expectation = readExpectation(expectationPath);
        const codeqa::rules::FileFindingsModel& file = fileFindingsNamed(findings, sourcePath);

        REQUIRE_MESSAGE(
            file.findings.size() == expectation.size(),
            "actual has " << file.findings.size() << " finding(s), expected " << expectation.size());

        for (std::size_t index = 0; index < expectation.size(); ++index)
        {
            CAPTURE(index);
            CHECK_MESSAGE(
                matches(file.findings[index], expectation[index]),
                "actual: " << describe(file.findings[index]) << "; expected symbol '"
                           << expectation[index].symbolName << "'");
        }
    }
}
