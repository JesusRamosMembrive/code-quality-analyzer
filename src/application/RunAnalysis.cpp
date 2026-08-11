#include "codeqa/application/RunAnalysis.h"

#include "codeqa/application/ApplyRules.h"
#include "codeqa/application/MeasureProject.h"

namespace codeqa::application
{

RunAnalysisResult RunAnalysis::execute(const RunAnalysisRequest& request) const
{
    RunAnalysisResult result;
    result.model = AnalyzeProject{}.execute(request.analysis);
    result.metrics = MeasureProject{}.execute(result.model);
    result.findings = ApplyRules{}.execute(result.model, result.metrics, request.config);
    return result;
}

} // namespace codeqa::application
