#pragma once

namespace codeqa::cli
{

enum class ExitCode : int
{
    /// Returned when the application ran to completion without a failed quality gate.
    Success = 0,
    /// Returned when analyze completed but the quality gate failed (new findings at/above the
    /// configured severity, or -- with no previous snapshot -- any qualifying finding at all).
    QualityGateFailed = 1,
    /// Returned when a --config file could not be read or failed to parse/validate.
    ConfigurationError = 2,
    /// Returned when the command-line arguments supplied by the user are invalid or unrecognized.
    InvalidArguments = 3,
    /// Returned when an unhandled exception (or other unexpected internal failure) is caught at the top level.
    InternalError = 4
};

} // namespace codeqa::cli
