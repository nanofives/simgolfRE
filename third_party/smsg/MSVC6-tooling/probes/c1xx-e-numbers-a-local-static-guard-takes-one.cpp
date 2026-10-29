struct D { D(); ~D(); int v; };
int g();
D a;
int f1() { static int s = g(); return s; }
D b;
int f() { return a.v + b.v; }
