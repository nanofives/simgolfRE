
int value_logic(int a, int b)
{
    int x = a && b;
    int y = a || b;
    int z = !a;
    int w = (a, b);
    return x + y + z + w + (a > b ? a : b);
}







