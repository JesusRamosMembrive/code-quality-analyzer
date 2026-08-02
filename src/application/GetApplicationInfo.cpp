#include "codeqa/application/GetApplicationInfo.h"

#include "codeqa/application/Version.h"

namespace codeqa::application
{

ApplicationInfo GetApplicationInfo::execute() const
{
    return ApplicationInfo{"Code Quality Analyzer", CODEQA_VERSION};
}

} // namespace codeqa::application
