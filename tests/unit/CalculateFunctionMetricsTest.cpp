#include "doctest/doctest.h"

#include "codeqa/metrics/CalculateFunctionMetrics.h"

#include <string>
#include <vector>

namespace
{

codeqa::model::CallableModel makeCallable(std::size_t beginLine, std::size_t endLine, std::size_t statementCount)
{
    codeqa::model::CallableModel callable;
    callable.symbol.name = "sample";
    callable.symbol.range.begin.line = beginLine;
    callable.symbol.range.end.line = endLine;
    callable.statements.resize(statementCount);
    return callable;
}

codeqa::core::SourceRange lineSpan(
    std::size_t beginLine, std::size_t beginColumn, std::size_t endLine, std::size_t endColumn)
{
    codeqa::core::SourceRange range;
    range.begin.line = beginLine;
    range.begin.column = beginColumn;
    range.end.line = endLine;
    range.end.column = endColumn;
    return range;
}

} // namespace

TEST_CASE("size metrics classify blank, comment and code lines")
{
    const std::vector<std::string> sourceLines{
        "int add(int a, int b)",
        "{",
        "    // sum the values",
        "",
        "    return a + b;",
        "}"};
    const std::vector<codeqa::core::SourceRange> fileComments{lineSpan(3, 5, 3, 22)};
    const codeqa::model::CallableModel callable = makeCallable(1, 6, 1);

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, fileComments, sourceLines);

    CHECK(metrics.size.physicalLines == 6);
    CHECK(metrics.size.blankLines == 1);
    CHECK(metrics.size.commentLines == 1);
    CHECK(metrics.size.codeLines == 4);
    CHECK(metrics.size.statementCount == 1);
}

TEST_CASE("a blank line inside a block comment counts as blank, not comment")
{
    const std::vector<std::string> sourceLines{
        "int f()",
        "{",
        "    /* line one",
        "",
        "       line two */",
        "    return 0;",
        "}"};
    const std::vector<codeqa::core::SourceRange> fileComments{lineSpan(3, 5, 5, 19)};
    const codeqa::model::CallableModel callable = makeCallable(1, 7, 1);

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, fileComments, sourceLines);

    CHECK(metrics.size.physicalLines == 7);
    CHECK(metrics.size.blankLines == 1);
    CHECK(metrics.size.commentLines == 2);
    CHECK(metrics.size.codeLines == 4);
}

TEST_CASE("a line with code followed by a trailing comment is a code line, not a comment line")
{
    const std::vector<std::string> sourceLines{
        "int g()",
        "{",
        "    int x = 1; // note",
        "    return x;",
        "}"};
    const std::vector<codeqa::core::SourceRange> fileComments{lineSpan(3, 16, 3, 23)};
    const codeqa::model::CallableModel callable = makeCallable(1, 5, 2);

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, fileComments, sourceLines);

    CHECK(metrics.size.physicalLines == 5);
    CHECK(metrics.size.blankLines == 0);
    CHECK(metrics.size.commentLines == 0);
    CHECK(metrics.size.codeLines == 5);
    CHECK(metrics.size.statementCount == 2);
}
