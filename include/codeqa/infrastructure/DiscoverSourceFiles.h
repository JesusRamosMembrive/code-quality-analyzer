#pragma once

#include "codeqa/infrastructure/DiscoveryOptions.h"
#include "codeqa/infrastructure/DiscoveryResult.h"

namespace codeqa::infrastructure
{

[[nodiscard]] DiscoveryResult discoverSourceFiles(const DiscoveryOptions& options);

} // namespace codeqa::infrastructure
