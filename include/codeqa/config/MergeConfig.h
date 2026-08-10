#pragma once

#include "codeqa/config/ConfigModels.h"

namespace codeqa::config
{

[[nodiscard]] ProjectConfig mergeConfig(const ProjectConfig& defaults, const ProjectConfig& user);

} // namespace codeqa::config
