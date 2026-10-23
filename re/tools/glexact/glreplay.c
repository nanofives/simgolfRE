/* Replay one captured Terrain.dll draw pass through the real Windows software GL and compare it with the game's own result.
   Input: a directory written by `re/tools/gl_record.py --init --snap` (stream.gl.txt, tex/<hash>.bin, vtx.bin, nrm.bin,
   dib_pre.bin, dib_post.bin, meta.txt). Steps: create the same DIB (size, 16 bpp, BI_BITFIELDS masks, orientation) and pixel
   format (PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL, 16 colour bits: g_pfd 0x10063e10), a context, replay every state call between
   wglCreateContext and the pass (draw blocks of earlier passes skipped, texture ids mapped), put dib_pre in the DIB, replay the
   pass (glPushMatrix .. glFlush), compare the DIB with dib_post.
   Usage: glreplay <dir> [out.bin]       GLREPLAY_CTL=1 skips one glBegin..glEnd block of the pass (negative control). */
#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int u32;
static char **lines; static int nlines;
static int meta_i(const char *dir, const char *key, int def) {
    char path[512], k[64]; int v; FILE *f;
    sprintf(path, "%s\\meta.txt", dir); f = fopen(path, "r"); if (!f) return def;
    while (fscanf(f, "%63s %d", k, &v) == 2) if (!strcmp(k, key)) { fclose(f); return v; }
    fclose(f); return def;
}
static unsigned char *slurp(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb"); unsigned char *b; long sz;
    if (!f) return 0;
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    b = (unsigned char *)malloc(sz + 1); fread(b, 1, sz, f); fclose(f); if (n) *n = sz; return b;
}
static unsigned long long fnv64(const unsigned char *p, size_t n) {
    unsigned long long h = 1469598103934665603ULL; while (n--) { h ^= *p++; h *= 1099511628211ULL; } return h;
}
static float F(u32 b) { float f; memcpy(&f, &b, 4); return f; }
static double D(u32 lo, u32 hi) { unsigned long long q = ((unsigned long long)hi << 32) | lo; double d; memcpy(&d, &q, 8); return d; }

#define MAXID 4096
static u32 idmap_rec[MAXID], idmap_real[MAXID]; static int nid;
static u32 mapid(u32 rec) { int i; for (i = nid - 1; i >= 0; i--) if (idmap_rec[i] == rec) return idmap_real[i]; return rec; }

static const char *dir; static unsigned char *vtx, *nrm; static int nskipped_tex;

/* returns number of u32 args parsed; extras: v[] floats bits, tex hash, ids */
static int parse(char *line, char *name, u32 *a, u32 *v, int *nv, char *tex, u32 *ids, int *nids) {
    char *sp = strchr(line, ' '), *p, *tok; int n = 0;
    *nv = 0; *nids = 0; tex[0] = 0;
    if (!sp) { strcpy(name, line); return 0; }
    memcpy(name, line, sp - line); name[sp - line] = 0;
    p = sp + 1;
    while (*p) {
        tok = p; while (*p && *p != ' ') p++;
        { char save = *p; *p = 0;
          if (!strncmp(tok, "v=", 2)) { char *c = tok + 2; while (*c && *nv < 8) { v[(*nv)++] = (u32)strtoul(c, &c, 16); if (*c == ',') c++; } }
          else if (!strncmp(tok, "ids=", 4)) { char *c = tok + 4; while (*c && *nids < 64) { ids[(*nids)++] = (u32)strtoul(c, &c, 16); if (*c == ',') c++; } }
          else if (!strncmp(tok, "tex=", 4)) strcpy(tex, tok + 4);
          else if (!strncmp(tok, "p=", 2)) { }
          else { char *c = tok; while (*c && n < 16) { a[n++] = (u32)strtoul(c, &c, 16); if (*c == ',') c++; } }
          *p = save; }
        if (*p == ' ') p++;
    }
    return n;
}

static int exec(char *line, int in_pass) {
    char name[64], tex[32]; u32 a[16] = {0}, v[8], ids[64]; int nv, nids; GLuint real[64]; int i;
    char buf[2048]; strncpy(buf, line, sizeof buf - 1); buf[sizeof buf - 1] = 0;
    if (!buf[0] || buf[0] == '#') return 0;
    parse(buf, name, a, v, &nv, tex, ids, &nids);
    if (!strncmp(name, "wgl", 3) || !strncmp(name, "glu", 3)) return 0;
    if (!strcmp(name, "glEnableClientState")) glEnableClientState(a[0]);
    else if (!strcmp(name, "glHint")) glHint(a[0], a[1]);
    else if (!strcmp(name, "glFrontFace")) glFrontFace(a[0]);
    else if (!strcmp(name, "glEnable")) glEnable(a[0]);
    else if (!strcmp(name, "glDisable")) glDisable(a[0]);
    else if (!strcmp(name, "glNormal3fv")) { float f[3]; for (i = 0; i < 3; i++) f[i] = F(v[i]); glNormal3fv(f); }
    else if (!strcmp(name, "glVertex3fv")) { float f[3]; for (i = 0; i < 3; i++) f[i] = F(v[i]); glVertex3fv(f); }
    else if (!strcmp(name, "glTexCoord2fv")) { float f[2]; for (i = 0; i < 2; i++) f[i] = F(v[i]); glTexCoord2fv(f); }
    else if (!strcmp(name, "glVertex2fv")) { float f[2]; for (i = 0; i < 2; i++) f[i] = F(v[i]); glVertex2fv(f); }
    else if (!strcmp(name, "glArrayElement")) glArrayElement(a[0]);
    else if (!strcmp(name, "glTexParameterf")) glTexParameterf(a[0], a[1], F(a[2]));
    else if (!strcmp(name, "glTexParameteri")) glTexParameteri(a[0], a[1], a[2]);
    else if (!strcmp(name, "glTexEnvi")) glTexEnvi(a[0], a[1], a[2]);
    else if (!strcmp(name, "glGenTextures")) {
        glGenTextures(a[0] > 64 ? 64 : a[0], real);
        for (i = 0; i < nids && i < (int)a[0] && nid < MAXID; i++) { idmap_rec[nid] = ids[i]; idmap_real[nid++] = real[i]; }
    }
    else if (!strcmp(name, "glDeleteTextures")) {
        for (i = 0; i < nids && i < 64; i++) real[i] = mapid(ids[i]);
        if (nids) glDeleteTextures(nids, real);
    }
    else if (!strcmp(name, "glBindTexture")) glBindTexture(a[0], mapid(a[1]));
    else if (!strcmp(name, "glTexImage2D")) {
        char path[600]; size_t n; unsigned char *px;
        sprintf(path, "%s\\tex\\%s.bin", dir, tex);
        px = tex[0] ? slurp(path, &n) : 0;
        if (!px) { nskipped_tex++; return 0; }
        glTexImage2D(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], px);
        free(px);
    }
    else if (!strcmp(name, "glVertexPointer")) glVertexPointer(a[0], a[1], a[2], vtx);
    else if (!strcmp(name, "glNormalPointer")) glNormalPointer(a[0], a[1], nrm);
    else if (!strcmp(name, "glViewport")) glViewport(a[0], a[1], a[2], a[3]);
    else if (!strcmp(name, "glScalef")) glScalef(F(a[0]), F(a[1]), F(a[2]));
    else if (!strcmp(name, "glNormal3f")) glNormal3f(F(a[0]), F(a[1]), F(a[2]));
    else if (!strcmp(name, "glTexCoord2f")) glTexCoord2f(F(a[0]), F(a[1]));
    else if (!strcmp(name, "glVertex3f")) glVertex3f(F(a[0]), F(a[1]), F(a[2]));
    else if (!strcmp(name, "glRotated")) glRotated(D(a[0], a[1]), D(a[2], a[3]), D(a[4], a[5]), D(a[6], a[7]));
    else if (!strcmp(name, "glRotatef")) glRotatef(F(a[0]), F(a[1]), F(a[2]), F(a[3]));
    else if (!strcmp(name, "glTranslatef")) glTranslatef(F(a[0]), F(a[1]), F(a[2]));
    else if (!strcmp(name, "glMatrixMode")) glMatrixMode(a[0]);
    else if (!strcmp(name, "glPushMatrix")) glPushMatrix();
    else if (!strcmp(name, "glLoadIdentity")) glLoadIdentity();
    else if (!strcmp(name, "glOrtho")) glOrtho(D(a[0], a[1]), D(a[2], a[3]), D(a[4], a[5]), D(a[6], a[7]), D(a[8], a[9]), D(a[10], a[11]));
    else if (!strcmp(name, "glBlendFunc")) glBlendFunc(a[0], a[1]);
    else if (!strcmp(name, "glLineWidth")) glLineWidth(F(a[0]));
    else if (!strcmp(name, "glColor4f")) glColor4f(F(a[0]), F(a[1]), F(a[2]), F(a[3]));
    else if (!strcmp(name, "glBegin")) glBegin(a[0]);
    else if (!strcmp(name, "glVertex2i")) glVertex2i(a[0], a[1]);
    else if (!strcmp(name, "glEnd")) glEnd();
    else if (!strcmp(name, "glPopMatrix")) glPopMatrix();
    else if (!strcmp(name, "glFlush")) glFlush();
    else if (!strcmp(name, "glMaterialfv")) { float f[4]; for (i = 0; i < 4; i++) f[i] = F(v[i]); glMaterialfv(a[0], a[1], f); }
    else if (!strcmp(name, "glMateriali")) glMateriali(a[0], a[1], a[2]);
    else if (!strcmp(name, "glLightfv")) { float f[4]; for (i = 0; i < 4; i++) f[i] = F(v[i]); glLightfv(a[0], a[1], f); }
    else { printf("unhandled call %s\n", name); return 1; }
    (void)in_pass;
    return 0;
}

int main(int argc, char **argv) {
    char path[600]; size_t n, sz; unsigned char *text, *p, *pre, *post;
    int W, H, wb, bpp, bih, comp, start, end, i, ctx_line = -1, unh = 0;
    BYTE bi_buf[sizeof(BITMAPINFOHEADER) + 12]; BITMAPINFOHEADER *bih_p = (BITMAPINFOHEADER *)bi_buf; DWORD *masks = (DWORD *)(bi_buf + sizeof(BITMAPINFOHEADER));
    void *bits = 0; HDC dc; HBITMAP bm; HGLRC rc; int pf; PIXELFORMATDESCRIPTOR pfd;
    if (argc < 2) { printf("usage: glreplay <dir> [out.bin]\n"); return 2; }
    dir = argv[1];
    W = meta_i(dir, "w", 800); H = meta_i(dir, "h", 600); wb = meta_i(dir, "wb", 1600); bpp = meta_i(dir, "bpp", 16);
    bih = meta_i(dir, "biHeight", 600); comp = meta_i(dir, "comp", 3); start = meta_i(dir, "startIdx", -1); end = meta_i(dir, "endIdx", -1);
    sprintf(path, "%s\\stream.gl.txt", dir); text = slurp(path, &sz);
    sprintf(path, "%s\\vtx.bin", dir); vtx = slurp(path, 0);
    sprintf(path, "%s\\nrm.bin", dir); nrm = slurp(path, 0);
    sprintf(path, "%s\\dib_pre.bin", dir); pre = slurp(path, &n);
    sprintf(path, "%s\\dib_post.bin", dir); post = slurp(path, &n);
    if (!text || !vtx || !nrm || !pre || !post || start < 0) { printf("missing input\n"); return 2; }
    text[sz] = 0;
    for (p = text; *p; ) {   /* split lines in place */
        unsigned char *e = p; while (*e && *e != '\n') e++;
        lines = (char **)realloc(lines, sizeof(char *) * (nlines + 1)); lines[nlines++] = (char *)p;
        if (*e) { if (e > p && e[-1] == '\r') e[-1] = 0; *e = 0; p = e + 1; } else p = e;
    }
    for (i = 0; i < nlines; i++) if (!strncmp(lines[i], "wglCreateContext", 16)) { ctx_line = i; break; }
    printf("stream %d lines, context at %d, pass %d..%d, DIB %dx%d %dbpp comp %d biHeight %d\n", nlines, ctx_line, start, end, W, H, bpp, comp, bih);

    memset(bi_buf, 0, sizeof bi_buf);
    bih_p->biSize = sizeof(BITMAPINFOHEADER); bih_p->biWidth = W; bih_p->biHeight = bih; bih_p->biPlanes = 1; bih_p->biBitCount = (WORD)bpp; bih_p->biCompression = comp;
    masks[0] = meta_i(dir, "m0", 0x7c00); masks[1] = meta_i(dir, "m1", 0x3e0); masks[2] = meta_i(dir, "m2", 0x1f);
    dc = CreateCompatibleDC(0);
    bm = CreateDIBSection(dc, (BITMAPINFO *)bi_buf, DIB_RGB_COLORS, &bits, 0, 0);
    SelectObject(dc, bm);
    memset(&pfd, 0, sizeof pfd); pfd.nSize = sizeof pfd; pfd.nVersion = 1; pfd.dwFlags = PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL; pfd.cColorBits = 16;
    pf = ChoosePixelFormat(dc, &pfd);
    if (!pf || !SetPixelFormat(dc, pf, &pfd)) { printf("pixel format failed\n"); return 2; }
    rc = wglCreateContext(dc); wglMakeCurrent(dc, rc);
    printf("pixel format %d, %s | %s\n", pf, glGetString(GL_VENDOR), glGetString(GL_RENDERER));

    {   /* state calls before the pass: no draw blocks, no flushes */
        int in_begin = 0;
        for (i = ctx_line + 1; i < start; i++) {
            if (!strncmp(lines[i], "glBegin", 7)) { in_begin = 1; continue; }
            if (!strncmp(lines[i], "glEnd", 5)) { in_begin = 0; continue; }
            if (in_begin || !strncmp(lines[i], "glFlush", 7)) continue;
            unh += exec(lines[i], 0);
        }
    }
    memcpy(bits, pre, (size_t)wb * (bih < 0 ? -bih : bih)); GdiFlush();
    {   int blocks = 0, skip_block = getenv("GLREPLAY_CTL") ? 500 : -1, skipping = 0;
        for (i = start; i <= end && i < nlines; i++) {
            if (!strncmp(lines[i], "glBegin", 7)) { if (blocks++ == skip_block) { skipping = 1; continue; } }
            if (skipping) { if (!strncmp(lines[i], "glEnd", 5)) skipping = 0; continue; }
            unh += exec(lines[i], 1);
        }
    }
    glFlush(); GdiFlush();
    {   size_t bytes = (size_t)wb * (bih < 0 ? -bih : bih), k, ndiff = 0, first = (size_t)-1; unsigned short *x = (unsigned short *)bits, *y = (unsigned short *)post;
        for (k = 0; k < bytes / 2; k++) if (x[k] != y[k]) { if (first == (size_t)-1) first = k; ndiff++; }
        printf("replay hash %016llx  game hash %016llx  pre hash %016llx\n", fnv64((unsigned char *)bits, bytes), fnv64(post, bytes), fnv64(pre, bytes));
        { size_t changed = 0; for (k = 0; k < bytes / 2; k++) if (((unsigned short *)pre)[k] != y[k]) changed++;
          printf("pixels the game changed in the pass: %lu\n", (unsigned long)changed); }
        printf("pixels differing from the game's result: %lu of %lu%s", (unsigned long)ndiff, (unsigned long)(bytes / 2), ndiff ? "" : "  -> BIT-EXACT");
        if (ndiff) printf("  (first at x=%lu row-in-memory=%lu: replay %04x game %04x)", (unsigned long)(first % W), (unsigned long)(first / W), x[first], y[first]);
        printf("\ntexture uploads without bytes skipped: %d, unhandled calls: %d\n", nskipped_tex, unh);
        if (argc > 2) { FILE *f = fopen(argv[2], "wb"); fwrite(bits, 1, bytes, f); fclose(f); }
        return ndiff ? 1 : 0;
    }
}
