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
