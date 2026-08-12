#include "doctest/doctest.h"

#include "codeqa/application/SaveSnapshot.h"

#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <system_error>

namespace
{

class TempProject
{
public:
    TempProject() : root_(std::filesystem::temp_directory_path() / "codeqa_save_snapshot_test")
    {
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(root_);
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

TEST_CASE("SaveSnapshot writes the report JSON under .codeqa/snapshots and returns the path")
{
    const TempProject fixture;
    const nlohmann::json report{{"schemaVersion", 1}, {"metadata", {{"toolName", "codeqa"}}}};

    const std::optional<std::filesystem::path> result =
        codeqa::application::SaveSnapshot{}.execute(fixture.root(), report);

    REQUIRE(result.has_value());
    CHECK(result->parent_path().filename() == "snapshots");
    CHECK(result->parent_path().parent_path().filename() == ".codeqa");
    CHECK(std::regex_match(result->stem().string(), std::regex(R"(\d{8}T\d{6}Z)")));

    std::ifstream savedStream(*result);
    const nlohmann::json saved = nlohmann::json::parse(savedStream);
    CHECK(saved == report);
}

TEST_CASE("SaveSnapshot returns nullopt when the snapshots directory can't be created")
{
    const TempProject fixture;
    std::ofstream(fixture.root() / ".codeqa") << ""; // a file, not a directory -- blocks .codeqa/snapshots

    const std::optional<std::filesystem::path> result =
        codeqa::application::SaveSnapshot{}.execute(fixture.root(), nlohmann::json{{"schemaVersion", 1}});

    CHECK_FALSE(result.has_value());
}
