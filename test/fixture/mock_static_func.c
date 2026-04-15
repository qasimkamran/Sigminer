#include <stdint.h>

static int MyStaticFunc(int value)
{
    return value;
}

int main(void)
{
    return MyStaticFunc(42);
}
