#ifndef STRINGUTIL_H
#define STRINGUTIL_H

#include "jglrect.h"

void purge_leading(char *s);
void purge_trailing(char *s);
void purge_spaces(char *s);
void kill_lf(char *s);
void add_lf(char *s);
int in_box(int x, int y, Rect *r);

inline int RectWidth(const Rect *r)
{
    return r->right - r->left;
}

inline int RectHeight(const Rect *r)
{
    return r->bottom - r->top;
}

#endif
