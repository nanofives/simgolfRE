
#ifndef IMAGEBUTTON_H
#define IMAGEBUTTON_H

#include "BaseButton.h"

struct ImageButton : public BaseButton {
    ImageButton();
    virtual ~ImageButton();

    virtual void on_redraw();
    GraphicWin *GetParent() { return parent; }
    void DrawTextLine(const char *s, Rect *rc)
    {
        if (s)
            RenderTextVerticallyCentered(s, rc, strlen(s));
    }
    virtual void on_mouse_move(int x, int y);
    virtual void on_mouse_leave(int x, int y);
    virtual void close();

    void SetImages(SpriteObject *idle, SpriteObject *hovered, SpriteObject *pressed)
    {
        m_images[0] = idle;
        m_images[1] = hovered;
        m_images[2] = pressed;
    }

    void SetFrames(BoxSpriteParams *idle, BoxSpriteParams *hover, BoxSpriteParams *pressed)
    {
        m_frames[0] = idle;
        m_frames[1] = hover;
        m_frames[2] = pressed;
    }
    void SetEdgeLeft(SpriteObject *idle, SpriteObject *hover, SpriteObject *pressed)
    {
        m_edgeLeft[0] = idle;
        m_edgeLeft[1] = hover;
        m_edgeLeft[2] = pressed;
    }
    void SetEdgeRight(SpriteObject *idle, SpriteObject *hover, SpriteObject *pressed)
    {
        m_edgeRight[0] = idle;
        m_edgeRight[1] = hover;
        m_edgeRight[2] = pressed;
    }
    void SetEdgeTop(SpriteObject *idle, SpriteObject *hover, SpriteObject *pressed)
    {
        m_edgeTop[0] = idle;
        m_edgeTop[1] = hover;
        m_edgeTop[2] = pressed;
    }
    void SetEdgeBottom(SpriteObject *idle, SpriteObject *hover, SpriteObject *pressed)
    {
        m_edgeBottom[0] = idle;
        m_edgeBottom[1] = hover;
        m_edgeBottom[2] = pressed;
    }
    void SetCornerTopLeft(SpriteObject *idle, SpriteObject *hover, SpriteObject *pressed)
    {
        m_cornerTopLeft[0] = idle;
        m_cornerTopLeft[1] = hover;
        m_cornerTopLeft[2] = pressed;
    }
    void SetCornerTopRight(SpriteObject *idle, SpriteObject *hover, SpriteObject *pressed)
    {
        m_cornerTopRight[0] = idle;
        m_cornerTopRight[1] = hover;
        m_cornerTopRight[2] = pressed;
    }
    void SetCornerBottomLeft(SpriteObject *idle, SpriteObject *hover, SpriteObject *pressed)
    {
        m_cornerBottomLeft[0] = idle;
        m_cornerBottomLeft[1] = hover;
        m_cornerBottomLeft[2] = pressed;
    }
    void SetCornerBottomRight(SpriteObject *idle, SpriteObject *hover, SpriteObject *pressed)
    {
        m_cornerBottomRight[0] = idle;
        m_cornerBottomRight[1] = hover;
        m_cornerBottomRight[2] = pressed;
    }
    void SetCentre(SpriteObject *idle, SpriteObject *hover, SpriteObject *pressed)
    {
        m_centre[0] = idle;
        m_centre[1] = hover;
        m_centre[2] = pressed;
    }


    static BoxSpriteParams *def_boxsprite_lone[3];
    static SpriteObject *def_spr_left[3];
    static SpriteObject *def_spr_right[3];
    static SpriteObject *def_spr_top[3];
    static SpriteObject *def_spr_bottom[3];
    static SpriteObject *def_spr_top_left[3];
    static SpriteObject *def_spr_top_right[3];
    static SpriteObject *def_spr_bottom_left[3];
    static SpriteObject *def_spr_bottom_right[3];
    static SpriteObject *def_spr_center[3];


    int m_tooltipDelay;

    int m_forcedState;

    int m_textColors[3];
    BoxSpriteParams *m_frames[3];


    SpriteObject *m_images[3];
    SpriteObject *m_mask;
    AssetSurface *m_backdrop;
    SpriteObject *m_edgeLeft[3];
    SpriteObject *m_edgeRight[3];
    SpriteObject *m_edgeTop[3];
    SpriteObject *m_edgeBottom[3];
    SpriteObject *m_cornerTopLeft[3];
    SpriteObject *m_cornerTopRight[3];
    SpriteObject *m_cornerBottomLeft[3];
    SpriteObject *m_cornerBottomRight[3];
    SpriteObject *m_centre[3];
};

#endif



#if !defined(IMAGEBUTTON_DTOR_DEFERRED) && !defined(IMAGEBUTTON_DTOR)
#define IMAGEBUTTON_DTOR

inline ImageButton::~ImageButton()
{
    close();
}
#endif
