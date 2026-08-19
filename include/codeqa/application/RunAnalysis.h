#pragma once

#include "codeqa/application/AnalysisProgress.h"
#include "codeqa/application/AnalyzeProject.h"
#include "codeqa/architecture/ArchitectureModels.h"
#include "codeqa/config/ConfigModels.h"
#include "codeqa/duplication/DuplicationModels.h"
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
    codeqa::duplication::ProjectDuplicationModel duplicates;
    codeqa::architecture::ProjectArchitectureModel architecture;
};

class RunAnalysis
{
public:
    [[nodiscard]] RunAnalysisResult execute(
        const RunAnalysisRequest& request, AnalysisProgressCallback progress = {}) const;
};

} // namespace codeqa::application
