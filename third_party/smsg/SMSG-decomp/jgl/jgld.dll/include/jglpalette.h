#ifndef JGLPALETTE_H
#define JGLPALETTE_H
#include "jgltypes.h"
#include "jglwin.h"
#include "gspalette.h"
#include "jglpalettetypes.h"
struct PaletteBase;
class JGLPalette : public GSPalette {
public:
    JGLPalette(PaletteBase *client);
    virtual ~JGLPalette();
    virtual int             init();
    virtual void            close();
    virtual void            set();
    virtual int             get_entries(unsigned char *rgb, int first, int count);
    virtual int             set_entries(const unsigned char *rgb, int first, int count);
    virtual unsigned short *get_555_table();
    virtual unsigned short *get_565_table();
    virtual PALETTEENTRY   *get_entry(int index);
    void                    reseed();
    void                    compute_tables();
    void                    fill_rgb(RgbQuad *dst);
    int                     GetPaletteVersion();
    int                     m_palHeader;
    PALETTEENTRY            m_palEntries[256];
    unsigned short          m_lut555[256];
    unsigned short          m_lut565[256];
    PaletteBase            *m_client;
    int                     m_palVersion;
    int                     m_lutVersion;
    LargeInt                m_seedTime;
    static int              init_class(PaletteBase *base, PaletteBase *alt);
    static void             close_class(void);
};
#endif
