/* Roadmap S3.20, measurement only: does Windows' generic OpenGL give the same pixels when one terrain pass is drawn by N contexts on N
   threads, each into its own full 16-bit 555 DIB with glScissor limiting it to one horizontal band of rows, the bands then composed?
   And how long does the pass take that way?

   For a corpus step (port/corpus/gl/<root>/snaps/<step>, the ORIGINAL's recorded pass, glreplay.c's input) it:
     1. replays the state stream and the pass in one context (glreplay.c's method), timing the pass only (glFlush + GdiFlush included);
     2. for each N given: N worker threads each make their own DC, DIB (the same BITMAPINFO), pixel format and context, replay the
        state stream, load dib_pre, set glScissor(0, y0, W, y1 - y0) + glEnable(GL_SCISSOR_TEST) for band [y0, y1) = [H*b/N, H*(b+1)/N)
        (memory row = GL window y for biHeight > 0), then wait on a barrier; the pass is timed from the release of the barrier until
        every thread has replayed it, flushed and copied its band's rows into the composed frame;
     3. compares: single vs the original's dib_post, composed vs single (must be 0), composed vs dib_post.
   Usage: glband <root> <step> <reps> <N> [N ...]
   Output: one line per run: `single us=<pass us> diff_post=<px>` and `bands n=<N> us=<pass us> diff_single=<px> diff_post=<px>`;
   reps runs of each, median printed at the end. Exit status 1 if any composed frame differs from the single one. */
#include "glband_exec.inc"

#include <process.h>

static BYTE g_bi[sizeof(BITMAPINFOHEADER) + 12];
static int W, H, WB, BIH, g_start, g_npl;
static char **g_plines;
static unsigned char *g_pre, *g_post;
static const char *g_step;

static LARGE_INTEGER qf;
static double now_us(void) { LARGE_INTEGER t; QueryPerformanceCounter(&t); return (double)t.QuadPart * 1e6 / (double)qf.QuadPart; }

typedef struct { HDC dc; HBITMAP bm; HGLRC rc; void *bits; } Surf;

/* GLBAND_SERIAL=1: the DC, DIB, pixel format and context of each thread made one thread at a time (the hang probe of 20_notes.md) */
static CRITICAL_SECTION g_mk; static volatile LONG g_mk_init;
static int make_surf_inner(Surf *s);
static int make_surf(Surf *s) {
    int r;
    if (!getenv("GLBAND_SERIAL")) return make_surf_inner(s);
    if (InterlockedCompareExchange(&g_mk_init, 1, 0) == 0) { InitializeCriticalSection(&g_mk); g_mk_init = 2; }
    while (g_mk_init != 2) Sleep(0);
    EnterCriticalSection(&g_mk); r = make_surf_inner(s); LeaveCriticalSection(&g_mk);
    return r;
}
static int make_surf_inner(Surf *s) {
    PIXELFORMATDESCRIPTOR pfd; int pf;
    s->dc = CreateCompatibleDC(0);
    s->bm = CreateDIBSection(s->dc, (BITMAPINFO *)g_bi, DIB_RGB_COLORS, &s->bits, 0, 0);
    SelectObject(s->dc, s->bm);
    memset(&pfd, 0, sizeof pfd); pfd.nSize = sizeof pfd; pfd.nVersion = 1; pfd.dwFlags = PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL; pfd.cColorBits = 16;
    pf = ChoosePixelFormat(s->dc, &pfd);
    if (!pf || !SetPixelFormat(s->dc, pf, &pfd)) return 0;
    s->rc = wglCreateContext(s->dc);
    return s->rc && wglMakeCurrent(s->dc, s->rc);
}
static void free_surf(Surf *s) { wglMakeCurrent(0, 0); wglDeleteContext(s->rc); DeleteDC(s->dc); DeleteObject(s->bm); }

static void replay_state(void) {
    int i, in_begin = 0, ctx_line = -1;
    for (i = 0; i < nlines; i++) if (!strncmp(lines[i], "wglCreateContext", 16)) { ctx_line = i; break; }
    for (i = ctx_line + 1; i < g_start && i < nlines; i++) {
        if (!strncmp(lines[i], "glBegin", 7)) { in_begin = 1; continue; }
        if (!strncmp(lines[i], "glEnd", 5)) { in_begin = 0; continue; }
        if (in_begin || !strncmp(lines[i], "glFlush", 7)) continue;
        exec(lines[i]);
    }
}
static void replay_lines(char **pl, int n) { int i; for (i = 0; i < n; i++) exec(pl[i]); glFlush(); glFinish(); GdiFlush(); }
static void replay_pass(void) { replay_lines(g_plines, g_npl); }

/* ------------------------------------------------------------------ bands */
typedef struct { int b, n; unsigned short *out; HANDLE ready, go, done; } Band;

static unsigned __stdcall band_main(void *arg) {
    Band *t = (Band *)arg; Surf s; int y0 = H * t->b / t->n, y1 = H * (t->b + 1) / t->n, y, ncut = 0; char **cut = 0;
    const char *cd = getenv("GLBAND_CUT");   /* per-band cuts of the pass (cut.py): <dir>/<step>/b<k>of<N>.txt */
    if (cd) {
        char p[900]; unsigned char *ct;
        sprintf(p, "%s/%s/b%dof%d.txt", cd, g_step, t->b, t->n); ct = slurp(p, 0);
        if (!ct) { printf("no cut %s\n", p); exit(2); }
        ncut = split(ct, &cut);
    }
    if (!make_surf(&s)) { printf("band %d: no pixel format\n", t->b); SetEvent(t->ready); return 1; }
    replay_state();
    memcpy(s.bits, g_pre, (size_t)WB * H); GdiFlush();
    glScissor(0, y0, W, y1 - y0); glEnable(GL_SCISSOR_TEST);
    SetEvent(t->ready);
    WaitForSingleObject(t->go, INFINITE);
    if (cut) replay_lines(cut, ncut); else replay_pass();
    for (y = y0; y < y1; y++) memcpy(t->out + (size_t)y * W, (unsigned short *)s.bits + (size_t)y * W, (size_t)W * 2);
    SetEvent(t->done);
    free_surf(&s);
    return 0;
}

static double run_bands(int n, unsigned short *out) {
    Band t[64]; HANDLE th[64], dn[64]; HANDLE go = CreateEvent(0, TRUE, FALSE, 0); int b; double t0, t1;
    for (b = 0; b < n; b++) {
        t[b].b = b; t[b].n = n; t[b].out = out; t[b].ready = CreateEvent(0, TRUE, FALSE, 0); t[b].go = go; t[b].done = CreateEvent(0, TRUE, FALSE, 0);
        dn[b] = t[b].done;
        th[b] = (HANDLE)_beginthreadex(0, 0, band_main, &t[b], 0, 0);
    }
    for (b = 0; b < n; b++) WaitForSingleObject(t[b].ready, INFINITE);
    t0 = now_us(); SetEvent(go);
    WaitForMultipleObjects(n, dn, TRUE, INFINITE);
    t1 = now_us();
    WaitForMultipleObjects(n, th, TRUE, INFINITE);
    for (b = 0; b < n; b++) { CloseHandle(th[b]); CloseHandle(t[b].ready); CloseHandle(t[b].done); }
    CloseHandle(go);
    return t1 - t0;
}

static unsigned long diff(const unsigned short *a, const unsigned short *b) {
    unsigned long n = 0; int k; for (k = 0; k < W * H; k++) n += a[k] != b[k]; return n;
}
static int cmpd(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
static double median(double *v, int n) { qsort(v, n, sizeof *v, cmpd); return n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2; }

int main(int argc, char **argv) {
    char path[800], sdpath[800]; size_t sz, psz; unsigned char *text, *ptext; int reps, a, r, bad = 0;
    BITMAPINFOHEADER *bh = (BITMAPINFOHEADER *)g_bi; DWORD *masks = (DWORD *)(g_bi + sizeof(BITMAPINFOHEADER));
    unsigned short *single, *comp; double ts[64], tb[64];
    if (argc < 5) { printf("usage: glband <root> <step> <reps> <N> [N ...]\n"); return 2; }
    QueryPerformanceFrequency(&qf);
    g_step = argv[2]; dir = argv[1]; sprintf(sdpath, "%s/snaps/%s", dir, argv[2]); sdir = sdpath; reps = atoi(argv[3]); if (reps > 64) reps = 64;
    W = meta_i(sdir, "w", 800); H = meta_i(sdir, "h", 600); WB = meta_i(sdir, "wb", 1600); BIH = meta_i(sdir, "biHeight", 600);
    g_start = meta_i(sdir, "stateIdx", -1);
    if (BIH <= 0 || WB != W * 2) { printf("only bottom-up 16-bit DIBs are measured\n"); return 2; }
    sprintf(path, "%s/stream.gl.txt", dir); text = slurp(path, &sz);
    sprintf(path, "%s/vtx.bin", sdir); vtx = slurp(path, 0);
    sprintf(path, "%s/nrm.bin", sdir); nrm = slurp(path, 0);
    sprintf(path, "%s/dib_pre.bin", sdir); g_pre = slurp(path, 0);
    sprintf(path, "%s/dib_post.bin", sdir); g_post = slurp(path, 0);
    sprintf(path, "%s/pass.gl.txt", sdir); ptext = slurp(path, &psz);
    if (!text || !vtx || !nrm || !g_pre || !g_post || !ptext || g_start < 0) { printf("missing input\n"); return 2; }
    nlines = split(text, &lines); g_npl = split(ptext, &g_plines);
    memset(g_bi, 0, sizeof g_bi);
    bh->biSize = sizeof(BITMAPINFOHEADER); bh->biWidth = W; bh->biHeight = BIH; bh->biPlanes = 1; bh->biBitCount = 16; bh->biCompression = BI_BITFIELDS;
    masks[0] = meta_i(sdir, "m0", 0x7c00); masks[1] = meta_i(sdir, "m1", 0x3e0); masks[2] = meta_i(sdir, "m2", 0x1f);
    single = (unsigned short *)malloc((size_t)W * H * 2); comp = (unsigned short *)malloc((size_t)W * H * 2);
    for (r = 0; r < reps; r++) {   /* the single context */
        Surf s; double t0;
        if (!make_surf(&s)) { printf("no pixel format\n"); return 2; }
        replay_state(); memcpy(s.bits, g_pre, (size_t)WB * H); GdiFlush();
        t0 = now_us(); replay_pass(); ts[r] = now_us() - t0;
        memcpy(single, s.bits, (size_t)W * H * 2);
        free_surf(&s);
        printf("single us=%.0f diff_post=%lu\n", ts[r], diff(single, (unsigned short *)g_post));
    }
    printf("SINGLE %s median_us=%.0f\n", argv[2], median(ts, reps));
    for (a = 4; a < argc; a++) {
        int n = atoi(argv[a]); unsigned long ds = 0, dp = 0;
        if (n < 1 || n > 64) continue;
        for (r = 0; r < reps; r++) {
            memset(comp, 0xff, (size_t)W * H * 2);
            tb[r] = run_bands(n, comp);
            ds = diff(comp, single); dp = diff(comp, (unsigned short *)g_post);
            printf("bands n=%d us=%.0f diff_single=%lu diff_post=%lu\n", n, tb[r], ds, dp);
            if (ds) bad = 1;
        }
        printf("BANDS %s n=%d median_us=%.0f diff_single=%lu diff_post=%lu\n", argv[2], n, median(tb, reps), ds, dp);
    }
    return bad;
}
