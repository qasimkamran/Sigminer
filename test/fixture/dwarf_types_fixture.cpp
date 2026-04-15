#include <cstdarg>

enum class FixtureColor : unsigned int
{
    Red = 1,
    Blue = 2,
};

struct FixturePoint
{
    int x;
    double y;
};

using WrappedUnsignedInt = const volatile unsigned int;
using PointAlias = FixturePoint;
using Callback = int (*)(int);

namespace fixture_ns {

__attribute__((noinline, visibility("default")))
int NestedFunction(int value)
{
    return value + 1;
}

} // namespace fixture_ns

extern "C" {

__attribute__((noinline, visibility("default")))
unsigned int TakesWrappedUnsignedInt(WrappedUnsignedInt value)
{
    return value + 1;
}

__attribute__((noinline, visibility("default")))
FixturePoint* TakesPointPointer(PointAlias* point)
{
    return point;
}

__attribute__((noinline, visibility("default")))
FixtureColor TakesEnum(FixtureColor color)
{
    return color;
}

__attribute__((noinline, visibility("default")))
bool MixedTypes(int count, const char* label, double ratio, FixturePoint value)
{
    return count > 0 && label != nullptr && ratio > 0.0 && value.x >= 0;
}

__attribute__((noinline, visibility("default")))
int VarArgFixture(int count, ...)
{
    va_list args;
    va_start(args, count);

    int total = count;
    for (int i = 0; i < count; ++i)
        total += va_arg(args, int);

    va_end(args);
    return total;
}

__attribute__((noinline, visibility("default")))
Callback TakesCallback(Callback callback)
{
    return callback;
}

} // extern "C"
