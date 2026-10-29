#define L1 __asm L1: __asm inc g
int g;
int k;
void h(void)
{
    L1 ; c /* d
    k = 1; /* e */
}
