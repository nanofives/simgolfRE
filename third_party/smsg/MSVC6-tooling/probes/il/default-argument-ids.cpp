



struct C { C(int); ~C(); int v; };
int f(int a, int b = 3);
int h(const C &c = C(1));
int g0(int a) { return a; }
int g1(int a) { return f(a); }
int g2(int a) { return f(a, 3); }
int g3(int a) { return h(); }
int g4(int a) { return h(C(1)); }
int g5(int a) { return a; }






