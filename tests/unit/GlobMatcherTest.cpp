#include "doctest/doctest.h"

#include "codeqa/infrastructure/GlobMatcher.h"

using codeqa::infrastructure::matchesExcludePattern;

TEST_CASE("a pattern without a slash matches the filename at any depth")
{
    CHECK(matchesExcludePattern("*_generated.cpp", "foo_generated.cpp"));
    CHECK(matchesExcludePattern("*_generated.cpp", "src/nested/foo_generated.cpp"));
    CHECK_FALSE(matchesExcludePattern("*_generated.cpp", "foo_generated.hpp"));
}

TEST_CASE("a pattern with a slash matches the full relative path")
{
    CHECK(matchesExcludePattern("*/vendor/*", "src/vendor/foo.cpp"));
    CHECK_FALSE(matchesExcludePattern("*/vendor/*", "vendor/foo.cpp"));
    CHECK_FALSE(matchesExcludePattern("*/vendor/*", "src/vendor/nested/foo.cpp"));
}

TEST_CASE("? matches exactly one character")
{
    CHECK(matchesExcludePattern("file?.cpp", "file1.cpp"));
    CHECK_FALSE(matchesExcludePattern("file?.cpp", "file12.cpp"));
    CHECK_FALSE(matchesExcludePattern("file?.cpp", "file.cpp"));
}

TEST_CASE("literal characters match exactly, including regex-special ones")
{
    CHECK(matchesExcludePattern("a.b.cpp", "a.b.cpp"));
    CHECK_FALSE(matchesExcludePattern("a.b.cpp", "aXbXcpp"));
}

TEST_CASE("a Windows-style backslash pattern is treated as a path separator")
{
    CHECK(matchesExcludePattern("third_party\\vendor\\*", "third_party/vendor/foo.cpp"));
    CHECK_FALSE(matchesExcludePattern("third_party\\vendor\\*", "third_party/other/foo.cpp"));
}

TEST_CASE("a trailing slash pattern matches the directory name at any depth")
{
    CHECK(matchesExcludePattern("vendor/", "vendor"));
    CHECK(matchesExcludePattern("vendor/", "src/vendor"));
}

TEST_CASE("a leading slash anchors the pattern without requiring a literal leading slash in the target")
{
    CHECK(matchesExcludePattern("/vendor/*", "vendor/foo.cpp"));
    CHECK_FALSE(matchesExcludePattern("/vendor/*", "src/vendor/foo.cpp"));
}

TEST_CASE("a pathological pattern that would overflow std::regex's complexity limit does not throw and simply does not match")
{
    // Chaining many "*x" segments against a target almost (but not quite) made of
    // matching characters forces the backtracking engine to explore a combinatorial
    // number of ways to partition the run before failing, which is exactly the shape
    // that trips libstdc++/MSVC's std::regex complexity guard (regex_error(error_complexity)).
    std::string pathologicalPattern;
    for (int i = 0; i < 20; ++i)
    {
        pathologicalPattern += "*x";
    }

    const std::string almostMatchingTarget = std::string(20, 'x') + "y";

    CHECK_FALSE(matchesExcludePattern(pathologicalPattern, almostMatchingTarget));
}
