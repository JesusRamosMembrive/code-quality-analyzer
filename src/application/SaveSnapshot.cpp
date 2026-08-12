#include "codeqa/application/SaveSnapshot.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace codeqa::application
{

namespace
{

std::string formatTimestamp(std::chrono::system_clock::time_point now)
{
    const std::time_t time = std::chrono::system_clock::to_time_t(now);

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
    const std::tm* utc = std::gmtime(&time);
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    std::ostringstream stream;
    stream << std::put_time(utc, "%Y%m%dT%H%M%SZ");
    return stream.str();
}

} // namespace

std::optional<std::filesystem::path> SaveSnapshot::execute(
    const std::filesystem::path& rootPath, const nlohmann::json& reportJson) const
{
    const std::filesystem::path snapshotsDirectory = rootPath / ".codeqa" / "snapshots";

    std::error_code createError;
    std::filesystem::create_directories(snapshotsDirectory, createError);

    if (createError)
    {
        return std::nullopt;
    }

    const std::filesystem::path snapshotPath =
        snapshotsDirectory / (formatTimestamp(std::chrono::system_clock::now()) + ".json");

    std::ofstream snapshotFile(snapshotPath);

    if (!snapshotFile)
    {
        return std::nullopt;
    }

    snapshotFile << reportJson.dump(2);

    if (!snapshotFile)
    {
        return std::nullopt;
    }

    return snapshotPath;
}

} // namespace codeqa::application
