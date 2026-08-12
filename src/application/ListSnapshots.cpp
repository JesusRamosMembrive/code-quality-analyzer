#include "codeqa/application/ListSnapshots.h"

#include <algorithm>
#include <system_error>

namespace codeqa::application
{

std::vector<std::string> ListSnapshots::execute(const std::filesystem::path& rootPath) const
{
    const std::filesystem::path snapshotsDirectory = rootPath / ".codeqa" / "snapshots";
    std::vector<std::string> timestamps;

    std::error_code existsError;
    if (!std::filesystem::is_directory(snapshotsDirectory, existsError))
    {
        return timestamps;
    }

    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(snapshotsDirectory))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
        {
            timestamps.push_back(entry.path().stem().string());
        }
    }

    std::sort(timestamps.begin(), timestamps.end());
    return timestamps;
}

} // namespace codeqa::application
