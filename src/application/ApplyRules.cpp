#include "codeqa/application/ApplyRules.h"

#include "codeqa/core/Diagnostic.h"
#include "codeqa/core/SourceRange.h"
#include "codeqa/core/SourceRangeText.h"
#include "codeqa/infrastructure/ReadFileContents.h"
#include "codeqa/infrastructure/StringUtilities.h"
#include "codeqa/rules/Rule.h"
#include "codeqa/rules/Rules.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace codeqa::application
{

namespace
{

bool overlapsParseError(
    const codeqa::core::SourceRange& range, const std::vector<codeqa::core::Diagnostic>& diagnostics)
{
    for (const codeqa::core::Diagnostic& diagnostic : diagnostics)
    {
        if (diagnostic.code != "PARSE001" && diagnostic.code != "PARSE002" && diagnostic.code != "PARSE004")
        {
            continue;
        }

        if (codeqa::core::positionInRange(diagnostic.location.line, diagnostic.location.column, range))
        {
            return true;
        }
    }

    return false;
}

const codeqa::metrics::FileMetricsModel* fileMetricsFor(
    const codeqa::metrics::ProjectMetricsModel& metrics, const std::filesystem::path& path)
{
    for (const codeqa::metrics::FileMetricsModel& file : metrics.files)
    {
        if (file.path == path)
        {
            return &file;
        }
    }

    return nullptr;
}

const codeqa::metrics::FunctionMetricsModel* functionMetricsFor(
    const codeqa::metrics::FileMetricsModel& file, const codeqa::model::SymbolIdentity& symbol)
{
    for (const codeqa::metrics::FunctionMetricsModel& function : file.functions)
    {
        if (function.symbol.range.begin.offset == symbol.range.begin.offset
            && function.symbol.range.end.offset == symbol.range.end.offset)
        {
            return &function;
        }
    }

    return nullptr;
}

std::optional<std::string> suppressedRuleIdOnLine(
    std::size_t line,
    const std::vector<codeqa::core::SourceRange>& fileComments,
    const std::vector<std::string>& sourceLines)
{
    static const std::regex directive(R"(codeqa-disable-next-line\s+([A-Za-z0-9_.\-]+))");

    for (const codeqa::core::SourceRange& comment : fileComments)
    {
        if (comment.begin.line > line || comment.end.line < line)
        {
            continue;
        }

        const std::string text = codeqa::core::extractRangeText(comment, sourceLines);
        std::smatch match;

        if (std::regex_search(text, match, directive))
        {
            return match[1].str();
        }
    }

    return std::nullopt;
}

bool isSuppressed(
    const codeqa::rules::Finding& finding,
    const std::vector<codeqa::core::SourceRange>& fileComments,
    const std::vector<std::string>& sourceLines)
{
    if (finding.location.line == 0)
    {
        return false;
    }

    const std::optional<std::string> suppressed =
        suppressedRuleIdOnLine(finding.location.line - 1, fileComments, sourceLines);
    return suppressed.has_value() && *suppressed == finding.ruleId;
}

} // namespace

codeqa::rules::ProjectFindingsModel ApplyRules::execute(
    const codeqa::model::ProjectModel& project,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::config::ProjectConfig& config) const
{
    codeqa::rules::ProjectFindingsModel result;
    result.rootPath = project.rootPath;
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(config);

    for (const codeqa::model::FileModel& file : project.files)
    {
        codeqa::rules::FileFindingsModel fileFindings;
        fileFindings.path = file.path;

        const codeqa::metrics::FileMetricsModel* fileMetrics = fileMetricsFor(metrics, file.path);

        if (fileMetrics == nullptr)
        {
            result.files.push_back(std::move(fileFindings));
            continue;
        }

        const std::optional<std::string> content = codeqa::infrastructure::readFileContents(file.path);
        const std::vector<std::string> sourceLines =
            content.has_value() ? codeqa::infrastructure::splitLines(*content) : std::vector<std::string>{};

        for (const codeqa::model::CallableModel& callable : file.callables)
        {
            if (callable.kind == codeqa::model::CallableKind::Prototype)
            {
                continue;
            }

            if (overlapsParseError(callable.symbol.range, file.diagnostics))
            {
                continue;
            }

            const codeqa::metrics::FunctionMetricsModel* functionMetrics =
                functionMetricsFor(*fileMetrics, callable.symbol);

            if (functionMetrics == nullptr)
            {
                continue;
            }

            const codeqa::rules::RuleContext context{*functionMetrics, callable};

            for (const std::unique_ptr<codeqa::rules::Rule>& rule : rules)
            {
                if (!rule->appliesTo(file.language))
                {
                    continue;
                }

                for (codeqa::rules::Finding& finding : rule->evaluate(context))
                {
                    if (isSuppressed(finding, file.comments, sourceLines))
                    {
                        fileFindings.suppressedFindings.push_back(std::move(finding));
                    }
                    else
                    {
                        fileFindings.findings.push_back(std::move(finding));
                    }
                }
            }
        }

        result.files.push_back(std::move(fileFindings));
    }

    return result;
}

} // namespace codeqa::application
