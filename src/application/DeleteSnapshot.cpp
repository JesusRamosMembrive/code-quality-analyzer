#include "codeqa/application/DeleteSnapshot.h"

#include "SnapshotPath.h"

#include <system_error>

namespace codeqa::application
{

std::optional<std::string> DeleteSnapshot::execute(
    const std::filesystem::path& projectRoot, const std::string& timestamp) const
{
    const SnapshotPathResolution resolution = resolveSnapshotPath(projectRoot, timestamp);
    if (!resolution.path.has_value())
    {
        return resolution.error;
    }

    const std::filesystem::path& snapshotPath = *resolution.path;
    std::error_code typeError;
    const bool isRegularFile = std::filesystem::is_regular_file(snapshotPath, typeError);
    if (typeError)
    {
        return "Error [SNAPSHOT002]: could not inspect snapshot '" + snapshotPath.string()
            + "': " + typeError.message() + ".";
    }
    if (!isRegularFile)
    {
        return "Error [SNAPSHOT003]: snapshot '" + timestamp + "' does not exist.";
    }

    std::error_code removeError;
    const bool removed = std::filesystem::remove(snapshotPath, removeError);
    if (removeError)
    {
        return "Error [SNAPSHOT004]: could not delete snapshot '" + snapshotPath.string()
            + "': " + removeError.message() + ".";
    }
    if (!removed)
    {
        return "Error [SNAPSHOT003]: snapshot '" + timestamp + "' does not exist.";
    }

    return std::nullopt;
}

} // namespace codeqa::application
