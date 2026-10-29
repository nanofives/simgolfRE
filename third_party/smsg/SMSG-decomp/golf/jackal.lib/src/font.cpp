#include "jgldefs.h"
#include "TextRenderer.h"
#include "EngineInit.h"
#include "Surface.h"
#include "crt.h"
#include <string.h>

#pragma intrinsic(strlen)

Font default_font;

Font::Font()
{
}

int Font::CreateFace(const char *face, int height, int style)
{
    if (face == 0)
        return 3;
    close();
    m_owner = graphsys->get_font_data(this);
    return m_owner->init(face, height, style);
}

int Font::init(const char *file, const char *face, int height, int style)
{
    if (file == 0 || face == 0)
        return 0x10;
    close();
    m_owner = graphsys->get_font_data(this);
    return m_owner->LoadTTF(file, face, height, style);
}

void Font::close()
{
    if (m_owner)
        graphsys->delete_font_data(m_owner);
    m_capHeight = 0;
    m_lineSpacing = 0;
    m_ascent = 0;
    m_descent = 0;
    m_extraLeading = -1;
}

int Font::MeasureTextWidth(const char *s)
{
    if (m_owner == 0 || s == 0)
        return 0;
    return m_owner->width(s, strlen(s));
}

int Font::MeasureTextWidth(const char *s, int len)
{
    if (m_owner && s) {
        if (len >= (int)strlen(s))
            len = strlen(s);
        return m_owner->width(s, len);
    }
    return 0;
}

char *Font::find_line_break_l(char *p, int *pRemain, int n)
{
    char *cur = p;
    int remain = *pRemain;
    while (n) {
        char *sp = (char *)memchr(cur + 1, ' ', n);
        if (sp == 0) {
            remain -= MeasureTextWidth(cur, n);
            if (cur == p) {
                if (remain < 0) {
                    if (g_lineWidthUsed != 0) {
                        *pRemain = 0;
                        return cur;
                    }
                    *pRemain = 0;
                    return 0;
                }
                g_lineWidthUsed += *pRemain - remain;
                *pRemain = remain;
                return 0;
            } else {
                if (remain < 0) {
                    *pRemain = 0;
                    return cur + 1;
                }
                g_lineWidthUsed += *pRemain - remain;
                *pRemain = remain;
                return 0;
            }
        }
        remain -= MeasureTextWidth(cur, sp - cur);
        if (cur == p) {
            if (remain < 0) {
                if (g_lineWidthUsed != 0) {
                    *pRemain = 0;
                    return cur;
                }
                *pRemain = 0;
                return sp + 1;
            }
        } else {
            if (remain < 0) {
                *pRemain = 0;
                return cur + 1;
            }
        }
        n -= sp - cur;
        cur = sp;
    }
    g_lineWidthUsed += *pRemain - remain;
    *pRemain = remain;
    return 0;
}

int SetDefaultFont(Font *font)
{
    int rc;

    if (font == 0)
        return 3;
    ReleaseDefaultFont();
    rc = graphsys->init_font_system();
    if (rc)
        return rc;
    g_defaultFont = font;
    if (font->m_owner == 0) {
        rc = font->CreateFace("Times New Roman", 12, 0);
        if (rc)
            return rc;
    }
    return 0;
}

void ReleaseDefaultFont()
{
    if (graphsys)
        graphsys->close_font_system();
    g_defaultFont = 0;
}

static Font *NewFonts(int count)
{
    return new Font[count];
}

static int s_spareWord = 0;
Font *g_defaultFont = 0;
static int s_spareWord2 = 0;
int task_flags = 0;
GraphSys *graphsys = 0;

int TrimTextToWidth(Font *font, char *s, int width)
{
    int w;
    while ((w = font->MeasureTextWidth(s)) > width) {
        char *sp = strrchr(s, ' ');
        if (sp == 0)
            break;
        *sp = 0;
    }
    return w;
}
