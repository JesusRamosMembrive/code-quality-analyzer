#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace codeqa::application
{

class ListSnapshots
{
public:
    /// Timestamps (filename stems) of every *.json file under
    /// <rootPath>/.codeqa/snapshots/, ascending. Empty if the directory doesn't exist -- not
    /// an error (first run has no history yet).
    [[nodiscard]] std::vector<std::string> execute(const std::filesystem::path& rootPath) const;
};

} // namespace codeqa::application
