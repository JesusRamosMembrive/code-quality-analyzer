#include "doctest/doctest.h"

#include "codeqa/application/DiscoverProjectSources.h"

#include <filesystem>
#include <fstream>

namespace
{

std::filesystem::path makeTempRoot()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "codeqa_discover_project_sources_test";

    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    return root;
}

} // namespace

TEST_CASE("DiscoverProjectSources delegates to the discovery engine")
{
    const std::filesystem::path root = makeTempRoot();
    std::ofstream(root / "main.c") << "";
    std::ofstream(root / "notes.txt") << "";

    codeqa::infrastructure::DiscoveryOptions options;
    options.rootPath = root;

    const codeqa::application::DiscoverProjectSources useCase;
    const codeqa::infrastructure::DiscoveryResult result = useCase.execute(options);

    CHECK(result.files.size() == 1);
    CHECK(result.files.front().path == std::filesystem::path{"main.c"});

    std::filesystem::remove_all(root);
}

TEST_CASE("DiscoverProjectSources forwards exclude patterns to the engine")
{
    const std::filesystem::path root = makeTempRoot();
    std::ofstream(root / "keep.cpp") << "";
    std::ofstream(root / "generated.cpp") << "";

    codeqa::infrastructure::DiscoveryOptions options;
    options.rootPath = root;
    options.excludePatterns = {"generated.cpp"};

    const codeqa::application::DiscoverProjectSources useCase;
    const codeqa::infrastructure::DiscoveryResult result = useCase.execute(options);

    REQUIRE(result.files.size() == 1);
    CHECK(result.files.front().path == std::filesystem::path{"keep.cpp"});
    CHECK(result.excludedCount == 1);

    std::filesystem::remove_all(root);
}
