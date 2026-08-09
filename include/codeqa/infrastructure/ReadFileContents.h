#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace codeqa::infrastructure
{

[[nodiscard]] std::optional<std::string> readFileContents(const std::filesystem::path& path);

} // namespace codeqa::infrastructure
