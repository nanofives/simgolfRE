// Fixtures for C3 batch c3bo (round 16, golf_clean.exe): four IJG libjpeg 6a compress-side routines
// (jinit_c_master_control, validate_script, prepare_for_pass, emit_eobrun). Only our own synthetic values
// (no game text, no live game object). Names are prefixed c3bo_.
//
// Allocations go through Memory.alloc (diff_hook rewrites it to __keepAlloc so blocks referenced only from a
// field survive the run); NativeCallbacks are pushed onto __diffKeepAlive by hand.
//
// Every callback is a pure function of its arguments or of memory diff_hook snapshots and restores between
// the two arms, so the second arm sees exactly the world the first one saw.
//
// libjpeg structures: cinfo+0 = err (jpeg_error_mgr: error_exit +0, emit_message +4, msg_code +0x14,
// msg_parm.i[] +0x18), cinfo+4 = mem (jpeg_memory_mgr: alloc_small +0; this cinfo's small-allocation pool is
// kept at mem+0x10, past the slots the code reads), cinfo+8 = progress (completed_passes +0xc,
// total_passes +0x10), cinfo+0x14 = dest (jpeg_destination_mgr: next_output_byte +0, free_in_buffer +4,
// empty_output_buffer +0xc; our own output buffer is stashed at dest+0x18). A jpeg_component_info is 0x54
// bytes, a jpeg_scan_info 0x24 (comps_in_scan +0, component_index[4] +4, Ss +0x14, Se +0x18, Ah +0x1c,
// Al +0x20), my_comp_master 0x24 (prepare_for_pass +0, pass_startup +4, finish_pass +8,
// call_pass_startup +0xc, is_last_pass +0x10, pass_type +0x14, pass_number +0x18, total_passes +0x1c,
// scan_number +0x20) and a phuff_entropy_encoder 0x6c (see shim/src/re/c3bo.cpp).
//
// We keep the pointer to a vector's call recorder at cinfo+0x170, past the end of jpeg_compress_struct
// (0x168): the nine prepare_for_pass method callbacks get only cinfo, so that is how each one finds it.
(function () {
  function keep(cb) { globalThis.__diffKeepAlive.push(cb); return cb; }
  function zero(p, n) { for (let i = 0; i < n; i += 4) p.add(i).writeU32(0); }
  function fill(p, n, v) { for (let i = 0; i < n; i += 4) p.add(i).writeU32(v); }
  function alloc(n, v) { const p = Memory.alloc(n); if (v === undefined) zero(p, n); else fill(p, n, v); return p; }

  const POOL = 0x1100;        // per-cinfo alloc_small pool: 0x40 slots of 0x40 bytes plus slack

  // alloc_small(cinfo, pool_id, size): a slot of this cinfo's pool chosen by the SIZE alone. Both arms get
  // the same pointer (a fresh Memory.alloc per call would differ and read RED). __cdecl.
  const ALLOC_SMALL = keep(new NativeCallback(function (cinfo, pool, size) {
    return cinfo.add(4).readPointer().add(0x10).readPointer().add((size & 0x3f) * 0x40);
  }, 'pointer', ['pointer', 'int', 'int']));

  // error_exit(cinfo): libjpeg's never returns; ours records the message code AND both integer parameters in
  // the error manager's msg_parm tail (bytes +0x30..+0x3f, which none of these functions writes) and returns,
  // so the instructions after every ERREXIT run - identically in both arms. The rolling hash makes the
  // sequence of messages and their scan numbers part of the compared state, and the counter their number.
  const ERR_REC = keep(new NativeCallback(function (cinfo) {
    const e = cinfo.readPointer();
    let h = e.add(0x30).readU32();
    h = (Math.imul(h, 33) + e.add(0x14).readU32()) >>> 0;
    h = (Math.imul(h, 33) + e.add(0x18).readU32()) >>> 0;
    h = (Math.imul(h, 33) + e.add(0x1c).readU32()) >>> 0;
    e.add(0x30).writeU32(h);
    e.add(0x34).writeU32((e.add(0x34).readU32() + 1) >>> 0);
    e.add(0x38).writeU32(e.add(0x14).readU32());        // last message code
  }, 'void', ['pointer']));

  // empty_output_buffer(cinfo) = TRUE: reset the destination manager to the buffer stashed at dest+0x18 with
  // 0x40 free. Deterministic (it writes constants into a region that is part of the state).
  const EMPTY_OK = keep(new NativeCallback(function (cinfo) {
    const d = cinfo.add(0x14).readPointer();
    d.writePointer(d.add(0x18).readPointer());
    d.add(4).writeU32(0x40);
    return 1;
  }, 'int', ['pointer']));

  // A 0x84-byte jpeg_error_mgr pre-filled 0x22 (so a write anywhere in it shows) with our handler and zeroed
  // recorder slots.
  function mkErr() {
    const e = alloc(0x84, 0x22222222);
    e.writePointer(ERR_REC);
    zero(e.add(0x30), 0x10);
    return e;
  }

  // A jpeg_memory_mgr whose alloc_small hands out slots of `pool`.
  function mkMem() {
    const mem = alloc(0x20);
    const pool = alloc(POOL, 0xcccccccc);
    mem.writePointer(ALLOC_SMALL);
    mem.add(0x10).writePointer(pool);
    return { mem: mem, pool: pool };
  }

  // 16 jpeg_component_info entries pre-filled 0x99, with (h_samp_factor +8, v_samp_factor +0xc,
  // width_in_blocks +0x1c, height_in_blocks +0x20) taken from `rows`.
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

  // Eight jpeg_scan_info entries pre-filled 0x7b, with `rows` = [comps_in_scan, [index...], Ss, Se, Ah, Al].
  function mkScans(rows) {
    const s = alloc(8 * 0x24, 0x7b7b7b7b);
    (rows || []).forEach(function (r, i) {
      const sc = s.add(i * 0x24);
      sc.writeS32(r[0]);
      for (let j = 0; j < 4; j++) sc.add(4 + j * 4).writeS32(j < r[1].length ? r[1][j] : 0);
      sc.add(0x14).writeS32(r[2]);
      sc.add(0x18).writeS32(r[3]);
      sc.add(0x1c).writeS32(r[4]);
      sc.add(0x20).writeS32(r[5]);
    });
    return s;
  }

  const FIELD = { progress: 0x08, image_width: 0x18, image_height: 0x1c, input_components: 0x20,
                  data_precision: 0x30, num_components: 0x34, num_scans: 0xa0, raw_data_in: 0xa8,
                  arith_code: 0xac, optimize_coding: 0xb0, progressive_mode: 0xdc, max_h: 0xe0, max_v: 0xe4,
                  comps_in_scan: 0xec, Ss: 0x134, Se: 0x138, Ah: 0x13c, Al: 0x140 };

  // ---- prepare_for_pass: nine recording method callbacks ---------------------------------------------------
  // Each module object's method slot holds one of these. They append (id, argument) to the recorder the
  // vector's cinfo points at (cinfo+0x170), so the ORDER of the calls and the value of every mode argument
  // are part of the compared state. rec+0 is the cursor, then pairs of dwords.
  function recorder(id, hasArg) {
    const types = hasArg ? ['pointer', 'int'] : ['pointer'];
    return keep(new NativeCallback(function (cinfo, arg) {
      const rec = cinfo.add(0x170).readPointer();
      if (rec.isNull()) return;
      let n = rec.readU32();
      if (n < 0x1e) {
        rec.add(4 + n * 8).writeS32(id);
        rec.add(8 + n * 8).writeS32(hasArg ? arg : -0x55);
        rec.writeU32(n + 1);
      }
    }, 'void', types));
  }
  const M_CCONVERT = recorder(1, false);     // cinfo+0x158 slot +0
  const M_DOWNSAMPLE = recorder(2, false);   // cinfo+0x15c slot +0
  const M_PREP = recorder(3, true);          // cinfo+0x14c slot +0
  const M_FDCT = recorder(4, false);         // cinfo+0x160 slot +0
  const M_ENTROPY = recorder(5, true);       // cinfo+0x164 slot +0
  const M_COEF = recorder(6, true);          // cinfo+0x150 slot +0
  const M_MAIN = recorder(7, true);          // cinfo+0x148 slot +0
  const M_FRAME_HDR = recorder(8, false);    // cinfo+0x154 slot +8
  const M_SCAN_HDR = recorder(9, false);     // cinfo+0x154 slot +0xc

  // The module objects are constant and shared by every vector (nothing writes them).
  function mkModule(slots) {
    const m = alloc(0x20);
    Object.keys(slots).forEach(function (off) { m.add(parseInt(off, 10)).writePointer(slots[off]); });
    return m;
  }
  const MOD = {
    cconvert: mkModule({ 0: M_CCONVERT }),
    downsample: mkModule({ 0: M_DOWNSAMPLE }),
    prep: mkModule({ 0: M_PREP }),
    fdct: mkModule({ 0: M_FDCT }),
    entropy: mkModule({ 0: M_ENTROPY }),
    coef: mkModule({ 0: M_COEF }),
    main: mkModule({ 0: M_MAIN }),
    marker: mkModule({ 8: M_FRAME_HDR, 12: M_SCAN_HDR }),
  };

  Object.assign(globalThis.DIFF_FIXTURES, {
    // ---- validate_script 0x004b5470, sequential scripts ----------------------------------------------------
    // The first scan has Ss == 0 and Se == 0x3f, so progressive_mode is cleared and component_sent is the
    // array that matters. progressive_mode is pre-set to 0x5a5a5a5a on every cinfo, so the clear is a visible
    // change. Every component_index stays inside 0..num_components-1 wherever the function later uses it as
    // an index (component_sent[thisi] at 0x004b573e is read AFTER the range ERREXIT returns), and no vector
    // has num_components above 10: last_bitpos is 10 rows of 64 ints and the `rep stosd` at 0x004b54ec would
    // run off the 0xa48-byte frame.
    _c3boVs(rows) {
      const fx = {};
      rows.forEach(function (r, i) {
        const err = mkErr();
        const scans = mkScans(r.scans);
        const c = alloc(0x180);
        c.writePointer(err);
        c.add(0x34).writeS32(r.nc);
        c.add(0xa0).writeS32(r.ns);
        c.add(0xa4).writePointer(scans);
        c.add(0xdc).writeU32(0x5a5a5a5a);
        fx['c' + i] = c;
        fx['e' + i] = err;
        fx['s' + i] = scans;
        if (i === 0) fx.obj = c;
      });
      return fx;
    },
    c3bo_vs_seq() {
      return this._c3boVs([
        { nc: 3, ns: 1, scans: [[3, [0, 1, 2], 0, 63, 0, 0]] },
        { nc: 3, ns: 2, scans: [[1, [0], 0, 63, 0, 0], [2, [1, 2], 0, 63, 0, 0]] },
        { nc: 3, ns: 1, scans: [[2, [0, 1], 0, 63, 0, 0]] },
        { nc: 2, ns: 1, scans: [[2, [0, 0], 0, 63, 0, 0]] },
        { nc: 2, ns: 1, scans: [[3, [0, 1, 1], 0, 63, 0, 0]] },
        // comps_in_scan 5: the fifth index read at 0x004b5539 is the scan's Ss field, which is 0 here.
        { nc: 4, ns: 1, scans: [[5, [0, 1, 2, 3], 0, 63, 0, 0]] },
        { nc: 1, ns: 0, scans: [[1, [0], 0, 63, 0, 0]] },
        { nc: 0, ns: 1, scans: [[0, [], 0, 63, 0, 0]] },
        { nc: 3, ns: 1, scans: [[1, [0], 0, 63, 1, 0]] },
        { nc: 3, ns: 1, scans: [[1, [0], 0, 63, 0, 2]] },
        { nc: 3, ns: 2, scans: [[3, [0, 1, 2], 0, 63, 0, 0], [1, [2], 0, 63, 0, 0]] },
        { nc: 10, ns: 1, scans: [[4, [0, 1, 2, 3], 0, 63, 0, 0]] },
        { nc: -1, ns: 1, scans: [[0, [], 0, 63, 0, 0]] },
        { nc: 2, ns: 3, scans: [[1, [0], 0, 63, 0, 0], [1, [1], 0, 63, 0, 0], [1, [0], 0, 63, 0, 0]] },
        { nc: 4, ns: 1, scans: [[4, [3, 2, 1, 0], 0, 63, 0, 0]] },
        { nc: 3, ns: 1, scans: [[2, [0, 2], 0, 63, 0, 0]] },
        { nc: 3, ns: 2, scans: [[1, [0], 0, 63, 0, 0], [2, [1, 2], 0, 62, 0, 0]] },
        { nc: 3, ns: 2, scans: [[1, [0], 0, 63, 0, 0], [1, [1], 7, 63, 0, 0]] },
      ]);
    },
    // ---- validate_script 0x004b5470, progressive scripts -------------------------------------------------
    // The first scan has Ss != 0 or Se != 0x3f, so progressive_mode is set and last_bitpos is the array that
    // matters. Every vector that reaches the coefficient loop keeps Ss and Se inside the rows the
    // `rep stosd` initialised: the out-of-range-index and out-of-range-Ss vectors use Se < Ss (so the loop at
    // 0x004b56a1 is skipped) or num_components 10 (so all ten rows are initialised).
    c3bo_vs_prog() {
      return this._c3boVs([
        { nc: 1, ns: 2, scans: [[1, [0], 0, 0, 0, 0], [1, [0], 1, 5, 0, 0]] },
        { nc: 1, ns: 1, scans: [[1, [0], 1, 5, 0, 0]] },
        { nc: 2, ns: 3, scans: [[2, [0, 1], 0, 0, 0, 1], [2, [0, 1], 0, 0, 1, 0], [1, [0], 1, 3, 0, 0]] },
        { nc: 1, ns: 2, scans: [[1, [0], 0, 0, 0, 2], [1, [0], 0, 0, 3, 1]] },
        { nc: 1, ns: 2, scans: [[1, [0], 0, 0, 0, 2], [1, [0], 0, 0, 2, 0]] },
        { nc: 1, ns: 1, scans: [[1, [0], 0, 0, 1, 0]] },
        { nc: 2, ns: 1, scans: [[2, [0, 1], 0, 1, 0, 0]] },
        { nc: 2, ns: 1, scans: [[2, [0, 1], 1, 3, 0, 0]] },
        { nc: 1, ns: 1, scans: [[1, [0], -1, -2, 0, 0]] },
        { nc: 10, ns: 1, scans: [[1, [0], 64, 64, 0, 0]] },
        { nc: 10, ns: 1, scans: [[1, [0], 2, 1, 0, 0]] },
        { nc: 10, ns: 1, scans: [[1, [0], 1, 64, 0, 0]] },
        { nc: 10, ns: 1, scans: [[1, [0], 0, 0, -1, 0]] },
        { nc: 10, ns: 1, scans: [[1, [0], 0, 0, 14, 0]] },
        { nc: 10, ns: 1, scans: [[1, [0], 0, 0, 0, -1]] },
        { nc: 10, ns: 1, scans: [[1, [0], 0, 0, 0, 14]] },
        { nc: 3, ns: 1, scans: [[2, [0, 5], 0, -1, 0, 0]] },
        { nc: 3, ns: 1, scans: [[2, [-1, 2], 0, -1, 0, 0]] },
        { nc: 2, ns: -1, scans: [[1, [0], 1, 5, 0, 0]] },
        { nc: 0, ns: 1, scans: [[0, [], 1, 5, 0, 0]] },
      ]);
    },

    // ---- jinit_c_master_control 0x004b51e0 ---------------------------------------------------------------
    // The master object comes out of the alloc_small pool (size 0x24 -> slot 0x24*0x40 = 0x900, which no other
    // allocation of these vectors shares). Rows with `scans` give cinfo->scan_info, so validate_script
    // 0x004b5470 runs and decides progressive_mode; rows without it take the scan_info == NULL arm. The
    // initial_setup 0x004b52a0 call fills comp_info, so the component array is part of the compared state.
    c3bo_master() {
      const rows = [
        // scan_info == NULL: the four (transcode_only, optimize_coding) combinations.
        { f: { image_width: 16, image_height: 16, num_components: 1, input_components: 1, data_precision: 8,
               optimize_coding: 0 }, comps: [[1, 1]] },
        { f: { image_width: 16, image_height: 16, num_components: 1, input_components: 1, data_precision: 8,
               optimize_coding: 0 }, comps: [[1, 1]] },
        { f: { image_width: 32, image_height: 48, num_components: 3, input_components: 3, data_precision: 8,
               optimize_coding: 1 }, comps: [[2, 2], [1, 1], [1, 1]] },
        { f: { image_width: 32, image_height: 48, num_components: 3, input_components: 3, data_precision: 8,
               optimize_coding: 1 }, comps: [[2, 2], [1, 1], [1, 1]] },
        // scan_info != NULL, a valid sequential script: validate_script clears progressive_mode.
        { f: { image_width: 64, image_height: 64, num_components: 3, input_components: 3, data_precision: 8,
               optimize_coding: 0, num_scans: 1 }, comps: [[1, 1], [1, 1], [1, 1]],
          scans: [[3, [0, 1, 2], 0, 63, 0, 0]] },
        // scan_info != NULL, a progressive script: validate_script sets progressive_mode, which forces
        // optimize_coding to 1 at 0x004b5248.
        { f: { image_width: 64, image_height: 64, num_components: 1, input_components: 1, data_precision: 8,
               optimize_coding: 0, num_scans: 2 }, comps: [[1, 1]],
          scans: [[1, [0], 0, 0, 0, 0], [1, [0], 1, 5, 0, 0]] },
        { f: { image_width: 64, image_height: 64, num_components: 1, input_components: 1, data_precision: 8,
               optimize_coding: 0, num_scans: 3 }, comps: [[1, 1]],
          scans: [[1, [0], 0, 0, 0, 1], [1, [0], 0, 0, 1, 0], [1, [0], 1, 3, 0, 0]] },
        { f: { image_width: 80, image_height: 24, num_components: 2, input_components: 2, data_precision: 8,
               optimize_coding: 1, num_scans: 2 }, comps: [[1, 1], [1, 1]],
          scans: [[1, [0], 0, 63, 0, 0], [1, [1], 0, 63, 0, 0]] },
        // A script that fails validation (component 2 never sent): the error recorder differs.
        { f: { image_width: 64, image_height: 64, num_components: 3, input_components: 3, data_precision: 8,
               optimize_coding: 0, num_scans: 1 }, comps: [[1, 1], [1, 1], [1, 1]],
          scans: [[2, [0, 1], 0, 63, 0, 0]] },
        // initial_setup guards: a zero width, 11 components and a 12-bit precision.
        { f: { image_width: 0, image_height: 16, num_components: 1, input_components: 1, data_precision: 8,
               optimize_coding: 0 }, comps: [[1, 1]] },
        { f: { image_width: 16, image_height: 16, num_components: 11, input_components: 11,
               data_precision: 8, optimize_coding: 0 },
          comps: [[1, 1], [1, 1], [1, 1], [1, 1], [1, 1], [1, 1], [1, 1], [1, 1], [1, 1], [1, 1], [1, 1]] },
        { f: { image_width: 16, image_height: 16, num_components: 1, input_components: 1, data_precision: 12,
               optimize_coding: 0 }, comps: [[1, 1]] },
        { f: { image_width: 100, image_height: 50, num_components: 2, input_components: 2, data_precision: 8,
               optimize_coding: 1, num_scans: 2 }, comps: [[2, 1], [1, 1]],
          scans: [[2, [0, 1], 0, 0, 0, 0], [1, [1], 1, 7, 0, 0]] },
      ];
      const fx = {};
      rows.forEach(function (r, i) {
        const m = mkMem();
        const err = mkErr();
        const comps = mkComps(r.comps);
        const c = alloc(0x180);
        c.writePointer(err);
        c.add(4).writePointer(m.mem);
        c.add(0x3c).writePointer(comps);
        c.add(0xdc).writeU32(0x5a5a5a5a);
        Object.keys(r.f).forEach(function (n) { c.add(FIELD[n]).writeS32(r.f[n]); });
        if (r.scans) c.add(0xa4).writePointer(mkScans(r.scans));
        fx['c' + i] = c;
        fx['e' + i] = err;
        fx['p' + i] = m.pool;
        fx['k' + i] = comps;
        if (i === 0) fx.obj = c;
      });
      return fx;
    },

    // ---- prepare_for_pass 0x004b5810 ---------------------------------------------------------------------
    // Each vector has its own master object (pre-filled 0x44 so every write shows), its own call recorder and
    // its own comp_info / scan_info; the eight module objects are shared and constant. select_scan_parameters
    // 0x004b59d0 copies the current scan's Ss / Se / Ah / Al into cinfo, so the pass_type 1 arm's three tests
    // read what the scan says; comps_in_scan therefore stays inside 1..4 (per_scan_setup 0x004b5ad0 would
    // otherwise walk cur_comp_info past its fourth entry after its ERREXIT returns) and every sampling factor
    // stays non-zero (per_scan_setup divides by them).
    c3bo_pass() {
      const rows = [
        // pass_type 0 (main_pass): raw_data_in, optimize_coding and total_passes select the sub-branches.
        { m: { pass_type: 0, pass_number: 0, total_passes: 1, scan_number: 0 },
          f: { raw_data_in: 0, optimize_coding: 0 }, prog: false },
        { m: { pass_type: 0, pass_number: 2, total_passes: 3, scan_number: 0 },
          f: { raw_data_in: 1, optimize_coding: 1 }, prog: true },
        { m: { pass_type: 0, pass_number: 0, total_passes: 2, scan_number: 0 },
          f: { raw_data_in: 0, optimize_coding: 1 }, prog: true },
        // pass_type 1 (huff_opt_pass): Ss != 0, then Ah == 0, then arith_code != 0 each take 0x004b58e2.
        { m: { pass_type: 1, pass_number: 1, total_passes: 2, scan_number: 0 },
          f: { arith_code: 0, optimize_coding: 1 }, scans: [[1, [0], 1, 5, 0, 0]], prog: false },
        { m: { pass_type: 1, pass_number: 0, total_passes: 4, scan_number: 0 },
          f: { arith_code: 0, optimize_coding: 1 }, scans: [[2, [0, 1], 0, 0, 0, 0]], prog: false },
        { m: { pass_type: 1, pass_number: 1, total_passes: 4, scan_number: 0 },
          f: { arith_code: 1, optimize_coding: 1 }, scans: [[1, [0], 0, 0, 2, 1]], prog: true },
        // The DC-refinement special case: pass_type becomes 2 and pass_number grows, then the output arm runs.
        { m: { pass_type: 1, pass_number: 1, total_passes: 4, scan_number: 0 },
          f: { arith_code: 0, optimize_coding: 0 }, scans: [[1, [0], 0, 0, 2, 1]], prog: false },
        { m: { pass_type: 1, pass_number: 2, total_passes: 8, scan_number: 2 },
          f: { arith_code: 0, optimize_coding: 1 }, nscans: 4,
          scans: [[1, [0], 0, 0, 1, 0], [1, [0], 0, 0, 2, 1], [1, [0], 0, 0, 3, 2], [1, [0], 1, 5, 0, 0]],
          prog: true },
        // pass_type 2 (output_pass).
        { m: { pass_type: 2, pass_number: 0, total_passes: 1, scan_number: 0 },
          f: { optimize_coding: 0 }, prog: false },
        { m: { pass_type: 2, pass_number: 1, total_passes: 5, scan_number: 3 },
          f: { optimize_coding: 1 }, nscans: 4,
          scans: [[1, [0], 0, 0, 0, 1], [1, [0], 0, 0, 1, 0], [1, [0], 1, 3, 0, 0], [1, [0], 4, 9, 0, 0]],
          prog: true },
        // The default arm: ERREXIT 0x2f, then the common tail.
        { m: { pass_type: 3, pass_number: 0, total_passes: 2, scan_number: 0 },
          f: { optimize_coding: 0 }, prog: true },
        { m: { pass_type: -1, pass_number: 1, total_passes: 1, scan_number: 1 },
          f: { optimize_coding: 1 }, prog: false },
        // is_last_pass on both sides, with and without a progress monitor. scan_number 1 needs a second scan:
        // select_scan_parameters 0x004b59d0 reads scan_info[scan_number] and walks comps_in_scan entries of
        // cur_comp_info, so an unwritten scan entry (pre-filled 0x7b7b7b7b) would make it run off the struct.
        { m: { pass_type: 2, pass_number: 4, total_passes: 5, scan_number: 1 },
          f: { optimize_coding: 0 }, nscans: 2,
          scans: [[1, [0], 0, 63, 0, 0], [2, [0, 1], 0, 0, 0, 0]], prog: true },
        { m: { pass_type: 0, pass_number: 1, total_passes: 7, scan_number: 0 },
          f: { raw_data_in: 0, optimize_coding: 0 }, prog: false },
        // scan_info == NULL: select_scan_parameters takes its own else arm (all components in one scan).
        { m: { pass_type: 0, pass_number: 0, total_passes: 1, scan_number: 0 },
          f: { raw_data_in: 0, optimize_coding: 0 }, noscans: true, prog: false },
        { m: { pass_type: 2, pass_number: 2, total_passes: 3, scan_number: 0 },
          f: { optimize_coding: 0 }, noscans: true, nc: 4,
          comps: [[1, 1, 8, 8], [1, 1, 8, 8], [1, 1, 8, 8], [1, 1, 8, 8]], prog: true },
      ];
      const fx = {};
      rows.forEach(function (r, i) {
        const err = mkErr();
        const comps = mkComps(r.comps || [[2, 2, 8, 8], [1, 1, 4, 4], [1, 1, 4, 4]]);
        const master = alloc(0x24, 0x44444444);
        const rec = alloc(0x100);
        const c = alloc(0x180);
        c.writePointer(err);
        c.add(0x3c).writePointer(comps);
        c.add(0x144).writePointer(master);
        c.add(0x170).writePointer(rec);
        c.add(0x18).writeS32(64);                    // image_width
        c.add(0x1c).writeS32(48);                    // image_height
        c.add(0x34).writeS32(r.nc === undefined ? 3 : r.nc);
        c.add(0xe0).writeS32(2);                     // max_h_samp_factor
        c.add(0xe4).writeS32(2);                     // max_v_samp_factor
        c.add(0xa0).writeS32(r.nscans === undefined ? 1 : r.nscans);
        if (!r.noscans) c.add(0xa4).writePointer(mkScans(r.scans || [[3, [0, 1, 2], 0, 63, 0, 0]]));
        Object.keys(r.f).forEach(function (n) { c.add(FIELD[n]).writeS32(r.f[n]); });
        Object.keys(r.m).forEach(function (n) {
          const off = { pass_type: 0x14, pass_number: 0x18, total_passes: 0x1c, scan_number: 0x20 }[n];
          c.add(0x144).readPointer().add(off).writeS32(r.m[n]);
        });
        c.add(0x148).writePointer(MOD.main);
        c.add(0x14c).writePointer(MOD.prep);
        c.add(0x150).writePointer(MOD.coef);
        c.add(0x154).writePointer(MOD.marker);
        c.add(0x158).writePointer(MOD.cconvert);
        c.add(0x15c).writePointer(MOD.downsample);
        c.add(0x160).writePointer(MOD.fdct);
        c.add(0x164).writePointer(MOD.entropy);
        if (r.prog) {
          const pg = alloc(0x20, 0x66666666);
          c.add(8).writePointer(pg);
          fx['g' + i] = pg;
        } else {
          const pg = alloc(0x20, 0x66666666);        // allocated but NOT linked: cinfo->progress stays NULL
          fx['g' + i] = pg;
        }
        fx['c' + i] = c;
        fx['e' + i] = err;
        fx['k' + i] = comps;
        fx['m' + i] = master;
        fx['r' + i] = rec;
        if (i === 0) fx.obj = c;
      });
      return fx;
    },

    // ---- emit_eobrun 0x004b25d0 --------------------------------------------------------------------------
    // Each vector gets its own phuff_entropy_encoder (pre-filled 0x55 so every write shows), its own cinfo
    // with an error manager and a destination manager, its own 0x40-byte output buffer, four derived Huffman
    // tables and four statistics arrays. `size` and `code` are written into ehufsi[symbol] / ehufco[symbol]
    // of the table the vector selects, with symbol = nbits << 4 computed here the same way the function
    // computes it. EOBRUN never has its top bit set: 0x004b25e6 shifts arithmetically, so a negative value
    // would never reach zero.
    _c3boEob(rows, defaultFree) {
      const fx = {};
      rows.forEach(function (r, i) {
        const free = r.free === undefined ? defaultFree : r.free;
        const err = mkErr();
        const buf = alloc(0x40, 0xcccccccc);
        const dest = alloc(0x20);
        dest.writePointer(buf);
        dest.add(4).writeU32(free);
        dest.add(0xc).writePointer(EMPTY_OK);
        dest.add(0x18).writePointer(buf);
        const c = alloc(0x180);
        c.writePointer(err);
        c.add(0x14).writePointer(dest);
        const ent = alloc(0x6c, 0x55555555);
        ent.add(0xc).writeS32(r.gather ? 1 : 0);
        ent.add(0x10).writePointer(buf);
        ent.add(0x14).writeU32(free);
        ent.add(0x18).writeU32(r.buf === undefined ? 0 : r.buf);
        ent.add(0x1c).writeS32(r.bits === undefined ? 0 : r.bits);
        ent.add(0x20).writePointer(c);
        ent.add(0x34).writeS32(r.tbl === undefined ? 0 : r.tbl);
        ent.add(0x38).writeU32(r.eob);
        ent.add(0x3c).writeU32(r.be === undefined ? 0 : r.be);
        // The correction-bit buffer: one char per bit, our own alternating pattern.
        const bb = alloc(0x20);
        for (let j = 0; j < 0x20; j++) bb.add(j).writeU8((j * 5 + 1) & 1);
        ent.add(0x40).writePointer(bb);
        // Four derived tables and four statistics arrays; only the selected one is reached.
        const counts = [];
        for (let t = 0; t < 4; t++) {
          const dt = alloc(0x500);
          for (let s = 0; s < 256; s++) {
            dt.add(s * 4).writeU32(0x1000 + s + t * 0x100);
            dt.add(0x400 + s).writeU8(((s >> 4) % 13) + 2);
          }
          const cnt = alloc(0x400, 0x11111111);
          ent.add(0x4c + t * 4).writePointer(dt);
          ent.add(0x5c + t * 4).writePointer(cnt);
          counts.push(cnt);
          if (t === (r.tbl === undefined ? 0 : r.tbl) && r.size !== undefined) {
            // symbol = nbits << 4, with nbits counted exactly as 0x004b25e6..0x004b25f3 does.
            let temp = r.eob >> 1, nbits = 0;
            while (temp !== 0) { nbits++; temp >>= 1; }
            const sym = nbits << 4;
            dt.add(0x400 + sym).writeS8(r.size);
            dt.add(sym * 4).writeU32(r.code);
          }
        }
        fx['n' + i] = ent;
        fx['b' + i] = buf;
        fx['d' + i] = dest;
        fx['e' + i] = err;
        fx['t' + i] = counts[r.tbl === undefined ? 0 : r.tbl];
        if (i === 0) fx.obj = ent;
      });
      return fx;
    },
    // Statistics mode off, 0x40 free bytes: no vector reaches dump_buffer 0x004b2420.
    c3bo_eob() {
      return this._c3boEob([
        { eob: 0, bits: 3, buf: 0x00ab0000 },
        { eob: 1, size: 4, code: 0x0a },
        { eob: 2, size: 5, code: 0x1f },
        { eob: 0x3fff, size: 7, code: 0x55 },
        { eob: 4, size: 0, code: 0x12 },
        { eob: 2, bits: 5, buf: 0x00070000, size: 6, code: 0x2a },
        { eob: 1, size: 8, code: 0xff },
        { eob: 1, bits: 10, buf: 0x00ab0000, size: 8, code: 0x5a },
        { eob: 0x7f, size: 5, code: 0x1f },
        { eob: 0x3ff, bits: 2, size: 5, code: 0x11 },
        { eob: 0x8000, size: 2, code: 0x03 },
        { eob: 3, size: 4, code: 0x05, be: 6 },
        { eob: 0x1234, bits: 1, size: 3, code: 0x07, be: 3, tbl: 2 },
        { eob: 7, bits: 7, buf: 0x00800000, size: 1, code: 0x01 },
        { eob: 0x7fff, size: 6, code: 0x3f, be: 2, tbl: 3 },
        { eob: 0x1ff, size: 8, code: 0xaa, tbl: 1 },
      ], 0x40);
    },
    // Statistics mode on: only count_ptrs[ac_tbl_no][symbol] moves, both emit_bits copies are skipped and
    // emit_buffered_bits 0x004b27d0 returns at once.
    c3bo_eob_gather() {
      return this._c3boEob([
        { eob: 0, gather: 1 },
        { eob: 1, gather: 1, tbl: 0 },
        { eob: 3, gather: 1, tbl: 1 },
        { eob: 0x1234, gather: 1, tbl: 2 },
        { eob: 0x7fff, gather: 1, tbl: 3 },
        { eob: 0x8000, gather: 1, tbl: 0, be: 5 },
        { eob: 2, gather: 1, tbl: 2, bits: 5, buf: 0x00123400 },
        { eob: 0x40, gather: 1, tbl: 1 },
      ], 0x40);
    },
    // A nearly full output buffer: every vector drives at least one dump_buffer 0x004b2420, whose
    // empty_output_buffer resets the manager to the same buffer with 0x40 free. Vector 1 reaches the first
    // emit_bits copy's dump at 0x004b26a2, vector 3 its stuffed-zero dump at 0x004b26cc, vector 1 of the
    // second copy 0x004b2753 and vector 4 its stuffed-zero dump at 0x004b277a.
    c3bo_eob_dump() {
      return this._c3boEob([
        { eob: 1, bits: 4, size: 6, code: 0x2a, free: 1 },
        { eob: 0x7f, size: 5, code: 0x1f, free: 1 },
        { eob: 1, size: 8, code: 0xff, free: 2 },
        { eob: 0x3f, buf: 0x00e00000, size: 3, code: 0x00, free: 2 },
        { eob: 0x3fff, size: 7, code: 0x55, free: 1 },
        { eob: 1, size: 0x18, code: 0xffffff, free: 3 },
      ], 1);
    },
  });
})();
