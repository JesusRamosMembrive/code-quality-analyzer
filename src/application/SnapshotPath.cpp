#include "SnapshotPath.h"

#include <algorithm>
#include <cctype>
#include <system_error>
#include <vector>

namespace codeqa::application
{

namespace
{

bool allDigits(const std::string& timestamp, const std::size_t begin, const std::size_t end)
{
    for (std::size_t index = begin; index < end; ++index)
    {
        if (std::isdigit(static_cast<unsigned char>(timestamp[index])) == 0)
        {
            return false;
        }
    }
    return true;
}

int numberAt(const std::string& timestamp, const std::size_t begin, const std::size_t length)
{
    return std::stoi(timestamp.substr(begin, length));
}

bool isLeapYear(const int year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

int daysInMonth(const int year, const int month)
{
    static constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && isLeapYear(year))
    {
        return 29;
    }
    return days[month - 1];
}

bool isValidSnapshotTimestamp(const std::string& timestamp)
{
    if (timestamp.size() != 16 || timestamp[8] != 'T' || timestamp[15] != 'Z'
        || !allDigits(timestamp, 0, 8) || !allDigits(timestamp, 9, 15))
    {
        return false;
    }

    const int year = numberAt(timestamp, 0, 4);
    const int month = numberAt(timestamp, 4, 2);
    const int day = numberAt(timestamp, 6, 2);
    const int hour = numberAt(timestamp, 9, 2);
    const int minute = numberAt(timestamp, 11, 2);
    const int second = numberAt(timestamp, 13, 2);

    return year > 0 && month >= 1 && month <= 12 && day >= 1 && day <= daysInMonth(year, month)
        && hour <= 23 && minute <= 59 && second <= 59;
}

std::string normalizedComponent(const std::filesystem::path& component)
{
    std::string normalized = component.string();
#ifdef _WIN32
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](const unsigned char character)
    {
        return static_cast<char>(std::tolower(character));
    });
#endif
    return normalized;
}

bool isManagedSnapshotDirectory(
    const std::filesystem::path& canonicalRoot,
    const std::filesystem::path& canonicalSnapshots)
{
    const std::filesystem::path relative = canonicalSnapshots.lexically_relative(canonicalRoot);
    if (relative.empty() || relative.is_absolute())
    {
        return false;
    }

    std::vector<std::string> components;
    for (const std::filesystem::path& component : relative)
    {
        if (component != ".")
        {
            components.push_back(normalizedComponent(component));
        }
    }

    return components.size() == 2 && components[0] == ".codeqa" && components[1] == "snapshots";
}

SnapshotPathResolution unsafePathError(const std::filesystem::path& path)
{
    return SnapshotPathResolution{
        std::nullopt,
        "Error [SNAPSHOT006]: snapshot path '" + path.string()
            + "' is unavailable or resolves outside the project snapshot directory."};
}

} // namespace

SnapshotPathResolution resolveSnapshotPath(
    const std::filesystem::path& projectRoot, const std::string& timestamp)
{
    if (!isValidSnapshotTimestamp(timestamp))
    {
        return SnapshotPathResolution{
            std::nullopt,
            "Error [SNAPSHOT001]: invalid snapshot timestamp '" + timestamp + "'."};
    }

    const std::filesystem::path snapshotsDirectory = projectRoot / ".codeqa" / "snapshots";
    const std::filesystem::path snapshotPath = snapshotsDirectory / (timestamp + ".json");

    std::error_code rootError;
    const std::filesystem::path canonicalRoot = std::filesystem::canonical(projectRoot, rootError);
    std::error_code directoryError;
    const std::filesystem::path canonicalSnapshots =
        std::filesystem::canonical(snapshotsDirectory, directoryError);
    if (rootError || directoryError || !isManagedSnapshotDirectory(canonicalRoot, canonicalSnapshots))
    {
        return unsafePathError(snapshotPath);
    }

    std::error_code statusError;
    const std::filesystem::file_status status = std::filesystem::symlink_status(snapshotPath, statusError);
    if (statusError || status.type() == std::filesystem::file_type::symlink)
    {
        return unsafePathError(snapshotPath);
    }

    std::error_code targetError;
    const std::filesystem::path canonicalTarget = std::filesystem::canonical(snapshotPath, targetError);
    std::error_code equivalentError;
    const bool hasExpectedParent = !targetError && std::filesystem::equivalent(
        canonicalTarget.parent_path(), canonicalSnapshots, equivalentError);
    if (targetError || equivalentError || !hasExpectedParent)
    {
        return unsafePathError(snapshotPath);
    }

    return SnapshotPathResolution{canonicalTarget, std::nullopt};
}

} // namespace codeqa::application
