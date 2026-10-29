#ifndef BASEPOP_H
#define BASEPOP_H

#include "WidgetBase.h"
#include "Heap.h"
#include "Sprite.h"
#include "ImageButton.h"
#include "Dialogs.h"
#include "Spot.h"

struct Font;

typedef void(__cdecl *DialogIconDrawFn)(SpriteObject *icon, WidgetBase *target, int item, int x, int y);

struct DialogSound {
    char name[0x104];
    int wave;
    long vol;
    long fade;
    int pitch;
    int pan;
};

enum DialogKeyword {
    DK_XS,
    DK_YS,
    DK_X,
    DK_Y,
    DK_COL,
    DK_CAPTION,
    DK_CHECKBOX,
    DK_EDITBOX,
    DK_LISTBOX,
    DK_ACTIVE,
    DK_TEXT,
    DK_ITEMLIST,
    DK_ITEMINDEX,
    DK_BUTTON,
    DK_EXTRA,
    DK_WAVE,
    DK_VOL,
    DK_PITCH,
    DK_FADE,
    DK_PAN,
    DK_SPRITE,
    DK_COMMENT
};

struct DialogTextFonts {
    Font *font;
    Font *braceFont, *bracketFont;
    Font *linkFont;
};
struct DialogButtonFonts {
    Font *font;
    Font *braceFont, *bracketFont;
};

struct BasePop : public GraphicWin {
    BasePop();
    int CaptionMargin() const { return m_captionMargin; }
    int FrameWidth() const;
    int FrameHeight() const;
    char *Title() const;
    virtual ~BasePop();

    virtual void on_redraw();
    virtual void on_left_click(int x, int y);
    virtual void on_left_down(int x, int y);
    virtual int on_key_click(int ch, int vk);
    virtual int on_key_down(int vk);
    virtual int on_key_up(int vk);
    virtual void on_button_clicked(int id);
    virtual int on_modal();
    virtual void on_redraw_nc(int which);
    virtual int on_nc_hittest(int x, int y);


    virtual void on_release_modal() {}
    virtual void set_sprites(SpriteObject *normal, SpriteObject *selected, SpriteObject *hot);
    virtual void set_highlight_color(int color);
    virtual void set_selected_color(int color);
    virtual int calculate_dimensions();
    virtual void on_post_calculate_dimensions() {}
    virtual void on_pre_window_init() {}
    virtual void on_post_window_init() {}
    virtual void on_adjust_button_width() {}
    virtual void on_adjust_button_placement(int *x, int *y, int *pressed) {}
    virtual int on_exec() { return 1; }
    virtual int on_draw_background() { return 0; }
    virtual void on_close() {}
    virtual void on_init() {}
    virtual char handleGameSpecificControlLines(char *line);

    void close();
    int init(unsigned flags, unsigned arenaBytes);
    int string(const char *text, char atFront);
    int button(const char *text);
    int title(const char *title);
    void set_height(int h);
    void set_width(int w);
    void set_loc(int x, int y);
    void write_check(int mask);
    int read_check();
    int start(const char *file, const char *section, int tag, const char *firstText, unsigned flags, GraphicWin *owner);
    int start(const char *file, const char *section, int tag, const char *firstText, unsigned flags);
    int exec(int layoutOnly, int (*pfnIdle)());
    int exec() { return exec(0, 0); }

    void CaptureSelection()
    {
        if (m_list.Dialog::m_items.m_count) {
            if (m_list.m_type == MW_CHECKBOX)
                m_result = m_list.GetCheckMask();
            else
                m_result = m_list.get_selected_id();
        }
    }
    int TakeButtonCode(int id)
    {
        if (id != -2) {
            if (id == -1)
                m_modalCode = 0;
            else
                m_modalCode = id;
            return 1;
        }
        if (m_dialogFlags & 0x40) {
            m_result = -1;
        } else {
            if (m_list.m_type != -1)
                return 0;
            m_result = 0;
        }
        m_modalCode = 0;
        return 1;
    }

    char m_keepStyle;
    const char *m_loadFirstText;
    const char *m_loadFile;
    const char *m_loadSection;
    int m_loadTag;
    unsigned m_loadFlags;
    GraphicWin *m_loadOwner;
    WidgetBase *m_host;
    int m_hostX;
    int m_hostY;
    int m_hostW;
    int m_hostH;
    Heap m_arena;
    GraphicWin *m_targetWin;
    int m_result;
    int m_tag;
    char *m_title;
    int m_5cc[4];
    ImageButton m_okButton;
    ImageButton m_cancelButton;
    int m_choiceCount;
    ImageButton *m_owned[8];
    SpriteObject m_sprite;
    SpriteObject *m_captionSprite;
    SpriteObject *m_icon;
    AssetSurface *m_backTile;
    SpriteObject *m_backSprite;
    SpriteObject *m_centreImage;
    StringList m_choices;
    StringList m_lines;
    int m_insetLeft;
    int m_insetRight;
    int m_insetTop;
    int m_insetBottom;
    int m_margin;
    int m_spacing;
    int m_textIndent;
    int m_listX;
    Dialogs m_list;
    DialogSound m_sounds[2];
    void (*m_soundHook)(DialogSound *sound, int opening);
    Spot m_hotspots;
    int m_frameStyle;
    unsigned m_dialogFlags;
    int m_extra;
    int m_buttonCount;
    int m_originX;
    int m_originY;
    int m_frameWidth;
    int m_frameHeight;
    int m_listHeight;
    int f1f88;
    int m_buttonWidth;
    int m_buttonHeight;
    int m_textHeight;
    int m_iconWidth;
    int m_iconHeight;
    int m_layoutMargin;
    int m_layoutSpacing;
    int m_layoutTop;
    int m_layoutBottom;
    int m_layoutLeft;
    int m_layoutRight;
    int m_posX;
    int m_posY;
    int m_modalCode;
    int m_drawParams[3];
    DialogTextFonts m_textFonts;
    int m_textColor[4];
    int m_textShadowColor[4];
    int m_textShadowDx[4];
    int m_textShadowDy[4];
    void(__cdecl *m_iconLeftClick)(int id);
    void(__cdecl *m_iconLeftDoubleClick)(int id);
    DialogIconDrawFn m_iconDraw;
    DialogButtonFonts m_buttonFonts;
    unsigned char m_buttonTextColor[4];
    int m_buttonShadowColor[3];
    int m_buttonShadowDx[3];
    int m_buttonShadowDy[3];
    unsigned char m_buttonColor;
    int m_buttonBevel[3];
    int m_buttonFrameColor;
    int m_buttonFrameColorAlt;
    SpriteObject *m_okButtonImages[3];
    SpriteObject *m_cancelButtonImages[3];
    BoxSpriteParams *m_skinFrames[3];
    SpriteObject *m_skinEdgeLeft[3];
    SpriteObject *m_skinEdgeRight[3];
    SpriteObject *m_skinEdgeTop[3];
    SpriteObject *m_skinEdgeBottom[3];
    SpriteObject *m_skinCornerTL[3];
    SpriteObject *m_skinCornerTR[3];
    SpriteObject *m_skinCornerBL[3];
    SpriteObject *m_skinCornerBR[3];
    SpriteObject *m_skinCentre[3];
    char *m_okCaption;
    char *m_cancelCaption;
    WidgetBase *m_root;
    static int init_class();
    static void close_class();
    static BasePop *basepop_alloc();
    static int set_def_file(const char *file);
    static void set_def_external_window(WidgetBase *host, int x, int y, int w, int h);
    static void (*def_popwave_func)(DialogSound *sound, int opening);
    static int fallout_flag;
    static int modal_count;
    static WidgetBase *def_external_parent;
    static int def_external_x;
    static int def_external_y;
    static int def_external_xs;
    static int def_external_ys;
    static BoxSpriteParams *def_pb_boxsprite[3];
    static SpriteObject *def_pb_spr_left[3];
    static SpriteObject *def_pb_spr_right[3];
    static SpriteObject *def_pb_spr_top[3];
    static SpriteObject *def_pb_spr_bottom[3];
    static SpriteObject *def_pb_spr_top_left[3];
    static SpriteObject *def_pb_spr_top_right[3];
    static SpriteObject *def_pb_spr_bottom_left[3];
    static SpriteObject *def_pb_spr_bottom_right[3];
    static SpriteObject *def_pb_spr_center[3];
    static SpriteObject *def_pb_ok_lone[3];
    static SpriteObject *def_pb_cancel_lone[3];
    static char *def_ok_text;
    static char *def_cancel_text;
    static int def_left_padding;
    static int def_right_padding;
    static int def_bottom_padding;
    static int def_top_padding;
    static Font *def_string_font[4];
    static Font *def_button_font[3];
    static int def_button_focus_box_color;
    static AssetSurface *def_back_tile_b;
    static SpriteObject *def_back_tile;
    static SpriteObject *def_center_sprite;
    static int last_x;
    static int last_y;
    static int def_window_style;
    static int fallout_result;
    static int def_margin_padding;
    static int def_object_padding;
    static int def_string_color[4];
    static int def_string_shadow[4];
    static int def_string_shadow_off_x[4];
    static int def_string_shadow_off_y[4];
    static int def_button_color[3];
    static int def_button_shadow[3];
    static int def_button_shadow_off_x[3];
    static int def_button_shadow_off_y[3];
    static char def_file[80];
};

extern DialogSound g_dlgOpenSound;
extern DialogSound g_dlgCloseSound;
extern int redraw_y;
extern int redraw_x;
extern BasePop *current_sprite_draw_popup;
extern const char *g_dlgOkTooltip;
extern const char *g_dlgCancelTooltip;
extern int g_dlgButtonFrameColor;
extern WidgetBase *g_dialogRoot;
extern int g_dlgDrawParams[3];
extern unsigned char g_dlgButtonColor;
extern int g_dlgButtonBevel[3];
extern char *CONTROL_LINES[22];

int decipherControlLine(char **s);
int SetDefaultOkCaption(const char *s);
int SetDefaultCancelCaption(const char *s);
void OnListItemActivated(int);
int Dialog_SetGlobalTextFonts(Font *font, Font *braceFont, Font *bracketFont, Font *linkFont);
void Dialog_SetGlobalTextColors(int color, int shadowColor, int shadowDx, int shadowDy);
int Dialog_SetGlobalButtonFonts(Font *font, Font *braceFont, Font *bracketFont);
void Dialog_SetGlobalButtonColors(int color, int shadowColor, int shadowDx, int shadowDy);
int parse_says(int slot, const char *text, int gender, int plural);
extern char parse_strings[10][0x100];
int parse_string(char *src, char *dst);
struct ComboBox;
struct DropDownSlot {
    ComboBox *list;
    int f4;
};
extern DropDownSlot g_parse_cb_dropdowns[];

#endif

#if !defined(BASEPOP_DTOR_DEFERRED) && !defined(BASEPOP_DTOR)
#define BASEPOP_DTOR

inline BasePop::~BasePop()
{
    close();
}
#endif
