#define WRAP(x) x
int g;
int k;
void h(void)
{
    WRAP(__asm inc g)
    k = 1; /* y */ k = 3;
}
