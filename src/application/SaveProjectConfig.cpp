#include "codeqa/application/SaveProjectConfig.h"

#include "codeqa/config/ConfigJson.h"

#include <fstream>

namespace codeqa::application
{

std::optional<std::string> SaveProjectConfig::execute(
    const std::filesystem::path& projectRoot, const codeqa::config::ProjectConfig& config) const
{
    const nlohmann::json document = codeqa::config::toJson(config);
    const codeqa::config::ParseConfigResult validation = codeqa::config::parseConfig(document.dump());

    if (!validation.config.has_value())
    {
        return validation.error.value_or("Error [CONFIG010]: configuration validation failed.");
    }

    const std::filesystem::path path = projectRoot / "codeqa.json";
    std::ofstream output(path);
    if (!output.is_open())
    {
        return "Error [CONFIG011]: could not open configuration file '" + path.string() + "' for writing.";
    }

    output << document.dump(2) << '\n';
    if (!output)
    {
        return "Error [CONFIG011]: could not write configuration file '" + path.string() + "'.";
    }

    return std::nullopt;
}

} // namespace codeqa::application
