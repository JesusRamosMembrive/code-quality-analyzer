#include "codeqa/application/DeleteSnapshot.h"

#include <cctype>
#include <system_error>

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

} // namespace

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

std::optional<std::string> DeleteSnapshot::execute(
    const std::filesystem::path& projectRoot, const std::string& timestamp) const
{
    if (!isValidSnapshotTimestamp(timestamp))
    {
        return "Error [SNAPSHOT001]: invalid snapshot timestamp '" + timestamp + "'.";
    }

    const std::filesystem::path snapshotPath =
        projectRoot / ".codeqa" / "snapshots" / (timestamp + ".json");

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
