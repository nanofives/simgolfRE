extern "C" {
extern int ext(int);
int one(int a) { int r = ext(a); return r + a; }
}
