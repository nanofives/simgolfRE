#include <windows.h>
#include "fx.h"

static int g_seed = 7;

int mix(int a, int b)
{
    return a * g_seed + b;
}

#line 300 "C:\\orig\\entry.c"
BOOL WINAPI start(HINSTANCE dll, DWORD why, LPVOID reserved)
{
    int r = sum(6) + mix(2, 3) + shape_area(3, 4) + g_name[0];
    if (why == 99)
        r += (int)GetTickCount();
    return r != 0;
}
