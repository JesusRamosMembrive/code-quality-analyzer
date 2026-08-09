#include "codeqa/metrics/CalculateFunctionMetrics.h"

#include <algorithm>
#include <cstddef>

namespace codeqa::metrics
{

namespace
{

enum class LineClassification
{
    Blank,
    Comment,
    Code
};

bool isHorizontalWhitespace(char character)
{
    return character == ' ' || character == '\t' || character == '\f' || character == '\v';
}

bool positionInRange(std::size_t line, std::size_t column, const codeqa::core::SourceRange& range)
{
    if (line < range.begin.line || line > range.end.line)
    {
        return false;
    }

    if (line == range.begin.line && column < range.begin.column)
    {
        return false;
    }

    if (line == range.end.line && column >= range.end.column)
    {
        return false;
    }

    return true;
}

LineClassification classifyLine(
    std::size_t line,
    const std::string& text,
    const std::vector<codeqa::core::SourceRange>& fileComments)
{
    bool sawNonBlank = false;
    bool allCommentCovered = true;

    for (std::size_t index = 0; index < text.size(); ++index)
    {
        if (isHorizontalWhitespace(text[index]))
        {
            continue;
        }

        sawNonBlank = true;
        const std::size_t column = index + 1;
        const bool covered = std::any_of(
            fileComments.begin(), fileComments.end(), [line, column](const codeqa::core::SourceRange& comment) {
                return positionInRange(line, column, comment);
            });

        if (!covered)
        {
            allCommentCovered = false;
            break;
        }
    }

    if (!sawNonBlank)
    {
        return LineClassification::Blank;
    }

    return allCommentCovered ? LineClassification::Comment : LineClassification::Code;
}

FunctionSizeMetrics calculateSizeMetrics(
    const codeqa::model::CallableModel& callable,
    const std::vector<codeqa::core::SourceRange>& fileComments,
    const std::vector<std::string>& sourceLines)
{
    FunctionSizeMetrics metrics;
    const std::size_t beginLine = callable.symbol.range.begin.line;
    const std::size_t endLine = callable.symbol.range.end.line;
    metrics.physicalLines = endLine - beginLine + 1;

    static const std::string emptyLine;

    for (std::size_t line = beginLine; line <= endLine; ++line)
    {
        const std::string& text = (line - 1) < sourceLines.size() ? sourceLines[line - 1] : emptyLine;

        switch (classifyLine(line, text, fileComments))
        {
            case LineClassification::Blank:
                ++metrics.blankLines;
                break;
            case LineClassification::Comment:
                ++metrics.commentLines;
                break;
            case LineClassification::Code:
                break;
        }
    }

    metrics.codeLines = metrics.physicalLines - metrics.blankLines - metrics.commentLines;
    metrics.statementCount = callable.statements.size();
    return metrics;
}

} // namespace

FunctionMetricsModel calculateFunctionMetrics(
    const codeqa::model::CallableModel& callable,
    const std::vector<codeqa::core::SourceRange>& fileComments,
    const std::vector<std::string>& sourceLines)
{
    FunctionMetricsModel metrics;
    metrics.symbol = callable.symbol;
    metrics.size = calculateSizeMetrics(callable, fileComments, sourceLines);
    return metrics;
}

} // namespace codeqa::metrics
