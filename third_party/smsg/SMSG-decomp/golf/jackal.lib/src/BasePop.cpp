




















































































































































































































































#include "jgldefs.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "BasePopParts.h"
#define IMAGEBUTTON_DTOR_DEFERRED
#define BASEPOP_DTOR_DEFERRED
#include "BasePop.h"
#include "ClassListPush.h"
#include "ComboBox.h"
#include "StringUtil.h"
#include "Surface.h"
#include "TextRenderer.h"
#include "Text.h"
#include "AssetSurface.h"
#include "JackalUtil.h"

#include "BasePopDtors.h"
#pragma hdrstop


const char g_dialogFieldSeparator[] = "\t";
inline int IsDialogFieldSeparator(char ch)
{
    return ch == g_dialogFieldSeparator[0];
}



int decipherControlLine(char **s)
{
    int found = -1;
    if (s != 0) {
        int idx;
        for (idx = 0; found == -1 && idx < 22; idx++) {
            if (strnicmp(*s, CONTROL_LINES[idx], strlen(CONTROL_LINES[idx])) == 0) {
                *s += strlen(CONTROL_LINES[idx]);
                found = idx;
                purge_spaces(*s);
            }
        }
    }
    return found;
}


BasePop::BasePop()
{
    int i;

    m_keepStyle = 0;
    m_frameStyle = BasePop::def_window_style;
    m_soundHook = BasePop::def_popwave_func;
    m_sounds[0] = g_dlgOpenSound;
    m_sounds[1] = g_dlgCloseSound;
    for (i = 0; i < 3; i++)
        m_skinFrames[i] = BasePop::def_pb_boxsprite[i];
    for (i = 0; i < 3; i++) {
        m_skinEdgeLeft[i] = BasePop::def_pb_spr_left[i];
        m_skinEdgeRight[i] = BasePop::def_pb_spr_right[i];
        m_skinEdgeTop[i] = BasePop::def_pb_spr_top[i];
        m_skinEdgeBottom[i] = BasePop::def_pb_spr_bottom[i];
        m_skinCornerTL[i] = BasePop::def_pb_spr_top_left[i];
        m_skinCornerTR[i] = BasePop::def_pb_spr_top_right[i];
        m_skinCornerBL[i] = BasePop::def_pb_spr_bottom_left[i];
        m_skinCornerBR[i] = BasePop::def_pb_spr_bottom_right[i];
        m_skinCentre[i] = BasePop::def_pb_spr_center[i];
    }
    m_textIndent = 0;
    m_listX = -1;
    m_okCaption = 0;
    m_cancelCaption = 0;
    m_targetWin = this;
    m_choiceCount = 0;
    m_result = 0;
    m_dialogFlags = 0;
    m_extra = 0;
    m_tag = -1;
    for (i = 0; i < 8; i++)
        m_owned[i] = 0;
    m_title = 0;
    m_insetTop = BasePop::def_top_padding;
    m_insetBottom = BasePop::def_bottom_padding;
    m_insetLeft = BasePop::def_left_padding;
    m_insetRight = BasePop::def_right_padding;
    m_margin = BasePop::def_margin_padding;
    m_spacing = BasePop::def_object_padding;
    m_sprite.close();
    m_captionSprite = 0;
    m_icon = 0;
    m_centreImage = BasePop::def_center_sprite;
    m_backSprite = BasePop::def_back_tile;
    m_backTile = BasePop::def_back_tile_b;
    m_buttonCount = 0;
    m_originX = 0;
    m_originY = 0;
    m_frameWidth = 0;
    m_frameHeight = 0;
    m_listHeight = 0;
    f1f88 = 0;
    m_buttonWidth = 0;
    m_buttonHeight = 0;
    m_textHeight = 0;
    m_iconWidth = 0;
    m_iconHeight = 0;
    m_layoutMargin = 0;
    m_layoutSpacing = 0;
    m_layoutTop = 0;
    m_layoutBottom = 0;
    m_layoutLeft = 0;
    m_layoutRight = 0;
    m_posX = 0x1000;
    m_posY = 0x1000;
    m_modalCode = 0;
    m_drawParams[0] = g_dlgDrawParams[0];
    m_drawParams[1] = g_dlgDrawParams[1];
    m_drawParams[2] = g_dlgDrawParams[2];
    m_textFonts.font = BasePop::def_string_font[0];
    m_textColor[0] = BasePop::def_string_color[0];
    m_textShadowColor[0] = BasePop::def_string_shadow[0];
    m_textShadowDx[0] = BasePop::def_string_shadow_off_x[0];
    m_textShadowDy[0] = BasePop::def_string_shadow_off_y[0];
    m_textFonts.braceFont = BasePop::def_string_font[1];
    m_textColor[1] = BasePop::def_string_color[1];
    m_textShadowColor[1] = BasePop::def_string_shadow[1];
    m_textShadowDx[1] = BasePop::def_string_shadow_off_x[1];
    m_textShadowDy[1] = BasePop::def_string_shadow_off_y[1];
    m_textFonts.bracketFont = BasePop::def_string_font[2];
    m_textColor[2] = BasePop::def_string_color[2];
    m_textShadowColor[2] = BasePop::def_string_shadow[2];
    m_textShadowDx[2] = BasePop::def_string_shadow_off_x[2];
    m_textShadowDy[2] = BasePop::def_string_shadow_off_y[2];
    m_textFonts.linkFont = BasePop::def_string_font[3];
    m_textColor[3] = BasePop::def_string_color[3];
    m_textShadowColor[3] = BasePop::def_string_shadow[3];
    m_textShadowDx[3] = BasePop::def_string_shadow_off_x[3];
    m_textShadowDy[3] = BasePop::def_string_shadow_off_y[3];
    m_iconLeftClick = 0;
    m_iconLeftDoubleClick = 0;
    m_iconDraw = 0;
    m_buttonFonts.font = BasePop::def_button_font[0];
    m_buttonTextColor[0] = BasePop::def_button_color[0];
    m_buttonShadowColor[0] = BasePop::def_button_shadow[0];
    m_buttonShadowDx[0] = BasePop::def_button_shadow_off_x[0];
    m_buttonShadowDy[0] = BasePop::def_button_shadow_off_y[0];
    m_buttonFonts.braceFont = BasePop::def_button_font[1];
    m_buttonTextColor[1] = BasePop::def_button_color[1];
    m_buttonShadowColor[1] = BasePop::def_button_shadow[1];
    m_buttonShadowDx[1] = BasePop::def_button_shadow_off_x[1];
    m_buttonShadowDy[1] = BasePop::def_button_shadow_off_y[1];
    m_buttonFonts.bracketFont = BasePop::def_button_font[2];
    m_buttonTextColor[2] = BasePop::def_button_color[2];
    m_buttonShadowColor[2] = BasePop::def_button_shadow[2];
    m_buttonShadowDx[2] = BasePop::def_button_shadow_off_x[2];
    m_buttonShadowDy[2] = BasePop::def_button_shadow_off_y[2];
    m_buttonColor = g_dlgButtonColor;
    m_buttonBevel[0] = g_dlgButtonBevel[0];
    m_buttonBevel[1] = g_dlgButtonBevel[1];
    m_buttonBevel[2] = g_dlgButtonBevel[2];
    m_buttonFrameColor = g_dlgButtonFrameColor;
    m_buttonFrameColorAlt = BasePop::def_button_focus_box_color;
    m_root = 0;
    m_root = g_dialogRoot;
    m_host = BasePop::def_external_parent;
    m_hostX = BasePop::def_external_x;
    m_hostY = BasePop::def_external_y;
    m_hostW = BasePop::def_external_xs;
    m_hostH = BasePop::def_external_ys;
    if (m_host != 0) {
        set_width(m_hostW);
        set_height(m_hostH);
        set_loc(m_hostX, m_hostY);
        m_list.PinSize();
    }
}



void BasePop::close()
{
    int i;

    on_close();
    m_frameStyle = BasePop::def_window_style;
    m_soundHook = BasePop::def_popwave_func;
    m_sounds[0] = g_dlgOpenSound;
    m_sounds[1] = g_dlgCloseSound;
    if (m_okCaption != 0) {
        FreeIfNotNull(m_okCaption);
        m_okCaption = 0;
    }
    if (m_cancelCaption != 0) {
        FreeIfNotNull(m_cancelCaption);
        m_cancelCaption = 0;
    }
    m_targetWin = this;
    for (i = 0; i < 3; i++)
        m_skinFrames[i] = BasePop::def_pb_boxsprite[i];
    for (i = 0; i < 3; i++) {
        m_skinEdgeLeft[i] = BasePop::def_pb_spr_left[i];
        m_skinEdgeRight[i] = BasePop::def_pb_spr_right[i];
        m_skinEdgeTop[i] = BasePop::def_pb_spr_top[i];
        m_skinEdgeBottom[i] = BasePop::def_pb_spr_bottom[i];
        m_skinCornerTL[i] = BasePop::def_pb_spr_top_left[i];
        m_skinCornerTR[i] = BasePop::def_pb_spr_top_right[i];
        m_skinCornerBL[i] = BasePop::def_pb_spr_bottom_left[i];
        m_skinCornerBR[i] = BasePop::def_pb_spr_bottom_right[i];
        m_skinCentre[i] = BasePop::def_pb_spr_center[i];
    }
    for (i = 0; i < 3; i++)
        m_okButtonImages[i] = BasePop::def_pb_ok_lone[i];
    for (i = 0; i < 3; i++)
        m_cancelButtonImages[i] = BasePop::def_pb_cancel_lone[i];
    m_list.ListBox::close();
    m_list.CheckBoxCtl::close();
    m_list.ListBoxCtl::close();
    m_list.SpriteBox::close();
    m_list.EditGroup::close();
    GraphicWin::close();
    m_choiceCount = 0;
    if (!m_keepStyle) {
        m_sprite.close();
        m_captionSprite = 0;
        m_icon = 0;
    }
    m_result = 0;
    m_tag = -1;
    if (m_title != 0) {
        FreeIfNotNull(m_title);
        m_title = 0;
    }
    m_textIndent = 0;
    m_listX = -1;
    m_okButton.close();
    m_cancelButton.close();
    m_lines.Clear();
    m_choices.Clear();
    for (i = 0; i < 8; i++) {
        ImageButton *o = m_owned[i];
        if (o != 0) {
            delete o;
            m_owned[i] = 0;
        }
    }
    m_insetTop = BasePop::def_top_padding;
    m_insetBottom = BasePop::def_bottom_padding;
    m_insetLeft = BasePop::def_left_padding;
    m_insetRight = BasePop::def_right_padding;
    m_margin = BasePop::def_margin_padding;
    m_spacing = BasePop::def_object_padding;
    m_buttonCount = 0;
    m_originX = 0;
    m_originY = 0;
    m_frameWidth = 0;
    m_frameHeight = 0;
    m_listHeight = 0;
    f1f88 = 0;
    m_buttonWidth = 0;
    m_buttonHeight = 0;
    m_textHeight = 0;
    m_iconWidth = 0;
    m_iconHeight = 0;
    m_layoutMargin = 0;
    m_layoutSpacing = 0;
    m_layoutTop = 0;
    m_layoutBottom = 0;
    m_layoutLeft = 0;
    m_layoutRight = 0;
    m_dialogFlags = 0;
    m_extra = 0;
    m_posX = 0x1000;
    m_posY = 0x1000;
    m_modalCode = 0;
    char keep = m_keepStyle;
    if (!keep) {
        m_drawParams[0] = g_dlgDrawParams[0];
        m_drawParams[1] = g_dlgDrawParams[0];
        m_drawParams[2] = g_dlgDrawParams[2];
        m_textFonts.font = BasePop::def_string_font[0];
        m_textColor[0] = BasePop::def_string_color[0];
        m_textShadowColor[0] = BasePop::def_string_shadow[0];
        m_textShadowDx[0] = BasePop::def_string_shadow_off_x[0];
        m_textShadowDy[0] = BasePop::def_string_shadow_off_y[0];
        m_textFonts.braceFont = BasePop::def_string_font[1];
        m_textColor[1] = BasePop::def_string_color[1];
        m_textShadowColor[1] = BasePop::def_string_shadow[1];
        m_textShadowDx[1] = BasePop::def_string_shadow_off_x[1];
        m_textShadowDy[1] = BasePop::def_string_shadow_off_y[1];
        m_textFonts.bracketFont = BasePop::def_string_font[2];
        m_textColor[2] = BasePop::def_string_color[2];
        m_textShadowColor[2] = BasePop::def_string_shadow[2];
        m_textShadowDx[2] = BasePop::def_string_shadow_off_x[2];
        m_textShadowDy[2] = BasePop::def_string_shadow_off_y[2];
        m_textFonts.linkFont = BasePop::def_string_font[3];
        m_textColor[3] = BasePop::def_string_color[3];
        m_textShadowColor[3] = BasePop::def_string_shadow[3];
        m_textShadowDx[3] = BasePop::def_string_shadow_off_x[3];
        m_textShadowDy[3] = BasePop::def_string_shadow_off_y[3];
        m_iconLeftClick = 0;
        m_iconLeftDoubleClick = 0;
        m_iconDraw = 0;
        m_buttonFonts.font = BasePop::def_button_font[0];
        m_buttonTextColor[0] = BasePop::def_button_color[0];
        m_buttonShadowColor[0] = BasePop::def_button_shadow[0];
        m_buttonShadowDx[0] = BasePop::def_button_shadow_off_x[0];
        m_buttonShadowDy[0] = BasePop::def_button_shadow_off_y[0];
        m_buttonFonts.braceFont = BasePop::def_button_font[1];
        m_buttonTextColor[1] = BasePop::def_button_color[1];
        m_buttonShadowColor[1] = BasePop::def_button_shadow[1];
        m_buttonShadowDx[1] = BasePop::def_button_shadow_off_x[1];
        m_buttonShadowDy[1] = BasePop::def_button_shadow_off_y[1];
        m_buttonFonts.bracketFont = BasePop::def_button_font[2];
        m_buttonTextColor[2] = BasePop::def_button_color[2];
        m_buttonShadowColor[2] = BasePop::def_button_shadow[2];
        m_buttonShadowDx[2] = BasePop::def_button_shadow_off_x[2];
        m_buttonShadowDy[2] = BasePop::def_button_shadow_off_y[2];
        m_buttonBevel[0] = g_dlgButtonBevel[0];
        m_buttonBevel[1] = g_dlgButtonBevel[1];
        m_buttonBevel[2] = g_dlgButtonBevel[2];
        m_buttonFrameColor = g_dlgButtonFrameColor;
        m_buttonFrameColorAlt = BasePop::def_button_focus_box_color;
    }
    if (!keep) {
        m_centreImage = BasePop::def_center_sprite;
        m_backSprite = BasePop::def_back_tile;
        m_backTile = BasePop::def_back_tile_b;
    }
    m_root = 0;
    m_root = g_dialogRoot;
    m_buttonColor = g_dlgButtonColor;
    m_host = BasePop::def_external_parent;
    m_hostX = BasePop::def_external_x;
    m_hostY = BasePop::def_external_y;
    m_hostW = BasePop::def_external_xs;
    m_hostH = BasePop::def_external_ys;
    if (m_host != 0) {
        set_width(m_hostW);
        set_height(m_hostH);
        set_loc(m_hostX, m_hostY);
        m_list.PinSize();
    }
    m_arena.shutdown();
}


int BasePop::init(unsigned flags, unsigned arenaBytes)
{
    close();
    if (!(flags & 0x100000))
        m_host = 0;
    if (flags & 1)
        m_list.m_type = MW_CHECKBOX;
    else if (flags & 2)
        m_list.m_type = MW_SCROLLLIST;
    else if (flags & 4)
        m_list.m_type = MW_TEXTINPUT;
    else
        m_list.m_type = MW_LISTBOX;
    if (arenaBytes != 0) {
        if (m_arena.init(arenaBytes))
            return 4;
    }
    if (arenaBytes != 0 && !(flags & 0x40000)) {
        m_lines.Reset(&m_arena);
        m_choices.Reset(&m_arena);
        m_list.init(&m_arena);
    } else {
        m_lines.Reset(0);
        m_choices.Reset(0);
        m_list.init(0);
    }
    m_dialogFlags = flags;
    if (m_host == 0) {
        m_list.UnpinSize();
        set_loc(0x1000, 0x1000);
    } else {
        set_width(m_hostW);
        set_height(m_hostH);
        set_loc(m_hostX, m_hostY);
        m_list.PinSize();
    }
    if (flags & 0x1000)
        set_loc(0x1000, 0x1000);
    m_hotspots.init(10);
    on_init();
    return 0;
}


int SetDefaultOkCaption(const char *s)
{
    if (s == 0)
        return 3;
    if (BasePop::def_ok_text) {
        FreeIfNotNull(BasePop::def_ok_text);
        BasePop::def_ok_text = 0;
    }
    BasePop::def_ok_text = (char *)AllocateBytes(strlen(s) + 1);
    if (BasePop::def_ok_text == 0)
        return 5;
    ClearString(BasePop::def_ok_text);
    AppendString(BasePop::def_ok_text, s);
    return 0;
}


int SetDefaultCancelCaption(const char *s)
{
    if (s == 0)
        return 3;
    if (BasePop::def_cancel_text) {
        FreeIfNotNull(BasePop::def_cancel_text);
        BasePop::def_cancel_text = 0;
    }
    BasePop::def_cancel_text = (char *)AllocateBytes(strlen(s) + 1);
    if (BasePop::def_cancel_text == 0)
        return 4;
    ClearString(BasePop::def_cancel_text);
    AppendString(BasePop::def_cancel_text, s);
    return 0;
}


int BasePop::string(const char *text, char atFront)
{
    int kind = 0;
    if (text == 0)
        return 3;
    if (text[0] == '^') {
        char d = text[1];
        if (d != 0) {
            if (d != '^') {
                kind = 1;
                ++text;
            } else {
                if (text[2] == '^') {
                    kind = 2;
                    text += 3;
                } else {
                    kind = 2;
                    text += 2;
                }
            }
        } else {
            kind = 3;
            ++text;
        }
    }
    int ok = 0;
    if (atFront) {
        if (m_lines.add_first(text, 0, kind, 0))
            ok = 1;
    } else {
        if (m_lines.Push(text, kind, 0, 0))
            ok = 1;
    }
    return ok;
}


int BasePop::button(const char *text)
{
    if (text == 0)
        return 3;
    if (m_choices.Push(text, 0, 0, 0))
        return 1;
    m_choiceCount++;
    return 0;
}


int BasePop::title(const char *title)
{
    if (title == 0)
        return 3;
    if (m_title != 0) {
        FreeIfNotNull(m_title);
        m_title = 0;
    }
    m_title = (char *)AllocateBytes(strlen(title) + 1);
    if (m_title == 0)
        return 4;
    ClearString(m_title);
    AppendString(m_title, title);
    return 0;
}


void BasePop::set_height(int h)
{
    if (m_host == 0 && !(m_dialogFlags & 0x400) && graphsys != 0 && graphsys->screen_width() >= 0x400)
        m_list.m_fixedHeight = h * 3 / 2;
    else
        m_list.m_fixedHeight = h;
}


void BasePop::set_width(int w)
{
    if (m_host == 0 && !(m_dialogFlags & 0x400) && graphsys != 0 && graphsys->screen_width() >= 0x400)
        m_list.m_fixedWidth = w * 3 / 2;
    else
        m_list.m_fixedWidth = w;
}


void BasePop::set_loc(int x, int y)
{
    if (x != 0x2000)
        m_posX = x;
    if (y != 0x2000)
        m_posY = y;
}

void BasePop::write_check(int mask)
{
    m_list.set_state_flag(mask);
}

int BasePop::read_check()
{
    return m_list.GetCheckMask();
}




int BasePop::start(const char *file, const char *section, int tag, const char *firstText, unsigned flags,
    GraphicWin *owner)
{
    char *line = 0;
    int inText = 1;
    int key;
    int w, h;
    char buf[256];

    m_loadFile = file;
    m_loadSection = section;
    m_loadTag = tag;
    m_loadFirstText = firstText;
    m_loadFlags = flags;
    m_loadOwner = owner;
    char haveKey = 0;
    if (!(flags & 0x80))
        close();
    if (flags & 1)
        m_list.m_type = MW_CHECKBOX;
    else if (flags & 2)
        m_list.m_type = MW_SCROLLLIST;
    else if (flags & 4)
        m_list.m_type = MW_TEXTINPUT;
    else
        m_list.m_type = MW_LISTBOX;
    if (!(flags & 0x80)) {
        int err = init(flags, 0);
        if (err)
            return err;
    }
    if (!(flags & 0x100000))
        m_host = 0;
    if (owner) {
        owner->is_visible();
        m_targetWin = owner;
    }
    m_tag = tag;
    if (section == 0)
        return 3;
    if (firstText)
        m_list.EditGroup::set_default(firstText, 0);
    if (file == 0)
        file = BasePop::def_file;
    if (text_open(file, section))
        return 1;
    while (1) {
        line = text_get();
        if (line == 0)
            return 1;
        if (*line == 0) {
            inText = 0;
            continue;
        }
        ClearString(buf);
        parse_string(line, buf);
        line = buf;
        if (buf[0] == '#') {
            int n;
            line = buf + 1;
            switch (decipherControlLine(&line)) {
            case -1:
                if (*line != '#' && !handleGameSpecificControlLines(line)) {
                    text_close();
                    return 0;
                }
                break;
            case DK_XS:
                if (m_host == 0)
                    set_width(atoi(line));
                break;
            case DK_YS:
                if (m_host == 0)
                    set_height(atoi(line));
                break;
            case DK_X: set_loc(atoi(line), 0x2000); continue;
            case DK_Y: set_loc(0x2000, atoi(line)); continue;
            case DK_COL: m_list.m_columnCount = atoi(line); break;
            case DK_CAPTION: title(line); break;
            case DK_CHECKBOX:
                m_dialogFlags |= 1;
                m_list.m_type = MW_CHECKBOX;
                if (atoi(line))
                    m_list.m_maxRows = atoi(line);
                break;
            case DK_EDITBOX:
                m_dialogFlags |= 4;
                m_list.m_type = MW_TEXTINPUT;
                break;
            case DK_LISTBOX:
                m_dialogFlags |= 2;
                m_list.m_type = MW_SCROLLLIST;
                if (atoi(line))
                    m_list.m_maxRows = atoi(line);
                break;
            case DK_ACTIVE: m_tag = atoi(line); break;
            case DK_TEXT: inText = 1; break;
            case DK_ITEMLIST: inText = 0; break;
            case DK_ITEMINDEX:
                haveKey = 1;
                key = atoi(line);
                break;
            case DK_BUTTON: button(line); break;
            case DK_EXTRA: m_extra = atoi(line); break;
            case DK_WAVE:
                if (*line == '2') {
                    n = 1;
                    purge_spaces(++line);
                } else
                    n = 0;
                if (isdigit(*line))
                    m_sounds[n].wave = atoi(line);
                else {
                    ClearString(m_sounds[n].name);
                    AppendString(m_sounds[n].name, line);
                }
                break;
            case DK_VOL:
                if (*line == '2') {
                    n = 1;
                    purge_spaces(++line);
                } else
                    n = 0;
                m_sounds[n].vol = atol(line);
                break;
            case DK_PITCH:
                if (*line == '2') {
                    n = 1;
                    purge_spaces(++line);
                } else
                    n = 0;
                m_sounds[n].pitch = atoi(line);
                break;
            case DK_FADE:
                if (*line == '2') {
                    n = 1;
                    purge_spaces(++line);
                } else
                    n = 0;
                m_sounds[n].fade = atol(line);
                break;
            case DK_PAN:
                if (*line == '2') {
                    n = 1;
                    purge_spaces(++line);
                } else
                    n = 0;
                m_sounds[n].pan = atoi(line);
                break;
            case DK_SPRITE:
                if (AssetSurface::get_pcx_dimensions(line, &w, &h) == 0 && m_sprite.init(line, w, h) == 0)
                    m_icon = &m_sprite;
                break;
            }
        } else if (inText) {
            string(line, 0);
        } else if (m_dialogFlags & 4) {

            if (m_list.get_num_items() < 10) {
                if (firstText && m_list.get_num_items() == 0) {
                    m_list.EditGroup::edit(line, firstText, tag);
                    continue;
                }
                const char *text = parse_strings[m_list.get_num_items()];
                m_list.EditGroup::edit(line, text, tag);
                continue;
            }
            m_list.EditGroup::edit(line, 0, tag);
            continue;
        } else if (haveKey) {
            m_list.item(line, key);
            haveKey = 0;
        } else {
            m_list.item(line, m_list.Dialog::m_items.m_count);
        }
    }
}




static inline int SurfaceWidthOf(GraphicWin *o)
{
    return o->m_surface ? o->m_surface->get_width() : 0;
}
static inline int SurfaceHeightOf(GraphicWin *o)
{
    return o->m_surface ? o->m_surface->get_height() : 0;
}
static inline int DialogWhite()
{
    return graphsys->screen_mask() == 1 ? 0x8000ffff : 0x80007fff;
}




inline int BasePop::FrameWidth() const
{
    return m_frameWidth;
}
inline int BasePop::FrameHeight() const
{
    return m_frameHeight;
}
inline char *BasePop::Title() const
{
    return m_title;
}









int BasePop::exec(int layoutOnly, int (*pfnIdle)())
{
    if (!on_exec())
        return -1;
    int rc = 0;
    if (m_list.Dialog::m_items.GetCount() == 0)
        m_list.m_type = -1;
    if (m_list.SpriteBox::m_cells.GetCount() > 1 && m_host == 0)
        m_list.m_type = MW_SPRITEBOX;
    if (m_host != 0 && m_host->is_visible()) {
        set_width(m_hostW);
        set_height(m_hostH);
        set_loc(m_hostX, m_hostY);
        m_list.SetPinned(1);
    } else {
        m_host = 0;
        m_dialogFlags &= ~0x100000;
    }
    int err = calculate_dimensions();
    if (err != 0)
        return err;
    if (layoutOnly != 0 || (m_dialogFlags & 0x8000))
        return 0;

    WidgetBase *owner = m_host != 0 ? m_host : m_root;
    int style = m_frameStyle;
    if (m_dialogFlags & 0x20)
        style |= 0x20;
    if (m_host == 0)
        style |= 1;
    else
        style |= 0x400000;
    if (g_frameCapTopLeft != 0)
        style |= 0x10000000;
    if (m_captionSprite != 0)
        style |= 0x10000;
    if (m_dialogFlags & 0x4100)
        style |= 0x1000;
    if ((m_dialogFlags & 0x20000) || m_host != 0)
        style |= 0x80000;

    if (m_targetWin == this) {
        int borders[3];
        on_pre_window_init();
        SpriteObject *cap = m_captionSprite;
        if (cap != 0) {
            int h = cap->m_height;
            borders[0] = max(h + 2 * CaptionMargin(), m_captionHeight);
        } else {
            borders[0] = m_captionHeight;
        }
        borders[1] = m_borderWidth;
        borders[2] = m_bottomBorder;
        if (borders[1] == 0)
            style &= ~1;
        err = GraphicWin::init(0, 0, FrameWidth(), FrameHeight(), Title(), style | 0x800, owner, 0, borders);
        if (err != 0)
            return err;
        set_text_color(DialogWhite(), m_textShadowColor[0], m_textShadowDx[0], m_textShadowDy[0]);
        set_text_color2(DialogWhite(), m_textShadowColor[1], m_textShadowDx[1], m_textShadowDy[1]);
        set_text_color3(DialogWhite(), m_textShadowColor[2], m_textShadowDx[2], m_textShadowDy[2]);
        set_text_color_hyper(DialogWhite(), m_textShadowColor[3], m_textShadowDx[3], m_textShadowDy[3]);
        SetFontsForStyles(m_textFonts.font, m_textFonts.braceFont, m_textFonts.bracketFont, m_textFonts.linkFont);
        if (m_title != 0)
            set_caption(m_title);
        on_post_window_init();
    }

    if (m_targetWin == this) {
        if (m_targetWin->m_status & 2) {
            if (m_posX == 0x1000)
                m_originX = (m_targetWin->m_screen.left - m_targetWin->m_screen.right
                                + (!graphsys ? 0 : graphsys->screen_width()))
                            >> 1;
            else if (m_posX < 0)
                m_originX = m_targetWin->m_screen.left - m_targetWin->m_screen.right
                            + (!graphsys ? 0 : graphsys->screen_width()) + m_posX;
            else
                m_originX = m_posX;
            if (m_posY == 0x1000)
                m_originY = 100;
            else if (m_posY < 0)
                m_originY = m_targetWin->m_screen.top - m_targetWin->m_screen.bottom
                            + (!graphsys ? 0 : graphsys->screen_height()) + m_posY;
            else
                m_originY = m_posY;
        } else {
            if (m_posX == 0x1000)
                m_originX = ((!graphsys ? 0 : graphsys->screen_width()) - SurfaceWidthOf(m_targetWin)) >> 1;
            else if (m_posX < 0)
                m_originX = (!graphsys ? 0 : graphsys->screen_width()) - SurfaceWidthOf(m_targetWin) + m_posX;
            else
                m_originX = m_posX;
            if (m_posY == 0x1000)
                m_originY = 100;
            else if (m_posY < 0)
                m_originY = (!graphsys ? 0 : graphsys->screen_height()) - SurfaceHeightOf(m_targetWin) + m_posY;
            else
                m_originY = m_posY;
        }
    } else {
        if (m_posX == 0x1000)
            m_originX = (SurfaceWidthOf(m_targetWin) - m_frameWidth) >> 1;
        else if (m_posX >= 0)
            m_originX = m_posX;
        else
            m_originX = SurfaceWidthOf(m_targetWin) - m_frameWidth + m_posX;
        if (m_posY == 0x1000)
            m_originY = 100;
        else if (m_posY >= 0)
            m_originY = m_posY;
        else
            m_originY = SurfaceHeightOf(m_targetWin) - m_frameHeight + m_posY;
    }

    on_redraw();
    int y = redraw_y;
    if (m_textHeight != 0)
        y += m_layoutSpacing;
    int x = m_listX < 0 ? redraw_x : m_listX;
    if (m_list.m_type != -1) {
        if (m_dialogFlags & 0x800000)
            m_list.Dialog::m_sizeFlags |= 0x1000;
        m_list.SetSelectionBevel(m_dialogFlags & 0x800);
        m_list.attach(m_list.m_type, m_targetWin, x, y, 0x80000);
        m_list.show(0);
    }

    x = m_layoutMargin;
    y = m_frameHeight - m_layoutBottom - m_layoutMargin - m_buttonHeight;
    int pressed = 0;
    int left = x;
    if (m_okButtonImages[0] != 0 && m_okButtonImages[1] != 0 && m_okButtonImages[2] != 0 && !(m_dialogFlags & 0x10000))
        x = GetSurfaceWidth() - m_okButtonImages[0]->m_width * 3;
    on_adjust_button_placement(&x, &y, &pressed);
    int firstX = x;
    if (!(m_dialogFlags & 0x80100) && m_targetWin == this) {
        ImageButton *btn;
        int w, h;
        if (!(m_dialogFlags & 0x10000)) {
            if (x < 0)
                x -= m_layoutSpacing + m_buttonWidth;
            else if (m_okButtonImages[0] == 0)
                x += (m_layoutSpacing + m_buttonWidth) * m_choiceCount;
            if (m_okCaption == 0 && !(m_dialogFlags & 0x1000000)) {
                if (BasePop::def_ok_text != 0) {
                    m_okCaption = (char *)AllocateBytes(strlen(BasePop::def_ok_text) + 1);
                    ClearString(m_okCaption);
                    AppendString(m_okCaption, BasePop::def_ok_text);
                } else {
                    m_okCaption = (char *)AllocateBytes(strlen("OK") + 1);
                    ClearString(m_okCaption);
                    AppendString(m_okCaption, "OK");
                }
            } else if (m_okCaption != 0 && (m_dialogFlags & 0x1000000)) {
                char *p = m_okCaption;
                if (p != 0)
                    free(p);
                m_okCaption = 0;
            }
            w = m_buttonWidth;
            h = m_buttonHeight;
            if (m_okButtonImages[0] != 0) {
                w = m_okButtonImages[0]->m_width;
                h = m_okButtonImages[0]->m_height;
            }
            if (m_okButtonImages[0] != 0 && m_okButtonImages[1] != 0 && m_okButtonImages[2] != 0) {
                btn = &m_okButton;
                btn->init(m_okCaption, -1, x, y, w, h, this, pressed);
                btn->SetImages(m_okButtonImages[0], m_okButtonImages[1], m_okButtonImages[2]);
            } else {
                btn = &m_okButton;
                btn->init(m_okCaption, -1, x, y, w, h, this, pressed);
                btn->SetFrames(m_skinFrames[0], m_skinFrames[1], m_skinFrames[2]);
                btn->SetEdgeLeft(m_skinEdgeLeft[0], m_skinEdgeLeft[1], m_skinEdgeLeft[2]);
                btn->SetEdgeRight(m_skinEdgeRight[0], m_skinEdgeRight[1], m_skinEdgeRight[2]);
                btn->SetEdgeTop(m_skinEdgeTop[0], m_skinEdgeTop[1], m_skinEdgeTop[2]);
                btn->SetEdgeBottom(m_skinEdgeBottom[0], m_skinEdgeBottom[1], m_skinEdgeBottom[2]);
                btn->SetCornerTopLeft(m_skinCornerTL[0], m_skinCornerTL[1], m_skinCornerTL[2]);
                btn->SetCornerTopRight(m_skinCornerTR[0], m_skinCornerTR[1], m_skinCornerTR[2]);
                btn->SetCornerBottomLeft(m_skinCornerBL[0], m_skinCornerBL[1], m_skinCornerBL[2]);
                btn->SetCornerBottomRight(m_skinCornerBR[0], m_skinCornerBR[1], m_skinCornerBR[2]);
                btn->SetCentre(m_skinCentre[0], m_skinCentre[1], m_skinCentre[2]);
            }
            btn->SetFontsForStyles(m_buttonFonts.font, m_buttonFonts.braceFont, m_buttonFonts.bracketFont, 0);
            btn->redraw();
            btn->set_text_color(m_buttonTextColor[0], m_buttonShadowColor[0], m_buttonShadowDx[0], m_buttonShadowDy[0]);
            btn->set_text_color2(m_buttonTextColor[1], m_buttonShadowColor[1], m_buttonShadowDx[1],
                m_buttonShadowDy[1]);
            btn->set_text_color3(m_buttonTextColor[2], m_buttonShadowColor[2], m_buttonShadowDx[2],
                m_buttonShadowDy[2]);
            btn->m_state.Set(0, m_buttonColor);
            btn->m_state.Set(1, m_buttonBevel[0]);
            btn->m_state.Set(2, m_buttonBevel[1]);
            btn->m_state.Set(3, m_buttonBevel[2]);
            btn->m_state.Set(4, m_buttonFrameColor);
            btn->m_state.Set(5, m_buttonFrameColorAlt);
            if (g_dlgOkTooltip != 0)
                btn->set_bubble_text(g_dlgOkTooltip);
            btn->show(0);
        }

        x = left;
        if (left < 0)
            x = left - (m_layoutSpacing + m_buttonWidth) * (m_buttonCount - 1);
        m_choices.SetPos(-1);
        for (int i = 0; i < m_choices.m_count; i++) {
            m_owned[i] = new ImageButton;
            m_owned[i]->init(m_choices.NextText(), i + 1, x, y, m_buttonWidth, m_buttonHeight, this, pressed);
            m_owned[i]->SetFrames(m_skinFrames[0], m_skinFrames[1], m_skinFrames[2]);
            m_owned[i]->SetEdgeLeft(m_skinEdgeLeft[0], m_skinEdgeLeft[1], m_skinEdgeLeft[2]);
            m_owned[i]->SetEdgeRight(m_skinEdgeRight[0], m_skinEdgeRight[1], m_skinEdgeRight[2]);
            m_owned[i]->SetEdgeTop(m_skinEdgeTop[0], m_skinEdgeTop[1], m_skinEdgeTop[2]);
            m_owned[i]->SetEdgeBottom(m_skinEdgeBottom[0], m_skinEdgeBottom[1], m_skinEdgeBottom[2]);
            m_owned[i]->SetCornerTopLeft(m_skinCornerTL[0], m_skinCornerTL[1], m_skinCornerTL[2]);
            m_owned[i]->SetCornerTopRight(m_skinCornerTR[0], m_skinCornerTR[1], m_skinCornerTR[2]);
            m_owned[i]->SetCornerBottomLeft(m_skinCornerBL[0], m_skinCornerBL[1], m_skinCornerBL[2]);
            m_owned[i]->SetCornerBottomRight(m_skinCornerBR[0], m_skinCornerBR[1], m_skinCornerBR[2]);
            m_owned[i]->SetCentre(m_skinCentre[0], m_skinCentre[1], m_skinCentre[2]);
            btn = m_owned[i];
            btn->SetFontsForStyles(m_buttonFonts.font, m_buttonFonts.braceFont, m_buttonFonts.bracketFont, 0);
            btn->redraw();
            m_owned[i]->set_text_color(m_buttonTextColor[0], m_buttonShadowColor[0], m_buttonShadowDx[0],
                m_buttonShadowDy[0]);
            m_owned[i]->set_text_color2(m_buttonTextColor[1], m_buttonShadowColor[1], m_buttonShadowDx[1],
                m_buttonShadowDy[1]);
            m_owned[i]->set_text_color3(m_buttonTextColor[2], m_buttonShadowColor[2], m_buttonShadowDx[2],
                m_buttonShadowDy[2]);
            m_owned[i]->m_state.Set(0, m_buttonColor);
            m_owned[i]->m_state.Set(1, m_buttonBevel[0]);
            m_owned[i]->m_state.Set(2, m_buttonBevel[1]);
            m_owned[i]->m_state.Set(3, m_buttonBevel[2]);
            m_owned[i]->m_state.Set(4, m_buttonFrameColor);
            m_owned[i]->m_state.Set(5, m_buttonFrameColorAlt);
            m_owned[i]->show(0);
            x += m_buttonWidth + m_layoutSpacing;
        }

        if (m_dialogFlags & 0x40) {
            x = firstX;
            if (firstX >= 0)
                x = (m_layoutSpacing + m_buttonWidth) * (m_buttonCount - 1) + firstX;
            if (m_cancelCaption == 0 && !(m_dialogFlags & 0x1000000)) {
                if (BasePop::def_cancel_text != 0) {
                    m_cancelCaption = (char *)AllocateBytes(strlen(BasePop::def_cancel_text) + 1);
                    ClearString(m_cancelCaption);
                    AppendString(m_cancelCaption, BasePop::def_cancel_text);
                } else {
                    m_cancelCaption = (char *)AllocateBytes(strlen("Cancel") + 1);
                    ClearString(m_cancelCaption);
                    AppendString(m_cancelCaption, "OK");
                }
            } else if (m_cancelCaption != 0 && (m_dialogFlags & 0x1000000)) {
                char *p = m_cancelCaption;
                if (p != 0)
                    free(p);
                m_cancelCaption = 0;
            }
            if (m_cancelButtonImages[0] != 0) {
                x = GetSurfaceWidth() - m_cancelButtonImages[0]->m_width * 2;
                w = m_cancelButtonImages[0]->m_width;
                h = m_cancelButtonImages[0]->m_height;
            } else {
                w = m_buttonWidth;
                h = m_buttonHeight;
            }
            btn = &m_cancelButton;
            btn->init(m_cancelCaption, -2, x, y, w, h, this, pressed);
            if (m_cancelButtonImages[0] != 0 && m_cancelButtonImages[1] != 0 && m_cancelButtonImages[2] != 0) {
                btn->SetImages(m_cancelButtonImages[0], m_cancelButtonImages[1], m_cancelButtonImages[2]);
            } else {
                btn->SetFrames(m_skinFrames[0], m_skinFrames[1], m_skinFrames[2]);
                btn->SetEdgeLeft(m_skinEdgeLeft[0], m_skinEdgeLeft[1], m_skinEdgeLeft[2]);
                btn->SetEdgeRight(m_skinEdgeRight[0], m_skinEdgeRight[1], m_skinEdgeRight[2]);
                btn->SetEdgeTop(m_skinEdgeTop[0], m_skinEdgeTop[1], m_skinEdgeTop[2]);
                btn->SetEdgeBottom(m_skinEdgeBottom[0], m_skinEdgeBottom[1], m_skinEdgeBottom[2]);
                btn->SetCornerTopLeft(m_skinCornerTL[0], m_skinCornerTL[1], m_skinCornerTL[2]);
                btn->SetCornerTopRight(m_skinCornerTR[0], m_skinCornerTR[1], m_skinCornerTR[2]);
                btn->SetCornerBottomLeft(m_skinCornerBL[0], m_skinCornerBL[1], m_skinCornerBL[2]);
                btn->SetCornerBottomRight(m_skinCornerBR[0], m_skinCornerBR[1], m_skinCornerBR[2]);
                btn->SetCentre(m_skinCentre[0], m_skinCentre[1], m_skinCentre[2]);
            }
            x -= m_layoutSpacing + m_buttonWidth;
            btn->SetFontsForStyles(m_buttonFonts.font, m_buttonFonts.braceFont, m_buttonFonts.bracketFont, 0);
            btn->redraw();
            btn->set_text_color(m_buttonTextColor[0], m_buttonShadowColor[0], m_buttonShadowDx[0], m_buttonShadowDy[0]);
            btn->set_text_color2(m_buttonTextColor[1], m_buttonShadowColor[1], m_buttonShadowDx[1],
                m_buttonShadowDy[1]);
            btn->set_text_color3(m_buttonTextColor[2], m_buttonShadowColor[2], m_buttonShadowDx[2],
                m_buttonShadowDy[2]);
            m_cancelButton.m_state.Set(0, m_buttonColor);
            m_cancelButton.m_state.Set(1, m_buttonBevel[0]);
            m_cancelButton.m_state.Set(2, m_buttonBevel[1]);
            m_cancelButton.m_state.Set(3, m_buttonBevel[2]);
            m_cancelButton.m_state.Set(4, m_buttonFrameColor);
            m_cancelButton.m_state.Set(5, m_buttonFrameColorAlt);
            if (g_dlgCancelTooltip != 0)
                btn->set_bubble_text(g_dlgCancelTooltip);
            btn->show(0);
        }
    }

    if (!(m_dialogFlags & 0x400000) && m_soundHook != 0)
        m_soundHook(&m_sounds[0], 1);
    show(3);
    if (!(m_dialogFlags & 0x4100) && m_targetWin == this) {
        ++BasePop::modal_count;
        WidgetBase *saved = g_dialogOverride;
        g_dialogOverride = this;
        if (m_host != 0)
            rc = m_host->set_modal((m_dialogFlags >> 21) & 1, pfnIdle, 0);
        else
            rc = set_modal((m_dialogFlags >> 21) & 1, pfnIdle, 0);
        g_dialogOverride = saved;
        BasePop::last_x = m_screen.left;
        BasePop::last_y = m_screen.top;
    }
    if (!(m_dialogFlags & 0x4300) && m_targetWin == this) {
        if (--BasePop::modal_count == 0)
            BasePop::fallout_flag = 0;
        on_release_modal();
        hide();
        if (!(m_dialogFlags & 0x400000) && m_soundHook != 0)
            m_soundHook(&m_sounds[1], 0);
    }
    if (m_list.m_type == MW_TEXTINPUT) {
        for (int i = 0; i < 8; i++)
            parse_says(i, m_list.EditGroup::get_text(i), -1, -1);
    }
    if (rc != 0) {
        if (m_dialogFlags & 0x40)
            return -1;
        return BasePop::fallout_result;
    }
    return m_result;
}


























































































































int BasePop::calculate_dimensions()
{
    m_targetWin->set_text_color(m_textColor[0], m_textShadowColor[0], m_textShadowDx[0], m_textShadowDy[0]);
    m_targetWin->set_text_color2(m_textColor[1], m_textShadowColor[1], m_textShadowDx[1], m_textShadowDy[1]);
    m_targetWin->set_text_color3(m_textColor[2], m_textShadowColor[2], m_textShadowDx[2], m_textShadowDy[2]);
    m_targetWin->set_text_color_hyper(m_textColor[3], m_textShadowColor[3], m_textShadowDx[3], m_textShadowDy[3]);
    m_targetWin->SetFontsForStyles(m_textFonts.font, m_textFonts.braceFont, m_textFonts.bracketFont,
        m_textFonts.linkFont);

    m_buttonCount = m_choiceCount;
    if (m_dialogFlags & 0x40)
        m_buttonCount++;
    if (!(m_dialogFlags & 0x10100))
        m_buttonCount++;

    if (m_host != 0) {
        m_layoutMargin = 0;
        m_layoutSpacing = m_spacing;
    } else if (!(m_dialogFlags & 0x400) && graphsys != 0 && graphsys->screen_width() >= 0x400) {
        m_layoutMargin = m_margin * 3 / 2;
        m_layoutSpacing = m_spacing * 3 / 2;
    } else {
        m_layoutMargin = m_margin;
        m_layoutSpacing = m_spacing;
    }
    if (m_host != 0) {
        m_layoutTop = m_insetTop;
        m_layoutBottom = m_insetBottom;
    } else {
        m_layoutTop = m_insetTop;
        m_layoutBottom = m_insetBottom;
    }

    SpriteObject *icon = m_icon;
    int left = m_insetLeft;
    if (icon != 0) {
        int right = m_insetRight;
        m_layoutLeft = left;
        m_layoutRight = right;
        int iconGap = m_lines.GetCount() != 0 ? m_layoutSpacing : 0;
        int num = m_drawParams[0];
        int den = m_drawParams[1];
        m_iconWidth = icon->m_imageWidth * num / den + m_layoutMargin + iconGap;
        m_iconHeight = icon->m_imageHeight * num / den;
    } else {
        int right = m_insetRight;
        m_layoutLeft = left;
        m_layoutRight = right;
        m_iconWidth = 0;
        m_iconHeight = 0;
    }

    int capHeight = m_buttonFonts.font->m_capHeight;
    if (m_host != 0) {
        set_width(m_hostW - m_layoutMargin * 2 - m_insetRight - m_layoutLeft);
        set_height(m_hostH - m_layoutBottom - m_layoutTop - (m_buttonCount != 0 ? m_buttonHeight : 0));
        set_loc(m_hostX, m_hostY);
        Dialogs *list = &m_list;
        list->PinSize();
    }

    m_list.calculate_dimensions();
    if (m_host != 0) {
        m_frameWidth = m_list.LayoutWidth();
        m_frameHeight = m_list.LayoutHeight();
    } else {
        m_frameWidth = max(m_list.LayoutWidth(), m_iconWidth);
        if (m_captionFont != 0) {
            SetFontsForStyles(m_captionFont, 0, 0, 0);
            m_frameWidth = max(m_frameWidth, text_width(m_title));
            m_targetWin->SetFontsForStyles(m_textFonts.font, m_textFonts.braceFont, m_textFonts.bracketFont,
                m_textFonts.linkFont);
        }
        int dropWidth = 0;
        if (g_parse_cb_dropdowns[0].list != 0 && g_parse_cb_dropdowns[0].list->getCurrentString() != 0)
            dropWidth = max(200, text_width(g_parse_cb_dropdowns[0].list->getCurrentString())) + m_iconWidth + 50;
        int width = max(m_frameWidth, dropWidth);
        m_frameWidth = width;
        if (m_listX == -1)
            m_frameWidth = width + m_layoutMargin * 2;
        else
            m_frameWidth = m_layoutMargin + m_listX + width;
    }

    m_listHeight = m_list.m_height;
    switch (m_list.m_type) {
    case -1: break;
    case MW_CHECKBOX: m_list.CheckBoxCtl::SetOnActivate(OnListItemActivated); break;
    case MW_SCROLLLIST:
        if (m_tag != -1) {
            m_list.set_selected_id(m_tag);
            m_list.ListBox::SetOnActivate(OnListItemActivated);
        } else {
            m_list.set_selected_pos(0, 1);
            m_list.ListBox::SetOnActivate(OnListItemActivated);
        }
        break;
    case MW_TEXTINPUT:
        if (m_tag != -1)
            m_list.set_text_limits(m_tag);
        break;
    case MW_SPRITEBOX:
        m_list.SetOnLeftClick(m_iconLeftClick);
        m_list.SetOnLeftDoubleClick(m_iconLeftDoubleClick);
        m_list.SetOnDrawCell(m_iconDraw);
        m_list.m_scaleDen = m_drawParams[1];
        m_list.m_scaleNum = m_drawParams[0];
        break;
    default:
        if (m_tag != -1)
            m_list.Dialog::set_selected_id(m_tag);
        else
            m_list.m_selected = 0;
        m_list.ListBoxCtl::SetOnActivate(OnListItemActivated);
        break;
    }

    m_lines.SetPos(0);
    int width = -m_layoutMargin * 2 - m_textIndent - m_iconWidth + m_frameWidth;
    int kind = m_lines.AttrAtCursor();
    m_textHeight = 0;
    if (kind == 3) {
        m_textHeight += m_targetWin->text_line_height();
        m_targetWin->TextLayout_BeginWithWidth(0, width);
    } else if (kind == 2) {
        m_textHeight += m_targetWin->wrap_height(m_lines.TextAtCursor(), width, 0, 0);
        m_targetWin->TextLayout_BeginWithWidth(0, width);
    } else {
        m_targetWin->TextLayout_BeginWithWidth(m_lines.TextAtCursor(), width);
    }
    for (int i = 0; i < m_lines.GetCount() - 1; i++) {
        char *text = m_lines.NextText();
        kind = m_lines.AttrAtCursor();
        if (kind != 0) {
            m_textHeight += m_targetWin->wrap_height_flying_flush();
            if (kind == 2) {
                m_textHeight += wrap_height(text, width, 0, 0);
                m_targetWin->TextLayout_BeginWithWidth(0, width);
            } else if (kind == 3) {
                m_textHeight += m_targetWin->text_line_height();
                m_targetWin->TextLayout_BeginWithWidth(0, width);
            } else {
                m_targetWin->TextLayout_BeginWithWidth(m_lines.TextAtCursor(), width);
            }
        } else {
            m_targetWin->wrap_height_flying(m_lines.TextAtCursor());
        }
    }
    m_textHeight += m_targetWin->wrap_height_flying_flush();
    int textHeight = m_textHeight;

    if (m_buttonCount != 0) {
        m_buttonHeight = capHeight * 7 / 4;
        m_buttonWidth = (-m_layoutMargin * 2 - (m_buttonCount - 1) * m_layoutSpacing + m_frameWidth) / m_buttonCount;
    } else {
        m_buttonHeight = 0;
        m_buttonWidth = 0;
    }
    if (m_host != 0 && (m_dialogFlags & 2)) {
        set_height(m_hostH - m_layoutSpacing * 2 - textHeight - m_buttonHeight);
        int rc = m_list.calculate_dimensions();
        if (rc != 0)
            return rc;
    }
    on_adjust_button_width();
    if (m_dialogFlags & 0x100)
        m_buttonHeight = 0;
    if (m_host != 0) {
        set_width(m_hostW - m_layoutMargin * 2 - m_layoutRight - m_layoutLeft);
        set_height(m_hostH - m_layoutBottom - max(m_textHeight, m_iconHeight)
                   - (m_buttonCount != 0 ? m_buttonHeight : 0) - m_layoutTop);
        set_loc(m_hostX, m_hostY);
        Dialogs *list = &m_list;
        list->PinSize();
    }
    if (m_host != 0) {
        m_frameWidth = m_hostW;
        m_frameHeight = m_hostH;
        if (m_list.m_type == -1) {
            m_listHeight = 0;
            m_list.m_height = 0;
        }
    } else {
        m_frameHeight = max(m_textHeight, m_iconHeight) + m_layoutMargin * 2 + m_listHeight + m_buttonHeight;
        if (m_listHeight != 0)
            m_frameHeight = m_frameHeight + m_layoutSpacing;
        if (m_buttonHeight != 0)
            m_frameHeight = m_frameHeight + m_layoutSpacing;
        m_frameHeight = m_frameHeight + m_layoutBottom + m_layoutTop;
        m_frameWidth = m_frameWidth + m_layoutRight + m_layoutLeft;
    }
    redraw_x = (m_targetWin != this ? m_originX : 0) + m_layoutMargin + m_layoutLeft + m_iconWidth;
    redraw_y = (m_targetWin != this ? m_originY : 0) + m_textHeight + m_layoutTop + m_layoutMargin;
    on_post_calculate_dimensions();
    return 0;
}



void BasePop::on_button_clicked(int id)
{
    m_result = 0;
    if (m_list.get_num_items() != 0) {
        if (m_list.m_type == MW_CHECKBOX)
            m_result = m_list.GetCheckMask();
        else
            m_result = m_list.get_selected_id();
    }
    if (!TakeButtonCode(id))
        return;
    if (!(m_dialogFlags & 0x100)) {
        WidgetBase *host = m_host;
        if (host != 0)
            host->release_modal();
        else
            release_modal();
    }
    if (m_dialogFlags & 0x4000)
        hide();
}

int BasePop::set_def_file(const char *file)
{
    if (file == 0 || strlen(file) > 0x4f)
        return 3;
    BasePop::def_file[0] = 0;
    strcat(BasePop::def_file, file);
    return 0;
}

int BasePop::on_key_down(int vk)
{
    if (m_dialogFlags & 0x4000)
        return 0;
    m_result = 0;
    int mode = m_list.m_type;
    if (mode != -1)
        CaptureSelection();
    return 1;
}


int BasePop::on_key_click(int ch, int vk)
{
    return !(m_dialogFlags & 0x4000);
}


int BasePop::on_nc_hittest(int x, int y)
{
    if (m_dialogFlags & 0x2000000)
        return 0;
    return GraphicWin::on_nc_hittest(x, y);
}


int BasePop::on_key_up(int vk)
{
    return !(m_dialogFlags & 0x4000);
}


void OnListItemActivated(int)
{
    BasePop *g = (BasePop *)g_dialogOverride;
    if (g == 0)
        return;
    int mode = g->m_list.m_type;
    unsigned flags = g->m_dialogFlags;
    if (mode != MW_CHECKBOX) {
        if (mode != -1)
            g->CaptureSelection();
        g->m_modalCode = 0;
        if (!(flags & 0x100)) {
            if (g->m_host != 0) {
                g->m_host->release_modal();
            } else {
                g->release_modal();
                g->on_release_modal();
            }
        }
        if (flags & 0x4000)
            g->hide();
    }
}


int BasePop::init_class()
{
    BasePop::def_string_font[0] = g_defaultFont;
    BasePop::def_button_font[0] = g_defaultFont;
    SetDefaultCancelCaption("Cancel");
    SetDefaultOkCaption("OK");
    g_dlgOkTooltip = 0;
    g_dlgCancelTooltip = 0;
    return 0;
}


void BasePop::close_class()
{
    BasePop::def_string_font[0] = 0;
    BasePop::def_button_font[0] = 0;
    if (BasePop::def_ok_text) {
        FreeIfNotNull(BasePop::def_ok_text);
        BasePop::def_ok_text = 0;
    }
    if (BasePop::def_cancel_text) {
        FreeIfNotNull(BasePop::def_cancel_text);
        BasePop::def_cancel_text = 0;
    }
}


int Dialog_SetGlobalTextFonts(Font *font, Font *braceFont, Font *bracketFont, Font *linkFont)
{
    if (font == 0)
        return 3;
    if (font->m_owner)
        BasePop::def_string_font[0] = font;
    BasePop::def_string_font[1] = braceFont;
    BasePop::def_string_font[2] = bracketFont;
    BasePop::def_string_font[3] = linkFont;
    return 0;
}


void Dialog_SetGlobalTextColors(int color, int shadowColor, int shadowDx, int shadowDy)
{
    BasePop::def_string_color[0] = color;
    BasePop::def_string_shadow[0] = shadowColor;
    BasePop::def_string_shadow_off_x[0] = shadowDx;
    BasePop::def_string_shadow_off_y[0] = shadowDy;
}


int Dialog_SetGlobalButtonFonts(Font *font, Font *braceFont, Font *bracketFont)
{
    if (font == 0)
        return 3;
    if (font->m_owner)
        BasePop::def_button_font[0] = font;
    BasePop::def_button_font[1] = braceFont;
    BasePop::def_button_font[2] = bracketFont;
    return 0;
}


void Dialog_SetGlobalButtonColors(int color, int shadowColor, int shadowDx, int shadowDy)
{
    BasePop::def_button_color[0] = color;
    BasePop::def_button_shadow[0] = shadowColor;
    BasePop::def_button_shadow_off_x[0] = shadowDx;
    BasePop::def_button_shadow_off_y[0] = shadowDy;
}


int BasePop::on_modal()
{
    return BasePop::fallout_flag == 0;
}







void BasePop::on_redraw()
{
    if (graphsys)
        graphsys->set_sprite_scale(1, 1, 1);
    clear_links();
    int x = m_layoutLeft + m_layoutMargin + m_textIndent;
    int y = m_layoutTop + m_layoutMargin;
    int iconY = y;
    int top = y;
    if (m_targetWin == this) {
        move(m_originX, m_originY);
        if (on_draw_background() == 0) {
            SpriteObject *back = m_backSprite;
            if ((back || m_backTile) && m_host == 0) {
                if (back) {
                    int h = GetSurfaceHeight();
                    int w = GetSurfaceWidth();
                    TileSprite(m_backSprite, 0, 0, 0, 0, w, h);
                } else {
                    int h = GetSurfaceHeight();
                    int w = GetSurfaceWidth();
                    tile(m_backTile, 0, 0, 0, 0, w, h);
                }
            } else {
                EraseWholeBackground(m_drawParams[2]);
            }
        }
        if (m_host == 0 && m_centreImage) {
            int cw = m_centreImage->m_imageWidth;
            if (cw < GetSurfaceWidth()) {
                int ch = m_centreImage->m_imageHeight;
                if (ch < GetSurfaceHeight()) {
                    AssetSurface *target = this;
                    int h = GetSurfaceHeight();
                    int imageH = m_centreImage->m_height;
                    int w = GetSurfaceWidth();
                    SpriteObject *image = m_centreImage;
                    image->BlitScaled(target, (w - image->m_width - m_textIndent) / 2 + m_textIndent, (h - imageH) / 2,
                        0);
                }
            }
        }
    } else {
        on_draw_background();
        x += m_originX;
        y += m_originY;
        iconY = y;
    }
    if (m_icon) {
        int num = m_drawParams[0];
        if (graphsys)
            graphsys->set_sprite_scale(num, num, m_drawParams[1]);
        current_sprite_draw_popup = this;
        if (m_iconDraw) {
            m_iconDraw(m_icon, m_targetWin, m_list.pos_to_id(0), x, y);
        } else {
            AssetSurface *target = m_targetWin;
            SpriteObject *icon = m_icon;
            int by = iconY - icon->m_top * m_drawParams[0] / m_drawParams[1];
            int bx = x - icon->m_left * m_drawParams[0] / m_drawParams[1];
            icon->BlitScaled(target, bx, by, 0);
        }
        current_sprite_draw_popup = 0;
        if (graphsys)
            graphsys->set_sprite_scale(1, 1, 1);
    }
    x += m_iconWidth;
    if (m_textHeight == 0) {
        redraw_x = x - m_iconWidth - m_textIndent;
        redraw_y = y > m_iconHeight + top ? y : m_iconHeight + top;
        return;
    }
    m_targetWin->set_text_color(m_textColor[0], m_textShadowColor[0], m_textShadowDx[0], m_textShadowDy[0]);
    m_targetWin->set_text_color2(m_textColor[1], m_textShadowColor[1], m_textShadowDx[1], m_textShadowDy[1]);
    m_targetWin->set_text_color3(m_textColor[2], m_textShadowColor[2], m_textShadowDx[2], m_textShadowDy[2]);
    m_targetWin->set_text_color_hyper(m_textColor[3], m_textShadowColor[3], m_textShadowDx[3], m_textShadowDy[3]);
    m_targetWin->SetFontsForStyles(m_textFonts.font, m_textFonts.braceFont, m_textFonts.bracketFont,
        m_textFonts.linkFont);
    int width = m_frameWidth - m_layoutRight - (m_layoutMargin << 1) - m_layoutLeft - m_iconWidth - m_textIndent;
    int bottom = m_targetWin->write_strings(&m_lines, x, y, width, 0);
    redraw_x = x - m_iconWidth - m_textIndent;
    redraw_y = bottom > m_iconHeight + top ? bottom : m_iconHeight + top;
}



void BasePop::on_redraw_nc(int which)
{
    GraphicWin::on_redraw_nc(which);
    if (!m_topStrip)
        return;
    if (which != -1 && which != 0)
        return;
    if (!m_captionSprite)
        return;
    Rect r;
    if (m_topStrip->m_surface)
        m_topStrip->m_surface->SURF_CopyBoundsRect(&r);
    int d = -m_captionMargin;
    r.left -= d;
    r.right += d;
    r.top -= d;
    r.bottom += d;
    m_captionSprite->BlitScaled(m_topStrip, r.left, r.top, 0);
}


BasePop *BasePop::basepop_alloc()
{
    return new BasePop;
}

void BasePop::set_def_external_window(WidgetBase *host, int x, int y, int w, int h)
{
    BasePop::def_external_parent = host;
    BasePop::def_external_x = x;
    BasePop::def_external_y = y;
    BasePop::def_external_xs = w;
    BasePop::def_external_ys = h;
}




void BasePop::on_left_click(int, int)
{
}


void BasePop::on_left_down(int x, int y)
{
    if (m_hotspots.check(x, y, &x, &y) != -1) {
        if (y == 0)
            on_button_clicked(x);
    }
}


void BasePop::set_sprites(SpriteObject *normal, SpriteObject *selected, SpriteObject *hot)
{
    m_list.m_normalSprite = normal;
    m_list.m_selectedSprite = selected;
    m_list.m_hotSprite = hot;
}


void BasePop::set_highlight_color(int color)
{
    m_list.m_hotTextColor = color;
}


void BasePop::set_selected_color(int color)
{
    m_list.m_selectedTextColor = color;
}

inline unsigned BasePopPadA(unsigned a, unsigned b)
{
    unsigned c = a * b;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    return c + b;
}




void Dialog::SetPinHeight(int on)
{
    if (on)
        m_sizeFlags |= 1;
    else
        m_sizeFlags &= ~1;
}

void Dialog::SetPinWidth(int on)
{
    if (on)
        m_sizeFlags |= 2;
    else
        m_sizeFlags &= ~2;
}

inline unsigned BasePopPadB(unsigned a, unsigned b)
{
    unsigned c = a * b;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    return c + b;
}





char BasePop::handleGameSpecificControlLines(char *line)
{
    return 0;
}

















char BasePop::def_file[80] = {0};
DialogSound g_dlgOpenSound = {0};
DialogSound g_dlgCloseSound = {0};
int redraw_y = 0;
int redraw_x = 0;
static int s_unused0 = 0;
BasePop *current_sprite_draw_popup = 0;
void (*BasePop::def_popwave_func)(DialogSound *sound, int opening) = 0;
int BasePop::fallout_flag = 0;
int BasePop::modal_count = 0;
WidgetBase *BasePop::def_external_parent = 0;
int BasePop::def_external_x = 0;
int BasePop::def_external_y = 0;
BoxSpriteParams *BasePop::def_pb_boxsprite[3] = {0};
SpriteObject *BasePop::def_pb_spr_left[3] = {0};
SpriteObject *BasePop::def_pb_spr_right[3] = {0};
SpriteObject *BasePop::def_pb_spr_top[3] = {0};
SpriteObject *BasePop::def_pb_spr_bottom[3] = {0};
SpriteObject *BasePop::def_pb_spr_top_left[3] = {0};
SpriteObject *BasePop::def_pb_spr_top_right[3] = {0};
SpriteObject *BasePop::def_pb_spr_bottom_left[3] = {0};
SpriteObject *BasePop::def_pb_spr_bottom_right[3] = {0};
SpriteObject *BasePop::def_pb_spr_center[3] = {0};
SpriteObject *BasePop::def_pb_ok_lone[3] = {0};
SpriteObject *BasePop::def_pb_cancel_lone[3] = {0};
WidgetBase *g_dialogOverride = 0;
char *BasePop::def_ok_text = 0;
char *BasePop::def_cancel_text = 0;
const char *g_dlgOkTooltip = 0;
const char *g_dlgCancelTooltip = 0;
int BasePop::def_left_padding = 0;
int BasePop::def_right_padding = 0;
int BasePop::def_bottom_padding = 0;
int BasePop::def_top_padding = 0;
Font *BasePop::def_string_font[4] = {0};
Font *BasePop::def_button_font[3] = {0};
int g_dlgButtonFrameColor = 0;
int BasePop::def_button_focus_box_color = 0;
AssetSurface *BasePop::def_back_tile_b = 0;
SpriteObject *BasePop::def_back_tile = 0;
SpriteObject *BasePop::def_center_sprite = 0;
int BasePop::last_x = 0;
int BasePop::last_y = 0;
int BasePop::def_window_style = 0;
WidgetBase *g_dialogRoot = 0;







int BasePop::fallout_result = -1;
int BasePop::def_external_xs = 100;
int BasePop::def_external_ys = 100;



BasePop *(*g_dialogFactory)() = BasePop::basepop_alloc;
int BasePop::def_margin_padding = 10;
int BasePop::def_object_padding = 4;
int g_dlgDrawParams[3] = {1, 1, 255};
int BasePop::def_string_color[4] = {0, -1, -1, -1};
int BasePop::def_string_shadow[4] = {-1, -1, -1, -1};
int BasePop::def_string_shadow_off_x[4] = {1, 1, 1, 1};
int BasePop::def_string_shadow_off_y[4] = {1, 1, 1, 1};
int BasePop::def_button_color[3] = {0, -1, -1};
int BasePop::def_button_shadow[3] = {-1, -1, -1};
int BasePop::def_button_shadow_off_x[3] = {1, 1, 1};
int BasePop::def_button_shadow_off_y[3] = {1, 1, 1};
unsigned char g_dlgButtonColor = 247;
int g_dlgButtonBevel[3] = {7, 248, 2};
char *CONTROL_LINES[22] = {"xs", "ys", "x ", "y ", "col ", "caption", "checkbox", "editbox", "listbox", "active",
    "text", "itemlist", "itemindex", "button", "extra", "wave", "vol", "pitch", "fade", "pan", "sprite", "comment"};


const char *GetEngineDialogFile()
{
    return "jackal";
}
