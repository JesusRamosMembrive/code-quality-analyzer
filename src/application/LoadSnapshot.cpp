#include "codeqa/application/LoadSnapshot.h"

#include "codeqa/infrastructure/ReadFileContents.h"

#include "SnapshotPath.h"

namespace codeqa::application
{

std::optional<nlohmann::json> LoadSnapshot::execute(
    const std::filesystem::path& rootPath, const std::string& timestamp) const
{
    const SnapshotPathResolution resolution = resolveSnapshotPath(rootPath, timestamp);
    if (!resolution.path.has_value())
    {
        return std::nullopt;
    }

    const std::optional<std::string> content =
        codeqa::infrastructure::readFileContents(*resolution.path);

    if (!content.has_value())
    {
        return std::nullopt;
    }

    try
    {
        return nlohmann::json::parse(*content);
    }
    catch (const nlohmann::json::parse_error&)
    {
        return std::nullopt;
    }
}

} // namespace codeqa::application
