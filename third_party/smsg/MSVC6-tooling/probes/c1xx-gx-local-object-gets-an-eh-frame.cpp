struct D { ~D(); };
int g();
int f() { D d; return g(); }
