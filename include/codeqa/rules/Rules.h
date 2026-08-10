#pragma once

#include "codeqa/config/ConfigModels.h"
#include "codeqa/rules/Rule.h"

#include <memory>
#include <vector>

namespace codeqa::rules
{

[[nodiscard]] std::vector<std::unique_ptr<Rule>> allRules(const codeqa::config::ProjectConfig& config);

} // namespace codeqa::rules
