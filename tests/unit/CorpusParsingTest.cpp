#include "doctest/doctest.h"

#include "codeqa/frontend/ParseSourceFile.h"
#include "codeqa/frontend/SelectGrammar.h"

#include <filesystem>
#include <string>
#include <vector>

namespace
{

struct CorpusFixture
{
    std::filesystem::path path;
    bool isMalformed{false};
};

std::vector<CorpusFixture> collectCorpusFixtures()
{
    std::vector<CorpusFixture> fixtures;
    const std::filesystem::path corpusRoot{CODEQA_CORPUS_DIR};

    for (const std::filesystem::directory_entry& entry :
        std::filesystem::recursive_directory_iterator(corpusRoot))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        const std::string extension = entry.path().extension().string();

        if (extension != ".c" && extension != ".cpp" && extension != ".h" && extension != ".hpp")
        {
            continue;
        }

        const std::string relative =
            entry.path().lexically_relative(corpusRoot).generic_string();

        fixtures.push_back(
            CorpusFixture{entry.path(), relative.rfind("malformed/", 0) == 0});
    }

    return fixtures;
}

codeqa::frontend::Grammar grammarForExtension(const std::filesystem::path& path)
{
    // Mirrors Fase 3: .c is C; .cpp/.hpp are C++; .h is ambiguous and
    // defaults to C++ via selectGrammar.
    const std::string extension = path.extension().string();

    if (extension == ".c")
    {
        return codeqa::frontend::selectGrammar(codeqa::core::Language::C).grammar;
    }

    if (extension == ".h")
    {
        return codeqa::frontend::selectGrammar(codeqa::core::Language::Unknown).grammar;
    }

    return codeqa::frontend::selectGrammar(codeqa::core::Language::Cpp).grammar;
}

} // namespace

TEST_CASE("the corpus contains fixtures to check")
{
    CHECK(collectCorpusFixtures().size() >= 13);
}

TEST_CASE("every well-formed corpus fixture parses with zero diagnostics")
{
    codeqa::frontend::TreeSitterParser parser;

    for (const CorpusFixture& fixture : collectCorpusFixtures())
    {
        if (fixture.isMalformed)
        {
            continue;
        }

        const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
            fixture.path, grammarForExtension(fixture.path), "corpus check", parser);

        INFO("fixture: " << fixture.path.generic_string());
        CHECK(result.parsed);
        CHECK(result.diagnostics.empty());
    }
}

TEST_CASE("every malformed corpus fixture produces at least one diagnostic")
{
    codeqa::frontend::TreeSitterParser parser;
    std::size_t malformedCount = 0;

    for (const CorpusFixture& fixture : collectCorpusFixtures())
    {
        if (!fixture.isMalformed)
        {
            continue;
        }

        ++malformedCount;

        const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
            fixture.path, grammarForExtension(fixture.path), "corpus check", parser);

        INFO("fixture: " << fixture.path.generic_string());
        CHECK(result.parsed);
        CHECK_FALSE(result.diagnostics.empty());
    }

    CHECK(malformedCount == 2);
}
