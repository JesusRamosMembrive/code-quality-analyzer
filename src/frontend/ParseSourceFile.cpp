#include "codeqa/frontend/ParseSourceFile.h"

#include "codeqa/core/DiagnosticSeverity.h"

#include <tree_sitter/api.h>

#include <cstring>
#include <fstream>
#include <optional>
#include <sstream>

namespace codeqa::frontend
{

namespace
{

codeqa::core::Diagnostic makeDiagnostic(
    const std::string& code,
    const std::string& message,
    const std::filesystem::path& path,
    TSPoint point)
{
    codeqa::core::Diagnostic diagnostic;
    diagnostic.code = code;
    diagnostic.severity = codeqa::core::DiagnosticSeverity::Error;
    diagnostic.message = message;
    diagnostic.location.filePath = path;
    // tree-sitter points are 0-based; SourceLocation is 1-based.
    diagnostic.location.line = static_cast<std::size_t>(point.row) + 1;
    diagnostic.location.column = static_cast<std::size_t>(point.column) + 1;

    return diagnostic;
}

void collectDiagnostics(
    TSNode node,
    const std::filesystem::path& path,
    std::vector<codeqa::core::Diagnostic>& diagnostics)
{
    if (ts_node_is_missing(node))
    {
        diagnostics.push_back(makeDiagnostic(
            "PARSE002", "Expected token is missing.", path, ts_node_start_point(node)));
        return;
    }

    if (ts_node_is_error(node))
    {
        diagnostics.push_back(makeDiagnostic(
            "PARSE001", "Could not parse this region.", path, ts_node_start_point(node)));
        return;
    }

    // A clean subtree cannot contain an ERROR or MISSING node, so skip it whole.
    if (!ts_node_has_error(node))
    {
        return;
    }

    const uint32_t childCount = ts_node_child_count(node);

    for (uint32_t index = 0; index < childCount; ++index)
    {
        collectDiagnostics(ts_node_child(node, index), path, diagnostics);
    }
}

std::optional<std::string> readFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);

    if (!input)
    {
        return std::nullopt;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    return buffer.str();
}

} // namespace

ParsedFile parseSourceFile(
    const std::filesystem::path& path,
    Grammar grammar,
    const std::string& grammarReason,
    TreeSitterParser& parser)
{
    ParsedFile result;
    result.path = path;
    result.grammar = grammar;
    result.grammarReason = grammarReason;

    const std::optional<std::string> source = readFile(path);

    if (!source.has_value())
    {
        result.diagnostics.push_back(makeDiagnostic(
            "PARSE003", "Could not read the file.", path, TSPoint{0, 0}));
        return result;
    }

    std::optional<SyntaxTree> tree = parser.parse(grammar, *source);

    if (!tree.has_value())
    {
        result.diagnostics.push_back(makeDiagnostic(
            "PARSE003", "The parser could not process the file.", path, TSPoint{0, 0}));
        return result;
    }

    result.parsed = true;
    collectDiagnostics(
        ts_tree_root_node(tree->handle()), path, result.diagnostics);

    return result;
}

} // namespace codeqa::frontend
