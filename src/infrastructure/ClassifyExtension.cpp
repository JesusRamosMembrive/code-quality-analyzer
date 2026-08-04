#include "codeqa/infrastructure/ClassifyExtension.h"

namespace codeqa::infrastructure
{

std::optional<ExtensionCategory> classifyExtension(const std::string& extension)
{
    if (extension == ".c")
    {
        return ExtensionCategory::C;
    }

    if (extension == ".cc" || extension == ".cpp" || extension == ".cxx"
        || extension == ".hpp" || extension == ".hh" || extension == ".hxx")
    {
        return ExtensionCategory::Cpp;
    }

    if (extension == ".h")
    {
        return ExtensionCategory::AmbiguousHeader;
    }

    return std::nullopt;
}

} // namespace codeqa::infrastructure
