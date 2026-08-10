#pragma once

#include "codeqa/config/ConfigModels.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace codeqa::config
{

struct ParseConfigResult
{
    std::optional<ProjectConfig> config;
    std::optional<std::string> error;
};

[[nodiscard]] ParseConfigResult parseConfig(const std::string& jsonText);
[[nodiscard]] nlohmann::json toJson(const ProjectConfig& config);

} // namespace codeqa::config
