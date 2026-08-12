#include "doctest/doctest.h"

#include "codeqa/application/LoadSnapshot.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>

namespace
{

class TempProject
{
public:
    TempProject() : root_(std::filesystem::temp_directory_path() / "codeqa_load_snapshot_test")
    {
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(root_ / ".codeqa" / "snapshots");
    }

    ~TempProject()
    {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    TempProject(const TempProject&) = delete;
    TempProject& operator=(const TempProject&) = delete;

    [[nodiscard]] const std::filesystem::path& root() const { return root_; }

private:
    std::filesystem::path root_;
};

} // namespace

TEST_CASE("LoadSnapshot returns the parsed JSON for an existing snapshot")
{
    const TempProject fixture;
    std::ofstream(fixture.root() / ".codeqa" / "snapshots" / "20260101T000000Z.json")
        << R"({ "schemaVersion": 1 })";

    const std::optional<nlohmann::json> result =
        codeqa::application::LoadSnapshot{}.execute(fixture.root(), "20260101T000000Z");

    REQUIRE(result.has_value());
    CHECK(result->at("schemaVersion").get<int>() == 1);
}

TEST_CASE("LoadSnapshot returns nullopt for a timestamp that doesn't exist")
{
    const TempProject fixture;

    const std::optional<nlohmann::json> result =
        codeqa::application::LoadSnapshot{}.execute(fixture.root(), "does-not-exist");

    CHECK_FALSE(result.has_value());
}

TEST_CASE("LoadSnapshot returns nullopt for a file that isn't valid JSON")
{
    const TempProject fixture;
    std::ofstream(fixture.root() / ".codeqa" / "snapshots" / "broken.json") << "{ not json";

    const std::optional<nlohmann::json> result =
        codeqa::application::LoadSnapshot{}.execute(fixture.root(), "broken");

    CHECK_FALSE(result.has_value());
}
