// Fixtures for C3 batch c3br (sound.dll: the speech codec's parameter stages). Every block is allocated here:
// no live sound object, device, list, DirectSound buffer or MIDI stream is ever handed to an arm, and none of
// the functions under test allocates or frees. Layouts come from the disassembly cited in shim/src/re/c3br.cpp.
// All values are synthetic.
//
// THREE MODULE GLOBALS ARE WRITTEN HERE, all three read-only for the functions under test:
//   g_codecOrder   0x100b58a0 = 10    (the filter order; read by quantizeParams at 0x1003c262, 0x1003c44c,
//                                      0x1003c484, 0x1003c49f and 0x1003c4f0)
//   frame length   0x100b58a4 = 0xb4
//   mode flag      0x100b58a8 = 0 or 1
// A byte scan of sound.dll's .text finds exactly one writing instruction for each of the three, all inside
// codecSetParams 0x1003aa50 (0x1003aa68 stores 10, 0x1003aa72 stores 0xb4, 0x1003aa7c stores 1), and
// log/c3/alt/c3bn_probe.py measured 0 entries into codecSetParams and 0 changes of the three globals in 15 s at
// the menu. Neither arm writes them.
Object.assign(globalThis.DIFF_FIXTURES, {

  // --- 0x1003c220 quantizeParams. Shared builder for the two mode variants.
  // The pair at [ebp+8] is read one-based after `sub eax, 4` (0x1003c24d), so a two-int block is passed and its
  // elements 1 and 2 are the block's dwords 0 and 1. The coefficient array at [ebp+0x14] and the out array at
  // [ebp+0x20] are decremented the same way (0x1003c244, 0x1003c23b), so a plain 0-based block of 10 floats and
  // one of 16 ints are what the function sees as elements 1..10.
  // The coefficient sets are chosen so that the two clamps of the second requantisation loop are both reached:
  // element i is scaled by the float 32768.0f at 0x1005c524 (0x1003c28e), halved, biased by the 8-entry table at
  // 0x10064684 and multiplied by the 8-entry float table at 0x100646a4 (0.0204 down to 0.0112), so a |coefficient|
  // near 1.0 drives the product past +/-127 and a small one leaves it inside.
  // The value cells drive the binary search over the threshold table at 0x100647e4: 5000 and 300000 are above the
  // 0x3ff clamp at 0x1003c31f, 936 is exactly a table entry (so the final decrement at 0x1003c390 is not taken)
  // and 500 is between two entries (so it is).
  c3br_quant: function (mode) {
    const m = Process.getModuleByName('sound.dll').base;
    m.add(0x000b58a0).writeS32(10);     // g_codecOrder
    m.add(0x000b58a4).writeS32(0xb4);   // the frame length
    m.add(0x000b58a8).writeS32(mode);   // the mode flag

    const icell = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    const fcell = (v) => { const p = __keepAlloc(4); p.writeFloat(v); return p; };
    const pair = (a, b) => {
      const p = __keepAlloc(8);
      p.writeS32(a); p.add(4).writeS32(b);
      return p;
    };
    const coeffs = (fn) => {
      const p = __keepAlloc(16 * 4);
      for (let i = 0; i < 16; ++i) p.add(i * 4).writeFloat(fn(i));
      return p;
    };

    const out = {
      p_3_4: pair(3, 4), p_0_0: pair(0, 0), p_0_5: pair(0, 5), p_7_0: pair(7, 0),
      p_1_1: pair(1, 1), p_2_9: pair(2, 9), p_0_3: pair(0, 3), p_5_5: pair(5, 5),
      p_6_2: pair(6, 2), p_0_7: pair(0, 7), p_4_0: pair(4, 0),
      v500: fcell(500.0), v1023: fcell(1023.0), v5000: fcell(5000.0), vm17: fcell(-17.0),
      v1: fcell(1.0), v0: fcell(0.0), v1024: fcell(1024.0), v936: fcell(936.0), v33: fcell(33.0),
      vtiny: fcell(1e-7), vhuge: fcell(300000.0), v250: fcell(250.0), v64: fcell(64.0),
      vm1000: fcell(-1000.0),
      // |c| close to or past 1.0 reaches the +0x7f / -0x7f clamps and the 0x3f clamp of the first loop
      c_rampA: coeffs((i) => -0.9 + i * 0.2),
      c_rampB: coeffs((i) => 0.9 - i * 0.2),
      c_rampC: coeffs((i) => (i & 1 ? 0.75 : -0.75)),
      c_small: coeffs((i) => 0.001 * (i + 1)),
      c_big: coeffs(() => 1.5),
      c_bigNeg: coeffs(() => -1.5),
      c_neg: coeffs((i) => -0.05 * (i + 1)),
      c_alt: coeffs((i) => (i & 1 ? 0.4 : -0.4)),
      c_third: coeffs(() => 1.0 / 3.0),           // not exact in float
      c_tiny: coeffs(() => 1e-7),
      c_zero: coeffs(() => 0.0),
    };
    for (let i = 0; i < 13; ++i) out['ix' + i] = icell(i);
    for (let i = 0; i < 14; ++i) {
      const q = __keepAlloc(16 * 4);
      for (let j = 0; j < 16; ++j) q.add(j * 4).writeS32(-0x2b2b2b00 + j);
      out['q' + i] = q;
      out['oc' + i] = icell(-0x5a5a5a5a);
      out['ec' + i] = icell(-0x5a5a5a5a);
    }
    return out;
  },

  // The mode flag clear: the out cell takes `pair[2] + pair[1]*2` (0x1003c300) whenever either element of the
  // pair is zero, and the permutation block at 0x1003c577 is never entered (`je 0x1003c620` at 0x1003c55d).
  c3br_quant_mode0() { return globalThis.DIFF_FIXTURES.c3br_quant(0); },

  // The mode flag set (the value codecSetParams 0x1003aa50 stores): the out cell takes 0 or 0x7f from the
  // equality test at 0x1003c2e7, and those two values are exactly the ones that let the permutation block run.
  c3br_quant_mode1() { return globalThis.DIFF_FIXTURES.c3br_quant(1); },

  // --- 0x1003b5a0 codecTransformStage: stage records, class cells, level cells, coefficient arrays and the
  // flag blocks. Shared builder for the two mode variants.
  // A record is 0xa8 bytes: six cells at +0..+0x14, a three-wide coefficient history at +0x18 (entry i uses
  // the dwords 3i-3, 3i-2 and 3i-1 of it, so ten entries fill +0x18..+0x8c), a three-entry gain history at
  // +0x90 and a three-entry level history at +0x9c.
  // Ranges the tables force (all of them are checked against the dumps of the tables themselves):
  //   the class cell indexes the 16-entry table at 0x10064014, so it stays in 0..15;
  //   the level cell and the level history stay in 0..31, because the output conversion indexes the 64-entry
  //     table at 0x10064214 with (0x1f - level) * 2 (0x1003bd7a);
  //   every coefficient and every history slot stays in [-16, 15], because coefficients 1 and 2 index the
  //     32-entry table at 0x10064314 with twice their magnitude and the magnitude is only clamped on the
  //     negative side (0x1003bdd3);
  //   the record's +8 is 0 or 1 and its +0xc is 0, 2 or 3, which are the values the body itself stores there
  //     (0x1003bce5, 0x1003bcdd), so the code index `class + (+8 << 4) + (+0xc)*4 + 1` stays in 1..32.
  // The +0x14 cell drives both the extra division by 64 (below the int 2048 at 0x10063f04) and the band index
  // (below 2048, 1024 and 128 at 0x10063f10, 0x10063f0c and 0x10063f08), so the four vectors' values 100, 500,
  // 1500 and 3000 give bands 1, 2, 3 and 4; band 4 is the one whose median thresholds are 0.0, which makes
  // every median run, and band 1's are 32767.0, which makes none of them run.
  c3br_transform: function (mode) {
    const m = Process.getModuleByName('sound.dll').base;
    m.add(0x000b58a0).writeS32(10);     // g_codecOrder
    m.add(0x000b58a4).writeS32(0xb4);   // the frame length
    m.add(0x000b58a8).writeS32(mode);   // the mode flag, read at 0x1003b64e

    const icell = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    const fcell = (v) => { const p = __keepAlloc(4); p.writeFloat(v); return p; };
    const pair = (a, b) => {
      const p = __keepAlloc(8);
      p.writeS32(a); p.add(4).writeS32(b);
      return p;
    };
    const iarr = (fn) => {
      const p = __keepAlloc(16 * 4);
      for (let i = 0; i < 16; ++i) p.add(i * 4).writeS32(fn(i));
      return p;
    };
    // history patterns: 'spread' puts the three columns of an entry far apart, 'tight' within one step
    const HIST = {
      spread: (j) => [-14, 0, 14][j % 3] + (j % 2),
      tight: (j) => (j % 3) - 1 + ((j / 3) | 0) % 2,
      ramp: (j) => ((j * 5) % 31) - 15,
      // descending columns, so that the middle column is ABOVE the third one and the second distance of the
      // median test at 0x1003bb87 comes out non-negative
      wave: (j) => [14, 0, -14][j % 3] + (j % 2),
      // carries -16, the one magnitude the output conversion replaces by 0 (`jle 0x1003bde0` at 0x1003bdd7)
      deep: (j) => (j % 4 === 0 ? -16 : ((j * 3) % 15) - 7),
    };
    // (f00, f04, f08, f0c, f10, f14, history pattern, gains, levels)
    const record = (f00, f04, f08, f0c, f10, f14, hfn, g, l) => {
      const p = __keepAlloc(0xa8);
      for (let o = 0; o < 0xa8; o += 4) p.add(o).writeS32(0);
      p.add(0x00).writeS32(f00);
      p.add(0x04).writeS32(f04);
      p.add(0x08).writeS32(f08);
      p.add(0x0c).writeS32(f0c);
      p.add(0x10).writeS32(f10);
      p.add(0x14).writeS32(f14);
      for (let j = 0; j < 30; ++j) p.add(0x18 + j * 4).writeS32(hfn(j));
      for (let j = 0; j < 3; ++j) p.add(0x90 + j * 4).writeS32(g[j]);
      for (let j = 0; j < 3; ++j) p.add(0x9c + j * 4).writeS32(l[j]);
      return p;
    };

    // (class, f00, f04, f08, f0c, f10, f14, history, gains, levels, level cell, coefficients)
    const V0 = [
      [0, 7, 0, 0, 0, 9, 3000, HIST.spread, [3, 9, 20], [4, 17, 29], 5, HIST.deep],
      [1, 2, 0, 0, 0, 5, 3000, HIST.tight, [1, 2, 3], [10, 11, 12], 0, HIST.spread],
      [2, 31, 0, 0, 0, 6, 100, HIST.ramp, [8, 8, 8], [31, 0, 15], 31, HIST.tight],
      [3, 4, 0, 1, 2, 7, 500, HIST.spread, [0, 25, 7], [2, 20, 9], 17, HIST.ramp],
      [4, 11, 0, 1, 0, 8, 1500, HIST.tight, [5, 6, 7], [1, 1, 1], 1, HIST.spread],
      [5, 0, 0, 0, 3, 3, 3000, HIST.ramp, [12, 3, 18], [7, 23, 3], 23, HIST.tight],
      [7, 19, 0, 1, 3, 2, 100, HIST.spread, [30, 2, 11], [13, 6, 28], 8, HIST.ramp],
      [8, 6, 0, 0, 2, 1, 500, HIST.tight, [9, 9, 10], [16, 16, 17], 16, HIST.spread],
      [9, 15, 0, 1, 2, 4, 1500, HIST.ramp, [21, 4, 13], [25, 11, 2], 11, HIST.tight],
      [11, 1, 0, 0, 0, 10, 3000, HIST.spread, [2, 27, 6], [3, 29, 14], 29, HIST.ramp],
      [12, 28, 0, 1, 0, 12, 100, HIST.tight, [14, 15, 16], [20, 21, 22], 4, HIST.spread],
      [13, 3, 0, 0, 2, 2, 500, HIST.ramp, [7, 18, 1], [9, 4, 26], 26, HIST.tight],
      [14, 22, 0, 1, 3, 6, 1500, HIST.spread, [26, 5, 17], [18, 30, 8], 14, HIST.ramp],
      [15, 9, 0, 0, 3, 11, 3000, HIST.tight, [4, 4, 5], [6, 7, 8], 21, HIST.spread],
    ];
    // The mode-set vectors: the comment above each is the code index `class + (+8 << 4) + (+0xc)*4 + 1` the
    // record produces and the bits of the flag word it leaves.
    const V1 = [
      [0, 7, 0, 0, 0, 9, 3000, HIST.spread, [3, 9, 20], [4, 17, 29], 5, HIST.ramp],   // idx 1, 0x10|0x20
      [0, 2, 0, 0, 0, 5, 100, HIST.ramp, [1, 12, 3], [10, 2, 27], 12, HIST.tight],    // idx 1 band 1
      [7, 31, 0, 0, 0, 6, 1500, HIST.tight, [8, 19, 8], [31, 0, 15], 31, HIST.ramp],  // idx 3 band 3
      [3, 4, 0, 0, 0, 7, 500, HIST.spread, [0, 25, 7], [2, 20, 9], 17, HIST.tight],   // idx 4 band 2
      [0, 11, 0, 1, 3, 8, 3000, HIST.tight, [5, 16, 7], [1, 13, 24], 1, HIST.spread], // idx 29, rotation 1
      [3, 0, 1, 0, 2, 3, 3000, HIST.ramp, [12, 3, 18], [7, 23, 3], 23, HIST.tight],   // +4 shortcut, entry <= 4
      [0, 19, 0, 1, 0, 2, 200, HIST.spread, [30, 2, 11], [13, 6, 28], 8, HIST.ramp],  // idx 17, 0x10|0x20
      [7, 6, 0, 1, 0, 3000, 3000, HIST.tight, [9, 21, 10], [16, 5, 17], 16, HIST.spread],  // idx 19, 0x20
      [11, 15, 0, 1, 2, 4, 3000, HIST.spread, [25, 60, 5], [25, 11, 2], 11, HIST.ramp],    // idx 27 band 4
      [14, 1, 0, 1, 2, 10, 500, HIST.tight, [9, 8, 10], [16, 20, 19], 0, HIST.tight],      // idx 27 band 2
      [3, 28, 0, 1, 2, 12, 3000, HIST.wave, [14, 2, 26], [20, 3, 22], 4, HIST.spread],     // idx 28 band 4
      [15, 3, 0, 1, 3, 2, 100, HIST.spread, [7, 18, 1], [9, 4, 26], 26, HIST.ramp],        // idx 31 band 1
      [7, 22, 1, 0, 0, 6, 3000, HIST.spread, [26, 5, 17], [18, 30, 8], 14, HIST.deep],     // +4 shortcut, entry > 4
      [15, 9, 0, 1, 2, 11, 1500, HIST.tight, [4, 32, 5], [6, 27, 8], 21, HIST.ramp],       // idx 27 band 3
    ];
    const V = mode === 0 ? V0 : V1;

    const out = {};
    for (let i = 0; i < V.length; ++i) {
      const v = V[i];
      out['tcls' + i] = icell(v[0]);
      out['trec' + i] = record(v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9]);
      out['tlvl' + i] = icell(v[10]);
      out['tco' + i] = iarr((j) => v[11](j));
      out['tfp' + i] = pair(-0x1a1a1a1a, -0x2b2b2b2b);
      out['tgn' + i] = icell(-0x5a5a5a5a);
      out['tlf' + i] = fcell(-6.25);
      const cf = __keepAlloc(16 * 4);
      for (let j = 0; j < 16; ++j) cf.add(j * 4).writeFloat(-3.5 - j);
      out['tcf' + i] = cf;
    }
    return out;
  },

  // The mode flag clear: the body stops at 0x1003b6e9 after deriving the two flags from the class cell, and
  // jumps straight to the output conversion at 0x1003bd77.
  c3br_transform_mode0() { return globalThis.DIFF_FIXTURES.c3br_transform(0); },

  // The mode flag set (the value codecSetParams 0x1003aa50 stores): the whole transform runs.
  c3br_transform_mode1() { return globalThis.DIFF_FIXTURES.c3br_transform(1); },

  // --- 0x1003da50 interpSubframeParams: codec contexts, flag blocks, coefficient arrays and the per-entry
  // output arrays.
  // A context is 0x1000 bytes; only the eight fields the function binds at 0x1003dade-0x1003db27 are seeded:
  // the previous flag at +0x64c, the previous sub-frame length at +0x650, the previous gain at +0x654, the ten
  // previous coefficients at +0x658 (read `[base + i*4 - 4]`, so element i of the function is dword i-1 of the
  // block), the carried sample count at +0x680 and the first-block flag at +0x684.
  // A flag block is read one-based after `sub eax, 4` (0x1003daa8), so its dwords 0 and 1 are what the body
  // calls elements 1 and 2.
  // Every |coefficient| stays below 1 so that `(1+k)/(1-k)` is positive and the log at 0x1003e04b has a defined
  // argument; the sets include 1/3 and 1e-7, which are not exact in float. Gains stay at or above 1.0 after the
  // two entry clamps (0x1003db3f, 0x1003db5a), so the gain logs at 0x1003e0e2 and 0x1003e104 are defined too.
  // The vectors are sized so that the entry count never passes 6 (the output arrays hold 256 entries and the
  // coefficient blocks 1024 floats).
  c3br_interp() {
    const m = Process.getModuleByName('sound.dll').base;
    m.add(0x000b58a0).writeS32(10);     // g_codecOrder
    m.add(0x000b58a4).writeS32(0xb4);   // the frame length
    m.add(0x000b58a8).writeS32(1);      // the mode flag (read by quantizeParams only)

    const icell = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    const fcell = (v) => { const p = __keepAlloc(4); p.writeFloat(v); return p; };
    const flagBlock = (a, b) => {
      const p = __keepAlloc(16);
      for (let i = 0; i < 4; ++i) p.add(i * 4).writeS32(0);
      p.writeS32(a); p.add(4).writeS32(b);
      return p;
    };
    const coeffs = (fn) => {
      const p = __keepAlloc(16 * 4);
      for (let i = 0; i < 16; ++i) p.add(i * 4).writeFloat(fn(i));
      return p;
    };
    // previous flag, previous length, previous gain, previous coefficients, carry, first-block flag
    const ctx = (flag, len, gain, kfn, carry, first) => {
      const p = __keepAlloc(0x1000);
      for (let o = 0; o < 0x1000; o += 4) p.add(o).writeU32(0);
      p.add(0x64c).writeS32(flag);
      p.add(0x650).writeS32(len);
      p.add(0x654).writeFloat(gain);
      for (let i = 0; i < 10; ++i) p.add(0x658 + i * 4).writeFloat(kfn(i));
      p.add(0x680).writeS32(carry);
      p.add(0x684).writeS32(first);
      return p;
    };

    const K = {
      k_ramp: (i) => -0.85 + i * 0.17,
      k_alt: (i) => (i & 1 ? 0.6 : -0.6),
      k_third: () => 1.0 / 3.0,
      k_tiny: () => 1e-7,
      k_small: (i) => 0.02 * (i + 1),
      k_steep: (i) => 0.9 / (i + 1),
    };

    const out = {
      ord10: icell(10), ord0: icell(0),
      fl180: icell(180), fl0: icell(0),
      // flag blocks: dword 0 = element 1, dword 1 = element 2
      fb_0_0: flagBlock(0, 0), fb_1_3: flagBlock(1, 3), fb_2_1: flagBlock(2, 1),
      fb_0_5: flagBlock(0, 5), fb_4_4: flagBlock(4, 4), fb_1_0: flagBlock(1, 0),
      fb_7_0: flagBlock(7, 0), fb_6_2: flagBlock(6, 2), fb_5_5: flagBlock(5, 5),
      k_a: coeffs(K.k_ramp), k_b: coeffs(K.k_alt), k_c: coeffs(K.k_third),
      k_d: coeffs(K.k_tiny), k_e: coeffs(K.k_small), k_f: coeffs(K.k_steep),
    };

    // (previous flag, previous length, previous gain, previous coefficients, carry, first-block flag)
    const CTX = [
      [0, 45, 1.0, K.k_ramp, 0, 1],      // v1  first block, element 2 zero
      [1, 60, 2.0, K.k_alt, 0, 1],       // v2  first block, element 2 non-zero
      [2, 50, 1.5, K.k_third, 0, 1],     // v3  first block, non-zero remainder
      [0, 10, 1.0, K.k_tiny, 0, 1],      // v4  the null-argument vector (order and frame length 0)
      [0, 45, 1.0, K.k_ramp, 0, 0],      // v5  shape (a), element 2 zero, ratio above 8
      [0, 45, 1.0, K.k_alt, 0, 0],       // v6  shape (a), element 2 zero, ratio at or below 8
      [4, 60, 3.0, K.k_small, 0, 0],     // v7  shape (a), element 2 non-zero, falling ramp
      [0, 30, 2.0, K.k_steep, 0, 0],     // v8  shape (b), previous flag equal to element 1
      [2, 30, 1.0, K.k_ramp, 0, 0],      // v9  shape (b), previous flag different
      [1, 40, 1.0, K.k_alt, 0, 0],       // v10 shape (c), previous flag equal to element 1
      [1, 200, 4.0, K.k_third, 40, 0],   // v11 shape (c), previous flag different, half-rest above 90
      [4, 500, 1.0, K.k_small, 0, 0],    // v12 shape (a), no entry is ever accepted
      [0, 45, 0.2, K.k_ramp, 25, 0],     // v13 shape (a), both entry gain clamps taken
      [6, 15, 2.5, K.k_steep, 0, 0],     // v14 shape (b), previous flag equal to element 1
    ];
    for (let i = 0; i < CTX.length; ++i) {
      const c = CTX[i];
      out['ictx' + i] = ctx(c[0], c[1], c[2], c[3], c[4], c[5]);
      out['imode' + i] = __keepAlloc(256 * 4);
      out['ilen' + i] = __keepAlloc(256 * 4);
      out['igain' + i] = __keepAlloc(256 * 4);
      out['ikout' + i] = __keepAlloc(1024 * 4);
      for (let o = 0; o < 256 * 4; o += 4) {
        out['imode' + i].add(o).writeS32(-0x3c3c0000 + o);
        out['ilen' + i].add(o).writeS32(-0x3d3d0000 + o);
        out['igain' + i].add(o).writeFloat(-9.5 - o);
      }
      for (let o = 0; o < 1024 * 4; o += 4) out['ikout' + i].add(o).writeFloat(-2.5 - (o >> 2));
      out['icount' + i] = icell(-0x5a5a5a5a);
      out['iratio' + i] = fcell(-7.25);
    }
    // the in/out cells and the in/out coefficient array, one per vector
    const SUBLEN = [60, 60, 50, 7, 60, 60, 30, 20, 30, 40, 55, 500, 60, 15];
    const GAIN = [100.0, 8.0, 2.0, 3.0, 1000.0, 1.0, 20.0, 50.0, 2.0, 7.0, 4.0, 1.0, 0.3, 9.0];
    const KIN = [K.k_alt, K.k_ramp, K.k_small, K.k_tiny, K.k_third, K.k_steep, K.k_ramp,
                 K.k_alt, K.k_small, K.k_third, K.k_ramp, K.k_steep, K.k_alt, K.k_tiny];
    for (let i = 0; i < CTX.length; ++i) {
      out['isub' + i] = icell(SUBLEN[i]);
      out['igin' + i] = fcell(GAIN[i]);
      out['ikin' + i] = coeffs(KIN[i]);
    }
    return out;
  },

  // --- 0x1003b230 codecSynthStage: full codec contexts (the stage drives interpSubframeParams 0x1003da50,
  // reflectionToLpc 0x1003d8d0, lpcSynthesize 0x1003d1d0 and applyIir2 0x1003d0a0, so every field those four
  // read is seeded), flag blocks, coefficient arrays, the in/out cells and one output array per vector.
  // Context layout, from the addresses the four bodies bind:
  //   +0x0a8  the synthesis ring, 360 floats (it ends where the cursor begins)
  //   +0x648  the ring write cursor (0x1003b289)
  //   +0x64c..+0x684  interpSubframeParams' six fields (0x1003dade-0x1003db27)
  //   +0x688  lpcSynthesize's previous sub-frame length, +0x68c its excitation array (166 floats),
  //           +0x924 its synthesis history (166 floats), +0xbbc..+0xbc4 and +0xbc8..+0xbd0 its two three-tap
  //           delay lines, +0xbd4 its previous level
  //   +0xbd8, +0xbdc  codecCombFilter's two cursors (both must stay in 1..5, which is the range the routine
  //           itself keeps them in), +0xbe0 its five 16-bit entries
  //   +0xbec..+0xbfc  applyIir2's five filter states (0x1003d0ca-0x1003d0ff)
  // Entry counts are kept at or below 9 so that the per-entry frame arrays of the original (seventeen slots
  // each between [ebp-0xe8] and [ebp-0xa4], [ebp-0x9c] and [ebp-0x58], [ebp-0x58] and [ebp-0x18]) stay inside
  // their own gaps, and so that the ring cursor stays below its 360 floats.
  c3br_synth() {
    const m = Process.getModuleByName('sound.dll').base;
    m.add(0x000b58a0).writeS32(10);     // g_codecOrder
    m.add(0x000b58a4).writeS32(0xb4);   // the frame length
    m.add(0x000b58a8).writeS32(1);      // the mode flag

    const icell = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    const flagBlock = (a, b) => {
      const p = __keepAlloc(16);
      for (let i = 0; i < 4; ++i) p.add(i * 4).writeS32(0);
      p.writeS32(a); p.add(4).writeS32(b);
      return p;
    };
    const fcell = (v) => { const p = __keepAlloc(4); p.writeFloat(v); return p; };
    const coeffs = (fn) => {
      const p = __keepAlloc(16 * 4);
      for (let i = 0; i < 16; ++i) p.add(i * 4).writeFloat(fn(i));
      return p;
    };
    // (cursor, previous flag, previous length, previous gain, previous coefficients, carry, first-block flag,
    //  seed)
    const ctx = (cursor, flag, plen, pgain, kfn, carry, first, seed) => {
      const p = __keepAlloc(0x1000);
      for (let o = 0; o < 0x1000; o += 4) p.add(o).writeU32(0);
      for (let i = 0; i < 360; ++i) p.add(0xa8 + i * 4).writeFloat(Math.sin((i + seed) * 0.11) * 700.0);
      p.add(0x648).writeS32(cursor);
      p.add(0x64c).writeS32(flag);
      p.add(0x650).writeS32(plen);
      p.add(0x654).writeFloat(pgain);
      for (let i = 0; i < 10; ++i) p.add(0x658 + i * 4).writeFloat(kfn(i));
      p.add(0x680).writeS32(carry);
      p.add(0x684).writeS32(first);
      p.add(0x688).writeS32(45);                      // lpcSynthesize's previous sub-frame length
      for (let i = 0; i < 166; ++i) {
        p.add(0x68c + i * 4).writeFloat(Math.sin((i + seed) * 0.37) * 110.0);
        p.add(0x924 + i * 4).writeFloat(Math.cos((i + seed) * 0.23) * 85.0);
      }
      p.add(0xbbc).writeFloat(2.5 + seed);
      p.add(0xbc0).writeFloat(-1.25 * seed);
      p.add(0xbc4).writeFloat(0.1);
      p.add(0xbc8).writeFloat(-6.0 + seed);
      p.add(0xbcc).writeFloat(1.0 / 3.0);
      p.add(0xbd0).writeFloat(1e-7);
      p.add(0xbd4).writeFloat(9.0 + seed);            // lpcSynthesize's previous level
      p.add(0xbd8).writeS32(1 + (seed % 5));          // codecCombFilter's two cursors, both in 1..5
      p.add(0xbdc).writeS32(1 + ((seed * 3) % 5));
      for (let i = 0; i < 5; ++i) p.add(0xbe0 + i * 2).writeS16(900 * (i + 1) - seed * 31);
      for (let i = 0; i < 5; ++i) p.add(0xbec + i * 4).writeFloat(0.5 * (i + 1) - seed * 0.07);
      return p;
    };

    const K = {
      k_ramp: (i) => -0.85 + i * 0.17,
      k_alt: (i) => (i & 1 ? 0.6 : -0.6),
      k_third: () => 1.0 / 3.0,
      k_small: (i) => 0.02 * (i + 1),
      k_steep: (i) => 0.9 / (i + 1),
      k_overHigh: () => 1.4,            // above the 0.99f at 0x1005ebd4, so 0x1003b32e clamps it
      k_overLow: () => -1.4,            // below the -0.99f at 0x1005ebd0, so 0x1003b35c clamps it
      k_mixed: (i) => (i & 1 ? 1.2 : -1.2),
    };

    const out = {
      sfb_0_0: flagBlock(0, 0), sfb_1_3: flagBlock(1, 3), sfb_2_1: flagBlock(2, 1),
      sfb_0_5: flagBlock(0, 5), sfb_4_4: flagBlock(4, 4), sfb_1_0: flagBlock(1, 0),
      sfb_7_0: flagBlock(7, 0), sfb_6_2: flagBlock(6, 2),
    };

    // (cursor, previous flag, previous length, previous gain, previous coefficients, carry, first, seed,
    //  sub-frame length cell, gain cell, input coefficients)
    const V = [
      [0, 0, 45, 1.0, K.k_ramp, 0, 1, 1, 60, 100.0, K.k_mixed],     // s1  first block, element 2 zero
      [0, 1, 60, 2.0, K.k_alt, 0, 1, 2, 200, 8.0, K.k_ramp],        // s2  first block, length clamped to 0x9c
      [40, 2, 50, 1.5, K.k_third, 0, 1, 3, 5, 2.0, K.k_small],      // s3  first block, length clamped to 0x14
      [0, 0, 45, 1.0, K.k_steep, 0, 1, 4, 50, 3.0, K.k_alt],        // s4  first block, non-zero remainder
      [0, 0, 45, 1.0, K.k_ramp, 0, 0, 5, 60, 1000.0, K.k_third],    // s5  shape (a), ratio above 8
      [30, 0, 45, 1.0, K.k_alt, 0, 0, 6, 60, 1.0, K.k_steep],       // s6  shape (a), ratio at or below 8
      [0, 4, 60, 3.0, K.k_small, 0, 0, 7, 30, 20.0, K.k_ramp],      // s7  shape (a), element 2 non-zero
      [0, 0, 30, 2.0, K.k_steep, 0, 0, 8, 20, 50.0, K.k_alt],       // s8  shape (b), previous flag equal
      [0, 2, 30, 1.0, K.k_ramp, 0, 0, 9, 30, 2.0, K.k_small],       // s9  shape (b), previous flag different
      [0, 1, 40, 1.0, K.k_alt, 0, 0, 10, 40, 7.0, K.k_third],       // s10 shape (c), two passes
      [0, 1, 200, 4.0, K.k_third, 40, 0, 11, 55, 4.0, K.k_ramp],    // s11 shape (c), half-rest above 90
      [0, 4, 100, 1.0, K.k_small, -170, 0, 12, 100, 1.0, K.k_steep],// s12 no entry at all (output array null)
      [0, 0, 45, 1.0, K.k_ramp, 0, 1, 13, 156, 6.0, K.k_overHigh],  // s13 first block, every input clamped high
      [60, 0, 45, 0.3, K.k_alt, 0, 0, 14, 60, 0.4, K.k_overLow],    // s14 shape (a), every input clamped low
    ];
    for (let i = 0; i < V.length; ++i) {
      const v = V[i];
      out['sctx' + i] = ctx(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
      out['ssub' + i] = icell(v[8]);
      out['sgin' + i] = fcell(v[9]);
      out['skin' + i] = coeffs(v[10]);
      out['scount' + i] = icell(-0x5a5a5a5a);
      const o = __keepAlloc(256 * 4);
      for (let j = 0; j < 256; ++j) o.add(j * 4).writeFloat(-4.5 - j);
      out['sout' + i] = o;
    }
    return out;
  },
});
