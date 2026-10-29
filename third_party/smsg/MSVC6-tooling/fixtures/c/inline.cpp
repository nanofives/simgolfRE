int sink(int);

#define STEP(k) s = s * 3 + a + k; if (s > 1000) s -= sink(s);
#define TEN(k) STEP(k##0) STEP(k##1) STEP(k##2) STEP(k##3) STEP(k##4) STEP(k##5) STEP(k##6) STEP(k##7) STEP(k##8) STEP(k##9)

inline int tiny(int a) { return a + 1; }

inline int small(int a) { int s = a; STEP(1) STEP(2) return s; }

inline int mid(int a) { int s = a; TEN(1) return s; }

inline int big(int a) { int s = a; TEN(1) TEN(2) TEN(3) TEN(4) TEN(5) TEN(6) TEN(7) TEN(8) return s; }

inline int nest(int a) { return mid(a) + small(a) + tiny(a); }

struct Lock {
    int n;
    Lock() : n(0) { sink(1); }
    ~Lock() { n = sink(n); }
    void add(int v) { n += v; }
};

struct Heavy {
    int n;
    Heavy() : n(0) { int a = 2; int s = a; TEN(1) n = s; }
    ~Heavy() { int a = n; int s = a; TEN(2) sink(s); }
};

int f_tiny(int x) { return tiny(x) + tiny(x + 1); }

int f_mixed(int x) { return small(x) + mid(x) + big(x) + tiny(x); }

int f_many(int x)
{
    int s = 0;
    s += mid(x);
    s += mid(x + 1);
    s += mid(x + 2);
    s += mid(x + 3);
    s += mid(x + 4);
    s += mid(x + 5);
    s += mid(x + 6);
    s += mid(x + 7);
    s += mid(x + 8);
    s += mid(x + 9);
    s += mid(x + 10);
    s += mid(x + 11);
    return s;
}

int f_nest(int x) { return nest(x) + nest(x + 1) + nest(x + 2); }

int f_lock(int x)
{
    Lock l;
    l.add(x);
    if (x > 3) {
        return l.n + small(x);
    }
    return l.n + tiny(x);
}

int f_heavy(int x)
{
    Heavy h;
    if (x > 1) {
        return h.n + mid(x);
    }
    if (x > 2) {
        return h.n + small(x);
    }
    return h.n;
}

int f_new(int x)
{
    Lock *p = new Lock;
    p->add(x);
    int r = p->n + tiny(x);
    delete p;
    return r;
}

int f_big(int x)
{
    int a = x;
    int s = x;
    TEN(1) TEN(2) TEN(3) TEN(4) TEN(5) TEN(6) TEN(7) TEN(8) TEN(9)
    TEN(1) TEN(2) TEN(3) TEN(4) TEN(5) TEN(6) TEN(7) TEN(8) TEN(9)
    return s + big(x) + big(x + 1) + mid(x);
}
