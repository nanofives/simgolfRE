struct B { virtual int v(); };
int B::v() { return 1; }
int f(B *p) { B b; return b.v() + p->v(); }
