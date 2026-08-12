#include "codeqa/cli/RunApplication.h"

#include "codeqa/application/ApplicationInfo.h"
#include "codeqa/core/EnumJson.h"
#include "codeqa/application/DetectFileLanguage.h"
#include "codeqa/application/DiscoverProjectSources.h"
#include "codeqa/application/GetApplicationInfo.h"
#include "codeqa/application/LoadProjectConfig.h"
#include "codeqa/application/ListSnapshots.h"
#include "codeqa/application/LoadSnapshot.h"
#include "codeqa/application/SaveSnapshot.h"
#include "codeqa/application/RunAnalysis.h"
#include "codeqa/cli/ExitCode.h"
#include "codeqa/config/ConfigModels.h"
#include "codeqa/config/DefaultConfig.h"
#include "codeqa/core/Diagnostic.h"
#include "codeqa/core/DiagnosticSeverity.h"
#include "codeqa/core/Language.h"
#include "codeqa/core/LanguageDetectionResult.h"
#include "codeqa/frontend/Grammar.h"
#include "codeqa/frontend/ParseSourceFile.h"
#include "codeqa/frontend/ParsedFile.h"
#include "codeqa/frontend/SelectGrammar.h"
#include "codeqa/frontend/StructuralExtractor.h"
#include "codeqa/frontend/TreeSitterParser.h"
#include "codeqa/history/CompareSnapshots.h"
#include "codeqa/infrastructure/DiscoveredFile.h"
#include "codeqa/infrastructure/DiscoveryOptions.h"
#include "codeqa/infrastructure/DiscoveryResult.h"
#include "codeqa/infrastructure/ExtensionCategory.h"
#include "codeqa/report/ReportJson.h"
#include "codeqa/report/TextReport.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <system_error>

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
              "                        [--config <path>] [--format text|json] [--output <file>]\n"
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
              "                                      automatically.\n"
              "      --config <path>                 Merge rule/language overrides from a\n"
              "                                       codeqa.json file over the defaults.\n"
              "      --format text|json               Report format (default: text).\n"
              "      --output <file>                  Write the report to a file instead of\n"
              "                                        stdout.\n";
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

        if (token == "--exclude")
        {
            if (index + 1 >= arguments.size())
            {
                errorOutput << "Error [CLI004]: Missing value for '--exclude'.\n";
                return DiscoverArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
            }

            excludePatterns.push_back(arguments[index + 1]);
            ++index;
            continue;
        }

        if (token == "--lang-override")
        {
            if (index + 1 >= arguments.size())
            {
                errorOutput << "Error [CLI006]: Missing value for '--lang-override'.\n";
                return DiscoverArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
            }

            const std::string& value = arguments[index + 1];
            const std::size_t equalsPosition = value.find('=');

            if (equalsPosition == std::string::npos || equalsPosition == 0
                || equalsPosition == value.size() - 1)
            {
                errorOutput << "Error [CLI007]: Invalid --lang-override value '" << value
                               << "'. Expected <pattern>=c|cpp.\n";
                return DiscoverArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
            }

            const std::string pattern = value.substr(0, equalsPosition);
            const std::string languageText = value.substr(equalsPosition + 1);
            const std::optional<codeqa::core::Language> language = parseLanguageName(languageText);

            if (!language.has_value())
            {
                errorOutput << "Error [CLI007]: Invalid --lang-override value '" << value
                               << "'. Expected <pattern>=c|cpp.\n";
                return DiscoverArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
            }

            languageOverrides.push_back(codeqa::application::LanguageOverride{pattern, *language});
            ++index;
            continue;
        }

        if (!path.has_value())
        {
            path = std::filesystem::path{token};
            continue;
        }

        errorOutput << "Error [CLI005]: Unexpected argument '" << token << "'.\n";
        return DiscoverArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
    }

    if (!path.has_value())
    {
        errorOutput << "Error [CLI002]: Missing required argument <path> for '" << subcommandName << "'.\n"
                       "Use 'codeqa --help' for usage.\n";
        return DiscoverArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
    }

    std::error_code existsError;
    const bool isDirectory = std::filesystem::is_directory(*path, existsError);

    if (!isDirectory)
    {
        errorOutput << "Error [CLI003]: Path '" << path->string() << "' does not exist or is not a directory.\n";
        return DiscoverArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
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

    const codeqa::application::DiscoverProjectSources discoverProjectSources;
    const codeqa::infrastructure::DiscoveryResult result = discoverProjectSources.execute(options);

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

    const codeqa::application::DiscoverProjectSources discoverProjectSources;
    const codeqa::infrastructure::DiscoveryResult discoveryResult = discoverProjectSources.execute(options);

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

        if (token == "--exclude")
        {
            if (index + 1 >= arguments.size())
            {
                errorOutput << "Error [CLI004]: Missing value for '--exclude'.\n";
                return AnalyzeArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
            }

            excludePatterns.push_back(arguments[index + 1]);
            ++index;
            continue;
        }

        if (token == "--lang-override")
        {
            if (index + 1 >= arguments.size())
            {
                errorOutput << "Error [CLI006]: Missing value for '--lang-override'.\n";
                return AnalyzeArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
            }

            const std::string& value = arguments[index + 1];
            const std::size_t equalsPosition = value.find('=');

            if (equalsPosition == std::string::npos || equalsPosition == 0
                || equalsPosition == value.size() - 1)
            {
                errorOutput << "Error [CLI007]: Invalid --lang-override value '" << value
                               << "'. Expected <pattern>=c|cpp.\n";
                return AnalyzeArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
            }

            const std::string pattern = value.substr(0, equalsPosition);
            const std::string languageText = value.substr(equalsPosition + 1);
            const std::optional<codeqa::core::Language> language = parseLanguageName(languageText);

            if (!language.has_value())
            {
                errorOutput << "Error [CLI007]: Invalid --lang-override value '" << value
                               << "'. Expected <pattern>=c|cpp.\n";
                return AnalyzeArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
            }

            languageOverrides.push_back(codeqa::application::LanguageOverride{pattern, *language});
            ++index;
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

            if (value != "text" && value != "json")
            {
                errorOutput << "Error [CLI010]: Invalid --format value '" << value
                               << "'. Expected 'text' or 'json'.\n";
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

        errorOutput << "Error [CLI005]: Unexpected argument '" << token << "'.\n";
        return AnalyzeArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
    }

    if (!path.has_value())
    {
        errorOutput << "Error [CLI002]: Missing required argument <path> for 'analyze'.\n"
                       "Use 'codeqa --help' for usage.\n";
        return AnalyzeArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
    }

    std::error_code existsError;
    const bool isDirectory = std::filesystem::is_directory(*path, existsError);

    if (!isDirectory)
    {
        errorOutput << "Error [CLI003]: Path '" << path->string() << "' does not exist or is not a directory.\n";
        return AnalyzeArgumentsOutcome{std::nullopt, static_cast<int>(ExitCode::InvalidArguments)};
    }

    return AnalyzeArgumentsOutcome{
        AnalyzeArguments{*path, excludePatterns, languageOverrides, configPath, format, outputPath},
        static_cast<int>(ExitCode::Success)};
}

codeqa::application::LanguageOverride toLanguageOverride(const codeqa::config::HeaderOverrideConfig& headerOverride)
{
    return codeqa::application::LanguageOverride{headerOverride.pattern, headerOverride.language};
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
            return static_cast<int>(ExitCode::InvalidArguments);
        }

        config = *configResult.config;
    }

    codeqa::application::RunAnalysisRequest request;
    request.analysis.discovery.rootPath = parsedArguments.path;
    request.analysis.discovery.excludePatterns = parsedArguments.excludePatterns;

    for (const codeqa::config::HeaderOverrideConfig& headerOverride : config.headerOverrides)
    {
        request.analysis.languageOverrides.push_back(toLanguageOverride(headerOverride));
    }

    for (const codeqa::application::LanguageOverride& languageOverride : parsedArguments.languageOverrides)
    {
        request.analysis.languageOverrides.push_back(languageOverride);
    }

    request.config = config;

    const codeqa::application::RunAnalysisResult result = codeqa::application::RunAnalysis{}.execute(request);

    const codeqa::application::GetApplicationInfo getApplicationInfo;
    const std::string toolVersion = getApplicationInfo.execute().version;
    const nlohmann::json reportJson =
        codeqa::report::toReportJson(result.model, result.metrics, result.findings, config, toolVersion);

    std::ofstream outputFile;
    std::ostream* destination = &output;

    if (parsedArguments.outputPath.has_value())
    {
        outputFile.open(*parsedArguments.outputPath);

        if (!outputFile)
        {
            errorOutput << "Error [CLI012]: Could not open '" << parsedArguments.outputPath->string()
                           << "' for writing.\n";
            return static_cast<int>(ExitCode::InvalidArguments);
        }

        destination = &outputFile;
    }

    const std::vector<std::string> previousSnapshots =
        codeqa::application::ListSnapshots{}.execute(parsedArguments.path);
    std::optional<codeqa::history::SnapshotComparison> comparison;
    std::string previousTimestamp;

    if (!previousSnapshots.empty())
    {
        previousTimestamp = previousSnapshots.back();
        const std::optional<nlohmann::json> previousReport =
            codeqa::application::LoadSnapshot{}.execute(parsedArguments.path, previousTimestamp);

        if (!previousReport.has_value())
        {
            errorOutput << "Warning: previous snapshot '" << previousTimestamp
                           << "' could not be read; skipping comparison.\n";
        }
        else
        {
            try
            {
                comparison = codeqa::history::compareSnapshots(
                    codeqa::history::extractFindingKeys(result.findings),
                    codeqa::history::extractFindingKeys(*previousReport));
            }
            catch (const nlohmann::json::exception&)
            {
                errorOutput << "Warning: previous snapshot '" << previousTimestamp
                               << "' has an unexpected shape; skipping comparison.\n";
            }
        }
    }

    if (parsedArguments.format == "json")
    {
        nlohmann::json displayJson = reportJson;

        if (comparison.has_value())
        {
            displayJson["comparison"] = {
                {"previousTimestamp", previousTimestamp},
                {"new", comparison->newCount},
                {"existing", comparison->existingCount},
                {"resolved", comparison->resolvedCount}};
        }

        *destination << displayJson.dump(2) << '\n';
    }
    else
    {
        codeqa::report::renderText(*destination, result.model, result.metrics, result.findings);

        if (comparison.has_value())
        {
            *destination << "\nFindings: " << comparison->newCount << " new, " << comparison->existingCount
                         << " existing, " << comparison->resolvedCount << " resolved.\n";
        }
    }

    if (!codeqa::application::SaveSnapshot{}.execute(parsedArguments.path, reportJson).has_value())
    {
        errorOutput << "Warning: could not save snapshot.\n";
    }

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

    errorOutput << "Error [CLI001]: Unknown option '" << firstArgument << "'.\n"
                   "Use 'codeqa --help' for usage.\n";
    return static_cast<int>(ExitCode::InvalidArguments);
}

} // namespace codeqa::cli
