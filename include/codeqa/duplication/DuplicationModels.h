#pragma once

#include "codeqa/core/SourceRange.h"

#include <string>
#include <vector>

namespace codeqa::duplication
{

struct DuplicateMember
{
    std::string qualifiedName;
    codeqa::core::SourceRange range;
};

struct DuplicateGroup
{
    std::vector<DuplicateMember> members;
};

struct ProjectDuplicationModel
{
    std::vector<DuplicateGroup> groups;
};

} // namespace codeqa::duplication
