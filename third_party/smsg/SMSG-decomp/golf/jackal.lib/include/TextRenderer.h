#ifndef TEXTRENDERER_H
#define TEXTRENDERER_H

#include "jglrect.h"

#include "jglfont.h"

struct Font : FontBase {
    Font();
    virtual ~Font() { close(); }

    int CreateFace(const char *face, int height, int style);
    int init(const char *file, const char *face, int height, int style);
    void close();

    int MeasureTextWidth(const char *s);
    int MeasureTextWidth(const char *s, int len);
    char *find_line_break_l(char *p, int *pRemain, int n);
};

struct FontSet {
    Font *slot[3];
};

extern int g_lineWidthUsed;

extern int g_layoutLineEnd;

extern char g_layoutLine[1024];
extern int g_layoutX;
extern int g_layoutY;
extern int g_layoutWidth;

char *get_special(char *s, int *pLen);
char *get_special_link(char *s, int *pLen);
char *get_special_dropdown(char *s, int *pLen);

extern Font *g_defaultFont;

#endif
