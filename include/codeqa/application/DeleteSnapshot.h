#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace codeqa::application
{

class DeleteSnapshot
{
public:
    [[nodiscard]] std::optional<std::string> execute(
        const std::filesystem::path& projectRoot, const std::string& timestamp) const;
};

} // namespace codeqa::application
