// FLAGS jgld.dll: /Od /ZI /GZ /GX
// jgld.dll Surface::writePcx8 0x10012140 (Surface::op1_8 in jgld_surface.cpp), round 36 batch m36e: writes an 8-bit
// surface as a PCX file through a MappedFile (jgld_mappedfile.cpp) local, hence the EH frame. The output name is the
// input with its extension replaced (0x1011d354, the extension text, is a masked operand); 7 when the surface has no
// bits (slot +0x18), 4 when the mapping fails, else 0. The mapping is width*height*2+0x80 bytes (lea at 0x10012293);
// rows are run-length coded (runs of up to 0x3e, a run of 1 is written raw when the byte is below 0x40), then 0x0c and
// 256 RGB triples from the surface's palette (+0x7c) or, without one, from g_palClient1 0x1012873c's (+4), whose slot
// +0x10 fills them; the file is truncated to the written length (closeTruncate). Names are chosen here.
#include <windows.h>
#include <string.h>
class MappedFile {
public:
    MappedFile();                                    // thunk 0x10001005
    ~MappedFile();                                   // thunk 0x100012c1
    void* create(const char* name, DWORD size, int sequential);   // thunk 0x100017bc
    void closeTruncate(unsigned char* end);          // thunk 0x10001aeb
    int m_0;
    void* m_view;
    HANDLE m_file;
    HANDLE m_map;
    DWORD m_size;
};
struct Rgb3 { unsigned char r, g, b; };
class Palette {
public:
    virtual void p0(); virtual void p1(); virtual void p2(); virtual void p3();
    virtual int getColors(Rgb3* out, int first, int count);   // slot 4 (+0x10)
};
struct PalClient { int m_0; Palette* m_palette; };
extern PalClient* g_palClient1;                      // 0x1012873c
extern const char s_pcxExt[];                        // 0x1011d354 (binary text, not reproduced)
struct PcxHeader {
    unsigned char manufacturer, version, encoding, bitsPerPixel;
    short xmin, ymin, xmax, ymax, hdpi, vdpi;
    unsigned char colormap[48];
    unsigned char reserved, planes;
    short bytesPerLine, paletteInfo, hscreen, vscreen;
    unsigned char filler[54];
};
class Surface {
public:
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5();
    virtual unsigned char* bits8();                  // slot 6 (+0x18)
    virtual void v7(); virtual void v8();
    virtual void unlock(int flag);                   // slot 9 (+0x24)
    virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19();
    virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29();
    virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39();
    virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49();
    virtual void v50(); virtual void v51(); virtual void v52(); virtual void v53();
    virtual int width();                             // slot 54 (+0xd8)
    virtual int height();                            // slot 55 (+0xdc)
    virtual int pitch();                             // slot 56 (+0xe0)
    int writePcx8(const char* name);
    char pad4[0x7c - 4];
    Palette* m_palette;                              // +0x7c
};

// MATCH: jgld.dll 0x10012140 ?writePcx8@Surface@@QAEHPBD@Z
int Surface::writePcx8(const char* name)
{
    MappedFile mf;
    PcxHeader hdr;
    unsigned char run;
    unsigned char value;
    unsigned char* out;
    unsigned char* src;
    int n;
    char path[80];
    unsigned int x;
    char* p;
    Palette* pal;
    Rgb3 colors[256];
    path[0] = 0;
    strcat(path, name);
    p = path + strlen(path);
    while (p > path && *p != '.')
        p--;
    if (*p == '.') {
        *p = 0;
        strcat(path, s_pcxExt);
    }
    src = bits8();
    if (src == 0)
        return 7;
    out = (unsigned char*)mf.create(path, width() * height() * 2 + 0x80, 1);
    if (out == 0) {
        unlock(1);
        return 4;
    }
    hdr.manufacturer = 10;
    hdr.version = 5;
    hdr.encoding = 1;
    hdr.bitsPerPixel = 8;
    hdr.xmin = 0;
    hdr.ymin = 0;
    hdr.xmax = width() - 1;
    hdr.ymax = height() - 1;
    hdr.hdpi = 0;
    hdr.vdpi = 0;
    hdr.reserved = 0;
    hdr.planes = 1;
    hdr.bytesPerLine = pitch();
    hdr.paletteInfo = 0;
    hdr.hscreen = 0;
    hdr.vscreen = 0;
    memcpy(out, &hdr, 0x80);
    out += 0x80;
    for (n = height(); n > 0; n--) {
        x = 0;
        while (x < pitch()) {
            for (run = 1, value = src[x]; x + run < pitch() - 1 && value == src[x + run] && run < 0x3f; run++)
                ;
            if (run > 1) {
                *out = run | 0xc0;
                out++;
                *out = value;
                out++;
            } else if (src[x] < 0x40) {
                *out = value;
                out++;
            } else {
                *out = run | 0xc0;
                out++;
                *out = value;
                out++;
            }
            x += run;
        }
        src += pitch();
    }
    *out = 0xc;
    out++;
    if (m_palette)
        pal = m_palette;
    else
        pal = g_palClient1->m_palette;
    if (pal != 0 && pal->getColors(colors, 0, 0x100) == 0) {
        for (n = 0; n < 0x100; n++) {
            *out = colors[n].r;
            out++;
            *out = colors[n].g;
            out++;
            *out = colors[n].b;
            out++;
        }
    }
    unlock(1);
    mf.closeTruncate(out);
    return 0;
}
