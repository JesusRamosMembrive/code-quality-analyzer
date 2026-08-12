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

    std::error_code constructionError;
    std::filesystem::directory_iterator it(snapshotsDirectory, constructionError);
    const std::filesystem::directory_iterator end;

    if (constructionError)
    {
        return timestamps;
    }

    while (it != end)
    {
        const std::filesystem::directory_entry entry = *it;

        std::error_code typeError;
        const bool isRegularFile = entry.is_regular_file(typeError);

        if (!typeError && isRegularFile && entry.path().extension() == ".json")
        {
            timestamps.push_back(entry.path().stem().string());
        }

        std::error_code incrementError;
        it.increment(incrementError);

        if (incrementError)
        {
            break;
        }
    }

    std::sort(timestamps.begin(), timestamps.end());
    return timestamps;
}

} // namespace codeqa::application
