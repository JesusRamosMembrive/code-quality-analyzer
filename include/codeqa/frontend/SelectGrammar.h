#pragma once

#include "codeqa/core/Language.h"
#include "codeqa/frontend/Grammar.h"

#include <string>

namespace codeqa::frontend
{

struct GrammarSelection
{
    Grammar grammar{Grammar::Cpp};
    std::string reason;
};

[[nodiscard]] GrammarSelection selectGrammar(codeqa::core::Language language);

} // namespace codeqa::frontend
