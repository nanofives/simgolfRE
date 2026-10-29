








int use(const char *);
int l00(int a) { return use("abc"); }
int l01(int a) { use("abc"); use("abc"); return a; }
int l02(int a) { if (use("abc")) return 1; return 0; }
int l03(int a) { int x = use("abc"); int y = a; return x + y; }
int l04(int a) { while (use("abc")) a--; return a; }




























