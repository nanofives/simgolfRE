#include "jgldefs.h"
#include "AssetSurface.h"
#include "FLCPalette.h"
#include "crt.h"
#include "jglpcx.h"

int AssetSurface::LoadPCXFromFile(FILE *fp)
{
    unsigned char pal[768];
    PcxHeader hdr;
    int x;
    unsigned char run;
    unsigned char *dst;
    int remaining;
    unsigned char b;
    int w, h, linePixels, rc, i;
    FLCPalette *p;

    fread(&hdr, sizeof(hdr), 1, fp);
    if (hdr.manufacturer == 0x0a && hdr.encoding == 1 && hdr.version >= 4) {
        w = hdr.xmax - hdr.xmin + 1;
        h = hdr.ymax - hdr.ymin + 1;
        rc = resize(w, h, 8);
        if (rc)
            return rc;

        int colors = (1 << hdr.bitsPerPixel) * hdr.numPlanes;
        if (colors == 0x100) {
            x = 0;
            linePixels = (8 / hdr.bitsPerPixel) * hdr.bytesPerLine * hdr.numPlanes;
            remaining = linePixels * h;

            dst = GetPixels();
            if (!dst)
                return 7;

            while (remaining != 0) {
                fread(&b, 1, 1, fp);
                if ((b & 0xc0) == 0xc0) {
                    run = (unsigned char)(b & 0x3f);
                    fread(&b, 1, 1, fp);
                } else {
                    run = 1;
                }
                memset(dst, b, run);
                dst += run;
                x += run;
                remaining -= run;
                if (x >= linePixels) {
                    dst += GetPitch() - x;
                    remaining += x - linePixels;
                    x = 0;
                }
            }

            if (m_surface)
                m_surface->release_data(1);

            fread(&b, 1, 1, fp);
            for (i = 0; i < 256; i++) {
                fread(&b, 1, 1, fp);
                pal[i * 3] = b;
                fread(&b, 1, 1, fp);
                pal[i * 3 + 1] = b;
                fread(&b, 1, 1, fp);
                pal[i * 3 + 2] = b;
            }

            p = new FLCPalette;
            m_palette = p;
            if (p->pal == 0)
                p->init();
            p->pal->set_entries(pal, 0, 256);
            sync_to_palette(m_palette);
            return 0;
        }
    }
    return 3;
}

int AssetSurface::SaveToPCX(FILE *fp)
{
    PcxHeader hdr;
    int row;
    unsigned char run;
    unsigned char *pRow;
    unsigned char pix;
    unsigned char *pBuf;
    unsigned x;
    int n = 0;
    int i;

    if (m_surface == 0)
        return 3;
    pRow = m_surface->get_data_8();
    hdr.manufacturer = 10;
    hdr.version = 5;
    hdr.encoding = 1;
    hdr.bitsPerPixel = 8;
    hdr.xmin = 0;
    hdr.ymin = 0;
    hdr.xmax = GetSurfaceWidth() - 1;
    hdr.ymax = GetSurfaceHeight() - 1;
    hdr.hdpi = 0;
    hdr.vdpi = 0;
    hdr.reserved = 0;
    hdr.numPlanes = 1;
    hdr.bytesPerLine = GetPitch();
    hdr.paletteInfo = 0;
    hdr.hScreenSize = 0;
    hdr.vScreenSize = 0;
    fwrite(&hdr, sizeof(hdr), 1, fp);
    pBuf = (unsigned char *)operator new(GetSurfaceWidth() * GetSurfaceHeight() + 0x301);
    for (row = GetSurfaceHeight(); row > 0; row--) {
        x = 0;
        while (x < (unsigned)GetPitch()) {
            run = 1;
            pix = pRow[x];
            while (x + run < (unsigned)(GetPitch() - 1) && pix == pRow[x + run] && run < 0x3f)
                run++;
            if (run > 1) {
                pBuf[n++] = (unsigned char)(run | 0xc0);
                pBuf[n++] = pix;
            } else if (pRow[x] >= 0x40) {
                pBuf[n++] = (unsigned char)(run | 0xc0);
                pBuf[n++] = pix;
            } else {
                pBuf[n++] = pix;
            }
            x += run;
        }
        pRow += GetPitch();
    }
    pBuf[n++] = 0x0c;
    for (i = 0; i < 256; i++) {
        pBuf[n++] = m_surface->get_current_palette()->get_entry(i)->peRed;
        pBuf[n++] = m_surface->get_current_palette()->get_entry(i)->peGreen;
        pBuf[n++] = m_surface->get_current_palette()->get_entry(i)->peBlue;
    }
    fwrite(pBuf, n, 1, fp);
    operator delete(pBuf);
    if (m_surface)
        m_surface->release_data(1);
    return 0;
}

void AssetSurface::dotted_hline_8(int fromX, int toX, int y, unsigned char color)
{
    unsigned char *pd;

    if (m_surface == 0)
        return;
    if (y < m_surface->get_clip()->top)
        return;
    if (y >= GetClip()->bottom)
        return;
    if (fromX == toX)
        return;
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
    pd = m_surface->SURF_GetPixelPtr8(fromX, y);
    if (pd == 0)
        return;
    __asm {
        push edi
        mov  ecx, toX
        sub  ecx, fromX
        inc  ecx
        mov  ah, color
        mov  edi, pd
        shr  ecx, 1
    fill_loop:
        mov  [edi], ah
        add  edi, 2
        loop fill_loop
        pop  edi
    }
    Unlock(1);
}

void AssetSurface::dotted_vline_8(int x, int fromY, int toY, unsigned char color)
{
    unsigned char *pd;

    if (m_surface == 0)
        return;
    if (x < m_surface->get_clip()->left)
        return;
    if (x >= GetClip()->right)
        return;
    if (fromY == toY)
        return;
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
    pd = m_surface->SURF_GetPixelPtr8(x, fromY);
    if (pd == 0)
        return;
    x = GetPitch();
    __asm {
        push edi
        mov  ecx, toY
        sub  ecx, fromY
        inc  ecx
        mov  ah, color
        mov  ebx, x
        mov  edi, pd
        shr  ecx, 1
        shl  ebx, 1
    fill_loop:
        mov  [edi], ah
        add  edi, ebx
        loop fill_loop
        pop  edi
    }
    Unlock(1);
}
