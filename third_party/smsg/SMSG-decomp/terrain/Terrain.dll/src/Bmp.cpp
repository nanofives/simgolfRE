#include "Bmp.h"
#include "Terrain.h"

#include "GL/glu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


unsigned char *LoadBmpFile(const char *pszFile, BITMAPINFOHEADER **ppbi)
{
    FILE *fp;
    unsigned char *pBits;
    int nSizeImage;
    int nHeaderBytes;
    BITMAPFILEHEADER bfh;

    fp = fopen(pszFile, "rb");
    if (fp == 0)
        return 0;

    if (fread(&bfh, sizeof(BITMAPFILEHEADER), 1, fp) < 1) {
        fclose(fp);
        return 0;
    }

    if (bfh.bfType != 0x4d42) {
        fclose(fp);
        return 0;
    }

    nHeaderBytes = bfh.bfOffBits - sizeof(BITMAPFILEHEADER);
    *ppbi = (BITMAPINFOHEADER *)malloc(nHeaderBytes);
    if (*ppbi == 0) {
        fclose(fp);
        return 0;
    }

    if (fread(*ppbi, 1, nHeaderBytes, fp) < nHeaderBytes) {
        free(*ppbi);
        fclose(fp);
        return 0;
    }

    nSizeImage = (*ppbi)->biSizeImage;
    if (nSizeImage == 0)
        nSizeImage = ((*ppbi)->biWidth * (*ppbi)->biBitCount + 7) / 8 * abs((*ppbi)->biHeight);

    pBits = (unsigned char *)malloc(nSizeImage);
    if (pBits == 0) {
        free(*ppbi);
        fclose(fp);
        return 0;
    }

    if (fread(pBits, 1, nSizeImage, fp) < nSizeImage) {
        free(*ppbi);
        free(pBits);
        fclose(fp);
        return 0;
    }

    fclose(fp);
    return pBits;
}

int WriteDibToBmpFile(const char *pszFile, BITMAPINFOHEADER *pbi, unsigned char *pBits)
{
    FILE *fp;
    int nFileSize;
    int nHeaderBytes;
    int nSizeImage;
    BITMAPFILEHEADER bfh;

    fp = fopen(pszFile, "wb");
    if (fp == 0)
        return -1;

    if (pbi->biSizeImage == 0)
        nSizeImage = (pbi->biWidth * pbi->biBitCount + 7) / 8 * abs(pbi->biHeight);
    else
        nSizeImage = pbi->biSizeImage;

    nHeaderBytes = sizeof(BITMAPINFOHEADER);

    switch (pbi->biCompression) {
    case 3:
        nHeaderBytes += 12;
        if (pbi->biClrUsed == 0)
            break;
    case 0:
        if (pbi->biBitCount > 8 && pbi->biClrUsed == 0)
            break;
    case 1:
    case 2:
        if (pbi->biClrUsed == 0)
            nHeaderBytes += (1 << pbi->biBitCount) * 4;
        else
            nHeaderBytes += pbi->biClrUsed * 4;
        break;
    }

    nFileSize = nHeaderBytes + nSizeImage + sizeof(BITMAPFILEHEADER);

    bfh.bfType = 0x4d42;
    bfh.bfSize = nFileSize;
    bfh.bfReserved1 = 0;
    bfh.bfReserved2 = 0;
    bfh.bfOffBits = nHeaderBytes + sizeof(BITMAPFILEHEADER);

    if (fwrite(&bfh, 1, sizeof(BITMAPFILEHEADER), fp) < sizeof(BITMAPFILEHEADER)) {
        fclose(fp);
        return -1;
    }
    if (fwrite(pbi, 1, nHeaderBytes, fp) < nHeaderBytes) {
        fclose(fp);
        return -1;
    }
    if (fwrite(pBits, 1, nSizeImage, fp) < nSizeImage) {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

void SwapBgrToRgb(unsigned char *pBits, BITMAPINFOHEADER *pbi)
{
    unsigned char *p;
    int n = 0;
    unsigned char t;

    n = pbi->biWidth * pbi->biHeight;

    for (p = pBits; n > 0; n--, p += 3) {
        t = p[0];
        p[0] = p[2];
        p[2] = t;
    }
}

void FlipImageRows24bpp(unsigned char *bits, BITMAPINFOHEADER *bih)
{
    unsigned char *p;
    unsigned char *q;
    int y = 0;
    unsigned char tmp[3];
    int x;

    for (y = 0; y < bih->biHeight >> 1; y++) {
        for (x = 0; x < bih->biWidth; x++) {
            p = bits + (x + bih->biWidth * y) * 3;
            q = p + (bih->biHeight - 1 - y) * bih->biWidth * 3;
            tmp[0] = p[0];
            tmp[1] = p[1];
            tmp[2] = p[2];
            p[0] = q[0];
            p[1] = q[1];
            p[2] = q[2];
            q[0] = tmp[0];
            q[1] = tmp[1];
            q[2] = tmp[2];
        }
    }
}

Tile *Terrain::tileAt(int x, int y)
{
    if (x >= m_width || x < 0 || y >= m_height || y < 0)
        return 0;

    return &m_tiles[x + y * m_width];
}

int Terrain::getElevation(Tile *pTile, int nCorner)
{
    if (pTile)
        return pTile->getCornerElevation(nCorner);
    else
        return 0;
}

bool Terrain::getWall(Tile *pTile, int nDir)
{
    return pTile->getWallEnabled(nDir);
}

int Terrain::getType(Tile *pTile)
{
    return pTile->getType();
}
