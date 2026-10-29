struct D { D(); ~D(); int v; };
struct H { static int m; int k; };
D a;
D b;
int f() { return a.v + b.v; }
