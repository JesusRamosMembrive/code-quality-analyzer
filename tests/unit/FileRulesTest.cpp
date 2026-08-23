#include "doctest/doctest.h"

#include "codeqa/config/DefaultConfig.h"
#include "codeqa/rules/Rules.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

const codeqa::rules::FileRule& fileRuleNamed(
    const std::vector<std::unique_ptr<codeqa::rules::FileRule>>& rules, const std::string& id)
{
    for (const auto& rule : rules)
    {
        if (rule->id() == id)
        {
            return *rule;
        }
    }

    throw std::runtime_error("Expected file rule was not registered: " + id);
}

codeqa::model::MacroModel makeMacro(
    std::string name, std::vector<std::string> parameters, std::string body, bool isFunctionLike = true)
{
    codeqa::model::MacroModel macro;
    macro.symbol.name = name;
    macro.symbol.qualifiedName = std::move(name);
    macro.symbol.range.begin.line = 5;
    macro.symbol.range.begin.column = 1;
    macro.isFunctionLike = isFunctionLike;
    macro.parameters = std::move(parameters);
    macro.body = std::move(body);
    return macro;
}

} // namespace

TEST_CASE("preprocessor.unparenthesized-macro-parameter flags a parameter used with an operator")
{
    const std::vector<std::unique_ptr<codeqa::rules::FileRule>> rules =
        codeqa::rules::allFileRules(codeqa::config::defaultConfig());
    const codeqa::rules::FileRule& rule =
        fileRuleNamed(rules, "preprocessor.unparenthesized-macro-parameter");

    codeqa::model::FileModel file;
    file.macros.push_back(makeMacro("SQUARE", {"x"}, "x*x"));

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::FileRuleContext{file});

    REQUIRE(findings.size() == 1);
    CHECK(findings[0].ruleId == "preprocessor.unparenthesized-macro-parameter");
    CHECK(findings[0].severity == codeqa::core::DiagnosticSeverity::Warning);
    CHECK(
        findings[0].description
        == "Parameter 'x' of macro 'SQUARE' is used without enclosing parentheses in the macro body.");
    CHECK(
        findings[0].recommendation
        == "Wrap the parameter as (x) everywhere it appears in the macro body to avoid "
           "operator-precedence bugs at the call site.");
    CHECK(findings[0].location.line == 5);
    CHECK(findings[0].symbolName == "SQUARE");
}

TEST_CASE("preprocessor.unparenthesized-macro-parameter ignores a fully parenthesized parameter")
{
    const std::vector<std::unique_ptr<codeqa::rules::FileRule>> rules =
        codeqa::rules::allFileRules(codeqa::config::defaultConfig());
    const codeqa::rules::FileRule& rule =
        fileRuleNamed(rules, "preprocessor.unparenthesized-macro-parameter");

    codeqa::model::FileModel file;
    file.macros.push_back(makeMacro("SQUARE", {"x"}, "((x)*(x))"));

    CHECK(rule.evaluate(codeqa::rules::FileRuleContext{file}).empty());
}

TEST_CASE("preprocessor.unparenthesized-macro-parameter treats a sole call argument as safe")
{
    const std::vector<std::unique_ptr<codeqa::rules::FileRule>> rules =
        codeqa::rules::allFileRules(codeqa::config::defaultConfig());
    const codeqa::rules::FileRule& rule =
        fileRuleNamed(rules, "preprocessor.unparenthesized-macro-parameter");

    codeqa::model::FileModel file;
    file.macros.push_back(makeMacro("LOG", {"x"}, "printf(x)"));

    CHECK(rule.evaluate(codeqa::rules::FileRuleContext{file}).empty());
}

TEST_CASE("preprocessor.unparenthesized-macro-parameter treats a sole subscript index as safe")
{
    const std::vector<std::unique_ptr<codeqa::rules::FileRule>> rules =
        codeqa::rules::allFileRules(codeqa::config::defaultConfig());
    const codeqa::rules::FileRule& rule =
        fileRuleNamed(rules, "preprocessor.unparenthesized-macro-parameter");

    codeqa::model::FileModel file;
    file.macros.push_back(makeMacro("AT", {"i"}, "buffer[i]"));

    CHECK(rule.evaluate(codeqa::rules::FileRuleContext{file}).empty());
}

TEST_CASE("preprocessor.unparenthesized-macro-parameter flags the array operand of a subscript, "
          "but not the index")
{
    const std::vector<std::unique_ptr<codeqa::rules::FileRule>> rules =
        codeqa::rules::allFileRules(codeqa::config::defaultConfig());
    const codeqa::rules::FileRule& rule =
        fileRuleNamed(rules, "preprocessor.unparenthesized-macro-parameter");

    codeqa::model::FileModel file;
    file.macros.push_back(makeMacro("AT", {"arr", "i"}, "arr[i]"));

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::FileRuleContext{file});

    REQUIRE(findings.size() == 1);
    CHECK(findings[0].description.find("'arr'") != std::string::npos);
}

TEST_CASE("preprocessor.unparenthesized-macro-parameter produces one finding per unsafe parameter")
{
    const std::vector<std::unique_ptr<codeqa::rules::FileRule>> rules =
        codeqa::rules::allFileRules(codeqa::config::defaultConfig());
    const codeqa::rules::FileRule& rule =
        fileRuleNamed(rules, "preprocessor.unparenthesized-macro-parameter");

    codeqa::model::FileModel file;
    file.macros.push_back(makeMacro("ADD", {"a", "b"}, "a+b"));

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::FileRuleContext{file});

    REQUIRE(findings.size() == 2);
    CHECK(findings[0].description.find("'a'") != std::string::npos);
    CHECK(findings[1].description.find("'b'") != std::string::npos);
}

TEST_CASE("preprocessor.unparenthesized-macro-parameter never fires on an object-like macro")
{
    const std::vector<std::unique_ptr<codeqa::rules::FileRule>> rules =
        codeqa::rules::allFileRules(codeqa::config::defaultConfig());
    const codeqa::rules::FileRule& rule =
        fileRuleNamed(rules, "preprocessor.unparenthesized-macro-parameter");

    codeqa::model::FileModel file;
    file.macros.push_back(makeMacro("MAX_SIZE", {}, "128", false));

    CHECK(rule.evaluate(codeqa::rules::FileRuleContext{file}).empty());
}

TEST_CASE("preprocessor.unparenthesized-macro-parameter ignores a parameter never used in the body")
{
    const std::vector<std::unique_ptr<codeqa::rules::FileRule>> rules =
        codeqa::rules::allFileRules(codeqa::config::defaultConfig());
    const codeqa::rules::FileRule& rule =
        fileRuleNamed(rules, "preprocessor.unparenthesized-macro-parameter");

    codeqa::model::FileModel file;
    file.macros.push_back(makeMacro("UNUSED", {"x"}, "0"));

    CHECK(rule.evaluate(codeqa::rules::FileRuleContext{file}).empty());
}

TEST_CASE("preprocessor.unparenthesized-macro-parameter checks every macro in the file independently")
{
    const std::vector<std::unique_ptr<codeqa::rules::FileRule>> rules =
        codeqa::rules::allFileRules(codeqa::config::defaultConfig());
    const codeqa::rules::FileRule& rule =
        fileRuleNamed(rules, "preprocessor.unparenthesized-macro-parameter");

    codeqa::model::FileModel file;
    file.macros.push_back(makeMacro("SAFE", {"x"}, "((x))"));
    file.macros.push_back(makeMacro("UNSAFE", {"y"}, "y*y"));

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::FileRuleContext{file});

    REQUIRE(findings.size() == 1);
    CHECK(findings[0].description.find("'y'") != std::string::npos);
}
