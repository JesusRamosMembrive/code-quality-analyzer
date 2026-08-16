#include "doctest/doctest.h"

#include "codeqa/config/ConfigModels.h"

TEST_CASE("RuleConfig defaults to enabled with zeroed thresholds")
{
    codeqa::config::RuleConfig config;
    CHECK(config.enabled);
    CHECK(config.warningLimit == 0);
    CHECK(config.highLimit == 0);
}

TEST_CASE("HeaderOverrideConfig defaults to an empty pattern and Unknown language")
{
    codeqa::config::HeaderOverrideConfig override;
    CHECK(override.pattern.empty());
    CHECK(override.language == codeqa::core::Language::Unknown);
}

TEST_CASE("ProjectConfig defaults to empty overrides and rules")
{
    codeqa::config::ProjectConfig config;
    CHECK(config.headerOverrides.empty());
    CHECK(config.rules.empty());
}

TEST_CASE("RuleConfig is aggregate-initializable in id order")
{
    const codeqa::config::RuleConfig config{false, 2, 4};
    CHECK_FALSE(config.enabled);
    CHECK(config.warningLimit == 2);
    CHECK(config.highLimit == 4);
}

TEST_CASE("QualityGateConfig defaults minSeverity to Warning")
{
    codeqa::config::QualityGateConfig config;
    CHECK(config.minSeverity == codeqa::core::DiagnosticSeverity::Warning);
}

TEST_CASE("ProjectConfig defaults qualityGate.minSeverity to Warning")
{
    codeqa::config::ProjectConfig config;
    CHECK(config.qualityGate.minSeverity == codeqa::core::DiagnosticSeverity::Warning);
}

TEST_CASE("DuplicationConfig defaults to enabled with a 30-token floor")
{
    codeqa::config::DuplicationConfig config;
    CHECK(config.enabled);
    CHECK(config.minNormalizedTokens == 30);
}

TEST_CASE("ProjectConfig defaults duplication to enabled with a 30-token floor")
{
    codeqa::config::ProjectConfig config;
    CHECK(config.duplication.enabled);
    CHECK(config.duplication.minNormalizedTokens == 30);
}
