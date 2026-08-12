#pragma once

#include <cstddef>

namespace codeqa::history
{

struct SnapshotComparison
{
    std::size_t newCount{0};
    std::size_t existingCount{0};
    std::size_t resolvedCount{0};
};

} // namespace codeqa::history
