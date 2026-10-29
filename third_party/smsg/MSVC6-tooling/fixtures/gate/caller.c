extern int g_a;
extern int g_b;
int __cdecl right(void);
int __cdecl wrong(void);

int __cdecl f(void)
{
    g_a = 3;
    return right();
}

int __cdecl entry(void)
{
    return f() + wrong() + g_b;
}
