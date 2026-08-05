int flat(int value)
{
    return value;
}

int oneLevel(int value)
{
    if (value > 0)
    {
        return 1;
    }

    return 0;
}

int fourLevels(const int* values, int count)
{
    int total = 0;

    while (count > 0)
    {
        for (int index = 0; index < count; ++index)
        {
            if (values[index] > 0)
            {
                if (values[index] % 2 == 0)
                {
                    total += values[index];
                }
            }
        }

        --count;
    }

    return total;
}
