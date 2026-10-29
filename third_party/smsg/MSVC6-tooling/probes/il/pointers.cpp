
int pointers(int *p, int *q, int i)
{
    int x = 1;
    int *r = &x;
    *p = 3;
    p[i] = 4;
    *(p + 2) = 5;
    return *r + p[1] + (int)(q - p);
}








