#pragma once

#include "codeqa/core/Language.h"
#include "codeqa/core/LanguageDetectionResult.h"
#include "codeqa/infrastructure/DiscoveredFile.h"

#include <string>
#include <vector>

namespace codeqa::application
{

struct LanguageOverride
{
    std::string pattern;
    codeqa::core::Language language;
};

class DetectFileLanguage
{
public:
    [[nodiscard]] codeqa::core::LanguageDetectionResult execute(
        const codeqa::infrastructure::DiscoveredFile& file,
        const std::vector<LanguageOverride>& overrides) const;
};

} // namespace codeqa::application
