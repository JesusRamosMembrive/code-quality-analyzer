#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace codeqa::application
{

class LoadSnapshot
{
public:
    /// nullopt if <rootPath>/.codeqa/snapshots/<timestamp>.json doesn't exist or isn't valid
    /// JSON -- both are "can't use this snapshot" for every caller this phase has.
    [[nodiscard]] std::optional<nlohmann::json> execute(
        const std::filesystem::path& rootPath, const std::string& timestamp) const;
};

} // namespace codeqa::application
