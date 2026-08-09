#include "doctest/doctest.h"

#include "codeqa/rules/Rules.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

const codeqa::rules::Rule& ruleNamed(
    const std::vector<std::unique_ptr<codeqa::rules::Rule>>& rules, const std::string& id)
{
    for (const auto& rule : rules)
    {
        if (rule->id() == id)
        {
            return *rule;
        }
    }

    throw std::runtime_error("Expected rule was not registered: " + id);
}

codeqa::model::CallableModel makeCallable()
{
    codeqa::model::CallableModel callable;
    callable.symbol.name = "sample";
    callable.symbol.qualifiedName = "Sample::sample";
    callable.symbol.range.begin.line = 10;
    callable.symbol.range.begin.column = 1;
    return callable;
}

} // namespace

TEST_CASE("size.function-length fires only at or above its warning threshold")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules();
    const codeqa::rules::Rule& rule = ruleNamed(rules, "size.function-length");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;

    metrics.size.physicalLines = 49;
    CHECK(rule.evaluate(codeqa::rules::RuleContext{metrics, callable}).empty());

    metrics.size.physicalLines = 50;
    const std::vector<codeqa::rules::Finding> atWarning =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(atWarning.size() == 1);
    CHECK(atWarning[0].ruleId == "size.function-length");
    CHECK(atWarning[0].severity == codeqa::core::DiagnosticSeverity::Warning);
    REQUIRE(atWarning[0].observedValue.has_value());
    CHECK(*atWarning[0].observedValue == 50);
    REQUIRE(atWarning[0].limitValue.has_value());
    CHECK(*atWarning[0].limitValue == 50);
    CHECK(atWarning[0].symbolName == "Sample::sample");
    CHECK(atWarning[0].location.line == 10);

    metrics.size.physicalLines = 90;
    const std::vector<codeqa::rules::Finding> atHigh =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(atHigh.size() == 1);
    CHECK(atHigh[0].severity == codeqa::core::DiagnosticSeverity::Error);
    REQUIRE(atHigh[0].limitValue.has_value());
    CHECK(*atHigh[0].limitValue == 90);
}

TEST_CASE("size.local-variables reads the callable's local variable count directly")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules();
    const codeqa::rules::Rule& rule = ruleNamed(rules, "size.local-variables");
    codeqa::model::CallableModel callable = makeCallable();
    callable.localVariables.resize(8);
    codeqa::metrics::FunctionMetricsModel metrics;

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(findings.size() == 1);
    REQUIRE(findings[0].observedValue.has_value());
    CHECK(*findings[0].observedValue == 8);
    CHECK(findings[0].severity == codeqa::core::DiagnosticSeverity::Warning);

    callable.localVariables.resize(7);
    CHECK(rule.evaluate(codeqa::rules::RuleContext{metrics, callable}).empty());
}

TEST_CASE("signature.parameter-count points at the parameter list, not the function start")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules();
    const codeqa::rules::Rule& rule = ruleNamed(rules, "signature.parameter-count");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::core::SourceRange parameterListRange;
    parameterListRange.begin.line = 10;
    parameterListRange.begin.column = 13;
    callable.parameterListRange = parameterListRange;
    codeqa::metrics::FunctionMetricsModel metrics;
    metrics.signature.parameterCount = 4;

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(findings.size() == 1);
    REQUIRE(findings[0].observedValue.has_value());
    CHECK(*findings[0].observedValue == 4);
    CHECK(findings[0].location.line == 10);
    CHECK(findings[0].location.column == 13);
}

TEST_CASE("signature.parameter-count falls back to the function start when there is no parameter list")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules();
    const codeqa::rules::Rule& rule = ruleNamed(rules, "signature.parameter-count");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;
    metrics.signature.parameterCount = 4;

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(findings.size() == 1);
    CHECK(findings[0].location.line == 10);
    CHECK(findings[0].location.column == 1);
}

TEST_CASE("signature.consecutive-booleans finds the longest run of by-value bool parameters")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules();
    const codeqa::rules::Rule& rule = ruleNamed(rules, "signature.consecutive-booleans");
    codeqa::model::CallableModel callable = makeCallable();
    callable.parameters = {
        codeqa::model::ParameterModel{"a", codeqa::model::ParameterPassing::Value, false, "bool"},
        codeqa::model::ParameterModel{"b", codeqa::model::ParameterPassing::Value, false, "int"},
        codeqa::model::ParameterModel{"c", codeqa::model::ParameterPassing::Value, false, "bool"},
        codeqa::model::ParameterModel{"d", codeqa::model::ParameterPassing::Value, false, "bool"},
        codeqa::model::ParameterModel{"e", codeqa::model::ParameterPassing::Pointer, false, "bool"}};
    codeqa::metrics::FunctionMetricsModel metrics;

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(findings.size() == 1);
    REQUIRE(findings[0].observedValue.has_value());
    CHECK(*findings[0].observedValue == 2);
    CHECK(findings[0].severity == codeqa::core::DiagnosticSeverity::Warning);
}

TEST_CASE("signature.consecutive-booleans produces no finding when no run reaches the threshold")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules();
    const codeqa::rules::Rule& rule = ruleNamed(rules, "signature.consecutive-booleans");
    codeqa::model::CallableModel callable = makeCallable();
    callable.parameters = {
        codeqa::model::ParameterModel{"a", codeqa::model::ParameterPassing::Value, false, "bool"},
        codeqa::model::ParameterModel{"b", codeqa::model::ParameterPassing::Value, false, "int"}};
    codeqa::metrics::FunctionMetricsModel metrics;

    CHECK(rule.evaluate(codeqa::rules::RuleContext{metrics, callable}).empty());
}

TEST_CASE("complexity.cyclomatic reads cyclomaticComplexity from the metrics")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules();
    const codeqa::rules::Rule& rule = ruleNamed(rules, "complexity.cyclomatic");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;

    metrics.complexity.cyclomaticComplexity = 9;
    CHECK(rule.evaluate(codeqa::rules::RuleContext{metrics, callable}).empty());

    metrics.complexity.cyclomaticComplexity = 15;
    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(findings.size() == 1);
    CHECK(findings[0].severity == codeqa::core::DiagnosticSeverity::Error);
}

TEST_CASE("complexity.returns reads returnCount from the metrics")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules();
    const codeqa::rules::Rule& rule = ruleNamed(rules, "complexity.returns");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;

    metrics.complexity.returnCount = 3;
    CHECK(rule.evaluate(codeqa::rules::RuleContext{metrics, callable}).empty());

    metrics.complexity.returnCount = 4;
    CHECK(rule.evaluate(codeqa::rules::RuleContext{metrics, callable}).size() == 1);
}

TEST_CASE("all six threshold rules apply to both C and C++")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules();
    REQUIRE(rules.size() == 6);

    for (const auto& rule : rules)
    {
        CHECK(rule->appliesTo(codeqa::core::Language::C));
        CHECK(rule->appliesTo(codeqa::core::Language::Cpp));
    }
}
