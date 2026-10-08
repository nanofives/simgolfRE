// Fixtures for C3 batch c3bv (sound.dll: the speech codec's stage drivers). Every block is allocated here:
// no live sound object, device, list, DirectSound buffer or MIDI stream is ever handed to an arm, and none of
// the functions under test allocates or frees. Layouts come from the disassembly cited in shim/src/re/c3bv.cpp.
// All values are synthetic.
//
// TWO MODULE GLOBALS ARE WRITTEN HERE, both read-only for the functions under test:
//   g_codecOrder   0x100b58a0 = 10    (read by codecStageDispatch at 0x1003cabf and 0x1003cfd4, and its
//                                      ADDRESS handed to codecAutocorrelation, gaussSolveClamped and
//                                      substituteIfBelowThreshold at 0x1003cf56, 0x1003cf7b, 0x1003cf9c)
//   frame length   0x100b58a4 = 0xb4  (read at 0x1003c75b, 0x1003c78a, ... and its address handed to five
//                                      stages)
// A byte scan of sound.dll's .text finds exactly one writing instruction for each, both inside codecSetParams
// 0x1003aa50 (0x1003aa68 stores 10, 0x1003aa72 stores 0xb4), and log/c3/alt/c3bn_probe.py measured 0 entries
// into codecSetParams and 0 changes of the codec globals in 15 s at the menu. Neither arm writes them.
//
// THE ENCODER CONTEXT is one 0x2600-byte object (the size c3bi already used for codecGainSmoother). Its
// layout, from the pointers codecStageDispatch binds at 0x1003c68b-0x1003c750 and codecFeatureDecision at
// 0x1003f73e-0x1003f7cd:
//   +0x10   pa  540 floats, element i at +0x10 + (i - 0xb5)*4   (one-based with the bias -0x2d4)
//   +0x880  qa  540 floats, same shape
//   +0x10f0 sa  696 floats, element i at +0x10f0 + (i - 0x19)*4 (bias -0x64)
//   +0x1bd0 ra  312 floats, element i at +0x1bd0 + (i - 0xe5)*4 (bias -0x394)
//   +0x20b0 the float offset cell          +0x20b4 ten one-based jump indices
//   +0x20dc the jump count                 +0x20e0 w[0..2]      +0x20ec u[0..5]
//   +0x2104 v[0..5]                        +0x211c t[0..7]      +0x213c x[0..2]
//   +0x2148 y, ten coefficients per frame three frames deep     +0x21c0 the first-order filter state
//   +0x21c4..+0x2220 codecMarkCorrelationJumps' state (re/analysis/systems/sound_9.md)
//   +0x2224 A, +0x2228 B, +0x222c C, +0x2230 D[0..5], +0x2248 E, +0x224c F, +0x2250 G, +0x2254 H,
//   +0x2258 I, +0x225c K, +0x2260 J, +0x2264 L                  codecFeatureDecision's cells
//   +0x2268..+0x253f codecGainSmoother's window (c3bi)          +0x2540 bitPermute's toggle (0x1003bf76)
//
// THE DECODER CONTEXT is a different, smaller object: its first 0xa8 bytes are codecTransformStage's stage
// record and +0xa8..+0xc08 is codecSynthStage's state, both already mapped by c3br
// (re/frida/js/fixtures.d/c3br.js).
Object.assign(globalThis.DIFF_FIXTURES, {

  // ---------------------------------------------------------------------------------------------------
  // shared helpers
  // ---------------------------------------------------------------------------------------------------

  // A block of n floats written in one go (writeByteArray is far cheaper than n writeFloat calls).
  c3bv_floats: function (p, n, fn) {
    const a = new Float32Array(n);
    for (let i = 0; i < n; ++i) a[i] = fn(i);
    p.writeByteArray(a.buffer);
  },

  c3bv_ints: function (p, n, fn) {
    const a = new Int32Array(n);
    for (let i = 0; i < n; ++i) a[i] = fn(i);
    p.writeByteArray(a.buffer);
  },

  // The 53-entry interleaving table bitPermute 0x1003bf30 uses at 0x1003c097 (dumped from sound.dll's rdata
  // at 0x1006447c; the first entry, index 0, is not part of the unpack loop). The loop walks j = 53 down to 1
  // and does `slot[PERM[j]] = slot[PERM[j]] * 2 + bits[j]`, so PERM says which quantised value each bit of
  // the packed word stream belongs to and the first bit a slot receives is its most significant.
  // Slot 1 collects 7 bits and becomes the class cell, slot 2 collects 5 bits and becomes the level cell,
  // and slots 4..13 collect 2, 3, 4, 4, 4, 4, 5, 5, 5 and 5 bits and become the ten quantised coefficients
  // (the sign-bit masks at 0x10064454 are 2, 4, 8, 8, 8, 8, 16, 16, 16, 16, applied at 0x1003c0f4).
  // bitPermute reads bit j from its pointer AFTER the `sub ecx, 4` at 0x1003bf5a, so bit j is dword j-1 of
  // the array codecProcessBlock passes.
  c3bv_perm: [16, 13, 12, 11, 1, 2, 13, 12, 11, 1, 2, 13, 10, 11, 2, 1, 10, 13, 12, 11, 10, 2, 13, 12, 11,
              10, 2, 1, 12, 7, 6, 1, 10, 9, 8, 7, 4, 6, 9, 8, 7, 5, 1, 9, 8, 4, 6, 1, 5, 9, 8, 7, 5, 6, 2, 0],

  // Build the 53 one-dword bits that make bitPermute's unpack produce the requested slot values.
  // slots is an object keyed by slot number (1, 2, 4..13); a slot left out comes out 0.
  c3bv_bits: function (slots) {
    const PERM = globalThis.DIFF_FIXTURES.c3bv_perm;
    const pos = {};
    for (let j = 53; j >= 1; --j) {        // the order the unpack loop consumes them in
      const s = PERM[j];
      if (!pos[s]) pos[s] = [];
      pos[s].push(j);                      // most significant bit of that slot first
    }
    const bits = new Int32Array(53);
    for (const s in pos) {
      const list = pos[s];
      const n = list.length;
      const v = (slots[s] | 0) & ((1 << n) - 1);
      for (let k = 0; k < n; ++k) bits[list[k] - 1] = (v >> (n - 1 - k)) & 1;
    }
    const p = __keepAlloc(64 * 4);
    p.writeByteArray(bits.buffer);
    return p;
  },

  // ---------------------------------------------------------------------------------------------------
  // 0x1003a980 codecProcessBlock: packed words in, samples out, over a decoder context
  // ---------------------------------------------------------------------------------------------------
  // The decoder context is c3br's stage record (0..0xa8) glued to c3br's synthesis state (+0xa8..+0xc08).
  // Both halves are seeded here because codecProcessBlock drives codecTransformStage and codecSynthStage one
  // after the other over the same object.
  // The packed words are built with c3bv_bits so that the class cell stays in 0..15 (its seven bits are
  // capped here), the level cell in 0..31 and the ten quantised values in the ranges their sign masks allow,
  // which is what keeps codecTransformStage's table lookups inside their tables (c3br's reading of the
  // 16-entry class table at 0x10064014 and the 64-entry level table at 0x10064214).
  c3bv_block() {
    const m = Process.getModuleByName('sound.dll').base;
    m.add(0x000b58a0).writeS32(10);        // g_codecOrder
    m.add(0x000b58a4).writeS32(0xb4);      // the frame length
    m.add(0x000b58a8).writeS32(1);         // the mode flag codecSetParams stores (codecTransformStage reads it)

    const F = globalThis.DIFF_FIXTURES;
    // the stage record (c3br's layout: six cells, a three-wide coefficient history, a gain and a level history)
    const decCtx = (rec, hist, gains, levels, cursor, pflag, plen, pgain, kfn, carry, first, seed) => {
      const p = __keepAlloc(0x1000);
      p.writeByteArray(new Uint8Array(0x1000).buffer);
      for (let i = 0; i < 6; ++i) p.add(i * 4).writeS32(rec[i]);
      for (let j = 0; j < 30; ++j) p.add(0x18 + j * 4).writeS32(hist(j));
      for (let j = 0; j < 3; ++j) p.add(0x90 + j * 4).writeS32(gains[j]);
      for (let j = 0; j < 3; ++j) p.add(0x9c + j * 4).writeS32(levels[j]);
      // codecSynthStage's state
      F.c3bv_floats(p.add(0xa8), 360, (i) => Math.sin((i + seed) * 0.11) * 700.0);
      p.add(0x648).writeS32(cursor);
      p.add(0x64c).writeS32(pflag);
      p.add(0x650).writeS32(plen);
      p.add(0x654).writeFloat(pgain);
      for (let i = 0; i < 10; ++i) p.add(0x658 + i * 4).writeFloat(kfn(i));
      p.add(0x680).writeS32(carry);
      p.add(0x684).writeS32(first);
      p.add(0x688).writeS32(45);
      F.c3bv_floats(p.add(0x68c), 166, (i) => Math.sin((i + seed) * 0.37) * 110.0);
      F.c3bv_floats(p.add(0x924), 166, (i) => Math.cos((i + seed) * 0.23) * 85.0);
      p.add(0xbbc).writeFloat(2.5 + seed);
      p.add(0xbc0).writeFloat(-1.25 * seed);
      p.add(0xbc4).writeFloat(0.1);
      p.add(0xbc8).writeFloat(-6.0 + seed);
      p.add(0xbcc).writeFloat(1.0 / 3.0);
      p.add(0xbd0).writeFloat(1e-7);
      p.add(0xbd4).writeFloat(9.0 + seed);
      p.add(0xbd8).writeS32(1 + (seed % 5));
      p.add(0xbdc).writeS32(1 + ((seed * 3) % 5));
      for (let i = 0; i < 5; ++i) p.add(0xbe0 + i * 2).writeS16(900 * (i + 1) - seed * 31);
      for (let i = 0; i < 5; ++i) p.add(0xbec + i * 4).writeFloat(0.5 * (i + 1) - seed * 0.07);
      return p;
    };

    const K = {
      ramp: (i) => -0.85 + i * 0.17,
      alt: (i) => (i & 1 ? 0.6 : -0.6),
      third: () => 1.0 / 3.0,
      small: (i) => 0.02 * (i + 1),
      steep: (i) => 0.9 / (i + 1),
    };
    const HIST = {
      spread: (j) => [-14, 0, 14][j % 3] + (j % 2),
      tight: (j) => (j % 3) - 1 + (((j / 3) | 0) % 2),
      ramp: (j) => ((j * 5) % 31) - 15,
      wave: (j) => [14, 0, -14][j % 3] + (j % 2),
      deep: (j) => (j % 4 === 0 ? -16 : ((j * 3) % 15) - 7),
    };

    // (class 0..15, level 0..31, the ten slot values 4..13, record cells, history, gains, levels,
    //  synthesis seed)
    const V = [
      // slot values are the raw bit fields; the sign masks turn the high bit into a negative coefficient
      [0, 5, [1, 3, 7, 7, 7, 7, 15, 15, 15, 15], [7, 0, 0, 0, 9, 3000], HIST.spread, [3, 9, 20], [4, 17, 29],
       1, 0, 45, 1.0, K.ramp, 0, 1],
      [1, 0, [0, 0, 0, 0, 0, 0, 0, 0, 0, 0], [2, 0, 0, 0, 5, 100], HIST.tight, [1, 2, 3], [10, 11, 12],
       2, 1, 60, 2.0, K.alt, 0, 1],
      [2, 31, [3, 7, 15, 15, 15, 15, 31, 31, 31, 31], [31, 0, 0, 0, 6, 1500], HIST.ramp, [8, 8, 8],
       [31, 0, 15], 3, 2, 50, 1.5, K.third, 0, 1],
      [3, 17, [2, 4, 8, 9, 10, 11, 16, 17, 18, 19], [4, 0, 1, 2, 7, 500], HIST.spread, [0, 25, 7], [2, 20, 9],
       4, 0, 45, 1.0, K.steep, 0, 1],
      [4, 1, [1, 1, 1, 1, 1, 1, 1, 1, 1, 1], [11, 0, 1, 0, 8, 3000], HIST.tight, [5, 6, 7], [1, 1, 1],
       5, 0, 45, 1.0, K.ramp, 0, 0],
      [5, 23, [2, 5, 9, 3, 12, 6, 20, 9, 25, 4], [0, 0, 0, 3, 3, 100], HIST.ramp, [12, 3, 18], [7, 23, 3],
       6, 0, 45, 1.0, K.alt, 0, 0],
      [7, 8, [3, 0, 14, 1, 13, 2, 30, 1, 29, 2], [19, 0, 1, 3, 2, 500], HIST.spread, [30, 2, 11], [13, 6, 28],
       7, 4, 60, 3.0, K.small, 0, 0],
      [8, 16, [0, 6, 4, 12, 5, 11, 8, 24, 7, 23], [6, 0, 0, 2, 1, 1500], HIST.tight, [9, 9, 10], [16, 16, 17],
       8, 0, 30, 2.0, K.steep, 0, 0],
      [9, 11, [1, 2, 3, 4, 5, 6, 7, 8, 9, 10], [15, 0, 1, 2, 4, 3000], HIST.ramp, [21, 4, 13], [25, 11, 2],
       9, 2, 30, 1.0, K.ramp, 0, 0],
      [11, 29, [3, 3, 15, 0, 15, 0, 31, 0, 31, 0], [1, 0, 0, 0, 10, 100], HIST.spread, [2, 27, 6], [3, 29, 14],
       10, 1, 40, 1.0, K.alt, 0, 0],
      [12, 4, [2, 7, 8, 8, 8, 8, 16, 16, 16, 16], [28, 0, 1, 0, 12, 3000], HIST.wave, [14, 15, 16],
       [20, 21, 22], 11, 1, 200, 4.0, K.third, 40, 0],
      // the carried sample count stays at or above 0: with -170 carried over, interpSubframeParams cuts the
      // frame into more entries than codecSynthStage's own per-entry arrays hold and the ORIGINAL faults
      // (measured on instances\c3bv, log/c3/alt/c3bv_probe.py)
      [13, 26, [1, 5, 6, 10, 3, 13, 12, 27, 5, 18], [3, 0, 0, 2, 2, 500], HIST.ramp, [7, 18, 1], [9, 4, 26],
       12, 4, 100, 1.0, K.small, 0, 0],
      [14, 14, [0, 1, 2, 13, 4, 9, 11, 22, 3, 30], [22, 0, 1, 3, 6, 1500], HIST.deep, [26, 5, 17], [18, 30, 8],
       13, 0, 45, 1.0, K.ramp, 0, 1],
      [15, 21, [3, 4, 11, 5, 14, 1, 28, 13, 17, 6], [9, 0, 0, 3, 11, 3000], HIST.tight, [4, 4, 5], [6, 7, 8],
       14, 0, 45, 0.3, K.alt, 0, 0],
    ];

    const out = {};
    for (let i = 0; i < V.length; ++i) {
      const v = V[i];
      const slots = { 1: v[0], 2: v[1] };
      for (let k = 0; k < 10; ++k) slots[4 + k] = v[2][k];
      out['bpk' + i] = F.c3bv_bits(slots);
      out['bctx' + i] = decCtx(v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10], v[11], v[12], v[13], v[7]);
      const o = __keepAlloc(512 * 4);
      F.c3bv_floats(o, 512, (j) => -4.5 - j);
      out['bout' + i] = o;
    }
    return out;
  },

  // ---------------------------------------------------------------------------------------------------
  // the encoder context, shared by codecFeatureDecision, codecStageDispatch and codecProcessResource
  // ---------------------------------------------------------------------------------------------------
  // opt fields: seed, lo, hi, lag, jumps[], w[], u[], v[], t[], x[], cells{} (the +0x2224.. block),
  // wave (a function of the sample index used for pa, qa, sa and ra).
  // Returns the BASE of a 0x3600 block whose context starts at +0x800: codecStageDispatch's chain writes
  // outside the 0x2600 object (running the ORIGINAL twice over a byte-exact restore of the object alone
  // kills the process, log/c3/alt/c3bv_probe.py --arm orig2), so the A/B snapshots 0x800 bytes of slack on
  // each side as well and the registry's state region is the whole block.
  c3bv_enc: function (opt) {
    const F = globalThis.DIFF_FIXTURES;
    const blk = __keepAlloc(0x3600);
    blk.writeByteArray(new Uint8Array(0x3600).buffer);
    const p = blk.add(0x800);
    const seed = opt.seed;
    const wave = opt.wave;
    F.c3bv_floats(p.add(0x10), 540, (k) => wave(0xb5 + k));            // pa, elements 0xb5..0x2d0
    F.c3bv_floats(p.add(0x880), 540, (k) => wave(0xb5 + k) * 0.75);    // qa
    F.c3bv_floats(p.add(0x10f0), 696, (k) => wave(0x19 + k) * 0.5);    // sa, elements 0x19..0x2d0
    F.c3bv_floats(p.add(0x1bd0), 312, (k) => wave(0xe5 + k) * 0.25);   // ra, elements 0xe5..0x21c
    p.add(0x20b0).writeFloat(opt.offset);
    F.c3bv_ints(p.add(0x20b4), 10, (k) => (k < opt.jumps.length ? opt.jumps[k] : 0));
    p.add(0x20dc).writeS32(opt.jumpCount);
    F.c3bv_ints(p.add(0x20e0), 3, (k) => opt.w[k]);
    F.c3bv_ints(p.add(0x20ec), 6, (k) => opt.u[k]);
    F.c3bv_ints(p.add(0x2104), 6, (k) => opt.v[k]);
    F.c3bv_ints(p.add(0x211c), 8, (k) => opt.t[k]);
    F.c3bv_ints(p.add(0x213c), 3, (k) => opt.x[k]);
    F.c3bv_floats(p.add(0x2148), 30, (k) => 0.2 - 0.03 * k + seed * 0.011);
    p.add(0x21c0).writeFloat(0.125 * seed);
    // codecMarkCorrelationJumps' own state (+0x21c4..+0x2220), seeded so its ring and running sum are defined
    p.add(0x21c4).writeFloat(120.0 + seed);
    p.add(0x21c8).writeFloat(900.0 + seed * 7);
    p.add(0x21cc).writeFloat(0.3);
    F.c3bv_floats(p.add(0x21d0), 16, (k) => 0.05 * k + seed * 0.01);
    p.add(0x2210).writeFloat(1.1 + seed * 0.02);
    p.add(0x2214).writeS32(seed % 16);
    p.add(0x2218).writeS32((seed * 3) % 16);
    p.add(0x221c).writeS32(0);
    p.add(0x2220).writeS32(0);
    // codecFeatureDecision's cells
    const c = opt.cells;
    p.add(0x2224).writeFloat(c.A);
    p.add(0x2228).writeFloat(c.B);
    p.add(0x222c).writeFloat(c.C);
    for (let k = 0; k < 6; ++k) p.add(0x2230 + k * 4).writeFloat(c.D[k]);
    p.add(0x2248).writeS32(c.E);
    p.add(0x224c).writeS32(c.F);
    p.add(0x2250).writeS32(c.G);
    p.add(0x2254).writeS32(c.H);
    p.add(0x2258).writeS32(c.I);
    p.add(0x225c).writeS32(c.K);
    p.add(0x2260).writeS32(c.J);
    p.add(0x2264).writeS32(c.L);
    // codecGainSmoother's window (c3bi's layout: 0x3c costs, two 0x3c back-pointer rows, row, level)
    F.c3bv_floats(p.add(0x2268), 0x3c, (k) => 10.0 + ((k * 7 + seed) % 23));
    F.c3bv_ints(p.add(0x2358), 0x3c, (k) => 1 + ((k + seed) % 0x3b));
    F.c3bv_ints(p.add(0x2358 + 0x3c * 4), 0x3c, (k) => 1 + ((k * 3 + seed) % 0x3b));
    p.add(0x2538).writeS32(1 + (seed % 2));
    p.add(0x253c).writeFloat(8.0 + seed);
    p.add(0x2540).writeS32(seed & 1);        // bitPermute's toggle
    return blk;
  },

  // ---------------------------------------------------------------------------------------------------
  // 0x1003f720 codecFeatureDecision
  // ---------------------------------------------------------------------------------------------------
  // One vector per branch group. The window range (lo, hi) is kept inside pa's 0xb5..0x2d0 and sa's
  // 0x19..0x2d0 with room for the lag on both sides, because codecWindowFeatures rebases the first array by
  // the descriptor's element 1 (0xb5) and the second by its element 3 (0x19) at 0x1004105f and 0x10041076 and
  // then reads first[i-1], first[i], second[i] and second[i +/- lag] for i from `lo + (half-1)*len/2 + 1`
  // to `lo + half*len/2` (0x100410cf-0x1004110e).
  // The band is chosen through the three count cells: the selector is `*H * floor((*G/max(*H,1) + *B)*63/64)
  // / max(*F,1)` (0x1003f942-0x1003f9b6) and the descending thresholds at 0x10064e10 are 600, 450, 300 and
  // 200 with a row count of 5, so the five bands are reachable by moving *B alone.
  // The score sign is driven by feature 6, which is dword 1 of the block argument (0x1003fa55): its weight is
  // -654, -1021, -1270 or -1300 depending on the band (0x10064c54 + band*40 + 24), so +-200 there moves the
  // score by more than every other term together.
  c3bv_decide() {
    const m = Process.getModuleByName('sound.dll').base;
    m.add(0x000b58a0).writeS32(10);
    m.add(0x000b58a4).writeS32(0xb4);
    const F = globalThis.DIFF_FIXTURES;

    const icell = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    const fcell = (v) => { const p = __keepAlloc(4); p.writeFloat(v); return p; };
    // the window descriptor, the same shape as the DLL's own at 0x10064a04
    const desc = __keepAlloc(8 * 4);
    F.c3bv_ints(desc, 8, (k) => [0xb5, 0x2d0, 0x19, 0x2d0, 0, 1, 8, -16][k]);

    const WAVE = {
      sine: (i) => Math.sin(i * 0.17) * 900.0,
      noisy: (i) => ((i * 2654435761) % 2048) - 1024.0,
      quiet: (i) => Math.sin(i * 0.41) * 0.5,
      loud: (i) => (i % 2 ? 30000.0 : -29000.0),
      third: (i) => (i % 3 === 0 ? 1.0 / 3.0 : -1e-7),
      ramp: (i) => (i % 97) * 11.0 - 500.0,
    };

    // (half, code pieces of[3..6], of[1], of[7], E, F, G, H, I, J, K, L, B, feature6, divisor, numerator,
    //  wave, lo, hi, lag, in-flag triple, D history)
    const V = [
      // --- half 1: the history shift and the feature-1 division (0x1003f85d-0x1003f8c6)
      {k: 'h1_big', half: 1, of: [0, 0, 0, 0], of1: 0, of7: 0, E: 1000, Fc: 1000, G: 0, H: 1000,
       I: 400, J: 400, K: 600, L: 600, B: 700.0, f6: -200.0, div: 4.0, num: 9.0, wave: WAVE.sine,
       lo: 0x120, hi: 0x220, lag: 0x30, inf: [0, 0, 0], D: [1, 2, 3, 4, 5, 6]},
      {k: 'h1_divclamp', half: 1, of: [0, 0, 0, 0], of1: 0, of7: 1, E: 0, Fc: 0, G: 7, H: 0,
       I: 0, J: 0, K: 0, L: 0, B: 102.0, f6: 200.0, div: 0.25, num: -3.0, wave: WAVE.quiet,
       lo: 0x100, hi: 0x180, lag: 0x10, inf: [0, 0, 0], D: [-1, -2, -3, -4, -5, -6]},
      {k: 'h1_band3', half: 1, of: [0, 0, 0, 0], of1: 1, of7: 0, E: 5, Fc: 1000, G: 50, H: 1000,
       I: 30, J: 30, K: 60, L: 60, B: 356.0, f6: 0.0, div: 1.0, num: 1.0 / 3.0, wave: WAVE.third,
       lo: 0x1c0, hi: 0x240, lag: 0x40, inf: [0, 0, 0], D: [0, 0, 0, 0, 0, 0]},
      {k: 'h1_band5', half: 1, of: [0, 0, 0, 0], of1: 0, of7: 0, E: 1, Fc: 1, G: 0, H: 0,
       I: 1, J: 1, K: 1, L: 1, B: 0.0, f6: 120.0, div: 1e-7, num: 3e5, wave: WAVE.loud,
       lo: 0x0c0, hi: 0x140, lag: 0x08, inf: [1, 1, 1], D: [7, -7, 7, -7, 7, -7]},
      // --- half 2: one vector per value of the four-bit code (0x1003fb5f-0x1003fbb3)
      {k: 'c0', half: 2, of: [0, 0, 0, 0], of1: 0, of7: 0, E: 1000, Fc: 1000, G: 10, H: 1000,
       I: 400, J: 400, K: 600, L: 600, B: 700.0, f6: -200.0, div: 2.0, num: 5.0, wave: WAVE.sine,
       lo: 0x120, hi: 0x220, lag: 0x30, inf: [0, 0, 0], D: [1, 2, 3, 4, 5, 6]},
      {k: 'c1', half: 2, of: [0, 0, 0, 1], of1: 0, of7: 1, E: 900, Fc: 900, G: 20, H: 900,
       I: 300, J: 300, K: 500, L: 500, B: 508.0, f6: 200.0, div: 1.5, num: 4.0, wave: WAVE.noisy,
       lo: 0x130, hi: 0x230, lag: 0x20, inf: [2, 0, 0], D: [-3, 1, -4, 1, -5, 9]},
      {k: 'c2', half: 2, of: [0, 0, 1, 0], of1: 0, of7: 1, E: 800, Fc: 800, G: 30, H: 800,
       I: 250, J: 250, K: 450, L: 450, B: 356.0, f6: -200.0, div: 1.0, num: 2.0, wave: WAVE.ramp,
       lo: 0x140, hi: 0x200, lag: 0x18, inf: [0, 1, 0], D: [5.0, -1.0, -8.0, 2.0, 0.5, -0.5]},
      {k: 'c2b', half: 2, of: [0, 0, 1, 0], of1: 0, of7: 0, E: 700, Fc: 700, G: 40, H: 700,
       I: 200, J: 200, K: 400, L: 400, B: 254.0, f6: 200.0, div: 0.5, num: 1.0, wave: WAVE.quiet,
       lo: 0x150, hi: 0x1f0, lag: 0x12, inf: [0, 0, 1], D: [1.0, 1.0, 9.0, -2.0, 1.0, 1.0]},
      {k: 'c3', half: 2, of: [0, 0, 1, 1], of1: 1, of7: 0, E: 600, Fc: 600, G: 50, H: 600,
       I: 150, J: 150, K: 350, L: 350, B: 102.0, f6: -200.0, div: 3.0, num: 7.0, wave: WAVE.third,
       lo: 0x160, hi: 0x1e0, lag: 0x0c, inf: [1, 0, 0], D: [0, 0, 0, 0, 0, 0]},
      {k: 'c4', half: 2, of: [0, 1, 0, 0], of1: 0, of7: 0, E: 500, Fc: 500, G: 60, H: 500,
       I: 120, J: 120, K: 300, L: 300, B: 700.0, f6: 200.0, div: 1.0, num: 1.0, wave: WAVE.loud,
       lo: 0x170, hi: 0x1d0, lag: 0x0a, inf: [0, 1, 1], D: [2, 4, 6, 8, 10, 12]},
      {k: 'c5', half: 2, of: [0, 1, 0, 1], of1: 0, of7: 1, E: 400, Fc: 400, G: 70, H: 400,
       I: 100, J: 100, K: 250, L: 250, B: 508.0, f6: -200.0, div: 1.0, num: 9.0, wave: WAVE.sine,
       lo: 0x180, hi: 0x240, lag: 0x28, inf: [2, 1, 1], D: [-6.0, 7.0, -2.0, 3.0, 1.0, 1.0]},
      {k: 'c5b', half: 2, of: [0, 1, 0, 1], of1: 0, of7: 0, E: 350, Fc: 350, G: 80, H: 350,
       I: 90, J: 90, K: 200, L: 200, B: 356.0, f6: 200.0, div: 1.0, num: 1.0, wave: WAVE.noisy,
       lo: 0x190, hi: 0x250, lag: 0x30, inf: [0, 0, 0], D: [4.0, -9.0, 1.0, 1.0, 1.0, 1.0]},
      {k: 'c6', half: 2, of: [0, 1, 1, 0], of1: 1, of7: 0, E: 300, Fc: 300, G: 90, H: 300,
       I: 80, J: 80, K: 150, L: 150, B: 254.0, f6: -200.0, div: 1.0, num: 2.0, wave: WAVE.ramp,
       lo: 0x1a0, hi: 0x230, lag: 0x20, inf: [0, 0, 0], D: [1, 1, 1, 1, 1, 1]},
      {k: 'c6b', half: 2, of: [0, 1, 1, 0], of1: 0, of7: 0, E: 250, Fc: 250, G: 100, H: 250,
       I: 70, J: 70, K: 120, L: 120, B: 102.0, f6: 200.0, div: 1.0, num: 3.0, wave: WAVE.quiet,
       lo: 0x1b0, hi: 0x220, lag: 0x18, inf: [0, 0, 0], D: [-2.0, 0.0, 0.0, 9.0, 0.0, 0.0]},
      {k: 'c6c', half: 2, of: [0, 1, 1, 0], of1: 0, of7: 0, E: 240, Fc: 240, G: 110, H: 240,
       I: 60, J: 60, K: 110, L: 110, B: 700.0, f6: -200.0, div: 1.0, num: 4.0, wave: WAVE.third,
       lo: 0x1c0, hi: 0x240, lag: 0x18, inf: [0, 0, 0], D: [3.0, 0.0, 0.0, -9.0, 0.0, 0.0]},
      {k: 'c7', half: 2, of: [0, 1, 1, 1], of1: 0, of7: 1, E: 230, Fc: 230, G: 120, H: 230,
       I: 55, J: 55, K: 100, L: 100, B: 508.0, f6: 200.0, div: 1.0, num: 5.0, wave: WAVE.loud,
       lo: 0x100, hi: 0x1c0, lag: 0x10, inf: [2, 0, 0], D: [0, 0, 0, 0, 0, 0]},
      {k: 'c7b', half: 2, of: [0, 1, 1, 1], of1: 0, of7: 1, E: 220, Fc: 220, G: 130, H: 220,
       I: 50, J: 50, K: 95, L: 95, B: 356.0, f6: -200.0, div: 1.0, num: 6.0, wave: WAVE.sine,
       lo: 0x110, hi: 0x1d0, lag: 0x10, inf: [0, 0, 1], D: [0, 0, 0, 0, 0, 0]},
      {k: 'c8', half: 2, of: [1, 0, 0, 0], of1: 0, of7: 0, E: 210, Fc: 210, G: 140, H: 210,
       I: 45, J: 45, K: 90, L: 90, B: 254.0, f6: 200.0, div: 1.0, num: 7.0, wave: WAVE.noisy,
       lo: 0x120, hi: 0x1e0, lag: 0x14, inf: [0, 1, 0], D: [0, 0, 0, 0, 0, 0]},
      {k: 'c9', half: 2, of: [1, 0, 0, 1], of1: 0, of7: 0, E: 200, Fc: 200, G: 150, H: 200,
       I: 40, J: 40, K: 85, L: 85, B: 102.0, f6: -200.0, div: 1.0, num: 8.0, wave: WAVE.ramp,
       lo: 0x130, hi: 0x1f0, lag: 0x16, inf: [0, 0, 0], D: [0, 0, 0, 0, 0, 0]},
      {k: 'c10', half: 2, of: [1, 0, 1, 0], of1: 0, of7: 0, E: 190, Fc: 190, G: 160, H: 190,
       I: 35, J: 35, K: 80, L: 80, B: 700.0, f6: 200.0, div: 1.0, num: 9.0, wave: WAVE.quiet,
       lo: 0x140, hi: 0x200, lag: 0x18, inf: [0, 0, 0], D: [-8.0, 2.0, 1.0, 1.0, 1.0, 1.0]},
      {k: 'c10b', half: 2, of: [1, 0, 1, 0], of1: 0, of7: 0, E: 180, Fc: 180, G: 170, H: 180,
       I: 30, J: 30, K: 75, L: 75, B: 508.0, f6: -200.0, div: 1.0, num: 1.0, wave: WAVE.third,
       lo: 0x150, hi: 0x210, lag: 0x1a, inf: [0, 0, 0], D: [9.0, -2.0, 1.0, 1.0, 1.0, 1.0]},
      {k: 'c11', half: 2, of: [1, 0, 1, 1], of1: 0, of7: 0, E: 170, Fc: 170, G: 180, H: 170,
       I: 25, J: 25, K: 70, L: 70, B: 356.0, f6: 200.0, div: 1.0, num: 2.0, wave: WAVE.loud,
       lo: 0x160, hi: 0x220, lag: 0x1c, inf: [0, 0, 0], D: [0, 0, 0, 0, 0, 0]},
      {k: 'c12', half: 2, of: [1, 1, 0, 0], of1: 0, of7: 0, E: 160, Fc: 160, G: 190, H: 160,
       I: 20, J: 20, K: 65, L: 65, B: 254.0, f6: -200.0, div: 1.0, num: 3.0, wave: WAVE.sine,
       lo: 0x170, hi: 0x230, lag: 0x1e, inf: [0, 0, 0], D: [0, 0, 0, 0, 0, 0]},
      {k: 'c13', half: 2, of: [1, 1, 0, 1], of1: 0, of7: 0, E: 150, Fc: 150, G: 200, H: 150,
       I: 18, J: 18, K: 60, L: 60, B: 102.0, f6: 200.0, div: 1.0, num: 4.0, wave: WAVE.noisy,
       lo: 0x180, hi: 0x240, lag: 0x20, inf: [0, 0, 0], D: [1.0, 1.0, -9.0, -2.0, 1.0, 1.0]},
      {k: 'c13b', half: 2, of: [1, 1, 0, 1], of1: 0, of7: 1, E: 140, Fc: 140, G: 210, H: 140,
       I: 16, J: 16, K: 55, L: 55, B: 700.0, f6: -200.0, div: 1.0, num: 5.0, wave: WAVE.ramp,
       lo: 0x190, hi: 0x250, lag: 0x20, inf: [0, 0, 0], D: [0, 0, 0, 0, 0, 0]},
      {k: 'c14', half: 2, of: [1, 1, 1, 0], of1: 0, of7: 0, E: 130, Fc: 130, G: 220, H: 130,
       I: 14, J: 14, K: 50, L: 50, B: 508.0, f6: 200.0, div: 1.0, num: 6.0, wave: WAVE.quiet,
       lo: 0x1a0, hi: 0x240, lag: 0x1c, inf: [2, 0, 0], D: [0, 0, 0, 0, 0, 0]},
      {k: 'c15', half: 2, of: [1, 1, 1, 1], of1: 0, of7: 0, E: 120, Fc: 120, G: 230, H: 120,
       I: 12, J: 12, K: 45, L: 45, B: 356.0, f6: -200.0, div: 1.0, num: 7.0, wave: WAVE.third,
       lo: 0x1b0, hi: 0x230, lag: 0x18, inf: [0, 0, 0], D: [0, 0, 0, 0, 0, 0]},
      // --- the second side of every test inside a switch case
      // case 2, the other side of `jne 0x1003fc4c` at 0x1003fc3e: -D[3] <= D[2]
      {k: 'c2c', half: 2, of: [0, 0, 1, 0], of1: 0, of7: 1, E: 310, Fc: 310, G: 44, H: 310,
       I: 33, J: 33, K: 88, L: 88, B: 356.0, f6: -200.0, div: 1.0, num: 2.0, wave: WAVE.sine,
       lo: 0x140, hi: 0x200, lag: 0x18, inf: [0, 0, 0], D: [0, 0, 9.0, -5.0, 0, 0]},
      // case 6, the other side of `je 0x1003fcbe` at 0x1003fcaa: of[7] == 1 with of[1] != 1
      {k: 'c6d', half: 2, of: [0, 1, 1, 0], of1: 0, of7: 1, E: 320, Fc: 320, G: 46, H: 320,
       I: 34, J: 34, K: 86, L: 86, B: 254.0, f6: 200.0, div: 1.0, num: 3.0, wave: WAVE.ramp,
       lo: 0x1a0, hi: 0x230, lag: 0x20, inf: [0, 0, 0], D: [1, 1, 1, 1, 1, 1]},
      // case 8, the other side of `je 0x1003fcfe` at 0x1003fcf2: the gate clear
      {k: 'c8b', half: 2, of: [1, 0, 0, 0], of1: 0, of7: 0, E: 330, Fc: 330, G: 48, H: 330,
       I: 36, J: 36, K: 84, L: 84, B: 102.0, f6: -200.0, div: 1.0, num: 4.0, wave: WAVE.noisy,
       lo: 0x120, hi: 0x1e0, lag: 0x14, inf: [0, 0, 0], D: [0, 0, 0, 0, 0, 0]},
      // case 13, the other side of `jne 0x1003fd66` at 0x1003fd58: -D[2] <= D[3] with of[7] == 0
      {k: 'c13c', half: 2, of: [1, 1, 0, 1], of1: 0, of7: 0, E: 340, Fc: 340, G: 52, H: 340,
       I: 38, J: 38, K: 82, L: 82, B: 700.0, f6: 200.0, div: 1.0, num: 5.0, wave: WAVE.quiet,
       lo: 0x180, hi: 0x240, lag: 0x20, inf: [0, 0, 0], D: [0, 0, 5.0, 9.0, 0, 0]},
      // case 14, the gate clear (`je 0x1003fd8b` at 0x1003fd76)
      {k: 'c14b', half: 2, of: [1, 1, 1, 0], of1: 0, of7: 0, E: 350, Fc: 350, G: 54, H: 350,
       I: 40, J: 40, K: 80, L: 80, B: 508.0, f6: -200.0, div: 1.0, num: 6.0, wave: WAVE.third,
       lo: 0x1a0, hi: 0x240, lag: 0x1c, inf: [0, 0, 0], D: [0, 0, 0, 0, 0, 0]},
      // case 14, the gate set and of[7] non-zero (`jne 0x1003fd8b` at 0x1003fd7f)
      {k: 'c14c', half: 2, of: [1, 1, 1, 0], of1: 0, of7: 1, E: 360, Fc: 360, G: 56, H: 360,
       I: 42, J: 42, K: 78, L: 78, B: 356.0, f6: 200.0, div: 1.0, num: 7.0, wave: WAVE.loud,
       lo: 0x1a0, hi: 0x240, lag: 0x1c, inf: [2, 0, 0], D: [0, 0, 0, 0, 0, 0]},
      // the default arm with the gate clear (`je 0x1003fc1d` at 0x1003fc08)
      {k: 'cdef1', half: 2, of: [0, 0, 0, 1], of1: 0, of7: 1, E: 370, Fc: 370, G: 58, H: 370,
       I: 44, J: 44, K: 76, L: 76, B: 254.0, f6: -200.0, div: 1.0, num: 8.0, wave: WAVE.sine,
       lo: 0x130, hi: 0x230, lag: 0x20, inf: [0, 0, 0], D: [0, 0, 0, 0, 0, 0]},
      // a code above 15 (`ja 0x1003fc04` at 0x1003fbab); of[7] clear, so the default arm writes nothing
      // through `jne 0x1003fc1d` at 0x1003fc11
      {k: 'cbig', half: 2, of: [2, 0, 0, 0], of1: 0, of7: 0, E: 110, Fc: 110, G: 240, H: 110,
       I: 10, J: 10, K: 40, L: 40, B: 254.0, f6: 200.0, div: 1.0, num: 8.0, wave: WAVE.loud,
       lo: 0x1c0, hi: 0x240, lag: 0x14, inf: [2, 0, 0], D: [0, 0, 0, 0, 0, 0]},
      // *I and *J large enough that the two `jg` of the min selections take their other side
      {k: 'capped', half: 2, of: [0, 0, 0, 0], of1: 0, of7: 0, E: 100, Fc: 100, G: 250, H: 100,
       I: 100000, J: 100000, K: 35, L: 35, B: 102.0, f6: -200.0, div: 1.0, num: 9.0, wave: WAVE.sine,
       lo: 0x1d0, hi: 0x250, lag: 0x10, inf: [0, 0, 0], D: [0, 0, 0, 0, 0, 0]},
      // *E and *F large enough that the final level passes 20.0 (`je 0x1003ff8c` at 0x1003ff7f)
      {k: 'hilevel', half: 2, of: [0, 0, 0, 0], of1: 0, of7: 0, E: 900000, Fc: 900000, G: 5, H: 5,
       I: 5, J: 5, K: 5, L: 5, B: 700.0, f6: 200.0, div: 1.0, num: 1.0, wave: WAVE.noisy,
       lo: 0x1e0, hi: 0x260, lag: 0x10, inf: [0, 0, 0], D: [0, 0, 0, 0, 0, 0]},
    ];

    const out = {d_desc: desc};
    for (let i = 0; i < V.length; ++i) {
      const v = V[i];
      const cells = {A: 1.5 + i * 0.25, B: v.B, C: 0.5 - i * 0.03, D: v.D,
                     E: v.E, F: v.Fc, G: v.G, H: v.H, I: v.I, J: v.J, K: v.K, L: v.L};
      out['dblk_ctx_' + v.k] = F.c3bv_enc({seed: i + 1, wave: v.wave, offset: 1.25 * i,
                                       jumps: [0xc0, 0x140, 0x1a0, 0, 0, 0, 0, 0, 0, 0], jumpCount: 4,
                                       w: [1, 2, 3], u: [0xb8, 0x118, 0xc0, 0x120, v.lo, v.hi],
                                       v: [0xc0, 0x130, 0xc8, 0x138, 0xd0, 0x140],
                                       t: [1, 2, 1, 2, 1, 2, 1, 2], x: [0, 1, 2], cells: cells});
      out['dctx_' + v.k] = out['dblk_ctx_' + v.k].add(0x800);
      const range = __keepAlloc(8);
      range.writeS32(v.lo); range.add(4).writeS32(v.hi);
      out['drange_' + v.k] = range;
      out['dhalf_' + v.k] = icell(v.half);
      out['ddiv_' + v.k] = fcell(v.div);
      out['dnum_' + v.k] = fcell(v.num);
      out['dlag_' + v.k] = icell(v.lag);
      const blk = __keepAlloc(8 * 4);
      F.c3bv_ints(blk, 8, () => 0);
      blk.add(4).writeFloat(v.f6);                 // dword 1: feature 6
      out['dblk_' + v.k] = blk;
      const inf = __keepAlloc(8 * 4);
      F.c3bv_ints(inf, 8, (k) => (k < 3 ? v.inf[k] : 0));
      out['dinf_' + v.k] = inf;
      const ofl = __keepAlloc(16 * 4);
      // of[k] of the body is dword k-1 here: of[1] = dword 0, of[3..6] = dwords 2..5, of[7] = dword 6
      F.c3bv_ints(ofl, 16, (k) => {
        if (k === 0) return v.of1;
        if (k >= 2 && k <= 5) return v.of[k - 2];
        if (k === 6) return v.of7;
        return 0;
      });
      out['dofl_' + v.k] = ofl;
    }
    return out;
  },

  // ---------------------------------------------------------------------------------------------------
  // 0x1003c640 codecStageDispatch and 0x1003b150 codecProcessResource
  // ---------------------------------------------------------------------------------------------------
  // Both run the whole analyser over one encoder context, so they share a builder. The seeded indices stay
  // inside the histories: the jump list and the u/v segment bounds are sample indices in 0xb5..0x2d0, the
  // jump count is at most 10 (the array at +0x20b4 holds ten one-based slots, 0x1003c8eb) and the v segment
  // is under 180 samples long, which is the most codecRemoveMean 0x1003ed00 can write into the
  // [ebp-0x368] block of codecStageDispatch's own frame (0x1003ceee).
  c3bv_stage_common: function (prefix) {
    const m = Process.getModuleByName('sound.dll').base;
    m.add(0x000b58a0).writeS32(10);
    m.add(0x000b58a4).writeS32(0xb4);
    m.add(0x000b58a8).writeS32(1);
    const F = globalThis.DIFF_FIXTURES;

    const WAVE = {
      sine: (i) => Math.sin(i * 0.17) * 900.0,
      voiced: (i) => Math.sin(i * 0.08) * 4000.0 + Math.sin(i * 0.33) * 600.0,
      noisy: (i) => ((i * 2654435761) % 4096) - 2048.0,
      quiet: (i) => Math.sin(i * 0.41) * 0.5,
      loud: (i) => (i % 2 ? 12000.0 : -11000.0),
      third: (i) => (i % 3 === 0 ? 1.0 / 3.0 : -1e-7),
      ramp: (i) => (i % 97) * 11.0 - 500.0,
      zero: () => 0.0,
    };

    // (key, wave, offset cell, jump list, jump count, w, u, v, t, x, input sample wave)
    const V = [
      ['a', WAVE.voiced, 0.0, [0xc0, 0x150, 0x1e0, 0x260, 0, 0, 0, 0, 0, 0], 5, [1, 2, 3],
       [0xb8, 0x118, 0xc0, 0x120, 0x160, 0x200], [0xc0, 0x130, 0xc8, 0x138, 0x200, 0x250],
       [1, 2, 1, 2, 1, 2, 1, 2], [0, 1, 2], WAVE.voiced],
      ['b', WAVE.sine, 12.5, [0xd0, 0x160, 0x200, 0, 0, 0, 0, 0, 0, 0], 4, [0, 1, 2],
       [0xc0, 0x130, 0xd0, 0x140, 0x180, 0x210], [0xd0, 0x140, 0xd8, 0x148, 0x1c0, 0x240],
       [2, 1, 2, 1, 2, 1, 2, 1], [1, 0, 1], WAVE.sine],
      ['c', WAVE.noisy, -8.0, [0x40, 0x120, 0x190, 0x210, 0x280, 0, 0, 0, 0, 0], 6, [2, 0, 1],
       [0xb6, 0x116, 0xbe, 0x11e, 0x140, 0x1c0], [0xb8, 0x128, 0xc0, 0x130, 0x1a0, 0x220],
       [1, 1, 2, 2, 3, 3, 1, 1], [2, 2, 0], WAVE.noisy],
      ['d', WAVE.quiet, 0.5, [0xf0, 0x1a0, 0, 0, 0, 0, 0, 0, 0, 0], 3, [1, 1, 1],
       [0xd0, 0x140, 0xe0, 0x150, 0x1a0, 0x230], [0xe0, 0x150, 0xe8, 0x158, 0x1d0, 0x248],
       [0, 1, 0, 1, 0, 1, 0, 1], [0, 0, 1], WAVE.quiet],
      ['e', WAVE.loud, 400.0, [0xc8, 0x138, 0x1c8, 0x238, 0, 0, 0, 0, 0, 0], 5, [3, 2, 1],
       [0xc8, 0x128, 0xd8, 0x138, 0x1b0, 0x240], [0xc8, 0x138, 0xd0, 0x140, 0x1e0, 0x258],
       [2, 2, 1, 1, 2, 2, 1, 1], [1, 2, 1], WAVE.loud],
      ['f', WAVE.third, -0.125, [0xb5, 0x2d0, 0, 0, 0, 0, 0, 0, 0, 0], 3, [1, 0, 2],
       [0xb8, 0x118, 0xc8, 0x128, 0x100, 0x190], [0xc0, 0x130, 0xc8, 0x138, 0x160, 0x1f0],
       [1, 0, 2, 1, 3, 2, 1, 0], [0, 1, 0], WAVE.third],
      ['g', WAVE.ramp, 3000.0, [0x20, 0x60, 0xa0, 0xf0, 0x100, 0x110, 0x120, 0x130, 0x140, 0x150], 11,
       [2, 1, 0], [0xbe, 0x11e, 0xce, 0x12e, 0x1c0, 0x250], [0xd8, 0x148, 0xe0, 0x150, 0x210, 0x260],
       [3, 1, 2, 2, 1, 3, 2, 2], [2, 1, 0], WAVE.ramp],
      ['h', WAVE.zero, 0.0, [0, 0, 0, 0, 0, 0, 0, 0, 0, 0], 1, [0, 0, 0],
       [0xb6, 0x116, 0xb6, 0x116, 0x0c0, 0x140], [0xc0, 0x130, 0xc0, 0x130, 0x180, 0x200],
       [0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 0], WAVE.zero],
      ['i', WAVE.voiced, -3000.0, [0x140, 0x1e0, 0x270, 0, 0, 0, 0, 0, 0, 0], 4, [1, 3, 2],
       [0xc4, 0x124, 0xd4, 0x134, 0x190, 0x220], [0xcc, 0x13c, 0xd4, 0x144, 0x1f0, 0x268],
       [2, 3, 1, 2, 3, 1, 2, 3], [1, 1, 1], WAVE.sine],
      ['j', WAVE.sine, 1.0 / 3.0, [0xb9, 0x151, 0x1e9, 0, 0, 0, 0, 0, 0, 0], 4, [0, 2, 1],
       [0xba, 0x11a, 0xca, 0x12a, 0x150, 0x1d0], [0xc2, 0x132, 0xca, 0x13a, 0x1b0, 0x230],
       [1, 2, 3, 1, 2, 3, 1, 2], [0, 2, 1], WAVE.noisy],
      ['k', WAVE.noisy, 1e-7, [0x200, 0x240, 0x280, 0x2c0, 0, 0, 0, 0, 0, 0], 5, [2, 2, 2],
       [0xc6, 0x126, 0xd6, 0x136, 0x1d0, 0x258], [0xd6, 0x146, 0xde, 0x14e, 0x220, 0x270],
       [3, 3, 3, 3, 3, 3, 3, 3], [2, 0, 2], WAVE.voiced],
      ['l', WAVE.quiet, 20000.0, [0xc0, 0x1c0, 0, 0, 0, 0, 0, 0, 0, 0], 3, [1, 2, 0],
       [0xbc, 0x11c, 0xcc, 0x12c, 0x170, 0x1f8], [0xc4, 0x134, 0xcc, 0x13c, 0x1c8, 0x248],
       [0, 2, 0, 2, 0, 2, 0, 2], [1, 0, 2], WAVE.loud],
      ['m', WAVE.loud, -20000.0, [0xb5, 0x140, 0x1d0, 0x2d0, 0, 0, 0, 0, 0, 0], 5, [3, 0, 3],
       [0xb8, 0x118, 0xd8, 0x138, 0x200, 0x280], [0xe0, 0x158, 0xe8, 0x160, 0x230, 0x288],
       [2, 0, 2, 0, 2, 0, 2, 0], [0, 1, 2], WAVE.ramp],
      ['n', WAVE.ramp, 64.0, [0x90, 0x1b0, 0x230, 0, 0, 0, 0, 0, 0, 0], 4, [0, 3, 1],
       [0xc2, 0x122, 0xd2, 0x132, 0x138, 0x1b8], [0xca, 0x13a, 0xd2, 0x142, 0x198, 0x218],
       [1, 3, 2, 0, 1, 3, 2, 0], [2, 2, 2], WAVE.third],
    ];

    const out = {};
    for (let i = 0; i < V.length; ++i) {
      const v = V[i];
      const key = prefix + v[0];
      out['blk_' + key] = F.c3bv_enc({
        seed: i + 1, wave: v[1], offset: v[2], jumps: v[3], jumpCount: v[4], w: v[5], u: v[6], v: v[7],
        t: v[8], x: v[9],
        cells: {A: 2.0 + i, B: 120.0 + i * 37.0, C: 0.4 + i * 0.05,
                D: [1.0 - i, -2.0 + i, 3.0, -4.0, 5.0, -6.0],
                E: 400 + i * 11, F: 380 + i * 13, G: 50 + i * 3, H: 420 + i * 7,
                I: 90 + i, J: 95 + i, K: 300 + i * 5, L: 310 + i * 5}});
      out['ctx_' + key] = out['blk_' + key].add(0x800);
      // the frame of new samples: one-based, so the function reads dwords 0..0xb3 after its own decrement.
      // 0x800 bytes of slack on each side, inside the compared state, for the same reason as the context.
      const sblk = __keepAlloc(0x800 + 1024 * 4 + 0x800);
      sblk.writeByteArray(new Uint8Array(0x800 + 1024 * 4 + 0x800).buffer);
      const s = sblk.add(0x800);
      F.c3bv_floats(s, 1024, (j) => v[10](j) / 4096.0);
      out['sblk_' + key] = sblk;
      out['samples_' + key] = s;
    }
    return out;
  },

  // codecStageDispatch: the four out blocks of the caller plus the shared contexts and sample frames.
  c3bv_stage() {
    const F = globalThis.DIFF_FIXTURES;
    const out = F.c3bv_stage_common('s');
    const keys = 'abcdefghijklmn'.split('');
    for (let i = 0; i < keys.length; ++i) {
      const key = 's' + keys[i];
      const pair = __keepAlloc(8);
      pair.writeS32(-0x5a5a5a5a); pair.add(4).writeS32(-0x5a5a5a5a);
      out['pair_' + key] = pair;
      const idx = __keepAlloc(4); idx.writeS32(-0x5a5a5a5a); out['idx_' + key] = idx;
      const lvl = __keepAlloc(4); lvl.writeFloat(-7.25); out['lvl_' + key] = lvl;
      const co = __keepAlloc(16 * 4);
      F.c3bv_floats(co, 16, (j) => -3.5 - j);
      out['coeff_' + key] = co;
    }
    return out;
  },

  // codecProcessResource: the shared contexts and sample frames plus one packed-word block per vector
  // (bitPack writes its bits one-based after the `sub ecx, 4` at 0x1003bf5a, so dwords 0..52 of the block).
  c3bv_resource() {
    const F = globalThis.DIFF_FIXTURES;
    const out = F.c3bv_stage_common('r');
    const keys = 'abcdefghijklmn'.split('');
    for (let i = 0; i < keys.length; ++i) {
      const key = 'r' + keys[i];
      const bits = __keepAlloc(64 * 4);
      F.c3bv_ints(bits, 64, (j) => -0x3b3b0000 + j);
      out['bits_' + key] = bits;
    }
    return out;
  },
});
