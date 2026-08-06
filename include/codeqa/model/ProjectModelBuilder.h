#pragma once

#include "codeqa/frontend/ParsedFile.h"
#include "codeqa/model/Models.h"

#include <filesystem>
#include <vector>

namespace codeqa::model
{

class ProjectModelBuilder
{
public:
    [[nodiscard]] ProjectModel build(
        const std::filesystem::path& rootPath,
        const std::vector<codeqa::frontend::ParsedFile>& files) const;
};

} // namespace codeqa::model
