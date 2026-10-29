#include "jgldefs.h"
#include "Degrees.h"
#include "AssetSurface.h"
#include "FLCPalette.h"
#include "crt.h"
#include "jglpcx.h"

int AssetSurface::load_pcx_8_8(char *pData, int nSize, FLCPalette *pPal, int first, int count)
{
    Dib bmi;
    PcxHeader hdr;
    unsigned char run;
    unsigned char *dst;
    unsigned char *src;
    int x;
    int remaining;
    int h, lineBytes, rc, i;
    unsigned char *rgb;
    unsigned char b;

    if (pData != 0) {
        if (strncmp(pData, Civ3encrypt, strlen(Civ3encrypt)) == 0) {
            pData += strlen(Civ3encrypt) + 1;
            nSize -= strlen(Civ3encrypt) + 1;
        }
        memcpy(&hdr, pData, sizeof(hdr));
        src = (unsigned char *)pData + sizeof(hdr);
        if (hdr.manufacturer == 0x0a && hdr.encoding == 1 && (hdr.version >= 4 || pPal == 0)) {
            h = hdr.ymax - hdr.ymin + 1;
            rc = resize(hdr.xmax - hdr.xmin + 1, h, 8);
            if (rc != 0)
                return rc;
            if ((1 << hdr.bitsPerPixel) * hdr.numPlanes == 0x100) {
                x = 0;
                lineBytes = (8 / hdr.bitsPerPixel) * hdr.bytesPerLine * hdr.numPlanes;
                remaining = lineBytes * h;
                dst = GetPixels();
                if (dst == 0)
                    return 7;
                while (remaining != 0) {
                    b = *src++;
                    if ((b & 0xc0) == 0xc0) {
                        run = (unsigned char)(b & 0x3f);
                        b = *src++;
                    } else
                        run = 1;
                    memset(dst, (unsigned char)(first + b), run);
                    dst += run;
                    x += run;
                    remaining -= run;
                    if (x >= lineBytes) {
                        dst += GetPitch() - x;
                        remaining += x - lineBytes;
                        x = 0;
                    }
                }
                if (m_surface != 0)
                    m_surface->release_data(1);
                rgb = (unsigned char *)pData + nSize - 0x300;
                for (i = first; i < first + count; i++) {
                    int k = i % 256;
                    bmi.bmiColors[k].rgbRed = *rgb++;
                    bmi.bmiColors[k].rgbGreen = *rgb++;
                    bmi.bmiColors[k].rgbBlue = *rgb++;
                    bmi.bmiColors[k].rgbReserved = 0;
                }
                if (pPal == 0)
                    return 0;
                pPal->set_from_dib(&bmi, first, count);
                sync_to_palette(pPal);
                return 0;
            }
        }
    }
    return 3;
}
