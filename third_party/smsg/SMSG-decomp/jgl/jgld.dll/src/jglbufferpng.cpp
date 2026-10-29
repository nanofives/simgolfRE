#include "jgldefs.h"
#include "jglsprite.h"
#include "filemap.h"
#include "jgltypes.h"
#include "jgl.h"
#include "jglpalettemgr.h"
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <stdio.h>
#include "jglpng.h"
#include "jglalloc.h"
int JGLBuffer::load_png(const char *name, JGLPalette *pPal, int first, int count)
{
    png_struct *png_ptr;
    png_info   *info_ptr;
    int         rc;
    ulong       width;
    ulong       height;
    int         bitDepth;
    int         colorType;
    int         interlace;
    FILE       *fp;
    float       gamma;
    int         rc2;
    png_color  *palette;
    int         numPalette;
    int         i;
    uchar       rgb[256 * 3];
    uchar     **rows;
    ulong       y;
    rc = 0;
    fp = fopen(name, "rb");
    if (fp == 0)
        return 6;
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, user_error_fn, user_error_fn);
    if (png_ptr == 0) {
        fclose(fp);
        return 1;
    }
    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == 0) {
        fclose(fp);
        png_destroy_read_struct(&png_ptr, 0, 0);
        return 1;
    }
    if (setjmp(png_ptr->jmpbuf)) {
        png_destroy_read_struct(&png_ptr, &info_ptr, 0);
        fclose(fp);
        return 1;
    }
    png_init_io(png_ptr, fp);
    png_read_info(png_ptr, info_ptr);
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bitDepth, &colorType, &interlace, 0, 0);
    if (colorType != 2 && info_ptr->pixel_depth != 0x18) {
        png_destroy_read_struct(&png_ptr, &info_ptr, 0);
        fclose(fp);
        return 0x17;
    }
    if (bitDepth == 16)
        png_set_swap(png_ptr);
    if (colorType & 4)
        png_set_strip_alpha(png_ptr);
    if (pPal != 0 && colorType == 3) {
        numPalette = 0x100;
        png_get_PLTE(png_ptr, info_ptr, &palette, &numPalette);
        for (i = 0; i < 256; i++) {
            rgb[i * 3 + 0] = palette[i].red;
            rgb[i * 3 + 1] = palette[i].green;
            rgb[i * 3 + 2] = palette[i].blue;
        }
        pPal->set_entries(&rgb[i * 3], first, count);
        this->set_current_palette(pPal);
    }
    gamma = 1.0f;
    png_set_bgr(png_ptr);
    rc2 = m_owner->resize_buffer(width, height, info_ptr->pixel_depth);
    this->fill(0, 2);
    rows = (uchar **)jgl_malloc(height * 4);
    for (y = 0; y < height; y++)
        rows[y] = (uchar *)this->SURF_GetPixelPtr(0, y);
    png_read_image(png_ptr, rows);
    png_read_end(png_ptr, info_ptr);
    png_destroy_read_struct(&png_ptr, &info_ptr, 0);
    fclose(fp);
    jgl_free(rows);
    return 0;
}
extern "C" void *jgl_malloc(unsigned int size)
{
    return malloc(size);
}
extern "C" int jgl_free(void *p)
{
    if (p == 0)
        return 0;
    free(p);
    return 0;
}
#include "jglglobals.h"
int g_scaleX = 1;
int g_scaleY = 1;
int g_scaleDenom = 1;
