




#ifndef ASSETSURFACE_H
#define ASSETSURFACE_H

#include "jglrect.h"
#include "TextRenderer.h"
#include "Spot.h"
#include "bufferbase.h"
#include "crt.h"

class JGLBuffer;
class FLCPalette;
class JGLPalette;
struct PaletteBase;
struct PixelFormat;
struct SpriteObject;
struct AssetSurface;
struct StringList;


struct BoxSpriteParams {
    SpriteObject *topLeft;
    SpriteObject *topRight;
    SpriteObject *bottomLeft;
    SpriteObject *bottomRight;
    SpriteObject *top;
    SpriteObject *bottom;
    SpriteObject *left;
    SpriteObject *right;
    AssetSurface *fill;
    int fillMode;
};

struct GraphicWin;
struct Vert;



struct AssetSurface : BufferBase {

    virtual int init_buffer(int w, int h, int bpp, int memType) { return Create(w, h, bpp, memType, 0, 0); }

    virtual int resize_buffer(int cx, int cy, int bpp) { return resize(cx, cy, bpp); }

    virtual void close_buffer() { close(); }

    virtual ~AssetSurface();

    AssetSurface();

    virtual GraphicWin *is_graphicwin();


    int m_memType;


    int (*m_drawHook)(const char *s, int x, int y, int n);


    void (*m_itemHook)(int key, int style);


    int m_hangingIndent;
    int m_pendingIndent;
    int m_lineCount;

    int m_depth;

    int m_layoutAux;
    char m_dirty;
    int m_hotLink;


    int m_renderStyle;

    int m_wrapStyle;



    int m_style;

    int m_linkState;

    int m_formatOn;


    int m_dropIndex;
    int m_wrapDropState;


    int m_dropState;

    int m_renderDropState;


    SpriteObject *m_dropArrow;

    int m_reserved58;

    union {
        Font *m_fonts[4];
        FontSet m_fontSet;
    };



    int m_textColors[4][4];

    FLCPalette *m_palette;




    int m_linkCount;
    Spot m_links;
    int m_linkCommand[40];
    char *m_linkParam[40];

    Spot m_dropSpots;
    int m_dropSpotCount;



    void clear_links();
    void resetDropDownSpots();

    void addDropDownSpot(int id, int param, int x, int y, int w, int h);


    static SpriteObject s_nullSprite;


    int Create(int w, int h, int bpp, int memType, int depth, int f);


    int Create(int w, int h, int memType, int depth, int f) { return Create(w, h, 0, memType, depth, f); }




    int load_pcx(const char *file, FLCPalette *pal, int first, int count, int mode);


    int sync_to_palette(PaletteBase *pal);


    void ClearToColor(unsigned color);
    int resize(int cx, int cy, int bpp);


    int LoadPCXFromFile(FILE *fp);

    int SaveToPCX(FILE *fp);


    void close();

    int SaveSnapshot(char *file, Rect *area);

    void copy(AssetSurface *dst, int sx, int sy, int dx, int dy, int cx, int cy);

    int BlitSurfaceRegion(AssetSurface *dst, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh);
    int BlitSurfaceRegion(AssetSurface *dst, Rect *src, Rect *dstRect);

    int CalculateFormattedTextWidth(const char *s, unsigned int len);

    int set_clip(Rect *r);

    void SetClipRect(int x, int y, int w, int h);

    int draw(AssetSurface *dst, int x, int y);
    int draw_dest(AssetSurface *dst, int x, int y, unsigned char *lut);
    int draw_multi_table_dest(AssetSurface *dst, int x, int y, unsigned char *lut);
    int BlitVtable40(AssetSurface *dst, int x, int y, int blitArg);

    int BlitRectToRect(AssetSurface *dst, int x, int y, int w, int h);
    int BlitRectToRect(AssetSurface *dst, Rect *r);


    unsigned char *LoadBMPFile(char *pszFile, BITMAPINFOHEADER **ppInfo);
    int ConvertBMP8ToSurface16(BITMAPINFOHEADER *info, unsigned char *pBits);
    int LoadBMPToSurface16(char *pszFile);
    void FlipImageVertically(unsigned char *pData, BITMAPINFOHEADER *pInfo);

    int Surface_FillRect(Rect *r, int color);
    int fill_shadow_over_underlay(Rect *r, AssetSurface *src, int pct, unsigned short *lut);
    void fill(int x, int y, int w, int h, int color);


    int map_colors(int left, int top, int right, int bottom, const unsigned char *table);

    int MapColorsSized(int x, int y, int w, int h, const unsigned char *table)
    {
        return map_colors(x, y, x + w - 1, y + h - 1, table);
    }

    int poly(Vert *pts, int count, int color);

    void line(int fromX, int fromY, int toX, int toY, int color);

    void hline(int fromX, int toX, int y, int color);
    void vline(int x, int fromY, int toY, int color);


    void box(int left, int top, int right, int bottom, int cHi, int cLo);
    int DrawBevelFrameInRect(Rect *rc, int cHi, int cLo);


    void dotted_hline(int fromX, int toX, int y, int color);
    void dotted_vline(int x, int fromY, int toY, int color);


    void DrawDottedRect(int left, int top, int right, int bottom, int color);
    int dotted_box(Rect *r, int color);

    void dotted_hline_8(int fromX, int toX, int y, unsigned char color);
    void dotted_vline_8(int x, int fromY, int toY, unsigned char color);
    void dotted_hline_16(int fromX, int toX, int y, int color);
    void dotted_vline_16(int x, int fromY, int toY, int color);


    int get_pixel(int x, int y);
    int put_pixel(int x, int y, int color);
    unsigned char *get_data(int x, int y);
    unsigned char GetPixelRed(int x, int y, unsigned char def);
    unsigned char GetPixelGreen(int x, int y, unsigned char def);
    unsigned char GetPixelBlue(int x, int y, unsigned char def);
    char GetDirty();
    void SetDirty(char dirty);




    int LoadImageFile(char *pData, int nSize, FLCPalette *pPal, int first, int count);

    int load_pcx_8_8(char *pData, int nSize, FLCPalette *pPal, int first, int count);
    int load_pcx_8_16(char *pData, int nSize, FLCPalette *pPal, int first, int count);


    int TileSprite(SpriteObject *src, int sx, int sy, int x, int y, int w, int h);
    int tile_mask(AssetSurface *src, int sx, int sy, int x, int y, int w, int h, int mode);
    int tile(AssetSurface *src, int sx, int sy, int x, int y, int w, int h);


    void box_sprite(Rect *r, BoxSpriteParams *frame);



    Rect *GetRect()
    {
        if (m_surface)
            return m_surface->get_rect();
        return 0;
    }
    int GetSurfaceWidth()
    {
        if (m_surface)
            return m_surface->get_width();
        return 0;
    }
    int GetSurfaceHeight()
    {
        if (m_surface)
            return m_surface->get_height();
        return 0;
    }
    unsigned GetTransparentColor() { return m_surface ? m_surface->get_seethru() : 0xff; }
    PixelFormat *GetPixelFormat() { return m_surface ? m_surface->get_pixformat() : 0; }
    Rect *GetClip()
    {
        if (m_surface)
            return m_surface->get_clip();
        return 0;
    }
    int GetPitch()
    {
        if (m_surface)
            return m_surface->get_pitch();
        return 0;
    }

    void GetSurfaceBounds(Rect *out)
    {
        if (m_surface)
            m_surface->SURF_CopyBoundsRect(out);
    }

    void ResetSurfaceClip()
    {
        if (m_surface)
            m_surface->reset_clip();
    }

    void CopySurfaceClip(Rect *out)
    {
        if (m_surface)
            m_surface->SURF_CopyClipRect(out);
    }

    void Unlock(int n)
    {
        if (m_surface)
            m_surface->release_data(n);
    }
    unsigned char *GetPixels() { return m_surface ? m_surface->get_data_8() : 0; }
    unsigned char *GetPixelPtr(int x, int y) { return !m_surface ? 0 : m_surface->SURF_GetPixelPtr8(x, y); }
    unsigned char *Lock8()
    {
        if (!m_surface)
            return 0;
        return m_surface->get_data_8();
    }
    unsigned short *Lock16()
    {
        if (!m_surface)
            return 0;
        return m_surface->get_data_16();
    }


    int text_height();
    int text_line_height();

    void set_text_color(int color, int shadowColor, int shadowDx, int shadowDy);
    void set_text_color2(int color, int shadowColor, int shadowDx, int shadowDy);
    void set_text_color3(int color, int shadowColor, int shadowDx, int shadowDy);
    void set_text_color_hyper(int color, int shadowColor, int shadowDx, int shadowDy);
    int SetFontsForStyles(Font *font, Font *braceFont, Font *bracketFont, Font *linkFont);
    int text_width(const char *s);



    int write_strings(StringList *lines, int x, int y, int maxWidth, int flags);



    int RenderTextCenteredInWidth(const char *s, int x, int y, int w, int n);
    int RenderTextCenteredInRect(const char *s, Rect *rc, unsigned int n);


    int write_cent_l(const char *s, int x, int y, int w, int h, int n);
    int RenderTextHorizontallyCentered(const char *s, int x, int y, int n);
    int RenderTextRightAlignedInWidth(const char *s, int x, int y, int w, int n);
    int write_right_l(const char *s, int x, int y, int n);
    int RenderTextRightAlignedInRectVCentered(const char *s, Rect *rc, int n);
    int RenderTextVerticallyCentered(const char *s, Rect *rc, int n);
    int write_l(const char *s, int x, int y, int n);


    void DrawTextLeftAlignedWithFont(Font *font, const char *s, int x, int y, int n);
    void DrawTextVerticallyCenteredWithFont(Font *font, const char *s, Rect *rc, int n);
    void DrawTextCenteredWithFont(Font *font, const char *s, int x, int y, int n);
    void DrawTextRightAlignedWithFont(Font *font, const char *s, int x, int y, int n);
    void DrawTextCenteredInWidthWithFont(Font *font, const char *s, int x, int y, int w, int n);
    void DrawTextCenteredInRectWithFont(Font *font, const char *s, Rect *rc, unsigned int n);
    void DrawTextCenteredInBoundsWithFont(Font *font, const char *s, int x, int y, int w, int h, int n);
    void DrawTextRightAlignedInRectVCenteredWithFont(Font *font, const char *s, Rect *rc, int n);
    void DrawMultilineTextInRectWithFont(Font *font, char *text, Rect *rc);
    void DrawMultilineTextCenteredWithFont(Font *font, char *text, int x, int y, int w);
    void DrawTextRightAlignedInWidthWithFont(Font *font, const char *s, int x, int y, int w, int n);
    int write_multi_font_raw_l(const char *s, int x, int y, int n);

    int write_raw_l(const char *s, int x, int y, int n);




    int wrap_height(char *text, int avail, char **pLines, int *pCount);
    char *find_line_break_l(char *text, int *pWidth, int n);



    int RenderMultilineTextInRect(char *text, Rect *rc, char **pLines, int *pCount);
    int wrap(char *text, int x, int y, int avail);
    int wrap_cent(char *text, int x, int y, int w);
    int RenderMultilineTextCenteredInRect(char *text, Rect *rc);
    int wrap_right(char *text, int x, int y, int avail);




    int TextLayout_BeginWithWidth(char *text, int width);
    int wrap_height_flying(char *text);
    int wrap_height_flying_flush();
    int wrap_flying(char *text, int x, int y, int width);
    int TextLayout_AppendAndWrap(char *text);
    int wrap_flying_flush();
    int ReplaceColorInRect(int x, int y, int w, int h, unsigned long from, unsigned long to);
    int change_color(unsigned long from, unsigned long to);
    static int get_pcx_dimensions(const char *szFile, int *pWidth, int *pHeight);
};






extern int language;



extern FLCPalette *g_lastPalette;


extern int g_defaultFormatOn;

#endif
