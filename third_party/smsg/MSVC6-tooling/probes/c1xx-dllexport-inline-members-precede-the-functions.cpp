struct __declspec(dllexport) T {
    int a;
    int f(int x) { return a + x; }
    int g(int x) { return a * x; }
};
inline int h(int x) { return x - 2; }
int use1(T* t, int x) { return t->g(x) + h(x); }
int use2(T* t, int x) { return t->f(x) + h(x); }
