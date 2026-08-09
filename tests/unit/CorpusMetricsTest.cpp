#include "doctest/doctest.h"

#include "codeqa/application/AnalyzeProject.h"
#include "codeqa/application/MeasureProject.h"

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

struct FunctionMetricsExpectation
{
    std::string name;
    std::size_t physicalLines{0};
    std::size_t codeLines{0};
    std::size_t blankLines{0};
    std::size_t commentLines{0};
    std::size_t statementCount{0};
    std::size_t parameterCount{0};
    std::size_t signatureLength{0};
    std::size_t booleanParameterCount{0};
    std::size_t cyclomaticComplexity{0};
    std::size_t maxConditionComplexity{0};
    std::size_t maxNestingDepth{0};
    std::size_t branchCount{0};
    std::size_t returnCount{0};
};

std::vector<FunctionMetricsExpectation> readExpectation(const std::filesystem::path& path)
{
    std::ifstream input(path);

    if (!input)
    {
        throw std::runtime_error("Could not open metrics expectation '" + path.generic_string() + "'.");
    }

    const Json document = Json::parse(input);
    std::vector<FunctionMetricsExpectation> expectations;

    for (const Json& function : document.at("functions"))
    {
        FunctionMetricsExpectation expected;
        expected.name = function.at("name").get<std::string>();
        const Json& size = function.at("size");
        expected.physicalLines = size.at("physicalLines").get<std::size_t>();
        expected.codeLines = size.at("codeLines").get<std::size_t>();
        expected.blankLines = size.at("blankLines").get<std::size_t>();
        expected.commentLines = size.at("commentLines").get<std::size_t>();
        expected.statementCount = size.at("statementCount").get<std::size_t>();
        const Json& signature = function.at("signature");
        expected.parameterCount = signature.at("parameterCount").get<std::size_t>();
        expected.signatureLength = signature.at("signatureLength").get<std::size_t>();
        expected.booleanParameterCount = signature.at("booleanParameterCount").get<std::size_t>();
        const Json& complexity = function.at("complexity");
        expected.cyclomaticComplexity = complexity.at("cyclomaticComplexity").get<std::size_t>();
        expected.maxConditionComplexity = complexity.at("maxConditionComplexity").get<std::size_t>();
        expected.maxNestingDepth = complexity.at("maxNestingDepth").get<std::size_t>();
        expected.branchCount = complexity.at("branchCount").get<std::size_t>();
        expected.returnCount = complexity.at("returnCount").get<std::size_t>();
        expectations.push_back(std::move(expected));
    }

    return expectations;
}

std::string describe(const codeqa::metrics::FunctionMetricsModel& value)
{
    std::ostringstream output;
    output << "{\"name\": \"" << value.symbol.name << "\", "
           << "\"size\": {\"physicalLines\": " << value.size.physicalLines
           << ", \"codeLines\": " << value.size.codeLines << ", \"blankLines\": " << value.size.blankLines
           << ", \"commentLines\": " << value.size.commentLines
           << ", \"statementCount\": " << value.size.statementCount << "}, "
           << "\"signature\": {\"parameterCount\": " << value.signature.parameterCount
           << ", \"signatureLength\": " << value.signature.signatureLength
           << ", \"booleanParameterCount\": " << value.signature.booleanParameterCount << "}, "
           << "\"complexity\": {\"cyclomaticComplexity\": " << value.complexity.cyclomaticComplexity
           << ", \"maxConditionComplexity\": " << value.complexity.maxConditionComplexity
           << ", \"maxNestingDepth\": " << value.complexity.maxNestingDepth
           << ", \"branchCount\": " << value.complexity.branchCount
           << ", \"returnCount\": " << value.complexity.returnCount << "}}";
    return output.str();
}

bool matches(const codeqa::metrics::FunctionMetricsModel& actual, const FunctionMetricsExpectation& expected)
{
    return actual.symbol.name == expected.name && actual.size.physicalLines == expected.physicalLines
        && actual.size.codeLines == expected.codeLines && actual.size.blankLines == expected.blankLines
        && actual.size.commentLines == expected.commentLines
        && actual.size.statementCount == expected.statementCount
        && actual.signature.parameterCount == expected.parameterCount
        && actual.signature.signatureLength == expected.signatureLength
        && actual.signature.booleanParameterCount == expected.booleanParameterCount
        && actual.complexity.cyclomaticComplexity == expected.cyclomaticComplexity
        && actual.complexity.maxConditionComplexity == expected.maxConditionComplexity
        && actual.complexity.maxNestingDepth == expected.maxNestingDepth
        && actual.complexity.branchCount == expected.branchCount
        && actual.complexity.returnCount == expected.returnCount;
}

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

    throw std::runtime_error("Expected metrics file was not produced: " + path.generic_string());
}

std::vector<std::filesystem::path> fixturesWithMetrics(const std::filesystem::path& corpusRoot)
{
    return {
        corpusRoot / "complexity" / "branching.cpp",
        corpusRoot / "nesting" / "deep-nesting.cpp",
        corpusRoot / "parameters" / "cpp-parameters.cpp"};
}

} // namespace

TEST_CASE("corpus function metrics match their expectations")
{
    const std::filesystem::path corpusRoot{CODEQA_CORPUS_DIR};
    codeqa::application::AnalyzeProjectRequest request;
    request.discovery.rootPath = corpusRoot;
    const codeqa::model::ProjectModel project = codeqa::application::AnalyzeProject{}.execute(request);
    const codeqa::metrics::ProjectMetricsModel metrics = codeqa::application::MeasureProject{}.execute(project);

    for (const std::filesystem::path& sourcePath : fixturesWithMetrics(corpusRoot))
    {
        INFO("fixture: " << sourcePath.generic_string());
        const std::filesystem::path expectationPath =
            sourcePath.parent_path() / (sourcePath.stem().string() + ".metrics.json");
        const std::vector<FunctionMetricsExpectation> expectation = readExpectation(expectationPath);
        const codeqa::metrics::FileMetricsModel& file = fileMetricsNamed(metrics, sourcePath);

        REQUIRE(file.diagnostics.empty());
        REQUIRE_MESSAGE(
            file.functions.size() == expectation.size(),
            "actual has " << file.functions.size() << " function(s), expected " << expectation.size());

        for (std::size_t index = 0; index < expectation.size(); ++index)
        {
            CAPTURE(index);
            CHECK_MESSAGE(
                matches(file.functions[index], expectation[index]),
                "actual: " << describe(file.functions[index]) << "; expected function '"
                           << expectation[index].name << "'");
        }
    }
}
