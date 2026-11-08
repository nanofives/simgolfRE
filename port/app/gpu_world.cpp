// P17B: the world thread's hr planes and world image on the GPU (gpu_world.h; log/roadmap/P17/P17B_notes.md section 1).
#include "gpu_world.h"

#include "gpu.h"
#include "gpu_raster.h"
#include "portsim/sim/gfx.h"
#include "portsim/sim/gl_raster.h"
#include "portsim/sim/perf.h"
#include "portsim/sim/terrain_live.h"

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
#endif

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace portapp {

namespace {

struct Stats {
  int mode = 0;   // 0 off, 1 on, 2 auto
  std::string state = "off";
  bool st_done = false, st_pass = false, st_corrupt = false;
  std::string st_result = "not run";
  int64_t st_words = 0, st_differing = 0;
  double st_ms = 0, st_compile_ms = 0;
  bool st_sync = false;                                         // fix 2: the preparation ran on the world thread
  double ready_ms = 0, first_ms = 0, backend_ms = 0;            // fix 2: since gpu_world_enable: shaders + self-test done, the backend made
  int64_t jobs_waiting = 0;                                     // fix 2: jobs that wanted the backend while the preparation ran (CPU path)
  int64_t resets = 0, bases_copied = 0, bases_uploaded = 0, base_rows = 0, flushes = 0, flush_blocks = 0, flush_recs = 0, list_entries = 0;
  int64_t nows = 0, hrcs = 0, box_resets = 0, composes = 0, roots_copied = 0, roots_uploaded = 0, cursor_reads = 0, failures = 0;
  int64_t base_ns = 0, flush_ns = 0, now_ns = 0, compose_ns = 0, wait_ns = 0, read_ns = 0, gpu_flush_ns = 0, gpu_flush_n = 0;
  int64_t gpu_comp_ns = 0, gpu_comp_n = 0;
  int64_t art_bytes = 0, table_bytes = 0;
  int64_t jobs = 0, jobs_gpu = 0, jobs_lost = 0, ops_ns = 0, image_ns = 0, job_ns = 0;
  int64_t img_compared = 0, img_differing = 0, img_differing_words = 0;
  int64_t pics_shared = 0, pics_unshared = 0, share_failures = 0;   // fix 2
  int64_t compares = 0, compare_ns = 0, compare_rows = 0, cmp_frames = 0, cmp_uploads = 0;   // fix 3: the queue starts' compares on the GPU
  int64_t base_reads = 0, base_read_ns = 0, base_read_px = 0;                                // fix 3: the published base tiles read back
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

// the entry points of this file, appended to P15C's CSObj text (gpu.h gpu_objects_hlsl: blit() and op_apply() are hr_blit_rows' per-pixel
// work and the five ops' bodies, its registers t0..t12 / u0; obase t11 is the base here)
const char kHlslWorld[] = R"HLSL(
// ---- P17B (gpu_world.cpp): the world thread's hr planes. Each plane is R32_UINT: the word in bits 0..15, the cover in bit 16 (hr, hrc);
// hrb holds hrb_lo | hrb_hi << 16, hrbc hrb_cov.
cbuffer WCB : register(b0) {
  int wpw; int wph; uint wplane; uint wrec;
  int wx0; int wy0; int wx1; int wy1;
  uint wnblk; uint wgx; int wsw; uint wflags;
};
StructuredBuffer<uint4> wblk : register(t13);   // per 8 x 8 block of a dirty tile: x0, y0, the tile's list offset, its count
Texture2D<uint> wroot : register(t14);
StructuredBuffer<uint> wruns : register(t15);
StructuredBuffer<int> wcol : register(t16);
StructuredBuffer<int> wrow : register(t17);
RWTexture2D<uint> whr : register(u1);
RWTexture2D<uint> whrc : register(u2);
RWTexture2D<uint> whrb : register(u3);
RWTexture2D<uint> whrbc : register(u4);
RWTexture2D<uint> wout : register(u5);

// hr_flush: a dirty tile's pixel starts from the base with no cover, then every command of the tile's list (queue order) whose box holds it
[numthreads(8, 8, 1)]
void CSFlush(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID) {
  uint bi = gid.y * wgx + gid.x;
  if (bi >= wnblk) return;
  uint4 t = wblk[bi];
  int X = (int)t.x + (int)tid.x, Y = (int)t.y + (int)tid.y;
  if (X >= wpw || Y >= wph) return;
  uint d = obase.Load(int3(X, Y, 0)) & 0xffffu;
  uint cov = 0u;
  [loop] for (uint i = 0u; i < t.w; ++i) {
    ORec R = orecs[olist[t.z + i]];
    if (X < R.x0 || X >= R.x1 || Y < R.y0 || Y >= R.y1) continue;
    if (blit(R, X, Y, d)) cov = 0x10000u;
  }
  whr[int2(X, Y)] = d | cov;
}
// hr_now / hr_box_now: record wrec over its box (wx0..wy1) into hr (wplane 0), hrc (1) or hrc, hrb_lo, hrb_hi with hrb_cov (2)
[numthreads(8, 8, 1)]
void CSNow(uint3 dt : SV_DispatchThreadID) {
  int X = wx0 + (int)dt.x, Y = wy0 + (int)dt.y;
  if (X >= wx1 || Y >= wy1) return;
  ORec R = orecs[wrec];
  int2 p = int2(X, Y);
  // the words drawn over: hr's or hrc's (one), or hrc's, hrb_lo's and hrb_hi's (three: hr_box_now's planes in its order); one blit() call
  // site (each inlined copy of blit() costs minutes of compile time)
  uint c = wplane == 0u ? whr[p] : whrc[p];
  uint b = wplane == 2u ? whrb[p] : 0u;
  uint v0 = c & 0xffffu, v1 = b & 0xffffu, v2 = b >> 16;
  uint n = wplane == 2u ? 3u : 1u;
  uint drawn = 0u;
  [loop] for (uint i = 0u; i < n; ++i) {
    uint d = i == 0u ? v0 : (i == 1u ? v1 : v2);
    if (blit(R, X, Y, d)) {
      drawn |= 1u << i;
      if (i == 0u) v0 = d;
      else if (i == 1u) v1 = d;
      else v2 = d;
    }
  }
  if (wplane == 0u) {
    if (drawn != 0u) whr[p] = v0 | 0x10000u;
  } else if (wplane == 1u) {
    if (drawn != 0u) whrc[p] = v0 | 0x10000u;
  } else {
    whrc[p] = v0 | (c & 0x10000u);
    whrb[p] = v1 | (v2 << 16);
    if (drawn != 0u) whrbc[p] = 1u;
  }
}
// hrc = hr with no cover (hr_now's first cursor-pass draw)
[numthreads(8, 8, 1)]
void CSHrc(uint3 dt : SV_DispatchThreadID) {
  int X = (int)dt.x, Y = (int)dt.y;
  if (X >= wpw || Y >= wph) return;
  whrc[int2(X, Y)] = whr[int2(X, Y)] & 0xffffu;
}
// the box planes' rectangle (wx0..wy1) back to hrb_lo 0, hrb_hi 0x7fff, hrb_cov 0
[numthreads(8, 8, 1)]
void CSBoxClr(uint3 dt : SV_DispatchThreadID) {
  int X = wx0 + (int)dt.x, Y = wy0 + (int)dt.y;
  if (X >= wx1 || Y >= wy1) return;
  whrb[int2(X, Y)] = 0x7fff0000u;
  whrbc[int2(X, Y)] = 0u;
}
// fix 3: hr_queue_begin's compare: per row, the first and the last column where the root differs from the base (atomics; the row
// untouched = no difference)
RWStructuredBuffer<uint> wrmin : register(u6);
RWStructuredBuffer<uint> wrmax : register(u7);
[numthreads(64, 1, 1)]
void CSCmp(uint3 dt : SV_DispatchThreadID) {
  int X = (int)dt.x, Y = (int)dt.y;
  if (X >= wpw || Y >= wph) return;
  if ((wroot.Load(int3(X, Y, 0)) & 0xffffu) != (obase.Load(int3(X, Y, 0)) & 0xffffu)) {
    InterlockedMin(wrmin[Y], (uint)X);
    InterlockedMax(wrmax[Y], (uint)X);
  }
}
// world_frame_of: the run of the logical pixel when there is one, else hrc where laid and covered, else hr where laid and covered, else root
[numthreads(8, 8, 1)]
void CSCompose(uint3 dt : SV_DispatchThreadID) {
  int X = (int)dt.x, Y = (int)dt.y;
  if (X >= wpw || Y >= wph) return;
  int2 p = int2(X, Y);
  uint v = wroot.Load(int3(X, Y, 0)) & 0xffffu;
  if ((wflags & 1u) != 0u) {
    uint h = whr[p];
    if ((h & 0x10000u) != 0u) v = h & 0xffffu;
  }
  if ((wflags & 2u) != 0u) {
    uint h = whrc[p];
    if ((h & 0x10000u) != 0u) v = h & 0xffffu;
  }
  if ((wflags & 4u) != 0u) {
    int lx = wcol[X];
    if (lx >= 0) {
      uint r = wruns[(uint)wrow[Y] * (uint)wsw + (uint)lx];
      if ((r & 0x10000u) != 0u) v = r & 0xffffu;
    }
  }
  wout[p] = v;
}
)HLSL";

struct OOpRec {  // gpu.cpp kHlslObj OOp
  uint32_t kind, art_off, art_w, pal_off, lut_off, lut_n, plane_k, flag, colour, steps_off, nsteps, pad;
};
struct ORecRec {  // gpu.cpp kHlslObj ORec
  uint32_t op;
  int32_t nsx, nsy, filter;
  uint32_t inv_lo, inv_hi;
  int32_t x0, y0, x1, y1;
  uint32_t col_off, row_off;
};
struct WCB {
  int32_t pw, ph;
  uint32_t plane, rec;
  int32_t x0, y0, x1, y1;
  uint32_t nblk, gx;
  int32_t sw;
  uint32_t flags;
};
static_assert(sizeof(OOpRec) == 48 && sizeof(ORecRec) == 48 && sizeof(WCB) == 48, "the HLSL layouts");

// a buffer for per-call data: structured (stride) or typed (fmt), DEFAULT usage, grown when needed, filled with UpdateSubresource
struct Buf {
  ID3D11Buffer *b = nullptr;
  ID3D11ShaderResourceView *srv = nullptr;
  size_t cap = 0;   // bytes
  ~Buf() {
    rel(srv);
    rel(b);
  }
  bool put(ID3D11Device *dev, ID3D11DeviceContext *ctx, const void *data, size_t bytes, uint32_t stride, DXGI_FORMAT fmt) {
    const size_t need = std::max<size_t>(bytes, 64);
    if (!b || need > cap) {
      rel(srv);
      rel(b);
      size_t c = std::max<size_t>(need + need / 2, 4096);
      c = (c + stride - 1) / stride * stride;
      D3D11_BUFFER_DESC d = {};
      d.ByteWidth = static_cast<UINT>(c);
      d.Usage = D3D11_USAGE_DEFAULT;
      d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
      d.MiscFlags = fmt == DXGI_FORMAT_UNKNOWN ? D3D11_RESOURCE_MISC_BUFFER_STRUCTURED : 0;
      d.StructureByteStride = fmt == DXGI_FORMAT_UNKNOWN ? stride : 0;
      if (FAILED(dev->CreateBuffer(&d, nullptr, &b))) return false;
      D3D11_SHADER_RESOURCE_VIEW_DESC s = {};
      s.Format = fmt;
      s.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
      s.Buffer.FirstElement = 0;
      s.Buffer.NumElements = static_cast<UINT>(c / stride);
      if (FAILED(dev->CreateShaderResourceView(b, &s, &srv))) return false;
      cap = c;
    }
    if (bytes) {
      D3D11_BOX box = {0, 0, 0, static_cast<UINT>(bytes), 1, 1};
      ctx->UpdateSubresource(b, 0, &box, data, 0, 0);
    }
    return true;
  }
};

// a pw x ph texture: R32_UINT with a UAV (the planes), R16_UINT with an SRV (base, root), R16_UINT with a UAV (outputs), or staging
struct Tex2 {
  ID3D11Texture2D *t = nullptr;
  ID3D11UnorderedAccessView *uav = nullptr;
  ID3D11ShaderResourceView *srv = nullptr;
  int w = 0, h = 0;
  ~Tex2() { reset(); }
  void reset() {
    rel(uav);
    rel(srv);
    rel(t);
    w = h = 0;
  }
  // kind 0 plane, 1 SRV, 2 output, 3 staging
  bool make(ID3D11Device *dev, int ww, int hh, int kind) {
    if (t && w == ww && h == hh) return true;
    reset();
    D3D11_TEXTURE2D_DESC d = {};
    d.Width = static_cast<UINT>(ww);
    d.Height = static_cast<UINT>(hh);
    d.MipLevels = 1;
    d.ArraySize = 1;
    d.Format = kind == 0 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
    d.SampleDesc.Count = 1;
    d.Usage = kind == 3 ? D3D11_USAGE_STAGING : D3D11_USAGE_DEFAULT;
    d.BindFlags = kind == 0 || kind == 2 ? D3D11_BIND_UNORDERED_ACCESS : kind == 1 ? D3D11_BIND_SHADER_RESOURCE : 0;
    d.CPUAccessFlags = kind == 3 ? D3D11_CPU_ACCESS_READ : 0;
    if (FAILED(dev->CreateTexture2D(&d, nullptr, &t))) return false;
    if ((kind == 0 || kind == 2) && FAILED(dev->CreateUnorderedAccessView(t, nullptr, &uav))) return false;
    if (kind == 1 && FAILED(dev->CreateShaderResourceView(t, nullptr, &srv))) return false;
    w = ww;
    h = hh;
    return true;
  }
};

// the five entry points of CSObj's text + kHlslWorld compiled (D3DCompile, the app's flags) into bytecode; false: why
bool compile_world_shaders(std::vector<std::vector<uint8_t>> &code, std::string &why) {
  code.clear();
  HMODULE dc = LoadLibraryW(L"d3dcompiler_47.dll");
  if (!dc) {
    why = "no d3dcompiler_47.dll";
    return false;
  }
  auto compile = reinterpret_cast<pD3DCompile>(reinterpret_cast<void *>(GetProcAddress(dc, "D3DCompile")));
  if (!compile) {
    why = "no D3DCompile";
    return false;
  }
  size_t n = 0;
  const char *obj = gpu_objects_hlsl(n);
  const std::string text = std::string(obj, n) + kHlslWorld;
  const char *entry[6] = {"CSFlush", "CSNow", "CSHrc", "CSBoxClr", "CSCompose", "CSCmp"};
  for (int i = 0; i < 6; ++i) {
    ID3DBlob *b = nullptr, *err = nullptr;
    const HRESULT hr = compile(text.data(), text.size(), "p17b.hlsl", nullptr, nullptr, entry[i], "cs_5_0",
                               D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_IEEE_STRICTNESS, 0, &b, &err);
    if (FAILED(hr) || !b) {
      why = std::string(entry[i]) + " failed" + (err ? std::string(": ") + static_cast<const char *>(err->GetBufferPointer()) : "");
      rel(b);
      rel(err);
      return false;
    }
    rel(err);
    const uint8_t *bp = static_cast<const uint8_t *>(b->GetBufferPointer());
    code.emplace_back(bp, bp + b->GetBufferSize());
    rel(b);
  }
  return true;
}

class D3DWorld final : public portsim::GfxHrGpu {
 public:
  D3DWorld(ID3D11Device *dev, ID3D11DeviceContext *ctx, int mode, bool selftest) : dev_(dev), ctx_(ctx), mode_(mode), selftest_(selftest) {
    const char *c = std::getenv("SG_P17_SELFTEST_CORRUPT");
    corrupt_ = selftest && c && c[0] == '1';
  }
  ~D3DWorld() override {
    rel(cs_flush_); rel(cs_now_); rel(cs_hrc_); rel(cs_boxclr_); rel(cs_compose_); rel(cs_cmp_); rel(cb_);
    for (int k = 0; k < 2; ++k) {
      rel(rows_uav_[k]);
      rel(rows_b_[k]);
      rel(rows_rd_[k]);
    }
    rel(q_dis_); rel(q_a_); rel(q_b_); rel(q_c_); rel(q_d_);
  }
  // the shaders from the bytecode compile_world_shaders made (fix 2: on the preparation thread, once), the constant buffer, the queries
  bool start(std::string &why, const std::vector<std::vector<uint8_t>> &code) {
    ID3D11ComputeShader **out[6] = {&cs_flush_, &cs_now_, &cs_hrc_, &cs_boxclr_, &cs_compose_, &cs_cmp_};
    if (code.size() != 6) return fail(why, "no shader bytecode");
    for (int i = 0; i < 6; ++i)
      if (FAILED(dev_->CreateComputeShader(code[static_cast<size_t>(i)].data(), code[static_cast<size_t>(i)].size(), nullptr, out[i])))
        return fail(why, "CreateComputeShader " + std::to_string(i));
    return rest(why);
  }
  bool rest(std::string &why) {
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(WCB);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(dev_->CreateBuffer(&bd, nullptr, &cb_))) return fail(why, "constant buffer");
    size_t pn = 0;
    const uint16_t *pt = portsim::gfx_hr_plane_table(pn);
    if (!planes_tab_.put(dev_, ctx_, pt, pn * 2, 2, DXGI_FORMAT_R16_UINT)) return fail(why, "plane table");
    D3D11_QUERY_DESC q = {D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
    dev_->CreateQuery(&q, &q_dis_);
    q.Query = D3D11_QUERY_TIMESTAMP;
    dev_->CreateQuery(&q, &q_a_);
    dev_->CreateQuery(&q, &q_b_);
    dev_->CreateQuery(&q, &q_c_);
    dev_->CreateQuery(&q, &q_d_);
    return true;
  }

  // ---- portsim::GfxHrGpu
  bool want(int32_t pw, int32_t ph) override {
    if (dead_) return false;
    if (selftest_ || mode_ == 1) return true;
    return mode_ == 2 && static_cast<int64_t>(pw) * ph > int64_t{1920} * 1200;
  }
  bool planes(int32_t pw, int32_t ph) const override { return !dead_ && hr_.t && hr_.w == pw && hr_.h == ph; }
  bool reset(int32_t pw, int32_t ph) override {
    if (dead_) return false;
    gpu_raster_context_wait();   // P17A's submit thread idle (it shares this context)
    pw_ = pw;
    ph_ = ph;
    if (!hr_.make(dev_, pw, ph, 0)) return die("hr plane");
    const UINT z[4] = {0, 0, 0, 0};
    ctx_->ClearUnorderedAccessViewUint(hr_.uav, z);
    count(&Stats::resets);
    return true;
  }
  bool base(const portsim::LiveTerrainWorld &w, const std::vector<uint16_t> &root, const std::vector<int32_t> &spans, bool all) override {
    if (dead_) return false;
    gpu_raster_context_wait();   // P17A's submit thread idle (it shares this context)
    const int64_t t0 = portsim::perf_now_ns();
    if (!base_.make(dev_, pw_, ph_, 1)) return die("base texture");
    if (root.size() != static_cast<size_t>(pw_) * static_cast<size_t>(ph_)) return die("base size");
    // the base = last_base = the root after hr_queue_begin's copy; the raster's GPU frame when `root` is that raster's current frame
    ID3D11Texture2D *f = nullptr;
    if (w.raster && &w.raster->cpixels() == &root && w.raster->width() == pw_ && w.raster->height() == ph_)
      f = static_cast<ID3D11Texture2D *>(gpu_raster_frame(*w.raster));
    int64_t rows = 0;
    if (f) {
      ctx_->CopyResource(base_.t, f);
      count(&Stats::bases_copied);
    } else if (all) {
      ctx_->UpdateSubresource(base_.t, 0, nullptr, root.data(), static_cast<UINT>(pw_) * 2u, 0);
      rows = ph_;
      count(&Stats::bases_uploaded);
    } else if (!spans.empty()) {   // the rectangle holding every changed span (root equals last_base everywhere after the copy)
      int32_t y0 = ph_, y1 = -1, x0 = pw_, x1 = 0;
      for (size_t i = 0; i + 2 < spans.size(); i += 3) {
        y0 = std::min(y0, spans[i]);
        y1 = std::max(y1, spans[i]);
        x0 = std::min(x0, spans[i + 1]);
        x1 = std::max(x1, spans[i + 2]);
      }
      if (y1 >= y0 && x1 > x0) {
        D3D11_BOX b = {static_cast<UINT>(x0), static_cast<UINT>(y0), 0, static_cast<UINT>(x1), static_cast<UINT>(y1 + 1), 1};
        ctx_->UpdateSubresource(base_.t, 0, &b, root.data() + static_cast<size_t>(y0) * static_cast<size_t>(pw_) + static_cast<size_t>(x0),
                                static_cast<UINT>(pw_) * 2u, 0);
        rows = y1 + 1 - y0;
      }
      count(&Stats::bases_uploaded);
    }
    if (selftest_) return true;
    std::lock_guard<std::mutex> lk(g_mu);
    g_st.base_rows += rows;
    g_st.base_ns += portsim::perf_now_ns() - t0;
    return true;
  }
  bool flush(const std::vector<uint8_t> &dirty, int32_t tx, int32_t ty, const portsim::GfxHrGpuBatch &b) override {
    if (dead_) return false;
    gpu_raster_context_wait();   // P17A's submit thread idle (it shares this context)
    const int64_t t0 = portsim::perf_now_ns();
    if (!hr_.t || !base_.t) return die("flush without planes");
    const int32_t T = portsim::kGfxHrTile;
    // per dirty tile the records whose box meets it, in queue order (CSR), and its 8 x 8 blocks
    const size_t nt = static_cast<size_t>(tx) * static_cast<size_t>(ty);
    if (dirty.size() != nt) return die("dirty tiles");
    cnt_.assign(nt + 1, 0);
    for (const portsim::GfxHrGpuRec &r : b.recs)
      for (int32_t j = r.y0 / T; j <= (r.y1 - 1) / T; ++j)
        for (int32_t i = r.x0 / T; i <= (r.x1 - 1) / T; ++i) {
          const size_t k = static_cast<size_t>(j) * static_cast<size_t>(tx) + static_cast<size_t>(i);
          if (dirty[k]) ++cnt_[k];
        }
    off_.resize(nt);
    uint32_t acc = 0;
    for (size_t k = 0; k < nt; ++k) {
      off_[k] = acc;
      acc += cnt_[k];
      cnt_[k] = off_[k];
    }
    list_.resize(acc);
    for (size_t ri = 0; ri < b.recs.size(); ++ri) {
      const portsim::GfxHrGpuRec &r = b.recs[ri];
      for (int32_t j = r.y0 / T; j <= (r.y1 - 1) / T; ++j)
        for (int32_t i = r.x0 / T; i <= (r.x1 - 1) / T; ++i) {
          const size_t k = static_cast<size_t>(j) * static_cast<size_t>(tx) + static_cast<size_t>(i);
          if (dirty[k]) list_[cnt_[k]++] = static_cast<uint32_t>(ri);
        }
    }
    blk_.clear();
    for (int32_t j = 0; j < ty; ++j)
      for (int32_t i = 0; i < tx; ++i) {
        const size_t k = static_cast<size_t>(j) * static_cast<size_t>(tx) + static_cast<size_t>(i);
        if (!dirty[k]) continue;
        const uint32_t n = (k + 1 < nt ? off_[k + 1] : acc) - off_[k];
        for (int32_t by = 0; by < T; by += 8)
          for (int32_t bx = 0; bx < T; bx += 8) {
            const int32_t x = i * T + bx, y = j * T + by;
            if (x >= pw_ || y >= ph_) continue;
            blk_.insert(blk_.end(), {static_cast<uint32_t>(x), static_cast<uint32_t>(y), off_[k], n});
          }
      }
    if (!blk_.empty()) {
      if (!put_batch(b)) return false;
      if (!list_buf_.put(dev_, ctx_, list_.data(), list_.size() * 4, 4, DXGI_FORMAT_UNKNOWN) ||
          !blk_buf_.put(dev_, ctx_, blk_.data(), blk_.size() * 4, 16, DXGI_FORMAT_UNKNOWN))
        return die("flush buffers");
      const uint32_t nb = static_cast<uint32_t>(blk_.size() / 4);
      const uint32_t gx = std::min<uint32_t>(nb, 65535u), gy = (nb + gx - 1) / gx;
      WCB c = {pw_, ph_, 0, 0, 0, 0, 0, 0, nb, gx, 0, 0};
      ctx_->UpdateSubresource(cb_, 0, nullptr, &c, 0, 0);
      const bool timed = timing_open();
      if (timed) ctx_->End(q_a_);
      bind(cs_flush_);
      ctx_->Dispatch(gx, gy, 1);
      unbind();
      if (timed) {
        ctx_->End(q_b_);
        have_flush_ = true;
      }
      if (!selftest_) {
        std::lock_guard<std::mutex> lk(g_mu);
        g_st.flush_blocks += nb;
        g_st.list_entries += static_cast<int64_t>(list_.size());
      }
    }
    if (selftest_) return true;
    std::lock_guard<std::mutex> lk(g_mu);
    ++g_st.flushes;
    g_st.flush_recs += static_cast<int64_t>(b.recs.size());
    g_st.flush_ns += portsim::perf_now_ns() - t0;
    return true;
  }
  bool make_hrc() override {
    if (dead_) return false;
    gpu_raster_context_wait();   // P17A's submit thread idle (it shares this context)
    if (!hr_.t || !hrc_.make(dev_, pw_, ph_, 0)) return die("hrc plane");
    WCB c = {pw_, ph_, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    ctx_->UpdateSubresource(cb_, 0, nullptr, &c, 0, 0);
    bind(cs_hrc_);
    ctx_->Dispatch(static_cast<UINT>((pw_ + 7) / 8), static_cast<UINT>((ph_ + 7) / 8), 1);
    unbind();
    count(&Stats::hrcs);
    return true;
  }
  bool reset_box(bool /*make*/, const int32_t area[4]) override {
    if (dead_) return false;
    gpu_raster_context_wait();   // P17A's submit thread idle (it shares this context)
    const bool fresh = !(hrb_.t && hrb_.w == pw_ && hrb_.h == ph_ && hrbc_.t && hrbc_.w == pw_ && hrbc_.h == ph_);
    if (fresh) {   // made: hrb_lo 0, hrb_hi 0x7fff, hrb_cov 0 everywhere
      if (!hrb_.make(dev_, pw_, ph_, 0) || !hrbc_.make(dev_, pw_, ph_, 0)) return die("box planes");
      const UINT v[4] = {0x7fff0000u, 0x7fff0000u, 0x7fff0000u, 0x7fff0000u}, z[4] = {0, 0, 0, 0};
      ctx_->ClearUnorderedAccessViewUint(hrb_.uav, v);
      ctx_->ClearUnorderedAccessViewUint(hrbc_.uav, z);
    } else if (area[2] > area[0] && area[3] > area[1]) {   // the last boxes' rectangle only (the planes hold their reset values outside it)
      WCB c = {pw_, ph_, 0, 0, area[0], area[1], area[2], area[3], 0, 0, 0, 0};
      ctx_->UpdateSubresource(cb_, 0, nullptr, &c, 0, 0);
      bind(cs_boxclr_);
      ctx_->Dispatch(static_cast<UINT>((area[2] - area[0] + 7) / 8), static_cast<UINT>((area[3] - area[1] + 7) / 8), 1);
      unbind();
    }
    count(&Stats::box_resets);
    return true;
  }
  bool now(int plane, const portsim::GfxHrGpuBatch &b) override {
    if (dead_) return false;
    gpu_raster_context_wait();   // P17A's submit thread idle (it shares this context)
    const int64_t t0 = portsim::perf_now_ns();
    if (b.recs.empty()) return true;
    if (!hr_.t || (plane >= 1 && !hrc_.t) || (plane == 2 && (!hrb_.t || !hrbc_.t))) return die("now without planes");
    if (!put_batch(b)) return false;
    const portsim::GfxHrGpuRec &r = b.recs[0];
    WCB c = {pw_, ph_, static_cast<uint32_t>(plane), 0, r.x0, r.y0, r.x1, r.y1, 0, 0, 0, 0};
    ctx_->UpdateSubresource(cb_, 0, nullptr, &c, 0, 0);
    bind(cs_now_);
    ctx_->Dispatch(static_cast<UINT>((r.x1 - r.x0 + 7) / 8), static_cast<UINT>((r.y1 - r.y0 + 7) / 8), 1);
    unbind();
    if (selftest_) return true;
    std::lock_guard<std::mutex> lk(g_mu);
    ++g_st.nows;
    g_st.now_ns += portsim::perf_now_ns() - t0;
    return true;
  }
  bool read(std::vector<uint16_t> *hr, std::vector<uint8_t> *hr_cov, std::vector<uint16_t> *hrc, std::vector<uint8_t> *hrc_cov,
            std::vector<uint16_t> *lo, std::vector<uint16_t> *hi, std::vector<uint8_t> *cov) override {
    if (dead_) return false;
    gpu_raster_context_wait();   // P17A's submit thread idle (it shares this context)
    const size_t np = static_cast<size_t>(pw_) * static_cast<size_t>(ph_);
    auto rd = [&](Tex2 &src, auto fn) -> bool {
      if (!src.t) return false;
      if (!stage32_.t || stage32_.w != pw_ || stage32_.h != ph_) {
        stage32_.reset();
        D3D11_TEXTURE2D_DESC d = {};
        src.t->GetDesc(&d);
        d.Usage = D3D11_USAGE_STAGING;
        d.BindFlags = 0;
        d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (FAILED(dev_->CreateTexture2D(&d, nullptr, &stage32_.t))) return false;
        stage32_.w = pw_;
        stage32_.h = ph_;
      }
      ctx_->CopyResource(stage32_.t, src.t);
      D3D11_MAPPED_SUBRESOURCE m;
      if (FAILED(ctx_->Map(stage32_.t, 0, D3D11_MAP_READ, 0, &m))) return false;
      for (int32_t y = 0; y < ph_; ++y) {
        const uint32_t *row = reinterpret_cast<const uint32_t *>(static_cast<const uint8_t *>(m.pData) + static_cast<size_t>(y) * m.RowPitch);
        for (int32_t x = 0; x < pw_; ++x) fn(static_cast<size_t>(y) * static_cast<size_t>(pw_) + static_cast<size_t>(x), row[x]);
      }
      ctx_->Unmap(stage32_.t, 0);
      return true;
    };
    if (hr || hr_cov) {
      if (hr) hr->assign(np, 0);
      if (hr_cov) hr_cov->assign(np, 0);
      if (!rd(hr_, [&](size_t i, uint32_t v) {
            if (hr) (*hr)[i] = static_cast<uint16_t>(v & 0xffffu);
            if (hr_cov) (*hr_cov)[i] = static_cast<uint8_t>((v >> 16) & 1u);
          }))
        return die("read hr");
    }
    if ((hrc || hrc_cov) && hrc_.t) {
      if (hrc) hrc->assign(np, 0);
      if (hrc_cov) hrc_cov->assign(np, 0);
      if (!rd(hrc_, [&](size_t i, uint32_t v) {
            if (hrc) (*hrc)[i] = static_cast<uint16_t>(v & 0xffffu);
            if (hrc_cov) (*hrc_cov)[i] = static_cast<uint8_t>((v >> 16) & 1u);
          }))
        return die("read hrc");
    } else {
      if (hrc) hrc->clear();
      if (hrc_cov) hrc_cov->clear();
    }
    if ((lo || hi) && hrb_.t) {
      if (lo) lo->assign(np, 0);
      if (hi) hi->assign(np, 0);
      if (!rd(hrb_, [&](size_t i, uint32_t v) {
            if (lo) (*lo)[i] = static_cast<uint16_t>(v & 0xffffu);
            if (hi) (*hi)[i] = static_cast<uint16_t>(v >> 16);
          }))
        return die("read hrb");
    } else {
      if (lo) lo->clear();
      if (hi) hi->clear();
    }
    if (cov && hrbc_.t) {
      cov->assign(np, 0);
      if (!rd(hrbc_, [&](size_t i, uint32_t v) { (*cov)[i] = static_cast<uint8_t>(v); })) return die("read hrb_cov");
    } else if (cov) {
      cov->clear();
    }
    if (corrupt_ && hr && !hr->empty()) (*hr)[hr->size() / 2] ^= 1u;   // the forced mismatch (self-test only)
    return true;
  }
  void dead(const char *why) override { die(why ? why : "the caller saw a failure"); }

  // ---- fix 3: hr_queue_begin's compare and the published base tiles
  int compare(const portsim::LiveTerrainWorld &w, const std::vector<uint16_t> *root, std::vector<int32_t> &spans) override {
    spans.clear();
    if (dead_) return -1;
    gpu_raster_context_wait();   // P17A's submit thread idle (it shares this context)
    const int64_t t0 = portsim::perf_now_ns();
    const int32_t pw = pw_, ph = ph_;
    const size_t np = static_cast<size_t>(pw) * static_cast<size_t>(ph);
    if (!base_.t || base_.w != pw || base_.h != ph || !root_.make(dev_, pw, ph, 1)) return die("compare textures") ? 1 : -1;
    if (rows_n_ != ph) {
      for (int k = 0; k < 2; ++k) {
        rel(rows_uav_[k]);
        rel(rows_b_[k]);
        rel(rows_rd_[k]);
        D3D11_BUFFER_DESC d = {};
        d.ByteWidth = static_cast<UINT>(ph) * 4u;
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
        d.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        d.StructureByteStride = 4;
        if (FAILED(dev_->CreateBuffer(&d, nullptr, &rows_b_[k])) || FAILED(dev_->CreateUnorderedAccessView(rows_b_[k], nullptr, &rows_uav_[k])))
          return die("compare buffers") ? 1 : -1;
        d.Usage = D3D11_USAGE_STAGING;
        d.BindFlags = 0;
        d.MiscFlags = 0;
        d.StructureByteStride = 0;
        d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (FAILED(dev_->CreateBuffer(&d, nullptr, &rows_rd_[k]))) return die("compare buffers") ? 1 : -1;
      }
      rows_n_ = ph;
    }
    // the root: the raster's GPU frame when current, else the CPU words
    ID3D11Texture2D *f = nullptr;
    if (!root && w.raster && w.raster->width() == pw && w.raster->height() == ph) f = static_cast<ID3D11Texture2D *>(gpu_raster_frame(*w.raster));
    if (f) {
      ctx_->CopyResource(root_.t, f);
      count(&Stats::cmp_frames);
    } else {
      const std::vector<uint16_t> *r = root ? root : w.raster ? &w.raster->cpixels() : nullptr;
      if (!r || r->size() != np) return die("compare root") ? 1 : -1;
      ctx_->UpdateSubresource(root_.t, 0, nullptr, r->data(), static_cast<UINT>(pw) * 2u, 0);
      count(&Stats::cmp_uploads);
    }
    const UINT hi[4] = {0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu}, lo[4] = {0, 0, 0, 0};
    ctx_->ClearUnorderedAccessViewUint(rows_uav_[0], hi);
    ctx_->ClearUnorderedAccessViewUint(rows_uav_[1], lo);
    WCB c = {pw, ph, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    ctx_->UpdateSubresource(cb_, 0, nullptr, &c, 0, 0);
    ID3D11ShaderResourceView *b11 = base_.srv, *b14 = root_.srv;
    ctx_->CSSetShaderResources(11, 1, &b11);
    ctx_->CSSetShaderResources(14, 1, &b14);
    ctx_->CSSetUnorderedAccessViews(6, 2, rows_uav_, nullptr);
    ctx_->CSSetConstantBuffers(0, 1, &cb_);
    ctx_->CSSetShader(cs_cmp_, nullptr, 0);
    ctx_->Dispatch(static_cast<UINT>((pw + 63) / 64), static_cast<UINT>(ph), 1);
    ID3D11ShaderResourceView *ns = nullptr;
    ID3D11UnorderedAccessView *nu[2] = {nullptr, nullptr};
    ctx_->CSSetShaderResources(11, 1, &ns);
    ctx_->CSSetShaderResources(14, 1, &ns);
    ctx_->CSSetUnorderedAccessViews(6, 2, nu, nullptr);
    ctx_->CSSetShader(nullptr, nullptr, 0);
    ctx_->CopyResource(base_.t, root_.t);   // the base = the root (hr_queue_begin's copy into last_base, everywhere)
    ctx_->CopyResource(rows_rd_[0], rows_b_[0]);
    ctx_->CopyResource(rows_rd_[1], rows_b_[1]);
    D3D11_MAPPED_SUBRESOURCE m0, m1;
    if (FAILED(ctx_->Map(rows_rd_[0], 0, D3D11_MAP_READ, 0, &m0))) return die("compare readback") ? 1 : -1;
    if (FAILED(ctx_->Map(rows_rd_[1], 0, D3D11_MAP_READ, 0, &m1))) {
      ctx_->Unmap(rows_rd_[0], 0);
      return die("compare readback") ? 1 : -1;
    }
    const uint32_t *mn = static_cast<const uint32_t *>(m0.pData), *mx = static_cast<const uint32_t *>(m1.pData);
    for (int32_t y = 0; y < ph; ++y)
      if (mn[y] != 0xffffffffu) spans.insert(spans.end(), {y, static_cast<int32_t>(mn[y]), static_cast<int32_t>(mx[y]) + 1});
    ctx_->Unmap(rows_rd_[0], 0);
    ctx_->Unmap(rows_rd_[1], 0);
    if (!selftest_) {
      std::lock_guard<std::mutex> lk(g_mu);
      ++g_st.compares;
      g_st.compare_ns += portsim::perf_now_ns() - t0;
      g_st.compare_rows += static_cast<int64_t>(spans.size() / 3);
    }
    return 1;
  }
  bool read_base(const std::vector<uint8_t> &tiles, int32_t tx, int32_t ty, uint16_t *dst) override {
    if (dead_) return false;
    gpu_raster_context_wait();   // P17A's submit thread idle (it shares this context)
    const int64_t t0 = portsim::perf_now_ns();
    const int32_t T = portsim::kGfxHrTile;
    if (!base_.t || tiles.size() != static_cast<size_t>(tx) * static_cast<size_t>(ty) || !stageb_.make(dev_, pw_, ph_, 3)) return die("base tiles");
    int64_t n = 0;
    for (int32_t j = 0; j < ty; ++j)   // runs of flagged tiles of each tile row copied into the staging texture at their place
      for (int32_t i = 0; i < tx;) {
        if (!tiles[static_cast<size_t>(j) * static_cast<size_t>(tx) + static_cast<size_t>(i)]) {
          ++i;
          continue;
        }
        int32_t k = i + 1;
        while (k < tx && tiles[static_cast<size_t>(j) * static_cast<size_t>(tx) + static_cast<size_t>(k)]) ++k;
        const UINT x0 = static_cast<UINT>(i * T), x1 = static_cast<UINT>(std::min(pw_, k * T));
        const UINT y0 = static_cast<UINT>(j * T), y1 = static_cast<UINT>(std::min(ph_, (j + 1) * T));
        D3D11_BOX b = {x0, y0, 0, x1, y1, 1};
        ctx_->CopySubresourceRegion(stageb_.t, 0, x0, y0, 0, base_.t, 0, &b);
        n += static_cast<int64_t>(x1 - x0) * static_cast<int64_t>(y1 - y0);
        i = k;
      }
    if (n) {
      D3D11_MAPPED_SUBRESOURCE m;
      if (FAILED(ctx_->Map(stageb_.t, 0, D3D11_MAP_READ, 0, &m))) return die("base tiles readback");
      for (int32_t j = 0; j < ty; ++j)
        for (int32_t i = 0; i < tx; ++i) {
          if (!tiles[static_cast<size_t>(j) * static_cast<size_t>(tx) + static_cast<size_t>(i)]) continue;
          const int32_t x0 = i * T, x1 = std::min(pw_, x0 + T);
          for (int32_t y = j * T; y < std::min(ph_, (j + 1) * T); ++y)
            std::memcpy(dst + static_cast<size_t>(y) * static_cast<size_t>(pw_) + static_cast<size_t>(x0),
                        static_cast<const uint8_t *>(m.pData) + static_cast<size_t>(y) * m.RowPitch + static_cast<size_t>(x0) * 2u,
                        static_cast<size_t>(x1 - x0) * 2u);
        }
      ctx_->Unmap(stageb_.t, 0);
    }
    if (!selftest_) {
      std::lock_guard<std::mutex> lk(g_mu);
      ++g_st.base_reads;
      g_st.base_read_ns += portsim::perf_now_ns() - t0;
      g_st.base_read_px += n;
    }
    return true;
  }

  // ---- the world image
  bool compose(const portsim::LiveTerrainWorld &w, const std::vector<uint16_t> *root, const portsim::WorldFramePlan &p, std::vector<uint16_t> &px,
               const portsim::WorldFramePlan *pt, std::vector<uint16_t> *pxt, std::shared_ptr<const GpuWorldPic> *pic) {
    if (dead_) return false;
    gpu_raster_context_wait();   // P17A's submit thread idle (it shares this context)
    const int64_t t0 = portsim::perf_now_ns();
    const int32_t pw = p.pw, ph = p.ph;
    const size_t np = static_cast<size_t>(pw) * static_cast<size_t>(ph);
    // fix 2: root null = w's raster: its GPU frame when current, else its CPU words (GlRaster::cpixels: read back on demand)
    if (!root && !(w.raster && w.raster->width() == pw && w.raster->height() == ph)) return false;
    if (pw <= 0 || ph <= 0 || (root && root->size() != np)) return false;
    if ((p.lay_hr || p.lay_hrc) && !(hr_.t && hr_.w == pw && hr_.h == ph)) return die("compose without planes");
    if (p.lay_hrc && !(hrc_.t && hrc_.w == pw && hrc_.h == ph)) return die("compose without hrc");
    if (pic) pic->reset();
    // fix 2: the outputs are a free slot's shared textures (its keyed mutexes acquired), else this backend's own
    Slot *sl = share_ ? take_slot(pw, ph, pt && pxt) : nullptr;
    // fix 3: the composition writes the backend's own textures (writing the shared ones raised its GPU time 7.6 -> 19.2 ms at 4K) and
    // the slot gets one GPU copy of each
    if (!root_.make(dev_, pw, ph, 1) || !stage_[0].make(dev_, pw, ph, 3) || !out_[0].make(dev_, pw, ph, 2)) return die("compose textures");
    if (pt && pxt && (!stage_[1].make(dev_, pw, ph, 3) || !out_[1].make(dev_, pw, ph, 2))) return die("compose textures");
    ID3D11Texture2D *f = root ? nullptr : static_cast<ID3D11Texture2D *>(gpu_raster_frame(*w.raster));
    if (f) {
      ctx_->CopyResource(root_.t, f);
      count(&Stats::roots_copied);
    } else {
      const std::vector<uint16_t> &r = root ? *root : w.raster->cpixels();
      if (r.size() != np) return false;
      ctx_->UpdateSubresource(root_.t, 0, nullptr, r.data(), static_cast<UINT>(pw) * 2u, 0);
      count(&Stats::roots_uploaded);
    }
    const bool timed = timing_open();
    if (timed) ctx_->End(q_c_);
    const portsim::WorldFramePlan *plans[2] = {&p, pt && pxt ? pt : nullptr};
    for (int k = 0; k < 2; ++k) {
      const portsim::WorldFramePlan *q = plans[k];
      if (!q) continue;
      uint32_t flags = (q->lay_hr ? 1u : 0u) | (q->lay_hrc ? 2u : 0u);
      if (!q->runs.empty() && q->colmap.size() == static_cast<size_t>(pw) && q->rowmap.size() == static_cast<size_t>(ph)) {
        flags |= 4u;
        if (!runs_[k].put(dev_, ctx_, q->runs.data(), q->runs.size() * 4, 4, DXGI_FORMAT_UNKNOWN) ||
            !col_[k].put(dev_, ctx_, q->colmap.data(), q->colmap.size() * 4, 4, DXGI_FORMAT_UNKNOWN) ||
            !row_[k].put(dev_, ctx_, q->rowmap.data(), q->rowmap.size() * 4, 4, DXGI_FORMAT_UNKNOWN))
          return die("compose buffers");
      }
      WCB c = {pw, ph, 0, 0, 0, 0, 0, 0, 0, 0, q->scene_w, flags};
      ctx_->UpdateSubresource(cb_, 0, nullptr, &c, 0, 0);
      ID3D11ShaderResourceView *srv[4] = {root_.srv, runs_[k].srv, col_[k].srv, row_[k].srv};
      ctx_->CSSetShaderResources(14, 4, srv);
      ID3D11UnorderedAccessView *u[5] = {hr_.uav, hrc_.uav, nullptr, nullptr, out_[k].uav};
      ctx_->CSSetUnorderedAccessViews(1, 5, u, nullptr);
      ctx_->CSSetConstantBuffers(0, 1, &cb_);
      ctx_->CSSetShader(cs_compose_, nullptr, 0);
      ctx_->Dispatch(static_cast<UINT>((pw + 7) / 8), static_cast<UINT>((ph + 7) / 8), 1);
      ID3D11ShaderResourceView *ns[4] = {};
      ID3D11UnorderedAccessView *nu[5] = {};
      ctx_->CSSetShaderResources(14, 4, ns);
      ctx_->CSSetUnorderedAccessViews(1, 5, nu, nullptr);
      ctx_->CSSetShader(nullptr, nullptr, 0);
      ctx_->CopyResource(stage_[k].t, out_[k].t);
      if (sl) ctx_->CopyResource(sl->t[k], out_[k].t);   // fix 3
    }
    if (sl) {   // released to the window (key 1); the slot is held by the picture until the window's WorldImage lets it go
      auto g = std::make_shared<GpuWorldPic>();
      g->w = pw;
      g->h = ph;
      for (int k = 0; k < 2; ++k) {
        if (!plans[k] || !sl->km[k]) continue;
        sl->km[k]->ReleaseSync(1);
        sl->key[k] = 1;
        g->handle[k] = sl->h[k];
        g->key[k] = &sl->key[k];
      }
      g->slot = std::shared_ptr<void>(static_cast<void *>(sl), [](void *v) { static_cast<Slot *>(v)->busy.store(false); });
      if (pic) *pic = std::move(g);
      count(&Stats::pics_shared);
    } else if (share_) {
      count(&Stats::pics_unshared);
    }
    if (timed) {
      ctx_->End(q_d_);
      have_comp_ = true;
    }
    if (dis_open_) {   // closed before the readback's wait, read after it
      ctx_->End(q_dis_);
      dis_open_ = false;
      dis_closed_ = true;
    }
    // the readback: the wait for the GPU (the flush, the draws and the composition), then the rows
    const int64_t tw = portsim::perf_now_ns();
    int64_t wait = 0;
    for (int k = 0; k < 2; ++k) {
      if (!plans[k]) continue;
      std::vector<uint16_t> &dst = k == 0 ? px : *pxt;
      dst.resize(np);
      D3D11_MAPPED_SUBRESOURCE m;
      const int64_t tm = portsim::perf_now_ns();
      if (FAILED(ctx_->Map(stage_[k].t, 0, D3D11_MAP_READ, 0, &m))) return die("compose readback");
      wait += portsim::perf_now_ns() - tm;
      for (int32_t y = 0; y < ph; ++y)
        std::memcpy(dst.data() + static_cast<size_t>(y) * static_cast<size_t>(pw), static_cast<const uint8_t *>(m.pData) + static_cast<size_t>(y) * m.RowPitch,
                    static_cast<size_t>(pw) * 2u);
      ctx_->Unmap(stage_[k].t, 0);
    }
    if (FAILED(dev_->GetDeviceRemovedReason())) return die("device removed");
    collect_timing();
    std::lock_guard<std::mutex> lk(g_mu);
    ++g_st.composes;
    g_st.wait_ns += wait;
    g_st.read_ns += portsim::perf_now_ns() - tw - wait;
    g_st.compose_ns += portsim::perf_now_ns() - t0;
    return true;
  }
  bool read_cursor(portsim::LiveTerrainWorld &w) {
    count(&Stats::cursor_reads);
    std::vector<uint16_t> lo, hi;
    std::vector<uint8_t> bc;
    if (!read(nullptr, nullptr, &w.hrc, &w.hrc_cov, w.hrb_valid ? &lo : nullptr, w.hrb_valid ? &hi : nullptr, w.hrb_valid ? &bc : nullptr))
      return false;
    if (w.hrb_valid) {
      w.hrb_lo.swap(lo);
      w.hrb_hi.swap(hi);
      w.hrb_cov.swap(bc);
    }
    return true;
  }
  bool is_dead() const { return dead_; }
  void share(bool on) { if (!on) share_ = false; }   // (a failure turns it off for the session; the switch only off)

 private:
  static bool fail(std::string &why, const std::string &w) {
    why = w;
    return false;
  }
  bool die(const std::string &w) {
    if (!dead_) {
      dead_ = true;
      if (!selftest_) {
        gpu_raster_lazy(false);   // P17A fix 3: the CPU path reads the raster's frame: the readback after every pass again
        std::fprintf(stderr, "P17 GPU WORLD off: %s\n", w.c_str());
        std::lock_guard<std::mutex> lk(g_mu);
        ++g_st.failures;
        g_st.state = w;
      }
    }
    return false;
  }
  void count(int64_t Stats::*f) {
    if (selftest_) return;
    std::lock_guard<std::mutex> lk(g_mu);
    ++(g_st.*f);
  }
  // the batch's ops (art, palettes, shade tables, alpha programs), records and tables
  bool put_batch(const portsim::GfxHrGpuBatch &b) {
    ops_.clear();
    art_.clear();
    pal_.clear();
    lut_.clear();
    steps_.clear();
    art_at_.clear();
    lut_at_.clear();
    for (const portsim::GfxHrGpuOp &o : b.ops) {
      OOpRec r = {};
      r.kind = static_cast<uint32_t>(o.kind);
      uint32_t at = ~0u;
      for (const auto &a : art_at_)
        if (a.first == o.art) at = a.second;
      if (at == ~0u) {
        at = static_cast<uint32_t>(art_.size());
        art_.insert(art_.end(), o.art, o.art + static_cast<size_t>(o.art_w) * static_cast<size_t>(o.art_h));
        if (art_at_.size() < 64) art_at_.emplace_back(o.art, at);   // (a short list: draws of one shared art are adjacent)
      }
      r.art_off = at;
      r.art_w = static_cast<uint32_t>(o.art_w);
      r.pal_off = static_cast<uint32_t>(pal_.size());
      pal_.insert(pal_.end(), o.pal, o.pal + 256);
      if (o.kind == 2 && o.lut && o.lut_n) {
        uint32_t lt = ~0u;
        for (const auto &a : lut_at_)
          if (a.first == o.lut) lt = a.second;
        if (lt == ~0u) {
          lt = static_cast<uint32_t>(lut_.size());
          lut_.insert(lut_.end(), o.lut, o.lut + o.lut_n);
          lut_at_.emplace_back(o.lut, lt);
        }
        r.lut_off = lt;
      }
      r.lut_n = o.kind == 2 ? o.lut_n : 0u;
      r.plane_k = o.plane_k;
      r.flag = o.flag;
      r.colour = o.colour;
      r.steps_off = static_cast<uint32_t>(steps_.size() / 4);
      r.nsteps = static_cast<uint32_t>(o.steps.size() / 4);
      steps_.insert(steps_.end(), o.steps.begin(), o.steps.end());
      ops_.push_back(r);
    }
    recs_.resize(b.recs.size());
    for (size_t i = 0; i < b.recs.size(); ++i) {
      const portsim::GfxHrGpuRec &s = b.recs[i];
      ORecRec &r = recs_[i];
      r.op = static_cast<uint32_t>(s.op);
      r.nsx = s.nsx;
      r.nsy = s.nsy;
      r.filter = s.filter;
      uint64_t bits = 0;
      std::memcpy(&bits, &s.inv, sizeof bits);
      r.inv_lo = static_cast<uint32_t>(bits & 0xffffffffull);
      r.inv_hi = static_cast<uint32_t>(bits >> 32);
      r.x0 = s.x0;
      r.y0 = s.y0;
      r.x1 = s.x1;
      r.y1 = s.y1;
      r.col_off = s.col_off;
      r.row_off = s.row_off;
    }
    if (pal_.empty()) pal_.push_back(0);
    if (lut_.empty()) lut_.push_back(0);
    if (art_.empty()) art_.push_back(0xff);
    const bool ok = ops_buf_.put(dev_, ctx_, ops_.data(), ops_.size() * sizeof(OOpRec), sizeof(OOpRec), DXGI_FORMAT_UNKNOWN) &&
                    recs_buf_.put(dev_, ctx_, recs_.data(), recs_.size() * sizeof(ORecRec), sizeof(ORecRec), DXGI_FORMAT_UNKNOWN) &&
                    cols_buf_.put(dev_, ctx_, b.cols.data(), b.cols.size() * 4, 16, DXGI_FORMAT_UNKNOWN) &&
                    rows_buf_.put(dev_, ctx_, b.rows.data(), b.rows.size() * 4, 16, DXGI_FORMAT_UNKNOWN) &&
                    art_buf_.put(dev_, ctx_, art_.data(), art_.size(), 1, DXGI_FORMAT_R8_UINT) &&
                    pal_buf_.put(dev_, ctx_, pal_.data(), pal_.size() * 2, 2, DXGI_FORMAT_R16_UINT) &&
                    lut_buf_.put(dev_, ctx_, lut_.data(), lut_.size() * 2, 2, DXGI_FORMAT_R16_UINT) &&
                    steps_buf_.put(dev_, ctx_, steps_.data(), steps_.size() * 4, 16, DXGI_FORMAT_UNKNOWN);
    if (!ok) return die("batch buffers");
    if (!selftest_) {
      std::lock_guard<std::mutex> lk(g_mu);
      g_st.art_bytes += static_cast<int64_t>(art_.size());
      g_st.table_bytes += static_cast<int64_t>((b.cols.size() + b.rows.size()) * 4);
    }
    return true;
  }
  // CSObj's registers t1..t11 (t0 oregs and t12 oworld0 unused here) and t13; the planes u1..u4
  void bind(ID3D11ComputeShader *cs) {
    ID3D11ShaderResourceView *srv[14] = {nullptr, recs_buf_.srv, ops_buf_.srv, cols_buf_.srv, rows_buf_.srv, list_buf_.srv, art_buf_.srv,
                                         pal_buf_.srv, lut_buf_.srv, planes_tab_.srv, steps_buf_.srv, base_.srv, nullptr, blk_buf_.srv};
    ctx_->CSSetShaderResources(0, 14, srv);
    ID3D11UnorderedAccessView *u[4] = {hr_.uav, hrc_.uav, hrb_.uav, hrbc_.uav};
    ctx_->CSSetUnorderedAccessViews(1, 4, u, nullptr);
    ctx_->CSSetConstantBuffers(0, 1, &cb_);
    ctx_->CSSetShader(cs, nullptr, 0);
  }
  void unbind() {
    ID3D11ShaderResourceView *ns[14] = {};
    ID3D11UnorderedAccessView *nu[4] = {};
    ctx_->CSSetShaderResources(0, 14, ns);
    ctx_->CSSetUnorderedAccessViews(1, 4, nu, nullptr);
    ctx_->CSSetShader(nullptr, nullptr, 0);
  }
  // GPU time of the flush and of the composition: one timestamp pair each inside one disjoint query per job, closed before the
  // composition's readback and read after it
  bool timing_open() {
    if (selftest_ || !q_dis_ || !q_a_ || !q_b_ || !q_c_ || !q_d_) return false;
    if (!dis_open_) {
      ctx_->Begin(q_dis_);
      dis_open_ = true;
      have_flush_ = have_comp_ = false;
    }
    return true;
  }
  void collect_timing() {
    if (!dis_closed_) return;
    dis_closed_ = false;
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj = {};
    UINT64 a = 0, b = 0, c = 0, d = 0;
    if (ctx_->GetData(q_dis_, &dj, sizeof dj, 0) != S_OK || dj.Disjoint || !dj.Frequency) return;
    const double f = 1e9 / static_cast<double>(dj.Frequency);
    std::lock_guard<std::mutex> lk(g_mu);
    if (have_flush_ && ctx_->GetData(q_a_, &a, sizeof a, 0) == S_OK && ctx_->GetData(q_b_, &b, sizeof b, 0) == S_OK && b >= a) {
      g_st.gpu_flush_ns += static_cast<int64_t>(static_cast<double>(b - a) * f);
      ++g_st.gpu_flush_n;
    }
    if (have_comp_ && ctx_->GetData(q_c_, &c, sizeof c, 0) == S_OK && ctx_->GetData(q_d_, &d, sizeof d, 0) == S_OK && d >= c) {
      g_st.gpu_comp_ns += static_cast<int64_t>(static_cast<double>(d - c) * f);
      ++g_st.gpu_comp_n;
    }
  }

  ID3D11Device *dev_;
  ID3D11DeviceContext *ctx_;
  int mode_;
  bool selftest_, corrupt_ = false, dead_ = false;
  int32_t pw_ = 0, ph_ = 0;
  ID3D11ComputeShader *cs_flush_ = nullptr, *cs_now_ = nullptr, *cs_hrc_ = nullptr, *cs_boxclr_ = nullptr, *cs_compose_ = nullptr;
  ID3D11ComputeShader *cs_cmp_ = nullptr;   // fix 3
  ID3D11Buffer *rows_b_[2] = {nullptr, nullptr}, *rows_rd_[2] = {nullptr, nullptr};   // fix 3: per row the min / max differing column
  ID3D11UnorderedAccessView *rows_uav_[2] = {nullptr, nullptr};
  int32_t rows_n_ = 0;
  Tex2 stageb_;   // fix 3: the base's tiles read back
  ID3D11Buffer *cb_ = nullptr;
  ID3D11Query *q_dis_ = nullptr, *q_a_ = nullptr, *q_b_ = nullptr, *q_c_ = nullptr, *q_d_ = nullptr;
  bool dis_open_ = false, dis_closed_ = false, have_flush_ = false, have_comp_ = false;
  Tex2 hr_, hrc_, hrb_, hrbc_, base_, root_, out_[2], stage_[2], stage32_;
  // fix 2: the pool of shared picture pairs (GpuWorldPic). A slot is busy from its composition until the picture's last reference is
  // dropped (the window's WorldImage); 4 cover the window's current and previous frames, a published one and the one being composed.
  struct Slot {
    ID3D11Texture2D *t[2] = {nullptr, nullptr};
    ID3D11UnorderedAccessView *u[2] = {nullptr, nullptr};
    IDXGIKeyedMutex *km[2] = {nullptr, nullptr};
    void *h[2] = {nullptr, nullptr};
    int key[2] = {0, 0};
    int w = 0, hh = 0;
    std::atomic<bool> busy{false};
    void reset() {
      for (int k = 0; k < 2; ++k) {
        rel(km[k]);
        rel(u[k]);
        rel(t[k]);
        h[k] = nullptr;
        key[k] = 0;
      }
      w = hh = 0;
    }
    ~Slot() { reset(); }
  };
  Slot slots_[4];
  bool share_ = true;
  // a free slot of w x h (made again at another size), its keyed mutexes of the textures used acquired with their next keys; null: none
  Slot *take_slot(int32_t w, int32_t h, bool text) {
    for (Slot &s : slots_) {
      bool expect = false;
      if (!s.busy.compare_exchange_strong(expect, true)) continue;
      if (s.w != w || s.hh != h || !s.t[0]) {
        s.reset();
        bool ok = true;
        for (int k = 0; k < 2 && ok; ++k) {
          D3D11_TEXTURE2D_DESC d = {};
          d.Width = static_cast<UINT>(w);
          d.Height = static_cast<UINT>(h);
          d.MipLevels = 1;
          d.ArraySize = 1;
          d.Format = DXGI_FORMAT_R16_UINT;
          d.SampleDesc.Count = 1;
          d.Usage = D3D11_USAGE_DEFAULT;
          d.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
          d.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
          IDXGIResource *r = nullptr;
          HANDLE hd = nullptr;
          ok = SUCCEEDED(dev_->CreateTexture2D(&d, nullptr, &s.t[k])) && SUCCEEDED(dev_->CreateUnorderedAccessView(s.t[k], nullptr, &s.u[k])) &&
               SUCCEEDED(s.t[k]->QueryInterface(__uuidof(IDXGIKeyedMutex), reinterpret_cast<void **>(&s.km[k]))) &&
               SUCCEEDED(s.t[k]->QueryInterface(__uuidof(IDXGIResource), reinterpret_cast<void **>(&r))) && SUCCEEDED(r->GetSharedHandle(&hd)) && hd;
          rel(r);
          s.h[k] = hd;
        }
        if (!ok) {
          s.reset();
          s.busy.store(false);
          share_ = false;   // sharing is not available: the window uploads as before for the rest of the session
          count(&Stats::share_failures);
          return nullptr;
        }
        s.w = w;
        s.hh = h;
      }
      for (int k = 0; k < (text ? 2 : 1); ++k)
        if (s.km[k]->AcquireSync(static_cast<UINT64>(s.key[k]), 100) != S_OK) {
          for (int j = 0; j < k; ++j) s.km[j]->ReleaseSync(static_cast<UINT64>(s.key[j]));
          s.busy.store(false);
          count(&Stats::share_failures);
          return nullptr;
        }
      return &s;
    }
    return nullptr;
  }
  Buf ops_buf_, recs_buf_, cols_buf_, rows_buf_, art_buf_, pal_buf_, lut_buf_, steps_buf_, planes_tab_, list_buf_, blk_buf_;
  Buf runs_[2], col_[2], row_[2];
  std::vector<OOpRec> ops_;
  std::vector<ORecRec> recs_;
  std::vector<uint8_t> art_;
  std::vector<uint16_t> pal_, lut_;
  std::vector<uint32_t> steps_, cnt_, off_, list_, blk_;
  std::vector<std::pair<const uint8_t *, uint32_t>> art_at_;
  std::vector<std::pair<const uint16_t *, uint32_t>> lut_at_;
};

std::unique_ptr<D3DWorld> g_be;   // the world thread's (made and used by that thread only)
bool g_tried = false;
int g_mode = 0;
bool g_share = true;   // fix 2: SG_P17_SHARE
#endif  // _WIN32

std::string esc(const std::string &s) {
  std::string o;
  for (char c : s) o += (c == '"' || c == '\\' || c < 0x20) ? ' ' : c;
  return o;
}
std::string num(double v) {
  char b[64];
  std::snprintf(b, sizeof b, "%.3f", v);
  return b;
}

}  // namespace

#ifdef _WIN32
namespace {
// fix 2: THE PREPARATION. The shaders' compile (seconds) and the START-UP SELF-TEST run on a thread of their own, started by
// gpu_world_enable at the app's start, with a Direct3D 11 device of their own on the same (default) adapter; until it is done the world
// thread keeps the CPU path (every picture published, exact), then makes its backend from the bytecode on its device. With
// SG_P17_PREP_SYNC=1 (development, fix 1's way) the preparation runs on the world thread at its first job that wants the backend.
struct Prep {
  std::mutex m;
  std::thread th;
  std::atomic<int> state{0};   // 0 not started, 1 running, 2 done
  bool pass = false;
  std::vector<std::vector<uint8_t>> code;
  ~Prep() {
    if (th.joinable()) th.join();
  }
};
Prep g_prep;
int64_t g_t0 = 0;   // gpu_world_enable's time (the app's start)
bool g_sync = false;

void prep_run() {
  const int64_t t0 = portsim::perf_now_ns();
  std::vector<std::vector<uint8_t>> code;
  std::string why, result;
  int64_t words = 0, differing = 0;
  bool pass = false;
  double compile_ms = 0;
  if (!compile_world_shaders(code, why)) {
    result = "shaders: " + why;
  } else {
    compile_ms = static_cast<double>(portsim::perf_now_ns() - t0) / 1e6;
    ID3D11Device *dev = nullptr;
    ID3D11DeviceContext *ctx = nullptr;
    const D3D_FEATURE_LEVEL want[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL got = D3D_FEATURE_LEVEL_10_0;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, want, 2, D3D11_SDK_VERSION, &dev, &got, &ctx);
    if (FAILED(hr)) hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, want + 1, 1, D3D11_SDK_VERSION, &dev, &got, &ctx);
    D3D11_FEATURE_DATA_DOUBLES fd = {};
    D3D11_FEATURE_DATA_D3D11_OPTIONS fo = {};
    if (FAILED(hr) || !dev) {
      result = "device: no D3D11 hardware device at feature level 11";
    } else if (!(SUCCEEDED(dev->CheckFeatureSupport(D3D11_FEATURE_DOUBLES, &fd, sizeof fd)) && fd.DoublePrecisionFloatShaderOps) ||
               !(SUCCEEDED(dev->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &fo, sizeof fo)) && fo.ExtendedDoublesShaderInstructions)) {
      result = "device: no extended double-precision shader ops";
    } else {
      D3DWorld test(dev, ctx, 1, true);
      if (!test.start(why, code)) result = "self-test backend: " + why;
      else pass = portsim::gfx_hr_gpu_selftest(test, result, words, differing);
    }
    if (ctx) ctx->ClearState();
    rel(ctx);
    rel(dev);
  }
  const double ms = static_cast<double>(portsim::perf_now_ns() - t0) / 1e6;
  const double since = static_cast<double>(portsim::perf_now_ns() - g_t0) / 1e6;
  const char *c = std::getenv("SG_P17_SELFTEST_CORRUPT");
  std::fprintf(stderr, "P17 GPU WORLD self-test: %s (%.1f ms, of which the shader compile %.1f; ready %.1f ms after the start, %s)%s\n",
               result.c_str(), ms, compile_ms, since, g_sync ? "on the world thread" : "on the preparation thread",
               pass ? "" : ": the CPU path for this session");
  {
    std::lock_guard<std::mutex> lk(g_mu);
    g_st.st_done = true;
    g_st.st_pass = pass;
    g_st.st_corrupt = c && c[0] == '1';
    g_st.st_result = result;
    g_st.st_words = words;
    g_st.st_differing = differing;
    g_st.st_ms = ms;
    g_st.st_compile_ms = compile_ms;
    g_st.ready_ms = since;
    g_st.st_sync = g_sync;
    if (!pass) g_st.state = "self-test: " + result;
  }
  {
    std::lock_guard<std::mutex> lk(g_prep.m);
    g_prep.pass = pass;
    g_prep.code = std::move(code);
  }
  g_prep.state.store(2);
}
}  // namespace
#endif

void gpu_world_enable(int mode) {
  std::lock_guard<std::mutex> lk(g_mu);
  g_st.mode = mode;
  g_st.state = mode ? "not started" : "off";
#ifdef _WIN32
  g_mode = mode;
  const char *e = std::getenv("SG_P17_SHARE");   // fix 2: development switch (0 = the pictures not shared)
  g_share = !(e && e[0] == '0');
  g_t0 = portsim::perf_now_ns();
  const char *ps = std::getenv("SG_P17_PREP_SYNC");
  g_sync = ps && ps[0] == '1';
  if (mode != 0 && !g_sync && g_prep.state.load() == 0) {   // fix 2: the preparation at once, beside the app's start
    g_prep.state.store(1);
    g_prep.th = std::thread([] {
      SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
      prep_run();
    });
  }
#endif
}

portsim::GfxHrGpu *gpu_world_backend(int32_t pw, int32_t ph) {
#ifdef _WIN32
  if (g_be) return g_be->is_dead() ? nullptr : g_be.get();
  if (g_tried || g_mode == 0) return nullptr;
  if (g_mode == 2 && static_cast<int64_t>(pw) * ph <= int64_t{1920} * 1200) return nullptr;   // auto: not made below the limit
  if (g_sync && g_prep.state.load() == 0) {   // SG_P17_PREP_SYNC=1: fix 1's way, on this thread
    g_prep.state.store(1);
    prep_run();
  }
  if (g_prep.state.load() != 2) {   // the preparation still runs: the CPU path for this job
    std::lock_guard<std::mutex> lk(g_mu);
    ++g_st.jobs_waiting;
    return nullptr;
  }
  g_tried = true;
  bool pass;
  std::vector<std::vector<uint8_t>> code;
  {
    std::lock_guard<std::mutex> lk(g_prep.m);
    pass = g_prep.pass;
    code = g_prep.code;
  }
  if (!pass) return nullptr;
  void *dv = nullptr, *cx = nullptr;
  std::string why;
  if (!gpu_raster_device(&dv, &cx, why)) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_st.state = "device: " + why;
    std::fprintf(stderr, "P17 GPU WORLD off: device: %s\n", why.c_str());
    return nullptr;
  }
  const int64_t tb = portsim::perf_now_ns();
  auto be = std::make_unique<D3DWorld>(static_cast<ID3D11Device *>(dv), static_cast<ID3D11DeviceContext *>(cx), g_mode, false);
  if (!be->start(why, code)) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_st.state = "shaders: " + why;
    return nullptr;
  }
  {
    std::lock_guard<std::mutex> lk(g_mu);
    g_st.state = "on";
    g_st.backend_ms = static_cast<double>(portsim::perf_now_ns() - tb) / 1e6;
    g_st.first_ms = static_cast<double>(portsim::perf_now_ns() - g_t0) / 1e6;
  }
  g_be = std::move(be);
  gpu_raster_lazy(true);   // P17A fix 3: the raster's frame stays on the GPU (this backend reads it there)
  return g_be.get();
#else
  (void)pw;
  (void)ph;
  return nullptr;
#endif
}

void gpu_world_image_check(int64_t differing) {
  std::lock_guard<std::mutex> lk(g_mu);
  ++g_st.img_compared;
  if (differing) {
    if (!g_st.img_differing) std::fprintf(stderr, "P17 WORLDCHECK: a world image differs from the CPU's (%lld words / flags)\n", static_cast<long long>(differing));
    ++g_st.img_differing;
    g_st.img_differing_words += differing;
  }
}

bool gpu_world_compose(const portsim::LiveTerrainWorld &w, const std::vector<uint16_t> *root, const portsim::WorldFramePlan &p,
                       std::vector<uint16_t> &px, const portsim::WorldFramePlan *pt, std::vector<uint16_t> *px_text,
                       std::shared_ptr<const GpuWorldPic> *pic) {
#ifdef _WIN32
  if (g_be) g_be->share(g_share);
  return g_be && !g_be->is_dead() && g_be->compose(w, root, p, px, pt, px_text, pic);
#else
  (void)w; (void)root; (void)p; (void)px; (void)pt; (void)px_text; (void)pic;
  return false;
#endif
}

void gpu_world_share(bool on) {
#ifdef _WIN32
  g_share = on;
#else
  (void)on;
#endif
}

bool gpu_world_read_cursor(portsim::LiveTerrainWorld &w) {
#ifdef _WIN32
  return g_be && !g_be->is_dead() && g_be->read_cursor(w);
#else
  (void)w;
  return false;
#endif
}

void gpu_world_job(int64_t ops_ns, int64_t image_ns, int64_t job_ns, bool gpu, bool lost) {
  std::lock_guard<std::mutex> lk(g_mu);
  ++g_st.jobs;
  if (gpu) {
    ++g_st.jobs_gpu;
    g_st.ops_ns += ops_ns;
    g_st.image_ns += image_ns;
    g_st.job_ns += job_ns;
  }
  if (lost) ++g_st.jobs_lost;
}

std::string gpu_world_json() {
  const portsim::GfxHrGpuCheck &h = portsim::gfx_hr_gpu_stats();
  std::lock_guard<std::mutex> lk(g_mu);
  const Stats &s = g_st;
  auto per = [](int64_t ns, int64_t n) { return num(n ? static_cast<double>(ns) / 1e6 / static_cast<double>(n) : 0.0); };
  return "{\"mode\":\"" + std::string(s.mode == 2 ? "auto" : s.mode == 1 ? "on" : "off") + "\",\"state\":\"" + esc(s.state) +
         "\",\"selftest\":{\"done\":" + (s.st_done ? "true" : "false") + ",\"pass\":" + (s.st_pass ? "true" : "false") + ",\"result\":\"" +
         esc(s.st_result) + "\",\"corrupt_switch\":" + (s.st_corrupt ? "true" : "false") + ",\"words\":" + std::to_string(s.st_words) +
         ",\"differing\":" + std::to_string(s.st_differing) + ",\"ms\":" + num(s.st_ms) + ",\"compile_ms\":" + num(s.st_compile_ms) + ",\"on_world_thread\":" + (s.st_sync ? "true" : "false") +
         "},\"ready_ms\":" + num(s.ready_ms) + ",\"first_backend_ms\":" + num(s.first_ms) + ",\"backend_make_ms\":" + num(s.backend_ms) +
         ",\"jobs_waiting\":" + std::to_string(s.jobs_waiting) +
         ",\"jobs\":" + std::to_string(s.jobs) + ",\"jobs_gpu\":" + std::to_string(s.jobs_gpu) + ",\"jobs_lost\":" + std::to_string(s.jobs_lost) +
         ",\"ops_ms_mean\":" + per(s.ops_ns, s.jobs_gpu) + ",\"image_ms_mean\":" + per(s.image_ns, s.jobs_gpu) +
         ",\"job_ms_mean\":" + per(s.job_ns, s.jobs_gpu) + ",\"resets\":" + std::to_string(s.resets) +
         ",\"bases_copied\":" + std::to_string(s.bases_copied) + ",\"bases_uploaded\":" + std::to_string(s.bases_uploaded) +
         ",\"base_rows\":" + std::to_string(s.base_rows) + ",\"base_ms_mean\":" + per(s.base_ns, s.bases_copied + s.bases_uploaded) +
         ",\"flushes\":" + std::to_string(s.flushes) + ",\"flush_ms_mean\":" + per(s.flush_ns, s.flushes) +
         ",\"flush_blocks_mean\":" + num(s.flushes ? static_cast<double>(s.flush_blocks) / static_cast<double>(s.flushes) : 0.0) +
         ",\"flush_recs_mean\":" + num(s.flushes ? static_cast<double>(s.flush_recs) / static_cast<double>(s.flushes) : 0.0) +
         ",\"list_entries_mean\":" + num(s.flushes ? static_cast<double>(s.list_entries) / static_cast<double>(s.flushes) : 0.0) +
         ",\"nows\":" + std::to_string(s.nows) + ",\"now_ms_mean\":" + per(s.now_ns, s.nows) + ",\"hrcs\":" + std::to_string(s.hrcs) +
         ",\"box_resets\":" + std::to_string(s.box_resets) + ",\"composes\":" + std::to_string(s.composes) +
         ",\"compose_ms_mean\":" + per(s.compose_ns, s.composes) + ",\"wait_ms_mean\":" + per(s.wait_ns, s.composes) +
         ",\"read_ms_mean\":" + per(s.read_ns, s.composes) + ",\"roots_copied\":" + std::to_string(s.roots_copied) +
         ",\"roots_uploaded\":" + std::to_string(s.roots_uploaded) + ",\"gpu_flush_ms_mean\":" + per(s.gpu_flush_ns, s.gpu_flush_n) +
         ",\"gpu_compose_ms_mean\":" + per(s.gpu_comp_ns, s.gpu_comp_n) +
         ",\"cursor_reads\":" + std::to_string(s.cursor_reads) + ",\"pics_shared\":" + std::to_string(s.pics_shared) +
         ",\"pics_unshared\":" + std::to_string(s.pics_unshared) + ",\"share_failures\":" + std::to_string(s.share_failures) +
         ",\"compares\":" + std::to_string(s.compares) + ",\"compare_ms_mean\":" + per(s.compare_ns, s.compares) +
         ",\"compare_rows_mean\":" + num(s.compares ? static_cast<double>(s.compare_rows) / static_cast<double>(s.compares) : 0.0) +
         ",\"compare_frames\":" + std::to_string(s.cmp_frames) + ",\"compare_uploads\":" + std::to_string(s.cmp_uploads) +
         ",\"base_reads\":" + std::to_string(s.base_reads) + ",\"base_read_ms_mean\":" + per(s.base_read_ns, s.base_reads) +
         ",\"base_read_px_mean\":" + num(s.base_reads ? static_cast<double>(s.base_read_px) / static_cast<double>(s.base_reads) : 0.0) + ",\"failures\":" + std::to_string(s.failures) +
         ",\"art_mb\":" + num(static_cast<double>(s.art_bytes) / 1e6) + ",\"table_mb\":" + num(static_cast<double>(s.table_bytes) / 1e6) +
         ",\"sim\":{\"flushes\":" + std::to_string(h.flushes) + ",\"nows\":" + std::to_string(h.nows) + ",\"batch_cmds\":" +
         std::to_string(h.batch_cmds) + ",\"table_entries\":" + std::to_string(h.table_entries) + ",\"base_all\":" + std::to_string(h.base_all) +
         ",\"base_spans\":" + std::to_string(h.base_spans) + ",\"dirty_tiles\":" + std::to_string(h.dirty_tiles) +
         ",\"prep_ms_mean\":" + per(h.prep_ns, h.flushes + h.nows) + ",\"failures\":" + std::to_string(h.failures) +
         ",\"starts_gpu\":" + std::to_string(h.starts_gpu) + ",\"pubs_gpu\":" + std::to_string(h.pubs_gpu) +
         "},\"check\":{\"compared\":" + std::to_string(h.compared) + ",\"differing\":" + std::to_string(h.differing) +
         ",\"differing_words\":" + std::to_string(h.differing_words) + ",\"images_compared\":" + std::to_string(s.img_compared) +
         ",\"images_differing\":" + std::to_string(s.img_differing) + ",\"image_differing_words\":" + std::to_string(s.img_differing_words) +
         ",\"start_checks\":" + std::to_string(h.start_checks) + ",\"start_differing\":" + std::to_string(h.start_differing) +
         ",\"pub_checks\":" + std::to_string(h.pub_checks) + ",\"pub_differing\":" + std::to_string(h.pub_differing) + "}}";
}

}  // namespace portapp
