void F(int);
int g;
int k;
#define F(x) G(x); __asm
#define G(x) F(x)
void h(void)
{
    F(1) inc g ; c /* d
    k = 1; /* e */
}
