#include "codeqa/cli/RunApplication.h"

#include "codeqa/application/ApplicationInfo.h"
#include "codeqa/application/GetApplicationInfo.h"
#include "codeqa/cli/ExitCode.h"

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
              "\n"
              "Options:\n"
              "  -h, --help       Show this help.\n"
              "  -v, --version    Show application version.\n";
}

void printVersion(std::ostream& output)
{
    const codeqa::application::GetApplicationInfo getApplicationInfo;
    const codeqa::application::ApplicationInfo info = getApplicationInfo.execute();

    output << info.name << ' ' << info.version << '\n';
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

    errorOutput << "Error [CLI001]: Unknown option '" << firstArgument << "'.\n"
                   "Use 'codeqa --help' for usage.\n";
    return static_cast<int>(ExitCode::InvalidArguments);
}

} // namespace codeqa::cli
