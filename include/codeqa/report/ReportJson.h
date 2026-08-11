#pragma once

#include "codeqa/config/ConfigModels.h"
#include "codeqa/metrics/MetricsModels.h"
#include "codeqa/model/Models.h"
#include "codeqa/rules/RuleModels.h"

#include <nlohmann/json.hpp>

#include <string>

namespace codeqa::report
{

[[nodiscard]] nlohmann::json toReportJson(
    const codeqa::model::ProjectModel& model,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::rules::ProjectFindingsModel& findings,
    const codeqa::config::ProjectConfig& config,
    const std::string& toolVersion);

} // namespace codeqa::report
