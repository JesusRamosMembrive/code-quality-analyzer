#include "doctest/doctest.h"

#include "codeqa/application/DeleteSnapshot.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace
{

class TempProject
{
public:
    TempProject()
        : root_(std::filesystem::temp_directory_path() / "codeqa_delete_snapshot_test"),
          outside_(root_.parent_path() / "codeqa_delete_snapshot_outside.json")
    {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
        std::filesystem::remove(outside_, ignored);
        std::filesystem::create_directories(root_ / ".codeqa" / "snapshots");
        std::ofstream(outside_) << "keep";
    }

    ~TempProject()
    {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
        std::filesystem::remove(outside_, ignored);
    }

    TempProject(const TempProject&) = delete;
    TempProject& operator=(const TempProject&) = delete;

    void writeSnapshot(const std::string& timestamp) const
    {
        std::ofstream(snapshotPath(timestamp)) << "{}";
    }

    [[nodiscard]] std::filesystem::path snapshotPath(const std::string& timestamp) const
    {
        return root_ / ".codeqa" / "snapshots" / (timestamp + ".json");
    }

    [[nodiscard]] const std::filesystem::path& root() const { return root_; }
    [[nodiscard]] const std::filesystem::path& outside() const { return outside_; }

private:
    std::filesystem::path root_;
    std::filesystem::path outside_;
};

} // namespace

TEST_CASE("DeleteSnapshot removes only the requested valid snapshot")
{
    const TempProject project;
    project.writeSnapshot("20260814T101112Z");
    project.writeSnapshot("20260814T101113Z");

    const auto error = codeqa::application::DeleteSnapshot{}.execute(project.root(), "20260814T101112Z");

    CHECK_FALSE(error.has_value());
    CHECK_FALSE(std::filesystem::exists(project.snapshotPath("20260814T101112Z")));
    CHECK(std::filesystem::exists(project.snapshotPath("20260814T101113Z")));
}

TEST_CASE("DeleteSnapshot rejects path traversal and leaves outside files untouched")
{
    const TempProject project;

    const auto error = codeqa::application::DeleteSnapshot{}.execute(project.root(), "../../../codeqa_delete_snapshot_outside");

    CHECK(error.has_value());
    CHECK(std::filesystem::exists(project.outside()));
}

TEST_CASE("DeleteSnapshot rejects malformed and impossible timestamps")
{
    const TempProject project;
    const std::vector<std::string> invalid = {
        "20260814T10111Z",
        "2026-08-14T101112Z",
        "20260814T101112Z.json",
        "20261301T000000Z",
        "20260230T000000Z",
        "20260814T246000Z"};

    for (const std::string& timestamp : invalid)
    {
        CAPTURE(timestamp);
        CHECK(codeqa::application::DeleteSnapshot{}.execute(project.root(), timestamp).has_value());
    }
}

TEST_CASE("DeleteSnapshot reports an error for a valid timestamp that does not exist")
{
    const TempProject project;

    const auto error = codeqa::application::DeleteSnapshot{}.execute(project.root(), "20260814T101112Z");

    CHECK(error.has_value());
}
