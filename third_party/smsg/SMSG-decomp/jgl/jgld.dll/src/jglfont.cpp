#include "jgldefs.h"
#include <windows.h>
#include "jglfont.h"
#include "jglwin.h"
#include <string.h>
typedef unsigned char BYTE;
#include "jglalloc.h"
void        *g_fontDC;
void __cdecl operator delete(void *);
JGLFont::JGLFont(FontBase *metrics)
{
    m_metrics = metrics;
    m_reserved10 = 0;
    m_flags = 0;
    m_hFont = 0;
    m_reserved1c = 0;
    m_fotName = 0;
}
JGLFont::~JGLFont()
{
    if (m_metrics != 0)
        m_metrics->m_owner = 0;
    close();
}
int JGLFont::init(const char *faceName, int height, int style)
{
    LOGFONTA    lf;
    TEXTMETRICA tm;
    if (faceName == 0) {
        return 3;
    }
    if ((m_flags & 1) == 0) {
        close();
    } else {
        m_metrics->m_capHeight = 0;
        m_metrics->m_lineSpacing = 0;
        m_metrics->m_ascent = 0;
        m_metrics->m_descent = 0;
        if (m_hFont != 0) {
            DeleteObject(m_hFont);
            m_hFont = 0;
        }
    }
    lf.lfHeight = -height;
    lf.lfWidth = 0;
    lf.lfEscapement = 0;
    lf.lfOrientation = 0;
    lf.lfWeight = (style & 1) ? FW_BOLD : 0;
    lf.lfItalic = (style & 2) != 0;
    lf.lfUnderline = (style & 4) != 0;
    lf.lfStrikeOut = 0;
    lf.lfCharSet = 0;
    lf.lfOutPrecision = OUT_TT_ONLY_PRECIS;
    lf.lfClipPrecision = 0;
    lf.lfQuality = 0;
    lf.lfPitchAndFamily = 0;
    strcpy(lf.lfFaceName, faceName);
    m_hFont = CreateFontIndirectA(&lf);
    if (m_hFont == 0) {
        return 13;
    }
    SelectObject((HDC)g_fontDC, m_hFont);
    GetTextMetricsA((HDC)g_fontDC, &tm);
    m_metrics->m_lineSpacing = tm.tmHeight + tm.tmExternalLeading;
    m_metrics->m_capHeight = tm.tmAscent - tm.tmInternalLeading;
    m_metrics->m_internalLeading = tm.tmInternalLeading;
    m_metrics->m_ascent = tm.tmAscent;
    m_metrics->m_descent = tm.tmDescent;
    SelectObject((HDC)g_fontDC, GetStockObject(SYSTEM_FONT));
    return 0;
}
int JGLFont::LoadTTF(const char *ttfFile, const char *faceName, int height, int style)
{
    char  path[MAX_PATH];
    char *ext;
    int   err;
    close();
    if (ttfFile == 0 || faceName == 0) {
        return 16;
    }
    m_fotName = (char *)jgl_malloc(strlen(ttfFile) + 1);
    if (m_fotName == 0) {
        return 4;
    }
    strcpy(m_fotName, ttfFile);
    ext = m_fotName + strlen(m_fotName);
    ext -= 4;
    *ext = 0;
    strcat(m_fotName, ".fot");
    GetCurrentDirectoryA(MAX_PATH, path);
    strcat(path, "\\");
    strcat(path, ttfFile);
    if (CreateScalableFontResourceA(0, m_fotName, path, 0) == 0) {
        err = GetLastError();
        if (err != ERROR_FILE_EXISTS && err != 0) {
            jgl_free(m_fotName);
            m_fotName = 0;
            return 1;
        }
    }
    if (AddFontResourceA(m_fotName) == 0) {
        GetLastError();
        jgl_free(m_fotName);
        m_fotName = 0;
        return 1;
    }
    PostMessageA(HWND_BROADCAST, WM_FONTCHANGE, 0, 0);
    m_flags |= 1;
    return init(faceName, height, style);
}
void JGLFont::close()
{
    if (m_hFont != 0) {
        DeleteObject(m_hFont);
        m_hFont = 0;
    }
    if (m_fotName != 0) {
        RemoveFontResourceA(m_fotName);
        jgl_free(m_fotName);
        m_fotName = 0;
    }
}
int JGLFont::width(const char *text, int count)
{
    SIZE sz;
    if (text == 0) {
        return 0;
    }
    SelectObject((HDC)g_fontDC, m_hFont);
    GetTextExtentPoint32A((HDC)g_fontDC, text, count, &sz);
    SelectObject((HDC)g_fontDC, GetStockObject(SYSTEM_FONT));
    return sz.cx;
}
int JGLFont::init_class(void)
{
    close_class();
    g_fontDC = CreateCompatibleDC(0);
    if (g_fontDC == 0)
        return 2;
    return 0;
}
void JGLFont::close_class(void)
{
    if (g_fontDC != 0) {
        DeleteDC((HDC)g_fontDC);
        g_fontDC = 0;
    }
}
inline int GSFont::init(const char *faceName, int height, int style)
{
    return 24;
}
inline int GSFont::LoadTTF(const char *ttfFile, const char *faceName, int height, int style)
{
    return 24;
}
inline void GSFont::close() {}
inline int  GSFont::width(const char *text, int count)
{
    return 0;
}
GSFont::~GSFont() {}
GSFont::GSFont() {}
