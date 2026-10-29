




int g(int);
int s00(int a) { return a; }
int s01(int a) { static int s; return s; }
int s02(int a) { static int s = 5; return s; }
int s03(int a) { static int s = g(a); return s; }
int s04(int a) { static int s = g(a); static int t = g(s); return s + t; }
static int fs;
int s05(int a) { return fs; }
int s06(int a) { return a; }
















