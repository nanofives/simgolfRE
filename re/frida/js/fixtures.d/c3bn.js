// Fixtures for C3 batch c3bn (sound.dll: three speech-codec functions and one sequencer function). Every
// block is allocated here: no live sound object, device, list, DirectSound buffer or MIDI stream is ever handed
// to an arm, and none of the functions under test allocates or frees. Layouts come from the disassembly cited
// in shim/src/re/c3bn.cpp. All values are synthetic.
//
// ONE MODULE GLOBAL IS WRITTEN HERE: g_codecOrder 0x100b58a0, which lpcSynthesize reads as its filter order
// (0x1003d2cc and five more sites). Its only writer in the whole DLL is codecSetParams 0x1003aa50, which
// stores 10; at the menu the global is 0 and neither codecSetParams nor any codec function is entered
// (log/c3/alt/c3bn_probe.py: 0 entries and 0 changes in 15 s). The fixture stores the same 10 so that the
// order is the one the codec runs with. Neither arm writes it.
Object.assign(globalThis.DIFF_FIXTURES, {

  // --- 0x1003d8d0 reflectionToLpc: reflection-coefficient arrays (one-based, so slot 0 is never read), one
  // LPC output buffer and one gain cell per vector, order cells and amplitude cells.
  // Every |k| stays below 1 so that the running product 1 - k*k never turns negative and the sqrt at
  // 0x1003d95f keeps a defined argument. Orders are 0, 1, 2, 3, 4, 6 and 8: the work array at
  // [ebp + j*4 - 0x34] only reaches the frame slots [ebp-8] and [ebp-4] from order 12 upwards, which no vector
  // uses. The coefficients include values whose products are not exact in float (0.1, 1/3, 1e-7).
  c3bn_reflection() {
    const cell = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    const fcell = (v) => { const p = __keepAlloc(4); p.writeFloat(v); return p; };
    const farr = (fn) => {
      const p = __keepAlloc(0x80);
      for (let i = 0; i < 32; ++i) p.add(i * 4).writeFloat(fn(i));
      return p;
    };
    const out = {
      o0: cell(0), o1: cell(1), o2: cell(2), o3: cell(3),
      o4: cell(4), o6: cell(6), o8: cell(8),
      amp1: fcell(1.0), ampTiny: fcell(1e-7), ampBig: fcell(300000.0), amp1234: fcell(1234.5),
      // k[i] is read one-based, so entry 0 of each array is never used
      k_ramp: farr((i) => i * 0.1 - 0.55),
      k_small: farr((i) => (i & 1 ? 0.1 : -0.1)),
      k_third: farr(() => 1.0 / 3.0),
      k_alt: farr((i) => (i & 1 ? 0.7 : -0.7)),
      k_tiny: farr(() => 1e-7),
      k_big: farr((i) => (i & 1 ? 0.99 : -0.99)),
      k_zero: farr(() => 0.0),
    };
    for (let i = 0; i < 14; ++i) {
      out['a' + i] = farr((j) => 0.5 + j * 0.25 + i);   // slots above the order keep this value
      out['g' + i] = fcell(-7.5);                        // overwritten with 1.0f at 0x1003d900
    }
    return out;
  },

  // --- 0x1003d1d0 lpcSynthesize: codec contexts, LPC coefficient arrays, sub-frame lengths, excitation modes,
  // output buffers and the three level / feedback cells.
  // A context is 0x1000 bytes with: the previous sub-frame length at +0x688, the excitation array at +0x68c
  // (one-based, indices 1..order+subLen), the synthesis history at +0x924 (same), the two three-tap delay
  // lines at +0xbbc..+0xbc4 and +0xbc8..+0xbd0, the previous level at +0xbd4 and codecCombFilter's state at
  // +0xbd8 (cursor A), +0xbdc (cursor B) and +0xbe0 (five 16-bit entries); both cursors must be 1..5, which is
  // the range codecCombFilter itself keeps them in (0x10040fe5, 0x10041003).
  // The coefficient arrays keep sum|a| below 1 so that the two recursive passes at 0x1003d6e7 and 0x1003d799
  // stay finite; the levels and the feedback gain include values whose products are not exact in float.
  c3bn_lpc() {
    Process.getModuleByName('sound.dll').base.add(0x000b58a0).writeS32(10);   // g_codecOrder, see the header
    const cell = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    const fcell = (v) => { const p = __keepAlloc(4); p.writeFloat(v); return p; };
    const farr = (n, fn) => {
      const p = __keepAlloc(n * 4);
      for (let i = 0; i < n; ++i) p.add(i * 4).writeFloat(fn(i));
      return p;
    };
    const ctx = (prevLen, seed, combA, combB) => {
      const p = __keepAlloc(0x1000);
      for (let o = 0; o < 0x1000; o += 4) p.add(o).writeU32(0);
      p.add(0x688).writeS32(prevLen);
      for (let i = 0; i < 64; ++i) {
        p.add(0x68c + i * 4).writeFloat(Math.sin((i + seed) * 0.37) * 120.0);
        p.add(0x924 + i * 4).writeFloat(Math.cos((i + seed) * 0.23) * 95.0);
      }
      p.add(0xbbc).writeFloat(3.5 + seed);
      p.add(0xbc0).writeFloat(-1.25 * seed);
      p.add(0xbc4).writeFloat(0.1);
      p.add(0xbc8).writeFloat(-8.0 + seed);
      p.add(0xbcc).writeFloat(1.0 / 3.0);
      p.add(0xbd0).writeFloat(1e-7);
      p.add(0xbd4).writeFloat(10.0 + seed);          // the previous level, numerator of the ratio
      p.add(0xbd8).writeS32(combA);
      p.add(0xbdc).writeS32(combB);
      for (let i = 0; i < 5; ++i) p.add(0xbe0 + i * 2).writeS16(1000 * (i + 1) - seed * 37);
      return p;
    };
    const out = {
      len0: cell(0), len1: cell(1), len4: cell(4), len10: cell(10), len20: cell(20), len30: cell(30),
      mode0: cell(0), mode1: cell(1), mode7: cell(7),
      // the ratio is prevLevel / (level + 1e-6f): lvl_tiny drives it above the 8.0f clamp at 0x1005ec24
      lvl_1: fcell(1.0), lvl_tiny: fcell(1e-7), lvl_big: fcell(300000.0), lvl_mid: fcell(3.25),
      // level2 / 4 * 342 is the pulse amplitude: lvl2_big is above the 2000.0f clamp at 0x1005ec18
      lvl2_10: fcell(10.0), lvl2_big: fcell(500.0), lvl2_small: fcell(0.1), lvl2_mid: fcell(23.0),
      fb_01: fcell(0.1), fb_05: fcell(0.5), fb_09: fcell(1.0 / 3.0),
      A_a: farr(32, (i) => (i & 1 ? 0.05 : -0.05)),
      A_b: farr(32, (i) => 0.1 / (i + 2)),
      A_zero: farr(32, () => 0.0),
      ctx_a: ctx(20, 1, 1, 3), ctx_b: ctx(20, 2, 5, 5), ctx_c: ctx(12, 3, 2, 4),
      ctx_d: ctx(4, 4, 3, 1), ctx_e: ctx(30, 5, 4, 2), ctx_f: ctx(0, 6, 1, 1),
      ctx_g: ctx(20, 7, 5, 1),
    };
    for (let i = 0; i < 14; ++i) out['out' + i] = farr(64, (j) => -1.5 - j);
    return out;
  },

  // --- 0x10040000 codecPitchAnalysis: sample arrays, span cells, lag tables, score arrays and the three index
  // cells per vector.
  // A lag table is allocated with FOUR lead-in dwords holding 0, because the candidate cursor starts at
  // `bestIdx - 2` and reads lags[-1] and lags[0] when the first codecAmdfPitch call leaves index 1 (a defect
  // of the original, 0x100400a4 and 0x10040118); 0 is below every candidate, so the scan advances out of the
  // lead-in at once. A score array is allocated with 0x20 lead-in floats because the third-call improvement
  // subtracts 0x14 from the best-score index (0x10040268) and the store at 0x1004027b then lands before the
  // array. The lead-in of both is part of the compared state.
  // Sample arrays are periodic: the period decides which lag scores lowest, and a period that is not in the
  // table is what makes the refinement calls improve on it.
  c3bn_pitch() {
    const cell = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    const samples = (fn) => {
      const p = __keepAlloc(256 * 4);
      for (let i = 0; i < 256; ++i) p.add(i * 4).writeFloat(fn(i));
      return p;
    };
    const lagTable = (vals) => {
      const p = __keepAlloc((vals.length + 8) * 4);
      for (let i = 0; i < vals.length + 8; ++i) p.add(i * 4).writeS32(0);
      vals.forEach((v, i) => p.add((4 + i) * 4).writeS32(v));
      return { base: p, ptr: p.add(4 * 4), count: vals.length };
    };
    const periodic = (period) => samples((i) => Math.sin((2 * Math.PI * i) / period) * 8000.0);

    const tables = {
      L_a: lagTable([40, 50, 60, 70, 80, 90, 100, 110, 120]),
      L_b: lagTable([80, 95, 105, 118, 130]),
      L_c: lagTable([35, 38, 41]),
      L_d: lagTable([42, 44, 46, 48, 50]),
      L_e: lagTable([44, 56, 68, 80, 92, 104, 116]),
    };
    const out = {
      sp16: cell(16), sp32: cell(32), sp64: cell(64), sp4: cell(4),
      s_p60: periodic(60), s_p118: periodic(118), s_p120: periodic(120), s_p61: periodic(61),
      s_p41: periodic(41), s_flat: samples(() => 500.0),
      s_saw: samples((i) => ((i * 37) % 97) * 80.0 - 4000.0),
    };
    for (const k of Object.keys(tables)) {
      out[k] = tables[k].ptr;
      out[k + '_base'] = tables[k].base;
      out[k + '_n'] = cell(tables[k].count);
    }
    for (let i = 0; i < 14; ++i) {
      const p = __keepAlloc((0x20 + 64) * 4);
      for (let j = 0; j < 0x20 + 64; ++j) p.add(j * 4).writeFloat(-3.0 - j);
      out['sc' + i] = p.add(0x20 * 4);      // the array the function sees
      out['scb' + i] = p;                   // its lead-in, part of the state
      out['bi' + i] = cell(-1);
      out['si' + i] = cell(-1);
      out['lo' + i] = cell(-1);
    }
    return out;
  },

  // --- 0x10023610 Seq_remarkEventRange: sequencers, event descriptors, track-record arenas and the counter
  // blocks their +0x458 points at.
  // A sequencer is 0x800 bytes with a synthetic vtable whose slot +0xb4 reports the track count (0x1002362b)
  // and whose slot +0x5c picks the arm (0x1002365c); Seq_listGetAt reads the list head at +0x1fc and writes
  // the cursor at +0x204 (both inside the sub-object at +0x60), Seq_scheduleEvent reads the step at +0x1bc,
  // and the tick it is called with is the dword at +0x71c.
  // A track record is 0x480 bytes: Seq_listGetAt returns the payload at node+8, Seq_remarkEventRange sets bit
  // 3 of the byte at +0x38, Seq_scheduleEvent reads the period at +0x54 and writes the scheduled tick at +0x50,
  // and the counter block at +0x458 has its dword at +0x34 incremented.
  // No descriptor carries bit 5 in its flag dword, so Seq_scheduleEvent never reaches Midi_seekTrackToTick and
  // no MIDI stream is needed.
  c3bn_remark() {
    const keep = (cb) => { globalThis.__diffKeepAlive.push(cb); return cb; };
    const arena = (n, withCounters) => {
      const a = __keepAlloc(n * 0x480);
      for (let o = 0; o < n * 0x480; o += 4) a.add(o).writeU32(0);
      const counters = __keepAlloc(n * 0x40);
      for (let o = 0; o < n * 0x40; o += 4) counters.add(o).writeU32(0);
      for (let i = 0; i < n; ++i) {
        a.add(i * 0x480 + 0x38).writeU8(i & 0x7);             // bit 3 is clear in every record
        a.add(i * 0x480 + 0x50).writeU32((0x5e5e0000 + i) >>> 0);
        a.add(i * 0x480 + 0x54).writeU32(i === 3 ? 0 : 7 + i * 5);   // record 3 has a zero period
        a.add(i * 0x480 + 0x458).writePointer(withCounters ? counters.add(i * 0x40) : ptr(0));
        counters.add(i * 0x40 + 0x34).writeU32(100 + i);
      }
      return { base: a, counters: counters, n: n };
    };
    const ar = arena(10, true);
    const arNoCounter = arena(10, false);

    const seq = (count, mode, nodes, source, step, tick) => {
      const p = __keepAlloc(0x800);
      for (let o = 0; o < 0x800; o += 4) p.add(o).writeU32(0);
      const vt = __keepAlloc(0x100);
      for (let o = 0; o < 0x100; o += 4) vt.add(o).writeU32(0);
      vt.add(0xb4).writePointer(keep(new NativeCallback(function () { return count; }, 'uint32', [])));
      vt.add(0x5c).writePointer(keep(new NativeCallback(function () { return mode; }, 'uint32', [])));
      p.writePointer(vt);
      p.add(0x1bc).writeU32(step);
      p.add(0x71c).writeU32(tick);
      const list = [];
      for (let i = 0; i < nodes; ++i) {
        const node = __keepAlloc(0x10);
        for (let o = 0; o < 0x10; o += 4) node.add(o).writeU32(0);
        node.add(8).writePointer(source.base.add(i * 0x480));
        list.push(node);
      }
      for (let i = 0; i + 1 < list.length; ++i) list[i].add(4).writePointer(list[i + 1]);
      p.add(0x1fc).writePointer(list.length ? list[0] : ptr(0));   // the list head at (this+0x60)+0x19c
      p.add(0x204).writeU32(0x4b4b4b4b);                           // the cursor at (this+0x60)+0x1a4
      return p;
    };

    const desc = (flags, first, last, trackIndex) => {
      const p = __keepAlloc(0x40);
      for (let o = 0; o < 0x40; o += 4) p.add(o).writeU32(0);
      p.add(0).writeU32(flags >>> 0);
      p.add(8).writeU32(first >>> 0);
      p.add(0xc).writeU32(last >>> 0);
      p.add(0x20).writeS32(trackIndex);
      return p;
    };

    return {
      arena: ar.base, counters: ar.counters,
      arena2: arNoCounter.base, counters2: arNoCounter.counters,
      // mode 0 = the marking arm at 0x10023663, mode 1 = the scheduling arm at 0x10023697
      s_mark: seq(8, 0, 8, ar, 16, 1000),
      s_mark_short: seq(8, 0, 3, ar, 16, 1000),        // the list ends before the range does
      s_mark_cnt3: seq(3, 0, 8, ar, 16, 1000),         // a range above the track count
      s_sched: seq(8, 1, 8, ar, 16, 1000),
      s_sched_short: seq(8, 1, 3, ar, 16, 1000),
      s_sched_nocnt: seq(8, 1, 8, arNoCounter, 16, 1000),   // every +0x458 is null
      s_sched_step7: seq(8, 1, 8, ar, 7, 37),
      d_bad: desc(0x01, 5, 2, 0),            // last below first
      d_over: desc(0x01, 0, 7, 0),           // last not below the track count of s_mark_cnt3
      d_0_3: desc(0x00, 0, 3, 0),
      d_1_1: desc(0x00, 1, 1, 0),
      d_0_7: desc(0x00, 0, 7, 0),
      d_0_5: desc(0x00, 0, 5, 0),
      d_f1_0_3: desc(0x01, 0, 3, 0),
      d_f4_0_3: desc(0x04, 0, 3, 0),
      d_f10_2_4: desc(0x10, 2, 4, 0),
      d_f0_0_2: desc(0x00, 0, 2, 0),
      d_f1_0_5: desc(0x01, 0, 5, 0),
      d_f1_ti2: desc(0x01, 1, 1, 2),
      d_f4_3_6: desc(0x04, 3, 6, 0),
      d_f1_0_7: desc(0x01, 0, 7, 0),
    };
  },
});
