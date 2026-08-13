#include "codeqa/config/MergeConfig.h"

namespace codeqa::config
{

ProjectConfig mergeConfig(const ProjectConfig& defaults, const ProjectConfig& user)
{
    ProjectConfig merged = defaults;

    for (const auto& [id, ruleConfig] : user.rules)
    {
        merged.rules[id] = ruleConfig;
    }

    merged.headerOverrides = user.headerOverrides;
    merged.qualityGate = user.qualityGate;

    return merged;
}

} // namespace codeqa::config
