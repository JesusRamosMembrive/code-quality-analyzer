#include "codeqa/infrastructure/DiscoverSourceFiles.h"

#include "codeqa/core/DiagnosticSeverity.h"
#include "codeqa/infrastructure/ClassifyExtension.h"
#include "codeqa/infrastructure/DefaultExclusions.h"
#include "codeqa/infrastructure/GlobMatcher.h"

#include <algorithm>
#include <system_error>

namespace codeqa::infrastructure
{

namespace
{

bool matchesAnyExcludePattern(const std::vector<std::string>& patterns, const std::filesystem::path& relativePath)
{
    return std::any_of(patterns.begin(), patterns.end(), [&relativePath](const std::string& pattern)
    {
        return matchesExcludePattern(pattern, relativePath);
    });
}

void recordAccessDiagnostic(DiscoveryResult& result, const std::filesystem::path& path, const std::error_code& error)
{
    codeqa::core::Diagnostic diagnostic;
    diagnostic.code = "DISC001";
    diagnostic.severity = codeqa::core::DiagnosticSeverity::Warning;
    diagnostic.message = "Could not access '" + path.string() + "': " + error.message();
    diagnostic.location.filePath = path;

    result.diagnostics.push_back(diagnostic);
}

void advance(
    std::filesystem::recursive_directory_iterator& it,
    const std::filesystem::path& currentPath,
    DiscoveryResult& result)
{
    std::error_code incrementError;
    it.increment(incrementError);

    if (incrementError)
    {
        recordAccessDiagnostic(result, currentPath, incrementError);
    }
}

} // namespace

DiscoveryResult discoverSourceFiles(const DiscoveryOptions& options)
{
    DiscoveryResult result;

    std::error_code constructionError;
    std::filesystem::recursive_directory_iterator it(
        options.rootPath,
        std::filesystem::directory_options::skip_permission_denied,
        constructionError);
    const std::filesystem::recursive_directory_iterator end;

    if (constructionError)
    {
        recordAccessDiagnostic(result, options.rootPath, constructionError);
        return result;
    }

    const std::filesystem::path root = options.rootPath.filename().empty()
        ? options.rootPath.parent_path()
        : options.rootPath;

    while (it != end)
    {
        const std::filesystem::directory_entry entry = *it;

        const std::filesystem::path relativePath = entry.path().lexically_relative(root);

        std::error_code typeError;
        const bool isDirectory = entry.is_directory(typeError);

        if (typeError)
        {
            recordAccessDiagnostic(result, entry.path(), typeError);
            advance(it, entry.path(), result);
            continue;
        }

        if (isDirectory)
        {
            const std::string directoryName = entry.path().filename().string();
            const bool excluded = isDefaultExcludedDirectoryName(directoryName)
                || matchesAnyExcludePattern(options.excludePatterns, relativePath);

            if (excluded)
            {
                result.excludedCount += 1;
                it.disable_recursion_pending();
            }

            advance(it, entry.path(), result);
            continue;
        }

        std::error_code fileTypeError;
        const bool isRegularFile = entry.is_regular_file(fileTypeError);

        if (fileTypeError)
        {
            recordAccessDiagnostic(result, entry.path(), fileTypeError);
            advance(it, entry.path(), result);
            continue;
        }

        if (!isRegularFile)
        {
            advance(it, entry.path(), result);
            continue;
        }

        const std::optional<ExtensionCategory> category = classifyExtension(entry.path().extension().string());

        if (!category.has_value())
        {
            advance(it, entry.path(), result);
            continue;
        }

        if (matchesAnyExcludePattern(options.excludePatterns, relativePath))
        {
            result.excludedCount += 1;
            advance(it, entry.path(), result);
            continue;
        }

        std::error_code sizeError;
        const std::uintmax_t sizeBytes = entry.file_size(sizeError);

        if (sizeError)
        {
            recordAccessDiagnostic(result, entry.path(), sizeError);
            advance(it, entry.path(), result);
            continue;
        }

        result.files.push_back(DiscoveredFile{relativePath, *category, sizeBytes});
        advance(it, entry.path(), result);
    }

    std::sort(result.files.begin(), result.files.end(), [](const DiscoveredFile& lhs, const DiscoveredFile& rhs)
    {
        return lhs.path < rhs.path;
    });

    return result;
}

} // namespace codeqa::infrastructure
