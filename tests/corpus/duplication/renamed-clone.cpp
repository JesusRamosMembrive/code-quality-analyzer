int mergeBuffersLeft(int firstLength, int secondLength)
{
    int combinedLength = firstLength + secondLength;
    if (combinedLength > 256)
    {
        combinedLength = 256;
    }
    return combinedLength;
}

int mergeBuffersRight(int leftSize, int rightSize)
{
    int totalSize = leftSize + rightSize;
    if (totalSize > 512)
    {
        totalSize = 512;
    }
    return totalSize;
}
