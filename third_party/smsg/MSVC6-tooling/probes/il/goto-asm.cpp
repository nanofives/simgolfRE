

#include <string.h>

int goto_asm(int a, char *d, const char *s)
{
    if (a < 0)
        goto out;
    __asm mov eax, a
    __assume(a >= 0);
    memcpy(d, s, 4);
    a = (int)strlen(s);
out:
    return a;
}










