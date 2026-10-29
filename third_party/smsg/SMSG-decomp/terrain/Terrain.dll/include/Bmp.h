#ifndef BMP_H
#define BMP_H

#include <windows.h>
#include "GL/gl.h"

unsigned char *LoadBmpFile(const char *pszFile, BITMAPINFOHEADER **ppbi);

void SwapBgrToRgb(unsigned char *pBits, BITMAPINFOHEADER *pbi);

GLuint UploadBmpTexture(const char *pszFile, int, GLenum eMinFilter, GLenum eMagFilter, GLenum eWrap);

int WriteDibToBmpFile(const char *pszFile, BITMAPINFOHEADER *pbi, unsigned char *pBits);

void FlipImageRows24bpp(unsigned char *bits, BITMAPINFOHEADER *bih);

#endif
