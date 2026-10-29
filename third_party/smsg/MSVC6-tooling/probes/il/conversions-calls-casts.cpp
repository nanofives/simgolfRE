









char gc; short gs; int gi; unsigned gu; float gf; double gd; __int64 gq; bool gb;
enum E { E0, E1 };
char *gp; int *gpi;
void tf(float); void td(double); void tc(char); void ts(short); void ti(int); void tu(unsigned); void tb(bool);
void tv(int, ...);
void c01() { tf(gi); }
void c02() { td(gf); }
void c03() { tc(gi); }
void c04() { ts(gc); }
void c05() { ti(gc); }
void c06() { tu(gi); }
void c07() { tb(gi); }
void c08() { tv(1, gc, gs, gf, gb, gq); }
void c09() { tf(gd); }
void c10() { ti(gf); }
char r01() { return gi; }
float r02() { return gc; }
int x01() { return (short)gi; }
int x02() { return (unsigned char)gi; }
int x03() { return (char)gf; }
int x04() { return (bool)gi; }
int x05() { return (int)gp; }
char *x06() { return (char *)gi; }
int x07() { return (int)(gp - (char *)gpi); }
int x08() { return gpi - (int *)gp; }
void *x09() { return gpi; }
int x10() { return (E)gi; }
int x11() { return (int)gq; }
int x12() { return (short)(gi + 1); }
int x13() { return (char)gc; }
int x14() { return (int)gi; }
int x15() { return gi; }
float x16() { return (float)gf; }






























