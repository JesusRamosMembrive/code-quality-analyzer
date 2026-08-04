#include "codeqa/application/DiscoverProjectSources.h"

#include "codeqa/infrastructure/DiscoverSourceFiles.h"

namespace codeqa::application
{

codeqa::infrastructure::DiscoveryResult DiscoverProjectSources::execute(
    const codeqa::infrastructure::DiscoveryOptions& options) const
{
    return codeqa::infrastructure::discoverSourceFiles(options);
}

} // namespace codeqa::application
