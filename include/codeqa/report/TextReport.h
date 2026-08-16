#pragma once

#include "codeqa/metrics/MetricsModels.h"
#include "codeqa/model/Models.h"
#include "codeqa/rules/RuleModels.h"
#include "codeqa/duplication/DuplicationModels.h"

#include <ostream>

namespace codeqa::report
{

void renderText(
    std::ostream& output,
    const codeqa::model::ProjectModel& model,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::rules::ProjectFindingsModel& findings,
    const codeqa::duplication::ProjectDuplicationModel& duplicates);

} // namespace codeqa::report
