












int g(int);
int w00(int a, int b) { if (a) b = 1; return b; }
int w01(int a, int b) { if (a) { b = 1; } return b; }
int w02(int a, int b) { while (a) a--; return a; }
int w03(int a, int b) { while (a) g(a); return a; }
int w04(int a, int b) { while (a) { a--; } return a; }
int w05(int a, int b) { for (b = 0; b < a; b++) b++; return b; }
int w06(int a, int b) { for (int i = 0; i < a; i++) b++; return b; }
int w07(int a, int b) { do a--; while (a); return a; }
int w08(int a, int b) { do { a--; } while (a); return a; }
int w09(int a, int b) { if (a) return g(b); return a; }
int w10(int a, int b) { g(a); return a; }
int w11(int a, int b) { a--; return a; }













