#include "codeqa/core/EnumJson.h"

namespace codeqa::core
{

std::string languageToJsonString(Language language)
{
    switch (language)
    {
    case Language::C:
        return "c";
    case Language::Cpp:
        return "cpp";
    case Language::Unknown:
        return "unknown";
    }

    return "unknown";
}

std::optional<Language> languageFromJsonString(const std::string& text)
{
    if (text == "c")
    {
        return Language::C;
    }

    if (text == "cpp")
    {
        return Language::Cpp;
    }

    return std::nullopt;
}

std::string diagnosticSeverityToJsonString(DiagnosticSeverity severity)
{
    switch (severity)
    {
    case DiagnosticSeverity::Error:
        return "error";
    case DiagnosticSeverity::Warning:
        return "warning";
    case DiagnosticSeverity::Information:
        return "information";
    }

    return "error";
}

} // namespace codeqa::core
