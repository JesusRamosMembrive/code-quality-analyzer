#pragma once

#include "codeqa/core/Diagnostic.h"
#include "codeqa/infrastructure/DiscoveredFile.h"

#include <cstddef>
#include <vector>

namespace codeqa::infrastructure
{

struct DiscoveryResult
{
    std::vector<DiscoveredFile> files;
    std::vector<codeqa::core::Diagnostic> diagnostics;
    std::size_t excludedCount{0};
};

} // namespace codeqa::infrastructure
