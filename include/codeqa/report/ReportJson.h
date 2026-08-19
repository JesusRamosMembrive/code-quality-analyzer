#pragma once

#include "codeqa/architecture/ArchitectureModels.h"
#include "codeqa/config/ConfigModels.h"
#include "codeqa/duplication/DuplicationModels.h"
#include "codeqa/metrics/MetricsModels.h"
#include "codeqa/model/Models.h"
#include "codeqa/rules/RuleModels.h"

#include <nlohmann/json.hpp>

#include <string>

namespace codeqa::report
{

/// metrics and findings must be index-aligned with model: metrics.files.size() and
/// findings.files.size() must both equal model.files.size(), with entry i of each vector
/// describing the same file, in the same order. Violating this precondition throws
/// std::invalid_argument.
[[nodiscard]] nlohmann::json toReportJson(
    const codeqa::model::ProjectModel& model,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::rules::ProjectFindingsModel& findings,
    const codeqa::duplication::ProjectDuplicationModel& duplicates,
    const codeqa::architecture::ProjectArchitectureModel& architecture,
    const codeqa::config::ProjectConfig& config,
    const std::string& toolVersion);

} // namespace codeqa::report
