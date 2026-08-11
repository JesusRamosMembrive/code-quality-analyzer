#include "doctest/doctest.h"

#include "codeqa/core/EnumJson.h"

TEST_CASE("languageToJsonString maps every Language value")
{
    CHECK(codeqa::core::languageToJsonString(codeqa::core::Language::C) == "c");
    CHECK(codeqa::core::languageToJsonString(codeqa::core::Language::Cpp) == "cpp");
    CHECK(codeqa::core::languageToJsonString(codeqa::core::Language::Unknown) == "unknown");
}

TEST_CASE("languageFromJsonString accepts only c and cpp")
{
    const std::optional<codeqa::core::Language> c = codeqa::core::languageFromJsonString("c");
    REQUIRE(c.has_value());
    CHECK(*c == codeqa::core::Language::C);

    const std::optional<codeqa::core::Language> cpp = codeqa::core::languageFromJsonString("cpp");
    REQUIRE(cpp.has_value());
    CHECK(*cpp == codeqa::core::Language::Cpp);

    CHECK_FALSE(codeqa::core::languageFromJsonString("unknown").has_value());
    CHECK_FALSE(codeqa::core::languageFromJsonString("rust").has_value());
}

TEST_CASE("diagnosticSeverityToJsonString maps every DiagnosticSeverity value")
{
    CHECK(codeqa::core::diagnosticSeverityToJsonString(codeqa::core::DiagnosticSeverity::Error) == "error");
    CHECK(codeqa::core::diagnosticSeverityToJsonString(codeqa::core::DiagnosticSeverity::Warning) == "warning");
    CHECK(
        codeqa::core::diagnosticSeverityToJsonString(codeqa::core::DiagnosticSeverity::Information)
        == "information");
}
