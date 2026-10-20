// The portsim C ABI (include/portsim_c.h) over the registry, the context and the object table (phase 2.6, 2026-10-19).
#include "portsim_c.h"

#include <cstring>

#include <memory>
#include <string>
#include <vector>

#include "portsim/execonst.h"
#include "portsim/gamefiles.h"
#include "portsim/registry.h"
#include "portsim/registry_list.h"
#include "portsim/state_hash.h"
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

struct sg_ctx {
  portsim::Ctx ctx;
  portsim::MemoryGameFiles mem_files;                // phase 3.0: the default game files of a context
  std::unique_ptr<portsim::DirGameFiles> dir_files;  // set by sg_files_root
  std::string run_detail;                            // phase 3.7a: why sg_run_frames stopped
  sg_ctx() { ctx.files_ptr = &mem_files; }
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
    const int r = portsim::tick_run_frames(c->ctx, trace, n, q, nq, max_frames, out_T, out_hashes, done, &c->run_detail);
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

const char *sg_run_frames_detail(sg_ctx *ctx) { return ctx ? ctx->run_detail.c_str() : ""; }

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
  ctx->mem_files.put(name, n ? std::vector<uint8_t>(data, data + n) : std::vector<uint8_t>());
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
