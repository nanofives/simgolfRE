#include "fx.h"

struct Shape {
    virtual int area() = 0;
};

struct Rect : Shape {
    int w;
    int h;
    Rect(int a, int b) : w(a), h(b) {}
    int area() { return w * h; }
};

extern "C" int __cdecl _purecall(void)
{
    return 0;
}

extern "C" int shape_area(int w, int h)
{
    Rect r(w, h);
    Shape *s = &r;
    return s->area();
}
