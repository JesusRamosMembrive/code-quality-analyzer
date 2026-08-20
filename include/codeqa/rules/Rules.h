#pragma once

#include "codeqa/config/ConfigModels.h"
#include "codeqa/rules/Rule.h"

#include <memory>
#include <vector>

namespace codeqa::rules
{

/// Requires config.rules to contain an entry for each of the 10 known rule ids --
/// guaranteed by codeqa::config::defaultConfig() and by codeqa::config::mergeConfig()
/// (which always starts from a full copy of the defaults). A default-constructed
/// codeqa::config::ProjectConfig{} does NOT satisfy this and will throw
/// std::out_of_range from config.rules.at() inside this function.
[[nodiscard]] std::vector<std::unique_ptr<Rule>> allRules(const codeqa::config::ProjectConfig& config);

} // namespace codeqa::rules
