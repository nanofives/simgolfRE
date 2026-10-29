void g(int *);
int f()
{
    int value, a;
    g(&a); g(&value);
    return a + value;
}
