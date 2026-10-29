#ifdef NDEBUG
#define DBGBREAK()
#else
#define DBGBREAK() __asm int 3
#endif
int g;
void h(int bad)
{
    if (bad) DBGBREAK();
    g = 1;
}
