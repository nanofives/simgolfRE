

struct Counter {
    int n;
    Counter() : n(0) {}
    ~Counter() { n = -1; }
    void add(int v) { n += v; }
    int get() const { return n; }
};

int members(int v)
{
    Counter c;
    c.add(v);
    return c.get();
}


















