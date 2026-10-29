


int k00(int a) { return a; }
struct A { int x, y; A(int); int f(int); int g(); static int h(int); virtual int v(int); };
A::A(int q) : x(q), y(0) {}
int A::f(int q) { return x + q; }
int A::g() { return f(1) + this->y; }
int A::h(int q) { return q; }
int A::v(int q) { return q + x; }
int k05(int a) { A o(a); return o.f(a) + o.v(a); }
















