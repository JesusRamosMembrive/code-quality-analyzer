#include "codeqa/history/CompareSnapshots.h"

#include <algorithm>
#include <filesystem>

namespace codeqa::history
{

FindingKeySet extractFindingKeys(const codeqa::rules::ProjectFindingsModel& findings)
{
    FindingKeySet keys;

    for (const codeqa::rules::FileFindingsModel& file : findings.files)
    {
        const std::string relativePath =
            std::filesystem::relative(file.path, findings.rootPath).generic_string();

        for (const codeqa::rules::Finding& finding : file.findings)
        {
            ++keys[FindingKey{finding.ruleId, finding.symbolName, relativePath}];
        }
    }

    return keys;
}

FindingKeySet extractFindingKeys(const nlohmann::json& snapshotReport)
{
    FindingKeySet keys;

    for (const nlohmann::json& file : snapshotReport.at("files"))
    {
        const std::string path = file.at("path").get<std::string>();

        for (const nlohmann::json& finding : file.at("findings"))
        {
            ++keys[FindingKey{
                finding.at("ruleId").get<std::string>(), finding.at("symbolName").get<std::string>(), path}];
        }
    }

    return keys;
}

SnapshotComparison compareSnapshots(const FindingKeySet& current, const FindingKeySet& previous)
{
    SnapshotComparison result;

    for (const auto& [key, currentCount] : current)
    {
        const auto previousEntry = previous.find(key);
        const std::size_t previousCount = previousEntry != previous.end() ? previousEntry->second : 0;
        const std::size_t matched = std::min(currentCount, previousCount);

        result.existingCount += matched;
        result.newCount += currentCount - matched;
    }

    for (const auto& [key, previousCount] : previous)
    {
        const auto currentEntry = current.find(key);
        const std::size_t currentCount = currentEntry != current.end() ? currentEntry->second : 0;
        const std::size_t matched = std::min(currentCount, previousCount);

        result.resolvedCount += previousCount - matched;
    }

    return result;
}

} // namespace codeqa::history
