#include <stdarg.h>

int declaredOnly(int value);

static int helper(int value)
{
    return value * 2;
}

int sumAll(int count, ...)
{
    va_list args;
    va_start(args, count);

    int total = 0;

    for (int index = 0; index < count; ++index)
    {
        total += va_arg(args, int);
    }

    va_end(args);
    return total;
}

int applyTwice(int value, int (*operation)(int))
{
    return operation(operation(value));
}

int findFirstNegative(const int* values, int count)
{
    int index = 0;

search:
    if (index >= count)
    {
        return -1;
    }

    if (values[index] < 0)
    {
        return index;
    }

    ++index;
    goto search;
}
