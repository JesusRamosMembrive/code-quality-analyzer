#pragma once

#include "codeqa/infrastructure/DiscoveryOptions.h"
#include "codeqa/infrastructure/DiscoveryResult.h"

namespace codeqa::application
{

class DiscoverProjectSources
{
public:
    [[nodiscard]] codeqa::infrastructure::DiscoveryResult execute(
        const codeqa::infrastructure::DiscoveryOptions& options) const;
};

} // namespace codeqa::application
