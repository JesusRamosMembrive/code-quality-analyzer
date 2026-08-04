#pragma once

#include "codeqa/infrastructure/ExtensionCategory.h"

#include <optional>
#include <string>

namespace codeqa::infrastructure
{

[[nodiscard]] std::optional<ExtensionCategory> classifyExtension(const std::string& extension);

} // namespace codeqa::infrastructure
