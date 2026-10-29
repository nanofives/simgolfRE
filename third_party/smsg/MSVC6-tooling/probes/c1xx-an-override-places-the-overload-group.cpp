struct B { virtual int f(int); };
struct V : B { virtual int g(); virtual int f(int); virtual int h(); virtual int f(char *); };
int B::f(int) { return 0; }
int V::g() { return 1; }
int V::f(int) { return 2; }
int V::h() { return 3; }
int V::f(char *) { return 4; }
V *make() { return new V; }
