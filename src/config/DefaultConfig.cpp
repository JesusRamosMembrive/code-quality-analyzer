#include "codeqa/config/DefaultConfig.h"

namespace codeqa::config
{

namespace
{

RuleConfig thresholds(long long warningLimit, long long highLimit)
{
    RuleConfig config;
    config.enabled = true;
    config.warningLimit = warningLimit;
    config.highLimit = highLimit;
    return config;
}

} // namespace

ProjectConfig defaultConfig()
{
    ProjectConfig config;
    config.rules["size.function-length"] = thresholds(50, 90);
    config.rules["size.local-variables"] = thresholds(8, 12);
    config.rules["signature.parameter-count"] = thresholds(4, 7);
    config.rules["signature.consecutive-booleans"] = thresholds(2, 3);
    config.rules["complexity.cyclomatic"] = thresholds(10, 15);
    config.rules["complexity.returns"] = thresholds(4, 6);
    config.rules["complexity.condition"] = thresholds(3, 5);
    config.rules["complexity.nesting"] = thresholds(3, 5);
    config.rules["security.unsafe-string-api"] = thresholds(0, 0);
    config.rules["memory.uninitialized-pointer"] = thresholds(0, 0);
    config.rules["memory.array-parameter-decay"] = thresholds(0, 0);
    return config;
}

} // namespace codeqa::config
