#include "codeqa/config/ConfigJson.h"

#include "codeqa/core/EnumJson.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>

namespace codeqa::config
{

namespace
{

using Json = nlohmann::json;

struct ParseFailure : std::runtime_error
{
    explicit ParseFailure(const std::string& message) : std::runtime_error(message) {}
};

const std::vector<std::string>& knownRuleIds()
{
    static const std::vector<std::string> ids = {
        "size.function-length",
        "size.local-variables",
        "signature.parameter-count",
        "signature.consecutive-booleans",
        "complexity.cyclomatic",
        "complexity.returns",
        "complexity.condition",
        "complexity.nesting"};
    return ids;
}

void requireObject(const Json& value, const std::string& path)
{
    if (!value.is_object())
    {
        throw ParseFailure("Error [CONFIG005]: '" + path + "' must be an object.");
    }
}

void requireKeysSubset(const Json& object, const std::vector<std::string>& allowed, const std::string& path)
{
    for (const auto& item : object.items())
    {
        if (std::find(allowed.begin(), allowed.end(), item.key()) == allowed.end())
        {
            throw ParseFailure("Error [CONFIG002]: unknown key '" + path + "." + item.key() + "'.");
        }
    }
}

std::string requireString(const Json& object, const std::string& field, const std::string& path)
{
    if (!object.contains(field))
    {
        throw ParseFailure("Error [CONFIG004]: '" + path + "' is missing required field '" + field + "'.");
    }
    if (!object.at(field).is_string())
    {
        throw ParseFailure("Error [CONFIG005]: '" + path + "." + field + "' must be a string.");
    }
    return object.at(field).get<std::string>();
}

bool requireBool(const Json& object, const std::string& field, const std::string& path)
{
    if (!object.contains(field))
    {
        throw ParseFailure("Error [CONFIG004]: '" + path + "' is missing required field '" + field + "'.");
    }
    if (!object.at(field).is_boolean())
    {
        throw ParseFailure("Error [CONFIG005]: '" + path + "." + field + "' must be a boolean.");
    }
    return object.at(field).get<bool>();
}

long long requireInteger(const Json& object, const std::string& field, const std::string& path)
{
    if (!object.contains(field))
    {
        throw ParseFailure("Error [CONFIG004]: '" + path + "' is missing required field '" + field + "'.");
    }

    const Json& value = object.at(field);

    if (!value.is_number_integer())
    {
        throw ParseFailure("Error [CONFIG005]: '" + path + "." + field + "' must be an integer.");
    }

    if (value.is_number_unsigned()
        && value.get<unsigned long long>() > static_cast<unsigned long long>(std::numeric_limits<long long>::max()))
    {
        throw ParseFailure("Error [CONFIG005]: '" + path + "." + field + "' is out of range for a signed integer.");
    }

    return value.get<long long>();
}

codeqa::core::Language parseLanguage(const std::string& text, const std::string& path)
{
    const std::optional<codeqa::core::Language> language = codeqa::core::languageFromJsonString(text);

    if (!language.has_value())
    {
        throw ParseFailure("Error [CONFIG006]: '" + path + "' must be 'c' or 'cpp', got '" + text + "'.");
    }

    return *language;
}

std::vector<HeaderOverrideConfig> parseHeaderOverrides(const Json& languages)
{
    requireKeysSubset(languages, {"headerOverrides"}, "languages");

    std::vector<HeaderOverrideConfig> overrides;

    if (!languages.contains("headerOverrides"))
    {
        return overrides;
    }

    const Json& list = languages.at("headerOverrides");

    if (!list.is_array())
    {
        throw ParseFailure("Error [CONFIG005]: 'languages.headerOverrides' must be an array.");
    }

    for (std::size_t index = 0; index < list.size(); ++index)
    {
        const std::string entryPath = "languages.headerOverrides[" + std::to_string(index) + "]";
        const Json& entry = list.at(index);
        requireObject(entry, entryPath);
        requireKeysSubset(entry, {"pattern", "language"}, entryPath);

        HeaderOverrideConfig parsedOverride;
        parsedOverride.pattern = requireString(entry, "pattern", entryPath);
        parsedOverride.language =
            parseLanguage(requireString(entry, "language", entryPath), entryPath + ".language");
        overrides.push_back(std::move(parsedOverride));
    }

    return overrides;
}

std::map<std::string, RuleConfig> parseRules(const Json& rules)
{
    std::map<std::string, RuleConfig> parsed;

    for (const auto& item : rules.items())
    {
        const std::string& id = item.key();

        if (std::find(knownRuleIds().begin(), knownRuleIds().end(), id) == knownRuleIds().end())
        {
            throw ParseFailure("Error [CONFIG003]: unknown rule id 'rules." + id + "'.");
        }

        const std::string path = "rules." + id;
        const Json& entry = item.value();
        requireObject(entry, path);
        requireKeysSubset(entry, {"enabled", "params"}, path);

        RuleConfig config;
        config.enabled = requireBool(entry, "enabled", path);

        if (!entry.contains("params"))
        {
            throw ParseFailure("Error [CONFIG004]: '" + path + "' is missing required field 'params'.");
        }

        const Json& params = entry.at("params");
        requireObject(params, path + ".params");
        requireKeysSubset(params, {"warning", "high"}, path + ".params");
        config.warningLimit = requireInteger(params, "warning", path + ".params");
        config.highLimit = requireInteger(params, "high", path + ".params");

        if (config.warningLimit > config.highLimit)
        {
            throw ParseFailure(
                "Error [CONFIG008]: '" + path + ".params' has warning (" + std::to_string(config.warningLimit)
                + ") greater than high (" + std::to_string(config.highLimit) + ").");
        }

        parsed[id] = config;
    }

    return parsed;
}

QualityGateConfig parseQualityGate(const Json& qualityGate)
{
    requireKeysSubset(qualityGate, {"minSeverity"}, "qualityGate");

    QualityGateConfig config;

    if (qualityGate.contains("minSeverity"))
    {
        const std::string text = requireString(qualityGate, "minSeverity", "qualityGate");
        const std::optional<codeqa::core::DiagnosticSeverity> severity =
            codeqa::core::diagnosticSeverityFromJsonString(text);

        if (!severity.has_value())
        {
            throw ParseFailure(
                "Error [CONFIG009]: 'qualityGate.minSeverity' must be 'information', 'warning', or 'error', got '"
                + text + "'.");
        }

        config.minSeverity = *severity;
    }

    return config;
}

DuplicationConfig parseDuplication(const Json& duplication)
{
    requireKeysSubset(duplication, {"enabled", "minNormalizedTokens"}, "duplication");

    DuplicationConfig config;

    if (duplication.contains("enabled"))
    {
        config.enabled = requireBool(duplication, "enabled", "duplication");
    }

    if (duplication.contains("minNormalizedTokens"))
    {
        const long long value = requireInteger(duplication, "minNormalizedTokens", "duplication");

        if (value <= 0)
        {
            throw ParseFailure(
                "Error [CONFIG011]: 'duplication.minNormalizedTokens' must be a positive integer, got "
                + std::to_string(value) + ".");
        }

        config.minNormalizedTokens = static_cast<std::size_t>(value);
    }

    return config;
}

} // namespace

ParseConfigResult parseConfig(const std::string& jsonText)
{
    Json document;

    try
    {
        document = Json::parse(jsonText);
    }
    catch (const Json::parse_error& error)
    {
        return ParseConfigResult{std::nullopt, std::string("Error [CONFIG001]: invalid JSON: ") + error.what()};
    }

    try
    {
        requireObject(document, "config");
        requireKeysSubset(document, {"languages", "rules", "qualityGate", "duplication"}, "config");

        ProjectConfig config;

        if (document.contains("languages"))
        {
            const Json& languages = document.at("languages");
            requireObject(languages, "languages");
            config.headerOverrides = parseHeaderOverrides(languages);
        }

        if (document.contains("rules"))
        {
            const Json& rules = document.at("rules");
            requireObject(rules, "rules");
            config.rules = parseRules(rules);
        }

        if (document.contains("qualityGate"))
        {
            const Json& qualityGate = document.at("qualityGate");
            requireObject(qualityGate, "qualityGate");
            config.qualityGate = parseQualityGate(qualityGate);
        }

        if (document.contains("duplication"))
        {
            const Json& duplication = document.at("duplication");
            requireObject(duplication, "duplication");
            config.duplication = parseDuplication(duplication);
        }

        return ParseConfigResult{config, std::nullopt};
    }
    catch (const ParseFailure& failure)
    {
        return ParseConfigResult{std::nullopt, std::string(failure.what())};
    }
    catch (const Json::exception& error)
    {
        // Safety net: any other nlohmann-internal exception not anticipated by the checks
        // above still turns into a ParseConfigResult error instead of propagating out of
        // parseConfig.
        return ParseConfigResult{std::nullopt, std::string("Error [CONFIG005]: malformed value: ") + error.what()};
    }
}

nlohmann::json toJson(const ProjectConfig& config)
{
    Json document;
    document["languages"]["headerOverrides"] = Json::array();

    for (const HeaderOverrideConfig& headerOverride : config.headerOverrides)
    {
        document["languages"]["headerOverrides"].push_back(
            {{"pattern", headerOverride.pattern},
             {"language", codeqa::core::languageToJsonString(headerOverride.language)}});
    }

    document["rules"] = Json::object();

    for (const auto& [id, ruleConfig] : config.rules)
    {
        document["rules"][id] = {
            {"enabled", ruleConfig.enabled},
            {"params", {{"warning", ruleConfig.warningLimit}, {"high", ruleConfig.highLimit}}}};
    }

    document["qualityGate"] = {
        {"minSeverity", codeqa::core::diagnosticSeverityToJsonString(config.qualityGate.minSeverity)}};

    document["duplication"] = {
        {"enabled", config.duplication.enabled},
        {"minNormalizedTokens", config.duplication.minNormalizedTokens}};

    return document;
}

} // namespace codeqa::config
