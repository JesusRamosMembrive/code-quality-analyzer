#include "codeqa/cli/RunApplication.h"

#include "codeqa/application/ApplicationFacade.h"
#include "codeqa/application/ApplicationInfo.h"
#include "codeqa/application/DetectFileLanguage.h"
#include "codeqa/application/GetApplicationInfo.h"
#include "codeqa/application/LoadProjectConfig.h"
#include "codeqa/application/ListSnapshots.h"
#include "codeqa/application/LoadSnapshot.h"
#include "codeqa/application/ReportDocumentAnalysis.h"
#include "codeqa/application/RunAnalysis.h"
#include "codeqa/cli/ExitCode.h"
#include "codeqa/config/ConfigModels.h"
#include "codeqa/config/DefaultConfig.h"
#include "codeqa/core/Diagnostic.h"
#include "codeqa/core/DiagnosticSeverity.h"
#include "codeqa/core/EnumJson.h"
#include "codeqa/core/Language.h"
#include "codeqa/core/LanguageDetectionResult.h"
#include "codeqa/frontend/Grammar.h"
#include "codeqa/frontend/ParseSourceFile.h"
#include "codeqa/frontend/ParsedFile.h"
#include "codeqa/frontend/SelectGrammar.h"
#include "codeqa/frontend/StructuralExtractor.h"
#include "codeqa/frontend/TreeSitterParser.h"
#include "codeqa/infrastructure/ClassifyExtension.h"
#include "codeqa/infrastructure/DiscoverSourceFiles.h"
#include "codeqa/infrastructure/DiscoveredFile.h"
#include "codeqa/infrastructure/DiscoveryOptions.h"
#include "codeqa/infrastructure/DiscoveryResult.h"
#include "codeqa/infrastructure/ExtensionCategory.h"
#include "codeqa/duplication/DuplicationModels.h"
#include "codeqa/report/MarkdownReport.h"
#include "codeqa/report/TextReport.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <system_error>
#include <utility>

namespace codeqa::cli
{

namespace
{

void printHelp(std::ostream& output)
{
    output << "Code Quality Analyzer\n"
              "\n"
              "Usage:\n"
              "  codeqa [options]\n"
              "  codeqa discover <path> [--exclude <glob>]... [--lang-override <pattern>=<lang>]...\n"
              "  codeqa parse <path> [--exclude <glob>]... [--lang-override <pattern>=<lang>]...\n"
              "  codeqa analyze <path> [--exclude <glob>]... [--lang-override <pattern>=<lang>]...\n"
              "                        [--config <path>] [--format text|json|markdown] [--output <file>]\n"
              "  codeqa snapshot list <path>\n"
              "  codeqa snapshot show <path> <timestamp>\n"
              "\n"
              "Options:\n"
              "  -h, --help       Show this help.\n"
              "  -v, --version    Show application version.\n"
              "\n"
              "Commands:\n"
              "  discover <path>                    Discover C/C++ source files under <path>.\n"
              "  parse <path>                       Discover, detect language, and parse\n"
              "                                      C/C++ source files under <path> with\n"
              "                                      tree-sitter.\n"
              "      --exclude <glob>                Exclude files/directories matching a glob\n"
              "                                       pattern (repeatable).\n"
              "      --lang-override <pattern>=<lang> Force the language of files matching\n"
              "                                       <pattern> (<lang> is 'c' or 'cpp';\n"
              "                                       repeatable, last match wins).\n"
              "  analyze <path>                     Run the full analysis pipeline (metrics +\n"
              "                                      rules) and print a report. Also saves a\n"
              "                                      snapshot under <path>/.codeqa/snapshots/\n"
              "                                      and compares it against the previous one\n"
              "                                      automatically. Exits 1 if the quality gate\n"
              "                                      fails (see qualityGate.minSeverity).\n"
              "      --config <path>                 Merge rule/language overrides from a\n"
              "                                       codeqa.json file over the defaults,\n"
              "                                       including qualityGate.minSeverity.\n"
              "      --format text|json|markdown       Report format (default: text). 'markdown'\n"
              "                                        writes a per-file checklist suitable for\n"
              "                                        a human or an AI to work through.\n"
              "      --output <file>                  Write the report to a file instead of\n"
              "                                        stdout.\n"
              "  snapshot list <path>               List saved snapshot timestamps for <path>,\n"
              "                                      oldest first.\n"
              "  snapshot show <path> <timestamp>   Print the JSON of one saved snapshot.\n";
}

void printVersion(std::ostream& output)
{
    const codeqa::application::GetApplicationInfo getApplicationInfo;
    const codeqa::application::ApplicationInfo info = getApplicationInfo.execute();

    output << info.name << ' ' << info.version << '\n';
}

void printDiscoveryResult(std::ostream& output, const codeqa::infrastructure::DiscoveryResult& result)
{
    std::size_t cCount = 0;
    std::size_t cppCount = 0;
    std::size_t ambiguousCount = 0;

    for (const codeqa::infrastructure::DiscoveredFile& file : result.files)
    {
        switch (file.category)
        {
        case codeqa::infrastructure::ExtensionCategory::C:
            ++cCount;
            break;
        case codeqa::infrastructure::ExtensionCategory::Cpp:
            ++cppCount;
            break;
        case codeqa::infrastructure::ExtensionCategory::AmbiguousHeader:
            ++ambiguousCount;
            break;
        }
    }

    output << "Files discovered: " << result.files.size() << '\n'
           << "C files: " << cCount << '\n'
           << "C++ files: " << cppCount << '\n'
           << "Ambiguous headers: " << ambiguousCount << '\n'
           << "Excluded files: " << result.excludedCount << '\n'
           << "Read errors: " << result.diagnostics.size() << '\n';
}

const char* languageName(codeqa::core::Language language)
{
    switch (language)
    {
    case codeqa::core::Language::C:
        return "C";
    case codeqa::core::Language::Cpp:
        return "C++";
    case codeqa::core::Language::Unknown:
        return "Unknown";
    }

    return "Unknown";
}

void printLanguageListing(
    std::ostream& output,
    const codeqa::infrastructure::DiscoveryResult& result,
    const std::vector<codeqa::application::LanguageOverride>& overrides)
{
    if (result.files.empty())
    {
        return;
    }

    output << '\n';
    output << "Detected languages:\n";

    const codeqa::application::DetectFileLanguage detectFileLanguage;

    for (const codeqa::infrastructure::DiscoveredFile& file : result.files)
    {
        const codeqa::core::LanguageDetectionResult detection =
            detectFileLanguage.execute(file, overrides);

        output << file.path.generic_string() << "  " << languageName(detection.language);

        if (detection.reason != "unambiguous extension")
        {
            std::ostringstream confidenceText;
            confidenceText << std::fixed << std::setprecision(2) << detection.confidence;

            output << " (confidence " << confidenceText.str() << ") - " << detection.reason;
        }

        output << '\n';
    }
}

std::optional<codeqa::core::Language> parseLanguageName(const std::string& languageText)
{
    if (languageText == "c")
    {
        return codeqa::core::Language::C;
    }

    if (languageText == "cpp")
    {
        return codeqa::core::Language::Cpp;
    }

    return std::nullopt;
}

enum class FlagParseResult
{
    NotAFlag,
    Consumed,
    Failed
};

/// --exclude / --lang-override, shared by discover, parse, and analyze. On Failed, the
/// caller always uses ExitCode::InvalidArguments -- there is only ever one exit code for
/// a parse failure in this file, so the result carries no error payload to thread back.
FlagParseResult tryParseDiscoveryFlag(
    const std::string& token,
    std::size_t& index,
    const std::vector<std::string>& arguments,
    std::ostream& errorOutput,
    std::vector<std::string>& excludePatterns,
    std::vector<codeqa::application::LanguageOverride>& languageOverrides)
{
    if (token == "--exclude")
    {
        if (index + 1 >= arguments.size())
        {
            errorOutput << "Error [CLI004]: Missing value for '--exclude'.\n";
            return FlagParseResult::Failed;
        }

        excludePatterns.push_back(arguments[index + 1]);
        ++index;
        return FlagParseResult::Consumed;
    }

    if (token == "--lang-override")
    {
        if (index + 1 >= arguments.size())
        {
            errorOutput << "Error [CLI006]: Missing value for '--lang-override'.\n";
            return FlagParseResult::Failed;
        }

        const std::string& value = arguments[index + 1];
        const std::size_t equalsPosition = value.find('=');

        if (equalsPosition == std::string::npos || equalsPosition == 0
            || equalsPosition == value.size() - 1)
        {
            errorOutput << "Error [CLI007]: Invalid --lang-override value '" << value
                           << "'. Expected <pattern>=c|cpp.\n";
            return FlagParseResult::Failed;
        }

        const std::string pattern = value.substr(0, equalsPosition);
        const std::string languageText = value.substr(equalsPosition + 1);
        const std::optional<codeqa::core::Language> language = parseLanguageName(languageText);

        if (!language.has_value())
        {
            errorOutput << "Error [CLI007]: Invalid --lang-override value '" << value
                           << "'. Expected <pattern>=c|cpp.\n";
            return FlagParseResult::Failed;
        }

        languageOverrides.push_back(codeqa::application::LanguageOverride{pattern, *language});
        ++index;
        return FlagParseResult::Consumed;
    }

    return FlagParseResult::NotAFlag;
}

/// Required <path>, shared by discover, parse, analyze, and snapshot.
std::optional<int> requirePathIsDirectory(
    const std::optional<std::filesystem::path>& path,
    const std::string& subcommandLabel,
    std::ostream& errorOutput)
{
    if (!path.has_value())
    {
        errorOutput << "Error [CLI002]: Missing required argument <path> for '" << subcommandLabel << "'.\n"
                       "Use 'codeqa --help' for usage.\n";
        return static_cast<int>(ExitCode::InvalidArguments);
    }

    std::error_code existsError;
    if (!std::filesystem::is_directory(*path, existsError))
    {
        errorOutput << "Error [CLI003]: Path '" << path->string() << "' does not exist or is not a directory.\n";
        return static_cast<int>(ExitCode::InvalidArguments);
    }

    return std::nullopt;
}

/// CLI005, shared by discover, parse, analyze, and snapshot.
int reportUnexpectedArgument(const std::string& token, std::ostream& errorOutput)
{
    errorOutput << "Error [CLI005]: Unexpected argument '" << token << "'.\n";
    return static_cast<int>(ExitCode::InvalidArguments);
}

/// The arguments shared by `discover` and `parse`: a required <path>, plus repeatable
/// --exclude and --lang-override flags.
struct DiscoverArguments
{
    std::filesystem::path path;
    std::vector<std::string> excludePatterns;
    std::vector<codeqa::application::LanguageOverride> languageOverrides;
};

/// Outcome of parsing a subcommand's shared arguments: either a filled-in
/// DiscoverArguments on success, or the exit code to propagate (having already written
/// the error to errorOutput) on failure.
struct DiscoverArgumentsOutcome
{
    std::optional<DiscoverArguments> arguments;
    int exitCode{static_cast<int>(ExitCode::Success)};
};

/// Parses the argument shape shared by `discover` and `parse`
/// (`<path> [--exclude <glob>]... [--lang-override <pattern>=<lang>]...`), reporting
/// CLI002-CLI007 to errorOutput on failure. subcommandName is used only for the CLI002
/// message text.
DiscoverArgumentsOutcome parseDiscoverArguments(
    const std::string& subcommandName,
    const std::vector<std::string>& arguments,
    std::ostream& errorOutput)
{
    std::optional<std::filesystem::path> path;
    std::vector<std::string> excludePatterns;
    std::vector<codeqa::application::LanguageOverride> languageOverrides;

    for (std::size_t index = 1; index < arguments.size(); ++index)
    {
        const std::string& token = arguments[index];

        const FlagParseResult flag = tryParseDiscoveryFlag(
            token, index, arguments, errorOutput, excludePatterns, languageOverrides);

        if (flag == FlagParseResult::Failed)
        {
            return DiscoverArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
        }

        if (flag == FlagParseResult::Consumed)
        {
            continue;
        }

        if (!path.has_value())
        {
            path = std::filesystem::path{token};
            continue;
        }

        return DiscoverArgumentsOutcome{std::nullopt, reportUnexpectedArgument(token, errorOutput)};
    }

    if (const std::optional<int> pathError = requirePathIsDirectory(path, subcommandName, errorOutput))
    {
        return DiscoverArgumentsOutcome{std::nullopt, *pathError};
    }

    return DiscoverArgumentsOutcome{
        DiscoverArguments{*path, excludePatterns, languageOverrides},
        static_cast<int>(ExitCode::Success)};
}

int runDiscover(const std::vector<std::string>& arguments, std::ostream& output, std::ostream& errorOutput)
{
    const DiscoverArgumentsOutcome outcome = parseDiscoverArguments("discover", arguments, errorOutput);

    if (!outcome.arguments.has_value())
    {
        return outcome.exitCode;
    }

    const DiscoverArguments& parsedArguments = *outcome.arguments;

    codeqa::infrastructure::DiscoveryOptions options;
    options.rootPath = parsedArguments.path;
    options.excludePatterns = parsedArguments.excludePatterns;

    const codeqa::infrastructure::DiscoveryResult result = codeqa::infrastructure::discoverSourceFiles(options);

    printDiscoveryResult(output, result);
    printLanguageListing(output, result, parsedArguments.languageOverrides);
    return static_cast<int>(ExitCode::Success);
}

int runParse(const std::vector<std::string>& arguments, std::ostream& output, std::ostream& errorOutput)
{
    const DiscoverArgumentsOutcome outcome = parseDiscoverArguments("parse", arguments, errorOutput);

    if (!outcome.arguments.has_value())
    {
        return outcome.exitCode;
    }

    const DiscoverArguments& parsedArguments = *outcome.arguments;

    codeqa::infrastructure::DiscoveryOptions options;
    options.rootPath = parsedArguments.path;
    options.excludePatterns = parsedArguments.excludePatterns;

    const codeqa::infrastructure::DiscoveryResult discoveryResult =
        codeqa::infrastructure::discoverSourceFiles(options);

    const codeqa::application::DetectFileLanguage detectFileLanguage;
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    std::size_t filesParsedCount = 0;
    std::size_t cCount = 0;
    std::size_t cppCount = 0;

    struct ParseDetail
    {
        std::filesystem::path relativePath;
        std::vector<codeqa::core::Diagnostic> diagnostics;
    };
    std::vector<ParseDetail> details;

    for (const codeqa::infrastructure::DiscoveredFile& file : discoveryResult.files)
    {
        const codeqa::core::LanguageDetectionResult detection =
            detectFileLanguage.execute(file, parsedArguments.languageOverrides);
        const codeqa::frontend::GrammarSelection grammarSelection =
            codeqa::frontend::selectGrammar(detection.language);

        const std::filesystem::path fullPath = parsedArguments.path / file.path;

        codeqa::frontend::ParsedFile parsedFile = codeqa::frontend::parseSourceFile(
            fullPath, grammarSelection.grammar, grammarSelection.reason, parser, extractor);

        if (parsedFile.parsed)
        {
            ++filesParsedCount;

            if (grammarSelection.grammar == codeqa::frontend::Grammar::C)
            {
                ++cCount;
            }
            else
            {
                ++cppCount;
            }
        }

        if (!parsedFile.diagnostics.empty())
        {
            details.push_back(ParseDetail{file.path, std::move(parsedFile.diagnostics)});
        }
    }

    output << "Files parsed: " << filesParsedCount << '\n'
           << "C files: " << cCount << '\n'
           << "C++ files: " << cppCount << '\n'
           << "Parse errors: " << details.size() << '\n';

    if (!details.empty())
    {
        output << '\n';

        for (const ParseDetail& detail : details)
        {
            output << detail.relativePath.generic_string() << '\n';

            for (const codeqa::core::Diagnostic& diagnostic : detail.diagnostics)
            {
                output << "  line " << diagnostic.location.line << ": "
                       << codeqa::core::diagnosticSeverityToJsonString(diagnostic.severity) << '\n';
            }
        }
    }

    return static_cast<int>(ExitCode::Success);
}

struct AnalyzeArguments
{
    std::filesystem::path path;
    std::vector<std::string> excludePatterns;
    std::vector<codeqa::application::LanguageOverride> languageOverrides;
    std::optional<std::filesystem::path> configPath;
    std::string format{"text"};
    std::optional<std::filesystem::path> outputPath;
};

struct AnalyzeArgumentsOutcome
{
    std::optional<AnalyzeArguments> arguments;
    int exitCode{static_cast<int>(ExitCode::Success)};
};

AnalyzeArgumentsOutcome parseAnalyzeArguments(const std::vector<std::string>& arguments, std::ostream& errorOutput)
{
    std::optional<std::filesystem::path> path;
    std::vector<std::string> excludePatterns;
    std::vector<codeqa::application::LanguageOverride> languageOverrides;
    std::optional<std::filesystem::path> configPath;
    std::string format = "text";
    std::optional<std::filesystem::path> outputPath;

    for (std::size_t index = 1; index < arguments.size(); ++index)
    {
        const std::string& token = arguments[index];

        const FlagParseResult flag = tryParseDiscoveryFlag(
            token, index, arguments, errorOutput, excludePatterns, languageOverrides);

        if (flag == FlagParseResult::Failed)
        {
            return AnalyzeArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
        }

        if (flag == FlagParseResult::Consumed)
        {
            continue;
        }

        if (token == "--config")
        {
            if (index + 1 >= arguments.size())
            {
                errorOutput << "Error [CLI008]: Missing value for '--config'.\n";
                return AnalyzeArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
            }

            configPath = std::filesystem::path{arguments[index + 1]};
            ++index;
            continue;
        }

        if (token == "--format")
        {
            if (index + 1 >= arguments.size())
            {
                errorOutput << "Error [CLI009]: Missing value for '--format'.\n";
                return AnalyzeArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
            }

            const std::string& value = arguments[index + 1];

            if (value != "text" && value != "json" && value != "markdown")
            {
                errorOutput << "Error [CLI010]: Invalid --format value '" << value
                               << "'. Expected 'text', 'json', or 'markdown'.\n";
                return AnalyzeArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
            }

            format = value;
            ++index;
            continue;
        }

        if (token == "--output")
        {
            if (index + 1 >= arguments.size())
            {
                errorOutput << "Error [CLI011]: Missing value for '--output'.\n";
                return AnalyzeArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
            }

            outputPath = std::filesystem::path{arguments[index + 1]};
            ++index;
            continue;
        }

        if (!path.has_value())
        {
            path = std::filesystem::path{token};
            continue;
        }

        return AnalyzeArgumentsOutcome{std::nullopt, reportUnexpectedArgument(token, errorOutput)};
    }

    if (const std::optional<int> pathError = requirePathIsDirectory(path, "analyze", errorOutput))
    {
        return AnalyzeArgumentsOutcome{std::nullopt, *pathError};
    }

    return AnalyzeArgumentsOutcome{
        AnalyzeArguments{*path, excludePatterns, languageOverrides, configPath, format, outputPath},
        static_cast<int>(ExitCode::Success)};
}

std::filesystem::path normalizedPath(const std::filesystem::path& path)
{
    std::error_code canonicalError;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, canonicalError);
    if (!canonicalError)
    {
        return canonical;
    }

    std::error_code absoluteError;
    const std::filesystem::path absolute = std::filesystem::absolute(path, absoluteError);
    return absoluteError ? path.lexically_normal() : absolute.lexically_normal();
}

bool samePath(const std::filesystem::path& left, const std::filesystem::path& right)
{
    std::error_code equivalentError;
    const bool equivalent = std::filesystem::equivalent(left, right, equivalentError);
    if (!equivalentError)
    {
        return equivalent;
    }
    return normalizedPath(left) == normalizedPath(right);
}

bool isWithin(const std::filesystem::path& candidate, const std::filesystem::path& directory)
{
    const std::filesystem::path relative = normalizedPath(candidate).lexically_relative(normalizedPath(directory));
    if (relative.empty() || relative.is_absolute())
    {
        return false;
    }

    for (const std::filesystem::path& component : relative)
    {
        if (component == "..")
        {
            return false;
        }
    }
    return true;
}

bool outputConflictsWithAnalysis(
    const AnalyzeArguments& arguments,
    const std::filesystem::path& outputPath)
{
    if (arguments.configPath.has_value() && samePath(outputPath, *arguments.configPath))
    {
        return true;
    }

    const std::filesystem::path snapshotsDirectory = arguments.path / ".codeqa" / "snapshots";
    if (isWithin(outputPath, snapshotsDirectory))
    {
        return true;
    }

    codeqa::infrastructure::DiscoveryOptions discoveryOptions;
    discoveryOptions.rootPath = arguments.path;
    discoveryOptions.excludePatterns = arguments.excludePatterns;
    const codeqa::infrastructure::DiscoveryResult discovery =
        codeqa::infrastructure::discoverSourceFiles(discoveryOptions);
    for (const codeqa::infrastructure::DiscoveredFile& file : discovery.files)
    {
        if (samePath(outputPath, arguments.path / file.path))
        {
            return true;
        }
    }

    return isWithin(outputPath, arguments.path)
        && codeqa::infrastructure::classifyExtension(outputPath.extension().string()).has_value();
}

int runAnalyze(const std::vector<std::string>& arguments, std::ostream& output, std::ostream& errorOutput)
{
    const AnalyzeArgumentsOutcome outcome = parseAnalyzeArguments(arguments, errorOutput);

    if (!outcome.arguments.has_value())
    {
        return outcome.exitCode;
    }

    const AnalyzeArguments& parsedArguments = *outcome.arguments;

    codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();

    if (parsedArguments.configPath.has_value())
    {
        const codeqa::application::LoadProjectConfigResult configResult =
            codeqa::application::LoadProjectConfig{}.execute(*parsedArguments.configPath);

        if (!configResult.config.has_value())
        {
            errorOutput << *configResult.error << '\n';
            return static_cast<int>(ExitCode::ConfigurationError);
        }

        config = *configResult.config;
    }

    std::ofstream outputFile;
    std::ostream* destination = &output;

    if (parsedArguments.outputPath.has_value())
    {
        if (outputConflictsWithAnalysis(parsedArguments, *parsedArguments.outputPath))
        {
            errorOutput << "Error [CLI017]: Output path '" << parsedArguments.outputPath->string()
                           << "' conflicts with an analysis input or managed snapshot.\n";
            return static_cast<int>(ExitCode::InvalidArguments);
        }

        outputFile.open(*parsedArguments.outputPath);
        if (!outputFile)
        {
            errorOutput << "Error [CLI012]: Could not open '" << parsedArguments.outputPath->string()
                           << "' for writing.\n";
            return static_cast<int>(ExitCode::InvalidArguments);
        }
        destination = &outputFile;
    }

    const codeqa::application::FacadeAnalysisRequest request{
        parsedArguments.path,
        config,
        parsedArguments.excludePatterns,
        parsedArguments.languageOverrides};
    const codeqa::application::AnalysisFacadeResult result =
        codeqa::application::ApplicationFacade{}.analyzeProject(request);

    if (result.error.hasError())
    {
        errorOutput << *result.error.message << '\n';
        return static_cast<int>(ExitCode::InternalError);
    }
    if (!result.document.has_value() || !result.reportJson.has_value())
    {
        errorOutput << "Error [APPLICATION002]: analysis produced no report.\n";
        return static_cast<int>(ExitCode::InternalError);
    }

    if (result.warning.has_value())
    {
        errorOutput << *result.warning << '\n';
    }

    if (parsedArguments.format == "json")
    {
        nlohmann::json displayJson = *result.reportJson;

        if (result.comparison.has_value())
        {
            displayJson["comparison"] = {
                {"previousTimestamp", result.comparison->previousTimestamp},
                {"new", result.comparison->newCount},
                {"existing", result.comparison->existingCount},
                {"resolved", result.comparison->resolvedCount}};
        }

        *destination << displayJson.dump(2) << '\n';
    }
    else if (parsedArguments.format == "markdown")
    {
        const codeqa::application::RunAnalysisResult textAnalysis =
            codeqa::application::toAnalysisResult(*result.document);
        codeqa::report::renderMarkdown(
            *destination, textAnalysis.model, textAnalysis.metrics, textAnalysis.findings);
    }
    else
    {
        const codeqa::application::RunAnalysisResult textAnalysis =
            codeqa::application::toAnalysisResult(*result.document);
        codeqa::report::renderText(
            *destination, textAnalysis.model, textAnalysis.metrics, textAnalysis.findings,
            textAnalysis.duplicates, textAnalysis.architecture);

        if (result.comparison.has_value())
        {
            *destination << "\nFindings: " << result.comparison->newCount << " new, "
                         << result.comparison->existingCount << " existing, "
                         << result.comparison->resolvedCount << " resolved.\n";
        }
    }

    return static_cast<int>(
        result.qualityGateOutcome == codeqa::application::QualityGateOutcome::Failed
            ? ExitCode::QualityGateFailed
            : ExitCode::Success);
}

struct SnapshotArguments
{
    std::string verb;
    std::filesystem::path path;
    std::optional<std::string> timestamp;
};

struct SnapshotArgumentsOutcome
{
    std::optional<SnapshotArguments> arguments;
    int exitCode{static_cast<int>(ExitCode::Success)};
};

SnapshotArgumentsOutcome parseSnapshotArguments(
    const std::vector<std::string>& arguments, std::ostream& errorOutput)
{
    if (arguments.size() < 2)
    {
        errorOutput << "Error [CLI013]: Missing subcommand ('list' or 'show') for 'snapshot'.\n"
                       "Use 'codeqa --help' for usage.\n";
        return SnapshotArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
    }

    const std::string& verb = arguments[1];

    if (verb != "list" && verb != "show")
    {
        errorOutput << "Error [CLI014]: Unknown snapshot subcommand '" << verb
                       << "'. Expected 'list' or 'show'.\n";
        return SnapshotArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
    }

    const std::optional<std::filesystem::path> path =
        arguments.size() >= 3 ? std::optional<std::filesystem::path>{arguments[2]} : std::nullopt;

    if (const std::optional<int> pathError = requirePathIsDirectory(path, "snapshot " + verb, errorOutput))
    {
        return SnapshotArgumentsOutcome{std::nullopt, *pathError};
    }

    std::optional<std::string> timestamp;

    if (verb == "show")
    {
        if (arguments.size() < 4)
        {
            errorOutput << "Error [CLI015]: Missing required argument <timestamp> for 'snapshot show'.\n";
            return SnapshotArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
        }

        timestamp = arguments[3];
    }

    const std::size_t expectedArgumentCount = (verb == "show") ? 4 : 3;

    if (arguments.size() > expectedArgumentCount)
    {
        return SnapshotArgumentsOutcome{
            std::nullopt, reportUnexpectedArgument(arguments[expectedArgumentCount], errorOutput)};
    }

    return SnapshotArgumentsOutcome{
        SnapshotArguments{verb, *path, timestamp}, static_cast<int>(ExitCode::Success)};
}

int runSnapshot(const std::vector<std::string>& arguments, std::ostream& output, std::ostream& errorOutput)
{
    const SnapshotArgumentsOutcome outcome = parseSnapshotArguments(arguments, errorOutput);

    if (!outcome.arguments.has_value())
    {
        return outcome.exitCode;
    }

    const SnapshotArguments& parsedArguments = *outcome.arguments;

    if (parsedArguments.verb == "list")
    {
        for (const std::string& timestamp : codeqa::application::ListSnapshots{}.execute(parsedArguments.path))
        {
            output << timestamp << '\n';
        }

        return static_cast<int>(ExitCode::Success);
    }

    const std::optional<nlohmann::json> snapshot =
        codeqa::application::LoadSnapshot{}.execute(parsedArguments.path, *parsedArguments.timestamp);

    if (!snapshot.has_value())
    {
        errorOutput << "Error [CLI016]: Snapshot '" << *parsedArguments.timestamp << "' not found for '"
                       << parsedArguments.path.string() << "'.\n";
        return static_cast<int>(ExitCode::InvalidArguments);
    }

    output << snapshot->dump(2) << '\n';
    return static_cast<int>(ExitCode::Success);
}

} // namespace

int runApplication(
    const std::vector<std::string>& arguments,
    std::ostream& output,
    std::ostream& errorOutput)
{
    if (arguments.empty())
    {
        printHelp(output);
        return static_cast<int>(ExitCode::Success);
    }

    const std::string& firstArgument = arguments.front();

    if (firstArgument == "-h" || firstArgument == "--help")
    {
        printHelp(output);
        return static_cast<int>(ExitCode::Success);
    }

    if (firstArgument == "-v" || firstArgument == "--version")
    {
        printVersion(output);
        return static_cast<int>(ExitCode::Success);
    }

    if (firstArgument == "discover")
    {
        return runDiscover(arguments, output, errorOutput);
    }

    if (firstArgument == "parse")
    {
        return runParse(arguments, output, errorOutput);
    }

    if (firstArgument == "analyze")
    {
        return runAnalyze(arguments, output, errorOutput);
    }

    if (firstArgument == "snapshot")
    {
        return runSnapshot(arguments, output, errorOutput);
    }

    errorOutput << "Error [CLI001]: Unknown option '" << firstArgument << "'.\n"
                   "Use 'codeqa --help' for usage.\n";
    return static_cast<int>(ExitCode::InvalidArguments);
}

} // namespace codeqa::cli
