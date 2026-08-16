#pragma once

#include "codeqa/application/ReportDocument.h"
#include "codeqa/application/RunAnalysis.h"

namespace codeqa::application
{

/// Converts a ReportDocument (the flat, GUI/JSON-friendly shape) back into the parallel
/// model/metrics/findings triplet that codeqa::report's renderers (renderText, renderMarkdown)
/// take as input. Used by both the CLI (analyze --format text|markdown) and the desktop
/// AppController (exporting the currently loaded report), so it has exactly one implementation.
[[nodiscard]] RunAnalysisResult toAnalysisResult(const ReportDocument& document);

} // namespace codeqa::application
