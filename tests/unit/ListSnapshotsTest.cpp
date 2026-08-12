#include "doctest/doctest.h"

#include "codeqa/application/ListSnapshots.h"

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
    TempProject() : root_(std::filesystem::temp_directory_path() / "codeqa_list_snapshots_test")
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

    void writeSnapshotFile(const std::string& name) const
    {
        const std::filesystem::path path = root_ / ".codeqa" / "snapshots" / name;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path) << "{}";
    }

    [[nodiscard]] const std::filesystem::path& root() const { return root_; }

private:
    std::filesystem::path root_;
};

} // namespace

TEST_CASE("ListSnapshots returns an empty vector when .codeqa/snapshots doesn't exist")
{
    const TempProject fixture;

    const std::vector<std::string> timestamps = codeqa::application::ListSnapshots{}.execute(fixture.root());

    CHECK(timestamps.empty());
}

TEST_CASE("ListSnapshots returns timestamps sorted ascending, ignoring non-JSON files")
{
    const TempProject fixture;
    fixture.writeSnapshotFile("20260301T000000Z.json");
    fixture.writeSnapshotFile("20260101T000000Z.json");
    fixture.writeSnapshotFile("20260201T000000Z.json");
    std::ofstream(fixture.root() / ".codeqa" / "snapshots" / "notes.txt") << "not a snapshot";

    const std::vector<std::string> timestamps = codeqa::application::ListSnapshots{}.execute(fixture.root());

    REQUIRE(timestamps.size() == 3);
    CHECK(timestamps[0] == "20260101T000000Z");
    CHECK(timestamps[1] == "20260201T000000Z");
    CHECK(timestamps[2] == "20260301T000000Z");
}
