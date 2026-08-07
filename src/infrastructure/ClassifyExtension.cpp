#include "codeqa/infrastructure/ClassifyExtension.h"

#include <algorithm>
#include <cctype>

namespace codeqa::infrastructure
{

namespace
{

std::string toLower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });

    return text;
}

} // namespace

std::optional<ExtensionCategory> classifyExtension(const std::string& extension)
{
    const std::string normalized = toLower(extension);

    if (normalized == ".c")
    {
        return ExtensionCategory::C;
    }

    if (normalized == ".cc" || normalized == ".cpp" || normalized == ".cxx"
        || normalized == ".hpp" || normalized == ".hh" || normalized == ".hxx")
    {
        return ExtensionCategory::Cpp;
    }

    if (normalized == ".h")
    {
        return ExtensionCategory::AmbiguousHeader;
    }

    return std::nullopt;
}

} // namespace codeqa::infrastructure
