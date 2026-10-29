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
int JGLBuffer::fill_shadow_over_underlay_16_16(Rect *pr, JGLBuffer *src, int pct, ushort *lut)
{
    ushort *dstp;
    ushort *srcp;
    int     advance;
    int     w;
    short   h;
    int     level = (15 - (int)(pct / 100.0f * 15.0f)) << 15;
    ushort *tbl = lut;
    if (IntersectRect((LPRECT)pr, (const RECT *)pr, (const RECT *)this->get_rect()) == 0)
        return 3;
    dstp = this->SURF_GetPixelPtr16(pr->left, pr->top);
    srcp = src->SURF_GetPixelPtr16(pr->left, pr->top);
    w = pr->right - pr->left;
    h = pr->bottom - pr->top;
    advance = (this->get_pitch() - w) * 2;
    __asm {
        push esi
        push edi
        mov  esi, srcp
        mov  edi, dstp
        mov  edx, w
        shl  edx, 10h
        mov  dx, h
        mov  ebx, level
        push ebp
        mov  ebp, tbl
    L_row:
        mov  ecx, edx
        shr  ecx, 10h
    L_pix:
        mov  eax, ebx
        cmp  word ptr [edi], 7c1fh
        jnz  L_self
        or   ax, word ptr [esi]
        mov  ax, word ptr [ebp + eax*2]
        mov  word ptr [edi], ax
        jmp  L_next
    L_self:
        or   ax, word ptr [edi]
        mov  ax, word ptr [ebp + eax*2]
        mov  word ptr [edi], ax
    L_next:
        add  esi, 2
        add  edi, 2
        dec  cx
        jnz  L_pix
        pop  ebp
        add  esi, advance
        add  edi, advance
        push ebp
        mov  ebp, tbl
        dec  dx
        jnz  L_row
        pop  ebp
        pop  edi
        pop  esi
    }
    return 0;
}
