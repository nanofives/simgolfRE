



struct C { int v; C(int); ~C(); };
struct E { int e; };
int g(int);
int x00(int a) { return a; }
int x01(int a) { C c(a); return a; }
int x02(int a) { C c(a); C d(a); return g(a); }
int x03(int a) { try { g(a); } catch (...) { return 1; } return 0; }
int x04(int a) { try { g(a); } catch (E) { return 1; } return 0; }
int x05(int a) { try { g(a); } catch (E &e) { return e.e; } return 0; }
int x06(int a) { if (a) throw E(); return 0; }
int x07(int a) { if (a) throw 1; return 0; }
int x08(int a) { return a; }































