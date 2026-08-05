#include "codeqa/application/DetectFileLanguage.h"

#include "codeqa/infrastructure/ExtensionCategory.h"
#include "codeqa/infrastructure/GlobMatcher.h"

namespace codeqa::application
{

codeqa::core::LanguageDetectionResult DetectFileLanguage::execute(
    const codeqa::infrastructure::DiscoveredFile& file,
    const std::vector<LanguageOverride>& overrides) const
{
    for (auto it = overrides.rbegin(); it != overrides.rend(); ++it)
    {
        if (codeqa::infrastructure::matchesExcludePattern(it->pattern, file.path))
        {
            return codeqa::core::LanguageDetectionResult{it->language, 1.0, "path override"};
        }
    }

    switch (file.category)
    {
    case codeqa::infrastructure::ExtensionCategory::C:
        return codeqa::core::LanguageDetectionResult{
            codeqa::core::Language::C, 1.0, "unambiguous extension"};
    case codeqa::infrastructure::ExtensionCategory::Cpp:
        return codeqa::core::LanguageDetectionResult{
            codeqa::core::Language::Cpp, 1.0, "unambiguous extension"};
    case codeqa::infrastructure::ExtensionCategory::AmbiguousHeader:
        return codeqa::core::LanguageDetectionResult{
            codeqa::core::Language::Unknown, 0.5, "ambiguous header extension"};
    }

    return codeqa::core::LanguageDetectionResult{};
}

} // namespace codeqa::application
