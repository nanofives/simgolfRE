#include "jgldefs.h"
#include "AssetSurface.h"
#include "Surface.h"
#include "PolyFill.h"
#include "FLCPalette.h"

const float FIXED8_SCALE = 256.0f;

inline float ToFixed8(float value)
{
    return value * FIXED8_SCALE;
}

static int s_color = 0;
static int s_bottomVertex = 0;
static int s_spare = 0;
static Vert *s_pts = 0;
static int s_clipLeft = 0;
static int s_clipTop = 0;
static int s_spareRun[5] = {0};
static int s_count = 0;
static void *s_bits16 = 0;
static void *s_bits8 = 0;
static int s_y = 0;
static int s_pitch = 0;
static int s_clipRight = 0;
static int s_clipBottom = 0;

int setup_edge_poly(EdgeScanPoly *edge, int vertex)
{
    int next, dx;
    for (;;) {
        if (vertex == s_bottomVertex)
            return 0;
        next = vertex + edge->dir;
        if (next > s_count - 1)
            next = 0;
        else if (next < 0)
            next = s_count - 1;
        edge->rowsLeft = s_pts[next].y - s_pts[vertex].y;
        if (edge->rowsLeft != 0) {
            if (edge->rowsLeft < 0)
                return 0;
            edge->endVertex = next;
            edge->x = s_pts[vertex].x;
            dx = s_pts[next].x - s_pts[vertex].x;
            if (dx < 0) {
                dx = -dx;
                edge->xRound = -1;
                edge->err = 1 - edge->rowsLeft;
                edge->xStep = -(dx / edge->rowsLeft);
            } else {
                edge->xRound = 1;
                edge->err = 0;
                edge->xStep = dx / edge->rowsLeft;
            }
            edge->errInc = dx % edge->rowsLeft;
            edge->errDec = edge->rowsLeft;
            return 1;
        }
        vertex = next;
    }
}

int step_edge_poly(EdgeScanPoly *edge)
{
    if (--edge->rowsLeft == 0)
        return setup_edge_poly(edge, edge->endVertex) != 0;
    edge->x += edge->xStep;
    edge->err += edge->errInc;
    if (edge->err > 0) {
        edge->x += edge->xRound;
        edge->err -= edge->errDec;
    }
    return 1;
}

void scan_out_line_poly_16(int xLeft, int xRight)
{
    __asm {
        push edi
        mov  ebx, xRight
        cmp  ebx, s_clipLeft
        jle  done
        mov  ecx, xLeft
        cmp  ecx, s_clipRight
        jge  done
        sub  ebx, ecx
        jle  done
        mov  ecx, xRight
        cmp  ecx, s_clipRight
        jl   have_r
        mov  ecx, s_clipRight
        mov  xRight, ecx
    have_r:
        mov  edi, xLeft
        cmp  edi, s_clipLeft
        jge  have_l
        mov  edi, s_clipLeft
        mov  xLeft, edi
    have_l:
        sub  ecx, edi
        jle  done
        mov  eax, s_y
        imul dword ptr s_pitch
        add  eax, xLeft
        add  eax, xLeft
        add  eax, s_bits16
        mov  edi, eax
        mov  ax, word ptr s_color
    fill_loop:
        mov  word ptr [edi], ax
        add  edi, 2
        dec  ecx
        jne  fill_loop
    done:
        pop  edi
    }
}

void scan_out_line_poly_8(int xLeft, int xRight)
{
    __asm {
        push edi
        mov  ebx, xRight
        cmp  ebx, s_clipLeft
        jle  done
        mov  ecx, xLeft
        cmp  ecx, s_clipRight
        jge  done
        sub  ebx, ecx
        jle  done
        mov  ecx, xRight
        cmp  ecx, s_clipRight
        jl   have_r
        mov  ecx, s_clipRight
        mov  xRight, ecx
    have_r:
        mov  edi, xLeft
        cmp  edi, s_clipLeft
        jge  have_l
        mov  edi, s_clipLeft
        mov  xLeft, edi
    have_l:
        sub  ecx, edi
        jle  done
        mov  eax, s_y
        imul dword ptr s_pitch
        add  eax, xLeft
        add  eax, s_bits8
        mov  edi, eax
        mov  ah, byte ptr s_color
    fill_loop:
        mov  [edi], ah
        inc  edi
        dec  ecx
        jne  fill_loop
    done:
        pop  edi
    }
}

int FillPolygon8(AssetSurface *surface, Vert *pts, int count, int color)
{
    EdgeScanPoly left, right;
    Rect *clip;
    void *bits;
    int i, top, minY, maxY, pitch;
    if (pts == 0 || surface == 0)
        return 16;
    bits = surface->Lock8();
    s_bits8 = bits;
    if (bits == 0)
        goto done;
    s_pts = pts;
    s_count = count;
    s_color = color;
    pitch = surface->m_surface ? surface->m_surface->get_pitch() : 0;
    s_pitch = pitch;
    if (surface->m_surface == 0)
        goto done;
    clip = surface->m_surface->get_clip();
    if (clip == 0)
        goto done;
    s_clipRight = clip->right - 1;
    s_clipLeft = clip->left;
    s_clipBottom = clip->bottom - 1;
    s_clipTop = clip->top;
    maxY = -32765;
    minY = 32767;
    for (i = 0; i < count; i++) {
        if (pts[i].y < minY) {
            minY = s_pts[i].y;
            top = i;
        }
        if (pts[i].y > maxY) {
            maxY = s_pts[i].y;
            s_bottomVertex = i;
        }
    }
    if (minY >= maxY)
        goto done;
    s_y = minY;
    left.dir = -1;
    if (setup_edge_poly(&left, top) == 0)
        goto done;
    right.dir = 1;
    if (setup_edge_poly(&right, top) == 0)
        goto done;
    for (;;) {
        if (s_y >= s_clipBottom)
            break;
        if (s_y >= s_clipTop) {
            if (left.x < right.x)
                scan_out_line_poly_8(left.x, right.x);
            else
                scan_out_line_poly_8(right.x, left.x);
        }
        if (step_edge_poly(&left) == 0)
            break;
        if (step_edge_poly(&right) == 0)
            break;
        s_y++;
    }
done:
    if (surface->m_surface)
        surface->m_surface->release_data(1);
    return 0;
}

int FillPolygon16(AssetSurface *surface, Vert *pts, int count, int color)
{
    EdgeScanPoly left, right;
    Rect *clip;
    void *bits;
    int i, top, minY, maxY;
    if (pts == 0 || surface == 0)
        return 16;
    bits = surface->Lock16();
    s_bits16 = bits;
    if (bits == 0)
        goto done;
    if ((color & 0x80000000) == 0) {
        FLCPalette *pal = g_activePalettePtr;
        if (pal != 0) {
            switch ((surface->m_surface ? surface->m_surface->get_pixformat() : 0)->code) {
            case 0: color = (pal->pal ? pal->pal->get_555_table() : 0)[color & 0xff]; break;
            case 1: color = (pal->pal ? pal->pal->get_565_table() : 0)[color & 0xff]; break;
            default: return 1;
            }
        }
    }
    s_pts = pts;
    s_count = count;
    s_color = color & 0xffff;
    s_pitch = (surface->m_surface ? surface->m_surface->get_pitch() : 0) * 2;
    if (surface->m_surface == 0)
        goto done;
    clip = surface->m_surface->get_clip();
    if (clip == 0)
        goto done;
    s_clipRight = clip->right - 1;
    s_clipLeft = clip->left;
    s_clipBottom = clip->bottom - 1;
    s_clipTop = clip->top;
    maxY = -32765;
    minY = 32767;
    for (i = 0; i < count; i++) {
        if (pts[i].y < minY) {
            minY = s_pts[i].y;
            top = i;
        }
        if (pts[i].y > maxY) {
            maxY = s_pts[i].y;
            s_bottomVertex = i;
        }
    }
    if (minY >= maxY)
        goto done;
    s_y = minY;
    left.dir = -1;
    if (setup_edge_poly(&left, top) == 0)
        goto done;
    right.dir = 1;
    if (setup_edge_poly(&right, top) == 0)
        goto done;
    for (;;) {
        if (s_y >= s_clipBottom)
            break;
        if (s_y >= s_clipTop) {
            if (left.x < right.x)
                scan_out_line_poly_16(left.x, right.x);
            else
                scan_out_line_poly_16(right.x, left.x);
        }
        if (step_edge_poly(&left) == 0)
            break;
        if (step_edge_poly(&right) == 0)
            break;
        s_y++;
    }
done:
    if (surface->m_surface)
        surface->m_surface->release_data(1);
    return 0;
}

int FillPolygon(AssetSurface *surface, Vert *pts, int count, int color)
{
    switch ((surface->m_surface ? surface->m_surface->get_pixformat() : 0)->bpp) {
    case 8: return FillPolygon8(surface, pts, count, color);
    case 16: return FillPolygon16(surface, pts, count, color);
    }
    return 23;
}

int g_polySpare[3] = {0};
