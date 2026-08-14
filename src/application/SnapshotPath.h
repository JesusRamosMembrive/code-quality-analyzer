#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace codeqa::application
{

struct SnapshotPathResolution
{
    std::optional<std::filesystem::path> path;
    std::optional<std::string> error;
};

[[nodiscard]] SnapshotPathResolution resolveSnapshotPath(
    const std::filesystem::path& projectRoot, const std::string& timestamp);

} // namespace codeqa::application
