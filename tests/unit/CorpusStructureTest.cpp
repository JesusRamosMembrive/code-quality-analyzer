#include "doctest/doctest.h"

#include "codeqa/core/DiagnosticSeverity.h"
#include "codeqa/core/Language.h"
#include "codeqa/frontend/ParseSourceFile.h"
#include "codeqa/frontend/SelectGrammar.h"
#include "codeqa/frontend/StructuralExtractor.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using Json = nlohmann::json;

struct CorpusFixture
{
    std::filesystem::path sourcePath;
    std::filesystem::path expectationPath;
    bool isMalformed{false};
};

struct RangeExpectation
{
    std::size_t startLine{0};
    std::size_t endLine{0};
};

struct ParameterExpectation
{
    std::string name;
    codeqa::frontend::ParameterPassing passing{codeqa::frontend::ParameterPassing::Value};
    bool hasDefaultValue{false};
};

struct FunctionExpectation
{
    std::string name;
    codeqa::frontend::FunctionKind kind{codeqa::frontend::FunctionKind::Free};
    RangeExpectation range;
    std::vector<ParameterExpectation> parameters;
    bool isStatic{false};
    bool isVariadic{false};
};

struct ClassExpectation
{
    std::string name;
    codeqa::frontend::ClassKind kind{codeqa::frontend::ClassKind::Class};
    RangeExpectation range;
    bool isTemplate{false};
    bool isScopedEnum{false};
    std::vector<std::string> bases;
};

struct IncludeExpectation
{
    std::string target;
    codeqa::frontend::IncludeDelimiter delimiter{codeqa::frontend::IncludeDelimiter::Local};
    RangeExpectation range;
};

struct DiagnosticExpectation
{
    std::size_t line{0};
    codeqa::core::DiagnosticSeverity severity{codeqa::core::DiagnosticSeverity::Error};
    bool hasColumn{false};
    std::size_t column{0};
};

struct CorpusExpectation
{
    std::vector<std::string> constructs;
    std::vector<FunctionExpectation> functions;
    std::vector<ClassExpectation> classes;
    std::vector<IncludeExpectation> includes;
    std::vector<DiagnosticExpectation> diagnostics;
};

std::vector<CorpusFixture> collectCorpusFixtures()
{
    const std::filesystem::path corpusRoot{CODEQA_CORPUS_DIR};
    std::vector<CorpusFixture> fixtures;

    for (const std::filesystem::directory_entry& entry :
        std::filesystem::recursive_directory_iterator(corpusRoot))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        const std::string extension = entry.path().extension().string();

        if (extension != ".c" && extension != ".cpp" && extension != ".h" && extension != ".hpp")
        {
            continue;
        }

        const std::filesystem::path expectationPath =
            entry.path().parent_path() / (entry.path().stem().string() + ".parsing.json");

        if (!std::filesystem::is_regular_file(expectationPath))
        {
            throw std::runtime_error(
                "Corpus fixture '" + entry.path().generic_string()
                + "' is missing sibling expectation '" + expectationPath.generic_string() + "'.");
        }

        const std::filesystem::path relativePath = entry.path().lexically_relative(corpusRoot);
        const auto firstSegment = relativePath.begin();

        if (firstSegment == relativePath.end())
        {
            throw std::runtime_error(
                "Corpus fixture '" + entry.path().generic_string()
                + "' is not below the corpus root.");
        }

        fixtures.push_back(CorpusFixture{
            entry.path(), expectationPath, firstSegment->generic_string() == "malformed"});
    }

    std::sort(
        fixtures.begin(), fixtures.end(), [](const CorpusFixture& left, const CorpusFixture& right) {
            return left.sourcePath.generic_string() < right.sourcePath.generic_string();
        });
    return fixtures;
}

codeqa::frontend::Grammar grammarForExtension(const std::filesystem::path& path)
{
    const std::string extension = path.extension().string();

    if (extension == ".c")
    {
        return codeqa::frontend::selectGrammar(codeqa::core::Language::C).grammar;
    }

    if (extension == ".h")
    {
        return codeqa::frontend::selectGrammar(codeqa::core::Language::Unknown).grammar;
    }

    return codeqa::frontend::selectGrammar(codeqa::core::Language::Cpp).grammar;
}

RangeExpectation readRange(const Json& value)
{
    return RangeExpectation{value.at("startLine").get<std::size_t>(), value.at("endLine").get<std::size_t>()};
}

codeqa::frontend::ParameterPassing parameterPassingFrom(const std::string& value)
{
    if (value == "value") return codeqa::frontend::ParameterPassing::Value;
    if (value == "pointer") return codeqa::frontend::ParameterPassing::Pointer;
    if (value == "reference") return codeqa::frontend::ParameterPassing::Reference;
    if (value == "function-pointer") return codeqa::frontend::ParameterPassing::FunctionPointer;
    throw std::runtime_error("Unknown parameter passing '" + value + "'.");
}

codeqa::frontend::FunctionKind functionKindFrom(const std::string& value)
{
    if (value == "free") return codeqa::frontend::FunctionKind::Free;
    if (value == "method") return codeqa::frontend::FunctionKind::Method;
    if (value == "constructor") return codeqa::frontend::FunctionKind::Constructor;
    if (value == "destructor") return codeqa::frontend::FunctionKind::Destructor;
    if (value == "operator") return codeqa::frontend::FunctionKind::Operator;
    if (value == "lambda") return codeqa::frontend::FunctionKind::Lambda;
    if (value == "prototype") return codeqa::frontend::FunctionKind::Prototype;
    throw std::runtime_error("Unknown function kind '" + value + "'.");
}

codeqa::frontend::ClassKind classKindFrom(const std::string& value)
{
    if (value == "class") return codeqa::frontend::ClassKind::Class;
    if (value == "struct") return codeqa::frontend::ClassKind::Struct;
    if (value == "union") return codeqa::frontend::ClassKind::Union;
    if (value == "enum") return codeqa::frontend::ClassKind::Enum;
    if (value == "namespace") return codeqa::frontend::ClassKind::Namespace;
    throw std::runtime_error("Unknown class kind '" + value + "'.");
}

codeqa::frontend::IncludeDelimiter includeDelimiterFrom(const std::string& value)
{
    if (value == "local") return codeqa::frontend::IncludeDelimiter::Local;
    if (value == "system") return codeqa::frontend::IncludeDelimiter::System;
    throw std::runtime_error("Unknown include delimiter '" + value + "'.");
}

codeqa::core::DiagnosticSeverity severityFrom(const std::string& value)
{
    if (value == "information") return codeqa::core::DiagnosticSeverity::Information;
    if (value == "warning") return codeqa::core::DiagnosticSeverity::Warning;
    if (value == "error") return codeqa::core::DiagnosticSeverity::Error;
    throw std::runtime_error("Unknown diagnostic severity '" + value + "'.");
}

CorpusExpectation readExpectation(const std::filesystem::path& path)
{
    std::ifstream input(path);

    if (!input)
    {
        throw std::runtime_error("Could not open corpus expectation '" + path.generic_string() + "'.");
    }

    const Json document = Json::parse(input);
    CorpusExpectation expectation;
    expectation.constructs = document.at("constructs").get<std::vector<std::string>>();

    if (expectation.constructs.empty())
    {
        throw std::runtime_error("Corpus expectation '" + path.generic_string() + "' has no constructs.");
    }

    for (const Json& function : document.at("functions"))
    {
        FunctionExpectation expected;
        expected.name = function.at("name").get<std::string>();
        expected.kind = functionKindFrom(function.at("kind").get<std::string>());
        expected.range = readRange(function);
        expected.isStatic = function.at("isStatic").get<bool>();
        expected.isVariadic = function.at("isVariadic").get<bool>();

        for (const Json& parameter : function.at("parameters"))
        {
            expected.parameters.push_back(ParameterExpectation{
                parameter.at("name").get<std::string>(),
                parameterPassingFrom(parameter.at("passing").get<std::string>()),
                parameter.at("hasDefaultValue").get<bool>()});
        }

        expectation.functions.push_back(std::move(expected));
    }

    for (const Json& type : document.at("classes"))
    {
        expectation.classes.push_back(ClassExpectation{
            type.at("name").get<std::string>(),
            classKindFrom(type.at("kind").get<std::string>()),
            readRange(type),
            type.at("isTemplate").get<bool>(),
            type.at("isScopedEnum").get<bool>(),
            type.at("bases").get<std::vector<std::string>>()});
    }

    for (const Json& include : document.at("includes"))
    {
        expectation.includes.push_back(IncludeExpectation{
            include.at("target").get<std::string>(),
            includeDelimiterFrom(include.at("delimiter").get<std::string>()),
            readRange(include)});
    }

    for (const Json& diagnostic : document.at("diagnostics"))
    {
        const bool hasColumn = diagnostic.contains("column");
        expectation.diagnostics.push_back(DiagnosticExpectation{
            diagnostic.at("line").get<std::size_t>(),
            severityFrom(diagnostic.at("severity").get<std::string>()),
            hasColumn,
            hasColumn ? diagnostic.at("column").get<std::size_t>() : 0});
    }

    return expectation;
}

bool matches(const codeqa::frontend::FunctionModel& actual, const FunctionExpectation& expected)
{
    if (actual.name != expected.name || actual.kind != expected.kind
        || actual.range.begin.line != expected.range.startLine
        || actual.range.end.line != expected.range.endLine || actual.isStatic != expected.isStatic
        || actual.isVariadic != expected.isVariadic || actual.parameters.size() != expected.parameters.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < expected.parameters.size(); ++index)
    {
        const codeqa::frontend::ParameterModel& parameter = actual.parameters[index];
        const ParameterExpectation& expectedParameter = expected.parameters[index];

        if (parameter.name != expectedParameter.name || parameter.passing != expectedParameter.passing
            || parameter.hasDefaultValue != expectedParameter.hasDefaultValue)
        {
            return false;
        }
    }

    return true;
}

bool matches(const codeqa::frontend::ClassModel& actual, const ClassExpectation& expected)
{
    return actual.name == expected.name && actual.kind == expected.kind
        && actual.range.begin.line == expected.range.startLine
        && actual.range.end.line == expected.range.endLine && actual.isTemplate == expected.isTemplate
        && actual.isScopedEnum == expected.isScopedEnum && actual.baseSpecifiers == expected.bases;
}

bool matches(const codeqa::frontend::IncludeDependency& actual, const IncludeExpectation& expected)
{
    return actual.target == expected.target && actual.delimiter == expected.delimiter
        && actual.range.begin.line == expected.range.startLine
        && actual.range.end.line == expected.range.endLine;
}

std::string describe(const codeqa::frontend::FunctionModel& value)
{
    std::ostringstream output;
    output << value.name << " kind=" << static_cast<int>(value.kind) << " range="
           << value.range.begin.line << '-' << value.range.end.line
           << " parameters=" << value.parameters.size() << " static=" << value.isStatic
           << " variadic=" << value.isVariadic;
    return output.str();
}

std::string describe(const FunctionExpectation& value)
{
    std::ostringstream output;
    output << value.name << " kind=" << static_cast<int>(value.kind) << " range="
           << value.range.startLine << '-' << value.range.endLine
           << " parameters=" << value.parameters.size() << " static=" << value.isStatic
           << " variadic=" << value.isVariadic;
    return output.str();
}

std::string describe(const codeqa::frontend::ClassModel& value)
{
    std::ostringstream output;
    output << value.name << " kind=" << static_cast<int>(value.kind) << " range="
           << value.range.begin.line << '-' << value.range.end.line
           << " template=" << value.isTemplate << " scoped-enum=" << value.isScopedEnum;
    return output.str();
}

std::string describe(const ClassExpectation& value)
{
    std::ostringstream output;
    output << value.name << " kind=" << static_cast<int>(value.kind) << " range="
           << value.range.startLine << '-' << value.range.endLine
           << " template=" << value.isTemplate << " scoped-enum=" << value.isScopedEnum;
    return output.str();
}

std::string describe(const codeqa::frontend::IncludeDependency& value)
{
    std::ostringstream output;
    output << value.target << " delimiter=" << static_cast<int>(value.delimiter) << " range="
           << value.range.begin.line << '-' << value.range.end.line;
    return output.str();
}

std::string describe(const IncludeExpectation& value)
{
    std::ostringstream output;
    output << value.target << " delimiter=" << static_cast<int>(value.delimiter) << " range="
           << value.range.startLine << '-' << value.range.endLine;
    return output.str();
}

template <typename Value>
std::string describeSequence(const std::vector<Value>& values)
{
    std::ostringstream output;

    for (const Value& value : values)
    {
        output << '[' << describe(value) << ']';
    }

    return output.str();
}

template <typename Actual, typename Expected>
void checkExactSequence(const std::vector<Actual>& actual, const std::vector<Expected>& expected)
{
    CAPTURE(actual.size());
    CAPTURE(expected.size());
    REQUIRE_MESSAGE(
        actual.size() == expected.size(),
        "actual: " << describeSequence(actual) << "; expected: " << describeSequence(expected));

    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        CAPTURE(index);
        CHECK_MESSAGE(
            matches(actual[index], expected[index]),
            "actual: " << describe(actual[index]) << "; expected: " << describe(expected[index]));
    }
}

template <typename Actual, typename Expected>
void checkMinimumSequence(const std::vector<Actual>& actual, const std::vector<Expected>& expected)
{
    std::size_t actualIndex = 0;

    for (const Expected& item : expected)
    {
        while (actualIndex < actual.size() && !matches(actual[actualIndex], item))
        {
            ++actualIndex;
        }

        REQUIRE(actualIndex < actual.size());
        ++actualIndex;
    }
}

void checkDiagnostics(
    const std::vector<codeqa::core::Diagnostic>& actual,
    const std::vector<DiagnosticExpectation>& expected)
{
    REQUIRE(actual.size() == expected.size());

    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        CHECK(actual[index].location.line == expected[index].line);
        CHECK(actual[index].severity == expected[index].severity);

        if (expected[index].hasColumn)
        {
            CHECK(actual[index].location.column == expected[index].column);
        }
    }
}

} // namespace

TEST_CASE("every corpus fixture matches its structural expectation")
{
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    for (const CorpusFixture& fixture : collectCorpusFixtures())
    {
        INFO("fixture: " << fixture.sourcePath.generic_string());
        const CorpusExpectation expectation = readExpectation(fixture.expectationPath);
        const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
            fixture.sourcePath,
            grammarForExtension(fixture.sourcePath),
            "corpus structural check",
            parser,
            extractor);

        REQUIRE(result.parsed);

        if (fixture.isMalformed)
        {
            checkMinimumSequence(result.functions, expectation.functions);
            checkMinimumSequence(result.classes, expectation.classes);
        }
        else
        {
            checkExactSequence(result.functions, expectation.functions);
            checkExactSequence(result.classes, expectation.classes);
        }

        checkExactSequence(result.includes, expectation.includes);
        checkDiagnostics(result.diagnostics, expectation.diagnostics);
    }
}
