
#include <setjmp.h>
#include <malloc.h>
struct M { int a, b; int f(int); };
jmp_buf jb;

int fnptr(int (*fp)(int), int M::*mp, M *m, int (M::*mf)(int))
{
    char *buf = (char *)_alloca(16);
    buf[0] = (char)fp(1);
    if (setjmp(jb))
        return 0;
    return m->*mp + (m->*mf)(2) + buf[0];
}






