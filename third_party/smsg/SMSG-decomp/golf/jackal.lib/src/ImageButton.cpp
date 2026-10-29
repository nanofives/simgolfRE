#include "jgldefs.h"
#include "ImageButton.h"
#include "StringUtil.h"
#include "JackalUtil.h"


BoxSpriteParams *ImageButton::def_boxsprite_lone[3] = {0};
SpriteObject *ImageButton::def_spr_left[3] = {0};
SpriteObject *ImageButton::def_spr_right[3] = {0};
SpriteObject *ImageButton::def_spr_top[3] = {0};
SpriteObject *ImageButton::def_spr_bottom[3] = {0};
SpriteObject *ImageButton::def_spr_top_left[3] = {0};
SpriteObject *ImageButton::def_spr_top_right[3] = {0};
SpriteObject *ImageButton::def_spr_bottom_left[3] = {0};
SpriteObject *ImageButton::def_spr_bottom_right[3] = {0};
SpriteObject *ImageButton::def_spr_center[3] = {0};

ImageButton::ImageButton()
{
    int i;
    m_tooltipDelay = 1000;
    m_selected = 0;
    m_reserved = 0;
    m_forcedState = -1;
    m_mask = 0;
    m_backdrop = 0;
    for (i = 0; i < 3; i++) {
        m_frames[i] = def_boxsprite_lone[i];
        m_textColors[i] = -1;
        m_edgeLeft[i] = def_spr_left[i];
        m_edgeRight[i] = def_spr_right[i];
        m_edgeTop[i] = def_spr_top[i];
        m_edgeBottom[i] = def_spr_bottom[i];
        m_cornerTopLeft[i] = def_spr_top_left[i];
        m_cornerTopRight[i] = def_spr_top_right[i];
        m_cornerBottomLeft[i] = def_spr_bottom_left[i];
        m_cornerBottomRight[i] = def_spr_bottom_right[i];
        m_centre[i] = def_spr_center[i];
        m_images[i] = 0;
    }
}

void ImageButton::close()
{
    int i;
    m_tooltipDelay = 1000;
    m_tooltipTimer.close();
    m_selected = 0;
    m_reserved = 0;
    m_forcedState = -1;
    BaseButton::close();
    for (i = 0; i < 3; i++) {
        m_frames[i] = def_boxsprite_lone[i];
        m_textColors[i] = -1;
        m_edgeLeft[i] = def_spr_left[i];
        m_edgeRight[i] = def_spr_right[i];
        m_edgeTop[i] = def_spr_top[i];
        m_edgeBottom[i] = def_spr_bottom[i];
        m_cornerTopLeft[i] = def_spr_top_left[i];
        m_cornerTopRight[i] = def_spr_top_right[i];
        m_cornerBottomLeft[i] = def_spr_bottom_left[i];
        m_cornerBottomRight[i] = def_spr_bottom_right[i];
        m_centre[i] = def_spr_center[i];
        m_images[i] = 0;
    }
}



void ImageButton::on_redraw()
{
    Rect rc;
    Rect *pr = &rc;
    int state;
    int pieces;
    int cw;
    g_renderingWidget = GetParent();
    if (GetParent() == 0)
        return;
    if (graphsys)
        graphsys->set_sprite_scale(1, 1, 1);
    sync_to_palette(g_widgetPalette);
    GetSurfaceBounds(pr);
    int key = GetTransparentColor();
    EraseWholeBackground(key);
    {
        int a = m_forcedState;
        state = a;
        if (a < 0 || a >= 3) {
            if (m_state.m_pressed != 0 || m_selected != 0)
                state = 2;
            else
                state = (m_hover != 0);
        }
    }
    if (m_images[state]) {
        if (m_mask == 0 || m_backdrop == 0) {
            AssetSurface *dst = this;
            int h = GetSurfaceHeight();
            int dy = m_images[state]->m_height;
            int w = GetSurfaceWidth();
            m_images[state]->BlitScaled(dst, (w - m_images[state]->m_width) / 2, (h - dy) / 2, 0);
        } else {
            AssetSurface tmp;
            int tmpHeight = GetSurfaceHeight();
            int tmpWidth = GetSurfaceWidth();
            tmp.Create(tmpWidth, tmpHeight, 0x10, 1, 0, 0);
            int ch = GetSurfaceHeight();
            cw = GetSurfaceWidth();
            int sh = GetSurfaceHeight();
            int sw = GetSurfaceWidth();
            m_backdrop->BlitSurfaceRegion(&tmp, m_local.left, m_local.top, sw, sh, 0, 0, cw, ch);
            AssetSurface *dst = this;
            int h = GetSurfaceHeight();
            int dy = m_images[state]->m_height;
            int w = GetSurfaceWidth();
            m_images[state]->draw_antialiased(m_mask, &tmp, dst, (w - m_images[state]->m_width) / 2, (h - dy) / 2, 0);
        }
    } else if (m_frames[state]) {
        box_sprite(pr, m_frames[state]);
    } else if (m_cornerTopLeft[state]) {
        pieces = 1;
        AssetSurface *dst0 = this;
        m_cornerTopLeft[state]->BlitScaled(dst0, 0, 0, 0);
        if (m_cornerTopRight[state]) {
            pieces = 2;
            AssetSurface *dst = this;
            int w = GetSurfaceWidth();
            m_cornerTopRight[state]->BlitScaled(dst, w - m_cornerTopRight[state]->m_width, 0, 0);
        }
        if (m_cornerBottomLeft[state]) {
            pieces++;
            AssetSurface *dst = this;
            int h = GetSurfaceHeight();
            m_cornerBottomLeft[state]->BlitScaled(dst, 0, h - m_cornerBottomLeft[state]->m_height, 0);
        }
        if (m_cornerBottomRight[state]) {
            pieces++;
            AssetSurface *dst = this;
            int h = GetSurfaceHeight();
            int dy = m_cornerBottomRight[state]->m_height;
            int w = GetSurfaceWidth();
            m_cornerBottomRight[state]->BlitScaled(dst, w - m_cornerBottomRight[state]->m_width, h - dy, 0);
        }
        if (m_edgeLeft[state] && m_cornerTopLeft[state] && m_cornerBottomLeft[state]) {
            pieces++;
            int h = GetSurfaceHeight();
            int t = m_cornerTopLeft[state]->m_height;
            TileSprite(m_edgeLeft[state], 0, 0, 0, t, m_edgeLeft[state]->m_width,
                (h - m_cornerBottomLeft[state]->m_height) - t);
        }
        if (m_edgeRight[state] && m_cornerTopRight[state] && m_cornerBottomRight[state]) {
            pieces++;
            int h = GetSurfaceHeight();
            int a = m_cornerTopRight[state]->m_height;
            int b = m_cornerBottomRight[state]->m_height;
            int c = m_edgeRight[state]->m_width;
            int w = GetSurfaceWidth();
            TileSprite(m_edgeRight[state], 0, 0, w - m_edgeRight[state]->m_width, a, c, (h - b) - a);
        }
        if (m_edgeTop[state] && m_cornerTopLeft[state] && m_cornerTopRight[state]) {
            pieces++;
            int t = m_edgeTop[state]->m_height;
            int w = GetSurfaceWidth();
            int c = m_cornerTopLeft[state]->m_width;
            TileSprite(m_edgeTop[state], 0, 0, c, 0, (w - m_cornerTopRight[state]->m_width) - c, t);
        }
        if (m_edgeBottom[state] && m_cornerBottomLeft[state] && m_cornerBottomRight[state]) {
            pieces++;
            int t = m_edgeBottom[state]->m_height;
            int w = GetSurfaceWidth();
            int leftW = m_cornerBottomLeft[state]->m_width;
            cw = m_cornerBottomRight[state]->m_width;
            int h = GetSurfaceHeight();
            TileSprite(m_edgeBottom[state], 0, 0, m_cornerBottomLeft[state]->m_width, h - m_edgeBottom[state]->m_height,
                (w - leftW) - cw, t);
        }
        if (m_centre[state] && pieces == 8) {
            int h = GetSurfaceHeight();
            int a = m_edgeTop[state]->m_height;
            int b = m_edgeBottom[state]->m_height;
            int w = GetSurfaceWidth();
            int c = m_edgeRight[state]->m_width;
            TileSprite(m_centre[state], 0, 0, c, m_edgeTop[state]->m_height, (w - m_edgeLeft[state]->m_width) - c,
                (h - b) - a);
        }
    } else {
        int n = 0;
        int oc = m_state.m_frameColor;
        if (oc != -1) {
            DrawBevelFrameInRect(pr, oc, oc);
            pr->left++;
            pr->right--;
            pr->top++;
            pr->bottom--;
            if ((~GetParent()->m_flags & 0x1000) != 0 && is_dialog_focus() != 0) {
                DrawBevelFrameInRect(pr, m_state.m_frameColor, m_state.m_frameColor);
                pr->left++;
                pr->right--;
                pr->top++;
                pr->bottom--;
                n = 1;
            }
        }
        if (m_state.m_pressed == 0) {
            while (n < m_state.m_colors[3]) {
                DrawBevelFrameInRect(pr, m_state.m_colors[1], m_state.m_colors[2]);
                pr->left++;
                pr->right--;
                pr->top++;
                pr->bottom--;
                n++;
            }
        } else {
            while (n < m_state.m_colors[3]) {
                DrawBevelFrameInRect(pr, m_state.m_colors[2], m_state.m_colors[1]);
                pr->left++;
                pr->right--;
                pr->top++;
                pr->bottom--;
                n++;
            }
            int d = m_state.m_colors[3];
            pr->left += d;
            pr->right += d;
            pr->top += d;
            pr->bottom += d;
        }
    }
    if (m_text == 0)
        return;
    int sv = m_textColors[state];
    if (sv >= 0)
        set_text_color(sv, -1, 1, 1);
    GraphicWin *p = GetParent();
    if (p == 0)
        return;
    if (p->on_button_text_draw(this, m_text, m_id) != 0)
        return;
    if (state == 2) {
        pr->Offset(m_pressOffsetX, m_pressOffsetY);
    }
    int th = wrap_height(m_text, RectWidth(pr) - 6, 0, 0);
    th -= text_line_height();
    th += text_height();
    int y = (RectHeight(pr) - th) / 2 + pr->top;
    int rw;
    if (y < 0 || (m_lineCount < 2 && ((rw = RectWidth(pr)), text_width(m_text) > rw - 6))) {
        pr->left += 3;
        pr->right -= 3;
        pr->right -= text_width("...");
        set_clip(pr);
        DrawTextLine(m_text, pr);
        ResetSurfaceClip();
        int w = RectWidth(pr);
        pr->left += w;
        pr->right += w;
        DrawTextLine("...", pr);
    } else {
        wrap_cent(m_text, pr->left + 3, y, RectWidth(pr) - 6);
    }
}

void ImageButton::on_mouse_move(int x, int y)
{
    if ((g_mouseDownWidget ? g_mouseDownWidget : g_mouseDownWidgetAlt) == this) {
        BaseButton::on_mouse_move(x, y);
        return;
    }
    Rect r;
    if (m_surface)
        m_surface->SURF_CopyBoundsRect(&r);
    r.Offset(-r.left, -r.top);
    if (in_box(x, y, &r) == 0) {
        if (m_hover == 1) {
            set_cursor(0x7f00);
            m_tooltipTimer.stop();
            if (m_tooltip)
                WidgetBase::clear_bubble_text();
            m_hover = 0;
            if (parent)
                parent->on_button_passover(m_id, 0);
            redraw();
        }
    } else {
        if (m_hover == 0) {
            if (g_buttonHoverHook)
                g_buttonHoverHook();
            m_hover = 1;
            if (parent->m_onHoverChanged)
                parent->m_onHoverChanged->play();
            if (parent)
                parent->on_button_passover(m_id, 1);
            if (m_tooltip) {
                if (m_tooltipDelay) {
                    m_tooltipTimer.SetCallback(timer_callback_daemon, 0, this, m_tooltipDelay, 5);
                    m_tooltipTimer.pulse();
                } else {
                    timer_callback(0);
                }
            }
            redraw();
        }
    }
}

void ImageButton::on_mouse_leave(int x, int y)
{
    if ((g_mouseDownWidget ? g_mouseDownWidget : g_mouseDownWidgetAlt) == this && g_leftButtonPress == 1) {
        on_mouse_move(x, y);
        return;
    }
    if (m_hover == 1) {
        m_tooltipTimer.stop();
        if (m_tooltip)
            WidgetBase::clear_bubble_text();
        m_hover = 0;
        if (parent)
            parent->on_button_passover(m_id, 0);
        redraw();
    }
}

int g_imageButtonSpare = 0;
