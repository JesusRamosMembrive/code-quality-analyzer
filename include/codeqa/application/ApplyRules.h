#pragma once

#include "codeqa/metrics/MetricsModels.h"
#include "codeqa/model/Models.h"
#include "codeqa/rules/RuleModels.h"

namespace codeqa::application
{

class ApplyRules
{
public:
    [[nodiscard]] codeqa::rules::ProjectFindingsModel execute(
        const codeqa::model::ProjectModel& project,
        const codeqa::metrics::ProjectMetricsModel& metrics) const;
};

} // namespace codeqa::application
