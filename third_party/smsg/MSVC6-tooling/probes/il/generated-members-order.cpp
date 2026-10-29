


struct B1 { int b1; virtual int f(); };
struct B2 { int b2; virtual int g(); };
struct D : B1, B2 { int d; int g(); int f(); };
struct VB { int vb; virtual int h(); };
struct E : virtual VB { int e; int h(); };
int D::g() { return d; }
int mk() { D d; E e; return d.d + e.e; }
int last(int a) { return a; }








