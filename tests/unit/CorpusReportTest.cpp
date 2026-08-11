#include "doctest/doctest.h"

#include "codeqa/application/RunAnalysis.h"
#include "codeqa/config/DefaultConfig.h"
#include "codeqa/report/ReportJson.h"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace
{

const nlohmann::json& fileEntryNamed(const nlohmann::json& document, const std::string& path)
{
    for (const nlohmann::json& file : document.at("files"))
    {
        if (file.at("path").get<std::string>() == path)
        {
            return file;
        }
    }

    throw std::runtime_error("Expected report file entry was not produced: " + path);
}

} // namespace

TEST_CASE("toReportJson over the real corpus reproduces the deep-nesting fixture's known finding and metric")
{
    codeqa::application::RunAnalysisRequest request;
    request.analysis.discovery.rootPath = std::filesystem::path{CODEQA_CORPUS_DIR};
    request.config = codeqa::config::defaultConfig();

    const codeqa::application::RunAnalysisResult result = codeqa::application::RunAnalysis{}.execute(request);
    const nlohmann::json document =
        codeqa::report::toReportJson(result.model, result.metrics, result.findings, request.config, "test");

    CHECK(document.at("schemaVersion").get<int>() == 1);

    const nlohmann::json& file = fileEntryNamed(document, "nesting/deep-nesting.cpp");
    CHECK(file.at("language").get<std::string>() == "cpp");

    bool foundMetric = false;
    for (const nlohmann::json& metric : file.at("metrics"))
    {
        if (metric.at("symbol").at("name").get<std::string>() == "fourLevels")
        {
            foundMetric = true;
            CHECK(metric.at("complexity").at("maxNestingDepth").get<int>() == 3);
        }
    }
    CHECK(foundMetric);

    REQUIRE(file.at("findings").size() == 1);
    const nlohmann::json& finding = file.at("findings")[0];
    CHECK(finding.at("ruleId").get<std::string>() == "complexity.nesting");
    CHECK(finding.at("severity").get<std::string>() == "warning");
    CHECK(finding.at("symbolName").get<std::string>() == "fourLevels");
    CHECK(finding.at("observedValue").get<int>() == 3);
    CHECK(finding.at("limitValue").get<int>() == 3);
    CHECK(finding.at("location").at("line").get<int>() == 26);
    CHECK(finding.at("location").at("column").get<int>() == 17);
}
