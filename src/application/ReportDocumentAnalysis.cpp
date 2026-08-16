#include "codeqa/application/ReportDocumentAnalysis.h"

#include <utility>

namespace codeqa::application
{

RunAnalysisResult toAnalysisResult(const ReportDocument& document)
{
    RunAnalysisResult analysis;
    analysis.model.rootPath = document.metadata.rootPath;
    analysis.metrics.rootPath = analysis.model.rootPath;
    analysis.findings.rootPath = analysis.model.rootPath;

    for (const ReportFile& reportFile : document.files)
    {
        const std::filesystem::path absolutePath = analysis.model.rootPath / reportFile.path;

        codeqa::model::FileModel modelFile;
        modelFile.path = absolutePath;
        modelFile.language = reportFile.language;
        modelFile.state = reportFile.state;
        modelFile.diagnostics = reportFile.diagnostics;
        analysis.model.files.push_back(std::move(modelFile));

        codeqa::metrics::FileMetricsModel metricsFile;
        metricsFile.path = absolutePath;
        metricsFile.functions = reportFile.metrics;
        analysis.metrics.files.push_back(std::move(metricsFile));

        codeqa::rules::FileFindingsModel findingsFile;
        findingsFile.path = absolutePath;
        findingsFile.findings = reportFile.findings;
        analysis.findings.files.push_back(std::move(findingsFile));
    }

    return analysis;
}

} // namespace codeqa::application
