#pragma once

#include "codeqa/core/SourceLocation.h"

namespace codeqa::core
{

struct SourceRange
{
    SourceLocation begin;
    SourceLocation end;
};

inline bool operator==(const SourceRange& left, const SourceRange& right)
{
    return left.begin.offset == right.begin.offset && left.end.offset == right.end.offset;
}

inline bool operator<(const SourceRange& left, const SourceRange& right)
{
    if (left.begin.offset != right.begin.offset)
    {
        return left.begin.offset < right.begin.offset;
    }
    return left.end.offset < right.end.offset;
}

} // namespace codeqa::core
