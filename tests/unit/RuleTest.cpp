#include "doctest/doctest.h"

#include "codeqa/rules/Rule.h"

#include <string>
#include <vector>

TEST_CASE("rule models default to empty, informational state")
{
    codeqa::rules::Finding finding;
    CHECK(finding.ruleId.empty());
    CHECK(finding.severity == codeqa::core::DiagnosticSeverity::Information);
    CHECK(finding.description.empty());
    CHECK(finding.recommendation.empty());
    CHECK_FALSE(finding.observedValue.has_value());
    CHECK_FALSE(finding.limitValue.has_value());
    CHECK(finding.symbolName.empty());

    codeqa::rules::FileFindingsModel file;
    CHECK(file.path.empty());
    CHECK(file.findings.empty());

    codeqa::rules::ProjectFindingsModel project;
    CHECK(project.rootPath.empty());
    CHECK(project.files.empty());
}

namespace
{

class AlwaysFiresRule : public codeqa::rules::Rule
{
public:
    [[nodiscard]] std::string id() const override { return "test.always-fires"; }

    [[nodiscard]] bool appliesTo(codeqa::core::Language language) const override
    {
        return language == codeqa::core::Language::Cpp;
    }

    [[nodiscard]] std::vector<codeqa::rules::Finding> evaluate(
        const codeqa::rules::RuleContext& context) const override
    {
        codeqa::rules::Finding finding;
        finding.ruleId = id();
        finding.symbolName = context.callable.symbol.name;
        return {finding};
    }
};

} // namespace

TEST_CASE("a concrete Rule can be evaluated through the base interface")
{
    codeqa::model::CallableModel callable;
    callable.symbol.name = "sample";
    codeqa::metrics::FunctionMetricsModel metrics;
    const codeqa::rules::RuleContext context{metrics, callable};

    const AlwaysFiresRule rule;
    CHECK(rule.id() == "test.always-fires");
    CHECK(rule.appliesTo(codeqa::core::Language::Cpp));
    CHECK_FALSE(rule.appliesTo(codeqa::core::Language::C));

    const std::vector<codeqa::rules::Finding> findings = rule.evaluate(context);
    REQUIRE(findings.size() == 1);
    CHECK(findings[0].ruleId == "test.always-fires");
    CHECK(findings[0].symbolName == "sample");
}
