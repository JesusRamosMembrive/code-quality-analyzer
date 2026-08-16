#include "doctest/doctest.h"

#include "codeqa/duplication/DetectDuplicates.h"

#include <filesystem>
#include <string>

namespace
{

codeqa::core::SourceRange rangeAt(const std::filesystem::path& path, std::size_t offset)
{
    return codeqa::core::SourceRange{
        codeqa::core::SourceLocation{path, 1, offset + 1, offset},
        codeqa::core::SourceLocation{path, 1, offset + 41, offset + 40}};
}

codeqa::model::CallableModel makeCallable(
    const std::string& qualifiedName, const std::filesystem::path& path, std::size_t offset,
    const std::string& normalizedTokenSequence, std::size_t normalizedTokenCount)
{
    codeqa::model::CallableModel callable;
    callable.symbol.name = qualifiedName;
    callable.symbol.qualifiedName = qualifiedName;
    callable.symbol.range = rangeAt(path, offset);
    callable.normalizedTokenSequence = normalizedTokenSequence;
    callable.normalizedTokenCount = normalizedTokenCount;
    return callable;
}

} // namespace

TEST_CASE("detectDuplicates groups two callables with an identical normalized sequence")
{
    codeqa::model::ProjectModel project;
    codeqa::model::FileModel file;
    file.path = "project/src/a.cpp";
    file.callables.push_back(makeCallable("first", file.path, 0, "int ID ( int ID ) { return ID ; }", 9));
    file.callables.push_back(makeCallable("second", file.path, 100, "int ID ( int ID ) { return ID ; }", 9));
    project.files.push_back(file);

    codeqa::config::DuplicationConfig config;
    config.minNormalizedTokens = 5;

    const codeqa::duplication::ProjectDuplicationModel result =
        codeqa::duplication::detectDuplicates(project, config);

    REQUIRE(result.groups.size() == 1);
    REQUIRE(result.groups.front().members.size() == 2);
    CHECK(result.groups.front().members[0].qualifiedName == "first");
    CHECK(result.groups.front().members[1].qualifiedName == "second");
}

TEST_CASE("detectDuplicates ignores a callable below the minimum token floor")
{
    codeqa::model::ProjectModel project;
    codeqa::model::FileModel file;
    file.path = "project/src/a.cpp";
    file.callables.push_back(makeCallable("tiny", file.path, 0, "int ID ( ) { return LIT ; }", 7));
    project.files.push_back(file);

    codeqa::config::DuplicationConfig config;
    config.minNormalizedTokens = 30;

    const codeqa::duplication::ProjectDuplicationModel result =
        codeqa::duplication::detectDuplicates(project, config);

    CHECK(result.groups.empty());
}

TEST_CASE("detectDuplicates does not group two callables with different normalized sequences")
{
    codeqa::model::ProjectModel project;
    codeqa::model::FileModel file;
    file.path = "project/src/a.cpp";
    file.callables.push_back(makeCallable("add", file.path, 0, "int ID ( int ID ) { return ID + ID ; }", 10));
    file.callables.push_back(makeCallable("sub", file.path, 100, "int ID ( int ID ) { return ID - ID ; }", 10));
    project.files.push_back(file);

    codeqa::config::DuplicationConfig config;
    config.minNormalizedTokens = 5;

    const codeqa::duplication::ProjectDuplicationModel result =
        codeqa::duplication::detectDuplicates(project, config);

    CHECK(result.groups.empty());
}

TEST_CASE("detectDuplicates finds duplicates across two different files")
{
    codeqa::model::ProjectModel project;
    codeqa::model::FileModel fileA;
    fileA.path = "project/src/a.cpp";
    fileA.callables.push_back(makeCallable("a::run", fileA.path, 0, "int ID ( ) { return LIT ; }", 7));
    codeqa::model::FileModel fileB;
    fileB.path = "project/src/b.cpp";
    fileB.callables.push_back(makeCallable("b::run", fileB.path, 0, "int ID ( ) { return LIT ; }", 7));
    project.files.push_back(fileA);
    project.files.push_back(fileB);

    codeqa::config::DuplicationConfig config;
    config.minNormalizedTokens = 5;

    const codeqa::duplication::ProjectDuplicationModel result =
        codeqa::duplication::detectDuplicates(project, config);

    REQUIRE(result.groups.size() == 1);
    CHECK(result.groups.front().members.size() == 2);
}

TEST_CASE("detectDuplicates groups three callables that all share the same sequence")
{
    codeqa::model::ProjectModel project;
    codeqa::model::FileModel file;
    file.path = "project/src/a.cpp";
    file.callables.push_back(makeCallable("one", file.path, 0, "int ID ( ) { return LIT ; }", 7));
    file.callables.push_back(makeCallable("two", file.path, 100, "int ID ( ) { return LIT ; }", 7));
    file.callables.push_back(makeCallable("three", file.path, 200, "int ID ( ) { return LIT ; }", 7));
    project.files.push_back(file);

    codeqa::config::DuplicationConfig config;
    config.minNormalizedTokens = 5;

    const codeqa::duplication::ProjectDuplicationModel result =
        codeqa::duplication::detectDuplicates(project, config);

    REQUIRE(result.groups.size() == 1);
    CHECK(result.groups.front().members.size() == 3);
}

TEST_CASE("detectDuplicates returns nothing when disabled")
{
    codeqa::model::ProjectModel project;
    codeqa::model::FileModel file;
    file.path = "project/src/a.cpp";
    file.callables.push_back(makeCallable("first", file.path, 0, "int ID ( ) { return LIT ; }", 7));
    file.callables.push_back(makeCallable("second", file.path, 100, "int ID ( ) { return LIT ; }", 7));
    project.files.push_back(file);

    codeqa::config::DuplicationConfig config;
    config.enabled = false;
    config.minNormalizedTokens = 1;

    const codeqa::duplication::ProjectDuplicationModel result =
        codeqa::duplication::detectDuplicates(project, config);

    CHECK(result.groups.empty());
}
