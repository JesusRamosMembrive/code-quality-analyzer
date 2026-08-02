#include "codeqa/cli/RunApplication.h"

#include "codeqa/cli/ExitCode.h"

#include <exception>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    const std::vector<std::string> arguments(argv + 1, argv + argc);

    try
    {
        return codeqa::cli::runApplication(arguments, std::cout, std::cerr);
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Internal error: " << exception.what() << '\n';
        return static_cast<int>(codeqa::cli::ExitCode::InternalError);
    }
    catch (...)
    {
        std::cerr << "Internal error: unknown exception.\n";
        return static_cast<int>(codeqa::cli::ExitCode::InternalError);
    }
}
