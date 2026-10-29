
int f(void);

int statics(void)
{
    static int s_const = 3;
    static int s_dyn = f();
    return s_const + s_dyn;
}










