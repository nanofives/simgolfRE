void g(int *);
int f(void)
{
    int a, b, c, d, e, h;
    g(&a); g(&b); g(&c); g(&d); g(&e); g(&h);
    return a + b + c + d + e + h;
}
