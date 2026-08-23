#pragma once

#include "codeqa/config/ConfigModels.h"
#include "codeqa/rules/FileRule.h"
#include "codeqa/rules/Rule.h"

#include <memory>
#include <vector>

namespace codeqa::rules
{

/// Requires config.rules to contain an entry for each of the 12 known rule ids
/// (shared with allFileRules -- config.rules is one flat map for both Rule and
/// FileRule ids) -- guaranteed by codeqa::config::defaultConfig() and by
/// codeqa::config::mergeConfig() (which always starts from a full copy of the
/// defaults). A default-constructed codeqa::config::ProjectConfig{} does NOT
/// satisfy this and will throw std::out_of_range from config.rules.at() inside
/// this function.
[[nodiscard]] std::vector<std::unique_ptr<Rule>> allRules(const codeqa::config::ProjectConfig& config);

/// Same precondition as allRules above -- one shared config.rules map.
[[nodiscard]] std::vector<std::unique_ptr<FileRule>> allFileRules(const codeqa::config::ProjectConfig& config);

} // namespace codeqa::rules
