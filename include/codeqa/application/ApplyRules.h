#pragma once

#include "codeqa/config/ConfigModels.h"
#include "codeqa/metrics/MetricsModels.h"
#include "codeqa/model/Models.h"
#include "codeqa/rules/RuleModels.h"

namespace codeqa::application
{

class ApplyRules
{
public:
    /// config must satisfy the same precondition as codeqa::rules::allRules(): pass
    /// codeqa::config::defaultConfig() (optionally merged via codeqa::config::mergeConfig())
    /// rather than a default-constructed codeqa::config::ProjectConfig{}, which has an
    /// empty rules map and will cause allRules() to throw.
    [[nodiscard]] codeqa::rules::ProjectFindingsModel execute(
        const codeqa::model::ProjectModel& project,
        const codeqa::metrics::ProjectMetricsModel& metrics,
        const codeqa::config::ProjectConfig& config) const;
};

} // namespace codeqa::application
