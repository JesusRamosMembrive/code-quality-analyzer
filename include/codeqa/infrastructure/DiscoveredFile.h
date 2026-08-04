#pragma once

#include "codeqa/infrastructure/ExtensionCategory.h"

#include <cstdint>
#include <filesystem>

namespace codeqa::infrastructure
{

struct DiscoveredFile
{
    std::filesystem::path path;
    ExtensionCategory category{ExtensionCategory::C};
    std::uintmax_t sizeBytes{0};
};

} // namespace codeqa::infrastructure
