#ifndef SPRITE_H
#define SPRITE_H

#include "Surface.h"
#include "AssetSurface.h"
#include "crt.h"

struct AssetSurface;
class FLCPalette;

struct SpriteObject {
    SpriteObject();
    virtual ~SpriteObject() { close(); }
    JGLSprite *m_surface;
    char *m_filename;
    FLCPalette *m_palette;
    int m_imageWidth;
    int m_imageHeight;
    int m_width;
    int m_height;
    int m_left;
    int m_top;
    int m_memType;

    void close();

    static AssetSurface s_sheet;

    int init(char *filename, int width, int height);
    int BlitSpriteWithOffset(AssetSurface *dst, int sx, int sy, PaletteBase *pal);
    int draw_antialiased(SpriteObject *mask, AssetSurface *dst, AssetSurface *target, int x, int y, PaletteBase *pal);
    int draw_antialiased(SpriteObject *mask, AssetSurface *dst, int x, int y, PaletteBase *pal);

    int BlitScaled(AssetSurface *ctx, int x, int y, unsigned short *shadow, PaletteBase *pal);
    int BlitScaled(AssetSurface *ctx, int x, int y, PaletteBase *pal);
    int BlitScaled(AssetSurface *ctx, int x, int y, unsigned short *shadow, int nx, int ny, int den, PaletteBase *pal);
    int draw_multi_table_dest(AssetSurface *ctx, int x, int y, unsigned short *shadow, int nx, int ny, int den,
        PaletteBase *pal);
    int draw_multi_table_dest(AssetSurface *ctx, int x, int y, unsigned short *shadow, PaletteBase *pal);
    int draw_16_shadows(AssetSurface *ctx, int x, int y, unsigned short *shadow, int nx, int ny, int den,
        PaletteBase *pal);
    int draw_16_shadows(AssetSurface *ctx, int x, int y, unsigned short *shadow, PaletteBase *pal);
    int draw_civ3_units(AssetSurface *dst, AssetSurface *target, int x, int y, unsigned short *shadow,
        PaletteBase *pal);
    int draw_civ3_units(AssetSurface *dst, AssetSurface *target, int x, int y, int nx, int ny, int den,
        unsigned short *shadow, PaletteBase *pal);
    int Draw(AssetSurface *ctx, int x, int y, int nx, int ny, int den, PaletteBase *pal);
    int Draw(AssetSurface *ctx, int x, int y, int n, int den, PaletteBase *pal)
    {
        return Draw(ctx, x, y, n, n, den, pal);
    }
    int BlitScaled(AssetSurface *ctx, int x, int y, unsigned short *shadow, int n, int den, PaletteBase *pal)
    {
        return BlitScaled(ctx, x, y, shadow, n, n, den, pal);
    }
    int BlitWithScaleOffset(AssetSurface *ctx, int x, int y, unsigned short *blend, PaletteBase *pal);

    int BlitWithScaleState(AssetSurface *ctx, int x, int y, unsigned nA, int sx, int sy, int den, PaletteBase *pal,
        unsigned char cB);
    int draw_translucent(AssetSurface *ctx, int x, int y, float scale, PaletteBase *pal, char flip);

    int BlitSpriteWithScaleState(AssetSurface *p, int x, int y, float scale, int sx, int sy, int denom,
        PaletteBase *pal, char flip);
    SpriteObject *copy(SpriteObject *src);

    int extractPriv(AssetSurface *src, int x, int y, int w, int h, int memType, int strict);
    int extract(AssetSurface *src, int x, int y, int w, int h, int memType, int strict);
    int extract_8(AssetSurface *src, int sx, int sy, int w, int h, int memType, unsigned char strict);
    int create_blank(int w, int h);
    int create_blank_8(int w, int h);
};

#endif
