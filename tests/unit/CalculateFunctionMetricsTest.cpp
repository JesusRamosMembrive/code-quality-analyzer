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

codeqa::model::ControlFlowModel makeFlow(
    codeqa::model::ControlFlowKind kind, std::size_t nestingDepth)
{
    codeqa::model::ControlFlowModel flow;
    flow.kind = kind;
    flow.nestingDepth = nestingDepth;
    return flow;
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

TEST_CASE("signature length counts the parameter list text, excluding parentheses")
{
    const std::vector<std::string> sourceLines{"int classify(int value, bool strict)"};
    codeqa::model::CallableModel callable = makeCallable(1, 1, 0);
    callable.parameters = {
        codeqa::model::ParameterModel{"value", codeqa::model::ParameterPassing::Value, false, "int"},
        codeqa::model::ParameterModel{"strict", codeqa::model::ParameterPassing::Value, false, "bool"}};
    callable.parameterListRange = lineSpan(1, 13, 1, 37);

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, {}, sourceLines);

    CHECK(metrics.signature.parameterCount == 2);
    CHECK(metrics.signature.signatureLength == 22);
    CHECK(metrics.signature.booleanParameterCount == 1);
}

TEST_CASE("signature length collapses whitespace so a multi-line signature measures the same as one line")
{
    const std::vector<std::string> multiLine{"int build(", "    int width,", "    int height)"};
    codeqa::model::CallableModel multiLineCallable = makeCallable(1, 3, 0);
    multiLineCallable.parameters = {
        codeqa::model::ParameterModel{"width", codeqa::model::ParameterPassing::Value, false, "int"},
        codeqa::model::ParameterModel{"height", codeqa::model::ParameterPassing::Value, false, "int"}};
    multiLineCallable.parameterListRange = lineSpan(1, 10, 3, 16);

    const codeqa::metrics::FunctionMetricsModel multiLineMetrics =
        codeqa::metrics::calculateFunctionMetrics(multiLineCallable, {}, multiLine);

    const std::vector<std::string> singleLine{"int build(int width, int height)"};
    codeqa::model::CallableModel singleLineCallable = makeCallable(1, 1, 0);
    singleLineCallable.parameters = multiLineCallable.parameters;
    singleLineCallable.parameterListRange = lineSpan(1, 10, 1, 33);

    const codeqa::metrics::FunctionMetricsModel singleLineMetrics =
        codeqa::metrics::calculateFunctionMetrics(singleLineCallable, {}, singleLine);

    CHECK(multiLineMetrics.signature.signatureLength == singleLineMetrics.signature.signatureLength);
    CHECK(multiLineMetrics.signature.signatureLength == 21);
}

TEST_CASE("an empty parameter list has signature length zero")
{
    const std::vector<std::string> sourceLines{"void noop()"};
    codeqa::model::CallableModel callable = makeCallable(1, 1, 0);
    callable.parameterListRange = lineSpan(1, 10, 1, 12);

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, {}, sourceLines);

    CHECK(metrics.signature.parameterCount == 0);
    CHECK(metrics.signature.signatureLength == 0);
}

TEST_CASE("a callable with no parameter list has signature length zero")
{
    const std::vector<std::string> sourceLines{"auto lambda = []{ return 1; };"};
    codeqa::model::CallableModel callable = makeCallable(1, 1, 0);

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, {}, sourceLines);

    CHECK(metrics.signature.parameterCount == 0);
    CHECK(metrics.signature.signatureLength == 0);
}

TEST_CASE("boolean parameter count requires both a bool-family type and pass-by-value")
{
    const std::vector<std::string> sourceLines{"void f(bool a, bool b, bool* c, bool& d, int e)"};
    codeqa::model::CallableModel callable = makeCallable(1, 1, 0);
    callable.parameters = {
        codeqa::model::ParameterModel{"a", codeqa::model::ParameterPassing::Value, false, "bool"},
        codeqa::model::ParameterModel{"b", codeqa::model::ParameterPassing::Value, false, "const bool"},
        codeqa::model::ParameterModel{"c", codeqa::model::ParameterPassing::Pointer, false, "bool"},
        codeqa::model::ParameterModel{"d", codeqa::model::ParameterPassing::Reference, false, "bool"},
        codeqa::model::ParameterModel{"e", codeqa::model::ParameterPassing::Value, false, "int"}};
    callable.parameterListRange = lineSpan(1, 7, 1, 48);

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, {}, sourceLines);

    CHECK(metrics.signature.parameterCount == 5);
    CHECK(metrics.signature.booleanParameterCount == 2);
}

TEST_CASE("complexity metrics for an if/else-if chain with && and ||, matching classify() in the corpus")
{
    const std::vector<std::string> sourceLines{
        "int classify(int value, bool strict)",
        "{",
        "    if (value < 0 && strict)",
        "    {",
        "        return -1;",
        "    }",
        "    else if (value == 0 || !strict)",
        "    {",
        "        return 0;",
        "    }",
        "",
        "    return 1;",
        "}"};
    codeqa::model::CallableModel callable = makeCallable(1, 13, 5);

    codeqa::model::ControlFlowModel outerIf = makeFlow(codeqa::model::ControlFlowKind::If, 0);
    outerIf.conditionRange = lineSpan(3, 9, 3, 28);
    codeqa::model::ControlFlowModel innerIf = makeFlow(codeqa::model::ControlFlowKind::If, 1);
    innerIf.conditionRange = lineSpan(7, 14, 7, 35);
    callable.controlFlow = {
        outerIf, makeFlow(codeqa::model::ControlFlowKind::Return, 1), innerIf,
        makeFlow(codeqa::model::ControlFlowKind::Return, 2), makeFlow(codeqa::model::ControlFlowKind::Return, 0)};

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, {}, sourceLines);

    CHECK(metrics.complexity.branchCount == 2);
    CHECK(metrics.complexity.cyclomaticComplexity == 5);
    CHECK(metrics.complexity.maxConditionComplexity == 1);
    CHECK(metrics.complexity.maxNestingDepth == 2);
    CHECK(metrics.complexity.returnCount == 3);
}

TEST_CASE("switch/case/default: only Case counts as a branch, matching describe() in the corpus")
{
    codeqa::model::CallableModel callable = makeCallable(1, 12, 7);
    callable.controlFlow = {
        makeFlow(codeqa::model::ControlFlowKind::Switch, 0),
        makeFlow(codeqa::model::ControlFlowKind::Case, 1),
        makeFlow(codeqa::model::ControlFlowKind::Return, 2),
        makeFlow(codeqa::model::ControlFlowKind::Case, 1),
        makeFlow(codeqa::model::ControlFlowKind::Return, 2),
        makeFlow(codeqa::model::ControlFlowKind::Default, 1),
        makeFlow(codeqa::model::ControlFlowKind::Return, 2)};

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, {}, {});

    CHECK(metrics.complexity.branchCount == 2);
    CHECK(metrics.complexity.cyclomaticComplexity == 3);
    CHECK(metrics.complexity.maxConditionComplexity == 0);
    CHECK(metrics.complexity.maxNestingDepth == 2);
    CHECK(metrics.complexity.returnCount == 3);
}

TEST_CASE("a function with no control flow has cyclomatic complexity 1 and depth 0")
{
    codeqa::model::CallableModel callable = makeCallable(1, 3, 1);

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, {}, {});

    CHECK(metrics.complexity.branchCount == 0);
    CHECK(metrics.complexity.cyclomaticComplexity == 1);
    CHECK(metrics.complexity.maxConditionComplexity == 0);
    CHECK(metrics.complexity.maxNestingDepth == 0);
    CHECK(metrics.complexity.returnCount == 0);
}

TEST_CASE("a boolean operator inside a comment inside a condition is not counted")
{
    const std::vector<std::string> sourceLines{"if (a /* && b */ && c) { }"};
    codeqa::model::CallableModel callable = makeCallable(1, 1, 0);
    codeqa::model::ControlFlowModel flow = makeFlow(codeqa::model::ControlFlowKind::If, 0);
    flow.conditionRange = lineSpan(1, 5, 1, 22);
    callable.controlFlow = {flow};
    const std::vector<codeqa::core::SourceRange> fileComments{lineSpan(1, 7, 1, 17)};

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, fileComments, sourceLines);

    CHECK(metrics.complexity.maxConditionComplexity == 1);
    CHECK(metrics.complexity.cyclomaticComplexity == 3);
}

TEST_CASE("a boolean operator inside a string literal inside a condition is not counted")
{
    const std::vector<std::string> sourceLines{"if (strcmp(a, \"x && y\") && ok) { }"};
    codeqa::model::CallableModel callable = makeCallable(1, 1, 0);
    codeqa::model::ControlFlowModel flow = makeFlow(codeqa::model::ControlFlowKind::If, 0);
    flow.conditionRange = lineSpan(1, 5, 1, 30);
    callable.controlFlow = {flow};

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, {}, sourceLines);

    CHECK(metrics.complexity.maxConditionComplexity == 1);
}

TEST_CASE("a digit separator in a numeric literal is not mistaken for a character literal")
{
    const std::vector<std::string> sourceLines{"if (count > 1'000 && flag) { }"};
    codeqa::model::CallableModel callable = makeCallable(1, 1, 0);
    codeqa::model::ControlFlowModel flow = makeFlow(codeqa::model::ControlFlowKind::If, 0);
    flow.conditionRange = lineSpan(1, 5, 1, 26);
    callable.controlFlow = {flow};

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, {}, sourceLines);

    CHECK(metrics.complexity.maxConditionComplexity == 1);
}

TEST_CASE("an apostrophe inside a comment inside a condition does not corrupt the operator scan")
{
    const std::vector<std::string> sourceLines{"if (count > 0 /* don't count zero */ && flag) { }"};
    codeqa::model::CallableModel callable = makeCallable(1, 1, 0);
    codeqa::model::ControlFlowModel flow = makeFlow(codeqa::model::ControlFlowKind::If, 0);
    flow.conditionRange = lineSpan(1, 5, 1, 45);
    callable.controlFlow = {flow};
    const std::vector<codeqa::core::SourceRange> fileComments{lineSpan(1, 15, 1, 37)};

    const codeqa::metrics::FunctionMetricsModel metrics =
        codeqa::metrics::calculateFunctionMetrics(callable, fileComments, sourceLines);

    CHECK(metrics.complexity.maxConditionComplexity == 1);
    CHECK(metrics.complexity.cyclomaticComplexity == 3);
}
