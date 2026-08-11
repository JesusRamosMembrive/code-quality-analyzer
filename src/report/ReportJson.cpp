#include "codeqa/report/ReportJson.h"

#include "codeqa/config/ConfigJson.h"
#include "codeqa/core/EnumJson.h"
#include "codeqa/report/FileJson.h"

#include <cstddef>
#include <filesystem>
#include <stdexcept>

namespace codeqa::report
{

namespace
{

using Json = nlohmann::json;

Json locationJson(const codeqa::core::SourceLocation& location)
{
    return Json{{"line", location.line}, {"column", location.column}};
}

Json functionMetricsJson(const codeqa::metrics::FunctionMetricsModel& function)
{
    return Json{
        {"symbol",
         {{"name", function.symbol.name},
          {"qualifiedName", function.symbol.qualifiedName},
          {"range",
           {{"begin", locationJson(function.symbol.range.begin)},
            {"end", locationJson(function.symbol.range.end)}}}}},
        {"size",
         {{"physicalLines", function.size.physicalLines},
          {"codeLines", function.size.codeLines},
          {"blankLines", function.size.blankLines},
          {"commentLines", function.size.commentLines},
          {"statementCount", function.size.statementCount}}},
        {"signature",
         {{"parameterCount", function.signature.parameterCount},
          {"signatureLength", function.signature.signatureLength},
          {"booleanParameterCount", function.signature.booleanParameterCount}}},
        {"complexity",
         {{"cyclomaticComplexity", function.complexity.cyclomaticComplexity},
          {"maxConditionComplexity", function.complexity.maxConditionComplexity},
          {"maxNestingDepth", function.complexity.maxNestingDepth},
          {"branchCount", function.complexity.branchCount},
          {"returnCount", function.complexity.returnCount}}}};
}

Json findingJson(const codeqa::rules::Finding& finding)
{
    Json document{
        {"ruleId", finding.ruleId},
        {"severity", codeqa::core::diagnosticSeverityToJsonString(finding.severity)},
        {"description", finding.description},
        {"recommendation", finding.recommendation},
        {"location", locationJson(finding.location)},
        {"symbolName", finding.symbolName}};

    document["observedValue"] = finding.observedValue.has_value() ? Json(*finding.observedValue) : Json(nullptr);
    document["limitValue"] = finding.limitValue.has_value() ? Json(*finding.limitValue) : Json(nullptr);

    return document;
}

} // namespace

nlohmann::json toReportJson(
    const codeqa::model::ProjectModel& model,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::rules::ProjectFindingsModel& findings,
    const codeqa::config::ProjectConfig& config,
    const std::string& toolVersion)
{
    if (metrics.files.size() != model.files.size() || findings.files.size() != model.files.size())
    {
        throw std::invalid_argument("toReportJson: model/metrics/findings must be index-aligned.");
    }

    Json document;
    document["schemaVersion"] = 1;
    document["metadata"] =
        Json{{"rootPath", model.rootPath.generic_string()}, {"toolName", "codeqa"}, {"toolVersion", toolVersion}};
    document["config"] = codeqa::config::toJson(config);

    document["files"] = Json::array();

    // Precondition: model.files, metrics.files and findings.files are index-aligned --
    // MeasureProject::execute and ApplyRules::execute both iterate project.files and push
    // exactly one entry per input file, in the same order (verified in both source files).
    for (std::size_t index = 0; index < model.files.size(); ++index)
    {
        Json fileDocument = fileJson(model.files[index]);

        // FileModel.path is rootPath-prefixed (see AnalyzeProject.cpp/ParseSourceFile.cpp), not
        // relative -- relativize here so the JSON stays portable across machines/invocations.
        fileDocument["path"] = std::filesystem::relative(model.files[index].path, model.rootPath).generic_string();

        fileDocument["metrics"] = Json::array();
        for (const codeqa::metrics::FunctionMetricsModel& function : metrics.files[index].functions)
        {
            fileDocument["metrics"].push_back(functionMetricsJson(function));
        }

        fileDocument["findings"] = Json::array();
        for (const codeqa::rules::Finding& finding : findings.files[index].findings)
        {
            fileDocument["findings"].push_back(findingJson(finding));
        }

        document["files"].push_back(std::move(fileDocument));
    }

    return document;
}

} // namespace codeqa::report
