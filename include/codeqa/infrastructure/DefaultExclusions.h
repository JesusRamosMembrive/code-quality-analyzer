#pragma once

#include <string>

namespace codeqa::infrastructure
{

[[nodiscard]] bool isDefaultExcludedDirectoryName(const std::string& directoryName);

} // namespace codeqa::infrastructure
