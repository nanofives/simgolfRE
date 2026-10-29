struct D { D(); ~D(); int v; };
D a;
D b;
int f() { return a.v + b.v; }
