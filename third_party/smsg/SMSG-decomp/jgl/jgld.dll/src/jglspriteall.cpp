#include "jgldefs.h"
#include "jgltypes.h"
#include "jglsprite.h"
#include "jglbuffertyped.h"
#include "jglwin.h"
#include <string.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
struct Format {
    int bpp;
    int code;
};
#include "jglglobals.h"
#include "jglblitadv.h"
static ushort   global_mono_color;
static int      z_buffer_advance;
static int      z_buffer_pitch;
static int      sprite_advance;
static unsigned scale_y_inc;
static unsigned frac_scale_y_inc;
static ushort  *global_table;
static unsigned scale_x;
static unsigned scale_y;
static int      buffer_pitch;
static int      buffer_advance;
static int      buffer_draw_width;
static uchar    global_depth;
static ushort  *_table;
static int      sprite_draw_width;
static ushort  *global_shadow_table;
static int      buffer_draw_height;
static int      sprite_pitch;
static int      x_inc_int;
static int      sprite_draw_height;
static unsigned scale_x_inc;
static float    scale_factor_x = 0;
static float    scale_factor_y = 0;
#include "jglalloc.h"

int JGLSprite::init_8(const uchar *pixels, int width, int height, int bpp, int memType)
{
    unsigned int budget;
    const uchar *rowSrc;
    uchar       *dstw;
    const uchar *src;
    int          y;
    int          x;
    this->close();
    this->m_width = width;
    this->m_height = height;
    this->m_bpp = 8;
    this->m_dataSize = width * height;
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
            dstw = this->m_pixels;
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
                    this->m_runs[y].offset = (ushort)(dstw - this->m_pixels);
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
int JGLSprite::change_color_8(int nOldIndex, int nNewIndex)
{
    uchar *p;
    int    nDwords;
    int    nBytes;
    if (this->m_pixels == 0)
        return 7;
    nOldIndex &= 0xff;
    nNewIndex &= 0xff;
    p = this->m_pixels;
    if (this->m_flags & 1) {
        nDwords = this->m_dataSize >> 2;
        nBytes = this->m_dataSize - (nDwords << 2);
    } else {
        nDwords = (this->m_srcStride * this->m_height) >> 2;
        nBytes = this->m_srcStride * this->m_height - (nDwords << 2);
    }
    __asm {
        push    esi
        mov     esi, p
        mov     ecx, nDwords
        mov     al,  byte ptr nOldIndex
        mov     ah,  byte ptr nNewIndex
        and     ecx, ecx
        jz      done
    scan0: cmp     byte ptr [esi], al
        jz      store0
    scan1: inc     esi
        cmp     byte ptr [esi], al
        jz      store1
    scan2: inc     esi
        cmp     byte ptr [esi], al
        jz      store2
    scan3: inc     esi
        cmp     byte ptr [esi], al
        jz      store3
    scanNext: inc     esi
        loop    scan0
        dec     esi
        jmp     tail
    storeTest0: cmp     byte ptr [esi], al
        jnz     scan1
    store0: mov     byte ptr [esi], ah
        inc     esi
        cmp     byte ptr [esi], al
        jnz     scan2
    store1: mov     byte ptr [esi], ah
        inc     esi
        cmp     byte ptr [esi], al
        jnz     scan3
    store2: mov     byte ptr [esi], ah
        inc     esi
        cmp     byte ptr [esi], al
        jnz     scanNext
    store3: mov     byte ptr [esi], ah
        inc     esi
        loop    storeTest0
        dec     esi
    tail:
        mov     ecx, nBytes
        and     ecx, ecx
        jz      done
    tailTest: inc     esi
        cmp     byte ptr [esi], al
        jnz     tailNext
        mov     byte ptr [esi], ah
    tailNext: loop    tailTest
    done:
        pop     esi
    }
    return 0;
}
int JGLSprite::draw_8_16(JGLBuffer *dst, int x, int y)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                L1_row:
                    mov  edx, buffer_draw_width
                L1_pix:
                    cmp  byte ptr [esi], 0feh
                    jnb  L1_next
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                L1_next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  L1_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  L1_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                L2_row:
                    mov  edx, buffer_draw_width
                L2_pix:
                    cmp  byte ptr [esi], 0feh
                    jnb  L2_next
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                L2_next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  L2_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  L2_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                L3_row:
                    mov  edx, buffer_draw_width
                L3_pix:
                    cmp  byte ptr [esi], 0feh
                    jnb  L3_next
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                L3_next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  L3_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  L3_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                L4_row:
                    mov  edx, buffer_draw_width
                L4_pix:
                    cmp  byte ptr [esi], 0feh
                    jnb  L4_next
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                L4_next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  L4_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  L4_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 16
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 16
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 16
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc+2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 16
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                L5_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                L5_pix:
                    cmp  byte ptr [esi], 0feh
                    jnb  L5_next
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                L5_next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  L5_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  L5_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 16
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 16
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 16
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc+2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 16
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                L6_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                L6_pix:
                    cmp  byte ptr [esi], 0feh
                    jnb  L6_next
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                L6_next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  L6_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  L6_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 16
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 16
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 16
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc+2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 16
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                L7_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                L7_pix:
                    cmp  byte ptr [esi], 0feh
                    jnb  L7_next
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                L7_next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  L7_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  L7_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 16
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 16
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 16
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc+2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 16
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                L8_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                L8_pix:
                    cmp  byte ptr [esi], 0feh
                    jnb  L8_next
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                L8_next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  L8_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  L8_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::draw_254_8_16(JGLBuffer *dst, int x, int y)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                L1_row:
                    mov  edx, buffer_draw_width
                L1_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnb  L1_next
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                L1_next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  L1_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  L1_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                L2_row:
                    mov  edx, buffer_draw_width
                L2_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnb  L2_next
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                L2_next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  L2_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  L2_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                L3_row:
                    mov  edx, buffer_draw_width
                L3_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnb  L3_next
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                L3_next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  L3_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  L3_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                L4_row:
                    mov  edx, buffer_draw_width
                L4_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnb  L4_next
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                L4_next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  L4_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  L4_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 16
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 16
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 16
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc+2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 16
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                L5_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                L5_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnb  L5_next
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                L5_next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  L5_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  L5_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 16
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 16
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 16
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc+2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 16
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                L6_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                L6_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnb  L6_next
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                L6_next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  L6_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  L6_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 16
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 16
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 16
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc+2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 16
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                L7_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                L7_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnb  L7_next
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                L7_next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  L7_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  L7_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 16
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 16
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 16
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc+2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 16
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                L8_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                L8_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnb  L8_next
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                L8_next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  L8_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  L8_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::draw_z_outline_8_16(JGLBuffer *dst, int x, int y, JGLBuffer *src, uchar cutoff, int colour)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    uchar      *depthp;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (src == 0 || src->get_data() == 0) {
        dst->release_data(1);
        return 3;
    }
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        src->release_data(2);
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    z_buffer_pitch = src->get_pitch();
    srcp = this->m_pixels;
    global_depth = cutoff;
    global_mono_color = colour;
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
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            src->release_data(2);
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                depthp = src->get_data_8() + r.left + r.top * z_buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                z_buffer_advance = z_buffer_pitch - buffer_draw_width;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  ecx, depthp
                    mov  ax,  global_mono_color
                    mov  bl,  global_depth
                    push ebp
                    mov  ebp, _table
                Q1_row:
                    mov  edx, buffer_draw_width
                Q1_pix:
                    cmp  byte ptr [esi], 0ffh
                    jz   Q1_next
                    cmp  byte ptr [ecx], bl
                    jnb  Q1_lit
                    cmp  byte ptr [esi], 0feh
                    jnz  Q1_next
                    mov  ax, global_mono_color
                    mov  word ptr [edi], ax
                    jmp  Q1_next
                Q1_lit:
                    cmp  byte ptr [esi], 0feh
                    jz   Q1_next
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ebp+eax*2]
                    mov  word ptr [edi], ax
                Q1_next:
                    inc  esi
                    add  edi, 2
                    inc  ecx
                    dec  edx
                    jnz  Q1_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    add  ecx, z_buffer_advance
                    dec  buffer_draw_height
                    jnz  Q1_row
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                z_buffer_advance = z_buffer_pitch - buffer_draw_width;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  ecx, depthp
                    mov  ax,  global_mono_color
                    mov  bl,  global_depth
                    push ebp
                    mov  ebp, _table
                Q2_row:
                    mov  edx, buffer_draw_width
                Q2_pix:
                    cmp  byte ptr [esi], 0ffh
                    jz   Q2_next
                    cmp  byte ptr [ecx], bl
                    jnb  Q2_lit
                    cmp  byte ptr [esi], 0feh
                    jnz  Q2_next
                    mov  ax, global_mono_color
                    mov  word ptr [edi], ax
                    jmp  Q2_next
                Q2_lit:
                    cmp  byte ptr [esi], 0feh
                    jz   Q2_next
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ebp+eax*2]
                    mov  word ptr [edi], ax
                Q2_next:
                    inc  esi
                    add  edi, 2
                    inc  ecx
                    dec  edx
                    jnz  Q2_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    add  ecx, z_buffer_advance
                    dec  buffer_draw_height
                    jnz  Q2_row
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                z_buffer_advance = z_buffer_pitch - buffer_draw_width;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  ecx, depthp
                    mov  ax,  global_mono_color
                    mov  bl,  global_depth
                    push ebp
                    mov  ebp, _table
                Q3_row:
                    mov  edx, buffer_draw_width
                Q3_pix:
                    cmp  byte ptr [esi], 0ffh
                    jz   Q3_next
                    cmp  byte ptr [ecx], bl
                    jnb  Q3_lit
                    cmp  byte ptr [esi], 0feh
                    jnz  Q3_next
                    mov  ax, global_mono_color
                    mov  word ptr [edi], ax
                    jmp  Q3_next
                Q3_lit:
                    cmp  byte ptr [esi], 0feh
                    jz   Q3_next
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ebp+eax*2]
                    mov  word ptr [edi], ax
                Q3_next:
                    dec  esi
                    add  edi, 2
                    add  ecx, 2
                    dec  edx
                    jnz  Q3_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    add  ecx, z_buffer_advance
                    dec  buffer_draw_height
                    jnz  Q3_row
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                z_buffer_advance = buffer_pitch - buffer_draw_width;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  ecx, depthp
                    mov  ax,  global_mono_color
                    mov  bl,  global_depth
                    push ebp
                    mov  ebp, _table
                Q4_row:
                    mov  edx, buffer_draw_width
                Q4_pix:
                    cmp  byte ptr [esi], 0ffh
                    jz   Q4_next
                    cmp  byte ptr [ecx], bl
                    jnb  Q4_lit
                    cmp  byte ptr [esi], 0feh
                    jnz  Q4_next
                    mov  ax, global_mono_color
                    mov  word ptr [edi], ax
                    jmp  Q4_next
                Q4_lit:
                    cmp  byte ptr [esi], 0feh
                    jz   Q4_next
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ebp+eax*2]
                    mov  word ptr [edi], ax
                Q4_next:
                    dec  esi
                    add  edi, 2
                    add  ecx, 2
                    dec  edx
                    jnz  Q4_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    add  ecx, z_buffer_advance
                    dec  buffer_draw_height
                    jnz  Q4_row
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            }
        }
    } else {
        src->release_data(2);
        dst->release_data(2);
    }
    dst->release_data(2);
    src->release_data(2);
    return 0;
}
int JGLSprite::draw_z_8_16(JGLBuffer *dst, int x, int y, JGLBuffer *src, uchar cutoff)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    uchar      *depthp;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (src == 0 || src->get_data() == 0) {
        dst->release_data(1);
        return 3;
    }
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        src->release_data(2);
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    z_buffer_pitch = src->get_pitch();
    srcp = this->m_pixels;
    global_depth = cutoff;
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
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            src->release_data(2);
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                depthp = src->get_data_8() + r.left + r.top * z_buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                z_buffer_advance = z_buffer_pitch - buffer_draw_width;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  ecx, depthp
                    mov  bl,  global_depth
                    push ebp
                    mov  ebp, _table
                Q1_row:
                    mov  edx, buffer_draw_width
                Q1_pix:
                    cmp  byte ptr [esi], 0feh
                    jnb  Q1_next
                    cmp  byte ptr [ecx], bl
                    jb   Q1_next
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ebp+eax*2]
                    mov  word ptr [edi], ax
                Q1_next:
                    inc  esi
                    add  edi, 2
                    inc  ecx
                    dec  edx
                    jnz  Q1_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    add  ecx, z_buffer_advance
                    dec  buffer_draw_height
                    jnz  Q1_row
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                z_buffer_advance = z_buffer_pitch - buffer_draw_width;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  ecx, depthp
                    mov  bl,  global_depth
                    push ebp
                    mov  ebp, _table
                Q2_row:
                    mov  edx, buffer_draw_width
                Q2_pix:
                    cmp  byte ptr [esi], 0feh
                    jnb  Q2_next
                    cmp  byte ptr [ecx], bl
                    jb   Q2_next
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ebp+eax*2]
                    mov  word ptr [edi], ax
                Q2_next:
                    inc  esi
                    add  edi, 2
                    inc  ecx
                    dec  edx
                    jnz  Q2_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    add  ecx, z_buffer_advance
                    dec  buffer_draw_height
                    jnz  Q2_row
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                z_buffer_advance = z_buffer_pitch - buffer_draw_width;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  ecx, depthp
                    mov  bl,  global_depth
                    push ebp
                    mov  ebp, _table
                Q3_row:
                    mov  edx, buffer_draw_width
                Q3_pix:
                    cmp  byte ptr [esi], 0feh
                    jnb  Q3_next
                    cmp  byte ptr [ecx], bl
                    jb   Q3_next
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ebp+eax*2]
                    mov  word ptr [edi], ax
                Q3_next:
                    dec  esi
                    add  edi, 2
                    add  ecx, 2
                    dec  edx
                    jnz  Q3_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    add  ecx, z_buffer_advance
                    dec  buffer_draw_height
                    jnz  Q3_row
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                z_buffer_advance = buffer_pitch - buffer_draw_width;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  ecx, depthp
                    mov  bl,  global_depth
                    push ebp
                    mov  ebp, _table
                Q4_row:
                    mov  edx, buffer_draw_width
                Q4_pix:
                    cmp  byte ptr [esi], 0feh
                    jnb  Q4_next
                    cmp  byte ptr [ecx], bl
                    jb   Q4_next
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ebp+eax*2]
                    mov  word ptr [edi], ax
                Q4_next:
                    dec  esi
                    add  edi, 2
                    add  ecx, 2
                    dec  edx
                    jnz  Q4_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    add  ecx, z_buffer_advance
                    dec  buffer_draw_height
                    jnz  Q4_row
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            }
        }
    } else {
        src->release_data(2);
        dst->release_data(2);
    }
    dst->release_data(2);
    src->release_data(2);
    return 0;
}
int JGLSprite::draw_mono_8_16(JGLBuffer *dst, int x, int y, int colour)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    if (colour >> 31) {
        global_mono_color = (ushort)colour;
    } else {
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
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  ax, global_mono_color
                row1:
                    mov  edx, buffer_draw_width
                pix1:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt1
                    mov  word ptr [edi], ax
                nxt1:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  pix1
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  row1
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  ax, global_mono_color
                row2:
                    mov  edx, buffer_draw_width
                pix2:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt2
                    mov  word ptr [edi], ax
                nxt2:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  pix2
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  row2
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  ax, global_mono_color
                row3:
                    mov  edx, buffer_draw_width
                pix3:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt3
                    mov  word ptr [edi], ax
                nxt3:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  pix3
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  row3
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  ax, global_mono_color
                row4:
                    mov  edx, buffer_draw_width
                pix4:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt4
                    mov  word ptr [edi], ax
                nxt4:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  pix4
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  row4
                    pop  edi
                    pop  esi
                }
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr [scale_x]
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr [scale_x_inc]
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc+2]
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    mov  ax, global_mono_color
                srow5:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix5:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt5
                    mov  ax, global_mono_color
                    mov  word ptr [edi], ax
                    xor  eax, eax
                snxt5:
                    add  dx, bx
                    adc  esi, ecx
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  spix5
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  srow5
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (int)(scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr [scale_x]
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr [scale_x_inc]
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc+2]
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    mov  ax, global_mono_color
                srow6:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix6:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt6
                    mov  ax, global_mono_color
                    mov  word ptr [edi], ax
                    xor  eax, eax
                snxt6:
                    add  dx, bx
                    adc  esi, ecx
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  spix6
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  srow6
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr [scale_x]
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr [scale_x_inc]
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc+2]
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    mov  ax, global_mono_color
                srow7:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix7:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt7
                    mov  ax, global_mono_color
                    mov  word ptr [edi], ax
                    xor  eax, eax
                snxt7:
                    add  dx, bx
                    sbb  esi, ecx
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  spix7
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  srow7
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (int)(scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr [scale_x]
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr [scale_x_inc]
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc+2]
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    mov  ax, global_mono_color
                srow8:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix8:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt8
                    mov  ax, global_mono_color
                    mov  word ptr [edi], ax
                    xor  eax, eax
                snxt8:
                    add  dx, bx
                    sbb  esi, ecx
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  spix8
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  srow8
                    pop  edi
                    pop  esi
                }
            }
        }
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::draw_dest_8_16(JGLBuffer *dst, int x, int y, uchar *remap, ushort *tint)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    global_table = (ushort *)remap;
    global_shadow_table = tint;
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
        if (!IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip())) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ebp
                    mov  ecx, _table
                C1_row:
                    mov  edx, buffer_draw_width
                C1_pix:
                    cmp  byte ptr [esi], 0feh
                    jnc  C1_next
                    cmp  byte ptr [esi], 0f8h
                    jnz  C1_opaque
                    mov  ebp, global_shadow_table
                    mov  bx, word ptr [edi]
                    mov  bx, word ptr [ebp + ebx*2]
                    mov  word ptr [edi], bx
                    jmp  C1_next
                C1_opaque:
                    mov  ebp, global_table
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp + eax]
                    mov  bx, word ptr [ecx + eax*2]
                    mov  word ptr [edi], bx
                C1_next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  C1_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C1_row
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ebp
                    mov  ecx, _table
                C2_row:
                    mov  edx, buffer_draw_width
                C2_pix:
                    cmp  byte ptr [esi], 0feh
                    jnc  C2_next
                    cmp  byte ptr [esi], 0f8h
                    jnz  C2_opaque
                    mov  ebp, global_shadow_table
                    mov  bx, word ptr [edi]
                    mov  bx, word ptr [ebp + ebx*2]
                    mov  word ptr [edi], bx
                    jmp  C2_next
                C2_opaque:
                    mov  ebp, global_table
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp + eax]
                    mov  bx, word ptr [ecx + eax*2]
                    mov  word ptr [edi], bx
                C2_next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  C2_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C2_row
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ebp
                    mov  ecx, _table
                C3_row:
                    mov  edx, buffer_draw_width
                C3_pix:
                    cmp  byte ptr [esi], 0feh
                    jnc  C3_next
                    cmp  byte ptr [esi], 0f8h
                    jnz  C3_opaque
                    mov  ebp, global_shadow_table
                    mov  bx, word ptr [edi]
                    mov  bx, word ptr [ebp + ebx*2]
                    mov  word ptr [edi], bx
                    jmp  C3_next
                C3_opaque:
                    mov  ebp, global_table
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp + eax]
                    mov  bx, word ptr [ecx + eax*2]
                    mov  word ptr [edi], bx
                C3_next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  C3_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C3_row
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ebp
                    mov  ecx, _table
                C4_row:
                    mov  edx, buffer_draw_width
                C4_pix:
                    cmp  byte ptr [esi], 0feh
                    jnc  C4_next
                    cmp  byte ptr [esi], 0f8h
                    jnz  C4_opaque
                    mov  ebp, global_shadow_table
                    mov  bx, word ptr [edi]
                    mov  bx, word ptr [ebp + ebx*2]
                    mov  word ptr [edi], bx
                    jmp  C4_next
                C4_opaque:
                    mov  ebp, global_table
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp + eax]
                    mov  bx, word ptr [ecx + eax*2]
                    mov  word ptr [edi], bx
                C4_next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  C4_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C4_row
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
        saved = r;
        if (!jgl_IntersectRect(&r, &r, dst->get_clip())) {
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ebp
                    mov  ecx, _table
                C5_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C5_pix:
                    cmp  byte ptr [esi], 0feh
                    jnc  C5_next
                    cmp  byte ptr [esi], 0f8h
                    jnz  C5_opaque
                    mov  ebp, global_shadow_table
                    mov  ax, word ptr [edi]
                    mov  ax, word ptr [ebp + eax*2]
                    mov  word ptr [edi], ax
                    xor  eax, eax
                    jmp  C5_next
                C5_opaque:
                    mov  ebp, global_table
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp + eax]
                    mov  bp, word ptr [ecx + eax*2]
                    mov  word ptr [edi], bp
                C5_next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C5_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C5_row
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ebp
                    mov  ecx, _table
                C6_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C6_pix:
                    cmp  byte ptr [esi], 0feh
                    jnc  C6_next
                    cmp  byte ptr [esi], 0f8h
                    jnz  C6_opaque
                    mov  ebp, global_shadow_table
                    mov  ax, word ptr [edi]
                    mov  ax, word ptr [ebp + eax*2]
                    mov  word ptr [edi], ax
                    xor  eax, eax
                    jmp  C6_next
                C6_opaque:
                    mov  ebp, global_table
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp + eax]
                    mov  bp, word ptr [ecx + eax*2]
                    mov  word ptr [edi], bp
                C6_next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C6_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C6_row
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ebp
                    mov  ecx, _table
                C7_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C7_pix:
                    cmp  byte ptr [esi], 0feh
                    jnc  C7_next
                    cmp  byte ptr [esi], 0f8h
                    jnz  C7_opaque
                    mov  ebp, global_shadow_table
                    mov  ax, word ptr [edi]
                    mov  ax, word ptr [ebp + eax*2]
                    mov  word ptr [edi], ax
                    xor  eax, eax
                    jmp  C7_next
                C7_opaque:
                    mov  ebp, global_table
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp + eax]
                    mov  bp, word ptr [ecx + eax*2]
                    mov  word ptr [edi], bp
                C7_next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C7_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C7_row
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ebp
                    mov  ecx, _table
                C8_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C8_pix:
                    cmp  byte ptr [esi], 0feh
                    jnc  C8_next
                    cmp  byte ptr [esi], 0f8h
                    jnz  C8_opaque
                    mov  ebp, global_shadow_table
                    mov  ax, word ptr [edi]
                    mov  ax, word ptr [ebp + eax*2]
                    mov  word ptr [edi], ax
                    xor  eax, eax
                    jmp  C8_next
                C8_opaque:
                    mov  ebp, global_table
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp + eax]
                    mov  bp, word ptr [ecx + eax*2]
                    mov  word ptr [edi], bp
                C8_next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C8_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C8_row
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            }
        }
    }
    dst->release_data(2);
    return 0;
}
#define BLIT_CORE_UNSCALED(Lrow, Lpix, Lother, Lnext, XSTEPOP, ROWOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm xor   eax, eax                            \
    __asm xor   ebx, ebx                            \
    __asm xor   ecx, ecx                            \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm push  ebp                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm mov   edx, buffer_draw_width                    \
    __asm Lpix:                                     \
    __asm cmp   byte ptr [esi], 0feh                \
    __asm jnc   Lnext                               \
    __asm cmp   byte ptr [esi], 0f8h                \
    __asm jnz   Lother                              \
    __asm mov   ebp, global_shadow_table                 \
    __asm mov   bx, word ptr [edi]                  \
    __asm mov   bx, word ptr [ebp + ebx*2]          \
    __asm mov   word ptr [edi], bx                  \
    __asm jmp   Lnext                               \
    __asm Lother:                                   \
    __asm mov   ebp, global_table                \
    __asm mov   al, byte ptr [esi]                  \
    __asm mov   bx, word ptr [ecx + eax*2]          \
    __asm mov   bx, word ptr [ebp + ebx*2]          \
    __asm mov   word ptr [edi], bx                  \
    __asm Lnext:                                    \
    __asm XSTEPOP esi                               \
    __asm add   edi, 2                              \
    __asm dec   edx                                 \
    __asm jnz   Lpix                                \
    __asm ROWOP esi, sprite_advance               \
    __asm add   edi, buffer_advance          \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ebp                                 \
    __asm pop   edi                                 \
    __asm pop   esi
#define BLIT_CORE_SCALED(Lrow, Lpix, Lother, Lnext, CARRYOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm mov   edx, scale_y                \
    __asm shl   edx, 10h                            \
    __asm mov   dx, word ptr scale_x        \
    __asm mov   ebx, buffer_draw_height                   \
    __asm shl   ebx, 10h                            \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   ecx, buffer_draw_width                    \
    __asm dec   ecx                                 \
    __asm shl   ecx, 10h                            \
    __asm mov   buffer_draw_width, ecx                    \
    __asm xor   ecx, ecx                            \
    __asm mov   cx, word ptr [scale_x_inc+2]   \
    __asm mov   x_inc_int, ecx               \
    __asm mov   eax, scale_y_inc               \
    __asm shl   eax, 10h                            \
    __asm mov   frac_scale_y_inc, eax             \
    __asm push  ebp                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm xor   eax, eax                            \
    __asm mov   ebx, buffer_draw_width                    \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   dx, word ptr scale_x        \
    __asm Lpix:                                     \
    __asm cmp   byte ptr [esi], 0feh                \
    __asm jnc   Lnext                               \
    __asm cmp   byte ptr [esi], 0f8h                \
    __asm jnz   Lother                              \
    __asm mov   ebp, global_shadow_table                 \
    __asm mov   ax, word ptr [edi]                  \
    __asm mov   ax, word ptr [ebp + eax*2]          \
    __asm mov   word ptr [edi], ax                  \
    __asm jmp   Lnext                               \
    __asm Lother:                                   \
    __asm xor   eax, eax                            \
    __asm mov   ebp, global_table                \
    __asm mov   al, byte ptr [esi]                  \
    __asm mov   ax, word ptr [ebp + eax*2]          \
    __asm mov   bp, word ptr [ecx + eax*2]          \
    __asm mov   word ptr [edi], bp                  \
    __asm Lnext:                                    \
    __asm add   dx, bx                              \
    __asm CARRYOP esi, x_inc_int             \
    __asm add   edi, 2                              \
    __asm sub   ebx, 10000h                         \
    __asm jns   Lpix                                \
    __asm add   edi, buffer_advance          \
    __asm add   edx, frac_scale_y_inc             \
    __asm sbb   eax, eax                            \
    __asm mov   ebx, y_advance_no_carry        \
    __asm add   esi, dword ptr [ebx + eax*4]        \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ebp                                 \
    __asm pop   edi                                 \
    __asm pop   esi
int JGLSprite::Complex(JGLBuffer *dst, int x, int y, ushort *shadow, ushort *blend)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    global_table = shadow;
    global_shadow_table = blend;
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
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u1row, u1pix, u1bl, u1nx, inc, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u2row, u2pix, u2bl, u2nx, inc, sub)
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u3row, u3pix, u3bl, u3nx, dec, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u4row, u4pix, u4bl, u4nx, dec, sub)
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s1row, s1pix, s1bl, s1nx, adc)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s2row, s2pix, s2bl, s2nx, adc)
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s3row, s3pix, s3bl, s3nx, sbb)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s4row, s4pix, s4bl, s4nx, sbb)
            }
        }
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::draw_tinted_8_16(JGLBuffer *dst, int x, int y, ushort tint, uchar skipFrom)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    push ebp
                    mov  bp, word ptr tint
                    shl  ebp, 10h
                    mov  ecx, _table
                Arow:
                    mov  edx, buffer_draw_width
                Apix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Anext
                    cmp  byte ptr [esi], 0f8h
                    jnc  Anext
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  bp, word ptr [ecx+eax*2]
                    push ecx
                    xor  eax, eax
                    mov  ecx, ebp
                    shr  ecx, 10h
                    mov  ax, bp
                    and  cx, 7c00h
                    and  ax, 7c00h
                    shr  cx, 0bh
                    shr  ax, 0bh
                    add  ax, cx
                    shl  ax, 0ah
                    and  bp, 83ffh
                    or   bp, ax
                    mov  ecx, ebp
                    shr  ecx, 10h
                    mov  ax, bp
                    and  cx, 3e0h
                    and  ax, 3e0h
                    shr  cx, 6
                    shr  ax, 6
                    add  ax, cx
                    shl  ax, 5
                    and  bp, 0fc1fh
                    or   bp, ax
                    mov  ecx, ebp
                    shr  ecx, 10h
                    mov  ax, bp
                    and  cx, 1fh
                    and  ax, 1fh
                    shr  cx, 1
                    shr  ax, 1
                    add  ax, cx
                    and  bp, 0ffe0h
                    or   bp, ax
                    mov  word ptr [edi], bp
                    pop  ecx
                Anext:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  Apix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  Arow
                    pop  ebp
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    push ebp
                    mov  bp, word ptr tint
                    shl  ebp, 10h
                    mov  ecx, _table
                Brow:
                    mov  edx, buffer_draw_width
                Bpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Bnext
                    cmp  byte ptr [esi], 0f8h
                    jnc  Bnext
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  bp, word ptr [ecx+eax*2]
                    push ecx
                    xor  eax, eax
                    mov  ecx, ebp
                    shr  ecx, 10h
                    mov  ax, bp
                    and  cx, 7c00h
                    and  ax, 7c00h
                    shr  cx, 0bh
                    shr  ax, 0bh
                    add  ax, cx
                    shl  ax, 0ah
                    and  bp, 83ffh
                    or   bp, ax
                    mov  ecx, ebp
                    shr  ecx, 10h
                    mov  ax, bp
                    and  cx, 3e0h
                    and  ax, 3e0h
                    shr  cx, 6
                    shr  ax, 6
                    add  ax, cx
                    shl  ax, 5
                    and  bp, 0fc1fh
                    or   bp, ax
                    mov  ecx, ebp
                    shr  ecx, 10h
                    mov  ax, bp
                    and  cx, 1fh
                    and  ax, 1fh
                    shr  cx, 1
                    shr  ax, 1
                    add  ax, cx
                    and  bp, 0ffe0h
                    or   bp, ax
                    mov  word ptr [edi], bp
                    pop  ecx
                Bnext:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  Bpix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  Brow
                    pop  ebp
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    push ebp
                    mov  bp, word ptr tint
                    shl  ebp, 10h
                    mov  ecx, _table
                Crow:
                    mov  edx, buffer_draw_width
                Cpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Cnext
                    cmp  byte ptr [esi], 0f8h
                    jnc  Cnext
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  bp, word ptr [ecx+eax*2]
                    push ecx
                    xor  eax, eax
                    mov  ecx, ebp
                    shr  ecx, 10h
                    mov  ax, bp
                    and  cx, 7c00h
                    and  ax, 7c00h
                    shr  cx, 0bh
                    shr  ax, 0bh
                    add  ax, cx
                    shl  ax, 0ah
                    and  bp, 83ffh
                    or   bp, ax
                    mov  ecx, ebp
                    shr  ecx, 10h
                    mov  ax, bp
                    and  cx, 3e0h
                    and  ax, 3e0h
                    shr  cx, 6
                    shr  ax, 6
                    add  ax, cx
                    shl  ax, 5
                    and  bp, 0fc1fh
                    or   bp, ax
                    mov  ecx, ebp
                    shr  ecx, 10h
                    mov  ax, bp
                    and  cx, 1fh
                    and  ax, 1fh
                    shr  cx, 1
                    shr  ax, 1
                    add  ax, cx
                    and  bp, 0ffe0h
                    or   bp, ax
                    mov  word ptr [edi], bp
                    pop  ecx
                Cnext:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  Cpix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  Crow
                    pop  ebp
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    push ebp
                    mov  bp, word ptr tint
                    shl  ebp, 10h
                    mov  ecx, _table
                Drow:
                    mov  edx, buffer_draw_width
                Dpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Dnext
                    cmp  byte ptr [esi], 0f8h
                    jnc  Dnext
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  bp, word ptr [ecx+eax*2]
                    push ecx
                    xor  eax, eax
                    mov  ecx, ebp
                    shr  ecx, 10h
                    mov  ax, bp
                    and  cx, 7c00h
                    and  ax, 7c00h
                    shr  cx, 0bh
                    shr  ax, 0bh
                    add  ax, cx
                    shl  ax, 0ah
                    and  bp, 83ffh
                    or   bp, ax
                    mov  ecx, ebp
                    shr  ecx, 10h
                    mov  ax, bp
                    and  cx, 3e0h
                    and  ax, 3e0h
                    shr  cx, 6
                    shr  ax, 6
                    add  ax, cx
                    shl  ax, 5
                    and  bp, 0fc1fh
                    or   bp, ax
                    mov  ecx, ebp
                    shr  ecx, 10h
                    mov  ax, bp
                    and  cx, 1fh
                    and  ax, 1fh
                    shr  cx, 1
                    shr  ax, 1
                    add  ax, cx
                    and  bp, 0ffe0h
                    or   bp, ax
                    mov  word ptr [edi], bp
                    pop  ecx
                Dnext:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  Dpix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  Drow
                    pop  ebp
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    push ebp
                    mov  bp, word ptr tint
                    shl  ebp, 10h
                    mov  ecx, _table
                Erow:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                Epix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Enext
                    cmp  byte ptr [esi], 0f8h
                    jnc  Enext
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  bp, word ptr [ecx+eax*2]
                    push ecx
                    xor  eax, eax
                    mov  ecx, ebp
                    shr  ecx, 10h
                    mov  ax, bp
                    and  cx, 7c00h
                    and  ax, 7c00h
                    shr  cx, 0bh
                    shr  ax, 0bh
                    add  ax, cx
                    shl  ax, 0ah
                    and  bp, 83ffh
                    or   bp, ax
                    mov  ecx, ebp
                    shr  ecx, 10h
                    mov  ax, bp
                    and  cx, 3e0h
                    and  ax, 3e0h
                    shr  cx, 6
                    shr  ax, 6
                    add  ax, cx
                    shl  ax, 5
                    and  bp, 0fc1fh
                    or   bp, ax
                    mov  ecx, ebp
                    shr  ecx, 10h
                    mov  ax, bp
                    and  cx, 1fh
                    and  ax, 1fh
                    shr  cx, 1
                    shr  ax, 1
                    add  ax, cx
                    and  bp, 0ffe0h
                    or   bp, ax
                    mov  word ptr [edi], bp
                    pop  ecx
                Enext:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  Epix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  Erow
                    pop  ebp
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    push ebp
                    mov  bp, word ptr tint
                    shl  ebp, 10h
                    mov  ecx, _table
                Frow:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                Fpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Fnext
                    cmp  byte ptr [esi], 0f8h
                    jnc  Fnext
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  bp, word ptr [ecx+eax*2]
                    push ecx
                    xor  eax, eax
                    mov  ecx, ebp
                    shr  ecx, 10h
                    mov  ax, bp
                    and  cx, 7c00h
                    and  ax, 7c00h
                    shr  cx, 0bh
                    shr  ax, 0bh
                    add  ax, cx
                    shl  ax, 0ah
                    and  bp, 83ffh
                    or   bp, ax
                    mov  ecx, ebp
                    shr  ecx, 10h
                    mov  ax, bp
                    and  cx, 3e0h
                    and  ax, 3e0h
                    shr  cx, 6
                    shr  ax, 6
                    add  ax, cx
                    shl  ax, 5
                    and  bp, 0fc1fh
                    or   bp, ax
                    mov  ecx, ebp
                    shr  ecx, 10h
                    mov  ax, bp
                    and  cx, 1fh
                    and  ax, 1fh
                    shr  cx, 1
                    shr  ax, 1
                    add  ax, cx
                    and  bp, 0ffe0h
                    or   bp, ax
                    mov  word ptr [edi], bp
                    pop  ecx
                Fnext:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  Fpix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  Frow
                    pop  ebp
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    push ebp
                    mov  bp, word ptr tint
                    shl  ebp, 10h
                    mov  ecx, _table
                Grow:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                Gpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Gnext
                    cmp  byte ptr [esi], 0f8h
                    jnc  Gnext
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  bp, word ptr [ecx+eax*2]
                    push ecx
                    xor  eax, eax
                    mov  ecx, ebp
                    shr  ecx, 10h
                    mov  ax, bp
                    and  cx, 7c00h
                    and  ax, 7c00h
                    shr  cx, 0bh
                    shr  ax, 0bh
                    add  ax, cx
                    shl  ax, 0ah
                    and  bp, 83ffh
                    or   bp, ax
                    mov  ecx, ebp
                    shr  ecx, 10h
                    mov  ax, bp
                    and  cx, 3e0h
                    and  ax, 3e0h
                    shr  cx, 6
                    shr  ax, 6
                    add  ax, cx
                    shl  ax, 5
                    and  bp, 0fc1fh
                    or   bp, ax
                    mov  ecx, ebp
                    shr  ecx, 10h
                    mov  ax, bp
                    and  cx, 1fh
                    and  ax, 1fh
                    shr  cx, 1
                    shr  ax, 1
                    add  ax, cx
                    and  bp, 0ffe0h
                    or   bp, ax
                    mov  word ptr [edi], bp
                    pop  ecx
                Gnext:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  Gpix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  Grow
                    pop  ebp
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    push ebp
                    mov  bp, word ptr tint
                    shl  ebp, 10h
                    mov  ecx, _table
                Hrow:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                Hpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Hnext
                    cmp  byte ptr [esi], 0f8h
                    jnc  Hnext
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  bp, word ptr [ecx+eax*2]
                    push ecx
                    xor  eax, eax
                    mov  ecx, ebp
                    shr  ecx, 10h
                    mov  ax, bp
                    and  cx, 7c00h
                    and  ax, 7c00h
                    shr  cx, 0bh
                    shr  ax, 0bh
                    add  ax, cx
                    shl  ax, 0ah
                    and  bp, 83ffh
                    or   bp, ax
                    mov  ecx, ebp
                    shr  ecx, 10h
                    mov  ax, bp
                    and  cx, 3e0h
                    and  ax, 3e0h
                    shr  cx, 6
                    shr  ax, 6
                    add  ax, cx
                    shl  ax, 5
                    and  bp, 0fc1fh
                    or   bp, ax
                    mov  ecx, ebp
                    shr  ecx, 10h
                    mov  ax, bp
                    and  cx, 1fh
                    and  ax, 1fh
                    shr  cx, 1
                    shr  ax, 1
                    add  ax, cx
                    and  bp, 0ffe0h
                    or   bp, ax
                    mov  word ptr [edi], bp
                    pop  ecx
                Hnext:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  Hpix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  Hrow
                    pop  ebp
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::draw_color_scaled_8_16(JGLBuffer *dst, int x, int y, ushort tint, uchar skipFrom)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    push ebp
                    mov  bp, word ptr tint
                    shl  ebp, 10h
                    mov  ecx, _table
                Arow:
                    mov  edx, buffer_draw_width
                Apix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Anext
                    cmp  byte ptr [esi], 0f8h
                    jnc  Anext
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  bp, word ptr [ecx+eax*2]
                    push ecx
                    push edx
                    xor  edx, edx
                    xor  ecx, ecx
                    mov  ax, bp
                    and  ax, 7c00h
                    shr  ax, 7
                    add  dx, ax
                    mov  ax, bp
                    and  ax, 3e0h
                    shr  ax, 1
                    add  dx, ax
                    mov  ax, bp
                    and  ax, 1fh
                    shl  ax, 3
                    add  dx, ax
                    shr  dx, 2
                    shr  ebp, 10h
                    mov  ax, bp
                    and  ax, 1fh
                    mul  dl
                    shr  ax, 8
                    or   cx, ax
                    mov  ax, bp
                    and  ax, 3e0h
                    shr  ax, 5
                    mul  dl
                    shr  ax, 8
                    shl  ax, 5
                    or   cx, ax
                    mov  ax, bp
                    and  ax, 7c00h
                    shr  ax, 0ah
                    mul  dl
                    shr  ax, 8
                    shl  ax, 0ah
                    or   cx, ax
                    shl  ebp, 10h
                    mov  bp, cx
                    mov  word ptr [edi], bp
                    pop  edx
                    pop  ecx
                Anext:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  Apix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  Arow
                    pop  ebp
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    push ebp
                    mov  bp, word ptr tint
                    shl  ebp, 10h
                    mov  ecx, _table
                Brow:
                    mov  edx, buffer_draw_width
                Bpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Bnext
                    cmp  byte ptr [esi], 0f8h
                    jnc  Bnext
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  bp, word ptr [ecx+eax*2]
                    push ecx
                    push edx
                    xor  edx, edx
                    xor  ecx, ecx
                    mov  ax, bp
                    and  ax, 7c00h
                    shr  ax, 7
                    add  dx, ax
                    mov  ax, bp
                    and  ax, 3e0h
                    shr  ax, 1
                    add  dx, ax
                    mov  ax, bp
                    and  ax, 1fh
                    shl  ax, 3
                    add  dx, ax
                    shr  dx, 2
                    shr  ebp, 10h
                    mov  ax, bp
                    and  ax, 1fh
                    mul  dl
                    shr  ax, 8
                    or   cx, ax
                    mov  ax, bp
                    and  ax, 3e0h
                    shr  ax, 5
                    mul  dl
                    shr  ax, 8
                    shl  ax, 5
                    or   cx, ax
                    mov  ax, bp
                    and  ax, 7c00h
                    shr  ax, 0ah
                    mul  dl
                    shr  ax, 8
                    shl  ax, 0ah
                    or   cx, ax
                    shl  ebp, 10h
                    mov  bp, cx
                    mov  word ptr [edi], bp
                    pop  edx
                    pop  ecx
                Bnext:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  Bpix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  Brow
                    pop  ebp
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    push ebp
                    mov  bp, word ptr tint
                    shl  ebp, 10h
                    mov  ecx, _table
                Crow:
                    mov  edx, buffer_draw_width
                Cpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Cnext
                    cmp  byte ptr [esi], 0f8h
                    jnc  Cnext
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  bp, word ptr [ecx+eax*2]
                    push ecx
                    push edx
                    xor  edx, edx
                    xor  ecx, ecx
                    mov  ax, bp
                    and  ax, 7c00h
                    shr  ax, 7
                    add  dx, ax
                    mov  ax, bp
                    and  ax, 3e0h
                    shr  ax, 1
                    add  dx, ax
                    mov  ax, bp
                    and  ax, 1fh
                    shl  ax, 3
                    add  dx, ax
                    shr  dx, 2
                    shr  ebp, 10h
                    mov  ax, bp
                    and  ax, 1fh
                    mul  dl
                    shr  ax, 8
                    or   cx, ax
                    mov  ax, bp
                    and  ax, 3e0h
                    shr  ax, 5
                    mul  dl
                    shr  ax, 8
                    shl  ax, 5
                    or   cx, ax
                    mov  ax, bp
                    and  ax, 7c00h
                    shr  ax, 0ah
                    mul  dl
                    shr  ax, 8
                    shl  ax, 0ah
                    or   cx, ax
                    shl  ebp, 10h
                    mov  bp, cx
                    mov  word ptr [edi], bp
                    pop  edx
                    pop  ecx
                Cnext:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  Cpix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  Crow
                    pop  ebp
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    push ebp
                    mov  bp, word ptr tint
                    shl  ebp, 10h
                    mov  ecx, _table
                Drow:
                    mov  edx, buffer_draw_width
                Dpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Dnext
                    cmp  byte ptr [esi], 0f8h
                    jnc  Dnext
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  bp, word ptr [ecx+eax*2]
                    push ecx
                    push edx
                    xor  edx, edx
                    xor  ecx, ecx
                    mov  ax, bp
                    and  ax, 7c00h
                    shr  ax, 7
                    add  dx, ax
                    mov  ax, bp
                    and  ax, 3e0h
                    shr  ax, 1
                    add  dx, ax
                    mov  ax, bp
                    and  ax, 1fh
                    shl  ax, 3
                    add  dx, ax
                    shr  dx, 2
                    shr  ebp, 10h
                    mov  ax, bp
                    and  ax, 1fh
                    mul  dl
                    shr  ax, 8
                    or   cx, ax
                    mov  ax, bp
                    and  ax, 3e0h
                    shr  ax, 5
                    mul  dl
                    shr  ax, 8
                    shl  ax, 5
                    or   cx, ax
                    mov  ax, bp
                    and  ax, 7c00h
                    shr  ax, 0ah
                    mul  dl
                    shr  ax, 8
                    shl  ax, 0ah
                    or   cx, ax
                    shl  ebp, 10h
                    mov  bp, cx
                    mov  word ptr [edi], bp
                    pop  edx
                    pop  ecx
                Dnext:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  Dpix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  Drow
                    pop  ebp
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    push ebp
                    mov  bp, word ptr tint
                    shl  ebp, 10h
                    mov  ecx, _table
                Erow:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                Epix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Enext
                    cmp  byte ptr [esi], 0f8h
                    jnc  Enext
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  bp, word ptr [ecx+eax*2]
                    push ecx
                    push edx
                    xor  edx, edx
                    xor  ecx, ecx
                    mov  ax, bp
                    and  ax, 7c00h
                    shr  ax, 7
                    add  dx, ax
                    mov  ax, bp
                    and  ax, 3e0h
                    shr  ax, 1
                    add  dx, ax
                    mov  ax, bp
                    and  ax, 1fh
                    shl  ax, 3
                    add  dx, ax
                    shr  dx, 2
                    shr  ebp, 10h
                    mov  ax, bp
                    and  ax, 1fh
                    mul  dl
                    shr  ax, 8
                    or   cx, ax
                    mov  ax, bp
                    and  ax, 3e0h
                    shr  ax, 5
                    mul  dl
                    shr  ax, 8
                    shl  ax, 5
                    or   cx, ax
                    mov  ax, bp
                    and  ax, 7c00h
                    shr  ax, 0ah
                    mul  dl
                    shr  ax, 8
                    shl  ax, 0ah
                    or   cx, ax
                    shl  ebp, 10h
                    mov  bp, cx
                    mov  word ptr [edi], bp
                    pop  edx
                    pop  ecx
                Enext:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  Epix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  Erow
                    pop  ebp
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    push ebp
                    mov  bp, word ptr tint
                    shl  ebp, 10h
                    mov  ecx, _table
                Frow:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                Fpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Fnext
                    cmp  byte ptr [esi], 0f8h
                    jnc  Fnext
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  bp, word ptr [ecx+eax*2]
                    push ecx
                    push edx
                    xor  edx, edx
                    xor  ecx, ecx
                    mov  ax, bp
                    and  ax, 7c00h
                    shr  ax, 7
                    add  dx, ax
                    mov  ax, bp
                    and  ax, 3e0h
                    shr  ax, 1
                    add  dx, ax
                    mov  ax, bp
                    and  ax, 1fh
                    shl  ax, 3
                    add  dx, ax
                    shr  dx, 2
                    shr  ebp, 10h
                    mov  ax, bp
                    and  ax, 1fh
                    mul  dl
                    shr  ax, 8
                    or   cx, ax
                    mov  ax, bp
                    and  ax, 3e0h
                    shr  ax, 5
                    mul  dl
                    shr  ax, 8
                    shl  ax, 5
                    or   cx, ax
                    mov  ax, bp
                    and  ax, 7c00h
                    shr  ax, 0ah
                    mul  dl
                    shr  ax, 8
                    shl  ax, 0ah
                    or   cx, ax
                    shl  ebp, 10h
                    mov  bp, cx
                    mov  word ptr [edi], bp
                    pop  edx
                    pop  ecx
                Fnext:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  Fpix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  Frow
                    pop  ebp
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    push ebp
                    mov  bp, word ptr tint
                    shl  ebp, 10h
                    mov  ecx, _table
                Grow:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                Gpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Gnext
                    cmp  byte ptr [esi], 0f8h
                    jnc  Gnext
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  bp, word ptr [ecx+eax*2]
                    push ecx
                    push edx
                    xor  edx, edx
                    xor  ecx, ecx
                    mov  ax, bp
                    and  ax, 7c00h
                    shr  ax, 7
                    add  dx, ax
                    mov  ax, bp
                    and  ax, 3e0h
                    shr  ax, 1
                    add  dx, ax
                    mov  ax, bp
                    and  ax, 1fh
                    shl  ax, 3
                    add  dx, ax
                    shr  dx, 2
                    shr  ebp, 10h
                    mov  ax, bp
                    and  ax, 1fh
                    mul  dl
                    shr  ax, 8
                    or   cx, ax
                    mov  ax, bp
                    and  ax, 3e0h
                    shr  ax, 5
                    mul  dl
                    shr  ax, 8
                    shl  ax, 5
                    or   cx, ax
                    mov  ax, bp
                    and  ax, 7c00h
                    shr  ax, 0ah
                    mul  dl
                    shr  ax, 8
                    shl  ax, 0ah
                    or   cx, ax
                    shl  ebp, 10h
                    mov  bp, cx
                    mov  word ptr [edi], bp
                    pop  edx
                    pop  ecx
                Gnext:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  Gpix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  Grow
                    pop  ebp
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    push ebp
                    mov  bp, word ptr tint
                    shl  ebp, 10h
                    mov  ecx, _table
                Hrow:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                Hpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Hnext
                    cmp  byte ptr [esi], 0f8h
                    jnc  Hnext
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  bp, word ptr [ecx+eax*2]
                    push ecx
                    push edx
                    xor  edx, edx
                    xor  ecx, ecx
                    mov  ax, bp
                    and  ax, 7c00h
                    shr  ax, 7
                    add  dx, ax
                    mov  ax, bp
                    and  ax, 3e0h
                    shr  ax, 1
                    add  dx, ax
                    mov  ax, bp
                    and  ax, 1fh
                    shl  ax, 3
                    add  dx, ax
                    shr  dx, 2
                    shr  ebp, 10h
                    mov  ax, bp
                    and  ax, 1fh
                    mul  dl
                    shr  ax, 8
                    or   cx, ax
                    mov  ax, bp
                    and  ax, 3e0h
                    shr  ax, 5
                    mul  dl
                    shr  ax, 8
                    shl  ax, 5
                    or   cx, ax
                    mov  ax, bp
                    and  ax, 7c00h
                    shr  ax, 0ah
                    mul  dl
                    shr  ax, 8
                    shl  ax, 0ah
                    or   cx, ax
                    shl  ebp, 10h
                    mov  bp, cx
                    mov  word ptr [edi], bp
                    pop  edx
                    pop  ecx
                Hnext:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  Hpix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  Hrow
                    pop  ebp
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::draw_antialiased_8_16(JGLSprite *alpha, JGLBuffer *dst, JGLBuffer *target, int x, int y)
{
    Rect        r;
    Rect        saved;
    ushort     *readp;
    ushort     *dstw;
    uchar      *srcp;
    uchar      *alphap;
    JGLPalette *pal;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    jgl::SetRect(&r, x, y, this->m_width, this->m_height);
    saved = r;
    if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
        dst->release_data(1);
        return 0;
    }
    srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
    alphap = alpha->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
    readp = dst->get_data_16() + r.left + r.top * buffer_pitch;
    dstw = target->get_data_16() + r.left + r.top * buffer_pitch;
    buffer_draw_height = r.bottom - r.top;
    buffer_draw_width = r.right - r.left;
    sprite_advance = sprite_pitch - buffer_draw_width;
    buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
    __asm {
        push esi
        push edi
        xor  eax, eax
        xor  ebx, ebx
        xor  ecx, ecx
        mov  esi, srcp
        mov  ebx, alphap
        mov  edi, readp
        push ecx
        push ebp
        mov  ecx, _table
    L_row:
        mov  edx, buffer_draw_width
    L_pix:
        cmp  byte ptr [esi], 0ffh
        jnc  L_next
        cmp  byte ptr [ebx], 0ffh
        jnc  L_next
        xor  eax, eax
        xor  ebp, ebp
        mov  al, byte ptr [ebx]
        cmp  al, 0
        jz   L_opaque
        mov  al, byte ptr [esi]
        mov  bp, word ptr [ecx + eax*2]
        mov  ax, word ptr [edi]
        push ecx
        push edx
        mov  dx, bp
        shl  edx, 10h
        mov  cx, ax
        shl  ecx, 10h
        mov  cl, byte ptr [ebx]
        shr  bp, 7
        and  bp, 0f8h
        shr  ax, 7
        and  al, 0f8h
        mul  cl
        shr  ax, 8
        add  ax, bp
        shr  ax, 3
        shl  ax, 0ah
        or   dx, ax
        mov  eax, ecx
        shr  eax, 10h
        mov  ebp, edx
        shr  ebp, 10h
        shr  bp, 2
        and  bp, 0f8h
        shr  ax, 2
        and  ax, 0f8h
        mul  cl
        shr  ax, 8
        add  ax, bp
        shr  ax, 3
        shl  ax, 5
        or   dx, ax
        mov  eax, ecx
        shr  eax, 10h
        mov  ebp, edx
        shr  ebp, 10h
        shl  bp, 3
        and  bp, 0f8h
        shl  ax, 3
        and  ax, 0f8h
        mul  cl
        shr  ax, 8
        add  ax, bp
        shr  ax, 3
        or   dx, ax
        mov  ax, dx
        pop  edx
        pop  ecx
        pop  ebp
        push edi
        mov  edi, dstw
        mov  word ptr [edi], ax
        pop  edi
        push ebp
        jmp  L_next
    L_opaque:
        mov  al, byte ptr [esi]
        mov  ax, word ptr [ecx + eax*2]
        pop  ebp
        push edi
        mov  edi, dstw
        mov  word ptr [edi], ax
        pop  edi
        push ebp
    L_next:
        inc  esi
        inc  ebx
        add  edi, 2
        pop  ebp
        mov  eax, dstw
        add  eax, 2
        mov  dstw, eax
        push ebp
        dec  edx
        jnz  L_pix
        add  esi, sprite_advance
        add  ebx, sprite_advance
        add  edi, buffer_advance
        pop  ebp
        mov  eax, dstw
        add  eax, buffer_advance
        mov  dstw, eax
        push ebp
        dec  buffer_draw_height
        jnz  L_row
        pop  ebp
        pop  ecx
        pop  edi
        pop  esi
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::MaskedBlit8to16(JGLSprite *alpha, JGLBuffer *dst, int x, int y)
{
    Rect        r;
    Rect        saved;
    ushort     *readp;
    uchar      *srcp;
    uchar      *alphap;
    JGLPalette *pal;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    jgl::SetRect(&r, x, y, this->m_width, this->m_height);
    saved = r;
    if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
        dst->release_data(1);
        return 0;
    }
    srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
    alphap = alpha->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
    readp = dst->get_data_16() + r.left + r.top * buffer_pitch;
    buffer_draw_height = r.bottom - r.top;
    buffer_draw_width = r.right - r.left;
    sprite_advance = sprite_pitch - buffer_draw_width;
    buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
    __asm {
        push esi
        push edi
        xor  eax, eax
        xor  ebx, ebx
        xor  ecx, ecx
        mov  esi, srcp
        mov  ebx, alphap
        mov  edi, readp
        push ecx
        push ebp
        mov  ecx, _table
    L_row:
        mov  edx, buffer_draw_width
    L_pix:
        cmp  byte ptr [esi], 0ffh
        jnc  L_next
        cmp  byte ptr [ebx], 0ffh
        jnc  L_next
        xor  eax, eax
        xor  ebp, ebp
        mov  al, byte ptr [ebx]
        cmp  al, 0
        jz   L_opaque
        mov  al, byte ptr [esi]
        mov  bp, word ptr [ecx + eax*2]
        mov  ax, word ptr [edi]
        push ecx
        push edx
        mov  dx, bp
        shl  edx, 10h
        mov  cx, ax
        shl  ecx, 10h
        mov  cl, byte ptr [ebx]
        shr  bp, 7
        and  bp, 0f8h
        shr  ax, 7
        and  al, 0f8h
        mul  cl
        shr  ax, 8
        add  ax, bp
        shr  ax, 3
        shl  ax, 0ah
        or   dx, ax
        mov  eax, ecx
        shr  eax, 10h
        mov  ebp, edx
        shr  ebp, 10h
        shr  bp, 2
        and  bp, 0f8h
        shr  ax, 2
        and  ax, 0f8h
        mul  cl
        shr  ax, 8
        add  ax, bp
        shr  ax, 3
        shl  ax, 5
        or   dx, ax
        mov  eax, ecx
        shr  eax, 10h
        mov  ebp, edx
        shr  ebp, 10h
        shl  bp, 3
        and  bp, 0f8h
        shl  ax, 3
        and  ax, 0f8h
        mul  cl
        shr  ax, 8
        add  ax, bp
        shr  ax, 3
        or   dx, ax
        mov  word ptr [edi], dx
        pop  edx
        pop  ecx
        jmp  L_next
    L_opaque:
        mov  al, byte ptr [esi]
        mov  ax, word ptr [ecx + eax*2]
        mov  word ptr [edi], ax
    L_next:
        inc  esi
        inc  ebx
        add  edi, 2
        dec  edx
        jnz  L_pix
        add  esi, sprite_advance
        add  ebx, sprite_advance
        add  edi, buffer_advance
        dec  buffer_draw_height
        jnz  L_row
        pop  ebp
        pop  ecx
        pop  edi
        pop  esi
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::SourceLutBlit8to16(JGLBuffer *dst, int x, int y, ushort *shadow)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    global_table = shadow;
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
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ebp
                    mov  ebp, global_table
                    mov  ecx, _table
                Arow:
                    mov  edx, buffer_draw_width
                Apix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Anext
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp+eax]
                    mov  bx, word ptr [ecx+eax*2]
                    mov  word ptr [edi], bx
                Anext:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  Apix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  Arow
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ebp
                    mov  ebp, global_table
                    mov  ecx, _table
                Brow:
                    mov  edx, buffer_draw_width
                Bpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Bnext
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp+eax]
                    mov  bx, word ptr [ecx+eax*2]
                    mov  word ptr [edi], bx
                Bnext:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  Bpix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  Brow
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ebp
                    mov  ebp, global_table
                    mov  ecx, _table
                Crow:
                    mov  edx, buffer_draw_width
                Cpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Cnext
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp+eax]
                    mov  bx, word ptr [ecx+eax*2]
                    mov  word ptr [edi], bx
                Cnext:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  Cpix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  Crow
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ebp
                    mov  ebp, global_table
                    mov  ecx, _table
                Drow:
                    mov  edx, buffer_draw_width
                Dpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Dnext
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp+eax]
                    mov  bx, word ptr [ecx+eax*2]
                    mov  word ptr [edi], bx
                Dnext:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  Dpix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  Drow
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ebp
                    mov  ebp, global_table
                    mov  ecx, _table
                Erow:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                Epix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Enext
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp+eax]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                Enext:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  Epix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  Erow
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ebp
                    mov  ebp, global_table
                    mov  ecx, _table
                Frow:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                Fpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Fnext
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp+eax]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                Fnext:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  Fpix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  Frow
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ebp
                    mov  ebp, global_table
                    mov  ecx, _table
                Grow:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                Gpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Gnext
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp+eax]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                Gnext:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  Gpix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  Grow
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ebp
                    mov  ebp, global_table
                    mov  ecx, _table
                Hrow:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                Hpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Hnext
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp+eax]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                Hnext:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  Hpix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  Hrow
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            }
        }
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::blitAlpha8to16_uncompressed(JGLBuffer *dst, int x, int y, ushort *shadow)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    global_table = shadow;
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
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ebp
                    mov  ebp, global_table
                    mov  ecx, _table
                Arow:
                    mov  edx, buffer_draw_width
                Apix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Anext
                    mov  al, byte ptr [esi]
                    mov  bx, word ptr [ecx+eax*2]
                    mov  bx, word ptr [ebp+ebx*2]
                    mov  word ptr [edi], bx
                Anext:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  Apix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  Arow
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ebp
                    mov  ebp, global_table
                    mov  ecx, _table
                Brow:
                    mov  edx, buffer_draw_width
                Bpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Bnext
                    mov  al, byte ptr [esi]
                    mov  bx, word ptr [ecx+eax*2]
                    mov  bx, word ptr [ebp+ebx*2]
                    mov  word ptr [edi], bx
                Bnext:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  Bpix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  Brow
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ebp
                    mov  ebp, global_table
                    mov  ecx, _table
                Crow:
                    mov  edx, buffer_draw_width
                Cpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Cnext
                    mov  al, byte ptr [esi]
                    mov  bx, word ptr [ecx+eax*2]
                    mov  bx, word ptr [ebp+ebx*2]
                    mov  word ptr [edi], bx
                Cnext:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  Cpix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  Crow
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ebp
                    mov  ebp, global_table
                    mov  ecx, _table
                Drow:
                    mov  edx, buffer_draw_width
                Dpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Dnext
                    mov  al, byte ptr [esi]
                    mov  bx, word ptr [ecx+eax*2]
                    mov  bx, word ptr [ebp+ebx*2]
                    mov  word ptr [edi], bx
                Dnext:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  Dpix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  Drow
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ebp
                    mov  ebp, global_table
                    mov  ecx, _table
                Erow:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                Epix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Enext
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  ax, word ptr [ebp+eax*2]
                    mov  word ptr [edi], ax
                Enext:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  Epix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  Erow
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ebp
                    mov  ebp, global_table
                    mov  ecx, _table
                Frow:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                Fpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Fnext
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  ax, word ptr [ebp+eax*2]
                    mov  word ptr [edi], ax
                Fnext:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  Fpix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  Frow
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ebp
                    mov  ebp, global_table
                    mov  ecx, _table
                Grow:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                Gpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Gnext
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  ax, word ptr [ebp+eax*2]
                    mov  word ptr [edi], ax
                Gnext:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  Gpix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  Grow
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ebp
                    mov  ebp, global_table
                    mov  ecx, _table
                Hrow:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                Hpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Hnext
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  ax, word ptr [ebp+eax*2]
                    mov  word ptr [edi], ax
                Hnext:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  Hpix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  Hrow
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            }
        }
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::SPR_DrawSprite_WithShadow(JGLBuffer *dst, int x, int y, ushort *shadow)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    global_table = shadow;
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
        if (!IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip())) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ebp
                    mov  ebp, global_table
                    mov  ecx, _table
                C1_row:
                    mov  edx, buffer_draw_width
                C1_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C1_next
                    cmp  byte ptr [esi], 0f8h
                    jnz  C1_opaque
                    mov  bx, word ptr [edi]
                    mov  bx, word ptr [ebp + ebx*2]
                    mov  word ptr [edi], bx
                    jmp  C1_next
                C1_opaque:
                    mov  al, byte ptr [esi]
                    mov  bx, word ptr [ecx + eax*2]
                    mov  word ptr [edi], bx
                C1_next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  C1_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C1_row
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ebp
                    mov  ebp, global_table
                    mov  ecx, _table
                C2_row:
                    mov  edx, buffer_draw_width
                C2_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C2_next
                    cmp  byte ptr [esi], 0f8h
                    jnz  C2_opaque
                    mov  bx, word ptr [edi]
                    mov  bx, word ptr [ebp + ebx*2]
                    mov  word ptr [edi], bx
                    jmp  C2_next
                C2_opaque:
                    mov  al, byte ptr [esi]
                    mov  bx, word ptr [ecx + eax*2]
                    mov  word ptr [edi], bx
                C2_next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  C2_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C2_row
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ebp
                    mov  ebp, global_table
                    mov  ecx, _table
                C3_row:
                    mov  edx, buffer_draw_width
                C3_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C3_next
                    cmp  byte ptr [esi], 0f8h
                    jnz  C3_opaque
                    mov  bx, word ptr [edi]
                    mov  bx, word ptr [ebp + ebx*2]
                    mov  word ptr [edi], bx
                    jmp  C3_next
                C3_opaque:
                    mov  al, byte ptr [esi]
                    mov  bx, word ptr [ecx + eax*2]
                    mov  word ptr [edi], bx
                C3_next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  C3_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C3_row
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ebp
                    mov  ebp, global_table
                    mov  ecx, _table
                C4_row:
                    mov  edx, buffer_draw_width
                C4_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C4_next
                    cmp  byte ptr [esi], 0f8h
                    jnz  C4_opaque
                    mov  bx, word ptr [edi]
                    mov  bx, word ptr [ebp + ebx*2]
                    mov  word ptr [edi], bx
                    jmp  C4_next
                C4_opaque:
                    mov  al, byte ptr [esi]
                    mov  bx, word ptr [ecx + eax*2]
                    mov  word ptr [edi], bx
                C4_next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  C4_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C4_row
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
        saved = r;
        if (!jgl_IntersectRect(&r, &r, dst->get_clip())) {
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ebp
                    mov  ebp, global_table
                    mov  ecx, _table
                C5_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C5_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C5_next
                    cmp  byte ptr [esi], 0f8h
                    jnz  C5_opaque
                    mov  ax, word ptr [edi]
                    mov  ax, word ptr [ebp + eax*2]
                    mov  word ptr [edi], ax
                    jmp  C5_next
                C5_opaque:
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx + eax*2]
                    mov  word ptr [edi], ax
                C5_next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C5_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C5_row
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ebp
                    mov  ebp, global_table
                    mov  ecx, _table
                C6_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C6_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C6_next
                    cmp  byte ptr [esi], 0f8h
                    jnz  C6_opaque
                    mov  ax, word ptr [edi]
                    mov  ax, word ptr [ebp + eax*2]
                    mov  word ptr [edi], ax
                    jmp  C6_next
                C6_opaque:
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx + eax*2]
                    mov  word ptr [edi], ax
                C6_next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C6_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C6_row
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ebp
                    mov  ebp, global_table
                    mov  ecx, _table
                C7_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C7_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C7_next
                    cmp  byte ptr [esi], 0f8h
                    jnz  C7_opaque
                    mov  ax, word ptr [edi]
                    mov  ax, word ptr [ebp + eax*2]
                    mov  word ptr [edi], ax
                    jmp  C7_next
                C7_opaque:
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx + eax*2]
                    mov  word ptr [edi], ax
                C7_next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C7_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C7_row
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ebp
                    mov  ebp, global_table
                    mov  ecx, _table
                C8_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C8_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C8_next
                    cmp  byte ptr [esi], 0f8h
                    jnz  C8_opaque
                    mov  ax, word ptr [edi]
                    mov  ax, word ptr [ebp + eax*2]
                    mov  word ptr [edi], ax
                    jmp  C8_next
                C8_opaque:
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx + eax*2]
                    mov  word ptr [edi], ax
                C8_next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C8_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C8_row
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            }
        }
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::draw_multi_table_8_16(JGLBuffer *dst, int x, int y, ushort *blend)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    global_shadow_table = blend;
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
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                    push ebp
                    mov  ebp, global_shadow_table
                Arow:
                    mov  edx, buffer_draw_width
                Apix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Anext
                    xor  eax, eax
                    cmp  byte ptr [esi], 0f8h
                    jnc  Ablend
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                    jmp  Anext
                Ablend:
                    mov  al, byte ptr [esi]
                    sub  al, 0f8h
                    shl  eax, 0fh
                    or   ax, word ptr [edi]
                    mov  ax, word ptr [ebp+eax*2]
                    mov  word ptr [edi], ax
                Anext:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  Apix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  Arow
                    pop  ebp
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                    push ebp
                    mov  ebp, global_shadow_table
                Brow:
                    mov  edx, buffer_draw_width
                Bpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Bnext
                    xor  eax, eax
                    cmp  byte ptr [esi], 0f8h
                    jnc  Bblend
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                    jmp  Bnext
                Bblend:
                    mov  al, byte ptr [esi]
                    sub  al, 0f8h
                    shl  eax, 0fh
                    or   ax, word ptr [edi]
                    mov  ax, word ptr [ebp+eax*2]
                    mov  word ptr [edi], ax
                Bnext:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  Bpix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  Brow
                    pop  ebp
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                    push ebp
                    mov  ebp, global_shadow_table
                Crow:
                    mov  edx, buffer_draw_width
                Cpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Cnext
                    xor  eax, eax
                    cmp  byte ptr [esi], 0f8h
                    jnc  Cblend
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                    jmp  Cnext
                Cblend:
                    mov  al, byte ptr [esi]
                    sub  al, 0f8h
                    shl  eax, 0fh
                    or   ax, word ptr [edi]
                    mov  ax, word ptr [ebp+eax*2]
                    mov  word ptr [edi], ax
                Cnext:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  Cpix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  Crow
                    pop  ebp
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                    push ebp
                    mov  ebp, global_shadow_table
                Drow:
                    mov  edx, buffer_draw_width
                Dpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Dnext
                    xor  eax, eax
                    cmp  byte ptr [esi], 0f8h
                    jnc  Dblend
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                    jmp  Dnext
                Dblend:
                    mov  al, byte ptr [esi]
                    sub  al, 0f8h
                    shl  eax, 0fh
                    or   ax, word ptr [edi]
                    mov  ax, word ptr [ebp+eax*2]
                    mov  word ptr [edi], ax
                Dnext:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  Dpix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  Drow
                    pop  ebp
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                    push ebp
                    mov  ebp, global_shadow_table
                Erow:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                Epix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Enext
                    xor  eax, eax
                    cmp  byte ptr [esi], 0f8h
                    jnc  Eblend
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                    jmp  Enext
                Eblend:
                    mov  al, byte ptr [esi]
                    sub  al, 0f8h
                    shl  eax, 0fh
                    or   ax, word ptr [edi]
                    mov  ax, word ptr [ebp+eax*2]
                    mov  word ptr [edi], ax
                Enext:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  Epix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  Erow
                    pop  ebp
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                    push ebp
                    mov  ebp, global_shadow_table
                Frow:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                Fpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Fnext
                    xor  eax, eax
                    cmp  byte ptr [esi], 0f8h
                    jnc  Fblend
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                    jmp  Fnext
                Fblend:
                    mov  al, byte ptr [esi]
                    sub  al, 0f8h
                    shl  eax, 0fh
                    or   ax, word ptr [edi]
                    mov  ax, word ptr [ebp+eax*2]
                    mov  word ptr [edi], ax
                Fnext:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  Fpix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  Frow
                    pop  ebp
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                    push ebp
                    mov  ebp, global_shadow_table
                Grow:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                Gpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Gnext
                    xor  eax, eax
                    cmp  byte ptr [esi], 0f8h
                    jnc  Gblend
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                    jmp  Gnext
                Gblend:
                    mov  al, byte ptr [esi]
                    sub  al, 0f8h
                    shl  eax, 0fh
                    or   ax, word ptr [edi]
                    mov  ax, word ptr [ebp+eax*2]
                    mov  word ptr [edi], ax
                Gnext:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  Gpix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  Grow
                    pop  ebp
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                    push ebp
                    mov  ebp, global_shadow_table
                Hrow:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                Hpix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  Hnext
                    xor  eax, eax
                    cmp  byte ptr [esi], 0f8h
                    jnc  Hblend
                    mov  al, byte ptr [esi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                    jmp  Hnext
                Hblend:
                    mov  al, byte ptr [esi]
                    sub  al, 0f8h
                    shl  eax, 0fh
                    or   ax, word ptr [edi]
                    mov  ax, word ptr [ebp+eax*2]
                    mov  word ptr [edi], ax
                Hnext:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  Hpix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  Hrow
                    pop  ebp
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::draw_multi_table_dest_8_16(JGLBuffer *dst, int x, int y, ushort *shadow)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    global_table = shadow;
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, global_table
                row1:
                    mov  edx, buffer_draw_width
                pix1:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt1
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    sub  al, 0f8h
                    shl  eax, 0fh
                    or   ax, word ptr [edi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                nxt1:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  pix1
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  row1
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, global_table
                row2:
                    mov  edx, buffer_draw_width
                pix2:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt2
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    sub  al, 0f8h
                    shl  eax, 0fh
                    or   ax, word ptr [edi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                nxt2:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  pix2
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  row2
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, global_table
                row3:
                    mov  edx, buffer_draw_width
                pix3:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt3
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    sub  al, 0f8h
                    shl  eax, 0fh
                    or   ax, word ptr [edi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                nxt3:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  pix3
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  row3
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, global_table
                row4:
                    mov  edx, buffer_draw_width
                pix4:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt4
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    sub  al, 0f8h
                    shl  eax, 0fh
                    or   ax, word ptr [edi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                nxt4:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  pix4
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  row4
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr [scale_x]
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr [scale_x_inc]
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc+2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, global_table
                srow5:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix5:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt5
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    sub  al, 0f8h
                    shl  eax, 0fh
                    or   ax, word ptr [edi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                snxt5:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  spix5
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  srow5
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (int)(scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr [scale_x]
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr [scale_x_inc]
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc+2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, global_table
                srow6:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix6:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt6
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    sub  al, 0f8h
                    shl  eax, 0fh
                    or   ax, word ptr [edi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                snxt6:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  spix6
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  srow6
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr [scale_x]
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr [scale_x_inc]
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc+2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, global_table
                srow7:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix7:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt7
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    sub  al, 0f8h
                    shl  eax, 0fh
                    or   ax, word ptr [edi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                snxt7:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  spix7
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  srow7
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (int)(scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr [scale_x]
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr [scale_x_inc]
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc+2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, global_table
                srow8:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix8:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt8
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    sub  al, 0f8h
                    shl  eax, 0fh
                    or   ax, word ptr [edi]
                    mov  ax, word ptr [ecx+eax*2]
                    mov  word ptr [edi], ax
                snxt8:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  spix8
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  srow8
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    }
    dst->release_data(2);
    return 0;
}
#define BLIT_TEST_AND_SHADOW(Lshade, Lnext)         \
    __asm cmp   byte ptr [esi], 0feh                \
    __asm jnc   Lnext                               \
    __asm xor   eax, eax                            \
    __asm mov   al, byte ptr [esi]                  \
    __asm or    al, al                              \
    __asm jnz   Lshade                              \
    __asm mov   word ptr [edi], 0                   \
    __asm jmp   Lnext                               \
    __asm Lshade:                                   \
    __asm dec   al                                  \
    __asm cmp   al, 0fh                             \
    __asm jnc   Lnext                               \
    __asm shl   eax, 0fh                            \
    __asm or    ax, word ptr [edi]                  \
    __asm mov   ax, word ptr [ebp + eax*2]          \
    __asm mov   word ptr [edi], ax
#define BLIT_CORE_UNSCALED(Lrow, Lpix, Lshade, Lnext, XSTEPOP, ROWOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm xor   eax, eax                            \
    __asm xor   ebx, ebx                            \
    __asm xor   ecx, ecx                            \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm push  ebp                                 \
    __asm mov   ebp, global_table                \
    __asm Lrow:                                     \
    __asm mov   edx, buffer_draw_width                    \
    __asm Lpix:                                     \
    BLIT_TEST_AND_SHADOW(Lshade, Lnext)             \
    __asm Lnext:                                    \
    __asm XSTEPOP esi                               \
    __asm add   edi, 2                              \
    __asm dec   edx                                 \
    __asm jnz   Lpix                                \
    __asm ROWOP esi, sprite_advance               \
    __asm add   edi, buffer_advance          \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ebp                                 \
    __asm pop   edi                                 \
    __asm pop   esi
#define BLIT_CORE_SCALED(Lrow, Lpix, Lshade, Lnext, CARRYOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm mov   edx, scale_y                \
    __asm shl   edx, 10h                            \
    __asm mov   dx, word ptr scale_x        \
    __asm mov   ebx, buffer_draw_height                   \
    __asm shl   ebx, 10h                            \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   ecx, buffer_draw_width                    \
    __asm dec   ecx                                 \
    __asm shl   ecx, 10h                            \
    __asm mov   buffer_draw_width, ecx                    \
    __asm xor   ecx, ecx                            \
    __asm mov   cx, word ptr [scale_x_inc+2]   \
    __asm mov   eax, scale_y_inc               \
    __asm shl   eax, 10h                            \
    __asm mov   frac_scale_y_inc, eax             \
    __asm push  ebp                                 \
    __asm mov   ebp, global_table                \
    __asm Lrow:                                     \
    __asm xor   eax, eax                            \
    __asm mov   ebx, buffer_draw_width                    \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   dx, word ptr scale_x        \
    __asm Lpix:                                     \
    BLIT_TEST_AND_SHADOW(Lshade, Lnext)             \
    __asm Lnext:                                    \
    __asm add   dx, bx                              \
    __asm CARRYOP esi, ecx                          \
    __asm add   edi, 2                              \
    __asm sub   ebx, 10000h                         \
    __asm jns   Lpix                                \
    __asm add   edi, buffer_advance          \
    __asm add   edx, frac_scale_y_inc             \
    __asm sbb   eax, eax                            \
    __asm mov   ebx, y_advance_no_carry        \
    __asm add   esi, dword ptr [ebx + eax*4]        \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ebp                                 \
    __asm pop   edi                                 \
    __asm pop   esi
int JGLSprite::draw_16_shadows_8_16(JGLBuffer *dst, int x, int y, ushort *shadow)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    global_table = shadow;
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u1row, u1pix, u1bl, u1nx, inc, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u2row, u2pix, u2bl, u2nx, inc, sub)
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u3row, u3pix, u3bl, u3nx, dec, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u4row, u4pix, u4bl, u4nx, dec, sub)
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s1row, s1pix, s1bl, s1nx, adc)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s2row, s2pix, s2bl, s2nx, adc)
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s3row, s3pix, s3bl, s3nx, sbb)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s4row, s4pix, s4bl, s4nx, sbb)
            }
        }
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::draw_civ3_units_8_16(JGLBuffer *dst, int x, int y, ushort *shadow, JGLBuffer *target)
{
    Rect        r;
    Rect        saved;
    ushort     *readp;
    uchar      *srcp;
    JGLPalette *pal;
    ushort     *dstw = 0;
    if (target != 0)
        dstw = target->get_data_16();
    else
        return 3;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    global_table = shadow;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
        readp = dst->get_data_16() + r.left + r.top * buffer_pitch;
        dstw = target->get_data_16() + r.left + r.top * target->get_pitch();
        buffer_draw_height = r.bottom - r.top;
        buffer_draw_width = r.right - r.left;
        sprite_advance = sprite_pitch - buffer_draw_width;
        buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
        __asm {
            push esi
            push edi
            xor  eax, eax
            xor  ebx, ebx
            xor  ecx, ecx
            mov  esi, srcp
            mov  edi, readp
            push eax
            push ebx
            mov  ebx, dstw
            push ebp
            mov  ebp, global_table
            push ecx
            mov  ecx, _table
        L_row:
            mov  edx, buffer_draw_width
        L_pix:
            cmp  byte ptr [esi], 0ffh
            jnc  L_next
            xor  eax, eax
            mov  al, byte ptr [esi]
            cmp  al, 0e0h
            jnc  L_shade
            mov  ax, word ptr [ecx + eax*2]
            mov  word ptr [ebx], ax
            jmp  L_next
        L_shade:
            cmp  al, 0f0h
            jnc  L_hi
            sub  al, 0d1h
            jmp  L_shl
        L_hi:
            sub  al, 0f0h
        L_shl:
            shl  eax, 0fh
            cmp  word ptr [ebx], 7c1fh
            jnz  L_self
            or   ax, word ptr [edi]
            mov  ax, word ptr [ebp + eax*2]
            mov  word ptr [ebx], ax
            jmp  L_next
        L_self:
            or   ax, word ptr [ebx]
            mov  ax, word ptr [ebp + eax*2]
            mov  word ptr [ebx], ax
        L_next:
            inc  esi
            add  edi, 2
            add  ebx, 2
            dec  edx
            jnz  L_pix
            add  esi, sprite_advance
            add  edi, buffer_advance
            add  ebx, buffer_advance
            dec  buffer_draw_height
            jnz  L_row
            pop  ecx
            pop  ebp
            pop  ebx
            pop  eax
            pop  edi
            pop  esi
        }
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::ShadowLutBlitScaledToTarget(JGLBuffer *dst, int x, int y, int denom, int num, ushort *shadow,
                                           JGLBuffer *target)
{
    Rect        r;
    ushort     *readp;
    uchar      *srcp;
    JGLPalette *pal;
    ushort     *dstw = 0;
    int         ax;
    int         ay;
    if (target != 0)
        dstw = target->get_data_16();
    else
        return 3;
    ax = abs(g_scaleX);
    scale_factor_x = (float)ax / num;
    ay = abs(g_scaleY);
    scale_factor_y = (float)ay / num;
    jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                 (int)(this->m_height * scale_factor_y));
    if (jgl_IntersectRect(&r, &r, dst->get_clip()) == 0) {
        dst->release_data(1);
        return 0;
    }
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    readp = dst->get_data_16() + r.left + r.top * buffer_pitch;
    dstw = dstw + (r.left + r.top * buffer_pitch);
    buffer_draw_width = r.right - r.left;
    buffer_draw_height = r.bottom - r.top;
    buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
    scale_x_inc = (num << 16) / abs(denom);
    scale_y_inc = (num << 16) / abs(denom);
    global_table = shadow;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (denom >= 0) {
        sprite_draw_width = (scale_x_inc * buffer_draw_width) >> 16;
        sprite_draw_height = (scale_y_inc * buffer_draw_height) >> 16;
        sprite_advance = sprite_pitch - sprite_draw_width;
        *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
        __asm {
            push esi
            push edi
            mov  esi, srcp
            mov  edi, readp
            mov  ecx, buffer_draw_width
            dec  ecx
            shl  ecx, 10h
            mov  buffer_draw_width, ecx
            push ecx
            mov  ecx, _table
        L_row:
            mov  ebx, buffer_draw_width
        L_pix:
            cmp  byte ptr [esi], 0ffh
            jnc  L_next
            xor  eax, eax
            push ebx
            push ebp
            mov  ebx, dstw
            mov  ebp, global_table
            mov  al, byte ptr [esi]
            cmp  al, 0e0h
            jnc  L_shade
            mov  ax, word ptr [ecx + eax*2]
            mov  word ptr [ebx], ax
            pop  ebp
            pop  ebx
            jmp  L_next
        L_shade:
            cmp  al, 0f0h
            jnc  L_hi
            sub  al, 0d1h
            jmp  L_shl
        L_hi:
            sub  al, 0f0h
        L_shl:
            shl  eax, 0fh
            cmp  word ptr [ebx], 7c1fh
            jnz  L_self
            or   ax, word ptr [edi]
            mov  ax, word ptr [ebp + eax*2]
            mov  word ptr [ebx], ax
            pop  ebp
            pop  ebx
            jmp  L_next
        L_self:
            or   ax, word ptr [ebx]
            mov  ax, word ptr [ebp + eax*2]
            mov  word ptr [ebx], ax
            pop  ebp
            pop  ebx
        L_next:
            add  esi, 2
            add  edi, 2
            add  dstw, 2
            sub  ebx, 10000h
            jns  L_pix
            add  edi, buffer_advance
            push edi
            mov  edi, dstw
            add  edi, buffer_advance
            mov  dstw, edi
            pop  edi
            mov  ebx, y_advance_no_carry
            add  esi, dword ptr [ebx]
            dec  buffer_draw_height
            jnz  L_row
            pop  ecx
            pop  edi
            pop  esi
        }
    }
    return 0;
}
#define BLIT_TEST_AND_BLEND(Lblend, Lnext)          \
    __asm cmp   byte ptr [esi], 0ffh                \
    __asm jnc   Lnext                               \
    __asm or    byte ptr alphaTest, 0               \
    __asm jz    Lblend                              \
    __asm cmp   byte ptr [esi], 0f8h                \
    __asm jnc   Lnext                               \
    __asm Lblend:                                   \
    __asm push  ebx                                 \
    __asm push  ecx                                 \
    __asm push  edx                                 \
    __asm xor   eax, eax                            \
    __asm xor   edx, edx                            \
    __asm mov   al, byte ptr [esi]                  \
    __asm mov   dx, word ptr [edi]                  \
    __asm mov   ax, word ptr [ecx + eax*2]          \
    __asm shl   eax, 4                              \
    __asm shl   edx, 4                              \
    __asm mov   ebx, eax                            \
    __asm mov   ecx, edx                            \
    __asm and   eax, 7c1f0h                         \
    __asm and   ebx, 3e00h                          \
    __asm and   ecx, 7c1f0h                         \
    __asm and   edx, 3e00h                          \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 1                              \
    __asm shr   ebx, 1                              \
    __asm and   eax, 7c1f0h                         \
    __asm and   ebx, 3e00h                          \
    __asm add   eax, ebx                            \
    __asm shr   eax, 4                              \
    __asm mov   word ptr [edi], ax                  \
    __asm pop   edx                                 \
    __asm pop   ecx                                 \
    __asm pop   ebx
#define BLIT_CORE_UNSCALED(Lrow, Lpix, Lblend, Lnext, XSTEPOP, ROWOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm xor   eax, eax                            \
    __asm xor   ebx, ebx                            \
    __asm xor   ecx, ecx                            \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm mov   edx, buffer_draw_width                    \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm XSTEPOP esi                               \
    __asm add   edi, 2                              \
    __asm dec   edx                                 \
    __asm jnz   Lpix                                \
    __asm ROWOP esi, sprite_advance               \
    __asm add   edi, buffer_advance          \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
#define BLIT_CORE_SCALED(Lrow, Lpix, Lblend, Lnext, CARRYOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm mov   edx, scale_y                \
    __asm shl   edx, 10h                            \
    __asm mov   dx, word ptr scale_x        \
    __asm mov   ebx, buffer_draw_height                   \
    __asm shl   ebx, 10h                            \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   ecx, buffer_draw_width                    \
    __asm dec   ecx                                 \
    __asm shl   ecx, 10h                            \
    __asm mov   buffer_draw_width, ecx                    \
    __asm xor   ecx, ecx                            \
    __asm mov   cx, word ptr [scale_x_inc+2]   \
    __asm mov   x_inc_int, ecx               \
    __asm mov   eax, scale_y_inc               \
    __asm shl   eax, 10h                            \
    __asm mov   frac_scale_y_inc, eax             \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm xor   eax, eax                            \
    __asm mov   ebx, buffer_draw_width                    \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   dx, word ptr scale_x        \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm add   dx, bx                              \
    __asm CARRYOP esi, x_inc_int             \
    __asm add   edi, 2                              \
    __asm sub   ebx, 10000h                         \
    __asm jns   Lpix                                \
    __asm add   edi, buffer_advance          \
    __asm add   edx, frac_scale_y_inc             \
    __asm sbb   eax, eax                            \
    __asm mov   ebx, y_advance_no_carry        \
    __asm add   esi, dword ptr [ebx + eax*4]        \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
int JGLSprite::draw_translucent_555_8_16_1_2(JGLBuffer *dst, int x, int y, char alphaTest)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u1row, u1pix, u1bl, u1nx, inc, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u2row, u2pix, u2bl, u2nx, inc, sub)
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u3row, u3pix, u3bl, u3nx, dec, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u4row, u4pix, u4bl, u4nx, dec, sub)
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s1row, s1pix, s1bl, s1nx, adc)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s2row, s2pix, s2bl, s2nx, adc)
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s3row, s3pix, s3bl, s3nx, sbb)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s4row, s4pix, s4bl, s4nx, sbb)
            }
        }
    }
    dst->release_data(2);
    return 0;
}
#define BLIT_TEST_AND_BLEND(Lblend, Lnext)          \
    __asm cmp   byte ptr [esi], 0ffh                \
    __asm jnc   Lnext                               \
    __asm or    byte ptr alphaTest, 0               \
    __asm jz    Lblend                              \
    __asm cmp   byte ptr [esi], 0f8h                \
    __asm jnc   Lnext                               \
    __asm Lblend:                                   \
    __asm push  ebx                                 \
    __asm push  ecx                                 \
    __asm push  edx                                 \
    __asm xor   eax, eax                            \
    __asm xor   edx, edx                            \
    __asm mov   al, byte ptr [esi]                  \
    __asm mov   dx, word ptr [edi]                  \
    __asm mov   ax, word ptr [ecx + eax*2]          \
    __asm shl   eax, 4                              \
    __asm shl   edx, 4                              \
    __asm mov   ebx, eax                            \
    __asm mov   ecx, edx                            \
    __asm and   eax, 07c1f0h                        \
    __asm and   ebx, 03e00h                         \
    __asm and   ecx, 07c1f0h                        \
    __asm and   edx, 03e00h                         \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 1                              \
    __asm shr   ebx, 1                              \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 1                              \
    __asm shr   ebx, 1                              \
    __asm and   eax, 07c1f0h                        \
    __asm and   ebx, 03e00h                         \
    __asm add   eax, ebx                            \
    __asm shr   eax, 4                              \
    __asm mov   word ptr [edi], ax                  \
    __asm pop   edx                                 \
    __asm pop   ecx                                 \
    __asm pop   ebx
#define BLIT_CORE_UNSCALED(Lrow, Lpix, Lblend, Lnext, XSTEPOP, ROWOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm xor   eax, eax                            \
    __asm xor   ebx, ebx                            \
    __asm xor   ecx, ecx                            \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm mov   edx, buffer_draw_width                    \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm XSTEPOP esi                               \
    __asm add   edi, 2                              \
    __asm dec   edx                                 \
    __asm jnz   Lpix                                \
    __asm ROWOP esi, sprite_advance               \
    __asm add   edi, buffer_advance          \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
#define BLIT_CORE_SCALED(Lrow, Lpix, Lblend, Lnext, CARRYOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm mov   edx, scale_y                \
    __asm shl   edx, 10h                            \
    __asm mov   dx, word ptr scale_x        \
    __asm mov   ebx, buffer_draw_height                   \
    __asm shl   ebx, 10h                            \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   ecx, buffer_draw_width                    \
    __asm dec   ecx                                 \
    __asm shl   ecx, 10h                            \
    __asm mov   buffer_draw_width, ecx                    \
    __asm xor   ecx, ecx                            \
    __asm mov   cx, word ptr [scale_x_inc+2]   \
    __asm mov   x_inc_int, ecx               \
    __asm mov   eax, scale_y_inc               \
    __asm shl   eax, 10h                            \
    __asm mov   frac_scale_y_inc, eax             \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm xor   eax, eax                            \
    __asm mov   ebx, buffer_draw_width                    \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   dx, word ptr scale_x        \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm add   dx, bx                              \
    __asm CARRYOP esi, x_inc_int             \
    __asm add   edi, 2                              \
    __asm sub   ebx, 10000h                         \
    __asm jns   Lpix                                \
    __asm add   edi, buffer_advance          \
    __asm add   edx, frac_scale_y_inc             \
    __asm sbb   eax, eax                            \
    __asm mov   ebx, y_advance_no_carry        \
    __asm add   esi, dword ptr [ebx + eax*4]        \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
int JGLSprite::draw_translucent_555_8_16_1_4(JGLBuffer *dst, int x, int y, char alphaTest)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u1row, u1pix, u1bl, u1nx, inc, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u2row, u2pix, u2bl, u2nx, inc, sub)
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u3row, u3pix, u3bl, u3nx, dec, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u4row, u4pix, u4bl, u4nx, dec, sub)
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s1row, s1pix, s1bl, s1nx, adc)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s2row, s2pix, s2bl, s2nx, adc)
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s3row, s3pix, s3bl, s3nx, sbb)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s4row, s4pix, s4bl, s4nx, sbb)
            }
        }
    }
    dst->release_data(2);
    return 0;
}
#define BLIT_TEST_AND_BLEND(Lblend, Lnext)          \
    __asm cmp   byte ptr [esi], 0ffh                \
    __asm jnc   Lnext                               \
    __asm or    byte ptr alphaTest, 0               \
    __asm jz    Lblend                              \
    __asm cmp   byte ptr [esi], 0f8h                \
    __asm jnc   Lnext                               \
    __asm Lblend:                                   \
    __asm push  ebx                                 \
    __asm push  ecx                                 \
    __asm push  edx                                 \
    __asm xor   eax, eax                            \
    __asm xor   edx, edx                            \
    __asm mov   dl, byte ptr [esi]                  \
    __asm mov   ax, word ptr [edi]                  \
    __asm mov   dx, word ptr [ecx + edx*2]          \
    __asm shl   eax, 4                              \
    __asm shl   edx, 4                              \
    __asm mov   ebx, eax                            \
    __asm mov   ecx, edx                            \
    __asm and   eax, 07c1f0h                        \
    __asm and   ebx, 03e00h                         \
    __asm and   ecx, 07c1f0h                        \
    __asm and   edx, 03e00h                         \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 1                              \
    __asm shr   ebx, 1                              \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 1                              \
    __asm shr   ebx, 1                              \
    __asm and   eax, 07c1f0h                        \
    __asm and   ebx, 03e00h                         \
    __asm add   eax, ebx                            \
    __asm shr   eax, 4                              \
    __asm mov   word ptr [edi], ax                  \
    __asm pop   edx                                 \
    __asm pop   ecx                                 \
    __asm pop   ebx
#define BLIT_CORE_UNSCALED(Lrow, Lpix, Lblend, Lnext, XSTEPOP, ROWOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm xor   eax, eax                            \
    __asm xor   ebx, ebx                            \
    __asm xor   ecx, ecx                            \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm mov   edx, buffer_draw_width                    \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm XSTEPOP esi                               \
    __asm add   edi, 2                              \
    __asm dec   edx                                 \
    __asm jnz   Lpix                                \
    __asm ROWOP esi, sprite_advance               \
    __asm add   edi, buffer_advance          \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
#define BLIT_CORE_SCALED(Lrow, Lpix, Lblend, Lnext, CARRYOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm mov   edx, scale_y                \
    __asm shl   edx, 10h                            \
    __asm mov   dx, word ptr scale_x        \
    __asm mov   ebx, buffer_draw_height                   \
    __asm shl   ebx, 10h                            \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   ecx, buffer_draw_width                    \
    __asm dec   ecx                                 \
    __asm shl   ecx, 10h                            \
    __asm mov   buffer_draw_width, ecx                    \
    __asm xor   ecx, ecx                            \
    __asm mov   cx, word ptr [scale_x_inc+2]   \
    __asm mov   x_inc_int, ecx               \
    __asm mov   eax, scale_y_inc               \
    __asm shl   eax, 10h                            \
    __asm mov   frac_scale_y_inc, eax             \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm xor   eax, eax                            \
    __asm mov   ebx, buffer_draw_width                    \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   dx, word ptr scale_x        \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm add   dx, bx                              \
    __asm CARRYOP esi, x_inc_int             \
    __asm add   edi, 2                              \
    __asm sub   ebx, 10000h                         \
    __asm jns   Lpix                                \
    __asm add   edi, buffer_advance          \
    __asm add   edx, frac_scale_y_inc             \
    __asm sbb   eax, eax                            \
    __asm mov   ebx, y_advance_no_carry        \
    __asm add   esi, dword ptr [ebx + eax*4]        \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
int JGLSprite::draw_translucent_555_8_16_3_4(JGLBuffer *dst, int x, int y, char alphaTest)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u1row, u1pix, u1bl, u1nx, inc, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u2row, u2pix, u2bl, u2nx, inc, sub)
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u3row, u3pix, u3bl, u3nx, dec, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u4row, u4pix, u4bl, u4nx, dec, sub)
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s1row, s1pix, s1bl, s1nx, adc)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s2row, s2pix, s2bl, s2nx, adc)
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s3row, s3pix, s3bl, s3nx, sbb)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s4row, s4pix, s4bl, s4nx, sbb)
            }
        }
    }
    dst->release_data(2);
    return 0;
}
#define BLIT_TEST_AND_BLEND(Lblend, Lnext)          \
    __asm cmp   byte ptr [esi], 0ffh                \
    __asm jnc   Lnext                               \
    __asm or    byte ptr alphaTest, 0               \
    __asm jz    Lblend                              \
    __asm cmp   byte ptr [esi], 0f8h                \
    __asm jnc   Lnext                               \
    __asm Lblend:                                   \
    __asm push  ebx                                 \
    __asm push  ecx                                 \
    __asm push  edx                                 \
    __asm xor   eax, eax                            \
    __asm xor   edx, edx                            \
    __asm mov   al, byte ptr [esi]                  \
    __asm mov   dx, word ptr [edi]                  \
    __asm mov   ax, word ptr [ecx + eax*2]          \
    __asm shl   eax, 4                              \
    __asm shl   edx, 4                              \
    __asm mov   ebx, eax                            \
    __asm mov   ecx, edx                            \
    __asm and   eax, 7c1f0h                         \
    __asm and   ebx, 3e00h                          \
    __asm and   ecx, 7c1f0h                         \
    __asm and   edx, 3e00h                          \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 1                              \
    __asm shr   ebx, 1                              \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 1                              \
    __asm shr   ebx, 1                              \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 1                              \
    __asm shr   ebx, 1                              \
    __asm and   eax, 7c1f0h                         \
    __asm and   ebx, 3e00h                          \
    __asm add   eax, ebx                            \
    __asm shr   eax, 4                              \
    __asm mov   word ptr [edi], ax                  \
    __asm pop   edx                                 \
    __asm pop   ecx                                 \
    __asm pop   ebx
#define BLIT_CORE_UNSCALED(Lrow, Lpix, Lblend, Lnext, XSTEPOP, ROWOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm xor   eax, eax                            \
    __asm xor   ebx, ebx                            \
    __asm xor   ecx, ecx                            \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm mov   edx, buffer_draw_width                    \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm XSTEPOP esi                               \
    __asm add   edi, 2                              \
    __asm dec   edx                                 \
    __asm jnz   Lpix                                \
    __asm ROWOP esi, sprite_advance               \
    __asm add   edi, buffer_advance          \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
#define BLIT_CORE_SCALED(Lrow, Lpix, Lblend, Lnext, CARRYOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm mov   edx, scale_y                \
    __asm shl   edx, 10h                            \
    __asm mov   dx, word ptr scale_x        \
    __asm mov   ebx, buffer_draw_height                   \
    __asm shl   ebx, 10h                            \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   ecx, buffer_draw_width                    \
    __asm dec   ecx                                 \
    __asm shl   ecx, 10h                            \
    __asm mov   buffer_draw_width, ecx                    \
    __asm xor   ecx, ecx                            \
    __asm mov   cx, word ptr [scale_x_inc+2]   \
    __asm mov   x_inc_int, ecx               \
    __asm mov   eax, scale_y_inc               \
    __asm shl   eax, 10h                            \
    __asm mov   frac_scale_y_inc, eax             \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm xor   eax, eax                            \
    __asm mov   ebx, buffer_draw_width                    \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   dx, word ptr scale_x        \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm add   dx, bx                              \
    __asm CARRYOP esi, x_inc_int             \
    __asm add   edi, 2                              \
    __asm sub   ebx, 10000h                         \
    __asm jns   Lpix                                \
    __asm add   edi, buffer_advance          \
    __asm add   edx, frac_scale_y_inc             \
    __asm sbb   eax, eax                            \
    __asm mov   ebx, y_advance_no_carry        \
    __asm add   esi, dword ptr [ebx + eax*4]        \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
int JGLSprite::draw_translucent_555_8_16_1_8(JGLBuffer *dst, int x, int y, char alphaTest)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u1row, u1pix, u1bl, u1nx, inc, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u2row, u2pix, u2bl, u2nx, inc, sub)
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u3row, u3pix, u3bl, u3nx, dec, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u4row, u4pix, u4bl, u4nx, dec, sub)
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s1row, s1pix, s1bl, s1nx, adc)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s2row, s2pix, s2bl, s2nx, adc)
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s3row, s3pix, s3bl, s3nx, sbb)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s4row, s4pix, s4bl, s4nx, sbb)
            }
        }
    }
    dst->release_data(2);
    return 0;
}
#define BLIT_TEST_AND_BLEND(Lblend, Lnext)          \
    __asm cmp   byte ptr [esi], 0ffh                \
    __asm jnc   Lnext                               \
    __asm or    byte ptr alphaTest, 0               \
    __asm jz    Lblend                              \
    __asm cmp   byte ptr [esi], 0f8h                \
    __asm jnc   Lnext                               \
    __asm Lblend:                                   \
    __asm push  ebx                                 \
    __asm push  ecx                                 \
    __asm push  edx                                 \
    __asm xor   eax, eax                            \
    __asm xor   edx, edx                            \
    __asm mov   dl, byte ptr [esi]                  \
    __asm mov   ax, word ptr [edi]                  \
    __asm mov   dx, word ptr [ecx + edx*2]          \
    __asm shl   eax, 4                              \
    __asm shl   edx, 4                              \
    __asm mov   ebx, eax                            \
    __asm mov   ecx, edx                            \
    __asm and   eax, 7c1f0h                         \
    __asm and   ebx, 3e00h                          \
    __asm and   ecx, 7c1f0h                         \
    __asm and   edx, 3e00h                          \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 1                              \
    __asm shr   ebx, 1                              \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 1                              \
    __asm shr   ebx, 1                              \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 1                              \
    __asm shr   ebx, 1                              \
    __asm and   eax, 7c1f0h                         \
    __asm and   ebx, 3e00h                          \
    __asm add   eax, ebx                            \
    __asm shr   eax, 4                              \
    __asm mov   word ptr [edi], ax                  \
    __asm pop   edx                                 \
    __asm pop   ecx                                 \
    __asm pop   ebx
#define BLIT_CORE_UNSCALED(Lrow, Lpix, Lblend, Lnext, XSTEPOP, ROWOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm xor   eax, eax                            \
    __asm xor   ebx, ebx                            \
    __asm xor   ecx, ecx                            \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm mov   edx, buffer_draw_width                    \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm XSTEPOP esi                               \
    __asm add   edi, 2                              \
    __asm dec   edx                                 \
    __asm jnz   Lpix                                \
    __asm ROWOP esi, sprite_advance               \
    __asm add   edi, buffer_advance          \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
#define BLIT_CORE_SCALED(Lrow, Lpix, Lblend, Lnext, CARRYOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm mov   edx, scale_y                \
    __asm shl   edx, 10h                            \
    __asm mov   dx, word ptr scale_x        \
    __asm mov   ebx, buffer_draw_height                   \
    __asm shl   ebx, 10h                            \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   ecx, buffer_draw_width                    \
    __asm dec   ecx                                 \
    __asm shl   ecx, 10h                            \
    __asm mov   buffer_draw_width, ecx                    \
    __asm xor   ecx, ecx                            \
    __asm mov   cx, word ptr [scale_x_inc+2]   \
    __asm mov   x_inc_int, ecx               \
    __asm mov   eax, scale_y_inc               \
    __asm shl   eax, 10h                            \
    __asm mov   frac_scale_y_inc, eax             \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm xor   eax, eax                            \
    __asm mov   ebx, buffer_draw_width                    \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   dx, word ptr scale_x        \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm add   dx, bx                              \
    __asm CARRYOP esi, x_inc_int             \
    __asm add   edi, 2                              \
    __asm sub   ebx, 10000h                         \
    __asm jns   Lpix                                \
    __asm add   edi, buffer_advance          \
    __asm add   edx, frac_scale_y_inc             \
    __asm sbb   eax, eax                            \
    __asm mov   ebx, y_advance_no_carry        \
    __asm add   esi, dword ptr [ebx + eax*4]        \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
int JGLSprite::draw_translucent_555_8_16_7_8(JGLBuffer *dst, int x, int y, char alphaTest)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u1row, u1pix, u1bl, u1nx, inc, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u2row, u2pix, u2bl, u2nx, inc, sub)
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u3row, u3pix, u3bl, u3nx, dec, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u4row, u4pix, u4bl, u4nx, dec, sub)
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s1row, s1pix, s1bl, s1nx, adc)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s2row, s2pix, s2bl, s2nx, adc)
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s3row, s3pix, s3bl, s3nx, sbb)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s4row, s4pix, s4bl, s4nx, sbb)
            }
        }
    }
    dst->release_data(2);
    return 0;
}
#define BLIT_TEST_AND_BLEND(Lblend, Lnext)          \
    __asm cmp   byte ptr [esi], 0ffh                \
    __asm jnc   Lnext                               \
    __asm or    byte ptr alphaTest, 0               \
    __asm jz    Lblend                              \
    __asm cmp   byte ptr [esi], 0f8h                \
    __asm jnc   Lnext                               \
    __asm Lblend:                                   \
    __asm push  ebx                                 \
    __asm push  ecx                                 \
    __asm push  edx                                 \
    __asm xor   eax, eax                            \
    __asm xor   edx, edx                            \
    __asm mov   dl, byte ptr [esi]                  \
    __asm mov   ax, word ptr [edi]                  \
    __asm mov   dx, word ptr [ecx + edx*2]          \
    __asm shl   eax, 4                              \
    __asm shl   edx, 4                              \
    __asm mov   ebx, eax                            \
    __asm mov   ecx, edx                            \
    __asm and   eax, 7c1f0h                         \
    __asm and   ebx, 3e00h                          \
    __asm and   ecx, 7c1f0h                         \
    __asm and   edx, 3e00h                          \
    __asm push  esi                                 \
    __asm mov   esi, eax                            \
    __asm shl   eax, 4                              \
    __asm sub   eax, esi                            \
    __asm mov   esi, ebx                            \
    __asm shl   ebx, 4                              \
    __asm sub   ebx, esi                            \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 4                              \
    __asm shr   ebx, 4                              \
    __asm pop   esi                                 \
    __asm and   eax, 7c1f0h                         \
    __asm and   ebx, 3e00h                          \
    __asm add   eax, ebx                            \
    __asm shr   eax, 4                              \
    __asm mov   word ptr [edi], ax                  \
    __asm pop   edx                                 \
    __asm pop   ecx                                 \
    __asm pop   ebx
#define BLIT_CORE_UNSCALED(Lrow, Lpix, Lblend, Lnext, XSTEPOP, ROWOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm xor   eax, eax                            \
    __asm xor   ebx, ebx                            \
    __asm xor   ecx, ecx                            \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm mov   edx, buffer_draw_width                    \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm XSTEPOP esi                               \
    __asm add   edi, 2                              \
    __asm dec   edx                                 \
    __asm jnz   Lpix                                \
    __asm ROWOP esi, sprite_advance               \
    __asm add   edi, buffer_advance          \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
#define BLIT_CORE_SCALED(Lrow, Lpix, Lblend, Lnext, CARRYOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm mov   edx, scale_y                \
    __asm shl   edx, 10h                            \
    __asm mov   dx, word ptr scale_x        \
    __asm mov   ebx, buffer_draw_height                   \
    __asm shl   ebx, 10h                            \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   ecx, buffer_draw_width                    \
    __asm dec   ecx                                 \
    __asm shl   ecx, 10h                            \
    __asm mov   buffer_draw_width, ecx                    \
    __asm xor   ecx, ecx                            \
    __asm mov   cx, word ptr [scale_x_inc+2]   \
    __asm mov   x_inc_int, ecx               \
    __asm mov   eax, scale_y_inc               \
    __asm shl   eax, 10h                            \
    __asm mov   frac_scale_y_inc, eax             \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm xor   eax, eax                            \
    __asm mov   ebx, buffer_draw_width                    \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   dx, word ptr scale_x        \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm add   dx, bx                              \
    __asm CARRYOP esi, x_inc_int             \
    __asm add   edi, 2                              \
    __asm sub   ebx, 10000h                         \
    __asm jns   Lpix                                \
    __asm add   edi, buffer_advance          \
    __asm add   edx, frac_scale_y_inc             \
    __asm sbb   eax, eax                            \
    __asm mov   ebx, y_advance_no_carry        \
    __asm add   esi, dword ptr [ebx + eax*4]        \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
int JGLSprite::draw_translucent_555_8_16_1_16(JGLBuffer *dst, int x, int y, char alphaTest)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u1row, u1pix, u1bl, u1nx, inc, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u2row, u2pix, u2bl, u2nx, inc, sub)
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u3row, u3pix, u3bl, u3nx, dec, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u4row, u4pix, u4bl, u4nx, dec, sub)
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s1row, s1pix, s1bl, s1nx, adc)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s2row, s2pix, s2bl, s2nx, adc)
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s3row, s3pix, s3bl, s3nx, sbb)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s4row, s4pix, s4bl, s4nx, sbb)
            }
        }
    }
    dst->release_data(2);
    return 0;
}
#define BLIT_TEST_AND_BLEND(Lblend, Lnext)          \
    __asm cmp   byte ptr [esi], 0ffh                \
    __asm jnc   Lnext                               \
    __asm or    byte ptr alphaTest, 0               \
    __asm jz    Lblend                              \
    __asm cmp   byte ptr [esi], 0f8h                \
    __asm jnc   Lnext                               \
    __asm Lblend:                                   \
    __asm push  ebx                                 \
    __asm push  ecx                                 \
    __asm push  edx                                 \
    __asm xor   eax, eax                            \
    __asm xor   edx, edx                            \
    __asm mov   al, byte ptr [esi]                  \
    __asm mov   dx, word ptr [edi]                  \
    __asm mov   ax, word ptr [ecx + eax*2]          \
    __asm shl   eax, 4                              \
    __asm shl   edx, 4                              \
    __asm mov   ebx, eax                            \
    __asm mov   ecx, edx                            \
    __asm and   eax, 7c1f0h                         \
    __asm and   ebx, 3e00h                          \
    __asm and   ecx, 7c1f0h                         \
    __asm and   edx, 3e00h                          \
    __asm push  esi                                 \
    __asm mov   esi, eax                            \
    __asm shl   eax, 4                              \
    __asm sub   eax, esi                            \
    __asm mov   esi, ebx                            \
    __asm shl   ebx, 4                              \
    __asm sub   ebx, esi                            \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 4                              \
    __asm shr   ebx, 4                              \
    __asm pop   esi                                 \
    __asm and   eax, 7c1f0h                         \
    __asm and   ebx, 3e00h                          \
    __asm add   eax, ebx                            \
    __asm shr   eax, 4                              \
    __asm mov   word ptr [edi], ax                  \
    __asm pop   edx                                 \
    __asm pop   ecx                                 \
    __asm pop   ebx
#define BLIT_CORE_UNSCALED(Lrow, Lpix, Lblend, Lnext, XSTEPOP, ROWOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm xor   eax, eax                            \
    __asm xor   ebx, ebx                            \
    __asm xor   ecx, ecx                            \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm mov   edx, buffer_draw_width                    \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm XSTEPOP esi                               \
    __asm add   edi, 2                              \
    __asm dec   edx                                 \
    __asm jnz   Lpix                                \
    __asm ROWOP esi, sprite_advance               \
    __asm add   edi, buffer_advance          \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
#define BLIT_CORE_SCALED(Lrow, Lpix, Lblend, Lnext, CARRYOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm mov   edx, scale_y                \
    __asm shl   edx, 10h                            \
    __asm mov   dx, word ptr scale_x        \
    __asm mov   ebx, buffer_draw_height                   \
    __asm shl   ebx, 10h                            \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   ecx, buffer_draw_width                    \
    __asm dec   ecx                                 \
    __asm shl   ecx, 10h                            \
    __asm mov   buffer_draw_width, ecx                    \
    __asm xor   ecx, ecx                            \
    __asm mov   cx, word ptr [scale_x_inc+2]   \
    __asm mov   x_inc_int, ecx               \
    __asm mov   eax, scale_y_inc               \
    __asm shl   eax, 10h                            \
    __asm mov   frac_scale_y_inc, eax             \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm xor   eax, eax                            \
    __asm mov   ebx, buffer_draw_width                    \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   dx, word ptr scale_x        \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm add   dx, bx                              \
    __asm CARRYOP esi, x_inc_int             \
    __asm add   edi, 2                              \
    __asm sub   ebx, 10000h                         \
    __asm jns   Lpix                                \
    __asm add   edi, buffer_advance          \
    __asm add   edx, frac_scale_y_inc             \
    __asm sbb   eax, eax                            \
    __asm mov   ebx, y_advance_no_carry        \
    __asm add   esi, dword ptr [ebx + eax*4]        \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
int JGLSprite::draw_translucent_555_8_16_15_16(JGLBuffer *dst, int x, int y, char alphaTest)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u1row, u1pix, u1bl, u1nx, inc, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u2row, u2pix, u2bl, u2nx, inc, sub)
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u3row, u3pix, u3bl, u3nx, dec, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u4row, u4pix, u4bl, u4nx, dec, sub)
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s1row, s1pix, s1bl, s1nx, adc)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s2row, s2pix, s2bl, s2nx, adc)
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s3row, s3pix, s3bl, s3nx, sbb)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s4row, s4pix, s4bl, s4nx, sbb)
            }
        }
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::draw_translucent_555_8_16_3_8(JGLBuffer *dst, int x, int y, char alphaTest)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                U1row:
                    mov  edx, buffer_draw_width
                U1pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short U1next
                    or   byte ptr alphaTest, 0
                    jz   short U1mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short U1next
                U1mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                U1next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  U1pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  U1row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                U2row:
                    mov  edx, buffer_draw_width
                U2pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short U2next
                    or   byte ptr alphaTest, 0
                    jz   short U2mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short U2next
                U2mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                U2next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  U2pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  U2row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                U3row:
                    mov  edx, buffer_draw_width
                U3pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short U3next
                    or   byte ptr alphaTest, 0
                    jz   short U3mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short U3next
                U3mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                U3next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  U3pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  U3row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                U4row:
                    mov  edx, buffer_draw_width
                U4pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short U4next
                    or   byte ptr alphaTest, 0
                    jz   short U4mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short U4next
                U4mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                U4next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  U4pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  U4row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                S5row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                S5pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short S5next
                    or   byte ptr alphaTest, 0
                    jz   short S5mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short S5next
                S5mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                S5next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  S5pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  S5row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                S6row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                S6pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short S6next
                    or   byte ptr alphaTest, 0
                    jz   short S6mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short S6next
                S6mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                S6next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  S6pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  S6row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                S7row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                S7pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short S7next
                    or   byte ptr alphaTest, 0
                    jz   short S7mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short S7next
                S7mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                S7next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  S7pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  S7row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                S8row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                S8pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short S8next
                    or   byte ptr alphaTest, 0
                    jz   short S8mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short S8next
                S8mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                S8next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  S8pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  S8row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::draw_translucent_555_8_16_5_8(JGLBuffer *dst, int x, int y, char alphaTest)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                U1row:
                    mov  edx, buffer_draw_width
                U1pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short U1next
                    or   byte ptr alphaTest, 0
                    jz   short U1mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short U1next
                U1mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                U1next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  U1pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  U1row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                U2row:
                    mov  edx, buffer_draw_width
                U2pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short U2next
                    or   byte ptr alphaTest, 0
                    jz   short U2mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short U2next
                U2mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                U2next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  U2pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  U2row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                U3row:
                    mov  edx, buffer_draw_width
                U3pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short U3next
                    or   byte ptr alphaTest, 0
                    jz   short U3mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short U3next
                U3mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                U3next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  U3pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  U3row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                U4row:
                    mov  edx, buffer_draw_width
                U4pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short U4next
                    or   byte ptr alphaTest, 0
                    jz   short U4mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short U4next
                U4mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                U4next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  U4pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  U4row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                S5row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                S5pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short S5next
                    or   byte ptr alphaTest, 0
                    jz   short S5mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short S5next
                S5mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                S5next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  S5pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  S5row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                S6row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                S6pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short S6next
                    or   byte ptr alphaTest, 0
                    jz   short S6mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short S6next
                S6mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                S6next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  S6pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  S6row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                S7row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                S7pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short S7next
                    or   byte ptr alphaTest, 0
                    jz   short S7mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short S7next
                S7mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                S7next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  S7pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  S7row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                S8row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                S8pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short S8next
                    or   byte ptr alphaTest, 0
                    jz   short S8mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short S8next
                S8mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                S8next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  S8pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  S8row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::draw_translucent_555_8_16_7_16(JGLBuffer *dst, int x, int y, char alphaTest)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                U1row:
                    mov  edx, buffer_draw_width
                U1pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short U1next
                    or   byte ptr alphaTest, 0
                    jz   short U1mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short U1next
                U1mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                U1next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  U1pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  U1row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                U2row:
                    mov  edx, buffer_draw_width
                U2pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short U2next
                    or   byte ptr alphaTest, 0
                    jz   short U2mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short U2next
                U2mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                U2next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  U2pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  U2row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                U3row:
                    mov  edx, buffer_draw_width
                U3pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short U3next
                    or   byte ptr alphaTest, 0
                    jz   short U3mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short U3next
                U3mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                U3next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  U3pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  U3row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                U4row:
                    mov  edx, buffer_draw_width
                U4pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short U4next
                    or   byte ptr alphaTest, 0
                    jz   short U4mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short U4next
                U4mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                U4next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  U4pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  U4row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                S5row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                S5pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short S5next
                    or   byte ptr alphaTest, 0
                    jz   short S5mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short S5next
                S5mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                S5next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  S5pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  S5row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                S6row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                S6pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short S6next
                    or   byte ptr alphaTest, 0
                    jz   short S6mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short S6next
                S6mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                S6next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  S6pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  S6row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                S7row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                S7pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short S7next
                    or   byte ptr alphaTest, 0
                    jz   short S7mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short S7next
                S7mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                S7next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  S7pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  S7row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                S8row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                S8pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short S8next
                    or   byte ptr alphaTest, 0
                    jz   short S8mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short S8next
                S8mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                S8next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  S8pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  S8row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::draw_translucent_555_8_16_9_16(JGLBuffer *dst, int x, int y, char alphaTest)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                U1row:
                    mov  edx, buffer_draw_width
                U1pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short U1next
                    or   byte ptr alphaTest, 0
                    jz   short U1mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short U1next
                U1mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                U1next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  U1pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  U1row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                U2row:
                    mov  edx, buffer_draw_width
                U2pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short U2next
                    or   byte ptr alphaTest, 0
                    jz   short U2mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short U2next
                U2mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                U2next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  U2pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  U2row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                U3row:
                    mov  edx, buffer_draw_width
                U3pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short U3next
                    or   byte ptr alphaTest, 0
                    jz   short U3mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short U3next
                U3mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                U3next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  U3pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  U3row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                U4row:
                    mov  edx, buffer_draw_width
                U4pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short U4next
                    or   byte ptr alphaTest, 0
                    jz   short U4mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short U4next
                U4mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                U4next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  U4pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  U4row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                S5row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                S5pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short S5next
                    or   byte ptr alphaTest, 0
                    jz   short S5mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short S5next
                S5mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                S5next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  S5pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  S5row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                S6row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                S6pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short S6next
                    or   byte ptr alphaTest, 0
                    jz   short S6mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short S6next
                S6mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                S6next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  S6pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  S6row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                S7row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                S7pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short S7next
                    or   byte ptr alphaTest, 0
                    jz   short S7mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short S7next
                S7mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                S7next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  S7pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  S7row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr scale_x_inc[2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                S8row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                S8pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  short S8next
                    or   byte ptr alphaTest, 0
                    jz   short S8mix
                    cmp  byte ptr [esi], 0f8h
                    jnc  short S8next
                S8mix:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    and  ecx, 7c1f0h
                    and  edx, 3e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 7c1f0h
                    and  ebx, 3e00h
                    add  eax, ebx
                    shr  eax, 4
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                S8next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  S8pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  S8row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    }
    dst->release_data(2);
    return 0;
}
#define BLIT_TEST_AND_BLEND(Lblend, Lnext)          \
    __asm cmp   byte ptr [esi], 0ffh                \
    __asm jnc   Lnext                               \
    __asm or    byte ptr alphaTest, 0               \
    __asm jz    Lblend                              \
    __asm cmp   byte ptr [esi], 0f8h                \
    __asm jnc   Lnext                               \
    __asm Lblend:                                   \
    __asm push  ebx                                 \
    __asm push  ecx                                 \
    __asm push  edx                                 \
    __asm xor   eax, eax                            \
    __asm xor   edx, edx                            \
    __asm mov   dl, byte ptr [esi]                  \
    __asm mov   ax, word ptr [edi]                  \
    __asm mov   dx, word ptr [ecx + edx*2]          \
    __asm shl   eax, 4                              \
    __asm shl   edx, 4                              \
    __asm mov   ebx, eax                            \
    __asm mov   ecx, edx                            \
    __asm and   eax, 7c1f0h                         \
    __asm and   ebx, 3e00h                          \
    __asm and   ecx, 7c1f0h                         \
    __asm and   edx, 3e00h                          \
    __asm push  esi                                 \
    __asm mov   esi, eax                            \
    __asm shl   eax, 3                              \
    __asm sub   eax, esi                            \
    __asm add   eax, ecx                            \
    __asm shl   eax, 1                              \
    __asm sub   eax, esi                            \
    __asm mov   esi, ebx                            \
    __asm shl   ebx, 3                              \
    __asm sub   ebx, esi                            \
    __asm add   ebx, edx                            \
    __asm shl   ebx, 1                              \
    __asm sub   ebx, esi                            \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 4                              \
    __asm shr   ebx, 4                              \
    __asm pop   esi                                 \
    __asm and   eax, 7c1f0h                         \
    __asm and   ebx, 3e00h                          \
    __asm add   eax, ebx                            \
    __asm shr   eax, 4                              \
    __asm mov   word ptr [edi], ax                  \
    __asm pop   edx                                 \
    __asm pop   ecx                                 \
    __asm pop   ebx
#define BLIT_CORE_UNSCALED(Lrow, Lpix, Lblend, Lnext, XSTEPOP, ROWOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm xor   eax, eax                            \
    __asm xor   ebx, ebx                            \
    __asm xor   ecx, ecx                            \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm mov   edx, buffer_draw_width                    \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm XSTEPOP esi                               \
    __asm add   edi, 2                              \
    __asm dec   edx                                 \
    __asm jnz   Lpix                                \
    __asm ROWOP esi, sprite_advance               \
    __asm add   edi, buffer_advance          \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
#define BLIT_CORE_SCALED(Lrow, Lpix, Lblend, Lnext, CARRYOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm mov   edx, scale_y                \
    __asm shl   edx, 10h                            \
    __asm mov   dx, word ptr scale_x        \
    __asm mov   ebx, buffer_draw_height                   \
    __asm shl   ebx, 10h                            \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   ecx, buffer_draw_width                    \
    __asm dec   ecx                                 \
    __asm shl   ecx, 10h                            \
    __asm mov   buffer_draw_width, ecx                    \
    __asm xor   ecx, ecx                            \
    __asm mov   cx, word ptr [scale_x_inc+2]   \
    __asm mov   x_inc_int, ecx               \
    __asm mov   eax, scale_y_inc               \
    __asm shl   eax, 10h                            \
    __asm mov   frac_scale_y_inc, eax             \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm xor   eax, eax                            \
    __asm mov   ebx, buffer_draw_width                    \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   dx, word ptr scale_x        \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm add   dx, bx                              \
    __asm CARRYOP esi, x_inc_int             \
    __asm add   edi, 2                              \
    __asm sub   ebx, 10000h                         \
    __asm jns   Lpix                                \
    __asm add   edi, buffer_advance          \
    __asm add   edx, frac_scale_y_inc             \
    __asm sbb   eax, eax                            \
    __asm mov   ebx, y_advance_no_carry        \
    __asm add   esi, dword ptr [ebx + eax*4]        \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
int JGLSprite::draw_translucent_555_8_16_3_16(JGLBuffer *dst, int x, int y, char alphaTest)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u1row, u1pix, u1bl, u1nx, inc, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u2row, u2pix, u2bl, u2nx, inc, sub)
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u3row, u3pix, u3bl, u3nx, dec, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u4row, u4pix, u4bl, u4nx, dec, sub)
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s1row, s1pix, s1bl, s1nx, adc)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s2row, s2pix, s2bl, s2nx, adc)
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s3row, s3pix, s3bl, s3nx, sbb)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s4row, s4pix, s4bl, s4nx, sbb)
            }
        }
    }
    dst->release_data(2);
    return 0;
}
#define BLIT_TEST_AND_BLEND(Lblend, Lnext)          \
    __asm cmp   byte ptr [esi], 0ffh                \
    __asm jnc   Lnext                               \
    __asm or    byte ptr alphaTest, 0               \
    __asm jz    Lblend                              \
    __asm cmp   byte ptr [esi], 0f8h                \
    __asm jnc   Lnext                               \
    __asm Lblend:                                   \
    __asm push  ebx                                 \
    __asm push  ecx                                 \
    __asm push  edx                                 \
    __asm xor   eax, eax                            \
    __asm xor   edx, edx                            \
    __asm mov   al, byte ptr [esi]                  \
    __asm mov   dx, word ptr [edi]                  \
    __asm mov   ax, word ptr [ecx + eax*2]          \
    __asm shl   eax, 4                              \
    __asm shl   edx, 4                              \
    __asm mov   ebx, eax                            \
    __asm mov   ecx, edx                            \
    __asm and   eax, 7c1f0h                         \
    __asm and   ebx, 3e00h                          \
    __asm and   ecx, 7c1f0h                         \
    __asm and   edx, 3e00h                          \
    __asm push  esi                                 \
    __asm mov   esi, eax                            \
    __asm shl   eax, 3                              \
    __asm sub   eax, esi                            \
    __asm add   eax, ecx                            \
    __asm shl   eax, 1                              \
    __asm sub   eax, esi                            \
    __asm mov   esi, ebx                            \
    __asm shl   ebx, 3                              \
    __asm sub   ebx, esi                            \
    __asm add   ebx, edx                            \
    __asm shl   ebx, 1                              \
    __asm sub   ebx, esi                            \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 4                              \
    __asm shr   ebx, 4                              \
    __asm pop   esi                                 \
    __asm and   eax, 7c1f0h                         \
    __asm and   ebx, 3e00h                          \
    __asm add   eax, ebx                            \
    __asm shr   eax, 4                              \
    __asm mov   word ptr [edi], ax                  \
    __asm pop   edx                                 \
    __asm pop   ecx                                 \
    __asm pop   ebx
#define BLIT_CORE_UNSCALED(Lrow, Lpix, Lblend, Lnext, XSTEPOP, ROWOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm xor   eax, eax                            \
    __asm xor   ebx, ebx                            \
    __asm xor   ecx, ecx                            \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm mov   edx, buffer_draw_width                    \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm XSTEPOP esi                               \
    __asm add   edi, 2                              \
    __asm dec   edx                                 \
    __asm jnz   Lpix                                \
    __asm ROWOP esi, sprite_advance               \
    __asm add   edi, buffer_advance          \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
#define BLIT_CORE_SCALED(Lrow, Lpix, Lblend, Lnext, CARRYOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm mov   edx, scale_y                \
    __asm shl   edx, 10h                            \
    __asm mov   dx, word ptr scale_x        \
    __asm mov   ebx, buffer_draw_height                   \
    __asm shl   ebx, 10h                            \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   ecx, buffer_draw_width                    \
    __asm dec   ecx                                 \
    __asm shl   ecx, 10h                            \
    __asm mov   buffer_draw_width, ecx                    \
    __asm xor   ecx, ecx                            \
    __asm mov   cx, word ptr [scale_x_inc+2]   \
    __asm mov   x_inc_int, ecx               \
    __asm mov   eax, scale_y_inc               \
    __asm shl   eax, 10h                            \
    __asm mov   frac_scale_y_inc, eax             \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm xor   eax, eax                            \
    __asm mov   ebx, buffer_draw_width                    \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   dx, word ptr scale_x        \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm add   dx, bx                              \
    __asm CARRYOP esi, x_inc_int             \
    __asm add   edi, 2                              \
    __asm sub   ebx, 10000h                         \
    __asm jns   Lpix                                \
    __asm add   edi, buffer_advance          \
    __asm add   edx, frac_scale_y_inc             \
    __asm sbb   eax, eax                            \
    __asm mov   ebx, y_advance_no_carry        \
    __asm add   esi, dword ptr [ebx + eax*4]        \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
int JGLSprite::draw_translucent_555_8_16_13_16(JGLBuffer *dst, int x, int y, char alphaTest)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u1row, u1pix, u1bl, u1nx, inc, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u2row, u2pix, u2bl, u2nx, inc, sub)
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u3row, u3pix, u3bl, u3nx, dec, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u4row, u4pix, u4bl, u4nx, dec, sub)
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s1row, s1pix, s1bl, s1nx, adc)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s2row, s2pix, s2bl, s2nx, adc)
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s3row, s3pix, s3bl, s3nx, sbb)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s4row, s4pix, s4bl, s4nx, sbb)
            }
        }
    }
    dst->release_data(2);
    return 0;
}
#define BLIT_TEST_AND_BLEND(Lblend, Lnext)          \
    __asm cmp   byte ptr [esi], 0ffh                \
    __asm jnc   Lnext                               \
    __asm or    byte ptr alphaTest, 0               \
    __asm jz    Lblend                              \
    __asm cmp   byte ptr [esi], 0f8h                \
    __asm jnc   Lnext                               \
    __asm Lblend:                                   \
    __asm push  ebx                                 \
    __asm push  ecx                                 \
    __asm push  edx                                 \
    __asm xor   eax, eax                            \
    __asm xor   edx, edx                            \
    __asm mov   dl, byte ptr [esi]                  \
    __asm mov   ax, word ptr [edi]                  \
    __asm mov   dx, word ptr [ecx + edx*2]          \
    __asm shl   eax, 4                              \
    __asm shl   edx, 4                              \
    __asm mov   ebx, eax                            \
    __asm mov   ecx, edx                            \
    __asm and   eax, 7c1f0h                         \
    __asm and   ebx, 3e00h                          \
    __asm and   ecx, 7c1f0h                         \
    __asm and   edx, 3e00h                          \
    __asm push  esi                                 \
    __asm mov   esi, eax                            \
    __asm shl   eax, 2                              \
    __asm sub   eax, esi                            \
    __asm add   eax, ecx                            \
    __asm shl   eax, 2                              \
    __asm sub   eax, esi                            \
    __asm mov   esi, ebx                            \
    __asm shl   ebx, 2                              \
    __asm sub   ebx, esi                            \
    __asm add   ebx, edx                            \
    __asm shl   ebx, 2                              \
    __asm sub   ebx, esi                            \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 4                              \
    __asm shr   ebx, 4                              \
    __asm pop   esi                                 \
    __asm and   eax, 7c1f0h                         \
    __asm and   ebx, 3e00h                          \
    __asm add   eax, ebx                            \
    __asm shr   eax, 4                              \
    __asm mov   word ptr [edi], ax                  \
    __asm pop   edx                                 \
    __asm pop   ecx                                 \
    __asm pop   ebx
#define BLIT_CORE_UNSCALED(Lrow, Lpix, Lblend, Lnext, XSTEPOP, ROWOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm xor   eax, eax                            \
    __asm xor   ebx, ebx                            \
    __asm xor   ecx, ecx                            \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm mov   edx, buffer_draw_width                    \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm XSTEPOP esi                               \
    __asm add   edi, 2                              \
    __asm dec   edx                                 \
    __asm jnz   Lpix                                \
    __asm ROWOP esi, sprite_advance               \
    __asm add   edi, buffer_advance          \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
#define BLIT_CORE_SCALED(Lrow, Lpix, Lblend, Lnext, CARRYOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm mov   edx, scale_y                \
    __asm shl   edx, 10h                            \
    __asm mov   dx, word ptr scale_x        \
    __asm mov   ebx, buffer_draw_height                   \
    __asm shl   ebx, 10h                            \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   ecx, buffer_draw_width                    \
    __asm dec   ecx                                 \
    __asm shl   ecx, 10h                            \
    __asm mov   buffer_draw_width, ecx                    \
    __asm xor   ecx, ecx                            \
    __asm mov   cx, word ptr [scale_x_inc+2]   \
    __asm mov   x_inc_int, ecx               \
    __asm mov   eax, scale_y_inc               \
    __asm shl   eax, 10h                            \
    __asm mov   frac_scale_y_inc, eax             \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm xor   eax, eax                            \
    __asm mov   ebx, buffer_draw_width                    \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   dx, word ptr scale_x        \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm add   dx, bx                              \
    __asm CARRYOP esi, x_inc_int             \
    __asm add   edi, 2                              \
    __asm sub   ebx, 10000h                         \
    __asm jns   Lpix                                \
    __asm add   edi, buffer_advance          \
    __asm add   edx, frac_scale_y_inc             \
    __asm sbb   eax, eax                            \
    __asm mov   ebx, y_advance_no_carry        \
    __asm add   esi, dword ptr [ebx + eax*4]        \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
int JGLSprite::draw_translucent_555_8_16_5_16(JGLBuffer *dst, int x, int y, char alphaTest)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u1row, u1pix, u1bl, u1nx, inc, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u2row, u2pix, u2bl, u2nx, inc, sub)
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u3row, u3pix, u3bl, u3nx, dec, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u4row, u4pix, u4bl, u4nx, dec, sub)
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s1row, s1pix, s1bl, s1nx, adc)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s2row, s2pix, s2bl, s2nx, adc)
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s3row, s3pix, s3bl, s3nx, sbb)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s4row, s4pix, s4bl, s4nx, sbb)
            }
        }
    }
    dst->release_data(2);
    return 0;
}
#define BLIT_TEST_AND_BLEND(Lblend, Lnext)          \
    __asm cmp   byte ptr [esi], 0ffh                \
    __asm jnc   Lnext                               \
    __asm or    byte ptr alphaTest, 0               \
    __asm jz    Lblend                              \
    __asm cmp   byte ptr [esi], 0f8h                \
    __asm jnc   Lnext                               \
    __asm Lblend:                                   \
    __asm push  ebx                                 \
    __asm push  ecx                                 \
    __asm push  edx                                 \
    __asm xor   eax, eax                            \
    __asm xor   edx, edx                            \
    __asm mov   al, byte ptr [esi]                  \
    __asm mov   dx, word ptr [edi]                  \
    __asm mov   ax, word ptr [ecx + eax*2]          \
    __asm shl   eax, 4                              \
    __asm shl   edx, 4                              \
    __asm mov   ebx, eax                            \
    __asm mov   ecx, edx                            \
    __asm and   eax, 7c1f0h                         \
    __asm and   ebx, 3e00h                          \
    __asm and   ecx, 7c1f0h                         \
    __asm and   edx, 3e00h                          \
    __asm push  esi                                 \
    __asm mov   esi, eax                            \
    __asm shl   eax, 2                              \
    __asm sub   eax, esi                            \
    __asm add   eax, ecx                            \
    __asm shl   eax, 2                              \
    __asm sub   eax, esi                            \
    __asm mov   esi, ebx                            \
    __asm shl   ebx, 2                              \
    __asm sub   ebx, esi                            \
    __asm add   ebx, edx                            \
    __asm shl   ebx, 2                              \
    __asm sub   ebx, esi                            \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 4                              \
    __asm shr   ebx, 4                              \
    __asm pop   esi                                 \
    __asm and   eax, 7c1f0h                         \
    __asm and   ebx, 3e00h                          \
    __asm add   eax, ebx                            \
    __asm shr   eax, 4                              \
    __asm mov   word ptr [edi], ax                  \
    __asm pop   edx                                 \
    __asm pop   ecx                                 \
    __asm pop   ebx
#define BLIT_CORE_UNSCALED(Lrow, Lpix, Lblend, Lnext, XSTEPOP, ROWOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm xor   eax, eax                            \
    __asm xor   ebx, ebx                            \
    __asm xor   ecx, ecx                            \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm mov   edx, buffer_draw_width                    \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm XSTEPOP esi                               \
    __asm add   edi, 2                              \
    __asm dec   edx                                 \
    __asm jnz   Lpix                                \
    __asm ROWOP esi, sprite_advance               \
    __asm add   edi, buffer_advance          \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
#define BLIT_CORE_SCALED(Lrow, Lpix, Lblend, Lnext, CARRYOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm mov   edx, scale_y                \
    __asm shl   edx, 10h                            \
    __asm mov   dx, word ptr scale_x        \
    __asm mov   ebx, buffer_draw_height                   \
    __asm shl   ebx, 10h                            \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   ecx, buffer_draw_width                    \
    __asm dec   ecx                                 \
    __asm shl   ecx, 10h                            \
    __asm mov   buffer_draw_width, ecx                    \
    __asm xor   ecx, ecx                            \
    __asm mov   cx, word ptr [scale_x_inc+2]   \
    __asm mov   x_inc_int, ecx               \
    __asm mov   eax, scale_y_inc               \
    __asm shl   eax, 10h                            \
    __asm mov   frac_scale_y_inc, eax             \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm xor   eax, eax                            \
    __asm mov   ebx, buffer_draw_width                    \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   dx, word ptr scale_x        \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm add   dx, bx                              \
    __asm CARRYOP esi, x_inc_int             \
    __asm add   edi, 2                              \
    __asm sub   ebx, 10000h                         \
    __asm jns   Lpix                                \
    __asm add   edi, buffer_advance          \
    __asm add   edx, frac_scale_y_inc             \
    __asm sbb   eax, eax                            \
    __asm mov   ebx, y_advance_no_carry        \
    __asm add   esi, dword ptr [ebx + eax*4]        \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
int JGLSprite::draw_translucent_555_8_16_11_16(JGLBuffer *dst, int x, int y, char alphaTest)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u1row, u1pix, u1bl, u1nx, inc, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u2row, u2pix, u2bl, u2nx, inc, sub)
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u3row, u3pix, u3bl, u3nx, dec, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u4row, u4pix, u4bl, u4nx, dec, sub)
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s1row, s1pix, s1bl, s1nx, adc)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s2row, s2pix, s2bl, s2nx, adc)
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s3row, s3pix, s3bl, s3nx, sbb)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s4row, s4pix, s4bl, s4nx, sbb)
            }
        }
    }
    dst->release_data(2);
    return 0;
}
#define BLIT_TEST_AND_BLEND(Lblend, Lnext)          \
    __asm cmp   byte ptr [esi], 0ffh                \
    __asm jnc   Lnext                               \
    __asm or    byte ptr alphaTest, 0               \
    __asm jz    Lblend                              \
    __asm cmp   byte ptr [esi], 0f8h                \
    __asm jnc   Lnext                               \
    __asm Lblend:                                   \
    __asm push  ebx                                 \
    __asm push  ecx                                 \
    __asm push  edx                                 \
    __asm xor   eax, eax                            \
    __asm xor   edx, edx                            \
    __asm mov   al, byte ptr [esi]                  \
    __asm mov   dx, word ptr [edi]                  \
    __asm mov   ax, word ptr [ecx + eax*2]          \
    __asm shl   eax, 4                              \
    __asm shl   edx, 4                              \
    __asm mov   ebx, eax                            \
    __asm mov   ecx, edx                            \
    __asm and   eax, 0f81f0h                        \
    __asm and   ebx, 07e00h                         \
    __asm and   ecx, 0f81f0h                        \
    __asm and   edx, 07e00h                         \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 1                              \
    __asm shr   ebx, 1                              \
    __asm and   eax, 0f81f0h                        \
    __asm and   ebx, 07e00h                         \
    __asm add   eax, ebx                            \
    __asm mov   word ptr [edi], ax                  \
    __asm pop   edx                                 \
    __asm pop   ecx                                 \
    __asm pop   ebx
#define BLIT_CORE_UNSCALED(Lrow, Lpix, Lblend, Lnext, XSTEPOP, ROWOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm xor   eax, eax                            \
    __asm xor   ebx, ebx                            \
    __asm xor   ecx, ecx                            \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm mov   edx, buffer_draw_width                    \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm XSTEPOP esi                               \
    __asm add   edi, 2                              \
    __asm dec   edx                                 \
    __asm jnz   Lpix                                \
    __asm ROWOP esi, sprite_advance               \
    __asm add   edi, buffer_advance          \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
#define BLIT_CORE_SCALED(Lrow, Lpix, Lblend, Lnext, CARRYOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm mov   edx, scale_y                \
    __asm shl   edx, 10h                            \
    __asm mov   dx, word ptr scale_x        \
    __asm mov   ebx, buffer_draw_height                   \
    __asm shl   ebx, 10h                            \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   ecx, buffer_draw_width                    \
    __asm dec   ecx                                 \
    __asm shl   ecx, 10h                            \
    __asm mov   buffer_draw_width, ecx                    \
    __asm xor   ecx, ecx                            \
    __asm mov   cx, word ptr [scale_x_inc+2]   \
    __asm mov   x_inc_int, ecx               \
    __asm mov   eax, scale_y_inc               \
    __asm shl   eax, 10h                            \
    __asm mov   frac_scale_y_inc, eax             \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm xor   eax, eax                            \
    __asm mov   ebx, buffer_draw_width                    \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   dx, word ptr scale_x        \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm add   dx, bx                              \
    __asm CARRYOP esi, x_inc_int             \
    __asm add   edi, 2                              \
    __asm sub   ebx, 10000h                         \
    __asm jns   Lpix                                \
    __asm add   edi, buffer_advance          \
    __asm add   edx, frac_scale_y_inc             \
    __asm sbb   eax, eax                            \
    __asm mov   ebx, y_advance_no_carry        \
    __asm add   esi, dword ptr [ebx + eax*4]        \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
int JGLSprite::draw_translucent_565_8_16_1_2(JGLBuffer *dst, int x, int y, char alphaTest)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u1row, u1pix, u1bl, u1nx, inc, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u2row, u2pix, u2bl, u2nx, inc, sub)
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u3row, u3pix, u3bl, u3nx, dec, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u4row, u4pix, u4bl, u4nx, dec, sub)
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s1row, s1pix, s1bl, s1nx, adc)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s2row, s2pix, s2bl, s2nx, adc)
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s3row, s3pix, s3bl, s3nx, sbb)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s4row, s4pix, s4bl, s4nx, sbb)
            }
        }
    }
    dst->release_data(2);
    return 0;
}
#define BLIT_TEST_AND_BLEND(Lblend, Lnext)          \
    __asm cmp   byte ptr [esi], 0ffh                \
    __asm jnc   Lnext                               \
    __asm or    byte ptr alphaTest, 0               \
    __asm jz    Lblend                              \
    __asm cmp   byte ptr [esi], 0f8h                \
    __asm jnc   Lnext                               \
    __asm Lblend:                                   \
    __asm push  ebx                                 \
    __asm push  ecx                                 \
    __asm push  edx                                 \
    __asm xor   eax, eax                            \
    __asm xor   edx, edx                            \
    __asm mov   al, byte ptr [esi]                  \
    __asm mov   dx, word ptr [edi]                  \
    __asm mov   ax, word ptr [ecx + eax*2]          \
    __asm shl   eax, 4                              \
    __asm shl   edx, 4                              \
    __asm mov   ebx, eax                            \
    __asm mov   ecx, edx                            \
    __asm and   eax, 0f81f0h                        \
    __asm and   ebx, 07e00h                         \
    __asm and   ecx, 0f81f0h                        \
    __asm and   edx, 07e00h                         \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 1                              \
    __asm shr   ebx, 1                              \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 1                              \
    __asm shr   ebx, 1                              \
    __asm and   eax, 0f81f0h                        \
    __asm and   ebx, 07e00h                         \
    __asm add   eax, ebx                            \
    __asm mov   word ptr [edi], ax                  \
    __asm pop   edx                                 \
    __asm pop   ecx                                 \
    __asm pop   ebx
#define BLIT_CORE_UNSCALED(Lrow, Lpix, Lblend, Lnext, XSTEPOP, ROWOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm xor   eax, eax                            \
    __asm xor   ebx, ebx                            \
    __asm xor   ecx, ecx                            \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm mov   edx, buffer_draw_width                    \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm XSTEPOP esi                               \
    __asm add   edi, 2                              \
    __asm dec   edx                                 \
    __asm jnz   Lpix                                \
    __asm ROWOP esi, sprite_advance               \
    __asm add   edi, buffer_advance          \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
#define BLIT_CORE_SCALED(Lrow, Lpix, Lblend, Lnext, CARRYOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm mov   edx, scale_y                \
    __asm shl   edx, 10h                            \
    __asm mov   dx, word ptr scale_x        \
    __asm mov   ebx, buffer_draw_height                   \
    __asm shl   ebx, 10h                            \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   ecx, buffer_draw_width                    \
    __asm dec   ecx                                 \
    __asm shl   ecx, 10h                            \
    __asm mov   buffer_draw_width, ecx                    \
    __asm xor   ecx, ecx                            \
    __asm mov   cx, word ptr [scale_x_inc+2]   \
    __asm mov   x_inc_int, ecx               \
    __asm mov   eax, scale_y_inc               \
    __asm shl   eax, 10h                            \
    __asm mov   frac_scale_y_inc, eax             \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm xor   eax, eax                            \
    __asm mov   ebx, buffer_draw_width                    \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   dx, word ptr scale_x        \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm add   dx, bx                              \
    __asm CARRYOP esi, x_inc_int             \
    __asm add   edi, 2                              \
    __asm sub   ebx, 10000h                         \
    __asm jns   Lpix                                \
    __asm add   edi, buffer_advance          \
    __asm add   edx, frac_scale_y_inc             \
    __asm sbb   eax, eax                            \
    __asm mov   ebx, y_advance_no_carry        \
    __asm add   esi, dword ptr [ebx + eax*4]        \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
int JGLSprite::draw_translucent_565_8_16_1_4(JGLBuffer *dst, int x, int y, char alphaTest)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u1row, u1pix, u1bl, u1nx, inc, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u2row, u2pix, u2bl, u2nx, inc, sub)
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u3row, u3pix, u3bl, u3nx, dec, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u4row, u4pix, u4bl, u4nx, dec, sub)
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s1row, s1pix, s1bl, s1nx, adc)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s2row, s2pix, s2bl, s2nx, adc)
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s3row, s3pix, s3bl, s3nx, sbb)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s4row, s4pix, s4bl, s4nx, sbb)
            }
        }
    }
    dst->release_data(2);
    return 0;
}
#define BLIT_TEST_AND_BLEND(Lblend, Lnext)          \
    __asm cmp   byte ptr [esi], 0ffh                \
    __asm jnc   Lnext                               \
    __asm or    byte ptr alphaTest, 0               \
    __asm jz    Lblend                              \
    __asm cmp   byte ptr [esi], 0f8h                \
    __asm jnc   Lnext                               \
    __asm Lblend:                                   \
    __asm push  ebx                                 \
    __asm push  ecx                                 \
    __asm push  edx                                 \
    __asm xor   eax, eax                            \
    __asm xor   edx, edx                            \
    __asm mov   dl, byte ptr [esi]                  \
    __asm mov   ax, word ptr [edi]                  \
    __asm mov   dx, word ptr [ecx + edx*2]          \
    __asm shl   eax, 4                              \
    __asm shl   edx, 4                              \
    __asm mov   ebx, eax                            \
    __asm mov   ecx, edx                            \
    __asm and   eax, 0f81f0h                        \
    __asm and   ebx, 07e00h                         \
    __asm and   ecx, 0f81f0h                        \
    __asm and   edx, 07e00h                         \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 1                              \
    __asm shr   ebx, 1                              \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 1                              \
    __asm shr   ebx, 1                              \
    __asm and   eax, 0f81f0h                        \
    __asm and   ebx, 07e00h                         \
    __asm add   eax, ebx                            \
    __asm mov   word ptr [edi], ax                  \
    __asm pop   edx                                 \
    __asm pop   ecx                                 \
    __asm pop   ebx
#define BLIT_CORE_UNSCALED(Lrow, Lpix, Lblend, Lnext, XSTEPOP, ROWOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm xor   eax, eax                            \
    __asm xor   ebx, ebx                            \
    __asm xor   ecx, ecx                            \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm mov   edx, buffer_draw_width                    \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm XSTEPOP esi                               \
    __asm add   edi, 2                              \
    __asm dec   edx                                 \
    __asm jnz   Lpix                                \
    __asm ROWOP esi, sprite_advance               \
    __asm add   edi, buffer_advance          \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
#define BLIT_CORE_SCALED(Lrow, Lpix, Lblend, Lnext, CARRYOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm mov   edx, scale_y                \
    __asm shl   edx, 10h                            \
    __asm mov   dx, word ptr scale_x        \
    __asm mov   ebx, buffer_draw_height                   \
    __asm shl   ebx, 10h                            \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   ecx, buffer_draw_width                    \
    __asm dec   ecx                                 \
    __asm shl   ecx, 10h                            \
    __asm mov   buffer_draw_width, ecx                    \
    __asm xor   ecx, ecx                            \
    __asm mov   cx, word ptr [scale_x_inc+2]   \
    __asm mov   x_inc_int, ecx               \
    __asm mov   eax, scale_y_inc               \
    __asm shl   eax, 10h                            \
    __asm mov   frac_scale_y_inc, eax             \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm xor   eax, eax                            \
    __asm mov   ebx, buffer_draw_width                    \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   dx, word ptr scale_x        \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm add   dx, bx                              \
    __asm CARRYOP esi, x_inc_int             \
    __asm add   edi, 2                              \
    __asm sub   ebx, 10000h                         \
    __asm jns   Lpix                                \
    __asm add   edi, buffer_advance          \
    __asm add   edx, frac_scale_y_inc             \
    __asm sbb   eax, eax                            \
    __asm mov   ebx, y_advance_no_carry        \
    __asm add   esi, dword ptr [ebx + eax*4]        \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
int JGLSprite::draw_translucent_565_8_16_3_4(JGLBuffer *dst, int x, int y, char alphaTest)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u1row, u1pix, u1bl, u1nx, inc, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u2row, u2pix, u2bl, u2nx, inc, sub)
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u3row, u3pix, u3bl, u3nx, dec, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u4row, u4pix, u4bl, u4nx, dec, sub)
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s1row, s1pix, s1bl, s1nx, adc)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s2row, s2pix, s2bl, s2nx, adc)
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s3row, s3pix, s3bl, s3nx, sbb)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s4row, s4pix, s4bl, s4nx, sbb)
            }
        }
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::draw_translucent_565_8_16_1_8(JGLBuffer *dst, int x, int y, char skipHigh)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (!IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip())) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C1_row:
                    mov  edx, buffer_draw_width
                C1_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C1_next
                    or   byte ptr skipHigh, 0
                    jz   C1_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C1_next
                C1_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C1_next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  C1_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C1_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C2_row:
                    mov  edx, buffer_draw_width
                C2_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C2_next
                    or   byte ptr skipHigh, 0
                    jz   C2_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C2_next
                C2_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C2_next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  C2_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C2_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C3_row:
                    mov  edx, buffer_draw_width
                C3_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C3_next
                    or   byte ptr skipHigh, 0
                    jz   C3_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C3_next
                C3_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C3_next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  C3_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C3_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C4_row:
                    mov  edx, buffer_draw_width
                C4_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C4_next
                    or   byte ptr skipHigh, 0
                    jz   C4_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C4_next
                C4_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C4_next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  C4_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C4_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
        saved = r;
        if (!jgl_IntersectRect(&r, &r, dst->get_clip())) {
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C5_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C5_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C5_next
                    or   byte ptr skipHigh, 0
                    jz   C5_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C5_next
                C5_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C5_next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C5_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C5_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C6_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C6_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C6_next
                    or   byte ptr skipHigh, 0
                    jz   C6_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C6_next
                C6_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C6_next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C6_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C6_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C7_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C7_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C7_next
                    or   byte ptr skipHigh, 0
                    jz   C7_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C7_next
                C7_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C7_next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C7_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C7_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C8_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C8_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C8_next
                    or   byte ptr skipHigh, 0
                    jz   C8_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C8_next
                C8_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C8_next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C8_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C8_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::draw_translucent_565_8_16_7_8(JGLBuffer *dst, int x, int y, char skipHigh)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (!IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip())) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C1_row:
                    mov  edx, buffer_draw_width
                C1_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C1_next
                    or   byte ptr skipHigh, 0
                    jz   C1_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C1_next
                C1_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C1_next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  C1_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C1_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C2_row:
                    mov  edx, buffer_draw_width
                C2_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C2_next
                    or   byte ptr skipHigh, 0
                    jz   C2_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C2_next
                C2_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C2_next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  C2_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C2_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C3_row:
                    mov  edx, buffer_draw_width
                C3_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C3_next
                    or   byte ptr skipHigh, 0
                    jz   C3_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C3_next
                C3_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C3_next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  C3_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C3_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C4_row:
                    mov  edx, buffer_draw_width
                C4_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C4_next
                    or   byte ptr skipHigh, 0
                    jz   C4_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C4_next
                C4_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C4_next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  C4_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C4_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
        saved = r;
        if (!jgl_IntersectRect(&r, &r, dst->get_clip())) {
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C5_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C5_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C5_next
                    or   byte ptr skipHigh, 0
                    jz   C5_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C5_next
                C5_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C5_next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C5_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C5_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C6_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C6_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C6_next
                    or   byte ptr skipHigh, 0
                    jz   C6_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C6_next
                C6_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C6_next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C6_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C6_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C7_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C7_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C7_next
                    or   byte ptr skipHigh, 0
                    jz   C7_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C7_next
                C7_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C7_next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C7_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C7_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C8_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C8_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C8_next
                    or   byte ptr skipHigh, 0
                    jz   C8_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C8_next
                C8_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C8_next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C8_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C8_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::draw_translucent_565_8_16_1_16(JGLBuffer *dst, int x, int y, char skipHigh)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (!IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip())) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C1_row:
                    mov  edx, buffer_draw_width
                C1_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C1_next
                    or   byte ptr skipHigh, 0
                    jz   C1_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C1_next
                C1_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 4
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 4
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 4
                    shr  ebx, 4
                    pop  esi
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C1_next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  C1_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C1_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C2_row:
                    mov  edx, buffer_draw_width
                C2_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C2_next
                    or   byte ptr skipHigh, 0
                    jz   C2_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C2_next
                C2_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 4
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 4
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 4
                    shr  ebx, 4
                    pop  esi
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C2_next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  C2_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C2_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C3_row:
                    mov  edx, buffer_draw_width
                C3_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C3_next
                    or   byte ptr skipHigh, 0
                    jz   C3_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C3_next
                C3_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 4
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 4
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 4
                    shr  ebx, 4
                    pop  esi
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C3_next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  C3_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C3_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C4_row:
                    mov  edx, buffer_draw_width
                C4_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C4_next
                    or   byte ptr skipHigh, 0
                    jz   C4_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C4_next
                C4_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 4
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 4
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 4
                    shr  ebx, 4
                    pop  esi
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C4_next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  C4_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C4_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
        saved = r;
        if (!jgl_IntersectRect(&r, &r, dst->get_clip())) {
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C5_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C5_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C5_next
                    or   byte ptr skipHigh, 0
                    jz   C5_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C5_next
                C5_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 4
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 4
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 4
                    shr  ebx, 4
                    pop  esi
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C5_next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C5_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C5_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C6_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C6_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C6_next
                    or   byte ptr skipHigh, 0
                    jz   C6_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C6_next
                C6_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 4
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 4
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 4
                    shr  ebx, 4
                    pop  esi
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C6_next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C6_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C6_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C7_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C7_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C7_next
                    or   byte ptr skipHigh, 0
                    jz   C7_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C7_next
                C7_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 4
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 4
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 4
                    shr  ebx, 4
                    pop  esi
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C7_next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C7_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C7_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C8_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C8_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C8_next
                    or   byte ptr skipHigh, 0
                    jz   C8_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C8_next
                C8_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 4
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 4
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 4
                    shr  ebx, 4
                    pop  esi
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C8_next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C8_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C8_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::draw_translucent_565_8_16_15_16(JGLBuffer *dst, int x, int y, char skipHigh)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (!IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip())) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C1_row:
                    mov  edx, buffer_draw_width
                C1_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C1_next
                    or   byte ptr skipHigh, 0
                    jz   C1_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C1_next
                C1_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 4
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 4
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 4
                    shr  ebx, 4
                    pop  esi
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C1_next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  C1_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C1_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C2_row:
                    mov  edx, buffer_draw_width
                C2_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C2_next
                    or   byte ptr skipHigh, 0
                    jz   C2_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C2_next
                C2_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 4
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 4
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 4
                    shr  ebx, 4
                    pop  esi
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C2_next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  C2_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C2_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C3_row:
                    mov  edx, buffer_draw_width
                C3_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C3_next
                    or   byte ptr skipHigh, 0
                    jz   C3_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C3_next
                C3_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 4
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 4
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 4
                    shr  ebx, 4
                    pop  esi
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C3_next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  C3_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C3_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C4_row:
                    mov  edx, buffer_draw_width
                C4_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C4_next
                    or   byte ptr skipHigh, 0
                    jz   C4_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C4_next
                C4_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 4
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 4
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 4
                    shr  ebx, 4
                    pop  esi
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C4_next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  C4_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C4_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
        saved = r;
        if (!jgl_IntersectRect(&r, &r, dst->get_clip())) {
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C5_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C5_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C5_next
                    or   byte ptr skipHigh, 0
                    jz   C5_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C5_next
                C5_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 4
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 4
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 4
                    shr  ebx, 4
                    pop  esi
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C5_next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C5_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C5_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C6_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C6_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C6_next
                    or   byte ptr skipHigh, 0
                    jz   C6_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C6_next
                C6_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 4
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 4
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 4
                    shr  ebx, 4
                    pop  esi
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C6_next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C6_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C6_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C7_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C7_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C7_next
                    or   byte ptr skipHigh, 0
                    jz   C7_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C7_next
                C7_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 4
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 4
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 4
                    shr  ebx, 4
                    pop  esi
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C7_next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C7_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C7_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C8_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C8_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C8_next
                    or   byte ptr skipHigh, 0
                    jz   C8_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C8_next
                C8_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 4
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 4
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 4
                    shr  ebx, 4
                    pop  esi
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C8_next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C8_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C8_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::draw_translucent_565_8_16_3_8(JGLBuffer *dst, int x, int y, char skipHigh)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (!IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip())) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C1_row:
                    mov  edx, buffer_draw_width
                C1_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C1_next
                    or   byte ptr skipHigh, 0
                    jz   C1_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C1_next
                C1_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C1_next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  C1_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C1_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C2_row:
                    mov  edx, buffer_draw_width
                C2_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C2_next
                    or   byte ptr skipHigh, 0
                    jz   C2_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C2_next
                C2_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C2_next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  C2_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C2_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C3_row:
                    mov  edx, buffer_draw_width
                C3_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C3_next
                    or   byte ptr skipHigh, 0
                    jz   C3_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C3_next
                C3_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C3_next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  C3_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C3_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C4_row:
                    mov  edx, buffer_draw_width
                C4_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C4_next
                    or   byte ptr skipHigh, 0
                    jz   C4_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C4_next
                C4_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C4_next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  C4_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C4_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
        saved = r;
        if (!jgl_IntersectRect(&r, &r, dst->get_clip())) {
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C5_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C5_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C5_next
                    or   byte ptr skipHigh, 0
                    jz   C5_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C5_next
                C5_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C5_next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C5_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C5_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C6_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C6_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C6_next
                    or   byte ptr skipHigh, 0
                    jz   C6_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C6_next
                C6_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C6_next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C6_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C6_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C7_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C7_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C7_next
                    or   byte ptr skipHigh, 0
                    jz   C7_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C7_next
                C7_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C7_next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C7_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C7_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C8_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C8_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C8_next
                    or   byte ptr skipHigh, 0
                    jz   C8_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C8_next
                C8_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C8_next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C8_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C8_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::draw_translucent_565_8_16_5_8(JGLBuffer *dst, int x, int y, char skipHigh)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (!IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip())) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C1_row:
                    mov  edx, buffer_draw_width
                C1_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C1_next
                    or   byte ptr skipHigh, 0
                    jz   C1_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C1_next
                C1_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C1_next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  C1_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C1_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C2_row:
                    mov  edx, buffer_draw_width
                C2_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C2_next
                    or   byte ptr skipHigh, 0
                    jz   C2_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C2_next
                C2_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C2_next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  C2_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C2_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C3_row:
                    mov  edx, buffer_draw_width
                C3_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C3_next
                    or   byte ptr skipHigh, 0
                    jz   C3_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C3_next
                C3_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C3_next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  C3_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C3_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C4_row:
                    mov  edx, buffer_draw_width
                C4_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C4_next
                    or   byte ptr skipHigh, 0
                    jz   C4_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C4_next
                C4_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C4_next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  C4_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C4_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
        saved = r;
        if (!jgl_IntersectRect(&r, &r, dst->get_clip())) {
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C5_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C5_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C5_next
                    or   byte ptr skipHigh, 0
                    jz   C5_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C5_next
                C5_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C5_next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C5_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C5_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C6_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C6_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C6_next
                    or   byte ptr skipHigh, 0
                    jz   C6_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C6_next
                C6_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C6_next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C6_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C6_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C7_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C7_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C7_next
                    or   byte ptr skipHigh, 0
                    jz   C7_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C7_next
                C7_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C7_next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C7_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C7_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C8_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C8_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C8_next
                    or   byte ptr skipHigh, 0
                    jz   C8_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C8_next
                C8_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 2
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 2
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 2
                    shr  ebx, 2
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C8_next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C8_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C8_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::draw_translucent_565_8_16_7_16(JGLBuffer *dst, int x, int y, char skipHigh)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (!IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip())) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C1_row:
                    mov  edx, buffer_draw_width
                C1_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C1_next
                    or   byte ptr skipHigh, 0
                    jz   C1_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C1_next
                C1_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C1_next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  C1_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C1_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C2_row:
                    mov  edx, buffer_draw_width
                C2_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C2_next
                    or   byte ptr skipHigh, 0
                    jz   C2_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C2_next
                C2_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C2_next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  C2_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C2_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C3_row:
                    mov  edx, buffer_draw_width
                C3_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C3_next
                    or   byte ptr skipHigh, 0
                    jz   C3_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C3_next
                C3_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C3_next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  C3_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C3_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C4_row:
                    mov  edx, buffer_draw_width
                C4_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C4_next
                    or   byte ptr skipHigh, 0
                    jz   C4_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C4_next
                C4_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C4_next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  C4_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C4_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
        saved = r;
        if (!jgl_IntersectRect(&r, &r, dst->get_clip())) {
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C5_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C5_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C5_next
                    or   byte ptr skipHigh, 0
                    jz   C5_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C5_next
                C5_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C5_next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C5_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C5_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C6_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C6_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C6_next
                    or   byte ptr skipHigh, 0
                    jz   C6_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C6_next
                C6_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C6_next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C6_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C6_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C7_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C7_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C7_next
                    or   byte ptr skipHigh, 0
                    jz   C7_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C7_next
                C7_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C7_next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C7_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C7_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C8_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C8_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C8_next
                    or   byte ptr skipHigh, 0
                    jz   C8_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C8_next
                C8_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  al, byte ptr [esi]
                    mov  dx, word ptr [edi]
                    mov  ax, word ptr [ecx + eax*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C8_next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C8_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C8_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    }
    dst->release_data(2);
    return 0;
}
int JGLSprite::draw_translucent_565_8_16_9_16(JGLBuffer *dst, int x, int y, char skipHigh)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (!IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip())) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C1_row:
                    mov  edx, buffer_draw_width
                C1_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C1_next
                    or   byte ptr skipHigh, 0
                    jz   C1_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C1_next
                C1_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C1_next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  C1_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C1_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C2_row:
                    mov  edx, buffer_draw_width
                C2_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C2_next
                    or   byte ptr skipHigh, 0
                    jz   C2_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C2_next
                C2_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C2_next:
                    inc  esi
                    add  edi, 2
                    dec  edx
                    jnz  C2_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C2_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C3_row:
                    mov  edx, buffer_draw_width
                C3_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C3_next
                    or   byte ptr skipHigh, 0
                    jz   C3_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C3_next
                C3_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C3_next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  C3_pix
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C3_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    push ecx
                    mov  ecx, _table
                C4_row:
                    mov  edx, buffer_draw_width
                C4_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C4_next
                    or   byte ptr skipHigh, 0
                    jz   C4_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C4_next
                C4_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C4_next:
                    dec  esi
                    add  edi, 2
                    dec  edx
                    jnz  C4_pix
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  C4_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
        saved = r;
        if (!jgl_IntersectRect(&r, &r, dst->get_clip())) {
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C5_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C5_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C5_next
                    or   byte ptr skipHigh, 0
                    jz   C5_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C5_next
                C5_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C5_next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C5_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C5_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C6_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C6_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C6_next
                    or   byte ptr skipHigh, 0
                    jz   C6_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C6_next
                C6_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C6_next:
                    add  dx, bx
                    adc  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C6_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C6_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C7_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C7_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C7_next
                    or   byte ptr skipHigh, 0
                    jz   C7_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C7_next
                C7_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C7_next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C7_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C7_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                __asm {
                    push esi
                    push edi
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  edx, scale_y
                    shl  edx, 10h
                    mov  dx, word ptr scale_x
                    mov  ebx, buffer_draw_height
                    shl  ebx, 10h
                    mov  bx, word ptr scale_x_inc
                    mov  ecx, buffer_draw_width
                    dec  ecx
                    shl  ecx, 10h
                    mov  buffer_draw_width, ecx
                    xor  ecx, ecx
                    mov  cx, word ptr [scale_x_inc + 2]
                    mov  x_inc_int, ecx
                    mov  eax, scale_y_inc
                    shl  eax, 10h
                    mov  frac_scale_y_inc, eax
                    push ecx
                    mov  ecx, _table
                C8_row:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr scale_x_inc
                    mov  dx, word ptr scale_x
                C8_pix:
                    cmp  byte ptr [esi], 0ffh
                    jnc  C8_next
                    or   byte ptr skipHigh, 0
                    jz   C8_blend
                    cmp  byte ptr [esi], 0f8h
                    jnc  C8_next
                C8_blend:
                    push ebx
                    push ecx
                    push edx
                    xor  eax, eax
                    xor  edx, edx
                    mov  dl, byte ptr [esi]
                    mov  ax, word ptr [edi]
                    mov  dx, word ptr [ecx + edx*2]
                    shl  eax, 4
                    shl  edx, 4
                    mov  ebx, eax
                    mov  ecx, edx
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    and  ecx, 0f81f0h
                    and  edx, 07e00h
                    push esi
                    mov  esi, eax
                    shl  eax, 3
                    sub  eax, esi
                    mov  esi, ebx
                    shl  ebx, 3
                    sub  ebx, esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 3
                    shr  ebx, 3
                    pop  esi
                    add  eax, ecx
                    add  ebx, edx
                    shr  eax, 1
                    shr  ebx, 1
                    and  eax, 0f81f0h
                    and  ebx, 07e00h
                    add  eax, ebx
                    mov  word ptr [edi], ax
                    pop  edx
                    pop  ecx
                    pop  ebx
                C8_next:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    add  edi, 2
                    sub  ebx, 10000h
                    jns  C8_pix
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, dword ptr [ebx + eax*4]
                    dec  buffer_draw_height
                    jnz  C8_row
                    pop  ecx
                    pop  edi
                    pop  esi
                }
            }
        }
    }
    dst->release_data(2);
    return 0;
}
#define BLIT_TEST_AND_BLEND(Lblend, Lnext)          \
    __asm cmp   byte ptr [esi], 0ffh                \
    __asm jnc   Lnext                               \
    __asm or    byte ptr alphaTest, 0               \
    __asm jz    Lblend                              \
    __asm cmp   byte ptr [esi], 0f8h                \
    __asm jnc   Lnext                               \
    __asm Lblend:                                   \
    __asm push  ebx                                 \
    __asm push  ecx                                 \
    __asm push  edx                                 \
    __asm xor   eax, eax                            \
    __asm xor   edx, edx                            \
    __asm mov   dl, byte ptr [esi]                  \
    __asm mov   ax, word ptr [edi]                  \
    __asm mov   dx, word ptr [ecx + edx*2]          \
    __asm shl   eax, 4                              \
    __asm shl   edx, 4                              \
    __asm mov   ebx, eax                            \
    __asm mov   ecx, edx                            \
    __asm and   eax, 0f81f0h                        \
    __asm and   ebx, 7e00h                          \
    __asm and   ecx, 0f81f0h                        \
    __asm and   edx, 7e00h                          \
    __asm push  esi                                 \
    __asm mov   esi, eax                            \
    __asm shl   eax, 3                              \
    __asm sub   eax, esi                            \
    __asm add   eax, ecx                            \
    __asm shl   eax, 1                              \
    __asm sub   eax, esi                            \
    __asm mov   esi, ebx                            \
    __asm shl   ebx, 3                              \
    __asm sub   ebx, esi                            \
    __asm add   ebx, edx                            \
    __asm shl   ebx, 1                              \
    __asm sub   ebx, esi                            \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 4                              \
    __asm shr   ebx, 4                              \
    __asm pop   esi                                 \
    __asm and   eax, 0f81f0h                        \
    __asm and   ebx, 7e00h                          \
    __asm add   eax, ebx                            \
    __asm mov   word ptr [edi], ax                  \
    __asm pop   edx                                 \
    __asm pop   ecx                                 \
    __asm pop   ebx
#define BLIT_CORE_UNSCALED(Lrow, Lpix, Lblend, Lnext, XSTEPOP, ROWOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm xor   eax, eax                            \
    __asm xor   ebx, ebx                            \
    __asm xor   ecx, ecx                            \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm mov   edx, buffer_draw_width                    \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm XSTEPOP esi                               \
    __asm add   edi, 2                              \
    __asm dec   edx                                 \
    __asm jnz   Lpix                                \
    __asm ROWOP esi, sprite_advance               \
    __asm add   edi, buffer_advance          \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
#define BLIT_CORE_SCALED(Lrow, Lpix, Lblend, Lnext, CARRYOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm mov   edx, scale_y                \
    __asm shl   edx, 10h                            \
    __asm mov   dx, word ptr scale_x        \
    __asm mov   ebx, buffer_draw_height                   \
    __asm shl   ebx, 10h                            \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   ecx, buffer_draw_width                    \
    __asm dec   ecx                                 \
    __asm shl   ecx, 10h                            \
    __asm mov   buffer_draw_width, ecx                    \
    __asm xor   ecx, ecx                            \
    __asm mov   cx, word ptr [scale_x_inc+2]   \
    __asm mov   x_inc_int, ecx               \
    __asm mov   eax, scale_y_inc               \
    __asm shl   eax, 10h                            \
    __asm mov   frac_scale_y_inc, eax             \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm xor   eax, eax                            \
    __asm mov   ebx, buffer_draw_width                    \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   dx, word ptr scale_x        \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm add   dx, bx                              \
    __asm CARRYOP esi, x_inc_int             \
    __asm add   edi, 2                              \
    __asm sub   ebx, 10000h                         \
    __asm jns   Lpix                                \
    __asm add   edi, buffer_advance          \
    __asm add   edx, frac_scale_y_inc             \
    __asm sbb   eax, eax                            \
    __asm mov   ebx, y_advance_no_carry        \
    __asm add   esi, dword ptr [ebx + eax*4]        \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
int JGLSprite::draw_translucent_565_8_16_3_16(JGLBuffer *dst, int x, int y, char alphaTest)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u1row, u1pix, u1bl, u1nx, inc, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u2row, u2pix, u2bl, u2nx, inc, sub)
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u3row, u3pix, u3bl, u3nx, dec, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u4row, u4pix, u4bl, u4nx, dec, sub)
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s1row, s1pix, s1bl, s1nx, adc)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s2row, s2pix, s2bl, s2nx, adc)
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s3row, s3pix, s3bl, s3nx, sbb)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s4row, s4pix, s4bl, s4nx, sbb)
            }
        }
    }
    dst->release_data(2);
    return 0;
}
#define BLIT_TEST_AND_BLEND(Lblend, Lnext)          \
    __asm cmp   byte ptr [esi], 0ffh                \
    __asm jnc   Lnext                               \
    __asm or    byte ptr alphaTest, 0               \
    __asm jz    Lblend                              \
    __asm cmp   byte ptr [esi], 0f8h                \
    __asm jnc   Lnext                               \
    __asm Lblend:                                   \
    __asm push  ebx                                 \
    __asm push  ecx                                 \
    __asm push  edx                                 \
    __asm xor   eax, eax                            \
    __asm xor   edx, edx                            \
    __asm mov   al, byte ptr [esi]                  \
    __asm mov   dx, word ptr [edi]                  \
    __asm mov   ax, word ptr [ecx + eax*2]          \
    __asm shl   eax, 4                              \
    __asm shl   edx, 4                              \
    __asm mov   ebx, eax                            \
    __asm mov   ecx, edx                            \
    __asm and   eax, 0f81f0h                        \
    __asm and   ebx, 7e00h                          \
    __asm and   ecx, 0f81f0h                        \
    __asm and   edx, 7e00h                          \
    __asm push  esi                                 \
    __asm mov   esi, eax                            \
    __asm shl   eax, 3                              \
    __asm sub   eax, esi                            \
    __asm add   eax, ecx                            \
    __asm shl   eax, 1                              \
    __asm sub   eax, esi                            \
    __asm mov   esi, ebx                            \
    __asm shl   ebx, 3                              \
    __asm sub   ebx, esi                            \
    __asm add   ebx, edx                            \
    __asm shl   ebx, 1                              \
    __asm sub   ebx, esi                            \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 4                              \
    __asm shr   ebx, 4                              \
    __asm pop   esi                                 \
    __asm and   eax, 0f81f0h                        \
    __asm and   ebx, 7e00h                          \
    __asm add   eax, ebx                            \
    __asm mov   word ptr [edi], ax                  \
    __asm pop   edx                                 \
    __asm pop   ecx                                 \
    __asm pop   ebx
#define BLIT_CORE_UNSCALED(Lrow, Lpix, Lblend, Lnext, XSTEPOP, ROWOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm xor   eax, eax                            \
    __asm xor   ebx, ebx                            \
    __asm xor   ecx, ecx                            \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm mov   edx, buffer_draw_width                    \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm XSTEPOP esi                               \
    __asm add   edi, 2                              \
    __asm dec   edx                                 \
    __asm jnz   Lpix                                \
    __asm ROWOP esi, sprite_advance               \
    __asm add   edi, buffer_advance          \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
#define BLIT_CORE_SCALED(Lrow, Lpix, Lblend, Lnext, CARRYOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm mov   edx, scale_y                \
    __asm shl   edx, 10h                            \
    __asm mov   dx, word ptr scale_x        \
    __asm mov   ebx, buffer_draw_height                   \
    __asm shl   ebx, 10h                            \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   ecx, buffer_draw_width                    \
    __asm dec   ecx                                 \
    __asm shl   ecx, 10h                            \
    __asm mov   buffer_draw_width, ecx                    \
    __asm xor   ecx, ecx                            \
    __asm mov   cx, word ptr [scale_x_inc+2]   \
    __asm mov   x_inc_int, ecx               \
    __asm mov   eax, scale_y_inc               \
    __asm shl   eax, 10h                            \
    __asm mov   frac_scale_y_inc, eax             \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm xor   eax, eax                            \
    __asm mov   ebx, buffer_draw_width                    \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   dx, word ptr scale_x        \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm add   dx, bx                              \
    __asm CARRYOP esi, x_inc_int             \
    __asm add   edi, 2                              \
    __asm sub   ebx, 10000h                         \
    __asm jns   Lpix                                \
    __asm add   edi, buffer_advance          \
    __asm add   edx, frac_scale_y_inc             \
    __asm sbb   eax, eax                            \
    __asm mov   ebx, y_advance_no_carry        \
    __asm add   esi, dword ptr [ebx + eax*4]        \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
int JGLSprite::draw_translucent_565_8_16_13_16(JGLBuffer *dst, int x, int y, char alphaTest)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u1row, u1pix, u1bl, u1nx, inc, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u2row, u2pix, u2bl, u2nx, inc, sub)
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u3row, u3pix, u3bl, u3nx, dec, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u4row, u4pix, u4bl, u4nx, dec, sub)
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s1row, s1pix, s1bl, s1nx, adc)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s2row, s2pix, s2bl, s2nx, adc)
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s3row, s3pix, s3bl, s3nx, sbb)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s4row, s4pix, s4bl, s4nx, sbb)
            }
        }
    }
    dst->release_data(2);
    return 0;
}
#define BLIT_TEST_AND_BLEND(Lblend, Lnext)          \
    __asm cmp   byte ptr [esi], 0ffh                \
    __asm jnc   Lnext                               \
    __asm or    byte ptr alphaTest, 0               \
    __asm jz    Lblend                              \
    __asm cmp   byte ptr [esi], 0f8h                \
    __asm jnc   Lnext                               \
    __asm Lblend:                                   \
    __asm push  ebx                                 \
    __asm push  ecx                                 \
    __asm push  edx                                 \
    __asm xor   eax, eax                            \
    __asm xor   edx, edx                            \
    __asm mov   dl, byte ptr [esi]                  \
    __asm mov   ax, word ptr [edi]                  \
    __asm mov   dx, word ptr [ecx + edx*2]          \
    __asm shl   eax, 4                              \
    __asm shl   edx, 4                              \
    __asm mov   ebx, eax                            \
    __asm mov   ecx, edx                            \
    __asm and   eax, 0f81f0h                        \
    __asm and   ebx, 7e00h                          \
    __asm and   ecx, 0f81f0h                        \
    __asm and   edx, 7e00h                          \
    __asm push  esi                                 \
    __asm mov   esi, eax                            \
    __asm shl   eax, 2                              \
    __asm sub   eax, esi                            \
    __asm add   eax, ecx                            \
    __asm shl   eax, 2                              \
    __asm sub   eax, esi                            \
    __asm mov   esi, ebx                            \
    __asm shl   ebx, 2                              \
    __asm sub   ebx, esi                            \
    __asm add   ebx, edx                            \
    __asm shl   ebx, 2                              \
    __asm sub   ebx, esi                            \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 4                              \
    __asm shr   ebx, 4                              \
    __asm pop   esi                                 \
    __asm and   eax, 0f81f0h                        \
    __asm and   ebx, 7e00h                          \
    __asm add   eax, ebx                            \
    __asm mov   word ptr [edi], ax                  \
    __asm pop   edx                                 \
    __asm pop   ecx                                 \
    __asm pop   ebx
#define BLIT_CORE_UNSCALED(Lrow, Lpix, Lblend, Lnext, XSTEPOP, ROWOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm xor   eax, eax                            \
    __asm xor   ebx, ebx                            \
    __asm xor   ecx, ecx                            \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm mov   edx, buffer_draw_width                    \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm XSTEPOP esi                               \
    __asm add   edi, 2                              \
    __asm dec   edx                                 \
    __asm jnz   Lpix                                \
    __asm ROWOP esi, sprite_advance               \
    __asm add   edi, buffer_advance          \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
#define BLIT_CORE_SCALED(Lrow, Lpix, Lblend, Lnext, CARRYOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm mov   edx, scale_y                \
    __asm shl   edx, 10h                            \
    __asm mov   dx, word ptr scale_x        \
    __asm mov   ebx, buffer_draw_height                   \
    __asm shl   ebx, 10h                            \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   ecx, buffer_draw_width                    \
    __asm dec   ecx                                 \
    __asm shl   ecx, 10h                            \
    __asm mov   buffer_draw_width, ecx                    \
    __asm xor   ecx, ecx                            \
    __asm mov   cx, word ptr [scale_x_inc+2]   \
    __asm mov   x_inc_int, ecx               \
    __asm mov   eax, scale_y_inc               \
    __asm shl   eax, 10h                            \
    __asm mov   frac_scale_y_inc, eax             \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm xor   eax, eax                            \
    __asm mov   ebx, buffer_draw_width                    \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   dx, word ptr scale_x        \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm add   dx, bx                              \
    __asm CARRYOP esi, x_inc_int             \
    __asm add   edi, 2                              \
    __asm sub   ebx, 10000h                         \
    __asm jns   Lpix                                \
    __asm add   edi, buffer_advance          \
    __asm add   edx, frac_scale_y_inc             \
    __asm sbb   eax, eax                            \
    __asm mov   ebx, y_advance_no_carry        \
    __asm add   esi, dword ptr [ebx + eax*4]        \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
int JGLSprite::draw_translucent_565_8_16_5_16(JGLBuffer *dst, int x, int y, char alphaTest)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u1row, u1pix, u1bl, u1nx, inc, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u2row, u2pix, u2bl, u2nx, inc, sub)
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u3row, u3pix, u3bl, u3nx, dec, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u4row, u4pix, u4bl, u4nx, dec, sub)
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s1row, s1pix, s1bl, s1nx, adc)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s2row, s2pix, s2bl, s2nx, adc)
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s3row, s3pix, s3bl, s3nx, sbb)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s4row, s4pix, s4bl, s4nx, sbb)
            }
        }
    }
    dst->release_data(2);
    return 0;
}
#define BLIT_TEST_AND_BLEND(Lblend, Lnext)          \
    __asm cmp   byte ptr [esi], 0ffh                \
    __asm jnc   Lnext                               \
    __asm or    byte ptr alphaTest, 0               \
    __asm jz    Lblend                              \
    __asm cmp   byte ptr [esi], 0f8h                \
    __asm jnc   Lnext                               \
    __asm Lblend:                                   \
    __asm push  ebx                                 \
    __asm push  ecx                                 \
    __asm push  edx                                 \
    __asm xor   eax, eax                            \
    __asm xor   edx, edx                            \
    __asm mov   al, byte ptr [esi]                  \
    __asm mov   dx, word ptr [edi]                  \
    __asm mov   ax, word ptr [ecx + eax*2]          \
    __asm shl   eax, 4                              \
    __asm shl   edx, 4                              \
    __asm mov   ebx, eax                            \
    __asm mov   ecx, edx                            \
    __asm and   eax, 0f81f0h                        \
    __asm and   ebx, 7e00h                          \
    __asm and   ecx, 0f81f0h                        \
    __asm and   edx, 7e00h                          \
    __asm push  esi                                 \
    __asm mov   esi, eax                            \
    __asm shl   eax, 2                              \
    __asm sub   eax, esi                            \
    __asm add   eax, ecx                            \
    __asm shl   eax, 2                              \
    __asm sub   eax, esi                            \
    __asm mov   esi, ebx                            \
    __asm shl   ebx, 2                              \
    __asm sub   ebx, esi                            \
    __asm add   ebx, edx                            \
    __asm shl   ebx, 2                              \
    __asm sub   ebx, esi                            \
    __asm add   eax, ecx                            \
    __asm add   ebx, edx                            \
    __asm shr   eax, 4                              \
    __asm shr   ebx, 4                              \
    __asm pop   esi                                 \
    __asm and   eax, 0f81f0h                        \
    __asm and   ebx, 7e00h                          \
    __asm add   eax, ebx                            \
    __asm mov   word ptr [edi], ax                  \
    __asm pop   edx                                 \
    __asm pop   ecx                                 \
    __asm pop   ebx
#define BLIT_CORE_UNSCALED(Lrow, Lpix, Lblend, Lnext, XSTEPOP, ROWOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm xor   eax, eax                            \
    __asm xor   ebx, ebx                            \
    __asm xor   ecx, ecx                            \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm mov   edx, buffer_draw_width                    \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm XSTEPOP esi                               \
    __asm add   edi, 2                              \
    __asm dec   edx                                 \
    __asm jnz   Lpix                                \
    __asm ROWOP esi, sprite_advance               \
    __asm add   edi, buffer_advance          \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
#define BLIT_CORE_SCALED(Lrow, Lpix, Lblend, Lnext, CARRYOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm mov   edx, scale_y                \
    __asm shl   edx, 10h                            \
    __asm mov   dx, word ptr scale_x        \
    __asm mov   ebx, buffer_draw_height                   \
    __asm shl   ebx, 10h                            \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   ecx, buffer_draw_width                    \
    __asm dec   ecx                                 \
    __asm shl   ecx, 10h                            \
    __asm mov   buffer_draw_width, ecx                    \
    __asm xor   ecx, ecx                            \
    __asm mov   cx, word ptr [scale_x_inc+2]   \
    __asm mov   x_inc_int, ecx               \
    __asm mov   eax, scale_y_inc               \
    __asm shl   eax, 10h                            \
    __asm mov   frac_scale_y_inc, eax             \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm xor   eax, eax                            \
    __asm mov   ebx, buffer_draw_width                    \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   dx, word ptr scale_x        \
    __asm Lpix:                                     \
    BLIT_TEST_AND_BLEND(Lblend, Lnext)              \
    __asm Lnext:                                    \
    __asm add   dx, bx                              \
    __asm CARRYOP esi, x_inc_int             \
    __asm add   edi, 2                              \
    __asm sub   ebx, 10000h                         \
    __asm jns   Lpix                                \
    __asm add   edi, buffer_advance          \
    __asm add   edx, frac_scale_y_inc             \
    __asm sbb   eax, eax                            \
    __asm mov   ebx, y_advance_no_carry        \
    __asm add   esi, dword ptr [ebx + eax*4]        \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
int JGLSprite::draw_translucent_565_8_16_11_16(JGLBuffer *dst, int x, int y, char alphaTest)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u1row, u1pix, u1bl, u1nx, inc, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u2row, u2pix, u2bl, u2nx, inc, sub)
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u3row, u3pix, u3bl, u3nx, dec, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u4row, u4pix, u4bl, u4nx, dec, sub)
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s1row, s1pix, s1bl, s1nx, adc)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s2row, s2pix, s2bl, s2nx, adc)
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s3row, s3pix, s3bl, s3nx, sbb)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s4row, s4pix, s4bl, s4nx, sbb)
            }
        }
    }
    dst->release_data(2);
    return 0;
}
#define BLIT_TEST_AND_STIPPLE(Lnext)                \
    __asm cmp   byte ptr [esi], 0ffh                \
    __asm jnc   Lnext                               \
    __asm mov   eax, edx                            \
    __asm and   eax, 1                              \
    __asm jnz   Lnext                               \
    __asm mov   eax, buffer_draw_height                   \
    __asm and   eax, 1                              \
    __asm jnz   Lnext                               \
    __asm xor   eax, eax                            \
    __asm mov   al, byte ptr [esi]                  \
    __asm mov   ax, word ptr [ecx + eax*2]          \
    __asm mov   word ptr [edi], ax
#define BLIT_CORE_UNSCALED(Lrow, Lpix, Lnext, XSTEPOP, ROWOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm xor   eax, eax                            \
    __asm xor   ebx, ebx                            \
    __asm xor   ecx, ecx                            \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm mov   edx, buffer_draw_width                    \
    __asm Lpix:                                     \
    BLIT_TEST_AND_STIPPLE(Lnext)                    \
    __asm Lnext:                                    \
    __asm XSTEPOP esi                               \
    __asm add   edi, 2                              \
    __asm dec   edx                                 \
    __asm jnz   Lpix                                \
    __asm ROWOP esi, sprite_advance               \
    __asm add   edi, buffer_advance          \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
#define BLIT_CORE_SCALED(Lrow, Lpix, Lnext, CARRYOP)\
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm mov   edx, scale_y                \
    __asm shl   edx, 10h                            \
    __asm mov   dx, word ptr scale_x        \
    __asm mov   ebx, buffer_draw_height                   \
    __asm shl   ebx, 10h                            \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   ecx, buffer_draw_width                    \
    __asm dec   ecx                                 \
    __asm shl   ecx, 10h                            \
    __asm mov   buffer_draw_width, ecx                    \
    __asm xor   ecx, ecx                            \
    __asm mov   cx, word ptr [scale_x_inc+2]   \
    __asm mov   x_inc_int, ecx               \
    __asm mov   eax, scale_y_inc               \
    __asm shl   eax, 10h                            \
    __asm mov   frac_scale_y_inc, eax             \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm xor   eax, eax                            \
    __asm mov   ebx, buffer_draw_width                    \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   dx, word ptr scale_x        \
    __asm Lpix:                                     \
    BLIT_TEST_AND_STIPPLE(Lnext)                    \
    __asm Lnext:                                    \
    __asm add   dx, bx                              \
    __asm CARRYOP esi, x_inc_int             \
    __asm add   edi, 2                              \
    __asm sub   ebx, 10000h                         \
    __asm jns   Lpix                                \
    __asm add   edi, buffer_advance          \
    __asm add   edx, frac_scale_y_inc             \
    __asm sbb   eax, eax                            \
    __asm mov   ebx, y_advance_no_carry        \
    __asm add   esi, dword ptr [ebx + eax*4]        \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
int JGLSprite::draw_sudo_trans_8_16_50(JGLBuffer *dst, int x, int y)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u1row, u1pix, u1nx, inc, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u2row, u2pix, u2nx, inc, sub)
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u3row, u3pix, u3nx, dec, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u4row, u4pix, u4nx, dec, sub)
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s1row, s1pix, s1nx, adc)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s2row, s2pix, s2nx, adc)
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s3row, s3pix, s3nx, sbb)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s4row, s4pix, s4nx, sbb)
            }
        }
    }
    dst->release_data(2);
    return 0;
}
#define BLIT_TEST_AND_STIPPLE(Lnext)                \
    __asm cmp   byte ptr [esi], 0ffh                \
    __asm jnc   Lnext                               \
    __asm mov   eax, edx                            \
    __asm and   eax, 1                              \
    __asm jnz   Lnext                               \
    __asm mov   eax, buffer_draw_height                   \
    __asm and   eax, 1                              \
    __asm jnz   Lnext                               \
    __asm mov   eax, edx                            \
    __asm add   eax, buffer_draw_height                   \
    __asm and   eax, 2                              \
    __asm jnz   Lnext                               \
    __asm xor   eax, eax                            \
    __asm mov   al, byte ptr [esi]                  \
    __asm mov   ax, word ptr [ecx + eax*2]          \
    __asm mov   word ptr [edi], ax
#define BLIT_CORE_UNSCALED(Lrow, Lpix, Lnext, XSTEPOP, ROWOP) \
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm xor   eax, eax                            \
    __asm xor   ebx, ebx                            \
    __asm xor   ecx, ecx                            \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm mov   edx, buffer_draw_width                    \
    __asm Lpix:                                     \
    BLIT_TEST_AND_STIPPLE(Lnext)                    \
    __asm Lnext:                                    \
    __asm XSTEPOP esi                               \
    __asm add   edi, 2                              \
    __asm dec   edx                                 \
    __asm jnz   Lpix                                \
    __asm ROWOP esi, sprite_advance               \
    __asm add   edi, buffer_advance          \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
#define BLIT_CORE_SCALED(Lrow, Lpix, Lnext, CARRYOP)\
    __asm push  esi                                 \
    __asm push  edi                                 \
    __asm mov   esi, srcp                           \
    __asm mov   edi, dstp                           \
    __asm mov   edx, scale_y                \
    __asm shl   edx, 10h                            \
    __asm mov   dx, word ptr scale_x        \
    __asm mov   ebx, buffer_draw_height                   \
    __asm shl   ebx, 10h                            \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   ecx, buffer_draw_width                    \
    __asm dec   ecx                                 \
    __asm shl   ecx, 10h                            \
    __asm mov   buffer_draw_width, ecx                    \
    __asm xor   ecx, ecx                            \
    __asm mov   cx, word ptr [scale_x_inc+2]   \
    __asm mov   x_inc_int, ecx               \
    __asm mov   eax, scale_y_inc               \
    __asm shl   eax, 10h                            \
    __asm mov   frac_scale_y_inc, eax             \
    __asm push  ecx                                 \
    __asm mov   ecx, _table                 \
    __asm Lrow:                                     \
    __asm xor   eax, eax                            \
    __asm mov   ebx, buffer_draw_width                    \
    __asm mov   bx, word ptr scale_x_inc       \
    __asm mov   dx, word ptr scale_x        \
    __asm Lpix:                                     \
    BLIT_TEST_AND_STIPPLE(Lnext)                    \
    __asm Lnext:                                    \
    __asm add   dx, bx                              \
    __asm CARRYOP esi, x_inc_int             \
    __asm add   edi, 2                              \
    __asm sub   ebx, 10000h                         \
    __asm jns   Lpix                                \
    __asm add   edi, buffer_advance          \
    __asm add   edx, frac_scale_y_inc             \
    __asm sbb   eax, eax                            \
    __asm mov   ebx, y_advance_no_carry        \
    __asm add   esi, dword ptr [ebx + eax*4]        \
    __asm dec   buffer_draw_height                        \
    __asm jnz   Lrow                                \
    __asm pop   ecx                                 \
    __asm pop   edi                                 \
    __asm pop   esi
int JGLSprite::draw_sudo_trans_8_16_25(JGLBuffer *dst, int x, int y)
{
    Rect        r;
    Rect        saved;
    ushort     *dstp;
    uchar      *srcp;
    int         dx;
    int         dy;
    JGLPalette *pal;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0) {
        dst->release_data(1);
        return 0x10;
    }
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    switch (dst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        dst->release_data(1);
        return 1;
    }
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u1row, u1pix, u1nx, inc, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u2row, u2pix, u2nx, inc, sub)
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u3row, u3pix, u3nx, dec, add)
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_16() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = (buffer_pitch - buffer_draw_width) * 2;
                BLIT_CORE_UNSCALED(u4row, u4pix, u4nx, dec, sub)
            }
        }
    } else {
        jgl::SetRect(&r, x + (int)scale_factor_x, y + (int)scale_factor_y, (int)(this->m_width * scale_factor_x),
                     (int)(this->m_height * scale_factor_y));
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
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                sprite_advance = sprite_pitch - sprite_draw_width;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch - sprite_pitch + sprite_advance;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s1row, s1pix, s1nx, adc)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s2row, s2pix, s2nx, adc)
            }
        } else {
            if (g_scaleY >= 0) {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (scale_y >> 16) * sprite_pitch + this->m_width - 1 - (scale_x >> 16);
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s3row, s3pix, s3nx, sbb)
            } else {
                dx = r.left - saved.left;
                dy = r.top - saved.top;
                scale_x = scale_x_inc * dx;
                scale_y = scale_y_inc * dy;
                sprite_draw_width = ((scale_x & 0xffff) + scale_x_inc * buffer_draw_width) >> 16;
                sprite_draw_height = ((scale_y & 0xffff) + scale_y_inc * buffer_draw_height) >> 16;
                srcp = this->m_pixels + (this->m_height - 1 - (scale_y >> 16)) * sprite_pitch + this->m_width - 1
                       - (scale_x >> 16);
                sprite_advance = -sprite_pitch + sprite_draw_width;
                *y_advance_no_carry = (scale_y_inc >> 16) * sprite_pitch + -sprite_draw_width;
                *y_advance_carry = *y_advance_no_carry + sprite_pitch;
                BLIT_CORE_SCALED(s4row, s4pix, s4nx, sbb)
            }
        }
    }
    dst->release_data(2);
    return 0;
}
