#include "doctest/doctest.h"

#include "codeqa/application/LoadProjectConfig.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace
{

class TempConfigFile
{
public:
    TempConfigFile(const std::string& name, const std::string& content)
        : path_(std::filesystem::temp_directory_path() / "codeqa_load_project_config_test" / name)
    {
        std::filesystem::create_directories(path_.parent_path());
        std::ofstream(path_) << content;
    }

    ~TempConfigFile()
    {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    TempConfigFile(const TempConfigFile&) = delete;
    TempConfigFile& operator=(const TempConfigFile&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("LoadProjectConfig merges a valid file over the embedded defaults")
{
    const TempConfigFile file{
        "codeqa.json",
        R"({ "rules": { "complexity.cyclomatic": { "enabled": false, "params": { "warning": 2, "high": 4 } } } })"};

    const codeqa::application::LoadProjectConfigResult result =
        codeqa::application::LoadProjectConfig{}.execute(file.path());

    REQUIRE(result.config.has_value());
    CHECK_FALSE(result.error.has_value());
    CHECK_FALSE(result.config->rules.at("complexity.cyclomatic").enabled);
    CHECK(result.config->rules.at("complexity.cyclomatic").warningLimit == 2);
    REQUIRE(result.config->rules.count("size.function-length") == 1);
    CHECK(result.config->rules.at("size.function-length").enabled);
    CHECK(result.config->rules.at("size.function-length").warningLimit == 50);
}

TEST_CASE("LoadProjectConfig reports CONFIG007 for a missing file")
{
    const std::filesystem::path missing =
        std::filesystem::temp_directory_path() / "codeqa_load_project_config_test" / "does-not-exist.json";

    const codeqa::application::LoadProjectConfigResult result =
        codeqa::application::LoadProjectConfig{}.execute(missing);

    CHECK_FALSE(result.config.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG007") != std::string::npos);
}

TEST_CASE("LoadProjectConfig propagates a parseConfig error for invalid JSON")
{
    const TempConfigFile file{"broken.json", "{ not json"};

    const codeqa::application::LoadProjectConfigResult result =
        codeqa::application::LoadProjectConfig{}.execute(file.path());

    CHECK_FALSE(result.config.has_value());
    REQUIRE(result.error.has_value());
    CHECK(result.error->find("CONFIG001") != std::string::npos);
}
