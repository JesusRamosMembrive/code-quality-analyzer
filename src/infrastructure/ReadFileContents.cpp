#include "codeqa/infrastructure/ReadFileContents.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <system_error>

namespace codeqa::infrastructure
{

std::optional<std::string> readFileContents(const std::filesystem::path& path)
{
    std::error_code sizeError;
    const std::uintmax_t size = std::filesystem::file_size(path, sizeError);

    if (sizeError)
    {
        return std::nullopt;
    }

    std::ifstream input(path, std::ios::binary);

    if (!input)
    {
        return std::nullopt;
    }

    std::string content(static_cast<std::size_t>(size), '\0');

    if (size > 0)
    {
        input.read(content.data(), static_cast<std::streamsize>(size));

        if (!input)
        {
            return std::nullopt;
        }
    }

    return content;
}

} // namespace codeqa::infrastructure
