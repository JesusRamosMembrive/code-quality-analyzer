#include "codeqa/application/MeasureProject.h"

#include "codeqa/core/DiagnosticSeverity.h"
#include "codeqa/metrics/CalculateFunctionMetrics.h"

#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace codeqa::application
{

namespace
{

// Mirrors the binary-read helper in ParseSourceFile.cpp: same contract (raw
// bytes, no line-ending translation), different layer (application, not
// frontend), so it is not shared across the frontend/application boundary.
std::optional<std::string> readFileContents(const std::filesystem::path& path)
{
    std::error_code sizeError;
    const std::uintmax_t size = std::filesystem::file_size(path, sizeError);

    if (sizeError)
    {
        return std::nullopt;
    }

    std::ifstream input(path, std::ios::binary);

    if (!input)
    {
        return std::nullopt;
    }

    std::string content(static_cast<std::size_t>(size), '\0');

    if (size > 0)
    {
        input.read(content.data(), static_cast<std::streamsize>(size));

        if (!input)
        {
            return std::nullopt;
        }
    }

    return content;
}

std::vector<std::string> splitLines(const std::string& content)
{
    std::vector<std::string> lines;
    std::size_t start = 0;

    while (start <= content.size())
    {
        const std::size_t newline = content.find('\n', start);
        const std::size_t end = (newline == std::string::npos) ? content.size() : newline;
        std::size_t lineEnd = end;

        if (lineEnd > start && content[lineEnd - 1] == '\r')
        {
            --lineEnd;
        }

        lines.push_back(content.substr(start, lineEnd - start));

        if (newline == std::string::npos)
        {
            break;
        }

        start = newline + 1;
    }

    return lines;
}

codeqa::core::Diagnostic makeUnreadableDiagnostic(const std::filesystem::path& path)
{
    codeqa::core::Diagnostic diagnostic;
    diagnostic.code = "METRICS001";
    diagnostic.severity = codeqa::core::DiagnosticSeverity::Error;
    diagnostic.message = "Could not reread the file to compute metrics.";
    diagnostic.location.filePath = path;
    diagnostic.location.line = 1;
    diagnostic.location.column = 1;
    return diagnostic;
}

} // namespace

codeqa::metrics::ProjectMetricsModel MeasureProject::execute(const codeqa::model::ProjectModel& project) const
{
    codeqa::metrics::ProjectMetricsModel result;
    result.rootPath = project.rootPath;

    for (const codeqa::model::FileModel& file : project.files)
    {
        codeqa::metrics::FileMetricsModel fileMetrics;
        fileMetrics.path = file.path;

        const std::optional<std::string> content = readFileContents(file.path);

        if (!content.has_value())
        {
            fileMetrics.diagnostics.push_back(makeUnreadableDiagnostic(file.path));
            result.files.push_back(std::move(fileMetrics));
            continue;
        }

        const std::vector<std::string> sourceLines = splitLines(*content);

        for (const codeqa::model::CallableModel& callable : file.callables)
        {
            if (callable.kind == codeqa::model::CallableKind::Prototype)
            {
                continue;
            }

            fileMetrics.functions.push_back(
                codeqa::metrics::calculateFunctionMetrics(callable, file.comments, sourceLines));
        }

        result.files.push_back(std::move(fileMetrics));
    }

    return result;
}

} // namespace codeqa::application
