#pragma once

#include "codeqa/application/AnalyzeProject.h"
#include "codeqa/config/ConfigModels.h"
#include "codeqa/metrics/MetricsModels.h"
#include "codeqa/model/Models.h"
#include "codeqa/rules/RuleModels.h"

namespace codeqa::application
{

struct RunAnalysisRequest
{
    AnalyzeProjectRequest analysis;
    codeqa::config::ProjectConfig config;
};

struct RunAnalysisResult
{
    codeqa::model::ProjectModel model;
    codeqa::metrics::ProjectMetricsModel metrics;
    codeqa::rules::ProjectFindingsModel findings;
};

class RunAnalysis
{
public:
    [[nodiscard]] RunAnalysisResult execute(const RunAnalysisRequest& request) const;
};

} // namespace codeqa::application
