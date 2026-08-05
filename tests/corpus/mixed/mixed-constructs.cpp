#include <string>
#include <vector>

namespace app
{

enum class Level
{
    Low,
    High
};

struct Settings
{
    int retries;
    bool verbose;
};

class Engine
{
public:
    explicit Engine(Settings settings)
        : settings_(settings)
    {
    }

    ~Engine() = default;

    bool run(const std::vector<int>& inputs);

    bool operator!() const
    {
        return settings_.retries == 0;
    }

private:
    Settings settings_;
};

bool Engine::run(const std::vector<int>& inputs)
{
    if (inputs.empty())
    {
        return false;
    }

    for (int input : inputs)
    {
        if (input < 0 && settings_.verbose)
        {
            return false;
        }
    }

    return true;
}

template <typename T>
bool isEmpty(const T& container)
{
    return container.size() == 0;
}

} // namespace app
