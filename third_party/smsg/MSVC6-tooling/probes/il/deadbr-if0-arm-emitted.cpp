



void Use(int); extern int h;
void f(int x)
{
    if (0) Use(x);
    const int k = 0;
    if (k) h = x;
    goto skip;
    Use(x + 1);
skip:
    h = 0;
}











