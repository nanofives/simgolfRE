











struct A { int x; int f() { return x; } int g() { return x + 1; } int unused() { return 3; } };
struct W { int w; virtual ~W() {} int get() { return w; } };
inline int freeinl(int a) { return a * 2; }
template <class T> struct V {
    T *d; int n;
    V() : d(0), n(0) {}
    virtual ~V() { n = 0; }
    int size() { return n; }
    int helper() { return n * 2; }
    int unused2() { return 7; }
    void push(T v);
    void clear();
};
template <class T> void V<T>::push(T v) { d[n] = v; n += helper(); }
template <class T> void V<T>::clear() { n = 0; }
int use1(A *p, W *w) { return p->g() + w->get() + freeinl(1); }
int use2(V<short> *s) { V<int> a; a.push(1); return a.size() + s->size(); }
int use3(A *p) { V<char> *q = new V<char>; int s = q->size(); delete q; return s + p->f(); }































