template<class T> struct C { T a(T x) { return x + 1; } T b(T x) { return x + 2; } };
int f(int v) { C<int> o; return o.b(v); }
int g(int v) { return v * 7; }
int h(int v) { C<int> o; return o.a(v); }
