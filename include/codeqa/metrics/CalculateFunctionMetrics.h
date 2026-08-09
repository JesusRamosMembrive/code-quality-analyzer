#pragma once

#include "codeqa/core/SourceRange.h"
#include "codeqa/metrics/MetricsModels.h"
#include "codeqa/model/Models.h"

#include <string>
#include <vector>

namespace codeqa::metrics
{

[[nodiscard]] FunctionMetricsModel calculateFunctionMetrics(
    const codeqa::model::CallableModel& callable,
    const std::vector<codeqa::core::SourceRange>& fileComments,
    const std::vector<std::string>& sourceLines);

} // namespace codeqa::metrics
