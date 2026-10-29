




#ifndef WIDGETBASE_H
#define WIDGETBASE_H

#include <windows.h>
#include "jglrect.h"

struct AssetSurface;
struct SpriteObject;
#include "TextRenderer.h"
#include "crt.h"
#include "StringUtil.h"
#include "Spot.h"


struct WidgetBase;
struct GraphicWin;
struct Scroll;
struct Menu;


struct CursorResource {
    void *m_reserved;
    HCURSOR m_hcursor;
};



struct WidgetObserver {
    virtual void s0();
    virtual void set_fade();
    virtual void set_fade_in();
    virtual void load();
    virtual void s4();
    virtual void unload();
    virtual void s6();
    virtual void play();
};




struct AutoSound {
    AutoSound();
    AutoSound(int);
    virtual ~AutoSound();
    void ResetToDefaults();

    void *m_unusedObserver1;
    void *m_unusedObserver2;
    WidgetObserver *m_onShown;
    WidgetObserver *m_onHidden;
    void *m_unusedObserver3;
    WidgetObserver *m_onClose;
    WidgetObserver *m_onMouseMove;
    WidgetObserver *m_onLeftClick;
    WidgetObserver *m_onLeftDown;
    WidgetObserver *m_onLeftUp;
    WidgetObserver *m_onRightClick;
    WidgetObserver *m_onRightDown;
    WidgetObserver *m_onRightUp;
    WidgetObserver *m_onCharKey;
    WidgetObserver *m_onKeyDown;
    WidgetObserver *m_onKeyUp;
    WidgetObserver *m_onLeftDoubleClick;
    WidgetObserver *m_onRightDoubleClick;

    WidgetObserver *m_onHoverChanged;
    WidgetObserver *m_onMouseMoveAlt;
    WidgetObserver *m_onLeftClickAlt;
    WidgetObserver *m_onLeftDownAlt;
    WidgetObserver *m_onLeftUpAlt;
    WidgetObserver *m_onRightClickAlt;
    WidgetObserver *m_onRightDownAlt;
    WidgetObserver *m_onRightUpAlt;
    WidgetObserver *m_onLeftDoubleClickAlt;
    WidgetObserver *m_onRightDoubleClickAlt;
    WidgetObserver *m_onButtonDown;
    WidgetObserver *m_onButtonReleased;
    WidgetObserver *m_onButtonPressed;
    WidgetObserver *m_onButtonUp;
    WidgetObserver *m_onScrollPosChanged;
    WidgetObserver *m_onScrolling;
    WidgetObserver *m_onMenuChoice;
    WidgetObserver *m_onCloseButton;
    WidgetObserver *m_onSelectionChanged;
    WidgetObserver *m_onItemActivated;
};

struct Heap;

struct LLNode {
    void *f00;
    WidgetBase *data;
    void *buf;
    LLNode *next;
    LLNode *prev;

    char pad_14[0x1c - 0x14];
};



struct ChildList {
    ChildList();
    virtual ~ChildList();


    void RemoveAll();

    WidgetBase *GetCurrent()
    {
        if (head)
            return cur->data;
        return 0;
    }
    void Advance()
    {
        if (head) {
            cur = cur->next;
            index++;
            if (index == count)
                index = 0;
        }
    }
    LLNode *head;
    LLNode *cur;
    int count;
    int index;
    Heap *arena;



    inline void NotifyFocusChange(WidgetBase *old);
};
enum WidgetBaseUnrecovered {
    WB_UNRECOVERED_1,
    WB_UNRECOVERED_2,
    WB_UNRECOVERED_3,
    WB_UNRECOVERED_4
};



#include "Heap.h"


extern void *g_tooltipOwner;

extern "C" {
extern char *g_tooltipText;
extern Rect g_tooltipBbox;
extern Font *g_uiFont;
}




void RenderWidgetTree(Rect *clip, WidgetBase *from);







WidgetBase *GetActiveWidget();
WidgetBase *FindWidgetAtPoint(int *px, int *py);
void UpdateCursor(WidgetBase *w, int doRender, int mouseX, int mouseY);




struct WidgetBase : public AutoSound {

    virtual ~WidgetBase();

    WidgetBase();


    void close();


    virtual void show(int flags);


    virtual void hide();

    virtual int resize(int cx, int cy, int notify, int depth);

    virtual void on_lose_mouse_capture();



    virtual int on_button_text_draw(WidgetBase *child, char *text, int id) { return 0; }

    virtual int on_scroll_create();

    virtual int on_scroll_delete(int bar) { return bar; }
    virtual int on_listbox_scroll_create();
    virtual int on_listbox_scroll_delete(int bar) { return bar; }


    virtual int on_dialog_back_draw(WidgetBase *w);




    virtual int on_dialog_item_back_draw(WidgetBase *w, int key, int state, Rect *r) { return 0; }




    virtual int on_spot_right_double_click(int id, int param, Rect *rc) { return 0; }
    virtual int on_spot_left_double_click(int id, int param, Rect *rc) { return 0; }
    virtual int on_spot_right_click(int id, int param, Rect *rc) { return 0; }
    virtual int on_spot_left_click(int id, int param, Rect *rc) { return 0; }
    virtual int on_spot_left_down(int id, int param, Rect *rc) { return 0; }
    virtual int on_spot_right_down(int id, int param, Rect *rc) { return 0; }
    virtual int on_spot_left_up(int id, int param, Rect *rc) { return 0; }
    virtual int on_spot_right_up(int id, int param, Rect *rc) { return 0; }
    virtual int on_spot_move(int id, int param, Rect *rc) { return 0; }

    virtual void on_check_size(int *pcx, int *pcy, int *pNotify) {}

    virtual void on_redraw();



    virtual void on_resize(int cx, int cy) {}

    virtual void on_sys_close();

    virtual void on_mouse_wheel(int delta, int x, int y) {}

    virtual void OnMouseMoveInternal(int x, int y, int alt);
    virtual void on_mouse_move(int x, int y) {}


    virtual void on_mouse_leave(int x, int y) {}
    virtual void on_left_click(int x, int y) {}
    virtual void on_left_down(int x, int y) {}
    virtual void on_left_up(int x, int y) {}
    virtual void on_right_click(int x, int y) {}
    virtual void on_right_down(int x, int y) {}
    virtual void on_right_up(int x, int y) {}

    virtual int on_key_click(int ch, int vk);

    virtual int on_key_down(int vk);
    virtual int on_key_up(int vk);
    virtual void on_left_double_click(int x, int y) {}
    virtual void on_right_double_click(int x, int y) {}
    virtual void OnMouseMoveAlt(int x, int y) {}
    virtual void OnLeftClickAlt(int x, int y) {}
    virtual void OnLeftDownAlt(int x, int y) {}
    virtual void OnLeftUpAlt(int x, int y) {}
    virtual void OnRightClickAlt(int x, int y) {}
    virtual void OnRightDownAlt(int x, int y) {}
    virtual void OnRightUpAlt(int x, int y) {}
    virtual void OnLeftDoubleClickAlt(int x, int y) {}
    virtual void OnRightDoubleClickAlt(int x, int y) {}
    virtual void on_button_passover(int, int) {}


    virtual int on_button_pressed(int id);
    virtual void on_button_up(int);
    virtual void on_button_down(int);
    virtual void on_button_clicked(int id);

    virtual void on_button_toggled(int id, int selected) {}


    virtual void on_group_clicked(int param, int oldId, int newId) {}
    virtual void OnGroupSelSwitched(int oldId, int newId) {}

    virtual void on_scrolled(int bar, int pos) {}
    virtual void on_scrolling(int bar, int pos) {}
    virtual void on_menu(int);

    virtual void on_dialog_focus(int on);
    virtual void on_closebox();

    virtual void on_hide();
    virtual void OnItemSelectedEx(int, int) {}
    virtual void on_selected(int);
    virtual void OnItemActivatedEx(int, int) {}
    virtual void on_double_clicked(int);


    virtual int set_modal(int flags, int (*pfnIdle)(), WidgetBase *focus);

    virtual void release_modal();


    virtual int on_modal() { return 1; }


    virtual void on_post_redraw_nc_buffer(AssetSurface *strip, int which) {}


    virtual GraphicWin *is_graphicwin();

    virtual void redraw();

    virtual void update();

    virtual void pass_dialog_focus();

    virtual void set_dialog_focus(WidgetBase *w);

    virtual void set_rects();


    virtual void on_redraw_nc(int which);

    virtual void on_paint(Rect *clip);

    virtual void on_nc_paint(Rect *clip, int which);


    virtual int on_nc_hittest(int x, int y);



    virtual void on_key(int key, int down);
    virtual void on_char(char ch);


    virtual void on_l_button_down(int hit, int x, int y, int alt);
    virtual void on_l_button_up(int x, int y, int flags);
    virtual void on_r_button_down(int hit, int x, int y, int alt);
    virtual void on_r_button_up(int x, int y, int flags);


    unsigned m_flags;

    int m_status;



    unsigned char *m_destRecolorLut;
    unsigned char *m_srcDstLut;
    int m_blitArg40;

    WidgetBase *m_sizeOwner;

    int m_aspect[2];

    Spot m_hotspots;



    AssetSurface *m_topStrip;
    AssetSurface *m_rightStrip;
    AssetSurface *m_leftStrip;
    AssetSurface *m_bottomStrip;


    WidgetBase *m_dragCompanion;

    GraphicWin *parent;

    ChildList m_childList;

    char *m_captionText;


    GraphicWin *m_captionButtons[3];

    Menu *m_peer;

    int m_backgroundColor;


    Font *m_captionFont;
    AssetSurface *m_frameTexture;
    unsigned m_frameColor;
    int m_captionColors[4];


    int m_captionHeight;
    int m_borderWidth;

    int m_bottomBorder;

    int m_bevelHighlight;
    int m_bevelShadow;

    int m_captionMargin;

    int m_captionIndent;


    AssetSurface *m_background;
    int m_backgroundCentered;
    int m_minWidth;
    int m_minHeight;


    Rect m_local;
    Rect m_screen;

    Rect m_createdLocal;
    Rect m_clip;


    int m_altCaptionHeight;
    int m_resizeMargin;

    int m_unk1f4[5];


    SpriteObject *m_cursorImage;
    int m_cursorHotX;
    int m_cursorHotY;
    CursorResource *m_cursorRes;
    int m_cursorId;
    int m_unk21c;

    int m_renderLatch;


    WidgetBase **m_children;

    int m_capacity;
    int m_childCount;


    void decrease_children();


    void (*pfnOnLayout)(int, int);
    void (*pfnOnClose)();

    void (*pfnMouseMove)(int, int);
    void (*pfnLeftDoubleClick)(int, int);
    void (*pfnRightDoubleClick)(int, int);
    void (*pfnLeftClick)(int, int);
    void (*pfnLeftDown)(int, int);
    void (*pfnLeftUp)(int, int);
    void (*pfnRightClick)(int, int);
    void (*pfnRightDown)(int, int);
    void (*pfnRightUp)(int, int);


    int (*pfnCharKey)(int, int);
    int (*pfnKeyDown)(int);
    int (*pfnKeyUp)(int);
    void (*pfnOnCloseButton)();


    Scroll *vbar;
    Scroll *hbar;







    int key_click_event(int ch, int vk);
    int key_down_event(int vk);
    int key_up_event(int vk);

    int OfferKeyToBar(GraphicWin *bar, int vk, int *handled);




    void set_bubble_text(char *text, Rect *anchor);


    int init(int x, int y, int cx, int cy, const char *caption, unsigned flags, GraphicWin *owner, Menu *peer,
        int *borders);
    void set_parent_dialog(GraphicWin *p);
    int is_visible();




    void update_window(Rect *r);
    void RenderRect(int x, int y, int w, int h, int);
    void client_to_screen(int *px, int *py);
    void nonclient_to_screen(int *px, int *py);

    void screen_to_client(int *px, int *py);
    void screen_to_nonclient(int *px, int *py);


    void center();


    void set_caption(const char *text);

    int SetCursorImage(SpriteObject *image, int hotX, int hotY);
    int set_cursor(int id);


    void get_mouse_pos(int *px, int *py);
    int get_lbutton_state();


    void RectToScreen(Rect *r);
    void NonclientRectToScreen(Rect *r);
    void bring_child_to_top(WidgetBase *child);
    void bring_to_top();
    void add_child(WidgetBase *child);
    void remove_child(WidgetBase *child);
    AssetSurface *redraw_nc_buffer(int which);

    void increase_children();

    int move(int x, int y);

    void remove_parent_dialog();



    void client_to_nonclient(int *pcx, int *pcy);
    void nonclient_to_client(int *pcx, int *pcy);
    void ExpandRectForBorders(Rect *r);
    void ShrinkRectForBorders(Rect *r);
    void reset_window_clip();

    void set_vert_pos(int pos);
    void set_horz_pos(int pos);

    void set_vert_range(int lo, int hi);
    void set_horz_range(int lo, int hi);
    void set_vert_paging(int n);
    void set_horz_paging(int n);
    void set_mouse_confines(Rect *r);
    void ReleaseCursorClip();
    int is_descendant(WidgetBase *w);

    void do_tracking(int x, int y);


    void mouse_move_event(int x, int y, int alt);
    void left_double_click_event(int x, int y, int alt);
    void right_double_click_event(int x, int y, int alt);
    void left_click_event(int x, int y, int alt);
    void left_down_event(int x, int y, int alt);
    void left_up_event(int x, int y, int alt);
    void right_click_event(int x, int y, int alt);
    void right_down_event(int x, int y, int alt);
    void right_up_event(int x, int y, int alt);

    int set_window_clip(Rect *r);
    void resize_event(int cx, int cy);

    void redo_caption_buttons();

    void do_caption_buttons();

    void set_border_thickness(int w);
    void set_bottom_border_thickness(int w);
    void SetEdgeColors(int highlight, int shadow)
    {
        m_bevelHighlight = highlight;
        m_bevelShadow = shadow;
    }

    int is_dialog_focus();



    static AssetSurface s_mouseSaveSurface;
    static AssetSurface s_primaryBuffer;
    static int init_class(const char *title, int flags);
    static void close_class();
    static void flip(Rect *r);
    static void update_screen_clips();
    static void clear_bubble_text();
    static void update_bubble_text();
    static void recurse_clips(WidgetBase *w, Rect *out);
    static void set_def_focus(WidgetBase *w);
    static void add_parent(WidgetBase *w);
    static void remove_parent(WidgetBase *w);
    static void bring_parent_to_top(WidgetBase *w);
    static void restore_cursor_back(int flush);
};

#include "AssetSurface.h"
#include "Sprite.h"


struct GraphicWin : public WidgetBase, public AssetSurface {

    virtual ~GraphicWin();

    GraphicWin();


    virtual int resize(int cx, int cy, int notify, int depth);

    virtual void redraw();

    virtual void update() { RenderFullSurface(0); }
    virtual void set_rects();
    virtual void on_redraw_nc(int which);
    virtual void on_paint(Rect *clip);



    virtual int on_link_clicked(int index, int command) { return 1; }
    virtual int OnHotspotDefault(int index, char *param) { return 1; }


    virtual GraphicWin *is_graphicwin() { return this; }


    virtual void on_caption_draw(char *text, AssetSurface *strip) {}

    virtual void on_size(int nType, int cx, int cy);




    SpriteObject *m_capTopLeft;
    SpriteObject *m_bottomLeft;
    SpriteObject *m_capTopRight;
    SpriteObject *m_bottomRight;
    SpriteObject *m_leftEdge;
    SpriteObject *m_rightEdge;
    SpriteObject *m_bottomEdge;
    SpriteObject *m_capTopEdge;
    SpriteObject *m_topLeft;
    SpriteObject *m_topRight;
    SpriteObject *m_topEdge;

    SpriteObject *m_altTopLeft;
    SpriteObject *m_altBottomLeft;
    SpriteObject *m_altTopRight;
    SpriteObject *m_altBottomRight;

    AssetSurface *m_altSurface;


    void (*m_captionHook)(GraphicWin *self, char *text, AssetSurface *strip);

    void (*m_renderHook)();



    void close();


    int init(int x, int y, int cx, int cy, const char *caption, int flags, WidgetBase *owner, Menu *peer, int *borders);

    int init(Rect *r, const char *caption, int flags, WidgetBase *owner, Menu *peer, int *borders);


    void soft_update();
    void soft_update(Rect *area);

    void EraseWholeBackground(int color);


    int EraseBackground(Rect *r, int color);

    void compute_min_size();


    void fill(int x, int y, int w, int h, int color);
    int RenderBoundsRect(Rect *r, int);
    void RenderFullSurface(int);
};



struct Menu : public GraphicWin {

    virtual int init(WidgetBase *owner);
    virtual int requested_height();
};


extern "C" int g_scrollbarThickness;


extern int buffer_resizing;


extern PaletteBase *g_widgetPalette;
extern int g_transparentColor;


extern WidgetBase *prev_dialog_focus;


extern SpriteObject *g_frameCapTopLeft;
extern SpriteObject *g_frameBottomLeft;
extern SpriteObject *g_frameCapTopRight;
extern SpriteObject *g_frameBottomRight;
extern SpriteObject *g_frameCapTopEdge;
extern SpriteObject *g_frameBottomEdge;
extern SpriteObject *g_frameLeftEdge;
extern SpriteObject *g_frameRightEdge;
extern SpriteObject *g_frameTopRight;
extern SpriteObject *g_frameTopLeft;
extern SpriteObject *g_frameTopEdge;



extern WidgetBase *g_mouseDownWidget;
extern WidgetBase *g_mouseDownWidgetAlt;

extern int g_leftButtonPress;


extern WidgetBase *g_modalWidget;



extern WidgetBase *g_popupStack[4];
extern WidgetBase *g_modalFocuses[4];
extern int g_modalDepth;

extern const unsigned char *g_frameShadeTable;
extern WidgetBase *g_modalFocus;

struct WidgetBaseModalUnrecovered;


void flush_input();
void wait_task();




extern WidgetBase *g_cursorClipWidget;
extern Rect g_cursorConfineRect;


extern void *g_renderingWidget;




extern char code_buffer[10];
extern char *code_mark;

int find_circular_match(const char *needle, int len);

extern int g_lastKeyDown;



extern void(__cdecl *g_lButtonDownHook)(int, int);


extern WidgetBase *g_dragWidget;
extern WidgetBase *g_dragTargetWidget;



extern int orig_x;
extern int orig_y;


extern "C" void UpdateAllWidgetRenderOrder();




#define MAX_PARENTS 0x80
extern "C" WidgetBase *g_toplevelViews[];
extern "C" int g_topLevelCount;




extern "C" void recurse_zorder(WidgetBase *w);
extern "C" void UpdateAllWidgetRenderOrder();


extern "C" WidgetBase *g_renderOrder[];
extern "C" int g_renderOrderCount;

extern "C" WidgetBase *g_redrawRoot;

extern "C" int g_renderOrderResetFlag;



extern "C" {
extern void (*g_beforeTooltipHook)();
extern int g_tooltipBorderColor;
extern int g_tooltipFillColor;
extern int g_tooltipTextColor;
}



extern AssetSurface *g_paintTarget;
extern int g_paintTargetX;
extern int g_paintTargetY;

extern void (*g_defaultCaptionHook)(GraphicWin *self, char *text, AssetSurface *strip);






extern WidgetBase *g_rootWidget;

extern WidgetBase *g_fallbackRoot;

extern WidgetBase *g_focusWidget;

extern WidgetBase *g_activeWidget;


extern WidgetBase *g_dialogOverride;


WidgetBase *GetActiveWidget();
void SetRootWidget(WidgetBase *w);

extern Font *g_widgetTitleFont;

extern AssetSurface *g_defFrameTexture;
extern int g_defCaptionHeight;
extern int g_defBottomBorder;
extern int g_defCaptionMargin;



extern int g_pointInFrame;

extern int g_cursorHotX;
extern int g_cursorHotY;

extern int g_presentingRegion;

extern int leave;

extern Rect g_mouseRedrawRect;


WidgetBase *FindWidgetAtPoint(int *px, int *py);

WidgetBase *get_mouse_window_recurse(WidgetBase *w, int *px, int *py);





void UpdateCursor(WidgetBase *w, int flush, int x, int y);



extern "C" {
extern void *g_cursorSprite;
extern int g_cursorX;
extern int g_cursorY;
extern int g_cursorW;
extern int g_cursorH;
}



#endif
