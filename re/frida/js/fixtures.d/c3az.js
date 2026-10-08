// Fixtures for C3 batch c3az (sound.dll: codec arithmetic leaves, channel voice sweeps and pan maths, the
// DirectSound buffer description switch, three sequencer descriptor helpers). Every block is allocated here:
// no live sound object, device, list, DirectSound buffer or module global is ever handed to an arm. Layouts
// come from the disassembly cited in shim/src/re/c3az.cpp. All values are synthetic.
// NativeCallbacks are pushed onto __diffKeepAlive by hand (only Memory.alloc is rewritten to __keepAlloc).
Object.assign(globalThis.DIFF_FIXTURES, {

  // --- 0x1003e480 medianOfThree: twelve int cells. The triples in the registry take both sides of all six
  // compares; the three 5s (i5, i5b) and the 2 (i5a) give the equal cases.
  c3az_ints() {
    const mk = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    return {
      i0: mk(-2147483648), i1: mk(-1000), i2: mk(-1), i3: mk(0), i4: mk(1), i5: mk(5), i5a: mk(2),
      i5b: mk(5), i6: mk(1000), i7: mk(2147483647), i10: mk(-5), i11: mk(7),
    };
  },

  // --- 0x1003e2c0 intPow: base and exponent cells. bbig squares into a wrap, b1/bm1/b2/bm3 drive the four
  // non-positive-exponent arms, emax is the exponent whose negation is 0x7fffffff.
  c3az_pow() {
    const mk = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    return {
      b1: mk(1), bm1: mk(-1), b2: mk(2), b3: mk(3), b7: mk(7), bm2: mk(-2), bm3: mk(-3), bbig: mk(0x10001),
      e0: mk(0), e1: mk(1), e2: mk(2), e3: mk(3), e4: mk(4), e5: mk(5), e10: mk(10), e31: mk(31),
      em1: mk(-1), em2: mk(-2), em3: mk(-3), emax: mk(-2147483647),
    };
  },

  // --- 0x10040da0 codecFirstOrderFilter: seven input buffers of 16 floats, eight output buffers of 18
  // floats (the function indexes from 1 after decrementing the pointer by one float), six count cells, four
  // coefficients and six filter states. No infinity and no NaN: those would compare an x87-generated default
  // quiet NaN against an SSE one.
  c3az_firstorder() {
    const buf = (vals) => {
      const p = __keepAlloc(0x48);
      for (let i = 0; i < 18; ++i) p.add(i * 4).writeFloat(i < vals.length ? vals[i] : 0);
      return p;
    };
    const cell = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    const fcell = (v) => { const p = __keepAlloc(4); p.writeFloat(v); return p; };
    const third = 1 / 3;
    return {
      in0: buf([0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9]),
      in1: buf([third, 2 * third, 4 * third, 8 * third, -third, -2 * third, 16 * third, 32 * third, third]),
      in2: buf([3e5, -3e5, 1.5e5, 7e4, -2.5e5, 3e5, 1, -1, 0]),
      in3: buf([1e-7, -1e-7, 2e-7, 5e-8, 1e-7, 3e-7, -4e-7, 1e-7, 0]),
      in4: buf([-1, -2, -4, -8, -16, -32, -64, -128, -256]),
      in5: buf([0, -0, 1e-45, -1e-45, 1.0000001, 0.9999999, 1e7, -1e7, 0.1]),
      in6: buf([5, 5, 5, 5, 5, 5, 5, 5, 5]),
      out0: buf([]), out1: buf([]), out2: buf([]), out3: buf([]),
      out4: buf([]), out5: buf([]), out6: buf([]), out7: buf([]),
      n0: cell(0), n1: cell(1), n2: cell(2), n4: cell(4), n8: cell(8), nm1: cell(-1),
      co0: fcell(0.1), co1: fcell(-third), co2: fcell(1.0), co3: fcell(1e-7),
      st0: fcell(0.25), st1: fcell(-0.1), st2: fcell(1e-7), st3: fcell(3e5), st4: fcell(0), st5: fcell(third),
    };
  },

  // --- 0x1003ed00 codecRemoveMean: six input buffers, six output buffers and six count cells. mn0 divides
  // the accumulator by zero and writes nothing; mnm1 skips both loops.
  c3az_mean() {
    const buf = (vals) => {
      const p = __keepAlloc(0x48);
      for (let i = 0; i < 18; ++i) p.add(i * 4).writeFloat(i < vals.length ? vals[i] : 0);
      return p;
    };
    const cell = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    const third = 1 / 3;
    return {
      mi0: buf([1, 2, 3, 4, 5, 6, 7, 8, 9]),
      mi1: buf([0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9]),
      mi2: buf([third, -third, 2 * third, -2 * third, 4 * third, -4 * third, third, -third, third]),
      mi3: buf([3e5, -3e5, 1e-7, -1e-7, 1, -1, 2.5, -2.5, 0]),
      mi4: buf([-1, -1, -1, -1, -1, -1, -1, -1, -1]),
      mi5: buf([1e-45, -1e-45, 0, -0, 1e7, -1e7, 0.5, -0.5, 1]),
      mo0: buf([]), mo1: buf([]), mo2: buf([]), mo3: buf([]), mo4: buf([]), mo5: buf([]),
      mn0: cell(0), mn1: cell(1), mn2: cell(2), mn4: cell(4), mn8: cell(8), mnm1: cell(-1),
    };
  },

  // --- 0x1003ec60 codecRmsEnergy: five input buffers, six accumulator cells and six count cells. rnm1 and
  // rnm3 skip the loop and take the square root of a signed zero; no NaN reaches the CRT's sqrt.
  c3az_rms() {
    const buf = (vals) => {
      const p = __keepAlloc(0x48);
      for (let i = 0; i < 18; ++i) p.add(i * 4).writeFloat(i < vals.length ? vals[i] : 0);
      return p;
    };
    const cell = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    const fcell = (v) => { const p = __keepAlloc(4); p.writeFloat(v); return p; };
    const third = 1 / 3;
    return {
      ri0: buf([1, 2, 3, 4, 5, 6, 7, 8, 9]),
      ri1: buf([0.1, -0.2, 0.3, -0.4, 0.5, -0.6, 0.7, -0.8, 0.9]),
      ri2: buf([third, third, third, third, third, third, third, third, third]),
      ri3: buf([3e5, 1e-7, 2, -2, 1e4, -1e4, 0.5, -0.5, 0]),
      ri4: buf([1e-45, 1e-45, 0, 0, 1, 1, 1, 1, 1]),
      ra0: fcell(123.5), ra1: fcell(-7.25), ra2: fcell(0), ra3: fcell(1e-7), ra4: fcell(3e5), ra5: fcell(0.1),
      rn1: cell(1), rn2: cell(2), rn4: cell(4), rn8: cell(8), rnm1: cell(-1), rnm3: cell(-3),
    };
  },

  // --- 0x1003e400 floatFloorToInt: sixteen float cells. f0..f7 are non-negative (the fadd arm), f8..f15 are
  // negative (the fsubr + fchs arm); each group holds an exact half, values just below and just above a half,
  // a value that is not exact in binary and a large magnitude. -0.0 compares as not-below so it takes the
  // non-negative arm.
  c3az_floors() {
    const vals = [0.0, 0.4, 0.5, 0.6, 1.5, 2.5, 1e7, 0.1,
                  -0.0, -0.4, -0.5, -0.6, -1.5, -2.5, -1e7, -1 / 3];
    const out = {};
    vals.forEach((v, i) => { const p = __keepAlloc(4); p.writeFloat(v); out['f' + i] = p; });
    return out;
  },

  // --- 0x10040f60 codecCombFilter: twelve contexts. A context carries cursor A at +0xbd8, cursor B at +0xbdc
  // and a five-entry 16-bit delay line at +0xbe0; both cursors are 1-based and wrap from 1 back to 5.
  // k6..k9 and k11 put a cursor at 1 so the wrap runs, k10 starts a cursor at 0 (its read lands on the high
  // half of cursor B, which the fixture fixes at a known value) and k11 makes the 16-bit sum wrap.
  c3az_comb() {
    const mk = (a, b, line) => {
      const p = __keepAlloc(0xc00);
      for (let o = 0xbd0; o < 0xc00; o += 4) p.add(o).writeU32(0);
      p.add(0xbd8).writeS32(a);
      p.add(0xbdc).writeS32(b);
      for (let i = 0; i < 5; ++i) p.add(0xbe0 + i * 2).writeS16(line[i]);
      return p;
    };
    return {
      k0: mk(5, 3, [100, 200, 300, 400, 500]),
      k1: mk(4, 2, [1, 2, 3, 4, 5]),
      k2: mk(3, 5, [-1, -2, -3, -4, -5]),
      k3: mk(2, 4, [1000, -1000, 32767, -32768, 0]),
      k4: mk(5, 5, [7, 7, 7, 7, 7]),
      k5: mk(3, 3, [0, 0, 0, 0, 0]),
      k6: mk(1, 3, [10, 20, 30, 40, 50]),
      k7: mk(3, 1, [10, 20, 30, 40, 50]),
      k8: mk(1, 1, [5, 6, 7, 8, 9]),
      k9: mk(2, 1, [-100, 100, -200, 200, -300]),
      k10: mk(0, 2, [11, 22, 33, 44, 55]),
      k11: mk(1, 1, [32767, 0, 0, 0, 0]),
    };
  },

  // --- 0x10034ac0 Channel_releaseVoices and 0x10034b90 Channel_stopVoices share this channel builder.
  // A channel is 0x1400 bytes: the flag dword at +0x58, a second flag dword at +0x5c, the parked pair at
  // +0x1e0 / +0x1e4 and 0x10 voices whose sweep pointer is +0x2d4 + i*0x110. Relative to that pointer the
  // fields used are -0x44 (double 1), -0x40 (its high dword), -0x3c / -0x38 (the second double slot),
  // -8 (a flag byte), 0 (the voice flag byte), +0x48 (a dword) and +0x54 (double 2).
  // `voices` is a function i -> {d1, d2, flag, prev, slot}; everything else is zeroed so both arms start equal.
  c3az_channel(flags, flags2, voices) {
    const p = __keepAlloc(0x1400);
    for (let o = 0; o < 0x1400; o += 4) p.add(o).writeU32(0);
    p.add(0x58).writeU32(flags >>> 0);
    p.add(0x5c).writeU32(flags2 >>> 0);
    p.add(0x1e0).writeU32(0xa1a1a1a1);
    p.add(0x1e4).writeU32(0xb2b2b2b2);
    p.add(0x210).writeU32(0xc3c3c3c3);
    p.add(0x214).writeU32(0xd4d4d4d4);
    for (let i = 0; i < 0x10; ++i) {
      const v = p.add(0x2d4 + i * 0x110);
      const s = voices(i);
      v.sub(0x44).writeDouble(s.d1);
      v.add(0x54).writeDouble(s.d2);
      v.sub(0x3c).writeDouble(s.second === undefined ? 0 : s.second);
      v.sub(8).writeU8(s.prev === undefined ? 0xff : s.prev);
      v.writeU8(s.flag === undefined ? 0 : s.flag);
      v.add(0x48).writeU32((s.slot === undefined ? 0x5e5e0000 + i : s.slot) >>> 0);
    }
    return p;
  },

  c3az_relvoices() {
    const ch = globalThis.DIFF_FIXTURES.c3az_channel;
    const zero = () => ({ d1: 0, d2: 0 });
    const mask = 0x0ffff000;
    return {
      r0: ch(0x00000001, 0, zero),                       // bit 6 clear -> 0x15
      r1: ch(0x00000051, 0, zero),                       // bit 4 set   -> 0x15
      r2: ch(0x00000040, 0, zero),                       // bit 0 clear -> only sets bit 3
      r3: ch(0x10000041, 0, zero),                       // bit 28 set  -> only sets bit 3
      r4: ch(0x00000041, 0, zero),                       // bits 12..27 clear -> sweep touches nothing
      r5: ch(0x00001041, 0, () => ({ d1: 0, d2: 3.5 })),             // first double zero
      r6: ch(0x00000041 | mask, 0, () => ({ d1: 2.5, d2: 0 })),      // second double zero
      r7: ch(0x00002041, 0, () => ({ d1: 2.5, d2: -3.5 })),          // both non-zero
      r8: ch(0x00000041 | mask, 0, (i) => (i % 3 === 0 ? { d1: 0, d2: 1 }
                                           : i % 3 === 1 ? { d1: 1, d2: 0 } : { d1: 1e-7, d2: -3e5 })),
      r9: ch(0x00000041 | mask, 0, () => ({ d1: NaN, d2: 1 })),      // unordered on the first compare
      r10: ch(0x00000041 | mask, 0, () => ({ d1: 1, d2: NaN })),     // unordered on the second compare
      r11: ch(0x00800041, 0, (i) => ({ d1: i % 2 ? -0 : 5e-324, d2: i % 2 ? 1e308 : 0,
                                       flag: i & 0xf, prev: 0xff - i, slot: 0x7777 + i })),
    };
  },

  c3az_stopvoices() {
    const ch = globalThis.DIFF_FIXTURES.c3az_channel;
    const mask = 0x0ffff000;
    const neg = (i) => ({ d1: -1.5 - i, d2: -2.5 - i, second: 9.5 + i });
    return {
      s0: ch(0x00000001, 0, neg),                        // bit 6 clear -> 0x15
      s1: ch(0x00000051, 0, neg),                        // bit 4 set   -> 0x15
      s2: ch(0x00000040, 0x00000005, neg),               // bit 0 clear -> the parking arm
      s3: ch(0x00000041, 0, neg),                        // bits 12..27 clear -> sweep touches nothing
      s4: ch(0x00000041 | mask, 0, neg),                 // every voice below zero, flag byte clear
      s5: ch(0x00000041 | mask, 0, (i) => ({ d1: 1.25 + i, d2: 0, second: 4 })),       // not below
      s6: ch(0x00000041 | mask, 0, (i) => ({ d1: 1.25 + i, d2: 7.5, second: 4 })),     // above zero
      s7: ch(0x00000041 | mask, 0, (i) => ({ d1: -1 - i, d2: -1 - i, flag: 8 })),      // bit 3 already set
      s8: ch(0x00000041 | mask, 0, (i) => ({ d1: -1 - i, d2: -1 - i, flag: 0x20 })),   // bit 5 already set
      s9: ch(0x00000041 | mask, 0, () => ({ d1: 3.5, d2: NaN })),                      // unordered
      s10: ch(0x00000041 | mask, 0, (i) => (i % 4 === 0 ? { d1: -8.5, d2: -0.5 }
                                            : i % 4 === 1 ? { d1: 1, d2: 0 }
                                            : i % 4 === 2 ? { d1: 2, d2: -1e-7, flag: 0x28 }
                                            : { d1: 3, d2: -3e5, flag: 1 })),
      s11: ch(0x00001041, 0, (i) => ({ d1: -5e-324, d2: -1e308, second: 1e-7, flag: i & 0x17 })),
    };
  },

  // --- 0x10035340 Channel_setPanGains: five channels differing only in the base level at +0x1d8 and in the
  // parked double at +0x1e0 / +0x1e4 that the zero-pan arm copies through.
  c3az_pangains() {
    const mk = (base, lo, hi) => {
      const p = __keepAlloc(0x400);
      for (let o = 0; o < 0x400; o += 4) p.add(o).writeU32(0);
      p.add(0x1d8).writeS32(base);
      p.add(0x1e0).writeU32(lo >>> 0);
      p.add(0x1e4).writeU32(hi >>> 0);
      p.add(0x224).writeS32(0x7f7f);
      return p;
    };
    return {
      g_zero: mk(0, 0x11112222, 0x33334444),
      g_small: mk(1, 0x55556666, 0x77778888),
      g_mid: mk(0x4000, 0x9999aaaa, 0xbbbbcccc),
      g_max: mk(0x10000, 0xddddeeee, 0x0f0f0f0f),
      g_neg: mk(-0x8000, 0x12345678, 0x9abcdef0),
    };
  },

  // --- 0x10033ba0 Channel_resetPanState: twelve channels. The flag word at +0x58 covers every combination of
  // the four bits the rebuild keeps (0, 2, 29, 31) plus words with unrelated bits set; the parked dwords at
  // +0x208, +0x210 and +0x214 and the pan at +0x224 differ per channel so the Channel_setPanGains call takes
  // its zero, positive, negative and both clamping arms.
  c3az_panstate() {
    const flagWords = [0x00000000, 0x00000001, 0x00000004, 0x00000005, 0x20000000, 0x20000001,
                       0x80000000, 0x80000005, 0xa0000005, 0x12345678, 0xffffffff, 0x5ffffffa];
    const pans = [0, 1, -1, 0x20, -0x20, 0x3f, -0x40, 0x40, -0x41, 0x7fffffff, -0x80000000, 0];
    const bases = [0, 1, 0x100, 0x4000, 0x10000, 0x20000, -0x8000, 0x7fff, 0x1234, 0x8000, 0x40, 0x10001];
    const out = {};
    for (let i = 0; i < 12; ++i) {
      const p = __keepAlloc(0x400);
      for (let o = 0; o < 0x400; o += 4) p.add(o).writeU32(0);
      p.add(0x58).writeU32(flagWords[i] >>> 0);
      p.add(0x5c).writeU32((0xe1e10000 + i) >>> 0);
      p.add(0x208).writeS32(bases[i]);
      p.add(0x210).writeU32((0x21210000 + i) >>> 0);
      p.add(0x214).writeU32((0x43430000 + i) >>> 0);
      p.add(0x224).writeS32(pans[i]);
      out['p' + i] = p;
    }
    return out;
  },

  // --- 0x10010f30 DsBuffer::configureDesc: four buffers. d_busy has a non-null dword at +0x60 so it is
  // refused with 6 (the value is never dereferenced). The others differ in the dword at +0, whose bit 1 the
  // function overwrites, and carry a marker pattern from +0x70 to +0xaf so the zeroing and the four constant
  // stores are visible.
  c3az_dsdesc() {
    const mk = (first, iface) => {
      const p = __keepAlloc(0x200);
      for (let o = 0; o < 0x200; o += 4) p.add(o).writeU32(0);
      p.add(0).writeU32(first >>> 0);
      p.add(0x60).writeU32(iface >>> 0);
      for (let o = 0x70; o < 0xb0; o += 4) p.add(o).writeU32((0x7e7e0000 + o) >>> 0);
      return p;
    };
    return {
      d_zero: mk(0x00000000, 0),
      d_ones: mk(0xffffffff, 0),
      d_mix: mk(0x12345678, 0),
      d_busy: mk(0x00000055, 0x00001234),
    };
  },

  // --- 0x10023e10 Seq_applyTrackFlag: three sequencers and eight descriptors.
  // A sequencer is our own 0x800-byte block with its own vtable; only slot +0xb4 is filled, with a
  // NativeCallback that returns the track count (the original reads the count through that slot and nothing
  // else). The list Seq_listGetAt walks lives at this+0x60, so its head is this+0x1fc and its cursor
  // this+0x204; a node is {next +4, payload +8} and a payload is a 0x480-byte track record out of one arena,
  // with the byte at +0x38 and the dword at +0x460 the only fields the function touches.
  // A descriptor carries the index at +0x28, the flag byte at +0x2c and the value at +0x30.
  c3az_trackflag() {
    const arena = __keepAlloc(8 * 0x480);
    for (let o = 0; o < 8 * 0x480; o += 4) arena.add(o).writeU32(0);
    const initialFlag = [0x00, 0xff, 0x10, 0xef, 0x55, 0xaa, 0x01, 0x80];
    for (let i = 0; i < 8; ++i) {
      arena.add(i * 0x480 + 0x38).writeU8(initialFlag[i]);
      arena.add(i * 0x480 + 0x460).writeU32((0x6c6c0000 + i) >>> 0);
    }
    const keep = (cb) => { globalThis.__diffKeepAlive.push(cb); return cb; };
    const seq = (count, nodes) => {
      const p = __keepAlloc(0x800);
      for (let o = 0; o < 0x800; o += 4) p.add(o).writeU32(0);
      const vt = __keepAlloc(0x100);
      for (let o = 0; o < 0x100; o += 4) vt.add(o).writeU32(0);
      vt.add(0xb4).writePointer(keep(new NativeCallback(function () { return count; }, 'uint32', [])));
      p.writePointer(vt);
      const chain = [];
      for (let i = 0; i < nodes; ++i) {
        const n = __keepAlloc(0x10);
        n.writeU32(0); n.add(4).writePointer(ptr(0)); n.add(8).writePointer(arena.add(i * 0x480));
        n.add(0xc).writeU32(0);
        chain.push(n);
      }
      for (let i = 0; i + 1 < nodes; ++i) chain[i].add(4).writePointer(chain[i + 1]);
      p.add(0x1fc).writePointer(nodes ? chain[0] : ptr(0));     // list head  (this+0x60 +0x19c)
      p.add(0x204).writeU32(0x3c3c3c3c);                        // walk cursor (this+0x60 +0x1a4)
      return p;
    };
    const desc = (index, flag, value) => {
      const p = __keepAlloc(0x40);
      for (let o = 0; o < 0x40; o += 4) p.add(o).writeU32(0);
      p.add(0x28).writeU32(index >>> 0);
      p.add(0x2c).writeU8(flag);
      p.add(0x30).writeU32(value >>> 0);
      return p;
    };
    return {
      arena: arena,
      q4: seq(4, 5), q1: seq(1, 2), q0: seq(0, 1),
      t0a: desc(0, 0x00, 0x11110000), t0b: desc(0, 0x02, 0x11110001),
      t1a: desc(1, 0x01, 0x22220000), t1b: desc(1, 0x03, 0x22220001),
      t3a: desc(3, 0xfd, 0x33330000), t3b: desc(3, 0xff, 0x33330001),
      t4: desc(4, 0x02, 0x44440000), t99: desc(99, 0x02, 0x99990000),
    };
  },

  // --- 0x10024280 Seq_loadChannelDesc: three destinations and five sources. Every +0x1c is NULL, so the
  // Sound_setName call neither frees nor allocates, and every +0x20 is NULL so it never builds a path.
  c3az_chandesc() {
    const rec = (flags, tag) => {
      const p = __keepAlloc(0x40);
      for (let o = 0; o < 0x40; o += 4) p.add(o).writeU32(0);
      p.add(0).writeU32((0xd0d00000 + tag) >>> 0);
      p.add(4).writeU32(flags >>> 0);
      p.add(8).writeU32((0xd8d80000 + tag) >>> 0);
      p.add(0xc).writeU32((0xdcdc0000 + tag) >>> 0);
      p.add(0x10).writeU32((0xe0e00000 + tag) >>> 0);
      p.add(0x14).writeU32((0xe4e40000 + tag) >>> 0);
      p.add(0x1c).writePointer(ptr(0));
      p.add(0x20).writePointer(ptr(0));
      return p;
    };
    return {
      c_dst0: rec(0x00000000, 0), c_dst1: rec(0xffffffff, 1), c_dst2: rec(0x5a5a5a5a, 2),
      c_src0: rec(0x00000000, 3), c_src1: rec(0x00000001, 4), c_src2: rec(0x00000002, 5),
      c_src3: rec(0x00000003, 6), c_src4: rec(0xfffffffc, 7),
    };
  },

  // --- 0x10024360 Seq_mergeChannelDesc: three destinations and eighteen sources. m_src0..m_src15 cover all
  // sixteen combinations of bits 0, 1, 3 and 4 of the source byte at +4; m_named0 and m_named1 add a name
  // pointer so the Sound_setName call runs. Every destination's +0x1c starts NULL and its +0x20 is NULL, so
  // the first call allocates without freeing and never builds a path; +0x1c is left out of the state regions
  // because each arm allocates its own copy.
  c3az_mergedesc() {
    const rec = (flags, tag, name) => {
      const p = __keepAlloc(0x40);
      for (let o = 0; o < 0x40; o += 4) p.add(o).writeU32(0);
      p.add(0).writeU32((0x90900000 + tag) >>> 0);
      p.add(4).writeU32(flags >>> 0);
      p.add(8).writeU32((0x98980000 + tag) >>> 0);
      p.add(0xc).writeU32((0x9c9c0000 + tag) >>> 0);
      p.add(0x10).writeU32((0xa0a00000 + tag) >>> 0);
      p.add(0x14).writeU32((0xa4a40000 + tag) >>> 0);
      p.add(0x1c).writePointer(name || ptr(0));
      p.add(0x20).writePointer(ptr(0));
      return p;
    };
    const str = (s) => {
      const p = __keepAlloc(s.length + 1);
      for (let i = 0; i < s.length; ++i) p.add(i).writeU8(s.charCodeAt(i));
      p.add(s.length).writeU8(0);
      return p;
    };
    const out = {
      m_dst0: rec(0x00000000, 0, 0),
      m_dst1: rec(0xffffffff, 1, 0),
      m_dst2: rec(0x0000a5a5, 2, 0),
      m_named0: rec(0x0000001b, 3, str('c3az_name_one')),
      m_named1: rec(0x00000000, 4, str('c3az_name_two')),
    };
    for (let i = 0; i < 16; ++i) {
      const flag = (i & 3) | ((i & 4) << 1) | ((i & 8) << 1);
      out['m_src' + i] = rec(0xffffff00 | flag, 0x10 + i, 0);
    }
    return out;
  },
});
