struct D { D(); ~D(); int v; };
struct H { static D m; int k; };
D a;
D b;
int f() { return a.v + b.v; }
