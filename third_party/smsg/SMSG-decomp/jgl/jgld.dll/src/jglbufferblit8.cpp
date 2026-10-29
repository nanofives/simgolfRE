#include "jgldefs.h"
#include "jglsprite.h"
#include "filemap.h"
#include "jgltypes.h"
#include "jgl.h"
#include "jglpalettemgr.h"
#include "jglpcx.h"
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <stdio.h>
#include "jglglobals.h"
extern "C" int g_blitRowPixels = 0;
static int     dst_advance = 0;
extern "C" int g_blitRowsLeft = 0;
static int     source_pitch = 0;
static ushort *_table = 0;
static int     loops_per_row = 0;
static int     src_advance = 0;
int            JGLBuffer::copy_mask_8_16(JGLBuffer *pDst, int sx, int sy, int dx, int dy, int w, int h, int key)
{
    ushort     *dstp;
    uchar      *srcp;
    int         dstPitch;
    int         srcPitch;
    Rect        r;
    int         rows;
    int         cols;
    JGLPalette *pal;
    if (pDst == 0)
        return 3;
    if (sx < 0) {
        w += sx;
        dx -= sx;
        sx = 0;
    }
    if (sy < 0) {
        h += sy;
        dy -= sy;
        sy = 0;
    }
    if (sx > this->get_width())
        return 0;
    if (sy > this->get_height())
        return 0;
    if (sx + w > this->get_width())
        w = this->get_width() - sx;
    if (sy + h > this->get_height())
        h = this->get_height() - sy;
    jgl::SetRect(&r, dx, dy, w, h);
    if (jgl_IntersectRect(&r, &r, pDst->get_clip()) == 0)
        return 0;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0)
        return 16;
    switch (pDst->get_pixformat()->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        return 1;
    }
    sx += r.left - dx;
    sy += r.top - dy;
    w = r.right - r.left;
    h = r.bottom - r.top;
    dstp = pDst->SURF_GetPixelPtr16(r.left, r.top);
    if (dstp == 0)
        return 3;
    srcp = this->SURF_GetPixelPtr8(sx, sy);
    if (srcp == 0)
        return 3;
    dstPitch = pDst->get_pitch();
    srcPitch = this->get_pitch();
    rows = r.bottom - r.top;
    cols = r.right - r.left;
    loops_per_row = cols >> 2;
    src_advance = srcPitch - cols;
    dst_advance = (dstPitch - cols) << 1;
    __asm {
        push esi
        push edi
        mov  esi, srcp
        mov  edi, dstp
        xor  eax, eax
        mov  al, byte ptr key
        shl  eax, 1
        add  eax, _table
        mov  ax, word ptr [eax]
        mov  ecx, src_advance
        mov  edx, cols
        mov  g_blitRowPixels, edx
        mov  edx, rows
        mov  g_blitRowsLeft, edx
        push ebp
        mov  ebp, dst_advance
    row:
        mov  edx, g_blitRowPixels
    pix:
        cmp  word ptr [edi], ax
        jnz  skip
        xor  ebx, ebx
        mov  bl, byte ptr [esi]
        shl  ebx, 1
        add  ebx, _table
        mov  bx, word ptr [ebx]
        mov  word ptr [edi], bx
    skip:
        inc  esi
        add  edi, 2
        dec  edx
        jnz  pix
        add  esi, ecx
        add  edi, ebp
        dec  g_blitRowsLeft
        jnz  row
        pop  ebp
        pop  edi
        pop  esi
    }
    this->release_data(1);
    pDst->release_data(1);
    return 0;
}

int JGLBuffer::copy_8_16(JGLBuffer *pDst, int sx, int sy, int dx, int dy, int w, int h)
{
    ushort     *dstp;
    uchar      *srcp;
    int         dstPitch;
    Rect        r;
    JGLPalette *pal;
    JGLBuffer  *dst = pDst;
    if (this->m_bits == 0)
        return 7;
    if (dst == 0)
        return 16;
    if (sx < 0) {
        w += sx;
        dx -= sx;
        sx = 0;
    }
    if (sy < 0) {
        h += sy;
        dy -= sy;
        sy = 0;
    }
    if (sx > this->get_width())
        return 0;
    if (sy > this->get_height())
        return 0;
    if (sx + w > this->get_width())
        w = this->get_width() - sx;
    if (sy + h > this->get_height())
        h = this->get_height() - sy;
    jgl::SetRect(&r, dx, dy, w, h);
    if (jgl_IntersectRect(&r, &r, dst->get_clip()) == 0)
        return 0;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal == 0)
        return 16;
    switch (dst->code) {
    case 0:
        _table = pal->get_555_table();
        break;
    case 1:
        _table = pal->get_565_table();
        break;
    default:
        return 1;
    }
    sx += r.left - dx;
    sy += r.top - dy;
    w = r.right - r.left;
    h = r.bottom - r.top;
    dstPitch = dst->m_pitch;
    source_pitch = this->m_pitch;
    if (dst != this) {
        dstp = dst->SURF_GetPixelPtr16(r.left, r.top);
        if (dstp == 0)
            return 3;
        srcp = this->SURF_GetPixelPtr8(sx, sy);
        if (srcp == 0) {
            dst->release_data(1);
            return 3;
        }
        __asm {
            pushf
            push edi
            push esi
            cld
            mov  edi, dstp
            mov  esi, srcp
            mov  ebx, dstPitch
            sub  ebx, w
            shl  ebx, 1
            mov  edx, source_pitch
            sub  edx, w
            mov  source_pitch, edx
            mov  ecx, w
            mov  eax, h
            mov  g_blitRowPixels, ecx
            mov  g_blitRowsLeft, eax
            xor  eax, eax
            push ebp
            mov  ebp, _table
        row0:
            mov  edx, g_blitRowPixels
        pix0:
            mov  al, byte ptr [esi]
            mov  cx, word ptr [ebp+eax*2]
            inc  esi
            mov  word ptr [edi], cx
            add  edi, 2
            dec  edx
            jnz  pix0
            add  esi, source_pitch
            add  edi, ebx
            dec  g_blitRowsLeft
            jnz  row0
            pop  ebp
            pop  esi
            pop  edi
            popf
        }
    } else if (dx < sx) {
        if (dy < sy) {
            dstp = dst->SURF_GetPixelPtr16(r.left, r.top);
            if (dstp == 0)
                return 3;
            srcp = this->SURF_GetPixelPtr8(sx, sy);
            if (srcp == 0) {
                dst->release_data(1);
                return 3;
            }
            __asm {
                pushf
                push edi
                push esi
                cld
                mov  edi, dstp
                mov  esi, srcp
                mov  eax, h
                mov  ebx, dstPitch
                sub  ebx, w
                shl  ebx, 1
                mov  edx, source_pitch
                sub  edx, w
                mov  source_pitch, edx
                mov  ecx, w
                mov  eax, h
                mov  g_blitRowPixels, ecx
                mov  g_blitRowsLeft, eax
                xor  eax, eax
                push ebp
                mov  ebp, _table
            row1:
                mov  edx, g_blitRowPixels
            pix1:
                mov  al, byte ptr [esi]
                mov  cx, word ptr [ebp+eax*2]
                inc  esi
                mov  word ptr [edi], cx
                add  edi, 2
                dec  edx
                jnz  pix1
                add  esi, source_pitch
                add  edi, ebx
                dec  g_blitRowsLeft
                jnz  row1
                pop  ebp
                pop  esi
                pop  edi
                popf
            }
        } else {
            dstp = dst->SURF_GetPixelPtr16(r.left, r.top + h - 1);
            if (dstp == 0)
                return 3;
            srcp = this->SURF_GetPixelPtr8(sx, sy + h - 1);
            if (srcp == 0) {
                dst->release_data(1);
                return 3;
            }
            __asm {
                pushf
                push edi
                push esi
                cld
                mov  edi, dstp
                mov  esi, srcp
                mov  eax, h
                mov  ebx, dstPitch
                add  ebx, w
                shl  ebx, 1
                mov  edx, source_pitch
                add  edx, w
                mov  source_pitch, edx
                mov  ecx, w
                mov  eax, h
                mov  g_blitRowPixels, ecx
                mov  g_blitRowsLeft, eax
                xor  eax, eax
                push ebp
                mov  ebp, _table
            row2:
                mov  edx, g_blitRowPixels
            pix2:
                mov  al, byte ptr [esi]
                mov  cx, word ptr [ebp+eax*2]
                inc  esi
                mov  word ptr [edi], cx
                add  edi, 2
                dec  edx
                jnz  pix2
                sub  esi, source_pitch
                sub  edi, ebx
                dec  g_blitRowsLeft
                jnz  row2
                pop  ebp
                pop  esi
                pop  edi
                popf
            }
        }
    } else {
        if (dy < sy) {
            if (w < 4) {
                dstp = dst->SURF_GetPixelPtr16(r.left + w - 1, r.top);
                srcp = this->SURF_GetPixelPtr8(sx + w - 1, sy);
            } else {
                dstp = dst->SURF_GetPixelPtr16(r.left + w - 4, r.top);
                srcp = this->SURF_GetPixelPtr8(sx + w - 4, sy);
            }
            if (dstp == 0)
                return 3;
            if (srcp == 0) {
                dst->release_data(1);
                return 3;
            }
            __asm {
                pushf
                push edi
                push esi
                std
                mov  edi, dstp
                mov  esi, srcp
                mov  eax, h
                mov  ebx, dstPitch
                add  ebx, w
                shl  ebx, 1
                mov  edx, source_pitch
                add  edx, w
                mov  source_pitch, edx
                mov  ecx, w
                mov  eax, h
                mov  g_blitRowPixels, ecx
                mov  g_blitRowsLeft, eax
                xor  eax, eax
                push ebp
                mov  ebp, _table
            row3:
                mov  edx, g_blitRowPixels
            pix3:
                mov  al, byte ptr [esi]
                mov  cx, word ptr [ebp+eax*2]
                inc  esi
                mov  word ptr [edi], cx
                add  edi, 2
                dec  edx
                jnz  pix3
                add  esi, source_pitch
                add  edi, ebx
                dec  g_blitRowsLeft
                jnz  row3
                pop  ebp
                pop  esi
                pop  edi
                popf
                cld
            }
        } else {
            if (w < 4) {
                dstp = dst->SURF_GetPixelPtr16(r.left + w - 1, r.top + h - 1);
                srcp = this->SURF_GetPixelPtr8(sx + w - 1, sy + h - 1);
            } else {
                dstp = dst->SURF_GetPixelPtr16(r.left + w - 4, r.top + h - 1);
                srcp = this->SURF_GetPixelPtr8(sx + w - 4, sy + h - 1);
            }
            if (dstp == 0)
                return 3;
            if (srcp == 0) {
                dst->release_data(1);
                return 3;
            }
            dstPitch = dst->m_pitch;
            source_pitch = this->m_pitch;
            __asm {
                pushf
                push edi
                push esi
                std
                mov  edi, dstp
                mov  esi, srcp
                mov  eax, h
                mov  ebx, dstPitch
                sub  ebx, w
                mov  edx, source_pitch
                sub  edx, w
                mov  source_pitch, edx
                mov  ecx, w
                mov  eax, h
                mov  g_blitRowPixels, ecx
                mov  g_blitRowsLeft, eax
                xor  eax, eax
                push ebp
                mov  ebp, _table
            row4:
                mov  edx, g_blitRowPixels
            pix4:
                mov  al, byte ptr [esi]
                mov  cx, word ptr [ebp+eax*2]
                inc  esi
                mov  word ptr [edi], cx
                add  edi, 2
                dec  edx
                jnz  pix4
                sub  esi, source_pitch
                sub  edi, ebx
                dec  eax
                jnz  row4
                pop  ebp
                pop  esi
                pop  edi
                popf
                cld
            }
        }
    }
    this->release_data(1);
    dst->release_data(1);
    return 0;
}

int JGLBuffer::copy_mask_8_8(JGLBuffer *pDst, int sx, int sy, int dx, int dy, int w, int h, int key)
{
    uchar *dstp;
    uchar *srcp;
    int    dstPitch;
    int    srcPitch;
    Rect   r;
    int    rows;
    int    cols;
    if (pDst == 0)
        return 3;
    if (sx < 0) {
        w += sx;
        dx -= sx;
        sx = 0;
    }
    if (sy < 0) {
        h += sy;
        dy -= sy;
        sy = 0;
    }
    if (sx > this->get_width())
        return 0;
    if (sy > this->get_height())
        return 0;
    if (sx + w > this->get_width())
        w = this->get_width() - sx;
    if (sy + h > this->get_height())
        h = this->get_height() - sy;
    jgl::SetRect(&r, dx, dy, w, h);
    if (jgl_IntersectRect(&r, &r, pDst->get_clip()) == 0)
        return 0;
    sx += r.left - dx;
    sy += r.top - dy;
    w = r.right - r.left;
    h = r.bottom - r.top;
    dstp = pDst->SURF_GetPixelPtr8(r.left, r.top);
    if (dstp == 0)
        return 3;
    srcp = this->SURF_GetPixelPtr8(sx, sy);
    if (srcp == 0)
        return 3;
    dstPitch = pDst->get_pitch();
    srcPitch = this->get_pitch();
    rows = r.bottom - r.top;
    cols = r.right - r.left;
    g_keyBlitDwords8 = cols >> 2;
    g_keyBlitSrcAdj8 = srcPitch - cols;
    g_keyBlitDstAdj8 = dstPitch - cols;
    __asm {
        push esi
        push edi
        mov  esi, srcp
        mov  edi, dstp
        mov  ecx, g_keyBlitDwords8
        mov  ebx, rows
        mov  al, byte ptr key
        mov  edx, cols
        and  edx, 3
        jz   start0
        dec  edx
        jz   start1
        dec  edx
        jz   start2
        dec  edx
        jz   start3
    start0:
        lea  edx, skipTest0
        jmp  skipTest0
    start1:
        lea  edx, skipTest3
        inc  ecx
        mov  g_keyBlitDwords8, ecx
        jmp  skipTest3
    start2:
        lea  edx, skipTest2
        inc  ecx
        mov  g_keyBlitDwords8, ecx
        jmp  skipTest2
    start3:
        lea  edx, skipTest1
        inc  ecx
        mov  g_keyBlitDwords8, ecx
        jmp  skipTest1
    row:
        jmp  edx
    copyTest0:
        cmp  byte ptr [edi], al
        jnz  skip0
    copy0:
        mov  ah, byte ptr [esi]
        mov  byte ptr [edi], ah
        inc  esi
        inc  edi
        cmp  byte ptr [edi], al
        jnz  skip1
    copy1:
        mov  ah, byte ptr [esi]
        mov  byte ptr [edi], ah
        inc  esi
        inc  edi
        cmp  byte ptr [edi], al
        jnz  skip2
    copy2:
        mov  ah, byte ptr [esi]
        mov  byte ptr [edi], ah
        inc  esi
        inc  edi
        cmp  byte ptr [edi], al
        jnz  skip3
    copy3:
        mov  ah, byte ptr [esi]
        mov  byte ptr [edi], ah
        inc  esi
        inc  edi
        dec  ecx
        jnz  copyTest0
        jmp  nextrow
    skipTest0:
        cmp  byte ptr [edi], al
        jz   copy0
    skip0:
        inc  esi
        inc  edi
    skipTest1:
        cmp  byte ptr [edi], al
        jz   copy1
    skip1:
        inc  esi
        inc  edi
    skipTest2:
        cmp  byte ptr [edi], al
        jz   copy2
    skip2:
        inc  esi
        inc  edi
    skipTest3:
        cmp  byte ptr [edi], al
        jz   copy3
    skip3:
        inc  esi
        inc  edi
        dec  ecx
        jnz  skipTest0
    nextrow:
        mov  ecx, g_keyBlitDwords8
        add  esi, g_keyBlitSrcAdj8
        add  edi, g_keyBlitDstAdj8
        dec  ebx
        jnz  row
        pop  edi
        pop  esi
    }
    this->release_data(1);
    pDst->release_data(1);
    return 0;
}

int JGLBuffer::copy_8_8(JGLBuffer *pDst, int sx, int sy, int dx, int dy, int w, int h)
{
    uchar     *dstp;
    uchar     *srcp;
    int        dstPitch;
    int        srcPitch;
    Rect       r;
    JGLBuffer *dst = pDst;
    if (this->m_bits == 0)
        return 7;
    if (dst == 0)
        return 16;
    if (sx < 0) {
        w += sx;
        dx -= sx;
        sx = 0;
    }
    if (sy < 0) {
        h += sy;
        dy -= sy;
        sy = 0;
    }
    if (sx > this->get_width())
        return 0;
    if (sy > this->get_height())
        return 0;
    if (sx + w > this->get_width())
        w = this->get_width() - sx;
    if (sy + h > this->get_height())
        h = this->get_height() - sy;
    jgl::SetRect(&r, dx, dy, w, h);
    if (jgl_IntersectRect(&r, &r, dst->get_clip()) == 0)
        return 0;
    sx += r.left - dx;
    sy += r.top - dy;
    w = r.right - r.left;
    h = r.bottom - r.top;
    if (dst != this) {
        dstp = dst->SURF_GetPixelPtr8(r.left, r.top);
        if (dstp == 0)
            return 3;
        srcp = this->SURF_GetPixelPtr8(sx, sy);
        if (srcp == 0) {
            dst->release_data(1);
            return 3;
        }
        dstPitch = dst->m_pitch;
        srcPitch = this->m_pitch;
        __asm {
            pushf
            push edi
            push esi
            cld
            mov  edi, dstp
            mov  esi, srcp
            mov  eax, h
            mov  ebx, dstPitch
            sub  ebx, w
            mov  edx, srcPitch
            sub  edx, w
            mov  ecx, w
            push ebp
            mov  ebp, ecx
            shr  ebp, 2
            and  ecx, 3
            mov  g_blitTailPixels8, ecx
        row0:
            mov  ecx, ebp
            rep  movsd
            mov  ecx, g_blitTailPixels8
            rep  movsb
            add  esi, edx
            add  edi, ebx
            dec  eax
            jnz  row0
            pop  ebp
            pop  esi
            pop  edi
            popf
        }
    } else if (dx < sx) {
        if (dy < sy) {
            dstp = dst->SURF_GetPixelPtr8(r.left, r.top);
            if (dstp == 0)
                return 3;
            srcp = this->SURF_GetPixelPtr8(sx, sy);
            if (srcp == 0) {
                dst->release_data(1);
                return 3;
            }
            dstPitch = dst->m_pitch;
            srcPitch = this->m_pitch;
            __asm {
                pushf
                push edi
                push esi
                cld
                mov  edi, dstp
                mov  esi, srcp
                mov  eax, h
                mov  ebx, dstPitch
                sub  ebx, w
                mov  edx, srcPitch
                sub  edx, w
                mov  ecx, w
                push ebp
                mov  ebp, ecx
                shr  ebp, 2
                and  ecx, 3
                mov  g_blitTailPixels8, ecx
            row1:
                mov  ecx, ebp
                rep  movsd
                mov  ecx, g_blitTailPixels8
                rep  movsb
                add  esi, edx
                add  edi, ebx
                dec  eax
                jnz  row1
                pop  ebp
                pop  esi
                pop  edi
                popf
            }
        } else {
            dstp = dst->SURF_GetPixelPtr8(r.left, r.top + h - 1);
            if (dstp == 0)
                return 3;
            srcp = this->SURF_GetPixelPtr8(sx, sy + h - 1);
            if (srcp == 0) {
                dst->release_data(1);
                return 3;
            }
            dstPitch = dst->m_pitch;
            srcPitch = this->m_pitch;
            __asm {
                pushf
                push edi
                push esi
                cld
                mov  edi, dstp
                mov  esi, srcp
                mov  eax, h
                mov  ebx, dstPitch
                add  ebx, w
                mov  edx, srcPitch
                add  edx, w
                mov  ecx, w
                push ebp
                mov  ebp, ecx
                shr  ebp, 2
                and  ecx, 3
                mov  g_blitTailPixels8, ecx
            row2:
                mov  ecx, ebp
                rep  movsd
                mov  ecx, g_blitTailPixels8
                rep  movsb
                sub  esi, edx
                sub  edi, ebx
                dec  eax
                jnz  row2
                pop  ebp
                pop  esi
                pop  edi
                popf
            }
        }
    } else {
        if (dy < sy) {
            if (w < 4) {
                dstp = dst->SURF_GetPixelPtr8(r.left + w - 1, r.top);
                srcp = this->SURF_GetPixelPtr8(sx + w - 1, sy);
            } else {
                dstp = dst->SURF_GetPixelPtr8(r.left + w - 4, r.top);
                srcp = this->SURF_GetPixelPtr8(sx + w - 4, sy);
            }
            if (dstp == 0)
                return 3;
            if (srcp == 0) {
                dst->release_data(1);
                return 3;
            }
            dstPitch = dst->m_pitch;
            srcPitch = this->m_pitch;
            __asm {
                pushf
                push edi
                push esi
                std
                mov  edi, dstp
                mov  esi, srcp
                mov  eax, h
                mov  ebx, dstPitch
                add  ebx, w
                mov  edx, srcPitch
                add  edx, w
                mov  ecx, w
                push ebp
                mov  ebp, ecx
                shr  ebp, 2
                and  ecx, 3
                mov  g_blitTailPixels8, ecx
            row3:
                mov  ecx, ebp
                rep  movsd
                mov  ecx, g_blitTailPixels8
                rep  movsb
                add  esi, edx
                add  edi, ebx
                dec  eax
                jnz  row3
                pop  ebp
                pop  esi
                pop  edi
                popf
                cld
            }
        } else {
            if (w < 4) {
                dstp = dst->SURF_GetPixelPtr8(r.left + w - 1, r.top + h - 1);
                srcp = this->SURF_GetPixelPtr8(sx + w - 1, sy + h - 1);
            } else {
                dstp = dst->SURF_GetPixelPtr8(r.left + w - 4, r.top + h - 1);
                srcp = this->SURF_GetPixelPtr8(sx + w - 4, sy + h - 1);
            }
            if (dstp == 0)
                return 3;
            if (srcp == 0) {
                dst->release_data(1);
                return 3;
            }
            dstPitch = dst->m_pitch;
            srcPitch = this->m_pitch;
            __asm {
                pushf
                push edi
                push esi
                std
                mov  edi, dstp
                mov  esi, srcp
                mov  eax, h
                mov  ebx, dstPitch
                sub  ebx, w
                mov  edx, srcPitch
                sub  edx, w
                mov  ecx, w
                push ebp
                mov  ebp, ecx
                shr  ebp, 2
                and  ecx, 3
                mov  g_blitTailPixels8, ecx
            row4:
                mov  ecx, ebp
                rep  movsd
                mov  ecx, g_blitTailPixels8
                rep  movsb
                sub  esi, edx
                sub  edi, ebx
                dec  eax
                jnz  row4
                pop  ebp
                pop  esi
                pop  edi
                popf
                cld
            }
        }
    }
    this->release_data(1);
    dst->release_data(1);
    return 0;
}
