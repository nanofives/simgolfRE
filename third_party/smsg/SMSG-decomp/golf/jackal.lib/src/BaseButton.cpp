#include "jgldefs.h"
#include "BaseButton.h"
#include "ButtonGroup.h"
#include "GameMain.h"
#include "EngineInit.h"
#include "StringUtil.h"
#include "JackalUtil.h"


BaseButton::BaseButton()
{
    m_pressOffsetX = g_buttonDefaults[17];
    m_pressOffsetY = g_buttonDefaults[18];
    m_reserved = 0;
    m_hover = 0;
    m_repeatDelay = -1;
    m_repeatPeriod = -1;
    m_id = 0;
    m_state.m_pressed = 0;
    m_text = 0;
    m_tooltip = 0;
    m_group = 0;
    m_disabled = 0;
    m_onClick = 0;
    m_onRepeat = 0;
    m_state.m_frameColor = g_buttonFrameColors[0];
    m_state.m_colors[0] = g_buttonDefaults[0];
    m_state.m_colors[1] = g_buttonDefaults[1];
    m_state.m_colors[2] = g_buttonDefaults[2];
    m_state.m_colors[3] = g_buttonDefaults[3];
    m_state.m_frameColorAlt = g_buttonFrameColors[1];
    m_pressTime = 0;
    m_state.m_minPressMs = g_buttonDefaults[4];
}


BaseButton::~BaseButton()
{
    close();
}

void BaseButton::close()
{
    GraphicWin::close();
    m_pressOffsetX = g_buttonDefaults[17];
    m_pressOffsetY = g_buttonDefaults[18];
    m_reserved = 0;
    m_hover = 0;
    m_state.m_pressed = 0;
    m_id = 0;
    m_repeatDelay = -1;
    m_repeatPeriod = -1;
    m_disabled = 0;
    m_onClick = 0;
    m_onRepeat = 0;
    m_state.m_frameColor = g_buttonFrameColors[0];
    m_state.m_colors[0] = g_buttonDefaults[0];
    m_state.m_colors[1] = g_buttonDefaults[1];
    m_state.m_colors[2] = g_buttonDefaults[2];
    m_state.m_colors[3] = g_buttonDefaults[3];
    m_state.m_frameColorAlt = g_buttonFrameColors[1];
    m_pressTime = 0;
    m_state.m_minPressMs = g_buttonDefaults[4];
    if (m_text) {
        FreeIfNotNull(m_text);
        m_text = 0;
    }
    if (m_tooltip) {
        FreeIfNotNull(m_tooltip);
        m_tooltip = 0;
    }
    m_group = 0;
}

int BaseButton::init(char *text, int id, int x, int y, int w, int h, WidgetBase *parent, int flag)
{
    close();
    if (!parent)
        return 3;
    if (text) {
        m_text = (char *)AllocateBytes(strlen(text) + 1);
        if (!m_text)
            return 4;
        ClearString(m_text);
        AppendString(m_text, text);
    }
    int rc = GraphicWin::init(x, y, w, h, 0, 0x1000220 | (flag ? 0x8000 : 0), parent, 0, 0);
    if (rc)
        return rc;
    m_id = id;
    AssetSurface::set_text_color(g_buttonDefaults[5], g_buttonDefaults[8], g_buttonDefaults[11], g_buttonDefaults[14]);
    AssetSurface::set_text_color2(g_buttonDefaults[6], g_buttonDefaults[9], g_buttonDefaults[12], g_buttonDefaults[15]);
    AssetSurface::set_text_color3(g_buttonDefaults[7], g_buttonDefaults[10], g_buttonDefaults[13],
        g_buttonDefaults[16]);
    SetFontsForStyles(g_buttonFonts[0], g_buttonFonts[1], g_buttonFonts[2], 0);
    show(0);
    return 0;
}


void BaseButton::set_text_color(int color, int shadowColor, int shadowDx, int shadowDy)
{
    if (parent) {
        sync_to_palette(g_widgetPalette);
        AssetSurface::set_text_color(color, shadowColor, shadowDx, shadowDy);
    }
}

void BaseButton::set_text_color2(int color, int shadowColor, int shadowDx, int shadowDy)
{
    if (parent) {
        sync_to_palette(g_widgetPalette);
        AssetSurface::set_text_color2(color, shadowColor, shadowDx, shadowDy);
    }
}

void BaseButton::set_text_color3(int color, int shadowColor, int shadowDx, int shadowDy)
{
    if (parent) {
        sync_to_palette(g_widgetPalette);
        AssetSurface::set_text_color3(color, shadowColor, shadowDx, shadowDy);
    }
}


int SetButtonDefaults(Font *font, Font *braceFont, Font *bracketFont)
{
    if (!font)
        return 3;
    if (font->m_owner)
        g_buttonFonts[0] = font;
    g_buttonFonts[1] = braceFont;
    g_buttonFonts[2] = bracketFont;
    return 0;
}

int BaseButton::set_name(const char *text)
{
    if (m_text) {
        free(m_text);
        m_text = 0;
    }
    if (text) {
        m_text = (char *)malloc(strlen(text) + 1);
        if (!m_text)
            return 4;
    }
    *m_text = 0;
    strcat(m_text, text);
    return 0;
}

int BaseButton::set_bubble_text(const char *text)
{
    if (m_tooltip) {
        FreeIfNotNull(m_tooltip);
        m_tooltip = 0;
    }
    if (!text)
        return 0;
    m_tooltip = (char *)AllocateBytes(strlen(text) + 1);
    if (!m_tooltip)
        return 4;
    ClearString(m_tooltip);
    AppendString(m_tooltip, text);
    return 0;
}



void BaseButton::on_left_down(int x, int y)
{
    if (g_dragWidget == 0) {
        g_dragWidget = this;
        g_dragTargetWidget = 0;
    }
    WidgetBase::clear_bubble_text();
    if (m_state.m_pressed == 0) {
        m_state.m_pressed = 1;
        if (parent->m_onButtonPressed)
            parent->m_onButtonPressed->play();
        if (m_group)
            parent->on_button_down(m_id);
        else
            parent->on_button_down(m_id);
        redraw();
        if (m_repeatDelay != -1)
            m_holdTimer.SetCallbackAndPulse((void *)timer_callback_daemon, 1, this, m_repeatDelay, 5);
    } else {
        m_pressTime = GetMilliseconds();
        g_renderingWidget = parent;
        if (m_disabled == 0) {
            if (parent->m_onButtonDown)
                parent->m_onButtonDown->play();
            if (m_group) {
                if (parent->on_button_pressed(m_id) && m_repeatDelay != -1)
                    m_holdTimer.SetCallbackAndPulse((void *)timer_callback_daemon, 1, this, m_repeatDelay, 5);
            } else {
                if (parent->on_button_pressed(m_id) && m_repeatDelay != -1)
                    m_holdTimer.SetCallbackAndPulse((void *)timer_callback_daemon, 1, this, m_repeatDelay, 5);
            }
        }
        if (m_onRepeat)
            m_onRepeat(m_id);
    }
}

void BaseButton::on_left_up(int x, int y)
{
    if (g_dragWidget == this) {
        g_dragWidget = 0;
        on_lose_mouse_capture();
    }
    if (g_dragTargetWidget == this)
        g_dragTargetWidget = 0;
    if (parent) {
        m_holdTimer.stop();
        unsigned long held = GetMilliseconds() - m_pressTime;
        if (held < m_state.m_minPressMs)
            Sleep(m_state.m_minPressMs - held);
        if (m_state.m_pressed == 1) {
            m_state.m_pressed = 0;
            if (parent->m_onButtonReleased)
                parent->m_onButtonReleased->play();
            if (m_group)
                parent->on_button_up(m_id);
            else
                parent->on_button_up(m_id);
            redraw();
        }
    }
}


int BaseButton::on_key_click(int ch, int vk)
{
    return 0;
}

void BaseButton::on_mouse_leave(int x, int y)
{
    if ((g_mouseDownWidget ? g_mouseDownWidget : g_mouseDownWidgetAlt) == this && g_leftButtonPress == 1)
        on_mouse_move(x, y);
}

void BaseButton::on_mouse_move(int x, int y)
{
    Rect r;
    GetSurfaceBounds(&r);
    r.Offset(-r.left, -r.top);
    if ((g_mouseDownWidget ? g_mouseDownWidget : g_mouseDownWidgetAlt) == this && g_leftButtonPress != 0) {
        if (!in_box(x, y, &r) && m_state.m_pressed == 1) {
            m_holdTimer.stop();
            m_state.m_pressed = 0;
            if (parent->m_onButtonReleased)
                parent->m_onButtonReleased->play();
            if (m_group)
                parent->on_button_up(m_id);
            else
                parent->on_button_up(m_id);
            redraw();
        } else if (in_box(x, y, &r)) {
            if (m_state.m_pressed) {
                if (get_lbutton_state())
                    on_left_down(x, y);
            } else {
                if (m_repeatDelay != -1)
                    m_holdTimer.SetCallbackAndStart((void *)timer_callback_daemon, 1, this, m_repeatDelay, 5);
                m_state.m_pressed = 1;
                if (parent->m_onButtonPressed)
                    parent->m_onButtonPressed->play();
                if (m_group)
                    parent->on_button_down(m_id);
                else
                    parent->on_button_down(m_id);
                redraw();
            }
        }
    }
}


void BaseButton::on_right_click(int x, int y)
{
    if (m_tooltip)
        timer_callback(0);
}

void BaseButton::on_left_click(int x, int y)
{
    if (g_dragWidget == this) {
        g_dragWidget = 0;
        on_lose_mouse_capture();
    }
    if (g_dragTargetWidget == this)
        g_dragTargetWidget = 0;
    g_renderingWidget = parent;
    if (m_disabled == 0) {
        int id = m_id;
        if (id == -1) {
            if (g_buttonIdMinus1Hook)
                g_buttonIdMinus1Hook();
        } else if (id == -2) {
            if (g_buttonIdMinus2Hook)
                g_buttonIdMinus2Hook();
        }
        if (parent->m_onButtonUp)
            parent->m_onButtonUp->play();
        if (m_group) {
            if (m_group->button_click(m_id))
                parent->on_button_clicked(m_id);
        } else {
            parent->on_button_clicked(m_id);
        }
    }
    if (m_onClick)
        m_onClick(m_id);
    if (m_state.m_pressed == 1) {
        m_state.m_pressed = 0;
        redraw();
    }
}


void BaseButton::on_dialog_focus(int on)
{
    redraw();
}


void __cdecl BaseButton::timer_callback_daemon(int id, void *ctx)
{
    if (ctx)
        ((BaseButton *)ctx)->timer_callback(id);
}

void BaseButton::timer_callback(int id)
{
    Rect r;
    int x;
    int y;
    if (id == 0) {
        m_tooltipTimer.stop();
        if (is_visible() == 0)
            return;
        if (m_tooltip == 0)
            return;
        x = 0;
        y = 0;
        r = m_local;
        client_to_screen(&x, &y);
        r.Offset(x - r.left, y - r.top);
        WidgetBase::set_bubble_text(m_tooltip, &r);
    } else if (id == 1) {
        int period = m_repeatPeriod;
        if (period <= 0)
            period = m_repeatDelay;
        m_holdTimer.SetCallbackAndStart((void *)timer_callback_daemon, 2, this, period, 5);
    } else {
        on_left_down(0, 0);
    }
}


void BaseButton::set(int selected)
{
    if (selected != m_selected) {
        m_selected = selected;
        redraw();
        if (parent)
            parent->on_button_toggled(m_id, selected);
    }
}

void BaseButton::hide()
{
    if (m_group == 0)
        m_state.m_pressed = 0;
    WidgetBase::hide();
}



int g_buttonFrameColors[2];
Font *g_buttonFonts[4];



int g_buttonDefaults[19] = {247, 7, 248, 2, 125, 0, -1, -1, -1, -1, -1, 1, 1, 1, 1, 1, 1, 2, 2};
