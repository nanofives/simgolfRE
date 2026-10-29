struct D { ~D(); };
int g();
extern "C" int f() { D d; return g(); }
