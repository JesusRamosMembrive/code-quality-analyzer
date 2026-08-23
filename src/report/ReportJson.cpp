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

Json duplicateGroupJson(const codeqa::duplication::DuplicateGroup& group, const std::filesystem::path& rootPath)
{
    Json members = Json::array();

    for (const codeqa::duplication::DuplicateMember& member : group.members)
    {
        members.push_back(Json{
            {"qualifiedName", member.qualifiedName},
            {"path", std::filesystem::relative(member.range.begin.filePath, rootPath).generic_string()},
            {"range", {{"begin", locationJson(member.range.begin)}, {"end", locationJson(member.range.end)}}}});
    }

    return Json{{"members", members}};
}

Json forbiddenDependencyJson(
    const codeqa::architecture::ForbiddenDependencyViolation& violation, const std::filesystem::path& rootPath)
{
    return Json{
        {"from",
         Json{
             {"path", std::filesystem::relative(violation.fromPath, rootPath).generic_string()},
             {"layer", violation.fromLayer}}},
        {"to",
         Json{
             {"path", std::filesystem::relative(violation.toPath, rootPath).generic_string()},
             {"layer", violation.toLayer}}}};
}

Json architectureJson(
    const codeqa::architecture::ProjectArchitectureModel& architecture, const std::filesystem::path& rootPath)
{
    Json files = Json::array();
    for (const codeqa::architecture::FileDependencyMetrics& file : architecture.files)
    {
        files.push_back(Json{
            {"path", std::filesystem::relative(file.path, rootPath).generic_string()},
            {"fanIn", file.fanIn},
            {"fanOut", file.fanOut}});
    }

    Json cycles = Json::array();
    for (const codeqa::architecture::DependencyCycle& cycle : architecture.cycles)
    {
        Json cycleFiles = Json::array();
        for (const std::filesystem::path& path : cycle.files)
        {
            cycleFiles.push_back(std::filesystem::relative(path, rootPath).generic_string());
        }
        cycles.push_back(Json{{"files", cycleFiles}});
    }

    Json fanOutFindings = Json::array();
    for (const codeqa::architecture::FanOutFinding& finding : architecture.fanOutFindings)
    {
        fanOutFindings.push_back(Json{
            {"path", std::filesystem::relative(finding.path, rootPath).generic_string()},
            {"fanOut", finding.fanOut},
            {"severity", codeqa::core::diagnosticSeverityToJsonString(finding.severity)}});
    }

    Json forbiddenDependencyViolations = Json::array();
    for (const codeqa::architecture::ForbiddenDependencyViolation& violation :
         architecture.forbiddenDependencyViolations)
    {
        forbiddenDependencyViolations.push_back(forbiddenDependencyJson(violation, rootPath));
    }

    return Json{
        {"files", files},
        {"cycles", cycles},
        {"fanOutFindings", fanOutFindings},
        {"forbiddenDependencyViolations", forbiddenDependencyViolations},
        {"externalOrUnresolvedIncludes", architecture.externalOrUnresolvedIncludes},
        {"ambiguousIncludes", architecture.ambiguousIncludes}};
}

} // namespace

nlohmann::json toReportJson(
    const codeqa::model::ProjectModel& model,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::rules::ProjectFindingsModel& findings,
    const codeqa::duplication::ProjectDuplicationModel& duplicates,
    const codeqa::architecture::ProjectArchitectureModel& architecture,
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

    document["duplicates"] = Json::array();
    for (const codeqa::duplication::DuplicateGroup& group : duplicates.groups)
    {
        document["duplicates"].push_back(duplicateGroupJson(group, model.rootPath));
    }

    document["architecture"] = architectureJson(architecture, model.rootPath);

    return document;
}

} // namespace codeqa::report
