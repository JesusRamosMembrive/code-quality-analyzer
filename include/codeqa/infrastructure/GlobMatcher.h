#pragma once

#include <filesystem>
#include <string>

namespace codeqa::infrastructure
{

[[nodiscard]] bool matchesExcludePattern(const std::string& pattern, const std::filesystem::path& relativePath);

} // namespace codeqa::infrastructure
