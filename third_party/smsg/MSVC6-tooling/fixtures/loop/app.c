extern int classify(int v);
extern int shape_area(int w, int h);

int g_counter;
static const char g_banner[] = "closed loop";

static int banner_sum(void)
{
    const char *p = g_banner;
    int s = 0;
    while (*p)
        s += *p++;
    return s;
}

int tally(int n)
{
    int i;
    int t = 0;
    for (i = 0; i < n; i++)
        t += classify(i);
    g_counter += t;
    return t;
}

int __cdecl entry(void)
{
    return tally(8) + banner_sum() + shape_area(3, 4);
}
