// P14A: the GPU compositor (gpu.h; log/roadmap/P14/P14A_notes.md).
#include "gpu.h"

#include "present.h"
#include "profile.h"
#include "portsim/sim/perf.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_3.h>
#endif

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <mutex>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace portapp {

#ifdef _WIN32
namespace {

template <class T>
void rel(T *&p) {
  if (p) {
    p->Release();
    p = nullptr;
  }
}

// ---- the shaders. Every rule is the CPU code's integer arithmetic on the stored words (present.cpp names in the comments); the
// course and HUD passes work on packed BGRA words as render does, and only the last write converts to the back buffer's UNORM8.
const char kHlslA[] = R"HLSL(
cbuffer CB : register(b0) {
  int cw, ch, vpx, vpy;
  int vpw, vph, slx, sly;
  int base_near, world_near, layer_near, mask_on;
  int wox, wfw, wpw, wph;
  int has_world, gap_on, gux, guy;
  int gin_x0, gin_x1, gin_y0, gin_y1;
  int nlayers, cur_layer, cur_on, box_on;
  int cdx, shx, shy, c2move;
  int lines_on, pieces_on, npieces, cpad0;
  int cb0, cb1, cb2, cb3;
  int shade_n0, shade_n1, toff_x, toff_y;
  int tn_x, tn_y, cpad1, cpad2;
};
cbuffer QCB : register(b1) { int qox, qoy, qcw, qch; };
// P15B: the FPS overlay drawn by the HUD pass: its box (darkened to half) and the slot of its runs in tlay
cbuffer TCB : register(b2) { int ov_on, ov_li, ov_x0, ov_x1, ov_y0, ov_y1, tpad0, tpad1; };
Texture2D<uint> tbase : register(t0);
Texture2D<uint> tworld : register(t1);
Texture2D<uint> twmask : register(t2);
Texture2D<uint> tprev : register(t3);
Texture2D<uint> tcourse : register(t4);
struct Layer { int x0, y0, x1, y1, base, centred, dx0, dim, dim_src, popups, bx0, bx1, by0, by1, p0, p1; };
StructuredBuffer<Layer> layers : register(t5);
StructuredBuffer<uint2> lpx : register(t6);
Texture2D<uint> ttabs : register(t7);
Texture2D<uint2> tcur : register(t8);
StructuredBuffer<uint> shade : register(t9);
StructuredBuffer<int> pdx : register(t10);
Texture2D<uint> tatlas : register(t11);
struct Quad { int dx, dy, w, h, sx, sy, p0, p1; };
StructuredBuffer<Quad> quads : register(t12);
Texture2D<uint> tfull : register(t13);
// per client column / row, computed by the CPU with the CPU code's arithmetic (GpuCompositor::upload_tables): k = index - toff
StructuredBuffer<int> lcol : register(t14);   // compose_layers lpos(k) over the content's columns (1/256 800-space)
StructuredBuffer<int> lrow : register(t15);
StructuredBuffer<int4> bcol : register(t16);  // content column: (floor map, tap i0, tap i1, weight) of render's base scaling
StructuredBuffer<int4> brow : register(t17);
StructuredBuffer<int> wcol : register(t18);   // compose_world pos(k) at the unslid viewport (1/256 world pixels)
StructuredBuffer<int> wrow : register(t19);
// per layer, 1 where an 8 x 8 block of 800-space pixels holds a tap that can reach a drawn pixel (the block of each drawn pixel
// and of its left / upper / upper-left neighbour): a pass whose first tap's block is 0 draws nothing at this client pixel
Texture2DArray<uint> tocc : register(t20);
// P15B: the HUD text runs as tables (textres.h TextTable): per layer the range of its runs and their union box, per run its client
// box and the base of its pixels' entry indices, the indices (0 none, k + 1 entry k), the entries (32 words: 2 per uint)
struct TLay { int first, n, x0, y0, x1, y1, pad0, pad1; };
struct TRun { int x0, y0, x1, y1, ibase, pad0, pad1, pad2; };
StructuredBuffer<TLay> tlay : register(t21);
StructuredBuffer<TRun> truns : register(t22);
StructuredBuffer<uint> tidx : register(t23);
StructuredBuffer<uint> tlut : register(t24);

// present.h rgb555_to_bgra
uint bgra(uint v) {
  uint r = (v >> 10) & 31, g = (v >> 5) & 31, b = v & 31;
  return 0xff000000u | (((r << 3) | (r >> 2)) << 16) | (((g << 3) | (g >> 2)) << 8) | ((b << 3) | (b >> 2));
}
// compose_layers to555
uint to555(uint c) { return (((c >> 19) & 31) << 10) | (((c >> 11) & 31) << 5) | ((c >> 3) & 31); }
// the 8-bit mix of render / compose_world / compose_layers
uint mixw(uint a, uint b, uint f) {
  uint g = 256 - f;
  uint rb = (((a & 0x00ff00ffu) * g + (b & 0x00ff00ffu) * f) >> 8) & 0x00ff00ffu;
  uint ag = ((((a >> 8) & 0x00ff00ffu) * g + ((b >> 8) & 0x00ff00ffu) * f) >> 8) & 0x00ff00ffu;
  return rb | (ag << 8);
}
// C++ truncating division and the floor division of the CPU code, b > 0, with unsigned divides only
int tdiv(int a, int b) { return a >= 0 ? (int)((uint)a / (uint)b) : -(int)((uint)(-a) / (uint)b); }
int fdiv(int a, int b) { return a >= 0 ? (int)((uint)a / (uint)b) : -(int)(((uint)(-a) + (uint)b - 1u) / (uint)b); }
// floor(A * 128 / n) without the 32-bit product overflowing
int fdiv128(int A, int n) {
  int q = fdiv(A, n);
  int r = A - q * n;
  return q * 128 + (int)(((uint)r * 128u) / (uint)n);
}
int floor256(int v) { return v >= 0 ? (int)((uint)v / 256u) : -(int)(((uint)(-v) + 255u) / 256u); }
// gfx.cpp gfx_layer_quarter
uint quarter(uint p, uint d) {
  uint P = p << 4, D = d << 4;
  uint e = ((P & 0x7c1f0u) + (D & 0x7c1f0u)) >> 1, f = ((P & 0x3e00u) + (D & 0x3e00u)) >> 1;
  e = (e + (D & 0x7c1f0u)) >> 1;
  f = (f + (D & 0x3e00u)) >> 1;
  return (((e & 0x7c1f0u) + (f & 0x3e00u)) >> 4) & 0xffffu;
}
float4 unpack(uint d) {
  return float4(((d >> 16) & 255) / 255.0, ((d >> 8) & 255) / 255.0, (d & 255) / 255.0, 1.0);
}

float4 VSFull(uint id : SV_VertexID) : SV_Position {
  float2 p = float2((id << 1) & 2, id & 2);
  return float4(p * float2(2, -2) + float2(-1, 1), 0, 1);
}

// ---- render's base scaling (taps) and compose_world (pos)
void taps(int k, int n_out, int n_in, out int i0, out int i1, out uint f) {
  int p = (int)(((uint)(2 * k + 1) * (uint)n_in * 128u) / (uint)n_out) - 128;
  if (p < 0) p = 0;
  int last = (n_in - 1) * 256;
  if (p > last) p = last;
  i0 = p >> 8;
  f = (uint)(p & 255);
  i1 = i0 + 1 < n_in ? i0 + 1 : i0;
}
int wpos(int k, int origin, int ncl, int nw, int worg, bool nearw) {
  int d = k - origin;
  if (nearw) return (fdiv(d * nw, ncl) + worg) * 256;
  return fdiv128((2 * d + 1) * nw, ncl) - 128 + worg * 256;
}
uint basepx(int x, int y) { return bgra(tbase.Load(int3(x, y, 0))); }
uint worldpx(int x, int y) { return bgra(tworld.Load(int3(x, y, 0)) & 0x7fffu); }

uint PSCourse(float4 pos : SV_Position) : SV_Target {
  int X = (int)pos.x, Y = (int)pos.y;
  uint o = 0xff000000u;
  int vox = vpx + slx, voy = vpy + sly;  // the slid viewport vs
  int bx = X - vox, by = Y - voy;
  bool in_content = bx >= 0 && bx < vpw && by >= 0 && by < vph;
  int4 tcx = int4(0, 0, 0, 0), tcy = int4(0, 0, 0, 0);
  if (in_content) {
    tcx = bcol[bx];
    tcy = brow[by];
  }
  bool world_drew = false;
  if (has_world != 0) {  // compose_world at vs
    bool nw = world_near != 0;
    int kx = X - slx + toff_x, ky = Y - sly + toff_y;  // pos(X, vox) = pos(X - slx, vpx): the unslid table
    int sx = (kx >= 0 && kx < tn_x) ? wcol[kx] : wpos(X, vox, vpw, wfw, wox, nw);
    int sy = (ky >= 0 && ky < tn_y) ? wrow[ky] : wpos(Y, voy, vph, wph, 0, nw);
    int x0 = sx >> 8, y0 = sy >> 8;
    if (!(y0 < -2 || y0 >= wph + 2) && !(x0 < -2 || x0 >= wpw + 2)) {
      bool frame_px = false;  // the frame's own pixel (mask 0)
      if (in_content && mask_on != 0) frame_px = twmask.Load(int3(tcx.x, tcy.x, 0)) == 0;
      if (!frame_px) {
        int c0 = clamp(x0, 0, wpw - 1), c1 = clamp(x0 + 1, 0, wpw - 1), r0 = clamp(y0, 0, wph - 1), r1 = clamp(y0 + 1, 0, wph - 1);
        world_drew = true;
        if (nw) {
          o = worldpx(c0, r0);
        } else {
          uint fx = (uint)(sx & 255), fy = (uint)(sy & 255);
          uint a = mixw(worldpx(c0, r0), worldpx(c1, r0), fx), b = mixw(worldpx(c0, r1), worldpx(c1, r1), fx);
          o = mixw(a, b, fy) | 0xff000000u;
        }
      }
    }
  }
  if (in_content && !world_drew) {  // render: the base over the content rectangle (drawn before the world, which covers it)
    if (base_near != 0) {
      o = basepx(tcx.x, tcy.x);
    } else {
      uint a = mixw(basepx(tcx.y, tcy.y), basepx(tcx.z, tcy.y), (uint)tcx.w), b = mixw(basepx(tcx.y, tcy.z), basepx(tcx.z, tcy.z), (uint)tcx.w);
      o = mixw(a, b, (uint)tcy.w) | 0xff000000u;
    }
  }
  if (gap_on != 0) {  // fill_gap: outside the slid world rectangle, the last painted course picture moved with the slide
    bool inside = Y >= gin_y0 && Y < gin_y1 && gin_x1 > gin_x0 && X >= gin_x0 && X < gin_x1;
    if (!inside) o = tprev.Load(int3(clamp(X - gux, 0, cw - 1), clamp(Y - guy, 0, ch - 1), 0));
  }
  return o;
}
)HLSL";

const char kHlslB[] = R"HLSL(
// ---- compose_layers
static const uint kOwnerMask = 0xffu, kC2 = 0x100u, kCurM = 0x200u, kLineM = 0x400u;
int moved_piece(uint t) {  // the moved piece of a pixel of a split layer, -1 none
  uint o = t & kOwnerMask;
  int k = (o != 0 && (int)o <= npieces) ? (int)o - 1 : -1;
  return (k >= 0 && pdx[k] != 0) ? k : -1;
}
bool tapl(Layer L, int x, int y, int pi, int cpass, bool split, bool cmove, bool wdim, out uint2 v) {
  v = uint2(0, 0);
  if (x < L.x0 || x >= L.x1 || y < L.y0 || y >= L.y1) return false;
  v = lpx[L.base + (y - L.y0) * (L.x1 - L.x0) + (x - L.x0)];
  uint kind = v.y & 255u;
  if (kind == 0) return false;
  uint t = (x >= 0 && y >= 0 && x < 800 && y < 600) ? ttabs.Load(int3(x, y, 0)) : 0u;
  if (split && moved_piece(t) != (pi == cpass ? -1 : pi)) return false;
  if (L.centred != 0 && cur_on != 0 && (t & kCurM) != 0) return false;                                       // the previews
  if (L.centred != 0 && lines_on != 0 && (t & kLineM) != 0 && kind == 1 && (v.x & 0xffffu) == (t >> 16)) return false;  // a tee line
  if (cmove && ((c2move != 0 && (t & kC2) != 0) != (pi == cpass))) return false;                    // the 2D previews' own pass
  if (wdim && kind == 4) return false;
  return true;
}
uint chn(uint dv, uint sv, uint k) { return ((((dv * k) >> 8) + sv) & 0xffffu) >> 3; }
uint over(uint2 v, uint d) {  // one layer pixel over client word d
  uint kind = v.y & 255u, k = (v.y >> 8) & 255u, px = v.x & 0xffffu, src = v.x >> 16;
  if (kind == 1) return bgra(px);
  uint w = to555(d);
  if (kind == 2) {  // jgld blitPair1
    uint rr = (chn((w >> 7) & 0xf8u, (src >> 7) & 0xf8u, k) << 10) & 0xffffu;
    uint gg = (chn((w >> 2) & 0xf8u, (src >> 2) & 0xf8u, k) << 5) & 0xffffu;
    uint bb = chn((w << 3) & 0xf8u, (src << 3) & 0xf8u, k);
    return bgra((rr | gg | bb) & 0x7fffu);
  }
  if (kind == 4) return bgra(quarter(src, w) & 0x7fffu);
  uint n = k == 0 ? (uint)shade_n0 : k == 1 ? (uint)shade_n1 : 0u;
  uint off = k == 0 ? 0u : (uint)shade_n0;
  if (n == 0 || w >= n) return bgra(px);
  return bgra(shade[off + w] & 0x7fffu);
}
// P15B: compose_layers' draw_runs for layer li at client pixel (X, Y) over word d: TextRaster::draw writes bgra(v) where GDI's word
// v differs from to555(d); v per channel from the run's entry at that channel's level of to555(d). Runs in draw order.
uint lutw(uint e, uint L) { uint w = tlut[e * 16u + (L >> 1)]; return (L & 1u) != 0 ? (w >> 16) : (w & 0xffffu); }
uint text_runs(int li, int X, int Y, uint d) {
  TLay T = tlay[li];
  if (T.n == 0 || X < T.x0 || X >= T.x1 || Y < T.y0 || Y >= T.y1) return d;
  [loop] for (int i = T.first; i < T.first + T.n; ++i) {
    TRun R = truns[i];
    if (X < R.x0 || X >= R.x1 || Y < R.y0 || Y >= R.y1) continue;
    uint e = tidx[R.ibase + (Y - R.y0) * (R.x1 - R.x0) + (X - R.x0)];
    if (e == 0) continue;
    uint w = to555(d);
    uint v = (lutw(e - 1u, (w >> 10) & 31u) & 0x7c00u) | (lutw(e - 1u, (w >> 5) & 31u) & 0x03e0u) | (lutw(e - 1u, w & 31u) & 0x001fu);
    if (v != w) d = bgra(v);
  }
  return d;
}
int lpos(int k, int origin, int ncl, int nlog, bool nearl) {
  int d = k - origin;
  if (nearl) return fdiv(d * nlog, ncl) * 256;
  return fdiv128((2 * d + 1) * nlog, ncl) - 128;
}
uint draw_cursor(int X, int Y, uint d) {  // compose_layers draw_cursor: the previews from world pixels, nearest
  if (cur_on == 0 || vpw <= 0 || vph <= 0 || wfw <= 0 || wph <= 0) return d;
  int cox = vpx + cdx + shx + slx, coy = vpy + shy + sly;
  int cx0 = max(0, cox + tdiv((cb0 - wox) * vpw, wfw) - 1), cx1 = min(cw, cox + tdiv((cb2 - wox) * vpw, wfw) + 2);
  int cy0 = max(0, coy + tdiv(cb1 * vph, wph) - 1), cy1 = min(ch, coy + tdiv(cb3 * vph, wph) + 2);
  if (X < cx0 || X >= cx1 || Y < cy0 || Y >= cy1) return d;
  int wy = fdiv((Y - coy) * wph, vph);
  if (wy < 0 || wy >= wph) return d;
  int wx = wox + fdiv((X - cox) * wfw, vpw);
  if (wx < 0 || wx >= wpw) return d;
  uint2 c = tcur.Load(int3(wx, wy, 0));
  if (c.y == 1) return bgra(c.x & 0xffffu);
  if (c.y == 2 && box_on != 0) {  // P10F: lo + (hi - lo) * d / 255
    uint lo = bgra(c.x & 0xffffu), hi = bgra(c.x >> 16), o = 0xff000000u;
    [unroll] for (int sh = 0; sh < 24; sh += 8) {
      int l = (int)((lo >> sh) & 255u), h = (int)((hi >> sh) & 255u);
      int v = l + tdiv((h - l) * (int)((d >> sh) & 255u) + 127, 255);
      o |= (uint)clamp(v, 0, 255) << sh;
    }
    return o;
  }
  return d;
}
uint hud(int X, int Y) {
  uint d = tcourse.Load(int3(X, Y, 0));
  bool nearl = layer_near != 0;
  [loop] for (int li = 0; li < nlayers; ++li) {
    if (li == cur_layer) d = draw_cursor(X, Y, d);
    Layer L = layers[li];
    bool wdim = L.dim != 0;
    if (wdim) d = bgra(quarter((uint)L.dim_src, to555(d)) & 0x7fffu) | 0xff000000u;
    if (L.x1 <= L.x0 || L.y1 <= L.y0) {
      d = text_runs(li, X, Y, d);  // P15B: the layer's runs (draw_runs also runs for a layer with no box)
      continue;
    }
    bool split = pieces_on != 0 && L.popups != 0;
    int npass = split ? npieces : 0;
    bool cmove = c2move != 0 && L.centred != 0;
    int cpass = cmove ? npass : -2;
    int pend = npass + (cmove ? 1 : 0);
    [loop] for (int pi = -1; pi < pend; ++pi) {
      if (pi >= 0 && pi != cpass && pdx[pi] == 0) continue;
      int dx = pi == cpass ? cdx + shx + slx : (pi >= 0 ? pdx[pi] : L.dx0);
      int dyp = pi == cpass ? shy + sly : 0;
      int cx0 = max(0, L.bx0 + dx), cx1 = min(cw, L.bx1 + dx), cy0 = max(0, L.by0 + dyp), cy1 = min(ch, L.by1 + dyp);
      if (X < cx0 || X >= cx1 || Y < cy0 || Y >= cy1) continue;
      int kx = X - dx + toff_x, ky = Y - dyp + toff_y;
      int sx = (kx >= 0 && kx < tn_x) ? lcol[kx] : lpos(X - dx, vpx, vpw, 800, nearl);
      int sy = (ky >= 0 && ky < tn_y) ? lrow[ky] : lpos(Y - dyp, vpy, vph, 600, nearl);
      int lx0 = sx >> 8, ly0 = sy >> 8;  // floor256
      if (tocc.Load(int4(clamp(lx0, 0, 799) >> 3, clamp(ly0, 0, 599) >> 3, li, 0)) == 0) continue;
      uint2 v00, v10, v01, v11;
      if (nearl) {
        if (tapl(L, lx0, ly0, pi, cpass, split, cmove, wdim, v00)) d = over(v00, d) | 0xff000000u;
        continue;
      }
      uint fx = (uint)(sx & 255), fy = (uint)(sy & 255);
      bool p00 = tapl(L, lx0, ly0, pi, cpass, split, cmove, wdim, v00), p10 = tapl(L, lx0 + 1, ly0, pi, cpass, split, cmove, wdim, v10);
      bool p01 = tapl(L, lx0, ly0 + 1, pi, cpass, split, cmove, wdim, v01), p11 = tapl(L, lx0 + 1, ly0 + 1, pi, cpass, split, cmove, wdim, v11);
      if (!p00 && !p10 && !p01 && !p11) continue;
      uint c00 = p00 ? over(v00, d) : d, c10 = p10 ? over(v10, d) : d, c01 = p01 ? over(v01, d) : d, c11 = p11 ? over(v11, d) : d;
      d = mixw(mixw(c00, c10, fx), mixw(c01, c11, fx), fy) | 0xff000000u;
    }
    d = text_runs(li, X, Y, d);  // P15B: the layer's runs once its pixels are composed (compose_layers' RunsAfter)
  }
  if (cur_layer == nlayers) d = draw_cursor(X, Y, d);
  if (ov_on != 0 && X >= ov_x0 && X < ov_x1 && Y >= ov_y0 && Y < ov_y1) {  // P15B: draw_overlay_rect over the whole picture
    d = ((d >> 1) & 0x007f7f7fu) | 0xff000000u;
    d = text_runs(ov_li, X, Y, d);
  }
  return d;
}
float4 PSHud(float4 pos : SV_Position) : SV_Target { return unpack(hud((int)pos.x, (int)pos.y)); }
float4 PSFull(float4 pos : SV_Position) : SV_Target { return unpack(tfull.Load(int3((int)pos.x, (int)pos.y, 0))); }

// ---- patches: atlas rectangles drawn at (dx + qox, dy + qoy)
struct QV {
  float4 pos : SV_Position;
  nointerpolation int4 q : QUAD;
};
QV VSQuad(uint vid : SV_VertexID, uint iid : SV_InstanceID) {
  Quad Q = quads[iid];
  int cx = (vid == 1 || vid == 2 || vid == 4) ? 1 : 0, cy = (vid == 2 || vid == 4 || vid == 5) ? 1 : 0;
  float px = (float)(Q.dx + qox + cx * Q.w), py = (float)(Q.dy + qoy + cy * Q.h);
  QV o;
  o.pos = float4(px / qcw * 2.0 - 1.0, 1.0 - py / qch * 2.0, 0, 1);
  o.q = int4(Q.dx + qox, Q.dy + qoy, Q.sx, Q.sy);
  return o;
}
uint PSQuadU(QV i) : SV_Target { return tatlas.Load(int3((int)i.pos.x - i.q.x + i.q.z, (int)i.pos.y - i.q.y + i.q.w, 0)); }
float4 PSQuadF(QV i) : SV_Target {
  return unpack(tatlas.Load(int3((int)i.pos.x - i.q.x + i.q.z, (int)i.pos.y - i.q.y + i.q.w, 0)));
}
)HLSL";

// the constant buffers (the HLSL's order; 16-byte rows)
struct GpuCB {
  int32_t cw, ch, vpx, vpy;
  int32_t vpw, vph, slx, sly;
  int32_t base_near, world_near, layer_near, mask_on;
  int32_t wox, wfw, wpw, wph;
  int32_t has_world, gap_on, gux, guy;
  int32_t gin_x0, gin_x1, gin_y0, gin_y1;
  int32_t nlayers, cur_layer, cur_on, box_on;
  int32_t cdx, shx, shy, c2move;
  int32_t lines_on, pieces_on, npieces, cpad0;
  int32_t cb0, cb1, cb2, cb3;
  int32_t shade_n0, shade_n1, toff_x, toff_y;
  int32_t tn_x, tn_y, cpad1, cpad2;
};
static_assert(sizeof(GpuCB) % 16 == 0, "cbuffer rows");
struct QuadCB {
  int32_t ox, oy, cw, ch;
};
struct LayerRec {
  int32_t x0, y0, x1, y1, base, centred, dx0, dim, dim_src, popups, bx0, bx1, by0, by1, p0, p1;
};
struct TextCB {  // P15B (HLSL TCB)
  int32_t ov_on, ov_li, ov_x0, ov_x1, ov_y0, ov_y1, pad0, pad1;
};
struct QuadRec {
  int32_t dx, dy, w, h, sx, sy, p0, p1;
};
struct Rect {
  int32_t x0, y0, x1, y1;  // half-open
};

// rectangles that overlap or lie within `gap` pixels of each other become their union
void merge_rects(std::vector<Rect> &r, int32_t gap) {
  for (bool again = true; again;) {
    again = false;
    for (size_t i = 0; i < r.size() && !again; ++i)
      for (size_t j = i + 1; j < r.size(); ++j) {
        const Rect &a = r[i], &b = r[j];
        if (a.x0 - gap < b.x1 && b.x0 - gap < a.x1 && a.y0 - gap < b.y1 && b.y0 - gap < a.y1) {
          r[i] = Rect{std::min(a.x0, b.x0), std::min(a.y0, b.y0), std::max(a.x1, b.x1), std::max(a.y1, b.y1)};
          r.erase(r.begin() + static_cast<std::ptrdiff_t>(j));
          again = true;
          break;
        }
      }
  }
}

bool write_bmp(const std::string &path, const uint32_t *px, int32_t w, int32_t h) {
  std::FILE *f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  const uint32_t bytes = static_cast<uint32_t>(w) * static_cast<uint32_t>(h) * 4u;
  BITMAPFILEHEADER fh = {};
  BITMAPINFOHEADER ih = {};
  fh.bfType = 0x4d42;
  fh.bfOffBits = sizeof(fh) + sizeof(ih);
  fh.bfSize = fh.bfOffBits + bytes;
  ih.biSize = sizeof(ih);
  ih.biWidth = w;
  ih.biHeight = -h;  // top-down
  ih.biPlanes = 1;
  ih.biBitCount = 32;
  ih.biCompression = BI_RGB;
  std::fwrite(&fh, sizeof(fh), 1, f);
  std::fwrite(&ih, sizeof(ih), 1, f);
  std::fwrite(px, 1, bytes, f);
  std::fclose(f);
  return true;
}

// a texture (and its SRV) kept between frames: recreated when the size or format changes
struct Tex {
  ID3D11Texture2D *t = nullptr;
  ID3D11ShaderResourceView *srv = nullptr;
  ID3D11RenderTargetView *rtv = nullptr;
  int32_t w = 0, h = 0;
  DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
  void reset() {
    rel(rtv);
    rel(srv);
    rel(t);
    w = h = 0;
  }
};
struct SBuf {
  ID3D11Buffer *b = nullptr;
  ID3D11ShaderResourceView *srv = nullptr;
  uint32_t cap = 0, stride = 0;
  void reset() {
    rel(srv);
    rel(b);
    cap = 0;
  }
};

}  // namespace

struct GpuCompositor::Impl {
  HWND hwnd = nullptr;
  bool ok = false;
  std::string why, adapter;
  ID3D11Device *dev = nullptr;
  ID3D11DeviceContext *ctx = nullptr;
  IDXGISwapChain2 *sc = nullptr;
  HANDLE wait = nullptr;
  ID3D11RenderTargetView *bb_rtv = nullptr;
  ID3D11VertexShader *vs_full = nullptr, *vs_quad = nullptr;
  ID3D11PixelShader *ps_course = nullptr, *ps_hud = nullptr, *ps_full = nullptr, *ps_quad_u = nullptr, *ps_quad_f = nullptr;
  ID3D11Buffer *cb = nullptr, *qcb = nullptr;
  ID3D11RasterizerState *rs = nullptr;
  int32_t w = 0, h = 0;  // the swap chain's size
  // the frame's data
  Tex base, world, wmask, tabs, cur, full, dummy_u, dummy_u2;
  SBuf layers, lpx, shade, pdx, quads_c, quads_f;
  SBuf lcol, lrow, bcol, brow, wcol, wrow;  // upload_tables
  std::string tables_key;
  Tex course, prev;       // the course picture targets (CPU: scene_last_, scene_prev_)
  ID3D11Texture2D *occ = nullptr;  // the layers' block occupancy (Texture2DArray R8_UINT 100 x 75 x layers)
  ID3D11ShaderResourceView *occ_srv = nullptr;
  int32_t occ_n = 0;
  Tex atlas_c, atlas_f;   // the patches
  ID3D11Texture2D *stage_bb = nullptr, *stage_u = nullptr;  // readbacks (the A/B check, shots)
  bool course_valid = false, prev_valid = false;
  uint64_t swap_gen = 0;
  // what is uploaded
  uint64_t frame_gen = ~0ull, world_gen = ~0ull;
  const void *frame_f = nullptr;
  int32_t frame_w = 0, frame_h = 0;
  bool frame_text = false;
  uint32_t shade_n0 = 0, shade_n1 = 0;
  int32_t nlayers = 0;
  bool cur_on = false, box_on = false;
  uint64_t remap_gen = ~0ull;
  int32_t remap_w = 0, remap_h = 0;
  // patches
  std::vector<QuadRec> qc, qf;
  uint64_t course_key = ~0ull;
  std::vector<uint32_t> atlas_buf, tmp;
  std::string final_key;
  // the final patches (build_final_patches)
  struct FRect {
    Rect r;
    bool dep = true;
    int32_t ax = 0, ay = 0;
  };
  std::vector<FRect> fstat;
  std::string fstat_key, fdyn_key;
  bool fov_ok = false;
  int32_t fstat_h = 0, atlas_fw = 0;
  std::vector<uint32_t> fbuf;
  uint64_t final_static_rects = 0, final_dep_rects = 0, final_rect_renders = 0, final_overflow = 0;
  bool classify_ok = true;   // upload_frame: no blitPair1 channel overflow, the shade tables monotone
  bool c2_any = false;       // upload_frame: the frame's cursor2d plane has a pixel (a 2D tool preview)
  uint64_t classify_off = 0;
  int shade_mono = -1;       // -1 not checked, 0 / 1
  int32_t gin[4] = {0, 0, 0, 0};  // fill_gap's world rectangle at this paint's slide (x0, x1, y0, y1)
  uint64_t prev_fetch_gen = ~0ull;
  uint64_t prev_fetches = 0;
  // fix 2: what changed under the final patches since their last build
  std::vector<std::array<int32_t, 4>> wdirty;  // world rectangles uploaded (upload_world)
  bool wdirty_all = false;                    // the whole world image was uploaded
  std::vector<Rect> cchanged;                 // client rectangles: the mover labels' course patches before and after a rebuild
  int32_t f_slx = 0, f_sly = 0, f_cdx = 0, f_shx = 0, f_shy = 0;  // the offsets of the last build
  int32_t c2_box[4] = {0, 0, 0, 0};           // the frame's cursor2d pixels' 800-space box (upload_frame; empty when none)
  uint64_t final_dep_renders = 0, final_dep_kept = 0;
  // fix 3: the flat path's base upload
  uint64_t flat_gen = ~0ull;
  const void *flat_src = nullptr;
  uint64_t paints_flat = 0;
  // fix 3: off by default (P14A_notes.md section 5: on the title menu its band patches, full-height strips merged with the runs'
  // rectangles, cost more CPU than the whole-frame render it replaces: 73 against 51 ms per paint at 3840 x 2108); SG_P14_FLAT=1 on
  bool no_flat = true;
  // the profile's GPU time: timestamp query sets in a ring, read one paint late
  struct Ts {
    ID3D11Query *dis = nullptr, *a = nullptr, *b = nullptr;
    bool pending = false;
  } ts[4];
  int ts_i = 0;
  // stats (session.json p14)
  uint64_t paints_layered = 0, paints_full = 0, course_builds = 0, final_builds = 0, course_px = 0, final_px = 0;
  uint64_t frame_uploads = 0, world_uploads = 0, losses = 0, restarts = 0, present_fail = 0;
  uint64_t verified = 0, verify_bad = 0, verify_bad_px = 0, verify_max_px = 0;
  int verify_every = 0;
  uint64_t verify_n = 0;
  std::string verify_dir, verify_first;
  int verify_saved = 0;
  std::string shot_path;
  uint64_t shots = 0;
  double gpu_ms_sum = 0;
  uint64_t gpu_ms_n = 0;
  bool ts_on = false;  // this paint's timestamps were begun (ts_begin)
  int latency = 2;     // SetMaximumFrameLatency
  // the paint does not block on the latency object (measured, P14A_notes.md: the vblank-paced tick came before DWM released the
  // last frame, 3.4 ms of waiting per paint at 1080); Present(1, 0) keeps vsync and the 2-frame queue gives the backpressure.
  // SG_P14_WAIT=1 waits (the measurement's other arm)
  bool no_wait = true;
  UINT sync_interval = 1;  // SG_P14_SYNC=0: Present without vsync (measurement)
  int64_t ns_wait = 0, ns_present = 0;
  // CPU time per stage (ns, session.json p14)
  int64_t ns_frame = 0, ns_world = 0, ns_course = 0, ns_final = 0, ns_draw = 0;
  IDXGIOutput *out = nullptr;  // the swap chain's monitor (wait_vblank)
  std::mutex out_mu;
  uint64_t world_rect_uploads = 0;
  // P15B: the HUD text runs as tables (build_text_tables). SG_APP_P15=off: P14A's final patches for them
  bool p15 = true;
  std::unique_ptr<TextRaster> traster;
  struct TCache {
    TextTable t;
    uint64_t used = 0;
  };
  std::map<std::string, TCache> tcache;  // key: everything TextRaster::table reads (run_key)
  uint64_t tbuild = 0;                   // builds of the set (the cache's clock)
  std::string tset_key;                  // the set's key: what decides each run's place and table
  std::vector<uint8_t> tabled;           // per run of the text frame: drawn by its table (no final patch)
  bool tbound = false;                   // the buffers hold at least one run
  std::vector<std::vector<const TextTable *>> tper;  // per HUD layer, its runs' tables with ink (into tcache)
  bool tdirty = false;                   // tper or the overlay's tables changed since the upload
  // fix 2: the FPS overlay (build_overlay_tables): its runs' tables, its box, its slot in tlay
  std::unique_ptr<TextRaster> oraster;
  std::string ov_key;
  bool ov_on = false;
  int32_t ov_box[4] = {0, 0, 0, 0};
  int32_t ov_li = 0;
  std::vector<TextTable> ov_tables;
  ID3D11Buffer *tcb = nullptr;
  uint64_t ov_builds = 0, ov_patch_paints = 0;
  SBuf tlay, truns, tidx, tlut;
  uint64_t text_sets = 0, text_tables_made = 0, text_tables_reused = 0, text_runs_tabled = 0, text_entries = 0;
  int64_t ns_text = 0;
};

namespace {

bool make_tex(ID3D11Device *dev, Tex &t, int32_t w, int32_t h, DXGI_FORMAT fmt, bool rt) {
  if (t.t && t.w == w && t.h == h && t.fmt == fmt && (!rt || t.rtv)) return true;
  t.reset();
  D3D11_TEXTURE2D_DESC d = {};
  d.Width = static_cast<UINT>(std::max(1, w));
  d.Height = static_cast<UINT>(std::max(1, h));
  d.MipLevels = 1;
  d.ArraySize = 1;
  d.Format = fmt;
  d.SampleDesc.Count = 1;
  d.Usage = D3D11_USAGE_DEFAULT;
  d.BindFlags = D3D11_BIND_SHADER_RESOURCE | (rt ? D3D11_BIND_RENDER_TARGET : 0);
  if (FAILED(dev->CreateTexture2D(&d, nullptr, &t.t))) return false;
  if (FAILED(dev->CreateShaderResourceView(t.t, nullptr, &t.srv))) return false;
  if (rt && FAILED(dev->CreateRenderTargetView(t.t, nullptr, &t.rtv))) return false;
  t.w = w;
  t.h = h;
  t.fmt = fmt;
  return true;
}

// rows of `rows` (row pitch in bytes) into t's region (x, y, w, h)
void put_tex(ID3D11DeviceContext *ctx, Tex &t, int32_t x, int32_t y, int32_t w, int32_t h, const void *rows, uint32_t pitch) {
  if (!t.t || w <= 0 || h <= 0) return;
  D3D11_BOX b = {static_cast<UINT>(x), static_cast<UINT>(y), 0, static_cast<UINT>(x + w), static_cast<UINT>(y + h), 1};
  ctx->UpdateSubresource(t.t, 0, &b, rows, pitch, 0);
}

bool make_sbuf(ID3D11Device *dev, ID3D11DeviceContext *ctx, SBuf &s, uint32_t stride, uint32_t count, const void *data) {
  const uint32_t n = std::max<uint32_t>(1, count);
  if (!s.b || s.cap < n || s.stride != stride) {
    s.reset();
    D3D11_BUFFER_DESC d = {};
    d.ByteWidth = stride * std::max<uint32_t>(n, 16);
    d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    d.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    d.StructureByteStride = stride;
    if (FAILED(dev->CreateBuffer(&d, nullptr, &s.b))) return false;
    if (FAILED(dev->CreateShaderResourceView(s.b, nullptr, &s.srv))) return false;
    s.cap = std::max<uint32_t>(n, 16);
    s.stride = stride;
  }
  if (data && count) {
    D3D11_BOX b = {0, 0, 0, stride * count, 1, 1};
    ctx->UpdateSubresource(s.b, 0, &b, data, 0, 0);
  }
  return true;
}

}  // namespace
#else
struct GpuCompositor::Impl {
  bool ok = false;
};
#endif

GpuCompositor::GpuCompositor() : d_(new Impl()) {}
GpuCompositor::~GpuCompositor() { stop(); }
bool GpuCompositor::ok() const { return d_ && d_->ok; }

#ifdef _WIN32
void GpuCompositor::stop() {
  if (!d_) return;
  Impl &d = *d_;
  if (d.ctx) d.ctx->ClearState();
  for (Impl::Ts &t : d.ts) {
    rel(t.dis);
    rel(t.a);
    rel(t.b);
  }
  for (Tex *t : {&d.base, &d.world, &d.wmask, &d.tabs, &d.cur, &d.full, &d.dummy_u, &d.dummy_u2, &d.course, &d.prev, &d.atlas_c,
                 &d.atlas_f})
    t->reset();
  for (SBuf *s : {&d.layers, &d.lpx, &d.shade, &d.pdx, &d.quads_c, &d.quads_f, &d.lcol, &d.lrow, &d.bcol, &d.brow, &d.wcol, &d.wrow})
    s->reset();
  d.tables_key.clear();
  for (SBuf *s : {&d.tlay, &d.truns, &d.tidx, &d.tlut}) s->reset();  // P15B (the table cache is CPU data: kept)
  d.tset_key.clear();
  d.tbound = false;
  rel(d.stage_bb);
  rel(d.stage_u);
  rel(d.occ_srv);
  rel(d.occ);
  d.occ_n = 0;
  rel(d.bb_rtv);
  rel(d.vs_full);
  rel(d.vs_quad);
  rel(d.ps_course);
  rel(d.ps_hud);
  rel(d.ps_full);
  rel(d.ps_quad_u);
  rel(d.ps_quad_f);
  rel(d.cb);
  rel(d.qcb);
  rel(d.tcb);  // P15B
  d.ov_key.clear();
  d.ov_on = false;
  rel(d.rs);
  if (d.wait) CloseHandle(d.wait);
  d.wait = nullptr;
  {
    std::lock_guard<std::mutex> lk(d.out_mu);
    rel(d.out);
  }
  rel(d.sc);
  rel(d.ctx);
  rel(d.dev);
  d.ok = false;
  d.w = d.h = 0;
  d.frame_gen = d.world_gen = d.course_key = d.remap_gen = ~0ull;
  d.final_key.clear();
  d.fstat_key.clear();
  d.fdyn_key.clear();
  d.fov_ok = false;
  d.prev_fetch_gen = ~0ull;
  d.course_valid = d.prev_valid = false;
  d.flat_gen = ~0ull;
}

bool GpuCompositor::start(void *hwnd, std::string &why) {
  Impl &d = *d_;
  stop();
  d.hwnd = static_cast<HWND>(hwnd);
  if (const char *e = std::getenv("SG_P14_VERIFY")) d.verify_every = std::max(0, std::atoi(e));
  if (const char *e = std::getenv("SG_P14_VERIFY_DIR")) d.verify_dir = e;
  auto fail = [&](const std::string &w) {
    why = w;
    d.why = w;
    stop();
    return false;
  };
  // the shader compiler: d3dcompiler_47.dll (an OS component on Windows 10 / 11)
  HMODULE dc = LoadLibraryW(L"d3dcompiler_47.dll");
  if (!dc) return fail("no d3dcompiler_47.dll");
  auto compile = reinterpret_cast<pD3DCompile>(reinterpret_cast<void *>(GetProcAddress(dc, "D3DCompile")));
  if (!compile) return fail("no D3DCompile");
  const D3D_FEATURE_LEVEL want[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
  D3D_FEATURE_LEVEL got = D3D_FEATURE_LEVEL_10_0;
  HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, want, 2,
                                 D3D11_SDK_VERSION, &d.dev, &got, &d.ctx);
  if (FAILED(hr)) hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, want + 1, 1,
                                         D3D11_SDK_VERSION, &d.dev, &got, &d.ctx);
  if (FAILED(hr) || !d.dev) return fail("no D3D11 hardware device at feature level 11");
  IDXGIDevice *xd = nullptr;
  IDXGIAdapter *ad = nullptr;
  IDXGIFactory2 *fac = nullptr;
  if (FAILED(d.dev->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void **>(&xd))) || FAILED(xd->GetAdapter(&ad)) ||
      FAILED(ad->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void **>(&fac)))) {
    rel(fac);
    rel(ad);
    rel(xd);
    return fail("no IDXGIFactory2");
  }
  DXGI_ADAPTER_DESC ade = {};
  if (SUCCEEDED(ad->GetDesc(&ade))) {
    char b[256];
    WideCharToMultiByte(CP_UTF8, 0, ade.Description, -1, b, sizeof b, nullptr, nullptr);
    d.adapter = b;
  }
  RECT rc;
  GetClientRect(d.hwnd, &rc);
  DXGI_SWAP_CHAIN_DESC1 sd = {};
  sd.Width = static_cast<UINT>(std::max<LONG>(1, rc.right - rc.left));
  sd.Height = static_cast<UINT>(std::max<LONG>(1, rc.bottom - rc.top));
  sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  sd.SampleDesc.Count = 1;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  // the queue: SG_P14_LATENCY=<1..3> frames (default 2: the paint rarely waits for DWM to release the last one), one buffer more
  int lat = 2;
  if (const char *e = std::getenv("SG_P14_LATENCY")) lat = std::min(3, std::max(1, std::atoi(e)));
  d.latency = lat;
  if (const char *e = std::getenv("SG_P14_WAIT")) d.no_wait = std::atoi(e) == 0;
  if (const char *e = std::getenv("SG_P14_FLAT")) d.no_flat = std::atoi(e) == 0;
  if (const char *e = std::getenv("SG_APP_P15")) d.p15 = std::strcmp(e, "off") != 0;  // P15B
  if (const char *e = std::getenv("SG_P14_SYNC")) d.sync_interval = static_cast<UINT>(std::max(0, std::min(4, std::atoi(e))));
  sd.BufferCount = static_cast<UINT>(lat + 1);
  sd.Scaling = DXGI_SCALING_NONE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  sd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
  sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
  IDXGISwapChain1 *sc1 = nullptr;
  hr = fac->CreateSwapChainForHwnd(d.dev, d.hwnd, &sd, nullptr, nullptr, &sc1);
  if (SUCCEEDED(hr)) fac->MakeWindowAssociation(d.hwnd, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);
  rel(fac);
  rel(ad);
  rel(xd);
  if (FAILED(hr) || !sc1) return fail("CreateSwapChainForHwnd failed");
  hr = sc1->QueryInterface(__uuidof(IDXGISwapChain2), reinterpret_cast<void **>(&d.sc));
  rel(sc1);
  if (FAILED(hr) || !d.sc) return fail("no IDXGISwapChain2");
  d.sc->SetMaximumFrameLatency(static_cast<UINT>(lat));
  d.wait = d.sc->GetFrameLatencyWaitableObject();
  {
    std::lock_guard<std::mutex> lk(d.out_mu);
    if (FAILED(d.sc->GetContainingOutput(&d.out))) d.out = nullptr;
  }
  d.w = static_cast<int32_t>(sd.Width);
  d.h = static_cast<int32_t>(sd.Height);
  ID3D11Texture2D *bb = nullptr;
  if (FAILED(d.sc->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&bb))) ||
      FAILED(d.dev->CreateRenderTargetView(bb, nullptr, &d.bb_rtv))) {
    rel(bb);
    return fail("no back buffer view");
  }
  rel(bb);
  // shaders
  const std::string src = std::string(kHlslA) + kHlslB;
  auto blob = [&](const char *entry, const char *target, ID3DBlob *&out) -> bool {
    ID3DBlob *err = nullptr;
    out = nullptr;
    const HRESULT h = compile(src.data(), src.size(), "p14a.hlsl", nullptr, nullptr, entry, target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
                              &out, &err);
    if (FAILED(h)) {
      std::string m = std::string("shader ") + entry + " failed";
      if (err) m += std::string(": ") + static_cast<const char *>(err->GetBufferPointer());
      d.why = m;
      rel(err);
      return false;
    }
    rel(err);
    return true;
  };
  struct PsDef {
    const char *entry;
    ID3D11PixelShader **ps;
  } pss[] = {{"PSCourse", &d.ps_course}, {"PSHud", &d.ps_hud}, {"PSFull", &d.ps_full}, {"PSQuadU", &d.ps_quad_u},
             {"PSQuadF", &d.ps_quad_f}};
  for (const PsDef &p : pss) {
    ID3DBlob *b = nullptr;
    if (!blob(p.entry, "ps_5_0", b)) return fail(d.why);
    hr = d.dev->CreatePixelShader(b->GetBufferPointer(), b->GetBufferSize(), nullptr, p.ps);
    rel(b);
    if (FAILED(hr)) return fail(std::string("CreatePixelShader ") + p.entry);
  }
  for (auto vs : {std::make_pair("VSFull", &d.vs_full), std::make_pair("VSQuad", &d.vs_quad)}) {
    ID3DBlob *b = nullptr;
    if (!blob(vs.first, "vs_5_0", b)) return fail(d.why);
    hr = d.dev->CreateVertexShader(b->GetBufferPointer(), b->GetBufferSize(), nullptr, vs.second);
    rel(b);
    if (FAILED(hr)) return fail(std::string("CreateVertexShader ") + vs.first);
  }
  D3D11_BUFFER_DESC bd = {};
  bd.Usage = D3D11_USAGE_DEFAULT;
  bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  bd.ByteWidth = sizeof(GpuCB);
  if (FAILED(d.dev->CreateBuffer(&bd, nullptr, &d.cb))) return fail("constant buffer");
  bd.ByteWidth = sizeof(QuadCB);
  if (FAILED(d.dev->CreateBuffer(&bd, nullptr, &d.qcb))) return fail("constant buffer");
  bd.ByteWidth = sizeof(TextCB);  // P15B
  if (FAILED(d.dev->CreateBuffer(&bd, nullptr, &d.tcb))) return fail("constant buffer");
  D3D11_RASTERIZER_DESC rd = {};
  rd.FillMode = D3D11_FILL_SOLID;
  rd.CullMode = D3D11_CULL_NONE;
  rd.DepthClipEnable = TRUE;
  if (FAILED(d.dev->CreateRasterizerState(&rd, &d.rs))) return fail("rasterizer state");
  // 1 x 1 stand-ins for planes a frame does not have
  if (!make_tex(d.dev, d.dummy_u, 1, 1, DXGI_FORMAT_R32_UINT, false) || !make_tex(d.dev, d.dummy_u2, 1, 1, DXGI_FORMAT_R32G32_UINT, false))
    return fail("stand-in textures");
  for (Impl::Ts &t : d.ts) {
    D3D11_QUERY_DESC q = {D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
    d.dev->CreateQuery(&q, &t.dis);
    q.Query = D3D11_QUERY_TIMESTAMP;
    d.dev->CreateQuery(&q, &t.a);
    d.dev->CreateQuery(&q, &t.b);
  }
  // the first frame waits on the latency object too (DXGI: wait before the first Present)
  if (d.wait) WaitForSingleObjectEx(d.wait, 100, FALSE);
  d.ok = true;
  d.why = "D3D11 FL " + std::to_string((static_cast<int>(got) >> 12) & 15) + "." + std::to_string((static_cast<int>(got) >> 8) & 15) +
          ", " + d.adapter;
  why = d.why;
  prof_set_gpu(true);
  return true;
}

void GpuCompositor::lose(const char *where, long hr) {
  Impl &d = *d_;
  ++d.losses;
  char b[160];
  std::snprintf(b, sizeof b, "GPU lost at %s (hr 0x%08lx)", where, static_cast<unsigned long>(hr));
  std::fprintf(stderr, "%s\n", b);
  const HWND h = d.hwnd;
  const uint64_t losses = d.losses;
  stop();
  d.losses = losses;
  if (d.losses <= 3) {  // the device again; the presenter keeps every frame's data, so the next paint uploads it
    std::string why;
    if (start(h, why)) {
      ++d.restarts;
      return;
    }
  }
  d.why = std::string(b) + "; CPU path from here";
  prof_set_gpu(false);
}

bool GpuCompositor::ensure_size(int32_t cw, int32_t ch) {
  Impl &d = *d_;
  if (d.w != cw || d.h != ch) {
    d.ctx->OMSetRenderTargets(0, nullptr, nullptr);
    d.ctx->ClearState();
    rel(d.bb_rtv);
    rel(d.stage_bb);
    rel(d.stage_u);
    HRESULT hr = d.sc->ResizeBuffers(0, static_cast<UINT>(cw), static_cast<UINT>(ch), DXGI_FORMAT_UNKNOWN,
                                     DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
    if (FAILED(hr)) {
      lose("ResizeBuffers", hr);
      return false;
    }
    ID3D11Texture2D *bb = nullptr;
    hr = d.sc->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&bb));
    if (SUCCEEDED(hr)) hr = d.dev->CreateRenderTargetView(bb, nullptr, &d.bb_rtv);
    rel(bb);
    if (FAILED(hr)) {
      lose("GetBuffer", hr);
      return false;
    }
    d.w = cw;
    d.h = ch;
    d.course_valid = d.prev_valid = false;
    d.course_key = ~0ull;
    d.final_key.clear();
    d.fstat_key.clear();
    d.fdyn_key.clear();
    d.prev_fetch_gen = ~0ull;
  }
  if (!make_tex(d.dev, d.course, cw, ch, DXGI_FORMAT_R32_UINT, true) || !make_tex(d.dev, d.prev, cw, ch, DXGI_FORMAT_R32_UINT, true)) {
    lose("course targets", 0);
    return false;
  }
  return true;
}

// ---- uploads
void GpuCompositor::upload_world(const Presenter &P, bool text) {
  Impl &d = *d_;
  const WorldImage &W = P.world_;
  const Viewport vp = fit_viewport(d.w, d.h);
  const bool wtext = text && W.mask_text.size() == W.mask.size() && W.px_text.size() == W.px.size();
  (void)vp;
  const std::vector<uint16_t> &wpx = wtext ? W.px_text : W.px;
  const int64_t t0 = portsim::perf_now_ns();
  const bool same = d.world.t && d.world.w == W.pw && d.world.h == W.ph && d.world_gen != ~0ull;
  if (same && !P.world_dirty_.empty() && P.world_dirty_.size() < 256) {  // only the rectangles objects_tick rewrote
    for (const std::array<int32_t, 4> &r : P.world_dirty_) {
      const int32_t x0 = std::max(0, r[0]), y0 = std::max(0, r[1]), x1 = std::min(W.pw, r[2]), y1 = std::min(W.ph, r[3]);
      if (x1 <= x0 || y1 <= y0) continue;
      put_tex(d.ctx, d.world, x0, y0, x1 - x0, y1 - y0,
              wpx.data() + static_cast<size_t>(y0) * static_cast<size_t>(W.pw) + static_cast<size_t>(x0), static_cast<uint32_t>(W.pw) * 2u);
    }
    ++d.world_rect_uploads;
    d.wdirty.insert(d.wdirty.end(), P.world_dirty_.begin(), P.world_dirty_.end());
  } else {
    d.wdirty_all = true;
    if (!make_tex(d.dev, d.world, W.pw, W.ph, DXGI_FORMAT_R16_UINT, false)) return;
    put_tex(d.ctx, d.world, 0, 0, W.pw, W.ph, wpx.data(), static_cast<uint32_t>(W.pw) * 2u);
    ++d.world_uploads;
  }
  P.world_dirty_.clear();
  d.world_gen = P.world_gen_;
  d.ns_world += portsim::perf_now_ns() - t0;
}

void GpuCompositor::upload_frame(const Presenter &P, const Viewport &vp, bool text) {
  Impl &d = *d_;
  const portsim::GfxHudFrame &f = P.hud_for(text);
  const WorldImage &W = P.world_;
  constexpr int32_t LW = kLogicalW, LH = kLogicalH;
  ++d.frame_uploads;
  const int64_t t0 = portsim::perf_now_ns();
  // base, world, mask (compose_world's wmask: mask_text with the runs drawn)
  make_tex(d.dev, d.base, LW, LH, DXGI_FORMAT_R16_UINT, false);
  put_tex(d.ctx, d.base, 0, 0, LW, LH, f.base.data(), LW * 2u);
  const bool wtext = text && W.mask_text.size() == W.mask.size() && W.px_text.size() == W.px.size();
  const std::vector<uint8_t> &wmask = wtext ? W.mask_text : W.mask;
  make_tex(d.dev, d.wmask, LW, LH, DXGI_FORMAT_R8_UINT, false);
  put_tex(d.ctx, d.wmask, 0, 0, LW, LH, wmask.data(), LW);
  d.world_gen = ~0ull;  // the whole world image
  upload_world(P, text);
  // the layers: records and pixels (px | src << 16, kind | k << 8)
  const bool anchor = P.hud_anchor_ && vp.origin_x > 0 && P.defect_ != PresentDefect::HudUnmoved;
  const int32_t dxl = -vp.origin_x, dxr = d.w - vp.origin_x - vp.client_w;
  std::vector<LayerRec> recs;
  std::vector<uint32_t> px;
  // the probes' premise (build_final_patches): no blitPair1 channel sum above 255 (a carry into the next channel), the shade tables
  // per channel non-decreasing in every input channel
  if (d.shade_mono < 0) {
    d.shade_mono = 1;
    for (uint8_t id = 0; id < 2; ++id) {
      size_t n = 0;
      const uint16_t *t = portsim::gfx_shade_table(id, n);
      for (size_t w = 0; t && w < n; ++w)
        for (int sh : {0, 5, 10}) {
          if (((w >> sh) & 31u) == 31u || w + (static_cast<size_t>(1) << sh) >= n) continue;
          const uint16_t a = t[w], b = t[w + (static_cast<size_t>(1) << sh)];
          for (int oc : {0, 5, 10})
            if (((b >> oc) & 31u) < ((a >> oc) & 31u)) d.shade_mono = 0;
        }
    }
  }
  bool wrap = false, any_shade = false;
  for (const portsim::GfxHudLayer &l : f.layers)
    for (const portsim::GfxLayerPx &p : l.px) {
      if (p.kind == 3) any_shade = true;
      if (p.kind != 2) continue;
      const uint32_t mx = std::max({(static_cast<uint32_t>(p.src) >> 7) & 0xf8u, (static_cast<uint32_t>(p.src) >> 2) & 0xf8u,
                                    (static_cast<uint32_t>(p.src) << 3) & 0xf8u});
      if (((0xf8u * p.k) >> 8) + mx > 255u) wrap = true;
    }
  d.classify_ok = !wrap && (d.shade_mono == 1 || !any_shade);
  for (const portsim::GfxHudLayer &l : f.layers) {
    LayerRec r = {};
    r.x0 = l.x0;
    r.y0 = l.y0;
    r.x1 = l.x1;
    r.y1 = l.y1;
    r.base = static_cast<int32_t>(px.size() / 2);
    r.centred = l.id == portsim::kGfxLayerCentred ? 1 : 0;
    r.dx0 = anchor ? P.layer_dx(l, vp, dxl, dxr) : 0;
    r.dim = l.dim != 0 && P.defect_ != PresentDefect::DimLocal ? 1 : 0;
    r.dim_src = l.dim_src;
    r.popups = l.popups;
    // compose_layers' pass box before its offset (the CPU's truncating divisions)
    r.bx0 = vp.origin_x + static_cast<int32_t>(static_cast<int64_t>(l.x0 - 1) * vp.client_w / kLogicalW) - 1;
    r.bx1 = vp.origin_x + static_cast<int32_t>(static_cast<int64_t>(l.x1 + 1) * vp.client_w / kLogicalW) + 1;
    r.by0 = vp.origin_y + static_cast<int32_t>(static_cast<int64_t>(l.y0 - 1) * vp.client_h / kLogicalH) - 1;
    r.by1 = vp.origin_y + static_cast<int32_t>(static_cast<int64_t>(l.y1 + 1) * vp.client_h / kLogicalH) + 1;
    px.reserve(px.size() + l.px.size() * 2);
    for (const portsim::GfxLayerPx &p : l.px) {
      px.push_back(static_cast<uint32_t>(p.px) | (static_cast<uint32_t>(p.src) << 16));
      px.push_back(static_cast<uint32_t>(p.kind) | (static_cast<uint32_t>(p.k) << 8));
    }
    recs.push_back(r);
  }
  d.nlayers = static_cast<int32_t>(recs.size());
  {  // the block occupancy (a block a tap can reach a drawn pixel from); a pixel at the 800-space edge's blocks are clamped
    constexpr int32_t BW = kLogicalW / 8, BH = kLogicalH / 8;
    const int32_t n = std::max<int32_t>(1, d.nlayers);
    std::vector<uint8_t> occ(static_cast<size_t>(BW) * BH * static_cast<size_t>(n), 0);
    for (size_t li = 0; li < f.layers.size(); ++li) {
      const portsim::GfxHudLayer &l = f.layers[li];
      uint8_t *o = occ.data() + li * static_cast<size_t>(BW) * BH;
      const int32_t bw = l.x1 - l.x0;
      for (int32_t y = l.y0; y < l.y1; ++y)
        for (int32_t x = l.x0; x < l.x1; ++x) {
          if (!l.px[static_cast<size_t>(y - l.y0) * static_cast<size_t>(bw) + static_cast<size_t>(x - l.x0)].kind) continue;
          for (int32_t yy = y - 1; yy <= y; ++yy)
            for (int32_t xx = x - 1; xx <= x; ++xx)
              o[static_cast<size_t>(std::min(std::max(yy, 0), kLogicalH - 1) >> 3) * BW +
                static_cast<size_t>(std::min(std::max(xx, 0), kLogicalW - 1) >> 3)] = 1;
        }
    }
    if (d.occ_n != n) {
      rel(d.occ_srv);
      rel(d.occ);
      D3D11_TEXTURE2D_DESC td = {};
      td.Width = BW;
      td.Height = BH;
      td.MipLevels = 1;
      td.ArraySize = static_cast<UINT>(n);
      td.Format = DXGI_FORMAT_R8_UINT;
      td.SampleDesc.Count = 1;
      td.Usage = D3D11_USAGE_DEFAULT;
      td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
      if (SUCCEEDED(d.dev->CreateTexture2D(&td, nullptr, &d.occ)) && SUCCEEDED(d.dev->CreateShaderResourceView(d.occ, nullptr, &d.occ_srv)))
        d.occ_n = n;
    }
    for (int32_t li = 0; d.occ && li < n; ++li)
      d.ctx->UpdateSubresource(d.occ, static_cast<UINT>(li), nullptr, occ.data() + static_cast<size_t>(li) * BW * BH, BW, 0);
  }
  make_sbuf(d.dev, d.ctx, d.layers, sizeof(LayerRec), static_cast<uint32_t>(recs.size()), recs.data());
  make_sbuf(d.dev, d.ctx, d.lpx, 8, static_cast<uint32_t>(px.size() / 2), px.data());
  // the per-pixel tables: owner piece, cursor2d, cur_mask, line800, line_word
  std::vector<uint32_t> tabs(static_cast<size_t>(LW) * LH, 0);
  const bool own = f.owner.size() == tabs.size(), c2 = f.cursor2d.size() == tabs.size(), cm = W.cur_mask.size() == tabs.size();
  d.c2_any = false;
  d.c2_box[0] = d.c2_box[1] = LW;
  d.c2_box[2] = d.c2_box[3] = 0;
  if (c2)
    for (int32_t y = 0; y < LH; ++y)
      for (int32_t x = 0; x < LW; ++x)
        if (f.cursor2d[static_cast<size_t>(y) * LW + static_cast<size_t>(x)]) {
          d.c2_any = true;
          d.c2_box[0] = std::min(d.c2_box[0], x);
          d.c2_box[1] = std::min(d.c2_box[1], y);
          d.c2_box[2] = std::max(d.c2_box[2], x + 1);
          d.c2_box[3] = std::max(d.c2_box[3], y + 1);
        }
  const bool lm = f.line800.size() == tabs.size() && f.line_word.size() == tabs.size();
  for (size_t i = 0; i < tabs.size(); ++i) {
    uint32_t t = own ? f.owner[i] : 0u;
    if (c2 && f.cursor2d[i]) t |= 0x100u;
    if (cm && W.cur_mask[i]) t |= 0x200u;
    if (lm && f.line800[i]) t |= 0x400u;
    if (lm) t |= static_cast<uint32_t>(f.line_word[i]) << 16;
    tabs[i] = t;
  }
  make_tex(d.dev, d.tabs, LW, LH, DXGI_FORMAT_R32_UINT, false);
  put_tex(d.ctx, d.tabs, 0, 0, LW, LH, tabs.data(), LW * 4u);
  // the previews' world planes
  d.cur_on = W.cur_mask.size() == tabs.size() && W.cur_px.size() == W.px.size() && W.cur_cov.size() == W.px.size() &&
             W.cur_box[2] > W.cur_box[0];
  d.box_on = W.cur_hi.size() == W.px.size();
  if (d.cur_on) {
    std::vector<uint32_t> cp(W.px.size() * 2);
    for (size_t i = 0; i < W.px.size(); ++i) {
      cp[i * 2] = static_cast<uint32_t>(W.cur_px[i]) | (d.box_on ? static_cast<uint32_t>(W.cur_hi[i]) << 16 : 0u);
      cp[i * 2 + 1] = W.cur_cov[i];
    }
    make_tex(d.dev, d.cur, W.pw, W.ph, DXGI_FORMAT_R32G32_UINT, false);
    put_tex(d.ctx, d.cur, 0, 0, W.pw, W.ph, cp.data(), static_cast<uint32_t>(W.pw) * 8u);
  }
  // the shade tables (gfx_shade_table 0 and 1), once
  if (!d.shade.b) {
    size_t n0 = 0, n1 = 0;
    const uint16_t *tab0 = portsim::gfx_shade_table(0, n0), *tab1 = portsim::gfx_shade_table(1, n1);
    if (!tab0) n0 = 0;
    if (!tab1) n1 = 0;
    std::vector<uint32_t> sh;
    for (size_t i = 0; i < n0; ++i) sh.push_back(tab0[i]);
    for (size_t i = 0; i < n1; ++i) sh.push_back(tab1[i]);
    make_sbuf(d.dev, d.ctx, d.shade, 4, static_cast<uint32_t>(sh.size()), sh.data());
    d.shade_n0 = static_cast<uint32_t>(n0);
    d.shade_n1 = static_cast<uint32_t>(n1);
  }
  d.ns_frame += portsim::perf_now_ns() - t0;
  d.frame_gen = P.content_gen_;
  d.frame_f = &f;
  d.frame_w = d.w;
  d.frame_h = d.h;
  d.frame_text = text;
}

// the per-column / per-row tables of the shaders, from the CPU code's own formulas (present.cpp: render's taps and floor map,
// compose_world's pos, compose_layers' lpos), for client columns -2 cw .. 3 cw and rows -2 ch .. 3 ch (an offset pass reads
// column X - dx)
void GpuCompositor::upload_tables(const Viewport &vp, bool near_, bool wnear, int32_t wox, int32_t wfw, int32_t wpw, int32_t wph) {
  Impl &d = *d_;
  const int32_t cw = d.w, ch = d.h;
  char key[160];
  std::snprintf(key, sizeof key, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", cw, ch, vp.origin_x, vp.origin_y, vp.client_w, vp.client_h,
                near_ ? 1 : 0, wnear ? 1 : 0, wox, wfw, wpw, wph);
  if (d.tables_key == key) return;
  d.tables_key = key;
  auto floordiv = [](int64_t a, int64_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); };
  auto lpos = [&](int32_t k, int32_t origin, int32_t ncl, int32_t nlog) -> int32_t {  // compose_layers lpos
    const int64_t dd = static_cast<int64_t>(k) - origin;
    if (near_) return static_cast<int32_t>(floordiv(dd * nlog, ncl) * 256);
    return static_cast<int32_t>(floordiv((2 * dd + 1) * nlog * 128, ncl) - 128);
  };
  auto wpos = [&](int32_t k, int32_t origin, int32_t ncl, int32_t nw, int32_t worg) -> int32_t {  // compose_world pos
    const int64_t dd = static_cast<int64_t>(k) - origin;
    if (wnear) return static_cast<int32_t>((floordiv(dd * nw, ncl) + worg) * 256);
    return static_cast<int32_t>(floordiv((2 * dd + 1) * nw * 128, ncl) - 128 + static_cast<int64_t>(worg) * 256);
  };
  auto taps = [](int32_t k, int32_t n_out, int32_t n_in, int32_t lx) {  // render's Tap and floor map
    int64_t p = ((2 * static_cast<int64_t>(k) + 1) * n_in * 128) / n_out - 128;
    if (p < 0) p = 0;
    const int64_t last = static_cast<int64_t>(n_in - 1) * 256;
    if (p > last) p = last;
    const int32_t i0 = static_cast<int32_t>(p >> 8);
    return std::array<int32_t, 4>{lx, i0, i0 + 1 < n_in ? i0 + 1 : i0, static_cast<int32_t>(p & 255)};
  };
  std::vector<int32_t> lc(static_cast<size_t>(5 * cw)), lr(static_cast<size_t>(5 * ch)), wc(lc.size()), wr(lr.size());
  for (int32_t i = 0; i < 5 * cw; ++i) {
    lc[static_cast<size_t>(i)] = vp.client_w > 0 ? lpos(i - 2 * cw, vp.origin_x, vp.client_w, kLogicalW) : 0;
    wc[static_cast<size_t>(i)] = vp.client_w > 0 && wfw > 0 ? wpos(i - 2 * cw, vp.origin_x, vp.client_w, wfw, wox) : 0;
  }
  for (int32_t i = 0; i < 5 * ch; ++i) {
    lr[static_cast<size_t>(i)] = vp.client_h > 0 ? lpos(i - 2 * ch, vp.origin_y, vp.client_h, kLogicalH) : 0;
    wr[static_cast<size_t>(i)] = vp.client_h > 0 && wph > 0 ? wpos(i - 2 * ch, vp.origin_y, vp.client_h, wph, 0) : 0;
  }
  std::vector<std::array<int32_t, 4>> bc(static_cast<size_t>(std::max(1, vp.client_w))), br(static_cast<size_t>(std::max(1, vp.client_h)));
  for (int32_t x = 0; x < vp.client_w; ++x)
    bc[static_cast<size_t>(x)] = taps(x, vp.client_w, kLogicalW, static_cast<int32_t>(static_cast<int64_t>(x) * kLogicalW / vp.client_w));
  for (int32_t y = 0; y < vp.client_h; ++y)
    br[static_cast<size_t>(y)] = taps(y, vp.client_h, kLogicalH, static_cast<int32_t>(static_cast<int64_t>(y) * kLogicalH / vp.client_h));
  make_sbuf(d.dev, d.ctx, d.lcol, 4, static_cast<uint32_t>(lc.size()), lc.data());
  make_sbuf(d.dev, d.ctx, d.lrow, 4, static_cast<uint32_t>(lr.size()), lr.data());
  make_sbuf(d.dev, d.ctx, d.wcol, 4, static_cast<uint32_t>(wc.size()), wc.data());
  make_sbuf(d.dev, d.ctx, d.wrow, 4, static_cast<uint32_t>(wr.size()), wr.data());
  make_sbuf(d.dev, d.ctx, d.bcol, 16, static_cast<uint32_t>(bc.size()), bc.data());
  make_sbuf(d.dev, d.ctx, d.brow, 16, static_cast<uint32_t>(br.size()), br.data());
}

// ---- patches
namespace {
// a text run's client box at offset dx / (mx, my) (TextRaster draws only inside first_client(x0) .. first_client(x1); one pixel
// more on each side)
Rect run_box(const portsim::GfxTextRun &r, const Viewport &vp, int32_t dx, int32_t my) {
  auto lo = [](int32_t v, int32_t n, int32_t d) {
    const int64_t p = static_cast<int64_t>(v) * n;
    return static_cast<int32_t>(p >= 0 ? p / d : -((-p + d - 1) / d));
  };
  return Rect{vp.origin_x + dx + lo(r.x0, vp.client_w, kLogicalW) - 1, vp.origin_y + my + lo(r.y0, vp.client_h, kLogicalH) - 1,
              vp.origin_x + dx + lo(r.x1, vp.client_w, kLogicalW) + 2, vp.origin_y + my + lo(r.y1, vp.client_h, kLogicalH) + 2};
}
}  // namespace

// packs rendered rectangles into an atlas texture and its quad list
static bool pack_patches(ID3D11Device *dev, ID3D11DeviceContext *ctx, const Presenter &P, int32_t cw, int32_t ch,
                         const std::vector<Rect> &rects, bool course, Tex &atlas, std::vector<uint32_t> &buf,
                         std::vector<uint32_t> &tmp, std::vector<QuadRec> &quads, uint64_t &px_count, const int32_t *overlay) {
  quads.clear();
  if (rects.empty()) return true;
  int32_t aw = std::max<int32_t>(256, cw);
  for (const Rect &r : rects) aw = std::max(aw, r.x1 - r.x0);
  aw = std::min<int32_t>(aw, 16384);
  // shelves
  int32_t x = 0, y = 0, shelf = 0;
  std::vector<std::pair<int32_t, int32_t>> at;
  for (const Rect &r : rects) {
    const int32_t w = r.x1 - r.x0, h = r.y1 - r.y0;
    if (x + w > aw) {
      x = 0;
      y += shelf;
      shelf = 0;
    }
    at.emplace_back(x, y);
    x += w;
    shelf = std::max(shelf, h);
  }
  const int32_t ah = y + shelf;
  if (ah <= 0 || ah > 16384) return false;
  buf.assign(static_cast<size_t>(aw) * static_cast<size_t>(ah), 0);
  for (size_t i = 0; i < rects.size(); ++i) {
    const Rect &r = rects[i];
    const int32_t w = r.x1 - r.x0, h = r.y1 - r.y0;
    P.render_rect(cw, ch, r.x0, r.y0, w, h, course, tmp);
    if (overlay && r.x0 < overlay[2] && overlay[0] < r.x1 && r.y0 < overlay[3] && overlay[1] < r.y1)
      P.draw_overlay_rect(tmp, cw, ch, r.x0, r.y0, w, h);
    if (tmp.size() != static_cast<size_t>(w) * static_cast<size_t>(h)) continue;
    for (int32_t yy = 0; yy < h; ++yy)
      std::memcpy(buf.data() + static_cast<size_t>(at[i].second + yy) * static_cast<size_t>(aw) + static_cast<size_t>(at[i].first),
                  tmp.data() + static_cast<size_t>(yy) * static_cast<size_t>(w), static_cast<size_t>(w) * 4u);
    quads.push_back(QuadRec{r.x0, r.y0, w, h, at[i].first, at[i].second, 0, 0});
    px_count += static_cast<uint64_t>(w) * static_cast<uint64_t>(h);
  }
  const int32_t tw = std::max<int32_t>(atlas.w, aw), th = std::max<int32_t>(atlas.h, ah);
  if (!make_tex(dev, atlas, tw, th, DXGI_FORMAT_R32_UINT, false)) return false;
  const ProfScope prof(Phase::Upload);
  put_tex(ctx, atlas, 0, 0, aw, ah, buf.data(), static_cast<uint32_t>(aw) * 4u);
  return true;
}

void GpuCompositor::build_course_patches(const Presenter &P, const Viewport &vp, bool text) {
  Impl &d = *d_;
  std::vector<Rect> rects;
  if (text) {
    // the course picture's rectangle at offset 0 (fill_gap's rx0 .. rx1 before the window clamp, the content rows)
    const WorldImage &w = P.world_;
    auto fdiv = [](int64_t a, int64_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); };
    int32_t rx0 = vp.origin_x, rx1 = vp.origin_x + vp.client_w;
    if (w.fw > 0) {
      rx0 = std::min<int32_t>(rx0, vp.origin_x + static_cast<int32_t>(fdiv(static_cast<int64_t>(-w.ox) * vp.client_w, w.fw)));
      rx1 = std::max<int32_t>(rx1, vp.origin_x - static_cast<int32_t>(fdiv(-static_cast<int64_t>(w.pw - w.ox) * vp.client_w, w.fw)));
    }
    const int32_t ry0 = vp.origin_y, ry1 = vp.origin_y + vp.client_h;
    for (const portsim::GfxTextRun &r : P.text_.runs) {
      if (r.layer != -1 && r.layer != -3) continue;
      if (r.target == 2 && P.world_.empty()) continue;
      int32_t mx = 0, my = 0;
      if (P.objects_on_ && r.mover.kind != portsim::kGfxMoverNone && !P.movers_.empty()) P.mover_offset(r.mover, vp, mx, my);
      Rect b = run_box(r, vp, mx, my);
      b = Rect{std::max(b.x0, rx0), std::max(b.y0, ry0), std::min(b.x1, rx1), std::min(b.y1, ry1)};
      if (b.x1 > b.x0 && b.y1 > b.y0) rects.push_back(b);
    }
    merge_rects(rects, 4);
  }
  ++d.course_builds;
  if (!pack_patches(d.dev, d.ctx, P, d.w, d.h, rects, true, d.atlas_c, d.atlas_buf, d.tmp, d.qc, d.course_px, nullptr)) d.qc.clear();
  make_sbuf(d.dev, d.ctx, d.quads_c, sizeof(QuadRec), static_cast<uint32_t>(d.qc.size()), d.qc.data());
}

// The final patches: the HUD layers' text runs, the tee lines and the FPS overlay, the CPU's whole picture in their rectangles.
// Static rectangles (the runs of objects that do not move) are made when the frame or the client changes; each is classified once:
// rendered over a black and over a white course picture with the layers alone (render_rect with probe_), equal results mean the
// course does not show through any pixel of it (every rule is per channel non-decreasing in the pixel under it: opaque words, the
// blitPair1 sums without a channel overflow, the quarter blend, the 8-bit mixes, GDI's text blends; the shade tables are checked at
// start; a frame where a blitPair1 channel can overflow, or a shade table is not monotone, or the tool previews / tee lines move
// with the slide, classifies every rectangle as dependent), so its pixels stay valid while the camera slides or objects move.
// Dependent ones are rendered again whenever the slide, the objects, the live cursor or the overlay changed. The dynamic set
// (runs that move with an object, the tee lines, the overlay) is made at every paint that changed.
void GpuCompositor::build_final_patches(const Presenter &P, const Viewport &vp, bool text, int32_t slx, int32_t sly, int32_t cdx,
                                        int32_t shx, int32_t shy, bool moving_tools) {
  Impl &d = *d_;
  const int32_t cw = d.w, ch = d.h;
  auto clip = [&](Rect b) { return Rect{std::max(b.x0, 0), std::max(b.y0, 0), std::min(b.x1, cw), std::min(b.y1, ch)}; };
  const portsim::GfxHudFrame &f = P.hud_for(text);
  const bool movers = P.objects_on_ && !P.movers_.empty();
  char sk[160], dk[200];
  std::snprintf(sk, sizeof sk, "%llu/%d,%d/%d/%d/%d/%d", static_cast<unsigned long long>(P.content_gen_), cw, ch, text ? 1 : 0,
                moving_tools ? 1 : 0, d.classify_ok ? 1 : 0, movers ? 1 : 0);
  std::snprintf(dk, sizeof dk, "%d,%d/%llu/%d,%d,%d/%d,%d,%d,%d", slx, sly, static_cast<unsigned long long>(P.world_gen_), cdx, shx, shy,
                P.live_.active ? 1 : 0, P.live_.sx, P.live_.sy, P.live_.dx);
  const bool new_static = d.fstat_key != std::string(sk) + "|" + d.tset_key;  // P15B: + the tabled runs' set
  const bool new_dyn = new_static || d.fdyn_key != dk;
  // the runs: static (no moving object) and dynamic (a label that moves with its object)
  std::vector<Rect> srects, drects;
  if (text && new_static) {
    const bool anchor = P.hud_anchor_ && vp.origin_x > 0 && P.defect_ != PresentDefect::HudUnmoved;
    const int32_t dxl = -vp.origin_x, dxr = cw - vp.origin_x - vp.client_w;
    for (const portsim::GfxTextRun &r : P.text_.runs) {
      if (r.layer < 0 || static_cast<size_t>(r.layer) >= f.layers.size()) continue;
      if (movers && r.mover.kind != portsim::kGfxMoverNone) continue;  // the dynamic set
      {  // P15B: a run drawn by its table in the HUD pass
        const size_t ri = static_cast<size_t>(&r - P.text_.runs.data());
        if (ri < d.tabled.size() && d.tabled[ri]) continue;
      }
      const portsim::GfxHudLayer &l = f.layers[static_cast<size_t>(r.layer)];
      int32_t dd = anchor ? P.layer_dx(l, vp, dxl, dxr) : 0;
      if (P.piece_frame_ && l.popups) {
        const int32_t k = Presenter::piece_of(*P.piece_frame_, (r.x0 + r.x1) / 2, (r.y0 + r.y1) / 2);
        if (k >= 0 && static_cast<size_t>(k) < P.piece_dx_.size() && P.piece_dx_[static_cast<size_t>(k)] != 0)
          dd = P.piece_dx_[static_cast<size_t>(k)];
      }
      const Rect b = clip(run_box(r, vp, dd, 0));
      if (b.x1 > b.x0 && b.y1 > b.y0) srects.push_back(b);
    }
    merge_rects(srects, 4);
  }
  {  // (geometry of the dynamic set: recomputed at every build, it is small)
    if (text && movers) {
      const bool anchor = P.hud_anchor_ && vp.origin_x > 0 && P.defect_ != PresentDefect::HudUnmoved;
      const int32_t dxl = -vp.origin_x, dxr = cw - vp.origin_x - vp.client_w;
      for (const portsim::GfxTextRun &r : P.text_.runs) {
        if (r.layer < 0 || static_cast<size_t>(r.layer) >= f.layers.size() || r.mover.kind == portsim::kGfxMoverNone) continue;
        const portsim::GfxHudLayer &l = f.layers[static_cast<size_t>(r.layer)];
        int32_t dd = anchor ? P.layer_dx(l, vp, dxl, dxr) : 0;
        int32_t mx = 0, my = 0;
        P.mover_offset(r.mover, vp, mx, my);
        const Rect b = clip(run_box(r, vp, dd + mx, my));
        if (b.x1 > b.x0 && b.y1 > b.y0) drects.push_back(b);
      }
    }
    const bool lines = P.line_native_ && !f.lines.empty() && f.line800.size() == static_cast<size_t>(kLogicalW) * kLogicalH &&
                       f.line_word.size() == f.line800.size();
    if (lines && vp.client_w > 0 && vp.client_h > 0) {  // draw_lines' box of each line, the same ends
      const double sx = static_cast<double>(vp.client_w) / kLogicalW, sy = static_cast<double>(vp.client_h) / kLogicalH;
      for (const portsim::GfxHudLine &ln : f.lines) {
        const double half = std::min(static_cast<double>(ln.width), 10.0) * (sx + sy) / 4.0;
        const double ax = vp.origin_x + slx + ln.x1 * sx, ay = vp.origin_y + sly + ln.y1 * sy;
        const double bx = vp.origin_x + cdx + shx + slx + ln.x2 * sx, by = vp.origin_y + shy + sly + ln.y2 * sy;
        const double reach = half + 1.5;
        const Rect b = clip(Rect{static_cast<int32_t>(std::floor(std::min(ax, bx) - reach)) - 1,
                                 static_cast<int32_t>(std::floor(std::min(ay, by) - reach)) - 1,
                                 static_cast<int32_t>(std::ceil(std::max(ax, bx) + reach)) + 2,
                                 static_cast<int32_t>(std::ceil(std::max(ay, by) + reach)) + 2});
        if (b.x1 > b.x0 && b.y1 > b.y0) drects.push_back(b);
      }
    }
    merge_rects(drects, 4);
  }
  // fill_gap's underlay (render_rect reads the presenter's scene_prev_; the GPU path keeps it on the GPU): read back once per slide
  // when a rectangle to render lies outside the slid world rectangle
  const bool gap = (slx != 0 || sly != 0) && P.prev_w_ == cw && P.prev_h_ == ch && d.prev_valid;
  auto need_prev = [&](const Rect &r) {
    return gap && !(r.x0 >= d.gin[0] && r.x1 <= d.gin[1] && r.y0 >= d.gin[2] && r.y1 <= d.gin[3]);
  };
  auto render_into = [&](const Rect &r, int32_t ax, int32_t ay, bool with_overlay) {
    const int32_t w = r.x1 - r.x0, h = r.y1 - r.y0;
    if (need_prev(r)) fetch_prev(P);
    P.render_rect(cw, ch, r.x0, r.y0, w, h, false, d.tmp);
    if (with_overlay) P.draw_overlay_rect(d.tmp, cw, ch, r.x0, r.y0, w, h);
    if (d.tmp.size() != static_cast<size_t>(w) * static_cast<size_t>(h)) return;
    for (int32_t y = 0; y < h; ++y)
      std::memcpy(d.fbuf.data() + static_cast<size_t>(ay + y) * static_cast<size_t>(d.atlas_fw) + static_cast<size_t>(ax),
                  d.tmp.data() + static_cast<size_t>(y) * static_cast<size_t>(w), static_cast<size_t>(w) * 4u);
    ++d.final_rect_renders;
    d.final_px += static_cast<uint64_t>(w) * static_cast<uint64_t>(h);
  };
  auto upload_rows = [&](int32_t y0, int32_t y1) {
    if (y1 <= y0) return;
    const ProfScope prof(Phase::Upload);
    put_tex(d.ctx, d.atlas_f, 0, y0, d.atlas_fw, y1 - y0, d.fbuf.data() + static_cast<size_t>(y0) * static_cast<size_t>(d.atlas_fw),
            static_cast<uint32_t>(d.atlas_fw) * 4u);
  };
  // shelf packing from row y0 at width aw
  auto pack = [](const std::vector<Rect> &rs, int32_t aw, int32_t y0, std::vector<std::pair<int32_t, int32_t>> &at) {
    int32_t x = 0, y = y0, shelf = 0;
    at.clear();
    for (const Rect &r : rs) {
      const int32_t w = r.x1 - r.x0, h = r.y1 - r.y0;
      if (x + w > aw) {
        x = 0;
        y += shelf;
        shelf = 0;
      }
      at.emplace_back(x, y);
      x += w;
      shelf = std::max(shelf, h);
    }
    return y + shelf;
  };
  ++d.final_builds;
  if (new_static) {
    if (moving_tools || !d.classify_ok) ++d.classify_off;
    d.atlas_fw = std::max<int32_t>(cw, 256);
    d.fstat.clear();
    std::vector<std::pair<int32_t, int32_t>> at;
    d.fstat_h = pack(srects, d.atlas_fw, 0, at);
    std::vector<uint32_t> pa, pb;
    for (size_t i = 0; i < srects.size(); ++i) {
      Impl::FRect fr;
      fr.r = srects[i];
      fr.ax = at[i].first;
      fr.ay = at[i].second;
      fr.dep = true;
      if (!moving_tools && d.classify_ok) {  // the probes
        const Rect &r = fr.r;
        P.probe_ = 0xff000000u;
        P.render_rect(cw, ch, r.x0, r.y0, r.x1 - r.x0, r.y1 - r.y0, false, pa);
        P.probe_ = 0xffffffffu;
        P.render_rect(cw, ch, r.x0, r.y0, r.x1 - r.x0, r.y1 - r.y0, false, pb);
        P.probe_ = 0;
        fr.dep = pa != pb;
        static const char *pdir = std::getenv("SG_P14_PROBE_DIR");  // development: the first probe pairs as BMPs
        static int pdumps = 0;
        if (pdir && pdumps < 12 && pa.size() == pb.size()) {
          const std::string base = std::string(pdir) + "/probe_" + std::to_string(pdumps++) + "_" + std::to_string(r.x0) + "_" +
                                   std::to_string(r.y0) + (fr.dep ? "_dep" : "_static");
          write_bmp(base + "_b.bmp", pa.data(), r.x1 - r.x0, r.y1 - r.y0);
          write_bmp(base + "_w.bmp", pb.data(), r.x1 - r.x0, r.y1 - r.y0);
        }
      }
      d.fstat.push_back(fr);
      if (fr.dep) ++d.final_dep_rects;
      else ++d.final_static_rects;
    }
    d.fbuf.assign(static_cast<size_t>(d.atlas_fw) * static_cast<size_t>(std::max(1, d.fstat_h)), 0);
    for (const Impl::FRect &fr : d.fstat) render_into(fr.r, fr.ax, fr.ay, false);
  } else if (new_dyn) {  // the dependent static rectangles again, where the picture under them changed (fix 2)
    const bool all = slx != d.f_slx || sly != d.f_sly || d.wdirty_all;  // the course moved: every one
    std::vector<Rect> changed = d.cchanged;
    const WorldImage &W = P.world_;
    if (!all && W.fw > 0 && W.ph > 0) {
      auto lo = [](int64_t a, int64_t b) { return static_cast<int32_t>(a >= 0 ? a / b : -((-a + b - 1) / b)); };
      for (const std::array<int32_t, 4> &r : d.wdirty)  // world pixels -> client (compose_world's mapping inverted), 3 px margin
        changed.push_back(Rect{vp.origin_x + slx + lo(static_cast<int64_t>(r[0] - W.ox) * vp.client_w, W.fw) - 3,
                               vp.origin_y + sly + lo(static_cast<int64_t>(r[1]) * vp.client_h, W.ph) - 3,
                               vp.origin_x + slx + lo(static_cast<int64_t>(r[2] - W.ox) * vp.client_w + W.fw - 1, W.fw) + 3,
                               vp.origin_y + sly + lo(static_cast<int64_t>(r[3]) * vp.client_h + W.ph - 1, W.ph) + 3});
      // the tool previews at the last build's and this build's offsets (draw_cursor's box; the 2D preview pixels' box)
      auto previews = [&](int32_t pdx, int32_t psx, int32_t psy) {
        if (d.cur_on) {
          const int32_t cox = vp.origin_x + pdx + psx + slx, coy = vp.origin_y + psy + sly;
          changed.push_back(Rect{cox + static_cast<int32_t>(static_cast<int64_t>(W.cur_box[0] - W.ox) * vp.client_w / W.fw) - 2,
                                 coy + static_cast<int32_t>(static_cast<int64_t>(W.cur_box[1]) * vp.client_h / W.ph) - 2,
                                 cox + static_cast<int32_t>(static_cast<int64_t>(W.cur_box[2] - W.ox) * vp.client_w / W.fw) + 3,
                                 coy + static_cast<int32_t>(static_cast<int64_t>(W.cur_box[3]) * vp.client_h / W.ph) + 3});
        }
        if (d.c2_any) {
          const int32_t ox = vp.origin_x + pdx + psx + slx, oy = vp.origin_y + psy + sly;
          changed.push_back(Rect{ox + d.c2_box[0] * vp.client_w / kLogicalW - 3, oy + d.c2_box[1] * vp.client_h / kLogicalH - 3,
                                 ox + d.c2_box[2] * vp.client_w / kLogicalW + 4, oy + d.c2_box[3] * vp.client_h / kLogicalH + 4});
        }
      };
      if (cdx != d.f_cdx || shx != d.f_shx || shy != d.f_shy) {
        previews(d.f_cdx, d.f_shx, d.f_shy);
        previews(cdx, shx, shy);
      }
    }
    auto meets = [&](const Rect &a) {
      for (const Rect &b : changed)
        if (a.x0 < b.x1 && b.x0 < a.x1 && a.y0 < b.y1 && b.y0 < a.y1) return true;
      return false;
    };
    for (const Impl::FRect &fr : d.fstat) {
      if (!fr.dep) continue;
      if (all || meets(fr.r)) {
        render_into(fr.r, fr.ax, fr.ay, false);
        ++d.final_dep_renders;
      } else {
        ++d.final_dep_kept;
      }
    }
  }
  // P15B fix 2: with the overlay drawn by the HUD pass (d.ov_on), a patch that meets its box holds the overlay too; a kept static
  // rectangle that meets it (its pixels made before this second's line) gives the overlay back to its own patch, drawn last
  int32_t ob[4] = {0, 0, 0, 0};
  bool ovgpu = d.p15 && d.ov_on;
  if (ovgpu) {
    std::memcpy(ob, d.ov_box, sizeof ob);
    for (const Impl::FRect &fr : d.fstat)
      if (fr.r.x0 < ob[2] && ob[0] < fr.r.x1 && fr.r.y0 < ob[3] && ob[1] < fr.r.y1) ovgpu = false;
  }
  const bool ov = ovgpu || (P.overlay_mode_ && P.overlay_box(cw, ch, ob));
  if (!ovgpu && d.ov_on) ++d.ov_patch_paints;
  auto meets_ov = [&](const Rect &r) { return ov && r.x0 < ob[2] && ob[0] < r.x1 && r.y0 < ob[3] && ob[1] < r.y1; };
  // the dynamic set and the overlay after the static rows (re-packed at every change)
  std::vector<Rect> rest = drects;
  if (ov && !ovgpu) rest.push_back(Rect{ob[0], ob[1], ob[2], ob[3]});  // last: drawn over the rest
  std::vector<std::pair<int32_t, int32_t>> at;
  const int32_t total = pack(rest, d.atlas_fw, d.fstat_h, at);
  if (total > 16384) {  // does not fit: no patches this paint (counted)
    ++d.final_overflow;
    d.qf.clear();
    return;
  }
  if (d.fbuf.size() < static_cast<size_t>(d.atlas_fw) * static_cast<size_t>(std::max(1, total)))
    d.fbuf.resize(static_cast<size_t>(d.atlas_fw) * static_cast<size_t>(std::max(1, total)), 0);
  for (size_t i = 0; i < rest.size(); ++i)
    render_into(rest[i], at[i].first, at[i].second, ovgpu ? meets_ov(rest[i]) : ov && i + 1 == rest.size());
  if (!make_tex(d.dev, d.atlas_f, std::max(d.atlas_f.w, d.atlas_fw), std::max({d.atlas_f.h, total, 1}), DXGI_FORMAT_R32_UINT, false)) {
    d.qf.clear();
    return;
  }
  if (new_static || d.atlas_f.h < total) upload_rows(0, total);
  else {
    if (new_dyn) upload_rows(0, d.fstat_h);  // the dependent rectangles (their rows)
    upload_rows(d.fstat_h, total);
  }
  d.qf.clear();
  for (const Impl::FRect &fr : d.fstat)
    d.qf.push_back(QuadRec{fr.r.x0, fr.r.y0, fr.r.x1 - fr.r.x0, fr.r.y1 - fr.r.y0, fr.ax, fr.ay, 0, 0});
  for (size_t i = 0; i < rest.size(); ++i)
    d.qf.push_back(QuadRec{rest[i].x0, rest[i].y0, rest[i].x1 - rest[i].x0, rest[i].y1 - rest[i].y0, at[i].first, at[i].second, 0, 0});
  make_sbuf(d.dev, d.ctx, d.quads_f, sizeof(QuadRec), static_cast<uint32_t>(d.qf.size()), d.qf.data());
  d.fstat_key = std::string(sk) + "|" + d.tset_key;  // P15B
  d.fdyn_key = dk;
  d.fov_ok = true;
  d.f_slx = slx;
  d.f_sly = sly;
  d.f_cdx = cdx;
  d.f_shx = shx;
  d.f_shy = shy;
  d.wdirty.clear();
  d.wdirty_all = false;
  d.cchanged.clear();
}

// P15B: the HUD layers' text runs that do not move with an object, as tables (textres.h TextTable) the HUD pass applies after each
// run's layer (text_runs in the shader). A run's place is draw_runs' (the layer's anchor offset, or the offset of the popup piece
// that owns its box's centre); its table is made again only when something TextRaster::table reads changed (the cache key), so a
// frame whose runs are unchanged makes no GDI draw, and a paint makes none. The runs left out (movers, SG_APP_P15=off) stay final
// patches (build_final_patches skips the tabled ones).
namespace {
struct TLayRec {
  int32_t first, n, x0, y0, x1, y1, pad0, pad1;
};
struct TRunRec {
  int32_t x0, y0, x1, y1, ibase, pad0, pad1, pad2;
};
}  // namespace

void GpuCompositor::build_text_tables(const Presenter &P, const Viewport &vp, bool text) {
  Impl &d = *d_;
  const int32_t cw = d.w, ch = d.h;
  const portsim::GfxHudFrame &f = P.hud_for(text);
  const bool movers = P.objects_on_ && !P.movers_.empty();
  std::string sk;
  {
    char b[256];
    std::snprintf(b, sizeof b, "%llu/%d,%d/%d/%d/%d,%d,%d,%d/%zu/%p/%d,%d,%d/", static_cast<unsigned long long>(P.content_gen_), cw, ch,
                  text ? 1 : 0, movers ? 1 : 0, vp.origin_x, vp.origin_y, vp.client_w, vp.client_h, f.layers.size(),
                  static_cast<const void *>(&f), static_cast<int>(P.text_aa_), P.text_fit_ ? 1 : 0, static_cast<int>(P.text_defect_));
    sk = b;
    for (int32_t v : P.piece_dx_) sk += std::to_string(v) + ",";
  }
  if (sk == d.tset_key) return;
  const int64_t t0 = portsim::perf_now_ns();
  d.tset_key = sk;
  ++d.tbuild;
  ++d.text_sets;
  d.tabled.assign(P.text_.runs.size(), 0);
  if (!d.traster) d.traster.reset(new TextRaster());
  d.traster->set_defect(P.text_defect_);
  d.traster->set_aa(P.text_aa_);
  d.traster->set_fit(P.text_fit_);
  std::vector<std::vector<const TextTable *>> per(f.layers.size());
  if (text && d.p15) {
    const bool anchor = P.hud_anchor_ && vp.origin_x > 0 && P.defect_ != PresentDefect::HudUnmoved;
    const int32_t dxl = -vp.origin_x, dxr = cw - vp.origin_x - vp.client_w;
    std::string key;
    auto put = [&key](int64_t v) { key.append(reinterpret_cast<const char *>(&v), sizeof v); };
    for (size_t i = 0; i < P.text_.runs.size(); ++i) {
      const portsim::GfxTextRun &r = P.text_.runs[i];
      if (r.layer < 0 || static_cast<size_t>(r.layer) >= f.layers.size()) continue;
      if (movers && r.mover.kind != portsim::kGfxMoverNone) continue;  // moves with its object: a final patch
      if (r.target == 2 && P.world_.empty()) {  // draw_runs skips it: nothing to draw
        d.tabled[i] = 1;
        continue;
      }
      // draw_runs' offset (compose_layers' dx0, or the popup piece's)
      const portsim::GfxHudLayer &l = f.layers[static_cast<size_t>(r.layer)];
      int32_t dd = anchor ? P.layer_dx(l, vp, dxl, dxr) : 0;
      if (P.piece_frame_ && static_cast<size_t>(r.layer) < P.piece_frame_->layers.size() &&
          P.piece_frame_->layers[static_cast<size_t>(r.layer)].popups) {
        const int32_t k = Presenter::piece_of(*P.piece_frame_, (r.x0 + r.x1) / 2, (r.y0 + r.y1) / 2);
        if (k >= 0 && static_cast<size_t>(k) < P.piece_dx_.size() && P.piece_dx_[static_cast<size_t>(k)] != 0)
          dd = P.piece_dx_[static_cast<size_t>(k)];
      }
      // the cache key: every input of TextRaster::table
      key.clear();
      key += r.face;
      key += '\0';
      key += r.text;
      key += '\0';
      for (int64_t v : {static_cast<int64_t>(r.size), static_cast<int64_t>(r.style), static_cast<int64_t>(r.colorref),
                        static_cast<int64_t>(r.x), static_cast<int64_t>(r.y), static_cast<int64_t>(r.x0), static_cast<int64_t>(r.y0),
                        static_cast<int64_t>(r.x1), static_cast<int64_t>(r.y1), static_cast<int64_t>(vp.origin_x),
                        static_cast<int64_t>(vp.origin_y), static_cast<int64_t>(vp.client_w), static_cast<int64_t>(vp.client_h),
                        static_cast<int64_t>(dd), static_cast<int64_t>(cw), static_cast<int64_t>(ch),
                        static_cast<int64_t>(P.text_aa_), static_cast<int64_t>(P.text_fit_ ? 1 : 0),
                        static_cast<int64_t>(P.text_defect_), static_cast<int64_t>(r.vis_layer.size())})
        put(v);
      key.append(reinterpret_cast<const char *>(r.vis_layer.data()), r.vis_layer.size());
      auto it = d.tcache.find(key);
      if (it == d.tcache.end()) {
        Impl::TCache c;
        d.traster->table(r, true, vp, dd, cw, ch, c.t);  // false = draw draws nothing either (an empty table)
        it = d.tcache.emplace(key, std::move(c)).first;
        ++d.text_tables_made;
      } else {
        ++d.text_tables_reused;
      }
      it->second.used = d.tbuild;
      d.tabled[i] = 1;
      ++d.text_runs_tabled;
      if (!it->second.t.idx.empty()) per[static_cast<size_t>(r.layer)].push_back(&it->second.t);
    }
  }
  d.tper = std::move(per);
  d.tdirty = true;
  // the cache keeps the tables of the last builds (a run that comes back, the date / money that alternate)
  if (d.tcache.size() > 512)
    for (auto it = d.tcache.begin(); it != d.tcache.end();)
      it = it->second.used + 8 < d.tbuild ? d.tcache.erase(it) : std::next(it);
  d.ns_text += portsim::perf_now_ns() - t0;
}

// P15B fix 2: the FPS overlay (draw_overlay_rect: the box darkened to half, then its runs with grey antialiasing) drawn by the HUD
// pass over the whole picture; its runs' tables are made when the profiler line, the mode or the client changes (once a second).
void GpuCompositor::build_overlay_tables(const Presenter &P, int32_t cw, int32_t ch) {
  Impl &d = *d_;
  char k[96];
  std::snprintf(k, sizeof k, "%d/%d,%d/%llu/%d", P.overlay_mode_, cw, ch, static_cast<unsigned long long>(P.overlay_line_.t_s * 1000.0),
                d.p15 ? 1 : 0);
  if (d.ov_key == k) return;
  d.ov_key = k;
  ++d.ov_builds;
  const int64_t t0 = portsim::perf_now_ns();
  d.ov_tables.clear();
  d.ov_on = false;
  d.tdirty = true;
  std::vector<portsim::GfxTextRun> runs;
  Viewport ovp;
  int32_t dxr = 0;
  if (d.p15 && P.overlay_mode_ && P.overlay_runs(cw, ch, runs, ovp, dxr, d.ov_box)) {
    if (!d.oraster) d.oraster.reset(new TextRaster());
    d.oraster->set_aa(TextAa::Grey);  // draw_overlay_rect's raster: grey, no fit, no defect
    d.ov_tables.resize(runs.size());
    // the box's words are darkened to half before the runs ((d >> 1) & 0x7f7f7f: every channel <= 127), so to555 gives levels 0..15
    for (size_t i = 0; i < runs.size(); ++i) d.oraster->table(runs[i], false, ovp, dxr, cw, ch, d.ov_tables[i], 16);
    d.ov_on = true;
  }
  d.ns_text += portsim::perf_now_ns() - t0;
}

// P15B: the text tables' buffers (the HUD layers' runs, then the overlay's in the slot after them)
void GpuCompositor::upload_text_tables(int32_t cw, int32_t ch) {
  Impl &d = *d_;
  if (!d.tdirty) return;
  d.tdirty = false;
  const size_t nl = std::max<size_t>(64, d.tper.size());  // at least 64 records: the shader reads tlay[li] for every layer
  std::vector<TLayRec> lay(nl + 1, TLayRec{0, 0, 0, 0, 0, 0, 0, 0});
  std::vector<TRunRec> runs;
  std::vector<uint32_t> idx, lut;
  std::vector<const TextTable *> ov;
  for (const TextTable &t : d.ov_tables)
    if (!t.idx.empty()) ov.push_back(&t);
  static const std::vector<const TextTable *> none;
  for (size_t li = 0; li <= nl; ++li) {
    const std::vector<const TextTable *> &list = li < d.tper.size() ? d.tper[li] : li == nl ? ov : none;
    TLayRec &L = lay[li];
    L.first = static_cast<int32_t>(runs.size());
    L.x0 = cw;
    L.y0 = ch;
    for (const TextTable *t : list) {
      runs.push_back(TRunRec{t->x0, t->y0, t->x1, t->y1, static_cast<int32_t>(idx.size()), 0, 0, 0});
      const uint32_t off = static_cast<uint32_t>(lut.size() / 16);
      for (uint32_t v : t->idx) idx.push_back(v ? v + off : 0u);
      for (size_t k = 0; k + 1 < t->lut.size(); k += 2)
        lut.push_back(static_cast<uint32_t>(t->lut[k]) | (static_cast<uint32_t>(t->lut[k + 1]) << 16));
      L.x0 = std::min(L.x0, t->x0);
      L.y0 = std::min(L.y0, t->y0);
      L.x1 = std::max(L.x1, t->x1);
      L.y1 = std::max(L.y1, t->y1);
    }
    L.n = static_cast<int32_t>(runs.size()) - L.first;
    if (L.n == 0) L.x0 = L.y0 = L.x1 = L.y1 = 0;
  }
  d.ov_li = static_cast<int32_t>(nl);
  d.text_entries += lut.size() / 16;
  d.tbound = !runs.empty();
  if (d.tbound) {
    const ProfScope prof(Phase::Upload);
    const bool ok = make_sbuf(d.dev, d.ctx, d.tlay, sizeof(TLayRec), static_cast<uint32_t>(lay.size()), lay.data()) &&
                    make_sbuf(d.dev, d.ctx, d.truns, sizeof(TRunRec), static_cast<uint32_t>(runs.size()), runs.data()) &&
                    make_sbuf(d.dev, d.ctx, d.tidx, 4, static_cast<uint32_t>(idx.size()), idx.data()) &&
                    make_sbuf(d.dev, d.ctx, d.tlut, 4, static_cast<uint32_t>(lut.size()), lut.data());
    if (!ok) {  // no buffers: the runs and the overlay back to final patches (the static set is made again)
      d.tbound = false;
      std::fill(d.tabled.begin(), d.tabled.end(), static_cast<uint8_t>(0));
      d.ov_on = false;
      d.fstat_key.clear();
    }
  }
}

// ---- a paint
void GpuCompositor::paint_layered(const Presenter &P, int32_t cw, int32_t ch, int32_t slx, int32_t sly) {
  Impl &d = *d_;
  const Viewport vp = fit_viewport(cw, ch);
  const bool text = P.text_now(vp);
  const portsim::GfxHudFrame &f = P.hud_for(text);
  ++d.paints_layered;
  // compose_layers' per-paint state: the piece offsets, the remap (rebuilt only when the frame or the client changed: it depends on
  // nothing else), the previews' offsets
  P.layer_pieces(f, vp, cw);
  if (d.remap_gen != P.content_gen_ || d.remap_w != cw || d.remap_h != ch) {
    P.layer_remap(f, vp, cw);
    d.remap_gen = P.content_gen_;
    d.remap_w = cw;
    d.remap_h = ch;
  }
  {
    const ProfScope prof(Phase::Upload);
    if (d.frame_gen != P.content_gen_ || d.frame_f != &f || d.frame_w != cw || d.frame_h != ch || d.frame_text != text)
      upload_frame(P, vp, text);
    else if (d.world_gen != P.world_gen_)
      upload_world(P, text);
  }
  const WorldImage &W = P.world_;
  const bool anchor = P.hud_anchor_ && vp.origin_x > 0 && P.defect_ != PresentDefect::HudUnmoved;
  const bool cur = d.cur_on;
  const bool c2 = f.cursor2d.size() == static_cast<size_t>(kLogicalW) * kLogicalH;
  const bool live = P.live_.active && (cur || c2);
  const int32_t cdx = (cur || c2) && anchor && P.remap_ ? (live ? P.live_.dx : P.remap_->cursor_dx()) : 0;
  auto scale_round = [](int32_t v, int32_t n, int32_t dd) {
    const int64_t p = static_cast<int64_t>(v) * n;
    return static_cast<int32_t>(p >= 0 ? (p + dd / 2) / dd : -((-p + dd / 2) / dd));
  };
  const int32_t shx = live ? scale_round(P.live_.sx, vp.client_w, kLogicalW) : 0;
  const int32_t shy = live ? scale_round(P.live_.sy, vp.client_h, kLogicalH) : 0;
  const bool c2move = c2 && (cdx != 0 || shx != 0 || shy != 0 || slx != 0 || sly != 0);
  const bool lines = P.line_native_ && !f.lines.empty() && f.line800.size() == static_cast<size_t>(kLogicalW) * kLogicalH &&
                     f.line_word.size() == f.line800.size();
  size_t cur_layer = f.layers.size(), line_layer = f.layers.size();
  P.draw_order(f, cur, lines, cur_layer, line_layer);
  {
    const ProfScope prof(Phase::Patch);
    // the course patches change with the frame, the client, and (mover labels) the objects' offsets
    bool mover_runs = false;
    if (P.objects_on_ && !P.movers_.empty())
      for (const portsim::GfxTextRun &r : P.text_.runs)
        if ((r.layer == -1 || r.layer == -3) && r.mover.kind != portsim::kGfxMoverNone) mover_runs = true;
    const uint64_t ck = P.content_gen_ * 1000003ull + (mover_runs ? P.world_gen_ * 7919ull : 0ull) + static_cast<uint64_t>(cw) * 31ull +
                        static_cast<uint64_t>(ch);
    if (ck != d.course_key) {
      const int64_t t0 = portsim::perf_now_ns();
      // fix 2: the final patches over the course patches' old and new places see a changed course picture
      for (const QuadRec &q : d.qc) d.cchanged.push_back(Rect{q.dx + slx, q.dy + sly, q.dx + q.w + slx, q.dy + q.h + sly});
      build_course_patches(P, vp, text);
      for (const QuadRec &q : d.qc) d.cchanged.push_back(Rect{q.dx + slx, q.dy + sly, q.dx + q.w + slx, q.dy + q.h + sly});
      d.course_key = ck;
      d.ns_course += portsim::perf_now_ns() - t0;
    }
    char k[256];
    std::snprintf(k, sizeof k, "%llu/%llu/%d,%d/%d,%d,%d/%d,%d,%d,%d/%llu/%d,%d/%d", static_cast<unsigned long long>(P.content_gen_),
                  static_cast<unsigned long long>(P.world_gen_), slx, sly, cdx, shx, shy, P.live_.active ? 1 : 0, P.live_.sx,
                  P.live_.sy, P.live_.dx, static_cast<unsigned long long>(P.overlay_line_.t_s * 1000.0), cw, ch, P.overlay_mode_);
    if (d.final_key != k) {
      const int64_t t0 = portsim::perf_now_ns();
      {  // fill_gap's world rectangle at this slide (the patches that need the previous picture lie outside it)
        auto fdiv = [](int64_t a, int64_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); };
        const int32_t ox0 = vp.origin_x + slx, oy0 = vp.origin_y + sly;
        int32_t rx0 = ox0, rx1 = ox0 + vp.client_w;
        if (W.fw > 0) {
          rx0 = std::min<int32_t>(rx0, ox0 + static_cast<int32_t>(fdiv(static_cast<int64_t>(-W.ox) * vp.client_w, W.fw)));
          rx1 = std::max<int32_t>(rx1, ox0 - static_cast<int32_t>(fdiv(-static_cast<int64_t>(W.pw - W.ox) * vp.client_w, W.fw)));
        }
        d.gin[0] = std::max(0, rx0);
        d.gin[1] = std::min(cw, rx1);
        d.gin[2] = std::max(0, oy0);
        d.gin[3] = std::min(ch, oy0 + vp.client_h);
      }
      build_text_tables(P, vp, text);  // P15B: before the final patches (which skip the tabled runs)
      build_overlay_tables(P, cw, ch);  // P15B fix 2
      upload_text_tables(cw, ch);
      // (the previews / the 2D preview pixels / the tee lines move with the slide and the live cursor: no rectangle is kept then)
      build_final_patches(P, vp, text, slx, sly, cdx, shx, shy, cur || d.c2_any || lines);
      d.final_key = k;
      d.ns_final += portsim::perf_now_ns() - t0;
    }
  }
  const bool near_t = P.effective_scale(vp.client_w, vp.client_h) == ScaleMode::Nearest;
  const bool wnear_t = (W.fw == vp.client_w && W.ph == vp.client_h) || near_t;
  {
    const ProfScope prof(Phase::Upload);
    upload_tables(vp, near_t, wnear_t, W.ox, W.fw, W.pw, W.ph);
  }
  // the constant buffer
  GpuCB c = {};
  c.cw = cw;
  c.ch = ch;
  c.vpx = vp.origin_x;
  c.vpy = vp.origin_y;
  c.vpw = vp.client_w;
  c.vph = vp.client_h;
  c.slx = slx;
  c.sly = sly;
  const bool near_ = P.effective_scale(vp.client_w, vp.client_h) == ScaleMode::Nearest;
  c.base_near = near_ ? 1 : 0;
  c.world_near = (W.fw == vp.client_w && W.ph == vp.client_h) || near_ ? 1 : 0;
  c.layer_near = near_ ? 1 : 0;
  c.mask_on = 1;
  c.wox = W.ox;
  c.wfw = W.fw;
  c.wpw = W.pw;
  c.wph = W.ph;
  c.has_world = W.empty() ? 0 : 1;
  // fill_gap (render calls it with a slide offset; it needs the last painted course picture at this client size)
  c.gap_on = (slx != 0 || sly != 0) && P.prev_w_ == cw && P.prev_h_ == ch && d.prev_valid ? 1 : 0;
  c.gux = slx - P.s0x_i_;
  c.guy = sly - P.s0y_i_;
  {
    Viewport vs = vp;
    vs.origin_x += slx;
    vs.origin_y += sly;
    auto fdiv = [](int64_t a, int64_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); };
    int32_t rx0 = vs.origin_x, rx1 = vs.origin_x + vs.client_w;
    if (W.fw > 0) {
      rx0 = std::min<int32_t>(rx0, vs.origin_x + static_cast<int32_t>(fdiv(static_cast<int64_t>(-W.ox) * vs.client_w, W.fw)));
      rx1 = std::max<int32_t>(rx1, vs.origin_x - static_cast<int32_t>(fdiv(-static_cast<int64_t>(W.pw - W.ox) * vs.client_w, W.fw)));
    }
    c.gin_x0 = std::max(0, rx0);
    c.gin_x1 = std::min(cw, rx1);
    c.gin_y0 = std::max(0, vs.origin_y);
    c.gin_y1 = std::min(ch, vs.origin_y + vs.client_h);
  }
  c.nlayers = d.nlayers;
  c.cur_layer = static_cast<int32_t>(cur_layer);
  c.cur_on = cur ? 1 : 0;
  c.box_on = d.box_on ? 1 : 0;
  c.cdx = cdx;
  c.shx = shx;
  c.shy = shy;
  c.c2move = c2move ? 1 : 0;
  c.lines_on = lines ? 1 : 0;
  c.pieces_on = P.pieces_on(f, vp) ? 1 : 0;
  c.npieces = static_cast<int32_t>(f.pieces.size());
  c.cb0 = W.cur_box[0];
  c.cb1 = W.cur_box[1];
  c.cb2 = W.cur_box[2];
  c.cb3 = W.cur_box[3];
  c.shade_n0 = static_cast<int32_t>(d.shade_n0);
  c.shade_n1 = static_cast<int32_t>(d.shade_n1);
  c.toff_x = 2 * cw;
  c.toff_y = 2 * ch;
  c.tn_x = 5 * cw;
  c.tn_y = 5 * ch;
  d.ctx->UpdateSubresource(d.cb, 0, nullptr, &c, 0, 0);
  std::vector<int32_t> pdx(P.piece_dx_.begin(), P.piece_dx_.end());
  make_sbuf(d.dev, d.ctx, d.pdx, 4, static_cast<uint32_t>(pdx.size()), pdx.data());
  // the passes
  const int64_t td0 = portsim::perf_now_ns();
  ts_begin();
  ID3D11DeviceContext *x = d.ctx;
  D3D11_VIEWPORT v = {0, 0, static_cast<float>(cw), static_cast<float>(ch), 0, 1};
  x->RSSetViewports(1, &v);
  x->RSSetState(d.rs);
  x->IASetInputLayout(nullptr);
  x->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  x->VSSetShader(d.vs_full, nullptr, 0);
  x->PSSetConstantBuffers(0, 1, &d.cb);
  ID3D11ShaderResourceView *none[21] = {};
  ID3D11ShaderResourceView *tabs[6] = {d.lcol.srv, d.lrow.srv, d.bcol.srv, d.brow.srv, d.wcol.srv, d.wrow.srv};
  // 1. the course picture
  x->PSSetShaderResources(0, 21, none);
  x->PSSetShaderResources(14, 6, tabs);
  x->PSSetShaderResources(20, 1, &d.occ_srv);
  x->OMSetRenderTargets(1, &d.course.rtv, nullptr);
  ID3D11ShaderResourceView *s1[4] = {d.base.srv, d.world.srv, d.wmask.srv, d.prev_valid ? d.prev.srv : d.dummy_u.srv};
  x->PSSetShaderResources(0, 4, s1);
  x->PSSetShader(d.ps_course, nullptr, 0);
  x->Draw(3, 0);
  // 2. the course patches at the slide's offset
  if (!d.qc.empty()) {
    QuadCB q = {slx, sly, cw, ch};
    x->UpdateSubresource(d.qcb, 0, nullptr, &q, 0, 0);
    x->VSSetShader(d.vs_quad, nullptr, 0);
    x->VSSetConstantBuffers(1, 1, &d.qcb);
    x->VSSetShaderResources(12, 1, &d.quads_c.srv);
    x->PSSetShaderResources(11, 1, &d.atlas_c.srv);
    x->PSSetShader(d.ps_quad_u, nullptr, 0);
    x->DrawInstanced(6, static_cast<UINT>(d.qc.size()), 0, 0);
    x->VSSetShader(d.vs_full, nullptr, 0);
  }
  // 3. the HUD layers into the back buffer
  x->OMSetRenderTargets(1, &d.bb_rtv, nullptr);
  ID3D11ShaderResourceView *s3[11] = {d.base.srv, d.world.srv, d.wmask.srv, d.dummy_u.srv, d.course.srv, d.layers.srv, d.lpx.srv,
                                      d.tabs.srv, cur ? d.cur.srv : d.dummy_u2.srv, d.shade.srv, d.pdx.srv};
  x->PSSetShaderResources(0, 11, s3);
  {  // P15B: the text tables (null views read 0: no run in any layer)
    ID3D11ShaderResourceView *st[4] = {d.tbound ? d.tlay.srv : nullptr, d.tbound ? d.truns.srv : nullptr,
                                       d.tbound ? d.tidx.srv : nullptr, d.tbound ? d.tlut.srv : nullptr};
    x->PSSetShaderResources(21, 4, st);
    const TextCB t = {d.p15 && d.ov_on ? 1 : 0, d.ov_li, d.ov_box[0], d.ov_box[2], d.ov_box[1], d.ov_box[3], 0, 0};
    x->UpdateSubresource(d.tcb, 0, nullptr, &t, 0, 0);
    x->PSSetConstantBuffers(2, 1, &d.tcb);
  }
  x->PSSetShader(d.ps_hud, nullptr, 0);
  x->Draw(3, 0);
  {  // P15B: the overlay cbuffer unbound (the flat path runs this shader too: an unbound cbuffer reads 0)
    ID3D11Buffer *nb = nullptr;
    x->PSSetConstantBuffers(2, 1, &nb);
  }
  // 4. the final patches
  if (!d.qf.empty()) {
    QuadCB q = {0, 0, cw, ch};
    x->UpdateSubresource(d.qcb, 0, nullptr, &q, 0, 0);
    x->VSSetShader(d.vs_quad, nullptr, 0);
    x->VSSetConstantBuffers(1, 1, &d.qcb);
    x->VSSetShaderResources(12, 1, &d.quads_f.srv);
    x->PSSetShaderResources(11, 1, &d.atlas_f.srv);
    x->PSSetShader(d.ps_quad_f, nullptr, 0);
    x->DrawInstanced(6, static_cast<UINT>(d.qf.size()), 0, 0);
  }
  x->PSSetShaderResources(0, 21, none);
  ID3D11ShaderResourceView *nv[1] = {nullptr};
  x->VSSetShaderResources(12, 1, nv);
  ts_end();
  d.ns_draw += portsim::perf_now_ns() - td0;
  d.course_valid = true;
}

// fix 3: a screen without a world layer (the title, menus, reports, the world map): render's flat path. The base (the frame, or its
// text-clean copy with the runs drawn) scaled by the course pass with no world; the text runs, the side bands (fill_bands: each side
// rendered with the content columns its rule reads) and the overlay as final patches, made again when the frame or the overlay
// changes (nothing on these screens moves between frames).
void GpuCompositor::paint_flat(const Presenter &P, int32_t cw, int32_t ch) {
  Impl &d = *d_;
  ++d.paints_flat;
  const Viewport vp = fit_viewport(cw, ch);
  const bool text = P.text_now(vp);
  const std::vector<uint16_t> &fsrc = text && P.text_defect_ != TextDefect::Unremoved ? P.text_.clean : P.frame_;
  const bool near_ = P.effective_scale(vp.client_w, vp.client_h) == ScaleMode::Nearest;
  {
    const ProfScope prof(Phase::Upload);
    if (d.flat_gen != P.content_gen_ || d.flat_src != fsrc.data() || !d.base.t) {
      make_tex(d.dev, d.base, kLogicalW, kLogicalH, DXGI_FORMAT_R16_UINT, false);
      put_tex(d.ctx, d.base, 0, 0, kLogicalW, kLogicalH, fsrc.data(), kLogicalW * 2u);
      d.flat_gen = P.content_gen_;
      d.flat_src = fsrc.data();
      d.frame_gen = ~0ull;  // the layered path uploads its own base again
    }
    upload_tables(vp, near_, near_, 0, 0, 0, 0);
  }
  {
    const ProfScope prof(Phase::Patch);
    char k[200];
    std::snprintf(k, sizeof k, "flat/%llu/%d,%d/%d/%llu/%d/%d/%d", static_cast<unsigned long long>(P.content_gen_), cw, ch, text ? 1 : 0,
                  static_cast<unsigned long long>(P.overlay_line_.t_s * 1000.0), P.overlay_mode_, static_cast<int>(P.wide_),
                  static_cast<int>(P.band_));
    if (d.final_key != k) {
      const int64_t t0 = portsim::perf_now_ns();
      std::vector<Rect> rects;
      auto add = [&](Rect b) {
        b = Rect{std::max(b.x0, 0), std::max(b.y0, 0), std::min(b.x1, cw), std::min(b.y1, ch)};
        if (b.x1 > b.x0 && b.y1 > b.y0) rects.push_back(b);
      };
      if (text)  // draw_runs' flat path: every run at its own place (no world: no world run, no moved panel, no mover)
        for (const portsim::GfxTextRun &r : P.text_.runs)
          if (r.target != 2) add(run_box(r, vp, 0, 0));
      if (P.wide_ != WideFill::Off && P.band_ != BandFill::Black) {  // the bands with the content columns fill_bands reads
        const int32_t ox = vp.origin_x, w = vp.client_w, kbg = std::max<int32_t>(1, w * 64 / kLogicalW) + 1;
        const int32_t bl = ox, br = cw - ox - w;
        if (bl > 0) add(Rect{0, 0, std::min(ox + w, ox + std::max(bl, kbg) + 1), ch});
        if (br > 0) add(Rect{std::max(ox, ox + w - std::max(br, kbg) - 1), 0, cw, ch});
      }
      int32_t ob[4] = {0, 0, 0, 0};
      const bool ov = P.overlay_mode_ && P.overlay_box(cw, ch, ob);
      if (ov) add(Rect{ob[0], ob[1], ob[2], ob[3]});
      merge_rects(rects, 4);
      ++d.final_builds;
      if (!pack_patches(d.dev, d.ctx, P, cw, ch, rects, false, d.atlas_f, d.atlas_buf, d.tmp, d.qf, d.final_px, ov ? ob : nullptr))
        d.qf.clear();
      make_sbuf(d.dev, d.ctx, d.quads_f, sizeof(QuadRec), static_cast<uint32_t>(d.qf.size()), d.qf.data());
      d.final_key = k;
      d.fstat_key.clear();  // the layered path's atlas is rebuilt when it runs again
      d.ns_final += portsim::perf_now_ns() - t0;
    }
  }
  GpuCB c = {};
  c.cw = cw;
  c.ch = ch;
  c.vpx = vp.origin_x;
  c.vpy = vp.origin_y;
  c.vpw = vp.client_w;
  c.vph = vp.client_h;
  c.base_near = c.world_near = c.layer_near = near_ ? 1 : 0;
  c.mask_on = 1;
  c.cur_layer = 0;
  c.toff_x = 2 * cw;
  c.toff_y = 2 * ch;
  c.tn_x = 5 * cw;
  c.tn_y = 5 * ch;
  d.ctx->UpdateSubresource(d.cb, 0, nullptr, &c, 0, 0);
  const int64_t td0 = portsim::perf_now_ns();
  ts_begin();
  ID3D11DeviceContext *x = d.ctx;
  D3D11_VIEWPORT v = {0, 0, static_cast<float>(cw), static_cast<float>(ch), 0, 1};
  x->RSSetViewports(1, &v);
  x->RSSetState(d.rs);
  x->IASetInputLayout(nullptr);
  x->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  x->VSSetShader(d.vs_full, nullptr, 0);
  x->PSSetConstantBuffers(0, 1, &d.cb);
  ID3D11ShaderResourceView *none[21] = {};
  ID3D11ShaderResourceView *tabs[6] = {d.lcol.srv, d.lrow.srv, d.bcol.srv, d.brow.srv, d.wcol.srv, d.wrow.srv};
  x->PSSetShaderResources(0, 21, none);
  x->PSSetShaderResources(14, 6, tabs);
  x->OMSetRenderTargets(1, &d.course.rtv, nullptr);
  ID3D11ShaderResourceView *s1[4] = {d.base.srv, d.dummy_u.srv, d.dummy_u.srv, d.dummy_u.srv};
  x->PSSetShaderResources(0, 4, s1);
  x->PSSetShader(d.ps_course, nullptr, 0);
  x->Draw(3, 0);
  x->OMSetRenderTargets(1, &d.bb_rtv, nullptr);
  ID3D11ShaderResourceView *s3[5] = {d.base.srv, d.dummy_u.srv, d.dummy_u.srv, d.dummy_u.srv, d.course.srv};
  x->PSSetShaderResources(0, 5, s3);
  x->PSSetShader(d.ps_hud, nullptr, 0);
  x->Draw(3, 0);
  if (!d.qf.empty()) {
    QuadCB q = {0, 0, cw, ch};
    x->UpdateSubresource(d.qcb, 0, nullptr, &q, 0, 0);
    x->VSSetShader(d.vs_quad, nullptr, 0);
    x->VSSetConstantBuffers(1, 1, &d.qcb);
    x->VSSetShaderResources(12, 1, &d.quads_f.srv);
    x->PSSetShaderResources(11, 1, &d.atlas_f.srv);
    x->PSSetShader(d.ps_quad_f, nullptr, 0);
    x->DrawInstanced(6, static_cast<UINT>(d.qf.size()), 0, 0);
  }
  x->PSSetShaderResources(0, 21, none);
  ID3D11ShaderResourceView *nv[1] = {nullptr};
  x->VSSetShaderResources(12, 1, nv);
  ts_end();
  d.ns_draw += portsim::perf_now_ns() - td0;
  d.course_valid = false;  // the course target holds a flat screen
}

void GpuCompositor::paint_full(const Presenter &P, int32_t cw, int32_t ch) {
  Impl &d = *d_;
  ++d.paints_full;
  std::vector<uint32_t> &img = P.scratch_;
  P.render(cw, ch, img);
  if (P.overlay_mode_) {
    const ProfScope prof(Phase::Overlay);
    P.draw_overlay(img, cw, ch);
  }
  {
    const ProfScope prof(Phase::Upload);
    if (!make_tex(d.dev, d.full, cw, ch, DXGI_FORMAT_R32_UINT, false)) return;
    put_tex(d.ctx, d.full, 0, 0, cw, ch, img.data(), static_cast<uint32_t>(cw) * 4u);
  }
  ts_begin();
  ID3D11DeviceContext *x = d.ctx;
  D3D11_VIEWPORT v = {0, 0, static_cast<float>(cw), static_cast<float>(ch), 0, 1};
  x->RSSetViewports(1, &v);
  x->RSSetState(d.rs);
  x->IASetInputLayout(nullptr);
  x->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  x->OMSetRenderTargets(1, &d.bb_rtv, nullptr);
  x->VSSetShader(d.vs_full, nullptr, 0);
  x->PSSetShader(d.ps_full, nullptr, 0);
  x->PSSetShaderResources(13, 1, &d.full.srv);
  x->Draw(3, 0);
  ID3D11ShaderResourceView *nv[1] = {nullptr};
  x->PSSetShaderResources(13, 1, nv);
  ts_end();
  d.course_valid = false;  // the course target was not drawn by this paint
}

// the back buffer's pixels (BGRA words, rows top-down) before Present
static bool read_back_buffer(ID3D11Device *dev, ID3D11DeviceContext *ctx, IDXGISwapChain2 *sc, ID3D11Texture2D *&stage, int32_t cw,
                             int32_t ch, std::vector<uint32_t> &out) {
  ID3D11Texture2D *bb = nullptr;
  if (FAILED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&bb)))) return false;
  if (!stage) {
    D3D11_TEXTURE2D_DESC td;
    bb->GetDesc(&td);
    td.Usage = D3D11_USAGE_STAGING;
    td.BindFlags = 0;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    td.MiscFlags = 0;
    if (FAILED(dev->CreateTexture2D(&td, nullptr, &stage))) {
      rel(bb);
      return false;
    }
  }
  ctx->CopyResource(stage, bb);
  rel(bb);
  D3D11_MAPPED_SUBRESOURCE m;
  if (FAILED(ctx->Map(stage, 0, D3D11_MAP_READ, 0, &m))) return false;
  out.resize(static_cast<size_t>(cw) * static_cast<size_t>(ch));
  for (int32_t y = 0; y < ch; ++y)
    std::memcpy(out.data() + static_cast<size_t>(y) * static_cast<size_t>(cw), static_cast<const uint8_t *>(m.pData) + static_cast<size_t>(y) * m.RowPitch,
                static_cast<size_t>(cw) * 4u);
  ctx->Unmap(stage, 0);
  return true;
}

// the previous course picture's rectangle (x0, y0) .. (x1, y1) read back into the presenter's scene_prev_ (client-size; the rest
// of it is not read by the render_rect calls that follow)
void GpuCompositor::fetch_prev(const Presenter &P) {
  Impl &d = *d_;
  const int32_t cw = d.w, ch = d.h;
  if (d.prev_fetch_gen == d.swap_gen && P.scene_prev_.size() == static_cast<size_t>(cw) * static_cast<size_t>(ch)) return;
  const int32_t x0 = 0, y0 = 0, x1 = cw, y1 = ch;
  if (!d.stage_u) {
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = static_cast<UINT>(cw);
    td.Height = static_cast<UINT>(ch);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R32_UINT;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_STAGING;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(d.dev->CreateTexture2D(&td, nullptr, &d.stage_u))) return;
  }
  const ProfScope prof(Phase::Upload);
  D3D11_BOX b = {static_cast<UINT>(x0), static_cast<UINT>(y0), 0, static_cast<UINT>(x1), static_cast<UINT>(y1), 1};
  d.ctx->CopySubresourceRegion(d.stage_u, 0, static_cast<UINT>(x0), static_cast<UINT>(y0), 0, d.prev.t, 0, &b);
  D3D11_MAPPED_SUBRESOURCE m;
  if (FAILED(d.ctx->Map(d.stage_u, 0, D3D11_MAP_READ, 0, &m))) return;
  P.scene_prev_.resize(static_cast<size_t>(cw) * static_cast<size_t>(ch));
  d.prev_fetch_gen = d.swap_gen;
  ++d.prev_fetches;
  for (int32_t y = y0; y < y1; ++y)
    std::memcpy(P.scene_prev_.data() + static_cast<size_t>(y) * static_cast<size_t>(cw) + static_cast<size_t>(x0),
                static_cast<const uint8_t *>(m.pData) + static_cast<size_t>(y) * m.RowPitch + static_cast<size_t>(x0) * 4u,
                static_cast<size_t>(x1 - x0) * 4u);
  d.ctx->Unmap(d.stage_u, 0);
}

// the A/B check (SG_P14_VERIFY=<n>): the CPU path at the same state against the back buffer
void GpuCompositor::verify(const Presenter &P, int32_t cw, int32_t ch, bool layered) {
  Impl &d = *d_;
  std::vector<uint32_t> gpu, cpu;
  if (!read_back_buffer(d.dev, d.ctx, d.sc, d.stage_bb, cw, ch, gpu)) return;
  // fill_gap's underlay: the GPU's previous course picture
  P.scene_prev_.clear();
  if (layered && d.prev_valid) {
    if (!d.stage_u) {
      D3D11_TEXTURE2D_DESC td = {};
      td.Width = static_cast<UINT>(cw);
      td.Height = static_cast<UINT>(ch);
      td.MipLevels = 1;
      td.ArraySize = 1;
      td.Format = DXGI_FORMAT_R32_UINT;
      td.SampleDesc.Count = 1;
      td.Usage = D3D11_USAGE_STAGING;
      td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      d.dev->CreateTexture2D(&td, nullptr, &d.stage_u);
    }
    if (d.stage_u) {
      d.ctx->CopyResource(d.stage_u, d.prev.t);
      D3D11_MAPPED_SUBRESOURCE m;
      if (SUCCEEDED(d.ctx->Map(d.stage_u, 0, D3D11_MAP_READ, 0, &m))) {
        P.scene_prev_.resize(static_cast<size_t>(cw) * static_cast<size_t>(ch));
        for (int32_t y = 0; y < ch; ++y)
          std::memcpy(P.scene_prev_.data() + static_cast<size_t>(y) * static_cast<size_t>(cw),
                      static_cast<const uint8_t *>(m.pData) + static_cast<size_t>(y) * m.RowPitch, static_cast<size_t>(cw) * 4u);
        d.ctx->Unmap(d.stage_u, 0);
      }
    }
  }
  d.prev_fetch_gen = ~0ull;  // scene_prev_ is cleared below
  const bool q = P.quiet_;
  P.quiet_ = true;
  P.render(cw, ch, cpu);
  P.quiet_ = q;
  if (P.overlay_mode_) P.draw_overlay(cpu, cw, ch);
  P.scene_prev_.clear();
  uint64_t bad = 0;
  int32_t fx = -1, fy = -1;
  for (size_t i = 0; i < gpu.size() && i < cpu.size(); ++i)
    if ((gpu[i] | 0xff000000u) != (cpu[i] | 0xff000000u)) {
      if (!bad) {
        fx = static_cast<int32_t>(i % static_cast<size_t>(cw));
        fy = static_cast<int32_t>(i / static_cast<size_t>(cw));
      }
      ++bad;
    }
  ++d.verified;
  if (bad) {
    ++d.verify_bad;
    d.verify_bad_px += bad;
    d.verify_max_px = std::max(d.verify_max_px, bad);
    const size_t i = static_cast<size_t>(fy) * static_cast<size_t>(cw) + static_cast<size_t>(fx);
    char b[200];
    std::snprintf(b, sizeof b, "paint %llu %s: %llu px differ, first (%d, %d) gpu %08x cpu %08x",
                  static_cast<unsigned long long>(d.paints_layered + d.paints_full), layered ? "layered" : "full",
                  static_cast<unsigned long long>(bad), fx, fy, gpu[i], cpu[i]);
    std::fprintf(stderr, "P14 VERIFY %s\n", b);
    if (d.verify_first.empty()) d.verify_first = b;
    if (!d.verify_dir.empty() && d.verify_saved < 3) {
      const std::string base = d.verify_dir + "/verify_" + std::to_string(d.verify_saved);
      std::vector<uint32_t> diff(gpu.size());
      for (size_t k = 0; k < gpu.size(); ++k) diff[k] = (gpu[k] | 0xff000000u) != (cpu[k] | 0xff000000u) ? 0xffff00ffu : (cpu[k] >> 2) & 0x003f3f3fu;
      write_bmp(base + "_gpu.bmp", gpu.data(), cw, ch);
      write_bmp(base + "_cpu.bmp", cpu.data(), cw, ch);
      write_bmp(base + "_diff.bmp", diff.data(), cw, ch);
      ++d.verify_saved;
    }
  }
}

void GpuCompositor::shot_if_asked(int32_t cw, int32_t ch) {
  Impl &d = *d_;
  if (d.shot_path.empty()) return;
  std::vector<uint32_t> img;
  if (read_back_buffer(d.dev, d.ctx, d.sc, d.stage_bb, cw, ch, img)) {
    for (uint32_t &v : img) v |= 0xff000000u;
    write_bmp(d.shot_path, img.data(), cw, ch);
    ++d.shots;
  }
  d.shot_path.clear();
}

void GpuCompositor::request_shot(const std::string &path) { d_->shot_path = path; }

bool GpuCompositor::wait_vblank() {
  Impl &d = *d_;
  std::lock_guard<std::mutex> lk(d.out_mu);
  return d.out && SUCCEEDED(d.out->WaitForVBlank());
}

// the GPU time of this paint's draws (not its uploads or the CPU's patches): timestamps around them, read one paint late
void GpuCompositor::ts_begin() {
  Impl &d = *d_;
  Impl::Ts &t = d.ts[d.ts_i];
  d.ts_on = t.dis && t.a && t.b && !t.pending && Profiler::get().enabled();
  if (!d.ts_on) return;
  d.ctx->Begin(t.dis);
  d.ctx->End(t.a);
}
void GpuCompositor::ts_end() {
  Impl &d = *d_;
  if (!d.ts_on) return;
  Impl::Ts &t = d.ts[d.ts_i];
  d.ctx->End(t.b);
  d.ctx->End(t.dis);
  t.pending = true;
  d.ts_on = false;
}

void GpuCompositor::paint(const Presenter &P, int32_t cw, int32_t ch, int32_t slx, int32_t sly) {
  Impl &d = *d_;
  if (!d.ok || cw <= 0 || ch <= 0) return;
  if (!ensure_size(cw, ch)) return;
  if (!d.ok) return;
  {
    const ProfScope prof(Phase::Present);  // the latency object: the paint starts after the last present left the queue
    const int64_t w0 = portsim::perf_now_ns();
    if (d.wait && !d.no_wait) WaitForSingleObjectEx(d.wait, 50, FALSE);
    d.ns_wait += portsim::perf_now_ns() - w0;
  }
  // slide_start swapped scene_prev_ / scene_last_ since the last paint: the course targets swap the same way
  if (P.swap_gen_ != d.swap_gen) {
    if ((P.swap_gen_ - d.swap_gen) & 1u) {
      std::swap(d.course, d.prev);
      d.prev_valid = d.course_valid;
      d.course_valid = false;
    }
    d.swap_gen = P.swap_gen_;
  }
  const WorldImage &W = P.world_;
  const bool layered = P.layered() && P.defect_ == PresentDefect::None && cw <= 4096 && ch <= 4096 && W.pw > 0 && W.ph > 0 &&
                       W.fw > 0 && W.pw <= 8192 && W.ph <= 8192;
  P.overlay_freeze(true);  // the paint's overlay patch and its A/B check show the same profiler line
  // fix 3: a screen without a world layer through the GPU path too (render's flat path)
  const bool flat = !layered && W.empty() && !P.frame_.empty() && !P.layered() && P.defect_ == PresentDefect::None && cw <= 4096 &&
                    ch <= 4096 && !d.no_flat;
  if (layered) paint_layered(P, cw, ch, slx, sly);
  else if (flat) paint_flat(P, cw, ch);
  else paint_full(P, cw, ch);
  if (d.verify_every > 0 && (d.verify_n++ % static_cast<uint64_t>(d.verify_every)) == 0) verify(P, cw, ch, layered);
  shot_if_asked(cw, ch);
  P.overlay_freeze(false);
  HRESULT hr;
  {
    const ProfScope prof(Phase::Present);
    const int64_t p0 = portsim::perf_now_ns();
    hr = d.sc->Present(d.sync_interval, 0);
    d.ns_present += portsim::perf_now_ns() - p0;
  }
  if (FAILED(hr)) {
    ++d.present_fail;
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) lose("Present", hr);
    return;
  }
  d.ts_i = (d.ts_i + 1) % 4;
  // the oldest timestamps that are ready
  for (Impl::Ts &o : d.ts) {
    if (!o.pending) continue;
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj;
    UINT64 a = 0, b = 0;
    if (d.ctx->GetData(o.dis, &dj, sizeof dj, D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) continue;
    if (d.ctx->GetData(o.a, &a, sizeof a, D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
        d.ctx->GetData(o.b, &b, sizeof b, D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
      continue;
    o.pending = false;
    if (dj.Disjoint || dj.Frequency == 0 || b < a) continue;
    const double ns = static_cast<double>(b - a) * 1e9 / static_cast<double>(dj.Frequency);
    prof_add(Phase::Gpu, static_cast<int64_t>(ns));
    d.gpu_ms_sum += ns * 1e-6;
    ++d.gpu_ms_n;
  }
}

std::string GpuCompositor::json() const {
  const Impl &d = *d_;
  auto esc = [](const std::string &s) {
    std::string o;
    for (char c : s) {
      if (c == '"' || c == '\\') o += '\\';
      if (static_cast<unsigned char>(c) >= 32) o += c;
    }
    return o;
  };
  char b[4096];
  std::snprintf(b, sizeof b,
                "{\"on\":%d,\"why\":\"%s\",\"paints_layered\":%llu,\"paints_full\":%llu,\"frame_uploads\":%llu,\"world_uploads\":%llu,"
                "\"course_builds\":%llu,\"course_px\":%llu,\"final_builds\":%llu,\"final_px\":%llu,\"gpu_ms_mean\":%.3f,"
                "\"losses\":%llu,\"restarts\":%llu,\"present_fail\":%llu,\"verified\":%llu,\"verify_bad\":%llu,\"verify_bad_px\":%llu,"
                "\"verify_max_px\":%llu,\"verify_first\":\"%s\",\"shots\":%llu,\"world_rect_uploads\":%llu,\"ms_frame_uploads\":%.1f,"
                "\"ms_world_uploads\":%.1f,\"ms_course_patches\":%.1f,\"ms_final_patches\":%.1f,\"ms_draw_calls\":%.1f,"
                "\"final_static_rects\":%llu,\"final_dep_rects\":%llu,\"final_rect_renders\":%llu,\"final_overflow\":%llu,"
                "\"prev_fetches\":%llu,\"shade_monotone\":%d,\"classify_off\":%llu,\"latency\":%d,\"ms_wait\":%.1f,\"ms_present\":%.1f,"
                "\"final_dep_renders\":%llu,\"final_dep_kept\":%llu,\"paints_flat\":%llu,"
                "\"p15_text\":%d,\"text_sets\":%llu,\"text_tables_made\":%llu,\"text_tables_reused\":%llu,\"text_runs_tabled\":%llu,"
                "\"text_table_draws\":%llu,\"text_entries\":%llu,\"ms_text_tables\":%.1f,\"text_cache\":%zu,\"ov_builds\":%llu,"
                "\"ov_patch_paints\":%llu,\"ov_table_draws\":%llu}",
                d.ok ? 1 : 0, esc(d.why).c_str(), static_cast<unsigned long long>(d.paints_layered),
                static_cast<unsigned long long>(d.paints_full), static_cast<unsigned long long>(d.frame_uploads),
                static_cast<unsigned long long>(d.world_uploads), static_cast<unsigned long long>(d.course_builds),
                static_cast<unsigned long long>(d.course_px), static_cast<unsigned long long>(d.final_builds),
                static_cast<unsigned long long>(d.final_px), d.gpu_ms_n ? d.gpu_ms_sum / static_cast<double>(d.gpu_ms_n) : 0.0,
                static_cast<unsigned long long>(d.losses), static_cast<unsigned long long>(d.restarts),
                static_cast<unsigned long long>(d.present_fail), static_cast<unsigned long long>(d.verified),
                static_cast<unsigned long long>(d.verify_bad), static_cast<unsigned long long>(d.verify_bad_px),
                static_cast<unsigned long long>(d.verify_max_px), esc(d.verify_first).c_str(), static_cast<unsigned long long>(d.shots),
                static_cast<unsigned long long>(d.world_rect_uploads), static_cast<double>(d.ns_frame) / 1e6,
                static_cast<double>(d.ns_world) / 1e6, static_cast<double>(d.ns_course) / 1e6, static_cast<double>(d.ns_final) / 1e6,
                static_cast<double>(d.ns_draw) / 1e6,
                static_cast<unsigned long long>(d.final_static_rects), static_cast<unsigned long long>(d.final_dep_rects),
                static_cast<unsigned long long>(d.final_rect_renders), static_cast<unsigned long long>(d.final_overflow),
                static_cast<unsigned long long>(d.prev_fetches), d.shade_mono, static_cast<unsigned long long>(d.classify_off), d.latency,
                static_cast<double>(d.ns_wait) / 1e6, static_cast<double>(d.ns_present) / 1e6,
                static_cast<unsigned long long>(d.final_dep_renders), static_cast<unsigned long long>(d.final_dep_kept),
                static_cast<unsigned long long>(d.paints_flat), d.p15 ? 1 : 0, static_cast<unsigned long long>(d.text_sets),
                static_cast<unsigned long long>(d.text_tables_made), static_cast<unsigned long long>(d.text_tables_reused),
                static_cast<unsigned long long>(d.text_runs_tabled),
                static_cast<unsigned long long>(d.traster ? d.traster->table_draws : 0ull),
                static_cast<unsigned long long>(d.text_entries), static_cast<double>(d.ns_text) / 1e6, d.tcache.size(),
                static_cast<unsigned long long>(d.ov_builds), static_cast<unsigned long long>(d.ov_patch_paints),
                static_cast<unsigned long long>(d.oraster ? d.oraster->table_draws : 0ull));
  return b;
}

// ---- the presenter's side (present.h)
bool Presenter::gpu_start(void *hwnd, std::string &why) {
  if (!gpu_) gpu_.reset(new GpuCompositor());
  if (gpu_->start(hwnd, why)) return true;
  return false;
}
bool Presenter::gpu_on() const { return gpu_ && gpu_->ok(); }
void Presenter::gpu_stop() {
  if (gpu_) gpu_->stop();
  prof_set_gpu(false);
}
std::string Presenter::gpu_json() const { return gpu_ ? gpu_->json() : std::string("{\"on\":0,\"why\":\"not started\"}"); }
void Presenter::gpu_shot(const std::string &path) {
  if (gpu_) gpu_->request_shot(path);
}
bool Presenter::gpu_wait_vblank() const { return gpu_ && gpu_->ok() && gpu_->wait_vblank(); }

void Presenter::paint_gpu(int32_t cw, int32_t ch) const {
  if (cw <= 0 || ch <= 0) return;
  const int64_t t0 = portsim::perf_now_ns();
  // render's slide decision (P12A fix 1) and its side effects on the kept scene, for the frames the GPU composes
  const bool keep = smooth_ && layered();
  if (!keep) {
    scene_ok_ = prev_ok_ = false;
    painted_cam_ = CameraSnap{};
  }
  int32_t slx = 0, sly = 0;
  if (keep && slide_on_ && prev_ok_ && prev_w_ == cw && prev_h_ == ch) {
    slx = cur_x_;
    sly = cur_y_;
  }
  gpu_->paint(*this, cw, ch, slx, sly);
  if (keep) {  // render's tail: the course picture of this paint is the next slide's underlay (the GPU's course target)
    scene_w_ = cw;
    scene_h_ = ch;
    painted_x_ = slx;
    painted_y_ = sly;
    painted_cam_ = world_.cam;
    scene_ok_ = true;
  }
  // P10E / P12A fix 1: the profiler's counts, as paint
  const bool fresh = frames_set_ != painted_set_;
  const bool cursor = !fresh && live_ != painted_live_ && live_.active;
  const bool slide = !fresh && (cur_x_ != painted_cur_x_ || cur_y_ != painted_cur_y_);
  Profiler::get().painted(fresh, portsim::perf_now_ns() - t0, cursor, slide, fresh || cursor || slide || changed_pending_);
  painted_set_ = frames_set_;
  painted_live_ = live_;
  painted_cur_x_ = cur_x_;
  painted_cur_y_ = cur_y_;
  changed_pending_ = false;
}

#else  // !_WIN32
void GpuCompositor::stop() {}
bool GpuCompositor::start(void *, std::string &why) {
  why = "not Windows";
  return false;
}
void GpuCompositor::paint(const Presenter &, int32_t, int32_t, int32_t, int32_t) {}
std::string GpuCompositor::json() const { return "{\"on\":0}"; }
void GpuCompositor::request_shot(const std::string &) {}
bool GpuCompositor::wait_vblank() { return false; }
bool Presenter::gpu_wait_vblank() const { return false; }
bool Presenter::gpu_start(void *, std::string &why) {
  why = "not Windows";
  return false;
}
bool Presenter::gpu_on() const { return false; }
void Presenter::gpu_stop() {}
std::string Presenter::gpu_json() const { return "{\"on\":0}"; }
void Presenter::gpu_shot(const std::string &) {}
#endif

}  // namespace portapp
