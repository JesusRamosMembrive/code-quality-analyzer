int manyBranches(int value)
{
    int total = 0;

    if (value == 0)
    {
        total += 1;
    }
    if (value == 1)
    {
        total += 1;
    }
    if (value == 2)
    {
        total += 1;
    }
    if (value == 3)
    {
        total += 1;
    }
    if (value == 4)
    {
        total += 1;
    }
    if (value == 5)
    {
        total += 1;
    }
    if (value == 6)
    {
        total += 1;
    }
    if (value == 7)
    {
        total += 1;
    }
    if (value == 8)
    {
        total += 1;
    }

    return total;
}

int earlyReturns(int value)
{
    if (value == 0)
    {
        return 0;
    }

    if (value == 1)
    {
        return 1;
    }

    if (value == 2)
    {
        return 2;
    }

    return -1;
}

bool inRange(int value, int lower, int upper)
{
    if (value >= lower && value <= upper && value != 0 && upper != lower)
    {
        return true;
    }

    return false;
}
