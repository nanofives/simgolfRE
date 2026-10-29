#define WRAP(x) x
int g;
int k;
void h(void)
{
    WRAP(__asm inc g) ; tail /* x
    k = 1; /* y */
}
