#include "codeqa/core/SourceRangeText.h"

#include <algorithm>

namespace codeqa::core
{

bool positionInRange(std::size_t line, std::size_t column, const SourceRange& range)
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

std::string extractRangeText(const SourceRange& range, const std::vector<std::string>& sourceLines)
{
    static const std::string emptyLine;
    std::string result;

    for (std::size_t line = range.begin.line; line <= range.end.line; ++line)
    {
        const std::string& text = (line - 1) < sourceLines.size() ? sourceLines[line - 1] : emptyLine;
        const std::size_t begin = (line == range.begin.line) ? range.begin.column - 1 : 0;
        const std::size_t end =
            (line == range.end.line) ? std::min(text.size(), range.end.column - 1) : text.size();

        if (begin < end)
        {
            result += text.substr(begin, end - begin);
        }

        if (line != range.end.line)
        {
            result += '\n';
        }
    }

    return result;
}

} // namespace codeqa::core
