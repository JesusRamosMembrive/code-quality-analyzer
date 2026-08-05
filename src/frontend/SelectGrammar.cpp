#include "codeqa/frontend/SelectGrammar.h"

namespace codeqa::frontend
{

GrammarSelection selectGrammar(codeqa::core::Language language)
{
    switch (language)
    {
    case codeqa::core::Language::C:
        return GrammarSelection{Grammar::C, "detected language"};
    case codeqa::core::Language::Cpp:
        return GrammarSelection{Grammar::Cpp, "detected language"};
    case codeqa::core::Language::Unknown:
        return GrammarSelection{Grammar::Cpp, "ambiguous header, defaulted to C++"};
    }

    return GrammarSelection{Grammar::Cpp, "ambiguous header, defaulted to C++"};
}

} // namespace codeqa::frontend
