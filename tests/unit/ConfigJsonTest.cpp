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

TEST_CASE("parseConfig reports CONFIG005 for a warning value out of range for a signed integer")
{
    const std::string text = R"({
        "rules": {
            "complexity.cyclomatic": {
                "enabled": true, "params": { "warning": 18446744073709551615, "high": 5 }
            }
        }
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

TEST_CASE("toJson round-trips header overrides for both C and C++ through parseConfig")
{
    codeqa::config::ProjectConfig original;
    original.headerOverrides.push_back({"include/c_api/**", codeqa::core::Language::C});
    original.headerOverrides.push_back({"include/cpp_api/**", codeqa::core::Language::Cpp});

    const std::string serialized = codeqa::config::toJson(original).dump();

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(serialized);

    REQUIRE(result.config.has_value());
    REQUIRE(result.config->headerOverrides.size() == 2);
    CHECK(result.config->headerOverrides[0].pattern == "include/c_api/**");
    CHECK(result.config->headerOverrides[0].language == codeqa::core::Language::C);
    CHECK(result.config->headerOverrides[1].pattern == "include/cpp_api/**");
    CHECK(result.config->headerOverrides[1].language == codeqa::core::Language::Cpp);
}

TEST_CASE("parseConfig accepts a document with qualityGate")
{
    const std::string text = R"({ "qualityGate": { "minSeverity": "error" } })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.config.has_value());
    CHECK(result.config->qualityGate.minSeverity == codeqa::core::DiagnosticSeverity::Error);
}

TEST_CASE("parseConfig defaults qualityGate.minSeverity to Warning when the object is present but empty")
{
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(R"({ "qualityGate": {} })");

    REQUIRE(result.config.has_value());
    CHECK(result.config->qualityGate.minSeverity == codeqa::core::DiagnosticSeverity::Warning);
}

TEST_CASE("parseConfig defaults qualityGate.minSeverity to Warning when qualityGate is absent")
{
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig("{}");

    REQUIRE(result.config.has_value());
    CHECK(result.config->qualityGate.minSeverity == codeqa::core::DiagnosticSeverity::Warning);
}

TEST_CASE("parseConfig reports CONFIG009 for an invalid qualityGate.minSeverity value")
{
    const std::string text = R"({ "qualityGate": { "minSeverity": "critical" } })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG009") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG002 for an unknown key inside qualityGate")
{
    const std::string text = R"({ "qualityGate": { "minSeverity": "error", "note": "x" } })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG002") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG002 for an unknown top-level key next to qualityGate")
{
    const std::string text = R"({ "qualityGate": { "minSeverity": "error" }, "outputFormat": "json" })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG002") != std::string::npos);
}

TEST_CASE("toJson round-trips qualityGate through parseConfig")
{
    codeqa::config::ProjectConfig original;
    original.qualityGate.minSeverity = codeqa::core::DiagnosticSeverity::Error;

    const std::string serialized = codeqa::config::toJson(original).dump();
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(serialized);

    REQUIRE(result.config.has_value());
    CHECK(result.config->qualityGate.minSeverity == codeqa::core::DiagnosticSeverity::Error);
}

TEST_CASE("parseConfig accepts a document with duplication")
{
    const std::string text = R"({ "duplication": { "enabled": false, "minNormalizedTokens": 50 } })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.config.has_value());
    CHECK_FALSE(result.config->duplication.enabled);
    CHECK(result.config->duplication.minNormalizedTokens == 50);
}

TEST_CASE("parseConfig defaults duplication fields when the object is present but empty")
{
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(R"({ "duplication": {} })");

    REQUIRE(result.config.has_value());
    CHECK(result.config->duplication.enabled);
    CHECK(result.config->duplication.minNormalizedTokens == 30);
}

TEST_CASE("parseConfig defaults duplication fields when duplication is absent")
{
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig("{}");

    REQUIRE(result.config.has_value());
    CHECK(result.config->duplication.enabled);
    CHECK(result.config->duplication.minNormalizedTokens == 30);
}

TEST_CASE("parseConfig reports CONFIG012 for a non-positive duplication.minNormalizedTokens")
{
    const std::string text = R"({ "duplication": { "minNormalizedTokens": 0 } })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG012") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG002 for an unknown key inside duplication")
{
    const std::string text = R"({ "duplication": { "enabled": true, "note": "x" } })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG002") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG002 for an unknown top-level key next to duplication")
{
    const std::string text = R"({ "duplication": { "enabled": true }, "outputFormat": "json" })";

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG002") != std::string::npos);
}

TEST_CASE("toJson round-trips duplication through parseConfig")
{
    codeqa::config::ProjectConfig original;
    original.duplication.enabled = false;
    original.duplication.minNormalizedTokens = 45;

    const std::string serialized = codeqa::config::toJson(original).dump();
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(serialized);

    REQUIRE(result.config.has_value());
    CHECK_FALSE(result.config->duplication.enabled);
    CHECK(result.config->duplication.minNormalizedTokens == 45);
}

TEST_CASE("parseConfig accepts a document with architecture")
{
    const std::string text = R"({ "architecture": { "enabled": false, "fanOutWarning": 5, "fanOutHigh": 8 } })";
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.config.has_value());
    CHECK_FALSE(result.config->architecture.enabled);
    CHECK(result.config->architecture.fanOutWarning == 5);
    CHECK(result.config->architecture.fanOutHigh == 8);
}

TEST_CASE("parseConfig defaults architecture fields when the object is present but empty")
{
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(R"({ "architecture": {} })");

    REQUIRE(result.config.has_value());
    CHECK(result.config->architecture.enabled);
    CHECK(result.config->architecture.fanOutWarning == 10);
    CHECK(result.config->architecture.fanOutHigh == 20);
}

TEST_CASE("parseConfig defaults architecture fields when architecture is absent")
{
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig("{}");

    REQUIRE(result.config.has_value());
    CHECK(result.config->architecture.enabled);
    CHECK(result.config->architecture.fanOutWarning == 10);
    CHECK(result.config->architecture.fanOutHigh == 20);
}

TEST_CASE("parseConfig reports CONFIG013 when fanOutWarning is greater than fanOutHigh")
{
    const std::string text = R"({ "architecture": { "fanOutWarning": 30, "fanOutHigh": 10 } })";
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE_FALSE(result.config.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG013") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG002 for an unknown key inside architecture")
{
    const std::string text = R"({ "architecture": { "enabled": true, "note": "x" } })";
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE_FALSE(result.config.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG002") != std::string::npos);
}

TEST_CASE("toJson round-trips architecture through parseConfig")
{
    codeqa::config::ProjectConfig original = codeqa::config::defaultConfig();
    original.architecture.enabled = false;
    original.architecture.fanOutWarning = 7;
    original.architecture.fanOutHigh = 14;

    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(codeqa::config::toJson(original).dump());

    REQUIRE(result.config.has_value());
    CHECK_FALSE(result.config->architecture.enabled);
    CHECK(result.config->architecture.fanOutWarning == 7);
    CHECK(result.config->architecture.fanOutHigh == 14);
}

TEST_CASE("parseConfig accepts architecture layers and forbiddenDependencies")
{
    const std::string text = R"({
        "architecture": {
            "layers": {"core": ["src/core/**"], "desktop": ["src/desktop/**"]},
            "forbiddenDependencies": [{"from": "core", "to": "desktop"}]
        }
    })";
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE(result.config.has_value());
    REQUIRE(result.config->architecture.layers.count("core") == 1);
    CHECK(result.config->architecture.layers.at("core") == std::vector<std::string>{"src/core/**"});
    REQUIRE(result.config->architecture.forbiddenDependencies.size() == 1);
    CHECK(result.config->architecture.forbiddenDependencies[0].from == "core");
    CHECK(result.config->architecture.forbiddenDependencies[0].to == "desktop");
}

TEST_CASE("parseConfig reports CONFIG005 when a layer's pattern list is not an array")
{
    const std::string text = R"({ "architecture": { "layers": {"core": "src/core/**"} } })";
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE_FALSE(result.config.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG005") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG005 when a layer pattern entry is not a string")
{
    const std::string text = R"({ "architecture": { "layers": {"core": [123]} } })";
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE_FALSE(result.config.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG005") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG005 when forbiddenDependencies is not an array")
{
    const std::string text = R"({ "architecture": { "forbiddenDependencies": {} } })";
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE_FALSE(result.config.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG005") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG014 when forbiddenDependencies references an undeclared 'from' layer")
{
    const std::string text = R"({
        "architecture": {
            "layers": {"desktop": ["src/desktop/**"]},
            "forbiddenDependencies": [{"from": "core", "to": "desktop"}]
        }
    })";
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE_FALSE(result.config.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG014") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG014 when forbiddenDependencies references an undeclared 'to' layer")
{
    const std::string text = R"({
        "architecture": {
            "layers": {"core": ["src/core/**"]},
            "forbiddenDependencies": [{"from": "core", "to": "desktop"}]
        }
    })";
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE_FALSE(result.config.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG014") != std::string::npos);
}

TEST_CASE("parseConfig reports CONFIG015 when a forbidden pair names the same layer twice")
{
    const std::string text = R"({
        "architecture": {
            "layers": {"core": ["src/core/**"]},
            "forbiddenDependencies": [{"from": "core", "to": "core"}]
        }
    })";
    const codeqa::config::ParseConfigResult result = codeqa::config::parseConfig(text);

    REQUIRE_FALSE(result.config.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG015") != std::string::npos);
}

TEST_CASE("toJson round-trips architecture layers and forbiddenDependencies through parseConfig")
{
    codeqa::config::ProjectConfig original = codeqa::config::defaultConfig();
    original.architecture.layers = {{"core", {"src/core/**"}}, {"desktop", {"src/desktop/**"}}};
    original.architecture.forbiddenDependencies = {{"core", "desktop"}};

    const codeqa::config::ParseConfigResult result =
        codeqa::config::parseConfig(codeqa::config::toJson(original).dump());

    REQUIRE(result.config.has_value());
    REQUIRE(result.config->architecture.layers.count("core") == 1);
    CHECK(result.config->architecture.layers.at("core") == std::vector<std::string>{"src/core/**"});
    REQUIRE(result.config->architecture.forbiddenDependencies.size() == 1);
    CHECK(result.config->architecture.forbiddenDependencies[0].from == "core");
    CHECK(result.config->architecture.forbiddenDependencies[0].to == "desktop");
}
