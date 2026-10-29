#include "jgldefs.h"
#include "jglbuffer.h"
#include "jgltypes.h"
#include "gssprite.h"
#include <stdlib.h>
#include <crtdbg.h>

#include "jglglobals.h"
#include "jglblitadv.h"

static float   scale_factor_x;
static float   scale_factor_y;
static int     sprite_pitch;
static int     buffer_pitch;
static int     buffer_draw_width;
static int     buffer_draw_height;
static int     sprite_advance;
static int     buffer_advance;
static int     scale_x_inc;
static int     scale_y_inc;
static int     scale_x;
static int     scale_y;
static ushort *_table = 0;
static uchar  *global_trans_table;
static int     s_lostWord;
static int     s_lostValue;
static int     s_lostIntWord;

const char g_fileJglSprite8_16c[] = "C:\\JackDev\\jglsprite_8_16c.cpp";

#define JGL_RPTF0(rptno, msg) \
        do { if ((1 == _CrtDbgReport(rptno, g_fileJglSprite8_16c, __LINE__, NULL, \
                JGL_MSG_FMT, msg))) \
                _CrtDbgBreak(); } while (0)

static int     z_buffer_pitch;
static int     z_buffer_advance;
static ushort  global_mono_color;
static ushort *global_shadow_table;

int JGLSprite::draw_8_16c(JGLBuffer *dst, int x, int y, ushort *alpha)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    JGLPalette *pal;
    int         i;
    int         j;
    int         xout;
    uchar       c;
    Run        *runs;
    int         dx;
    int         rightClip;
    int         rightSpace;
    int         count;
    unsigned    sy;
    int         t;

    if (this->m_pixels == 0 || dst == 0)
        return 3;

    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0)
        return 16;

    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    this->m_colorKey = (uchar)this->m_colorKey;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    global_trans_table = (uchar *)alpha;

    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        return 1;
    }

    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {

        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (jgl_IntersectRect(&r, &r, dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }

        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
                dx = r.left - saved.left;
                rightClip = saved.right - r.right;
                runs = this->m_runs + (r.top - saved.top);



                for (i = 0; i < buffer_draw_height; i++) {
                    rightSpace = this->m_width - runs[i].start - runs[i].len;
                    xout = (runs[i].start - dx) < 0 ? 0 : (runs[i].start - dx);
                    srcp = this->m_pixels + runs[i].offset + ((dx - runs[i].start) < 0 ? 0 : (dx - runs[i].start));
                    count = runs[i].len - ((dx - runs[i].start) < 0 ? 0 : (dx - runs[i].start))
                            - ((rightClip - rightSpace) < 0 ? 0 : (rightClip - rightSpace));
                    for (j = 0; j < count; j++) {
                        c = *srcp;
                        if (c < 0xfe)
                            dstp[xout] = ((ushort *)global_trans_table)[_table[c]];
                        xout++;
                        srcp++;
                    }
                    dstp += buffer_pitch;
                }
            } else {



                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        } else {
            if (g_scaleY >= 0) {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        }

    } else {

        jgl::SetRect(&r, x, y, (int)(this->m_width * scale_factor_x), (int)(this->m_height * scale_factor_y));
        saved = r;
        if (jgl_IntersectRect(&r, &r, dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }

        dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
        buffer_pitch = dst->get_pitch();
        buffer_draw_width = r.right - r.left;
        buffer_draw_height = r.bottom - r.top;
        buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
        scale_x_inc = (g_scaleDenom << 16) / abs(g_scaleX);
        scale_y_inc = (g_scaleDenom << 16) / abs(g_scaleY);

        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                dx = r.left - saved.left;
                scale_y = scale_y_inc * (r.top - saved.top);
                runs = this->m_runs;

                for (i = 0; i < buffer_draw_height; i++) {
                    sy = (unsigned)scale_y >> 16;
                    if (runs[sy].len == 0)
                        continue;
                    scale_x = dx * scale_x_inc;
                    srcp = this->m_pixels + runs[sy].offset;
                    for (j = 0; j < buffer_draw_width; j++) {
                        t = ((unsigned)scale_x >> 16) - runs[sy].start;
                        if (t < 0) {
                            scale_x += scale_x_inc;
                            continue;
                        }
                        if (t > runs[sy].len)
                            break;
                        c = srcp[t];
                        if (c < 0xfe)
                            dstp[j] = ((ushort *)global_trans_table)[_table[c]];
                        scale_x += scale_x_inc;
                    }
                    scale_y += scale_y_inc;
                    dstp += buffer_pitch;
                }
            } else {













                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        } else {
            if (g_scaleY >= 0) {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        }
    }

    dst->release_data(1);
    return 0;
}


int JGLSprite::blit8to16_compressed(JGLBuffer *dst, int x, int y)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    JGLPalette *pal;
    int         i;
    int         j;
    int         xout;
    uchar       c;
    Run        *runs;
    int         dx;
    int         rightClip;
    int         rightSpace;
    int         count;
    unsigned    sy;
    int         t;

    if (this->m_pixels == 0 || dst == 0)
        return 3;

    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0)
        return 16;

    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    this->m_colorKey = (uchar)this->m_colorKey;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();

    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        return 1;
    }

    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {

        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (jgl_IntersectRect(&r, &r, dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }

        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
                dx = r.left - saved.left;
                rightClip = saved.right - r.right;
                runs = this->m_runs + (r.top - saved.top);



                for (i = 0; i < buffer_draw_height; i++) {
                    rightSpace = this->m_width - runs[i].start - runs[i].len;
                    xout = (runs[i].start - dx) < 0 ? 0 : (runs[i].start - dx);
                    srcp = this->m_pixels + runs[i].offset + ((dx - runs[i].start) < 0 ? 0 : (dx - runs[i].start));
                    count = runs[i].len - ((dx - runs[i].start) < 0 ? 0 : (dx - runs[i].start))
                            - ((rightClip - rightSpace) < 0 ? 0 : (rightClip - rightSpace));
                    for (j = 0; j < count; j++) {
                        c = *srcp;
                        if (c < 0xfe)
                            dstp[xout] = _table[c];
                        xout++;
                        srcp++;
                    }
                    dstp += buffer_pitch;
                }
            } else {


                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        } else {
            if (g_scaleY >= 0) {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        }

    } else {

        jgl::SetRect(&r, x, y, (int)(this->m_width * scale_factor_x), (int)(this->m_height * scale_factor_y));
        saved = r;
        if (jgl_IntersectRect(&r, &r, dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }

        dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
        buffer_pitch = dst->get_pitch();
        buffer_draw_width = r.right - r.left;
        buffer_draw_height = r.bottom - r.top;
        buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
        scale_x_inc = (g_scaleDenom << 16) / abs(g_scaleX);
        scale_y_inc = (g_scaleDenom << 16) / abs(g_scaleY);

        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                dx = r.left - saved.left;
                scale_y = scale_y_inc * (r.top - saved.top);
                runs = this->m_runs;

                for (i = 0; i < buffer_draw_height; i++) {
                    sy = (unsigned)scale_y >> 16;
                    if (runs[sy].len == 0)
                        continue;
                    scale_x = dx * scale_x_inc;
                    srcp = this->m_pixels + runs[sy].offset;
                    for (j = 0; j < buffer_draw_width; j++) {
                        t = ((unsigned)scale_x >> 16) - runs[sy].start;
                        if (t < 0) {
                            scale_x += scale_x_inc;
                            continue;
                        }
                        if (t > runs[sy].len)
                            break;
                        c = srcp[t];
                        if (c < 0xfe)
                            dstp[j] = _table[c];
                        scale_x += scale_x_inc;
                    }
                    scale_y += scale_y_inc;
                    dstp += buffer_pitch;
                }
            } else {













                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        } else {
            if (g_scaleY >= 0) {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        }
    }

    dst->release_data(1);
    return 0;
}


int JGLSprite::draw_z_outline_8_16c(JGLBuffer *dst, int x, int y, JGLBuffer *src, uchar cutoff, int colour)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp2;
    uchar      *srcp;
    JGLPalette *pal;
    int         i;
    int         j;
    int         xout;
    uchar       c;
    Run        *runs;
    int         dx;
    int         rightClip;
    int         rightSpace;
    int         count;

    if (this->m_pixels == 0 || dst == 0 || src == 0)
        return 3;

    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0)
        return 16;

    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    this->m_colorKey = (uchar)this->m_colorKey;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    z_buffer_pitch = src->get_pitch();

    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        return 1;
    }

    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {

        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (jgl_IntersectRect(&r, &r, dst->get_clip()) == 0) {
            src->release_data(1);
            dst->release_data(1);
            return 0;
        }

        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                srcp2 = src->get_data_8() + r.left + r.top * z_buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
                z_buffer_advance = z_buffer_pitch - buffer_draw_width;
                dx = r.left - saved.left;
                rightClip = saved.right - r.right;
                runs = this->m_runs + (r.top - saved.top);






                for (i = 0; i < buffer_draw_height; i++) {
                    rightSpace = this->m_width - runs[i].start - runs[i].len;
                    xout = (runs[i].start - dx) < 0 ? 0 : (runs[i].start - dx);
                    srcp = this->m_pixels + runs[i].offset + ((dx - runs[i].start) < 0 ? 0 : (dx - runs[i].start));
                    count = runs[i].len - ((dx - runs[i].start) < 0 ? 0 : (dx - runs[i].start))
                            - ((rightClip - rightSpace) < 0 ? 0 : (rightClip - rightSpace));
                    for (j = 0; j < count; j++) {
                        c = *srcp;
                        if (c != 0xff) {
                            if (srcp2[xout] > cutoff) {
                                if (c != 0xfe)
                                    dstp[xout] = _table[c];
                            } else {
                                if (c == 0xfe)
                                    dstp[xout] = colour;
                            }
                        }
                        xout++;
                        srcp++;
                    }
                    dstp += buffer_pitch;
                    srcp2 += buffer_pitch;
                }
            } else {


                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        } else {
            if (g_scaleY >= 0) {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        }
    }

    dst->release_data(1);
    src->release_data(1);
    return 0;
}







































































int JGLSprite::draw_z_8_16c(JGLBuffer *dst, int x, int y, JGLBuffer *src, uchar cutoff)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp2;
    uchar      *srcp;
    JGLPalette *pal;
    int         i;
    int         j;
    int         xout;
    uchar       c;
    Run        *runs;
    int         dx;
    int         rightClip;
    int         rightSpace;
    int         count;

    if (this->m_pixels == 0 || dst == 0 || src == 0)
        return 3;

    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0)
        return 16;

    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    this->m_colorKey = (uchar)this->m_colorKey;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    z_buffer_pitch = src->get_pitch();

    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        return 1;
    }

    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {

        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (jgl_IntersectRect(&r, &r, dst->get_clip()) == 0) {
            src->release_data(1);
            dst->release_data(1);
            return 0;
        }

        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                srcp2 = src->get_data_8() + r.left + r.top * z_buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
                z_buffer_advance = z_buffer_pitch - buffer_draw_width;
                dx = r.left - saved.left;
                rightClip = saved.right - r.right;
                runs = this->m_runs + (r.top - saved.top);






                for (i = 0; i < buffer_draw_height; i++) {
                    rightSpace = this->m_width - runs[i].start - runs[i].len;
                    xout = (runs[i].start - dx) < 0 ? 0 : (runs[i].start - dx);
                    srcp = this->m_pixels + runs[i].offset + ((dx - runs[i].start) < 0 ? 0 : (dx - runs[i].start));
                    count = runs[i].len - ((dx - runs[i].start) < 0 ? 0 : (dx - runs[i].start))
                            - ((rightClip - rightSpace) < 0 ? 0 : (rightClip - rightSpace));
                    for (j = 0; j < count; j++) {
                        c = *srcp;
                        if (c < 0xfe) {
                            if (srcp2[xout] > cutoff)
                                if (c != 0xfe)
                                    dstp[xout] = _table[c];
                        }
                        xout++;
                        srcp++;
                    }
                    dstp += buffer_pitch;
                    srcp2 += buffer_pitch;
                }
            } else {


                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        } else {
            if (g_scaleY >= 0) {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        }
    }

    dst->release_data(1);
    src->release_data(1);
    return 0;
}








































































int JGLSprite::draw_dest_8_16c(JGLBuffer *dst, int x, int y, ushort *darken)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    JGLPalette *pal;
    int         i;
    int         j;
    int         xout;
    uchar       c;
    Run        *runs;
    int         dx;
    int         rightClip;
    int         rightSpace;
    int         count;
    unsigned    sy;
    int         t;

    if (this->m_pixels == 0 || dst == 0)
        return 3;

    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0)
        return 16;

    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    this->m_colorKey = (uchar)this->m_colorKey;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    global_shadow_table = darken;

    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        return 1;
    }

    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {

        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (jgl_IntersectRect(&r, &r, dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }

        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
                dx = r.left - saved.left;
                rightClip = saved.right - r.right;
                runs = this->m_runs + (r.top - saved.top);



                for (i = 0; i < buffer_draw_height; i++) {
                    rightSpace = this->m_width - runs[i].start - runs[i].len;
                    xout = (runs[i].start - dx) < 0 ? 0 : (runs[i].start - dx);
                    srcp = this->m_pixels + runs[i].offset + ((dx - runs[i].start) < 0 ? 0 : (dx - runs[i].start));
                    count = runs[i].len - ((dx - runs[i].start) < 0 ? 0 : (dx - runs[i].start))
                            - ((rightClip - rightSpace) < 0 ? 0 : (rightClip - rightSpace));
                    for (j = 0; j < count; j++) {
                        c = *srcp;
                        if (c != 0xff) {
                            if (c >= 0xf8)
                                dstp[xout] = global_shadow_table[dstp[xout]];
                            else
                                dstp[xout] = _table[c];
                        }
                        xout++;
                        srcp++;
                    }
                    dstp += buffer_pitch;
                }
            } else {


                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        } else {
            if (g_scaleY >= 0) {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        }

    } else {

        jgl::SetRect(&r, x, y, (int)(this->m_width * scale_factor_x), (int)(this->m_height * scale_factor_y));
        saved = r;
        if (jgl_IntersectRect(&r, &r, dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }

        dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
        buffer_pitch = dst->get_pitch();
        buffer_draw_width = r.right - r.left;
        buffer_draw_height = r.bottom - r.top;
        buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
        scale_x_inc = (g_scaleDenom << 16) / abs(g_scaleX);
        scale_y_inc = (g_scaleDenom << 16) / abs(g_scaleY);

        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                dx = r.left - saved.left;
                scale_y = scale_y_inc * (r.top - saved.top);
                runs = this->m_runs;

                for (i = 0; i < buffer_draw_height; i++) {
                    sy = (unsigned)scale_y >> 16;
                    if (runs[sy].len == 0)
                        continue;
                    scale_x = dx * scale_x_inc;
                    srcp = this->m_pixels + runs[sy].offset;
                    for (j = 0; j < buffer_draw_width; j++) {
                        t = ((unsigned)scale_x >> 16) - runs[sy].start;
                        if (t < 0) {
                            scale_x += scale_x_inc;
                            continue;
                        }
                        if (t > runs[sy].len)
                            break;
                        c = srcp[t];
                        if (c != 0xff) {
                            if (c >= 0xf8)
                                dstp[j] = global_shadow_table[dstp[j]];
                            else
                                dstp[j] = _table[c];
                        }
                        scale_x += scale_x_inc;
                    }
                    scale_y += scale_y_inc;
                    dstp += buffer_pitch;
                }
            } else {













                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        } else {
            if (g_scaleY >= 0) {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        }
    }

    dst->release_data(1);
    return 0;
}



int JGLSprite::draw_mono_8_16c(JGLBuffer *dst, int x, int y, int colour)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    JGLPalette *pal;
    int         i;
    int         j;
    int         xout;
    uchar       c;
    Run        *runs;
    int         dx;
    int         rightClip;
    int         rightSpace;
    int         count;
    unsigned    sy;
    int         t;

    if (this->m_pixels == 0 || dst == 0)
        return 3;

    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0)
        return 16;

    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    this->m_colorKey = (uchar)this->m_colorKey;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();

    switch (dst->get_pixformat()->code) {
    case 0:
        global_mono_color = pal->get_555_table()[colour & 0xff];
        break;
    case 1:
        global_mono_color = pal->get_565_table()[colour & 0xff];
        break;
    default:
        return 1;
    }

    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {

        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (jgl_IntersectRect(&r, &r, dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }

        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
                dx = r.left - saved.left;
                rightClip = saved.right - r.right;
                runs = this->m_runs + (r.top - saved.top);



                for (i = 0; i < buffer_draw_height; i++) {
                    rightSpace = this->m_width - runs[i].start - runs[i].len;
                    xout = (runs[i].start - dx) < 0 ? 0 : (runs[i].start - dx);
                    srcp = this->m_pixels + runs[i].offset + ((dx - runs[i].start) < 0 ? 0 : (dx - runs[i].start));
                    count = runs[i].len - ((dx - runs[i].start) < 0 ? 0 : (dx - runs[i].start))
                            - ((rightClip - rightSpace) < 0 ? 0 : (rightClip - rightSpace));
                    for (j = 0; j < count; j++) {
                        c = *srcp;
                        if (c != 0xff)
                            dstp[xout] = global_mono_color;
                        xout++;
                        srcp++;
                    }
                    dstp += buffer_pitch;
                }
            } else {


                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        } else {
            if (g_scaleY >= 0) {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        }

    } else {

        jgl::SetRect(&r, x, y, (int)(this->m_width * scale_factor_x), (int)(this->m_height * scale_factor_y));
        saved = r;
        if (jgl_IntersectRect(&r, &r, dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }

        dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
        buffer_pitch = dst->get_pitch();
        buffer_draw_width = r.right - r.left;
        buffer_draw_height = r.bottom - r.top;
        buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
        scale_x_inc = (g_scaleDenom << 16) / abs(g_scaleX);
        scale_y_inc = (g_scaleDenom << 16) / abs(g_scaleY);

        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                dx = r.left - saved.left;
                scale_y = scale_y_inc * (r.top - saved.top);
                runs = this->m_runs;

                for (i = 0; i < buffer_draw_height; i++) {
                    sy = (unsigned)scale_y >> 16;
                    if (runs[sy].len == 0)
                        continue;
                    scale_x = dx * scale_x_inc;
                    srcp = this->m_pixels + runs[sy].offset;
                    for (j = 0; j < buffer_draw_width; j++) {
                        t = ((unsigned)scale_x >> 16) - runs[sy].start;
                        if (t < 0) {
                            scale_x += scale_x_inc;
                            continue;
                        }
                        if (t > runs[sy].len)
                            break;
                        c = srcp[t];
                        if (c != 0xff)
                            dstp[j] = global_mono_color;
                        scale_x += scale_x_inc;
                    }
                    scale_y += scale_y_inc;
                    dstp += buffer_pitch;
                }
            } else {













                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        } else {
            if (g_scaleY >= 0) {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        }
    }

    dst->release_data(1);
    return 0;
}


int JGLSprite::blitRecolored8to16_compressed(JGLBuffer *dst, int x, int y, uchar *xlat, ushort *darken)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    JGLPalette *pal;
    int         i;
    int         j;
    int         xout;
    uchar       c;
    Run        *runs;
    int         dx;
    int         rightClip;
    int         rightSpace;
    int         count;
    unsigned    sy;
    int         t;

    if (this->m_pixels == 0 || dst == 0)
        return 3;

    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0)
        return 16;

    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    this->m_colorKey = (uchar)this->m_colorKey;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    global_shadow_table = darken;
    global_trans_table = xlat;

    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        return 1;
    }

    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {

        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (jgl_IntersectRect(&r, &r, dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }

        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
                dx = r.left - saved.left;
                rightClip = saved.right - r.right;
                runs = this->m_runs + (r.top - saved.top);



                for (i = 0; i < buffer_draw_height; i++) {
                    rightSpace = this->m_width - runs[i].start - runs[i].len;
                    xout = (runs[i].start - dx) < 0 ? 0 : (runs[i].start - dx);
                    srcp = this->m_pixels + runs[i].offset + ((dx - runs[i].start) < 0 ? 0 : (dx - runs[i].start));
                    count = runs[i].len - ((dx - runs[i].start) < 0 ? 0 : (dx - runs[i].start))
                            - ((rightClip - rightSpace) < 0 ? 0 : (rightClip - rightSpace));
                    for (j = 0; j < count; j++) {
                        c = *srcp;
                        if (c != 0xff) {
                            if (c >= 0xf8) {
                                dstp[xout] = global_shadow_table[dstp[xout]];
                            } else {
                                dstp[xout] = _table[global_trans_table[c]];
                            }
                        }
                        xout++;
                        srcp++;
                    }
                    dstp += buffer_pitch;
                }
            } else {


                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        } else {
            if (g_scaleY >= 0) {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        }

    } else {

        jgl::SetRect(&r, x, y, (int)(this->m_width * scale_factor_x), (int)(this->m_height * scale_factor_y));
        saved = r;
        if (jgl_IntersectRect(&r, &r, dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }

        dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
        buffer_pitch = dst->get_pitch();
        buffer_draw_width = r.right - r.left;
        buffer_draw_height = r.bottom - r.top;
        buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
        scale_x_inc = (g_scaleDenom << 16) / abs(g_scaleX);
        scale_y_inc = (g_scaleDenom << 16) / abs(g_scaleY);

        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                dx = r.left - saved.left;
                scale_y = scale_y_inc * (r.top - saved.top);
                runs = this->m_runs;

                for (i = 0; i < buffer_draw_height; i++) {
                    sy = (unsigned)scale_y >> 16;
                    if (runs[sy].len == 0)
                        continue;
                    scale_x = dx * scale_x_inc;
                    srcp = this->m_pixels + runs[sy].offset;
                    for (j = 0; j < buffer_draw_width; j++) {
                        t = ((unsigned)scale_x >> 16) - runs[sy].start;
                        if (t < 0) {
                            scale_x += scale_x_inc;
                            continue;
                        }
                        if (t > runs[sy].len)
                            break;
                        c = srcp[t];
                        if (c != 0xff) {
                            if (c >= 0xf8) {
                                dstp[j] = global_shadow_table[dstp[j]];
                            } else {
                                dstp[j] = _table[global_trans_table[c]];
                            }
                        }
                        scale_x += scale_x_inc;
                    }
                    scale_y += scale_y_inc;
                    dstp += buffer_pitch;
                }
            } else {













                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        } else {
            if (g_scaleY >= 0) {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        }
    }

    dst->release_data(1);
    return 0;
}


int JGLSprite::blitRecoloredAfterPalette8to16_compressed(JGLBuffer *dst, int x, int y, uchar *xlat, ushort *darken)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    JGLPalette *pal;
    int         i;
    int         j;
    int         xout;
    uchar       c;
    Run        *runs;
    int         dx;
    int         rightClip;
    int         rightSpace;
    int         count;
    unsigned    sy;
    int         t;

    if (this->m_pixels == 0 || dst == 0)
        return 3;

    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0)
        return 16;

    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    this->m_colorKey = (uchar)this->m_colorKey;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    global_shadow_table = darken;
    global_trans_table = xlat;

    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        return 1;
    }

    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {

        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (jgl_IntersectRect(&r, &r, dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }

        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
                dx = r.left - saved.left;
                rightClip = saved.right - r.right;
                runs = this->m_runs + (r.top - saved.top);



                for (i = 0; i < buffer_draw_height; i++) {
                    rightSpace = this->m_width - runs[i].start - runs[i].len;
                    xout = (runs[i].start - dx) < 0 ? 0 : (runs[i].start - dx);
                    srcp = this->m_pixels + runs[i].offset + ((dx - runs[i].start) < 0 ? 0 : (dx - runs[i].start));
                    count = runs[i].len - ((dx - runs[i].start) < 0 ? 0 : (dx - runs[i].start))
                            - ((rightClip - rightSpace) < 0 ? 0 : (rightClip - rightSpace));
                    for (j = 0; j < count; j++) {
                        c = *srcp;
                        if (c != 0xff) {
                            if (c >= 0xf8) {
                                dstp[xout] = global_shadow_table[dstp[j]];
                            } else {
                                dstp[xout] = ((ushort *)global_trans_table)[_table[c]];
                            }
                        }
                        xout++;
                        srcp++;
                    }
                    dstp += buffer_pitch;
                }
            } else {


                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        } else {
            if (g_scaleY >= 0) {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        }

    } else {

        jgl::SetRect(&r, x, y, (int)(this->m_width * scale_factor_x), (int)(this->m_height * scale_factor_y));
        saved = r;
        if (jgl_IntersectRect(&r, &r, dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }

        dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
        buffer_pitch = dst->get_pitch();
        buffer_draw_width = r.right - r.left;
        buffer_draw_height = r.bottom - r.top;
        buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
        scale_x_inc = (g_scaleDenom << 16) / abs(g_scaleX);
        scale_y_inc = (g_scaleDenom << 16) / abs(g_scaleY);

        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                dx = r.left - saved.left;
                scale_y = scale_y_inc * (r.top - saved.top);
                runs = this->m_runs;

                for (i = 0; i < buffer_draw_height; i++) {
                    sy = (unsigned)scale_y >> 16;
                    if (runs[sy].len == 0)
                        continue;
                    scale_x = dx * scale_x_inc;
                    srcp = this->m_pixels + runs[sy].offset;
                    for (j = 0; j < buffer_draw_width; j++) {
                        t = ((unsigned)scale_x >> 16) - runs[sy].start;
                        if (t < 0) {
                            scale_x += scale_x_inc;
                            continue;
                        }
                        if (t > runs[sy].len)
                            break;
                        c = srcp[t];
                        if (c != 0xff) {
                            if (c >= 0xf8) {
                                dstp[j] = global_shadow_table[dstp[j]];
                            } else {
                                dstp[j] = ((ushort *)global_trans_table)[_table[c]];
                            }
                        }
                        scale_x += scale_x_inc;
                    }
                    scale_y += scale_y_inc;
                    dstp += buffer_pitch;
                }
            } else {













                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        } else {
            if (g_scaleY >= 0) {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            }
        }
    }

    dst->release_data(1);
    return 0;
}
