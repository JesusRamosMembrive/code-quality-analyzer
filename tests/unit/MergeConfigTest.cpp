#include "doctest/doctest.h"

#include "codeqa/config/MergeConfig.h"

TEST_CASE("mergeConfig replaces a rule mentioned by the user and keeps the rest at their default")
{
    codeqa::config::ProjectConfig defaults;
    defaults.rules["complexity.cyclomatic"] = codeqa::config::RuleConfig{true, 10, 15};
    defaults.rules["size.function-length"] = codeqa::config::RuleConfig{true, 50, 90};

    codeqa::config::ProjectConfig user;
    user.rules["complexity.cyclomatic"] = codeqa::config::RuleConfig{false, 2, 4};

    const codeqa::config::ProjectConfig merged = codeqa::config::mergeConfig(defaults, user);

    REQUIRE(merged.rules.count("complexity.cyclomatic") == 1);
    CHECK_FALSE(merged.rules.at("complexity.cyclomatic").enabled);
    CHECK(merged.rules.at("complexity.cyclomatic").warningLimit == 2);
    CHECK(merged.rules.at("complexity.cyclomatic").highLimit == 4);

    REQUIRE(merged.rules.count("size.function-length") == 1);
    CHECK(merged.rules.at("size.function-length").enabled);
    CHECK(merged.rules.at("size.function-length").warningLimit == 50);
}

TEST_CASE("mergeConfig replaces headerOverrides wholesale with the user's list")
{
    codeqa::config::ProjectConfig defaults;
    defaults.headerOverrides.push_back(codeqa::config::HeaderOverrideConfig{"a/**", codeqa::core::Language::C});

    codeqa::config::ProjectConfig user;
    user.headerOverrides.push_back(codeqa::config::HeaderOverrideConfig{"b/**", codeqa::core::Language::Cpp});

    const codeqa::config::ProjectConfig merged = codeqa::config::mergeConfig(defaults, user);

    REQUIRE(merged.headerOverrides.size() == 1);
    CHECK(merged.headerOverrides[0].pattern == "b/**");
    CHECK(merged.headerOverrides[0].language == codeqa::core::Language::Cpp);
}

TEST_CASE("mergeConfig with an empty user config reproduces the defaults exactly")
{
    codeqa::config::ProjectConfig defaults;
    defaults.rules["complexity.cyclomatic"] = codeqa::config::RuleConfig{true, 10, 15};
    defaults.headerOverrides.push_back(codeqa::config::HeaderOverrideConfig{"a/**", codeqa::core::Language::C});

    const codeqa::config::ProjectConfig merged =
        codeqa::config::mergeConfig(defaults, codeqa::config::ProjectConfig{});

    REQUIRE(merged.rules.count("complexity.cyclomatic") == 1);
    CHECK(merged.rules.at("complexity.cyclomatic").warningLimit == 10);
    CHECK(merged.headerOverrides.empty());
}

TEST_CASE("mergeConfig replaces qualityGate wholesale with the user's value")
{
    codeqa::config::ProjectConfig defaults;
    defaults.qualityGate.minSeverity = codeqa::core::DiagnosticSeverity::Warning;

    codeqa::config::ProjectConfig user;
    user.qualityGate.minSeverity = codeqa::core::DiagnosticSeverity::Error;

    const codeqa::config::ProjectConfig merged = codeqa::config::mergeConfig(defaults, user);

    CHECK(merged.qualityGate.minSeverity == codeqa::core::DiagnosticSeverity::Error);
}

TEST_CASE("mergeConfig with an empty user config resets qualityGate to the struct's own default")
{
    codeqa::config::ProjectConfig defaults;
    defaults.qualityGate.minSeverity = codeqa::core::DiagnosticSeverity::Error;

    const codeqa::config::ProjectConfig merged =
        codeqa::config::mergeConfig(defaults, codeqa::config::ProjectConfig{});

    // Wholesale replace, same as headerOverrides: an empty user config's default-constructed
    // qualityGate (minSeverity == Warning) overwrites the defaults' Error. This mirrors real
    // production usage exactly, since mergeConfig is always called as mergeConfig(defaultConfig(),
    // ...) and defaultConfig() never sets qualityGate either -- it always inherits the same
    // struct default. The two only diverge in this direct unit test's contrived defaults.
    CHECK(merged.qualityGate.minSeverity == codeqa::core::DiagnosticSeverity::Warning);
}

TEST_CASE("mergeConfig replaces duplication wholesale with the user's value")
{
    codeqa::config::ProjectConfig defaults;
    defaults.duplication.enabled = true;
    defaults.duplication.minNormalizedTokens = 30;

    codeqa::config::ProjectConfig user;
    user.duplication.enabled = false;
    user.duplication.minNormalizedTokens = 60;

    const codeqa::config::ProjectConfig merged = codeqa::config::mergeConfig(defaults, user);

    CHECK_FALSE(merged.duplication.enabled);
    CHECK(merged.duplication.minNormalizedTokens == 60);
}

TEST_CASE("mergeConfig replaces architecture wholesale with the user's value")
{
    codeqa::config::ProjectConfig defaults;
    defaults.architecture.enabled = true;
    defaults.architecture.fanOutWarning = 10;
    defaults.architecture.fanOutHigh = 20;

    codeqa::config::ProjectConfig user;
    user.architecture.enabled = false;
    user.architecture.fanOutWarning = 3;
    user.architecture.fanOutHigh = 6;

    const codeqa::config::ProjectConfig merged = codeqa::config::mergeConfig(defaults, user);

    CHECK_FALSE(merged.architecture.enabled);
    CHECK(merged.architecture.fanOutWarning == 3);
    CHECK(merged.architecture.fanOutHigh == 6);
}

TEST_CASE("mergeConfig preserves architecture layers and forbiddenDependencies through a wholesale replace")
{
    codeqa::config::ProjectConfig defaults;

    codeqa::config::ProjectConfig user;
    user.architecture.layers = {{"core", {"src/core/**"}}, {"desktop", {"src/desktop/**"}}};
    user.architecture.forbiddenDependencies = {{"core", "desktop"}};

    const codeqa::config::ProjectConfig merged = codeqa::config::mergeConfig(defaults, user);

    REQUIRE(merged.architecture.layers.count("core") == 1);
    CHECK(merged.architecture.layers.at("core") == std::vector<std::string>{"src/core/**"});
    REQUIRE(merged.architecture.forbiddenDependencies.size() == 1);
    CHECK(merged.architecture.forbiddenDependencies[0].from == "core");
    CHECK(merged.architecture.forbiddenDependencies[0].to == "desktop");
}
