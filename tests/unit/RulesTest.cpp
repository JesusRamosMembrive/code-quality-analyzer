#include "doctest/doctest.h"

#include "codeqa/config/DefaultConfig.h"
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

codeqa::model::CallModel makeCall(std::string calleeText, codeqa::model::CallKind kind)
{
    codeqa::model::CallModel call;
    call.calleeText = std::move(calleeText);
    call.kind = kind;
    return call;
}

} // namespace

TEST_CASE("size.function-length fires only at or above its warning threshold")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(codeqa::config::defaultConfig());
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
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(codeqa::config::defaultConfig());
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
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(codeqa::config::defaultConfig());
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
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(codeqa::config::defaultConfig());
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
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(codeqa::config::defaultConfig());
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
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(codeqa::config::defaultConfig());
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
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(codeqa::config::defaultConfig());
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
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(codeqa::config::defaultConfig());
    const codeqa::rules::Rule& rule = ruleNamed(rules, "complexity.returns");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;

    metrics.complexity.returnCount = 3;
    CHECK(rule.evaluate(codeqa::rules::RuleContext{metrics, callable}).empty());

    metrics.complexity.returnCount = 4;
    CHECK(rule.evaluate(codeqa::rules::RuleContext{metrics, callable}).size() == 1);
}

TEST_CASE("complexity.condition points at the exact condition that produced the maximum")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(codeqa::config::defaultConfig());
    const codeqa::rules::Rule& rule = ruleNamed(rules, "complexity.condition");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;
    metrics.complexity.maxConditionComplexity = 3;
    codeqa::core::SourceRange conditionRange;
    conditionRange.begin.line = 42;
    conditionRange.begin.column = 9;
    metrics.complexity.maxConditionRange = conditionRange;

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(findings.size() == 1);
    REQUIRE(findings[0].observedValue.has_value());
    CHECK(*findings[0].observedValue == 3);
    CHECK(findings[0].location.line == 42);
    CHECK(findings[0].location.column == 9);
}

TEST_CASE("complexity.condition produces no finding when there is no condition to point at")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(codeqa::config::defaultConfig());
    const codeqa::rules::Rule& rule = ruleNamed(rules, "complexity.condition");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;
    metrics.complexity.maxConditionComplexity = 10;

    CHECK(rule.evaluate(codeqa::rules::RuleContext{metrics, callable}).empty());
}

TEST_CASE("complexity.nesting points at the control-flow entry that reached the maximum depth")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(codeqa::config::defaultConfig());
    const codeqa::rules::Rule& rule = ruleNamed(rules, "complexity.nesting");
    codeqa::model::CallableModel callable = makeCallable();

    codeqa::model::ControlFlowModel outer;
    outer.kind = codeqa::model::ControlFlowKind::If;
    outer.nestingDepth = 0;
    outer.range.begin.line = 20;
    outer.range.begin.column = 5;

    codeqa::model::ControlFlowModel inner;
    inner.kind = codeqa::model::ControlFlowKind::If;
    inner.nestingDepth = 3;
    inner.range.begin.line = 24;
    inner.range.begin.column = 9;

    callable.controlFlow = {outer, inner};

    codeqa::metrics::FunctionMetricsModel metrics;
    metrics.complexity.maxNestingDepth = 3;

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(findings.size() == 1);
    REQUIRE(findings[0].observedValue.has_value());
    CHECK(*findings[0].observedValue == 3);
    CHECK(findings[0].location.line == 24);
    CHECK(findings[0].location.column == 9);
}

TEST_CASE("complexity.nesting resolves a tie in favor of the first matching entry")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(codeqa::config::defaultConfig());
    const codeqa::rules::Rule& rule = ruleNamed(rules, "complexity.nesting");
    codeqa::model::CallableModel callable = makeCallable();

    codeqa::model::ControlFlowModel first;
    first.kind = codeqa::model::ControlFlowKind::If;
    first.nestingDepth = 3;
    first.range.begin.line = 20;
    first.range.begin.column = 5;

    codeqa::model::ControlFlowModel second;
    second.kind = codeqa::model::ControlFlowKind::If;
    second.nestingDepth = 3;
    second.range.begin.line = 30;
    second.range.begin.column = 9;

    callable.controlFlow = {first, second};

    codeqa::metrics::FunctionMetricsModel metrics;
    metrics.complexity.maxNestingDepth = 3;

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(findings.size() == 1);
    CHECK(findings[0].location.line == 20);
    CHECK(findings[0].location.column == 5);
}

TEST_CASE("all ten rules are registered and apply to both C and C++")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(codeqa::config::defaultConfig());
    REQUIRE(rules.size() == 10);

    for (const auto& rule : rules)
    {
        CHECK(rule->appliesTo(codeqa::core::Language::C));
        CHECK(rule->appliesTo(codeqa::core::Language::Cpp));
    }
}

TEST_CASE("allRules omits a rule disabled via config")
{
    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.rules.at("complexity.cyclomatic").enabled = false;

    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(config);

    REQUIRE(rules.size() == 9);
    for (const auto& rule : rules)
    {
        CHECK(rule->id() != "complexity.cyclomatic");
    }
}

TEST_CASE("allRules applies a threshold overridden via config")
{
    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.rules.at("complexity.cyclomatic").warningLimit = 2;
    config.rules.at("complexity.cyclomatic").highLimit = 4;

    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(config);
    const codeqa::rules::Rule& rule = ruleNamed(rules, "complexity.cyclomatic");

    codeqa::model::CallableModel callable;
    callable.symbol.name = "sample";
    callable.symbol.qualifiedName = "Sample::sample";
    codeqa::metrics::FunctionMetricsModel metrics;
    metrics.complexity.cyclomaticComplexity = 3;

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(findings.size() == 1);
    CHECK(findings[0].severity == codeqa::core::DiagnosticSeverity::Warning);
    REQUIRE(findings[0].limitValue.has_value());
    CHECK(*findings[0].limitValue == 2);
}

TEST_CASE("allRules applies a threshold overridden via config for size.function-length")
{
    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.rules.at("size.function-length").warningLimit = 2;
    config.rules.at("size.function-length").highLimit = 4;

    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(config);
    const codeqa::rules::Rule& rule = ruleNamed(rules, "size.function-length");

    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;
    metrics.size.physicalLines = 3;

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(findings.size() == 1);
    CHECK(findings[0].severity == codeqa::core::DiagnosticSeverity::Warning);
    REQUIRE(findings[0].limitValue.has_value());
    CHECK(*findings[0].limitValue == 2);
}

TEST_CASE("allRules applies a threshold overridden via config for size.local-variables")
{
    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.rules.at("size.local-variables").warningLimit = 2;
    config.rules.at("size.local-variables").highLimit = 4;

    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(config);
    const codeqa::rules::Rule& rule = ruleNamed(rules, "size.local-variables");

    codeqa::model::CallableModel callable = makeCallable();
    callable.localVariables.resize(3);
    codeqa::metrics::FunctionMetricsModel metrics;

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(findings.size() == 1);
    CHECK(findings[0].severity == codeqa::core::DiagnosticSeverity::Warning);
    REQUIRE(findings[0].limitValue.has_value());
    CHECK(*findings[0].limitValue == 2);
}

TEST_CASE("allRules applies a threshold overridden via config for signature.parameter-count")
{
    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.rules.at("signature.parameter-count").warningLimit = 2;
    config.rules.at("signature.parameter-count").highLimit = 4;

    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(config);
    const codeqa::rules::Rule& rule = ruleNamed(rules, "signature.parameter-count");

    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;
    metrics.signature.parameterCount = 3;

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(findings.size() == 1);
    CHECK(findings[0].severity == codeqa::core::DiagnosticSeverity::Warning);
    REQUIRE(findings[0].limitValue.has_value());
    CHECK(*findings[0].limitValue == 2);
}

TEST_CASE("allRules applies a threshold overridden via config for signature.consecutive-booleans")
{
    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.rules.at("signature.consecutive-booleans").warningLimit = 1;
    config.rules.at("signature.consecutive-booleans").highLimit = 2;

    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(config);
    const codeqa::rules::Rule& rule = ruleNamed(rules, "signature.consecutive-booleans");

    codeqa::model::CallableModel callable = makeCallable();
    callable.parameters = {
        codeqa::model::ParameterModel{"a", codeqa::model::ParameterPassing::Value, false, "bool"}};
    codeqa::metrics::FunctionMetricsModel metrics;

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(findings.size() == 1);
    CHECK(findings[0].severity == codeqa::core::DiagnosticSeverity::Warning);
    REQUIRE(findings[0].limitValue.has_value());
    CHECK(*findings[0].limitValue == 1);
}

TEST_CASE("allRules applies a threshold overridden via config for complexity.returns")
{
    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.rules.at("complexity.returns").warningLimit = 2;
    config.rules.at("complexity.returns").highLimit = 4;

    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(config);
    const codeqa::rules::Rule& rule = ruleNamed(rules, "complexity.returns");

    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;
    metrics.complexity.returnCount = 3;

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(findings.size() == 1);
    CHECK(findings[0].severity == codeqa::core::DiagnosticSeverity::Warning);
    REQUIRE(findings[0].limitValue.has_value());
    CHECK(*findings[0].limitValue == 2);
}

TEST_CASE("allRules applies a threshold overridden via config for complexity.condition")
{
    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.rules.at("complexity.condition").warningLimit = 1;
    config.rules.at("complexity.condition").highLimit = 3;

    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(config);
    const codeqa::rules::Rule& rule = ruleNamed(rules, "complexity.condition");

    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;
    metrics.complexity.maxConditionComplexity = 2;
    codeqa::core::SourceRange conditionRange;
    conditionRange.begin.line = 42;
    conditionRange.begin.column = 9;
    metrics.complexity.maxConditionRange = conditionRange;

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(findings.size() == 1);
    CHECK(findings[0].severity == codeqa::core::DiagnosticSeverity::Warning);
    REQUIRE(findings[0].limitValue.has_value());
    CHECK(*findings[0].limitValue == 1);
}

TEST_CASE("allRules applies a threshold overridden via config for complexity.nesting")
{
    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();
    config.rules.at("complexity.nesting").warningLimit = 1;
    config.rules.at("complexity.nesting").highLimit = 3;

    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(config);
    const codeqa::rules::Rule& rule = ruleNamed(rules, "complexity.nesting");

    codeqa::model::CallableModel callable = makeCallable();
    codeqa::model::ControlFlowModel flow;
    flow.kind = codeqa::model::ControlFlowKind::If;
    flow.nestingDepth = 2;
    flow.range.begin.line = 24;
    flow.range.begin.column = 9;
    callable.controlFlow = {flow};

    codeqa::metrics::FunctionMetricsModel metrics;
    metrics.complexity.maxNestingDepth = 2;

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});
    REQUIRE(findings.size() == 1);
    CHECK(findings[0].severity == codeqa::core::DiagnosticSeverity::Warning);
    REQUIRE(findings[0].limitValue.has_value());
    CHECK(*findings[0].limitValue == 1);
}

TEST_CASE("security.unsafe-string-api flags named calls to banned functions, including std:: qualified ones")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules =
        codeqa::rules::allRules(codeqa::config::defaultConfig());
    const codeqa::rules::Rule& rule = ruleNamed(rules, "security.unsafe-string-api");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;

    callable.calls.push_back(makeCall("strcpy", codeqa::model::CallKind::Named));
    callable.calls.push_back(makeCall("std::strcpy", codeqa::model::CallKind::Named));
    callable.calls.push_back(makeCall("strcpy", codeqa::model::CallKind::Member));
    callable.calls.push_back(makeCall("strncpy", codeqa::model::CallKind::Named));

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});

    REQUIRE(findings.size() == 2);
    CHECK(findings[0].ruleId == "security.unsafe-string-api");
    CHECK(findings[0].severity == codeqa::core::DiagnosticSeverity::Warning);
    CHECK(findings[0].description == "Call to unsafe function 'strcpy'.");
    CHECK(findings[0].recommendation == "Use strncpy or strlcpy (bounds-checked) instead.");
    CHECK_FALSE(findings[0].observedValue.has_value());
    CHECK_FALSE(findings[0].limitValue.has_value());
    CHECK(findings[0].symbolName == "Sample::sample");
    CHECK(findings[1].description == "Call to unsafe function 'strcpy'.");
}

TEST_CASE("security.unsafe-string-api produces no findings for calls outside its blocklist")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules =
        codeqa::rules::allRules(codeqa::config::defaultConfig());
    const codeqa::rules::Rule& rule = ruleNamed(rules, "security.unsafe-string-api");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;

    callable.calls.push_back(makeCall("strncpy", codeqa::model::CallKind::Named));
    callable.calls.push_back(makeCall("my_strcpy", codeqa::model::CallKind::Named));

    CHECK(rule.evaluate(codeqa::rules::RuleContext{metrics, callable}).empty());
}

TEST_CASE("security.unsafe-string-api recommends a specific safe alternative for each blocklisted function")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules =
        codeqa::rules::allRules(codeqa::config::defaultConfig());
    const codeqa::rules::Rule& rule = ruleNamed(rules, "security.unsafe-string-api");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;

    callable.calls.push_back(makeCall("gets", codeqa::model::CallKind::Named));
    callable.calls.push_back(makeCall("strcat", codeqa::model::CallKind::Named));
    callable.calls.push_back(makeCall("sprintf", codeqa::model::CallKind::Named));
    callable.calls.push_back(makeCall("vsprintf", codeqa::model::CallKind::Named));

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});

    REQUIRE(findings.size() == 4);
    CHECK(findings[0].description == "Call to unsafe function 'gets'.");
    CHECK(findings[0].recommendation == "gets was removed from C11; use fgets(buf, size, stream) instead.");
    CHECK(findings[1].description == "Call to unsafe function 'strcat'.");
    CHECK(findings[1].recommendation == "Use strncat or strlcat (bounds-checked) instead.");
    CHECK(findings[2].description == "Call to unsafe function 'sprintf'.");
    CHECK(findings[2].recommendation == "Use snprintf instead, which takes a buffer size.");
    CHECK(findings[3].description == "Call to unsafe function 'vsprintf'.");
    CHECK(findings[3].recommendation == "Use vsnprintf instead, which takes a buffer size.");
}

TEST_CASE("memory.uninitialized-pointer flags a raw pointer local with no initializer")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules =
        codeqa::rules::allRules(codeqa::config::defaultConfig());
    const codeqa::rules::Rule& rule = ruleNamed(rules, "memory.uninitialized-pointer");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;

    codeqa::model::LocalVariableModel local;
    local.symbol.name = "p";
    local.symbol.qualifiedName = "Sample::sample::p";
    local.symbol.range.begin.line = 12;
    local.symbol.range.begin.column = 5;
    local.declaredType = "int*";
    local.hasInitializer = false;
    callable.localVariables.push_back(local);

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});

    REQUIRE(findings.size() == 1);
    CHECK(findings[0].ruleId == "memory.uninitialized-pointer");
    CHECK(findings[0].severity == codeqa::core::DiagnosticSeverity::Warning);
    CHECK(findings[0].description == "Local pointer variable 'p' is declared without being initialized.");
    CHECK(
        findings[0].recommendation
        == "Initialize the pointer at declaration (e.g., to nullptr or a valid address) "
           "to avoid undefined behavior from using it before assignment.");
    CHECK_FALSE(findings[0].observedValue.has_value());
    CHECK_FALSE(findings[0].limitValue.has_value());
    CHECK(findings[0].symbolName == "Sample::sample");
    CHECK(findings[0].location.line == 12);
    CHECK(findings[0].location.column == 5);
}

TEST_CASE("memory.uninitialized-pointer ignores an initialized raw pointer local")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules =
        codeqa::rules::allRules(codeqa::config::defaultConfig());
    const codeqa::rules::Rule& rule = ruleNamed(rules, "memory.uninitialized-pointer");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;

    codeqa::model::LocalVariableModel local;
    local.symbol.name = "p";
    local.declaredType = "int*";
    local.hasInitializer = true;
    callable.localVariables.push_back(local);

    CHECK(rule.evaluate(codeqa::rules::RuleContext{metrics, callable}).empty());
}

TEST_CASE("memory.uninitialized-pointer ignores non-pointer, array, smart-pointer, and function-pointer locals")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules =
        codeqa::rules::allRules(codeqa::config::defaultConfig());
    const codeqa::rules::Rule& rule = ruleNamed(rules, "memory.uninitialized-pointer");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;

    codeqa::model::LocalVariableModel plainInt;
    plainInt.symbol.name = "x";
    plainInt.declaredType = "int";
    plainInt.hasInitializer = false;

    codeqa::model::LocalVariableModel array;
    array.symbol.name = "arr";
    array.declaredType = "int[10]";
    array.hasInitializer = false;

    codeqa::model::LocalVariableModel smartPointer;
    smartPointer.symbol.name = "sp";
    smartPointer.declaredType = "std::unique_ptr<int>";
    smartPointer.hasInitializer = false;

    codeqa::model::LocalVariableModel functionPointer;
    functionPointer.symbol.name = "fp";
    functionPointer.declaredType = "int(*)(int)";
    functionPointer.hasInitializer = false;

    callable.localVariables = {plainInt, array, smartPointer, functionPointer};

    CHECK(rule.evaluate(codeqa::rules::RuleContext{metrics, callable}).empty());
}

TEST_CASE("memory.uninitialized-pointer produces one finding per matching local, in declaration order")
{
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules =
        codeqa::rules::allRules(codeqa::config::defaultConfig());
    const codeqa::rules::Rule& rule = ruleNamed(rules, "memory.uninitialized-pointer");
    codeqa::model::CallableModel callable = makeCallable();
    codeqa::metrics::FunctionMetricsModel metrics;

    codeqa::model::LocalVariableModel first;
    first.symbol.name = "a";
    first.declaredType = "char*";
    first.hasInitializer = false;

    codeqa::model::LocalVariableModel initialized;
    initialized.symbol.name = "b";
    initialized.declaredType = "char*";
    initialized.hasInitializer = true;

    codeqa::model::LocalVariableModel second;
    second.symbol.name = "c";
    second.declaredType = "char**";
    second.hasInitializer = false;

    callable.localVariables = {first, initialized, second};

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::RuleContext{metrics, callable});

    REQUIRE(findings.size() == 2);
    CHECK(findings[0].description == "Local pointer variable 'a' is declared without being initialized.");
    CHECK(findings[1].description == "Local pointer variable 'c' is declared without being initialized.");
}
