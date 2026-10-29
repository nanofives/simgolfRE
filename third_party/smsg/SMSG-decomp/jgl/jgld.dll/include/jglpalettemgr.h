#ifndef JGLPALETTEMGR_H
#define JGLPALETTEMGR_H
class JGLPalette;
struct PaletteBase {
    PaletteBase() : pal(0) {}
    virtual int init() = 0;
    JGLPalette *pal;
};
extern PaletteBase *def_palette;
extern PaletteBase *def_text_palette;
#endif
