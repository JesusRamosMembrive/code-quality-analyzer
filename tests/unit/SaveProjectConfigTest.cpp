#include "doctest/doctest.h"

#include "codeqa/application/SaveProjectConfig.h"
#include "codeqa/config/ConfigJson.h"
#include "codeqa/config/DefaultConfig.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{

class TempProject
{
public:
    TempProject()
        : root_(std::filesystem::temp_directory_path() / "codeqa_save_project_config_test"),
          outside_(std::filesystem::temp_directory_path() / "codeqa_save_project_config_outside")
    {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
        std::filesystem::remove_all(outside_, ignored);
        std::filesystem::create_directories(root_);
        std::filesystem::create_directories(outside_);
    }

    ~TempProject()
    {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
        std::filesystem::remove_all(outside_, ignored);
    }

    TempProject(const TempProject&) = delete;
    TempProject& operator=(const TempProject&) = delete;

    [[nodiscard]] const std::filesystem::path& root() const { return root_; }
    [[nodiscard]] const std::filesystem::path& outside() const { return outside_; }

private:
    std::filesystem::path root_;
    std::filesystem::path outside_;
};

std::string readText(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::ostringstream content;
    content << input.rdbuf();
    return content.str();
}

bool createFileLink(const std::filesystem::path& target, const std::filesystem::path& link)
{
    std::error_code linkError;
    std::filesystem::create_symlink(target, link, linkError);
    if (!linkError)
    {
        return true;
    }

    linkError.clear();
    std::filesystem::create_hard_link(target, link, linkError);
    return !linkError;
}

bool hasConfigTemporaryFile(const std::filesystem::path& root)
{
    std::error_code iteratorError;
    for (std::filesystem::directory_iterator current(root, iteratorError), end;
         !iteratorError && current != end;
         current.increment(iteratorError))
    {
        if (current->path().filename().string().find("codeqa.json.tmp.") == 0)
        {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("SaveProjectConfig validates and writes prettified codeqa.json")
{
    const TempProject project;
    std::ofstream(project.root() / "codeqa.json")
        << codeqa::config::toJson(codeqa::config::defaultConfig()).dump(2) << '\n';
    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.rules.at("complexity.cyclomatic").warningLimit = 7;

    const auto error = codeqa::application::SaveProjectConfig{}.execute(project.root(), config);

    CHECK_FALSE(error.has_value());
    const std::filesystem::path path = project.root() / "codeqa.json";
    REQUIRE(std::filesystem::is_regular_file(path));
    const std::string saved = readText(path);
    CHECK(saved.find("\n  \"languages\"") != std::string::npos);

    const auto parsed = codeqa::config::parseConfig(saved);
    REQUIRE(parsed.config.has_value());
    CHECK(parsed.config->rules.at("complexity.cyclomatic").warningLimit == 7);
    CHECK_FALSE(hasConfigTemporaryFile(project.root()));
}

TEST_CASE("SaveProjectConfig rejects an invalid config before overwriting the file")
{
    const TempProject project;
    const std::filesystem::path path = project.root() / "codeqa.json";
    std::ofstream(path) << "keep me";
    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.rules.at("complexity.cyclomatic").warningLimit = 20;
    config.rules.at("complexity.cyclomatic").highLimit = 10;

    const auto error = codeqa::application::SaveProjectConfig{}.execute(project.root(), config);

    REQUIRE(error.has_value());
    CHECK(error->find("CONFIG008") != std::string::npos);
    CHECK(readText(path) == "keep me");
}

TEST_CASE("SaveProjectConfig reports an error when codeqa.json cannot be opened")
{
    const TempProject project;
    std::filesystem::create_directory(project.root() / "codeqa.json");

    const auto error = codeqa::application::SaveProjectConfig{}.execute(
        project.root(), codeqa::config::defaultConfig());

    CHECK(error.has_value());
}

TEST_CASE("SaveProjectConfig rejects an existing codeqa.json link outside the project")
{
    const TempProject project;
    const std::filesystem::path outsideConfig = project.outside() / "outside.json";
    std::ofstream(outsideConfig) << "outside stays unchanged";
    REQUIRE(createFileLink(outsideConfig, project.root() / "codeqa.json"));

    const auto error = codeqa::application::SaveProjectConfig{}.execute(
        project.root(), codeqa::config::defaultConfig());

    REQUIRE(error.has_value());
    CHECK(error->find("CONFIG011") != std::string::npos);
    CHECK(readText(outsideConfig) == "outside stays unchanged");
    CHECK_FALSE(hasConfigTemporaryFile(project.root()));
}

TEST_CASE("SaveProjectConfig preserves the existing file when atomic replacement cannot finalize")
{
#ifdef _WIN32
    const TempProject project;
    const std::filesystem::path path = project.root() / "codeqa.json";
    const std::string original = codeqa::config::toJson(codeqa::config::defaultConfig()).dump(2) + '\n';
    std::ofstream(path) << original;
    const HANDLE lock = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    REQUIRE(lock != INVALID_HANDLE_VALUE);
    codeqa::config::ProjectConfig replacement = codeqa::config::defaultConfig();
    replacement.rules.at("complexity.cyclomatic").warningLimit = 7;

    const auto error = codeqa::application::SaveProjectConfig{}.execute(
        project.root(), replacement);
    REQUIRE(CloseHandle(lock) != 0);

    REQUIRE(error.has_value());
    CHECK(error->find("CONFIG011") != std::string::npos);
    CHECK(readText(path) == original);
    CHECK_FALSE(hasConfigTemporaryFile(project.root()));
#else
    WARN("Atomic replacement sharing-failure coverage is Windows-specific.");
#endif
}
