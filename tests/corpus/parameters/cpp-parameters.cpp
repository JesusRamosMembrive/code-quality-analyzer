#include <string>

int withDefaults(int required, int optional = 5, bool verbose = false)
{
    return verbose ? required + optional : required;
}

void byReference(std::string& target, const std::string& source)
{
    target = source;
}

void byPointer(int* output, const int* input)
{
    *output = *input;
}

int ignoresArgument(int)
{
    return 0;
}

int manyParameters(int first, int second, int third, int fourth, int fifth)
{
    return first + second + third + fourth + fifth;
}
