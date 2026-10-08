// Fixtures for C3 batch c3bi (sound.dll: six speech-codec functions and three sequencer / MIDI-device
// functions). Every block is allocated here: no live sound object, device, list, DirectSound buffer or module
// global is ever handed to an arm, and none of the functions under test allocates or frees. Layouts come from
// the disassembly cited in shim/src/re/c3bi.cpp. All values are synthetic; the MIDI byte streams are written
// here byte by byte and are not taken from any game file.
Object.assign(globalThis.DIFF_FIXTURES, {

  // --- 0x1003bf30 bitPermute: mode cells are plain numbers, so this fixture only holds the counts, the two
  // extra value slots, the value arrays (16 dwords, one-based), the bit arrays (0x100 bytes, because the pack
  // case writes the 54th bit at byte offset 0xd8 of the decremented pointer) and the codec contexts, whose only
  // touched field is the toggle at +0x2540.
  // Pack vectors use counts 10, 4 and 0; unpack vectors use 10, 4, 1 and 0. bits arrays b_all1 / b_all0 /
  // b_alt drive both sides of the sign-extension test at 0x1003c0fd.
  c3bi_bits() {
    const cell = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    const vals = (fn) => {
      const p = __keepAlloc(0x40);
      for (let i = 0; i < 16; ++i) p.add(i * 4).writeS32(fn(i));
      return p;
    };
    const bitbuf = (fn) => {
      const p = __keepAlloc(0x100);
      for (let i = 0; i < 64; ++i) p.add(i * 4).writeS32(fn(i));
      return p;
    };
    const ctx = (toggle) => {
      const p = __keepAlloc(0x2600);
      for (let o = 0; o < 0x2600; o += 4) p.add(o).writeU32(0);
      p.add(0x2540).writeS32(toggle);
      return p;
    };
    const out = {
      n10: cell(10), n4: cell(4), n1: cell(1), n0: cell(0),
      ctx0: ctx(0), ctx1: ctx(1), ctx2: ctx(0),
      // value arrays: 15-bit magnitudes, a few at the field limits of the table at 0x10064454
      x_ramp: vals((i) => i * 37 + 1),
      x_max: vals(() => 0x7fff),
      x_mix: vals((i) => (i % 2 === 0 ? 0x1234 + i : 0x7f00 - i * 11)),
      x_zero: vals(() => 0),
      x_neg: vals((i) => -(i * 101 + 3)),
      // bit arrays: index 1..53 carry the packed bits, index 0x36 the 54th
      b_all1: bitbuf(() => 1),
      b_all0: bitbuf(() => 0),
      b_alt: bitbuf((i) => i & 1),
      b_pat: bitbuf((i) => ((i >> 2) + i) & 1),
    };
    for (let i = 0; i < 14; ++i) {
      out['pa' + i] = cell(0x51510000 + i);
      out['pb' + i] = cell(-(0x22220000 + i));
      out['px' + i] = vals((j) => 0x6d6d0000 + i * 16 + j);
      out['py' + i] = bitbuf((j) => (i + j) & 1);
    }
    return out;
  },

  // --- 0x1003e6d0 gaussSolveClamped: order cells, packed row-major matrices (the function rebases the pointer
  // by order+1 floats, so matrix[j + i*order] on the rebased pointer is caller element (i-1)*order + (j-1)),
  // right-hand sides and solution buffers. The work matrix is a frame slot indexed row*10+column, so the order
  // never exceeds 10; these vectors use 1..4.
  // gm_spd is diagonally dominant (every pivot well above the 1e-10 floor at 0x1005ec40), gm_neg has negative
  // diagonal entries (the fchs arm at 0x1003e86e), gm_sing0 is all zeros (the pivot test fails on row 1) and
  // gm_sing3 is fine until row 3. gr_big and gr_negbig push the solution past +/-0.999 so both clamps fire.
  c3bi_gauss() {
    const cell = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    const buf = (fn) => {
      const p = __keepAlloc(0x80);
      for (let i = 0; i < 32; ++i) p.add(i * 4).writeFloat(fn(i));
      return p;
    };
    const third = 1 / 3;
    const out = {
      o1: cell(1), o2: cell(2), o3: cell(3), o4: cell(4),
      // 4x4 row-major; element (r,c) at index r*4+c
      gm_spd: buf((i) => { const r = (i / 4) | 0, c = i % 4; return i < 16 ? (r === c ? 4 + r : 0.5 / (1 + Math.abs(r - c))) : 0; }),
      gm_neg: buf((i) => { const r = (i / 4) | 0, c = i % 4; return i < 16 ? (r === c ? -(2 + r) : 0.25 * (r - c)) : 0; }),
      gm_sing0: buf(() => 0),
      gm_sing3: buf((i) => { const r = (i / 4) | 0, c = i % 4; return i < 16 ? (r === 2 ? 0 : (r === c ? 3 : 0.3)) : 0; }),
      gm_mix: buf((i) => { const r = (i / 4) | 0, c = i % 4; return i < 16 ? (r === c ? 1 + third * r : (c > r ? 1e-7 : 3e5 * 1e-6)) : 0; }),
      gr_small: buf((i) => 0.1 + i * 0.05),
      gr_big: buf(() => 12.5),
      gr_negbig: buf(() => -9.75),
      gr_odd: buf((i) => (i % 2 === 0 ? third : -0.1) * (1 + i)),
      gr_huge: buf((i) => (i === 0 ? 3e5 : 1e-7)),
    };
    for (let i = 0; i < 14; ++i) out['gs' + i] = buf(() => 0);
    return out;
  },

  // --- 0x100414f0 codecAmdfPitch: sample buffers of 128 floats, lag arrays (one-based), count cells, span
  // cells, window cells and result buffers with their smallest/largest index cells.
  // With window 64 and lags 8..32 the scan starts at (64-lag)/2+1 = 17..29 and reads at most index
  // start + span - 1 + lag, which stays inside 128 for span 16.
  c3bi_amdf() {
    const cell = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    const buf = (fn) => {
      const p = __keepAlloc(0x200);
      for (let i = 0; i < 128; ++i) p.add(i * 4).writeFloat(fn(i));
      return p;
    };
    const lagv = (list) => {
      const p = __keepAlloc(0x80);
      for (let i = 0; i < 32; ++i) p.add(i * 4).writeS32(0);
      list.forEach((v, i) => p.add(i * 4).writeS32(v));
      return p;
    };
    const third = 1 / 3;
    const out = {
      ax_sine: buf((i) => Math.sin(i * 0.23) * 0.8),
      ax_alt: buf((i) => (i % 2 === 0 ? 1 : -1) * (0.2 + i * 0.01)),
      ax_zero: buf(() => 0),
      ax_tiny: buf((i) => (i % 3 === 0 ? 1e-7 : -1e-7)),
      ax_huge: buf((i) => (i === 40 ? 3e5 : third * ((i % 5) - 2))),
      // one-based: lags[1..n]
      al4: lagv([0, 8, 16, 24, 32]),
      al3: lagv([0, 12, 4, 20]),
      al1: lagv([0, 16]),
      al0: lagv([0, 0, 8, 0]),
      anc4: cell(4), anc3: cell(3), anc1: cell(1), anc0: cell(0),
      asp16: cell(16), asp8: cell(8), asp4: cell(4), asp1: cell(1), asp0: cell(0),
      awin64: cell(64),
    };
    for (let i = 0; i < 14; ++i) {
      out['ares' + i] = buf(() => 0);
      out['amin' + i] = cell(0);
      out['amax' + i] = cell(0);
    }
    return out;
  },

  // --- 0x10040ab0 codecMarkCorrelationJumps: sample buffers of 128 floats, jump index arrays, count and
  // capacity cells, offset / last / window cells and codec contexts.
  // The context's touched window is +0x21c4..+0x2223: the smoothed product, the smoothed energy, their ratio,
  // the 16-slot ring at +0x21d0, the running sum at +0x2210, the two indices at +0x2214 / +0x2218, the last
  // jump at +0x221c and the open flag at +0x2220.
  // mc_zero (energy stays exactly 0.0f) takes the `jne 0x10040c0b` side; mc_lopsided seeds a large product with
  // a tiny energy so the magnitude test at 0x10040c40 picks floatCopySign; mc_ring seeds alternating +/-6 in
  // the ring so the running-sum move passes the 1.7f limit at 0x1005ecac.
  c3bi_jumps() {
    const cell = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    const buf = (fn) => {
      const p = __keepAlloc(0x200);
      for (let i = 0; i < 128; ++i) p.add(i * 4).writeFloat(fn(i));
      return p;
    };
    const ctx = (product, energy, ring, sum, ia, ib, lastJump, open) => {
      const p = __keepAlloc(0x2300);
      for (let o = 0; o < 0x2300; o += 4) p.add(o).writeU32(0);
      p.add(0x21c4).writeFloat(product);
      p.add(0x21c8).writeFloat(energy);
      p.add(0x21cc).writeFloat(0);
      for (let i = 0; i < 16; ++i) p.add(0x21d0 + i * 4).writeFloat(ring(i));
      p.add(0x2210).writeFloat(sum);
      p.add(0x2214).writeS32(ia);
      p.add(0x2218).writeS32(ib);
      p.add(0x221c).writeS32(lastJump);
      p.add(0x2220).writeS32(open);
      return p;
    };
    const third = 1 / 3;
    const out = {
      ms_sine: buf((i) => Math.sin(i * 0.31) * 0.9),
      ms_zero: buf(() => 0),
      ms_step: buf((i) => (i < 20 ? 0.05 : (i < 24 ? 2.5 : -2.5))),
      ms_alt: buf((i) => (i % 2 === 0 ? third : -third) * (1 + (i % 7))),
      ms_tiny: buf((i) => (i % 3 === 0 ? 1e-7 : -1e-7)),
      mc_zero: ctx(0, 0, () => 0, 0, 1, 1, 0, 0),
      mc_warm: ctx(0.5, 1.25, (i) => 0.1 * (i - 8), 0.4, 3, 9, 5, 0),
      mc_lopsided: ctx(40, 1e-7, (i) => (i % 2 ? 0.2 : -0.2), 0, 1, 16, 0, 0),
      mc_ring: ctx(0.75, 0.9, (i) => (i % 2 ? 6 : -6), 6, 2, 5, 12, 0),
      mc_open: ctx(0.3, 0.6, (i) => 0.05 * i, 0.5, 16, 1, 3, 1),
      mc_open2: ctx(-0.9, 1.1, (i) => (i % 4 === 0 ? 4 : -1), 1, 8, 8, 2, 1),
      mj_off4: cell(4), mj_off0: cell(0),
      mj_last40: cell(40), mj_last24: cell(24),
      mj_win32: cell(32), mj_win8: cell(8), mj_win0: cell(0),
      mj_cap8: cell(8), mj_cap0: cell(0),
    };
    for (let i = 0; i < 14; ++i) {
      const p = __keepAlloc(0x80);
      for (let o = 0; o < 0x80; o += 4) p.add(o).writeS32(0);
      out['mjidx' + i] = p;
      out['mjcnt' + i] = cell(1);
    }
    return out;
  },

  // --- 0x10041020 codecWindowFeatures: two sample arrays of 64 floats, a frame range (its dwords at +0 and +4
  // after the one-dword rebase are the low and high sample index), a rebase descriptor (its dwords at +0 and +8
  // rebase the first and the second array; both are 0 here so the arrays are used as they are), sub-frame, lag
  // and dither cells, and one output set per vector.
  // Range 20..51 gives a window of 32; sub-frame 1 scans 21..36 and sub-frame 2 scans 37..52, so with lag 8
  // every index from 13 to 60 stays inside the 64-float arrays. Range 20..20 gives a window of 1 and an empty
  // scan, which leaves every accumulator at zero and makes all six divisor clamps fire.
  c3bi_features() {
    const cell = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    const fcell = (v) => { const p = __keepAlloc(4); p.writeFloat(v); return p; };
    const buf = (fn) => {
      const p = __keepAlloc(0x100);
      for (let i = 0; i < 64; ++i) p.add(i * 4).writeFloat(fn(i));
      return p;
    };
    const pair = (a, b) => {
      const p = __keepAlloc(0x20);
      for (let o = 0; o < 0x20; o += 4) p.add(o).writeS32(0);
      p.add(0).writeS32(a); p.add(4).writeS32(b);
      return p;
    };
    const third = 1 / 3;
    const out = {
      wf_sine: buf((i) => Math.sin(i * 0.37) * 0.7),
      wf_alt: buf((i) => (i % 2 === 0 ? 1 : -1) * (0.3 + i * 0.02)),
      wf_zero: buf(() => 0),
      wf_huge: buf((i) => (i % 2 === 0 ? 3e5 : -2.5e5)),
      wf_tiny: buf((i) => (i % 3 === 0 ? 1e-7 : -third * 1e-6)),
      wf_ramp: buf((i) => i * 0.11 - 3),
      wr_32: pair(20, 51),       // window 32
      wr_1: pair(20, 20),        // window 1, empty sub-frame
      wr_8: pair(20, 27),        // window 8
      wd_0: pair(0, 0),          // descriptor: both rebases zero (+0 and +8 are read)
      wsub1: cell(1), wsub2: cell(2),
      wlag8: cell(8), wlag4: cell(4),
    };
    for (let i = 0; i < 13; ++i) {
      out['wdit' + i] = fcell(i % 2 === 0 ? 0.25 : -0.4);
      out['wcr' + i] = cell(0);
      out['wl1' + i] = cell(0);
      out['wl2' + i] = cell(0);
      out['wsr' + i] = fcell(0);
      out['wlp' + i] = fcell(0);
      out['wcm' + i] = fcell(0);
      out['wcp' + i] = fcell(0);
    }
    return out;
  },

  // --- 0x1003f310 codecGainSmoother: gain arrays (one-based after the one-float rebase), count / index / mode
  // cells, two index outputs per vector and codec contexts.
  // A context's touched window is +0x2268..+0x253f: the 0x3c running costs, the two 0x3c-dword back-pointer
  // rows at +0x2358, the row number at +0x2538 and the smoothed level at +0x253c.
  // gc_hi seeds costs that every running cost beats (the `je 0x1003f443` write arm), gc_lo seeds costs no
  // running cost beats (the reset arm), gc_dip has its minimum at index 27 so the backoff loop at 0x1003f650
  // finds an index above 20.
  c3bi_gains() {
    const cell = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    const buf = (fn) => {
      const p = __keepAlloc(0x100);
      for (let i = 0; i < 64; ++i) p.add(i * 4).writeFloat(fn(i));
      return p;
    };
    const ctx = (cost, row0, row1, rowSlot, level) => {
      const p = __keepAlloc(0x2600);
      for (let o = 0; o < 0x2600; o += 4) p.add(o).writeU32(0);
      for (let i = 0; i < 0x3c; ++i) {
        p.add(0x2268 + i * 4).writeFloat(cost(i));
        p.add(0x2358 + i * 4).writeS32(row0(i));
        p.add(0x2358 + (0x3c + i) * 4).writeS32(row1(i));
      }
      p.add(0x2538).writeS32(rowSlot);
      p.add(0x253c).writeFloat(level);
      return p;
    };
    const third = 1 / 3;
    const out = {
      gg_flat: buf(() => 10),
      gg_ramp: buf((i) => 2 + i * 0.5),
      gg_dip: buf((i) => (i === 26 ? -40 : 20 + (i % 7))),
      gg_odd: buf((i) => (i % 2 === 0 ? third : -0.1) * (1 + (i % 11))),
      gg_huge: buf((i) => (i === 3 ? 3e5 : 1e-7)),
      gc_hi: ctx(() => 1e6, (i) => (i % 0x3c) + 1, (i) => 0x3c - (i % 0x3c), 0, 200),
      gc_lo: ctx(() => -1e6, () => 1, () => 2, 1, 50),
      gc_dip: ctx((i) => (i === 26 ? -500 : 100 + i), (i) => (i % 5) + 1, (i) => (i % 3) + 1, 0, 300),
      // a second dip at cost index 6 so that, with the chosen index at 27, a quarter of the largest cost is
      // strictly above cost[27-20-1] and the backoff at 0x1003f67e actually fires
      gc_dip2: ctx((i) => (i === 26 ? -500 : (i === 6 ? -480 : 100 + i)),
                   (i) => (i % 5) + 1, (i) => (i % 3) + 1, 0, 300),
      gc_mix: ctx((i) => (i % 2 === 0 ? 5 : -5) * (1 + i), (i) => i + 1, (i) => i + 1, 1, 100),
      gc_zero: ctx(() => 0, () => 1, () => 1, 0, 0),
      gn48: cell(48), gn20: cell(20), gn8: cell(8), gn2: cell(2), gn1: cell(1),
      gi1: cell(1), gi5: cell(5),
      gmode0: cell(0), gmode1: cell(1), gmode2: cell(2),
    };
    for (let i = 0; i < 14; ++i) { out['gco' + i] = cell(0); out['gio' + i] = cell(0); }
    return out;
  },

  // --- 0x10023a60 Seq_cmdHandler10: sequencers with their own vtable (only slot +0xb4 is filled, with a
  // NativeCallback that reports the track count), a node chain at +0x218 whose nodes are {next +4, payload +8}
  // and whose payloads carry the tag at +4 and the range at +8 / +0xc, and the list Seq_listGetAt walks at
  // this+0x60 (head at this+0x1fc, cursor at this+0x204) over 0x480-byte track records out of one arena. The
  // only record fields the function touches are the byte at +0x38 and the dword at +0x45c.
  c3bi_cmd10() {
    const arena = __keepAlloc(10 * 0x480);
    for (let o = 0; o < 10 * 0x480; o += 4) arena.add(o).writeU32(0);
    for (let i = 0; i < 10; ++i) {
      arena.add(i * 0x480 + 0x38).writeU8(i & 0xff);
      arena.add(i * 0x480 + 0x45c).writeU32((0x7a7a0000 + i) >>> 0);
    }
    const keep = (cb) => { globalThis.__diffKeepAlive.push(cb); return cb; };
    const payload = (tag, lo, hi) => {
      const p = __keepAlloc(0x20);
      for (let o = 0; o < 0x20; o += 4) p.add(o).writeU32(0);
      p.add(4).writeU32(tag >>> 0);
      p.add(8).writeU32(lo >>> 0);
      p.add(0xc).writeU32(hi >>> 0);
      return p;
    };
    const seq = (gate, count, listNodes, chain) => {
      const p = __keepAlloc(0x800);
      for (let o = 0; o < 0x800; o += 4) p.add(o).writeU32(0);
      const vt = __keepAlloc(0x100);
      for (let o = 0; o < 0x100; o += 4) vt.add(o).writeU32(0);
      vt.add(0xb4).writePointer(keep(new NativeCallback(function () { return count; }, 'uint32', [])));
      p.writePointer(vt);
      p.add(0x224).writeS32(gate);
      // the chain at +0x218: an array of [tag, lo, hi] or null for a node with no payload
      const nodes = [];
      for (let i = 0; i < chain.length; ++i) {
        const n = __keepAlloc(0x10);
        for (let o = 0; o < 0x10; o += 4) n.add(o).writeU32(0);
        n.add(8).writePointer(chain[i] ? payload(chain[i][0], chain[i][1], chain[i][2]) : ptr(0));
        nodes.push(n);
      }
      for (let i = 0; i + 1 < nodes.length; ++i) nodes[i].add(4).writePointer(nodes[i + 1]);
      p.add(0x218).writePointer(nodes.length ? nodes[0] : ptr(0));
      // the Seq_listGetAt list at this+0x60: head at +0x1fc, cursor at +0x204
      const lnodes = [];
      for (let i = 0; i < listNodes; ++i) {
        const n = __keepAlloc(0x10);
        for (let o = 0; o < 0x10; o += 4) n.add(o).writeU32(0);
        n.add(8).writePointer(arena.add(i * 0x480));
        lnodes.push(n);
      }
      for (let i = 0; i + 1 < lnodes.length; ++i) lnodes[i].add(4).writePointer(lnodes[i + 1]);
      p.add(0x1fc).writePointer(listNodes ? lnodes[0] : ptr(0));
      p.add(0x204).writeU32(0x4b4b4b4b);
      return p;
    };
    return {
      arena: arena,
      q_gate0: seq(0, 8, 6, [[1, 0, 2]]),            // +0x224 is zero: returns 0x24 at once
      q_nochain: seq(1, 8, 6, []),                   // +0x218 is null
      q_nopay: seq(1, 8, 6, [null]),                 // first node has no payload
      q_short: seq(1, 8, 6, [[1, 0, 2], [2, 1, 3]]), // tag 9 is not in the chain
      q_nopay2: seq(1, 8, 6, [[1, 0, 2], null]),     // the SECOND node has no payload
      q_bad: seq(1, 8, 6, [[5, 4, 1]]),              // hi below lo
      q_overcount: seq(1, 3, 6, [[6, 0, 5]]),        // hi is not below the track count 3
      q_ok: seq(1, 8, 6, [[7, 1, 4]]),
      q_one: seq(1, 8, 6, [[8, 2, 2]]),
      q_deep: seq(1, 8, 6, [[1, 0, 0], [2, 0, 1], [3, 0, 3]]),
      q_long: seq(1, 8, 2, [[4, 0, 5]]),             // the list is shorter than the range
      q_zero: seq(1, 8, 6, [[9, 0, 0]]),
      q_full: seq(1, 8, 6, [[10, 0, 7]]),
    };
  },

  // --- 0x10024b80 Seq_scheduleEvent: one sequencer per vector (the list at +0x60 and the step at +0x1bc, plus
  // the controller block at +0x254 that Midi_seekTrackToTick writes), and track records of 0x60 bytes with a
  // synthetic MIDI track at +0x3c, a period at +0x54 and the scheduled tick at +0x50.
  c3bi_schedule() {
    const vlq = (n) => {
      const o = [n & 0x7f];
      n >>>= 7;
      while (n > 0) { o.unshift((n & 0x7f) | 0x80); n >>>= 7; }
      return o;
    };
    const END = [0xff, 0x2f, 0x00];
    const track = (period, bytes) => {
      const data = __keepAlloc(bytes.length + 16);
      bytes.forEach((b, i) => data.add(i).writeU8(b & 0xff));
      for (let i = bytes.length; i < bytes.length + 16; ++i) data.add(i).writeU8(0);
      const rec = __keepAlloc(0x60);
      for (let o = 0; o < 0x60; o += 4) rec.add(o).writeU32(0);
      rec.add(0x3c).writePointer(data);
      rec.add(0x54).writeS32(period);
      return rec;
    };
    const notes = [].concat(vlq(0), [0x90, 60, 100], vlq(40), [0x80, 60, 0], vlq(40), [0xb0, 7, 90],
                            vlq(40), END);
    const seq = (step, listPeriods) => {
      const p = __keepAlloc(0x800);
      for (let o = 0; o < 0x800; o += 4) p.add(o).writeU32(0);
      p.add(0x1bc).writeS32(step);
      const nodes = [];
      for (let i = 0; i < listPeriods.length; ++i) {
        const rec = track(listPeriods[i], notes);
        const n = __keepAlloc(0x10);
        for (let o = 0; o < 0x10; o += 4) n.add(o).writeU32(0);
        n.add(8).writePointer(rec);
        nodes.push(n);
      }
      for (let i = 0; i + 1 < nodes.length; ++i) nodes[i].add(4).writePointer(nodes[i + 1]);
      p.add(0x1fc).writePointer(nodes.length ? nodes[0] : ptr(0));
      p.add(0x204).writeU32(0x5c5c5c5c);
      return p;
    };
    const out = { sq_a: seq(64, [120, 48, 7]), sq_b: seq(13, [0, 33]), sq_empty: seq(100, []) };
    const periods = [96, 96, 96, 50, 50, 7, 7, 1, 0, 0, 96, 50, 7, 96];
    for (let i = 0; i < 14; ++i) out['st' + i] = track(periods[i], notes);
    return out;
  },

  // --- 0x100213d0 Midi_Device::setChannelVolume: MIDI devices of 0x800 bytes (the per-channel dwords at
  // +0x254 + 0x4c*channel and +0x260 + 0x4c*channel, and the scale at +0x730), two-byte controller events,
  // channel-state objects whose 0x80 group pointers sit at +0x58..+0x254 with the flag byte at +0x38 and the
  // channel at +0x48, and group objects with the selection mask in bits 12..27 of +0xc8 and sixteen voice slots
  // at +0x2a8, stride 0x110, each with its flag byte at +0x9c and its level at +0xe8.
  // A group with bit 0 of +0xc8 CLEAR is called through its own vtable slot +0x40; that slot is a stdcall
  // NativeCallback which records the call count and the last value it was handed in a 16-byte log block, so
  // the two arms are compared on how often and with what they reached it.
  c3bi_volume() {
    const keep = (cb) => { globalThis.__diffKeepAlive.push(cb); return cb; };
    const device = (scale) => {
      const p = __keepAlloc(0x800);
      for (let o = 0; o < 0x800; o += 4) p.add(o).writeU32(0);
      p.add(0x730).writeS32(scale);
      return p;
    };
    const event = (controller, value) => {
      const p = __keepAlloc(4);
      p.writeU32(0);
      p.add(0).writeU8(controller);
      p.add(1).writeU8(value);
      return p;
    };
    const log = __keepAlloc(0x10);
    for (let o = 0; o < 0x10; o += 4) log.add(o).writeU32(0);
    const slot40 = keep(new NativeCallback(function (v) {
      log.writeU32((log.readU32() + 1) >>> 0);
      log.add(4).writeU32(v >>> 0);
    }, 'void', ['uint32'], 'stdcall'));
    const group = (flags, mask, voiceFlags, levels) => {
      const p = __keepAlloc(0x1500);
      for (let o = 0; o < 0x1500; o += 4) p.add(o).writeU32(0);
      const vt = __keepAlloc(0x80);
      for (let o = 0; o < 0x80; o += 4) vt.add(o).writeU32(0);
      vt.add(0x40).writePointer(slot40);
      p.writePointer(vt);
      p.add(0xc8).writeU32((((mask & 0xffff) << 12) | (flags & 1)) >>> 0);
      for (let k = 0; k < 16; ++k) {
        p.add(0x2a8 + k * 0x110 + 0x9c).writeU8(voiceFlags(k));
        p.add(0x2a8 + k * 0x110 + 0xe8).writeS32(levels(k));
      }
      return p;
    };
    const state = (flagByte, channel, groups) => {
      const p = __keepAlloc(0x300);
      for (let o = 0; o < 0x300; o += 4) p.add(o).writeU32(0);
      p.add(0x38).writeU8(flagByte);
      p.add(0x48).writeS32(channel);
      // the walk starts at +0x254 and steps DOWN four bytes 0x80 times, i.e. +0x58..+0x254
      groups.forEach((g, i) => p.add(0x254 - i * 4).writePointer(g));
      return p;
    };
    const gVoices = group(1, 0x0005, (k) => (k === 2 ? 8 : 0), (k) => 0x40 + k * 3);
    const gVoicesAll = group(1, 0xffff, () => 0, (k) => 1 + k * 17);
    const gVoicesNone = group(1, 0x0000, () => 0, (k) => 100);
    const gVtable = group(0, 0x00ff, () => 0, () => 7);
    return {
      log: log,
      vg_voices: gVoices, vg_all: gVoicesAll, vg_none: gVoicesNone, vg_vt: gVtable,
      vdev_a: device(0x80), vdev_b: device(0x40), vdev_c: device(0),
      ve7_100: event(7, 100), ve7_1: event(7, 1), ve7_255: event(7, 255),
      ve10_64: event(0xa, 64), ve1_90: event(1, 90), ve127_5: event(127, 5),
      vst_on: state(0, 0, [gVoices, gVoicesAll, ptr(0), gVtable]),
      vst_ch3: state(0, 3, [gVoicesNone, gVoices]),
      vst_ch15: state(0, 15, [gVoicesAll]),
      vst_masked: state(1, 0, [gVoices]),        // bit 0 of +0x38 set: returns at once
      vst_other: state(0, 9, [gVoices]),         // +0x48 names another channel
      vst_empty: state(0, 0, []),                // every group pointer is null
      vst_vt: state(0, 0, [gVtable, gVtable]),
    };
  },
});
