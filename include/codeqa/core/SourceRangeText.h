#pragma once

#include "codeqa/core/SourceRange.h"

#include <cstddef>
#include <string>
#include <vector>

namespace codeqa::core
{

bool positionInRange(std::size_t line, std::size_t column, const SourceRange& range);

std::string extractRangeText(const SourceRange& range, const std::vector<std::string>& sourceLines);

} // namespace codeqa::core
