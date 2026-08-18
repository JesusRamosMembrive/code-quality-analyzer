#include "codeqa/metrics/CalculateFunctionMetrics.h"

#include "codeqa/core/SourceRangeText.h"
#include "codeqa/model/ParameterUtilities.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>

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
                return codeqa::core::positionInRange(line, column, comment);
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

std::string trim(const std::string& text)
{
    const std::size_t begin = text.find_first_not_of(" \t\r\n\f\v");

    if (begin == std::string::npos)
    {
        return std::string{};
    }

    const std::size_t end = text.find_last_not_of(" \t\r\n\f\v");
    return text.substr(begin, end - begin + 1);
}

std::size_t collapsedLength(const std::string& text)
{
    std::size_t length = 0;
    bool inWhitespaceRun = false;

    for (const char character : text)
    {
        const bool isWhitespace = character == ' ' || character == '\t' || character == '\n'
            || character == '\r' || character == '\f' || character == '\v';

        if (isWhitespace)
        {
            if (!inWhitespaceRun)
            {
                ++length;
                inWhitespaceRun = true;
            }

            continue;
        }

        inWhitespaceRun = false;
        ++length;
    }

    return length;
}

FunctionSignatureMetrics calculateSignatureMetrics(
    const codeqa::model::CallableModel& callable, const std::vector<std::string>& sourceLines)
{
    FunctionSignatureMetrics metrics;
    metrics.parameterCount = callable.parameters.size();

    if (callable.parameterListRange.has_value())
    {
        const std::string text = codeqa::core::extractRangeText(*callable.parameterListRange, sourceLines);
        const std::string inner = text.size() >= 2 ? text.substr(1, text.size() - 2) : std::string{};
        metrics.signatureLength = collapsedLength(trim(inner));
    }

    for (const codeqa::model::ParameterModel& parameter : callable.parameters)
    {
        if (codeqa::model::isBooleanValueParameter(parameter))
        {
            ++metrics.booleanParameterCount;
        }
    }

    return metrics;
}

bool countsAsBranch(codeqa::model::ControlFlowKind kind)
{
    switch (kind)
    {
        case codeqa::model::ControlFlowKind::If:
        case codeqa::model::ControlFlowKind::For:
        case codeqa::model::ControlFlowKind::While:
        case codeqa::model::ControlFlowKind::DoWhile:
        case codeqa::model::ControlFlowKind::Case:
        case codeqa::model::ControlFlowKind::Conditional:
            return true;
        default:
            return false;
    }
}

bool isDigitSeparator(const std::string& text, std::size_t index)
{
    if (index == 0 || index + 1 >= text.size())
    {
        return false;
    }

    const auto isHexDigit = [](char character) {
        return std::isxdigit(static_cast<unsigned char>(character)) != 0;
    };

    return isHexDigit(text[index - 1]) && isHexDigit(text[index + 1]);
}

std::size_t countBooleanOperators(
    const codeqa::core::SourceRange& range,
    const std::vector<std::string>& sourceLines,
    const std::vector<codeqa::core::SourceRange>& fileComments)
{
    static const std::string emptyLine;
    std::size_t count = 0;
    bool inString = false;
    bool inChar = false;

    for (std::size_t line = range.begin.line; line <= range.end.line; ++line)
    {
        const std::string& text = (line - 1) < sourceLines.size() ? sourceLines[line - 1] : emptyLine;
        const std::size_t startIndex = (line == range.begin.line) ? range.begin.column - 1 : 0;
        const std::size_t endIndex =
            (line == range.end.line) ? std::min(text.size(), range.end.column - 1) : text.size();

        std::size_t index = startIndex;

        while (index < endIndex)
        {
            const char character = text[index];
            const std::size_t column = index + 1;
            const bool covered = std::any_of(
                fileComments.begin(), fileComments.end(),
                [line, column](const codeqa::core::SourceRange& comment) {
                    return positionInRange(line, column, comment);
                });

            if (covered)
            {
                ++index;
                continue;
            }

            if (inString || inChar)
            {
                if (character == '\\')
                {
                    index += 2;
                    continue;
                }

                if ((inString && character == '"') || (inChar && character == '\''))
                {
                    inString = false;
                    inChar = false;
                }

                ++index;
                continue;
            }

            if (character == '"')
            {
                inString = true;
                ++index;
                continue;
            }

            if (character == '\'')
            {
                if (isDigitSeparator(text, index))
                {
                    ++index;
                    continue;
                }

                inChar = true;
                ++index;
                continue;
            }

            if (index + 1 < text.size()
                && ((character == '&' && text[index + 1] == '&')
                    || (character == '|' && text[index + 1] == '|')))
            {
                ++count;
                index += 2;
                continue;
            }

            ++index;
        }
    }

    return count;
}

FunctionComplexityMetrics calculateComplexityMetrics(
    const codeqa::model::CallableModel& callable,
    const std::vector<std::string>& sourceLines,
    const std::vector<codeqa::core::SourceRange>& fileComments)
{
    FunctionComplexityMetrics metrics;
    std::size_t booleanOperatorCount = 0;

    for (const codeqa::model::ControlFlowModel& flow : callable.controlFlow)
    {
        if (countsAsBranch(flow.kind))
        {
            ++metrics.branchCount;
        }

        if (flow.kind == codeqa::model::ControlFlowKind::Return)
        {
            ++metrics.returnCount;
        }

        metrics.maxNestingDepth = std::max(metrics.maxNestingDepth, flow.nestingDepth);

        if (flow.conditionRange.has_value())
        {
            const std::size_t operators = countBooleanOperators(*flow.conditionRange, sourceLines, fileComments);

            if (operators > metrics.maxConditionComplexity)
            {
                metrics.maxConditionComplexity = operators;
                metrics.maxConditionRange = flow.conditionRange;
            }

            booleanOperatorCount += operators;
        }
    }

    metrics.cyclomaticComplexity = 1 + metrics.branchCount + booleanOperatorCount;
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
    metrics.signature = calculateSignatureMetrics(callable, sourceLines);
    metrics.complexity = calculateComplexityMetrics(callable, sourceLines, fileComments);
    return metrics;
}

} // namespace codeqa::metrics
