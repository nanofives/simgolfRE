




int g(int);
int s00(int a, int b) { return a; }
int s01(int a, int b) { int x = a; return x; }
int s02(int a, int b) { if (a) return b; return a; }
int s03(int a, int b) { if (a) b = 1; else b = 2; return b; }
int s04(int a, int b) { while (a) a--; return a; }
int s05(int a, int b) { for (b = 0; b < a; b++) g(b); return b; }
int s06(int a, int b) { do a--; while (a); return a; }
int s07(int a, int b) { switch (a) { case 1: return 2; case 2: return 3; } return 0; }
int s08(int a, int b) { switch (a) { case 1: case 2: return 3; default: break; } return 0; }
int s09(int a, int b) { goto L; L: return a; }
int s10(int a, int b) { if (a && b) return 1; return 0; }
int s11(int a, int b) { if (a || b) return 1; return 0; }
int s12(int a, int b) { for (;;) { if (a) break; a++; } return a; }
int s13(int a, int b) { while (a) { if (b) continue; a--; } return a; }
int s14(int a, int b) { { int x = a; b = x; } return b; }
int s15(int a, int b) { return 0; }


















































