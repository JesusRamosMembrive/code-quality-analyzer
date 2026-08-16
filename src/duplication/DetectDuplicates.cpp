#include "codeqa/duplication/DetectDuplicates.h"

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <utility>

namespace codeqa::duplication
{

namespace
{

struct Candidate
{
    std::string qualifiedName;
    codeqa::core::SourceRange range;
    const std::string* normalizedTokenSequence{nullptr};
};

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

    std::vector<Candidate> candidates;

    for (const codeqa::model::FileModel& file : project.files)
    {
        for (const codeqa::model::CallableModel& callable : file.callables)
        {
            if (callable.normalizedTokenCount < config.minNormalizedTokens)
            {
                continue;
            }

            candidates.push_back(Candidate{
                callable.symbol.qualifiedName, callable.symbol.range, &callable.normalizedTokenSequence});
        }
    }

    // Bucket by an in-process hash purely as a fast pre-filter -- std::hash has no
    // cross-implementation stability guarantee, so it is never serialized or compared
    // across runs. Membership within a bucket is always re-verified below by full
    // string equality, so a hash collision cannot fabricate a false duplicate group.
    std::unordered_map<std::size_t, std::vector<std::size_t>> buckets;

    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        buckets[std::hash<std::string>{}(*candidates[index].normalizedTokenSequence)].push_back(index);
    }

    for (const auto& [hash, bucket] : buckets)
    {
        std::vector<bool> grouped(bucket.size(), false);

        for (std::size_t i = 0; i < bucket.size(); ++i)
        {
            if (grouped[i])
            {
                continue;
            }

            const Candidate& base = candidates[bucket[i]];
            DuplicateGroup group;
            group.members.push_back(DuplicateMember{base.qualifiedName, base.range});
            grouped[i] = true;

            for (std::size_t j = i + 1; j < bucket.size(); ++j)
            {
                const Candidate& other = candidates[bucket[j]];
                if (!grouped[j] && *other.normalizedTokenSequence == *base.normalizedTokenSequence)
                {
                    group.members.push_back(DuplicateMember{other.qualifiedName, other.range});
                    grouped[j] = true;
                }
            }

            if (group.members.size() >= 2)
            {
                std::sort(group.members.begin(), group.members.end(), byLocation);
                result.groups.push_back(std::move(group));
            }
        }
    }

    std::sort(result.groups.begin(), result.groups.end(), [](const DuplicateGroup& left, const DuplicateGroup& right) {
        return byLocation(left.members.front(), right.members.front());
    });

    return result;
}

} // namespace codeqa::duplication
