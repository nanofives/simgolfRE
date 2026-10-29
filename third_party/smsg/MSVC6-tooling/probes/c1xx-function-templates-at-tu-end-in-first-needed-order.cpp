template<class T> T t1(T x) { return x + 1; }
template<class T> T t2(T x) { return x + 2; }
int f(int a) { return t2(a); }
int g(int a) { return a * 7; }
int h(int a) { return t1(a); }
