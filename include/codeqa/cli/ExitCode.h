#pragma once

namespace codeqa::cli
{

enum class ExitCode : int
{
    Success = 0,
    InvalidArguments = 2,
    InternalError = 4
};

} // namespace codeqa::cli
