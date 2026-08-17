#include "codeqa/application/ReportDocumentFactory.h"

#include "codeqa/config/ConfigJson.h"
#include "codeqa/core/EnumJson.h"

#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace codeqa::application
{

namespace
{

using Json = nlohmann::json;

class ReportParseFailure : public std::runtime_error
{
public:
    explicit ReportParseFailure(const std::string& message) : std::runtime_error(message) {}
};

[[noreturn]] void failRequired(const std::string& message)
{
    throw ReportParseFailure("Error [REPORT004]: " + message);
}

const Json& requireField(const Json& object, const std::string& field, const std::string& path)
{
    if (!object.is_object())
    {
        failRequired("'" + path + "' must be an object.");
    }

    if (!object.contains(field))
    {
        failRequired("'" + path + "' is missing required field '" + field + "'.");
    }

    return object.at(field);
}

const Json& requireObject(const Json& object, const std::string& field, const std::string& path)
{
    const Json& value = requireField(object, field, path);
    if (!value.is_object())
    {
        failRequired("'" + path + "." + field + "' must be an object.");
    }
    return value;
}

const Json& requireArray(const Json& object, const std::string& field, const std::string& path)
{
    const Json& value = requireField(object, field, path);
    if (!value.is_array())
    {
        failRequired("'" + path + "." + field + "' must be an array.");
    }
    return value;
}

std::string requireString(const Json& object, const std::string& field, const std::string& path)
{
    const Json& value = requireField(object, field, path);
    if (!value.is_string())
    {
        failRequired("'" + path + "." + field + "' must be a string.");
    }
    return value.get<std::string>();
}

std::size_t requireSize(const Json& object, const std::string& field, const std::string& path)
{
    const Json& value = requireField(object, field, path);
    if ((!value.is_number_unsigned() && !value.is_number_integer()) || value.is_number_integer() && value.get<long long>() < 0)
    {
        failRequired("'" + path + "." + field + "' must be a non-negative integer.");
    }

    const unsigned long long number = value.is_number_unsigned()
        ? value.get<unsigned long long>()
        : static_cast<unsigned long long>(value.get<long long>());
    if (number > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max()))
    {
        failRequired("'" + path + "." + field + "' is out of range.");
    }
    return static_cast<std::size_t>(number);
}

long long requireOptionalInteger(const Json& object, const std::string& field, const std::string& path)
{
    const Json& value = requireField(object, field, path);
    if (!value.is_number_integer() && !value.is_number_unsigned())
    {
        failRequired("'" + path + "." + field + "' must be an integer or null.");
    }

    if (value.is_number_unsigned()
        && value.get<unsigned long long>() > static_cast<unsigned long long>(std::numeric_limits<long long>::max()))
    {
        failRequired("'" + path + "." + field + "' is out of range.");
    }

    return value.get<long long>();
}

codeqa::core::SourceLocation parseLocation(const Json& document, const std::string& path)
{
    if (!document.is_object())
    {
        failRequired("'" + path + "' must be an object.");
    }

    codeqa::core::SourceLocation location;
    location.line = requireSize(document, "line", path);
    location.column = requireSize(document, "column", path);
    return location;
}

codeqa::core::SourceRange parseRange(const Json& document, const std::string& path)
{
    if (!document.is_object())
    {
        failRequired("'" + path + "' must be an object.");
    }

    return codeqa::core::SourceRange{
        parseLocation(requireObject(document, "begin", path), path + ".begin"),
        parseLocation(requireObject(document, "end", path), path + ".end")};
}

codeqa::model::SymbolIdentity parseSymbol(const Json& document, const std::string& path)
{
    if (!document.is_object())
    {
        failRequired("'" + path + "' must be an object.");
    }

    codeqa::model::SymbolIdentity symbol;
    symbol.name = requireString(document, "name", path);
    symbol.qualifiedName = requireString(document, "qualifiedName", path);
    symbol.range = parseRange(requireObject(document, "range", path), path + ".range");
    return symbol;
}

codeqa::core::Language parseLanguage(const std::string& text, const std::string& path)
{
    if (text == "unknown")
    {
        return codeqa::core::Language::Unknown;
    }

    const auto language = codeqa::core::languageFromJsonString(text);
    if (!language.has_value())
    {
        failRequired("'" + path + "' has an unsupported language value '" + text + "'.");
    }
    return *language;
}

codeqa::model::AnalysisState parseState(const std::string& text, const std::string& path)
{
    if (text == "complete")
    {
        return codeqa::model::AnalysisState::Complete;
    }
    if (text == "partial")
    {
        return codeqa::model::AnalysisState::Partial;
    }
    if (text == "unavailable")
    {
        return codeqa::model::AnalysisState::Unavailable;
    }
    failRequired("'" + path + "' has an unsupported analysis state '" + text + "'.");
}

codeqa::core::DiagnosticSeverity parseSeverity(const std::string& text, const std::string& path)
{
    const auto severity = codeqa::core::diagnosticSeverityFromJsonString(text);
    if (!severity.has_value())
    {
        failRequired("'" + path + "' has an unsupported severity value '" + text + "'.");
    }
    return *severity;
}

codeqa::core::Diagnostic parseDiagnostic(const Json& document, const std::string& path, const std::filesystem::path& filePath)
{
    if (!document.is_object())
    {
        failRequired("'" + path + "' must be an object.");
    }

    codeqa::core::Diagnostic diagnostic;
    diagnostic.code = requireString(document, "code", path);
    diagnostic.severity = parseSeverity(requireString(document, "severity", path), path + ".severity");
    diagnostic.message = requireString(document, "message", path);
    diagnostic.location = parseLocation(requireObject(document, "location", path), path + ".location");
    diagnostic.location.filePath = filePath;
    return diagnostic;
}

codeqa::metrics::FunctionMetricsModel parseMetrics(const Json& document, const std::string& path)
{
    if (!document.is_object())
    {
        failRequired("'" + path + "' must be an object.");
    }

    codeqa::metrics::FunctionMetricsModel metrics;
    metrics.symbol = parseSymbol(requireObject(document, "symbol", path), path + ".symbol");

    const Json& size = requireObject(document, "size", path);
    metrics.size.physicalLines = requireSize(size, "physicalLines", path + ".size");
    metrics.size.codeLines = requireSize(size, "codeLines", path + ".size");
    metrics.size.blankLines = requireSize(size, "blankLines", path + ".size");
    metrics.size.commentLines = requireSize(size, "commentLines", path + ".size");
    metrics.size.statementCount = requireSize(size, "statementCount", path + ".size");

    const Json& signature = requireObject(document, "signature", path);
    metrics.signature.parameterCount = requireSize(signature, "parameterCount", path + ".signature");
    metrics.signature.signatureLength = requireSize(signature, "signatureLength", path + ".signature");
    metrics.signature.booleanParameterCount = requireSize(signature, "booleanParameterCount", path + ".signature");

    const Json& complexity = requireObject(document, "complexity", path);
    metrics.complexity.cyclomaticComplexity = requireSize(complexity, "cyclomaticComplexity", path + ".complexity");
    metrics.complexity.maxConditionComplexity = requireSize(complexity, "maxConditionComplexity", path + ".complexity");
    metrics.complexity.maxNestingDepth = requireSize(complexity, "maxNestingDepth", path + ".complexity");
    metrics.complexity.branchCount = requireSize(complexity, "branchCount", path + ".complexity");
    metrics.complexity.returnCount = requireSize(complexity, "returnCount", path + ".complexity");
    return metrics;
}

codeqa::rules::Finding parseFinding(const Json& document, const std::string& path, const std::filesystem::path& filePath)
{
    if (!document.is_object())
    {
        failRequired("'" + path + "' must be an object.");
    }

    codeqa::rules::Finding finding;
    finding.ruleId = requireString(document, "ruleId", path);
    finding.severity = parseSeverity(requireString(document, "severity", path), path + ".severity");
    finding.description = requireString(document, "description", path);
    finding.recommendation = requireString(document, "recommendation", path);

    const Json& observedValue = requireField(document, "observedValue", path);
    if (!observedValue.is_null())
    {
        finding.observedValue = requireOptionalInteger(document, "observedValue", path);
    }

    const Json& limitValue = requireField(document, "limitValue", path);
    if (!limitValue.is_null())
    {
        finding.limitValue = requireOptionalInteger(document, "limitValue", path);
    }

    finding.location = parseLocation(requireObject(document, "location", path), path + ".location");
    finding.location.filePath = filePath;
    finding.symbolName = requireString(document, "symbolName", path);
    return finding;
}

ReportFile parseFile(const Json& document, const std::string& path)
{
    if (!document.is_object())
    {
        failRequired("'" + path + "' must be an object.");
    }

    ReportFile file;
    file.path = requireString(document, "path", path);
    file.language = parseLanguage(requireString(document, "language", path), path + ".language");
    file.state = parseState(requireString(document, "state", path), path + ".state");

    const Json& diagnostics = requireArray(document, "diagnostics", path);
    for (std::size_t index = 0; index < diagnostics.size(); ++index)
    {
        file.diagnostics.push_back(parseDiagnostic(
            diagnostics.at(index), path + ".diagnostics[" + std::to_string(index) + "]", file.path));
    }

    const Json& metrics = requireArray(document, "metrics", path);
    for (std::size_t index = 0; index < metrics.size(); ++index)
    {
        file.metrics.push_back(parseMetrics(metrics.at(index), path + ".metrics[" + std::to_string(index) + "]"));
    }

    const Json& findings = requireArray(document, "findings", path);
    for (std::size_t index = 0; index < findings.size(); ++index)
    {
        file.findings.push_back(parseFinding(
            findings.at(index), path + ".findings[" + std::to_string(index) + "]", file.path));
    }

    return file;
}

codeqa::duplication::DuplicateMember parseDuplicateMember(const Json& document, const std::string& path)
{
    if (!document.is_object())
    {
        failRequired("'" + path + "' must be an object.");
    }

    codeqa::duplication::DuplicateMember member;
    member.qualifiedName = requireString(document, "qualifiedName", path);
    const std::string filePath = requireString(document, "path", path);
    member.range = parseRange(requireObject(document, "range", path), path + ".range");
    member.range.begin.filePath = filePath;
    member.range.end.filePath = filePath;
    return member;
}

codeqa::duplication::DuplicateGroup parseDuplicateGroup(const Json& document, const std::string& path)
{
    if (!document.is_object())
    {
        failRequired("'" + path + "' must be an object.");
    }

    codeqa::duplication::DuplicateGroup group;
    const Json& members = requireArray(document, "members", path);
    for (std::size_t index = 0; index < members.size(); ++index)
    {
        group.members.push_back(
            parseDuplicateMember(members.at(index), path + ".members[" + std::to_string(index) + "]"));
    }
    return group;
}

} // namespace

ReportDocument makeReportDocument(
    const RunAnalysisResult& analysis,
    const codeqa::config::ProjectConfig& config,
    const std::string& toolVersion,
    const QualityGateOutcome qualityGateOutcome,
    const std::optional<ReportComparison>& comparison)
{
    if (analysis.metrics.files.size() != analysis.model.files.size()
        || analysis.findings.files.size() != analysis.model.files.size())
    {
        throw std::invalid_argument("makeReportDocument: model/metrics/findings must be index-aligned.");
    }

    ReportDocument document;
    document.metadata.rootPath = analysis.model.rootPath.generic_string();
    document.metadata.toolName = "codeqa";
    document.metadata.toolVersion = toolVersion;
    document.config = config;
    document.comparison = comparison;
    document.qualityGateOutcome = qualityGateOutcome;
    document.duplicates = analysis.duplicates;

    for (std::size_t index = 0; index < analysis.model.files.size(); ++index)
    {
        const codeqa::model::FileModel& sourceFile = analysis.model.files[index];
        ReportFile file;
        file.path = std::filesystem::relative(sourceFile.path, analysis.model.rootPath);
        file.language = sourceFile.language;
        file.state = sourceFile.state;
        file.diagnostics = sourceFile.diagnostics;
        for (codeqa::core::Diagnostic& diagnostic : file.diagnostics)
        {
            diagnostic.location.filePath = file.path;
        }
        file.metrics = analysis.metrics.files[index].functions;
        file.findings = analysis.findings.files[index].findings;
        file.suppressedFindings = analysis.findings.files[index].suppressedFindings;
        for (codeqa::rules::Finding& finding : file.findings)
        {
            finding.location.filePath = file.path;
        }
        for (codeqa::rules::Finding& finding : file.suppressedFindings)
        {
            finding.location.filePath = file.path;
        }
        document.files.push_back(std::move(file));
    }

    return document;
}

ReportDocumentResult parseReportDocument(const nlohmann::json& document)
{
    try
    {
        if (!document.is_object())
        {
            failRequired("'report' must be an object.");
        }

        const Json& schemaVersion = requireField(document, "schemaVersion", "report");
        if (!schemaVersion.is_number_integer() && !schemaVersion.is_number_unsigned())
        {
            failRequired("'report.schemaVersion' must be an integer.");
        }

        const std::string versionText = schemaVersion.is_number_unsigned()
            ? std::to_string(schemaVersion.get<unsigned long long>())
            : std::to_string(schemaVersion.get<long long>());
        if (versionText != "1")
        {
            return ReportDocumentResult{
                std::nullopt,
                "Error [REPORT003]: unsupported schemaVersion " + versionText + "; expected 1."};
        }

        ReportDocument result;
        const Json& metadata = requireObject(document, "metadata", "report");
        result.metadata.rootPath = requireString(metadata, "rootPath", "report.metadata");
        result.metadata.toolName = requireString(metadata, "toolName", "report.metadata");
        result.metadata.toolVersion = requireString(metadata, "toolVersion", "report.metadata");

        const Json& config = requireField(document, "config", "report");
        const codeqa::config::ParseConfigResult configResult = codeqa::config::parseConfig(config.dump());
        if (!configResult.config.has_value())
        {
            failRequired("'report.config' is invalid: " + configResult.error.value_or("unknown config error"));
        }
        result.config = *configResult.config;

        const Json& files = requireArray(document, "files", "report");
        for (std::size_t index = 0; index < files.size(); ++index)
        {
            result.files.push_back(parseFile(files.at(index), "report.files[" + std::to_string(index) + "]"));
        }

        if (document.contains("duplicates"))
        {
            const Json& duplicates = requireArray(document, "duplicates", "report");
            for (std::size_t index = 0; index < duplicates.size(); ++index)
            {
                result.duplicates.groups.push_back(parseDuplicateGroup(
                    duplicates.at(index), "report.duplicates[" + std::to_string(index) + "]"));
            }
        }

        return ReportDocumentResult{std::move(result), std::nullopt};
    }
    catch (const ReportParseFailure& error)
    {
        return ReportDocumentResult{std::nullopt, error.what()};
    }
    catch (const Json::exception& error)
    {
        return ReportDocumentResult{
            std::nullopt, std::string("Error [REPORT004]: malformed report field: ") + error.what()};
    }
}

ReportDocumentResult loadReportDocument(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input.is_open())
    {
        return ReportDocumentResult{
            std::nullopt, "Error [REPORT001]: unable to read report '" + path.generic_string() + "'."};
    }

    try
    {
        return parseReportDocument(Json::parse(input));
    }
    catch (const Json::parse_error& error)
    {
        return ReportDocumentResult{
            std::nullopt, std::string("Error [REPORT002]: invalid JSON report: ") + error.what()};
    }
}

} // namespace codeqa::application
