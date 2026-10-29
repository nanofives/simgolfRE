static int g_table[8] = { 2, 3, 5, 7, 11, 13, 17, 19 };
static const char *g_names[4] = { "north", "east", "south", "west" };

__declspec(dllexport) int __stdcall lib_prime(int i)
{
    return g_table[i & 7];
}

__declspec(dllexport) int __stdcall lib_name_len(int d)
{
    const char *p = g_names[d & 3];
    int n = 0;
    while (p[n])
        n++;
    return n;
}

static int step(int v)
{
    switch (v % 6)
    {
    case 0:
        return v + 1;
    case 1:
        return v * 3;
    case 2:
        return v - 7;
    case 3:
        return v / 2;
    case 4:
        return v ^ 0x55;
    default:
        return -v;
    }
}

__declspec(dllexport) int __cdecl lib_walk(int v, int n)
{
    while (n-- > 0)
        v = step(v);
    return v;
}
