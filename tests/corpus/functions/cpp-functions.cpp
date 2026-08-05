#include <algorithm>
#include <vector>

class Counter
{
public:
    Counter()
        : value_(0)
    {
    }

    ~Counter()
    {
        value_ = 0;
    }

    void increment()
    {
        ++value_;
    }

    int value() const;

    bool operator==(const Counter& other) const
    {
        return value_ == other.value_;
    }

private:
    int value_;
};

int Counter::value() const
{
    return value_;
}

int countPositive(const std::vector<int>& values)
{
    return static_cast<int>(std::count_if(values.begin(), values.end(), [](int value)
    {
        return value > 0;
    }));
}

template <typename T>
T largest(T first, T second)
{
    return first < second ? second : first;
}
