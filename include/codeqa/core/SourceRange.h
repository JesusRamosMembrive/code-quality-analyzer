#pragma once

#include "codeqa/core/SourceLocation.h"

namespace codeqa::core
{

struct SourceRange
{
    SourceLocation begin;
    SourceLocation end;
};

} // namespace codeqa::core
