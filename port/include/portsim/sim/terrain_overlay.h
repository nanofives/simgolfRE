// Terrain.dll's 2-D overlay line and the exe's line drawers built on it (roadmap S3.8; notes log/roadmap/S3/8_notes.md).
//
// Terrain::drawLine(x1, y1, x2, y2, colour, width, alpha) (re/analysis/terrain/000048a0_TerrainDll_Terrain_drawLine.md) draws one
// GL_LINES segment through the terrain's GL context, so it lands in the root surface the terrain passes draw into (roadmap S3.2):
// projection glOrtho(0, W, 0, H, -1, 1) when the Terrain object's flip byte (+0x28) is set, else glOrtho(0, W, H, 0, -1, 1), with
// W, H its screen size (+0x20, +0x24); lighting and texturing off, blending (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) on, line width =
// width, colour (((c >> 7) & 0xf8) / 255, ((c >> 2) & 0xf8) / 255, ((c << 3) & 0xf8) / 255, alpha / 10); afterwards lighting and
// texturing on, blending off, the matrices popped, glFlush. Terrain::initGL leaves GL_LINE_SMOOTH on with GL_LINE_SMOOTH_HINT =
// GL_NICEST (every recorded stream after wglCreateContext), so the line is antialiased.
//
// How Windows' generic implementation rasterises that line, measured with re/tools/glexact/glreplay.exe (the real software GL) on
// synthetic drawLine sequences (log/roadmap/S3/8_probe.py; numbers in the notes):
//   - the segment is clipped to the viewport [0, W] x [0, H] in window coordinates, then each end goes through the same fixed-point
//     setup as triangles (U-1076: c + (v - c) * (1 - 2^-8 / h), rounded to 8 fractional bits, c / h the viewport's centre and
//     half-size per axis); a segment of length 0 draws nothing;
//   - the width is clamped to 10 (widths 12, 15, 20, 30 draw exactly what 10 draws); glLineWidth(w <= 0) is GL_INVALID_VALUE and
//     keeps the previous width;
//   - at each pixel centre, with n its distance from the segment's line and t its position along it from the first end, the
//     coverage is clamp(w / 2 + 0.5 - |n|) * clamp(t + 0.5) * clamp(L - t + 0.5) (each factor clamped to [0, 1]);
//   - the fragment's alpha is alpha * coverage, blended against the 5-bit destination expanded by 255 / 31 and written with the
//     4x4 ordered dither of the triangles (gl_raster.cpp quantize).
//   What it does not reproduce: a few pixels at the footprint's edge whose coverage is under 0.13 are not drawn by the real GL
//   (10 of 12,080 covered pixels in the probe; U-1370).
//
// The exe's drawers over it (all drops of the frame loop; with a graphics backend and a LiveTerrain attached they draw into the
// root surface, gfx_terrain.cpp's family lists their bodies): drawMapLine (Terrain::drawLine with the seven arguments),
// drawMinimapSegment (the same seven arguments), setLinePoint / lineTo (the shared line point g_linePoint5a412c; lineTo draws from
// (x, y) to the point, then stores (x, y)), drawCircle (25 segments around a world point through worldToScreen; c3eh.cpp
// DrawCircle_re). The line point lives in the overlay's own state, seeded from the GameState at its first use: nothing here writes
// the GameState or the address space.
#ifndef PORTSIM_SIM_TERRAIN_OVERLAY_H
#define PORTSIM_SIM_TERRAIN_OVERLAY_H

#include <cstdint>
#include <vector>

#include "portsim/registry.h"

namespace portsim {
namespace gl {
class GlRaster;
}
class Gfx;

namespace overlay {

// the GL state drawLine reads and leaves behind
struct LineGl {
  bool context = true;           // a current context exists (Terrain object +0x00 nonzero); without one every call is ignored
  bool flip_y = true;            // Terrain object +0x28
  int32_t screen_w = 800, screen_h = 600;   // Terrain object +0x20 / +0x24 (the glOrtho extent)
  float line_width = 1.0f;       // GL's line width (it outlasts the call)
};

// One Terrain::drawLine into an RGB555 buffer of fb_w x fb_h (row 0 = window y 0, the viewport 0, 0, fb_w, fb_h); returns the
// pixels written. Updates gl.line_width as glLineWidth does.
int64_t draw_line(uint16_t *fb, int32_t fb_w, int32_t fb_h, LineGl &gl, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t colour,
                  int32_t width, int32_t alpha);

// drawLine's lasting GL state changes, given to the terrain rasteriser (lighting and texturing on, blending off with its function
// set, the colour, matrix mode GL_MODELVIEW)
void apply_state(gl::GlRaster &r, uint32_t colour, int32_t alpha);

// drawCircle's segment ends: the screen points of its 25 steps (visible = worldToScreen's result) around world (cx, cy) with
// radius argument r (c3eh.cpp DrawCircle_re); reads the GameState only
struct CirclePoint {
  int32_t sx, sy;
  bool visible;
};
std::vector<CirclePoint> circle_points(Ctx &ctx, int32_t cx, int32_t cy, int32_t r);

// the gfx bodies (registered in gfx_terrain.cpp's family table)
void body_draw_map_line(Gfx &g, Ctx &ctx, const uint32_t *a, int n);
void body_draw_minimap_segment(Gfx &g, Ctx &ctx, const uint32_t *a, int n);
void body_set_line_point(Gfx &g, Ctx &ctx, const uint32_t *a, int n);
void body_line_to(Gfx &g, Ctx &ctx, const uint32_t *a, int n);
void body_draw_circle(Gfx &g, Ctx &ctx, const uint32_t *a, int n);

}  // namespace overlay

// negative controls (sg_negctl_set "overlay.rasterLine"): 1 hard-edged line (no coverage ramp), 2 alpha ignored, 3 no clipping
// (the end ramps at the unclipped ends), 4 the flip byte inverted, 5 no fixed-point snap of the ends, 6 lineTo keeps the old line
// point, 7 drawCircle's segments 3 wide instead of 2, 8 the width not clamped at 10, 9 drawn without a GL context, 10 the gfx bodies draw nothing (the baseline of the frame tests)
extern int g_negctl_terrain_overlay;

}  // namespace portsim

#endif  // PORTSIM_SIM_TERRAIN_OVERLAY_H
