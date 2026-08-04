#include "codeqa/infrastructure/DefaultExclusions.h"

namespace codeqa::infrastructure
{

bool isDefaultExcludedDirectoryName(const std::string& directoryName)
{
    if (!directoryName.empty() && directoryName.front() == '.')
    {
        return true;
    }

    if (directoryName == "build")
    {
        return true;
    }

    if (directoryName.rfind("cmake-build-", 0) == 0)
    {
        return true;
    }

    return false;
}

} // namespace codeqa::infrastructure
