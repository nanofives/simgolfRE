struct C { int a(int x) { return x + 1; } int b(int x) { return x + 2; } };
int f(int v) { C o; return o.b(v); }
int g(int v) { return v * 7; }
int h(int v) { C o; return o.a(v); }
