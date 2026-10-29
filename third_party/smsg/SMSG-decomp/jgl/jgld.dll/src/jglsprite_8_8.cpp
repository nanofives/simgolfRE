#include "jgldefs.h"
#include "jgltypes.h"
#include "jglsprite.h"
#include "jglwin.h"
#include "jglglobals.h"
static int      magic_array_y[2];
static int     *y_advance_carry = &magic_array_y[0];
static int     *y_advance_no_carry = &magic_array_y[1];
static int      s_blit8Unread[3] = {-1, -1, -1};
static int      sprite_advance;
static unsigned scale_y_inc;
static unsigned frac_scale_y_inc;
static unsigned scale_x;
static unsigned scale_y;
static int      buffer_pitch;
static int      buffer_advance;
static int      buffer_draw_width;
static int      sprite_draw_width;
static int      buffer_draw_height;
static int      sprite_pitch;
static int      sprite_draw_height;
static unsigned scale_x_inc;
static float    scale_factor_x = 0;
static float    scale_factor_y = 0;
static int      global_color;
static int      x_inc_int;
static uchar   *global_table;
static uchar   *_table;
static uchar   *global_shadow_table;
static int      s_lostDword;
static int      s_lostInteger;
static int      s_lostInt;
static int      s_lostItem;
static int      s_lostRecords;
static int      s_lostVar;
static int      s_lostStatic;
static int      s_lostState;
int             JGLSprite::draw_254_8_8(JGLBuffer *dst, int x, int y)
{
    Rect   r;
    Rect   saved;
    uchar *dstp;
    uchar *srcp;
    int    dx;
    int    dy;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (LPRECT)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                row1:
                    mov  edx, buffer_draw_width
                pix1:
                    cmp  byte ptr [esi], 0ffh
                    jnc  nxt1
                    mov  ah, byte ptr [esi]
                    mov  byte ptr [edi], ah
                nxt1:
                    inc  esi
                    inc  edi
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
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                row2:
                    mov  edx, buffer_draw_width
                pix2:
                    cmp  byte ptr [esi], 0ffh
                    jnc  nxt2
                    mov  ah, byte ptr [esi]
                    mov  byte ptr [edi], ah
                nxt2:
                    inc  esi
                    inc  edi
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
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                row3:
                    mov  edx, buffer_draw_width
                pix3:
                    cmp  byte ptr [esi], 0ffh
                    jnc  nxt3
                    mov  ah, byte ptr [esi]
                    mov  byte ptr [edi], ah
                nxt3:
                    dec  esi
                    inc  edi
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
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                row4:
                    mov  edx, buffer_draw_width
                pix4:
                    cmp  byte ptr [esi], 0ffh
                    jnc  nxt4
                    mov  ah, byte ptr [esi]
                    mov  byte ptr [edi], ah
                nxt4:
                    dec  esi
                    inc  edi
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
        dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
        buffer_pitch = dst->get_pitch();
        buffer_draw_width = r.right - r.left;
        buffer_draw_height = r.bottom - r.top;
        buffer_advance = buffer_pitch - buffer_draw_width;
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
                srow5:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix5:
                    cmp  byte ptr [esi], 0ffh
                    jnc  snxt5
                    mov  ah, byte ptr [esi]
                    mov  byte ptr [edi], ah
                snxt5:
                    add  dx, bx
                    adc  esi, ecx
                    inc  edi
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
                srow6:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix6:
                    cmp  byte ptr [esi], 0ffh
                    jnc  snxt6
                    mov  ah, byte ptr [esi]
                    mov  byte ptr [edi], ah
                snxt6:
                    add  dx, bx
                    adc  esi, ecx
                    inc  edi
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
                srow7:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix7:
                    cmp  byte ptr [esi], 0ffh
                    jnc  snxt7
                    mov  ah, byte ptr [esi]
                    mov  byte ptr [edi], ah
                snxt7:
                    add  dx, bx
                    sbb  esi, ecx
                    inc  edi
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
                srow8:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix8:
                    cmp  byte ptr [esi], 0ffh
                    jnc  snxt8
                    mov  ah, byte ptr [esi]
                    mov  byte ptr [edi], ah
                snxt8:
                    add  dx, bx
                    sbb  esi, ecx
                    inc  edi
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
int JGLSprite::draw_8_8(JGLBuffer *dst, int x, int y)
{
    Rect   r;
    Rect   saved;
    uchar *dstp;
    uchar *srcp;
    int    dx;
    int    dy;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (LPRECT)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                row1:
                    mov  edx, buffer_draw_width
                pix1:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt1
                    mov  ah, byte ptr [esi]
                    mov  byte ptr [edi], ah
                nxt1:
                    inc  esi
                    inc  edi
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
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                row2:
                    mov  edx, buffer_draw_width
                pix2:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt2
                    mov  ah, byte ptr [esi]
                    mov  byte ptr [edi], ah
                nxt2:
                    inc  esi
                    inc  edi
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
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                row3:
                    mov  edx, buffer_draw_width
                pix3:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt3
                    mov  ah, byte ptr [esi]
                    mov  byte ptr [edi], ah
                nxt3:
                    dec  esi
                    inc  edi
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
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                row4:
                    mov  edx, buffer_draw_width
                pix4:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt4
                    mov  ah, byte ptr [esi]
                    mov  byte ptr [edi], ah
                nxt4:
                    dec  esi
                    inc  edi
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
        dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
        buffer_pitch = dst->get_pitch();
        buffer_draw_width = r.right - r.left;
        buffer_draw_height = r.bottom - r.top;
        buffer_advance = buffer_pitch - buffer_draw_width;
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
                srow5:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix5:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt5
                    mov  ah, byte ptr [esi]
                    mov  byte ptr [edi], ah
                snxt5:
                    add  dx, bx
                    adc  esi, ecx
                    inc  edi
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
                srow6:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix6:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt6
                    mov  ah, byte ptr [esi]
                    mov  byte ptr [edi], ah
                snxt6:
                    add  dx, bx
                    adc  esi, ecx
                    inc  edi
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
                srow7:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix7:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt7
                    mov  ah, byte ptr [esi]
                    mov  byte ptr [edi], ah
                snxt7:
                    add  dx, bx
                    sbb  esi, ecx
                    inc  edi
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
                srow8:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix8:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt8
                    mov  ah, byte ptr [esi]
                    mov  byte ptr [edi], ah
                snxt8:
                    add  dx, bx
                    sbb  esi, ecx
                    inc  edi
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
int JGLSprite::draw_mono_8_8(JGLBuffer *dst, int x, int y, int colour)
{
    Rect   r;
    Rect   saved;
    uchar *dstp;
    uchar *srcp;
    int    dx;
    int    dy;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    global_color = colour & 0xff;
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (LPRECT)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  al, byte ptr [global_color]
                row1:
                    mov  edx, buffer_draw_width
                pix1:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt1
                    mov  byte ptr [edi], al
                nxt1:
                    inc  esi
                    inc  edi
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
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  al, byte ptr [global_color]
                row2:
                    mov  edx, buffer_draw_width
                pix2:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt2
                    mov  byte ptr [edi], al
                nxt2:
                    inc  esi
                    inc  edi
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
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  al, byte ptr [global_color]
                row3:
                    mov  edx, buffer_draw_width
                pix3:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt3
                    mov  byte ptr [edi], al
                nxt3:
                    dec  esi
                    inc  edi
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
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
                __asm {
                    push esi
                    push edi
                    xor  eax, eax
                    xor  ebx, ebx
                    xor  ecx, ecx
                    mov  esi, srcp
                    mov  edi, dstp
                    mov  al, byte ptr [global_color]
                row4:
                    mov  edx, buffer_draw_width
                pix4:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt4
                    mov  byte ptr [edi], al
                nxt4:
                    dec  esi
                    inc  edi
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
        dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
        buffer_pitch = dst->get_pitch();
        buffer_draw_width = r.right - r.left;
        buffer_draw_height = r.bottom - r.top;
        buffer_advance = buffer_pitch - buffer_draw_width;
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
                    mov  al, byte ptr [global_color]
                srow5:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix5:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt5
                    mov  byte ptr [edi], al
                snxt5:
                    add  dx, bx
                    adc  esi, ecx
                    inc  edi
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
                    mov  al, byte ptr [global_color]
                srow6:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix6:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt6
                    mov  byte ptr [edi], al
                snxt6:
                    add  dx, bx
                    adc  esi, ecx
                    inc  edi
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
                    mov  al, byte ptr [global_color]
                srow7:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix7:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt7
                    mov  byte ptr [edi], al
                snxt7:
                    add  dx, bx
                    sbb  esi, ecx
                    inc  edi
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
                    mov  al, byte ptr [global_color]
                srow8:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix8:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt8
                    mov  byte ptr [edi], al
                snxt8:
                    add  dx, bx
                    sbb  esi, ecx
                    inc  edi
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
int JGLSprite::draw_dest_8_8(JGLBuffer *dst, int x, int y, uchar *lut)
{
    Rect   r;
    Rect   saved;
    uchar *dstp;
    uchar *srcp;
    int    dx;
    int    dy;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    global_table = lut;
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (LPRECT)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
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
                row1:
                    mov  edx, buffer_draw_width
                pix1:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt1
                    cmp  byte ptr [esi], 0f8h
                    jnz  pnxt1
                    mov  bl, byte ptr [edi]
                    mov  ah, byte ptr [ebp+ebx]
                    mov  byte ptr [edi], ah
                    jmp  nxt1
                pnxt1:
                    mov  ah, byte ptr [esi]
                    mov  byte ptr [edi], ah
                nxt1:
                    inc  esi
                    inc  edi
                    dec  edx
                    jnz  pix1
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  row1
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
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
                row2:
                    mov  edx, buffer_draw_width
                pix2:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt2
                    cmp  byte ptr [esi], 0f8h
                    jnz  pnxt2
                    mov  bl, byte ptr [edi]
                    mov  ah, byte ptr [ebp+ebx]
                    mov  byte ptr [edi], ah
                    jmp  nxt2
                pnxt2:
                    mov  ah, byte ptr [esi]
                    mov  byte ptr [edi], ah
                nxt2:
                    inc  esi
                    inc  edi
                    dec  edx
                    jnz  pix2
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  row2
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
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
                row3:
                    mov  edx, buffer_draw_width
                pix3:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt3
                    cmp  byte ptr [esi], 0f8h
                    jnz  pnxt3
                    mov  bl, byte ptr [edi]
                    mov  ah, byte ptr [ebp+ebx]
                    mov  byte ptr [edi], ah
                    jmp  nxt3
                pnxt3:
                    mov  ah, byte ptr [esi]
                    mov  byte ptr [edi], ah
                nxt3:
                    dec  esi
                    inc  edi
                    dec  edx
                    jnz  pix3
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  row3
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
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
                row4:
                    mov  edx, buffer_draw_width
                pix4:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt4
                    cmp  byte ptr [esi], 0f8h
                    jnz  pnxt4
                    mov  bl, byte ptr [edi]
                    mov  ah, byte ptr [ebp+ebx]
                    mov  byte ptr [edi], ah
                    jmp  nxt4
                pnxt4:
                    mov  ah, byte ptr [esi]
                    mov  byte ptr [edi], ah
                nxt4:
                    dec  esi
                    inc  edi
                    dec  edx
                    jnz  pix4
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  row4
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
        dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
        buffer_pitch = dst->get_pitch();
        buffer_draw_width = r.right - r.left;
        buffer_draw_height = r.bottom - r.top;
        buffer_advance = buffer_pitch - buffer_draw_width;
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
                    push ebp
                    mov  ebp, global_table
                srow5:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix5:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt5
                    cmp  byte ptr [esi], 0f8h
                    jnz  psnxt5
                    mov  cl, byte ptr [edi]
                    mov  ah, byte ptr [ebp+ecx]
                    mov  byte ptr [edi], ah
                    jmp  snxt5
                psnxt5:
                    mov  ah, byte ptr [esi]
                    mov  byte ptr [edi], ah
                snxt5:
                    add  dx, bx
                    adc  esi, x_inc_int
                    inc  edi
                    sub  ebx, 10000h
                    jns  spix5
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  srow5
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
                    push ebp
                    mov  ebp, global_table
                srow6:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix6:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt6
                    cmp  byte ptr [esi], 0f8h
                    jnz  psnxt6
                    mov  cl, byte ptr [edi]
                    mov  ah, byte ptr [ebp+ecx]
                    mov  byte ptr [edi], ah
                    jmp  snxt6
                psnxt6:
                    mov  ah, byte ptr [esi]
                    mov  byte ptr [edi], ah
                snxt6:
                    add  dx, bx
                    adc  esi, x_inc_int
                    inc  edi
                    sub  ebx, 10000h
                    jns  spix6
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  srow6
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
                    push ebp
                    mov  ebp, global_table
                srow7:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix7:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt7
                    cmp  byte ptr [esi], 0f8h
                    jnz  psnxt7
                    mov  cl, byte ptr [edi]
                    mov  ah, byte ptr [ebp+ecx]
                    mov  byte ptr [edi], ah
                    jmp  snxt7
                psnxt7:
                    mov  ah, byte ptr [esi]
                    mov  byte ptr [edi], ah
                snxt7:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    inc  edi
                    sub  ebx, 10000h
                    jns  spix7
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  srow7
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
                    push ebp
                    mov  ebp, global_table
                srow8:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix8:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt8
                    cmp  byte ptr [esi], 0f8h
                    jnz  psnxt8
                    mov  cl, byte ptr [edi]
                    mov  ah, byte ptr [ebp+ecx]
                    mov  byte ptr [edi], ah
                    jmp  snxt8
                psnxt8:
                    mov  ah, byte ptr [esi]
                    mov  byte ptr [edi], ah
                snxt8:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    inc  edi
                    sub  ebx, 10000h
                    jns  spix8
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  srow8
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
int JGLSprite::SourceLutBlit(JGLBuffer *dst, int x, int y, uchar *lut)
{
    Rect   r;
    Rect   saved;
    uchar *dstp;
    uchar *srcp;
    int    dx;
    int    dy;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    global_table = lut;
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (LPRECT)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
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
                row1:
                    mov  edx, buffer_draw_width
                pix1:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt1
                    mov  bl, byte ptr [esi]
                    mov  al, byte ptr [ebp+ebx*1]
                    mov  byte ptr [edi], al
                nxt1:
                    inc  esi
                    inc  edi
                    dec  edx
                    jnz  pix1
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  row1
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
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
                row2:
                    mov  edx, buffer_draw_width
                pix2:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt2
                    mov  bl, byte ptr [esi]
                    mov  al, byte ptr [ebp+ebx*1]
                    mov  byte ptr [edi], al
                nxt2:
                    inc  esi
                    inc  edi
                    dec  edx
                    jnz  pix2
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  row2
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
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
                row3:
                    mov  edx, buffer_draw_width
                pix3:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt3
                    mov  bl, byte ptr [esi]
                    mov  al, byte ptr [ebp+ebx*1]
                    mov  byte ptr [edi], al
                nxt3:
                    dec  esi
                    inc  edi
                    dec  edx
                    jnz  pix3
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  row3
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
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
                row4:
                    mov  edx, buffer_draw_width
                pix4:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt4
                    mov  bl, byte ptr [esi]
                    mov  al, byte ptr [ebp+ebx*1]
                    mov  byte ptr [edi], al
                nxt4:
                    dec  esi
                    inc  edi
                    dec  edx
                    jnz  pix4
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  row4
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
        dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
        buffer_pitch = dst->get_pitch();
        buffer_draw_width = r.right - r.left;
        buffer_draw_height = r.bottom - r.top;
        buffer_advance = buffer_pitch - buffer_draw_width;
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
                    push ebp
                    mov  ebp, global_table
                srow5:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix5:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt5
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp+eax*1]
                    mov  byte ptr [edi], al
                snxt5:
                    add  dx, bx
                    adc  esi, ecx
                    inc  edi
                    sub  ebx, 10000h
                    jns  spix5
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  srow5
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
                    push ebp
                    mov  ebp, global_table
                srow6:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix6:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt6
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp+eax*1]
                    mov  byte ptr [edi], al
                snxt6:
                    add  dx, bx
                    adc  esi, ecx
                    inc  edi
                    sub  ebx, 10000h
                    jns  spix6
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  srow6
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
                    push ebp
                    mov  ebp, global_table
                srow7:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix7:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt7
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp+eax*1]
                    mov  byte ptr [edi], al
                snxt7:
                    add  dx, bx
                    sbb  esi, ecx
                    inc  edi
                    sub  ebx, 10000h
                    jns  spix7
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  srow7
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
                    push ebp
                    mov  ebp, global_table
                srow8:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix8:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt8
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp+eax*1]
                    mov  byte ptr [edi], al
                snxt8:
                    add  dx, bx
                    sbb  esi, ecx
                    inc  edi
                    sub  ebx, 10000h
                    jns  spix8
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  srow8
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
int JGLSprite::SourceLutBlitWithDestRecolorKey(JGLBuffer *dst, int x, int y, uchar *lut, uchar *shadowLut)
{
    Rect   r;
    Rect   saved;
    uchar *dstp;
    uchar *srcp;
    int    dx;
    int    dy;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    global_table = lut;
    global_shadow_table = shadowLut;
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (LPRECT)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
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
                row1:
                    mov  edx, buffer_draw_width
                pix1:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt1
                    cmp  byte ptr [esi], 0f8h
                    jnz  rmnxt1
                    mov  ebp, global_shadow_table
                    mov  bl, byte ptr [edi]
                    mov  bl, byte ptr [ebp+ebx*1]
                    mov  byte ptr [edi], bl
                    jmp  nxt1
               rmnxt1:
                    mov  ebp, global_table
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp+eax*1]
                    mov  bl, byte ptr [ecx+eax*1]
                    mov  byte ptr [edi], bl
                nxt1:
                    inc  esi
                    inc  edi
                    dec  edx
                    jnz  pix1
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  row1
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
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
                row2:
                    mov  edx, buffer_draw_width
                pix2:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt2
                    cmp  byte ptr [esi], 0f8h
                    jnz  rmnxt2
                    mov  ebp, global_shadow_table
                    mov  bl, byte ptr [edi]
                    mov  bl, byte ptr [ebp+ebx*1]
                    mov  byte ptr [edi], bl
                    jmp  nxt2
               rmnxt2:
                    mov  ebp, global_table
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp+eax*1]
                    mov  bl, byte ptr [ecx+eax*1]
                    mov  byte ptr [edi], bl
                nxt2:
                    inc  esi
                    inc  edi
                    dec  edx
                    jnz  pix2
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  row2
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
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
                row3:
                    mov  edx, buffer_draw_width
                pix3:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt3
                    cmp  byte ptr [esi], 0f8h
                    jnz  rmnxt3
                    mov  ebp, global_shadow_table
                    mov  bl, byte ptr [edi]
                    mov  bl, byte ptr [ebp+ebx*1]
                    mov  byte ptr [edi], bl
                    jmp  nxt3
               rmnxt3:
                    mov  ebp, global_table
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp+eax*1]
                    mov  bl, byte ptr [ecx+eax*1]
                    mov  byte ptr [edi], bl
                nxt3:
                    dec  esi
                    inc  edi
                    dec  edx
                    jnz  pix3
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  row3
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
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
                row4:
                    mov  edx, buffer_draw_width
                pix4:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt4
                    cmp  byte ptr [esi], 0f8h
                    jnz  rmnxt4
                    mov  ebp, global_shadow_table
                    mov  bl, byte ptr [edi]
                    mov  bl, byte ptr [ebp+ebx*1]
                    mov  byte ptr [edi], bl
                    jmp  nxt4
               rmnxt4:
                    mov  ebp, global_table
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp+eax*1]
                    mov  bl, byte ptr [ecx+eax*1]
                    mov  byte ptr [edi], bl
                nxt4:
                    dec  esi
                    inc  edi
                    dec  edx
                    jnz  pix4
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  row4
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
        dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
        buffer_pitch = dst->get_pitch();
        buffer_draw_width = r.right - r.left;
        buffer_draw_height = r.bottom - r.top;
        buffer_advance = buffer_pitch - buffer_draw_width;
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
                    push ebp
                    mov  ecx, _table
                srow5:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix5:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt5
                    cmp  byte ptr [esi], 0f8h
                    jnz  rmsnxt5
                    mov  ebp, global_shadow_table
                    mov  al, byte ptr [edi]
                    mov  al, byte ptr [ebp+eax*1]
                    mov  byte ptr [edi], al
                    xor  eax, eax
                    jmp  snxt5
               rmsnxt5:
                    mov  ebp, global_table
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp+eax*1]
                    mov  al, byte ptr [ecx+eax*1]
                    mov  byte ptr [edi], al
                snxt5:
                    add  dx, bx
                    adc  esi, x_inc_int
                    inc  edi
                    sub  ebx, 10000h
                    jns  spix5
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  srow5
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
                    push ebp
                    mov  ecx, _table
                srow6:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix6:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt6
                    cmp  byte ptr [esi], 0f8h
                    jnz  rmsnxt6
                    mov  ebp, global_shadow_table
                    mov  al, byte ptr [edi]
                    mov  al, byte ptr [ebp+eax*1]
                    mov  byte ptr [edi], al
                    xor  eax, eax
                    jmp  snxt6
               rmsnxt6:
                    mov  ebp, global_table
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp+eax*1]
                    mov  al, byte ptr [ecx+eax*1]
                    mov  byte ptr [edi], al
                snxt6:
                    add  dx, bx
                    adc  esi, x_inc_int
                    inc  edi
                    sub  ebx, 10000h
                    jns  spix6
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  srow6
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
                    push ebp
                    mov  ecx, _table
                srow7:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix7:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt7
                    cmp  byte ptr [esi], 0f8h
                    jnz  rmsnxt7
                    mov  ebp, global_shadow_table
                    mov  al, byte ptr [edi]
                    mov  al, byte ptr [ebp+eax*1]
                    mov  byte ptr [edi], al
                    xor  eax, eax
                    jmp  snxt7
               rmsnxt7:
                    mov  ebp, global_table
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp+eax*1]
                    mov  al, byte ptr [ecx+eax*1]
                    mov  byte ptr [edi], al
                snxt7:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    inc  edi
                    sub  ebx, 10000h
                    jns  spix7
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  srow7
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
                    push ebp
                    mov  ecx, _table
                srow8:
                    xor  eax, eax
                    mov  ebx, buffer_draw_width
                    mov  bx, word ptr [scale_x_inc]
                    mov  dx, word ptr [scale_x]
                spix8:
                    cmp  byte ptr [esi], 0feh
                    jnc  snxt8
                    cmp  byte ptr [esi], 0f8h
                    jnz  rmsnxt8
                    mov  ebp, global_shadow_table
                    mov  al, byte ptr [edi]
                    mov  al, byte ptr [ebp+eax*1]
                    mov  byte ptr [edi], al
                    xor  eax, eax
                    jmp  snxt8
               rmsnxt8:
                    mov  ebp, global_table
                    mov  al, byte ptr [esi]
                    mov  al, byte ptr [ebp+eax*1]
                    mov  al, byte ptr [ecx+eax*1]
                    mov  byte ptr [edi], al
                snxt8:
                    add  dx, bx
                    sbb  esi, x_inc_int
                    inc  edi
                    sub  ebx, 10000h
                    jns  spix8
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  srow8
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
int JGLSprite::draw_multi_table_dest_8_8(JGLBuffer *dst, int x, int y, uchar *lut)
{
    Rect   r;
    Rect   saved;
    uchar *dstp;
    uchar *srcp;
    int    dx;
    int    dy;
    if (this->m_pixels == 0 || dst == 0 || dst->get_data() == 0)
        return 3;
    scale_factor_x = (float)abs(g_scaleX) / g_scaleDenom;
    scale_factor_y = (float)abs(g_scaleY) / g_scaleDenom;
    sprite_pitch = this->m_srcStride;
    buffer_pitch = dst->get_pitch();
    srcp = this->m_pixels;
    global_table = lut;
    if (abs(g_scaleX) == abs(g_scaleDenom) && abs(g_scaleY) == abs(g_scaleDenom)) {
        jgl::SetRect(&r, x, y, this->m_width, this->m_height);
        saved = r;
        if (IntersectRect((LPRECT)&r, (LPRECT)&r, (const RECT *)dst->get_clip()) == 0) {
            dst->release_data(1);
            return 0;
        }
        if (g_scaleX >= 0) {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
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
                row1:
                    mov  edx, buffer_draw_width
                pix1:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt1
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    shl  eax, 8
                    mov  al, byte ptr [edi]
                    mov  al, byte ptr [ebp+eax*1]
                    mov  byte ptr [edi], al
                nxt1:
                    inc  esi
                    inc  edi
                    dec  edx
                    jnz  pix1
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  row1
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
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
                row2:
                    mov  edx, buffer_draw_width
                pix2:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt2
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    shl  eax, 8
                    mov  al, byte ptr [edi]
                    mov  al, byte ptr [ebp+eax*1]
                    mov  byte ptr [edi], al
                nxt2:
                    inc  esi
                    inc  edi
                    dec  edx
                    jnz  pix2
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  row2
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            }
        } else {
            if (g_scaleY >= 0) {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch + buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
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
                row3:
                    mov  edx, buffer_draw_width
                pix3:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt3
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    shl  eax, 8
                    mov  al, byte ptr [edi]
                    mov  al, byte ptr [ebp+eax*1]
                    mov  byte ptr [edi], al
                nxt3:
                    dec  esi
                    inc  edi
                    dec  edx
                    jnz  pix3
                    add  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  row3
                    pop  ebp
                    pop  edi
                    pop  esi
                }
            } else {
                srcp = this->m_pixels + (r.left - saved.left) + (r.top - saved.top) * sprite_pitch;
                dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
                buffer_draw_height = r.bottom - r.top;
                buffer_draw_width = r.right - r.left;
                sprite_advance = sprite_pitch - buffer_draw_width;
                buffer_advance = buffer_pitch - buffer_draw_width;
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
                row4:
                    mov  edx, buffer_draw_width
                pix4:
                    cmp  byte ptr [esi], 0feh
                    jnc  nxt4
                    xor  eax, eax
                    mov  al, byte ptr [esi]
                    shl  eax, 8
                    mov  al, byte ptr [edi]
                    mov  al, byte ptr [ebp+eax*1]
                    mov  byte ptr [edi], al
                nxt4:
                    dec  esi
                    inc  edi
                    dec  edx
                    jnz  pix4
                    sub  esi, sprite_advance
                    add  edi, buffer_advance
                    dec  buffer_draw_height
                    jnz  row4
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
        dstp = dst->get_data_8() + r.left + r.top * buffer_pitch;
        buffer_pitch = dst->get_pitch();
        buffer_draw_width = r.right - r.left;
        buffer_draw_height = r.bottom - r.top;
        buffer_advance = buffer_pitch - buffer_draw_width;
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
                    push ebp
                    mov  ebp, global_table
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
                    shl  eax, 8
                    mov  al, byte ptr [edi]
                    mov  al, byte ptr [ebp+eax*1]
                    mov  byte ptr [edi], al
                snxt5:
                    add  dx, bx
                    adc  esi, ecx
                    inc  edi
                    sub  ebx, 10000h
                    jns  spix5
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  srow5
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
                    push ebp
                    mov  ebp, global_table
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
                    shl  eax, 8
                    mov  al, byte ptr [edi]
                    mov  al, byte ptr [ebp+eax*1]
                    mov  byte ptr [edi], al
                snxt6:
                    add  dx, bx
                    adc  esi, ecx
                    inc  edi
                    sub  ebx, 10000h
                    jns  spix6
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  srow6
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
                    push ebp
                    mov  ebp, global_table
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
                    shl  eax, 8
                    mov  al, byte ptr [edi]
                    mov  al, byte ptr [ebp+eax*1]
                    mov  byte ptr [edi], al
                snxt7:
                    add  dx, bx
                    sbb  esi, ecx
                    inc  edi
                    sub  ebx, 10000h
                    jns  spix7
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  srow7
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
                    push ebp
                    mov  ebp, global_table
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
                    shl  eax, 8
                    mov  al, byte ptr [edi]
                    mov  al, byte ptr [ebp+eax*1]
                    mov  byte ptr [edi], al
                snxt8:
                    add  dx, bx
                    sbb  esi, ecx
                    inc  edi
                    sub  ebx, 10000h
                    jns  spix8
                    add  edi, buffer_advance
                    add  edx, frac_scale_y_inc
                    sbb  eax, eax
                    mov  ebx, y_advance_no_carry
                    add  esi, [ebx+eax*4]
                    dec  buffer_draw_height
                    jnz  srow8
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
