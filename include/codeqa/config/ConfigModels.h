#pragma once

#include "codeqa/core/Language.h"

#include <map>
#include <string>
#include <vector>

namespace codeqa::config
{

struct RuleConfig
{
    bool enabled{true};
    long long warningLimit{0};
    long long highLimit{0};
};

struct HeaderOverrideConfig
{
    std::string pattern;
    codeqa::core::Language language{codeqa::core::Language::Unknown};
};

struct ProjectConfig
{
    std::vector<HeaderOverrideConfig> headerOverrides;
    std::map<std::string, RuleConfig> rules;
};

} // namespace codeqa::config
