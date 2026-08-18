#pragma once

#include <string>
#include <vector>

namespace codeqa::infrastructure
{

std::string toLower(std::string text);

std::vector<std::string> splitLines(const std::string& content);

} // namespace codeqa::infrastructure
