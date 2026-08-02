#pragma once

#include "codeqa/core/DiagnosticSeverity.h"
#include "codeqa/core/SourceLocation.h"

#include <string>

namespace codeqa::core
{

struct Diagnostic
{
    std::string code;
    DiagnosticSeverity severity{DiagnosticSeverity::Error};
    std::string message;
    SourceLocation location;
};

} // namespace codeqa::core
