// bitmap.c in Terrain.dll (debug build, compiled as C): LoadDIBitmap / SaveDIBitmap, the BMP helpers from the
// OpenGL SuperBible sample code (Michael Sweet), used by the texture loader. abs() here is the CRT's
// (0x100158b0, re/match/terrain_iabs.cpp).
// LANG c
// FLAGS Terrain.dll: /Od /ZI /GZ /MTd
#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>

// MATCH: Terrain.dll 0x100016a0 _LoadDIBitmap
GLubyte *LoadDIBitmap(const char *filename, BITMAPINFO **info)
{
    FILE             *fp;
    GLubyte          *bits;
    int              bitsize;
    int              infosize;
    BITMAPFILEHEADER header;

    if ((fp = fopen(filename, "rb")) == NULL)
        return (NULL);
    if (fread(&header, sizeof(BITMAPFILEHEADER), 1, fp) < 1)
    {
        fclose(fp);
        return (NULL);
    }
    if (header.bfType != 'MB')
    {
        fclose(fp);
        return (NULL);
    }
    infosize = header.bfOffBits - sizeof(BITMAPFILEHEADER);
    if ((*info = (BITMAPINFO *)malloc(infosize)) == NULL)
    {
        fclose(fp);
        return (NULL);
    }
    if (fread(*info, 1, infosize, fp) < infosize)
    {
        free(*info);
        fclose(fp);
        return (NULL);
    }
    if ((bitsize = (*info)->bmiHeader.biSizeImage) == 0)
        bitsize = ((*info)->bmiHeader.biWidth *
                   (*info)->bmiHeader.biBitCount + 7) / 8 *
                  abs((*info)->bmiHeader.biHeight);
    if ((bits = malloc(bitsize)) == NULL)
    {
        free(*info);
        fclose(fp);
        return (NULL);
    }
    if (fread(bits, 1, bitsize, fp) < bitsize)
    {
        free(*info);
        free(bits);
        fclose(fp);
        return (NULL);
    }
    fclose(fp);
    return (bits);
}

// MATCH: Terrain.dll 0x10001900 _SaveDIBitmap
int SaveDIBitmap(const char *filename, BITMAPINFO *info, GLubyte *bits)
{
    FILE             *fp;
    int              size, infosize, bitsize;
    BITMAPFILEHEADER header;

    if ((fp = fopen(filename, "wb")) == NULL)
        return (-1);
    if (info->bmiHeader.biSizeImage == 0)
        bitsize = (info->bmiHeader.biWidth *
                   info->bmiHeader.biBitCount + 7) / 8 *
                  abs(info->bmiHeader.biHeight);
    else
        bitsize = info->bmiHeader.biSizeImage;
    infosize = sizeof(BITMAPINFOHEADER);
    switch (info->bmiHeader.biCompression)
    {
        case BI_BITFIELDS :
            infosize += 12;
            if (info->bmiHeader.biClrUsed == 0)
              break;
        case BI_RGB :
            if (info->bmiHeader.biBitCount > 8 &&
                info->bmiHeader.biClrUsed == 0)
              break;
        case BI_RLE8 :
        case BI_RLE4 :
            if (info->bmiHeader.biClrUsed == 0)
              infosize += (1 << info->bmiHeader.biBitCount) * 4;
            else
              infosize += info->bmiHeader.biClrUsed * 4;
            break;
    }
    size = sizeof(BITMAPFILEHEADER) + infosize + bitsize;
    header.bfType      = 'MB';
    header.bfSize      = size;
    header.bfReserved1 = 0;
    header.bfReserved2 = 0;
    header.bfOffBits   = sizeof(BITMAPFILEHEADER) + infosize;
    if (fwrite(&header, 1, sizeof(BITMAPFILEHEADER), fp) < sizeof(BITMAPFILEHEADER))
    {
        fclose(fp);
        return (-1);
    }
    if (fwrite(info, 1, infosize, fp) < infosize)
    {
        fclose(fp);
        return (-1);
    }
    if (fwrite(bits, 1, bitsize, fp) < bitsize)
    {
        fclose(fp);
        return (-1);
    }
    fclose(fp);
    return (0);
}
