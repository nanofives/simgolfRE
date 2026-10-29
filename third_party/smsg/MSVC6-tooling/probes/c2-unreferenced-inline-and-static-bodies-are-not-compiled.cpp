inline int k(int a) { return a + 1; }
inline int u(int a) { return a + 2; }
static int s1(int a) { return a + 3; }
static int s2(int a) { return a + 4; }
int f(int a) { return k(a) + s1(a); }
