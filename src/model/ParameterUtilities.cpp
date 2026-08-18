#include "codeqa/model/ParameterUtilities.h"

#include <sstream>
#include <string>
#include <vector>

namespace codeqa::model
{

bool isBooleanValueParameter(const ParameterModel& parameter)
{
    if (parameter.passing != ParameterPassing::Value)
    {
        return false;
    }

    std::istringstream stream(parameter.declaredType);
    std::vector<std::string> tokens;
    std::string token;

    while (stream >> token)
    {
        if (token != "const" && token != "volatile")
        {
            tokens.push_back(token);
        }
    }

    return tokens.size() == 1 && (tokens[0] == "bool" || tokens[0] == "_Bool");
}

} // namespace codeqa::model
