void g(int *);
int f(void)
{
    int count, total, index, value, width, height;
    g(&count); g(&total); g(&index); g(&value); g(&width); g(&height);
    return count + total + index + value + width + height;
}
