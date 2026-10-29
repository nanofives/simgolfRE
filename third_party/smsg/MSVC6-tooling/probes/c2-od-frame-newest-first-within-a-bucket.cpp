void g(int *);
int f()
{
    int a, value;
    g(&a); g(&value);
    return a + value;
}
