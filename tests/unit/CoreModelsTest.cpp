#include "doctest/doctest.h"

#include "codeqa/core/Diagnostic.h"
#include "codeqa/core/DiagnosticSeverity.h"
#include "codeqa/core/SourceLocation.h"
#include "codeqa/core/SourceRange.h"

#include <filesystem>

TEST_CASE("SourceLocation has sensible defaults")
{
    const codeqa::core::SourceLocation location;

    CHECK(location.filePath.empty());
    CHECK(location.line == 1);
    CHECK(location.column == 1);
    CHECK(location.offset == 0);
}

TEST_CASE("SourceLocation stores the values it is constructed with")
{
    const codeqa::core::SourceLocation location{"src/main.cpp", 12, 5, 84};

    CHECK(location.filePath == std::filesystem::path{"src/main.cpp"});
    CHECK(location.line == 12);
    CHECK(location.column == 5);
    CHECK(location.offset == 84);
}

TEST_CASE("SourceRange stores begin and end locations")
{
    const codeqa::core::SourceLocation begin{"src/main.cpp", 1, 1, 0};
    const codeqa::core::SourceLocation end{"src/main.cpp", 3, 1, 40};

    const codeqa::core::SourceRange range{begin, end};

    CHECK(range.begin.line == 1);
    CHECK(range.end.line == 3);
}

TEST_CASE("Diagnostic defaults to Error severity")
{
    const codeqa::core::Diagnostic diagnostic;

    CHECK(diagnostic.severity == codeqa::core::DiagnosticSeverity::Error);
    CHECK(diagnostic.code.empty());
    CHECK(diagnostic.message.empty());
}

TEST_CASE("Diagnostic stores the values it is constructed with")
{
    const codeqa::core::Diagnostic diagnostic{
        "CLI001",
        codeqa::core::DiagnosticSeverity::Warning,
        "Unknown option '--unknown'.",
        codeqa::core::SourceLocation{}};

    CHECK(diagnostic.code == "CLI001");
    CHECK(diagnostic.severity == codeqa::core::DiagnosticSeverity::Warning);
    CHECK(diagnostic.message == "Unknown option '--unknown'.");
}
