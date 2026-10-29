#define K 3

static const char *g_name = "{ not a brace }";

struct Point
{
    int x;
    int y;
    int Sum() const { return x + y; }
    void Move(int dx, int dy);
    Point();
    ~Point();
};

Point::Point() : x(0), y(0)
{
}

Point::~Point()
{
    x = -1;
}

void Point::Move(int dx, int dy)
{
    x += dx;
    y += dy;
}

/* } */
static int Helper(int a)
{
    return a * K + 1;
}

static int Unused(int a)
{
    return a - 1;
}

inline int Twice(int a)
{
    return a + a;
}

template <class T> T Biggest(T a, T b)
{
    return a > b ? a : b;
}

int Plain(int a, int b)
{
    if (a > b) {
        return a - b;
    }
    return b - a;
}

void Other(int *p)
{
    char c = '}';
    *p = c;
}

int Later(int a);

int Target(int gi)
{
    Point pt;
    const char *s = "}";
    pt.Move(gi, 2);
    switch (gi) {
    case 0: return Helper(gi) + __LINE__;
    case 1: return Plain(gi, 2) + s[0];
    case 2: return Twice(gi) + Biggest(gi, 7);
    case 3: return Later(gi) + pt.Sum();
    case 4: return g_name[1];
    }
    return __LINE__;
}

int Target(double d)
{
    return (int)d + __LINE__;
}

int Later(int a)
{
    return a * 2;
}
