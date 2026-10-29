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

int JGLBuffer::change_color_8(int x, int y, int w, int h, int from, int to)
{
    Rect   r;
    uchar *dst;
    int    advance;
    jgl::SetRect(&r, x, y, w, h);
    if (!jgl_IntersectRect(&r, this->get_clip(), &r))
        return 0;
    dst = (uchar *)this->SURF_GetPixelPtr8(r.left, r.top);
    if (dst == 0)
        return 7;
    w = GetRectWidth(&r);
    h = GetRectHeight(&r);
    advance = this->m_pitch - w;
    __asm {
        push edi
        push esi
        mov  ah, byte ptr from
        mov  al, byte ptr to
        mov  edi, dst
        mov  ebx, w
        mov  ecx, h
        mov  edx, advance
    L_row:
        mov  esi, ebx
    L_col:
        cmp  byte ptr [edi], ah
        jnz  L_skip
        mov  byte ptr [edi], al
    L_skip:
        inc  edi
        dec  esi
        jnz  L_col
        add  edi, edx
        dec  ecx
        jnz  L_row
        pop  esi
        pop  edi
    }
    this->release_data(1);
    return 0;
}

extern "C" void SetRectLTRB(Rect *r, int l, int t, int rt, int b);

int JGLBuffer::map_colors_8(int l, int t, int rt, int b, const uchar *lut)
{
    Rect   r;
    uchar *dstp;
    int    w;
    int    h;
    int    skip;
    if (lut == 0)
        return 0x10;
    SetRectLTRB(&r, l, t, rt, b);
    if (jgl_IntersectRect(&r, &r, this->get_clip()) == 0)
        return 0;
    w = GetRectWidth(&r);
    h = GetRectHeight(&r);
    skip = this->get_pitch() - w;
    dstp = this->SURF_GetPixelPtr(r.left, r.top);
    if (dstp == 0)
        return 7;
    __asm {
        push esi
        push edi
        mov  edi, dstp
        mov  esi, lut
        mov  ebx, skip
        mov  edx, h
        mov  eax, w
        push ebp
        mov  ebp, eax
        xor  eax, eax
    rowloop:
        mov  ecx, ebp
    colloop:
        mov  al, byte ptr [edi]
        mov  al, byte ptr [esi+eax*1]
        mov  byte ptr [edi], al
        inc  edi
        dec  ecx
        jnz  colloop
        add  edi, ebx
        dec  edx
        jnz  rowloop
        pop  ebp
        pop  edi
        pop  esi
    }
    this->release_data(1);
    return 0;
}


void JGLBuffer::line_8(int x0, int y0, int x1, int y1, int color)
{
    Rect   clip;
    int    d;
    int    dx;
    int    dy;
    int    incE;
    int    incNE;
    int    incNE2;
    uchar *p;
    if (x0 == x1) {
        this->vline_8(x0, y0, y1, color);
        return;
    }
    if (y0 == y1) {
        this->hline_8(x0, x1, y0, color);
        return;
    }
    if (x1 < x0) {
        __asm { mov  eax, x0
                xchg x1, eax
                mov  x0, eax
                mov  eax, y0
                xchg y1, eax
                mov  y0, eax }
    }
    if (this->get_data() == 0)
        return;
    this->SURF_CopyClipRect(&clip);
    dx = x1 - x0;
    dy = y1 - y0;
    if (y0 < y1) {
        if (x0 > clip.right || y0 > clip.bottom || x1 < clip.left || y1 < clip.top)
            goto done;
        if (dx >= dy) {
            d = 2 * dy - dx;
            incE = 2 * dy;
            incNE = 2 * (dy - dx);
            while (x0 < clip.left || y0 < clip.top) {
                if (d <= 0) {
                    d += incE;
                } else {
                    d += incNE;
                    y0++;
                }
                x0++;
            }
            if (x0 >= clip.right || y0 >= clip.bottom)
                goto done;
            p = this->get_data_8() + x0 + y0 * this->get_pitch();
            *p = (uchar)color;
            if (x1 >= clip.right)
                x1 = clip.right - 1;
            while (x0 < x1) {
                if (d <= 0) {
                    d += incE;
                } else {
                    y0++;
                    if (y0 >= clip.bottom)
                        goto done;
                    d += incNE;
                    p += this->get_pitch();
                }
                p++;
                x0++;
                *p = (uchar)color;
            }
            goto done;
        } else {
            d = 2 * dx - dy;
            incE = 2 * dx;
            incNE = 2 * (dx - dy);
            while (x0 < clip.left || y0 < clip.top) {
                if (d <= 0) {
                    d += incE;
                } else {
                    d += incNE;
                    x0++;
                }
                y0++;
            }
            if (x0 >= clip.right || y0 >= clip.bottom)
                goto done;
            p = this->get_data_8() + x0 + y0 * this->get_pitch();
            *p = (uchar)color;
            if (y1 >= clip.bottom)
                y1 = clip.bottom - 1;
            while (y0 < y1) {
                if (d <= 0) {
                    d += incE;
                } else {
                    x0++;
                    if (x0 >= clip.right)
                        goto done;
                    d += incNE;
                    p++;
                }
                y0++;
                p += this->get_pitch();
                *p = (uchar)color;
            }
            goto done;
        }
    } else {
        if (x0 > clip.right || y0 < clip.top || x1 < clip.left || y1 > clip.bottom)
            goto done;
        dy = abs(dy);
        if (dx >= dy) {
            d = 2 * dy - dx;
            incE = 2 * dy;
            incNE2 = 2 * (dy - dx);
            while (x0 < clip.left || y0 >= clip.bottom) {
                if (d <= 0) {
                    d += incE;
                } else {
                    d += incNE2;
                    y0--;
                }
                x0++;
            }
            if (x0 >= clip.right || y0 < clip.top)
                goto done;
            p = this->get_data_8() + x0 + y0 * this->get_pitch();
            *p = (uchar)color;
            if (x1 >= clip.right)
                x1 = clip.right - 1;
            while (x0 < x1) {
                if (d <= 0) {
                    d += incE;
                } else {
                    y0--;
                    if (y0 < clip.top)
                        goto done;
                    d += incNE2;
                    p -= this->get_pitch();
                }
                x0++;
                p++;
                *p = (uchar)color;
            }
            goto done;
        } else {
            d = 2 * dx - dy;
            incE = 2 * dx;
            incNE2 = 2 * (dx - dy);
            while (x0 < clip.left || y0 >= clip.bottom) {
                if (d <= 0) {
                    d += incE;
                } else {
                    d += incNE2;
                    x0++;
                }
                y0--;
            }
            if (x0 >= clip.right || y0 < clip.top)
                goto done;
            p = this->get_data_8() + x0 + y0 * this->get_pitch();
            *p = (uchar)color;
            if (y1 < clip.top)
                y1 = clip.top;
            while (y0 > y1) {
                if (d <= 0) {
                    d += incE;
                } else {
                    x0++;
                    if (x0 >= clip.right)
                        goto done;
                    d += incNE2;
                    p++;
                }
                y0--;
                p -= this->get_pitch();
                *p = (uchar)color;
            }
            goto done;
        }
    }
done:
    this->release_data(1);
}


extern "C" inline void SetRectLTRB(Rect *r, int left, int top, int right, int bottom)
{
    r->left = left;
    r->top = top;
    r->right = right;
    r->bottom = bottom;
}

void JGLBuffer::hline_8(int x0, int x1, int y, int color)
{
    uchar *p;
    if (y < this->get_clip()->top || y >= this->get_clip()->bottom)
        return;
    if (x0 == x1)
        return;
    if (x0 > x1) {
        x0 ^= x1;
        x1 ^= x0;
        x0 ^= x1;
    }
    if (x0 >= this->get_clip()->right || x1 < this->get_clip()->left)
        return;
    if (x0 < this->get_clip()->left)
        x0 = this->get_clip()->left;
    if (x1 >= this->get_clip()->right)
        x1 = this->get_clip()->right - 1;
    p = this->SURF_GetPixelPtr8(x0, y);
    if (p == 0)
        return;
    memset(p, color, x1 - x0 + 1);
    this->release_data(1);
}


void JGLBuffer::vline_8(int x, int y0, int y1, int color)
{
    uchar *p;
    int    stride;
    if (x < this->get_clip()->left || x >= this->get_clip()->right)
        return;
    if (y0 == y1)
        return;
    if (y0 > y1) {
        y0 ^= y1;
        y1 ^= y0;
        y0 ^= y1;
    }
    if (y0 >= this->get_clip()->bottom || y1 < this->get_clip()->top)
        return;
    if (y0 < this->get_clip()->top)
        y0 = this->get_clip()->top;
    if (y1 >= this->get_clip()->bottom)
        y1 = this->get_clip()->bottom - 1;
    p = this->SURF_GetPixelPtr8(x, y0);
    if (p == 0)
        return;
    stride = this->get_pitch();
    __asm {
        push edi
        mov  ecx, y1
        sub  ecx, y0
        inc  ecx
        mov  ah, byte ptr color
        mov  ebx, stride
        mov  edi, p
    nextRow:
        mov  byte ptr [edi], ah
        add  edi, ebx
        loop nextRow
        pop  edi
    }
    this->release_data(1);
}

#define PLOT                                            \
    if (phase < 0) {                                    \
        if (onColor != -1) *dstp = (uchar)onColor;                \
        phase++;                                        \
        if (phase == 0) phase = off;                    \
    } else {                                            \
        if (offColor != -1) *dstp = (uchar)offColor;                \
        phase--;                                        \
        if (phase == 0) phase = on;                     \
    }

void JGLBuffer::DrawDashedLine8(int x0, int y0, int x1, int y1, int onColor, int offColor, int on, int off, int phase)
{
    Rect   clip;
    int    d;
    int    dx;
    int    dy;
    int    incE;
    int    incNE;
    int    incNE2;
    uchar *dstp;
    if (x0 == x1) {
        this->DrawDashedVLine8(x0, y0, y1, onColor, offColor, on, off, phase);
        return;
    }
    if (y0 == y1) {
        this->DrawDashedHLine8(x0, x1, y0, onColor, offColor, on, off, phase);
        return;
    }
    if (x1 < x0) {
        __asm { mov  eax, x0
                xchg x1, eax
                mov  x0, eax
                mov  eax, y0
                xchg y1, eax
                mov  y0, eax }
        on ^= off;
        off ^= on;
        on ^= off;
        onColor ^= offColor;
        offColor ^= onColor;
        onColor ^= offColor;
        phase = on + off - phase;
    }
    if (this->get_data() == 0)
        return;
    this->SURF_CopyClipRect(&clip);
    phase = phase % (on + off);
    if (phase >= on) {
        phase = phase - on;
        phase = off - phase;
    } else {
        phase = on - phase;
        phase = -phase;
    }
    on = -on;
    dx = x1 - x0;
    dy = y1 - y0;
    if (y0 < y1) {
        if (x0 > clip.right || y0 > clip.bottom || x1 < clip.left || y1 < clip.top)
            goto done;
        if (dx >= dy) {
            d = 2 * dy - dx;
            incE = 2 * dy;
            incNE = 2 * (dy - dx);
            while (x0 < clip.left || y0 < clip.top) {
                if (d <= 0) {
                    d += incE;
                    x0++;
                } else {
                    d += incNE;
                    x0++;
                    y0++;
                }
            }
            if (x0 >= clip.right || y0 >= clip.bottom)
                goto done;
            dstp = this->get_data_8() + x0 + y0 * this->get_pitch();
            PLOT if (x1 >= clip.right) x1 = clip.right - 1;
            while (x0 < x1) {
                if (d <= 0) {
                    d += incE;
                    x0++;
                    dstp++;
                } else {
                    y0++;
                    if (y0 >= clip.bottom)
                        goto done;
                    d += incNE;
                    x0++;
                    dstp += this->get_pitch() + 1;
                }
                PLOT
            }
            goto done;
        } else {
            d = 2 * dx - dy;
            incE = 2 * dx;
            incNE = 2 * (dx - dy);
            while (x0 < clip.left || y0 < clip.top) {
                if (d <= 0) {
                    d += incE;
                    y0++;
                } else {
                    d += incNE;
                    y0++;
                    x0++;
                }
            }
            if (x0 >= clip.right || y0 >= clip.bottom)
                goto done;
            dstp = this->get_data_8() + x0 + y0 * this->get_pitch();
            PLOT if (y1 >= clip.bottom) y1 = clip.bottom - 1;
            while (y0 < y1) {
                if (d <= 0) {
                    d += incE;
                    y0++;
                    dstp += this->get_pitch();
                } else {
                    x0++;
                    if (x0 >= clip.right)
                        goto done;
                    d += incNE;
                    y0++;
                    dstp += this->get_pitch() + 1;
                }
                PLOT
            }
            goto done;
        }
    } else {
        if (x0 > clip.right || y0 < clip.top || x1 < clip.left || y1 > clip.bottom)
            goto done;
        dy = abs(dy);
        if (dx >= dy) {
            d = 2 * dy - dx;
            incE = 2 * dy;
            incNE2 = 2 * (dy - dx);
            while (x0 < clip.left || y0 >= clip.bottom) {
                if (d <= 0) {
                    d += incE;
                    x0++;
                } else {
                    d += incNE2;
                    x0++;
                    y0--;
                }
            }
            if (x0 >= clip.right || y0 < clip.top)
                goto done;
            dstp = this->get_data_8() + x0 + y0 * this->get_pitch();
            PLOT if (x1 >= clip.right) x1 = clip.right - 1;
            while (x0 < x1) {
                if (d <= 0) {
                    d += incE;
                    x0++;
                    dstp++;
                } else {
                    y0--;
                    if (y0 < clip.top)
                        goto done;
                    d += incNE2;
                    x0++;
                    dstp = dstp - this->get_pitch() + 1;
                }
                PLOT
            }
            goto done;
        } else {
            d = 2 * dx - dy;
            incE = 2 * dx;
            incNE2 = 2 * (dx - dy);
            while (x0 < clip.left || y0 >= clip.bottom) {
                if (d <= 0) {
                    d += incE;
                    y0--;
                } else {
                    d += incNE2;
                    y0--;
                    x0++;
                }
            }
            if (x0 >= clip.right || y0 < clip.top)
                goto done;
            dstp = this->get_data_8() + x0 + y0 * this->get_pitch();
            PLOT if (y1 < clip.top) y1 = clip.top;
            while (y0 > y1) {
                if (d <= 0) {
                    d += incE;
                    y0--;
                    dstp = dstp - this->get_pitch();
                } else {
                    x0++;
                    if (x0 >= clip.right)
                        goto done;
                    d += incNE2;
                    y0--;
                    dstp = dstp - this->get_pitch() + 1;
                }
                PLOT
            }
            goto done;
        }
    }
done:
    this->release_data(1);
}


void JGLBuffer::DrawDashedHLine8(int x0, int x1, int y, int onColor, int offColor, int on, int off, int phase)
{
    uchar *dstp;
    if (y < this->get_clip()->top || y >= this->get_clip()->bottom)
        return;
    if (x0 == x1)
        return;
    if (onColor == -1 && offColor == -1)
        return;
    if (x0 > x1) {
        x0 ^= x1;
        x1 ^= x0;
        x0 ^= x1;
        on ^= off;
        off ^= on;
        on ^= off;
        onColor ^= offColor;
        offColor ^= onColor;
        onColor ^= offColor;
        phase = on + off - phase;
    }
    if (x0 >= this->get_clip()->right || x1 < this->get_clip()->left)
        return;
    if (x0 < this->get_clip()->left)
        x0 = this->get_clip()->left;
    if (x1 >= this->get_clip()->right)
        x1 = this->get_clip()->right - 1;
    dstp = this->SURF_GetPixelPtr8(x0, y);
    if (dstp == 0)
        return;
    phase = phase % (on + off);
    if (phase >= on) {
        phase = phase - on;
        phase = off - phase;
    } else {
        phase = on - phase;
        phase = -phase;
    }
    on = -on;
    if (onColor == -1) {
        __asm {
            push edi
            push esi
            mov  ecx, x1
            sub  ecx, x0
            inc  ecx
            mov  ah,  byte ptr onColor
            mov  al,  byte ptr offColor
            mov  ebx, on
            mov  esi, off
            mov  edi, dstp
            mov  edx, phase
        lpA:
            and  edx, edx
            js   ngA
            mov  [edi], al
            dec  edx
            jnz  ctA
            mov  edx, ebx
            jmp  ctA
        ngA:
            inc  edx
            jnz  ctA
            mov  edx, esi
        ctA:
            inc  edi
            dec  ecx
            jnz  lpA
            pop  esi
            pop  edi
        }
    } else if (offColor == -1) {
        __asm {
            push esi
            push edi
            mov  ecx, x1
            sub  ecx, x0
            inc  ecx
            mov  ah,  byte ptr onColor
            mov  al,  byte ptr offColor
            mov  ebx, on
            mov  esi, off
            mov  edi, dstp
            mov  edx, phase
        lpB:
            and  edx, edx
            js   ngB
            dec  edx
            jnz  ctB
            mov  edx, ebx
            jmp  ctB
        ngB:
            mov  [edi], ah
            inc  edx
            jnz  ctB
            mov  edx, esi
        ctB:
            inc  edi
            dec  ecx
            jnz  lpB
            pop  edi
            pop  esi
        }
    } else {
        __asm {
            push esi
            push edi
            mov  ecx, x1
            sub  ecx, x0
            inc  ecx
            mov  ah,  byte ptr onColor
            mov  al,  byte ptr offColor
            mov  ebx, on
            mov  esi, off
            mov  edi, dstp
            mov  edx, phase
        lpC:
            and  edx, edx
            js   ngC
            mov  [edi], al
            dec  edx
            jnz  ctC
            mov  edx, ebx
            jmp  ctC
        ngC:
            mov  [edi], ah
            inc  edx
            jnz  ctC
            mov  edx, esi
        ctC:
            inc  edi
            dec  ecx
            jnz  lpC
            pop  edi
            pop  esi
        }
    }
    this->release_data(1);
}


void JGLBuffer::DrawDashedVLine8(int x, int y0, int y1, int onColor, int offColor, int on, int off, int phase)
{
    uchar *dstp;
    int    stride;
    if (x < this->get_clip()->left || x >= this->get_clip()->right)
        return;
    if (y0 == y1)
        return;
    if (onColor == -1 && offColor == -1)
        return;
    if (y0 > y1) {
        y0 ^= y1;
        y1 ^= y0;
        y0 ^= y1;
        on ^= off;
        off ^= on;
        on ^= off;
        onColor ^= offColor;
        offColor ^= onColor;
        onColor ^= offColor;
        phase = on + off - phase;
    }
    if (y0 >= this->get_clip()->bottom || y1 < this->get_clip()->top)
        return;
    if (y0 < this->get_clip()->top)
        y0 = this->get_clip()->top;
    if (y1 >= this->get_clip()->bottom)
        y1 = this->get_clip()->bottom - 1;
    dstp = this->SURF_GetPixelPtr8(x, y0);
    if (dstp == 0)
        return;
    stride = this->get_pitch();
    phase = phase % (on + off);
    if (phase >= on) {
        phase = phase - on;
        phase = off - phase;
    } else {
        phase = on - phase;
        phase = -phase;
    }
    on = -on;
    if (onColor == -1) {
        __asm {
            push esi
            push edi
            mov  ecx, y1
            sub  ecx, y0
            inc  ecx
            mov  ah,  byte ptr onColor
            mov  al,  byte ptr offColor
            mov  ebx, stride
            mov  edi, dstp
            mov  edx, phase
        lpA:
            and  edx, edx
            js   ngA
            mov  [edi], al
            dec  edx
            jnz  ctA
            mov  edx, on
            jmp  ctA
        ngA:
            inc  edx
            jnz  ctA
            mov  edx, off
        ctA:
            add  edi, ebx
            dec  ecx
            jnz  lpA
            pop  edi
            pop  esi
        }
    } else if (offColor == -1) {
        __asm {
            push edi
            push esi
            mov  ecx, y1
            sub  ecx, y0
            inc  ecx
            mov  ah,  byte ptr onColor
            mov  al,  byte ptr offColor
            mov  ebx, stride
            mov  edi, dstp
            mov  edx, phase
        lpB:
            and  edx, edx
            js   ngB
            dec  edx
            jnz  ctB
            mov  edx, on
            jmp  ctB
        ngB:
            mov  [edi], ah
            inc  edx
            jnz  ctB
            mov  edx, off
        ctB:
            add  edi, ebx
            dec  ecx
            jnz  lpB
            pop  esi
            pop  edi
        }
    } else {
        __asm {
            push edi
            push esi
            mov  ecx, y1
            sub  ecx, y0
            inc  ecx
            mov  ah,  byte ptr onColor
            mov  al,  byte ptr offColor
            mov  ebx, stride
            mov  edi, dstp
            mov  edx, phase
        lpC:
            and  edx, edx
            js   ngC
            mov  [edi], al
            dec  edx
            jnz  ctC
            mov  edx, on
            jmp  ctC
        ngC:
            mov  [edi], ah
            inc  edx
            jnz  ctC
            mov  edx, off
        ctC:
            add  edi, ebx
            dec  ecx
            jnz  lpC
            pop  esi
            pop  edi
        }
    }
    this->release_data(1);
}


void JGLBuffer::fill_8(int color)
{
    int    height;
    int    width;
    uchar *p;
    if (RectEqual(this->get_clip(), this->get_rect()) == 0) {
        this->fill(this->get_clip(), color);
        return;
    }
    p = this->get_data_8();
    height = this->get_rect()->bottom;
    width = this->get_rect()->right;
    __asm {
        pushf
        push edi
        push esi
        cld
        mov  edi, p
        mov  eax, width
        mov  ebx, eax
        xor  ecx, ecx
        and  ebx, 3
        setnz cl
        shr  eax, 2
        add  eax, ecx
        mov  ecx, height
        mul  ecx
        mov  ecx, eax
        mov  dl, byte ptr color
        mov  al, dl
        mov  ah, dl
        shl  eax, 16
        mov  al, dl
        mov  ah, dl
        rep  stosd
        pop  esi
        pop  edi
        popf
    }
    this->release_data(1);
}


int JGLBuffer::fill8(Rect *pRect, int nColour)
{
    int    nHeight;
    int    nWidth;
    Rect   rc;
    uchar *pDst = 0;
    if (pRect == 0) {
        fill_8(nColour);
        return 0;
    }
    rc = *pRect;
    if (!jgl_IntersectRect(&rc, &rc, get_clip()))
        return 0;
    pDst = SURF_GetPixelPtr8(rc.left, rc.top);
    if (pDst == 0)
        return 0;
    nHeight = rc.bottom - rc.top;
    nWidth = rc.right - rc.left;
    g_fill8DstSkip = get_pitch() - nWidth;
    __asm {
        pushf
        push    edi
        push    esi
        cld
        mov     edi, pDst
        mov     edx, nWidth
        mov     ebx, edx
        and     ebx, 3
        shr     edx, 2
        mov     cl, byte ptr nColour
        mov     al, cl
        mov     ah, cl
        shl     eax, 16
        mov     al, cl
        mov     ah, cl
        mov     ecx, nHeight
        push    ebp
        mov     ebp, ecx
      row:
        mov     ecx, edx
        rep     stosd
        mov     ecx, ebx
        rep     stosb
        add     edi, g_fill8DstSkip
        dec     ebp
        jnz     row
        pop     ebp
        pop     esi
        pop     edi
        popf
    }
    release_data(1);
    return 0;
}


int JGLBuffer::dither_8(Rect *pRect, int nColour)
{
    uchar *pDst = 0;
    int    nSkip;
    int    nHeight;
    int    nWidth;
    Rect   rc;
    if (pRect == 0)
        pRect = get_rect();
    rc = *pRect;
    if (jgl_IntersectRect(&rc, &rc, get_clip())) {
        pDst = SURF_GetPixelPtr8(rc.left, rc.top);
        if (pDst != 0) {
            nHeight = rc.bottom - rc.top;
            nWidth = rc.right - rc.left;
            nSkip = get_pitch() - nWidth;
            __asm {
                pushf
                push    edi
                mov     ah, byte ptr nColour
                mov     edi, pDst
                mov     edx, nHeight
                mov     ecx, nWidth
                mov     ebx, ecx
                and     ebx, 1
              row:
                test    edx, 1
                jz      phase0
                inc     edi
              phase0:
                mov     ecx, nWidth
                shr     ecx, 1
                cmp     ecx, 0
                jz      nopairs
              pair:
                mov     byte ptr [edi], ah
                add     edi, 2
                loop    pair
              nopairs:
                test    edx, 1
                jz      evenrow
                test    ebx, 1
                jnz     nextrow
                dec     edi
                jmp     nextrow
              evenrow:
                test    ebx, 1
                jz      nextrow
                mov     byte ptr [edi], ah
                inc     edi
              nextrow:
                add     edi, nSkip
                dec     edx
                jnz     row
                pop     edi
                popf
            }
            release_data(1);
        }
    }
    return 0;
}

const char g_extPcx[] = ".pcx";
int        JGLBuffer::write_pcx_8(const char *path)
{
    Filemap     file;
    PcxHeader   hdr;
    uchar       runLen;
    uchar       pix;
    uchar      *out;
    uchar      *srcRow;
    int         i;
    char        fname[80];
    unsigned    x;
    char       *p;
    JGLPalette *pal;
    uchar       palbuf[768];
    fname[0] = 0;
    strcat(fname, path);
    p = fname + strlen(fname);
    while (p > fname && *p != '.')
        p--;
    if (*p == '.') {
        *p = 0;
        strcat(fname, g_extPcx);
    }
    srcRow = this->get_data_8();
    if (srcRow == 0)
        return 7;
    out = (uchar *)file.create(fname, this->get_width() * this->get_height() * 2 + 128, 1);
    if (out == 0) {
        this->release_data(1);
        return 4;
    }
    hdr.manufacturer = 0x0a;
    hdr.version = 5;
    hdr.encoding = 1;
    hdr.bitsPerPixel = 8;
    hdr.xmin = 0;
    hdr.ymin = 0;
    hdr.xmax = this->get_width() - 1;
    hdr.ymax = this->get_height() - 1;
    hdr.hdpi = 0;
    hdr.vdpi = 0;
    hdr.reserved = 0;
    hdr.numPlanes = 1;
    hdr.bytesPerLine = this->get_pitch();
    hdr.paletteInfo = 0;
    hdr.hScreenSize = 0;
    hdr.vScreenSize = 0;
    memcpy(out, &hdr, 128);
    out += 128;
    for (i = this->get_height(); i > 0; i--) {
        x = 0;
        while (x < this->get_pitch()) {
            runLen = 1;
            pix = srcRow[x];
            for (; x + runLen < this->get_pitch() - 1 && pix == srcRow[x + runLen] && runLen < 0x3f; runLen++)
                ;
            if (runLen > 1) {
                *out = runLen | 0xc0;
                out++;
                *out = pix;
                out++;
            } else if (srcRow[x] < 0x40) {
                *out = pix;
                out++;
            } else {
                *out = runLen | 0xc0;
                out++;
                *out = pix;
                out++;
            }
            x += runLen;
        }
        srcRow += this->get_pitch();
    }
    *out = 0x0c;
    out++;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else
        pal = def_palette->pal;
    if (pal != 0) {
        if (pal->get_entries(palbuf, 0, 256) == 0) {
            for (i = 0; i < 256; i++) {
                *out = palbuf[i * 3];
                out++;
                *out = palbuf[i * 3 + 1];
                out++;
                *out = palbuf[i * 3 + 2];
                out++;
            }
        }
    }
    this->release_data(1);
    file.Truncate(out);
    return 0;
}
