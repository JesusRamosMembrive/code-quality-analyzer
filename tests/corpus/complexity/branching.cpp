#include <stdexcept>

int classify(int value, bool strict)
{
    if (value < 0 && strict)
    {
        return -1;
    }
    else if (value == 0 || !strict)
    {
        return 0;
    }

    return 1;
}

int scan(const int* values, int count)
{
    int found = 0;

    for (int index = 0; index < count; ++index)
    {
        if (values[index] == 0)
        {
            continue;
        }

        if (values[index] < 0)
        {
            break;
        }

        ++found;
    }

    return found;
}

int describe(int code)
{
    switch (code)
    {
    case 0:
        return 100;
    case 1:
        return 200;
    default:
        return 900;
    }
}

int drain(int remaining)
{
    do
    {
        --remaining;
    } while (remaining > 0);

    return remaining;
}

int guarded(int value)
{
    try
    {
        if (value < 0)
        {
            throw std::runtime_error("negative");
        }

        return value;
    }
    catch (const std::runtime_error&)
    {
        return -1;
    }
}
