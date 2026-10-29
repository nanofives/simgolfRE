#define INCG(v) _asm inc v
int g;
int k;
void h(void)
{
    INCG(g) ; c /* d
    k = 1; /* e */
}
