#include "doctest/doctest.h"

#include "codeqa/config/DefaultConfig.h"

#include <string>

namespace
{

void checkRule(
    const codeqa::config::ProjectConfig& config, const std::string& id, long long warning, long long high)
{
    REQUIRE(config.rules.count(id) == 1);
    const codeqa::config::RuleConfig& rule = config.rules.at(id);
    CHECK(rule.enabled);
    CHECK(rule.warningLimit == warning);
    CHECK(rule.highLimit == high);
}

} // namespace

TEST_CASE("defaultConfig ships the eight Fase 7 thresholds plus the unsafe-string-api, uninitialized-pointer, and array-parameter-decay rules, all enabled, no header overrides")
{
    const codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();

    CHECK(config.headerOverrides.empty());
    REQUIRE(config.rules.size() == 11);

    checkRule(config, "size.function-length", 50, 90);
    checkRule(config, "size.local-variables", 8, 12);
    checkRule(config, "signature.parameter-count", 4, 7);
    checkRule(config, "signature.consecutive-booleans", 2, 3);
    checkRule(config, "complexity.cyclomatic", 10, 15);
    checkRule(config, "complexity.returns", 4, 6);
    checkRule(config, "complexity.condition", 3, 5);
    checkRule(config, "complexity.nesting", 3, 5);
    checkRule(config, "security.unsafe-string-api", 0, 0);
    checkRule(config, "memory.uninitialized-pointer", 0, 0);
    checkRule(config, "memory.array-parameter-decay", 0, 0);
}
