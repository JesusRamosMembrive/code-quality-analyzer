int computeChecksumA(unsigned char value, unsigned char seed)
{
    unsigned char result = value ^ seed;
    result = static_cast<unsigned char>(result + 1);
    return result;
}

int computeChecksumB(unsigned char value, unsigned char seed)
{
    unsigned char result = value ^ seed;
    result = static_cast<unsigned char>(result + 1);
    return result;
}
