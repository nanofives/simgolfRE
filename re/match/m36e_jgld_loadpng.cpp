// FLAGS jgld.dll: /Od /ZI /GZ
// jgld.dll Surface::loadPng 0x100145a0 (Surface vtable slot +0x74; SurfaceBase's default 0x10009bd0 returns 0x18),
// round 36 batch m36e. Reads a PNG through libpng 1.0.5 (callees matched in jgld_png_*.cpp): 6 when fopen fails, 1 for
// a libpng failure (create, info, or a longjmp back to the setjmp at 0x10014667), 0x17 unless the image is RGB
// (color_type 2) or 24 bits per pixel (info_ptr->pixel_depth, byte +0x1e); swaps 16-bit samples, strips alpha, hands an
// 8-bit palette to the caller's Palette (slot +0x14) and installs it (this slot +0xec), sets BGR order, has the object at
// this+0x80 create the surface (its slot +4 with width, height, pixel depth), clears it (slot +0x44 with 0, 2), and reads
// the rows into pointers from lockAt(0, y) (slot +0xc). Names are chosen here.
// Facts the instructions show: the error and warning callbacks are one function (both pushes 0x1000117c at
// 0x100145ee); the palette call passes &colors[i] AFTER the loop (i = 256, lea at 0x100147f3), past the 768-byte array;
// the frame slots ebp-0x10 (= 0), ebp-0x2c (= 1.0f) and ebp-0x30 (the create result) are written and never read.
// With setjmp in the body VC6 emits no closing __chkesp, as the original (0x10014942).
#include <windows.h>
#include <stdio.h>
#include "vendor/libpng-1.0.5/png.h"
void* jalloc(size_t n);                  // 0x10014a40 (thunk 0x100017c6)
int jfree(void* p);                      // 0x10014a90 (thunk 0x100017b7)
void pngHandler(png_structp png_ptr, png_const_charp msg);   // thunk 0x1000117c, error and warning callback
extern const char s_rb[];                // 0x1011d370 (fopen mode)
class Palette {
public:
    virtual void p0(); virtual void p1(); virtual void p2(); virtual void p3(); virtual void p4();
    virtual void setColors(png_color* c, int a, int b);     // slot 5 (+0x14)
};
class SurfaceOwner {
public:
    virtual void o0();
    virtual int create(png_uint_32 w, png_uint_32 h, int bpp);   // slot 1 (+4)
};
class Surface {
public:
    virtual void v0(); virtual void v1(); virtual void v2();
    virtual void* lockAt(int x, int y);  // slot 3 (+0x0c)
    virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9();
    virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16();
    virtual void fillRectC(RECT* r, int c);   // slot 17 (+0x44)
    virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28();
    virtual int loadPng(const char* name, Palette* pal, int a, int b);   // slot 29 (+0x74)
    virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39();
    virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49();
    virtual void v50(); virtual void v51(); virtual void v52(); virtual void v53(); virtual void v54(); virtual void v55(); virtual void v56(); virtual void v57(); virtual void v58();
    virtual void setPalette(Palette* p);  // slot 59 (+0xec)
    char pad4[0x80 - 4];
    SurfaceOwner* m_owner;               // +0x80
};

// MATCH: jgld.dll 0x100145a0 ?loadPng@Surface@@UAEHPBDPAVPalette@@HH@Z
int Surface::loadPng(const char* name, Palette* pal, int a, int b)
{
    png_structp png_ptr;
    png_infop info_ptr;
    int unused10;
    png_uint_32 width;
    png_uint_32 height;
    int bit_depth;
    int color_type;
    int interlace_type;
    FILE* fp;
    float gamma;
    int created;
    png_colorp palette;
    int num_palette;
    int i;
    png_color colors[256];
    png_bytep* rows;
    png_uint_32 y;
    unused10 = 0;
    fp = fopen(name, s_rb);
    if (fp == 0)
        return 6;
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, pngHandler, pngHandler);
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
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, 0, 0);
    if (color_type != 2 && info_ptr->pixel_depth != 24) {
        png_destroy_read_struct(&png_ptr, &info_ptr, 0);
        fclose(fp);
        return 0x17;
    }
    if (bit_depth == 16)
        png_set_swap(png_ptr);
    if (color_type & 4)
        png_set_strip_alpha(png_ptr);
    if (pal != 0 && color_type == 3) {
        num_palette = 0x100;
        png_get_PLTE(png_ptr, info_ptr, &palette, &num_palette);
        for (i = 0; i < 0x100; i++) {
            colors[i].red = palette[i].red;
            colors[i].green = palette[i].green;
            colors[i].blue = palette[i].blue;
        }
        pal->setColors(&colors[i], a, b);
        setPalette(pal);
    }
    gamma = 1.0f;
    png_set_bgr(png_ptr);
    created = m_owner->create(width, height, info_ptr->pixel_depth);
    fillRectC(0, 2);
    rows = (png_bytep*)jalloc(height * sizeof(png_bytep));
    for (y = 0; y < height; y++)
        rows[y] = (png_bytep)lockAt(0, y);
    png_read_image(png_ptr, rows);
    png_read_end(png_ptr, info_ptr);
    png_destroy_read_struct(&png_ptr, &info_ptr, 0);
    fclose(fp);
    jfree(rows);
    return 0;
}
