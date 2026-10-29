#ifndef JGLWIN_H
#define JGLWIN_H
#include <windows.h>
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL 0x020A
#endif
#include "jglrect.h"
struct RgbQuad {
    unsigned char rgbBlue, rgbGreen, rgbRed, rgbReserved;
};
#include "jgltypes.h"
struct DevModeA {
    char   dmDeviceName[CCHDEVICENAME];
    ushort dmSpecVersion;
    ushort dmDriverVersion;
    ushort dmSize;
    ushort dmDriverExtra;
    ulong  dmFields;
    short  dmOrientation;
    short  dmPaperSize;
    short  dmPaperLength;
    short  dmPaperWidth;
    short  dmScale;
    short  dmCopies;
    short  dmDefaultSource;
    short  dmPrintQuality;
    short  dmColor;
    short  dmDuplex;
    short  dmYResolution;
    short  dmTTOption;
    short  dmCollate;
    char   dmFormName[CCHFORMNAME];
    ushort dmLogPixels;
    ulong  dmBitsPerPel;
    ulong  dmPelsWidth;
    ulong  dmPelsHeight;
    ulong  dmDisplayFlags;
    ulong  dmDisplayFrequency;
    ulong  dmICMMethod;
    ulong  dmICMIntent;
    ulong  dmMediaType;
    ulong  dmDitherType;
    ulong  dmReserved1;
    ulong  dmReserved2;
};
struct BitmapInfoHeader {
    ulong  biSize;
    int    biWidth;
    int    biHeight;
    ushort biPlanes;
    ushort biBitCount;
    ulong  biCompression;
    ulong  biSizeImage;
    int    biXPelsPerMeter;
    int    biYPelsPerMeter;
    ulong  biClrUsed;
    ulong  biClrImportant;
};
struct BitmapInfo256 {
    BitmapInfoHeader bmiHeader;
    RgbQuad          bmiColors[256];
};
#endif
