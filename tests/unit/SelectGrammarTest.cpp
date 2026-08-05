#include "doctest/doctest.h"

#include "codeqa/frontend/SelectGrammar.h"

TEST_CASE("a C file selects the C grammar")
{
    const codeqa::frontend::GrammarSelection selection =
        codeqa::frontend::selectGrammar(codeqa::core::Language::C);

    CHECK(selection.grammar == codeqa::frontend::Grammar::C);
    CHECK(selection.reason == "detected language");
}

TEST_CASE("a C++ file selects the C++ grammar")
{
    const codeqa::frontend::GrammarSelection selection =
        codeqa::frontend::selectGrammar(codeqa::core::Language::Cpp);

    CHECK(selection.grammar == codeqa::frontend::Grammar::Cpp);
    CHECK(selection.reason == "detected language");
}

TEST_CASE("an unknown-language file falls back to the C++ grammar and says so")
{
    const codeqa::frontend::GrammarSelection selection =
        codeqa::frontend::selectGrammar(codeqa::core::Language::Unknown);

    CHECK(selection.grammar == codeqa::frontend::Grammar::Cpp);
    CHECK(selection.reason == "ambiguous header, defaulted to C++");
}
