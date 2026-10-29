#ifndef JGLFONT_H
#define JGLFONT_H
#include "gsobject.h"
class JGLFont;
struct FontBase {
    FontBase()
    {
        m_owner = 0;
    }
    JGLFont *m_owner;
    int      m_extraLeading;
    int      m_lineSpacing;
    int      m_capHeight;
    int      m_internalLeading;
    int      m_ascent;
    int      m_descent;
};
class GSFont {
public:
    GSFont();
    virtual ~GSFont();
    virtual int  LoadTTF(const char *ttfFile, const char *faceName, int height, int style);
    virtual int  init(const char *faceName, int height, int style);
    virtual void close();
    virtual int  width(const char *text, int count);
    FontBase    *m_metrics;
};
class JGLFont : public GSFont, public GSObject {
public:
    JGLFont(FontBase *metrics);
#ifdef _USRDLL
    JGLFont() {}
#endif
    virtual ~JGLFont();
    virtual int  LoadTTF(const char *ttfFile, const char *faceName, int height, int style);
    virtual int  init(const char *faceName, int height, int style);
    virtual void close();
    virtual int  width(const char *text, int count);
    int          m_reserved10;
    int          m_flags;
    void        *m_hFont;
    int          m_reserved1c;
    char        *m_fotName;
    static int   init_class(void);
    static void  close_class(void);
};
#endif
