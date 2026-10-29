


int logic(int a, int b, int c)
{
    int r = 0;
    if (a == b && b != c) r |= 1;
    if (a < b || b > c) r |= 2;
    if (!(a <= c)) r |= 4;
    if (a >= c) r |= 8;
    return r;
}









