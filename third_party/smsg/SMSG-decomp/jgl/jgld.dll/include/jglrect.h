#ifndef JGLRECT_H
#define JGLRECT_H

struct Rect {
    long left, top, right, bottom;

    void SetRect(int x, int y, int cx, int cy)
    {
        left = x;
        top = y;
        right = x + cx;
        bottom = y + cy;
    }

    void Offset(int dx, int dy)
    {
        left += dx;
        right += dx;
        top += dy;
        bottom += dy;
    }
};

#endif
