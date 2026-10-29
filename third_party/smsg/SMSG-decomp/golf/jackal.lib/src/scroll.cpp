#include "jgldefs.h"
#include <math.h>
#include <wchar.h>






#include "Scroll.h"
#include "MiscUtil.h"
#include "JackalUtil.h"










int g_scrollbarThickness = GetSystemMetrics(SM_CYHSCROLL);















static int s_spareWord = 0;
int scroll_from_key = 0;
int g_scrollDefaultFlagsLow[2] = {0};
SpriteObject *g_scrollHorzNear[3] = {0};
SpriteObject *g_scrollVertNear[3] = {0};
SpriteObject *g_scrollHorzFar[3] = {0};
SpriteObject *g_scrollVertFar[3] = {0};
static int s_spareWord2 = 0;
SpriteObject *g_scrollThumbTrack[2] = {0};
int g_scrollbarNonClient = 0;


Scroll::Scroll()
{
    m_flags = g_scrollDefaultFlagsLow[0];
    m_bevelColor = g_scrollTheme[3];
    m_inverted = 0;
    m_rangeLo = g_scrollDefaultFlagsLow[1];
    m_rangeHi = g_scrollTheme[4];
    m_scrollPos = g_scrollDefaultFlagsLow[1];
    m_repeatDelay = g_scrollTheme[2];
    m_drag = -1;
    m_firstStepTime = 0;
    m_rate = g_scrollTheme[1];
    m_pageStep = g_scrollTheme[0];
    m_pageDir = 0;
    m_thumb.left = 0;
    m_thumb.top = 0;
    m_thumb.right = 0;
    m_thumb.bottom = 0;
    f5A8 = g_scrollTheme[5];
    m_arrowColor = g_scrollTheme[6];
    m_backColor = g_scrollTheme[7];
    m_bevelDepth = g_scrollTheme[8];
    m_bevelHiColor = g_scrollTheme[9];
    m_bevelLoColor = g_scrollTheme[10];
    for (int i = 0; i < 3; ++i) {
        m_imgHorzNear[i] = g_scrollHorzNear[i];
        m_imgVertNear[i] = g_scrollVertNear[i];
        m_imgHorzFar[i] = g_scrollHorzFar[i];
        m_imgVertFar[i] = g_scrollVertFar[i];
    }
    m_thumbSprite = g_scrollThumbTrack[0];
    m_trackSprite = g_scrollThumbTrack[1];
    m_onStep = 0;
    m_onRepeat = 0;
}

void Scroll::close()
{
    m_flags = g_scrollDefaultFlagsLow[0];
    m_bevelColor = g_scrollTheme[3];
    m_inverted = 0;
    m_rangeLo = g_scrollDefaultFlagsLow[1];
    m_rangeHi = g_scrollTheme[4];
    m_scrollPos = g_scrollDefaultFlagsLow[1];
    m_drag = -1;
    m_firstStepTime = 0;
    m_repeatDelay = g_scrollTheme[2];
    m_rate = g_scrollTheme[1];
    m_pageStep = g_scrollTheme[0];
    m_pageDir = 0;
    m_thumb.left = 0;
    m_thumb.top = 0;
    m_thumb.right = 0;
    m_thumb.bottom = 0;
    f5A8 = g_scrollTheme[5];
    m_arrowColor = g_scrollTheme[6];
    m_backColor = g_scrollTheme[7];
    m_bevelDepth = g_scrollTheme[8];
    m_bevelHiColor = g_scrollTheme[9];
    m_bevelLoColor = g_scrollTheme[10];
    for (int i = 0; i < 3; ++i) {
        m_imgHorzNear[i] = g_scrollHorzNear[i];
        m_imgVertNear[i] = g_scrollVertNear[i];
        m_imgHorzFar[i] = g_scrollHorzFar[i];
        m_imgVertFar[i] = g_scrollVertFar[i];
    }
    m_thumbSprite = g_scrollThumbTrack[0];
    m_trackSprite = g_scrollThumbTrack[1];
    m_onStep = 0;
    m_onRepeat = 0;
    m_arrowNear.close();
    m_arrowFar.close();
    GraphicWin::close();
}



int Scroll::init(int x, int y, int w, int h, WidgetBase *parent, int id, int flags)
{
    if (parent == 0 || w == 0 || h == 0)
        return 3;
    close();
    m_flags |= flags;
    m_id = id;
    m_rangeLo = 0;
    SpriteObject **imgNear;
    SpriteObject **imgFar;
    if (w > h) {
        set_bar_thickness(h);
        m_rangeHi = w - h * 2;
        imgNear = m_imgHorzNear;
        imgFar = m_imgHorzFar;
    } else {
        set_bar_thickness(w);
        m_rangeHi = h - w * 2;
        imgNear = m_imgVertNear;
        imgFar = m_imgVertFar;
    }
    int style = 0;
    if (m_flags & 1)
        style = 0x80000;
    int rc;
    if (g_scrollbarNonClient)
        rc = GraphicWin::init(x, y, w, h, 0, style | 0x9020, parent, 0, 0);
    else
        rc = GraphicWin::init(x, y, w, h, 0, style | 0x1020, parent, 0, 0);
    if (rc)
        return rc;
    if (!(m_flags & 2)) {
        if (m_bevelColor != -1)
            rc = m_arrowNear.init(0, 1, 1, 1, m_extent - 2, m_extent - 2, this, 0);
        else
            rc = m_arrowNear.init(0, 1, 0, 0, m_extent, m_extent, this, 0);
        if (rc)
            return rc;
        ImageButton *near_ = &m_arrowNear;
        SpriteObject *na = imgNear[0], *nc = imgNear[2], *nb = imgNear[1];
        near_->m_images[0] = na;
        near_->m_images[1] = nb;
        near_->m_images[2] = nc;
        ButtonState *ns = &m_arrowNear.m_state;
        ns->m_colors[0] = m_arrowColor;
        ns->m_colors[3] = m_bevelDepth;
        ns->m_colors[1] = m_bevelHiColor;
        ns->m_colors[2] = m_bevelLoColor;
        ns->m_minPressMs = 0;
        m_arrowNear.m_repeatDelay = 0x32;
        m_arrowNear.redraw();
        int fx, fy;
        if (m_bevelColor == -1) {
            fy = (w > h) ? 0 : (GetSurfaceHeight() - m_extent);
            fx = (w > h) ? (GetSurfaceWidth() - m_extent) : 0;
            rc = m_arrowFar.init(0, 2, fx, fy, m_extent, m_extent, this, 0);
        } else {
            fy = (w > h) ? 1 : (GetSurfaceHeight() - m_extent + 1);
            fx = (w > h) ? (GetSurfaceWidth() - m_extent + 1) : 1;
            rc = m_arrowFar.init(0, 2, fx, fy, m_extent - 2, m_extent - 2, this, 0);
        }
        if (rc)
            return rc;
        ImageButton *far_ = &m_arrowFar;
        SpriteObject *fa = imgFar[0], *fc = imgFar[2], *fb = imgFar[1];
        far_->m_images[0] = fa;
        far_->m_images[1] = fb;
        far_->m_images[2] = fc;
        ButtonState *fs = &m_arrowFar.m_state;
        fs->m_colors[0] = m_arrowColor;
        fs->m_colors[3] = m_bevelDepth;
        fs->m_colors[1] = m_bevelHiColor;
        fs->m_colors[2] = m_bevelLoColor;
        fs->m_minPressMs = 0;
        m_arrowFar.m_repeatDelay = 0x32;
        m_arrowFar.redraw();
    }
    m_drag = -1;
    m_scrollPos = m_rangeLo;
    show(0);
    g_scrollbarNonClient = 0;
    return 0;
}

int Scroll::init(Rect *rect, WidgetBase *parent, int id, int flags)
{
    if (!rect || !parent)
        return 3;
    return init(rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, parent, id, flags);
}

int Scroll::init_vert(int x, int y, int height, WidgetBase *parent, int id)
{
    if (parent == 0 || height == 0)
        return 3;
    return init(x, y, g_scrollbarThickness, height, parent, id, 0);
}

int Scroll::init_horz(int x, int y, int width, WidgetBase *parent, int id)
{
    if (parent == 0 || width == 0)
        return 3;
    return init(x, y, width, g_scrollbarThickness, parent, id, 0);
}

int Scroll::init_vert_nc(int x, int y, int height, WidgetBase *parent, int id)
{
    g_scrollbarNonClient = 1;
    return init_vert(x, y, height, parent, id);
}

int Scroll::init_horz_nc(int x, int y, int width, WidgetBase *parent, int id)
{
    g_scrollbarNonClient = 1;
    return init_horz(x, y, width, parent, id);
}

void Scroll::set_range(int lo, int hi)
{
    if (lo < hi) {
        m_inverted = 0;
    } else {
        m_inverted = 1;
        swap(&lo, &hi);
    }
    m_rangeLo = lo;
    m_scrollPos = lo;
    m_rangeHi = hi;
    redraw();
}


void Scroll::set_bar_thickness(int extent)
{
    m_extent = extent;
    set_thumb_rect();
}

void Scroll::set_pos(int pos)
{
    g_renderingWidget = parent;
    int lo = m_rangeLo;
    if (pos < lo) {
        m_scrollPos = lo;
    } else {
        int hi = m_rangeHi;
        if (pos > hi)
            m_scrollPos = hi;
        else
            m_scrollPos = pos;
    }
    if (m_inverted != 0) {
        int hi = m_rangeHi;
        int cur = m_scrollPos;
        m_scrollPos = hi - cur + lo;
    }
    redraw();
}















void Scroll::on_mouse_move(int x, int y)
{
    if ((g_mouseDownWidget ? g_mouseDownWidget : g_mouseDownWidgetAlt) != this)
        return;
    if (g_leftButtonPress == 0)
        return;
    scroll_from_key = 0;
    int prev = m_scrollPos;
    if (m_pageDir == 0) {
        int thumb = RectWidth(&m_thumb);
        int travel = -(thumb * 3);
        if (m_bevelColor != -1)
            travel -= 2;
        if (GetSurfaceWidth() > GetSurfaceHeight()) {
            travel += GetSurfaceWidth();
            x -= thumb;
            if (m_bevelColor != -1)
                x--;
            int lo = m_rangeLo;
            m_scrollPos = (x - (thumb >> 1)) * (m_rangeHi - lo) / travel + lo;
            m_drag = x;
        } else {
            travel += GetSurfaceHeight();
            y -= thumb;
            if (m_bevelColor != -1)
                y--;
            int lo = m_rangeLo;
            m_scrollPos = (y - (thumb >> 1)) * (m_rangeHi - lo) / travel + lo;
            m_drag = y;
        }
        m_scrollPos = max(m_rangeLo, min(m_rangeHi, m_scrollPos));
    } else {
        int elapsed = GetMilliseconds() - m_firstStepTime;
        if (m_firstStepTime != 0 && elapsed < m_repeatDelay * 2) {
            if (get_lbutton_state())
                get_mouse_pos(&x, &y);
            return;
        }
        if (m_firstStepTime == 0)
            m_firstStepTime = GetMilliseconds();
        if (GetSurfaceWidth() > GetSurfaceHeight()) {
            if (x > m_thumb.right && m_pageDir > 0) {
                m_scrollPos += m_pageStep;
                if (m_scrollPos > m_rangeHi)
                    m_scrollPos = m_rangeHi;
            } else if (x < m_thumb.left && m_pageDir < 0) {
                m_scrollPos -= m_pageStep;
                if (m_scrollPos < m_rangeLo)
                    m_scrollPos = m_rangeLo;
            }
        } else {
            if (y > m_thumb.bottom && m_pageDir > 0) {
                m_scrollPos += m_pageStep;
                if (m_scrollPos > m_rangeHi)
                    m_scrollPos = m_rangeHi;
            } else if (y < m_thumb.top && m_pageDir < 0) {
                m_scrollPos -= m_pageStep;
                if (m_scrollPos < m_rangeLo)
                    m_scrollPos = m_rangeLo;
            }
        }
        get_mouse_pos(&x, &y);
        get_lbutton_state();
    }
    g_renderingWidget = parent;
    if (prev != m_scrollPos) {
        if (m_inverted == 0) {
            parent->on_scrolling(m_id, m_scrollPos);
            if (m_onRepeat != 0)
                m_onRepeat(m_id, m_scrollPos);
        } else {
            parent->on_scrolling(m_id, m_rangeLo - m_scrollPos + m_rangeHi);
            if (m_onRepeat != 0)
                m_onRepeat(m_id, m_rangeLo - m_scrollPos + m_rangeHi);
        }
        if (parent->m_onScrolling != 0)
            parent->m_onScrolling->play();
    }
    redraw();
}




















void Scroll::on_left_down(int x, int y)
{
    int w = GetSurfaceWidth();
    int h = GetSurfaceHeight();

    if (w > h) {
        if (x < m_thumb.left) {
            m_pageDir = -1;
            m_firstStepTime = 0;
            on_mouse_move(x, y);
        } else if (x > m_thumb.right) {
            m_pageDir = 1;
            m_firstStepTime = 0;
            on_mouse_move(x, y);
        } else {
            m_pageDir = 0;
        }
    } else {
        if (y < m_thumb.top) {
            m_pageDir = -1;
            m_firstStepTime = 0;
            on_mouse_move(x, y);
        } else if (y > m_thumb.bottom) {
            m_pageDir = 1;
            m_firstStepTime = 0;
            on_mouse_move(x, y);
        } else {
            m_pageDir = 0;
        }
    }

    if (g_dragWidget == 0) {
        g_dragWidget = this;
        g_dragTargetWidget = 0;
    }
}

void Scroll::on_left_up(int, int)
{
    redraw();
    if (g_dragWidget == this) {
        g_dragWidget = 0;
        on_lose_mouse_capture();
    }
    if (g_dragTargetWidget == this)
        g_dragTargetWidget = 0;
}


void Scroll::on_left_click(int, int)
{
    g_renderingWidget = parent;
    scroll_from_key = 0;
    if (m_inverted == 0) {
        parent->on_scrolled(m_id, m_scrollPos);
        if (m_onStep != 0)
            m_onStep(m_id, m_scrollPos);
    } else {
        parent->on_scrolled(m_id, m_rangeHi - m_scrollPos + m_rangeLo);
        if (m_onStep != 0)
            m_onStep(m_id, m_rangeHi - m_scrollPos + m_rangeLo);
    }
    if (parent->m_onScrollPosChanged != 0)
        parent->m_onScrollPosChanged->play();
    m_drag = -1;
    redraw();
}






int Scroll::on_key_click(int, int vk)
{
    int rc = 0;
    scroll_from_key = 1;
    int w = GetSurfaceWidth();
    int h = GetSurfaceHeight();
    if (w > h) {
        switch (vk) {
        case 0x23: m_scrollPos = m_rangeHi; break;
        case 0x24: m_scrollPos = m_rangeLo; break;
        default: goto done;
        }
    } else {
        switch (vk) {
        case 0x21:
            m_scrollPos = m_scrollPos - m_pageStep;
            if (m_scrollPos < m_rangeLo)
                m_scrollPos = m_rangeLo;
            break;
        case 0x22:
            m_scrollPos = m_scrollPos + m_pageStep;
            if (m_scrollPos > m_rangeHi)
                m_scrollPos = m_rangeHi;
            break;
        default: goto done;
        }
    }
    rc = 1;
    g_renderingWidget = parent;
    if (m_inverted == 0) {
        parent->on_scrolled(m_id, m_scrollPos);
        if (m_onStep)
            m_onStep(m_id, m_scrollPos);
    } else {
        parent->on_scrolled(m_id, m_rangeHi - m_scrollPos + m_rangeLo);
        if (m_onStep)
            m_onStep(m_id, m_rangeHi - m_scrollPos + m_rangeLo);
    }
    {
        WidgetObserver *n = parent->m_onScrollPosChanged;
        if (n)
            n->play();
    }
    m_drag = -1;
    redraw();
done:
    return rc;
}












void Scroll::on_redraw()
{
    Rect r;
    if (graphsys)
        graphsys->set_sprite_scale(1, 1, 1);
    m_arrowNear.redraw();
    m_arrowFar.redraw();
    EraseWholeBackground(m_backColor);
    if (m_bevelColor != -1)
        DrawBevelFrameInRect(GetRect(), (unsigned char)m_bevelColor, (unsigned char)m_bevelColor);
    compute_thumb_rect(&r);
    EraseBackground(&r, m_backColor);
    if (m_trackSprite)
        TileSprite(m_trackSprite, 0, 0, 0, 0, GetSurfaceWidth(), GetSurfaceHeight());
    if (m_thumbSprite) {
        SpriteObject *s = m_thumbSprite;
        s->BlitScaled(this, r.left + (r.right - s->m_width - r.left) / 2, r.top + (r.bottom - s->m_height - r.top) / 2,
            0);
    }
    for (int i = 0; i < m_bevelDepth; ++i) {
        DrawBevelFrameInRect(&r, m_bevelHiColor, m_bevelLoColor);
        r.left++;
        r.right--;
        r.top++;
        r.bottom--;
    }
}





















void Scroll::on_resize(int w, int h)
{
    m_extent = (w > h) ? h : w;
    set_thumb_rect();

    if (m_bevelColor == -1) {
        m_arrowFar.resize(m_extent, m_extent, 0, 0);
        m_arrowFar.move((w > h) ? w - m_extent : 0, (w > h) ? 0 : h - m_extent);
    } else {
        m_arrowFar.resize(m_extent - 2, m_extent - 2, 0, 0);
        m_arrowFar.move((w > h) ? w - m_extent + 1 : 1, (w > h) ? 1 : h - m_extent + 1);
    }
}






int Scroll::on_button_pressed(int id)
{
    unsigned long start = GetMilliseconds();
    if (m_firstStepTime != 0) {
        if ((int)GetMilliseconds() - m_firstStepTime < m_repeatDelay)
            return 1;
    }
    if (m_firstStepTime == 0)
        m_firstStepTime = (int)GetMilliseconds();
    scroll_from_key = 0;
    int prev = m_scrollPos;
    switch (id) {
    case 1:
        if (m_scrollPos > m_rangeLo)
            m_scrollPos = m_scrollPos - 1;
        break;
    case 2:
        if (m_scrollPos < m_rangeHi)
            m_scrollPos = m_scrollPos + 1;
        break;
    }
    g_renderingWidget = parent;
    if (prev != m_scrollPos) {
        if (m_inverted == 0) {
            parent->on_scrolling(m_id, m_scrollPos);
            if (m_onRepeat != 0)
                m_onRepeat(m_id, m_scrollPos);
        } else {
            parent->on_scrolling(m_id, m_rangeLo - m_scrollPos + m_rangeHi);
            if (m_onRepeat != 0)
                m_onRepeat(m_id, m_rangeLo - m_scrollPos + m_rangeHi);
        }
        if (parent->m_onScrolling != 0)
            parent->m_onScrolling->play();
    }
    m_drag = -1;
    redraw();
    unsigned long spent = GetMilliseconds() - start;
    unsigned long budget = 1000 / m_rate;
    if (spent < budget)
        Sleep(budget - spent);
    return 1;
}

void Scroll::on_button_clicked(int id)
{
    m_firstStepTime = 0;
    g_renderingWidget = parent;
    scroll_from_key = 0;
    switch (id) {
    case 1:
        if (m_scrollPos > m_rangeLo)
            m_scrollPos = m_scrollPos - 1;
        break;
    case 2:
        if (m_scrollPos < m_rangeHi)
            m_scrollPos = m_scrollPos + 1;
        break;
    }
    if (m_inverted == 0) {
        parent->on_scrolled(m_id, m_scrollPos);
        if (m_onStep != 0)
            m_onStep(m_id, m_scrollPos);
    } else {
        parent->on_scrolled(m_id, m_rangeHi - m_scrollPos + m_rangeLo);
        if (m_onStep != 0)
            m_onStep(m_id, m_rangeHi - m_scrollPos + m_rangeLo);
    }
    if (parent->m_onScrollPosChanged != 0)
        parent->m_onScrollPosChanged->play();
    m_drag = -1;
    redraw();
}

#define SMIN(a, b) ((a) < (b) ? (a) : (b))
#define SMAX(a, b) ((a) > (b) ? (a) : (b))
















void Scroll::compute_thumb_rect(Rect *out)
{
    *out = m_thumb;

    int dy = -out->top;
    int dx = -out->left;
    out->left += dx;
    out->right += dx;
    out->top += dy;
    out->bottom += dy;

    if (m_bevelColor != -1) {
        out->left++;
        out->right++;
        out->top++;
        out->bottom++;
    }

    int lo = out->left;
    int w = out->right - lo;

    int &dg = m_drag;
    if (dg == -1) {
        int ofs;
        int d;
        if (m_flags & 2) {
            ofs = 0;
            d = -1 - w;
        } else {
            ofs = w + 1;
            d = -1 - w * 3;
        }
        if (m_bevelColor != -1)
            d -= 2;

        if (GetSurfaceWidth() > GetSurfaceHeight()) {
            int len = GetSurfaceWidth();
            if (m_rangeHi != m_rangeLo)
                ofs += (m_scrollPos - m_rangeLo) * (len + d) / (m_rangeHi - m_rangeLo);
            out->left += ofs;
            out->right += ofs;
        } else {
            int len = GetSurfaceHeight();
            if (m_rangeHi != m_rangeLo)
                ofs += (m_scrollPos - m_rangeLo) * (len + d) / (m_rangeHi - m_rangeLo);
            out->top += ofs;
            out->bottom += ofs;
        }
    } else {
        int v;
        if (m_flags & 2) {
            int e = out->left + w;
            dg += w >> 1;
            if (GetSurfaceWidth() > GetSurfaceHeight()) {
                v = SMAX(0, SMIN(dg, GetSurfaceWidth() - e));
                dg = v;
                out->left += v;
                out->right += v;
            } else {
                v = SMAX(0, SMIN(dg, GetSurfaceHeight() - e));
                dg = v;
                out->top += v;
                out->bottom += v;
            }
        } else {
            int e = out->left + w;
            dg += w >> 1;
            if (GetSurfaceWidth() > GetSurfaceHeight()) {
                v = SMAX(e, SMIN(dg, GetSurfaceWidth() - e * 2));
                dg = v;
                out->left += v;
                out->right += v;
            } else {
                v = SMAX(e, SMIN(dg, GetSurfaceHeight() - e * 2));
                dg = v;
                out->top += v;
                out->bottom += v;
            }
        }
        dg = -1;
    }

    m_thumb = *out;
}


static __inline void SetRect(Rect *r, int l, int t, int rt, int b)
{
    r->left = l;
    r->top = t;
    r->right = rt;
    r->bottom = b;
}

static __inline void InflateRect(Rect *r, int dx, int dy)
{
    r->left -= dx;
    r->right += dx;
    r->top -= dy;
    r->bottom += dy;
}

void Scroll::set_thumb_rect()
{
    SetRect(&m_thumb, 0, 0, m_extent, m_extent);
    if (m_bevelColor != -1) {
        InflateRect(&m_thumb, -1, -1);
    }
}








int g_scrollTheme[11] = {2, -1, 250, -1, 100, 247, 247, 7, 2, 255, 248};
