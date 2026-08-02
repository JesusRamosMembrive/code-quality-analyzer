#pragma once

#include <cstddef>
#include <filesystem>

namespace codeqa::core
{

struct SourceLocation
{
    std::filesystem::path filePath;
    std::size_t line{1};
    std::size_t column{1};
    std::size_t offset{0};
};

} // namespace codeqa::core
