#include "codeqa/application/LoadSnapshot.h"

#include "codeqa/infrastructure/ReadFileContents.h"

namespace codeqa::application
{

std::optional<nlohmann::json> LoadSnapshot::execute(
    const std::filesystem::path& rootPath, const std::string& timestamp) const
{
    const std::filesystem::path snapshotPath = rootPath / ".codeqa" / "snapshots" / (timestamp + ".json");
    const std::optional<std::string> content = codeqa::infrastructure::readFileContents(snapshotPath);

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
