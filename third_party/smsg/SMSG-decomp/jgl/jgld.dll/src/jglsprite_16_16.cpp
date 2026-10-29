#include "jgldefs.h"
#include "jgltypes.h"
#include "jglsprite.h"
#include "jglwin.h"
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include "jglglobals.h"
#include "jglalloc.h"

static int     s_lostEntry;
static int     sprite_advance;
static int     scale_y_inc;
static int     s_lostInts[3];
static int     buffer_pitch;
static int     buffer_advance;
static int     buffer_draw_width;
static ushort  global_seethru;
static ushort *_table;
static int     s_lostSlots[2];
static int     buffer_draw_height;
static int     sprite_pitch;
static int     s_lostVal;
static int     scale_x_inc;
static float   scale_factor_x = 0;
static float   scale_factor_y = 0;

int JGLSprite::init_16(ushort *pixels, int width, int height, int bpp, int memType)
{
    unsigned int  budget;
    const ushort *rowSrc;
    ushort       *dstw;
    const ushort *src;
    int           y;
    int           x;
    this->close();
    this->m_width = width;
    this->m_height = height;
    this->m_bpp = 16;
    this->m_dataSize = width * height * 2;
    this->m_pixels = (uchar *)jgl_malloc(this->m_dataSize);
    if (this->m_pixels == 0)
        return 4;
    g_spriteBytesRaw += width * height;
    if (pixels != 0) {
        if (g_spriteCompress != 0 && width > sizeof(Run) && width < 256 && width * height < 0x10000) {
            budget = width * height - height * sizeof(Run);
            this->m_runs = (Run *)jgl_malloc(height * sizeof(Run));
            if (this->m_runs == 0) {
                this->close();
                return 4;
            }
            dstw = (ushort *)this->m_pixels;
            src = pixels;
            for (y = 0; y < height; y++) {
                rowSrc = src;
                for (x = 0; x < width; x++)
                    if (rowSrc[x] != this->m_colorKey)
                        break;
                this->m_runs[y].start = (uchar)x;
                for (x = width - 1; x > this->m_runs[y].start; x--)
                    if (rowSrc[x] != this->m_colorKey)
                        break;
                this->m_runs[y].len = (uchar)(x + 1 - this->m_runs[y].start);
                if (this->m_runs[y].len != 0) {
                    if (budget < this->m_runs[y].len) {
                        jgl_free(this->m_runs);
                        this->m_runs = 0;
                        goto NOCOMPRESS;
                    }
                    this->m_runs[y].offset = (ushort)(dstw - (ushort *)this->m_pixels);
                    memcpy(dstw, rowSrc + this->m_runs[y].start, this->m_runs[y].len);
                    budget -= this->m_runs[y].len;
                    dstw += this->m_runs[y].len;
                }
                src += width;
            }
            this->m_flags |= 1;
            this->m_dataSize = width * height - budget;
            this->m_pixels = (uchar *)_expand(this->m_pixels, this->m_dataSize);
            g_spriteBytesSaved += width * height - this->m_dataSize - height * sizeof(Run);
            goto SETSTRIDE;
        }
NOCOMPRESS:
        this->m_flags &= ~1;
        memcpy(this->m_pixels, pixels, width * height);
    }
SETSTRIDE:
    this->m_srcStride = width;
    return 0;
}
int JGLSprite::draw_16_16(JGLBuffer *dst, int x, int y)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    ushort     *srcp;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0)
        return 3;
    if ((this->m_colorKey & 0x80000000) == 0) {
        pal = 0;
        if (dst->get_current_palette() != 0)
            pal = dst->get_current_palette();
        else if (def_palette != 0)
            pal = def_palette->pal;
        if (pal != 0) {
            switch (dst->get_pixformat()->code) {
            case 0:
                this->m_colorKey = pal->get_555_table()[(uchar)this->m_colorKey];
                break;
            case 1:
                this->m_colorKey = pal->get_565_table()[(uchar)this->m_colorKey];
                break;
            default:
                return 1;
            }
        }
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    global_seethru = (ushort)this->m_colorKey;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = (ushort *)this->m_pixels;
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (LPRECT)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0 && g_scaleY >= 0) {
            srcp = (ushort *)this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
            dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
            buffer_draw_height = r.bottom - r.top;
            buffer_draw_width = r.right - r.left;
            sprite_advance = (sprite_pitch - buffer_draw_width) * 2;
            buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
            __asm {
                push esi
                push edi
                mov  esi, srcp
                mov  edi, dstp
                mov  ax,  global_seethru
                mov  ecx, sprite_advance
                push ebp
                mov  ebp, buffer_advance
            L_row:
                mov  edx, buffer_draw_width
            L_pix:
                cmp  word ptr [esi], ax
                jz   L_next
                mov  bx, word ptr [esi]
                mov  word ptr [edi], bx
            L_next:
                add  esi, 2
                add  edi, 2
                dec  edx
                jnz  L_pix
                add  esi, ecx
                add  edi, ebp
                dec  buffer_draw_height
                jnz  L_row
                pop  ebp
                pop  edi
                pop  esi
            }
        }
    }
    dst->release_data(1);
    return 0;
}
int JGLSprite::draw_dest_16_16(JGLBuffer *dst, int x, int y, ushort *shadow)
{
    Rect    r;
    Rect    saved;
    ushort *dstp;
    ushort *srcp;
    if (this->m_pixels == 0 || dst == 0)
        return 3;
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    global_seethru = (uchar)this->m_colorKey;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = (ushort *)this->m_pixels;
    _table = shadow;
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (LPRECT)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0 && g_scaleY >= 0) {
            srcp = (ushort *)this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
            dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
            buffer_draw_height = r.bottom - r.top;
            buffer_draw_width = r.right - r.left;
            sprite_advance = (sprite_pitch - buffer_draw_width) * 2;
            buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
            __asm {
                push esi
                push edi
                xor  ebx, ebx
                mov  esi, srcp
                mov  edi, dstp
                mov  al,  byte ptr global_seethru
                mov  ecx, sprite_advance
                push ebp
                mov  ebp, _table
            L_row:
                mov  edx, buffer_draw_width
            L_pix:
                cmp  byte ptr [esi], al
                jz   L_next
                cmp  byte ptr [esi], 0
                jnz  L_opaque
                mov  bx, word ptr [edi]
                mov  bx, word ptr [ebp+ebx*2]
                mov  word ptr [edi], bx
                jmp  L_next
            L_opaque:
                mov  bx, word ptr [esi]
                mov  word ptr [edi], bx
            L_next:
                add  esi, 2
                add  edi, 2
                dec  edx
                jnz  L_pix
                add  esi, ecx
                add  edi, buffer_advance
                dec  buffer_draw_height
                jnz  L_row
                pop  ebp
                pop  edi
                pop  esi
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
        buffer_advance = buffer_pitch - buffer_draw_width;
        scale_x_inc = (g_scaleDenom << 16) / abs(g_scaleX);
        scale_y_inc = (g_scaleDenom << 16) / abs(g_scaleY);
    }
    dst->release_data(1);
    return 0;
}
int JGLSprite::draw_multi_table_dest_16_16(JGLBuffer *dst, int x, int y, ushort *shadow)
{
    Rect    r;
    Rect    saved;
    ushort *dstp;
    ushort *srcp;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    this->m_colorKey = 0xff;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = (ushort *)this->m_pixels;
    _table = shadow;
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (LPRECT)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0 && g_scaleY >= 0) {
            srcp = (ushort *)this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
            dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
            buffer_draw_height = r.bottom - r.top;
            buffer_draw_width = r.right - r.left;
            sprite_advance = (sprite_pitch - buffer_draw_width) * 2;
            buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
            __asm {
                push esi
                push edi
                xor  ebx, ebx
                mov  esi, srcp
                mov  edi, dstp
                mov  ecx, sprite_advance
                push ebp
                mov  ebp, _table
            L_row:
                mov  edx, buffer_draw_width
            L_pix:
                cmp  byte ptr [esi], 0ffh
                jz   L_next
                xor  eax, eax
                mov  ax, word ptr [esi]
                shl  eax, 16
                mov  ax, word ptr [edi]
                mov  ax, word ptr [eax+ebp]
                mov  word ptr [edi], ax
            L_next:
                add  esi, 2
                add  edi, 2
                dec  edx
                jnz  L_pix
                add  esi, ecx
                add  edi, buffer_advance
                dec  buffer_draw_height
                jnz  L_row
                pop  ebp
                pop  edi
                pop  esi
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
        buffer_advance = buffer_pitch - buffer_draw_width;
        scale_x_inc = (g_scaleDenom << 16) / abs(g_scaleX);
        scale_y_inc = (g_scaleDenom << 16) / abs(g_scaleY);
    }
    dst->release_data(1);
    return 0;
}
int JGLSprite::draw_16_shadows_16_16(JGLBuffer *dst, int x, int y, ushort *shadow)
{
    return 0;
}
int JGLSprite::draw_civ3_units_16_16(JGLBuffer *dst, int x, int y, ushort *shadow)
{
    return 0;
}
int JGLSprite::draw_sudo_trans_16_16_50(JGLBuffer *dst, int x, int y)
{
    return 0;
}
int JGLSprite::draw_sudo_trans_16_16_25(JGLBuffer *dst, int x, int y)
{
    return 0;
}
