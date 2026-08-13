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

TEST_CASE("mergeConfig with an empty user config keeps the default qualityGate")
{
    codeqa::config::ProjectConfig defaults;
    defaults.qualityGate.minSeverity = codeqa::core::DiagnosticSeverity::Error;

    const codeqa::config::ProjectConfig merged =
        codeqa::config::mergeConfig(defaults, codeqa::config::ProjectConfig{});

    CHECK(merged.qualityGate.minSeverity == codeqa::core::DiagnosticSeverity::Error);
}
