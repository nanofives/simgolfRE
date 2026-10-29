extern "C" {
int g[8];
extern int ext(int);
int four(int a, int b, int c, int d) { int r = ext(a); r += ext(b + r); r += ext(c + r); r += ext(d + r); g[0] = a + b + c + d; return r + a + b + c + d; }
}
