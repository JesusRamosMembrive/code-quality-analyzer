#include <stddef.h>

int globalCounter = 0;

struct Point
{
    int x;
    int y;
};

union Value
{
    int asInteger;
    float asFloat;
};

enum Status
{
    StatusIdle,
    StatusRunning,
    StatusStopped
};

typedef struct Point PointAlias;

size_t pointSize(void)
{
    return sizeof(PointAlias);
}

#include "local-config.h"
