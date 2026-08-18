#include "codeqa/infrastructure/ClassifyExtension.h"

#include "codeqa/infrastructure/StringUtilities.h"

namespace codeqa::infrastructure
{

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
