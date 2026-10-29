#include "jgldefs.h"
#include "ComboBox.h"
#include "Surface.h"

SpriteObject *ComboBox::m_defaultSprButton[3] = {0};
SpriteObject *ComboBox::m_defaultSprTopBar[3] = {0};

int BasePop::start(const char *file, const char *section, int tag, const char *firstText, unsigned flags)
{
    return start(file, section, tag, firstText, flags, 0);
}

ComboBox::ComboBox()
{
    memset(&m_flags, 0, sizeof(m_flags));
    memset(m_trackSprites, 0, sizeof(m_trackSprites));
    memset(m_buttonSprites, 0, sizeof(m_buttonSprites));
    m_frame.m_owner = this;
    m_scrollList.m_owner = this;
    m_dialog.m_owner = this;
    f0020 = 0;
    m_x = 0;
    m_y = 0;
    m_buttonW = 0;
    m_buttonH = 0;
    m_owner = 0;
    m_selId = -1;
    m_rowFont = 0;
    m_reserved = 0;
}

ComboBox::~ComboBox()
{
    m_frame.close();
    m_scrollList.close();
}

int ComboBox::init(int x, int y, int w, int h, WidgetBase *owner, unsigned flags)
{
    if (flags & 1)
        m_flags |= 1;
    else
        m_flags &= ~1;
    if (flags & 2)
        m_flags |= 4;
    else
        m_flags &= ~4;
    if (flags & 4)
        m_flags |= 8;

    Rect r;
    int th;
    if (ComboBox::m_defaultSprTopBar[0])
        th = ComboBox::m_defaultSprTopBar[0]->m_height;
    else if (m_trackSprites[0])
        th = m_trackSprites[0]->m_height;
    else
        return 7;
    r.SetRect(x, y, w, th);
    int rc = m_frame.init(&r, 0, 0xa0, owner, 0, 0);
    if (rc)
        return rc;
    if (m_flags & 1) {
        m_frame.set_text_color(graphsys->screen_mask() == 1 ? 0x8000001f : 0x8000001f, -1, 2, 2);
        m_frame.SetFontsForStyles(BasePop::def_string_font[3], 0, 0, 0);
    }
    if (ComboBox::m_defaultSprTopBar[0])
        setTopBarSprites(ComboBox::m_defaultSprTopBar);

    int bw, bh;
    if (m_buttonSprites[0]) {
        bh = m_buttonSprites[0]->m_height;
        bw = m_buttonSprites[0]->m_imageWidth;
        m_button.SetImages(m_buttonSprites[0], m_buttonSprites[1], m_buttonSprites[2]);
    } else if (ComboBox::m_defaultSprButton[0]) {
        bh = ComboBox::m_defaultSprButton[0]->m_height;
        bw = ComboBox::m_defaultSprButton[0]->m_imageWidth;
        m_button.SetImages(ComboBox::m_defaultSprButton[0], ComboBox::m_defaultSprButton[1],
            ComboBox::m_defaultSprButton[2]);
    } else {
        bh = 0x19;
        bw = bh;
    }
    if (m_flags & 1) {
        r.right -= bw;
        r.Offset(-r.left, -r.top);
        SetDropRect(&r);
    }
    int bx = m_frame.GetSurfaceWidth() - bw + 5;
    int by = (m_frame.GetSurfaceHeight() - bh) / 2;
    Rect b;
    b.left = bx;
    b.top = by;
    b.bottom = by + bh;
    b.right = bx + bw + 5;
    m_button.init(0, 1, b.left, b.top, b.right - b.left, b.bottom - b.top, &m_frame, 0);
    if (m_buttonSprites[0]) {
        setButtonSprites(m_buttonSprites);
    } else if (ComboBox::m_defaultSprButton[0]) {
        m_button.SetImages(ComboBox::m_defaultSprButton[0], ComboBox::m_defaultSprButton[1],
            ComboBox::m_defaultSprButton[2]);
        setButtonSprites(ComboBox::m_defaultSprButton);
    }
    m_x = x;
    m_y = y;
    m_buttonW = bw;
    m_dropH = h;
    m_owner = owner;
    m_buttonH = bh;
    rc = updateListBoxPositionAndSize(bw, h);
    if (m_flags & 4)
        m_dialog.m_list.ListBox::m_options |= 1;
    m_scrollList.m_bevelWidth = 1;
    m_scrollList.SetBevelColors(graphsys->screen_mask() == 1 ? 0x8000ad72 : 0x800056b2,
        graphsys->screen_mask() == 1 ? 0x8000ad72 : 0x800056b2);
    m_scrollList.m_selTextColor = graphsys->screen_mask() == 1 ? 0x80000185 : 0x800000c5;
    m_scrollList.set_border_thickness(1);
    m_scrollList.m_frameTexture = 0;
    m_scrollList.m_focusColor = graphsys->screen_mask() == 1 ? 0x8000fded : 0x80007eed;
    m_scrollList.m_options |= 1;
    return rc;
}

void ComboBox::show()
{
    updateTopBarText();
    m_frame.show(0);
    m_button.show(0);
}

void ComboBox::updateTopBarText()
{
    const char *text = getCurrentString();
    m_selId = getCurrentID();
    drawTopBarSprites();
    if (m_flags & 1) {
        m_frame.set_text_color(graphsys->screen_mask() == 1 ? 0x8000001f : 0x8000001f, -1, 2, 2);
        m_frame.SetFontsForStyles(BasePop::def_string_font[3], 0, 0, 0);
    }
    Rect r = m_frame.m_local;
    Rect *b = m_button.GetRect();
    r.right += b->left - b->right;
    if (m_flags & 8) {
        if (text) {
            m_frame.write_l(text, 4, 4, strlen(text));
            m_frame.soft_update();
            return;
        }
    } else if (text) {
        m_frame.write_cent_l(text, 0, 0, r.right - r.left, r.bottom - r.top, strlen(text));
    }
    m_frame.soft_update();
}

int ComboBox::updateListBoxPositionAndSize(int w, int h)
{
    int x = m_frame.m_local.left;
    int y = m_frame.m_local.bottom;
    if (m_flags & 4)
        m_owner->client_to_screen(&x, &y);
    if (m_flags & 4) {
        int right = x + m_frame.GetSurfaceWidth();
        m_dialog.set_loc(right - m_dialog.m_list.Dialog::LayoutWidth(), y);
        updateTopBarText();
        return 0;
    }
    m_scrollList.move(x, y + m_buttonH);
    m_scrollList.resize(w, h - m_buttonH, 0, 0);
    m_scrollList.m_fixedWidth = width();
    Rect *lr = &m_scrollList.m_local;
    m_scrollList.m_fixedHeight = lr->bottom - lr->top;
    m_scrollList.PinWidth();
    m_scrollList.PinHeight();
    m_scrollList.m_inset = 0;
    m_scrollList.m_bevelWidth = 0;
    m_scrollList.SetBevelColors(graphsys->screen_mask() == 1 ? 0x8000dc71 : 0x80006e31,
        graphsys->screen_mask() == 1 ? 0x8000dc71 : 0x80006e31);
    m_scrollList.m_selTextColor = graphsys->screen_mask() == 1 ? 0x8000021f : 0x8000011f;
    m_scrollList.set_border_thickness(0);
    m_scrollList.m_frameTexture = 0;
    m_scrollList.m_frameColor = graphsys->screen_mask() == 1 ? 0x80000400 : 0x80000200;
    m_scrollList.m_focusColor = graphsys->screen_mask() == 1 ? 0x8000ff91 : 0x80007fd1;
    m_scrollList.SetEdgeColors(graphsys->screen_mask() == 1 ? 0x80000400 : 0x80000200,
        graphsys->screen_mask() == 1 ? 0x80000400 : 0x80000200);
    m_scrollList.attach(m_owner, m_x, m_frame.m_local.bottom, 0);
    if (m_rowFont)
        m_scrollList.set_dialog_font(m_rowFont, 0, 0);
    m_scrollList.update_changes();
    updateTopBarText();
    return 0;
}

void ComboBox::setListOverlap(WidgetBase *overlap)
{
    m_flags |= 4;
    m_listOverlap = overlap;
}

void ComboBox::hide()
{
    m_frame.hide();
}

void ComboBox::close()
{
    m_frame.close();
}

int ComboBox::width()
{
    return m_frame.GetSurfaceWidth();
}

void ComboBox::hideListBox()
{
    if (m_flags & 4)
        m_dialog.hide();
    else
        m_scrollList.hide();
    m_flags &= ~2;
}

void ComboBox::showListBox()
{
    if (m_flags & 4) {
        setSelectedID(m_selId);
        updateListBoxPositionAndSize(m_buttonW, m_dropH);
        m_dialog.m_dialogFlags |= 0x2210000;
        g_modalFocus = &m_button;
        m_flags |= 2;
        m_dialog.exec(0, 0);
        return;
    }
    m_scrollList.parent->set_dialog_focus(&m_scrollList);
    m_scrollList.show(0);
    m_flags |= 2;
}

void ComboBox::setSelectedID(int id)
{
    if (id == -1)
        return;
    m_selId = id;
    if (!m_frame.is_visible())
        return;
    if (m_flags & 4) {
        if (m_dialog.m_list.Dialog::m_items.SeekKey(id))
            m_dialog.m_list.Dialog::m_items.SeekKey(id);
    } else if (m_scrollList.m_items.SeekKey(m_selId)) {
        StringList *items = &m_scrollList.m_items;
        if (items->m_head && items->m_cur)
            items->m_cur->key = m_selId;
    }
    updateTopBarText();
}

void ComboBox::setTopBarSprites(SpriteObject **sprites)
{
    for (int i = 0; i < 3; i++)
        m_trackSprites[i] = sprites[i];
    SpriteObject *left = m_trackSprites[0];
    int height = left->m_height;
    m_frame.resize(m_frame.GetSurfaceWidth(), height, 0, 0);
    drawTopBarSprites();
}

void ComboBox::drawTopBarSprites()
{
    m_trackSprites[0]->BlitScaled(&m_frame, 0, 0, 0);
    if (m_frame.GetSurfaceWidth() > m_trackSprites[0]->m_width + m_trackSprites[2]->m_width) {
        int h = m_trackSprites[1]->m_height;
        m_frame.TileSprite(m_trackSprites[1], 0, 0, 2, 0, m_frame.GetSurfaceWidth() - 4, h);
    }
    m_trackSprites[2]->BlitScaled(&m_frame, m_frame.GetSurfaceWidth() - m_trackSprites[2]->m_width, 0, 0);
}

void ComboBox::setButtonSprites(SpriteObject **sprites)
{
    for (int i = 0; i < 3; i++)
        m_buttonSprites[i] = sprites[i];
    m_button.SetImages(m_buttonSprites[0], m_buttonSprites[1], m_buttonSprites[2]);
    int frameH = m_frame.GetSurfaceHeight();
    int imageH = m_buttonSprites[0]->m_height;
    int frameW = m_frame.GetSurfaceWidth();
    m_button.move(frameW - m_buttonSprites[0]->m_width - 4, (frameH - imageH) / 2);
    m_button.redraw();
}

void ComboBox::setDefaultTopBarSprites(SpriteObject **sprites)
{
    for (int i = 0; i < 3; i++)
        ComboBox::m_defaultSprTopBar[i] = sprites[i];
}

void ComboBox::deleteDefaultTopBarSprites()
{
    for (int i = 0; i < 3; i++) {
        if (ComboBox::m_defaultSprTopBar[i]) {
            delete ComboBox::m_defaultSprTopBar[i];
            ComboBox::m_defaultSprTopBar[i] = 0;
        }
    }
}

void ComboBox::setDefaultButtonSprites(SpriteObject **sprites)
{
    for (int i = 0; i < 3; i++)
        ComboBox::m_defaultSprButton[i] = sprites[i];
}

void ComboBox::deleteDefaultButtonSprites()
{
    for (int i = 0; i < 3; i++) {
        if (ComboBox::m_defaultSprButton[i]) {
            delete ComboBox::m_defaultSprButton[i];
            ComboBox::m_defaultSprButton[i] = 0;
        }
    }
}

const char *ComboBox::getItemString(int key)
{
    if (m_flags & 4) {
        if (!m_dialog.m_list.Dialog::m_items.SeekKey(key))
            return 0;
        return m_dialog.m_list.Dialog::m_items.current_entry()->text;
    }
    if (!m_scrollList.m_items.SeekKey(key))
        return 0;
    return m_scrollList.m_items.current_entry()->text;
}

const char *ComboBox::getCurrentString()
{
    if (m_flags & 4) {
        m_dialog.m_list.Dialog::m_items.SeekKey(m_dialog.m_list.Dialog::m_items.KeyAtCursor());
        return m_dialog.m_list.Dialog::m_items.current_entry()->text;
    }
    return m_scrollList.m_items.current_entry()->text;
}

int ComboBox::getCurrentID()
{
    if (m_flags & 4)
        return m_dialog.m_list.Dialog::m_items.KeyAtCursor();
    return m_scrollList.m_items.KeyAtCursor();
}

int ComboBox::addItem(char *text, int id)
{
    if (m_flags & 4) {
        if (!(m_dialog.m_dialogFlags & 2))
            m_dialog.init(2, 0);
        return m_dialog.m_list.item(text, id);
    }
    if (m_scrollList.m_items.m_count == 0) {
        ListBox *list = &m_scrollList;
        list->close();
        list->InitOwnArena(0);
    }
    return m_scrollList.item(text, id, 0);
}

CBTopBar::CBTopBar()
{
    m_owner = 0;
    memset(&m_dropRect, 0, sizeof(m_dropRect));
}

CBTopBar::~CBTopBar()
{
    m_owner = 0;
}

void CBTopBar::on_button_clicked(int id)
{
    ImageButton *button = &m_owner->m_button;
    button->m_state.m_pressed = 0;
    button->redraw();
    if (m_owner) {
        if (m_owner->m_flags & 2)
            m_owner->hideListBox();
        else
            m_owner->showListBox();
    }
}

void CBTopBar::OnMouseMoveInternal(int x, int y, int alt)
{
    if (m_owner->m_flags & 1) {
        if (m_cursorRes || m_cursorImage || m_cursorId != 0x7f89)
            set_cursor(0x7f89);
    }
}

void CBListBox::on_left_double_click(int x, int y)
{
    hide();
    ListBox::on_left_double_click(x, y);
}

void CBListBox::on_mouse_move(int x, int y)
{
    parent->set_dialog_focus(this);
    ListBox::on_mouse_move(x, y);
}

void CBListBox::on_left_click(int x, int y)
{
    if (m_owner) {
        m_owner->updateTopBarText();
        m_owner->onSelect();
    }
    m_owner->hideListBox();
}

void CBListPop::on_left_click(int x, int y)
{
    if (m_owner) {
        m_owner->updateTopBarText();
        m_owner->onSelect();
    }
    m_owner->hideListBox();
}

int g_comboSpare[3] = {0};
