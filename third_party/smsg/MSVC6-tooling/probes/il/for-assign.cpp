
int for_assign(int n)
{
    int i, s = 0;
    for (i = 0; i < n; i++)
        s += i;
    while (s > 100)
        s -= 3;
    do { s++; } while (s < 10);
    return s;
}












