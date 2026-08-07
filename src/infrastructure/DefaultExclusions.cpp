#include "codeqa/infrastructure/DefaultExclusions.h"

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

bool isDefaultExcludedDirectoryName(const std::string& directoryName)
{
    if (!directoryName.empty() && directoryName.front() == '.')
    {
        return true;
    }

    const std::string normalized = toLower(directoryName);

    if (normalized == "build")
    {
        return true;
    }

    if (normalized.rfind("cmake-build-", 0) == 0)
    {
        return true;
    }

    return false;
}

} // namespace codeqa::infrastructure
