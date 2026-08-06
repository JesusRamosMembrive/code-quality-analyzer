#include "doctest/doctest.h"

#include "codeqa/frontend/ParseSourceFile.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace
{

class TempSourceFile
{
public:
    TempSourceFile(const std::string& name, const std::string& content)
        : path_(std::filesystem::temp_directory_path() / "codeqa_parse_source_file_test" / name)
    {
        std::filesystem::create_directories(path_.parent_path());
        std::ofstream(path_) << content;
    }

    ~TempSourceFile()
    {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    TempSourceFile(const TempSourceFile&) = delete;
    TempSourceFile& operator=(const TempSourceFile&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("a well-formed C file parses with no diagnostics")
{
    const TempSourceFile file{"clean.c", "int main(void)\n{\n    return 0;\n}\n"};
    codeqa::frontend::TreeSitterParser parser;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::C, "detected language", parser);

    CHECK(result.parsed);
    CHECK(result.diagnostics.empty());
    CHECK(result.grammar == codeqa::frontend::Grammar::C);
    CHECK(result.grammarReason == "detected language");
}

TEST_CASE("a well-formed C++ file parses with no diagnostics")
{
    const TempSourceFile file{
        "clean.cpp", "class Foo\n{\npublic:\n    int bar() { return 1; }\n};\n"};
    codeqa::frontend::TreeSitterParser parser;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser);

    CHECK(result.parsed);
    CHECK(result.diagnostics.empty());
}

TEST_CASE("a broken file produces at least one diagnostic with a plausible line")
{
    const TempSourceFile file{
        "broken.cpp", "int fine(void)\n{\n    return 0;\n}\n\nint broken( {\n"};
    codeqa::frontend::TreeSitterParser parser;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser);

    CHECK(result.parsed);
    REQUIRE_FALSE(result.diagnostics.empty());
    CHECK(result.diagnostics.front().severity == codeqa::core::DiagnosticSeverity::Error);
    CHECK(result.diagnostics.front().location.line >= 6);
    CHECK(result.diagnostics.front().location.filePath == file.path());
}

TEST_CASE("a cascading error does not produce a diagnostic per nested node")
{
    const TempSourceFile file{
        "cascade.cpp",
        "void a()\n{\n    if (x) {\n        if (y) {\n            if (z) {\n"};
    codeqa::frontend::TreeSitterParser parser;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser);

    CHECK(result.parsed);
    REQUIRE_FALSE(result.diagnostics.empty());
    CHECK(result.diagnostics.size() <= 3);
}

TEST_CASE("a missing file produces PARSE003 and is not marked parsed")
{
    codeqa::frontend::TreeSitterParser parser;
    const std::filesystem::path missing =
        std::filesystem::temp_directory_path() / "codeqa_does_not_exist_12345.c";

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        missing, codeqa::frontend::Grammar::C, "detected language", parser);

    CHECK_FALSE(result.parsed);
    REQUIRE(result.diagnostics.size() == 1);
    CHECK(result.diagnostics.front().code == "PARSE003");
}

TEST_CASE("an empty file parses cleanly")
{
    const TempSourceFile file{"empty.c", ""};
    codeqa::frontend::TreeSitterParser parser;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::C, "detected language", parser);

    CHECK(result.parsed);
    CHECK(result.diagnostics.empty());
}

TEST_CASE("parsing the same file twice produces identical diagnostics")
{
    const TempSourceFile file{"repeat.cpp", "int broken( {\n"};
    codeqa::frontend::TreeSitterParser parser;

    const codeqa::frontend::ParsedFile first = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser);
    const codeqa::frontend::ParsedFile second = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::Cpp, "detected language", parser);

    REQUIRE(first.diagnostics.size() == second.diagnostics.size());

    for (std::size_t index = 0; index < first.diagnostics.size(); ++index)
    {
        CHECK(first.diagnostics[index].code == second.diagnostics[index].code);
        CHECK(first.diagnostics[index].location.line == second.diagnostics[index].location.line);
        CHECK(first.diagnostics[index].location.column == second.diagnostics[index].location.column);
    }
}
