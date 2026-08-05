#include "doctest/doctest.h"

#include "codeqa/application/DetectFileLanguage.h"

#include "codeqa/infrastructure/DiscoveredFile.h"
#include "codeqa/infrastructure/ExtensionCategory.h"

#include <filesystem>
#include <vector>

namespace
{

codeqa::infrastructure::DiscoveredFile makeFile(
    const std::filesystem::path& path,
    codeqa::infrastructure::ExtensionCategory category)
{
    return codeqa::infrastructure::DiscoveredFile{path, category, 0};
}

} // namespace

TEST_CASE("an unambiguous C extension resolves to Language::C with full confidence")
{
    const codeqa::application::DetectFileLanguage detect;
    const codeqa::infrastructure::DiscoveredFile file =
        makeFile("main.c", codeqa::infrastructure::ExtensionCategory::C);

    const codeqa::core::LanguageDetectionResult result = detect.execute(file, {});

    CHECK(result.language == codeqa::core::Language::C);
    CHECK(result.confidence == 1.0);
    CHECK(result.reason == "unambiguous extension");
}

TEST_CASE("an unambiguous C++ extension resolves to Language::Cpp with full confidence")
{
    const codeqa::application::DetectFileLanguage detect;
    const codeqa::infrastructure::DiscoveredFile file =
        makeFile("util.cpp", codeqa::infrastructure::ExtensionCategory::Cpp);

    const codeqa::core::LanguageDetectionResult result = detect.execute(file, {});

    CHECK(result.language == codeqa::core::Language::Cpp);
    CHECK(result.confidence == 1.0);
    CHECK(result.reason == "unambiguous extension");
}

TEST_CASE("an ambiguous header without an override resolves to Unknown with partial confidence")
{
    const codeqa::application::DetectFileLanguage detect;
    const codeqa::infrastructure::DiscoveredFile file =
        makeFile("util.h", codeqa::infrastructure::ExtensionCategory::AmbiguousHeader);

    const codeqa::core::LanguageDetectionResult result = detect.execute(file, {});

    CHECK(result.language == codeqa::core::Language::Unknown);
    CHECK(result.confidence == 0.5);
    CHECK(result.reason == "ambiguous header extension");
}

TEST_CASE("a filename override resolves an ambiguous header")
{
    const codeqa::application::DetectFileLanguage detect;
    const codeqa::infrastructure::DiscoveredFile file =
        makeFile("util.h", codeqa::infrastructure::ExtensionCategory::AmbiguousHeader);
    const std::vector<codeqa::application::LanguageOverride> overrides{
        {"util.h", codeqa::core::Language::Cpp}};

    const codeqa::core::LanguageDetectionResult result = detect.execute(file, overrides);

    CHECK(result.language == codeqa::core::Language::Cpp);
    CHECK(result.confidence == 1.0);
    CHECK(result.reason == "path override");
}

TEST_CASE("a full-path override resolves a nested ambiguous header")
{
    const codeqa::application::DetectFileLanguage detect;
    const codeqa::infrastructure::DiscoveredFile file = makeFile(
        "include/network/socket.h", codeqa::infrastructure::ExtensionCategory::AmbiguousHeader);
    const std::vector<codeqa::application::LanguageOverride> overrides{
        {"include/network/socket.h", codeqa::core::Language::C}};

    const codeqa::core::LanguageDetectionResult result = detect.execute(file, overrides);

    CHECK(result.language == codeqa::core::Language::C);
    CHECK(result.reason == "path override");
}

TEST_CASE("an override wins even over an unambiguous extension")
{
    const codeqa::application::DetectFileLanguage detect;
    const codeqa::infrastructure::DiscoveredFile file =
        makeFile("legacy.c", codeqa::infrastructure::ExtensionCategory::C);
    const std::vector<codeqa::application::LanguageOverride> overrides{
        {"legacy.c", codeqa::core::Language::Cpp}};

    const codeqa::core::LanguageDetectionResult result = detect.execute(file, overrides);

    CHECK(result.language == codeqa::core::Language::Cpp);
    CHECK(result.confidence == 1.0);
    CHECK(result.reason == "path override");
}

TEST_CASE("when multiple overrides match the same file, the last one wins")
{
    const codeqa::application::DetectFileLanguage detect;
    const codeqa::infrastructure::DiscoveredFile file =
        makeFile("ambiguous.h", codeqa::infrastructure::ExtensionCategory::AmbiguousHeader);
    const std::vector<codeqa::application::LanguageOverride> overrides{
        {"ambiguous.h", codeqa::core::Language::C},
        {"ambiguous.h", codeqa::core::Language::Cpp}};

    const codeqa::core::LanguageDetectionResult result = detect.execute(file, overrides);

    CHECK(result.language == codeqa::core::Language::Cpp);
}

TEST_CASE("an override that does not match the file is ignored")
{
    const codeqa::application::DetectFileLanguage detect;
    const codeqa::infrastructure::DiscoveredFile file =
        makeFile("main.c", codeqa::infrastructure::ExtensionCategory::C);
    const std::vector<codeqa::application::LanguageOverride> overrides{
        {"other.c", codeqa::core::Language::Cpp}};

    const codeqa::core::LanguageDetectionResult result = detect.execute(file, overrides);

    CHECK(result.language == codeqa::core::Language::C);
    CHECK(result.reason == "unambiguous extension");
}
