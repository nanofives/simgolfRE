#include "jgldefs.h"
#include "Sprite.h"
#include "FLCPalette.h"
#include "PaletteList.h"
#include "Display.h"
#include "SpriteBox.h"
#include "Heap.h"

AssetSurface SpriteObject::s_sheet;

SpriteObject::SpriteObject() : m_surface(0)
{
    m_filename = 0;
    m_imageWidth = 0;
    m_imageHeight = 0;
    m_width = 0;
    m_height = 0;
    m_left = 0;
    m_top = 0;
    m_palette = 0;
}

void SpriteObject::close()
{
    if (graphsys) {
        if (m_surface)
            graphsys->delete_sprite_data(m_surface);
        if (m_filename) {
            free(m_filename);
            m_filename = 0;
        }
        if (m_palette) {
            delete m_palette;
            m_palette = 0;
        }
        m_imageWidth = 0;
        m_imageHeight = 0;
        m_width = 0;
        m_height = 0;
        m_left = 0;
        m_top = 0;
    }
}

int SpriteObject::init(char *filename, int width, int height)
{
    if (!filename)
        return 0x10;
    close();
    m_filename = (char *)malloc(strlen(filename) + 1);
    if (!m_filename)
        return 4;
    m_filename[0] = 0;
    strcat(m_filename, filename);
    m_left = 0;
    m_top = 0;
    m_imageWidth = width;
    m_imageHeight = height;
    m_width = width;
    m_height = height;
    return 0;
}

int SpriteObject::extract(AssetSurface *src, int x, int y, int w, int h, int memType, int strict)
{
    if (src == 0)
        return 0x10;
    if (src->m_surface == 0)
        return 7;
    if (src->m_surface->get_pixformat()->bpp != 8)
        return 0;
    return extract_8(src, x, y, w, h, memType, strict);
}

int SpriteObject::extractPriv(AssetSurface *src, int x, int y, int w, int h, int memType, int strict)
{
    int rc = extract(src, x, y, w, h, memType, strict);
    m_palette = src->m_palette;
    src->m_palette = 0;
    return rc;
}

int SpriteObject::Draw(AssetSurface *ctx, int x, int y, int nx, int ny, int den, PaletteBase *pal)
{
    if (ctx == 0)
        return 0x10;
    if (m_surface == 0 || ctx->m_surface == 0)
        return 7;
    if (m_filename) {
        int rc = s_sheet.load_pcx(m_filename, 0, 10, 0xec, 0);
        if (rc)
            return rc;
        rc = s_sheet.draw(ctx, x, y);
        s_sheet.close();
        return rc;
    }
    int oldDenom, oldScaleY, oldScaleX;
    graphsys->get_sprite_scale(&oldScaleX, &oldScaleY, &oldDenom);
    graphsys->set_sprite_scale(nx, ny, den);
    int sx = m_left * nx / den;
    int sy = m_top * ny / den;
    m_surface->draw(ctx->m_surface, sx + x, sy + y, pal ? pal->pal : 0);
    graphsys->set_sprite_scale(oldScaleX, oldScaleY, oldDenom);
    return 0;
}

int SpriteObject::BlitSpriteWithOffset(AssetSurface *dst, int sx, int sy, PaletteBase *pal)
{
    if (m_filename != 0)
        return 0x18;
    if (dst == 0)
        return 0x10;
    if (m_surface == 0 || dst->m_surface == 0)
        return 7;
    return m_surface->draw_254(dst->m_surface, m_left + sx, m_top + sy, pal ? pal->pal : 0);
}

int SpriteObject::BlitScaled(AssetSurface *ctx, int x, int y, PaletteBase *pal)
{
    if (m_filename) {
        int rc = s_sheet.load_pcx(m_filename, 0, 10, 0xec, 0);
        if (rc)
            return rc;
        rc = s_sheet.draw(ctx, x, y);
        s_sheet.close();
        return rc;
    }
    if (ctx == 0)
        return 0x10;
    if (m_surface == 0 || ctx->m_surface == 0)
        return 7;
    int nx, ny, den;
    graphsys->get_sprite_scale(&nx, &ny, &den);
    int sx = m_left * nx / den;
    int sy = m_top * ny / den;
    return m_surface->draw(ctx->m_surface, sx + x, sy + y, pal ? pal->pal : 0);
}

int SpriteObject::draw_antialiased(SpriteObject *mask, AssetSurface *dst, int x, int y, PaletteBase *pal)
{
    if (dst == 0 || mask == 0)
        return 0x10;
    if (m_surface == 0 || dst->m_surface == 0 || mask->m_surface == 0)
        return 7;
    return m_surface->draw_antialiased(mask->m_surface, dst->m_surface, x, y, pal ? pal->pal : 0);
}

int SpriteObject::draw_antialiased(SpriteObject *mask, AssetSurface *dst, AssetSurface *target, int x, int y,
    PaletteBase *pal)
{
    if (dst == 0 || mask == 0)
        return 0x10;
    if (m_surface == 0 || dst->m_surface == 0 || mask->m_surface == 0 || target->m_surface == 0)
        return 7;
    return m_surface->MaskedBlitToTarget(mask->m_surface, dst->m_surface, target->m_surface, x, y, pal ? pal->pal : 0);
}

int SpriteObject::BlitScaled(AssetSurface *ctx, int x, int y, unsigned short *shadow, int nx, int ny, int den,
    PaletteBase *pal)
{
    int oldDenom, oldScaleY, oldScaleX;
    graphsys->get_sprite_scale(&oldScaleX, &oldScaleY, &oldDenom);
    graphsys->set_sprite_scale(nx, ny, den);
    int oldLeft = m_left;
    int oldTop = m_top;
    m_left = oldLeft * nx / den;
    m_top = oldTop * ny / den;
    int r = BlitScaled(ctx, x, y, shadow, pal);
    m_left = oldLeft;
    m_top = oldTop;
    graphsys->set_sprite_scale(oldScaleX, oldScaleY, oldDenom);
    return r;
}

int SpriteObject::BlitScaled(AssetSurface *ctx, int x, int y, unsigned short *shadow, PaletteBase *pal)
{
    if (m_filename != 0)
        return 0x18;
    if (ctx == 0)
        return 16;
    if (m_surface == 0 || ctx->m_surface == 0)
        return 7;
    int nx, ny, den;
    graphsys->get_sprite_scale(&nx, &ny, &den);
    int sx = m_left * nx / den;
    int sy = m_top * ny / den;
    return m_surface->ShadowBlit(ctx->m_surface, sx + x, sy + y, shadow, pal ? pal->pal : 0);
}

int SpriteObject::draw_multi_table_dest(AssetSurface *ctx, int x, int y, unsigned short *shadow, int nx, int ny,
    int den, PaletteBase *pal)
{
    int oldDenom, oldScaleY, oldScaleX;
    graphsys->get_sprite_scale(&oldScaleX, &oldScaleY, &oldDenom);
    graphsys->set_sprite_scale(nx, ny, den);
    int oldLeft = m_left;
    int oldTop = m_top;
    m_left = oldLeft * nx / den;
    m_top = oldTop * ny / den;
    int r = draw_multi_table_dest(ctx, x, y, shadow, pal);
    m_left = oldLeft;
    m_top = oldTop;
    graphsys->set_sprite_scale(oldScaleX, oldScaleY, oldDenom);
    return r;
}

int SpriteObject::draw_multi_table_dest(AssetSurface *ctx, int x, int y, unsigned short *shadow, PaletteBase *pal)
{
    if (m_filename != 0)
        return 0x18;
    if (ctx == 0)
        return 0x10;
    if (m_surface == 0 || ctx->m_surface == 0)
        return 7;
    return m_surface->ShadowMaskBlit(ctx->m_surface, m_left + x, m_top + y, shadow, pal ? pal->pal : 0);
}

int SpriteObject::draw_16_shadows(AssetSurface *ctx, int x, int y, unsigned short *shadow, int nx, int ny, int den,
    PaletteBase *pal)
{
    int oldDenom, oldScaleY, oldScaleX;
    int oldLeft = m_left;
    int oldTop = m_top;
    int r;
    graphsys->get_sprite_scale(&oldScaleX, &oldScaleY, &oldDenom);
    graphsys->set_sprite_scale(nx, ny, den);
    m_left = nx * m_left / den;
    m_top = m_top * ny / den;
    r = draw_16_shadows(ctx, x, y, shadow, pal);
    m_left = oldLeft;
    m_top = oldTop;
    graphsys->set_sprite_scale(oldScaleX, oldScaleY, oldDenom);
    return r;
}

int SpriteObject::draw_16_shadows(AssetSurface *ctx, int x, int y, unsigned short *shadow, PaletteBase *pal)
{
    if (m_filename != 0)
        return 0x18;
    if (ctx == 0)
        return 0x10;
    if (m_surface == 0 || ctx->m_surface == 0)
        return 7;
    return m_surface->draw_16_shadows(ctx->m_surface, m_left + x, m_top + y, shadow, pal ? pal->pal : 0);
}

int SpriteObject::draw_civ3_units(AssetSurface *dst, AssetSurface *target, int x, int y, unsigned short *shadow,
    PaletteBase *pal)
{
    if (m_filename != 0)
        return 0x18;
    if (dst == 0 || target == 0)
        return 0x10;
    if (m_surface == 0 || dst->m_surface == 0 || target->m_surface == 0)
        return 7;
    return m_surface->DrawWithPaletteAndShadowLut(dst->m_surface, target->m_surface, m_left + x, m_top + y, shadow,
        pal ? pal->pal : 0);
}

int SpriteObject::draw_civ3_units(AssetSurface *dst, AssetSurface *target, int x, int y, int nx, int ny, int den,
    unsigned short *shadow, PaletteBase *pal)
{
    if (dst == 0)
        return 0x10;
    if (m_surface == 0 || dst->m_surface == 0)
        return 7;
    int oldDenom, oldScaleY, oldScaleX;
    int oldLeft = m_left;
    int oldTop = m_top;
    int newLeft, newTop, r;
    graphsys->get_sprite_scale(&oldScaleX, &oldScaleY, &oldDenom);
    graphsys->set_sprite_scale(nx, ny, den);
    newLeft = m_left * nx / den;
    m_left = newLeft;
    newTop = ny * m_top / den;
    m_top = newTop;
    r = m_surface->DrawWithPaletteAndShadowLutScaled(dst->m_surface, target->m_surface, newLeft + x, newTop + y, nx, ny,
        den, shadow, pal ? pal->pal : 0);
    m_left = oldLeft;
    m_top = oldTop;
    graphsys->set_sprite_scale(oldScaleX, oldScaleY, oldDenom);
    return r;
}

int SpriteObject::BlitWithScaleOffset(AssetSurface *ctx, int x, int y, unsigned short *blend, PaletteBase *pal)
{
    if (ctx == 0)
        return 16;
    if (m_surface == 0 || ctx->m_surface == 0)
        return 7;
    int nx, ny, den;
    graphsys->get_sprite_scale(&nx, &ny, &den);
    int sx = m_left * nx / den;
    int sy = m_top * ny / den;
    return m_surface->draw_multi_table(ctx->m_surface, sx + x, sy + y, blend, pal ? pal->pal : 0);
}

int SpriteObject::BlitWithScaleState(AssetSurface *ctx, int x, int y, unsigned nA, int sx, int sy, int den,
    PaletteBase *pal, unsigned char cB)
{
    if (ctx == 0)
        return 16;
    if (m_surface == 0 || ctx->m_surface == 0)
        return 7;
    int oldDenom, oldScaleY, oldScaleX;
    int oldLeft, oldTop, newLeft, newTop, r;
    graphsys->get_sprite_scale(&oldScaleX, &oldScaleY, &oldDenom);
    graphsys->set_sprite_scale(sx, sy, den);
    oldLeft = m_left;
    oldTop = m_top;
    newLeft = oldLeft * sx / den;
    m_left = newLeft;
    newTop = oldTop * sy / den;
    m_top = newTop;
    r = m_surface->draw_color_scaled(ctx->m_surface, newLeft + x, newTop + y, nA, pal ? pal->pal : 0, cB);
    m_left = oldLeft;
    m_top = oldTop;
    graphsys->set_sprite_scale(oldScaleX, oldScaleY, oldDenom);
    return r;
}

int SpriteObject::BlitSpriteWithScaleState(AssetSurface *p, int x, int y, float scale, int sx, int sy, int denom,
    PaletteBase *pal, char flip)
{
    int oldDenom, oldScaleY, oldScaleX;
    int oldLeft = m_left;
    int oldTop = m_top;
    int r;
    graphsys->get_sprite_scale(&oldScaleX, &oldScaleY, &oldDenom);
    graphsys->set_sprite_scale(sx, sy, denom);
    m_left = sx * m_left / denom;
    m_top = m_top * sy / denom;
    r = draw_translucent(p, x, y, scale, pal, flip);
    m_left = oldLeft;
    m_top = oldTop;
    graphsys->set_sprite_scale(oldScaleX, oldScaleY, oldDenom);
    return r;
}

int SpriteObject::draw_translucent(AssetSurface *ctx, int x, int y, float scale, PaletteBase *pal, char flip)
{
    if (m_filename != 0)
        return 0x18;
    if (ctx == 0)
        return 16;
    if (m_surface == 0 || ctx->m_surface == 0)
        return 7;
    return m_surface->draw_translucent(ctx->m_surface, m_left + x, m_top + y, scale, pal ? pal->pal : 0, flip);
}

SpriteObject *SpriteObject::copy(SpriteObject *src)
{
    SpriteObject *dst = this;
    if (src->m_surface == 0)
        return dst;
    dst->close();
    dst->m_filename = src->m_filename;
    dst->m_imageWidth = src->m_imageWidth;
    dst->m_imageHeight = src->m_imageHeight;
    dst->m_width = src->m_width;
    dst->m_height = src->m_height;
    dst->m_left = src->m_left;
    dst->m_top = src->m_top;
    dst->m_memType = src->m_memType;
    JGLSprite *s = graphsys->get_sprite_data(dst ? &dst->m_surface : 0, dst->m_memType);
    dst->m_surface = s;
    s->set_seethru(0xff);
    dst->m_surface->CopyFrom(src->m_surface);
    return dst;
}

int SpriteObject::create_blank_8(int w, int h)
{
    close();
    m_surface = graphsys->get_sprite_data(this ? &m_surface : 0, 1);
    m_surface->set_seethru(0xff);
    int rc = m_surface->init(0, w, h, 8, 1, g_startupPalette.pal);
    if (rc == 0) {
        m_imageWidth = w;
        m_imageHeight = h;
        m_width = w;
        m_height = h;
        m_left = rc;
        m_top = rc;
    }
    return rc;
}

int SpriteObject::create_blank(int w, int h)
{
    close();
    m_surface = graphsys->get_sprite_data(this ? &m_surface : 0, 1);
    m_surface->set_seethru(0xff);
    if (m_surface->init(0, w, h, display_format()->bpp, 1, g_startupPalette.pal)) {
        m_imageWidth = w;
        m_imageHeight = h;
        m_width = w;
        m_height = h;
        m_left = 0;
        m_top = 0;
    }
    return 0;
}

int SpriteList::fill_entry(SpriteStruct *elem)
{
    elem->sprite = m_stageSprite;
    if (m_stageText == 0)
        elem->text = m_stageText;
    else {
        if (m_pool)
            elem->text = (char *)m_pool->Alloc(strlen(m_stageText) + 1);
        else
            elem->text = (char *)malloc(strlen(m_stageText) + 1);
        if (elem->text == 0)
            return 4;
        elem->text[0] = 0;
        strcat(elem->text, m_stageText);
    }
    elem->extra = m_stageExtra;
    return 0;
}

void SpriteList::kill_entry(SpriteStruct *elem)
{
    if (m_pool == 0 && elem->text)
        free(elem->text);
}

static SpriteObject *NewSpriteObjects(int count)
{
    return new SpriteObject[count];
}

int g_spriteLastX = -1;
int g_spriteLastY = -1;
float g_spriteMaxScale = 10.0f;

char g_lineBuffer[80] = {0};
