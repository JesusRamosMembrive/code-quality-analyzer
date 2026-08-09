#include "codeqa/rules/Rules.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace codeqa::rules
{

namespace
{

std::optional<Finding> evaluateThreshold(
    std::string ruleId,
    std::string description,
    std::string recommendation,
    long long observedValue,
    long long warningLimit,
    long long highLimit,
    codeqa::core::SourceLocation location,
    std::string symbolName)
{
    if (observedValue < warningLimit)
    {
        return std::nullopt;
    }

    Finding finding;
    finding.ruleId = std::move(ruleId);
    finding.severity = observedValue >= highLimit
        ? codeqa::core::DiagnosticSeverity::Error
        : codeqa::core::DiagnosticSeverity::Warning;
    finding.description = std::move(description);
    finding.recommendation = std::move(recommendation);
    finding.observedValue = observedValue;
    finding.limitValue = observedValue >= highLimit ? highLimit : warningLimit;
    finding.location = std::move(location);
    finding.symbolName = std::move(symbolName);
    return finding;
}

// Duplicated from CalculateFunctionMetrics.cpp deliberately -- this is its
// second occurrence, still under the "rule of three" this project already
// uses to decide when small predicates get shared vs. copied (see the Fase
// 6b final review and this phase's design doc, section 7).
bool isBooleanValueParameter(const codeqa::model::ParameterModel& parameter)
{
    if (parameter.passing != codeqa::model::ParameterPassing::Value)
    {
        return false;
    }

    std::istringstream stream(parameter.declaredType);
    std::vector<std::string> tokens;
    std::string token;

    while (stream >> token)
    {
        if (token != "const" && token != "volatile")
        {
            tokens.push_back(token);
        }
    }

    return tokens.size() == 1 && (tokens[0] == "bool" || tokens[0] == "_Bool");
}

std::vector<Finding> singleOrEmpty(const std::optional<Finding>& finding)
{
    return finding.has_value() ? std::vector<Finding>{*finding} : std::vector<Finding>{};
}

class FunctionLengthRule : public Rule
{
public:
    static constexpr long long warningLimit = 50;
    static constexpr long long highLimit = 90;

    [[nodiscard]] std::string id() const override { return "size.function-length"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        return singleOrEmpty(evaluateThreshold(
            id(),
            "Function is too long.",
            "Consider splitting this function into smaller ones.",
            static_cast<long long>(context.metrics.size.physicalLines),
            warningLimit,
            highLimit,
            context.callable.symbol.range.begin,
            context.callable.symbol.qualifiedName));
    }
};

class LocalVariableCountRule : public Rule
{
public:
    static constexpr long long warningLimit = 8;
    static constexpr long long highLimit = 12;

    [[nodiscard]] std::string id() const override { return "size.local-variables"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        return singleOrEmpty(evaluateThreshold(
            id(),
            "Function declares too many local variables.",
            "Consider extracting part of this function's logic into helper functions.",
            static_cast<long long>(context.callable.localVariables.size()),
            warningLimit,
            highLimit,
            context.callable.symbol.range.begin,
            context.callable.symbol.qualifiedName));
    }
};

class ParameterCountRule : public Rule
{
public:
    static constexpr long long warningLimit = 4;
    static constexpr long long highLimit = 7;

    [[nodiscard]] std::string id() const override { return "signature.parameter-count"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        const codeqa::core::SourceLocation location = context.callable.parameterListRange.has_value()
            ? context.callable.parameterListRange->begin
            : context.callable.symbol.range.begin;
        return singleOrEmpty(evaluateThreshold(
            id(),
            "Function has too many parameters.",
            "Consider grouping related parameters into a struct.",
            static_cast<long long>(context.metrics.signature.parameterCount),
            warningLimit,
            highLimit,
            location,
            context.callable.symbol.qualifiedName));
    }
};

class ConsecutiveBooleanParametersRule : public Rule
{
public:
    static constexpr long long warningLimit = 2;
    static constexpr long long highLimit = 3;

    [[nodiscard]] std::string id() const override { return "signature.consecutive-booleans"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        std::size_t longestRun = 0;
        std::size_t currentRun = 0;

        for (const codeqa::model::ParameterModel& parameter : context.callable.parameters)
        {
            if (isBooleanValueParameter(parameter))
            {
                ++currentRun;
                longestRun = std::max(longestRun, currentRun);
            }
            else
            {
                currentRun = 0;
            }
        }

        const codeqa::core::SourceLocation location = context.callable.parameterListRange.has_value()
            ? context.callable.parameterListRange->begin
            : context.callable.symbol.range.begin;
        return singleOrEmpty(evaluateThreshold(
            id(),
            "Function has consecutive boolean parameters.",
            "Consider replacing consecutive boolean parameters with an enum or a struct to "
            "avoid the \"boolean trap\" at call sites.",
            static_cast<long long>(longestRun),
            warningLimit,
            highLimit,
            location,
            context.callable.symbol.qualifiedName));
    }
};

class CyclomaticComplexityRule : public Rule
{
public:
    static constexpr long long warningLimit = 10;
    static constexpr long long highLimit = 15;

    [[nodiscard]] std::string id() const override { return "complexity.cyclomatic"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        return singleOrEmpty(evaluateThreshold(
            id(),
            "Cyclomatic complexity is too high.",
            "Consider splitting this function to reduce the number of independent paths through it.",
            static_cast<long long>(context.metrics.complexity.cyclomaticComplexity),
            warningLimit,
            highLimit,
            context.callable.symbol.range.begin,
            context.callable.symbol.qualifiedName));
    }
};

class ReturnCountRule : public Rule
{
public:
    static constexpr long long warningLimit = 4;
    static constexpr long long highLimit = 6;

    [[nodiscard]] std::string id() const override { return "complexity.returns"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        return singleOrEmpty(evaluateThreshold(
            id(),
            "Function has too many return points.",
            "Consider consolidating exit points to make control flow easier to follow.",
            static_cast<long long>(context.metrics.complexity.returnCount),
            warningLimit,
            highLimit,
            context.callable.symbol.range.begin,
            context.callable.symbol.qualifiedName));
    }
};

class ConditionComplexityRule : public Rule
{
public:
    static constexpr long long warningLimit = 3;
    static constexpr long long highLimit = 5;

    [[nodiscard]] std::string id() const override { return "complexity.condition"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        if (!context.metrics.complexity.maxConditionRange.has_value())
        {
            return {};
        }

        return singleOrEmpty(evaluateThreshold(
            id(),
            "A condition in this function is too complex.",
            "Consider extracting this condition into a well-named helper function or variable.",
            static_cast<long long>(context.metrics.complexity.maxConditionComplexity),
            warningLimit,
            highLimit,
            context.metrics.complexity.maxConditionRange->begin,
            context.callable.symbol.qualifiedName));
    }
};

class NestingDepthRule : public Rule
{
public:
    static constexpr long long warningLimit = 3;
    static constexpr long long highLimit = 5;

    [[nodiscard]] std::string id() const override { return "complexity.nesting"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        codeqa::core::SourceLocation location = context.callable.symbol.range.begin;

        for (const codeqa::model::ControlFlowModel& flow : context.callable.controlFlow)
        {
            if (flow.nestingDepth == context.metrics.complexity.maxNestingDepth)
            {
                location = flow.range.begin;
                break;
            }
        }

        return singleOrEmpty(evaluateThreshold(
            id(),
            "This function has excessive nesting.",
            "Consider using early returns or extracting nested blocks into helper functions.",
            static_cast<long long>(context.metrics.complexity.maxNestingDepth),
            warningLimit,
            highLimit,
            location,
            context.callable.symbol.qualifiedName));
    }
};

} // namespace

std::vector<std::unique_ptr<Rule>> allRules()
{
    std::vector<std::unique_ptr<Rule>> rules;
    rules.push_back(std::make_unique<FunctionLengthRule>());
    rules.push_back(std::make_unique<LocalVariableCountRule>());
    rules.push_back(std::make_unique<ParameterCountRule>());
    rules.push_back(std::make_unique<ConsecutiveBooleanParametersRule>());
    rules.push_back(std::make_unique<CyclomaticComplexityRule>());
    rules.push_back(std::make_unique<ReturnCountRule>());
    rules.push_back(std::make_unique<ConditionComplexityRule>());
    rules.push_back(std::make_unique<NestingDepthRule>());
    return rules;
}

} // namespace codeqa::rules
