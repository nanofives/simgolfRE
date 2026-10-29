#include "jgldefs.h"
#include "crt.h"
#include "JackalUtil.h"
#include "WidgetBase.h"
#include "Scroll.h"
#include "Display.h"
#include "GameMain.h"






SpriteObject *g_frameSpareSprites[10] = {0};
int g_startupCleared = 0;
const unsigned char *g_frameShadeTable = 0;
SpriteObject *g_frameCapTopLeft = 0;
SpriteObject *g_frameBottomLeft = 0;
SpriteObject *g_frameCapTopRight = 0;
SpriteObject *g_frameBottomRight = 0;
SpriteObject *g_frameCapTopEdge = 0;
SpriteObject *g_frameBottomEdge = 0;
SpriteObject *g_frameLeftEdge = 0;
SpriteObject *g_frameRightEdge = 0;
SpriteObject *g_frameTopRight = 0;
SpriteObject *g_frameTopLeft = 0;
SpriteObject *g_frameTopEdge = 0;





GraphicWin::GraphicWin()
{
    m_renderHook = 0;
    m_minWidth = 0;
    m_minHeight = 0;
    m_capTopLeft = 0;
    m_bottomLeft = 0;
    m_capTopRight = 0;
    m_bottomRight = 0;
    m_leftEdge = 0;
    m_rightEdge = 0;
    m_bottomEdge = 0;
    m_capTopEdge = 0;
    m_topLeft = 0;
    m_topRight = 0;
    m_topEdge = 0;
    m_altTopLeft = 0;
    m_altBottomLeft = 0;
    m_altTopRight = 0;
    m_altBottomRight = 0;
    m_altSurface = 0;
    m_captionHook = g_defaultCaptionHook;
}


GraphicWin::~GraphicWin()
{
    m_renderHook = 0;
}

void GraphicWin::close()
{
    WidgetBase::close();
    AssetSurface::close();
    m_renderHook = 0;
    m_minWidth = 0;
    m_minHeight = 0;
    m_capTopLeft = 0;
    m_bottomLeft = 0;
    m_capTopRight = 0;
    m_bottomRight = 0;
    m_leftEdge = 0;
    m_rightEdge = 0;
    m_bottomEdge = 0;
    m_capTopEdge = 0;
    m_topLeft = 0;
    m_topRight = 0;
    m_topEdge = 0;
    m_altTopLeft = 0;
    m_altBottomLeft = 0;
    m_altTopRight = 0;
    m_altBottomRight = 0;
    m_captionHook = g_defaultCaptionHook;
    if (m_altSurface) {
        delete m_altSurface;
        m_altSurface = 0;
    }
}

int GraphicWin::init(int x, int y, int cx, int cy, const char *caption, int flags, WidgetBase *owner, Menu *peer,
    int *borders)
{
    close();
    if (flags & 0x30000000) {
        m_capTopLeft = g_frameCapTopLeft;
        m_capTopRight = g_frameCapTopRight;
        m_bottomLeft = g_frameBottomLeft;
        m_bottomRight = g_frameBottomRight;
        m_capTopEdge = g_frameCapTopEdge;
        m_bottomEdge = g_frameBottomEdge;
        m_leftEdge = g_frameLeftEdge;
        m_rightEdge = g_frameRightEdge;
        m_topLeft = g_frameTopLeft;
        m_topRight = g_frameTopRight;
        m_topEdge = g_frameTopEdge;
    }
    if (flags & 0x40000000)
        m_altSurface = new AssetSurface;


    int rc = WidgetBase::init(x, y, cx, cy, caption, flags, (GraphicWin *)owner, peer, borders);
    if (rc != 0)
        return rc;
    compute_min_size();
    if (!(flags & 0x800)) {
        nonclient_to_client(&cx, &cy);
    } else {
        if (m_flags & 8)
            cx += g_scrollbarThickness;
        if (m_flags & 4)
            cy += g_scrollbarThickness;
    }
    rc = AssetSurface::Create(cx, cy, (flags & 0x80000000) ? 8 : 0, 1, 0, 0);
    if (rc != 0)
        return rc;
    sync_to_palette(g_widgetPalette);
    return 0;
}

int GraphicWin::init(Rect *r, const char *caption, int flags, WidgetBase *owner, Menu *peer, int *borders)
{
    if (r == 0)
        return 0x10;
    return init(r->left, r->top, r->right - r->left, r->bottom - r->top, caption, flags, owner, peer, borders);
}





void GraphicWin::EraseWholeBackground(int color)
{
    if ((m_flags & 0x80000) && parent && parent->is_graphicwin()) {
        Rect screen = m_screen;
        Rect local = m_local;
        int y = local.top + screen.top;
        int x = screen.left + local.left;
        GraphicWin *p = parent;
        p->copy(this, x, y, 0, 0, GetSurfaceWidth(), GetSurfaceHeight());
        if (g_frameShadeTable)
            MapColorsSized(0, 0, GetSurfaceWidth(), GetSurfaceHeight(), g_frameShadeTable);
    } else {
        ClearToColor(color);
    }
}

int GraphicWin::EraseBackground(Rect *r, int color)
{
    if ((m_flags & 0x80000) && parent && parent->is_graphicwin()) {
        int ox = m_screen.left + m_local.left;
        int oy = m_screen.top + m_local.top;
        GraphicWin *p = parent;
        p->copy(this, r->left + ox, r->top + oy, r->left, r->top, RectWidth(r), RectHeight(r));
        if (g_frameShadeTable)
            MapColorsSized(r->left, r->top, RectWidth(r), RectHeight(r), g_frameShadeTable);
        return 0;
    }
    return Surface_FillRect(r, color);
}

void GraphicWin::fill(int x, int y, int w, int h, int color)
{
    AssetSurface::fill(x, y, w, h, color);
}





















int GraphicWin::resize(int cx, int cy, int flag, int depth)
{
    on_check_size(&cx, &cy, &flag);

    if (flag != 0) {
        if (cx <= 1)
            cx = 1;
        if (cy <= 1)
            cy = 1;
    } else {
        int minW = 1, minH = 1;
        client_to_nonclient(&minW, &minH);
        if (minW > cx)
            cx = minW;
        if (minH > cy)
            cy = minH;
    }
    if (flag == 0)
        nonclient_to_client(&cx, &cy);

    int r;
    buffer_resizing = 1;
    if ((r = Create(cx, cy, depth, 0, (m_flags >> 28) & 8, 0)) != 0) {
        buffer_resizing = 0;
        return r;
    }
    buffer_resizing = 0;




    return WidgetBase::resize(cx, cy, 1, 0);
}

void WidgetBase::RenderRect(int x, int y, int w, int h, int)
{
    if (is_visible()) {
        Rect r;
        r.SetRect(x, y, w, h);
        update_window(&r);
        RectToScreen(&r);
        flip(&r);
    }
}

int GraphicWin::RenderBoundsRect(Rect *r, int)
{
    if (r == 0)
        r = GetRect();
    RenderRect(r->left, r->top, r->right - r->left, r->bottom - r->top, 0);
    return 0;
}

void GraphicWin::RenderFullSurface(int)
{
    RenderRect(0, 0, RectWidth(GetRect()), RectHeight(GetRect()), 0);
}














void GraphicWin::soft_update()
{
    if (!is_visible())
        return;

    Rect r;
    if (m_surface)
        m_surface->SURF_CopyBoundsRect(&r);

    int x = 0;
    int y = 0;
    client_to_screen(&x, &y);

    r.Offset(x, y);

    graphsys->invalidate_rect(&r);
}

void GraphicWin::soft_update(Rect *area)
{
    if (!is_visible())
        return;
    if (area == 0) {
        soft_update();
        return;
    }
    Rect r = *area;
    if (!IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)GetRect()))
        return;
    int x = 0;
    int y = 0;
    client_to_screen(&x, &y);
    r.Offset(x, y);
    graphsys->invalidate_rect(&r);
}















void GraphicWin::redraw()
{
    if (m_renderLatch & 1)
        return;
    m_renderLatch |= 1;

    g_renderingWidget = this;

    if (m_renderHook)
        m_renderHook();

    on_redraw();

    m_renderLatch &= ~1;
    soft_update();
}






void GraphicWin::on_paint(Rect *clip)
{
    if (graphsys)
        graphsys->set_sprite_scale(1, 1, 1);

    Rect r = m_clip;
    int px, py;

    AssetSurface *target = g_paintTarget;
    if (target) {
        r.Offset(-g_paintTargetX, -g_paintTargetY);
        target->set_clip(&r);
    } else {
        WidgetBase::s_primaryBuffer.set_clip(&r);
    }

    px = 0;
    py = 0;
    client_to_screen(&px, &py);





    AssetSurface *dst = g_paintTarget;
    if (dst) {
        px -= g_paintTargetX;
        py -= g_paintTargetY;
    } else {
        dst = &WidgetBase::s_primaryBuffer;
    }

    if (m_flags & 0x1000000) {
        if (m_destRecolorLut == 0 && m_srcDstLut == 0 && m_blitArg40 == 0)
            draw(dst, px, py);
        else if (m_destRecolorLut)
            draw_dest(dst, px, py, m_destRecolorLut);
        else if (m_blitArg40 == 0)
            draw_multi_table_dest(dst, px, py, m_srcDstLut);
        else
            BlitVtable40(dst, px, py, m_blitArg40);
    } else {
        copy(dst, 0, 0, px, py, RectWidth(&m_local), RectHeight(&m_local));
    }

    if (dst->m_surface)
        dst->m_surface->reset_clip();
}

























void GraphicWin::on_size(int nType, int cx, int cy)
{
    if (nType == 1)
        return;
    if (cx == 0 || cy == 0) {
        int w = GetSurfaceWidth();
        cx = w;
        int h = GetSurfaceHeight();
        cy = h;
    }
    if (m_flags & 8)
        cx -= g_scrollbarThickness;
    if (m_flags & 4)
        cy -= g_scrollbarThickness;

    Create(cx, cy, m_depth, 1, 0, 0);
    sync_to_palette(g_widgetPalette);

    if ((m_flags & 0x40000000) != 0 && m_altSurface != 0) {
        m_altSurface->resize(cx, cy, 0);
        m_altSurface->sync_to_palette(g_widgetPalette);
    }

    set_rects();
    on_nc_paint(0, -1);
    resize_event(RectWidth(GetRect()), RectHeight(GetRect()));
    if (is_visible())
        redraw();
}


void GraphicWin::set_rects()
{
    WidgetBase::set_rects();
}

void GraphicWin::on_redraw_nc(int which)
{
    if (m_status & 2) {
        if (graphsys)
            graphsys->set_sprite_scale(1, 1, 1);

        SpriteObject *topRight;
        SpriteObject *topLeft;
        int border;
        SpriteObject *topEdge;
        if ((m_flags & 0x10) && !(m_flags & 0x400000)) {
            border = m_captionHeight;
            topLeft = m_capTopLeft;
            topEdge = m_capTopEdge;
            topRight = m_capTopRight;
        } else {
            border = m_borderWidth;
            topLeft = m_topLeft;
            topEdge = m_topEdge;
            topRight = m_topRight;
        }
        int bottom;
        if (m_bottomBorder == -1)
            bottom = m_borderWidth;
        else
            bottom = m_bottomBorder;

        if (m_flags & 0x10000000) {
            if (m_topStrip && (which == -1 || which == 0)) {
                m_topStrip->ClearToColor(m_topStrip->GetTransparentColor());
                if (topLeft)
                    topLeft->BlitScaled(m_topStrip, 0, 0, 0);
                if (topRight)
                    topRight->BlitScaled(m_topStrip, m_topStrip->GetSurfaceWidth() - topRight->m_width, 0, 0);
                if (topEdge && topRight && topLeft) {
                    int x = topLeft->m_width;
                    int w = m_screen.right - m_screen.left - topRight->m_width - x;
                    m_topStrip->TileSprite(topEdge, 0, 0, x, 0, w, m_topStrip->GetSurfaceHeight());
                }
                if (m_captionText && (m_flags & 0x10)) {
                    if (m_captionHook)
                        m_captionHook(this, m_captionText, m_topStrip);
                    on_caption_draw(m_captionText, m_topStrip);
                }
                on_post_redraw_nc_buffer(m_topStrip, 0);
            }

            if (m_bottomStrip && (which == -1 || which == 2)) {
                m_bottomStrip->ClearToColor(m_bottomStrip->GetTransparentColor());
                if (m_bottomLeft)
                    m_bottomLeft->BlitScaled(m_bottomStrip, 0,
                        m_bottomStrip->GetSurfaceHeight() - m_bottomLeft->m_height, 0);
                if (m_bottomRight) {
                    int h = m_bottomStrip->GetSurfaceHeight();
                    int dy = m_bottomRight->m_height;
                    int w = m_bottomStrip->GetSurfaceWidth();
                    m_bottomRight->BlitScaled(m_bottomStrip, w - m_bottomRight->m_width, h - dy, 0);
                }
                if (m_bottomEdge && m_bottomLeft && m_bottomRight) {
                    int x = m_bottomLeft->m_width;
                    int y = m_bottomStrip->GetSurfaceHeight() - m_bottomEdge->m_height;
                    int w = m_bottomStrip->GetSurfaceWidth() - m_bottomRight->m_width - m_bottomLeft->m_width;
                    int h = m_bottomEdge->m_height;
                    m_bottomStrip->TileSprite(m_bottomEdge, 0, 0, x, y, w, h);
                }
                on_post_redraw_nc_buffer(m_bottomStrip, 2);
            }

            if (m_leftStrip && (which == -1 || which == 3)) {
                m_leftStrip->ClearToColor(m_leftStrip->GetTransparentColor());
                if (topLeft)
                    topLeft->BlitScaled(m_leftStrip, 0, -border, 0);
                if (m_bottomLeft)
                    m_bottomLeft->BlitScaled(m_leftStrip, 0,
                        m_leftStrip->GetSurfaceHeight() - m_bottomLeft->m_height + bottom, 0);
                if (m_leftEdge && topLeft && m_bottomLeft) {
                    int y = topLeft->m_height - border;
                    int w = m_borderWidth;
                    int h =
                        m_leftStrip->GetSurfaceHeight() - m_bottomLeft->m_height - topLeft->m_height + bottom + border;
                    m_leftStrip->TileSprite(m_leftEdge, 0, 0, 0, y, w, h);
                }
                on_post_redraw_nc_buffer(m_leftStrip, 3);
            }

            if (m_rightStrip && (which == -1 || which == 1)) {
                m_rightStrip->ClearToColor(m_rightStrip->GetTransparentColor());
                if (topRight)
                    topRight->BlitScaled(m_rightStrip, m_rightStrip->GetSurfaceWidth() - topRight->m_width, -border, 0);
                if (m_bottomRight) {
                    int x = m_rightStrip->GetSurfaceWidth() - m_bottomRight->m_width;
                    m_bottomRight->BlitScaled(m_rightStrip, x,
                        m_rightStrip->GetSurfaceHeight() - m_bottomRight->m_height + bottom, 0);
                }
                if (m_rightEdge && topRight && m_bottomRight) {
                    int x = m_rightStrip->GetSurfaceWidth() - m_rightEdge->m_width;
                    int y = topRight->m_height - border;
                    int w = m_rightEdge->m_width;
                    int h = m_rightStrip->GetSurfaceHeight() - m_bottomRight->m_height - topRight->m_height + bottom
                            + border;
                    m_rightStrip->TileSprite(m_rightEdge, 0, 0, x, y, w, h);
                }
                on_post_redraw_nc_buffer(m_rightStrip, 1);
            }
        } else {
            WidgetBase::on_redraw_nc(which);
        }
    }
}

void SetFrameSprites(SpriteObject **sprites)
{
    if (sprites) {
        g_frameCapTopLeft = sprites[0];
        g_frameCapTopRight = sprites[1];
        g_frameBottomLeft = sprites[2];
        g_frameBottomRight = sprites[3];
        g_frameCapTopEdge = sprites[4];
        g_frameLeftEdge = sprites[5];
        g_frameRightEdge = sprites[6];
        g_frameBottomEdge = sprites[7];
        g_frameTopLeft = sprites[9];
        g_frameTopRight = sprites[10];
        g_frameTopEdge = sprites[8];
    }
}




#define WIDGETBASE_MAX(a, b) ((a) > (b) ? (a) : (b))
void GraphicWin::compute_min_size()
{
    if (this->m_flags & 0x10) {
        if (this->m_capTopLeft && this->m_capTopRight && this->m_bottomLeft && this->m_bottomRight) {
            this->m_minWidth = this->m_capTopLeft->m_width + this->m_capTopRight->m_width;
            this->m_minWidth =
                WIDGETBASE_MAX(this->m_minWidth, this->m_bottomRight->m_width + this->m_bottomLeft->m_width);
            this->m_minHeight = this->m_capTopLeft->m_height + this->m_bottomLeft->m_height;
            this->m_minHeight =
                WIDGETBASE_MAX(this->m_minHeight, this->m_bottomRight->m_height + this->m_capTopRight->m_height);
        }
    } else {
        if (this->m_topLeft && this->m_topRight && this->m_bottomLeft && this->m_bottomRight) {
            this->m_minWidth = this->m_topLeft->m_width + this->m_topRight->m_width;
            this->m_minWidth =
                WIDGETBASE_MAX(this->m_minWidth, this->m_bottomRight->m_width + this->m_bottomLeft->m_width);
            this->m_minHeight = this->m_topLeft->m_height + this->m_bottomLeft->m_height;
            this->m_minHeight =
                WIDGETBASE_MAX(this->m_minHeight, this->m_bottomRight->m_height + this->m_topRight->m_height);
        }
    }
}



unsigned short *g_fadeTable = 0;
int g_fadeSteps = 0;
int g_fadeTableWidth;

void (*g_defaultCaptionHook)(GraphicWin *self, char *text, AssetSurface *strip) = 0;

int InitFadeTable(int steps)
{
    int i, row, col;
    int r, g, b;
    int rShift, gShift, bShift;
    int rMask, gMask, bMask;
    double *ramp;
    double step;

    if (steps < 4 || (steps & 1))
        return 3;
    g_fadeSteps = steps;
    g_fadeTableWidth = (display_format()->is565 == 0) ? 0x8000 : 0x10000;
    g_fadeTable = (unsigned short *)AllocateBytes(g_fadeTableWidth * g_fadeSteps * 2);
    if (g_fadeTable == 0)
        return 4;
    i = g_fadeSteps * 8;
    ramp = (double *)AllocateBytes(i);
    if (ramp == 0)
        return 4;

    step = 1.0 / ((double)g_fadeSteps - 1.0);
    ramp[0] = 0.0;
    for (i = 1; i < g_fadeSteps / 2; i++) {
        ramp[i] = ramp[i - 1] + step;
        ramp[g_fadeSteps - i - 1] = 1.0 - ramp[i];
    }
    ramp[g_fadeSteps - 1] = 1.0;

    rShift = display_format()->rShift;
    gShift = display_format()->gShift;
    bShift = display_format()->bShift;
    rMask = display_format()->rMask;
    gMask = display_format()->gMask;
    bMask = display_format()->bMask;

    for (row = 0; row < g_fadeSteps / 2; row++) {
        for (col = 0; col < g_fadeTableWidth; col++) {
            r = (col & rMask) >> rShift;
            g = (col & gMask) >> gShift;
            b = (col & bMask) >> bShift;
            g_fadeTable[row * g_fadeTableWidth + col] =
                ((int)(g * ramp[row]) << gShift) | ((int)(r * ramp[row]) << rShift) | (int)(b * ramp[row]);
            g_fadeTable[(g_fadeSteps - row - 1) * g_fadeTableWidth + col] =
                ((g << gShift) | (r << rShift) | b) - g_fadeTable[row * g_fadeTableWidth + col];
        }
    }
    return 0;
}

void FreeFadeTable()
{
    if (g_fadeTable != 0) {
        FreeIfNotNull(g_fadeTable);
        g_fadeTable = 0;
    }
}

int g_renderSpare[2] = {0};
