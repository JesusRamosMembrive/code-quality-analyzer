#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace codeqa::infrastructure
{

struct DiscoveryOptions
{
    std::filesystem::path rootPath;
    std::vector<std::string> excludePatterns;
};

} // namespace codeqa::infrastructure
