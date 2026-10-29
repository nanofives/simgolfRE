#ifdef _DEBUG
#define DBGBREAK() __asm int 3
#else
#define DBGBREAK()
#endif
int g;
int k;
void h(int bad)
{
    if (bad) DBGBREAK(); k = 2; /* why */
    g = 1;
}
