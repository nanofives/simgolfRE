#define KW(a, b) a##b
int g;
int k;
void h(void)
{
    KW(__, asm) inc g ; tail /* x
    k = 1; /* y */
}
