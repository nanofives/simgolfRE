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
/* phase 4 (re/PHASE4_BOOT_PLAN.md): sg_run_frames from process start. The context holds the exe's initialised data
 * (sg_exe_fill_state 1) and the trace starts at the game's spawn; the records before the first FRAME run the boot (gameMain's
 * part, mainLoop's Setup, the title menu and Session) and output record 0 is the state at the first frame head. */
SG_API int sg_run_boot(sg_ctx *ctx, const uint8_t *trace, size_t trace_len, int32_t max_frames, uint32_t *out_T,
                       uint64_t *out_hashes, int32_t *frames_done);
/* phase 3.7a: why the last sg_run_frames of this context stopped (empty after a full run); valid until the next call */
SG_API const char *sg_run_frames_detail(sg_ctx *ctx);
/* phase 3.7a (development): one "name=count" line per callee the frame step called on this thread since the last reset; returns
 * the text length (the text is cut at cap - 1); reset != 0 clears the counts */
SG_API int32_t sg_tick_callee_counts(char *buf, int32_t cap, int32_t reset);

/* ---- live runs (roadmap S7.1, portsim/sim/live.h) ----
 * sg_run_boot with the outside values asked on demand from a host instead of read from a trace: the frame-head clock, every
 * clock read the port makes, every cursor and GetAsyncKeyState read, every pump call. The sim runs on the calling thread
 * and blocks inside the callbacks. Every answer is recorded as a schema v1 trace (sg_live_recording) that sg_run_boot
 * replays to the same frame records. out_T / out_hashes / records as sg_run_boot (cap = max_records; max_records < 0:
 * no limit, then out_T / out_hashes may be NULL and the records are read with sg_live_records). frame_ra: the recording's
 * meta frame_ra (each clock site is written as frame_ra + site; 0 = the sites themselves). sg_run_frames_detail says why
 * the session ended. */
typedef struct sg_live_pump {
  uint32_t kind;          /* 1 Blocking, 2 Drain (commands.h PumpKind) */
  int32_t body;           /* a pump of the frame body (P6 / H1) */
  int32_t may_deliver;    /* 0: a batch given here replays at an earlier pump call (applied, counted as a violation) */
  int32_t must_deliver;   /* 1: the host is asked again until it delivers or stops */
  uint32_t frame;         /* live frame: 0 = the boot */
  uint32_t cursor_reads;  /* cursor reads of this frame before this call */
  int32_t pace_site;      /* the paced loop's clock site, 0 = none */
  uint32_t calls;         /* pump calls of this class since the last delivery, this one included */
  int32_t idle_ok;        /* roadmap S7.6: 1 = a decline here replays as an idle pass of a modal loop (live.h) */
} sg_live_pump;
typedef struct sg_live_cmd {  /* one root-window handler call (commands.h Command, kind RootHandler) */
  uint32_t handler;           /* 1..11 (trace_schema.md HANDLER) */
  int32_t a0, a1;
  uint32_t async_ctrl, async_shift;  /* handlers 1 / 4: GetAsyncKeyState(VK_CONTROL) / (VK_SHIFT), low 16 bits */
  uint32_t t;                 /* handlers 2 / 3 / 7: the deadline tail's timeGetTime (0 = no read) */
} sg_live_cmd;
typedef struct sg_live_host {
  void *user;
  /* the head of live frame `frame` (1, 2, ...): 1 with *t set, 0 = end the session */
  int32_t (*frame_head)(void *user, uint32_t frame, uint32_t *t);
  /* a timeGetTime read at `site` (offset from mainLoop's frame-head return address); body = the frame body's own read
     (must answer); returns 1 with *t set, 0 = no value */
  int32_t (*clock)(void *user, int32_t site, int32_t body, uint32_t *t);
  void (*cursor)(void *user, int32_t modal, int32_t *x, int32_t *y);
  uint32_t (*async_key)(void *user, int32_t vk);
  /* 0 decline, 1 deliver (*n commands written to out, at most cap), 2 stop */
  int32_t (*pump)(void *user, const sg_live_pump *p, sg_live_cmd *out, int32_t cap, int32_t *n);
} sg_live_host;
SG_API int sg_live_run(sg_ctx *ctx, const sg_live_host *host, uint32_t frame_ra, int32_t max_records, uint32_t *out_T,
                       uint64_t *out_hashes, int32_t *records);
/* the built-in hosts: kind 1 = a schema v1 trace as the host (data = the trace; the equivalence gate), kind 2 = a script
   (data = the script text, live.cpp SCRIPT HOST), kind 3 = a trace until its first dispatch holding a focus-child key, then a
   script (data = u32 trace length, the trace, the script text; live.cpp PREFIX HOST, roadmap S7.7) */
SG_API int sg_live_run_builtin(sg_ctx *ctx, int32_t kind, const uint8_t *data, size_t n, uint32_t frame_ra,
                               int32_t max_records, uint32_t *out_T, uint64_t *out_hashes, int32_t *records);
/* the last live run's recording: copies min(len, cap) bytes, *len = its length */
SG_API int sg_live_recording(sg_ctx *ctx, uint8_t *buf, size_t cap, size_t *len);
/* the last live run's records from `first`: up to cap records into out_T / out_hashes (n_sim hashes each); returns the count */
SG_API int32_t sg_live_records(sg_ctx *ctx, int32_t first, int32_t cap, uint32_t *out_T, uint64_t *out_hashes);
/* the last live run's counters: violations, first violation record, asks, reasks, clocks, cursors, asyncs, dispatches,
   writes, body / modal / paced pump calls (12 values) */
SG_API int sg_live_stats(sg_ctx *ctx, int64_t out[12]);
/* roadmap S7.6: idle passes (declines at idle_ok calls) and dispatches recorded with a CURSOR record in front of them */
SG_API int sg_live_stats_ext(sg_ctx *ctx, int64_t out[2]);
/* roadmap S7.6: the last live run's LiveMarker positions (gfx.snapshotAt positions in its recording): copies up to cap,
   returns the count */
SG_API int32_t sg_live_marks(sg_ctx *ctx, uint32_t *out, int32_t cap);

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
/* Profiles over the sg_defect switches (S6, log/roadmap/S6/3_merge.md). `original` = every switch 1. `fixed` = the members
 * the caller passes (switch names and values, from re/port/defect_profiles.tsv read by the host): every switch is reset to 1,
 * then each member is set. Returns SG_OK, or SG_E_ARG for an unknown profile or switch, a NULL list with n > 0, n < 0, or a
 * member list given with `original` (nothing is changed then). */
SG_API int sg_profile_set(const char *profile, const char *const *switches, const int32_t *values, int32_t n);
/* "original" when every switch is 1, "fixed" when set by sg_profile_set("fixed", ...) and not changed since, else "custom". */
SG_API const char *sg_profile_get(void);

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
/* up to cap bytes of `name` from `offset` as the port sees it (overlay writes included); bytes read, -1 when absent */
SG_API int64_t sg_files_read(sg_ctx *ctx, const char *name, uint64_t offset, uint8_t *buf, uint64_t cap);

/* ---- terrain renderer (phase 5 step 3, portsim/sim/terrain_render.h) ----
 * The Terrain.dll statics the render path reads, supplied by the host (the test harness fills them from a recorded
 * snapshot, port/test/glcorpus.py). Arrays are copied. Float values travel as IEEE bit patterns where a scalar. */
typedef struct sg_gl_globals {
  const float *vertices; uint32_t n_vertices;       /* 3 floats per vertex */
  const float *normals; uint32_t n_normals;         /* 3 floats per vertex */
  const uint32_t *textures; uint32_t n_textures;    /* [0x25][0x19][9] GL texture ids */
  uint32_t wall_texture, fence_texture0, fence_texture1;
  const float *tex_coords; uint32_t n_tex_coords;   /* floats: [selector][3][4][2] */
  const float *side_normals;                        /* 4 blocks x 2 x 3 floats */
  uint32_t screen_scale_bits;                       /* the x rotation of every pass (a float) */
  int32_t view_index;
} sg_gl_globals;
SG_API int sg_gl_set_globals(sg_ctx *ctx, const sg_gl_globals *g);
/* Run one render entry point of the original over the context's address space (the Terrain object mapped with sg_map)
 * and write the GL calls it makes as text (portsim/sim/gl_sink.h): entry 1 = stripRender(a0 = pivot, a1 = direction,
 * a2 = angle bits), 2 = Terrain::render(a0 = tile address, a1 = angle bits), 3 = localRender(a0 = tile, a1 = pivot,
 * a2 = angle bits), 4 = pathUpdateRender(a0 = pivot, a1 = angle bits); SG_E_NO_FUNCTION otherwise. out gets the text (NUL-terminated); *len = its length; SG_E_ARG when cap is too small. After the
 * call sg_gl_view_index returns the view quadrant the pass left in the globals. */
SG_API int sg_gl_render(sg_ctx *ctx, int32_t entry, uint32_t terrain, uint32_t a0, uint32_t a1, uint32_t a2, char *out,
                        size_t cap, size_t *len);
SG_API int32_t sg_gl_view_index(sg_ctx *ctx);
/* The GL setup of Terrain.dll produced by the port from the context's game files (portsim/sim/terrain_gl_setup.h): the Terrain
   constructor's texture load without a context, initSystem (800 x 600, display surface, y flipped), initTerrain. The calls come
   back as text (portsim/sim/gl_sink.h); textures_out (may be NULL) receives the texture table, 0x25 * 0x19 * 9 names. */
SG_API int sg_gl_setup(sg_ctx *ctx, char *out, size_t cap, size_t *len, uint32_t *textures_out);
typedef struct sg_raster sg_raster;
/* the same setup with the rasteriser as the sink (its state starts with no context, like the original's constructor) */
SG_API int sg_gl_setup_to_raster(sg_ctx *ctx, sg_raster *r, int32_t zoom, uint32_t *textures_out);   /* zoom 0 / 1 / 2: setZoomLevel after */
typedef struct sg_raster sg_raster;
/* the same render entry, with the rasteriser as the sink: the port's Terrain::render drives sg_raster directly (its state
   already set up with sg_raster_exec, its arrays taken from sg_gl_set_globals) */
SG_API int sg_gl_render_to_raster(sg_ctx *ctx, sg_raster *r, int32_t entry, uint32_t terrain, uint32_t a0, uint32_t a1, uint32_t a2);
SG_API int sg_gl_negctl(int32_t mode);                    /* deliberately wrong renderer bodies (0 = correct); previous mode */

/* ---- terrain state side (phase 5 step 3, portsim/sim/terrain_state.h) over the context's address space and the globals of
 * sg_gl_set_globals. One entry per operation, by name:
 *   construct (t = Terrain, a0 = width, a1 = height)   build_arrays (t)          init_grid (a0 = width, a1 = height)
 *   tile_reset (t = tile, a0 = x, a1 = y)               faces_build (t = tile, a0 = x, a1 = y, a2 = width)
 *   face_normal (t = face)   smooth_normals (t = tile, a0, a1)   calc_normals (t = tile; the exported, NULL-safe one)
 *   calc_all_normals (t = Terrain, a0 = tile)   set_face_texture (t = tile, a0 = type, a1 = face, a2 = piece)
 *   blend (t = tile, a0 = type or -1)   reblend (t = tile)   reset_face_textures (t = tile)   edge_kind (t = tile, a0 = other)
 *   tile_at (t = Terrain, a0 = x, a1 = y)
 *   load_dll_constants: Terrain.dll's constant tables from the context's files (ret 1, 0 without a Terrain.dll)
 *   gl_setup (t = Terrain, a0 = zoom 0 / 1 / 2): the port's GL setup from the context's files (sg_gl_setup), its results written
 *            into the object and the globals (texture table, type names and counts, flags, window size, zoom, course type)
 * *ret (may be NULL) receives the value the operation returns (0 for void ones). */
SG_API int sg_ts_op(sg_ctx *ctx, const char *op, uint32_t t, int32_t a0, int32_t a1, int32_t a2, int32_t *ret);
SG_API int sg_ts_arrays(sg_ctx *ctx, float *vertices, float *normals, uint32_t n);   /* copy out the first n vertices / normals */
SG_API int sg_ts_textures(sg_ctx *ctx, uint32_t *out, uint32_t n);                   /* copy out the texture table */
SG_API int sg_ts_set_collar(sg_ctx *ctx, const int32_t *table, int32_t n);           /* passCollarInfo; n < 0 = NULL again */
SG_API int sg_ts_set_course_type(sg_ctx *ctx, int32_t type);
/* The exported Terrain methods as the exe calls them (terrain_dll_call): sg_ts_dll_begin starts a fresh DLL state (globals reset, the
   22,500-entry arrays zeroed) whose getInstance builds the object at `terrain` (the host maps 0x164ad0 bytes there, filled 0xcd);
   sg_ts_call runs one method by undecorated name with ecx = self and the five stack dwords; table / n = passCollarInfo's ints. */
SG_API int sg_ts_dll_begin(sg_ctx *ctx, uint32_t terrain);
SG_API int sg_ts_call(sg_ctx *ctx, const char *name, uint32_t self, const uint32_t *args, const int32_t *table, int32_t n, uint32_t *ret);
SG_API int32_t sg_ts_view_index(sg_ctx *ctx);
/* LiveTerrain (portsim/sim/terrain_live.h): the context's TerrainApi becomes the port's own Terrain.dll state, so a run's exe-side calls
   build the object (at `va`, 0 = a free range). snap_after: call counts after which the state is copied. The log has one line per
   call in re/frida/js/terrain_calls.js's form; sg_ts_live_snapshot(k < 0) returns the number of copies, else fills copy k (object
   0x164ad0 bytes, 22,500 x 3 floats each array, 0x25 * 0x19 * 9 texture names) and returns the object size. */
SG_API int sg_ts_live_attach(sg_ctx *ctx, uint32_t va, const uint32_t *snap_after, int32_t n);
SG_API int sg_ts_live_detach(sg_ctx *ctx);
SG_API uint32_t sg_ts_live_object(sg_ctx *ctx);
SG_API int sg_ts_live_log(sg_ctx *ctx, char *out, size_t cap, size_t *len);
SG_API int32_t sg_ts_live_snapshot(sg_ctx *ctx, int32_t k, uint8_t *object, float *vertices, float *normals, uint32_t *textures,
                                   int32_t *view, uint32_t *after);
/* The renderer's inputs the state side does not produce, set without touching the replayed arrays and table: the constant
   texture-coordinate and side-normal tables of the DLL's data, the wall / fence textures and the screen scale (bits; 0 = the value
   the replayed initSystem set). */
/* the renderer inputs the state side holds: the texture-coordinate table (up to cap floats; *n = its length), the side normals
   (24 floats), the wall / fence textures and the screen scale's bits */
SG_API int sg_ts_render_data(sg_ctx *ctx, float *tex_coords, uint32_t cap, uint32_t *n_tex_coords, float *side_normals,
                             uint32_t *wall, uint32_t *fence0, uint32_t *fence1, uint32_t *screen_scale_bits);
SG_API int sg_ts_set_render_data(sg_ctx *ctx, const float *tex_coords, uint32_t n_tex_coords, const float *side_normals,
                                 uint32_t wall, uint32_t fence0, uint32_t fence1, uint32_t screen_scale_bits);
SG_API int sg_ts_negctl(int32_t mode);                                               /* previous mode */

/* ---- software rasteriser (phase 5 step 3, portsim/sim/gl_raster.h): replay a recorded GL stream into an RGB555 frame ---- */
typedef struct sg_raster sg_raster;
SG_API sg_raster *sg_raster_new(int32_t w, int32_t h);
SG_API void sg_raster_free(sg_raster *r);
SG_API void sg_raster_texture(sg_raster *r, const char *hash, const uint8_t *data, size_t n);  /* tex=<hash> bytes (RGB or RGBA) */
SG_API void sg_raster_arrays(sg_raster *r, const float *vertices, size_t n_vertices, const float *normals, size_t n_normals);
/* newline-separated recorder lines; returns the number of lines whose call is unknown to the rasteriser */
SG_API int32_t sg_raster_exec(sg_raster *r, const char *text, size_t len);
SG_API void sg_raster_set_pixels(sg_raster *r, const uint16_t *px, size_t n);   /* n = w * h */
SG_API size_t sg_raster_get_pixels(sg_raster *r, uint16_t *out, size_t cap);    /* pixels copied (0 when cap is short) */
SG_API void sg_raster_keep_float(sg_raster *r, int32_t on);                     /* diagnostics: keep pre-quantisation colours */
SG_API size_t sg_raster_get_float(sg_raster *r, float *out, size_t cap);        /* w * h * 3 floats (-1 = never written) */
SG_API size_t sg_raster_get_tri_ids(sg_raster *r, int32_t *out, size_t cap);  /* w * h: triangle index (keep_float), -1 none */
SG_API size_t sg_raster_get_triangles(sg_raster *r, double *out, size_t cap); /* 12 doubles per triangle: x,y x3 (ccw), s,t x3; returns the count of doubles */
SG_API size_t sg_raster_get_rho(sg_raster *r, float *out, size_t cap);          /* w * h floats: rho (keep_float), -1 unwritten */
SG_API size_t sg_raster_get_source(sg_raster *r, float *out, size_t cap);       /* w * h * 8 floats: src rgb, alpha, dst rgb, blended */
SG_API int sg_raster_stats(sg_raster *r, int64_t out[5]);   /* triangles, culled, drawn px, blended px, skipped */

/* ---- audio core (step S4.2, portsim/sim/audio.h): the sound.dll port's calls outside itself ----
 * A port function calls a callback, a system DLL export, a COM method, a sound.dll address the host claims, or (development
 * only) a sound.dll function no port function is registered for: each goes to the context's host. sg_snd_set_host installs
 * a C callback as that host (fn NULL removes it); the callback fills eax / edx / st0 / has_st0 and returns 0, anything else
 * stops the port call (sg_call returns SG_E_EXCEPTION). The same struct is portsim::SndCallC. Configuration (image base,
 * claims, the recorded-script host) goes through sg_call entries "audio_core.*" and the files "$audio/..." (audio_core.cpp). */
typedef struct sg_snd_call {
  uint32_t va;        /* target virtual address (thunks followed) */
  uint32_t rva;       /* va - image base inside sound.dll, else 0 */
  int32_t kind;       /* 0 external, 1 claimed by the host, 2 fallback (no port function yet) */
  int32_t abi;        /* 0 cdecl, 1 stdcall, 2 thiscall (args[0] = ecx), 3 fastcall (args[0..1] = ecx, edx) */
  int32_t nargs;
  int32_t want_st0;   /* the caller reads st(0) */
  uint32_t args[16];
  uint32_t eax, edx;  /* results */
  double st0;
  int32_t has_st0;
  int32_t status;     /* 0 = served */
} sg_snd_call;
typedef int (*sg_snd_host_fn)(void *user, sg_snd_call *call);
SG_API int sg_snd_set_host(sg_ctx *ctx, sg_snd_host_fn fn, void *user);

SG_API const char *sg_build_info(void);                   /* "portsim <arch> <config>" */

#ifdef __cplusplus
}
#endif
#endif /* PORTSIM_C_H */
