


struct S {
    int a;
    short f;
    char c;
};

int member_read(S s, S *p)
{
    return s.f + p->c;
}













