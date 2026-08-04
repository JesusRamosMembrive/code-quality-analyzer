#include "doctest/doctest.h"

#include "codeqa/infrastructure/DiscoverSourceFiles.h"

#include <filesystem>
#include <fstream>
#include <set>
#include <vector>

namespace
{

class TempDirectory
{
public:
    TempDirectory()
        : path_(std::filesystem::temp_directory_path() / "codeqa_discover_source_files_test")
    {
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }

    ~TempDirectory()
    {
        std::filesystem::remove_all(path_);
    }

    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const
    {
        return path_;
    }

    void createFile(const std::filesystem::path& relativePath, const std::string& content = "") const
    {
        const std::filesystem::path fullPath = path_ / relativePath;
        std::filesystem::create_directories(fullPath.parent_path());
        std::ofstream(fullPath) << content;
    }

private:
    std::filesystem::path path_;
};

std::set<std::filesystem::path> discoveredPaths(const codeqa::infrastructure::DiscoveryResult& result)
{
    std::set<std::filesystem::path> paths;

    for (const codeqa::infrastructure::DiscoveredFile& file : result.files)
    {
        paths.insert(file.path);
    }

    return paths;
}

} // namespace

TEST_CASE("discoverSourceFiles finds supported files and classifies them by extension")
{
    TempDirectory root;
    root.createFile("main.c");
    root.createFile("util.cpp");
    root.createFile("util.h");
    root.createFile("notes.txt");

    codeqa::infrastructure::DiscoveryOptions options;
    options.rootPath = root.path();

    const codeqa::infrastructure::DiscoveryResult result = codeqa::infrastructure::discoverSourceFiles(options);

    REQUIRE(result.files.size() == 3);
    CHECK(discoveredPaths(result)
        == std::set<std::filesystem::path>{
            std::filesystem::path{"main.c"},
            std::filesystem::path{"util.cpp"},
            std::filesystem::path{"util.h"}});
    CHECK(result.diagnostics.empty());
}

TEST_CASE("discoverSourceFiles excludes default directories without descending into them")
{
    TempDirectory root;
    root.createFile("main.c");
    root.createFile(".git/HEAD");
    root.createFile("build/generated.cpp");
    root.createFile("cmake-build-debug/generated.cpp");

    codeqa::infrastructure::DiscoveryOptions options;
    options.rootPath = root.path();

    const codeqa::infrastructure::DiscoveryResult result = codeqa::infrastructure::discoverSourceFiles(options);

    REQUIRE(result.files.size() == 1);
    CHECK(result.files.front().path == std::filesystem::path{"main.c"});
    CHECK(result.excludedCount == 3);
}

TEST_CASE("discoverSourceFiles applies --exclude glob patterns")
{
    TempDirectory root;
    root.createFile("main.cpp");
    // Nested under src/ (not at the root) because the "*/vendor/*" glob requires a path
    // segment on both sides of "vendor" per GlobMatcher's contract (see GlobMatcherTest.cpp:
    // CHECK_FALSE(matchesExcludePattern("*/vendor/*", "vendor/foo.cpp"))). A root-level
    // "vendor/lib.cpp" would not match and this test would incorrectly fail.
    root.createFile("src/vendor/lib.cpp");
    root.createFile("main_generated.cpp");

    codeqa::infrastructure::DiscoveryOptions options;
    options.rootPath = root.path();
    options.excludePatterns = {"*/vendor/*", "*_generated.cpp"};

    const codeqa::infrastructure::DiscoveryResult result = codeqa::infrastructure::discoverSourceFiles(options);

    REQUIRE(result.files.size() == 1);
    CHECK(result.files.front().path == std::filesystem::path{"main.cpp"});
    CHECK(result.excludedCount == 2);
}

TEST_CASE("discoverSourceFiles returns files in deterministic order")
{
    TempDirectory root;
    root.createFile("z.c");
    root.createFile("a.c");
    root.createFile("m.c");

    codeqa::infrastructure::DiscoveryOptions options;
    options.rootPath = root.path();

    const codeqa::infrastructure::DiscoveryResult firstRun = codeqa::infrastructure::discoverSourceFiles(options);
    const codeqa::infrastructure::DiscoveryResult secondRun = codeqa::infrastructure::discoverSourceFiles(options);

    std::vector<std::filesystem::path> firstPaths;
    for (const codeqa::infrastructure::DiscoveredFile& file : firstRun.files)
    {
        firstPaths.push_back(file.path);
    }

    CHECK(firstPaths
        == std::vector<std::filesystem::path>{
            std::filesystem::path{"a.c"}, std::filesystem::path{"m.c"}, std::filesystem::path{"z.c"}});

    std::vector<std::filesystem::path> secondPaths;
    for (const codeqa::infrastructure::DiscoveredFile& file : secondRun.files)
    {
        secondPaths.push_back(file.path);
    }

    CHECK(firstPaths == secondPaths);
}

TEST_CASE("discoverSourceFiles includes empty files with no size limit")
{
    TempDirectory root;
    root.createFile("empty.c", "");

    codeqa::infrastructure::DiscoveryOptions options;
    options.rootPath = root.path();

    const codeqa::infrastructure::DiscoveryResult result = codeqa::infrastructure::discoverSourceFiles(options);

    REQUIRE(result.files.size() == 1);
    CHECK(result.files.front().sizeBytes == 0);
}

TEST_CASE("discoverSourceFiles does not descend into a symlinked directory")
{
    TempDirectory root;
    root.createFile("main.c");
    root.createFile("real/inner.cpp");

    std::error_code symlinkError;
    std::filesystem::create_directory_symlink(root.path() / "real", root.path() / "link", symlinkError);

    if (symlinkError)
    {
        MESSAGE("Skipping: cannot create a directory symlink in this environment ("
            << symlinkError.message() << ").");
        return;
    }

    codeqa::infrastructure::DiscoveryOptions options;
    options.rootPath = root.path();

    const codeqa::infrastructure::DiscoveryResult result = codeqa::infrastructure::discoverSourceFiles(options);
    const std::set<std::filesystem::path> paths = discoveredPaths(result);

    CHECK(paths.count(std::filesystem::path{"real/inner.cpp"}) == 1);
    CHECK(paths.count(std::filesystem::path{"link/inner.cpp"}) == 0);
}
