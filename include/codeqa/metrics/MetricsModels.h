#pragma once

#include "codeqa/core/Diagnostic.h"
#include "codeqa/core/SourceRange.h"
#include "codeqa/model/Models.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <vector>

namespace codeqa::metrics
{

struct FunctionSizeMetrics
{
    std::size_t physicalLines{0};
    std::size_t codeLines{0};
    std::size_t blankLines{0};
    std::size_t commentLines{0};
    std::size_t statementCount{0};
};

struct FunctionSignatureMetrics
{
    std::size_t parameterCount{0};
    std::size_t signatureLength{0};
    std::size_t booleanParameterCount{0};
};

struct FunctionComplexityMetrics
{
    std::size_t cyclomaticComplexity{0};
    std::size_t maxConditionComplexity{0};
    std::size_t maxNestingDepth{0};
    std::size_t branchCount{0};
    std::size_t returnCount{0};
    std::optional<codeqa::core::SourceRange> maxConditionRange;
};

struct FunctionMetricsModel
{
    codeqa::model::SymbolIdentity symbol;
    FunctionSizeMetrics size;
    FunctionSignatureMetrics signature;
    FunctionComplexityMetrics complexity;
};

struct FileMetricsModel
{
    std::filesystem::path path;
    std::vector<FunctionMetricsModel> functions;
    std::vector<codeqa::core::Diagnostic> diagnostics;
};

struct ProjectMetricsModel
{
    std::filesystem::path rootPath;
    std::vector<FileMetricsModel> files;
};

} // namespace codeqa::metrics
