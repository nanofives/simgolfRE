/* glreplay.c lines 1..138 (re/tools/glexact/glreplay.c), the texture-name map made thread-local for roadmap S3.20's band measurement (glband.c). */
/* Replay captured Terrain.dll draw passes through the real Windows software GL and compare each with the game's own result.
   Inputs come from `re/tools/gl_record.py --init --snap` (one pass, flat directory: stream.gl.txt, tex/<hash>.bin, vtx.bin, nrm.bin,
   dib_pre.bin, dib_post.bin, meta.txt) or from `re/tools/gl_corpus.py` (a corpus root with stream.gl.txt, tex/ and snaps/<step>/ holding
   pass.gl.txt, vtx.bin, nrm.bin, dib_pre.bin, dib_post.bin, meta.txt).
   Steps: create the same DIB (size, 16 bpp, BI_BITFIELDS masks, orientation) and pixel format (PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL,
   16 colour bits: g_pfd 0x10063e10), a context, replay every state call between wglCreateContext and the pass (draw blocks and flushes
   skipped, texture ids mapped), put dib_pre in the DIB, replay the pass (glPushMatrix .. glFlush), compare the DIB with dib_post.
   Usage: glreplay <dir> [out.bin]            single-pass directory
          glreplay <root> <step> [out.bin]    corpus step
   GLREPLAY_CTL=1 skips one glBegin..glEnd block of the pass (negative control). Exit status 0 = bit-exact. */
#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int u32;
static char **lines; static int nlines;
static int meta_i(const char *dir, const char *key, int def) {
    char path[700], k[64]; int v; FILE *f;
    sprintf(path, "%s/meta.txt", dir); f = fopen(path, "r"); if (!f) return def;
    while (fscanf(f, "%63s %d", k, &v) == 2) if (!strcmp(k, key)) { fclose(f); return v; }
    fclose(f); return def;
}
static unsigned char *slurp(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb"); unsigned char *b; long sz;
    if (!f) return 0;
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    b = (unsigned char *)malloc(sz + 1); fread(b, 1, sz, f); fclose(f); b[sz] = 0; if (n) *n = sz; return b;
}
static unsigned long long fnv64(const unsigned char *p, size_t n) {
    unsigned long long h = 1469598103934665603ULL; while (n--) { h ^= *p++; h *= 1099511628211ULL; } return h;
}
static float F(u32 b) { float f; memcpy(&f, &b, 4); return f; }
static double D(u32 lo, u32 hi) { unsigned long long q = ((unsigned long long)hi << 32) | lo; double d; memcpy(&d, &q, 8); return d; }

#define MAXID 4096
static __declspec(thread) u32 idmap_rec[MAXID], idmap_real[MAXID]; static __declspec(thread) int nid;
static u32 mapid(u32 rec) { int i; for (i = nid - 1; i >= 0; i--) if (idmap_rec[i] == rec) return idmap_real[i]; return rec; }

static const char *dir, *sdir; static unsigned char *vtx, *nrm; static __declspec(thread) int nskipped_tex;

/* split a text buffer into lines in place; returns the count */
static int split(unsigned char *text, char ***out) {
    unsigned char *p; int n = 0; char **v = 0;
    for (p = text; *p; ) {
        unsigned char *e = p; while (*e && *e != 10) e++;
        v = (char **)realloc(v, sizeof(char *) * (n + 1)); v[n++] = (char *)p;
        if (*e) { if (e > p && e[-1] == 13) e[-1] = 0; *e = 0; p = e + 1; } else p = e;
    }
    *out = v; return n;
}

/* returns number of u32 args parsed; extras: v[] float bits, tex hash, ids */
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

static int exec(char *line) {
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
        char path[800]; size_t n; unsigned char *px;
        sprintf(path, "%s/tex/%s.bin", dir, tex);
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
    return 0;
}
