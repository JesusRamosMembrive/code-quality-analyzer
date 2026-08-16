#include "doctest/doctest.h"

#include "codeqa/application/AnalyzeProject.h"
#include "codeqa/config/DefaultConfig.h"
#include "codeqa/duplication/DetectDuplicates.h"

#include <algorithm>
#include <filesystem>
#include <string>

namespace
{

bool groupContains(
    const codeqa::duplication::DuplicateGroup& group, const std::string& qualifiedNameA,
    const std::string& qualifiedNameB)
{
    const bool hasA = std::any_of(
        group.members.begin(), group.members.end(),
        [&](const codeqa::duplication::DuplicateMember& member) { return member.qualifiedName == qualifiedNameA; });
    const bool hasB = std::any_of(
        group.members.begin(), group.members.end(),
        [&](const codeqa::duplication::DuplicateMember& member) { return member.qualifiedName == qualifiedNameB; });
    return hasA && hasB;
}

bool anyGroupContains(
    const codeqa::duplication::ProjectDuplicationModel& duplicates, const std::string& qualifiedNameA,
    const std::string& qualifiedNameB)
{
    return std::any_of(
        duplicates.groups.begin(), duplicates.groups.end(), [&](const codeqa::duplication::DuplicateGroup& group) {
            return groupContains(group, qualifiedNameA, qualifiedNameB);
        });
}

} // namespace

TEST_CASE("corpus duplication fixtures are detected against the whole real corpus")
{
    const std::filesystem::path corpusRoot{CODEQA_CORPUS_DIR};
    codeqa::application::AnalyzeProjectRequest request;
    request.discovery.rootPath = corpusRoot;
    const codeqa::model::ProjectModel project = codeqa::application::AnalyzeProject{}.execute(request);

    const codeqa::duplication::ProjectDuplicationModel duplicates =
        codeqa::duplication::detectDuplicates(project, codeqa::config::defaultConfig().duplication);

    CHECK(anyGroupContains(duplicates, "computeChecksumA", "computeChecksumB"));
    CHECK(anyGroupContains(duplicates, "mergeBuffersLeft", "mergeBuffersRight"));
    CHECK(anyGroupContains(duplicates, "isValidPercentage", "isValidByteValue"));
    CHECK_FALSE(anyGroupContains(duplicates, "identityValue", "passThroughValue"));
}
