#include "doctest/doctest.h"

#include "codeqa/config/ConfigJson.h"
#include "codeqa/config/DefaultConfig.h"

#include <string>

TEST_CASE("parseConfig accepts a document with languages and rules")
{
    const std::string text = R"({
        "languages": { "headerOverrides": [ { "pattern": "include/c_api/**", "language": "c" } ] },
        "rules": {
            "complexity.cyclomatic": { "enabled": false, "params": { "warning": 8, "high": 12 } }
        }
    })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.config.has_value());
    CHECK_FALSE(result.error.has_value());
    REQUIRE(result.config->headerOverrides.size() == 1);
    CHECK(result.config->headerOverrides[0].pattern == "include/c_api/**");
    CHECK(result.config->headerOverrides[0].language == codeqa::core::Language::C);
    REQUIRE(result.config->rules.count("complexity.cyclomatic") == 1);
    CHECK_FALSE(result.config->rules.at("complexity.cyclomatic").enabled);
    CHECK(result.config->rules.at("complexity.cyclomatic").warningLimit == 8);
    CHECK(result.config->rules.at("complexity.cyclomatic").highLimit == 12);
}

TEST_CASE("parseConfig accepts a document with only rules")
{
    const std::string text = R"({
        "rules": { "size.function-length": { "enabled": true, "params": { "warning": 40, "high": 80 } } }
    })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.config.has_value());
    CHECK(result.config->headerOverrides.empty());
    REQUIRE(result.config->rules.size() == 1);
}

TEST_CASE("parseConfig accepts an empty document")
{
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig("{}");

    REQUIRE(result.config.has_value());
    CHECK(result.config->headerOverrides.empty());
    CHECK(result.config->rules.empty());
}

TEST_CASE("parseConfig reports CONFIG001 for malformed JSON")
{
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig("{ not json");

    CHECK_FALSE(result.config.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG001") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG002 for an unknown top-level key")
{
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(R"({ "outputFormat": "json" })");

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG002") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG002 for an unknown key inside a header override")
{
    const std::string text =
        R"({ "languages": { "headerOverrides": [ { "pattern": "a/**", "language": "c", "note": "x" } ] } })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG002") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG002 for an unknown key inside a rule entry")
{
    const std::string text = R"({
        "rules": {
            "complexity.cyclomatic": {
                "enabled": true, "params": { "warning": 8, "high": 12 }, "severity": "error"
            }
        }
    })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG002") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG003 for an unknown rule id")
{
    const std::string text =
        R"({ "rules": { "missing-override": { "enabled": true, "params": { "warning": 1, "high": 2 } } } })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG003") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG004 for a rule entry missing params")
{
    const std::string text = R"({ "rules": { "complexity.cyclomatic": { "enabled": true } } })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG004") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG004 for params missing high")
{
    const std::string text =
        R"({ "rules": { "complexity.cyclomatic": { "enabled": true, "params": { "warning": 8 } } } })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG004") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG005 for a non-boolean enabled field")
{
    const std::string text = R"({
        "rules": { "complexity.cyclomatic": { "enabled": "yes", "params": { "warning": 8, "high": 12 } } }
    })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG005") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG006 for an invalid language value")
{
    const std::string text =
        R"({ "languages": { "headerOverrides": [ { "pattern": "a/**", "language": "rust" } ] } })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG006") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG008 when warning is greater than high")
{
    const std::string text =
        R"({ "rules": { "complexity.cyclomatic": { "enabled": true, "params": { "warning": 20, "high": 10 } } } })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG008") != std::string::npos);
}

TEST_CASE("toJson round-trips the default configuration through parseConfig")
{
    const codeqa::config::ProjectConfig original = codeqa::config::defaultConfig();
    const std::string serialized = codeqa::config::toJson(original).dump();

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(serialized);

    REQUIRE(result.config.has_value());
    CHECK(result.config->rules.size() == original.rules.size());

    for (const auto& [id, ruleConfig] : original.rules)
    {
        REQUIRE(result.config->rules.count(id) == 1);
        CHECK(result.config->rules.at(id).enabled == ruleConfig.enabled);
        CHECK(result.config->rules.at(id).warningLimit == ruleConfig.warningLimit);
        CHECK(result.config->rules.at(id).highLimit == ruleConfig.highLimit);
    }
}
