#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace codeqa::application
{

[[nodiscard]] bool isValidSnapshotTimestamp(const std::string& timestamp);

class DeleteSnapshot
{
public:
    [[nodiscard]] std::optional<std::string> execute(
        const std::filesystem::path& projectRoot, const std::string& timestamp) const;
};

} // namespace codeqa::application
