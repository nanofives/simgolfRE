



#ifndef LISTBOXCTL_H
#define LISTBOXCTL_H

#include "WidgetBase.h"
#include "HeapNew.h"

#include "ClassList.h"



struct StringStruct : virtual HeapNew {

    char *text;
    int attr;
    int span;
};



struct StringList : ClassList<StringStruct> {

    const char *m_pendingText;
    int m_pendingAttr;
    int m_pendingSpan;




    virtual int fill_entry(StringStruct *item);
    virtual void kill_entry(StringStruct *item);




    ~StringList() { ClassList<StringStruct>::Clear(); }


    int Push(const char *text, int attr, int span, int key)
    {
        m_pendingText = text;
        m_pendingAttr = attr;
        m_pendingSpan = span;
        return add(key);
    }

    int CurrentAttr()
    {
        if (m_cur != 0)
            return current_entry()->attr;
        return 0;
    }
    int NextAttr()
    {
        Advance();
        return CurrentAttr();
    }

    char *CurrentText()
    {
        if (m_cur != 0)
            return current_entry()->text;
        return 0;
    }
    char *NextText()
    {
        Advance();
        return CurrentText();
    }

    int CurrentSpan()
    {
        if (m_cur != 0)
            return current_entry()->span;
        return 0;
    }
    int NextSpan()
    {
        Advance();
        return CurrentSpan();
    }

    int AttrAtCursor()
    {
        if (m_head != 0)
            return current_entry()->attr;
        return 0;
    }
    char *TextAtCursor()
    {
        if (m_head != 0)
            return current_entry()->text;
        return 0;
    }

    int SpanAtCursor()
    {
        if (m_head != 0)
            return current_entry()->span;
        return 0;
    }

    int KeyAtCursor()
    {
        if (m_head != 0)
            return m_cur->key;
        return 0;
    }


    int SeekKey(int key)
    {
        if (m_head) {
            int n = m_count;
            m_curIndex = 0;
            m_cur = m_head;
            for (int i = 0; i < n; i++) {
                if (m_cur->key == key)
                    return 1;
                m_curIndex++;
                m_cur = m_cur->next;
            }
        }
        return 0;
    }

    void SeekForward(int idx)
    {
        if (idx > m_count - 1)
            return;
        m_cur = m_head;
        int n = idx;
        while (n > 0) {
            m_cur = m_cur->next;
            --n;
        }
        m_curIndex = idx;
    }

    void Reset(Heap *arena)
    {
        Clear();
        m_pool = arena;
    }


    int add_first(const char *text, int key, int attr, int span);
};




struct Dialog {

    virtual ~Dialog();
    virtual int attach(WidgetBase *owner, int x, int y, int flags) = 0;
    virtual void on_redraw() = 0;

    Dialog();

    void close();
    int clear_strings();
    int InitOwnArenaAt(int x, int y, int width, int height, int bytes);
    int InitOwnArena(int bytes);
    int InitSharedArena(Heap *arena);
    int InitOwnArenaAt(Rect *rect, int bytes);

    int item(const char *text, int idx);
    void set_selected_id(int key);
    void set_dialog_text_color(int color, int shadowColor, int shadowDx, int shadowDy);
    void set_dialog_text_color2(int color, int shadowColor, int shadowDx, int shadowDy);
    int id_to_pos(int key);
    int pos_to_id(int idx);
    void set_dialog_text_color3(int color, int shadowColor, int shadowDx, int shadowDy);
    void PinSize() { m_sizeFlags |= 3; }

    void UnpinSize() { m_sizeFlags &= ~3; }

    void SetPinHeight(int on);
    void SetPinWidth(int on);

    void SetPinned(int on)
    {
        SetPinHeight(on);
        SetPinWidth(on);
    }

    int LayoutWidth() { return m_width != 0 ? m_width : m_fixedWidth; }
    int LayoutHeight() { return m_height != 0 ? m_height : m_fixedHeight; }

    int set_dialog_font(Font *font, Font *braceFont, Font *bracketFont);

    void PinWidth() { m_sizeFlags |= 2; }
    void PinHeight() { m_sizeFlags |= 1; }

    Heap m_arena;
    Heap *m_sharedArena;
    int m_arenaSize;

    int m_sizeFlags;

    int m_originX;
    int m_originY;

    int m_fixedWidth;
    int m_fixedHeight;
    int m_indent;
    int m_topPadding;
    int m_bottomPadding;
    int m_padding;

    int m_inset;
    int m_fixedRowHeight;


    int m_columnCount;
    int m_maxRows;
    int m_layoutColumns;

    int m_columnStride;

    int m_width;
    int m_height;


    int m_contentWidth;
    int m_rowHeight;
    int m_backColor;




    Font *m_rowFonts[3];
    int m_textColor[3];
    int m_textShadowColor[3];
    int m_textShadowDx[3];
    int m_textShadowDy[3];
    int m_itemStyle[4];
    StringList m_items;


    int m_selected;

    int m_hotIndex;

    int m_notifiedHotIndex;


    int get_selected_id();
    static int set_def_dialog_font(Font *font, Font *braceFont, Font *bracketFont);
    static void set_def_dialog_text_color(int color, int shadowColor, int shadowDx, int shadowDy, int row);
};



extern int g_panelLayout[5];
extern int g_panelSizing[3];
extern int g_panelItemStyle[4];
extern int g_panelSizeFlags;
extern Font *g_panelFonts[3];
extern int g_panelTextColor[3];
extern int g_panelShadowColor[3];
extern int g_panelShadowDx[3];
extern int g_panelShadowDy[3];

struct ListBoxCtl : virtual GraphicWin, virtual Dialog {

    virtual int on_key_down(int vk);

    virtual void on_left_double_click(int x, int y);

    virtual void on_redraw();
    virtual int attach(WidgetBase *owner, int x, int y, int flags);




    int calculate_dimensions();

    virtual void on_mouse_move(int x, int y);



    virtual void on_mouse_leave(int x, int y);
    virtual void on_left_down(int x, int y);
    virtual void on_dialog_focus(int on);

    int ResetAndInit(int bytes)
    {
        close();
        return InitOwnArena(bytes);
    }
    int ResetAndInitAlt(Heap *arena)
    {
        close();
        return InitSharedArena(arena);
    }



    SpriteObject *m_normalSprite;
    SpriteObject *m_selectedSprite;
    SpriteObject *m_hotSprite;


    void(__cdecl *onSelect)(int);
    void(__cdecl *onActivate)(int);
    void SetOnActivate(void(__cdecl *fn)(int)) { onActivate = fn; }


    int m_hotTextColor;
    int m_selectedTextColor;
    ListBoxCtl();


    virtual ~ListBoxCtl() { close(); }
    void close();




    void draw_item(int idx, int selected);

    void DrawLeftAligned(const char *s, Rect *r)
    {
        if (s)
            RenderTextVerticallyCentered(s, r, strlen(s));
    }
    void DrawRightAligned(const char *s, Rect *r)
    {
        if (s)
            RenderTextRightAlignedInRectVCentered(s, r, strlen(s));
    }


    static SpriteObject s_normalCell;
    static SpriteObject s_glowCell;
    static SpriteObject s_highlightCell;
    static SpriteObject *s_defaultSprites[3];


    static int init_class();
    static void close_class();
};


#endif
