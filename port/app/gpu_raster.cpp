// P16B: the world terrain rasteriser's pixel loop on the GPU (gpu_raster.h; log/roadmap/P16/P16B_notes.md section 1).
#include "gpu_raster.h"

#include "portsim/sim/gl_raster.h"

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
#include <intrin.h>
#include <smmintrin.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <functional>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace portapp {

namespace {

// what the backends did (session.json p16b)
struct Stats {
  bool enabled = false;
  std::string state = "off";   // off / not started / on / the failure
  int64_t batches = 0, tris = 0, pairs = 0, groups = 0, dispatches = 0, uploads = 0, clears = 0, tex_uploads = 0, failures = 0;
  int64_t covered = 0, exact = 0, tex_bytes = 0, tex_count = 0;   // pixel writes, and those the exact path made (the rest: the checked path)
  int64_t bin_ns = 0, gpu_ns = 0, wait_ns = 0, read_ns = 0, upload_ns = 0, draw_ns = 0, max_draw_ns = 0, max_gpu_ns = 0;
  int64_t gpu_timed = 0;
  int64_t parts = 0, part_ns = 0, join_ns = 0, map_ns = 0, copy_ns = 0, span_ns = 0;   // P17A (D3DRaster::end_)
  int64_t streamed = 0;   // P17A fix 2: readbacks copied by streaming loads
  int64_t sub_rows = 0, pass_rows = 0;   // P17A fix 2: the submissions' row extents summed, and the passes' rows
  int slice = 0;
  bool force_exact = false;
  int mode = 0;            // P16B fix 2: 0 off, 1 on, 2 auto
  int64_t auto_off = 0;    // rasters the auto rule left on the CPU (1920 x 1200 pixels or fewer)
  int64_t auto_on = 0;     // rasters given a backend
  // the start-up self-test (selftest below)
  bool st_done = false, st_pass = false, st_corrupt = false;
  std::string st_result = "not run";
  int64_t st_words = 0, st_differing = 0, st_covered = 0, st_pass2 = 0, st_tris = 0;
  double st_device_ms = 0, st_test_ms = 0;
};
std::mutex g_mu;
Stats g_st;

#ifdef _WIN32
template <class T>
void rel(T *&p) {
  if (p) {
    p->Release();
    p = nullptr;
  }
}

// ---- the pixel loop: GlRaster::fast_rows_ (gl_raster.cpp) per pixel, one thread group per 16 x 16 tile, one thread per pixel walking
// the tile's triangles in their order, so every pixel gets GlRaster's writes in GlRaster's order.
// COVERAGE (exact, integers): fast_rows_'s edge function w = K - B * PX (int64, units of 2^-16) at pixel (x0 + dx, y0 + dy) of the tile
// whose first pixel is (x0, y0) is W0 + 256 * (A * dy - B * dx), W0 = w at (x0, y0). The CPU computes per (tile, triangle) pair and
// edge W0 and Tc = ceil((t - W0) / 256), t = 0 on an owning edge, else 1; then w >= t <=> D = A * dy - B * dx >= Tc (D below 2^28 in
// magnitude: int32; Tc clamped to +-2^30, beyond which the answer is the same). The weights a = w * 2^-16 = W0 * 2^-16 + D * 2^-8 are
// exact doubles (fast_rows_'s a0..a2).
// THE EXACT PATH (exactpx): fast_rows_'s exact branch, every double operation in the CPU's order as a separately rounded `precise
// double` (D3DCOMPILE_IEEE_STRICTNESS: no fused multiply-add, no reassociation), division = DDIV (accurate to 0.5 ULP: correctly
// rounded, as the CPU's divsd).
// THE CHECKED PATH (fastpx; GlRaster's checked reciprocal path carried into float32): a pixel is written from it only when every
// rounding decision is farther from its boundary than the two computations can differ; any other pixel takes the exact path.
//   - texture coordinates: double, the divisions by the area replaced by products with the CPU's reciprocal; with |u|, |v| < 2^16 they
//     differ from the CPU's by less than 1e-10 (fast_triangle_'s argument), so the texel indices are equal when the fractions are
//     farther than 1e-9 from 0 and 1 (GlRaster's kTexEps), and ax, ay differ by less than 1e-10;
//   - the filter, the colour, modulate, blend and the quantiser value q = c * 31 / 255 + d in float32. Bound of |q_float - q_cpu|,
//     every float operation counted at 1 ulp (2^-23 relative; fused or not), the CPU's double rounding below 1e-12: filter weights
//     4.8e-7, filtered channel 7.0e-4, colour (vertex colours in [0, 1], the CPU's own condition) 1.5e-6, modulated channel 1.11e-3,
//     blend alpha 3.0e-6 and blended channel 2.77e-3, q 1.44e-4 unblended / 3.5e-4 blended. The pixel is kept when every channel's q
//     is farther than kEq = 3e-4 / kEqB = 7e-4 (about twice the bound) from an integer: floor(q) is then the CPU's.
// TWO PASSES: pass 1 (CSRaster) runs the checked path only (no double division, few registers); a pixel with a write it cannot
// decide is handed to pass 2 (CSResume, a thread per handed pixel) with the pair index and its value there, and pass 2 finishes that
// pixel in order with the exact path where needed. Every pixel still gets every write in triangle order.
// SG_P16_EXACT=1 (development) hands every covered pixel to pass 2 at its first write (the exact path everywhere).
const char kHlslRaster[] = R"HLSL(
#ifndef FORCE_EXACT
#define FORCE_EXACT 0
#endif
#ifndef PAIRS_NEGCTL
#define PAIRS_NEGCTL 0
#endif
cbuffer CB : register(b0) {
  int W, H, tilesX, tileBase;
  uint listCap, cb1, cb2, cb3;
};
struct Tri {
  int A0, A1, A2, B0, B1, B2;   // edge i (P = (i + 1) % 3 to Q = (i + 2) % 3): A = XQ - XP, B = YQ - YP (units of 2^-8)
  uint flags, texoff, tw, th, bpp, fast;   // fast: the checked path may be tried (colours in [0, 1], finite areas)
  float cf[9];                  // r0 r1 r2 g0 g1 g2 b0 b1 b2 as floats
  float inv_af;                 // float(1 / arA)
  uint2 d[19];                  // s0 s1 s2 t0 t1 t2 r0 r1 r2 g0 g1 g2 b0 b1 b2 div_st arA 1/div_st 1/arA (the CPU's doubles)
  uint2 K[3];                   // P17A: per edge B * XP - A * YP (double, exact: below 2^47), for CSPairs
  int minx, maxx, miny, maxy;   // P17A: the box (frame pixels), for CSPairs
};
struct Pair {
  uint tri, box;                // box: the triangle's box in tile pixels, x0 | x1 << 8 | y0 << 16 | y1 << 24
  int Tc0, Tc1, Tc2;
  uint pad;
  uint2 W0[3];                  // w at the tile's first pixel (double, exact)
};
StructuredBuffer<Tri> tris : register(t0);
StructuredBuffer<uint2> tileRange : register(t1);   // per tile from tile cb2 on: first pair, count
StructuredBuffer<Pair> pairs : register(t2);
StructuredBuffer<uint> tileList : register(t3);     // the tiles with pairs
ByteAddressBuffer texels : register(t4);
StructuredBuffer<uint2> kc : register(t5);          // [0..31] d * 255.0 / 31.0, [32..47] the dither offsets (double)
Texture2D<uint> src : register(t6);
StructuredBuffer<float> kcf : register(t7);         // the same as floats
RWTexture2D<uint> dst : register(u0);
RWByteAddressBuffer cnt : register(u1);             // [0] pixel writes of pass 1, [4] pixels handed to pass 2, [8] list overflow
RWStructuredBuffer<uint2> lst : register(u2);       // pass 2's pixels: x | y << 16, the pair index << 15 | the pixel's value
RWByteAddressBuffer args : register(u3);            // pass 2's DispatchIndirect arguments
RWByteAddressBuffer acc : register(u4);             // P17A: the pass's counters summed over its parts (CSAcc)
RWStructuredBuffer<Pair> pairsOut : register(u5);   // P17A: the pairs CSPairs fills in (then read as pairs, t2)
StructuredBuffer<uint2> cpairs : register(t8);      // P17A: per pair the triangle and the tile (the CPU's binning)
groupshared uint g_cov;

double D(uint2 v) { return asdouble(v.x, v.y); }
double TD(uint ti, uint k) { return asdouble(tris[ti].d[k].x, tris[ti].d[k].y); }
// static_cast<int>(std::floor(v)) for |v| < 2^31 (the CPU's set-up guard keeps every value here inside it)
int floor_i(double v) {
  int i = (int)v;
  return (v < (double)i) ? i - 1 : i;
}
// gl_raster.cpp wrap_repeat / wrap_fast
uint wrapi(int i, uint n) {
  if (i >= 0) return ((uint)i < n) ? (uint)i : (uint)i % n;
  uint m = ((uint)(-i)) % n;
  return m == 0u ? 0u : n - m;
}
// the texel's bytes at byte offset o (r, g, b, a; a = byte o + 3, read only for 4-byte texels): one 8-byte load
uint4 texel(uint o) {
  uint2 w = texels.Load2(o & ~3u);
  uint sh = (o & 3u) * 8u;
  uint lo = sh == 0u ? w.x : ((w.x >> sh) | (w.y << (32u - sh)));
  return uint4(lo & 0xffu, (lo >> 8) & 0xffu, (lo >> 16) & 0xffu, lo >> 24);
}
double lin3(double a0, double a1, double a2, double v0, double v1, double v2) {
  precise double p0 = a0 * v0;
  precise double p1 = a1 * v1;
  precise double p2 = a2 * v2;
  precise double s = p0 + p1;
  precise double r = s + p2;
  return r;
}
uint quant(double c, double d) {   // quantize_d: floor(c * 31.0 / 255.0 + d), clamped to 0..31
  precise double m = c * 31.0;
  precise double q = m / 255.0;
  precise double v = q + d;
  int o = floor_i(v);
  return (uint)clamp(o, 0, 31);
}
double bil(double w00, double w01, double w10, double w11, uint p00, uint p01, uint p10, uint p11) {
  precise double acc = w00 * (double)p00;
  precise double q1 = w01 * (double)p01;
  acc = acc + q1;
  precise double q2 = w10 * (double)p10;
  acc = acc + q2;
  precise double q3 = w11 * (double)p11;
  acc = acc + q3;
  return acc;
}
// the weights a0..a2 of pair P's triangle at tile pixel (dx, dy) (exact: W0 * 2^-16 + D * 2^-8); false when not covered
bool cover(Pair P, int dx, int dy, out double a0, out double a1, out double a2) {
  a0 = 0; a1 = 0; a2 = 0;
  int bx0 = (int)(P.box & 255u), bx1 = (int)((P.box >> 8) & 255u), by0 = (int)((P.box >> 16) & 255u), by1 = (int)(P.box >> 24);
  if (dx < bx0 || dx > bx1 || dy < by0 || dy > by1) return false;
  uint ti = P.tri;
  int D0 = tris[ti].A0 * dy - tris[ti].B0 * dx, D1 = tris[ti].A1 * dy - tris[ti].B1 * dx, D2 = tris[ti].A2 * dy - tris[ti].B2 * dx;
  if (D0 < P.Tc0 || D1 < P.Tc1 || D2 < P.Tc2) return false;
  double u16 = asdouble(0u, 0x3EF00000u);   // 2^-16
  double u8 = asdouble(0u, 0x3F700000u);    // 2^-8
  precise double h0 = D(P.W0[0]) * u16;
  precise double l0 = (double)D0 * u8;
  a0 = h0 + l0;
  precise double h1 = D(P.W0[1]) * u16;
  precise double l1 = (double)D1 * u8;
  a1 = h1 + l1;
  precise double h2 = D(P.W0[2]) * u16;
  precise double l2 = (double)D2 * u8;
  a2 = h2 + l2;
  return true;
}
// fast_rows_'s exact branch for one pixel
uint exactpx(uint ti, double a0, double a1, double a2, int x, int y, uint px) {
  uint flags = tris[ti].flags, texoff = tris[ti].texoff, tw = tris[ti].tw, th = tris[ti].th, bpp = tris[ti].bpp;
  double div_st = TD(ti, 15), arA = TD(ti, 16);
  precise double sn = lin3(a0, a1, a2, TD(ti, 0), TD(ti, 1), TD(ti, 2));
  precise double s = sn / div_st;
  precise double tn = lin3(a0, a1, a2, TD(ti, 3), TD(ti, 4), TD(ti, 5));
  precise double t = tn / div_st;
  precise double u = s * (double)tw;
  precise double v = t * (double)th;
  double r, g, bl, al = 255.0;
  if ((flags & 8u) == 0u) {   // GL_NEAREST
    uint iu = wrapi(floor_i(u), tw), iv = wrapi(floor_i(v), th);
    uint4 p = texel(texoff + (iv * tw + iu) * bpp);
    r = (double)p.x; g = (double)p.y; bl = (double)p.z;
    if (bpp == 4u) al = (double)p.w;
  } else {   // GL_LINEAR
    precise double fu = u - 0.5;
    precise double fv = v - 0.5;
    int i0 = floor_i(fu), j0 = floor_i(fv);
    precise double ax = fu - (double)i0;
    precise double ay = fv - (double)j0;
    uint x0 = wrapi(i0, tw), x1 = wrapi(i0 + 1, tw);
    uint ry0 = wrapi(j0, th) * tw, ry1 = wrapi(j0 + 1, th) * tw;
    precise double omx = 1.0 - ax;
    precise double omy = 1.0 - ay;
    precise double w00 = omx * omy;
    precise double w01 = ax * omy;
    precise double w10 = omx * ay;
    precise double w11 = ax * ay;
    uint4 p00 = texel(texoff + (ry0 + x0) * bpp), p01 = texel(texoff + (ry0 + x1) * bpp);
    uint4 p10 = texel(texoff + (ry1 + x0) * bpp), p11 = texel(texoff + (ry1 + x1) * bpp);
    r = bil(w00, w01, w10, w11, p00.x, p01.x, p10.x, p11.x);
    g = bil(w00, w01, w10, w11, p00.y, p01.y, p10.y, p11.y);
    bl = bil(w00, w01, w10, w11, p00.z, p01.z, p10.z, p11.z);
    if (bpp == 4u) al = bil(w00, w01, w10, w11, p00.w, p01.w, p10.w, p11.w);
  }
  if ((flags & 16u) != 0u) {   // GL_MODULATE
    precise double crn = lin3(a0, a1, a2, TD(ti, 6), TD(ti, 7), TD(ti, 8));
    precise double cgn = lin3(a0, a1, a2, TD(ti, 9), TD(ti, 10), TD(ti, 11));
    precise double cbn = lin3(a0, a1, a2, TD(ti, 12), TD(ti, 13), TD(ti, 14));
    precise double cr = crn / arA;
    precise double cg = cgn / arA;
    precise double cb = cbn / arA;
    precise double mr = r * cr;
    precise double mg = g * cg;
    precise double mb = bl * cb;
    r = mr; g = mg; bl = mb;
  }
  if ((flags & 32u) != 0u) {   // GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA over the 5-bit destination
    precise double alpha = al / 255.0;
    precise double oma = 1.0 - alpha;
    precise double pr = r * alpha;
    precise double er = D(kc[(px >> 10) & 31u]) * oma;
    precise double ro = pr + er;
    precise double pg = g * alpha;
    precise double eg = D(kc[(px >> 5) & 31u]) * oma;
    precise double go = pg + eg;
    precise double pb = bl * alpha;
    precise double eb = D(kc[px & 31u]) * oma;
    precise double bo = pb + eb;
    r = ro; g = go; bl = bo;
  }
  double dd = D(kc[32u + (uint)((y & 3) * 4 + ((x + (int)((flags >> 8) & 3u)) & 3))]);
  return (quant(r, dd) << 10) | (quant(g, dd) << 5) | quant(bl, dd);
}
// floor(q) when q is farther than e from an integer (else -1000: undecided)
int qdecide(float q, float e) {
  float f = floor(q);
  float fr = q - f;
  return (fr > e && fr < 1.0f - e) ? clamp((int)f, 0, 31) : -1000;
}
// the checked path: true when px was written with the exact path's value (bounds in the comment above kHlslRaster)
bool fastpx(uint ti, double a0, double a1, double a2, int x, int y, inout uint px) {
  uint flags = tris[ti].flags, texoff = tris[ti].texoff, tw = tris[ti].tw, th = tris[ti].th, bpp = tris[ti].bpp;
  if (tris[ti].fast == 0u) return false;
  double inv_st = TD(ti, 17);
  precise double sn = lin3(a0, a1, a2, TD(ti, 0), TD(ti, 1), TD(ti, 2));
  precise double s = sn * inv_st;
  precise double tn = lin3(a0, a1, a2, TD(ti, 3), TD(ti, 4), TD(ti, 5));
  precise double t = tn * inv_st;
  precise double u = s * (double)tw;
  precise double v = t * (double)th;
  if (!(u < 65536.0 && u > -65536.0 && v < 65536.0 && v > -65536.0)) return false;
  float r, g, bl, al = 255.0f;
  double fr_u, fr_v;
  if ((flags & 8u) == 0u) {   // GL_NEAREST
    int iu0 = floor_i(u), iv0 = floor_i(v);
    fr_u = u - (double)iu0;
    fr_v = v - (double)iv0;
    uint4 p = texel(texoff + (wrapi(iv0, th) * tw + wrapi(iu0, tw)) * bpp);
    r = (float)p.x; g = (float)p.y; bl = (float)p.z;
    if (bpp == 4u) al = (float)p.w;
  } else {   // GL_LINEAR
    double fu = u - 0.5, fv = v - 0.5;
    int i0 = floor_i(fu), j0 = floor_i(fv);
    double ax = fu - (double)i0, ay = fv - (double)j0;
    fr_u = ax;
    fr_v = ay;
    uint x0 = wrapi(i0, tw), x1 = wrapi(i0 + 1, tw);
    uint ry0 = wrapi(j0, th) * tw, ry1 = wrapi(j0 + 1, th) * tw;
    float axf = (float)ax, ayf = (float)ay;
    float omx = 1.0f - axf, omy = 1.0f - ayf;
    float w00 = omx * omy, w01 = axf * omy, w10 = omx * ayf, w11 = axf * ayf;
    float4 p00 = (float4)texel(texoff + (ry0 + x0) * bpp), p01 = (float4)texel(texoff + (ry0 + x1) * bpp);
    float4 p10 = (float4)texel(texoff + (ry1 + x0) * bpp), p11 = (float4)texel(texoff + (ry1 + x1) * bpp);
    float4 c = w00 * p00 + w01 * p01 + w10 * p10 + w11 * p11;
    r = c.x; g = c.y; bl = c.z;
    if (bpp == 4u) al = c.w;
  }
  if (!(fr_u > 1e-9 && fr_u < 1.0 - 1e-9 && fr_v > 1e-9 && fr_v < 1.0 - 1e-9)) return false;
  if ((flags & 16u) != 0u) {   // GL_MODULATE
    float ia = tris[ti].inv_af;
    float b0 = (float)a0 * ia, b1 = (float)a1 * ia, b2 = (float)a2 * ia;
    r *= b0 * tris[ti].cf[0] + b1 * tris[ti].cf[1] + b2 * tris[ti].cf[2];
    g *= b0 * tris[ti].cf[3] + b1 * tris[ti].cf[4] + b2 * tris[ti].cf[5];
    bl *= b0 * tris[ti].cf[6] + b1 * tris[ti].cf[7] + b2 * tris[ti].cf[8];
  }
  bool blend = (flags & 32u) != 0u;
  if (blend) {
    float alpha = al * (1.0f / 255.0f), oma = 1.0f - alpha;
    r = r * alpha + kcf[(px >> 10) & 31u] * oma;
    g = g * alpha + kcf[(px >> 5) & 31u] * oma;
    bl = bl * alpha + kcf[px & 31u] * oma;
  }
  float d = kcf[32u + (uint)((y & 3) * 4 + ((x + (int)((flags >> 8) & 3u)) & 3))];
  float e = blend ? 7e-4f : 3e-4f;
  const float k31 = 31.0f / 255.0f;
  int orr = qdecide(r * k31 + d, e), og = qdecide(g * k31 + d, e), ob = qdecide(bl * k31 + d, e);
  if (orr < 0 || og < 0 || ob < 0) return false;
  px = ((uint)orr << 10) | ((uint)og << 5) | (uint)ob;
  return true;
}

// PASS 1: every pixel of a tile with pairs; the checked path only. A pixel whose write the checked path cannot decide is handed to
// pass 2 with the pair index and its value at that point, and pass 1 stops on it.
[numthreads(16, 16, 1)]
void CSRaster(uint3 gid : SV_GroupID, uint3 lt : SV_GroupThreadID, uint li : SV_GroupIndex) {
  if (li == 0u) g_cov = 0u;
  GroupMemoryBarrierWithGroupSync();
  uint tile = tileList[(uint)tileBase + gid.x];
  int x = (int)((tile % (uint)tilesX) * 16u + lt.x);
  int y = (int)((tile / (uint)tilesX) * 16u + lt.y);
  int dx = (int)lt.x, dy = (int)lt.y;
  bool inside = x < W && y < H;
  uint px = inside ? src[int2(x, y)] : 0u;
  uint2 rg = tileRange[tile - cb2];   // P17A: the table starts at the submission's first tile row (cb2)
  uint ncov = 0u;
  for (uint k = 0u; inside && k < rg.y; ++k) {
    Pair P = pairs[rg.x + k];
    double a0, a1, a2;
    if (!cover(P, dx, dy, a0, a1, a2)) continue;
    ++ncov;
    bool done = false;
#if !FORCE_EXACT
    done = fastpx(P.tri, a0, a1, a2, x, y, px);
#endif
    if (!done) {
      uint i;
      cnt.InterlockedAdd(4, 1u, i);
      if (i < listCap && k < 131072u) lst[i] = uint2((uint)x | ((uint)y << 16), (k << 15) | px);
      else cnt.InterlockedOr(8, 1u);
      break;
    }
  }
  if (inside) dst[int2(x, y)] = px;
  if (ncov != 0u) InterlockedAdd(g_cov, ncov);
  GroupMemoryBarrierWithGroupSync();
  if (li == 0u && g_cov != 0u) cnt.InterlockedAdd(0, g_cov);
}

// pass 2's dispatch size from pass 1's count (64 pixels per group)
[numthreads(1, 1, 1)]
void CSArgs() {
  uint n = min(cnt.Load(4), listCap);
  args.Store3(0, uint3((n + 63u) / 64u, 1u, 1u));
}

// PASS 2: each handed pixel resumes at its pair (that write by the exact path), then the rest of its tile's pairs as pass 1 does,
// with the exact path wherever the checked path cannot decide
[numthreads(64, 1, 1)]
void CSResume(uint3 tid : SV_DispatchThreadID) {
  uint n = min(cnt.Load(4), listCap);
  if (tid.x >= n) return;
  uint2 e = lst[tid.x];
  int x = (int)(e.x & 0xffffu), y = (int)(e.x >> 16);
  uint k0 = e.y >> 15, px = e.y & 0x7fffu;
  uint tile = (uint)(y >> 4) * (uint)tilesX + (uint)(x >> 4);
  int dx = x & 15, dy = y & 15;
  uint2 rg = tileRange[tile - cb2];   // P17A: the table starts at the submission's first tile row (cb2)
  for (uint k = k0; k < rg.y; ++k) {
    Pair P = pairs[rg.x + k];
    double a0, a1, a2;
    if (!cover(P, dx, dy, a0, a1, a2)) continue;
    bool done = false;
#if !FORCE_EXACT
    if (k != k0) done = fastpx(P.tri, a0, a1, a2, x, y, px);
#endif
    if (!done) px = exactpx(P.tri, a0, a1, a2, x, y, px);
  }
  dst[int2(x, y)] = px;
}

)HLSL"   // P17A: two literals (MSVC limits one to 16 KB; adjacent literals are joined)
                           R"HLSL(
// P17A: THE PAIRS BUILT ON THE GPU. The CPU bins (which tiles, in triangle order) and uploads per pair only (triangle, tile); a thread per
// pair fills in what P16B's CPU binning computed: the triangle's box in tile pixels and per edge W0 = A * (PY0 - YP) + B * XP - B * PX0
// = A * PY0 - B * PX0 + K (K = B * XP - A * YP from the CPU; every term an integer below 2^48, so each double operation is exact and W0
// is the CPU's int64 value) and Tc = ceil((t - W0) / 256) clamped to +-2^30 ((t - W0) * 2^-8 is exact; clamping before the ceiling gives
// the same integer). cb1 = the pair count.
[numthreads(64, 1, 1)]
void CSPairs(uint3 tid : SV_DispatchThreadID) {
  if (tid.x >= cb1) return;
  uint2 cp = cpairs[tid.x];
  uint ti = cp.x;
  int x0 = (int)((cp.y % (uint)tilesX) * 16u), y0 = (int)((cp.y / (uint)tilesX) * 16u);
  Pair P;
  P.tri = ti;
  uint bx0 = (uint)max(tris[ti].minx - x0, 0), bx1 = (uint)min(tris[ti].maxx - x0, 15);
  uint by0 = (uint)max(tris[ti].miny - y0, 0), by1 = (uint)min(tris[ti].maxy - y0, 15);
  P.box = bx0 | (bx1 << 8) | (by0 << 16) | (by1 << 24);
  P.pad = 0u;
  double PX0 = (double)(256 * x0 + 128), PY0 = (double)(256 * y0 + 128);
  double u8 = asdouble(0u, 0x3F700000u);   // 2^-8
  int tc[3];
  [unroll] for (int e = 0; e < 3; ++e) {
    int A = e == 0 ? tris[ti].A0 : (e == 1 ? tris[ti].A1 : tris[ti].A2);
    int B = e == 0 ? tris[ti].B0 : (e == 1 ? tris[ti].B1 : tris[ti].B2);
    precise double ay = (double)A * PY0;
    precise double bx = (double)B * PX0;
    precise double d0 = ay - bx;
    precise double w0 = d0 + D(tris[ti].K[e]);
    double t = ((tris[ti].flags >> (uint)e) & 1u) != 0u ? 0.0 : 1.0;
    precise double n = t - w0;
    precise double q = n * u8;
    q = clamp(q, -1073741824.0, 1073741824.0);
    int i = (int)q;
    tc[e] = ((double)i < q) ? i + 1 : i;
    uint lo, hi;
    asuint(w0, lo, hi);
    P.W0[e] = uint2(lo, hi);
  }
  P.Tc0 = tc[0] + PAIRS_NEGCTL;   // SG_P17_PAIRS_NEGCTL=1 (development): edge 0's threshold one too high (the checks must read RED)
  P.Tc1 = tc[1];
  P.Tc2 = tc[2];
  pairsOut[tid.x] = P;
}

// P17A: one part's counters added to the pass's (pixel writes and handed pixels summed, the overflow flag kept)
[numthreads(1, 1, 1)]
void CSAcc() {
  acc.Store3(0, uint3(acc.Load(0) + cnt.Load(0), acc.Load(4) + cnt.Load(4), acc.Load(8) | cnt.Load(8)));
}
)HLSL";

// the GPU's copy of one triangle and one (tile, triangle) pair (kHlslRaster's Tri and Pair)
struct TriG {
  int32_t A[3], B[3];
  uint32_t flags, texoff, tw, th, bpp, fast;
  float cf[9];
  float inv_af;
  uint32_t d[38];
  uint32_t K[6];        // P17A: per edge B * XP - A * YP as a double (CSPairs)
  int32_t box[4];       // P17A: minx, maxx, miny, maxy
};
static_assert(sizeof(TriG) == 280, "TriG layout");
struct PairG {
  uint32_t tri, box;
  int32_t Tc[3];
  uint32_t pad;
  double W0[3];
};
static_assert(sizeof(PairG) == 48, "PairG layout");
struct CBData {
  int32_t W, H, tilesX, tileBase;
  uint32_t listCap, cb1, cb2, cb3;
};

// floor(a / b) / ceil(a / b) for b > 0 (gl_raster.cpp floor_div / ceil_div)
int64_t ceil_div(int64_t a, int64_t b) {
  const int64_t q = a / b;
  return (a % b != 0 && a > 0) ? q + 1 : q;
}

// P17A: THE SUBMIT THREAD. The parts of a batch (GlPixelGpu::part) are binned and dispatched here while the drawing thread (the world
// thread) goes on with the set-up; the draw waits for it to be idle and then uses the device's context itself, so the context is only
// ever used by one thread at a time (the wait orders them). It runs at the priority of the thread that made it.
struct Worker {
  std::thread th;
  std::mutex m;
  std::condition_variable cv, idle;
  std::deque<std::function<void()>> q;
  bool busy = false, stop = false;
  Worker() {
#ifdef _WIN32
    const int prio = GetThreadPriority(GetCurrentThread());
#endif
    th = std::thread([this] {
      for (;;) {
        std::function<void()> f;
        {
          std::unique_lock<std::mutex> lk(m);
          cv.wait(lk, [this] { return stop || !q.empty(); });
          if (q.empty()) return;
          f = std::move(q.front());
          q.pop_front();
          busy = true;
        }
        f();
        std::lock_guard<std::mutex> lk(m);
        busy = false;
        if (q.empty()) idle.notify_all();
      }
    });
#ifdef _WIN32
    if (prio != THREAD_PRIORITY_ERROR_RETURN) SetThreadPriority(th.native_handle(), prio);
#endif
  }
  ~Worker() {
    {
      std::lock_guard<std::mutex> lk(m);
      stop = true;
    }
    cv.notify_all();
    th.join();   // the queue is run to its end first
  }
  void post(std::function<void()> f) {
    std::lock_guard<std::mutex> lk(m);
    q.push_back(std::move(f));
    cv.notify_one();
  }
  void wait() {
    std::unique_lock<std::mutex> lk(m);
    idle.wait(lk, [this] { return q.empty() && !busy; });
  }
};
// P17A: THE READBACK COPY on n threads (the caller is part 0): fn(k, n) for k = 0..n-1, returning when every part is done
struct Copier {
  std::vector<std::thread> th;
  std::mutex m;
  std::condition_variable cv, done;
  std::function<void(int, int)> fn;
  uint64_t gen = 0;
  int pending = 0, n = 1;
  bool stop = false;
  explicit Copier(int threads) : n(std::max(1, threads)) {
#ifdef _WIN32
    const int prio = GetThreadPriority(GetCurrentThread());
#endif
    for (int k = 1; k < n; ++k) {
      th.emplace_back([this, k] {
        uint64_t seen = 0;
        for (;;) {
          std::function<void(int, int)> f;
          {
            std::unique_lock<std::mutex> lk(m);
            cv.wait(lk, [&] { return stop || gen != seen; });
            if (stop) return;
            seen = gen;
            f = fn;
          }
          f(k, n);
          std::lock_guard<std::mutex> lk(m);
          if (--pending == 0) done.notify_all();
        }
      });
#ifdef _WIN32
      if (prio != THREAD_PRIORITY_ERROR_RETURN) SetThreadPriority(th.back().native_handle(), prio);
#endif
    }
  }
  ~Copier() {
    {
      std::lock_guard<std::mutex> lk(m);
      stop = true;
    }
    cv.notify_all();
    for (std::thread &t : th) t.join();
  }
  void run(const std::function<void(int, int)> &f) {
    if (n == 1) {
      f(0, 1);
      return;
    }
    {
      std::lock_guard<std::mutex> lk(m);
      fn = f;
      pending = n - 1;
      ++gen;
    }
    cv.notify_all();
    f(0, n);
    std::unique_lock<std::mutex> lk(m);
    done.wait(lk, [this] { return pending == 0; });
  }
};

// the device every backend of the process shares (made by the first draw; one thread draws: the world thread)
struct Dev {
  bool tried = false, ok = false;
  std::string why = "not started";
  ID3D11Device *dev = nullptr;
  ID3D11DeviceContext *ctx = nullptr;
  ID3D11ComputeShader *cs = nullptr, *cs_args = nullptr, *cs_resume = nullptr, *cs_acc = nullptr, *cs_pairs = nullptr;
  ID3D11Buffer *args = nullptr;
  ID3D11UnorderedAccessView *args_uav = nullptr;
  ID3D11Buffer *cb = nullptr;
  ID3D11Buffer *kc = nullptr, *kcf = nullptr;
  ID3D11ShaderResourceView *kc_srv = nullptr, *kcf_srv = nullptr;
  ID3D11Buffer *cnt = nullptr, *cnt_rd = nullptr, *acc = nullptr;
  ID3D11UnorderedAccessView *cnt_uav = nullptr, *acc_uav = nullptr;
  ID3D11Query *dis = nullptr, *ta = nullptr, *tb = nullptr;
  bool force_exact = false;
  std::unique_ptr<Worker> worker;   // P17A: made by the first part, by the drawing thread
  std::unique_ptr<Copier> copier;   // P17A: made by the first readback
  ~Dev() {
    worker.reset();
    copier.reset();
    rel(acc_uav); rel(acc); rel(cs_acc); rel(cs_pairs);
    rel(args_uav); rel(args); rel(cs_resume); rel(cs_args);
    rel(ta); rel(tb); rel(dis); rel(cnt_uav); rel(cnt_rd); rel(cnt); rel(kcf_srv); rel(kcf); rel(kc_srv); rel(kc); rel(cb); rel(cs);
    rel(ctx); rel(dev);
  }
  bool start() {
    if (tried) return ok;
    tried = true;
    HMODULE dc = LoadLibraryW(L"d3dcompiler_47.dll");
    if (!dc) return fail("no d3dcompiler_47.dll");
    auto compile = reinterpret_cast<pD3DCompile>(reinterpret_cast<void *>(GetProcAddress(dc, "D3DCompile")));
    if (!compile) return fail("no D3DCompile");
    const D3D_FEATURE_LEVEL want[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL got = D3D_FEATURE_LEVEL_10_0;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, want, 2, D3D11_SDK_VERSION, &dev, &got, &ctx);
    if (FAILED(hr)) hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, want + 1, 1, D3D11_SDK_VERSION, &dev, &got, &ctx);
    if (FAILED(hr) || !dev) return fail("no D3D11 hardware device at feature level 11");
    D3D11_FEATURE_DATA_DOUBLES fd = {};
    D3D11_FEATURE_DATA_D3D11_OPTIONS fo = {};
    UINT fs = 0;
    if (!(SUCCEEDED(dev->CheckFeatureSupport(D3D11_FEATURE_DOUBLES, &fd, sizeof fd)) && fd.DoublePrecisionFloatShaderOps) ||
        !(SUCCEEDED(dev->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &fo, sizeof fo)) && fo.ExtendedDoublesShaderInstructions))
      return fail("no extended double-precision shader ops");
    if (!(SUCCEEDED(dev->CheckFormatSupport(DXGI_FORMAT_R16_UINT, &fs)) && (fs & D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW)))
      return fail("no typed UAV of R16_UINT");
    const char *fe = std::getenv("SG_P16_EXACT");
    force_exact = fe && fe[0] == '1';
    const char *pn = std::getenv("SG_P17_PAIRS_NEGCTL");
    const bool pairs_negctl = pn && pn[0] == '1';
    const D3D_SHADER_MACRO defs[] = {{"FORCE_EXACT", force_exact ? "1" : "0"}, {"PAIRS_NEGCTL", pairs_negctl ? "1" : "0"}, {nullptr, nullptr}};
    const char *entry[5] = {"CSRaster", "CSArgs", "CSResume", "CSAcc", "CSPairs"};
    ID3D11ComputeShader **out[5] = {&cs, &cs_args, &cs_resume, &cs_acc, &cs_pairs};
    for (int i = 0; i < 5; ++i) {
      ID3DBlob *b = nullptr, *err = nullptr;
      hr = compile(kHlslRaster, sizeof(kHlslRaster) - 1, "p16b.hlsl", defs, nullptr, entry[i], "cs_5_0",
                   D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_IEEE_STRICTNESS, 0, &b, &err);
      if (FAILED(hr) || !b) {
        std::string w = std::string(entry[i]) + " failed" + (err ? std::string(": ") + static_cast<const char *>(err->GetBufferPointer()) : "");
        rel(b);
        rel(err);
        return fail(w);
      }
      rel(err);
      hr = dev->CreateComputeShader(b->GetBufferPointer(), b->GetBufferSize(), nullptr, out[i]);
      rel(b);
      if (FAILED(hr)) return fail(std::string("CreateComputeShader ") + entry[i]);
    }
    {  // pass 2's DispatchIndirect arguments (written by CSArgs)
      D3D11_BUFFER_DESC ad = {};
      ad.ByteWidth = 16;
      ad.Usage = D3D11_USAGE_DEFAULT;
      ad.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
      ad.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS | D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
      if (FAILED(dev->CreateBuffer(&ad, nullptr, &args))) return fail("indirect arguments");
      D3D11_UNORDERED_ACCESS_VIEW_DESC au = {};
      au.Format = DXGI_FORMAT_R32_TYPELESS;
      au.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
      au.Buffer.NumElements = 4;
      au.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
      if (FAILED(dev->CreateUnorderedAccessView(args, &au, &args_uav))) return fail("indirect arguments");
    }
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(CBData);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(dev->CreateBuffer(&bd, nullptr, &cb))) return fail("constant buffer");
    // the constants the CPU computes by expression, as its own bits: dst * 255.0 / 31.0 (FastTables::expand5) and the dither
    // offsets (kDither[y][x] + 0.5) / 16.0 (FastTables::dither[0]); and the same as floats for the checked path
    static const int kD[4][4] = {{15, 7, 13, 5}, {3, 11, 1, 9}, {12, 4, 14, 6}, {0, 8, 2, 10}};
    double kv[48];
    float kf[48];
    for (int d = 0; d < 32; ++d) kv[d] = d * 255.0 / 31.0;
    for (int yy = 0; yy < 4; ++yy)
      for (int xx = 0; xx < 4; ++xx) kv[32 + yy * 4 + xx] = (kD[yy][xx] + 0.5) / 16.0;
    for (int i = 0; i < 48; ++i) kf[i] = static_cast<float>(kv[i]);
    D3D11_BUFFER_DESC kd = {};
    kd.ByteWidth = sizeof kv;
    kd.Usage = D3D11_USAGE_IMMUTABLE;
    kd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    kd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    kd.StructureByteStride = 8;
    D3D11_SUBRESOURCE_DATA init = {kv, 0, 0};
    if (FAILED(dev->CreateBuffer(&kd, &init, &kc)) || FAILED(dev->CreateShaderResourceView(kc, nullptr, &kc_srv))) return fail("constants");
    kd.ByteWidth = sizeof kf;
    kd.StructureByteStride = 4;
    init.pSysMem = kf;
    if (FAILED(dev->CreateBuffer(&kd, &init, &kcf)) || FAILED(dev->CreateShaderResourceView(kcf, nullptr, &kcf_srv))) return fail("constants");
    // the pixel counters (raw UAV) and their readback copy
    D3D11_BUFFER_DESC cd = {};
    cd.ByteWidth = 16;
    cd.Usage = D3D11_USAGE_DEFAULT;
    cd.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    cd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
    if (FAILED(dev->CreateBuffer(&cd, nullptr, &cnt))) return fail("counters");
    D3D11_UNORDERED_ACCESS_VIEW_DESC ud = {};
    ud.Format = DXGI_FORMAT_R32_TYPELESS;
    ud.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    ud.Buffer.NumElements = 4;
    ud.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
    if (FAILED(dev->CreateUnorderedAccessView(cnt, &ud, &cnt_uav))) return fail("counters");
    if (FAILED(dev->CreateBuffer(&cd, nullptr, &acc)) || FAILED(dev->CreateUnorderedAccessView(acc, &ud, &acc_uav))) return fail("counters");
    cd.Usage = D3D11_USAGE_STAGING;
    cd.BindFlags = 0;
    cd.MiscFlags = 0;
    cd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(dev->CreateBuffer(&cd, nullptr, &cnt_rd))) return fail("counters");
    D3D11_QUERY_DESC q = {D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
    dev->CreateQuery(&q, &dis);
    q.Query = D3D11_QUERY_TIMESTAMP;
    dev->CreateQuery(&q, &ta);
    dev->CreateQuery(&q, &tb);
    ok = true;
    why = "on";
    return true;
  }
  bool fail(const std::string &w) {
    ok = false;
    why = w;
    return false;
  }
};
std::shared_ptr<Dev> g_dev;   // guarded by g_mu for its creation

// a dynamic structured / raw buffer with its view, grown as needed
struct DynBuf {
  ID3D11Buffer *buf = nullptr;
  ID3D11ShaderResourceView *srv = nullptr;
  size_t cap = 0;
  ~DynBuf() { rel(srv); rel(buf); }
  bool fill(ID3D11Device *dev, ID3D11DeviceContext *ctx, const void *data, size_t bytes, size_t stride, bool raw) {
    const size_t need = std::max<size_t>(bytes, 16);
    if (need > cap) {
      rel(srv);
      rel(buf);
      size_t c = std::max<size_t>(need + need / 2, 4096);
      const size_t unit = raw ? 16 : stride;   // a structured buffer's size is a multiple of its stride
      c = (c + unit - 1) / unit * unit;
      D3D11_BUFFER_DESC bd = {};
      bd.ByteWidth = static_cast<UINT>(c);
      bd.Usage = D3D11_USAGE_DYNAMIC;
      bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
      bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
      bd.MiscFlags = raw ? D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS : D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
      bd.StructureByteStride = raw ? 0 : static_cast<UINT>(stride);
      if (FAILED(dev->CreateBuffer(&bd, nullptr, &buf))) return false;
      D3D11_SHADER_RESOURCE_VIEW_DESC sd = {};
      if (raw) {
        sd.Format = DXGI_FORMAT_R32_TYPELESS;
        sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
        sd.BufferEx.FirstElement = 0;
        sd.BufferEx.NumElements = static_cast<UINT>(c / 4);
        sd.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
      } else {
        sd.Format = DXGI_FORMAT_UNKNOWN;
        sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        sd.Buffer.FirstElement = 0;
        sd.Buffer.NumElements = static_cast<UINT>(c / stride);
      }
      if (FAILED(dev->CreateShaderResourceView(buf, &sd, &srv))) return false;
      cap = c;
    }
    D3D11_MAPPED_SUBRESOURCE m;
    if (FAILED(ctx->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) return false;
    if (bytes) std::memcpy(m.pData, data, bytes);
    ctx->Unmap(buf, 0);
    return true;
  }
};

constexpr int kDefaultPart = 1024, kDefaultReadThreads = 4, kSelftestPart = 96;   // P17A (the notes' measurements; fix 2: parts of 1024)

// P17A: a structured buffer the GPU writes (UAV) and then reads (SRV), grown as needed (CSPairs' pairs)
struct RwBuf {
  ID3D11Buffer *buf = nullptr;
  ID3D11ShaderResourceView *srv = nullptr;
  ID3D11UnorderedAccessView *uav = nullptr;
  size_t cap = 0;
  ~RwBuf() { rel(uav); rel(srv); rel(buf); }
  bool ensure(ID3D11Device *dev, size_t n, size_t stride) {
    if (n <= cap && buf) return true;
    rel(uav);
    rel(srv);
    rel(buf);
    const size_t c = std::max<size_t>(n + n / 2, 1024);
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = static_cast<UINT>(c * stride);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bd.StructureByteStride = static_cast<UINT>(stride);
    if (FAILED(dev->CreateBuffer(&bd, nullptr, &buf)) || FAILED(dev->CreateShaderResourceView(buf, nullptr, &srv)) ||
        FAILED(dev->CreateUnorderedAccessView(buf, nullptr, &uav)))
      return false;
    cap = c;
    return true;
  }
};

int64_t ns_since(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count();
}

// P17A: the settings. SG_P17_PART=<triangles per part> (0 = no parts: one submission per batch, as P16B), SG_P17_READ_THREADS=<n>
// (the readback copy's threads; 1 = as P16B); SG_APP_P17=off sets both to P16B's.
bool p17_off() {
  const char *e = std::getenv("SG_APP_P17");
  return e && std::strcmp(e, "off") == 0;
}
int part_setting() {
  if (p17_off()) return 0;
  const char *e = std::getenv("SG_P17_PART");
  return e ? std::max(0, std::atoi(e)) : kDefaultPart;
}
bool gpu_pairs_setting() {   // SG_P17_GPU_PAIRS=0: the pairs built on the CPU (P16B)
  if (p17_off()) return false;
  const char *e = std::getenv("SG_P17_GPU_PAIRS");
  return !(e && e[0] == '0');
}
// SG_P17_STREAM=0: the readback rows copied with memcpy (P16B). Otherwise, with SSE4.1, by streaming loads (MOVNTDQA), the read made
// for write-combined memory; the bytes copied are the same either way.
bool stream_setting() {
  if (p17_off()) return false;
  const char *e = std::getenv("SG_P17_STREAM");
  if (e && e[0] == '0') return false;
  int r[4] = {0, 0, 0, 0};
  __cpuid(r, 1);
  return ((r[2] >> 19) & 1) != 0;   // CPUID.1:ECX bit 19 = SSE4.1
}
// one row of n bytes from s (16-byte aligned) to d by streaming loads
void stream_row(uint8_t *d, const uint8_t *s, size_t n) {
  size_t k = 0;
  for (; k + 64 <= n; k += 64) {
    const __m128i a = _mm_stream_load_si128(reinterpret_cast<__m128i *>(const_cast<uint8_t *>(s + k)));
    const __m128i b = _mm_stream_load_si128(reinterpret_cast<__m128i *>(const_cast<uint8_t *>(s + k + 16)));
    const __m128i c = _mm_stream_load_si128(reinterpret_cast<__m128i *>(const_cast<uint8_t *>(s + k + 32)));
    const __m128i e = _mm_stream_load_si128(reinterpret_cast<__m128i *>(const_cast<uint8_t *>(s + k + 48)));
    _mm_storeu_si128(reinterpret_cast<__m128i *>(d + k), a);
    _mm_storeu_si128(reinterpret_cast<__m128i *>(d + k + 16), b);
    _mm_storeu_si128(reinterpret_cast<__m128i *>(d + k + 32), c);
    _mm_storeu_si128(reinterpret_cast<__m128i *>(d + k + 48), e);
  }
  for (; k + 16 <= n; k += 16)
    _mm_storeu_si128(reinterpret_cast<__m128i *>(d + k), _mm_stream_load_si128(reinterpret_cast<__m128i *>(const_cast<uint8_t *>(s + k))));
  if (k < n) std::memcpy(d + k, s + k, n - k);
}
int read_threads() {
  if (p17_off()) return 1;
  const char *e = std::getenv("SG_P17_READ_THREADS");
  return std::min(16, e ? std::max(1, std::atoi(e)) : kDefaultReadThreads);
}

// P17A (log/roadmap/P17/P17A_notes.md): a batch is ONE PASS made of parts. The first submission of a pass (a part or the draw) brings
// the backend's copy up to date (the mirror); every submission (chunk_) uploads its new texture images, bins its triangles, copies the
// source rows its tiles read and dispatches pass 1, pass 2 and CSAcc; the draw ends the pass (end_: the summed counters and the rows any
// part wrote read back into fb, the copy split over the Copier's threads). Parts run on the device's Worker while the drawing thread
// sets up the next triangles; the draw waits for the Worker first. Without parts (part_tris 0) a batch is one draw, as in P16B.
class D3DRaster : public portsim::gl::GlPixelGpu {
 public:
  // selftest: the start-up self-test's backend (its batches are not counted in p16b; SG_P16_SELFTEST_CORRUPT=1 flips one word of its
  // result: the forced mismatch). part: triangles per part (0 = none).
  D3DRaster(std::shared_ptr<Dev> d, bool selftest, int part) : d_(std::move(d)), selftest_(selftest), part_(std::max(0, part)) {
    if (const char *e = std::getenv("SG_P16_SLICE")) slice_ = std::max(0, std::atoi(e));
    const char *c = std::getenv("SG_P16_SELFTEST_CORRUPT");
    corrupt_ = selftest && c && c[0] == '1';
    std::lock_guard<std::mutex> lk(g_mu);
    g_st.slice = slice_;
  }
  int64_t covered = 0, pass2 = 0;   // the pixel writes of the last batch, and those pass 2 made
  ~D3DRaster() override {
    if (d_->worker) d_->worker->wait();   // a part still queued names this backend
    free_frame();
    rel(tsrv_);
    rel(tbuf_);
  }
  const char *why() const override { return why_.c_str(); }
  int part_tris() const override { return part_; }
  // P17B (gpu_raster_frame): the backend's copy of the frame (equal to the raster's frame when GlRaster says so)
  ID3D11Texture2D *frame_texture(int w, int h) const { return !dead_.load() && fw_ == w && fh_ == h ? mtex_ : nullptr; }

  void part(int w, int h, const uint16_t *fb, const portsim::gl::GlMirror &m, const std::vector<portsim::gl::GlGpuTri> &tris,
            const std::vector<portsim::gl::GlGpuTex> &texs) override {
    if (dead_.load()) return;
    if (!d_->worker) d_->worker = std::make_unique<Worker>();
    ++pass_parts_;
    auto job = std::make_shared<Job>(Job{w, h, fb, m, tris, texs});
    d_->worker->post([this, job] {
      const auto t0 = std::chrono::steady_clock::now();
      if (!dead_.load() && (pass_open_ || begin_(job->w, job->h, job->fb, job->m))) chunk_(job->tris, job->texs, true);
      part_ns_ += ns_since(t0);
    });
  }

  bool draw(int w, int h, uint16_t *fb, const portsim::gl::GlMirror &mm, const std::vector<portsim::gl::GlGpuTri> &tris,
            const std::vector<portsim::gl::GlGpuTex> &texs) override {
    const auto t0 = std::chrono::steady_clock::now();
    if (d_->worker) d_->worker->wait();   // the pass's parts are dispatched (or failed)
    const int64_t join_ns = ns_since(t0);
    const int64_t parts = pass_parts_;
    pass_parts_ = 0;
    if (dead_.load()) {
      pass_open_ = false;
      return false;
    }
    if (!pass_open_ && !begin_(w, h, fb, mm)) return false;
    if (!tris.empty() && !chunk_(tris, texs, false)) {
      pass_open_ = false;
      return false;
    }
    const bool ok = end_(fb, t0, join_ns, parts);
    pass_open_ = false;
    return ok;
  }

 private:
  struct Job {
    int w, h;
    const uint16_t *fb;
    portsim::gl::GlMirror m;
    std::vector<portsim::gl::GlGpuTri> tris;
    std::vector<portsim::gl::GlGpuTex> texs;
  };
  // the pass's first submission: the device, the frame textures, the mirror, the counters and the GPU timer
  bool begin_(int w, int h, const uint16_t *fb, const portsim::gl::GlMirror &mm) {
    portsim::gl::GlMirror mirror = mm;
    if (!d_->start()) return die(d_->why);
    ID3D11DeviceContext *ctx = d_->ctx;
    if (w <= 0 || h <= 0 || w > 4096 * 4 || h > 4096 * 4) return die("frame size");
    if (w != fw_ || h != fh_) {
      free_frame();
      if (!make_frame(w, h)) return die("frame textures");
      mirror.full = true;
    }
    // the mirror: all of fb uploaded, or filled and then the rows drawn on the CPU since uploaded
    const auto tu = std::chrono::steady_clock::now();
    if (mirror.full) {
      ctx->UpdateSubresource(mtex_, 0, nullptr, fb, static_cast<UINT>(w) * 2u, 0);
    } else {
      if (mirror.clear) {
        const UINT v[4] = {mirror.value, mirror.value, mirror.value, mirror.value};
        ctx->ClearUnorderedAccessViewUint(muav_, v);
      }
      const int y0 = std::max(0, static_cast<int>(mirror.y0)), y1 = std::min(h - 1, static_cast<int>(mirror.y1));
      if (y0 <= y1) {
        D3D11_BOX box = {0, static_cast<UINT>(y0), 0, static_cast<UINT>(w), static_cast<UINT>(y1 + 1), 1};
        ctx->UpdateSubresource(mtex_, 0, &box, fb + static_cast<size_t>(y0) * static_cast<size_t>(w), static_cast<UINT>(w) * 2u, 0);
      }
    }
    pass_ = Pass{};
    pass_.up_ns = ns_since(tu);
    pass_.full = mirror.full;
    pass_.clear = !mirror.full && mirror.clear;
    pass_.ymin = fh_;
    pass_.ymax = -1;
    const UINT zero[4] = {0, 0, 0, 0};
    ctx->ClearUnorderedAccessViewUint(d_->acc_uav, zero);
    pass_.timed = d_->dis != nullptr;
    if (pass_.timed) ctx->Begin(d_->dis);
    pass_open_ = true;
    return true;
  }
  // a timestamp query pair per submission (made as needed)
  bool stamp_(size_t k, bool end) {
    while (stamps_.size() <= k * 2 + 1) {
      ID3D11Query *q = nullptr;
      D3D11_QUERY_DESC qd = {D3D11_QUERY_TIMESTAMP, 0};
      if (FAILED(d_->dev->CreateQuery(&qd, &q)) || !q) return false;
      stamps_.push_back(q);
    }
    d_->ctx->End(stamps_[k * 2 + (end ? 1 : 0)]);
    return true;
  }
  // one submission: textures, binning, the source rows, pass 1, pass 2, CSAcc (as P16B's draw between the mirror and the readback)
  bool chunk_(const std::vector<portsim::gl::GlGpuTri> &tris, const std::vector<portsim::gl::GlGpuTex> &texs, bool flush) {
    ID3D11Device *dev = d_->dev;
    ID3D11DeviceContext *ctx = d_->ctx;
    const int w = fw_, h = fh_;
    // textures: every image the backend has seen keeps its place in one raw buffer; a new image is appended, and only when the buffer
    // is full is it made again from the submission's images (twice their size when they do not fit)
    int64_t tex_bytes = 0;
    auto tsize = [](const portsim::gl::GlGpuTex &t) { return (static_cast<size_t>(t.w) * t.h * t.bpp + 3) & ~static_cast<size_t>(3); };
    size_t need = 0, batch = 0;
    for (const portsim::gl::GlGpuTex &t : texs) {
      batch += tsize(t);
      if (!tex_off_.count(t.serial)) need += tsize(t);
    }
    if (batch + 8 >= 0x7fffffffu) return die("textures too large");
    if (need && (!tbuf_ || tex_tail_ + need + 8 > tex_cap_)) {   // made again (a part already queued keeps the old buffer alive)
      rel(tsrv_);
      rel(tbuf_);
      tex_off_.clear();
      tex_tail_ = 0;
      tex_cap_ = std::max<size_t>(std::max<size_t>(batch * 2 + 8, 32u << 20), tex_cap_);
      tex_cap_ = std::min<size_t>(tex_cap_, 0x7ffffff0u);
      D3D11_BUFFER_DESC bd = {};
      bd.ByteWidth = static_cast<UINT>((tex_cap_ + 15) & ~static_cast<size_t>(15));
      bd.Usage = D3D11_USAGE_DEFAULT;
      bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
      bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
      if (FAILED(dev->CreateBuffer(&bd, nullptr, &tbuf_))) return die("texture buffer");
      D3D11_SHADER_RESOURCE_VIEW_DESC sd = {};
      sd.Format = DXGI_FORMAT_R32_TYPELESS;
      sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
      sd.BufferEx.NumElements = bd.ByteWidth / 4;
      sd.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
      if (FAILED(dev->CreateShaderResourceView(tbuf_, &sd, &tsrv_))) return die("texture buffer view");
      ++tex_rebuilds_;
    }
    for (const portsim::gl::GlGpuTex &t : texs) {
      if (tex_off_.count(t.serial)) continue;
      const size_t n = static_cast<size_t>(t.w) * t.h * t.bpp, sz = tsize(t);
      std::vector<uint8_t> tmp;
      const uint8_t *src = t.data;
      if (sz != n) {   // the box is whole words: the image padded with zeros
        tmp.assign(t.data, t.data + n);
        tmp.resize(sz, 0);
        src = tmp.data();
      }
      D3D11_BOX box = {static_cast<UINT>(tex_tail_), 0, 0, static_cast<UINT>(tex_tail_ + sz), 1, 1};
      ctx->UpdateSubresource(tbuf_, 0, &box, src, 0, 0);
      tex_off_[t.serial] = static_cast<uint32_t>(tex_tail_);
      tex_tail_ += sz;
      tex_bytes += static_cast<int64_t>(sz);
    }
    pass_.tex_bytes += tex_bytes;
    pass_.tex_up = pass_.tex_up || tex_bytes != 0;
    pass_.tex_count += static_cast<int64_t>(texs.size());
    // the triangles, and per 16 x 16 tile the (tile, triangle) pairs in triangle order with their coverage thresholds
    const auto tb = std::chrono::steady_clock::now();
    const int tx = (w + 15) / 16, ty = (h + 15) / 16;
    const size_t ntile = static_cast<size_t>(tx) * static_cast<size_t>(ty);
    tg_.resize(tris.size());
    if (count_.size() != ntile + 1) count_.assign(ntile + 1, 0);   // P17A: zero outside the part's tile rows between submissions
    int ymin = h, ymax = -1;
    for (size_t i = 0; i < tris.size(); ++i) {
      const portsim::gl::GlGpuTri &s = tris[i];
      TriG &g = tg_[i];
      for (int e = 0; e < 3; ++e) {
        const int p = (e + 1) % 3, q = (e + 2) % 3;
        g.A[e] = s.X[q] - s.X[p];
        g.B[e] = s.Y[q] - s.Y[p];
      }
      g.flags = s.flags;
      g.texoff = tex_off_[texs[s.tex].serial];
      g.tw = texs[s.tex].w;
      g.th = texs[s.tex].h;
      g.bpp = texs[s.tex].bpp;
      const double inv_st = 1.0 / s.div_st, inv_a = 1.0 / s.arA;   // fast_triangle_'s reciprocals
      const double dv[19] = {s.s[0], s.s[1], s.s[2], s.t[0], s.t[1], s.t[2], s.r[0], s.r[1], s.r[2], s.g[0],
                             s.g[1], s.g[2], s.b[0], s.b[1], s.b[2], s.div_st, s.arA, inv_st, inv_a};
      std::memcpy(g.d, dv, sizeof dv);
      bool fast = !force_exact_ && s.div_st > 0 && s.div_st < 1e300 && s.arA > 0 && s.arA < 1e300;
      for (int k = 0; k < 3; ++k) {
        const double c3[3] = {s.r[k], s.g[k], s.b[k]};
        for (int c = 0; c < 3; ++c) fast = fast && c3[c] >= 0 && c3[c] <= 1;
        g.cf[k] = static_cast<float>(s.r[k]);
        g.cf[3 + k] = static_cast<float>(s.g[k]);
        g.cf[6 + k] = static_cast<float>(s.b[k]);
      }
      g.fast = fast ? 1u : 0u;
      g.inv_af = static_cast<float>(inv_a);
      for (int e = 0; e < 3; ++e) {   // P17A: CSPairs' K (|B XP - A YP| < 2^46: exact as a double)
        const int p = (e + 1) % 3;
        const double K = static_cast<double>(static_cast<int64_t>(g.B[e]) * s.X[p] - static_cast<int64_t>(g.A[e]) * s.Y[p]);
        std::memcpy(&g.K[e * 2], &K, sizeof K);
      }
      g.box[0] = s.minx;
      g.box[1] = s.maxx;
      g.box[2] = s.miny;
      g.box[3] = s.maxy;
      if (s.minx > s.maxx || s.miny > s.maxy) continue;
      ymin = std::min(ymin, s.miny);
      ymax = std::max(ymax, s.maxy);
      for (int yy = s.miny >> 4; yy <= (s.maxy >> 4); ++yy)
        for (int xx = s.minx >> 4; xx <= (s.maxx >> 4); ++xx) ++count_[static_cast<size_t>(yy) * tx + xx];
    }
    // P17A: the tile table over the submission's tile rows only (tiles t0 .. t1 - 1; the shaders index it from t0, cb2)
    const size_t t0 = ymax >= ymin ? static_cast<size_t>(ymin >> 4) * static_cast<size_t>(tx) : 0;
    const size_t t1 = ymax >= ymin ? static_cast<size_t>((ymax >> 4) + 1) * static_cast<size_t>(tx) : 0;
    range_.resize((t1 - t0) * 2);
    tiles_.clear();
    uint32_t acc = 0;
    for (size_t t = t0; t < t1; ++t) {
      range_[(t - t0) * 2] = acc;
      range_[(t - t0) * 2 + 1] = count_[t];
      if (count_[t]) tiles_.push_back(static_cast<uint32_t>(t));
      acc += count_[t];
      count_[t] = range_[(t - t0) * 2];   // the next free slot
    }
    if (gpu_pairs_) {   // P17A: per pair only (triangle, tile); CSPairs computes the rest
      cpairs_.resize(static_cast<size_t>(acc) * 2);
      for (size_t i = 0; i < tris.size(); ++i) {
        const portsim::gl::GlGpuTri &s = tris[i];
        if (s.minx > s.maxx || s.miny > s.maxy) continue;
        for (int yy = s.miny >> 4; yy <= (s.maxy >> 4); ++yy)
          for (int xx = s.minx >> 4; xx <= (s.maxx >> 4); ++xx) {
            const size_t t = static_cast<size_t>(yy) * tx + xx;
            uint32_t *c = &cpairs_[static_cast<size_t>(count_[t]++) * 2];
            c[0] = static_cast<uint32_t>(i);
            c[1] = static_cast<uint32_t>(t);
          }
      }
    }
    pairs_.resize(gpu_pairs_ ? 0 : acc);
    for (size_t i = 0; i < tris.size() && !gpu_pairs_; ++i) {
      const portsim::gl::GlGpuTri &s = tris[i];
      if (s.minx > s.maxx || s.miny > s.maxy) continue;
      const TriG &g = tg_[i];
      for (int yy = s.miny >> 4; yy <= (s.maxy >> 4); ++yy)
        for (int xx = s.minx >> 4; xx <= (s.maxx >> 4); ++xx) {
          PairG &pr = pairs_[count_[static_cast<size_t>(yy) * tx + xx]++];
          const int x0 = xx * 16, y0 = yy * 16;
          pr.tri = static_cast<uint32_t>(i);
          const uint32_t bx0 = static_cast<uint32_t>(std::max(s.minx - x0, 0)), bx1 = static_cast<uint32_t>(std::min(s.maxx - x0, 15));
          const uint32_t by0 = static_cast<uint32_t>(std::max(s.miny - y0, 0)), by1 = static_cast<uint32_t>(std::min(s.maxy - y0, 15));
          pr.box = bx0 | (bx1 << 8) | (by0 << 16) | (by1 << 24);
          pr.pad = 0;
          const int64_t PX0 = 256 * static_cast<int64_t>(x0) + 128, PY0 = 256 * static_cast<int64_t>(y0) + 128;
          for (int e = 0; e < 3; ++e) {
            const int p = (e + 1) % 3;
            const int64_t A = g.A[e], B = g.B[e];
            const int64_t W0 = A * (PY0 - s.Y[p]) + B * s.X[p] - B * PX0;   // fast_rows_'s K - B * PX at the tile's first pixel
            const int64_t t = (s.flags & (portsim::gl::kGtInc0 << e)) ? 0 : 1;
            int64_t tc = ceil_div(t - W0, 256);
            tc = std::min<int64_t>(std::max<int64_t>(tc, -(int64_t{1} << 30)), int64_t{1} << 30);
            pr.Tc[e] = static_cast<int32_t>(tc);
            pr.W0[e] = static_cast<double>(W0);   // exact: |W0| < 2^48
          }
        }
    }
    std::fill(count_.begin() + static_cast<std::ptrdiff_t>(t0), count_.begin() + static_cast<std::ptrdiff_t>(t1), 0u);   // P17A
    if (!tribuf_.fill(dev, ctx, tg_.data(), tg_.size() * sizeof(TriG), sizeof(TriG), false) ||
        !rangebuf_.fill(dev, ctx, range_.data(), range_.size() * 4, 8, false) ||
        (!gpu_pairs_ && !pairbuf_.fill(dev, ctx, pairs_.data(), pairs_.size() * sizeof(PairG), sizeof(PairG), false)) ||
        (gpu_pairs_ && (!cpairbuf_.fill(dev, ctx, cpairs_.data(), cpairs_.size() * 4, 8, false) || !pairrw_.ensure(dev, acc, sizeof(PairG)))) ||
        !tilebuf_.fill(dev, ctx, tiles_.data(), tiles_.size() * 4, 4, false))
      return die("triangle buffers");
    pass_.bin_ns += ns_since(tb);
    pass_.tris += static_cast<int64_t>(tris.size());
    pass_.pairs += static_cast<int64_t>(acc);
    pass_.groups += static_cast<int64_t>(tiles_.size());
    if (ymax >= ymin) {
      pass_.ymin = std::min(pass_.ymin, ymin);
      pass_.ymax = std::max(pass_.ymax, ymax);
      pass_.sub_rows += ymax - ymin + 1;   // P17A fix 2: how far the submissions' rows overlap (against the pass's rows)
    }
    if (tiles_.empty()) return true;
    // the pass: the source rows the tiles read (every pixel of a tile with pairs is read: the whole tile rows), then the tiles in slices
    const bool timed = pass_.timed && stamp_(static_cast<size_t>(pass_.subs), false);
    const UINT zero[4] = {0, 0, 0, 0};
    ctx->ClearUnorderedAccessViewUint(d_->cnt_uav, zero);
    {
      const UINT r0 = static_cast<UINT>((ymin >> 4) * 16), r1 = static_cast<UINT>(std::min(h, ((ymax >> 4) + 1) * 16));
      D3D11_BOX box = {0, r0, 0, static_cast<UINT>(w), r1, 1};
      ctx->CopySubresourceRegion(stex_, 0, 0, r0, 0, mtex_, 0, &box);
    }
    if (gpu_pairs_) {   // P17A: the pairs filled in on the GPU (CSPairs), before pass 1 reads them
      D3D11_MAPPED_SUBRESOURCE m;
      if (FAILED(ctx->Map(d_->cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) return die("constant buffer map");
      const CBData c = {w, h, tx, 0, list_cap_, acc, static_cast<uint32_t>(t0), 0};
      std::memcpy(m.pData, &c, sizeof c);
      ctx->Unmap(d_->cb, 0);
      ctx->CSSetConstantBuffers(0, 1, &d_->cb);
      ctx->CSSetShader(d_->cs_pairs, nullptr, 0);
      ctx->CSSetShaderResources(0, 1, &tribuf_.srv);
      ctx->CSSetShaderResources(8, 1, &cpairbuf_.srv);
      ctx->CSSetUnorderedAccessViews(5, 1, &pairrw_.uav, nullptr);
      ctx->Dispatch((acc + 63u) / 64u, 1, 1);
      ++pass_.dispatches;
      ID3D11UnorderedAccessView *nu = nullptr;
      ID3D11ShaderResourceView *ns = nullptr;
      ctx->CSSetUnorderedAccessViews(5, 1, &nu, nullptr);
      ctx->CSSetShaderResources(8, 1, &ns);
    }
    ctx->CSSetShader(d_->cs, nullptr, 0);
    ID3D11ShaderResourceView *srvs[8] = {tribuf_.srv, rangebuf_.srv, gpu_pairs_ ? pairrw_.srv : pairbuf_.srv, tilebuf_.srv, tsrv_,
                                         d_->kc_srv, ssrv_, d_->kcf_srv};
    ctx->CSSetShaderResources(0, 8, srvs);
    ID3D11UnorderedAccessView *uavs[3] = {muav_, d_->cnt_uav, luav_};
    ctx->CSSetUnorderedAccessViews(0, 3, uavs, nullptr);
    const size_t per = slice_ > 0 ? static_cast<size_t>(slice_) : tiles_.size();
    for (size_t base = 0; base < tiles_.size(); base += per) {
      const size_t n = std::min(per, tiles_.size() - base);
      D3D11_MAPPED_SUBRESOURCE m;
      if (FAILED(ctx->Map(d_->cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) return die("constant buffer map");
      const CBData c = {w, h, tx, static_cast<int32_t>(base), list_cap_, 0, static_cast<uint32_t>(t0), 0};
      std::memcpy(m.pData, &c, sizeof c);
      ctx->Unmap(d_->cb, 0);
      ctx->CSSetConstantBuffers(0, 1, &d_->cb);
      ctx->Dispatch(static_cast<UINT>(n), 1, 1);
      ++pass_.dispatches;
      if (base + n < tiles_.size()) ctx->Flush();
    }
    // pass 2: its size from pass 1's count, then the handed pixels; then this submission's counters added to the pass's
    ID3D11UnorderedAccessView *au[4] = {muav_, d_->cnt_uav, luav_, d_->args_uav};
    ctx->CSSetUnorderedAccessViews(0, 4, au, nullptr);
    ctx->CSSetShader(d_->cs_args, nullptr, 0);
    ctx->Dispatch(1, 1, 1);
    ID3D11UnorderedAccessView *noargs = nullptr;
    ctx->CSSetUnorderedAccessViews(3, 1, &noargs, nullptr);   // the arguments buffer is read by DispatchIndirect, not bound
    ctx->CSSetShader(d_->cs_resume, nullptr, 0);
    ctx->DispatchIndirect(d_->args, 0);
    ++pass_.dispatches;
    ctx->CSSetUnorderedAccessViews(4, 1, &d_->acc_uav, nullptr);
    ctx->CSSetShader(d_->cs_acc, nullptr, 0);
    ctx->Dispatch(1, 1, 1);
    ID3D11ShaderResourceView *none[8] = {};
    ID3D11UnorderedAccessView *nouav[5] = {};
    ctx->CSSetShaderResources(0, 8, none);
    ctx->CSSetUnorderedAccessViews(0, 5, nouav, nullptr);
    if (timed) stamp_(static_cast<size_t>(pass_.subs), true);
    ++pass_.subs;
    if (flush) ctx->Flush();   // a part: the GPU starts on it now
    return true;
  }
  // the pass's end: the summed counters and the rows written copied back, both queued before the wait; then the rows into fb
  bool end_(uint16_t *fb, std::chrono::steady_clock::time_point t0, int64_t join_ns, int64_t parts) {
    ID3D11Device *dev = d_->dev;
    ID3D11DeviceContext *ctx = d_->ctx;
    const int w = fw_;
    const int ymin = pass_.ymin, ymax = pass_.ymax;
    ctx->CopyResource(d_->cnt_rd, d_->acc);
    if (ymax >= ymin) {
      D3D11_BOX box = {0, static_cast<UINT>(ymin), 0, static_cast<UINT>(w), static_cast<UINT>(ymax + 1), 1};
      ctx->CopySubresourceRegion(rtex_, 0, 0, static_cast<UINT>(ymin), 0, mtex_, 0, &box);
    }
    if (pass_.timed) ctx->End(d_->dis);
    // the counters first (this waits for the pass): a pixel pass 2 could not take (list full, or 2^17 pairs or more in its tile;
    // neither happens with one list entry per pixel and real tiles) was not finished, and fb is still untouched here
    const auto tr = std::chrono::steady_clock::now();
    uint32_t counts[4] = {0, 0, 0, 0};
    {
      D3D11_MAPPED_SUBRESOURCE m;
      if (FAILED(ctx->Map(d_->cnt_rd, 0, D3D11_MAP_READ, 0, &m))) return die("counter readback");
      std::memcpy(counts, m.pData, sizeof counts);
      ctx->Unmap(d_->cnt_rd, 0);
    }
    const int64_t wait_ns = ns_since(tr);
    if (counts[2] != 0) return die("pass 2 list overflow");
    if (FAILED(dev->GetDeviceRemovedReason())) return die("device removed");
    int64_t map_ns = 0, copy_ns = 0;
    if (ymax >= ymin) {
      const auto tm = std::chrono::steady_clock::now();
      D3D11_MAPPED_SUBRESOURCE m;
      const HRESULT hr = ctx->Map(rtex_, 0, D3D11_MAP_READ, 0, &m);
      if (FAILED(hr)) {
        char b[64];
        std::snprintf(b, sizeof b, "readback map 0x%08lx", static_cast<unsigned long>(hr));
        return die(b);
      }
      map_ns = ns_since(tm);
      const auto tc = std::chrono::steady_clock::now();
      if (!d_->copier) d_->copier = std::make_unique<Copier>(read_threads());
      const uint8_t *base = static_cast<const uint8_t *>(m.pData);
      const size_t pitch = m.RowPitch, rows = static_cast<size_t>(ymax - ymin + 1);
      const bool stream = stream_ && ((reinterpret_cast<uintptr_t>(base) | pitch) & 15u) == 0;   // P17A: streaming loads need 16-byte rows
      d_->copier->run([&](int k, int n) {   // rows split into n bands
        const size_t a = rows * static_cast<size_t>(k) / static_cast<size_t>(n), b = rows * static_cast<size_t>(k + 1) / static_cast<size_t>(n);
        for (size_t r = a; r < b; ++r) {
          const size_t yy = static_cast<size_t>(ymin) + r;
          uint8_t *dst = reinterpret_cast<uint8_t *>(fb + yy * static_cast<size_t>(w));
          if (stream) stream_row(dst, base + yy * pitch, static_cast<size_t>(w) * 2u);
          else std::memcpy(dst, base + yy * pitch, static_cast<size_t>(w) * 2u);
        }
      });
      if (stream) ++pass_streamed_;
      copy_ns = ns_since(tc);
      ctx->Unmap(rtex_, 0);
      if (corrupt_) fb[static_cast<size_t>(ymin) * static_cast<size_t>(w) + static_cast<size_t>(w / 2)] ^= 1u;   // the forced mismatch
    }
    const int64_t read_ns = ns_since(tr);
    covered = counts[0];
    pass2 = counts[1];
    int64_t gpu_ns = -1, span_ns = -1;
    if (pass_.timed) {
      D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj = {};
      if (ctx->GetData(d_->dis, &dj, sizeof dj, 0) == S_OK && !dj.Disjoint && dj.Frequency) {
        UINT64 sum = 0, first = 0, last = 0;
        bool good = true;
        for (int64_t k = 0; k < pass_.subs && good; ++k) {
          UINT64 a = 0, b = 0;
          good = ctx->GetData(stamps_[static_cast<size_t>(k) * 2], &a, sizeof a, 0) == S_OK &&
                 ctx->GetData(stamps_[static_cast<size_t>(k) * 2 + 1], &b, sizeof b, 0) == S_OK && b >= a;
          if (good) {
            sum += b - a;
            if (k == 0) first = a;
            last = b;
          }
        }
        if (good) {
          gpu_ns = static_cast<int64_t>(static_cast<double>(sum) * 1e9 / static_cast<double>(dj.Frequency));
          span_ns = pass_.subs ? static_cast<int64_t>(static_cast<double>(last - first) * 1e9 / static_cast<double>(dj.Frequency)) : 0;
        }
      }
    }
    const int64_t draw_ns = ns_since(t0);
    const int64_t part_ns = part_ns_;
    part_ns_ = 0;
    if (selftest_) return true;
    std::lock_guard<std::mutex> lk(g_mu);
    ++g_st.batches;
    g_st.tris += pass_.tris;
    g_st.pairs += pass_.pairs;
    g_st.groups += pass_.groups;
    g_st.dispatches += pass_.dispatches;
    g_st.uploads += pass_.full ? 1 : 0;
    g_st.clears += pass_.clear ? 1 : 0;
    g_st.tex_uploads += pass_.tex_up ? 1 : 0;
    g_st.tex_bytes += pass_.tex_bytes;
    g_st.tex_count += pass_.tex_count;
    g_st.covered += counts[0];
    g_st.exact += counts[1];
    g_st.bin_ns += pass_.bin_ns;
    g_st.upload_ns += pass_.up_ns;
    g_st.wait_ns += wait_ns;
    g_st.read_ns += read_ns;
    g_st.map_ns += map_ns;
    g_st.copy_ns += copy_ns;
    g_st.join_ns += join_ns;
    g_st.part_ns += part_ns;
    g_st.parts += parts;
    g_st.streamed += pass_streamed_;
    g_st.sub_rows += pass_.sub_rows;
    g_st.pass_rows += ymax >= ymin ? ymax - ymin + 1 : 0;
    pass_streamed_ = 0;
    g_st.draw_ns += draw_ns;
    g_st.max_draw_ns = std::max(g_st.max_draw_ns, draw_ns);
    if (gpu_ns >= 0) {
      g_st.gpu_ns += gpu_ns;
      g_st.span_ns += span_ns;
      ++g_st.gpu_timed;
      g_st.max_gpu_ns = std::max(g_st.max_gpu_ns, gpu_ns);
    }
    g_st.force_exact = force_exact_;
    g_st.state = "on";
    return true;
  }
  bool die(const std::string &w) {
    dead_ = true;
    why_ = w;
    std::fprintf(stderr, "P16 GPU RASTER off: %s\n", w.c_str());
    std::lock_guard<std::mutex> lk(g_mu);
    ++g_st.failures;
    g_st.state = w;
    return false;
  }
  bool make_frame(int w, int h) {
    ID3D11Device *dev = d_->dev;
    force_exact_ = d_->force_exact;
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = static_cast<UINT>(w);
    td.Height = static_cast<UINT>(h);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R16_UINT;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    if (FAILED(dev->CreateTexture2D(&td, nullptr, &mtex_)) || FAILED(dev->CreateUnorderedAccessView(mtex_, nullptr, &muav_))) return false;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(dev->CreateTexture2D(&td, nullptr, &stex_)) || FAILED(dev->CreateShaderResourceView(stex_, nullptr, &ssrv_))) return false;
    td.BindFlags = 0;
    td.Usage = D3D11_USAGE_STAGING;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(dev->CreateTexture2D(&td, nullptr, &rtex_))) return false;
    list_cap_ = static_cast<uint32_t>(w) * static_cast<uint32_t>(h);   // one entry per pixel at most
    D3D11_BUFFER_DESC ld = {};
    ld.ByteWidth = list_cap_ * 8u;
    ld.Usage = D3D11_USAGE_DEFAULT;
    ld.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    ld.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    ld.StructureByteStride = 8;
    if (FAILED(dev->CreateBuffer(&ld, nullptr, &lbuf_)) || FAILED(dev->CreateUnorderedAccessView(lbuf_, nullptr, &luav_))) return false;
    fw_ = w;
    fh_ = h;
    return true;
  }
  void free_frame() {
    rel(muav_); rel(mtex_); rel(ssrv_); rel(stex_); rel(rtex_); rel(luav_); rel(lbuf_);
    for (ID3D11Query *&q : stamps_) rel(q);
    stamps_.clear();
    fw_ = fh_ = 0;
  }
  // what one pass (a batch: its parts and its draw) did, summed into p16b at its end
  struct Pass {
    int64_t up_ns = 0, bin_ns = 0, tris = 0, pairs = 0, groups = 0, dispatches = 0, subs = 0, tex_bytes = 0, tex_count = 0, sub_rows = 0;
    bool full = false, clear = false, tex_up = false, timed = false;
    int ymin = 0, ymax = -1;
  };
  std::shared_ptr<Dev> d_;
  std::atomic<bool> dead_{false};
  bool force_exact_ = false, selftest_ = false, corrupt_ = false;
  bool pass_open_ = false;   // a pass begun by a part (Worker) or the draw; the draw's wait orders the two threads
  Pass pass_;
  int64_t pass_parts_ = 0;   // parts handed over in the current pass (the drawing thread)
  int64_t part_ns_ = 0;      // the Worker's time on the current pass's parts
  std::string why_ = "on";
  int slice_ = 0;   // SG_P16_SLICE: tiles per dispatch (0 = pass 1 in one dispatch; slices showed no effect on the paints, P16B_notes.md)
  int part_ = 0;    // P17A: triangles per part (0 = none)
  int fw_ = 0, fh_ = 0;
  ID3D11Texture2D *mtex_ = nullptr, *stex_ = nullptr, *rtex_ = nullptr;
  ID3D11UnorderedAccessView *muav_ = nullptr, *luav_ = nullptr;
  ID3D11Buffer *lbuf_ = nullptr;
  uint32_t list_cap_ = 0;
  ID3D11ShaderResourceView *ssrv_ = nullptr;
  std::vector<ID3D11Query *> stamps_;   // P17A: a timestamp pair per submission of the pass
  DynBuf tribuf_, rangebuf_, pairbuf_, tilebuf_;
  DynBuf cpairbuf_;   // P17A: per pair (triangle, tile), the CPU's binning
  RwBuf pairrw_;      // P17A: the pairs CSPairs fills in
  bool gpu_pairs_ = gpu_pairs_setting();
  bool stream_ = stream_setting();
  int64_t pass_streamed_ = 0;   // readbacks copied by streaming loads
  std::vector<uint32_t> cpairs_;
  ID3D11Buffer *tbuf_ = nullptr;   // the texture images (raw); tex_off_: byte offset of each image by serial
  ID3D11ShaderResourceView *tsrv_ = nullptr;
  std::map<uint64_t, uint32_t> tex_off_;
  size_t tex_tail_ = 0, tex_cap_ = 0;
  int64_t tex_rebuilds_ = 0;
  std::vector<TriG> tg_;
  std::vector<PairG> pairs_;
  std::vector<uint32_t> count_, range_, tiles_;
};

// ---- THE START-UP SELF-TEST (P16B fix 2; P16B_notes.md section 1.3). Before the device's first backend, a fixed GL stream (an LCG) is
// drawn into two 256 x 192 rasterisers, one with this backend and one with the CPU loops (fast_rows_), and the frames compared word by
// word. The stream reaches what the world raster uses: GL_LINEAR on an RGBA texture and GL_NEAREST on an RGB one, GL_MODULATE with
// vertex colours, blended and unblended triangles, every dither column phase (set_dither_dx), small triangles and frame-wide ones that
// the clipper cuts, and degenerate ones; the GPU's counters say how many pixel writes pass 1's checked path and pass 2's exact path
// made (st_covered / st_pass2). A difference, or a backend that fails, keeps the CPU raster for the session.
void selftest_feed(portsim::gl::GlRaster &r, int W, int H, int ddx) {
  uint32_t seed = 0x2468ace1u;
  auto rnd = [&seed]() {
    seed = seed * 1664525u + 1013904223u;
    return seed;
  };
  auto uf = [&rnd](float a, float b) { return a + (b - a) * static_cast<float>(rnd() >> 8) / 16777216.0f; };
  r.set_dither_dx(ddx);
  r.viewport(0, 0, W, H);
  uint32_t names[2] = {0, 0};
  r.gen_textures(2, names);
  const uint32_t kTex2d = 0x0DE1, kMin = 0x2801, kMag = 0x2800;
  const int tw[2] = {64, 16}, th[2] = {32, 16}, bpp[2] = {4, 3};
  const int filt[2] = {0x2601, 0x2600};   // GL_LINEAR, GL_NEAREST
  for (int t = 0; t < 2; ++t) {
    std::vector<uint8_t> img(static_cast<size_t>(tw[t]) * th[t] * bpp[t]);
    for (uint8_t &b : img) b = static_cast<uint8_t>(rnd() >> 24);
    r.bind_texture(kTex2d, names[t]);
    r.tex_parameteri(kTex2d, kMin, filt[t]);
    r.tex_parameteri(kTex2d, kMag, filt[t]);
    r.tex_image2d(kTex2d, 0, bpp[t] == 4 ? 0x1908 : 0x1907, tw[t], th[t], 0, bpp[t] == 4 ? 0x1908 : 0x1907, 0x1401, img.data());
  }
  r.enable(kTex2d);
  r.blend_func(0x0302, 0x0303);
  for (int i = 0; i < 400; ++i) {
    r.bind_texture(kTex2d, names[i % 3 == 2 ? 1 : 0]);
    if (i % 4 == 1) r.enable(0x0BE2);
    else r.disable(0x0BE2);
    const bool big = i % 50 == 7, flat = i % 97 == 13;
    const float cx = uf(-1.0f, 1.0f), cy = uf(-1.0f, 1.0f), k = big ? 1.3f : 0.35f;
    r.begin(4);   // GL_TRIANGLES
    for (int v = 0; v < 3; ++v) {
      r.color4f(uf(0.0f, 1.0f), uf(0.0f, 1.0f), uf(0.0f, 1.0f), 1.0f);
      r.tex_coord2f(uf(-1.5f, 2.5f), uf(-1.5f, 2.5f));
      const float x = big ? uf(-k, k) : cx + uf(-k, k), y = big ? uf(-k, k) : cy + uf(-k, k);
      r.vertex3f(flat ? cx + 0.1f * static_cast<float>(v) : x, flat ? cy + 0.05f * static_cast<float>(v) : y, 0.0f);   // flat: collinear
    }
    r.end();
  }
  // P17A fix 2: a mesh of squares cut on a diagonal whose vertices are pixel centres, so the edges pass through pixel centres (an edge
  // function exactly 0, or within one step of the coverage threshold): every such centre is drawn by the one triangle that owns it. The
  // random stream above almost never puts a centre there (SG_P17_PAIRS_NEGCTL=1, a threshold one too high, passed it).
  r.disable(0x0BE2);
  const int cell = 23;
  auto cx_ = [W](int x) { return (static_cast<float>(x) + 0.5f) * 2.0f / static_cast<float>(W) - 1.0f; };
  auto cy_ = [H](int y) { return (static_cast<float>(y) + 0.5f) * 2.0f / static_cast<float>(H) - 1.0f; };
  for (int gy = 1; gy + cell < H; gy += cell)
    for (int gx = 1; gx + cell < W; gx += cell) {
      r.bind_texture(kTex2d, names[(gx / cell + gy / cell) % 2]);
      const int qx[6] = {gx, gx + cell, gx + cell, gx, gx + cell, gx}, qy[6] = {gy, gy, gy + cell, gy, gy + cell, gy + cell};
      r.begin(4);
      for (int v = 0; v < 6; ++v) {
        r.color4f(uf(0.0f, 1.0f), uf(0.0f, 1.0f), uf(0.0f, 1.0f), 1.0f);
        r.tex_coord2f(uf(-1.5f, 2.5f), uf(-1.5f, 2.5f));
        r.vertex3f(cx_(qx[v]), cy_(qy[v]), 0.0f);
      }
      r.end();
    }
  r.flush();
}
void selftest(const std::shared_ptr<Dev> &d) {
  const auto t0 = std::chrono::steady_clock::now();
  const bool started = d->start();
  const double dev_ms = static_cast<double>(ns_since(t0)) / 1e6;
  std::string result;
  bool pass = false;
  int64_t words = 0, differing = 0, covered = 0, pass2 = 0;
  bool corrupt = false;
  const auto t1 = std::chrono::steady_clock::now();
  if (!started) {
    result = "device: " + d->why;
  } else {
    constexpr int W = 256, H = 192;
    portsim::gl::GlRaster cpu(W, H), gpu(W, H);
    auto be = std::make_shared<D3DRaster>(d, true, kSelftestPart);   // P17A: the self-test draws its stream in parts
    gpu.set_gpu(be);
    selftest_feed(cpu, W, H, 1);
    selftest_feed(gpu, W, H, 1);
    const std::vector<uint16_t> &a = gpu.cpixels(), &b = cpu.cpixels();
    covered = be->covered;
    pass2 = be->pass2;
    corrupt = std::getenv("SG_P16_SELFTEST_CORRUPT") && std::getenv("SG_P16_SELFTEST_CORRUPT")[0] == '1';
    if (!gpu.has_gpu()) {
      result = std::string("backend failed: ") + be->why();
    } else {
      words = static_cast<int64_t>(a.size());
      size_t first = 0;
      for (size_t i = 0; i < a.size() && i < b.size(); ++i)
        if (a[i] != b[i]) {
          if (!differing) first = i;
          ++differing;
        }
      if (differing || a.size() != b.size()) {
        char m[160];
        std::snprintf(m, sizeof m, "FAIL: %lld of %lld words differ, first (%d, %d) gpu 0x%04x cpu 0x%04x", static_cast<long long>(differing),
                      static_cast<long long>(words), static_cast<int>(first % W), static_cast<int>(first / W), a[first], b[first]);
        result = m;
      } else {
        pass = true;
        result = "pass";
      }
    }
  }
  const double test_ms = static_cast<double>(ns_since(t1)) / 1e6;
  std::fprintf(stderr, "P16 GPU RASTER self-test: %s (pixel writes %lld, by pass 2 %lld; device %.1f ms, test %.1f ms)%s\n", result.c_str(),
               static_cast<long long>(covered), static_cast<long long>(pass2), dev_ms, test_ms,
               pass ? "" : ": the CPU raster for this session");
  std::lock_guard<std::mutex> lk(g_mu);
  g_st.st_done = true;
  g_st.st_pass = pass;
  g_st.st_corrupt = corrupt;
  g_st.st_result = result;
  g_st.st_words = words;
  g_st.st_differing = differing;
  g_st.st_covered = covered;
  g_st.st_pass2 = pass2;
  g_st.st_tris = 400 + 2 * 11 * 8;   // P17A fix 2: the random stream and the mesh (11 x 8 squares)
  g_st.st_device_ms = dev_ms;
  g_st.st_test_ms = test_ms;
  if (!pass) g_st.state = "self-test: " + result;
}
#endif  // _WIN32

std::string num(double v) {
  char b[64];
  std::snprintf(b, sizeof b, "%.3f", v);
  return b;
}

}  // namespace

void gpu_raster_enable(int mode) {
  const bool on = mode == 1 || mode == 2;
  {
    std::lock_guard<std::mutex> lk(g_mu);
    g_st.enabled = on;
    g_st.mode = on ? mode : 0;
    g_st.state = on ? "not started" : "off";
  }
#ifdef _WIN32
  if (on) {
    portsim::gl::set_pixel_gpu_factory([mode](int w, int h) -> std::shared_ptr<portsim::gl::GlPixelGpu> {
      // auto: the GPU raster only above 1920 x 1200 pixels (P15C's --object-rate auto limit): at 1920 x 1080 the CPU workers are at
      // least as fast with the GPU at idle clocks, at 4K the GPU gives back 2-3 cores (P16B_notes.md section 2)
      if (mode == 2 && static_cast<int64_t>(w) * h <= int64_t{1920} * 1200) {
        std::lock_guard<std::mutex> lk(g_mu);
        ++g_st.auto_off;
        if (!g_st.st_done) g_st.state = "auto: CPU raster at this size";
        return nullptr;
      }
      std::shared_ptr<Dev> d;
      bool test = false;
      {
        std::lock_guard<std::mutex> lk(g_mu);
        if (!g_dev) g_dev = std::make_shared<Dev>();
        d = g_dev;
        test = !g_st.st_done;
      }
      if (test) selftest(d);   // once, before the first backend (one thread makes them: the world thread)
      {
        std::lock_guard<std::mutex> lk(g_mu);
        if (!g_st.st_pass) return nullptr;   // a device that failed, or a self-test difference: the CPU raster
        ++g_st.auto_on;
      }
      return std::make_shared<D3DRaster>(d, false, part_setting());
    });
    return;
  }
#endif
  portsim::gl::set_pixel_gpu_factory(nullptr);
}

// ---- P17B (gpu_raster.h)
bool gpu_raster_device(void **dev, void **ctx, std::string &why) {
#ifdef _WIN32
  std::shared_ptr<Dev> d;
  {
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_dev) g_dev = std::make_shared<Dev>();
    d = g_dev;
  }
  if (!d->start()) {
    why = d->why;
    return false;
  }
  *dev = d->dev;
  *ctx = d->ctx;
  return true;
#else
  (void)dev;
  (void)ctx;
  why = "not Windows";
  return false;
#endif
}
void gpu_raster_context_wait() {
#ifdef _WIN32
  std::shared_ptr<Dev> d;
  {
    std::lock_guard<std::mutex> lk(g_mu);
    d = g_dev;
  }
  if (d && d->worker) d->worker->wait();
#endif
}
void *gpu_raster_frame(const portsim::gl::GlRaster &r) {
#ifdef _WIN32
  const D3DRaster *b = dynamic_cast<const D3DRaster *>(r.gpu_backend());
  if (!b || !r.gpu_current()) return nullptr;
  return b->frame_texture(r.width(), r.height());
#else
  (void)r;
  return nullptr;
#endif
}

std::string gpu_raster_json() {
  const portsim::gl::GlGpuStats r = portsim::gl::pixel_gpu_stats();
  std::lock_guard<std::mutex> lk(g_mu);
  const Stats &s = g_st;
  const double nb = s.batches ? static_cast<double>(s.batches) : 1.0;
#ifdef _WIN32
  const std::string p17 = ",\"part_tris\":" + std::to_string(part_setting()) + ",\"read_threads\":" + std::to_string(read_threads()) +
                          ",\"gpu_pairs\":" + std::string(gpu_pairs_setting() ? "true" : "false") +
                          ",\"stream\":" + std::string(stream_setting() ? "true" : "false");
#else
  const std::string p17;
#endif
  std::string o = "{\"enabled\":" + std::string(s.enabled ? "true" : "false") + ",\"state\":\"";
  for (char c : s.state) o += (c == '"' || c == '\\' || c < 0x20) ? ' ' : c;
  std::string st;
  for (char c : s.st_result) st += (c == '"' || c == '\\' || c < 0x20) ? ' ' : c;
  o += "\",\"mode\":\"" + std::string(s.mode == 2 ? "auto" : s.mode == 1 ? "on" : "off") + "\",\"auto_off\":" + std::to_string(s.auto_off) +
       ",\"backends\":" + std::to_string(s.auto_on) + ",\"selftest\":{\"done\":" + std::string(s.st_done ? "true" : "false") +
       ",\"pass\":" + std::string(s.st_pass ? "true" : "false") + ",\"result\":\"" + st + "\",\"corrupt_switch\":" +
       std::string(s.st_corrupt ? "true" : "false") + ",\"triangles\":" + std::to_string(s.st_tris) + ",\"words\":" + std::to_string(s.st_words) +
       ",\"differing\":" + std::to_string(s.st_differing) + ",\"pixel_writes\":" + std::to_string(s.st_covered) +
       ",\"pass2_writes\":" + std::to_string(s.st_pass2) + ",\"device_ms\":" + num(s.st_device_ms) + ",\"test_ms\":" + num(s.st_test_ms) + "}";
  o += ",\"force_exact\":" + std::string(s.force_exact ? "true" : "false") + ",\"batches\":" + std::to_string(s.batches) +
       ",\"tris\":" + std::to_string(s.tris) + ",\"pairs\":" + std::to_string(s.pairs) + ",\"groups\":" + std::to_string(s.groups) +
       ",\"dispatches\":" + std::to_string(s.dispatches) + ",\"slice\":" + std::to_string(s.slice) +
       ",\"covered_px\":" + std::to_string(s.covered) + ",\"exact_px\":" + std::to_string(s.exact) +
       ",\"uploads\":" + std::to_string(s.uploads) + ",\"clears\":" + std::to_string(s.clears) + ",\"tex_uploads\":" + std::to_string(s.tex_uploads) +
       ",\"tex_mb\":" + num(s.tex_bytes / 1e6) + ",\"tex_per_batch\":" + num(s.tex_count / nb) +
       ",\"failures\":" + std::to_string(s.failures) + ",\"bin_ms_mean\":" + num(s.bin_ns / 1e6 / nb) +
       ",\"upload_ms_mean\":" + num(s.upload_ns / 1e6 / nb) + ",\"wait_ms_mean\":" + num(s.wait_ns / 1e6 / nb) +
       ",\"read_ms_mean\":" + num(s.read_ns / 1e6 / nb) + ",\"draw_ms_mean\":" + num(s.draw_ns / 1e6 / nb) +
       ",\"draw_ms_max\":" + num(s.max_draw_ns / 1e6) +
       p17 + ",\"streamed\":" + std::to_string(s.streamed) + ",\"sub_rows_per_pass\":" + num(s.sub_rows / nb) +
       ",\"rows_per_pass\":" + num(s.pass_rows / nb) +
       ",\"parts\":" + std::to_string(s.parts) + ",\"parts_per_batch\":" + num(s.parts / nb) +
       ",\"part_ms_mean\":" + num(s.part_ns / 1e6 / nb) + ",\"join_ms_mean\":" + num(s.join_ns / 1e6 / nb) +
       ",\"map_ms_mean\":" + num(s.map_ns / 1e6 / nb) + ",\"copy_ms_mean\":" + num(s.copy_ns / 1e6 / nb) +
       ",\"gpu_span_ms_mean\":" + num(s.gpu_timed ? s.span_ns / 1e6 / static_cast<double>(s.gpu_timed) : 0.0) +
       ",\"gpu_ms_mean\":" + num(s.gpu_timed ? s.gpu_ns / 1e6 / static_cast<double>(s.gpu_timed) : 0.0) +
       ",\"gpu_ms_max\":" + num(s.max_gpu_ns / 1e6) +
       ",\"raster\":{\"batches\":" + std::to_string(r.batches) + ",\"tris\":" + std::to_string(r.tris) +
       ",\"islands\":" + std::to_string(r.islands) + ",\"dropped\":" + std::to_string(r.dropped) +
       ",\"not\":{\"kind\":" + std::to_string(r.not_kind) + ",\"area\":" + std::to_string(r.not_area) + ",\"range\":" + std::to_string(r.not_range) +
       ",\"colour\":" + std::to_string(r.not_colour) + "}" +
       ",\"uploads\":" + std::to_string(r.uploads) + ",\"row_uploads\":" + std::to_string(r.row_uploads) + ",\"clears\":" + std::to_string(r.clears) +
       ",\"failures\":" + std::to_string(r.failures) + ",\"fallback_tris\":" + std::to_string(r.fallback_tris) +
       ",\"submit_ms_mean\":" + num(r.batches ? r.submit_ns / 1e6 / static_cast<double>(r.batches) : 0.0) +
       ",\"parts\":" + std::to_string(r.parts) + ",\"part_ms_mean\":" + num(r.batches ? r.part_ns / 1e6 / static_cast<double>(r.batches) : 0.0) +
       ",\"check\":{\"passes\":" + std::to_string(r.checked) + ",\"bad\":" + std::to_string(r.check_bad) +
       ",\"bad_px\":" + std::to_string(r.check_badpx) + "}}}";
  return o;
}

}  // namespace portapp
