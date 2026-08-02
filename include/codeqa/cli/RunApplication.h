#pragma once

#include <ostream>
#include <string>
#include <vector>

namespace codeqa::cli
{

[[nodiscard]] int runApplication(
    const std::vector<std::string>& arguments,
    std::ostream& output,
    std::ostream& errorOutput);

} // namespace codeqa::cli
