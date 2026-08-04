#include "codeqa/infrastructure/GlobMatcher.h"

#include <regex>

namespace codeqa::infrastructure
{

namespace
{

std::string translateGlobToRegex(const std::string& pattern)
{
    static const std::string kRegexSpecialChars = R"(\.^$|()[]{}+)";

    std::string regexPattern;
    regexPattern.reserve(pattern.size() * 2);

    for (const char character : pattern)
    {
        if (character == '*')
        {
            regexPattern += "[^/]*";
        }
        else if (character == '?')
        {
            regexPattern += "[^/]";
        }
        else if (kRegexSpecialChars.find(character) != std::string::npos)
        {
            regexPattern += '\\';
            regexPattern += character;
        }
        else
        {
            regexPattern += character;
        }
    }

    return regexPattern;
}

} // namespace

bool matchesExcludePattern(const std::string& pattern, const std::filesystem::path& relativePath)
{
    const bool matchWholePath = pattern.find('/') != std::string::npos;
    const std::string target = matchWholePath
        ? relativePath.generic_string()
        : relativePath.filename().generic_string();

    try
    {
        const std::regex compiledPattern("^" + translateGlobToRegex(pattern) + "$");

        return std::regex_match(target, compiledPattern);
    }
    catch (const std::regex_error&)
    {
        return false;
    }
}

} // namespace codeqa::infrastructure
