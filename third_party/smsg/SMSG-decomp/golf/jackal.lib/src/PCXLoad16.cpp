#include "jgldefs.h"
#include "AssetSurface.h"
#include "FLCPalette.h"
#include "crt.h"
#include "jglpixelformat.h"
#include "MiscUtil.h"
#include "jglpcx.h"

int AssetSurface::load_pcx_8_16(char *pData, int nSize, FLCPalette *pPal, int first, int count)
{
    FLCPalette pal;
    PcxHeader hdr;
    unsigned short *lut;
    unsigned char *src;
    int remaining;
    unsigned char run;
    int w, h, lineBytes, rc, x;
    unsigned short *dst;

    if (pData) {
        if (GetPixelFormat()->code == 0)
            lut = pal.pal ? pal.pal->get_555_table() : 0;
        else
            lut = pal.pal ? pal.pal->get_565_table() : 0;
        if (strncmp(pData, Civ3encrypt, strlen(Civ3encrypt)) == 0)
            pData += strlen(Civ3encrypt) + 1;
        memcpy(&hdr, pData, sizeof(hdr));
        src = (unsigned char *)pData + sizeof(hdr);
        if (hdr.manufacturer == 0x0a && hdr.encoding == 1) {
            if (hdr.version >= 4 || pPal == 0) {
                w = hdr.xmax - hdr.xmin + 1;
                h = hdr.ymax - hdr.ymin + 1;
                rc = resize(w, h, 16);
                if (rc)
                    return rc;
                int bits = (1 << hdr.bitsPerPixel) * hdr.numPlanes;
                if (bits == 0x100) {
                    lineBytes = (8 / hdr.bitsPerPixel) * hdr.bytesPerLine * hdr.numPlanes;
                    remaining = lineBytes * h;
                    x = 0;
                    dst = m_surface ? m_surface->get_data_16() : 0;
                    if (!dst)
                        return 7;
                    while (remaining != 0) {
                        unsigned char b = *src++;
                        if ((b & 0xc0) == 0xc0) {
                            run = (unsigned char)(b & 0x3f);
                            b = *src++;
                        } else {
                            run = 1;
                        }
                        b += (unsigned char)first;
                        unsigned short colour = lut[b];
                        memsetword(dst, colour, run);
                        x += run;
                        dst += run;
                        remaining -= run;
                        if (x >= lineBytes) {
                            int pitch = m_surface ? m_surface->get_pitch() : 0;
                            dst += pitch - x;
                            x -= lineBytes;
                            remaining += x;
                            x = 0;
                        }
                    }
                    if (m_surface)
                        m_surface->release_data(1);
                    if (pPal == 0)
                        return 0;
                    *pPal = pal;
                    return 0;
                }
            }
        }
    }
    return 3;
}
