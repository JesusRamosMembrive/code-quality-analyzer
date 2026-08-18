#include "codeqa/infrastructure/DefaultExclusions.h"

#include "codeqa/infrastructure/StringUtilities.h"

namespace codeqa::infrastructure
{

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
