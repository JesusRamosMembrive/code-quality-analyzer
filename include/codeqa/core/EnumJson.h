#pragma once

#include "codeqa/core/DiagnosticSeverity.h"
#include "codeqa/core/Language.h"

#include <optional>
#include <string>

namespace codeqa::core
{

[[nodiscard]] std::string languageToJsonString(Language language);
[[nodiscard]] std::optional<Language> languageFromJsonString(const std::string& text);
[[nodiscard]] std::string diagnosticSeverityToJsonString(DiagnosticSeverity severity);
[[nodiscard]] std::optional<DiagnosticSeverity> diagnosticSeverityFromJsonString(const std::string& text);

} // namespace codeqa::core
