









int f1();
int st(int a) { static int x = f1(); static int y = f1(); return x + y; }
int st2(int a) { static int z = f1(); return z; }










