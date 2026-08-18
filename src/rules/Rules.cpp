#include "codeqa/rules/Rules.h"

#include "codeqa/model/ParameterUtilities.h"

#include <algorithm>
#include <memory>
#include <optional>
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

std::vector<Finding> singleOrEmpty(const std::optional<Finding>& finding)
{
    return finding.has_value() ? std::vector<Finding>{*finding} : std::vector<Finding>{};
}

class FunctionLengthRule : public Rule
{
public:
    FunctionLengthRule(long long warningLimit, long long highLimit)
        : warningLimit_(warningLimit), highLimit_(highLimit)
    {
    }

    [[nodiscard]] std::string id() const override { return "size.function-length"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        return singleOrEmpty(evaluateThreshold(
            id(),
            "Function is too long.",
            "Consider splitting this function into smaller ones.",
            static_cast<long long>(context.metrics.size.physicalLines),
            warningLimit_,
            highLimit_,
            context.callable.symbol.range.begin,
            context.callable.symbol.qualifiedName));
    }

private:
    long long warningLimit_;
    long long highLimit_;
};

class LocalVariableCountRule : public Rule
{
public:
    LocalVariableCountRule(long long warningLimit, long long highLimit)
        : warningLimit_(warningLimit), highLimit_(highLimit)
    {
    }

    [[nodiscard]] std::string id() const override { return "size.local-variables"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        return singleOrEmpty(evaluateThreshold(
            id(),
            "Function declares too many local variables.",
            "Consider extracting part of this function's logic into helper functions.",
            static_cast<long long>(context.callable.localVariables.size()),
            warningLimit_,
            highLimit_,
            context.callable.symbol.range.begin,
            context.callable.symbol.qualifiedName));
    }

private:
    long long warningLimit_;
    long long highLimit_;
};

class ParameterCountRule : public Rule
{
public:
    ParameterCountRule(long long warningLimit, long long highLimit)
        : warningLimit_(warningLimit), highLimit_(highLimit)
    {
    }

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
            warningLimit_,
            highLimit_,
            location,
            context.callable.symbol.qualifiedName));
    }

private:
    long long warningLimit_;
    long long highLimit_;
};

class ConsecutiveBooleanParametersRule : public Rule
{
public:
    ConsecutiveBooleanParametersRule(long long warningLimit, long long highLimit)
        : warningLimit_(warningLimit), highLimit_(highLimit)
    {
    }

    [[nodiscard]] std::string id() const override { return "signature.consecutive-booleans"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        std::size_t longestRun = 0;
        std::size_t currentRun = 0;

        for (const codeqa::model::ParameterModel& parameter : context.callable.parameters)
        {
            if (codeqa::model::isBooleanValueParameter(parameter))
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
            warningLimit_,
            highLimit_,
            location,
            context.callable.symbol.qualifiedName));
    }

private:
    long long warningLimit_;
    long long highLimit_;
};

class CyclomaticComplexityRule : public Rule
{
public:
    CyclomaticComplexityRule(long long warningLimit, long long highLimit)
        : warningLimit_(warningLimit), highLimit_(highLimit)
    {
    }

    [[nodiscard]] std::string id() const override { return "complexity.cyclomatic"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        return singleOrEmpty(evaluateThreshold(
            id(),
            "Cyclomatic complexity is too high.",
            "Consider splitting this function to reduce the number of independent paths through it.",
            static_cast<long long>(context.metrics.complexity.cyclomaticComplexity),
            warningLimit_,
            highLimit_,
            context.callable.symbol.range.begin,
            context.callable.symbol.qualifiedName));
    }

private:
    long long warningLimit_;
    long long highLimit_;
};

class ReturnCountRule : public Rule
{
public:
    ReturnCountRule(long long warningLimit, long long highLimit)
        : warningLimit_(warningLimit), highLimit_(highLimit)
    {
    }

    [[nodiscard]] std::string id() const override { return "complexity.returns"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const RuleContext& context) const override
    {
        return singleOrEmpty(evaluateThreshold(
            id(),
            "Function has too many return points.",
            "Consider consolidating exit points to make control flow easier to follow.",
            static_cast<long long>(context.metrics.complexity.returnCount),
            warningLimit_,
            highLimit_,
            context.callable.symbol.range.begin,
            context.callable.symbol.qualifiedName));
    }

private:
    long long warningLimit_;
    long long highLimit_;
};

class ConditionComplexityRule : public Rule
{
public:
    ConditionComplexityRule(long long warningLimit, long long highLimit)
        : warningLimit_(warningLimit), highLimit_(highLimit)
    {
    }

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
            warningLimit_,
            highLimit_,
            context.metrics.complexity.maxConditionRange->begin,
            context.callable.symbol.qualifiedName));
    }

private:
    long long warningLimit_;
    long long highLimit_;
};

class NestingDepthRule : public Rule
{
public:
    NestingDepthRule(long long warningLimit, long long highLimit)
        : warningLimit_(warningLimit), highLimit_(highLimit)
    {
    }

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
            warningLimit_,
            highLimit_,
            location,
            context.callable.symbol.qualifiedName));
    }

private:
    long long warningLimit_;
    long long highLimit_;
};

} // namespace

std::vector<std::unique_ptr<Rule>> allRules(const codeqa::config::ProjectConfig& config)
{
    std::vector<std::unique_ptr<Rule>> rules;

    if (const codeqa::config::RuleConfig& c = config.rules.at("size.function-length"); c.enabled)
    {
        rules.push_back(std::make_unique<FunctionLengthRule>(c.warningLimit, c.highLimit));
    }

    if (const codeqa::config::RuleConfig& c = config.rules.at("size.local-variables"); c.enabled)
    {
        rules.push_back(std::make_unique<LocalVariableCountRule>(c.warningLimit, c.highLimit));
    }

    if (const codeqa::config::RuleConfig& c = config.rules.at("signature.parameter-count"); c.enabled)
    {
        rules.push_back(std::make_unique<ParameterCountRule>(c.warningLimit, c.highLimit));
    }

    if (const codeqa::config::RuleConfig& c = config.rules.at("signature.consecutive-booleans"); c.enabled)
    {
        rules.push_back(std::make_unique<ConsecutiveBooleanParametersRule>(c.warningLimit, c.highLimit));
    }

    if (const codeqa::config::RuleConfig& c = config.rules.at("complexity.cyclomatic"); c.enabled)
    {
        rules.push_back(std::make_unique<CyclomaticComplexityRule>(c.warningLimit, c.highLimit));
    }

    if (const codeqa::config::RuleConfig& c = config.rules.at("complexity.returns"); c.enabled)
    {
        rules.push_back(std::make_unique<ReturnCountRule>(c.warningLimit, c.highLimit));
    }

    if (const codeqa::config::RuleConfig& c = config.rules.at("complexity.condition"); c.enabled)
    {
        rules.push_back(std::make_unique<ConditionComplexityRule>(c.warningLimit, c.highLimit));
    }

    if (const codeqa::config::RuleConfig& c = config.rules.at("complexity.nesting"); c.enabled)
    {
        rules.push_back(std::make_unique<NestingDepthRule>(c.warningLimit, c.highLimit));
    }

    return rules;
}

} // namespace codeqa::rules
