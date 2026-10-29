



int use(const char *);
int l00(int a) { return use("abc"); }
int l01(int a) { use("abc"); use("abc"); return a; }
int l02(int a) { use("xyz"); return a; }





