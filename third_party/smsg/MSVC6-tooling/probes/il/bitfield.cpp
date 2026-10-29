
struct Bits {
    unsigned a : 3;
    unsigned b : 5;
    int c : 4;
};

int bitfield(Bits *p, int v)
{
    p->b = v;
    return p->a + p->c;
}



