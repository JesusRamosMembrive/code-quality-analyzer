#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>

namespace codeqa::application
{

class SaveSnapshot
{
public:
    /// Writes reportJson to <rootPath>/.codeqa/snapshots/<timestamp>.json, creating the
    /// directory if needed. Returns the written path, or nullopt if the write failed (disk
    /// full, permissions) -- the caller decides whether/how to warn.
    [[nodiscard]] std::optional<std::filesystem::path> execute(
        const std::filesystem::path& rootPath, const nlohmann::json& reportJson) const;
};

} // namespace codeqa::application
