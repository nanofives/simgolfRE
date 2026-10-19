/* portsim C ABI (phase 2.6, 2026-10-19): the surface the Python test harness (port/test/) calls through ctypes.
 *
 * Built as portsim_c.dll (CMake target portsim_c, x64 to match the 64-bit Python; Win32 builds too). No original
 * address appears in the library: the harness chooses every base of the 32-bit address space below.
 *
 * ADDRESS SPACE. The original is a 32-bit program whose state holds 32-bit pointers (the model stores them as
 * uint32_t). A context therefore owns a small 32-bit VIRTUAL address space: the GameState is mapped at a base the
 * caller sets (sg_set_data_base, default 0x20000000), and the harness maps its own memory blocks (fixture objects of the
 * vector corpus) with sg_map. A pointer argument, a pointer stored in state and a pointer return are all 32-bit virtual
 * addresses; port code turns them into host memory through the context (portsim/registry.h), never by casting.
 *
 * CALLS. sg_call runs a registered port function by NAME (the hooks.csv name of the original function, e.g.
 * "Random::range"). Arguments are 32-bit slots in the original's stack order (an 8-byte argument takes two slots, low
 * dword first; thiscall `this` and fastcall ecx/edx are slots 0 / 0-1 as in the vector corpus). The result is what the
 * original leaves in eax, edx and st(0).
 */
#ifndef PORTSIM_C_H
#define PORTSIM_C_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#  ifdef PORTSIM_C_BUILD
#    define SG_API __declspec(dllexport)
#  else
#    define SG_API __declspec(dllimport)
#  endif
#else
#  define SG_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* status codes */
#define SG_OK 0
#define SG_E_NO_FUNCTION (-1)     /* no port function of that name is registered */
#define SG_E_ARITY (-2)           /* wrong number of argument slots */
#define SG_E_FAULT (-3)           /* the port function dereferenced an address no mapping covers */
#define SG_E_MAP (-4)             /* sg_map: overlapping or out-of-range mapping */
#define SG_E_UNSUPPORTED (-5)     /* e.g. an x87 control word this build cannot honour */
#define SG_E_NOT_IMPLEMENTED (-6) /* sg_run_frames until phases 3.7 / 3.8 provide the frame step */
#define SG_E_ARG (-7)             /* bad argument (NULL, unknown class, short buffer) */
#define SG_E_EXCEPTION (-8)       /* the port function threw something other than a fault */
#define SG_E_DIVIDE (-9)          /* phase 3.0: an idiv / div the original runs with divisor 0 or INT_MIN / -1 (#DE) */
#define SG_E_NO_RETURN (-10)      /* phase 3.0: a loop the original can never leave from this state */
#define SG_E_NO_RESOURCE (-11)    /* phase 3.0: no exe constants / game files attached, or an unknown name */

typedef struct sg_ctx sg_ctx;

/* the original's register results of one call */
typedef struct sg_ret {
  uint32_t eax, edx;
  double st0;        /* valid when has_st0 */
  int32_t has_st0;
} sg_ret;

/* registry metadata of one port function */
typedef struct sg_func_info {
  const char *name;   /* hooks.csv name, e.g. "Random::range" */
  const char *module; /* port module file (port/src/sim/<module>.cpp) */
  const char *abi;    /* the ORIGINAL's convention as the vector corpus names it: default / thiscall / stdcall / fastcall */
  const char *ret;    /* the corpus return type: int, uint32, uint8, int8, uint16, bool, pointer, double, void ... */
  int32_t arity;      /* argument slots */
  uint32_t fpcw;      /* x87 control word the original ran under when the result depends on it (0x027f), 0 = none */
} sg_func_info;

/* ---- state and address space ---- */
SG_API size_t sg_state_size(void);                        /* sizeof(portsim::GameState) = 0x381174 */
SG_API sg_ctx *sg_state_new(void);                        /* zero-filled GameState on the heap, no block mapped */
SG_API void sg_state_free(sg_ctx *ctx);
SG_API uint8_t *sg_state_ptr(sg_ctx *ctx);                /* the GameState bytes */
SG_API int sg_set_data_base(sg_ctx *ctx, uint32_t base);  /* virtual base of the GameState (before any sg_map) */
SG_API uint32_t sg_data_base(sg_ctx *ctx);
/* map `size` bytes of caller memory at virtual `base` (caller keeps ownership; read-only when `readonly`) */
SG_API int sg_map(sg_ctx *ctx, uint32_t base, void *host, uint32_t size, int32_t readonly);
SG_API void sg_unmap_all(sg_ctx *ctx);                    /* drop every sg_map mapping (the GameState stays) */

/* ---- registry and calls ---- */
SG_API int32_t sg_func_count(void);
SG_API const char *sg_func_name(int32_t i);
SG_API int sg_func_info_get(int32_t i, sg_func_info *out);
SG_API int32_t sg_func_find(const char *name);           /* index or SG_E_NO_FUNCTION */
SG_API int sg_call(sg_ctx *ctx, const char *name, const uint32_t *args, int32_t nargs, sg_ret *out);
/* x87 control word for the calling thread: Win32 sets it; x64 (SSE2 doubles) accepts only 0x027f-equivalent
 * precision/rounding (53-bit, nearest) and returns SG_E_UNSUPPORTED otherwise */
SG_API int sg_set_fpcw(uint32_t cw);

/* ---- negative controls: deliberately wrong bodies, selected at run time (never a fixed-address page) ---- */
SG_API int sg_negctl_set(const char *name, int32_t mode); /* mode 0 = correct body; returns previous mode */
SG_API int32_t sg_negctl_get(const char *name);
SG_API void sg_negctl_reset(void);

/* ---- object table and hashes (gamestate_meta.inc order) ---- */
SG_API int32_t sg_object_count(const char *cls);          /* cls NULL = every object */
SG_API const char *sg_object_name(const char *cls, int32_t i);
SG_API int sg_object_span(const char *cls, int32_t i, uint32_t *off, uint32_t *size);
SG_API uint64_t sg_hash_bytes(const uint8_t *data, size_t n);  /* sgh64 (re/tools/state_image.py) */
/* sgh64 of every object of class `cls` of the context's state, pointer words hashed as 0; returns the count */
SG_API int32_t sg_hash_class(sg_ctx *ctx, const char *cls, uint64_t *out, int32_t cap);

/* ---- segments (phase 3.7 / 3.8) ----
 * Advance the state frame by frame through a trace slice (re/model/trace_schema.md, schema v1, the segment's
 * trace.trace bytes) after posting `queued` (start.json queued_messages: msg, wp, lp, cls x n_queued). For each frame head
 * k = 1..max_frames reached, writes T to out_T[k-1] and the sgh64 of the sim class objects (sg_object_count("sim"),
 * gamestate_meta.inc order) to out_hashes[(k-1) * n_sim ...]. *frames_done = frame heads reached. Returns
 * SG_E_NOT_IMPLEMENTED until the frame step exists; the signature is final. */
typedef struct sg_msg {
  uint32_t msg, wp, lp, cls;
} sg_msg;
SG_API int sg_run_frames(sg_ctx *ctx, const uint8_t *trace, size_t trace_len, const sg_msg *queued, int32_t n_queued,
                         int32_t max_frames, uint32_t *out_T, uint64_t *out_hashes, int32_t *frames_done);

/* ---- compatibility switches for defects of the original (phase 3.0, port/CORE.md) ----
 * Process-wide; 1 (default) = reproduce the original, 0 = the fixed behaviour. */
typedef struct sg_defect_info {
  const char *name;   /* "<function>.<what>" */
  const char *module; /* port module file */
  const char *ref;    /* re/analysis/data/model_defects.md entry */
  const char *what;
} sg_defect_info;
SG_API int32_t sg_defect_count(void);
SG_API int sg_defect_info_get(int32_t i, sg_defect_info *out);
SG_API int sg_defect_set(const char *name, int32_t on); /* returns the previous value, or SG_E_ARG for an unknown name */
SG_API int32_t sg_defect_get(const char *name);         /* 0 / 1, or SG_E_ARG */
SG_API void sg_defect_reset(void);                      /* every switch back to 1 */

/* ---- exe constants: the user's own golf_clean.exe (phase 3.0, portsim/execonst.h) ----
 * One process-wide instance, attached to every context at sg_call. sg_exe_load verifies the file (the .data size and
 * every descriptor's hash) and returns SG_E_NO_RESOURCE with a reason in err (may be NULL) when it is not the expected
 * exe. */
SG_API int sg_exe_load(const char *path, char *err, size_t errcap);
SG_API int32_t sg_exe_loaded(void);
/* a descriptor's bytes by model name: copies min(size, cap) bytes, *size = the descriptor's size */
SG_API int sg_exe_const(const char *name, uint8_t *out, uint32_t cap, uint32_t *size);
/* initial GameState values from the exe's initialised .data: mode 0 = only `const`-class objects, 1 = the whole image */
SG_API int sg_exe_fill_state(sg_ctx *ctx, int32_t mode);

/* ---- game files (phase 3.0, portsim/gamefiles.h): the context's in-memory files, or a directory ---- */
SG_API int sg_files_put(sg_ctx *ctx, const char *name, const uint8_t *data, size_t n);
SG_API void sg_files_clear(sg_ctx *ctx);
SG_API int sg_files_root(sg_ctx *ctx, const char *dir);     /* read <dir>/<name> instead (NULL: back to memory) */
SG_API int64_t sg_files_size(sg_ctx *ctx, const char *name); /* -1 when absent */

SG_API const char *sg_build_info(void);                   /* "portsim <arch> <config>" */

#ifdef __cplusplus
}
#endif
#endif /* PORTSIM_C_H */
