#pragma once

#include "codeqa/metrics/MetricsModels.h"
#include "codeqa/model/Models.h"

namespace codeqa::application
{

class MeasureProject
{
public:
    [[nodiscard]] codeqa::metrics::ProjectMetricsModel execute(const codeqa::model::ProjectModel& project) const;
};

} // namespace codeqa::application
