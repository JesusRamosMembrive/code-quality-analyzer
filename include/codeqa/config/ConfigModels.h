#pragma once

#include "codeqa/core/DiagnosticSeverity.h"
#include "codeqa/core/Language.h"

#include <cstddef>
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

struct QualityGateConfig
{
    codeqa::core::DiagnosticSeverity minSeverity{codeqa::core::DiagnosticSeverity::Warning};
};

struct DuplicationConfig
{
    bool enabled{true};
    std::size_t minNormalizedTokens{30};
};

struct LayerDependency
{
    std::string from;
    std::string to;
};

struct ArchitectureConfig
{
    bool enabled{true};
    long long fanOutWarning{10};
    long long fanOutHigh{20};
    std::map<std::string, std::vector<std::string>> layers;
    std::vector<LayerDependency> forbiddenDependencies;
};

struct ProjectConfig
{
    std::vector<HeaderOverrideConfig> headerOverrides;
    std::map<std::string, RuleConfig> rules;
    QualityGateConfig qualityGate;
    DuplicationConfig duplication;
    ArchitectureConfig architecture;
};

} // namespace codeqa::config
