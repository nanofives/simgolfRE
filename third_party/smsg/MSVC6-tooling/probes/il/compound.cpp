
int compound(int a, int b)
{
    a += b; a -= 2; a *= b; a /= 3; a %= b; a <<= 1; a >>= b; a &= 0xff; a ^= b; a |= 4;
    int x = a++;
    int y = ++a;
    int z = b--;
    int w = --b;
    return x + y + z + w;
}















