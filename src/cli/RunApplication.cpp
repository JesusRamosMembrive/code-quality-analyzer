#include "codeqa/cli/RunApplication.h"

#include "codeqa/application/ApplicationInfo.h"
#include "codeqa/application/DiscoverProjectSources.h"
#include "codeqa/application/GetApplicationInfo.h"
#include "codeqa/cli/ExitCode.h"
#include "codeqa/infrastructure/DiscoveredFile.h"
#include "codeqa/infrastructure/DiscoveryOptions.h"
#include "codeqa/infrastructure/DiscoveryResult.h"
#include "codeqa/infrastructure/ExtensionCategory.h"

#include <filesystem>
#include <optional>
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
              "  codeqa discover <path> [--exclude <glob>]...\n"
              "\n"
              "Options:\n"
              "  -h, --help       Show this help.\n"
              "  -v, --version    Show application version.\n"
              "\n"
              "Commands:\n"
              "  discover <path>       Discover C/C++ source files under <path>.\n"
              "      --exclude <glob>  Exclude files/directories matching a glob pattern\n"
              "                        (repeatable).\n";
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

int runDiscover(const std::vector<std::string>& arguments, std::ostream& output, std::ostream& errorOutput)
{
    std::optional<std::filesystem::path> path;
    std::vector<std::string> excludePatterns;

    for (std::size_t index = 1; index < arguments.size(); ++index)
    {
        const std::string& token = arguments[index];

        if (token == "--exclude")
        {
            if (index + 1 >= arguments.size())
            {
                errorOutput << "Error [CLI004]: Missing value for '--exclude'.\n";
                return static_cast<int>(ExitCode::InvalidArguments);
            }

            excludePatterns.push_back(arguments[index + 1]);
            ++index;
            continue;
        }

        if (!path.has_value())
        {
            path = std::filesystem::path{token};
            continue;
        }

        errorOutput << "Error [CLI005]: Unexpected argument '" << token << "'.\n";
        return static_cast<int>(ExitCode::InvalidArguments);
    }

    if (!path.has_value())
    {
        errorOutput << "Error [CLI002]: Missing required argument <path> for 'discover'.\n"
                       "Use 'codeqa --help' for usage.\n";
        return static_cast<int>(ExitCode::InvalidArguments);
    }

    std::error_code existsError;
    const bool isDirectory = std::filesystem::is_directory(*path, existsError);

    if (!isDirectory)
    {
        errorOutput << "Error [CLI003]: Path '" << path->string() << "' does not exist or is not a directory.\n";
        return static_cast<int>(ExitCode::InvalidArguments);
    }

    codeqa::infrastructure::DiscoveryOptions options;
    options.rootPath = *path;
    options.excludePatterns = excludePatterns;

    const codeqa::application::DiscoverProjectSources discoverProjectSources;
    const codeqa::infrastructure::DiscoveryResult result = discoverProjectSources.execute(options);

    printDiscoveryResult(output, result);
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

    errorOutput << "Error [CLI001]: Unknown option '" << firstArgument << "'.\n"
                   "Use 'codeqa --help' for usage.\n";
    return static_cast<int>(ExitCode::InvalidArguments);
}

} // namespace codeqa::cli
