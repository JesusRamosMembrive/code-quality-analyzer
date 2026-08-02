#pragma once

namespace codeqa::cli
{

enum class ExitCode : int
{
    /// Returned when the application ran to completion without errors.
    Success = 0,
    /// Returned when the command-line arguments supplied by the user are invalid or unrecognized.
    InvalidArguments = 2,
    /// Returned when an unhandled exception (or other unexpected internal failure) is caught at the top level.
    InternalError = 4
};

} // namespace codeqa::cli
