// Fixtures for C3 batch c3ca (sound.dll: the codec's last two unnamed analysis stages, the two codec context
// initialisers and five small setters). Every block is allocated here: no live sound object, device, list,
// DirectSound buffer or MIDI stream is handed to an arm, and none of the functions under test allocates or
// frees. Layouts come from the disassembly cited in shim/src/re/c3ca.cpp. All values are synthetic.
//
// NO MODULE GLOBAL IS SEEDED HERE. The two stages take the frame index, the frame length, the window length,
// the minimum separation and the lowest start as POINTER ARGUMENTS (the live caller passes 0x100648f4,
// 0x100b58a4, 0x100648fc, 0x100648f8 and 0x10064900), so each vector owns its own cells and the fixtures can
// run frame indices 2, 3 and 4 and frame lengths other than 180. codecInitContextA and codecInitContextB do
// write the three codec globals through codecSetParams 0x1003aa50, which stores the constants 10, 0xb4 and 1
// (0x1003aa68, 0x1003aa72, 0x1003aa7c) - the same values the live configuration holds, and the same in both
// arms.
//
// THE TWO STAGES USE ONE BLOCK PER VECTOR. Each argument of a vector points into a single 0x200-byte block
// whose unused parts are left at 0, and the WHOLE block is the compared state region: that compares the
// records the body writes, the cells it only reads, and 0xb0 bytes of slack that nothing should touch.
// Record indices below are given as the CALLER sees them (index 0 = the first dword of the record); the
// bodies move a bounds record back three dwords and the flag record back one, so for a frame index n the
// pair they write is record[2n-2] / record[2n-1] and the pair they read from the previous record is the
// same two indices of that record.
Object.assign(globalThis.DIFF_FIXTURES, {

  // ---------------------------------------------------------------------------------------------------
  // shared helpers
  // ---------------------------------------------------------------------------------------------------

  c3ca_zero: function (p, n) { p.writeByteArray(new Uint8Array(n).buffer); },

  c3ca_ints: function (p, values) {
    const a = new Int32Array(values.length);
    for (let i = 0; i < values.length; ++i) a[i] = values[i];
    p.writeByteArray(a.buffer);
  },

  c3ca_keep: function (cb) { globalThis.__diffKeepAlive.push(cb); return cb; },

  // ---------------------------------------------------------------------------------------------------
  // 0x1003edc0 codecAlignAnalysisWindow
  // ---------------------------------------------------------------------------------------------------
  // Block layout (one per vector, 0x200 bytes):
  //   +0x000 step   +0x010 flag record, 16 dwords   +0x050 mode   +0x060 frame index n
  //   +0x070 previous bounds record, 16 dwords      +0x0b0 bounds record written by the body, 16 dwords
  //   +0x0f0 second bounds record written by the body, 16 dwords
  //   +0x130 frame length   +0x140 window length    +0x150..0x1ff slack
  // The five flags the body reads are record[2n-3] .. record[2n+1]; `allSet` needs all five equal to 1 and
  // `anySet` needs record[2n] or record[2n+1] equal to 1. The step is always one of the values of the live
  // table 0x10064910 (1, 20, 21, ... 34): a step of 0 faults on the `idiv` at 0x1003ef66 and a negative one
  // would not end the two `while` loops.
  c3ca_window() {
    const F = globalThis.DIFF_FIXTURES;
    const O = {STEP: 0x00, FLAGS: 0x10, MODE: 0x50, PN: 0x60, PREV: 0x70, CUR: 0xb0, OUT: 0xf0,
               FLEN: 0x130, WLEN: 0x140};

    // (key, step, flags[16], mode, n, prev[16], cur[16], frameLen, windowLen)
    // flags/prev/cur are given in full so that the index arithmetic is explicit.
    const f = (i3, i4, i5, i6, i7) => [0, 0, 0, i3, i4, i5, i6, i7, 0, 0, 0, 0, 0, 0, 0, 0];
    const b = (i2, i3, i4, i5, i6, i7) => [0, 0, i2, i3, i4, i5, i6, i7, 0, 0, 0, 0, 0, 0, 0, 0];

    // With n = 3 and a frame length of 180 the limits are lo = 181 and hi = 540; the lattice anchor is
    // cur[2] (the body's cur[2n-1]) and the window length is 156 unless the row says otherwise.
    const V = [
      // allSet, mode 0: the align path with no mode correction and no sliding
      ['allset',        20, f(1, 1, 1, 1, 1), 0, 3, b(0, 0, 250, 420, 0, 0), b(200, 0, -1, -1, 0, 0), 180, 156],
      // the four flags of the chain that can break it, one zero each: every `jne` of the chain fires once
      ['f1zero',        20, f(1, 0, 1, 1, 1), 0, 3, b(0, 0, 250, 420, 0, 0), b(200, 0, -1, -1, 0, 0), 180, 156],
      ['f2zero',        20, f(1, 1, 0, 1, 1), 0, 3, b(0, 0, 250, 420, 0, 0), b(200, 0, -1, -1, 0, 0), 180, 156],
      ['f3zero',        20, f(1, 1, 1, 0, 1), 0, 3, b(0, 0, 250, 420, 0, 0), b(200, 0, -1, -1, 0, 0), 180, 156],
      ['f4zero',        20, f(1, 1, 1, 1, 0), 0, 3, b(0, 0, 250, 420, 0, 0), b(200, 0, -1, -1, 0, 0), 180, 156],
      // allSet false (the FIRST flag is 0), anySet true, mode 0: the second half of the entry condition
      ['any_mode0',     21, f(0, 1, 1, 1, 0), 0, 3, b(0, 0, 230, 400, 0, 0), b(210, 0, -1, -1, 0, 0), 180, 156],
      // anySet true, mode 1: the copy path, then the third output arm
      ['any_mode1',     20, f(0, 1, 1, 1, 1), 1, 3, b(0, 0, 240, 430, 0, 0), b(200, 0, -1, -1, 0, 0), 180, 156],
      // anySet true, mode 2 on the copy path: the middle output arm (done == 0 and mode == 2)
      ['any_mode2',     20, f(0, 1, 1, 1, 1), 2, 3, b(0, 0, 240, 430, 0, 0), b(200, 0, -1, -1, 0, 0), 180, 156],
      // anySet true through the SECOND flag of frame n only, mode 3: copy path, third output arm
      ['any_mode3',     22, f(0, 1, 1, 0, 1), 3, 3, b(0, 0, 260, 450, 0, 0), b(220, 0, -1, -1, 0, 0), 180, 156],
      // no flag of frame n set: copy path and the FIRST output arm (anySet == 0)
      ['none',          20, f(1, 1, 1, 0, 0), 0, 3, b(0, 0, 250, 420, 0, 0), b(200, 0, -1, -1, 0, 0), 180, 156],
      // mode 2 with an end past prev.hi: the one-step subtraction fires and nothing slides after it
      ['m2_sub',        20, f(1, 1, 1, 1, 1), 2, 3, b(0, 0, 300, 335, 0, 0), b(220, 0, -1, -1, 0, 0), 180, 156],
      // mode 2 with an end at or below prev.hi: it does not
      ['m2_nosub',      20, f(1, 1, 1, 1, 1), 2, 3, b(0, 0, 300, 500, 0, 0), b(220, 0, -1, -1, 0, 0), 180, 156],
      // mode 1 with a start before prev.lo: the one-step addition fires and nothing slides after it
      ['m1_add',        20, f(1, 1, 1, 1, 1), 1, 3, b(0, 0, 400, 435, 0, 0), b(200, 0, -1, -1, 0, 0), 180, 156],
      // mode 3 with a start at or after prev.lo: it does not
      ['m3_noadd',      20, f(1, 1, 1, 1, 1), 3, 3, b(0, 0, 200, 450, 0, 0), b(220, 0, -1, -1, 0, 0), 180, 156],
      // a far prev pair: the first `while` slides the window down eight times
      ['slide_down',    20, f(1, 1, 1, 1, 1), 0, 3, b(0, 0, 520, 700, 0, 0), b(200, 0, -1, -1, 0, 0), 180, 156],
      // a prev pair below the lower limit: the second `while` slides it up twenty times
      ['slide_up',      20, f(1, 1, 1, 1, 1), 0, 3, b(0, 0, -200, -40, 0, 0), b(200, 0, -1, -1, 0, 0), 180, 156],
      // step 1 (the table's first entry): the sub-window is the whole pair
      ['step1',          1, f(1, 1, 1, 1, 1), 0, 3, b(0, 0, 250, 420, 0, 0), b(200, 0, -1, -1, 0, 0), 180, 156],
      // the largest step of the table, with an anchor that is not a multiple of it
      ['step34',        34, f(1, 1, 1, 1, 1), 0, 3, b(0, 0, 250, 420, 0, 0), b(203, 0, -1, -1, 0, 0), 180, 156],
      // a copied pair shorter than the step: the sub-window length is 0 and the FIRST output arm runs
      ['sub0',          34, f(0, 1, 1, 1, 1), 1, 3, b(0, 0, 300, 309, 0, 0), b(200, 0, -1, -1, 0, 0), 180, 156],
      // THE FOUR ROUNDING VECTORS. floatFloorToInt rounds half away from zero, so a shift of exactly +/-0.5
      // steps moves the window by one step and a shift of +/-0.4 does not. The two negative ones use the
      // anchor 405, whose lattice is 5 mod 20: the body's `idiv` truncates toward zero, so the first lattice
      // point it computes is 205 and not the smallest one at or above the limit (185), which is what makes a
      // shift of -1 visible (with an aligned anchor the upward slide would put every negative shift back on
      // the same point).
      ['posthird',      20, f(1, 1, 1, 1, 1), 0, 3, b(0, 0, 275, 300, 0, 0), b(200, 0, -1, -1, 0, 0), 180, 156],
      ['possmall',      20, f(1, 1, 1, 1, 1), 0, 3, b(0, 0, 271, 300, 0, 0), b(200, 0, -1, -1, 0, 0), 180, 156],
      ['neghalf',       20, f(1, 1, 1, 1, 1), 0, 3, b(0, 0, 265, 280, 0, 0), b(405, 0, -1, -1, 0, 0), 180, 156],
      ['negsmall',      20, f(1, 1, 1, 1, 1), 0, 3, b(0, 0, 269, 280, 0, 0), b(405, 0, -1, -1, 0, 0), 180, 156],
      // frame index 2: the five flags are record[1..5], the pair written is record[2] / record[3] and the
      // lattice anchor is record[0]
      ['n2',            20, [0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], 0, 2,
                        [0, 0, 120, 300, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
                        [100, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], 180, 156],
      // frame index 4: the five flags are record[5..9], the pair written is record[6] / record[7] and the
      // lattice anchor is record[4]
      ['n4',            20, [0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0], 0, 4,
                        [0, 0, 0, 0, 0, 0, 420, 600, 0, 0, 0, 0, 0, 0, 0, 0],
                        [0, 0, 0, 0, 400, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0], 180, 156],
      // a short frame length and window, so that the limits are small and the upward slide runs
      ['shortframe',    20, f(1, 1, 1, 1, 1), 0, 3, b(0, 0, 40, 90, 0, 0), b(5, 0, -1, -1, 0, 0), 40, 30],
    ];

    const out = {};
    for (let i = 0; i < V.length; ++i) {
      const v = V[i];
      const k = v[0];
      const p = __keepAlloc(0x200);
      F.c3ca_zero(p, 0x200);
      p.add(O.STEP).writeS32(v[1]);
      F.c3ca_ints(p.add(O.FLAGS), v[2]);
      p.add(O.MODE).writeS32(v[3]);
      p.add(O.PN).writeS32(v[4]);
      F.c3ca_ints(p.add(O.PREV), v[5]);
      F.c3ca_ints(p.add(O.CUR), v[6]);
      F.c3ca_ints(p.add(O.OUT), [-2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2]);
      p.add(O.FLEN).writeS32(v[7]);
      p.add(O.WLEN).writeS32(v[8]);
      out['w_blk_' + k] = p;
      out['w_step_' + k] = p.add(O.STEP);
      out['w_flags_' + k] = p.add(O.FLAGS);
      out['w_mode_' + k] = p.add(O.MODE);
      out['w_pn_' + k] = p.add(O.PN);
      out['w_prev_' + k] = p.add(O.PREV);
      out['w_cur_' + k] = p.add(O.CUR);
      out['w_out_' + k] = p.add(O.OUT);
      out['w_flen_' + k] = p.add(O.FLEN);
      out['w_wlen_' + k] = p.add(O.WLEN);
    }
    return out;
  },

  // ---------------------------------------------------------------------------------------------------
  // 0x10040740 codecSelectWindowFromJumps
  // ---------------------------------------------------------------------------------------------------
  // Block layout (one per vector, 0x200 bytes):
  //   +0x000 jump index array, 32 dwords            +0x080 slot count   +0x090 the unread third argument
  //   +0x0a0 mode cell written by the body          +0x0b0 bounds record, 16 dwords
  //   +0x0f0 frame index n   +0x100 frame length    +0x110 minimum separation
  //   +0x120 window length   +0x130 lowest start    +0x140 the unread eleventh argument
  //   +0x150..0x1ff slack
  // The body reads the array one-based after moving it back one dword, so entry i of the body is dword i-1
  // of the array and the scan starts at entry *count - 1, exactly as codecMarkCorrelationJumps fills it
  // (*count is one past the last entry). The slots at and above *count - 1 are set to 0x7000, well above
  // every upper limit used here, so that a vector which did walk past the end of its run would be visible
  // rather than reading whatever follows in the block.
  c3ca_jumps() {
    const F = globalThis.DIFF_FIXTURES;
    const O = {JUMPS: 0x00, COUNT: 0x80, UNUSED3: 0x90, MODE: 0xa0, BOUNDS: 0xb0, PN: 0xf0, FLEN: 0x100,
               GAP: 0x110, WLEN: 0x120, MINSTART: 0x130, UNUSED11: 0x140};
    const SENTINEL = 0x7000;

    // (key, real jump entries, bounds[16], n, frameLen, minGap, winLen, minStart)
    const b = (i3) => [0, 0, 0, i3, -3, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];

    const V = [
      // no entry at all: the scan never runs, i comes out 1 and the mode-0 arm takes `start`
      ['none_empty',    [],                 b(400), 3, 180, 90, 156, 307],
      // every entry above the upper limit: the scan walks the whole array down to 0, mode 0 again, and
      // `start` is below the lowest start, so the other side of that max is taken
      ['none_high',     [600, 700, 800],    b(100), 3, 180, 90, 156, 307],
      // every entry below the lower limit: the scan stops at once but the entry fails the `>= lo` test
      ['none_low',      [100, 120, 150],    b(100), 3, 180, 90, 156, 307],
      // `start` exactly equal to the lowest start: the boundary of the mode-0 max
      ['none_eqstart',  [600],              b(306), 3, 180, 90, 156, 307],
      // one usable entry, not past the frame bar: the run loop ends on the sentinel, mode 1
      ['m1_single',     [300],              b(100), 3, 180, 90, 156, 307],
      // the same with an end past the upper limit: the mode-1 min clamps it
      ['m1_clamped',    [500],              b(100), 3, 180, 90, 156, 307],
      // one entry past the frame bar: mode 2, and the start comes from the window length (lo < back)
      ['m2_back',       [450],              b(100), 3, 180, 90, 156, 307],
      // one entry past a frame bar that the gap bar overtakes: mode 2 with the start coming from lo
      ['m2_lo',         [540],              b(399), 3, 180, 90, 156, 307],
      // two entries at least the minimum separation apart and the second inside the window: mode 3
      ['m3_pair',       [200, 300],         b(100), 3, 180, 90, 156, 307],
      // two entries far apart: the run loop leaves on the window test, mode 1
      ['m1_far',        [200, 400],         b(100), 3, 180, 90, 156, 307],
      // two entries closer than the minimum separation: the gap loop finds nothing, mode 1
      ['m1_close',      [200, 230],         b(100), 3, 180, 90, 156, 307],
      // three entries, the gap loop breaking on its second step and the run loop on its second
      ['m3_third',      [200, 230, 330],    b(100), 3, 180, 90, 156, 307],
      // three close entries: the gap loop runs to exhaustion, mode 1
      ['m1_three',      [200, 230, 250],    b(100), 3, 180, 90, 156, 307],
      // entries above the limit first, so the downward scan takes several steps
      ['m3_scan',       [200, 300, 600, 700], b(100), 3, 180, 90, 156, 307],
      // a first entry below the lower limit, so the second scan stops on it
      ['m3_jscan',      [100, 250, 350],    b(100), 3, 180, 90, 156, 307],
      // frame index 2
      ['n2',            [100],              [0, 50, -3, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
                        2, 180, 90, 156, 307],
      // frame index 4
      ['n4',            [500],              [0, 0, 0, 0, 0, 400, -3, -3, 0, 0, 0, 0, 0, 0, 0, 0],
                        4, 180, 90, 156, 307],
      // a frame length of 2: the upper limit is 4, so the mode-1 end is clamped down to it
      ['tinyframe',     [3],                [0, 0, -3, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
                        2, 2, 90, 156, 307],
      // THE BOUND OF THE LAST LOOP: *count says one entry, so the run ends at index 1, but a second entry
      // sits in the next slot and is inside the window. A loop bounded by anything larger than the run
      // would read it and report mode 3; the original stops at the end of the run and reports mode 1.
      ['past_run',      [200],              b(100), 3, 180, 90, 156, 307, [[1, 300]]],
      // a larger minimum separation than the window length: the mode-3 test can never fire
      ['biggap',        [200, 300],         b(100), 3, 180, 200, 156, 307],
    ];

    const out = {};
    for (let i = 0; i < V.length; ++i) {
      const v = V[i];
      const k = v[0];
      const entries = v[1];
      const p = __keepAlloc(0x200);
      F.c3ca_zero(p, 0x200);
      const jumps = new Array(32);
      for (let s = 0; s < 32; ++s) jumps[s] = (s < entries.length) ? entries[s] : SENTINEL;
      const extra = v[8] || [];
      for (let e = 0; e < extra.length; ++e) jumps[extra[e][0]] = extra[e][1];
      F.c3ca_ints(p.add(O.JUMPS), jumps);
      p.add(O.COUNT).writeS32(entries.length + 1);
      p.add(O.UNUSED3).writeS32(10);
      p.add(O.MODE).writeS32(-4);
      F.c3ca_ints(p.add(O.BOUNDS), v[2]);
      p.add(O.PN).writeS32(v[3]);
      p.add(O.FLEN).writeS32(v[4]);
      p.add(O.GAP).writeS32(v[5]);
      p.add(O.WLEN).writeS32(v[6]);
      p.add(O.MINSTART).writeS32(v[7]);
      p.add(O.UNUSED11).writeS32(462);
      out['j_blk_' + k] = p;
      out['j_jumps_' + k] = p.add(O.JUMPS);
      out['j_count_' + k] = p.add(O.COUNT);
      out['j_unused3_' + k] = p.add(O.UNUSED3);
      out['j_mode_' + k] = p.add(O.MODE);
      out['j_bounds_' + k] = p.add(O.BOUNDS);
      out['j_pn_' + k] = p.add(O.PN);
      out['j_flen_' + k] = p.add(O.FLEN);
      out['j_gap_' + k] = p.add(O.GAP);
      out['j_wlen_' + k] = p.add(O.WLEN);
      out['j_minstart_' + k] = p.add(O.MINSTART);
      out['j_unused11_' + k] = p.add(O.UNUSED11);
    }
    return out;
  },

  // ---------------------------------------------------------------------------------------------------
  // 0x1003aae0 codecInitContextA: the 0x2544-byte encoder context
  // ---------------------------------------------------------------------------------------------------
  // Each context sits in a 0x2944 block with 0x200 bytes of slack on each side, and the WHOLE block is the
  // compared state: the body should write nothing outside 0..0x2544. The five regions the body does NOT
  // write (+0x20b4..+0x20db, +0x20ec..+0x20fb, +0x2104..+0x2113, +0x21cc and +0x221c) are seeded with a
  // different pattern per vector, which is what makes the fourteen final states distinguishable; every
  // other byte is seeded too, so the A/B also sees the whole context being overwritten.
  c3ca_ctxa() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    for (let i = 0; i < 14; ++i) {
      const blk = __keepAlloc(0x2944);
      F.c3ca_zero(blk, 0x2944);
      const ctx = blk.add(0x200);
      // a cheap deterministic fill of the whole context, different for every vector
      const fill = new Int32Array(0x2544 / 4);
      for (let w = 0; w < fill.length; ++w) fill[w] = ((w * 2654435761) ^ (i * 0x9e3779b9)) | 0;
      ctx.writeByteArray(fill.buffer);
      // the five regions the body never writes, with values that stay small and legible
      for (let s = 0; s < 10; ++s) ctx.add(0x20b4 + s * 4).writeS32(0xb5 + i * 7 + s * 13);
      for (let s = 0; s < 4; ++s) ctx.add(0x20ec + s * 4).writeS32(0x100 + i * 11 + s * 17);
      for (let s = 0; s < 4; ++s) ctx.add(0x2104 + s * 4).writeS32(0x180 + i * 5 + s * 19);
      ctx.add(0x21cc).writeFloat(0.25 * (i + 1));
      ctx.add(0x221c).writeS32(i * 3 - 7);
      out['a_blk' + i] = blk;
      out['a_ctx' + i] = ctx;
    }
    return out;
  },

  // ---------------------------------------------------------------------------------------------------
  // 0x1003aef0 codecInitContextB: the 0xc00-byte decoder context
  // ---------------------------------------------------------------------------------------------------
  // Same shape. The four regions the body does NOT write are +0x64c, +0x650, +0x658..+0x683 and
  // +0xbea..+0xbeb.
  c3ca_ctxb() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    for (let i = 0; i < 14; ++i) {
      const blk = __keepAlloc(0x1000);
      F.c3ca_zero(blk, 0x1000);
      const ctx = blk.add(0x200);
      const fill = new Int32Array(0xc00 / 4);
      for (let w = 0; w < fill.length; ++w) fill[w] = ((w * 40503) ^ (i * 0x85ebca6b)) | 0;
      ctx.writeByteArray(fill.buffer);
      ctx.add(0x64c).writeS32(0x40 + i);
      ctx.add(0x650).writeS32(0x900 - i * 3);
      for (let s = 0; s < 11; ++s) ctx.add(0x658 + s * 4).writeFloat(0.125 * (s + 1) - i * 0.01);
      ctx.add(0xbea).writeS16(0x1200 + i);
      out['b_blk' + i] = blk;
      out['b_ctx' + i] = ctx;
    }
    return out;
  },

  // ---------------------------------------------------------------------------------------------------
  // 0x10037f30 Voice_setPanGains
  // ---------------------------------------------------------------------------------------------------
  // A synthetic voice record of 0x100 bytes. Only the int at +0x50 and the double at +0x58 are inputs; the
  // outputs are the ints at +0x68 and +0x78 and the doubles at +0x70 and +0x80. The levels are chosen so
  // that both the 0x10000 clamp of each int and the 1.0 clamp of each double are taken on both sides, and
  // the base doubles include values whose products are not exact in binary (0.1, 1/3).
  c3ca_voice() {
    const F = globalThis.DIFF_FIXTURES;
    const SPEC = [['a', 0x10000, 0.9], ['b', 0x8000, 0.5], ['c', 0x20000, 1.0], ['d', 100, 0.1],
                  ['e', 0x10000, 2.0], ['f', -5000, 1.0 / 3.0], ['g', 1, 1e-7]];
    const out = {};
    for (let i = 0; i < SPEC.length; ++i) {
      const p = __keepAlloc(0x100);
      F.c3ca_zero(p, 0x100);
      p.add(0x48).writeS32(-99);
      p.add(0x50).writeS32(SPEC[i][1]);
      p.add(0x58).writeDouble(SPEC[i][2]);
      p.add(0x68).writeS32(-1);
      p.add(0x70).writeDouble(-1.5);
      p.add(0x78).writeS32(-1);
      p.add(0x80).writeDouble(-2.5);
      out['v_' + SPEC[i][0]] = p;
    }
    return out;
  },

  // ---------------------------------------------------------------------------------------------------
  // 0x1001e530 Track_clearFlag8
  // ---------------------------------------------------------------------------------------------------
  // Twelve track records of 0x60 bytes, differing only in the flag dword at +0x4c, so that the `and al,
  // 0xf7` is seen on words with and without bit 3, with other low bits set and with the high three bytes
  // carrying values the mask must not touch.
  c3ca_track() {
    const F = globalThis.DIFF_FIXTURES;
    const VALS = [0, 8, 7, 0xf, 0xf7, 0xff, 0x100, 0x1008, 0x12345678, -1, -0x80000000, 0x7fffffff];
    const out = {};
    for (let i = 0; i < VALS.length; ++i) {
      const p = __keepAlloc(0x60);
      F.c3ca_zero(p, 0x60);
      p.add(0x4c).writeS32(VALS[i] | 0);
      p.add(0x50).writeS32(0x5a5a0000 + i);
      out['t' + i] = p;
    }
    return out;
  },

  // ---------------------------------------------------------------------------------------------------
  // 0x10035750 Channel_setPitchRate
  // ---------------------------------------------------------------------------------------------------
  // Four channel records of 0x300 bytes; the body writes the dword at +0x220 and nothing else, so the
  // neighbouring dwords are seeded to show that.
  c3ca_channel() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    for (let i = 0; i < 4; ++i) {
      const p = __keepAlloc(0x300);
      F.c3ca_zero(p, 0x300);
      p.add(0x21c).writeS32(0x11110000 + i);
      p.add(0x220).writeS32(0x22220000 + i);
      p.add(0x224).writeS32(0x33330000 + i);
      out['c' + i] = p;
    }
    return out;
  },

  // ---------------------------------------------------------------------------------------------------
  // 0x1002b260 Seq_applyChannelLevel
  // ---------------------------------------------------------------------------------------------------
  // A sequencer record of 0x80 bytes whose +0x40 is either null or a SYNTHETIC inner object built here:
  // never a live MIDI or DirectSound device. The inner object's vtable slot 0x40 is a __thiscall
  // NativeCallback that records the value it was called with at inner+0x80 and counts the calls at
  // inner+0x84, so the A/B sees whether the arm made the call and with which argument.
  c3ca_seq() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    for (let i = 0; i < 5; ++i) {
      const seq = __keepAlloc(0x80);
      F.c3ca_zero(seq, 0x80);
      seq.add(0x04).writeS32(-7 - i);
      out['s_null' + i] = seq;
    }
    for (let i = 0; i < 5; ++i) {
      const inner = __keepAlloc(0x100);
      F.c3ca_zero(inner, 0x100);
      const vt = __keepAlloc(0x100);
      F.c3ca_zero(vt, 0x100);
      vt.add(0x40).writePointer(F.c3ca_keep(new NativeCallback(function (self, value) {
        self.add(0x80).writeS32(value);
        self.add(0x84).writeS32(self.add(0x84).readS32() + 1);
      }, 'void', ['pointer', 'int'], 'thiscall')));
      inner.writePointer(vt);
      inner.add(0x80).writeS32(-55);
      inner.add(0x84).writeS32(1000 + i);
      const seq = __keepAlloc(0x80);
      F.c3ca_zero(seq, 0x80);
      seq.add(0x04).writeS32(-7 - i);
      seq.add(0x40).writePointer(inner);
      out['s_dev' + i] = seq;
      out['s_inner' + i] = inner;
    }
    return out;
  },

  // ---------------------------------------------------------------------------------------------------
  // 0x1000f160 DsBuffer::setFrequency and 0x1000f190 DsBuffer::setVolumeRaw
  // ---------------------------------------------------------------------------------------------------
  // A synthetic buffer record of 0x100 bytes whose +0x60 is either null or a SYNTHETIC interface built
  // here: never a real IDirectSoundBuffer. The interface's vtable slot (0x44 for setFrequency, 0x3c for
  // setVolumeRaw) is a __stdcall NativeCallback - the wrapper pushes (interface, value) and cleans nothing
  // up before its own `ret 4` - that records the value at interface+8 and counts the calls at interface+0xc.
  c3ca_dsbuf: function (prefix, slot) {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    for (let i = 0; i < 4; ++i) {
      const p = __keepAlloc(0x100);
      F.c3ca_zero(p, 0x100);
      p.add(0xc4).writeS32(-33 - i);
      out[prefix + '_null' + i] = p;
    }
    for (let i = 0; i < 4; ++i) {
      const iface = __keepAlloc(0x40);
      F.c3ca_zero(iface, 0x40);
      const vt = __keepAlloc(0x100);
      F.c3ca_zero(vt, 0x100);
      vt.add(slot).writePointer(F.c3ca_keep(new NativeCallback(function (self, value) {
        self.add(8).writeS32(value);
        self.add(0xc).writeS32(self.add(0xc).readS32() + 1);
      }, 'void', ['pointer', 'int'], 'stdcall')));
      iface.writePointer(vt);
      iface.add(8).writeS32(-66);
      iface.add(0xc).writeS32(2000 + i);
      const p = __keepAlloc(0x100);
      F.c3ca_zero(p, 0x100);
      p.add(0x60).writePointer(iface);
      p.add(0xc4).writeS32(-33 - i);
      out[prefix + '_dev' + i] = p;
      out[prefix + '_iface' + i] = iface;
    }
    return out;
  },

  c3ca_dsfreq() { return globalThis.DIFF_FIXTURES.c3ca_dsbuf('f', 0x44); },
  c3ca_dsvol() { return globalThis.DIFF_FIXTURES.c3ca_dsbuf('g', 0x3c); },
});
