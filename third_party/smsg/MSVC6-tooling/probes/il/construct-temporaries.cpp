





struct P { int x, y, z; };
P mk(int);
struct C { int v; C(int); C(const C &); ~C(); int get() const; C operator+(const C &) const; };
C mkc(int);
struct D { int v; operator int() const; };
D mkd(int);
void takec(C);
void takecr(const C &);
void takeir(const int &);
struct B1 { int b1; virtual void v(); };
struct B2 { int b2; virtual void w(); };
struct DD : B1, B2 { int d; };
void takepmf(int (DD::*)(int));
int t01(int a) { return mk(a).y; }
int t02(int a) { return mkc(a).get(); }
int t03(int a) { takec(C(a)); return a; }
int t04(int a) { takecr(a); return a; }
int t05(int a) { takeir(a + 1); return a; }
int t06(int a) { return mkd(a); }
int t07(int a) { if (mkc(a).get()) return 1; return 0; }
int t08(int a) { const int &r = a + 1; return r; }
int t09(int a) { int *p = new int; delete p; return a; }
int t10(int a) { takepmf((int (DD::*)(int))0); return a; }
int t11(int a) { P p = mk(a); return p.x; }
int t12(int a) { C c(a); C d = c + c; return d.v; }



















































