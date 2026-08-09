#include "doctest/doctest.h"

#include "codeqa/infrastructure/ReadFileContents.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace
{

class TempFile
{
public:
    TempFile(const std::string& name, const std::string& content)
        : path_(std::filesystem::temp_directory_path() / "codeqa_read_file_contents_test" / name)
    {
        std::filesystem::create_directories(path_.parent_path());
        std::ofstream(path_, std::ios::binary) << content;
    }

    ~TempFile()
    {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("readFileContents returns the exact bytes of an existing file")
{
    const TempFile file{"sample.txt", "line one\nline two\n"};

    const std::optional<std::string> content = codeqa::infrastructure::readFileContents(file.path());

    REQUIRE(content.has_value());
    CHECK(*content == "line one\nline two\n");
}

TEST_CASE("readFileContents returns an empty string for an empty file")
{
    const TempFile file{"empty.txt", ""};

    const std::optional<std::string> content = codeqa::infrastructure::readFileContents(file.path());

    REQUIRE(content.has_value());
    CHECK(content->empty());
}

TEST_CASE("readFileContents returns nullopt for a missing file")
{
    const std::filesystem::path missing =
        std::filesystem::temp_directory_path() / "codeqa_read_file_contents_test" / "does-not-exist.txt";

    const std::optional<std::string> content = codeqa::infrastructure::readFileContents(missing);

    CHECK_FALSE(content.has_value());
}
