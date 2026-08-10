#pragma once

#include "codeqa/config/ConfigModels.h"

#include <filesystem>
#include <optional>
#include <string>

namespace codeqa::application
{

struct LoadProjectConfigResult
{
    std::optional<codeqa::config::ProjectConfig> config;
    std::optional<std::string> error;
};

class LoadProjectConfig
{
public:
    [[nodiscard]] LoadProjectConfigResult execute(const std::filesystem::path& path) const;
};

} // namespace codeqa::application
