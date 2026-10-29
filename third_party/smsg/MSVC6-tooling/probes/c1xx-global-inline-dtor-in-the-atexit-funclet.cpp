int close(int);
int open();
struct M { int h; M() { h = open(); } ~M() { close(h); } };
M g_m;
