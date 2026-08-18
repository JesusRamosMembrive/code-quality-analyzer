#include "codeqa/duplication/DetectDuplicates.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace codeqa::duplication
{

namespace
{

bool byLocation(const DuplicateMember& left, const DuplicateMember& right)
{
    if (left.range.begin.filePath != right.range.begin.filePath)
    {
        return left.range.begin.filePath < right.range.begin.filePath;
    }

    return left.range.begin.offset < right.range.begin.offset;
}

} // namespace

ProjectDuplicationModel detectDuplicates(
    const codeqa::model::ProjectModel& project, const codeqa::config::DuplicationConfig& config)
{
    ProjectDuplicationModel result;

    if (!config.enabled)
    {
        return result;
    }

    // Grouping key is the normalized sequence itself -- std::unordered_map's own hash
    // and equality (both over the real std::string) do the exact-match grouping, so
    // there is no separate collision-safety pass to write by hand.
    std::unordered_map<std::string, std::vector<DuplicateMember>> byNormalizedSequence;

    for (const codeqa::model::FileModel& file : project.files)
    {
        for (const codeqa::model::CallableModel& callable : file.callables)
        {
            if (callable.normalizedTokenCount >= config.minNormalizedTokens)
            {
                byNormalizedSequence[callable.normalizedTokenSequence].push_back(
                    DuplicateMember{callable.symbol.qualifiedName, callable.symbol.range});
            }
        }
    }

    for (auto& [sequence, members] : byNormalizedSequence)
    {
        if (members.size() < 2)
        {
            continue;
        }

        std::sort(members.begin(), members.end(), byLocation);
        result.groups.push_back(DuplicateGroup{std::move(members)});
    }

    std::stable_sort(
        result.groups.begin(), result.groups.end(), [](const DuplicateGroup& left, const DuplicateGroup& right) {
            return byLocation(left.members.front(), right.members.front());
        });

    return result;
}

} // namespace codeqa::duplication
