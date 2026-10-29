#include "jgldefs.h"
#include "crt.h"
#include "WidgetBase.h"
#include "Heap.h"
#include "Scroll.h"
#include "Network.h"
#include "AutoSoundCtor.h"
#include "MultiDebug.h"
#include "Display.h"
#include "GameMain.h"
#include "SysMessageHandler.h"
#include "BaseButton.h"
#include "JackalUtil.h"
extern "C" {
WidgetBase *g_renderOrder[512];
WidgetBase *g_toplevelViews[128];
}

extern "C" {
int g_tooltipTextColor = 255;
int g_tooltipBorderColor = 255;
}
int g_defFrameColor = 0xf7;
int g_defCaptionHeight = 0x18;
int g_defTitleShadowColor = -1;
int g_defTitleShadowDx = 1;
int g_defTitleShadowDy = 1;
int g_defBorderThickness = 8;
int g_defBottomBorder = -1;
int g_defBevelColorA = 7;
int g_defCaptionMargin = 2;

int orig_x;
int orig_y;
Rect g_tooltipBbox;
Rect g_mouseRedrawRect;
SysMessageHandler g_windowCallbacks;
AssetSurface WidgetBase::s_mouseSaveSurface;
AssetSurface WidgetBase::s_primaryBuffer;
Rect g_cursorConfineRect;

WidgetBase *g_cursorClipWidget = 0;
WidgetBase *g_rootWidget = 0;
void *g_cursorSprite = 0;
int g_cursorX = 0;
int g_cursorY = 0;
int g_cursorHotX = 0;
int g_cursorHotY = 0;
int g_cursorW = 0;
int g_cursorH = 0;
void (*g_beforeTooltipHook)() = 0;
void *g_tooltipOwner = 0;
char *g_tooltipText = 0;
Font *g_uiFont = 0;
static int s_tooltipSpare[3] = {0};
int g_tooltipFillColor = 0;
AssetSurface *g_paintTarget = 0;
extern "C" {
WidgetBase *g_redrawRoot = 0;
}
int g_paintTargetX = 0;
int g_paintTargetY = 0;
extern "C" {
int g_renderOrderResetFlag = 0;
}
WidgetBase *g_hoverWidget = 0;
static int s_hoverSpare[2] = {0};
void(__stdcall *g_afterInputHook)() = 0;
void(__cdecl *g_keyDownHook)(int key) = 0;
void(__cdecl *g_lButtonDownHook)(int, int) = 0;
void(__cdecl *g_rButtonDownHook)(int, int) = 0;
void (*g_buttonIdMinus1Hook)() = 0;
void (*g_buttonIdMinus2Hook)() = 0;
void (*g_buttonHoverHook)() = 0;
int g_pointInFrame = 0;
int g_resizeHit = 0;
WidgetBase *g_resizingWidget = 0;
int g_resizeGrabX = 0;
int g_resizeGrabY = 0;
void *g_renderingWidget = 0;
WidgetBase *g_dragWidget = 0;
WidgetBase *g_dragTargetWidget = 0;
WidgetBase *g_focusWidget = 0;
WidgetBase *g_activeWidget = 0;
WidgetBase *g_mouseDownWidget = 0;
WidgetBase *g_mouseDownWidgetAlt = 0;
int g_leftButtonPress = 0;
int g_presentingRegion = 0;
WidgetBase *g_modalFocus = 0;
WidgetBase *g_modalWidget = 0;
int g_modalDepth = 0;
static int s_modalSpare = 0;
WidgetBase *g_fallbackRoot = 0;
AssetSurface *g_defFrameTexture = 0;
Font *g_widgetTitleFont = 0;
int g_themeTitleGlyphColor = 0;
int g_themeBevelColorB = 0;
static int s_themeSpare[5] = {0};
int g_lastKeyDown = 0;
static int s_keySpare = 0;
extern "C" {
int g_renderOrderCount = 0;
int g_topLevelCount = 0;
}
WidgetBase *prev_dialog_focus = 0;
char g_waitActive = 0;
int leave = 0;

char code_buffer[10] = {0};
char *code_mark = code_buffer;



#pragma intrinsic(strcat)
#pragma intrinsic(strlen)



extern "C" void __cdecl ResetManagerModule(void);

void WidgetBase::set_mouse_confines(Rect *r)
{
    if (r) {
        g_cursorConfineRect = *r;
    } else {
        g_cursorConfineRect = m_local;
        RectToScreen(&g_cursorConfineRect);
    }
    g_cursorClipWidget = this;
    if (is_visible()) {
        if (GetForegroundWindow() == graphsys->get_window_handle())
            ClipCursor((const RECT *)&g_cursorConfineRect);
    }
}

void WidgetBase::ReleaseCursorClip()
{
    if (g_cursorClipWidget == this) {
        ClipCursor(0);
        g_cursorClipWidget = 0;
    }
}

void WidgetBase::increase_children()
{
    m_capacity += 10;
    WidgetBase **grown = (WidgetBase **)AllocateBytes(m_capacity * 4);
    for (int i = 0; i < m_childCount; ++i)
        grown[i] = m_children[i];
    FreeIfNotNull(m_children);
    m_children = grown;
}

void WidgetBase::decrease_children()
{
    int c = m_capacity;
    if (c && c < 10)
        return;
    m_capacity -= 10;
    if (m_capacity == 0) {
        FreeIfNotNull(m_children);
        m_children = 0;
        return;
    }
    WidgetBase **nb = (WidgetBase **)AllocateBytes(m_capacity * 4);
    for (int i = 0; i < m_childCount; ++i)
        nb[i] = m_children[i];
    FreeIfNotNull(m_children);
    m_children = nb;
}

WidgetBase::WidgetBase()
{
    m_sizeOwner = this;
    m_capacity = 0;
    m_children = 0;
    m_childCount = 0;
    m_status = 0;
    m_destRecolorLut = 0;
    m_srcDstLut = 0;
    m_blitArg40 = 0;
    m_aspect[0] = 0;
    m_aspect[1] = 0;
    m_minWidth = 0;
    m_minHeight = 0;
    m_cursorImage = 0;
    m_cursorHotX = 0;
    m_cursorHotY = 0;
    m_cursorRes = 0;
    m_cursorId = 0;
    m_unk1f4[4] = 0;
    parent = 0;
    m_peer = 0;
    m_unk21c = 0;
    m_background = 0;
    m_backgroundCentered = 1;
    m_frameTexture = g_defFrameTexture;
    m_frameColor = g_defFrameColor;
    m_captionHeight = g_defCaptionHeight;
    m_captionColors[0] = g_themeTitleGlyphColor;
    m_captionColors[1] = g_defTitleShadowColor;
    m_captionColors[2] = g_defTitleShadowDx;
    m_captionColors[3] = g_defTitleShadowDy;
    m_borderWidth = g_defBorderThickness;
    m_bottomBorder = g_defBottomBorder;
    m_bevelHighlight = g_defBevelColorA;
    m_bevelShadow = g_themeBevelColorB;
    m_captionMargin = g_defCaptionMargin;
    m_captionFont = g_widgetTitleFont;
    m_captionText = 0;
    m_captionIndent = 0;
    m_captionButtons[0] = 0;
    m_captionButtons[1] = 0;
    m_captionButtons[2] = 0;
    vbar = 0;
    hbar = 0;
    m_backgroundColor = 0;
    m_createdLocal.left = 0;
    m_createdLocal.top = 0;
    m_createdLocal.right = 0;
    m_createdLocal.bottom = 0;
    m_local.left = 0;
    m_local.top = 0;
    m_local.right = 0;
    m_local.bottom = 0;
    m_screen.left = 0;
    m_screen.top = 0;
    m_screen.right = 0;
    m_screen.bottom = 0;
    m_topStrip = 0;
    m_bottomStrip = 0;
    m_leftStrip = 0;
    m_rightStrip = 0;
    m_altCaptionHeight = 0;
    m_resizeMargin = 0;
    m_flags = 0;
    m_unk1f4[0] = 1;
    m_unk1f4[1] = 1;
    m_unk1f4[2] = 1;
    m_unk1f4[3] = 1;
    pfnOnLayout = 0;
    pfnOnClose = 0;
    pfnMouseMove = 0;
    pfnLeftDoubleClick = 0;
    pfnRightDoubleClick = 0;
    pfnLeftClick = 0;
    pfnLeftDown = 0;
    pfnLeftUp = 0;
    pfnRightClick = 0;
    pfnRightDown = 0;
    pfnRightUp = 0;
    pfnCharKey = 0;
    pfnKeyDown = 0;
    pfnKeyUp = 0;
    pfnOnCloseButton = 0;
    m_dragCompanion = 0;
    m_renderLatch = 2;
}

void WidgetBase::close()
{
    m_hotspots.shutdown();
    if ((g_mouseDownWidget ? g_mouseDownWidget : g_mouseDownWidgetAlt) == this
        || is_descendant(g_mouseDownWidget ? g_mouseDownWidget : g_mouseDownWidgetAlt)) {
        g_mouseDownWidget = 0;
        g_mouseDownWidgetAlt = 0;
    }
    if (g_dragWidget == this) {
        g_dragWidget = 0;
        on_lose_mouse_capture();
    }
    if (g_dragTargetWidget == this)
        g_dragTargetWidget = 0;
    if (g_tooltipOwner == this)
        clear_bubble_text();
    if (g_hoverWidget == this)
        g_hoverWidget = 0;
    hide();
    if (vbar) {
        if (m_flags & 0x40)
            vbar = (Scroll *)on_scroll_delete((int)vbar);
        if (vbar) {
            delete vbar;
            vbar = 0;
        }
    }
    if (hbar) {
        if (m_flags & 0x40)
            hbar = (Scroll *)on_scroll_delete((int)hbar);
        if (hbar) {
            delete hbar;
            hbar = 0;
        }
    }
    if (m_flags & 0x100000)
        remove_parent_dialog();
    for (int i = 0; i < m_childCount; ++i)
        m_children[i]->close();
    if (parent)
        parent->remove_child(this);
    else
        remove_parent(this);
    parent = 0;
    m_unk21c = 0;
    m_aspect[0] = 0;
    m_aspect[1] = 0;
    m_minWidth = 0;
    m_minHeight = 0;
    m_sizeOwner = this;
    m_cursorImage = 0;
    m_cursorHotX = 0;
    m_cursorHotY = 0;
    m_cursorId = 0;
    m_cursorRes = 0;
    m_unk1f4[4] = 0;
    m_peer = 0;
    m_childList.RemoveAll();
    m_background = 0;
    m_backgroundCentered = 1;
    m_frameTexture = g_defFrameTexture;
    m_frameColor = g_defFrameColor;
    m_captionColors[0] = g_themeTitleGlyphColor;
    m_captionColors[1] = g_defTitleShadowColor;
    m_captionColors[2] = g_defTitleShadowDx;
    m_captionColors[3] = g_defTitleShadowDy;
    m_captionHeight = g_defCaptionHeight;
    m_borderWidth = g_defBorderThickness;
    m_bottomBorder = g_defBottomBorder;
    m_bevelHighlight = g_defBevelColorA;
    m_bevelShadow = g_themeBevelColorB;
    m_captionMargin = g_defCaptionMargin;
    m_captionFont = g_widgetTitleFont;
    if (m_captionText) {
        FreeIfNotNull(m_captionText);
        m_captionText = 0;
    }
    if (m_captionButtons[0]) {
        delete m_captionButtons[0];
        m_captionButtons[0] = 0;
    }
    if (m_captionButtons[1]) {
        delete m_captionButtons[1];
        m_captionButtons[1] = 0;
    }
    if (m_captionButtons[2]) {
        delete m_captionButtons[2];
        m_captionButtons[2] = 0;
    }
    m_backgroundColor = 0;
    m_flags = 0;
    m_unk1f4[0] = 1;
    m_unk1f4[1] = 1;
    m_unk1f4[2] = 1;
    m_unk1f4[3] = 1;
    m_createdLocal.SetRect(0, 0, 0, 0);
    m_local.SetRect(0, 0, 0, 0);
    m_screen.SetRect(0, 0, 0, 0);
    m_altCaptionHeight = 0;
    m_resizeMargin = 0;
    pfnOnLayout = 0;
    pfnOnClose = 0;
    pfnMouseMove = 0;
    pfnLeftDoubleClick = 0;
    pfnRightDoubleClick = 0;
    pfnLeftClick = 0;
    pfnLeftDown = 0;
    pfnLeftUp = 0;
    pfnRightClick = 0;
    pfnRightDown = 0;
    pfnRightUp = 0;
    pfnCharKey = 0;
    pfnKeyDown = 0;
    pfnKeyUp = 0;
    pfnOnCloseButton = 0;
    m_renderLatch = 2;
    m_status = 0;
    m_destRecolorLut = 0;
    m_srcDstLut = 0;
    m_blitArg40 = 0;
    m_childCount = 0;
    if (m_topStrip) {
        delete m_topStrip;
        m_topStrip = 0;
    }
    if (m_rightStrip) {
        delete m_rightStrip;
        m_rightStrip = 0;
    }
    if (m_leftStrip) {
        delete m_leftStrip;
        m_leftStrip = 0;
    }
    if (m_bottomStrip) {
        delete m_bottomStrip;
        m_bottomStrip = 0;
    }
    ResetToDefaults();
}

WidgetBase::~WidgetBase()
{
    close();
}

int WidgetBase::init(int x, int y, int cx, int cy, const char *caption, unsigned flags, GraphicWin *owner, Menu *peer,
    int *borders)
{
    close();
    if (borders) {
        m_captionHeight = borders[0];
        m_borderWidth = borders[1];
        m_bottomBorder = borders[2];
    }
    m_renderLatch |= 2;

    m_topStrip = new AssetSurface;
    if (!m_topStrip)
        return 4;
    m_rightStrip = new AssetSurface;
    if (!m_rightStrip)
        return 4;
    m_leftStrip = new AssetSurface;
    if (!m_leftStrip)
        return 4;
    m_bottomStrip = new AssetSurface;
    if (!m_bottomStrip)
        return 4;

    if (flags & 0xf0000000) {
        if (!is_graphicwin())
            return 3;
    }
    if (flags & 0x80000)
        flags |= 0x20;

    if (x < 0) {
        if (owner)
            x += owner->m_local.right - owner->m_local.left - cx;
        else
            x += (!graphsys ? 0 : graphsys->screen_width()) - cx;
    }
    if (y < 0) {
        if (owner)
            y += owner->m_local.bottom - owner->m_local.top - cy;
        else
            y += (!graphsys ? 0 : graphsys->screen_height()) - cy;
    }
    if (!owner && g_rootWidget)
        owner = (GraphicWin *)g_rootWidget;

    m_flags = flags;
    parent = owner;
    m_peer = peer;
    if (caption && *caption && !(flags & 0x400080))
        m_flags = flags | 0x10;

    if (owner) {
        owner->add_child(this);
    } else {
        if (g_topLevelCount == MAX_PARENTS) {
            close();
            MessageBoxA(0, "Too many parents", "Increase #define MAX_PARENTS", 0);
            exit(4);
        }
        add_parent(this);
    }

    if (!(m_flags & 0x80) && (m_flags & 0x1d)) {
        m_status |= 2;
    } else {
        m_status &= ~2;
        m_flags &= 0xfffffbee;
    }

    if (!(m_flags & 0x800) && (m_status & 2)) {
        m_screen.SetRect(x, y, cx, cy);
        m_local = m_screen;
        ShrinkRectForBorders(&m_local);
        m_local.Offset(-m_screen.left, -m_screen.top);
    } else {
        m_local.SetRect(x, y, cx, cy);
        if (m_status & 2) {
            m_screen = m_local;
            ExpandRectForBorders(&m_screen);
            m_local.Offset(-m_screen.left, -m_screen.top);
        }
    }
    m_createdLocal = m_local;

    set_caption(caption);
    set_rects();

    if (peer) {
        int rc = peer->init(this);
        if (rc) {
            close();
            return rc;
        }
    }

    {
        int h;
        if (m_flags & 0x10) {
            h = m_captionHeight;
            m_altCaptionHeight = h;
        } else if (!(m_flags & 0x20)) {
            h = m_borderWidth;
            m_altCaptionHeight = h;
        }
    }
    if (m_flags & 0x400)
        m_resizeMargin = m_borderWidth;
    else
        m_resizeMargin = 0;

    if (m_flags & 4) {
        if (m_flags & 0x40)
            hbar = (Scroll *)on_scroll_create();
        if (!hbar) {
            Scroll *bar = new Scroll;
            hbar = bar;
            if (!bar)
                return 4;
            int top = m_local.bottom - g_scrollbarThickness;
            bar->init_horz_nc(0, top,
                (m_flags & 8) ? m_local.right - m_local.left - g_scrollbarThickness : m_local.right - m_local.left,
                this, -2);
            hbar->show(0);
        }
    }
    if (m_flags & 8) {
        if (m_flags & 0x40)
            vbar = (Scroll *)on_scroll_create();
        if (!vbar) {
            Scroll *bar = new Scroll;
            vbar = bar;
            if (!bar)
                return 4;
            int left = m_local.right - g_scrollbarThickness;
            bar->init_vert_nc(left, 0,
                (m_flags & 4) ? m_local.bottom - m_local.top - g_scrollbarThickness : m_local.bottom - m_local.top,
                this, -1);
            vbar->show(0);
        }
    }

    do_caption_buttons();
    g_renderingWidget = this;
    if (m_flags & 0x100000)
        set_parent_dialog(owner);
    m_status |= 4;
    return 0;
}

int WidgetBase::SetCursorImage(SpriteObject *image, int hotX, int hotY)
{
    if (image != m_cursorImage) {
        if (image) {
            m_cursorImage = image;
            m_cursorHotX = hotX;
            m_cursorHotY = hotY;
        }
        UpdateCursor(0, 1, -1, -1);
    }
    return 0;
}

int WidgetBase::set_cursor(int id)
{
    if (id < 0x7f00 || id > 0x7f8a)
        return 3;
    m_cursorImage = 0;
    m_cursorId = id;
    m_cursorRes = 0;
    UpdateCursor(0, 1, -1, -1);
    return 0;
}

void WidgetBase::get_mouse_pos(int *px, int *py)
{
    if (px == 0 || py == 0)
        return;
    if (graphsys)
        graphsys->get_cursor_pos(px, py);
    screen_to_client(px, py);
    if (this == g_rootWidget) {
        Rect r;
        if (graphsys)
            graphsys->get_window_rect(&r);
        *px -= r.left;
        *py -= r.top;
    }
}

int WidgetBase::get_lbutton_state()
{
    return (GetAsyncKeyState(VK_LBUTTON) >> 15) ^ GetSystemMetrics(SM_SWAPBUTTON);
}

static inline void AppendChild(ChildList *list, WidgetBase *w)
{
    if (list->count == 0) {
        list->head = (LLNode *)(list->arena ? list->arena->Alloc(sizeof(LLNode)) : AllocateBytes(sizeof(LLNode)));
        if (!list->head)
            return;
        list->head->data = w;
        list->head->next = list->head;
        list->head->prev = list->head;
        if (list->arena)
            list->head->buf = list->arena->Alloc(4);
        else
            list->head->buf = AllocateBytes(4);
        if (!list->head->buf)
            return;
        list->cur = list->head;
    } else {
        list->head->prev->next =
            (LLNode *)(list->arena ? list->arena->Alloc(sizeof(LLNode)) : AllocateBytes(sizeof(LLNode)));
        if (!list->head->prev->next)
            return;
        list->head->prev->next->prev = list->head->prev;
        list->head->prev->next->next = list->head;
        list->head->prev = list->head->prev->next;
        list->cur = list->head->prev;
        list->cur->data = w;
        if (list->arena)
            list->cur->buf = list->arena->Alloc(4);
        else
            list->cur->buf = AllocateBytes(4);
        if (!list->cur->buf)
            return;
    }
    list->index = list->count;
    list->count++;
}

void WidgetBase::set_parent_dialog(GraphicWin *p)
{
    if (m_flags & 0x200000)
        return;
    if (m_status & 8)
        return;
    if ((m_flags & 0x1000) && !p)
        return;
    if (!(~p->m_flags & 0x1000))
        return;

    AppendChild(&p->m_childList, this);
    if (p->m_childList.count - 1 >= 0) {
        p->m_childList.index = 0;
        p->m_childList.cur = p->m_childList.head;
    }
    parent = p;
}

void WidgetBase::remove_parent_dialog()
{
    WidgetBase *p = parent;
    if (!p)
        return;
    if (!p->m_childList.head)
        return;
    int n = p->m_childList.count;
    for (int i = 0; i < n; i++) {
        if (p->m_childList.cur->data == this) {
            p->m_childList.cur->next->prev = p->m_childList.cur->prev;
            p->m_childList.cur->prev->next = p->m_childList.cur->next;
            LLNode *nd = p->m_childList.cur;
            if (nd == p->m_childList.head)
                p->m_childList.head = nd->next;
            p->m_childList.cur = nd->next;
            if (!p->m_childList.arena) {
                FreeIfNotNull(nd->buf);
                nd->buf = 0;
                FreeIfNotNull(nd);
            }
            p->m_childList.count--;
            break;
        }
        p->m_childList.cur = p->m_childList.cur->next;
    }
    if (p->m_childList.count == 0)
        p->m_childList.head = 0;
    p->m_childList.index = p->m_childList.count - 1;
}

inline void ChildList::RemoveAll()
{
    if (head) {
        if (!arena) {
            for (int i = 0; i < count; i++) {
                cur = head->next;
                FreeIfNotNull(head->buf);
                head->buf = 0;
                FreeIfNotNull(head);
                head = cur;
            }
        }
        head = 0;
        index = 0;
        count = 0;
    }
    index = 0;
}

inline void ChildList::NotifyFocusChange(WidgetBase *old)
{
    if (old && count > 1)
        old->on_dialog_focus(0);
    WidgetBase *c = head ? cur->data : 0;
    if (c)
        c->on_dialog_focus(1);
}

void WidgetBase::set_dialog_focus(WidgetBase *w)
{
    if (!w)
        return;
    if (!m_childList.count)
        return;
    WidgetBase *old = m_childList.head ? m_childList.cur->data : 0;
    if (w == old)
        return;
    prev_dialog_focus = old;
    if (!m_childList.head)
        return;
    int n = m_childList.count;
    m_childList.index = 0;
    m_childList.cur = m_childList.head;
    for (int i = 0; i < n; i++) {
        if (m_childList.cur->data == w) {
            m_childList.NotifyFocusChange(old);
            break;
        }
        m_childList.index++;
        m_childList.cur = m_childList.cur->next;
    }
}

void WidgetBase::pass_dialog_focus()
{
    if (!parent)
        return;
    if (parent->m_childList.count <= 1)
        return;
    prev_dialog_focus = parent->m_childList.GetCurrent();
    for (int i = 0; i < parent->m_childList.count; i++) {
        parent->m_childList.Advance();
        if (parent->m_childList.GetCurrent()->is_visible())
            break;
    }
    if (parent->m_childList.GetCurrent() != prev_dialog_focus) {
        on_dialog_focus(0);
        WidgetBase *c = parent->m_childList.GetCurrent();
        if (c)
            c->on_dialog_focus(1);
    }
}

int WidgetBase::is_descendant(WidgetBase *w)
{
    if (w != 0) {
        for (int i = 0; i < m_childCount; i++) {
            WidgetBase *c = m_children[i];
            if (c == w)
                return 1;
            if (c->is_descendant(w))
                return 1;
        }
    }
    return 0;
}

void WidgetBase::RectToScreen(Rect *r)
{
    if (r == 0)
        return;
    int x = 0, y = 0;
    client_to_screen(&x, &y);
    r->Offset(x, y);
}

void WidgetBase::NonclientRectToScreen(Rect *r)
{
    if (r == 0)
        return;
    int x = 0, y = 0;
    nonclient_to_screen(&x, &y);
    r->Offset(x, y);
}

void WidgetBase::client_to_screen(int *px, int *py)
{
    *px += m_screen.left + m_local.left;
    *py += m_screen.top + m_local.top;
    if ((m_flags & 0x20) && parent != 0) {
        parent->client_to_screen(px, py);
        if (m_flags & 0x8000) {
            *px -= parent->m_local.left;
            *py -= parent->m_local.top;
        }
    }
}

void WidgetBase::screen_to_client(int *px, int *py)
{
    *px -= m_screen.left + m_local.left;
    *py -= m_screen.top + m_local.top;
    if ((m_flags & 0x20) && parent != 0) {
        parent->screen_to_client(px, py);
        if (m_flags & 0x8000) {
            *px += parent->m_local.left;
            *py += parent->m_local.top;
        }
    }
}

void WidgetBase::nonclient_to_screen(int *px, int *py)
{
    client_to_screen(px, py);
    *px -= m_local.left;
    *py -= m_local.top;
}

void WidgetBase::screen_to_nonclient(int *px, int *py)
{
    screen_to_client(px, py);
    *px += m_local.left;
    *py += m_local.top;
}

void WidgetBase::center()
{
    int x, y;
    if (m_status & 2) {
        x = (m_screen.left - m_screen.right + (graphsys == 0 ? 0 : graphsys->screen_width())) / 2;
        y = (m_screen.top - m_screen.bottom + (graphsys == 0 ? 0 : graphsys->screen_height())) / 2;
    } else {
        x = (m_local.left - m_local.right + (graphsys == 0 ? 0 : graphsys->screen_width())) / 2;
        y = (m_local.top - m_local.bottom + (graphsys == 0 ? 0 : graphsys->screen_height())) / 2;
    }
    if (parent)
        parent->screen_to_client(&x, &y);
    move(x, y);
}

int WidgetBase::move(int x, int y)
{
    if (m_status & 2)
        m_screen.Offset(x - m_screen.left, y - m_screen.top);
    else
        m_local.Offset(x - m_local.left, y - m_local.top);
    recurse_clips(this, &m_clip);
    return 0;
}

int WidgetBase::resize(int cx, int cy, int notify, int depth)
{
    if (!is_graphicwin())
        on_check_size(&cx, &cy, &notify);

    if (notify != 0 && (m_status & 2) == 0) {
        m_local.SetRect(m_local.left, m_local.top, cx, cy);
    } else {
        if (notify != 0) {
            client_to_nonclient(&cx, &cy);
            notify = 0;
        }
        m_screen.SetRect(m_screen.left, m_screen.top, cx, cy);
        m_local = m_screen;
        Rect *r = &m_local;
        int dy = -m_local.top;
        int dx = -m_local.left;
        r->left += dx;
        r->right += dx;
        r->top += dy;
        r->bottom += dy;
        ShrinkRectForBorders(r);
    }

    set_rects();
    reset_window_clip();
    recurse_clips(this, &m_clip);
    if (m_sizeOwner != this)
        m_sizeOwner->resize(cx, cy, 0, 0);
    resize_event(m_local.right - m_local.left, m_local.bottom - m_local.top);
    if (graphsys)
        graphsys->invalidate_rect(0);
    on_redraw();
    return 0;
}

void WidgetBase::show(int flags)
{
    if (m_status & 1)
        return;
    if (!(m_status & 4))
        return;
    m_renderLatch |= 2;
    if (!(m_flags & 2)) {
        g_mouseDownWidget = 0;
        g_mouseDownWidgetAlt = 0;
    }
    m_status |= 1;
    if (!(flags & 4))
        bring_to_top();
    UpdateAllWidgetRenderOrder();
    if (!(flags & 2))
        resize_event(RectWidth(&m_local), RectHeight(&m_local));
    if (!(flags & 1))
        on_redraw();
    if (m_peer != 0)
        m_peer->redraw();
    if (m_childList.count != 0 && m_childList.head != 0 && m_childList.cur->data != 0)
        m_childList.cur->data->on_dialog_focus(1);
    Rect r;
    if (m_status & 2) {
        r = m_screen;
        int dy = -r.top;
        int dx = -r.left;
        r.left += dx;
        r.right += dx;
        r.top += dy;
        r.bottom += dy;
        NonclientRectToScreen(&r);
    } else {
        r = m_local;
        int dy = -r.top;
        int dx = -r.left;
        r.left += dx;
        r.right += dx;
        r.top += dy;
        r.bottom += dy;
        RectToScreen(&r);
    }
    if (graphsys != 0)
        graphsys->invalidate_rect(&r);
    if (m_onShown != 0)
        m_onShown->play();
    if (g_cursorClipWidget == this)
        set_mouse_confines(&g_cursorConfineRect);
}

void WidgetBase::hide()
{
    on_hide();
    if (g_cursorClipWidget == this)
        ReleaseCursorClip();
    if (g_tooltipOwner == this)
        clear_bubble_text();
    if ((g_mouseDownWidget ? g_mouseDownWidget : g_mouseDownWidgetAlt) == this
        || is_descendant(g_mouseDownWidget ? g_mouseDownWidget : g_mouseDownWidgetAlt)) {
        g_mouseDownWidget = 0;
        g_mouseDownWidgetAlt = 0;
    }
    if (g_dragWidget == this) {
        g_dragWidget = 0;
        on_lose_mouse_capture();
    }
    if (g_dragTargetWidget == this)
        g_dragTargetWidget = 0;
    if (m_status & 1) {
        m_status &= ~1;
        if (g_modalWidget == this)
            release_modal();
        UpdateAllWidgetRenderOrder();
        if (graphsys)
            graphsys->invalidate_rect(0);
        if (m_onHidden)
            m_onHidden->play();
    }
}

void WidgetBase::set_caption(const char *text)
{
    if (m_captionText) {
        FreeIfNotNull(m_captionText);
        m_captionText = 0;
    }
    if (text) {
        m_captionText = (char *)AllocateBytes(strlen(text) + 1);
        if (m_captionText == 0)
            return;
        ClearString(m_captionText);
        AppendString(m_captionText, text);
    }
    if (is_visible()) {
        on_redraw_nc(0);
        on_nc_paint(0, 0);
    }
}

void WidgetBase::reset_window_clip()
{
    Rect r = m_local;
    int dy = -r.top;
    int dx = -r.left;
    r.left += dx;
    r.right += dx;
    r.top += dy;
    r.bottom += dy;
    WidgetBase::set_window_clip(&r);
}

void WidgetBase::set_vert_pos(int pos)
{
    if (vbar)
        vbar->set_pos(pos);
}

void WidgetBase::set_horz_pos(int pos)
{
    if (hbar)
        hbar->set_pos(pos);
}

void WidgetBase::set_vert_range(int lo, int hi)
{
    if (vbar)
        vbar->set_range(lo, hi);
}

void WidgetBase::set_horz_range(int lo, int hi)
{
    if (hbar)
        hbar->set_range(lo, hi);
}

void WidgetBase::set_vert_paging(int n)
{
    if (vbar)
        vbar->m_pageStep = n;
}

void WidgetBase::set_horz_paging(int n)
{
    if (hbar)
        hbar->m_pageStep = n;
}

int WidgetBase::set_modal(int flags, int (*pfnIdle)(), WidgetBase *focus)
{
    int result = 0;
    if (g_modalDepth >= 4)
        return 0;
    WidgetBase *drag = g_dragWidget;
    g_popupStack[g_modalDepth] = this;
    g_modalFocuses[g_modalDepth] = focus;
    g_modalDepth++;
    g_modalWidget = this;
    g_mouseDownWidget = 0;
    g_mouseDownWidgetAlt = 0;
    if (drag != 0 && drag != this) {
        drag->on_lose_mouse_capture();
        g_dragWidget = 0;
    }
    g_focusWidget = 0;
    if (focus != 0) {
        g_modalFocus = focus;
        focus->show(0);
    }
    if ((flags & 2) == 0)
        show(0);
    flush_input();
    SetCursor(LoadCursorA(0, IDC_ARROW));
    if ((flags & 2) == 0) {
        while (g_modalWidget == this) {
            if (!on_modal())
                break;
            if (pfnIdle != 0 && pfnIdle() == 0) {
                if (g_modalWidget != 0)
                    g_modalWidget->release_modal();
                result = 1;
                break;
            }
            wait_task();
        }
    }
    return result;
}

void WidgetBase::release_modal()
{
    int top = g_modalDepth;
    if (top == 0)
        return;
    top--;
    g_modalDepth = top;
    if (g_popupStack[top] != this) {
        int i = 0;
        if (top > 0) {
            WidgetBase **p = g_popupStack;
            while (1) {
                if (*p == this)
                    break;
                i++;
                p++;
                if (i >= top)
                    break;
            }
        }
        if (i == top) {
            g_modalDepth = top + 1;
            return;
        }
        while (i < top) {
            g_popupStack[i] = g_popupStack[i + 1];
            g_modalFocuses[i] = g_modalFocuses[i + 1];
            i++;
        }
        return;
    }
    g_popupStack[top] = 0;
    g_modalFocuses[top] = 0;
    if (top > 0) {
        g_modalFocus = g_modalFocuses[top - 1];
        g_modalWidget = g_popupStack[top - 1];
        return;
    }
    g_modalWidget = 0;
}

void WidgetBase::resize_event(int cx, int cy)
{
    g_renderingWidget = this;
    if ((m_flags & 0x40) == 0) {
        if (vbar) {
            vbar->move(m_local.right - m_local.left, 0);
            if (hbar) {
                int hh = hbar->GetSurfaceHeight();
                int vw = vbar->GetSurfaceWidth();
                vbar->resize(vw, m_local.bottom - m_local.top - hh, 0, 0);
            } else {
                int vw = vbar->GetSurfaceWidth();
                vbar->resize(vw, m_local.bottom - m_local.top, 0, 0);
            }
        }
        if (hbar) {
            hbar->move(0, m_local.bottom - m_local.top);
            if (vbar) {
                int hh = hbar->GetSurfaceHeight();
                int vw = vbar->GetSurfaceWidth();
                hbar->resize(m_local.right - m_local.left - vw, hh, 0, 0);
            } else {
                int hh = hbar->GetSurfaceHeight();
                hbar->resize(m_local.right - m_local.left, hh, 0, 0);
            }
        }
    }
    if (pfnOnLayout)
        pfnOnLayout(cx, cy);
    on_resize(cx, cy);
    redo_caption_buttons();
}

void WidgetBase::mouse_move_event(int x, int y, int alt)
{
    if (m_flags & 0x200000)
        return;
    if (m_status & 8)
        return;
    if (alt == 0) {
        g_renderingWidget = this;
        Rect rc;
        int id, param;
        int handled;
        if (pfnMouseMove)
            pfnMouseMove(x, y);
        if (m_hotspots.check(x, y, &id, &param, &rc) >= 0) {
            handled = on_spot_move(id, param, &rc);
            m_hotspots.on_move(id, param, this);
        } else {
            m_hotspots.on_leave();
            handled = 0;
        }
        if (!handled)
            on_mouse_move(x, y);
        if (m_onMouseMove)
            m_onMouseMove->play();
        return;
    }
    OnMouseMoveAlt(x, y);
    if (m_onMouseMoveAlt)
        m_onMouseMoveAlt->play();
}

void WidgetBase::left_double_click_event(int x, int y, int alt)
{
    if (m_flags & 0x200000)
        return;
    if (m_status & 8)
        return;
    if (alt == 0) {
        g_renderingWidget = this;
        Rect rc;
        int id, param;
        if (pfnLeftDoubleClick)
            pfnLeftDoubleClick(x, y);
        if (m_hotspots.check(x, y, &id, &param, &rc) < 0 || !on_spot_left_double_click(id, param, &rc))
            on_left_double_click(x, y);
        if (m_onLeftDoubleClick)
            m_onLeftDoubleClick->play();
        return;
    }
    OnLeftDoubleClickAlt(x, y);
    if (m_onLeftDoubleClickAlt)
        m_onLeftDoubleClickAlt->play();
}

void WidgetBase::right_double_click_event(int x, int y, int alt)
{
    if (m_flags & 0x200000)
        return;
    if (m_status & 8)
        return;
    if (alt == 0) {
        g_renderingWidget = this;
        Rect rc;
        int id, param;
        if (pfnRightDoubleClick)
            pfnRightDoubleClick(x, y);
        if (m_hotspots.check(x, y, &id, &param, &rc) < 0 || !on_spot_right_double_click(id, param, &rc))
            on_right_double_click(x, y);
        if (m_onRightDoubleClick)
            m_onRightDoubleClick->play();
        return;
    }
    OnRightDoubleClickAlt(x, y);
    if (m_onRightDoubleClickAlt)
        m_onRightDoubleClickAlt->play();
}

void WidgetBase::left_click_event(int x, int y, int alt)
{
    if (m_flags & 0x200000)
        return;
    if (m_status & 8)
        return;
    if (alt == 0) {
        g_renderingWidget = this;
        Rect rc;
        int id, param;
        if (pfnLeftClick)
            pfnLeftClick(x, y);
        if (m_hotspots.check(x, y, &id, &param, &rc) < 0 || !on_spot_left_click(id, param, &rc))
            on_left_click(x, y);
        if (m_onLeftClick)
            m_onLeftClick->play();
        return;
    }
    OnLeftClickAlt(x, y);
    if (m_onLeftClickAlt)
        m_onLeftClickAlt->play();
}

void WidgetBase::left_down_event(int x, int y, int alt)
{
    if (m_flags & 0x200000)
        return;
    if (m_status & 8)
        return;
    if (alt == 0) {
        g_renderingWidget = this;
        if (pfnLeftDown)
            pfnLeftDown(x, y);
    }
    if (g_lButtonDownHook)
        g_lButtonDownHook(x, y);
    if (alt == 0) {
        Rect rc;
        int id, param;
        if (m_hotspots.check(x, y, &id, &param, &rc) < 0 || !on_spot_left_down(id, param, &rc))
            on_left_down(x, y);
        if (m_onLeftDown)
            m_onLeftDown->play();
        return;
    }
    OnLeftDownAlt(x, y);
    if (m_onLeftDownAlt)
        m_onLeftDownAlt->play();
}

void WidgetBase::left_up_event(int x, int y, int alt)
{
    if (m_flags & 0x200000)
        return;
    if (m_status & 8)
        return;
    if (alt == 0) {
        g_renderingWidget = this;
        Rect rc;
        int id, param;
        if (pfnLeftUp)
            pfnLeftUp(x, y);
        if (m_hotspots.check(x, y, &id, &param, &rc) < 0 || !on_spot_left_up(id, param, &rc))
            on_left_up(x, y);
        if (m_onLeftUp)
            m_onLeftUp->play();
        return;
    }
    OnLeftUpAlt(x, y);
    if (m_onLeftUpAlt)
        m_onLeftUpAlt->play();
}

void WidgetBase::right_click_event(int x, int y, int alt)
{
    if (m_flags & 0x200000)
        return;
    if (m_status & 8)
        return;
    if (alt == 0) {
        g_renderingWidget = this;
        Rect rc;
        int id, param;
        if (pfnRightClick)
            pfnRightClick(x, y);
        if (m_hotspots.check(x, y, &id, &param, &rc) < 0 || !on_spot_right_click(id, param, &rc))
            on_right_click(x, y);
        if (m_onRightClick)
            m_onRightClick->play();
        return;
    }
    OnRightClickAlt(x, y);
    if (m_onRightClickAlt)
        m_onRightClickAlt->play();
}

void WidgetBase::right_down_event(int x, int y, int alt)
{
    if (m_flags & 0x200000)
        return;
    if (m_status & 8)
        return;
    if (alt == 0) {
        g_renderingWidget = this;
        Rect rc;
        int id, param;
        if (pfnRightDown)
            pfnRightDown(x, y);
        if (m_hotspots.check(x, y, &id, &param, &rc) < 0 || !on_spot_right_down(id, param, &rc))
            on_right_down(x, y);
        if (m_onRightDown)
            m_onRightDown->play();
        return;
    }
    OnRightDownAlt(x, y);
    if (m_onRightDownAlt)
        m_onRightDownAlt->play();
}

void WidgetBase::right_up_event(int x, int y, int alt)
{
    if (m_flags & 0x200000)
        return;
    if (m_status & 8)
        return;
    if (alt == 0) {
        g_renderingWidget = this;
        Rect rc;
        int id, param;
        if (pfnRightUp)
            pfnRightUp(x, y);
        if (m_hotspots.check(x, y, &id, &param, &rc) < 0 || !on_spot_right_up(id, param, &rc))
            on_right_up(x, y);
        if (m_onRightUp)
            m_onRightUp->play();
        return;
    }
    OnRightUpAlt(x, y);
    if (m_onRightUpAlt)
        m_onRightUpAlt->play();
}

int WidgetBase::key_click_event(int ch, int vk)
{
    int r = 0;
    if ((m_flags & 0x200000) == 0 && (m_status & 8) == 0) {
        switch (vk) {
        case 0xd:
        case 0x1000d: on_button_clicked(-1); break;
        case 0x1b: on_button_clicked(-2); break;
        }
        if (m_childList.count) {
            WidgetBase *w = m_childList.GetCurrent();
            if (vk == 9) {
                w->pass_dialog_focus();
            } else {
                r = w->key_click_event(ch, vk);
                if (r)
                    return 1;
            }
        }
        g_renderingWidget = this;
        if (pfnCharKey)
            r = pfnCharKey(ch, vk);
        r += on_key_click(ch, vk);
        if (m_onCharKey)
            m_onCharKey->play();
        return r;
    }
    return 0;
}

inline int WidgetBase::OfferKeyToBar(GraphicWin *bar, int vk, int *handled)
{
    if (bar) {
        if (bar->on_key_click(0, vk))
            return 1;
        if (*handled == 0) {
            int ch = 0;
            if (vk < 0x60 || vk > 0x69)
                ch = MapVirtualKeyA(vk & 0xfff8ffff, 2) & 0xffff;
            if (ch == 0)
                *handled = key_click_event(0, vk);
        }
    }
    return 0;
}

int WidgetBase::key_down_event(int vk)
{
    int r = 0;
    if ((m_flags & 0x200000) == 0 && (m_status & 8) == 0) {
        int handled = 0;
        if (m_childList.count) {
            WidgetBase *w = m_childList.GetCurrent();
            if (w->OfferKeyToBar(w->vbar, vk, &handled))
                return 1;
            if (w->OfferKeyToBar(w->hbar, vk, &handled))
                return 1;
            if (w->hbar == 0 && w->vbar == 0 && handled == 0) {
                int ch = 0;
                if (vk < 0x60 || vk > 0x69)
                    ch = MapVirtualKeyA(vk & 0xfff8ffff, 2) & 0xffff;
                if (ch == 0 && w->key_click_event(0, vk))
                    return 1;
            }
            r = w->key_down_event(vk);
            if (r)
                return 1;
        }
        if (OfferKeyToBar(vbar, vk, &handled))
            return 1;
        if (OfferKeyToBar(hbar, vk, &handled))
            return 1;
        if (hbar == 0 && vbar == 0 && handled == 0) {
            int ch = 0;
            if (vk < 0x60 || vk > 0x69)
                ch = MapVirtualKeyA(vk & 0xfff8ffff, 2) & 0xffff;
            if (ch == 0 && key_click_event(0, vk))
                return 1;
        }
        g_renderingWidget = this;
        if (pfnKeyDown)
            r = pfnKeyDown(vk);
        r += on_key_down(vk);
        if (m_onKeyDown)
            m_onKeyDown->play();
        return r;
    }
    return 0;
}

int WidgetBase::key_up_event(int vk)
{
    int r = 0;
    if ((m_flags & 0x200000) == 0 && (m_status & 8) == 0) {
        if (m_childList.count) {
            r = m_childList.GetCurrent()->key_up_event(vk);
            if (r)
                return 1;
        }
        g_renderingWidget = this;
        if (pfnKeyUp)
            r = pfnKeyUp(vk);
        r += on_key_up(vk);
        if (m_onKeyUp)
            m_onKeyUp->play();
        return r;
    }
    return 0;
}

void WidgetBase::client_to_nonclient(int *pcx, int *pcy)
{
    if (pcx == 0)
        return;
    if (pcy == 0)
        return;

    if (m_flags & 4)
        *pcy += g_scrollbarThickness;
    if (m_flags & 8)
        *pcx += g_scrollbarThickness;

    if (m_flags & 0x400) {
        *pcx += m_borderWidth * 2;
        *pcy += m_borderWidth * 2;
        if (m_bottomBorder != -1)
            *pcy += m_bottomBorder - m_borderWidth;
    } else if (m_flags & 0x11) {
        *pcx += m_borderWidth * 2;
        *pcy += m_borderWidth * 2;
        if (m_bottomBorder != -1)
            *pcy += m_bottomBorder - m_borderWidth;
    }
    if (m_flags & 0x10)
        *pcy += m_captionHeight - m_borderWidth;

    if (m_peer && !(m_flags & 0x20000000))
        *pcy += m_peer->requested_height();
}

void WidgetBase::nonclient_to_client(int *pcx, int *pcy)
{
    if (pcx == 0)
        return;
    if (pcy == 0)
        return;

    if (m_flags & 4)
        *pcy -= g_scrollbarThickness;
    if (m_flags & 8)
        *pcx -= g_scrollbarThickness;

    if (m_flags & 0x400) {
        *pcx += -m_borderWidth * 2;
        *pcy += -m_borderWidth * 2;
        if (m_bottomBorder != -1)
            *pcy += m_borderWidth - m_bottomBorder;
    } else if (m_flags & 0x11) {
        *pcx += -m_borderWidth * 2;
        *pcy += -m_borderWidth * 2;
        if (m_bottomBorder != -1)
            *pcy += m_borderWidth - m_bottomBorder;
    }
    if (m_flags & 0x10)
        *pcy += m_borderWidth - m_captionHeight;

    if (m_peer && !(m_flags & 0x20000000))
        *pcy -= m_peer->requested_height();
}

void WidgetBase::ExpandRectForBorders(Rect *r)
{
    if (r == 0)
        return;

    if (m_flags & 4)
        r->bottom += g_scrollbarThickness;
    if (m_flags & 8)
        r->right += g_scrollbarThickness;

    if ((m_flags & 0x400) || (m_flags & 0x11)) {
        int bw = m_borderWidth;
        r->left -= bw;
        r->right += bw;
        r->top -= bw;
        r->bottom += bw;
        if (m_bottomBorder != -1)
            r->bottom += m_bottomBorder - m_borderWidth;
    }
    if (m_flags & 0x10)
        r->top += m_borderWidth - m_captionHeight;

    if (m_peer && !(m_flags & 0x20000000))
        r->top -= m_peer->requested_height();
}

void WidgetBase::ShrinkRectForBorders(Rect *r)
{
    if (r == 0)
        return;

    if (m_flags & 4)
        r->bottom -= g_scrollbarThickness;
    if (m_flags & 8)
        r->right -= g_scrollbarThickness;

    int bordered = (m_flags & 0x400) || (m_flags & 0x11);
    if (bordered) {
        int bw = -m_borderWidth;
        r->left -= bw;
        r->right += bw;
        r->top -= bw;
        r->bottom += bw;
    }
    if (bordered && m_bottomBorder != -1)
        r->bottom += m_borderWidth - m_bottomBorder;
    if (m_flags & 0x10)
        r->top += m_captionHeight - m_borderWidth;

    if (m_peer && !(m_flags & 0x20000000))
        r->top += m_peer->requested_height();
}

void WidgetBase::flip(Rect *clip)
{
    if (g_beforeTooltipHook)
        g_beforeTooltipHook();
    if (g_tooltipText != 0) {
        Rect *r = &g_tooltipBbox;
        if (clip)
            WidgetBase::s_primaryBuffer.set_clip(clip);
        WidgetBase::s_primaryBuffer.DrawBevelFrameInRect(r, g_tooltipBorderColor, g_tooltipBorderColor);
        r->left++;
        r->right--;
        r->top++;
        r->bottom--;
        WidgetBase::s_primaryBuffer.Surface_FillRect(r, g_tooltipFillColor);
        WidgetBase::s_primaryBuffer.SetFontsForStyles(g_uiFont, 0, 0, 0);
        WidgetBase::s_primaryBuffer.set_text_color(g_tooltipTextColor, -1, 2, 2);
        if (strchr(g_tooltipText, '^')) {
            int y = r->top + 1;
            char *s = g_tooltipText;
            char *p;
            do {
                p = strchr(s, '^');
                if (p)
                    *p = 0;
                if (s)
                    WidgetBase::s_primaryBuffer.RenderTextCenteredInWidth(s, r->left, y, RectWidth(r), strlen(s));
                {
                    Font *f = g_uiFont;
                    int h;
                    if (f->m_extraLeading >= 0)
                        h = f->m_capHeight + f->m_extraLeading;
                    else
                        h = f->m_lineSpacing;
                    y += h;
                }
                if (p)
                    *p = '^';
                s = p + 1;
            } while (p);
        } else {
            char *t = g_tooltipText;
            if (t)
                WidgetBase::s_primaryBuffer.RenderTextCenteredInRect(t, r, strlen(t));
        }
        r->left--;
        r->right++;
        r->top--;
        r->bottom++;
        if (clip) {
            if (WidgetBase::s_primaryBuffer.m_surface)
                WidgetBase::s_primaryBuffer.m_surface->reset_clip();
        }
    }
    if (g_cursorSprite) {
        if (graphsys)
            graphsys->set_sprite_scale(1, 1, 1);
        ((SpriteObject *)g_cursorSprite)->BlitScaled(&WidgetBase::s_primaryBuffer, g_cursorX, g_cursorY, 0);
    }
    if (graphsys)
        graphsys->flip(clip);
}

int WidgetBase::set_window_clip(Rect *)
{
    return 0;
}

int WidgetBase::init_class(const char *title, int flags)
{
    AssetSurface scratch;
    g_popupStack[0] = 0;
    g_popupStack[1] = 0;
    g_popupStack[2] = 0;
    g_popupStack[3] = 0;
    ResetManagerModule();
    return 0;
}

void WidgetBase::close_class()
{
    if (graphsys)
        graphsys->close_app();
}

void WidgetBase::set_border_thickness(int w)
{
    if (m_status & 2) {
        m_borderWidth = w;
        resize(m_screen.right - m_screen.left, m_screen.bottom - m_screen.top, 0, 0);
    }
}

void WidgetBase::set_bottom_border_thickness(int w)
{
    if (m_status & 2) {
        m_bottomBorder = w;
        resize(m_screen.right - m_screen.left, m_screen.bottom - m_screen.top, 0, 0);
    }
}

void WidgetBase::restore_cursor_back(int flush)
{
    if (g_cursorSprite == 0)
        return;
    Rect r;
    WidgetBase::s_mouseSaveSurface.copy(&WidgetBase::s_primaryBuffer, 0, 0, g_cursorX, g_cursorY, g_cursorW, g_cursorH);
    r.top = g_cursorY;
    r.right = g_cursorX + g_cursorW;
    r.left = g_cursorX;
    r.bottom = r.top + g_cursorH;
    UnionRect((LPRECT)&g_mouseRedrawRect, (const RECT *)&g_mouseRedrawRect, (const RECT *)&r);
    if (flush) {
        void *save = g_cursorSprite;
        g_cursorSprite = 0;
        WidgetBase::flip(&g_mouseRedrawRect);
        g_cursorSprite = save;
        g_mouseRedrawRect.left = 0;
        g_mouseRedrawRect.right = 0;
    }
}

void UpdateCursor(WidgetBase *w, int flush, int x, int y)
{
    int px, py;
    Rect r;
    if (x != -1 && y != -1) {
        px = x;
        py = y;
    } else if (graphsys != 0) {
        graphsys->get_cursor_pos(&px, &py);
    }
    x = px;
    y = py;
    if (w == 0)
        w = FindWidgetAtPoint(&x, &y);
    else
        w->screen_to_nonclient(&x, &y);
    if (w == 0) {
        WidgetBase::restore_cursor_back(1);
        g_cursorSprite = 0;
        SetCursor(LoadCursorA(0, IDC_ARROW));
        return;
    }
    if (g_pointInFrame == 0 && (w->m_flags & 0x4000000) == 0) {
        if (w->m_cursorImage != 0) {
            if (g_cursorSprite == 0) {
                while (ShowCursor(0) >= 0)
                    ;
            } else {
                WidgetBase::restore_cursor_back(0);
            }
            if (w->m_cursorImage != g_cursorSprite) {
                g_cursorW = w->m_cursorImage->m_width;
                g_cursorH = w->m_cursorImage->m_height;
                g_cursorHotX = w->m_cursorHotX;
                g_cursorHotY = w->m_cursorHotY;
                if (WidgetBase::s_mouseSaveSurface.GetSurfaceWidth() != g_cursorW
                    || WidgetBase::s_mouseSaveSurface.GetSurfaceHeight() != g_cursorH)
                    WidgetBase::s_mouseSaveSurface.Create(g_cursorW, g_cursorH, 0, 1, 0, 0);
            }
            g_cursorX = px - g_cursorHotX;
            g_cursorY = py - g_cursorHotY;
            g_cursorSprite = w->m_cursorImage;
            WidgetBase::s_primaryBuffer.copy(&WidgetBase::s_mouseSaveSurface, g_cursorX, g_cursorY, 0, 0, g_cursorW,
                g_cursorH);
            r.top = g_cursorY;
            r.left = g_cursorX;
            r.right = g_cursorW + r.left;
            r.bottom = g_cursorH + r.top;
            UnionRect((LPRECT)&g_mouseRedrawRect, (const RECT *)&r, (const RECT *)&g_mouseRedrawRect);
            if (flush) {
                int save = g_presentingRegion;
                g_presentingRegion = 1;
                WidgetBase::flip(&g_mouseRedrawRect);
                g_presentingRegion = save;
                g_mouseRedrawRect.left = g_mouseRedrawRect.right;
            }
            return;
        }
        if (g_cursorSprite != 0) {
            WidgetBase::restore_cursor_back(1);
            while (ShowCursor(1) <= 0)
                ;
        }
        g_cursorSprite = 0;
        if (w->m_cursorRes != 0 && w->m_cursorRes->m_hcursor != 0)
            SetCursor(w->m_cursorRes->m_hcursor);
        else if (w->m_cursorId != 0)
            SetCursor(LoadCursorA(0, (const char *)w->m_cursorId));
        else
            SetCursor(LoadCursorA(0, IDC_ARROW));
        return;
    }
    switch (w->on_nc_hittest(x, y)) {
    case 0:
        WidgetBase::restore_cursor_back(1);
        g_cursorSprite = 0;
        leave = 1;
        SetCursor(LoadCursorA(0, IDC_ARROW));
        break;
    case 2:
        WidgetBase::restore_cursor_back(1);
        g_cursorSprite = 0;
        leave = 1;
        SetCursor(LoadCursorA(0, IDC_ARROW));
        break;
    case 10:
        WidgetBase::restore_cursor_back(1);
        g_cursorSprite = 0;
        leave = 1;
        SetCursor(LoadCursorA(0, IDC_SIZEWE));
        break;
    case 11:
        WidgetBase::restore_cursor_back(1);
        g_cursorSprite = 0;
        leave = 1;
        SetCursor(LoadCursorA(0, IDC_SIZEWE));
        break;
    case 12:
        WidgetBase::restore_cursor_back(1);
        g_cursorSprite = 0;
        leave = 1;
        SetCursor(LoadCursorA(0, IDC_SIZENS));
        break;
    case 15:
        WidgetBase::restore_cursor_back(1);
        g_cursorSprite = 0;
        leave = 1;
        SetCursor(LoadCursorA(0, IDC_SIZENS));
        break;
    case 14:
        WidgetBase::restore_cursor_back(1);
        g_cursorSprite = 0;
        leave = 1;
        SetCursor(LoadCursorA(0, IDC_SIZENESW));
        break;
    case 16:
        WidgetBase::restore_cursor_back(1);
        g_cursorSprite = 0;
        leave = 1;
        SetCursor(LoadCursorA(0, IDC_SIZENESW));
        break;
    case 13:
        WidgetBase::restore_cursor_back(1);
        g_cursorSprite = 0;
        leave = 1;
        SetCursor(LoadCursorA(0, IDC_SIZENWSE));
        break;
    case 17:
        WidgetBase::restore_cursor_back(1);
        g_cursorSprite = 0;
        leave = 1;
        SetCursor(LoadCursorA(0, IDC_SIZENWSE));
        break;
    case -2: break;
    case -1: break;
    case 1: break;
    default:
        WidgetBase::restore_cursor_back(1);
        g_cursorSprite = 0;
        SetCursor(LoadCursorA(0, IDC_ARROW));
        break;
    }
}

void WidgetBase::do_caption_buttons()
{
    if (m_captionButtons[0]) {
        delete m_captionButtons[0];
        m_captionButtons[0] = 0;
    }
    if (m_captionButtons[1]) {
        delete m_captionButtons[1];
        m_captionButtons[1] = 0;
    }
    if (m_captionButtons[2]) {
        delete m_captionButtons[2];
        m_captionButtons[2] = 0;
    }
}

void WidgetBase::redo_caption_buttons()
{
    int y = m_captionMargin;
    int x = m_screen.right - (m_captionMargin << 1) - m_screen.left - m_borderWidth - m_captionHeight - 2;
    if (m_captionButtons[2]) {
        m_captionButtons[2]->move(x, y);
        x += -3 - m_captionButtons[2]->GetSurfaceWidth();
    }
    if (m_captionButtons[1])
        m_captionButtons[1]->move(x, y);
    if (m_captionButtons[0])
        m_captionButtons[0]->move(x, y);
}

void WidgetBase::set_rects()
{
    reset_window_clip();
    if (m_status & 2) {
        int capH;
        if ((m_flags & 0x10) && !(m_flags & 0x400000))
            capH = m_captionHeight;
        else
            capH = m_borderWidth;

        int botH = m_bottomBorder;
        if (botH == -1)
            botH = m_borderWidth;
        if (botH < 1)
            botH = 1;
        if (capH < 1)
            capH = 1;

        m_topStrip->Create(m_screen.right - m_screen.left, capH, 0, 1, 0, 0);
        m_topStrip->sync_to_palette(g_widgetPalette);
        m_topStrip->ClearToColor(g_transparentColor);

        if (m_bottomBorder != 0) {
            m_bottomStrip->Create(m_screen.right - m_screen.left, botH, 0, 1, 0, 0);
            m_bottomStrip->sync_to_palette(g_widgetPalette);
            m_bottomStrip->ClearToColor(g_transparentColor);
        }
        if (m_borderWidth != 0) {
            int leftH = m_screen.bottom - m_screen.top;
            m_leftStrip->Create(m_borderWidth, leftH - botH - capH, 0, 1, 0, 0);
            m_leftStrip->sync_to_palette(g_widgetPalette);
            m_leftStrip->ClearToColor(g_transparentColor);
            int rightH = m_screen.bottom - m_screen.top;
            m_rightStrip->Create(m_borderWidth, rightH - botH - capH, 0, 1, 0, 0);
            m_rightStrip->sync_to_palette(g_widgetPalette);
            m_rightStrip->ClearToColor(g_transparentColor);
        }
        on_redraw_nc(-1);
    }
}

#include "AutoSoundDtor.h"

int WidgetBase::is_dialog_focus()
{
    if (m_flags & 0x1000)
        return 1;
    if (parent && (parent->m_childList.head ? parent->m_childList.cur->data : 0) == this)
        return 1;
    return 0;
}

void WidgetBase::set_def_focus(WidgetBase *w)
{
    g_fallbackRoot = w;
}

static __inline void InflateRect(Rect *r, int dx, int dy)
{
    r->left -= dx;
    r->right += dx;
    r->top -= dy;
    r->bottom += dy;
}

static __inline void OffsetRect(Rect *r, int dx, int dy)
{
    r->left += dx;
    r->right += dx;
    r->top += dy;
    r->bottom += dy;
}

void WidgetBase::on_redraw_nc(int which)
{
    Rect bounds;
    Rect sunken;
    Rect r;
    int i;

    if (!(m_status & 2))
        return;

    unsigned flags = m_flags;
    if (!(flags & 0x11))
        return;

    int bw;
    if ((flags & 0x10) && !(flags & 0x400000))
        bw = m_captionHeight;
    else
        bw = m_borderWidth;

    int rw;
    if (m_bottomBorder == -1)
        rw = m_borderWidth;
    else
        rw = m_bottomBorder;

    int cw = m_screen.right - m_screen.left;
    int ch = m_screen.bottom - m_screen.top;
    Rect frame;
    frame.SetRect(0, 0, cw, ch);
    int dx = m_local.right - m_local.left;
    int topInset = bw - 1;
    int sideInset = m_borderWidth - 1;
    sunken.left = sideInset;
    sunken.top = topInset;
    sunken.right = dx + sideInset + 2;
    int hh = ch - rw;
    sunken.bottom = hh - bw + topInset + 2;

    if (m_topStrip != 0 && (which == -1 || which == 0)) {
        m_topStrip->GetSurfaceBounds(&bounds);

        if (m_frameTexture)
            m_topStrip->tile(m_frameTexture, 0, 0, 0, 0, cw, ch);
        else
            m_topStrip->ClearToColor(m_frameColor);

        r = frame;
        for (i = 0; i < m_captionMargin; i++) {
            m_topStrip->DrawBevelFrameInRect(&r, m_bevelHighlight, m_bevelShadow);
            r.left++;
            r.right--;
            r.top++;
            r.bottom--;
        }

        r = sunken;
        for (i = 0; i < m_captionMargin; i++) {
            m_topStrip->DrawBevelFrameInRect(&r, m_bevelShadow, m_bevelHighlight);
            r.left--;
            r.right++;
            r.top--;
            r.bottom++;
        }

        if (m_captionText) {
            m_topStrip->GetSurfaceBounds(&r);

            InflateRect(&r, -m_captionMargin, -m_captionMargin);

            m_topStrip->set_text_color(m_captionColors[0], m_captionColors[1], m_captionColors[2], m_captionColors[3]);
            m_topStrip->SetFontsForStyles(m_captionFont, 0, 0, 0);
            m_topStrip->set_clip(&r);
            r.left += m_captionIndent;

            flags = m_flags;
            if (flags & 0x10000) {
                const char *s = m_captionText;
                if (s)
                    m_topStrip->RenderTextVerticallyCentered(s, &r, strlen(s));
            } else if (flags & 0x40000) {
                const char *s = m_captionText;
                if (s)
                    m_topStrip->RenderTextRightAlignedInRectVCentered(s, &r, strlen(s));
            } else {
                const char *s = m_captionText;
                if (s)
                    m_topStrip->RenderTextCenteredInRect(s, &r, strlen(s));
            }
        }

        m_topStrip->ResetSurfaceClip();
        on_post_redraw_nc_buffer(m_topStrip, 0);
    }

    if (m_bottomStrip != 0 && (which == -1 || which == 2)) {
        m_bottomStrip->GetSurfaceBounds(&bounds);

        if (m_frameTexture)
            m_bottomStrip->tile(m_frameTexture, 0, hh, 0, 0, cw, ch);
        else
            m_bottomStrip->ClearToColor(m_frameColor);

        r = frame;
        OffsetRect(&r, 0, rw - (m_screen.bottom - m_screen.top));
        for (i = 0; i < m_captionMargin; i++) {
            m_bottomStrip->DrawBevelFrameInRect(&r, m_bevelHighlight, m_bevelShadow);
            r.left++;
            r.right--;
            r.top++;
            r.bottom--;
        }

        r = sunken;
        OffsetRect(&r, 0, rw - (m_screen.bottom - m_screen.top));
        for (i = 0; i < m_captionMargin; i++) {
            m_bottomStrip->DrawBevelFrameInRect(&r, m_bevelShadow, m_bevelHighlight);
            r.left--;
            r.right++;
            r.top--;
            r.bottom++;
        }

        m_bottomStrip->ResetSurfaceClip();
        on_post_redraw_nc_buffer(m_bottomStrip, 2);
    }

    if (m_leftStrip != 0 && (which == -1 || which == 3)) {
        m_leftStrip->GetSurfaceBounds(&bounds);
        m_leftStrip->ResetSurfaceClip();

        if (m_frameTexture)
            m_leftStrip->tile(m_frameTexture, 0, bw, 0, 0, cw, ch);
        else
            m_leftStrip->ClearToColor(m_frameColor);

        r = frame;
        OffsetRect(&r, 0, -bw);
        for (i = 0; i < m_captionMargin; i++) {
            m_leftStrip->DrawBevelFrameInRect(&r, m_bevelHighlight, m_bevelShadow);
            r.left++;
            r.right--;
            r.top++;
            r.bottom--;
        }

        r = sunken;
        OffsetRect(&r, 0, -bw);
        for (i = 0; i < m_captionMargin; i++) {
            m_leftStrip->DrawBevelFrameInRect(&r, m_bevelShadow, m_bevelHighlight);
            r.left--;
            r.right++;
            r.top--;
            r.bottom++;
        }

        m_leftStrip->ResetSurfaceClip();
        on_post_redraw_nc_buffer(m_leftStrip, 3);
    }

    if (m_rightStrip != 0 && (which == -1 || which == 1)) {
        m_rightStrip->GetSurfaceBounds(&bounds);

        if (m_frameTexture)
            m_rightStrip->tile(m_frameTexture, cw - m_borderWidth, bw, 0, 0, cw, ch);
        else
            m_rightStrip->ClearToColor(m_frameColor);

        r = frame;
        OffsetRect(&r, m_screen.left - m_screen.right + m_borderWidth, -bw);
        for (i = 0; i < m_captionMargin; i++) {
            m_rightStrip->DrawBevelFrameInRect(&r, m_bevelHighlight, m_bevelShadow);
            r.left++;
            r.right--;
            r.top++;
            r.bottom--;
        }

        r = sunken;
        OffsetRect(&r, m_screen.left - m_screen.right + m_borderWidth, -bw);
        for (i = 0; i < m_captionMargin; i++) {
            m_rightStrip->DrawBevelFrameInRect(&r, m_bevelShadow, m_bevelHighlight);
            r.left--;
            r.right++;
            r.top--;
            r.bottom++;
        }

        m_rightStrip->ResetSurfaceClip();
        on_post_redraw_nc_buffer(m_rightStrip, 1);
    }
}

inline int WidgetBase::on_key_click(int, int)
{
    return 0;
}

void WidgetBase::bring_to_top()
{
    if (parent) {
        parent->bring_child_to_top(this);
        return;
    }
    bring_parent_to_top(this);
}

AssetSurface *WidgetBase::redraw_nc_buffer(int which)
{
    on_redraw_nc(which);
    switch (which) {
    case 0: return m_topStrip;
    case 1: return m_rightStrip;
    case 2: return m_bottomStrip;
    case 3: return m_leftStrip;
    }
    return 0;
}

void WidgetBase::recurse_clips(WidgetBase *w, Rect *clip)
{
    Rect frameClip = w->m_screen;
    int dy = -frameClip.top;
    int dx = -frameClip.left;
    frameClip.Offset(dx, dy);
    w->NonclientRectToScreen(&frameClip);

    Rect clientClip = w->m_local;
    int ey = -clientClip.top;
    int ex = -clientClip.left;
    clientClip.Offset(ex, ey);
    w->RectToScreen(&clientClip);

    IntersectRect((LPRECT)&clientClip, (const RECT *)&clientClip, (const RECT *)clip);
    IntersectRect((LPRECT)&frameClip, (const RECT *)&frameClip, (const RECT *)clip);

    for (int i = 0; i < w->m_childCount; i++) {
        if (w->m_children[i]->is_visible()) {
            WidgetBase *c = w->m_children[i];
            WidgetBase::recurse_clips(c, (c->m_flags & 0x8000) ? &frameClip : &clientClip);
        }
    }
    if (w->is_visible()) {
        if (w->m_flags & 0x20) {
            w->m_clip = *clip;
        } else if (WidgetBase::s_primaryBuffer.GetRect()) {
            w->m_clip = *WidgetBase::s_primaryBuffer.GetRect();
        }
    }
}

void WidgetBase::update_screen_clips()
{
    Rect clip;
    if (WidgetBase::s_primaryBuffer.m_surface)
        WidgetBase::s_primaryBuffer.m_surface->SURF_CopyBoundsRect(&clip);
    for (int i = 0; i < g_topLevelCount; i++) {
        WidgetBase *w = g_toplevelViews[i];
        if (w->m_status & 1)
            recurse_clips(w, &clip);
    }
}

extern "C" void recurse_zorder(WidgetBase *w)
{
    int found = 0;
    int i;
    for (i = 0; i < w->m_childCount; i++) {
        if (w->m_children[i]->m_flags & 0x20)
            continue;
        if (g_redrawRoot && g_redrawRoot == w->m_children[i]) {
            g_renderOrderCount = 0;
            found = 1;
        }
        if (w->m_children[i]->is_visible())
            recurse_zorder(w->m_children[i]);
        if (g_redrawRoot && found)
            break;
    }
    if (!(g_redrawRoot && found)) {
        for (i = 0; i < w->m_childCount; i++) {
            if (g_redrawRoot && g_redrawRoot == w->m_children[i]) {
                g_renderOrderCount = 0;
                found = 1;
            }
            if (w->m_sizeOwner == w) {
                if (w->m_children[i]->m_flags & 0x20) {
                    if (w->m_children[i]->is_visible())
                        recurse_zorder(w->m_children[i]);
                }
            }
            if (g_redrawRoot && found)
                break;
        }
    }
    if (w->is_visible()) {
        g_renderOrder[g_renderOrderCount] = w;
        g_renderOrderCount++;
    }
}

extern "C" void UpdateAllWidgetRenderOrder()
{
    g_renderOrderCount = 0;
    for (int i = 0; i < g_topLevelCount; i++) {
        if (g_redrawRoot && g_redrawRoot == g_toplevelViews[i]) {
            g_renderOrderCount = 0;
            g_renderOrderResetFlag = 0;
        }
        if (g_toplevelViews[i]->m_status & 1)
            recurse_zorder(g_toplevelViews[i]);
    }
    WidgetBase::update_screen_clips();
}

void WidgetBase::add_parent(WidgetBase *w)
{
    if (!w)
        return;
    unsigned flags = w->m_flags;
    int count = g_topLevelCount;
    if (flags & 0x02000000) {
        g_toplevelViews[count] = w;
        g_topLevelCount = count + 1;
        return;
    }
    if (count > 0) {
        for (int i = count; i > 0; i--)
            g_toplevelViews[i] = g_toplevelViews[i - 1];
    }
    count++;
    g_toplevelViews[0] = w;
    g_topLevelCount = count;
}

void WidgetBase::remove_parent(WidgetBase *w)
{
    if (!w)
        return;
    int i, n;
    n = g_topLevelCount;
    for (i = 0; i < n; i++) {
        if (g_toplevelViews[i] == w)
            break;
    }
    if (i >= n)
        return;
    g_topLevelCount = n - 1;
    w->hide();
    n = g_topLevelCount;
    while (i < n) {
        g_toplevelViews[i] = g_toplevelViews[i + 1];
        i++;
    }
}

void WidgetBase::bring_parent_to_top(WidgetBase *w)
{
    if (!w)
        return;
    if (w->m_flags & 0x02000000)
        return;
    int i = 0;
    if (g_topLevelCount > 0) {
        {
            WidgetBase **p = g_toplevelViews;
            while (i < g_topLevelCount) {
                if (*p == w)
                    break;
                i++;
                p++;
            }
        }
        if (i >= g_topLevelCount) {
            UpdateAllWidgetRenderOrder();
            return;
        }
        if (i < g_topLevelCount) {
            if (i > 0) {
                WidgetBase **q = &g_toplevelViews[i];
                do {
                    *q = *(q - 1);
                    q += -1;
                    i--;
                } while (i != 0);
            }
            g_toplevelViews[0] = w;
        }
    }
    UpdateAllWidgetRenderOrder();
}

inline void WidgetBase::on_lose_mouse_capture()
{
}
inline int WidgetBase::on_scroll_create()
{
    return 0;
}
inline int WidgetBase::on_listbox_scroll_create()
{
    return 0;
}
inline int WidgetBase::on_dialog_back_draw(WidgetBase *)
{
    return 0;
}
inline void WidgetBase::on_redraw()
{
}
inline void WidgetBase::on_sys_close()
{
}
inline int WidgetBase::on_key_down(int)
{
    return 0;
}
inline int WidgetBase::on_key_up(int)
{
    return 0;
}
inline int WidgetBase::on_button_pressed(int)
{
    return 0;
}
inline void WidgetBase::on_button_up(int)
{
}
inline void WidgetBase::on_button_down(int)
{
}
inline void WidgetBase::on_button_clicked(int)
{
}
inline void WidgetBase::on_menu(int)
{
}
inline void WidgetBase::on_dialog_focus(int)
{
}
inline void WidgetBase::on_closebox()
{
}
inline void WidgetBase::on_hide()
{
}
inline void WidgetBase::on_selected(int)
{
}
inline void WidgetBase::on_double_clicked(int)
{
}
inline GraphicWin *WidgetBase::is_graphicwin()
{
    return 0;
}
inline void WidgetBase::redraw()
{
}
inline void WidgetBase::update()
{
}

void WidgetBase::add_child(WidgetBase *child)
{
    if (!child)
        return;
    if (m_childCount + 1 > m_capacity)
        increase_children();
    if (child->m_flags & 0x2000000) {
        m_children[m_childCount] = child;
        m_childCount++;
    } else {
        for (int i = m_childCount; i > 0; i--)
            m_children[i] = m_children[i - 1];
        m_children[0] = child;
        m_childCount++;
    }
}

void WidgetBase::remove_child(WidgetBase *child)
{
    if (!child)
        return;
    int i;
    for (i = 0; i < m_childCount; i++)
        if (m_children[i] == child)
            break;
    if (i < m_childCount) {
        m_childCount--;
        child->hide();
        for (; i < m_childCount; i++)
            m_children[i] = m_children[i + 1];
    }
    if (m_childCount <= m_capacity - 10)
        decrease_children();
}

void WidgetBase::bring_child_to_top(WidgetBase *child)
{
    if (!child)
        return;
    if (child->m_flags & 0x2000000)
        return;
    int i;
    for (i = 0; i < m_childCount; i++)
        if (m_children[i] == child)
            break;
    if (i < m_childCount) {
        for (; i > 0; i--)
            m_children[i] = m_children[i - 1];
        m_children[0] = child;
    }
    UpdateAllWidgetRenderOrder();
}

static inline AssetSurface *SelectPaintTarget(int *px, int *py, Rect *pr)
{
    AssetSurface *t = g_paintTarget;
    if (t) {
        *px -= g_paintTargetX;
        *py -= g_paintTargetY;
        pr->Offset(-g_paintTargetX, -g_paintTargetY);
        return t;
    }
    return &WidgetBase::s_primaryBuffer;
}

void WidgetBase::on_paint(Rect *clip)
{
    AssetSurface *bg = m_background;
    Rect r = m_local;
    Rect *pr = &r;
    pr->Offset(m_screen.left, m_screen.top);

    if (bg == 0) {
        if (clip) {
            IntersectRect((LPRECT)pr, (const RECT *)pr, (const RECT *)clip);
            s_primaryBuffer.Surface_FillRect(pr, m_backgroundColor);
            return;
        }
        s_primaryBuffer.Surface_FillRect(pr, m_backgroundColor);
        return;
    }

    Rect saved;
    s_primaryBuffer.CopySurfaceClip(&saved);
    Rect cut;
    if (clip) {
        IntersectRect((LPRECT)&cut, (const RECT *)pr, (const RECT *)clip);
        s_primaryBuffer.set_clip(&cut);
    } else {
        s_primaryBuffer.set_clip(pr);
    }

    int w = bg->GetSurfaceWidth();
    int h = bg->GetSurfaceHeight();
    int x = (RectWidth(pr) - w) / 2 + pr->left;
    int y = (RectHeight(pr) - h) / 2 + pr->top;

    if (m_backgroundCentered == 0) {
        if (x > 0)
            x = w - abs(x) % w;
        else
            x = -x;
        if (y > 0)
            y = h - abs(y) % h;
        else
            y = -y;
        s_primaryBuffer.tile(bg, x, y, pr->left, pr->top, RectWidth(pr), RectHeight(pr));
    } else {
        s_primaryBuffer.fill(pr->left, pr->top, RectWidth(pr), y, m_backgroundColor);
        s_primaryBuffer.fill(pr->left, pr->top + y, x, h, m_backgroundColor);
        s_primaryBuffer.fill(pr->left + x + w, pr->top + y, RectWidth(pr) - x - w, h, m_backgroundColor);
        s_primaryBuffer.fill(pr->left, pr->top + y + h, RectWidth(pr), RectHeight(pr) - (y - pr->top) - h,
            m_backgroundColor);
        bg->copy(&s_primaryBuffer, 0, 0, pr->left + x, pr->top + y, bg->GetSurfaceWidth(), bg->GetSurfaceHeight());
    }

    s_primaryBuffer.set_clip(&saved);
}

void WidgetBase::on_nc_paint(Rect *clip, int which)
{
    if (!(m_flags & 0x11) || !is_visible())
        return;
    if (graphsys)
        graphsys->set_sprite_scale(1, 1, 1);

    Rect r;
    int x = 0;
    int y = 0;
    nonclient_to_screen(&x, &y);

    GraphicWin *p = parent;
    if (p && (m_flags & 0x20)) {
        if (m_flags & 0x8000) {
            r = p->m_screen;
            r.Offset(-r.left, -r.top);
            p->NonclientRectToScreen(&r);
        } else {
            r = p->m_local;
            r.Offset(-r.left, -r.top);
            p->RectToScreen(&r);
        }
        if (clip) {
            which = -1;
            if (!IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)clip))
                return;
        }
    } else if (clip) {
        which = -1;
        r = *clip;
    } else if (s_primaryBuffer.m_surface) {
        s_primaryBuffer.m_surface->SURF_CopyBoundsRect(&r);
    }

    AssetSurface *dst = SelectPaintTarget(&x, &y, &r);
    dst->set_clip(&r);

    int border;
    if ((m_flags & 0x10) && !(m_flags & 0x400000))
        border = m_captionHeight;
    else
        border = m_borderWidth;

    if (m_topStrip && (which == -1 || which == 0))
        m_topStrip->draw(dst, x, y);
    if (m_bottomStrip && (which == -1 || which == 2)) {
        int bottom = m_bottomBorder;
        if (bottom == -1)
            bottom = m_borderWidth;
        m_bottomStrip->draw(dst, x, m_screen.bottom - m_screen.top - bottom + y);
    }
    if (m_leftStrip && (which == -1 || which == 3))
        m_leftStrip->draw(dst, x, y + border);
    if (m_rightStrip && (which == -1 || which == 1))
        m_rightStrip->draw(dst, m_screen.right - m_screen.left - m_borderWidth + x, border + y);

    if (dst->m_surface)
        dst->m_surface->reset_clip();
}

int WidgetBase::on_nc_hittest(int x, int y)
{
    if (m_status & 2) {
        if (x < m_resizeMargin && y < m_resizeMargin)
            return 13;
        if (x < m_resizeMargin && y > (m_screen.bottom - m_screen.top) - m_resizeMargin)
            return 16;
        if (x > (m_screen.right - m_screen.left) - m_resizeMargin && y < m_resizeMargin)
            return 14;
        if (x > (m_screen.right - m_screen.left) - m_resizeMargin
            && y > (m_screen.bottom - m_screen.top) - m_resizeMargin)
            return 17;
        if (x < m_resizeMargin)
            return 10;
        if (y < m_resizeMargin)
            return 12;
        if (x > (m_screen.right - m_screen.left) - m_resizeMargin)
            return 11;
        if (y > (m_screen.bottom - m_screen.top) - m_resizeMargin)
            return 15;
    }
    if (m_flags & 1)
        return 2;
    if (m_flags & 0x10) {
        if (y < m_captionHeight)
            return 2;
    } else if (m_flags & 0x4000000) {
        if (y < m_altCaptionHeight)
            return 2;
    }
    return 0;
}

void WidgetBase::on_key(int key, int down)
{
    Net *net = g_pNetwork;
    if (net) {
        if (key == net->m_startKey) {
            if (down) {
                net->StartPacketVoice(net->m_startArg);
                return;
            }
            net->stop_voice();
            return;
        }
        if (key == net->m_startVoiceKey) {
            if (down) {
                net->start_voice(net->m_voiceSendMode);
                return;
            }
            net->stop_voice();
            return;
        }
    }
    if (!down) {
        key_up_event(key);
        return;
    }
    g_lastKeyDown = key;
    key_down_event(key);
}

int find_circular_match(const char *needle, int len)
{
    const char *q = needle + len - 1;
    char *p = code_mark;
    int i;
    for (i = len; i > 0; i--) {
        if (*p != *q)
            return 1;
        p--;
        q--;
        if (p < code_buffer)
            p = &code_buffer[9];
    }
    return 0;
}

void WidgetBase::on_char(char ch)
{
    *code_mark = ch;
    if (find_circular_match("mdebug", 6) == 0) {
        if (multi_debug.is_visible())
            multi_debug.hide();
        else
            multi_debug.show(0);
    }
    code_mark++;
    if (code_mark >= &code_buffer[10])
        code_mark = code_buffer;
    if (vbar && vbar->on_key_click(ch, g_lastKeyDown))
        return;
    if (hbar && hbar->on_key_click(ch, g_lastKeyDown))
        return;
    key_click_event(ch, g_lastKeyDown);
}

void WidgetBase::OnMouseMoveInternal(int x, int y, int alt)
{
    mouse_move_event(x, y, alt);
}

void WidgetBase::on_l_button_down(int hit, int x, int y, int alt)
{
    int idx;

    bring_to_top();
    WidgetBase *p = parent;
    if (p)
        p->set_dialog_focus(this);
    if (alt) {
        g_mouseDownWidgetAlt = this;
        g_mouseDownWidget = 0;
    } else {
        g_mouseDownWidget = this;
        g_mouseDownWidgetAlt = 0;
    }
    g_leftButtonPress = 1;
    if (hit != 0 && (m_flags & 0x200) == 0)
        left_double_click_event(x, y, alt);
    if (is_graphicwin()) {
        GraphicWin *self = static_cast<GraphicWin *>(this);
        if (self->m_links.check(x, y, &idx, 0) >= 0) {
            int command = self->m_linkCommand[idx];
            int more;
            if (command == -1)
                more = self->OnHotspotDefault(idx, self->m_linkParam[idx]);
            else
                more = self->on_link_clicked(idx, command);
            if (more == 0)
                return;
        }
    }
    left_down_event(x, y, alt);
}

void WidgetBase::on_l_button_up(int x, int y, int flags)
{
    WidgetBase *cap = g_mouseDownWidget;
    if (cap == 0)
        cap = g_mouseDownWidgetAlt;
    if (cap != this) {
        g_mouseDownWidget = 0;
        g_mouseDownWidgetAlt = 0;
    } else {
        g_mouseDownWidget = 0;
        g_mouseDownWidgetAlt = 0;
        Rect rc = m_local;
        int dy = -rc.top;
        int dx = -rc.left;
        rc.left += dx;
        rc.right += dx;
        rc.top += dy;
        rc.bottom += dy;
        if (in_box(x, y, &rc))
            left_click_event(x, y, flags);
    }
    if (is_visible())
        left_up_event(x, y, flags);
}

void WidgetBase::on_r_button_down(int hit, int x, int y, int alt)
{
    bring_to_top();
    WidgetBase *p = parent;
    if (p)
        p->set_dialog_focus(this);
    if (alt) {
        g_mouseDownWidgetAlt = this;
        g_mouseDownWidget = 0;
    } else {
        g_mouseDownWidget = this;
        g_mouseDownWidgetAlt = 0;
    }
    g_leftButtonPress = 0;
    if (hit != 0 && (m_flags & 0x200) == 0)
        right_double_click_event(x, y, alt);
    right_down_event(x, y, alt);
}

void WidgetBase::on_r_button_up(int x, int y, int flags)
{
    WidgetBase *cap = g_mouseDownWidget;
    if (cap == 0)
        cap = g_mouseDownWidgetAlt;
    if (cap != this) {
        g_mouseDownWidget = 0;
        g_mouseDownWidgetAlt = 0;
    } else {
        g_mouseDownWidget = 0;
        g_mouseDownWidgetAlt = 0;
        Rect rc = m_local;
        int dy = -rc.top;
        int dx = -rc.left;
        rc.left += dx;
        rc.right += dx;
        rc.top += dy;
        rc.bottom += dy;
        if (in_box(x, y, &rc))
            right_click_event(x, y, flags);
    }
    right_up_event(x, y, flags);
}

WidgetBase *GetActiveWidget()
{
    WidgetBase *w;
    if (g_modalWidget) {
        w = g_modalWidget;
        if (g_dialogOverride)
            w = g_dialogOverride;
    } else {
        w = g_focusWidget;
        if (!w)
            w = g_fallbackRoot;
    }
    g_activeWidget = w;
    if (w) {
        w = w->m_sizeOwner;
        if (w && !w->is_visible())
            return 0;
    }
    return w;
}

WidgetBase *get_mouse_window_recurse(WidgetBase *w, int *px, int *py)
{
    int i;
    int savedX, b, a, savedY;
    int pix;

    if (!w->is_visible())
        return 0;

    savedX = *px;
    savedY = *py;

    if (!(w->m_flags & 0x20)) {
        *px = orig_x;
        *py = orig_y;
    }

    for (i = 0; i < w->m_childCount; i++) {
        WidgetBase *c = w->m_children[i];
        if (!(c->m_flags & 0x20)) {
            WidgetBase *hit = get_mouse_window_recurse(c, px, py);
            if (hit)
                return hit;
        }
    }

    if (w->m_status & 2) {
        if (in_box(*px, *py, &w->m_screen) == 0)
            goto restore;
        *px -= w->m_screen.left;
        *py -= w->m_screen.top;

        for (i = 0; i < w->m_childCount; i++) {
            if (w == w->m_sizeOwner) {
                WidgetBase *c = w->m_children[i];
                if ((c->m_flags & 0x8000) && (c->m_flags & 0x20)) {
                    WidgetBase *hit = get_mouse_window_recurse(c, px, py);
                    if (hit)
                        return hit;
                }
            }
        }

        if (in_box(*px, *py, &w->m_local) == 0) {
            if (w->is_graphicwin() != 0 && (w->m_flags & 0x10000000) != 0) {
                Rect *r = &w->m_screen;
                if ((w->m_flags & 0x10) && !(w->m_flags & 0x400000))
                    a = w->m_captionHeight;
                else
                    a = w->m_borderWidth;

                b = (w->m_bottomBorder != -1) ? w->m_bottomBorder : w->m_borderWidth;

                if (*py < a) {
                    if (w->m_topStrip->get_pixel(*px, *py) == (g_transparentColor & 0x7fffffff)) {
                        *px = *px + w->m_screen.left;
                        *py = *py + w->m_screen.top;
                        goto restore;
                    }
                } else if (*py > w->m_screen.bottom - w->m_screen.top - b) {
                    if (w->m_bottomStrip->get_pixel(*px, *py - (w->m_screen.bottom - w->m_screen.top - b))
                        == (g_transparentColor & 0x7fffffff)) {
                        *px = *px + w->m_screen.left;
                        *py = *py + w->m_screen.top;
                        goto restore;
                    }
                } else {
                    int lx = *px;
                    if (lx < w->m_borderWidth) {
                        if (w->m_leftStrip->get_pixel(lx, *py - a) == (g_transparentColor & 0x7fffffff)) {
                            *px = *px + w->m_screen.left;
                            *py = *py + w->m_screen.top;
                            goto restore;
                        }
                    } else {
                        if (w->m_rightStrip->get_pixel(lx - (r->right - r->left - w->m_borderWidth), *py - a)
                            == (g_transparentColor & 0x7fffffff)) {
                            *px = *px + w->m_screen.left;
                            *py = *py + w->m_screen.top;
                            goto restore;
                        }
                    }
                }
            }
            g_pointInFrame = 1;
            return w;
        }
        *px -= w->m_local.left;
        *py -= w->m_local.top;
        g_pointInFrame = 0;
    } else {
        if (in_box(*px, *py, &w->m_local) == 0)
            goto restore;
        *px -= w->m_local.left;
        *py -= w->m_local.top;
        g_pointInFrame = 0;
    }

    for (i = 0; i < w->m_childCount; i++) {
        if (w == w->m_sizeOwner) {
            WidgetBase *c = w->m_children[i];
            if (!(c->m_flags & 0x8000) && (c->m_flags & 0x20)) {
                WidgetBase *hit = get_mouse_window_recurse(c, px, py);
                if (hit)
                    return hit;
            }
        }
    }

    if (w->m_flags & 2)
        goto restore;
    if (w->is_graphicwin() == 0 || !(w->m_flags & 0x1000000) || (w->m_flags & 0x100))
        return w;
    pix = ((GraphicWin *)w)->get_pixel(*px, *py);
    if (pix == (g_transparentColor & 0x7fffffff) || (w->m_destRecolorLut != 0 && pix == 0))
        goto restore;
    return w;

restore:
    *px = savedX;
    *py = savedY;
    return 0;
}

WidgetBase *FindWidgetAtPoint(int *px, int *py)
{
    orig_x = *px;
    WidgetBase *hit = 0;
    orig_y = *py;
    for (;;) {
        if (g_mouseDownWidget != 0) {
            if (g_mouseDownWidget->m_status & 1) {
                g_mouseDownWidget->screen_to_client(px, py);
                g_pointInFrame = 0;
                hit = g_mouseDownWidget;
                break;
            }
            g_mouseDownWidget = 0;
            continue;
        }
        if (g_mouseDownWidgetAlt != 0) {
            if (g_mouseDownWidgetAlt->m_status & 1) {
                g_mouseDownWidgetAlt->screen_to_nonclient(px, py);
                g_pointInFrame = 1;
                hit = g_mouseDownWidgetAlt;
                break;
            }
            g_mouseDownWidgetAlt = 0;
            continue;
        }
        if (g_dragWidget != 0) {
            if (g_dragWidget->m_status & 1) {
                g_dragWidget->screen_to_client(px, py);
                g_pointInFrame = 0;
                hit = g_dragWidget;
                break;
            }
            g_dragWidget = 0;
            continue;
        }
        if (g_dragTargetWidget != 0) {
            if (g_dragTargetWidget->m_status & 1) {
                g_dragTargetWidget->screen_to_nonclient(px, py);
                g_pointInFrame = 1;
                hit = g_dragTargetWidget;
                break;
            }
            g_dragTargetWidget = 0;
            continue;
        }
        break;
    }
    if (hit != 0)
        return hit;
    if (g_modalWidget != 0) {
        g_modalWidget->screen_to_nonclient(px, py);
        hit = get_mouse_window_recurse(g_modalWidget, px, py);
        if (hit == 0) {
            if (g_modalFocus != 0) {
                *px = orig_x;
                *py = orig_y;
                g_modalFocus->screen_to_nonclient(px, py);
                hit = get_mouse_window_recurse(g_modalFocus, px, py);
            }
            if (hit == 0) {
                *px = orig_x;
                *py = orig_y;
                g_modalWidget->screen_to_client(px, py);
                hit = g_modalWidget;
                g_pointInFrame = 0;
            }
        }
    } else {
        int i;
        for (i = 0; i < g_topLevelCount; i++) {
            hit = get_mouse_window_recurse(g_toplevelViews[i], px, py);
            if (hit != 0)
                break;
        }
    }
    if (hit != 0 && g_pointInFrame == 0)
        hit = hit->m_sizeOwner;
    return hit;
}

void RenderWidgetTree(Rect *clip, WidgetBase *from)
{
    int n = g_renderOrderCount;
    if (n == 0)
        return;
    if (g_redrawRoot == 0)
        from = 0;
    int i = n - 1;
    if (from != 0) {
        for (i = 0; i < n; i++)
            if (from == g_renderOrder[i])
                break;
        if (i == n)
            return;
    }
    for (; i >= 0; i--) {
        if (g_renderOrder[i]->is_visible()) {
            WidgetBase *w = g_renderOrder[i];
            if (w->m_status & 2)
                w->on_nc_paint(clip, -1);
            if (from == 0 || from == from->m_sizeOwner)
                w->m_sizeOwner->on_paint(clip);
        }
    }
    if (g_cursorSprite == 0)
        return;
    Rect rc;
    rc.top = g_cursorY;
    rc.left = g_cursorX;
    rc.top = g_cursorY;
    rc.right = g_cursorX + g_cursorW;
    rc.bottom = g_cursorY + g_cursorH;
    if (clip != 0) {
        if (!IntersectRect((LPRECT)&rc, (const RECT *)&rc, (const RECT *)clip))
            return;
    }
    WidgetBase::s_primaryBuffer.copy(&WidgetBase::s_mouseSaveSurface, rc.left, rc.top, rc.left - g_cursorX,
        rc.top - g_cursorY, RectWidth(&rc), RectHeight(&rc));
}

void WidgetBase::update_window(Rect *r)
{
    if (!r) {
        RenderWidgetTree(0, this);
        return;
    }
    int x = 0, y = 0;
    client_to_screen(&x, &y);
    Rect s = *r;
    s.Offset(x, y);
    RenderWidgetTree(&s, this);
}

void WidgetBase::do_tracking(int x, int y)
{
    int limW, limH;
    Rect rOld, rNew, rCompanion;
    int dx, dy;

    if (!(m_status & 2))
        screen_to_client(&x, &y);
    else
        screen_to_nonclient(&x, &y);
    dx = x - g_resizeGrabX;
    dy = y - g_resizeGrabY;
    if (dx == 0 && dy == 0)
        return;

    limW = m_minWidth;
    limH = m_minHeight;
    client_to_nonclient(&limW, &limH);

    if (!(m_status & 2)) {
        rOld = m_local;
        rOld.Offset(-rOld.left, -rOld.top);
        RectToScreen(&rOld);
    } else {
        rOld = m_screen;
        rOld.Offset(-rOld.left, -rOld.top);
        NonclientRectToScreen(&rOld);
    }

    switch (g_resizeHit) {
    case HTCAPTION:
        if (dx == 0 && dy == 0)
            break;
        if (!(m_status & 2)) {
            m_local.Offset(dx, dy);
        } else {
            m_screen.Offset(dx, dy);
            if (m_dragCompanion) {
                rCompanion = m_dragCompanion->m_local;
                m_dragCompanion->m_local.Offset(dx, dy);
                UnionRect((LPRECT)&rCompanion, (const RECT *)&rCompanion, (const RECT *)&m_dragCompanion->m_local);
            }
        }
        break;

    case HTLEFT:
        if (dx == 0)
            break;
        m_screen.left += dx;
        if (m_screen.right - m_screen.left < limW)
            m_screen.left = m_screen.right - limW;
        m_sizeOwner->resize(m_screen.right - m_screen.left, m_screen.bottom - m_screen.top, 0, 0);
        break;

    case HTRIGHT: {
        if (dx == 0)
            break;
        int minW = limW;
        m_screen.right += dx;
        g_resizeGrabX += dx;
        if (m_screen.right - m_screen.left < minW) {
            int adj = minW - (m_screen.right - m_screen.left);
            m_screen.right += adj;
            g_resizeGrabX += adj;
        }
        m_sizeOwner->resize(m_screen.right - m_screen.left, m_screen.bottom - m_screen.top, 0, 0);
        break;
    }

    case HTTOP:
        if (dy == 0)
            break;
        m_screen.top += dy;
        if (m_screen.bottom - m_screen.top < limH)
            m_screen.top = m_screen.bottom - limH;
        m_sizeOwner->resize(m_screen.right - m_screen.left, m_screen.bottom - m_screen.top, 0, 0);
        break;

    case HTBOTTOM:
        if (dy == 0)
            break;
        m_screen.bottom += dy;
        g_resizeGrabY += dy;
        if (m_screen.bottom - m_screen.top < limH) {
            int adj = limH - (m_screen.bottom - m_screen.top);
            m_screen.bottom += adj;
            g_resizeGrabY += adj;
        }
        m_sizeOwner->resize(m_screen.right - m_screen.left, m_screen.bottom - m_screen.top, 0, 0);
        break;

    case HTTOPLEFT:
        m_screen.left += dx;
        m_screen.top += dy;
        if (m_screen.bottom - m_screen.top < limH)
            m_screen.top = m_screen.bottom - limH;
        if (m_screen.right - m_screen.left < limW)
            m_screen.left = m_screen.right - limW;
        m_sizeOwner->resize(m_screen.right - m_screen.left, m_screen.bottom - m_screen.top, 0, 0);
        break;

    case HTTOPRIGHT:
        m_screen.right += dx;
        m_screen.top += dy;
        g_resizeGrabX += dx;
        if (m_screen.bottom - m_screen.top < limH)
            m_screen.top = m_screen.bottom - limH;
        if (m_screen.right - m_screen.left < limW) {
            int adj = limW - (m_screen.right - m_screen.left);
            m_screen.right += adj;
            g_resizeGrabX += adj;
        }
        m_sizeOwner->resize(m_screen.right - m_screen.left, m_screen.bottom - m_screen.top, 0, 0);
        break;

    case HTBOTTOMLEFT:
        m_screen.left += dx;
        m_screen.bottom += dy;
        g_resizeGrabY += dy;
        if (m_screen.bottom - m_screen.top < limH) {
            int adj = limH - (m_screen.bottom - m_screen.top);
            m_screen.bottom += adj;
            g_resizeGrabY += adj;
        }
        if (m_screen.right - m_screen.left < limW)
            m_screen.left = m_screen.right - limW;
        m_sizeOwner->resize(m_screen.right - m_screen.left, m_screen.bottom - m_screen.top, 0, 0);
        break;

    case HTBOTTOMRIGHT:
        m_screen.right += dx;
        m_screen.bottom += dy;
        g_resizeGrabX += dx;
        g_resizeGrabY += dy;
        if (m_screen.bottom - m_screen.top < limH) {
            int adj = limH - (m_screen.bottom - m_screen.top);
            m_screen.bottom += adj;
            g_resizeGrabY += adj;
        }
        if (m_screen.right - m_screen.left < limW) {
            int adj = limW - (m_screen.right - m_screen.left);
            m_screen.right += adj;
            g_resizeGrabX += adj;
        }
        m_sizeOwner->resize(m_screen.right - m_screen.left, m_screen.bottom - m_screen.top, 0, 0);
        break;

    default: return;
    }

    if (!(m_status & 2)) {
        rNew = m_local;
        rNew.Offset(-rNew.left, -rNew.top);
        RectToScreen(&rNew);
    } else {
        rNew = m_screen;
        rNew.Offset(-rNew.left, -rNew.top);
        NonclientRectToScreen(&rNew);
    }
    UnionRect((LPRECT)&rOld, (const RECT *)&rOld, (const RECT *)&rNew);
    if (m_dragCompanion)
        UnionRect((LPRECT)&rOld, (const RECT *)&rOld, (const RECT *)&rCompanion);
    recurse_clips(this, &m_clip);
    g_presentingRegion = 1;
    RenderWidgetTree(&rOld, 0);
    flip(&rOld);
    g_presentingRegion = 0;
}

int WidgetBase::is_visible()
{
    if (!(m_status & 1))
        return 0;
    WidgetBase *p = parent;
    if (p) {
        if (!p->is_visible())
            return 0;
    }
    return 1;
}

void WidgetBase::update_bubble_text()
{
    if (g_tooltipText != 0)
        WidgetBase::flip(&g_tooltipBbox);
}

void WidgetBase::set_bubble_text(char *text, Rect *anchor)
{
    if (text == 0)
        return;
    if (anchor == 0)
        return;

    clear_bubble_text();
    g_tooltipOwner = this;

    int maxw = 0;
    int nlines = 0;
    char *s = text;
    char *p;
    do {
        p = strchr(s, '^');
        if (p)
            *p = 0;
        int w = g_uiFont->MeasureTextWidth(" ") * 2 + g_uiFont->MeasureTextWidth(s);
        if (w > maxw)
            maxw = w;
        if (p)
            *p = '^';
        nlines++;
        s = p + 1;
    } while (p);

    Font *f = g_uiFont;
    int lh;
    if (f->m_extraLeading >= 0)
        lh = f->m_capHeight + f->m_extraLeading;
    else
        lh = f->m_lineSpacing;
    int h = lh * nlines;

    int y = anchor->top - h;
    if (y < 0)
        y = anchor->bottom;

    int x = (anchor->left + (anchor->right - maxw)) / 2;
    if (x < 0)
        x = 0;
    else if (x + maxw > (graphsys == 0 ? 0 : graphsys->screen_width()))
        x = (graphsys == 0 ? 0 : graphsys->screen_width()) - maxw;

    g_tooltipBbox.left = x;
    g_tooltipBbox.top = y;
    g_tooltipBbox.right = x + maxw;
    g_tooltipBbox.bottom = y + h;
    g_tooltipText = text;
    flip(&g_tooltipBbox);
}

void WidgetBase::clear_bubble_text(void)
{
    if (g_tooltipText != 0) {
        g_tooltipOwner = 0;
        g_tooltipText = 0;
        RenderWidgetTree(&g_tooltipBbox, 0);
        WidgetBase::flip(&g_tooltipBbox);
    }
}

void SetRootWidget(WidgetBase *w)
{
    g_rootWidget = w;
    if (g_rootWidget) {
        Rect r;
        r.SetRect(g_rootWidget->m_local.left, g_rootWidget->m_local.top,
            g_rootWidget->m_local.right - g_rootWidget->m_local.left,
            g_rootWidget->m_local.bottom - g_rootWidget->m_local.top);
        g_rootWidget->ExpandRectForBorders(&r);
        OffsetRect(&r, g_rootWidget->m_screen.left, g_rootWidget->m_screen.top);
        if (graphsys)
            graphsys->set_window_pos(r.left, r.top, r.right - r.left, r.bottom - r.top);
        g_rootWidget->move(0, 0);
    }
}

inline ChildList::~ChildList()
{
    RemoveAll();
}

inline ChildList::ChildList()
{
    head = 0;
    cur = 0;
    count = 0;
    index = 0;
    arena = 0;
}

extern "C" void __cdecl ResetManagerModule(void)
{
}

WidgetBase *g_popupStack[4];
WidgetBase *g_modalFocuses[4];

int g_padField[2];
