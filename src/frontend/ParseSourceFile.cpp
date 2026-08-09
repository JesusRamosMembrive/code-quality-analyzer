#include "codeqa/frontend/ParseSourceFile.h"

#include "codeqa/core/DiagnosticSeverity.h"
#include "codeqa/frontend/StructuralExtractor.h"
#include "codeqa/infrastructure/ReadFileContents.h"

#include <cstring>
#include <filesystem>
#include <optional>

namespace codeqa::frontend
{

namespace
{

codeqa::core::Diagnostic makeDiagnostic(
    const std::string& code,
    const std::string& message,
    const std::filesystem::path& path,
    std::size_t line,
    std::size_t column)
{
    codeqa::core::Diagnostic diagnostic;
    diagnostic.code = code;
    diagnostic.severity = codeqa::core::DiagnosticSeverity::Error;
    diagnostic.message = message;
    diagnostic.location.filePath = path;
    diagnostic.location.line = line + 1;
    diagnostic.location.column = column + 1;

    return diagnostic;
}

} // namespace

ParsedFile parseSourceFile(
    const std::filesystem::path& path,
    Grammar grammar,
    const std::string& grammarReason,
    TreeSitterParser& parser,
    StructuralExtractor& extractor)
{
    ParsedFile result;
    result.path = path;
    result.language = grammar == Grammar::C
        ? codeqa::core::Language::C
        : codeqa::core::Language::Cpp;
    result.grammar = grammar;
    result.grammarReason = grammarReason;

    const std::optional<std::string> source = codeqa::infrastructure::readFileContents(path);

    if (!source.has_value())
    {
        result.diagnostics.push_back(makeDiagnostic(
            "PARSE003", "Could not read the file.", path, 0, 0));
        return result;
    }

    std::optional<SyntaxTree> tree = parser.parse(grammar, *source);

    if (!tree.has_value())
    {
        result.diagnostics.push_back(makeDiagnostic(
            "PARSE003", "The parser could not process the file.", path, 0, 0));
        return result;
    }

    result.parsed = true;
    for (const SyntaxError& error : tree->syntaxErrors())
    {
        result.diagnostics.push_back(makeDiagnostic(
            error.missing ? "PARSE002" : "PARSE001",
            error.missing ? "Expected token is missing." : "Could not parse this region.",
            path,
            error.line,
            error.column));
    }

    extractor.extract(*tree, grammar, *source, path, result);

    return result;
}

} // namespace codeqa::frontend
