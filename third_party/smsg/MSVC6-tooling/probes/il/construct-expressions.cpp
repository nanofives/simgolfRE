



int g(int);
int g2(int, int);
struct S { int a; short b; S *n; int m(int); };
S gs;
S *gp;
int arr[8];
int e00(int a, int b) { return a; }
int e01(int a, int b) { return a ? b : a; }
int e02(int a, int b) { return a && b; }
int e03(int a, int b) { return a || b; }
int e04(int a, int b) { return !a; }
int e05(int a, int b) { return a < b; }
int e06(int a, int b) { return a += b; }
int e07(int a, int b) { return ++a; }
int e08(int a, int b) { return a++; }
int e09(int a, int b) { return g(a); }
int e10(int a, int b) { return g(a + b); }
int e11(int a, int b) { return g2(a, b); }
int e12(int a, int b) { return gs.b; }
int e13(int a, int b) { return gp->n->a; }
int e14(int a, int b) { return arr[a]; }
int e15(int a, int b) { return (a, b); }
int e16(int a, int b) { return (short)a; }
int e17(int a, int b) { return short(a); }
int e18(int a, int b) { return static_cast<short>(a); }
int e19(int a, int b) { return sizeof(S); }
int e20(int a, int b) { return sizeof a; }
int e21(int a, int b) { return gs.m(a); }
int e22(int a, int b) { return 2 + 3 + a; }
int e23(int a, int b) { return a + 2 + 3; }
int e24(int a, int b) { return b + a; }
int e25(int a, int b) { return *&a; }
int e26(int a, int b) { return -a; }
























































