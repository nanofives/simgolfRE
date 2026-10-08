// Fixtures for C3 batch c3bj (round 15, golf_clean.exe): eight IJG libjpeg 6a compress-side setup routines and
// one Bink player setter. Only our own synthetic values (no game text, no live game object). Names are
// prefixed c3bj_.
//
// Allocations go through Memory.alloc (diff_hook rewrites it to __keepAlloc so blocks referenced only from a
// field survive the run); NativeCallbacks are pushed onto __diffKeepAlive by hand.
//
// Every callback is a pure function of its arguments or of memory diff_hook snapshots and restores between the
// two arms, so the second arm sees exactly the world the first one saw.
//
// libjpeg structures: cinfo+0 = err (jpeg_error_mgr: error_exit +0, emit_message +4, msg_code +0x14,
// msg_parm.i[] +0x18), cinfo+4 = mem (jpeg_memory_mgr: alloc_small +0, alloc_large +4, alloc_sarray +8; we keep
// this cinfo's small-allocation pool at mem+0x10 and its row-pointer pool at mem+0x14, both past the slots the
// code reads). A jpeg_component_info is 0x54 bytes (component_id +0, component_index +4, h_samp_factor +8,
// v_samp_factor +0xc, width_in_blocks +0x1c, height_in_blocks +0x20, ...).
(function () {
  function keep(cb) { globalThis.__diffKeepAlive.push(cb); return cb; }
  function zero(p, n) { for (let i = 0; i < n; i += 4) p.add(i).writeU32(0); }
  function fill(p, n, v) { for (let i = 0; i < n; i += 4) p.add(i).writeU32(v); }
  function alloc(n, v) { const p = Memory.alloc(n); if (v === undefined) zero(p, n); else fill(p, n, v); return p; }

  const POOL = 0x1100;        // per-cinfo alloc_small pool: 0x40 slots of 0x40 bytes plus slack
  const SPOOL = 0x2000;       // per-cinfo alloc_sarray pool: 0x20 slots of 0x100 bytes (0x40 row pointers)

  // alloc_small(cinfo, pool_id, size): a slot of this cinfo's pool chosen by the SIZE alone. Both arms get the
  // same pointer (a fresh Memory.alloc per call would differ and read RED), and two live allocations of
  // different sizes land in different slots. __cdecl.
  const ALLOC_SMALL = keep(new NativeCallback(function (cinfo, pool, size) {
    return cinfo.add(4).readPointer().add(0x10).readPointer().add((size & 0x3f) * 0x40);
  }, 'pointer', ['pointer', 'int', 'int']));

  // alloc_sarray, tag flavour: jinit_c_main_controller and jinit_c_prep_controller only STORE the pointer, so
  // we hand back a value that encodes the two computed arguments (samples per row and rows) and is never
  // dereferenced. The stored dword therefore shows both computations. __cdecl.
  const ALLOC_SARRAY_TAG = keep(new NativeCallback(function (cinfo, pool, spr, rows) {
    return ptr(0x60000000 + ((spr & 0xffff) * 0x100) + (rows & 0xff));
  }, 'pointer', ['pointer', 'int', 'int', 'int']));

  // alloc_sarray, real flavour: create_context_buffer READS the row pointers it gets back, so this one returns
  // a slot of the cinfo's row-pointer pool, chosen by the samples-per-row alone. The pool is pre-filled with a
  // distinct dword per position and the function never writes it, so it stays identical in both arms. __cdecl.
  const ALLOC_SARRAY_REAL = keep(new NativeCallback(function (cinfo, pool, spr, rows) {
    return cinfo.add(4).readPointer().add(0x14).readPointer().add((spr & 0x1f) * 0x100);
  }, 'pointer', ['pointer', 'int', 'int', 'int']));

  // error_exit(cinfo) / emit_message(cinfo, level): libjpeg's error_exit never returns; ours records the
  // message in the error manager's msg_parm area (bytes +0x30..+0x3f, which none of these functions writes) and
  // returns, so the instructions after every ERREXIT run - identically in both arms. The counter and the hash
  // make the number and the order of the messages part of the compared state.
  const ERR_EXIT = keep(new NativeCallback(function (cinfo) {
    const e = cinfo.readPointer();
    e.add(0x30).writeU32((Math.imul(e.add(0x30).readU32(), 33) + e.add(0x14).readU32()) >>> 0);
    e.add(0x34).writeU32((e.add(0x34).readU32() + 1) >>> 0);
  }, 'void', ['pointer']));
  // The same recorder, plus it clears comps_in_scan (cinfo+0xec). per_scan_setup's bound check at 0x004b5b43
  // raises ERREXIT 0x18 for a count above 4 and, because our handler returns, would then walk cur_comp_info
  // past its fourth entry - that slot is MCUs_per_row (cinfo+0x100), which the function has just written as an
  // integer, so the ORIGINAL would dereference a non-pointer. Clearing the count (the same write in both arms,
  // inside a compared region) lets the `jle` at 0x004b5b43 be exercised on its not-taken side.
  const ERR_EXIT_CLAMP = keep(new NativeCallback(function (cinfo) {
    const e = cinfo.readPointer();
    e.add(0x30).writeU32((Math.imul(e.add(0x30).readU32(), 33) + e.add(0x14).readU32()) >>> 0);
    e.add(0x34).writeU32((e.add(0x34).readU32() + 1) >>> 0);
    cinfo.add(0xec).writeS32(0);
  }, 'void', ['pointer']));
  const EMIT_MSG = keep(new NativeCallback(function (cinfo, level) {
    const e = cinfo.readPointer();
    e.add(0x38).writeU32((Math.imul(e.add(0x38).readU32(), 33) + e.add(0x14).readU32() + level * 7) >>> 0);
    e.add(0x3c).writeU32((e.add(0x3c).readU32() + 1) >>> 0);
  }, 'void', ['pointer', 'int']));

  // A 0x84-byte jpeg_error_mgr pre-filled 0x22 (so a write anywhere in it shows) with our two handlers and
  // zeroed recorder slots.
  function mkErr() {
    const e = alloc(0x84, 0x22222222);
    e.writePointer(ERR_EXIT);
    e.add(4).writePointer(EMIT_MSG);
    zero(e.add(0x30), 0x10);
    return e;
  }

  // A jpeg_memory_mgr. `sarray` is the alloc_sarray callback (tag or real); the pools live past the slots.
  function mkMem(sarray) {
    const mem = alloc(0x20);
    const pool = alloc(POOL, 0xcccccccc);
    mem.writePointer(ALLOC_SMALL);
    if (sarray) mem.add(8).writePointer(sarray);
    mem.add(0x10).writePointer(pool);
    if (sarray === ALLOC_SARRAY_REAL) {
      const sp = alloc(SPOOL);
      for (let k = 0; k < SPOOL; k += 4) sp.add(k).writeU32(0x70000000 + k);
      mem.add(0x14).writePointer(sp);
    }
    return { mem: mem, pool: pool };
  }

  // 16 jpeg_component_info entries pre-filled 0x99, with (h_samp_factor, v_samp_factor, width_in_blocks,
  // height_in_blocks) taken from `rows` for the first entries.
  function mkComps(rows) {
    const k = alloc(16 * 0x54, 0x99999999);
    (rows || []).forEach(function (r, i) {
      const cp = k.add(i * 0x54);
      cp.add(8).writeS32(r[0]);
      cp.add(0xc).writeS32(r[1]);
      cp.add(0x1c).writeS32(r.length > 2 ? r[2] : 0);
      cp.add(0x20).writeS32(r.length > 3 ? r[3] : 0);
    });
    return k;
  }

  // A 0x180-byte jpeg_compress_struct with our err, mem and comp_info, plus the named fields of `f`.
  const FIELD = { global_state: 0x10, image_width: 0x18, image_height: 0x1c, input_components: 0x20,
                  in_color_space: 0x24, data_precision: 0x30, num_components: 0x34, jpeg_color_space: 0x38,
                  raw_data_in: 0xa8, CCIR601_sampling: 0xb4, smoothing_factor: 0xb8, restart_in_rows: 0xc4,
                  max_h: 0xe0, max_v: 0xe4, comps_in_scan: 0xec };
  function mkCinfo(sarray, comps, f) {
    const m = mkMem(sarray);
    const err = mkErr();
    const c = alloc(0x180);
    c.writePointer(err);
    c.add(4).writePointer(m.mem);
    if (comps) c.add(0x3c).writePointer(comps);
    Object.keys(f || {}).forEach(function (name) { c.add(FIELD[name]).writeS32(f[name]); });
    return { c: c, err: err, pool: m.pool, comps: comps };
  }

  // Build one fixture entry set from a list of {sarray, comps, fields} rows.
  function build(rows, sarray, extra) {
    const fx = {};
    rows.forEach(function (r, i) {
      const comps = mkComps(r.comps);
      const o = mkCinfo(sarray, comps, r.f);
      fx['c' + i] = o.c;
      fx['e' + i] = o.err;
      fx['p' + i] = o.pool;
      fx['k' + i] = comps;
      if (extra) extra(fx, o, r, i);
      if (i === 0) fx.obj = o.c;
    });
    return fx;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    // ---- jinit_c_main_controller 0x004b0560 ---------------------------------------------------------------
    // raw_data_in selects the early return (and is tested BEFORE need_full_buffer), need_full_buffer selects
    // the ERREXIT, num_components selects how many strip buffers are allocated. alloc_sarray is the tag
    // flavour, so the dword stored at main+0x18+4*ci shows width_in_blocks * 8 and v_samp_factor * 8.
    c3bj_main() {
      return build([
        { f: { raw_data_in: 1, num_components: 3 }, comps: [[1, 1, 4], [1, 1, 4], [1, 1, 4]] },
        { f: { raw_data_in: 1, num_components: 3 }, comps: [[2, 2, 6], [1, 1, 3], [1, 1, 3]] },
        { f: { raw_data_in: 0, num_components: 3 }, comps: [[1, 1, 4], [1, 1, 4], [1, 1, 4]] },
        { f: { raw_data_in: 0, num_components: 2 }, comps: [[1, 1, 5], [1, 1, 5]] },
        { f: { raw_data_in: 0, num_components: 0 }, comps: [] },
        { f: { raw_data_in: 0, num_components: -1 }, comps: [] },
        { f: { raw_data_in: 0, num_components: 1 }, comps: [[1, 1, 4]] },
        { f: { raw_data_in: 0, num_components: 2 }, comps: [[2, 2, 7], [1, 1, 3]] },
        { f: { raw_data_in: 0, num_components: 3 }, comps: [[1, 2, 5], [2, 1, 9], [4, 4, 1]] },
        { f: { raw_data_in: 0, num_components: 4 }, comps: [[1, 1, 0], [1, 1, 0x1000], [3, 2, 17], [2, 3, 255]] },
        { f: { raw_data_in: 0, num_components: 10 },
          comps: [[1, 1, 1], [1, 2, 2], [2, 1, 3], [2, 2, 4], [3, 1, 5],
                  [1, 3, 6], [4, 4, 7], [3, 3, 8], [2, 4, 9], [4, 2, 10]] },
        { f: { raw_data_in: 0, num_components: 2 }, comps: [[0, 0, 0], [-1, -2, -3]] },
      ], ALLOC_SARRAY_TAG);
    },

    // ---- jinit_c_prep_controller 0x004b3a80 ---------------------------------------------------------------
    // need_context_rows = 0 in the downsampler at cinfo+0x15c: the per-component colour buffers are allocated
    // here. The samples-per-row is (width_in_blocks * max_h * 8) / h_samp_factor with a SIGNED division, so one
    // row uses a negative h_samp_factor.
    c3bj_prep() {
      return build([
        { f: { num_components: 1, max_h: 1, max_v: 1 }, comps: [[1, 1, 8]] },
        { f: { num_components: 3, max_h: 2, max_v: 2 }, comps: [[2, 2, 10], [1, 1, 5], [1, 1, 5]] },
        { f: { num_components: 0, max_h: 1, max_v: 1 }, comps: [] },
        { f: { num_components: -2, max_h: 1, max_v: 1 }, comps: [] },
        { f: { num_components: 2, max_h: 2, max_v: 1 }, comps: [[2, 1, 9], [1, 1, 4]] },
        { f: { num_components: 1, max_h: 4, max_v: 3 }, comps: [[4, 3, 7]] },
        { f: { num_components: 2, max_h: 3, max_v: 2 }, comps: [[1, 1, 11], [3, 2, 4]] },
        { f: { num_components: 4, max_h: 4, max_v: 4 }, comps: [[1, 1, 1], [2, 2, 2], [4, 4, 3], [1, 1, 0]] },
        { f: { num_components: 1, max_h: 1, max_v: 1 }, comps: [[-1, 1, 5]] },
        { f: { num_components: 2, max_h: 0, max_v: 0 }, comps: [[1, 1, 6], [2, 2, 7]] },
        { f: { num_components: 1, max_h: 2, max_v: 1 }, comps: [[3, 1, 5]] },
      ], ALLOC_SARRAY_TAG, function (fx, o, r, i) {
        const ds = alloc(0x34);               // downsample object: need_context_rows (+8) stays 0
        o.c.add(0x15c).writePointer(ds);
      });
    },

    // ---- jinit_c_prep_controller 0x004b3a80, context path -------------------------------------------------
    // need_context_rows != 0: prep+4 takes 0x004b3d40 and create_context_buffer 0x004b3f10 runs, which needs a
    // real alloc_sarray. The fake-pointer block it allocates is num_components * max_v * 5 * 4 bytes; none of
    // the four sizes here is a multiple of 0x40, so it never lands in the 0x40-byte slot the prep object uses.
    c3bj_prep_ctx() {
      return build([
        { f: { num_components: 3, max_h: 2, max_v: 1 }, comps: [[2, 1, 10], [1, 1, 5], [1, 1, 7]] },   // 60
        { f: { num_components: 1, max_h: 1, max_v: 2 }, comps: [[1, 2, 8]] },                           // 40
        { f: { num_components: 2, max_h: 2, max_v: 3 }, comps: [[2, 3, 6], [1, 1, 3]] },                // 120
        { f: { num_components: 2, max_h: 4, max_v: 1 }, comps: [[4, 1, 2], [2, 1, 9]] },                // 40
      ], ALLOC_SARRAY_REAL, function (fx, o, r, i) {
        const ds = alloc(0x34);
        ds.add(8).writeS32(1);                // need_context_rows
        o.c.add(0x15c).writePointer(ds);
      });
    },

    // ---- create_context_buffer 0x004b3f10 -----------------------------------------------------------------
    // The pre-processing controller is supplied by the fixture (cinfo+0x14c) and pre-filled 0x55, so the
    // colour_buf writes at prep+8 are observable. Rows 6 and 7 skip the component loop, row 8 has
    // max_v_samp_factor 0 so the wraparound loop is skipped as well.
    c3bj_ctx() {
      return build([
        { f: { num_components: 1, max_h: 1, max_v: 1 }, comps: [[1, 1, 8]] },
        { f: { num_components: 3, max_h: 2, max_v: 1 }, comps: [[2, 1, 10], [1, 1, 5], [1, 1, 7]] },
        { f: { num_components: 2, max_h: 2, max_v: 2 }, comps: [[1, 2, 4], [2, 2, 6]] },
        { f: { num_components: 1, max_h: 1, max_v: 3 }, comps: [[1, 3, 3]] },
        { f: { num_components: 4, max_h: 4, max_v: 1 }, comps: [[4, 1, 2], [2, 1, 3], [1, 1, 4], [1, 1, 5]] },
        { f: { num_components: 2, max_h: 1, max_v: 2 }, comps: [[1, 2, 1], [1, 2, 2]] },
        { f: { num_components: 0, max_h: 1, max_v: 1 }, comps: [] },
        { f: { num_components: -1, max_h: 1, max_v: 1 }, comps: [] },
        { f: { num_components: 1, max_h: 1, max_v: 0 }, comps: [[1, 1, 9]] },
        { f: { num_components: 2, max_h: 3, max_v: 1 }, comps: [[3, 1, 5], [1, 1, 2]] },
        { f: { num_components: 1, max_h: 2, max_v: 4 }, comps: [[2, 4, 3]] },
        { f: { num_components: 3, max_h: 1, max_v: 1 }, comps: [[1, 1, 0], [1, 1, 6], [1, 1, 12]] },
      ], ALLOC_SARRAY_REAL, function (fx, o, r, i) {
        const prep = alloc(0x40, 0x55555555);
        o.c.add(0x14c).writePointer(prep);
        fx['pr' + i] = prep;
      });
    },

    // ---- jpeg_set_colorspace 0x004ae990 -------------------------------------------------------------------
    // global_state 0x64 on every cinfo except 16 and 17, which take the ERREXIT 0x12 and then carry on.
    // input_components only matters for colour space 0 (JCS_UNKNOWN).
    c3bj_cs() {
      return build([
        { f: { global_state: 0x64, input_components: 3 }, comps: [] },      // 0 -> UNKNOWN, 3 components
        { f: { global_state: 0x64, input_components: 1 }, comps: [] },      // 1 GRAYSCALE
        { f: { global_state: 0x64, input_components: 3 }, comps: [] },      // 2 RGB
        { f: { global_state: 0x64, input_components: 3 }, comps: [] },      // 3 YCbCr
        { f: { global_state: 0x64, input_components: 4 }, comps: [] },      // 4 CMYK
        { f: { global_state: 0x64, input_components: 4 }, comps: [] },      // 5 YCCK
        { f: { global_state: 0x64, input_components: 3 }, comps: [] },      // 6 -> ERREXIT 8
        { f: { global_state: 0x64, input_components: 3 }, comps: [] },      // -1 -> ERREXIT 8
        { f: { global_state: 0x64, input_components: 3 }, comps: [] },      // 100 -> ERREXIT 8
        { f: { global_state: 0x64, input_components: 0 }, comps: [] },      // 0 with 0 components
        { f: { global_state: 0x64, input_components: 11 }, comps: [] },     // 0 with 11 components
        { f: { global_state: 0x64, input_components: 10 }, comps: [] },     // 0 with the upper bound
        { f: { global_state: 0x64, input_components: 1 }, comps: [] },      // 0 with the lower bound
        { f: { global_state: 0x64, input_components: -3 }, comps: [] },     // 0 with a negative count
        { f: { global_state: 0x64, input_components: 3 }, comps: [] },      // 0x7FFFFFFF -> ERREXIT 8
        { f: { global_state: 0x64, input_components: 3 }, comps: [] },      // -0x80000000 -> ERREXIT 8
        { f: { global_state: 0x63, input_components: 3 }, comps: [] },      // bad state, then YCbCr
        { f: { global_state: 0, input_components: 1 }, comps: [] },         // bad state, then GRAYSCALE
      ], null);
    },

    // ---- jinit_downsampler 0x004b4040 ---------------------------------------------------------------------
    // CCIR601_sampling, smoothing_factor and the (h_samp_factor, v_samp_factor) of each component against the
    // maxima select which of the five method branches runs. Every component keeps h_samp_factor and
    // v_samp_factor non-zero: the int_downsample test at 0x004b414b/0x004b4158 divides by them.
    c3bj_down() {
      return build([
        { f: { CCIR601_sampling: 0, smoothing_factor: 0, num_components: 1, max_h: 1, max_v: 1 },
          comps: [[1, 1]] },
        { f: { CCIR601_sampling: 0, smoothing_factor: 1, num_components: 1, max_h: 1, max_v: 1 },
          comps: [[1, 1]] },
        { f: { CCIR601_sampling: 0, smoothing_factor: 0, num_components: 2, max_h: 2, max_v: 1 },
          comps: [[2, 1], [1, 1]] },
        { f: { CCIR601_sampling: 0, smoothing_factor: 0, num_components: 3, max_h: 2, max_v: 2 },
          comps: [[2, 2], [1, 1], [1, 2]] },
        { f: { CCIR601_sampling: 0, smoothing_factor: 5, num_components: 2, max_h: 2, max_v: 2 },
          comps: [[2, 2], [1, 1]] },
        { f: { CCIR601_sampling: 0, smoothing_factor: 0, num_components: 1, max_h: 4, max_v: 4 },
          comps: [[1, 1]] },
        { f: { CCIR601_sampling: 0, smoothing_factor: 0, num_components: 1, max_h: 4, max_v: 4 },
          comps: [[3, 2]] },
        { f: { CCIR601_sampling: 1, smoothing_factor: 0, num_components: 1, max_h: 1, max_v: 1 },
          comps: [[1, 1]] },
        { f: { CCIR601_sampling: 0, smoothing_factor: 2, num_components: 2, max_h: 2, max_v: 1 },
          comps: [[2, 1], [1, 1]] },
        { f: { CCIR601_sampling: 0, smoothing_factor: 0, num_components: 0, max_h: 1, max_v: 1 },
          comps: [] },
        { f: { CCIR601_sampling: 0, smoothing_factor: 3, num_components: 0, max_h: 1, max_v: 1 },
          comps: [] },
        { f: { CCIR601_sampling: 0, smoothing_factor: 0, num_components: -1, max_h: 1, max_v: 1 },
          comps: [] },
        { f: { CCIR601_sampling: 0, smoothing_factor: 0, num_components: 4, max_h: 4, max_v: 2 },
          comps: [[4, 2], [2, 2], [2, 1], [1, 1]] },
        { f: { CCIR601_sampling: 1, smoothing_factor: 7, num_components: 1, max_h: 2, max_v: 2 },
          comps: [[1, 1]] },
        // h equal to max_h with v different: the second half of the first test (0x004b40c7) fails.
        { f: { CCIR601_sampling: 0, smoothing_factor: 0, num_components: 1, max_h: 1, max_v: 2 },
          comps: [[1, 1]] },
        // h doubled equal to max_h with v neither equal nor half: 0x004b40ff and 0x004b4127 both fail.
        { f: { CCIR601_sampling: 0, smoothing_factor: 0, num_components: 1, max_h: 2, max_v: 4 },
          comps: [[1, 1]] },
        // max_h divisible by h but max_v not divisible by v: the second half of the int test (0x004b415c).
        { f: { CCIR601_sampling: 0, smoothing_factor: 0, num_components: 1, max_h: 4, max_v: 3 },
          comps: [[1, 2]] },
      ], null);
    },

    // ---- jinit_color_converter 0x004b4a60 -----------------------------------------------------------------
    // (input_components, in_color_space, num_components, jpeg_color_space) per row. The first switch checks the
    // input pair, the second picks the converter from the JPEG space and the input space.
    c3bj_cconv() {
      const rows = [[1, 1, 1, 1], [3, 2, 1, 1], [3, 3, 1, 1], [4, 4, 1, 1],
                    [3, 2, 3, 2], [3, 3, 3, 2],
                    [3, 2, 3, 3], [3, 3, 3, 3], [1, 1, 3, 3],
                    [4, 4, 4, 4], [4, 5, 4, 4],
                    [4, 4, 4, 5], [4, 5, 4, 5], [3, 3, 4, 5],
                    [2, 6, 2, 6], [2, 6, 3, 6],
                    [0, 0, 1, 1], [2, 1, 1, 1], [3, 2, 2, 2], [4, 4, 3, 4], [4, 5, 2, 7],
                    // num_components wrong for JPEG spaces 1, 3 and 5, and input_components wrong for the
                    // input-space groups {2,3} and {4,5}: the not-taken sides of those five compares.
                    [1, 1, 2, 1], [3, 3, 2, 3], [4, 5, 3, 5], [2, 2, 3, 2], [2, 4, 4, 4]];
      return build(rows.map(function (r) {
        return { f: { input_components: r[0], in_color_space: r[1], num_components: r[2],
                      jpeg_color_space: r[3] }, comps: [] };
      }), null);
    },

    // ---- initial_setup 0x004b52a0 -------------------------------------------------------------------------
    // The four guards, the sampling-factor check and the two loops. Row 9 has 11 components (the comp_info
    // array holds 16 entries), rows 10 and 11 a sampling factor outside 1..4.
    c3bj_setup() {
      return build([
        { f: { image_width: 16, image_height: 16, num_components: 1, input_components: 1, data_precision: 8 },
          comps: [[1, 1]] },
        { f: { image_width: 17, image_height: 33, num_components: 3, input_components: 3, data_precision: 8 },
          comps: [[2, 2], [1, 1], [1, 1]] },
        { f: { image_width: 0, image_height: 16, num_components: 1, input_components: 1, data_precision: 8 },
          comps: [[1, 1]] },
        { f: { image_width: 16, image_height: 0, num_components: 1, input_components: 1, data_precision: 8 },
          comps: [[1, 1]] },
        { f: { image_width: 16, image_height: 16, num_components: 0, input_components: 1, data_precision: 8 },
          comps: [] },
        { f: { image_width: 16, image_height: 16, num_components: 1, input_components: 0, data_precision: 8 },
          comps: [[1, 1]] },
        { f: { image_width: 0xffdd, image_height: 16, num_components: 1, input_components: 1, data_precision: 8 },
          comps: [[1, 1]] },
        { f: { image_width: 16, image_height: 0xfffe, num_components: 1, input_components: 1, data_precision: 8 },
          comps: [[1, 1]] },
        { f: { image_width: 16, image_height: 16, num_components: 1, input_components: 1, data_precision: 12 },
          comps: [[1, 1]] },
        { f: { image_width: 16, image_height: 16, num_components: 11, input_components: 11, data_precision: 8 },
          comps: [[1, 1], [1, 1], [1, 1], [1, 1], [1, 1], [1, 1], [1, 1], [1, 1], [1, 1], [1, 1], [1, 1]] },
        { f: { image_width: 100, image_height: 50, num_components: 2, input_components: 2, data_precision: 8 },
          comps: [[5, 1], [1, 1]] },
        { f: { image_width: 100, image_height: 50, num_components: 2, input_components: 2, data_precision: 8 },
          comps: [[1, 0], [1, 1]] },
        { f: { image_width: 640, image_height: 480, num_components: 3, input_components: 3, data_precision: 8 },
          comps: [[2, 2], [1, 1], [1, 1]] },
        { f: { image_width: 1, image_height: 1, num_components: 4, input_components: 4, data_precision: 8 },
          comps: [[4, 4], [1, 2], [2, 1], [3, 3]] },
        { f: { image_width: 0xffdc, image_height: 0xffdc, num_components: 1, input_components: 1,
               data_precision: 8 }, comps: [[1, 1]] },
        // h_samp_factor 0 (the taken side of 0x004b536a) and v_samp_factor 5 (the not-taken side of
        // 0x004b537a), the two ends of the 1..4 range check the other rows never reach.
        { f: { image_width: 100, image_height: 50, num_components: 2, input_components: 2, data_precision: 8 },
          comps: [[0, 1], [1, 1]] },
        { f: { image_width: 100, image_height: 50, num_components: 2, input_components: 2, data_precision: 8 },
          comps: [[1, 5], [1, 1]] },
      ], null);
    },

    // ---- per_scan_setup 0x004b5ad0 ------------------------------------------------------------------------
    // cur_comp_info[0..3] (cinfo+0xf0) point into this cinfo's comp_info array. comps_in_scan stays within
    // -2..4 so the loop never indexes past cur_comp_info[3]. Every component keeps h_samp_factor and
    // v_samp_factor non-zero: both are divisors (unsigned div at 0x004b5b07, 0x004b5be2 and 0x004b5bfb).
    c3bj_scan() {
      return build([
        { f: { comps_in_scan: 1, max_h: 1, max_v: 1, image_width: 64, image_height: 64, restart_in_rows: 0 },
          comps: [[1, 1, 8, 8]] },
        { f: { comps_in_scan: 1, max_h: 1, max_v: 1, image_width: 64, image_height: 64, restart_in_rows: 0 },
          comps: [[1, 3, 8, 8]] },
        { f: { comps_in_scan: 1, max_h: 2, max_v: 2, image_width: 64, image_height: 64, restart_in_rows: 2 },
          comps: [[2, 2, 5, 7]] },
        { f: { comps_in_scan: 1, max_h: 1, max_v: 1, image_width: 64, image_height: 64,
               restart_in_rows: 10000 }, comps: [[1, 1, 9, 9]] },
        { f: { comps_in_scan: 2, max_h: 2, max_v: 2, image_width: 64, image_height: 48, restart_in_rows: 0 },
          comps: [[2, 2, 8, 6], [1, 1, 4, 3]] },
        { f: { comps_in_scan: 3, max_h: 2, max_v: 2, image_width: 33, image_height: 17, restart_in_rows: 1 },
          comps: [[2, 2, 3, 2], [1, 1, 2, 1], [1, 1, 2, 1]] },
        { f: { comps_in_scan: 4, max_h: 4, max_v: 1, image_width: 100, image_height: 20, restart_in_rows: 0 },
          comps: [[4, 1, 13, 3], [2, 1, 7, 3], [1, 1, 4, 3], [1, 1, 4, 3]] },
        { f: { comps_in_scan: 0, max_h: 1, max_v: 1, image_width: 16, image_height: 16, restart_in_rows: 0 },
          comps: [[1, 1, 2, 2]] },
        { f: { comps_in_scan: -2, max_h: 1, max_v: 1, image_width: 16, image_height: 16, restart_in_rows: 0 },
          comps: [[1, 1, 2, 2]] },
        { f: { comps_in_scan: 3, max_h: 2, max_v: 2, image_width: 64, image_height: 64, restart_in_rows: 0 },
          comps: [[2, 2, 8, 8], [2, 2, 8, 8], [2, 2, 8, 8]] },
        { f: { comps_in_scan: 2, max_h: 2, max_v: 1, image_width: 31, image_height: 15, restart_in_rows: 3 },
          comps: [[2, 1, 4, 2], [1, 1, 2, 2]] },
        { f: { comps_in_scan: 1, max_h: 1, max_v: 1, image_width: 7, image_height: 7, restart_in_rows: -1 },
          comps: [[1, 1, 1, 1]] },
        { f: { comps_in_scan: 1, max_h: 1, max_v: 1, image_width: 7, image_height: 7, restart_in_rows: 1 },
          comps: [[1, 1, 3, 3]] },
        { f: { comps_in_scan: 4, max_h: 2, max_v: 2, image_width: 70, image_height: 70, restart_in_rows: 0 },
          comps: [[2, 2, 9, 9], [1, 1, 5, 5], [1, 1, 5, 5], [1, 1, 5, 5]] },
        { f: { comps_in_scan: 1, max_h: 1, max_v: 1, image_width: 16, image_height: 16, restart_in_rows: 0 },
          comps: [[1, 1, 0, 0]] },
        // comps_in_scan 5: the not-taken side of the upper bound at 0x004b5b43. Its error_exit clears the
        // count, so the loop that would read cur_comp_info[4] does not run (see ERR_EXIT_CLAMP above).
        { f: { comps_in_scan: 5, max_h: 1, max_v: 1, image_width: 16, image_height: 16, restart_in_rows: 0 },
          comps: [[1, 1, 2, 2], [1, 1, 2, 2], [1, 1, 2, 2], [1, 1, 2, 2]] },
        // h_samp_factor * v_samp_factor overflowing to 0: the taken side of 0x004b5c29, where the component
        // contributes no MCU_membership entry at all.
        { f: { comps_in_scan: 2, max_h: 2, max_v: 1, image_width: 32, image_height: 16, restart_in_rows: 0 },
          comps: [[0x10000, 0x10000, 4, 4], [1, 1, 2, 2]] },
      ], null, function (fx, o, r, i) {
        for (let j = 0; j < 4; j++) o.c.add(0xf0 + j * 4).writePointer(o.comps.add(j * 0x54));
        if (i === 15) o.err.writePointer(ERR_EXIT_CLAMP);
      });
    },

    // ---- BinkPlayer::setTarget 0x00487050 -----------------------------------------------------------------
    // Twelve 0x20-byte objects pre-filled 0xAA, so the single store at this+0xc is an observable change and the
    // rest of the object is compared as untouched. The last three vectors pass plain numbers (0, 1, -1).
    c3bj_bink() {
      const fx = {};
      for (let i = 0; i < 12; i++) {
        const o = alloc(0x20, 0xaaaaaaaa);
        o.writeU32(0x3000 + i);                 // marker the function never overwrites
        fx['o' + i] = o;
        const t = alloc(0x10);
        t.writeU32(0x4000 + i);
        fx['t' + i] = t;
        if (i === 0) fx.obj = o;
      }
      return fx;
    },
  });
})();
