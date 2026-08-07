#include "doctest/doctest.h"

#include "codeqa/infrastructure/ClassifyExtension.h"
#include "codeqa/infrastructure/DefaultExclusions.h"
#include "codeqa/infrastructure/DiscoveryResult.h"

#include <string>

TEST_CASE("classifyExtension recognizes the C extension")
{
    const auto category = codeqa::infrastructure::classifyExtension(".c");

    REQUIRE(category.has_value());
    CHECK(*category == codeqa::infrastructure::ExtensionCategory::C);
}

TEST_CASE("classifyExtension recognizes C++ extensions")
{
    for (const std::string& extension : {".cc", ".cpp", ".cxx", ".hpp", ".hh", ".hxx"})
    {
        const auto category = codeqa::infrastructure::classifyExtension(extension);

        REQUIRE(category.has_value());
        CHECK(*category == codeqa::infrastructure::ExtensionCategory::Cpp);
    }
}

TEST_CASE("classifyExtension treats .h as an ambiguous header")
{
    const auto category = codeqa::infrastructure::classifyExtension(".h");

    REQUIRE(category.has_value());
    CHECK(*category == codeqa::infrastructure::ExtensionCategory::AmbiguousHeader);
}

TEST_CASE("classifyExtension returns nullopt for unsupported extensions")
{
    CHECK_FALSE(codeqa::infrastructure::classifyExtension(".py").has_value());
    CHECK_FALSE(codeqa::infrastructure::classifyExtension(".md").has_value());
    CHECK_FALSE(codeqa::infrastructure::classifyExtension("").has_value());
}

TEST_CASE("classifyExtension is case-insensitive")
{
    const auto cExtension = codeqa::infrastructure::classifyExtension(".C");
    REQUIRE(cExtension.has_value());
    CHECK(*cExtension == codeqa::infrastructure::ExtensionCategory::C);

    const auto cppExtension = codeqa::infrastructure::classifyExtension(".CPP");
    REQUIRE(cppExtension.has_value());
    CHECK(*cppExtension == codeqa::infrastructure::ExtensionCategory::Cpp);

    const auto headerExtension = codeqa::infrastructure::classifyExtension(".H");
    REQUIRE(headerExtension.has_value());
    CHECK(*headerExtension == codeqa::infrastructure::ExtensionCategory::AmbiguousHeader);
}

TEST_CASE("isDefaultExcludedDirectoryName excludes hidden directories")
{
    CHECK(codeqa::infrastructure::isDefaultExcludedDirectoryName(".git"));
    CHECK(codeqa::infrastructure::isDefaultExcludedDirectoryName(".worktrees"));
    CHECK(codeqa::infrastructure::isDefaultExcludedDirectoryName(".anything"));
}

TEST_CASE("isDefaultExcludedDirectoryName excludes build directories")
{
    CHECK(codeqa::infrastructure::isDefaultExcludedDirectoryName("build"));
    CHECK(codeqa::infrastructure::isDefaultExcludedDirectoryName("cmake-build-debug"));
    CHECK(codeqa::infrastructure::isDefaultExcludedDirectoryName("cmake-build-release"));
}

TEST_CASE("isDefaultExcludedDirectoryName excludes build directories regardless of case")
{
    CHECK(codeqa::infrastructure::isDefaultExcludedDirectoryName("Build"));
    CHECK(codeqa::infrastructure::isDefaultExcludedDirectoryName("CMake-Build-Debug"));
}

TEST_CASE("isDefaultExcludedDirectoryName allows ordinary directories")
{
    CHECK_FALSE(codeqa::infrastructure::isDefaultExcludedDirectoryName("src"));
    CHECK_FALSE(codeqa::infrastructure::isDefaultExcludedDirectoryName("include"));
    CHECK_FALSE(codeqa::infrastructure::isDefaultExcludedDirectoryName("builder"));
}

TEST_CASE("DiscoveryResult default-constructs empty")
{
    const codeqa::infrastructure::DiscoveryResult result;

    CHECK(result.files.empty());
    CHECK(result.diagnostics.empty());
    CHECK(result.excludedCount == 0);
}
