#pragma once

#include "codeqa/rules/Rule.h"

#include <memory>
#include <vector>

namespace codeqa::rules
{

[[nodiscard]] std::vector<std::unique_ptr<Rule>> allRules();

} // namespace codeqa::rules
