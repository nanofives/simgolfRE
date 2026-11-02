// Body family `terrain` of the graphics backend (roadmap S3.2; port/src/sim/gfx_body.h, listed in gfx_families.inc): the course view
// the original's Terrain.dll draws, rasterised into the backend's root surface. LiveTerrain (terrain_live.h) hands these drops over
// from the TerrainApi calls updateTerrainRender makes; nothing here writes the GameState or the address space.
//
// What the original does (cited in terrain_live.h and the notes named here):
//   - initTerrain gives Terrain::initSystem the DC of the root window's jgld surface (the object at g_rootWindow + 0x278, its slot
//     +0x28; re/analysis/terrain/00449790_initTerrain.md): the generic GL context draws straight into the root surface's pixels.
//     Recorded: the bits of that DC's bitmap are the root surface, rows top-down in memory order like the backend's Surface16
//     (log/roadmap/S3/2_notes.md, the orientation probe), and the rasteriser's row 0 is memory row 0 (port/test/glraster.py).
//   - Window::paint(root window, 0x80000000) (re/analysis/render/004808c0_Window_paint.md): the root window has no parent, so
//     Widget_clearArea calls the surface's slot +0x44 with (NULL, 0x80000000) -> fillRect16 -> clear16: bit 31 set = a direct colour,
//     masked to 16 bits = 0 (re/analysis/render/0000da70_jgld_Surface_clear16.md): the root surface is filled with 0.
//   - Surface_blitThunk(this, dst, x1, y1, x2, y2, w, h) -> Surface_blit3 -> jgld stretchTo: this's rectangle (x1, y1, w, h) onto
//     dst at (x2, y2), 1:1 (re/analysis/render/00475c60_Surface_blitThunk.md). updateTerrainRender copies the root surface whole to
//     the exe's back surface after every pass and the back surface whole to the root surface when the camera has not moved;
//     renderTerrainStrip copies a shifted rectangle of the back surface (re/analysis/terrain/004498a0_updateTerrainRender.md,
//     0044a6e0_renderTerrainStrip.md). The back surface is 16-bit (the restored root surface equals the original's frame in the
//     view region: the copy loses nothing, U-1311).
//
// Drops (a[0..12) the argument slots):
//   terrain.windowPaint                        the root surface filled with 0
//   terrain.pass(kind, self, a0, a1, a2)       one draw pass (terrain_render.h's entry kinds) rasterised over the root surface; kind 5
//                                              (renderTileAtFiveOffsets, lowerEdgeCorner's draw half, roadmap S3.28) takes no pass
//                                              number: the original's pass copies (terrain_calls.js PASSES) count kinds 1..4 only
//   terrain.blit(dir, sx, sy, tx, ty, w, h, u) dir 1 the root surface onto the back surface, else the back surface onto the root
//                                              surface; u = 1: renderTerrainStrip's screen size has uninitialised widths
//   terrain.frameBegin(k) / terrain.frameEnd(k) updateTerrainRender's k-th call (from 0) begins / returns
//   (harness: sg_call terrainLive.keepFrame(k), terrain_live.cpp, has the root surface copied around frame k into
//    `$gfx/terrain_<k>_entry.555`, `_exit`, and per pass j < 8 `_p<j>_pre`, `_p<j>_post`)
//
// Negative controls (g_negctl_gfx_terrain, sg_negctl_set "gfx.family.terrain"; port/test/test_terrain_live.py):
//   1 wrong zoom: setZoomLevel's projection is not given to the rasteriser (terrain_live.cpp)
//   2 wrong theme texture set: loadNewCourseType's texture reload is not given to the rasteriser (terrain_live.cpp)
//   3 pass skipped: terrain.pass draws nothing
//   4 renderTerrainStrip's shifted copy made without its shift (source = destination position)
//   5 Window::paint leaves the root surface as it was
//   6 LiveTerrain's call log kept off (set_keep_log(false) at the first call: the recorder's lines are empty; terrain_live.cpp)
//   7 LiveTerrain's compact log record drops setType's variation (its line has no `var=`; terrain_live.cpp)
//   8 the host-GL backend's DIB has RGB565 masks (terrain_live.cpp; read when the LiveTerrain is made, SG_GL_BACKEND=wgl)
//   9 the host-GL backend's DIB has its rows top-down (biHeight < 0): an equivalent mutant on every pinned frame (the generic
//     implementation writes the same memory rows either way; log/roadmap/S3/15_notes.md)
//  10 the host-GL pass does not load the root surface into the DIB first (the DIB keeps what the previous pass left)
//  11 the host-GL backend binds the port's texture names unmapped (gl_wgl.h gen_textures_mapped)
//  12 the host-GL backend finds no pixel format (as a host without a generic 16-bit one): terrainLive.hostGl returns 0, the
//     setup and the passes stay GlRaster's (the fallback; port/test/test_gl_wgl.py)
//  13 the startup self-check's pinned hash wrong (one hex digit): LiveTerrain::choose_renderer("auto") falls back to GlRaster
//  14 the self-check's reference pass perturbed (every texture coordinate one texel off): "auto" falls back to GlRaster
//     (both roadmap S3.20, terrain_live.cpp choose_renderer; port/test/test_gl_wgl_selfcheck.py)
//  15 lowerEdgeCorner's pass (kind 5) is not handed to the backend (terrain_live.cpp; roadmap S3.28, port/test/test_terrain_s3_28.py)
//
// Host OpenGL (roadmap S3.15, U-1810): with SG_GL_BACKEND=wgl the LiveTerrain also feeds the GL setup to the host's own OpenGL 1.1
// software implementation (gl_wgl.h: Windows' generic opengl32 on the original's 16-bit DIB) and terrain.pass draws through it: the
// terrain picture is then the one the original's own implementation drew, bit for bit on Windows (GlRaster, the portable default,
// leaves a one-level residual on about 0.05 % of the textured pixels, U-1392). A host without it keeps GlRaster (host_gl() null).
// Roadmap S3.20 (decision G13, final): GlRaster stays the default; the app and framegate take the host's OpenGL as an opt-in
// (--gl host, or --gl auto: LiveTerrain::choose_renderer runs the startup self-check of gl_wgl.h wgl_self_check first and keeps
// GlRaster when it fails), with the choice and its reason logged.
//
// Rasteriser workers (roadmap S3.10, log/roadmap/S3/10_notes.md): the passes run the rasteriser's pixel loop on
// GlRaster::set_threads workers, min(4, max(1, hardware threads / 2)) of them, SG_RASTER_THREADS=<n> to choose (1 = none). The
// frame is the same for every count (gl_raster.h). Development switch: SG_TERRAIN_PASS_LOG=<file> appends one line per
// terrain.pass, `<updateTerrainRender call k> <pass j in it> <entry kind> <microseconds in terrain_render_entry> <pixels written>
// <triangles> <pixels of the checked reciprocal path> <workers>` (the host GL's passes, roadmap S3.20: the three counts -1, workers 0).
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS   // std::getenv / std::fopen of the development switch
#endif
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "gfx_body.h"
#include "portsim/arith.h"
#include "portsim/sim/gl_raster.h"
#include "portsim/sim/terrain_api.h"
#include "portsim/sim/terrain_live.h"
#include "portsim/sim/terrain_overlay.h"
#include "portsim/sim/terrain_render.h"

namespace portsim {

int g_negctl_gfx_terrain = 0;

namespace {

LiveTerrain *live(Gfx &g, Ctx &ctx, const char *who) {
  LiveTerrain *lt = has_terrain_api(ctx) ? dynamic_cast<LiveTerrain *>(&terrain_api(ctx)) : nullptr;
  if (!lt) g.unsup(std::string(who) + ": the TerrainApi is not a LiveTerrain");
  return lt;
}

void keep(Gfx &, Ctx &ctx, LiveTerrain &lt, const char *what, const Surface16 &s) {
  const LiveTerrainView &v = lt.view();
  if (!v.in_frame || !v.keep_frames.count(v.frames)) return;
  gfxb::put_file(ctx, "$gfx/terrain_" + std::to_string(v.frames) + "_" + what + ".555", gfxb::surface_bytes(s));
}

// a 1:1 copy of src's rectangle (sx, sy, w, h) to (tx, ty) on dst, clipped to both (StretchBlt with equal sizes)
void copy16(const std::vector<uint16_t> &src, int32_t sw, int32_t sh, std::vector<uint16_t> &dst, int32_t dw, int32_t dh, int32_t sx,
            int32_t sy, int32_t tx, int32_t ty, int32_t w, int32_t h) {
  for (int32_t r = 0; r < h; ++r) {
    const int32_t yy = add32(sy, r), ty2 = add32(ty, r);
    if (yy < 0 || yy >= sh || ty2 < 0 || ty2 >= dh) continue;
    for (int32_t c = 0; c < w; ++c) {
      const int32_t xx = add32(sx, c), tx2 = add32(tx, c);
      if (xx < 0 || xx >= sw || tx2 < 0 || tx2 >= dw) continue;
      dst[static_cast<size_t>(ty2) * static_cast<size_t>(dw) + static_cast<size_t>(tx2)] =
          src[static_cast<size_t>(yy) * static_cast<size_t>(sw) + static_cast<size_t>(xx)];
    }
  }
}

// the rasteriser's pixel-loop workers (GlRaster::set_threads): SG_RASTER_THREADS when set (1 = none), else
// min(4, max(1, hardware threads / 2))
int raster_threads() {
  static const int n = [] {
    if (const char *e = std::getenv("SG_RASTER_THREADS")) return std::atoi(e) > 1 ? std::atoi(e) : 1;
    const int hw = static_cast<int>(std::thread::hardware_concurrency()) / 2;
    return hw < 1 ? 1 : (hw > 4 ? 4 : hw);
  }();
  return n;
}

void b_window_paint(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  g.ensure_root(ctx);
  if (SG_NEGCTL(g_negctl_gfx_terrain) == 5) return;
  std::fill(g.root.px.begin(), g.root.px.end(), static_cast<uint16_t>(0));
  g.root_terrain = 0;
}

void b_pass(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  LiveTerrain *lt = live(g, ctx, "terrain.pass");
  if (!lt) return;
  gl::GlRaster &r = lt->raster();
  if (r.width() != g.root.w || r.height() != g.root.h) {
    g.unsup("terrain.pass: a root surface of another size than the rasteriser's 800 x 600");
    return;
  }
  if (lt->setup_names_differ()) g.unsup("terrain.pass: the rasteriser's texture names differ from the setup's");
  LiveTerrainView &v = lt->view();
  const bool numbered = a[0] != static_cast<uint32_t>(kEntryRenderTileAtFiveOffsets);
  const int32_t j = numbered ? v.passes_in_frame++ : -1;
  const bool keep_pass = numbered && j < 8;
  if (keep_pass) keep(g, ctx, *lt, ("p" + std::to_string(j) + "_pre").c_str(), g.root);
  if (SG_NEGCTL(g_negctl_gfx_terrain) != 3 && lt->host_gl()) {
    // roadmap S3.15 (U-1810): the host's own OpenGL draws the pass into its DIB, which holds the root surface's pixels around it
    gl::WglSink &w = *lt->host_gl();
    TerrainGlobals &tg = lt->globals();
    const auto t0 = std::chrono::steady_clock::now();
    if (SG_NEGCTL(g_negctl_gfx_terrain) != 10) w.set_pixels(g.root.px);
    w.set_arrays(tg.vertices.data(), tg.normals.data());
    if (!terrain_render_entry(ctx, w, tg, static_cast<int32_t>(a[0]), a[1], a[2], a[3], a[4])) {
      g.unsup("terrain.pass: an entry kind the renderer does not have");
      return;
    }
    w.get_pixels(g.root.px);
    // roadmap S3.20: the pass log line of the host GL (the DIB load, the pass and the read back; the implementation reports no
    // pixel or triangle counts: -1, workers 0); v.pixels_drawn is not counted on this path
    if (const char *log = std::getenv("SG_TERRAIN_PASS_LOG")) {
      const long long us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0).count();
      if (FILE *f = std::fopen(log, "a")) {
        std::fprintf(f, "%u %d %u %lld -1 -1 -1 0\n", v.frames, j, a[0], us);
        std::fclose(f);
      }
    }
    ++v.passes;
  } else if (SG_NEGCTL(g_negctl_gfx_terrain) != 3) {
    if (const int nt = raster_threads(); r.threads() != nt) r.set_threads(nt);
    r.pixels() = g.root.px;
    TerrainGlobals &tg = lt->globals();
    r.set_arrays(tg.vertices.data(), tg.vertices.size() / 3, tg.normals.data(), tg.normals.size() / 3);
    const int64_t before = r.stats.drawn_px, tri_before = r.stats.triangles, fast_before = r.stats.fast_px;
    const auto t0 = std::chrono::steady_clock::now();
    if (!terrain_render_entry(ctx, r, tg, static_cast<int32_t>(a[0]), a[1], a[2], a[3], a[4])) {
      g.unsup("terrain.pass: an entry kind the renderer does not have");
      return;
    }
    r.finish();   // the workers' pixels and counters
    if (const char *log = std::getenv("SG_TERRAIN_PASS_LOG")) {
      const long long us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0).count();
      if (FILE *f = std::fopen(log, "a")) {
        std::fprintf(f, "%u %d %u %lld %lld %lld %lld %d\n", v.frames, j, a[0], us, static_cast<long long>(r.stats.drawn_px - before),
                     static_cast<long long>(r.stats.triangles - tri_before), static_cast<long long>(r.stats.fast_px - fast_before),
                     r.threads());
        std::fclose(f);
      }
    }
    v.pixels_drawn += r.stats.drawn_px - before;
    ++v.passes;
    g.root.px = r.pixels();
  }
  g.root_terrain = 1;
  if (keep_pass) keep(g, ctx, *lt, ("p" + std::to_string(j) + "_post").c_str(), g.root);
}

void b_blit(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  LiveTerrain *lt = live(g, ctx, "terrain.blit");
  if (!lt) return;
  LiveTerrainView &v = lt->view();
  if (v.back_w == 0) {   // gameMain's back surface: a new 16-bit surface of the screen's size, zero-filled
    v.back_w = g.root.w;
    v.back_h = g.root.h;
    v.back_surface.assign(static_cast<size_t>(v.back_w) * static_cast<size_t>(v.back_h), 0);
  }
  if (a[7] != 0) g.unsup("terrain.blit: renderTerrainStrip at a screen size whose widths are uninitialised");
  int32_t sx = static_cast<int32_t>(a[1]), sy = static_cast<int32_t>(a[2]);
  const int32_t tx = static_cast<int32_t>(a[3]), ty = static_cast<int32_t>(a[4]), w = static_cast<int32_t>(a[5]),
                h = static_cast<int32_t>(a[6]);
  if (a[0] == 1) {
    copy16(g.root.px, g.root.w, g.root.h, v.back_surface, v.back_w, v.back_h, sx, sy, tx, ty, w, h);
    v.back_terrain = g.root_terrain;
    return;
  }
  if (SG_NEGCTL(g_negctl_gfx_terrain) == 4) {
    sx = tx;
    sy = ty;
  }
  copy16(v.back_surface, v.back_w, v.back_h, g.root.px, g.root.w, g.root.h, sx, sy, tx, ty, w, h);
  g.root_terrain = (g.root_terrain || v.back_terrain) ? 1 : 0;
}

void b_frame_begin(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  g.ensure_root(ctx);
  if (LiveTerrain *lt = live(g, ctx, "terrain.frameBegin")) keep(g, ctx, *lt, "entry", g.root);
}
void b_frame_end(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  g.ensure_root(ctx);
  if (LiveTerrain *lt = live(g, ctx, "terrain.frameEnd")) keep(g, ctx, *lt, "exit", g.root);
}

const GfxBodyDef kBodies[] = {
    {"terrain.windowPaint", b_window_paint}, {"terrain.pass", b_pass},          {"terrain.blit", b_blit},
    {"terrain.frameBegin", b_frame_begin},   {"terrain.frameEnd", b_frame_end},
    // roadmap S3.8: Terrain::drawLine and the exe drawers over it (terrain_overlay.h)
    {"drawMapLine", overlay::body_draw_map_line},       {"drawMinimapSegment", overlay::body_draw_minimap_segment},
    {"setLinePoint", overlay::body_set_line_point},     {"lineTo", overlay::body_line_to},
    {"drawCircle", overlay::body_draw_circle},
};

}  // namespace

extern const GfxFamily kGfxFamily_terrain = {"terrain", kBodies, sizeof(kBodies) / sizeof(kBodies[0]), nullptr, 0};

}  // namespace portsim
