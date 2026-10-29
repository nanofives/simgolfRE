#include "jgldefs.h"
#include "Degrees.h"
#include "AssetSurface.h"
#include "Surface.h"
#include "FLCPalette.h"

void AssetSurface::dotted_hline_16(int fromX, int toX, int y, int color)
{
    unsigned short *pd;

    if (m_surface == 0)
        return;
    if (y < m_surface->get_clip()->top)
        return;
    if (y >= GetClip()->bottom)
        return;
    if (fromX == toX)
        return;
    if ((color & 0x80000000) == 0) {
        FLCPalette *pal = g_activePalettePtr;
        if (pal != 0) {
            switch (GetPixelFormat()->code) {
            case 0: color = (pal->pal ? pal->pal->get_555_table() : 0)[color & 0xff]; break;
            case 1: color = (pal->pal ? pal->pal->get_565_table() : 0)[color & 0xff]; break;
            default: return;
            }
        }
    }
    color = (unsigned short)color;
    if (fromX > toX) {
        fromX ^= toX;
        toX ^= fromX;
        fromX ^= toX;
    }
    if (fromX >= GetClip()->right)
        return;
    if (toX < GetClip()->left)
        return;
    if (fromX < GetClip()->left)
        fromX = GetClip()->left;
    if (toX >= GetClip()->right)
        toX = GetClip()->right - 1;
    if (m_surface == 0)
        return;
    pd = m_surface->SURF_GetPixelPtr16(fromX, y);
    if (pd == 0)
        return;
    __asm {
        push edi
        mov  ecx, toX
        sub  ecx, fromX
        inc  ecx
        mov  ax, word ptr color
        mov  edi, pd
        shr  ecx, 1
    fill_loop:
        mov  word ptr [edi], ax
        add  edi, 4
        dec  ecx
        jnz  fill_loop
        pop  edi
    }
    Unlock(1);
}

void AssetSurface::dotted_vline_16(int x, int fromY, int toY, int color)
{
    unsigned short *pd;

    if (m_surface == 0)
        return;
    if (x < m_surface->get_clip()->left)
        return;
    if (x >= GetClip()->right)
        return;
    if (fromY == toY)
        return;
    if ((color & 0x80000000) == 0) {
        FLCPalette *pal = g_activePalettePtr;
        if (pal != 0) {
            switch (GetPixelFormat()->code) {
            case 0: color = (pal->pal ? pal->pal->get_555_table() : 0)[color & 0xff]; break;
            case 1: color = (pal->pal ? pal->pal->get_565_table() : 0)[color & 0xff]; break;
            default: return;
            }
        }
    }
    color = (unsigned short)color;
    if (fromY > toY) {
        fromY ^= toY;
        toY ^= fromY;
        fromY ^= toY;
    }
    if (fromY >= GetClip()->bottom)
        return;
    if (toY < GetClip()->top)
        return;
    if (fromY < GetClip()->top)
        fromY = GetClip()->top;
    if (toY >= GetClip()->bottom)
        toY = GetClip()->bottom - 1;
    if (m_surface == 0)
        return;
    pd = m_surface->SURF_GetPixelPtr16(x, fromY);
    if (pd == 0)
        return;
    x = m_surface ? m_surface->get_pitch() : 0;
    __asm {
        push edi
        mov  ecx, toY
        sub  ecx, fromY
        inc  ecx
        mov  ax, word ptr color
        mov  ebx, x
        mov  edi, pd
        shr  ecx, 1
        shl  ebx, 2
    fill_loop:
        mov  word ptr [edi], ax
        add  edi, ebx
        dec  ecx
        jnz  fill_loop
        pop  edi
    }
    Unlock(1);
}
