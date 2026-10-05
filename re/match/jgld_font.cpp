// FLAGS jgld.dll: /Od /ZI /GZ /GX
// jgld.dll fonts (debug build). Font derives from FontBase and from Tracked (jgld_list.cpp), whose subobject sits
// at +8. Names are chosen here.
#include <windows.h>
#include <string.h>
int jfree(void* p);              // 0x10014a90
void* jalloc(size_t n);          // 0x10014a40
extern const char s_fotExt[];    // 0x1011da88 (placeholder)
extern const char s_backslash[]; // 0x1011da84 (placeholder)
class Tracked {
public:
    Tracked();
    virtual ~Tracked();
};
class FontBase {
public:
    FontBase();                  // 0x10069220
    virtual ~FontBase();         // 0x10069010
    virtual void f1();
    virtual int create(const char* face, int size, unsigned style);   // +8
    virtual void reset();        // +0xc
    void* m_4;
};
class Font : public FontBase, public Tracked {
public:
    Font(void* owner);
    virtual ~Font();
    void release();              // 0x10068da0
    int textWidth(const char* s, int n);
    virtual int create(const char* face, int size, unsigned style);
    int loadFile(const char* file, const char* face, int size, unsigned style);
    int m_c, m_10, m_14;
    HGDIOBJ m_18;
    int m_1c;
    char* m_fot;                 // +0x20
};
extern HDC g_fontDC;             // 0x10128724
struct FontMetrics { int m_0, m_4, m_height, m_ascent, m_internal, m_tmAscent, m_descent; };
void initFontTable();
// MATCH: jgld.dll 0x10068660 ??0Font@@QAE@PAX@Z
Font::Font(void* owner)
{
    m_4 = owner;
    m_10 = 0;
    m_14 = 0;
    m_18 = 0;
    m_1c = 0;
    m_fot = 0;
}
// MATCH: jgld.dll 0x10068760 ??1Font@@UAE@XZ
Font::~Font()
{
    if (m_4)
        *(int*)m_4 = 0;
    release();
}
// MATCH: jgld.dll 0x10068e60 ?textWidth@Font@@QAEHPBDH@Z
int Font::textWidth(const char* s, int n)
{
    SIZE sz;
    if (s == 0)
        return 0;
    SelectObject(g_fontDC, m_18);
    GetTextExtentPoint32A(g_fontDC, s, n, &sz);
    SelectObject(g_fontDC, GetStockObject(SYSTEM_FONT));
    return sz.cx;
}
// MATCH: jgld.dll 0x10068fa0 ?initFontTable@@YAXXZ
void initFontTable()
{
    if (g_fontDC) {
        DeleteDC(g_fontDC);
        g_fontDC = 0;
    }
}
// MATCH: jgld.dll 0x10069220 ??0FontBase@@QAE@XZ
FontBase::FontBase()
{
}
// MATCH: jgld.dll 0x10069010 ??1FontBase@@UAE@XZ
FontBase::~FontBase()
{
}
// MATCH: jgld.dll 0x10068da0 ?release@Font@@QAEXXZ
void Font::release()
{
    if (m_18) {
        DeleteObject(m_18);
        m_18 = 0;
    }
    if (m_fot) {
        RemoveFontResourceA((LPCSTR)m_fot);
        jfree((void*)m_fot);
        m_fot = 0;
    }
}
template <class T> class Array {
public:
    virtual ~Array();
    int add(T v);
    T* m_data;
    int m_cap;
    int m_count;
    int m_grow;
};
template <class T> int Array<T>::add(T v)
{
    if (m_count < m_cap) {
        m_data[m_count] = v;
        m_count++;
    } else {
        T* n = new T[m_cap + m_grow];
        m_cap += m_grow;
        if (m_data)
            memcpy(n, m_data, m_count * sizeof(T));
        n[m_count] = v;
        m_count++;
        delete m_data;
        m_data = n;
    }
    return m_count - 1;
}
struct Rec14 { int v[5]; };
struct Rec94 { int v[0x25]; };
template class Array<Rec14>;
template class Array<Rec94>;
// MATCH: jgld.dll 0x100681a0 ?add@?$Array@URec14@@@@QAEHURec14@@@Z
// MATCH: jgld.dll 0x10068400 ?add@?$Array@URec94@@@@QAEHURec94@@@Z
// MATCH: jgld.dll 0x10068f30 ?initFonts@@YAHXZ
int initFonts()
{
    initFontTable();
    g_fontDC = CreateCompatibleDC(0);
    if (g_fontDC == 0)
        return 2;
    return 0;
}
// MATCH: jgld.dll 0x10068850 ?create@Font@@UAEHPBDHI@Z
int Font::create(const char* face, int size, unsigned style)
{
    LOGFONTA lf;
    TEXTMETRICA tm;
    if (face == 0)
        return 3;
    if (!(m_14 & 1)) {
        reset();
    } else {
        ((FontMetrics*)m_4)->m_ascent = 0;
        ((FontMetrics*)m_4)->m_height = 0;
        ((FontMetrics*)m_4)->m_tmAscent = 0;
        ((FontMetrics*)m_4)->m_descent = 0;
        if (m_18) {
            DeleteObject(m_18);
            m_18 = 0;
        }
    }
    lf.lfHeight = -size;
    lf.lfWidth = 0;
    lf.lfEscapement = 0;
    lf.lfOrientation = 0;
    lf.lfWeight = (style & 1) ? 700 : 0;
    lf.lfItalic = (style & 2) != 0;
    lf.lfUnderline = (style & 4) != 0;
    lf.lfStrikeOut = 0;
    lf.lfCharSet = 0;
    lf.lfOutPrecision = 7;
    lf.lfClipPrecision = 0;
    lf.lfQuality = 0;
    lf.lfPitchAndFamily = 0;
    strcpy(lf.lfFaceName, face);
    m_18 = CreateFontIndirectA(&lf);
    if (m_18 == 0)
        return 13;
    SelectObject(g_fontDC, m_18);
    GetTextMetricsA(g_fontDC, &tm);
    ((FontMetrics*)m_4)->m_height = tm.tmHeight + tm.tmExternalLeading;
    ((FontMetrics*)m_4)->m_ascent = tm.tmAscent - tm.tmInternalLeading;
    ((FontMetrics*)m_4)->m_internal = tm.tmInternalLeading;
    ((FontMetrics*)m_4)->m_tmAscent = tm.tmAscent;
    ((FontMetrics*)m_4)->m_descent = tm.tmDescent;
    SelectObject(g_fontDC, GetStockObject(SYSTEM_FONT));
    return 0;
}
// MATCH: jgld.dll 0x10068ae0 ?loadFile@Font@@QAEHPBD0HI@Z
int Font::loadFile(const char* file, const char* face, int size, unsigned style)
{
    char path[MAX_PATH];
    char* p;
    DWORD err;
    reset();
    if (file == 0 || face == 0)
        return 16;
    m_fot = (char*)jalloc(strlen(file) + 1);
    if (m_fot == 0)
        return 4;
    strcpy(m_fot, file);
    p = m_fot + strlen(m_fot);
    p -= 4;
    *p = 0;
    strcat(m_fot, s_fotExt);
    GetCurrentDirectoryA(MAX_PATH, path);
    strcat(path, s_backslash);
    strcat(path, file);
    if (!CreateScalableFontResourceA(0, m_fot, path, 0)) {
        err = GetLastError();
        if (err != ERROR_FILE_EXISTS && err != 0) {
            jfree(m_fot);
            m_fot = 0;
            return 1;
        }
    }
    if (!AddFontResourceA(m_fot)) {
        GetLastError();
        jfree(m_fot);
        m_fot = 0;
        return 1;
    }
    PostMessageA(HWND_BROADCAST, WM_FONTCHANGE, 0, 0);
    m_14 |= 1;
    return create(face, size, style);
}
