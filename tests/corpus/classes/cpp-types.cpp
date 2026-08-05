#include <cstddef>

namespace outer
{

namespace
{

int internalHelper()
{
    return 1;
}

} // namespace

enum class Mode
{
    Fast,
    Slow
};

struct Config
{
    int retries;
};

class Container
{
public:
    class Iterator
    {
    public:
        int position() const
        {
            return position_;
        }

    private:
        int position_;
    };

    std::size_t size() const
    {
        return size_;
    }

private:
    std::size_t size_;
};

template <typename T>
class Box
{
public:
    explicit Box(T content)
        : content_(content)
    {
    }

private:
    T content_;
};

} // namespace outer

enum Color
{
    Red,
    Green,
    Blue
};

class Base
{
};

class Derived : public Base
{
};
