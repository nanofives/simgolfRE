// The portsim C ABI (include/portsim_c.h) over the registry, the context and the object table (phase 2.6, 2026-10-19).
#include "portsim_c.h"

#include <algorithm>
#include <cstring>

#include <memory>
#include <string>
#include <vector>

#include "portsim/execonst.h"
#include "portsim/gamefiles.h"
#include "portsim/registry.h"
#include "portsim/registry_list.h"
#include "portsim/state_hash.h"
#include "portsim/sim/gl_raster.h"
#include "portsim/sim/gl_sink.h"
#include "portsim/sim/terrain_gl_setup.h"
#include "portsim/sim/terrain_render.h"
#include "portsim/sim/terrain_live.h"
#include "portsim/sim/terrain_state.h"
#include "portsim/sim/live.h"
#include "portsim/sim/tick.h"

#if defined(_MSC_VER)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

struct sg_raster {
  portsim::gl::GlRaster r;
  sg_raster(int w, int h) : r(w, h) {}
};

struct sg_ctx {
  portsim::Ctx ctx;
  portsim::MemoryGameFiles mem_files;                // phase 3.0: the default game files of a context
  std::unique_ptr<portsim::DirGameFiles> dir_files;  // set by sg_files_root
  std::string run_detail;                            // phase 3.7a: why sg_run_frames stopped
  bool boot_next = false;                            // phase 4: the next sg_run_frames is sg_run_boot's
  portsim::TerrainGlobals gl_globals;                // phase 5: the Terrain.dll statics of the render path (sg_gl_set_globals)
  std::unique_ptr<portsim::TerrainDll> terrain_dll;  // phase 5: the state side driven call by call (sg_ts_call)
  std::unique_ptr<portsim::LiveTerrain> live;        // phase 5: the TerrainApi over that state (sg_ts_live_attach)
  portsim::LiveResult live_res;                      // roadmap S7.1: the last live run (sg_live_*)
  sg_ctx() { ctx.files_ptr = &mem_files; }
  ~sg_ctx() { portsim::release_headless_terrain(ctx); }  // roadmap S7.9 (U-1681): the headless Terrain blocks of this context
};

namespace {
// the process-wide exe constants are attached to a context once loaded
void attach_exe(sg_ctx *c) {
  portsim::ExeConstants &e = portsim::default_exe_constants();
  c->ctx.exe_ptr = e.loaded() ? &e : nullptr;
}
// what the run had written into its detail before it stopped (tick_run_frames' input summary)
std::string tail(const sg_ctx *c) { return c->run_detail.empty() ? std::string() : "; " + c->run_detail; }
int run_frames_cpp(sg_ctx *c, const uint8_t *trace, size_t n, const uint32_t *q, int32_t nq, int32_t max_frames,
                   uint32_t *out_T, uint64_t *out_hashes, int32_t *done) {
  try {
    const bool boot = c->boot_next;
    c->boot_next = false;
    const int r = portsim::tick_run_frames(c->ctx, trace, n, q, nq, max_frames, out_T, out_hashes, done, &c->run_detail, boot);
    return r;
  } catch (const portsim::Fault &f) {
    c->run_detail = "fault at va " + std::to_string(f.va) + tail(c);
    return SG_E_FAULT;
  } catch (const portsim::DivideError &) {
    c->run_detail = "divide error" + tail(c);
    return SG_E_DIVIDE;
  } catch (const portsim::NoReturn &e) {
    c->run_detail = std::string("no return: ") + (e.where ? e.where : "?") + tail(c);
    return SG_E_NO_RETURN;
  } catch (const portsim::MissingResource &e) {
    c->run_detail = std::string("missing: ") + (e.what ? e.what : "?") + tail(c);
    return SG_E_NO_RESOURCE;
  } catch (...) {
    c->run_detail = "exception" + tail(c);
    return SG_E_EXCEPTION;
  }
}

#if defined(_MSC_VER)
int run_frames_guarded(sg_ctx *c, const uint8_t *trace, size_t n, const uint32_t *q, int32_t nq, int32_t max_frames,
                       uint32_t *out_T, uint64_t *out_hashes, int32_t *done) {
  __try {
    return run_frames_cpp(c, trace, n, q, nq, max_frames, out_T, out_hashes, done);
  } __except (GetExceptionCode() == EXCEPTION_INT_DIVIDE_BY_ZERO || GetExceptionCode() == EXCEPTION_INT_OVERFLOW
                  ? EXCEPTION_EXECUTE_HANDLER
                  : EXCEPTION_CONTINUE_SEARCH) {
    return SG_E_DIVIDE;
  }
}
#else
int run_frames_guarded(sg_ctx *c, const uint8_t *trace, size_t n, const uint32_t *q, int32_t nq, int32_t max_frames,
                       uint32_t *out_T, uint64_t *out_hashes, int32_t *done) {
  return run_frames_cpp(c, trace, n, q, nq, max_frames, out_T, out_hashes, done);
}
#endif
}  // namespace

extern "C" {

size_t sg_state_size(void) { return sizeof(portsim::GameState); }

sg_ctx *sg_state_new(void) {
  try {
    return new sg_ctx();
  } catch (...) {
    return nullptr;
  }
}

void sg_state_free(sg_ctx *ctx) { delete ctx; }

uint8_t *sg_state_ptr(sg_ctx *ctx) { return ctx ? ctx->ctx.bytes() : nullptr; }

int sg_set_data_base(sg_ctx *ctx, uint32_t base) {
  if (!ctx) return SG_E_ARG;
  return ctx->ctx.mem.set_data_base(base) ? SG_OK : SG_E_MAP;
}

uint32_t sg_data_base(sg_ctx *ctx) { return ctx ? ctx->ctx.mem.data_base() : 0u; }

int sg_map(sg_ctx *ctx, uint32_t base, void *host, uint32_t size, int32_t readonly) {
  if (!ctx) return SG_E_ARG;
  return ctx->ctx.mem.map(base, static_cast<uint8_t *>(host), size, readonly != 0) ? SG_OK : SG_E_MAP;
}

void sg_unmap_all(sg_ctx *ctx) {
  if (ctx) ctx->ctx.mem.unmap_all();
}

int32_t sg_func_count(void) { return portsim::registry_count(); }

const char *sg_func_name(int32_t i) {
  const portsim::FuncDef *d = portsim::registry_at(i, nullptr);
  return d ? d->name : nullptr;
}

int sg_func_info_get(int32_t i, sg_func_info *out) {
  const char *module = nullptr;
  const portsim::FuncDef *d = portsim::registry_at(i, &module);
  if (!d || !out) return SG_E_ARG;
  out->name = d->name;
  out->module = module;
  out->abi = d->abi;
  out->ret = d->ret;
  out->arity = d->arity;
  out->fpcw = d->fpcw;
  return SG_OK;
}

int32_t sg_func_find(const char *name) {
  int i = portsim::registry_find(name);
  return i < 0 ? SG_E_NO_FUNCTION : i;
}

int sg_call(sg_ctx *ctx, const char *name, const uint32_t *args, int32_t nargs, sg_ret *out) {
  if (!ctx || !out || (nargs > 0 && !args)) return SG_E_ARG;
  int i = portsim::registry_find(name);
  if (i < 0) return SG_E_NO_FUNCTION;
  const portsim::FuncDef *d = portsim::registry_at(i, nullptr);
  if (nargs != d->arity) return SG_E_ARITY;
  portsim::Ret r;
  attach_exe(ctx);
  try {
    d->fn(ctx->ctx, args, r);
  } catch (const portsim::Fault &) {
    return SG_E_FAULT;
  } catch (const portsim::DivideError &) {
    return SG_E_DIVIDE;
  } catch (const portsim::NoReturn &) {
    return SG_E_NO_RETURN;
  } catch (const portsim::MissingResource &) {
    return SG_E_NO_RESOURCE;
  } catch (...) {
    return SG_E_EXCEPTION;
  }
  out->eax = r.eax;
  out->edx = r.edx;
  out->st0 = r.st0;
  out->has_st0 = r.has_st0 ? 1 : 0;
  return SG_OK;
}

int sg_set_fpcw(uint32_t cw) {
#if defined(_M_IX86) && defined(_MSC_VER)
  unsigned short w = static_cast<unsigned short>(cw);
  __asm fldcw w
  return SG_OK;
#else
  // SSE2 doubles: 53-bit significand, round to nearest, exceptions masked = x87 word 0x027f (bits 0-5 and 8-11)
  return (cw & 0x0F3Fu) == 0x023Fu ? SG_OK : SG_E_UNSUPPORTED;
#endif
}

int sg_negctl_set(const char *name, int32_t mode) {
  int i = portsim::registry_find(name);
  if (i < 0) return SG_E_NO_FUNCTION;
  const portsim::FuncDef *d = portsim::registry_at(i, nullptr);
  if (!d->negctl) return SG_E_UNSUPPORTED;
  int prev = *d->negctl;
  *d->negctl = mode;
  return prev;
}

int32_t sg_negctl_get(const char *name) {
  int i = portsim::registry_find(name);
  if (i < 0) return SG_E_NO_FUNCTION;
  const portsim::FuncDef *d = portsim::registry_at(i, nullptr);
  return d->negctl ? *d->negctl : 0;
}

void sg_negctl_reset(void) {
  for (int i = 0; i < portsim::registry_count(); ++i) {
    const portsim::FuncDef *d = portsim::registry_at(i, nullptr);
    if (d->negctl) *d->negctl = 0;
  }
}

int32_t sg_object_count(const char *cls) { return static_cast<int32_t>(portsim::objects_of(cls).size()); }

const char *sg_object_name(const char *cls, int32_t i) {
  const auto &v = portsim::objects_of(cls);
  return i >= 0 && static_cast<size_t>(i) < v.size() ? v[static_cast<size_t>(i)]->name : nullptr;
}

int sg_object_span(const char *cls, int32_t i, uint32_t *off, uint32_t *size) {
  const auto &v = portsim::objects_of(cls);
  if (i < 0 || static_cast<size_t>(i) >= v.size() || !off || !size) return SG_E_ARG;
  *off = v[static_cast<size_t>(i)]->off;
  *size = v[static_cast<size_t>(i)]->size;
  return SG_OK;
}

uint64_t sg_hash_bytes(const uint8_t *data, size_t n) { return portsim::sgh64(data, n); }

int32_t sg_hash_class(sg_ctx *ctx, const char *cls, uint64_t *out, int32_t cap) {
  if (!ctx || !out) return SG_E_ARG;
  const auto &v = portsim::objects_of(cls);
  if (v.empty() || static_cast<size_t>(cap) < v.size()) return SG_E_ARG;
  const uint8_t *s = ctx->ctx.bytes();
  for (size_t k = 0; k < v.size(); ++k) out[k] = portsim::object_hash(s, *v[k]);
  return static_cast<int32_t>(v.size());
}

int sg_run_frames(sg_ctx *ctx, const uint8_t *trace, size_t trace_len, const sg_msg *queued, int32_t n_queued,
                  int32_t max_frames, uint32_t *out_T, uint64_t *out_hashes, int32_t *frames_done) {
  if (!ctx || !frames_done || !trace || (max_frames > 0 && (!out_T || !out_hashes))) return SG_E_ARG;
  *frames_done = 0;
  ctx->run_detail.clear();
  std::vector<uint32_t> q;
  for (int32_t i = 0; i < n_queued && queued; ++i) {
    q.push_back(queued[i].msg);
    q.push_back(queued[i].wp);
    q.push_back(queued[i].lp);
    q.push_back(queued[i].cls);
  }
  // phase 3.7a: the frame step (port/src/sim/tick.cpp); a hardware #DE inside it (a plain division the original runs with
  // divisor 0) is reported as SG_E_DIVIDE like DivideError
  return run_frames_guarded(ctx, trace, trace_len, q.data(), n_queued, max_frames, out_T, out_hashes, frames_done);
}

int sg_run_boot(sg_ctx *ctx, const uint8_t *trace, size_t trace_len, int32_t max_frames, uint32_t *out_T,
                uint64_t *out_hashes, int32_t *frames_done) {
  if (!ctx) return SG_E_ARG;
  ctx->boot_next = true;
  const int r = sg_run_frames(ctx, trace, trace_len, nullptr, 0, max_frames, out_T, out_hashes, frames_done);
  ctx->boot_next = false;
  return r;
}

const char *sg_run_frames_detail(sg_ctx *ctx) { return ctx ? ctx->run_detail.c_str() : ""; }

// ---- live runs (roadmap S7.1, portsim/sim/live.h)
}  // extern "C"
namespace {
class CHost : public portsim::LiveHost {
 public:
  explicit CHost(const sg_live_host &h) : h_(h) {}
  bool frame_head(portsim::Ctx &, uint32_t frame, uint32_t &t) override {
    return h_.frame_head && h_.frame_head(h_.user, frame, &t) != 0;
  }
  bool clock(portsim::Ctx &, int32_t site, bool body, uint32_t &t) override {
    if (!h_.clock) return false;
    const bool ok = h_.clock(h_.user, site, body ? 1 : 0, &t) != 0;
    if (!ok && body) throw portsim::MissingResource{"live host: no value for the frame body's clock read"};
    return ok;
  }
  void cursor(portsim::Ctx &, bool modal, int32_t &x, int32_t &y) override {
    if (h_.cursor) h_.cursor(h_.user, modal ? 1 : 0, &x, &y);
  }
  uint16_t async_key(portsim::Ctx &, int vk) override {
    return h_.async_key ? static_cast<uint16_t>(h_.async_key(h_.user, vk)) : 0;
  }
  portsim::LiveReply pump(portsim::Ctx &, const portsim::LivePump &p, std::vector<portsim::Command> &out) override {
    if (!h_.pump) return portsim::LiveReply::Stop;
    sg_live_pump cp{static_cast<uint32_t>(p.kind), p.body ? 1 : 0, p.may_deliver ? 1 : 0, p.must_deliver ? 1 : 0,
                    p.frame, p.cursor_reads, p.pace_site, p.calls, p.idle_ok ? 1 : 0};
    sg_live_cmd buf[64];
    int32_t n = 0;
    const int32_t r = h_.pump(h_.user, &cp, buf, 64, &n);
    if (r == 2) return portsim::LiveReply::Stop;
    if (r != 1) return portsim::LiveReply::Decline;
    for (int32_t i = 0; i < n && i < 64; ++i) {
      portsim::Command c;
      c.kind = portsim::CmdKind::RootHandler;
      c.handler = buf[i].handler;
      c.a0 = buf[i].a0;
      c.a1 = buf[i].a1;
      c.async_ctrl = buf[i].async_ctrl;
      c.async_shift = buf[i].async_shift;
      c.t = buf[i].t;
      out.push_back(c);
    }
    return portsim::LiveReply::Deliver;
  }

 private:
  sg_live_host h_;
};

int live_run_cpp(sg_ctx *c, portsim::LiveHost &host, uint32_t frame_ra, int32_t max_records, uint32_t *out_T,
                 uint64_t *out_hashes, int32_t *records) {
  c->live_res = portsim::LiveResult();
  c->run_detail.clear();
  portsim::LiveOptions opt;
  opt.max_records = max_records;
  opt.frame_ra = frame_ra;
  int code = SG_OK;
  try {
    portsim::run_live(c->ctx, host, opt, c->live_res);
  } catch (const portsim::Fault &f) {
    c->run_detail = "fault at va " + std::to_string(f.va);
    code = SG_E_FAULT;
  } catch (const portsim::DivideError &) {
    c->run_detail = "divide error";
    code = SG_E_DIVIDE;
  } catch (const portsim::NoReturn &e) {
    c->run_detail = std::string("no return: ") + (e.where ? e.where : "?");
    code = SG_E_NO_RETURN;
  } catch (const portsim::MissingResource &e) {
    c->run_detail = std::string("missing: ") + (e.what ? e.what : "?");
    code = SG_E_NO_RESOURCE;
  } catch (...) {
    c->run_detail = "exception";
    code = SG_E_EXCEPTION;
  }
  const portsim::LiveResult &r = c->live_res;
  c->run_detail += (c->run_detail.empty() ? "" : "; ") + std::string("live: ") + r.stop + ", " +
                   std::to_string(r.records) + " records, " + std::to_string(r.dispatches) + " dispatches, " +
                   std::to_string(r.violations) + " violations" +
                   (r.first_violation >= 0
                        ? " (first at record " + std::to_string(r.first_violation) + ": " + r.first_violation_what + ")"
                        : std::string());
  const int32_t n = r.records;
  const size_t nsim = portsim::objects_of("sim").size();
  *records = max_records >= 0 ? std::min(n, max_records) : n;
  if (out_T && out_hashes && max_records > 0)  // caller buffers of max_records; else sg_live_records
    for (int32_t k = 0; k < *records; ++k) {
      const size_t i = static_cast<size_t>(k);
      out_T[k] = r.T[i];
      for (size_t j = 0; j < nsim; ++j) out_hashes[i * nsim + j] = r.hashes[i * nsim + j];
    }
  return code;
}

#if defined(_MSC_VER)
int live_run_guarded(sg_ctx *c, portsim::LiveHost &host, uint32_t frame_ra, int32_t max_records, uint32_t *out_T,
                     uint64_t *out_hashes, int32_t *records) {
  __try {
    return live_run_cpp(c, host, frame_ra, max_records, out_T, out_hashes, records);
  } __except (GetExceptionCode() == EXCEPTION_INT_DIVIDE_BY_ZERO || GetExceptionCode() == EXCEPTION_INT_OVERFLOW
                  ? EXCEPTION_EXECUTE_HANDLER
                  : EXCEPTION_CONTINUE_SEARCH) {
    return SG_E_DIVIDE;
  }
}
#else
int live_run_guarded(sg_ctx *c, portsim::LiveHost &host, uint32_t frame_ra, int32_t max_records, uint32_t *out_T,
                     uint64_t *out_hashes, int32_t *records) {
  return live_run_cpp(c, host, frame_ra, max_records, out_T, out_hashes, records);
}
#endif

std::unique_ptr<portsim::LiveHost> builtin_host(sg_ctx *ctx, int32_t kind, const uint8_t *data, size_t n, std::string &why) {
  if (kind == 1) return portsim::make_trace_live_host(ctx->ctx, data, n, &why);
  if (kind == 2) return portsim::make_script_live_host(std::string(reinterpret_cast<const char *>(data), n), &why);
  if (kind == 3) {  // roadmap S7.7: u32 trace length, the trace, then the script text (portsim::make_prefix_live_host)
    uint32_t tn = 0;
    if (n >= 4) std::memcpy(&tn, data, 4);
    if (n < 4 || tn > n - 4) {
      why = "kind 3: bad trace length";
      return nullptr;
    }
    return portsim::make_prefix_live_host(ctx->ctx, data + 4, tn,
                                          std::string(reinterpret_cast<const char *>(data + 4 + tn), n - 4 - tn), &why);
  }
  why = "unknown live host kind";
  return nullptr;
}
}  // namespace
extern "C" {

int sg_live_run(sg_ctx *ctx, const sg_live_host *host, uint32_t frame_ra, int32_t max_records, uint32_t *out_T,
                uint64_t *out_hashes, int32_t *records) {
  if (!ctx || !host || !records || (max_records > 0 && (!out_T || !out_hashes))) return SG_E_ARG;
  *records = 0;
  CHost h(*host);
  return live_run_guarded(ctx, h, frame_ra, max_records, out_T, out_hashes, records);
}

int sg_live_run_builtin(sg_ctx *ctx, int32_t kind, const uint8_t *data, size_t n, uint32_t frame_ra, int32_t max_records,
                        uint32_t *out_T, uint64_t *out_hashes, int32_t *records) {
  if (!ctx || !records || !data || (max_records > 0 && (!out_T || !out_hashes))) return SG_E_ARG;
  *records = 0;
  std::string why;
  std::unique_ptr<portsim::LiveHost> h;
  try {
    h = builtin_host(ctx, kind, data, n, why);
  } catch (...) {
    return SG_E_EXCEPTION;
  }
  if (!h) {
    ctx->run_detail = why;
    return SG_E_ARG;
  }
  return live_run_guarded(ctx, *h, frame_ra, max_records, out_T, out_hashes, records);
}

int sg_live_recording(sg_ctx *ctx, uint8_t *buf, size_t cap, size_t *len) {
  if (!ctx || !len) return SG_E_ARG;
  const std::vector<uint8_t> &t = ctx->live_res.trace;
  *len = t.size();
  if (buf && cap && !t.empty()) std::memcpy(buf, t.data(), std::min(cap, t.size()));
  return SG_OK;
}

int32_t sg_live_records(sg_ctx *ctx, int32_t first, int32_t cap, uint32_t *out_T, uint64_t *out_hashes) {
  if (!ctx || first < 0 || cap < 0) return SG_E_ARG;
  const portsim::LiveResult &r = ctx->live_res;
  const size_t nsim = portsim::objects_of("sim").size();
  int32_t k = 0;
  for (; k < cap && first + k < r.records; ++k) {
    const size_t i = static_cast<size_t>(first + k);
    if (out_T) out_T[k] = r.T[i];
    if (out_hashes)
      for (size_t j = 0; j < nsim; ++j) out_hashes[static_cast<size_t>(k) * nsim + j] = r.hashes[i * nsim + j];
  }
  return k;
}

int sg_live_stats(sg_ctx *ctx, int64_t out[12]) {
  if (!ctx || !out) return SG_E_ARG;
  const portsim::LiveResult &r = ctx->live_res;
  const int64_t v[12] = {static_cast<int64_t>(r.violations), r.first_violation, static_cast<int64_t>(r.asks),
                        static_cast<int64_t>(r.reasks), static_cast<int64_t>(r.clocks), static_cast<int64_t>(r.cursors),
                        static_cast<int64_t>(r.asyncs), static_cast<int64_t>(r.dispatches), static_cast<int64_t>(r.writes),
                        static_cast<int64_t>(r.body_calls), static_cast<int64_t>(r.modal_calls),
                        static_cast<int64_t>(r.paced_calls)};
  for (int i = 0; i < 12; ++i) out[i] = v[i];
  return SG_OK;
}

int sg_live_stats_ext(sg_ctx *ctx, int64_t out[2]) {
  if (!ctx || !out) return SG_E_ARG;
  out[0] = static_cast<int64_t>(ctx->live_res.idle_declines);
  out[1] = static_cast<int64_t>(ctx->live_res.inflated);
  return SG_OK;
}

int32_t sg_live_marks(sg_ctx *ctx, uint32_t *out, int32_t cap) {
  if (!ctx || cap < 0) return SG_E_ARG;
  const std::vector<uint32_t> &m = ctx->live_res.mark_pos;
  for (int32_t k = 0; out && k < cap && static_cast<size_t>(k) < m.size(); ++k) out[k] = m[static_cast<size_t>(k)];
  return static_cast<int32_t>(m.size());
}

int32_t sg_tick_callee_counts(char *buf, int32_t cap, int32_t reset) {
  std::string out;
  for (const auto &kv : portsim::tick_callee_counts()) out += kv.first + "=" + std::to_string(kv.second) + "\n";
  if (reset) portsim::tick_callee_counts_reset();
  if (buf && cap > 0) {
    const size_t n = out.size() < static_cast<size_t>(cap - 1) ? out.size() : static_cast<size_t>(cap - 1);
    std::memcpy(buf, out.data(), n);
    buf[n] = 0;
  }
  return static_cast<int32_t>(out.size());
}

// ------------------------------------------------------------------------------------------------ defects (phase 3.0)
namespace {
bool g_profile_fixed = false;  // the last sg_profile_set was "fixed" and no switch was set directly since
}  // namespace

int32_t sg_defect_count(void) { return portsim::defect_count(); }

int sg_defect_info_get(int32_t i, sg_defect_info *out) {
  const char *module = nullptr;
  const portsim::DefectDef *d = portsim::defect_at(i, &module);
  if (!d || !out) return SG_E_ARG;
  out->name = d->name;
  out->module = module;
  out->ref = d->ref;
  out->what = d->what;
  return SG_OK;
}

int sg_defect_set(const char *name, int32_t on) {
  const portsim::DefectDef *d = portsim::defect_at(portsim::defect_find(name), nullptr);
  if (!d) return SG_E_ARG;
  g_profile_fixed = false;
  const int prev = *d->flag;
  *d->flag = on ? 1 : 0;
  return prev;
}

int32_t sg_defect_get(const char *name) {
  const portsim::DefectDef *d = portsim::defect_at(portsim::defect_find(name), nullptr);
  return d ? *d->flag : SG_E_ARG;
}

void sg_defect_reset(void) {
  for (int i = 0; i < portsim::defect_count(); ++i) *portsim::defect_at(i, nullptr)->flag = 1;
}

// S6: profiles over the switches (log/roadmap/S6/3_merge.md); every name is checked before anything changes
int sg_profile_set(const char *profile, const char *const *switches, const int32_t *values, int32_t n) {
  if (!profile || n < 0 || (n > 0 && (!switches || !values))) return SG_E_ARG;
  const bool fixed = std::strcmp(profile, "fixed") == 0;
  if (!fixed && (std::strcmp(profile, "original") != 0 || n != 0)) return SG_E_ARG;
  for (int32_t i = 0; i < n; ++i)
    if (!switches[i] || !portsim::defect_at(portsim::defect_find(switches[i]), nullptr)) return SG_E_ARG;
  sg_defect_reset();
  for (int32_t i = 0; i < n; ++i)
    *portsim::defect_at(portsim::defect_find(switches[i]), nullptr)->flag = values[i] ? 1 : 0;
  g_profile_fixed = fixed;
  return SG_OK;
}

const char *sg_profile_get(void) {
  bool all_on = true;
  for (int i = 0; i < portsim::defect_count(); ++i)
    if (*portsim::defect_at(i, nullptr)->flag != 1) all_on = false;
  if (all_on) return "original";
  return g_profile_fixed ? "fixed" : "custom";
}

// ------------------------------------------------------------------------------------------------ exe constants
int sg_exe_load(const char *path, char *err, size_t errcap) {
  std::string why;
  const bool ok = portsim::default_exe_constants().load_file(path, &why);
  if (err && errcap) {
    const size_t n = why.size() < errcap - 1 ? why.size() : errcap - 1;
    std::memcpy(err, why.data(), n);
    err[n] = 0;
  }
  return ok ? SG_OK : SG_E_NO_RESOURCE;
}

int32_t sg_exe_loaded(void) { return portsim::default_exe_constants().loaded() ? 1 : 0; }

int sg_exe_const(const char *name, uint8_t *out, uint32_t cap, uint32_t *size) {
  try {
    uint32_t n = 0;
    const uint8_t *p = portsim::default_exe_constants().bytes(name, &n);
    if (size) *size = n;
    if (out) std::memcpy(out, p, n < cap ? n : cap);
    return SG_OK;
  } catch (const portsim::MissingResource &) {
    return SG_E_NO_RESOURCE;
  }
}

int sg_exe_fill_state(sg_ctx *ctx, int32_t mode) {
  if (!ctx || (mode != 0 && mode != 1)) return SG_E_ARG;
  try {
    portsim::default_exe_constants().fill_state(
        ctx->ctx.gs(), mode ? portsim::ExeConstants::Fill::InitialImage : portsim::ExeConstants::Fill::ConstObjects);
    return SG_OK;
  } catch (const portsim::MissingResource &) {
    return SG_E_NO_RESOURCE;
  }
}

// ------------------------------------------------------------------------------------------------ game files
int sg_files_put(sg_ctx *ctx, const char *name, const uint8_t *data, size_t n) {
  if (!ctx || !name || (n && !data)) return SG_E_ARG;
  std::vector<uint8_t> bytes = n ? std::vector<uint8_t>(data, data + n) : std::vector<uint8_t>();
  // with a root directory attached the file joins the directory's view (its in-memory overlay: the install on disk is
  // never touched), so a harness can add the fixture files a recording's install held
  if (ctx->dir_files) ctx->dir_files->write(name, bytes);
  else ctx->mem_files.put(name, std::move(bytes));
  return SG_OK;
}

void sg_files_clear(sg_ctx *ctx) {
  if (ctx) ctx->mem_files.clear();
}

int sg_files_root(sg_ctx *ctx, const char *dir) {
  if (!ctx) return SG_E_ARG;
  if (dir) {
    ctx->dir_files = std::make_unique<portsim::DirGameFiles>(dir);
    ctx->ctx.files_ptr = ctx->dir_files.get();
  } else {
    ctx->ctx.files_ptr = &ctx->mem_files;
    ctx->dir_files.reset();
  }
  return SG_OK;
}

int64_t sg_files_size(sg_ctx *ctx, const char *name) {
  if (!ctx || !name || !ctx->ctx.files_ptr) return -1;
  return ctx->ctx.files_ptr->size(name);
}

int64_t sg_files_read(sg_ctx *ctx, const char *name, uint64_t offset, uint8_t *buf, uint64_t cap) {
  if (!ctx || !name || !ctx->ctx.files_ptr || (cap && !buf)) return -1;
  return ctx->ctx.files_ptr->read(name, offset, buf, cap);
}

int sg_gl_set_globals(sg_ctx *ctx, const sg_gl_globals *g) {
  if (!ctx || !g) return SG_E_ARG;
  portsim::TerrainGlobals &t = ctx->gl_globals;
  auto fcopy = [](const float *p, uint32_t n) { return p ? std::vector<float>(p, p + n) : std::vector<float>(); };
  t.vertices = fcopy(g->vertices, g->n_vertices * 3);
  t.normals = fcopy(g->normals, g->n_normals * 3);
  t.textures = g->textures ? std::vector<uint32_t>(g->textures, g->textures + g->n_textures) : std::vector<uint32_t>();
  t.wall_texture = g->wall_texture;
  t.fence_texture0 = g->fence_texture0;
  t.fence_texture1 = g->fence_texture1;
  t.tex_coords = fcopy(g->tex_coords, g->n_tex_coords);
  if (g->side_normals) std::memcpy(t.side_normals, g->side_normals, sizeof t.side_normals);
  std::memcpy(&t.screen_scale, &g->screen_scale_bits, 4);
  t.view_index = g->view_index;
  return SG_OK;
}

int sg_gl_render(sg_ctx *ctx, int32_t entry, uint32_t terrain, uint32_t a0, uint32_t a1, uint32_t a2, char *out,
                 size_t cap, size_t *len) {
  (void)a2;
  if (!ctx || !out || !len || cap == 0) return SG_E_ARG;
  *len = 0;
  portsim::gl::GlStreamSink sink;
  try {
    if (!portsim::terrain_render_entry(ctx->ctx, sink, ctx->gl_globals, entry, terrain, a0, a1, a2)) return SG_E_NO_FUNCTION;
  } catch (const portsim::Fault &f) {
    ctx->run_detail = "fault at va " + std::to_string(f.va);
    return SG_E_FAULT;
  } catch (...) {
    return SG_E_EXCEPTION;
  }
  *len = sink.text.size();
  if (sink.text.size() + 1 > cap) return SG_E_ARG;
  std::memcpy(out, sink.text.data(), sink.text.size());
  out[sink.text.size()] = 0;
  return SG_OK;
}

int sg_gl_render_to_raster(sg_ctx *ctx, sg_raster *rast, int32_t entry, uint32_t terrain, uint32_t a0, uint32_t a1, uint32_t a2) {
  (void)a2;
  if (!ctx || !rast) return SG_E_ARG;
  try {
    // the vertex and normal arrays the faces index are the renderer's globals; the rasteriser reads them for glArrayElement
    const portsim::TerrainGlobals &g = ctx->gl_globals;
    rast->r.set_arrays(g.vertices.data(), g.vertices.size() / 3, g.normals.data(), g.normals.size() / 3);
    if (!portsim::terrain_render_entry(ctx->ctx, rast->r, ctx->gl_globals, entry, terrain, a0, a1, a2)) return SG_E_NO_FUNCTION;
  } catch (const portsim::Fault &f) {
    ctx->run_detail = "fault at va " + std::to_string(f.va);
    return SG_E_FAULT;
  } catch (...) {
    return SG_E_EXCEPTION;
  }
  return SG_OK;
}

int sg_gl_setup(sg_ctx *ctx, char *out, size_t cap, size_t *len, uint32_t *textures_out) {
  if (!ctx || !out || !len || cap == 0) return SG_E_ARG;
  *len = 0;
  portsim::gl::GlStreamSink sink;
  portsim::TerrainGlState st;
  try {
    portsim::terrain_gl_construct(ctx->ctx, sink, st);
    portsim::terrain_gl_init_system(ctx->ctx, sink, st, 800, 600, true);
    portsim::terrain_gl_init_terrain(sink);
  } catch (const portsim::Fault &f) {
    ctx->run_detail = "fault at va " + std::to_string(f.va);
    return SG_E_FAULT;
  } catch (const portsim::MissingResource &) {
    return SG_E_NO_RESOURCE;
  } catch (const portsim::NoReturn &) {
    return SG_E_NO_RETURN;
  } catch (...) {
    return SG_E_EXCEPTION;
  }
  if (textures_out) std::memcpy(textures_out, st.textures.data(), st.textures.size() * 4);
  *len = sink.text.size();
  if (sink.text.size() + 1 > cap) return SG_E_ARG;
  std::memcpy(out, sink.text.data(), sink.text.size());
  out[sink.text.size()] = 0;
  return SG_OK;
}

int sg_gl_setup_to_raster(sg_ctx *ctx, sg_raster *rast, int32_t zoom, uint32_t *textures_out) {
  if (!ctx || !rast) return SG_E_ARG;
  portsim::TerrainGlState st;
  try {
    rast->r.no_context();   // the Terrain constructor runs before any context exists
    portsim::terrain_gl_construct(ctx->ctx, rast->r, st);
    portsim::terrain_gl_init_system(ctx->ctx, rast->r, st, 800, 600, true);
    portsim::terrain_gl_init_terrain(rast->r);
    if (zoom == 1) portsim::terrain_gl_set_zoom_level(rast->r, st, 2);   // the exe's setZoomLevel(2) / (1) after a zoom key
    else if (zoom == 2) portsim::terrain_gl_set_zoom_level(rast->r, st, 1);
  } catch (const portsim::Fault &f) {
    ctx->run_detail = "fault at va " + std::to_string(f.va);
    return SG_E_FAULT;
  } catch (const portsim::MissingResource &) {
    return SG_E_NO_RESOURCE;
  } catch (const portsim::NoReturn &) {
    return SG_E_NO_RETURN;
  } catch (...) {
    return SG_E_EXCEPTION;
  }
  if (textures_out) std::memcpy(textures_out, st.textures.data(), st.textures.size() * 4);
  return SG_OK;
}

int32_t sg_gl_view_index(sg_ctx *ctx) { return ctx ? ctx->gl_globals.view_index : 0; }

int sg_gl_negctl(int32_t mode) {
  static int32_t prev = 0;
  const int32_t p = prev;
  prev = mode;
  portsim::terrain_render_negctl(mode);
  return p;
}

int sg_ts_op(sg_ctx *ctx, const char *op, uint32_t t, int32_t a0, int32_t a1, int32_t a2, int32_t *ret) {
  if (!ctx || !op) return SG_E_ARG;
  portsim::Ctx &c = ctx->ctx;
  portsim::TerrainGlobals &g = ctx->gl_globals;
  int32_t r = 0;
  const std::string o = op;
  try {
    if (o == "construct") portsim::terrain_construct(c, g, t, a0, a1);
    else if (o == "build_arrays") r = portsim::terrain_build_arrays(c, g, t) ? 1 : 0;
    else if (o == "init_grid") portsim::init_grid(g, a0, a1);
    else if (o == "tile_reset") portsim::tile_reset(c, t, a0, a1);
    else if (o == "faces_build") portsim::faces_build(c, t, a0, a1, a2);
    else if (o == "face_normal") portsim::face_normal(c, g, t);
    else if (o == "smooth_normals") portsim::tile_smooth_normals(c, g, t, a0, a1);
    else if (o == "calc_normals") portsim::terrain_calc_normals(c, g, t);
    else if (o == "calc_all_normals") portsim::terrain_calc_all_normals(c, g, t, static_cast<uint32_t>(a0));
    else if (o == "set_face_texture") portsim::tile_set_face_texture(c, g, t, a0, a1, a2);
    else if (o == "blend") portsim::tile_blend_face_textures(c, g, t, a0);
    else if (o == "reblend") portsim::tile_reblend_neighbourhood(c, g, t);
    else if (o == "reset_face_textures") portsim::tile_reset_face_textures(c, g, t);
    else if (o == "edge_kind") r = portsim::tile_edge_kind(c, g, t, static_cast<uint32_t>(a0));
    else if (o == "tile_at") r = static_cast<int32_t>(portsim::terrain_tile_at(c, t, a0, a1));
    else if (o == "load_dll_constants") r = portsim::terrain_load_dll_constants(c, g) ? 1 : 0;
    else if (o == "gl_setup") {
      portsim::gl::GlStreamSink sink;
      portsim::TerrainGlState st;
      portsim::terrain_gl_construct(c, sink, st);
      portsim::terrain_gl_init_system(c, sink, st, 800, 600, true);
      portsim::terrain_gl_init_terrain(sink);
      if (a0 == 1) portsim::terrain_gl_set_zoom_level(sink, st, 2);
      else if (a0 == 2) portsim::terrain_gl_set_zoom_level(sink, st, 1);
      portsim::terrain_apply_gl_state(c, g, t, st);
    } else return SG_E_NO_FUNCTION;
  } catch (const portsim::Fault &f) {
    ctx->run_detail = "fault at va " + std::to_string(f.va);
    return SG_E_FAULT;
  } catch (const portsim::MissingResource &) {
    return SG_E_NO_RESOURCE;
  } catch (const portsim::NoReturn &) {
    return SG_E_NO_RETURN;
  } catch (...) {
    return SG_E_EXCEPTION;
  }
  if (ret) *ret = r;
  return SG_OK;
}

int sg_ts_dll_begin(sg_ctx *ctx, uint32_t terrain) {
  if (!ctx) return SG_E_ARG;
  ctx->terrain_dll = std::make_unique<portsim::TerrainDll>();
  ctx->terrain_dll->terrain = terrain;
  ctx->gl_globals = portsim::TerrainGlobals();
  ctx->gl_globals.vertices.assign(static_cast<size_t>(22500) * 3, 0.0f);   // the DLL's bss arrays: 22,500 vertices each
  ctx->gl_globals.normals.assign(static_cast<size_t>(22500) * 3, 0.0f);
  return SG_OK;
}

int sg_ts_call(sg_ctx *ctx, const char *name, uint32_t self, const uint32_t *args, const int32_t *table, int32_t n, uint32_t *ret) {
  if (!ctx || !name || !args || !ctx->terrain_dll) return SG_E_ARG;
  try {
    const uint32_t r = portsim::terrain_dll_call(ctx->ctx, ctx->gl_globals, *ctx->terrain_dll, name, self, args, table, n);
    if (ret) *ret = r;
  } catch (const portsim::Fault &f) {
    ctx->run_detail = std::string(name) + ": fault at va " + std::to_string(f.va);
    return SG_E_FAULT;
  } catch (const portsim::MissingResource &m) {
    ctx->run_detail = std::string(name) + ": " + m.what;
    return SG_E_NO_RESOURCE;
  } catch (const portsim::NoReturn &) {
    return SG_E_NO_RETURN;
  } catch (...) {
    return SG_E_EXCEPTION;
  }
  return SG_OK;
}

int32_t sg_ts_view_index(sg_ctx *ctx) { return ctx ? ctx->gl_globals.view_index : 0; }

int sg_ts_live_attach(sg_ctx *ctx, uint32_t va, const uint32_t *snap_after, int32_t n) {
  if (!ctx || (n > 0 && !snap_after)) return SG_E_ARG;
  if (ctx->live) portsim::set_terrain_api(ctx->ctx, nullptr);
  ctx->live = std::make_unique<portsim::LiveTerrain>(ctx->ctx, va,
                                                     std::vector<uint32_t>(snap_after, snap_after + (n > 0 ? n : 0)));
  portsim::set_terrain_api(ctx->ctx, ctx->live.get());
  return SG_OK;
}

int sg_ts_live_detach(sg_ctx *ctx) {
  if (!ctx) return SG_E_ARG;
  if (ctx->live) portsim::set_terrain_api(ctx->ctx, nullptr);
  ctx->live.reset();
  return SG_OK;
}

uint32_t sg_ts_live_object(sg_ctx *ctx) { return ctx && ctx->live ? ctx->live->object_va() : 0; }

int sg_ts_live_log(sg_ctx *ctx, char *out, size_t cap, size_t *len) {
  if (!ctx || !ctx->live || !len) return SG_E_ARG;
  std::string s;
  for (const std::string &l : ctx->live->log()) s += l + "\n";
  *len = s.size();
  if (!out || s.size() + 1 > cap) return SG_E_ARG;
  std::memcpy(out, s.data(), s.size());
  out[s.size()] = 0;
  return SG_OK;
}

int32_t sg_ts_live_snapshot(sg_ctx *ctx, int32_t k, uint8_t *object, float *vertices, float *normals, uint32_t *textures,
                            int32_t *view, uint32_t *after) {
  if (!ctx || !ctx->live) return -1;
  const auto &v = ctx->live->snapshots();
  if (k < 0) return static_cast<int32_t>(v.size());
  if (static_cast<size_t>(k) >= v.size()) return -1;
  const auto &s = v[static_cast<size_t>(k)];
  if (object) std::memcpy(object, s.object.data(), s.object.size());
  if (vertices) std::memcpy(vertices, s.vertices.data(), s.vertices.size() * 4);
  if (normals) std::memcpy(normals, s.normals.data(), s.normals.size() * 4);
  if (textures) std::memcpy(textures, s.textures.data(), s.textures.size() * 4);
  if (view) *view = s.view_index;
  if (after) *after = s.after;
  return static_cast<int32_t>(s.object.size());
}

int sg_ts_render_data(sg_ctx *ctx, float *tex_coords, uint32_t cap, uint32_t *n_tex_coords, float *side_normals, uint32_t *wall,
                      uint32_t *fence0, uint32_t *fence1, uint32_t *screen_scale_bits) {
  if (!ctx) return SG_E_ARG;
  const portsim::TerrainGlobals &t = ctx->gl_globals;
  if (n_tex_coords) *n_tex_coords = static_cast<uint32_t>(t.tex_coords.size());
  if (tex_coords) std::memcpy(tex_coords, t.tex_coords.data(), std::min<size_t>(cap, t.tex_coords.size()) * 4);
  if (side_normals) std::memcpy(side_normals, t.side_normals, sizeof t.side_normals);
  if (wall) *wall = t.wall_texture;
  if (fence0) *fence0 = t.fence_texture0;
  if (fence1) *fence1 = t.fence_texture1;
  if (screen_scale_bits) std::memcpy(screen_scale_bits, &t.screen_scale, 4);
  return SG_OK;
}

int sg_ts_set_render_data(sg_ctx *ctx, const float *tex_coords, uint32_t n_tex_coords, const float *side_normals, uint32_t wall,
                          uint32_t fence0, uint32_t fence1, uint32_t screen_scale_bits) {
  if (!ctx || (n_tex_coords && !tex_coords)) return SG_E_ARG;
  portsim::TerrainGlobals &t = ctx->gl_globals;
  t.tex_coords.assign(tex_coords, tex_coords + n_tex_coords);
  if (side_normals) std::memcpy(t.side_normals, side_normals, sizeof t.side_normals);
  t.wall_texture = wall;
  t.fence_texture0 = fence0;
  t.fence_texture1 = fence1;
  std::memcpy(&t.screen_scale, &screen_scale_bits, 4);
  if (screen_scale_bits == 0 && ctx->terrain_dll) t.screen_scale = ctx->terrain_dll->st.screen_scale;   // initSystem's value
  return SG_OK;
}

int sg_ts_arrays(sg_ctx *ctx, float *vertices, float *normals, uint32_t n) {
  if (!ctx) return SG_E_ARG;
  const portsim::TerrainGlobals &g = ctx->gl_globals;
  if (static_cast<size_t>(n) * 3 > g.vertices.size() || static_cast<size_t>(n) * 3 > g.normals.size()) return SG_E_ARG;
  if (vertices) std::memcpy(vertices, g.vertices.data(), static_cast<size_t>(n) * 12);
  if (normals) std::memcpy(normals, g.normals.data(), static_cast<size_t>(n) * 12);
  return SG_OK;
}

int sg_ts_textures(sg_ctx *ctx, uint32_t *out, uint32_t n) {
  if (!ctx || !out || n > ctx->gl_globals.textures.size()) return SG_E_ARG;
  std::memcpy(out, ctx->gl_globals.textures.data(), static_cast<size_t>(n) * 4);
  return SG_OK;
}

int sg_ts_set_collar(sg_ctx *ctx, const int32_t *table, int32_t n) {
  if (!ctx || (n > 0 && !table)) return SG_E_ARG;
  if (n < 0) {
    ctx->gl_globals.collar_set = false;
    ctx->gl_globals.collar.clear();
  } else {
    portsim::terrain_pass_collar_info(ctx->gl_globals, table, n);
  }
  return SG_OK;
}

int sg_ts_set_course_type(sg_ctx *ctx, int32_t type) {
  if (!ctx) return SG_E_ARG;
  ctx->gl_globals.course_type = type;
  return SG_OK;
}

int sg_ts_negctl(int32_t mode) {
  static int32_t prev = 0;
  const int32_t p = prev;
  prev = mode;
  portsim::terrain_state_negctl(mode);
  return p;
}

sg_raster *sg_raster_new(int32_t w, int32_t h) { return (w > 0 && h > 0) ? new sg_raster(w, h) : nullptr; }
void sg_raster_free(sg_raster *r) { delete r; }
void sg_raster_texture(sg_raster *r, const char *hash, const uint8_t *data, size_t n) {
  if (r && hash && (data || n == 0)) r->r.add_texture_bytes(hash, std::vector<uint8_t>(data, data + n));
}
void sg_raster_arrays(sg_raster *r, const float *v, size_t nv, const float *n, size_t nn) {
  if (r) r->r.set_arrays(v, nv, n, nn);
}
int32_t sg_raster_exec(sg_raster *r, const char *text, size_t len) {
  if (!r || !text) return -1;
  int32_t unknown = 0;
  size_t i = 0;
  while (i < len) {
    size_t j = i;
    while (j < len && text[j] != 10) ++j;
    size_t e = j;
    if (e > i && text[e - 1] == 13) --e;
    if (!r->r.exec_line(std::string(text + i, e - i))) ++unknown;
    i = j + 1;
  }
  return unknown;
}
void sg_raster_set_pixels(sg_raster *r, const uint16_t *px, size_t n) {
  if (r && px && n == r->r.pixels().size()) std::memcpy(r->r.pixels().data(), px, n * 2);
}
size_t sg_raster_get_pixels(sg_raster *r, uint16_t *out, size_t cap) {
  if (!r || !out || cap < r->r.pixels().size()) return 0;
  std::memcpy(out, r->r.pixels().data(), r->r.pixels().size() * 2);
  return r->r.pixels().size();
}
void sg_raster_keep_float(sg_raster *r, int32_t on) { if (r) r->r.keep_float(on != 0); }
size_t sg_raster_get_float(sg_raster *r, float *out, size_t cap) {
  if (!r || !out || cap < r->r.float_pixels().size()) return 0;
  std::memcpy(out, r->r.float_pixels().data(), r->r.float_pixels().size() * 4);
  return r->r.float_pixels().size();
}
size_t sg_raster_get_tri_ids(sg_raster *r, int32_t *out, size_t cap) {
  if (!r || !out || cap < r->r.triangle_ids().size()) return 0;
  std::memcpy(out, r->r.triangle_ids().data(), r->r.triangle_ids().size() * 4);
  return r->r.triangle_ids().size();
}
size_t sg_raster_get_triangles(sg_raster *r, double *out, size_t cap) {
  if (!r) return 0;
  const auto &v = r->r.triangles();
  if (out && cap >= v.size()) std::memcpy(out, v.data(), v.size() * 8);
  return v.size();
}
size_t sg_raster_get_rho(sg_raster *r, float *out, size_t cap) {
  if (!r || !out || cap < r->r.rho_pixels().size()) return 0;
  std::memcpy(out, r->r.rho_pixels().data(), r->r.rho_pixels().size() * 4);
  return r->r.rho_pixels().size();
}
size_t sg_raster_get_source(sg_raster *r, float *out, size_t cap) {
  if (!r || !out || cap < r->r.source_pixels().size()) return 0;
  std::memcpy(out, r->r.source_pixels().data(), r->r.source_pixels().size() * 4);
  return r->r.source_pixels().size();
}
int sg_raster_stats(sg_raster *r, int64_t out[5]) {
  if (!r || !out) return SG_E_ARG;
  out[0] = r->r.stats.triangles; out[1] = r->r.stats.culled; out[2] = r->r.stats.drawn_px;
  out[3] = r->r.stats.blended_px; out[4] = r->r.stats.bad_texture;
  return SG_OK;
}

const char *sg_build_info(void) {
#if defined(_M_X64) || defined(__x86_64__)
#define SG_ARCH "x64"
#else
#define SG_ARCH "x86"
#endif
#ifdef NDEBUG
#define SG_CFG "release"
#else
#define SG_CFG "debug"
#endif
#if PORTSIM_NEGCTL
  return "portsim " SG_ARCH " " SG_CFG " negctl";
#else
  return "portsim " SG_ARCH " " SG_CFG;
#endif
}

}  // extern "C"
