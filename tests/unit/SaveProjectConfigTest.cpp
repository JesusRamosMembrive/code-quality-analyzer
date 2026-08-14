#include "doctest/doctest.h"

#include "codeqa/application/SaveProjectConfig.h"
#include "codeqa/config/ConfigJson.h"
#include "codeqa/config/DefaultConfig.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

namespace
{

class TempProject
{
public:
    TempProject() : root_(std::filesystem::temp_directory_path() / "codeqa_save_project_config_test")
    {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
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

std::string readText(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::ostringstream content;
    content << input.rdbuf();
    return content.str();
}

} // namespace

TEST_CASE("SaveProjectConfig validates and writes prettified codeqa.json")
{
    const TempProject project;
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
