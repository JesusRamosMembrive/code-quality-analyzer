#pragma once

#include <functional>

namespace codeqa::application
{

enum class AnalysisPhase
{
    DiscoveringFiles,
    ParsingSourceFiles,
    MeasuringFunctions,
    ApplyingRules,
    PreparingReport
};

using AnalysisProgressCallback = std::function<void(AnalysisPhase)>;

} // namespace codeqa::application
