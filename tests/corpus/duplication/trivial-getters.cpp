// codeqa: documented "borderline" fixture -- these two validators are
// structurally identical because there is essentially one reasonable way to
// write a range check this small, not because of copy-paste. Included to make
// the minNormalizedTokens threshold's false-positive trade-off visible in the
// test suite instead of only in prose (see the duplication design spec, S9).
bool isValidPercentage(int value)
{
    if (value < 0)
    {
        return false;
    }
    if (value > 100)
    {
        return false;
    }
    return true;
}

bool isValidByteValue(int value)
{
    if (value < 0)
    {
        return false;
    }
    if (value > 100)
    {
        return false;
    }
    return true;
}
