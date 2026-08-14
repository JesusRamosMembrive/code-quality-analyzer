#pragma once

#include "codeqa/config/ConfigModels.h"

#include <filesystem>
#include <optional>
#include <string>

namespace codeqa::application
{

class SaveProjectConfig
{
public:
    [[nodiscard]] std::optional<std::string> execute(
        const std::filesystem::path& projectRoot, const codeqa::config::ProjectConfig& config) const;
};

} // namespace codeqa::application
