
struct P { int x, y, z; };
P make(int v);

int structs(P a, int v)
{
    P b = a;
    b = make(v);
    return b.y + make(v + 1).z;
}







