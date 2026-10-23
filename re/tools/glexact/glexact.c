/* Software-GL determinism probe (Phase 5 step 3, re/PHASE5_TERRAIN_PLAN.md section 6).
   Terrain.dll draws with Microsoft's generic OpenGL onto a 16-bit DIB (g_pfd 0x10063e10: DRAW_TO_BITMAP | SUPPORT_OPENGL,
   16 colour bits, no depth). This renders a synthetic terrain-like scene through the same setup (the state recorded from the original,
   log/p5/gl/init_a.gl.txt: GL_CW, REPLACE, LINEAR / NEAREST, no mip-maps, the recorded light, ortho and viewport; vertex arrays + glArrayElement + glTexCoord2fv, GL_TRIANGLES, line smoothing) and prints the FNV-64
   of the DIB bits. Usage: glexact [passes] [out.bmp]. Same hash in every pass and every process = bit-exact. */
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define W 800
#define H 600
static unsigned lcg_s = 12345;
static unsigned lcg(void) { lcg_s = lcg_s * 1664525u + 1013904223u; return lcg_s >> 8; }

static unsigned long long fnv64(const unsigned char *p, size_t n) {
    unsigned long long h = 1469598103934665603ULL;
    while (n--) { h ^= *p++; h *= 1099511628211ULL; }
    return h;
}

#define GRID 40
static float verts[(GRID + 1) * (GRID + 1) * 3], norms[(GRID + 1) * (GRID + 1) * 3], tcs[4][2] = {{0,0},{1,0},{1,1},{0,1}};
static unsigned short idx[GRID * GRID * 6];

static void build(void) {
    int x, y, n = 0;
    lcg_s = 777;
    for (y = 0; y <= GRID; y++) for (x = 0; x <= GRID; x++) {
        float hgt = (float)(lcg() % 40);
        verts[3*(y*(GRID+1)+x)+0] = (x - GRID/2) * 14.0f;
        verts[3*(y*(GRID+1)+x)+1] = hgt;
        verts[3*(y*(GRID+1)+x)+2] = (y - GRID/2) * 14.0f;
        norms[3*(y*(GRID+1)+x)+0] = (float)((int)(lcg() % 21) - 10) / 40.0f;
        norms[3*(y*(GRID+1)+x)+1] = 1.0f;
        norms[3*(y*(GRID+1)+x)+2] = (float)((int)(lcg() % 21) - 10) / 40.0f;
    }
    for (y = 0; y < GRID; y++) for (x = 0; x < GRID; x++) {
        unsigned short a = y*(GRID+1)+x, b = a+1, c = a+(GRID+1), d = c+1;
        idx[n++]=a; idx[n++]=b; idx[n++]=c; idx[n++]=b; idx[n++]=d; idx[n++]=c;   /* front faces under glFrontFace(GL_CW) with this view */
    }
}

static void render(void) {
    int i, t;
    static const float amb[4] = {0.784313738f,0.784313738f,0.705882370f,0.0f}, dif[4] = {0.941176474f,0.941176474f,0.941176474f,0.784313738f},
        pos[4] = {-0.607330859f,0.768866241f,0.199984953f,0.0f}, spec[4] = {1.0f,1.0f,0.960784316f,0.941176474f}, mspec[4] = {1,1,1,1};
    glViewport(0, 0, W, H);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(-884.0, 883.0, -663.0, 662.0, -5000.0, 5000.0);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    glEnable(GL_LINE_SMOOTH); glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_TEXTURE_2D); glEnable(GL_LIGHTING); glFrontFace(GL_CW); glEnable(GL_CULL_FACE);
    glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_NORMAL_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, verts); glNormalPointer(GL_FLOAT, 0, norms);
    glEnable(GL_LIGHT0); glLightfv(GL_LIGHT0, GL_AMBIENT, amb); glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
    glLightfv(GL_LIGHT0, GL_POSITION, pos); glLightfv(GL_LIGHT0, GL_SPECULAR, spec);
    glMaterialfv(GL_FRONT, GL_SPECULAR, mspec); glMateriali(GL_FRONT, GL_SHININESS, 13);
    glPushMatrix();
    glRotated(38.682186, 1, 0, 0); glRotatef(45.0f, 0, 1, 0); glTranslatef(0, 0, 0);
    for (t = 0; t < 2; t++) {
        glBindTexture(GL_TEXTURE_2D, 1 + t);
        glBegin(GL_TRIANGLES);
        for (i = t * (GRID*GRID*3); i < (t + 1) * (GRID*GRID*3); i++) {
            glTexCoord2fv(tcs[i % 4]);
            glArrayElement(idx[i]);
        }
        glEnd();
    }
    glPopMatrix();
    glFlush();
}

int main(int argc, char **argv) {
    int passes = argc > 1 ? atoi(argv[1]) : 3, p, t, x, y;
    const char *out = argc > 2 ? argv[2] : 0;
    BITMAPINFO bi; void *bits = 0; HDC dc; HBITMAP bm; HGLRC rc; int pf; unsigned long long h0 = 0, h;
    PIXELFORMATDESCRIPTOR pfd;
    unsigned char tex[64*64*3];
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = W; bi.bmiHeader.biHeight = H;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 16; bi.bmiHeader.biCompression = BI_RGB;
    dc = CreateCompatibleDC(0);
    bm = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, 0, 0);
    SelectObject(dc, bm);
    memset(&pfd, 0, sizeof pfd); pfd.nSize = sizeof pfd; pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL; pfd.cColorBits = 16;   /* g_pfd 0x10063e10 */
    pf = ChoosePixelFormat(dc, &pfd);
    if (!pf || !SetPixelFormat(dc, pf, &pfd)) { printf("pixel format failed\n"); return 2; }
    rc = wglCreateContext(dc); wglMakeCurrent(dc, rc);
    { PIXELFORMATDESCRIPTOR got; DescribePixelFormat(dc, pf, sizeof got, &got);
      printf("pf %d flags 0x%lx color %d depth %d  GL %s | %s\n", pf, got.dwFlags, got.cColorBits, got.cDepthBits,
             glGetString(GL_VENDOR), glGetString(GL_RENDERER)); }
    build();
    if (getenv("GLEXACT_CTL")) verts[3*800+1] += 1.0f;   /* negative control: one vertex 1 unit higher must change the hash */
    for (t = 0; t < 2; t++) {
        lcg_s = 99 + t;
        for (y = 0; y < 64; y++) for (x = 0; x < 64; x++) {
            unsigned v = lcg(); int k = 3*(y*64+x);
            tex[k] = (unsigned char)(80 + (v & 63) + t * 40); tex[k+1] = (unsigned char)(120 + ((v >> 6) & 63));
            tex[k+2] = (unsigned char)(40 + ((v >> 12) & 31));
        }
        glBindTexture(GL_TEXTURE_2D, 1 + t);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);   /* 0x2802 / 0x2803 = 0x2901 */
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);   /* recorded: 0x2800 = 0x2600 */
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);    /* 0x2801 = 0x2601, no mip-maps */
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);          /* 0x2200 = 0x1e01 */
        glTexImage2D(GL_TEXTURE_2D, 0, 3, 64, 64, 0, GL_RGB, GL_UNSIGNED_BYTE, tex);
    }
    for (p = 0; p < passes; p++) {
        memset(bits, 0, W * H * 2);
        render();
        GdiFlush();
        h = fnv64((unsigned char *)bits, W * H * 2);
        if (!p) h0 = h;
        printf("pass %d hash %016llx %s\n", p, h, h == h0 ? "same" : "DIFFERENT");
    }
    if (out) { BITMAPFILEHEADER fh; FILE *f = fopen(out, "wb"); memset(&fh, 0, sizeof fh);
        fh.bfType = 0x4d42; fh.bfOffBits = sizeof fh + sizeof bi.bmiHeader; fh.bfSize = fh.bfOffBits + W*H*2;
        fwrite(&fh, sizeof fh, 1, f); fwrite(&bi.bmiHeader, sizeof bi.bmiHeader, 1, f); fwrite(bits, 1, W*H*2, f); fclose(f); }
    wglMakeCurrent(0, 0); wglDeleteContext(rc);
    return 0;
}
