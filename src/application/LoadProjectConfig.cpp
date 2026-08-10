#include "codeqa/application/LoadProjectConfig.h"

#include "codeqa/config/ConfigJson.h"
#include "codeqa/config/DefaultConfig.h"
#include "codeqa/config/MergeConfig.h"
#include "codeqa/infrastructure/ReadFileContents.h"

namespace codeqa::application
{

LoadProjectConfigResult LoadProjectConfig::execute(const std::filesystem::path& path) const
{
    const std::optional<std::string> content = codeqa::infrastructure::readFileContents(path);

    if (!content.has_value())
    {
        return LoadProjectConfigResult{
            std::nullopt, "Error [CONFIG007]: could not read configuration file '" + path.string() + "'."};
    }

    const codeqa::config::ParseConfigResult parsed = codeqa::config::parseConfig(*content);

    if (parsed.error.has_value())
    {
        return LoadProjectConfigResult{std::nullopt, parsed.error};
    }

    return LoadProjectConfigResult{
        codeqa::config::mergeConfig(codeqa::config::defaultConfig(), *parsed.config), std::nullopt};
}

} // namespace codeqa::application
