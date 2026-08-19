#include "codeqa/application/RunAnalysis.h"

#include "codeqa/application/ApplyRules.h"
#include "codeqa/application/MeasureProject.h"
#include "codeqa/architecture/AnalyzeArchitecture.h"
#include "codeqa/duplication/DetectDuplicates.h"

namespace codeqa::application
{

RunAnalysisResult RunAnalysis::execute(
    const RunAnalysisRequest& request, const AnalysisProgressCallback progress) const
{
    RunAnalysisResult result;
    result.model = AnalyzeProject{}.execute(request.analysis, progress);

    if (progress)
    {
        progress(AnalysisPhase::MeasuringFunctions);
    }
    result.metrics = MeasureProject{}.execute(result.model);

    if (progress)
    {
        progress(AnalysisPhase::ApplyingRules);
    }
    result.findings = ApplyRules{}.execute(result.model, result.metrics, request.config);
    result.duplicates = codeqa::duplication::detectDuplicates(result.model, request.config.duplication);
    result.architecture = codeqa::architecture::analyzeArchitecture(result.model, request.config.architecture);
    return result;
}

} // namespace codeqa::application
