
#include <excpt.h>
int risky(int);

int seh(int a)
{
    int r = 0;
    __try {
        r = risky(a);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        r = -1;
    }
    __try {
        if (a > 5)
            __leave;
        r += risky(a + 1);
    } __finally {
        r += 2;
    }
    return r;
}













