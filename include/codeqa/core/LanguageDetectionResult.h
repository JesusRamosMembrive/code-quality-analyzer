#pragma once

#include "codeqa/core/Language.h"

#include <string>

namespace codeqa::core
{

struct LanguageDetectionResult
{
    Language language{Language::Unknown};
    double confidence{0.0};
    std::string reason;
};

} // namespace codeqa::core
