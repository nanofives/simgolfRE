// Fixtures for C3 batch c3cx (sound.dll, round 23): the 3D position chain, the rate/timer family, MultiSound flag
// slots, Channel_allocAndInitVoice, getDeviceCaps, getNodeByIndex, Ctrl5b19c_ctor, ostreamResetVtable and the old
// small-block heap's sbhResizeBlock / sbhPageFree. Layouts come from the disassembly cited in shim/src/re/c3cx.cpp.
// Every object, vtable, interface, list node, string, region and page here is allocated by the fixture; no live
// sound object, device, DirectSound interface, list or file is handed to an arm, and nothing is freed.
// Two module globals are written, each saved first and restored at the flush (re/frida/js/freeze.js), before the
// first thread is resumed:
//   g_soundTimer 0x100b49ec (the c3cx_timer_* fixtures, with the game's first thread frozen: the main thread's
//     sound calls read it), swapped for a fixture-owned timer or for null;
//   the old small-block heap's counter 0x100b5590 (the c3cx_sbh_* fixtures; the running sound.dll uses the system
//     heap, 0x100b5d20 = 1, so nothing else reads it).
// All values are synthetic. NativeCallbacks are kept alive through __diffKeepAlive.
Object.assign(globalThis.DIFF_FIXTURES, {

  c3cx_fill(p, n, b) { p.writeByteArray(new Array(n).fill(b)); return p; },
  c3cx_keep(cb) { globalThis.__diffKeepAlive.push(cb); return cb; },
  c3cx_snd(rva) { return Process.getModuleByName('sound.dll').base.add(rva); },

  // --- the 3D position chain ---------------------------------------------------------------------------------
  // One 0x400 block per shape, so one state region covers everything a vector can touch:
  //   +0x000  the object: a channel (flag byte +0x5c, Ds3DBuffer pointer +0x54) or a MultiSound (the channel at
  //           +0x70, so the flag byte is +0xcc and the buffer pointer +0xc4; +0x40 the device that 0x1002c270
  //           forwards to, +0x60 the field it stores);
  //   +0x100  the device: vtable slot +0xec (thiscall, one argument) bumps +0x84, records the argument at +0x88 and
  //           returns its own code at +0x80;
  //   +0x180  the Ds3DBuffer: +0x64 the interface, +0xc8 / +0xcc / +0xd0 the three position floats (0.5, 2.0, 3.0
  //           at start, so a setter that passes the wrong neighbour reads RED);
  //   +0x300  the interface: vtable slot +0x4c (stdcall, five arguments: the interface, x, y, z, apply) bumps
  //           +0x384, records the four arguments at +0x388..+0x394 and returns its own code at +0x380.
  c3cx_3d() {
    const F = globalThis.DIFF_FIXTURES;
    const comVt = F.c3cx_fill(__keepAlloc(0x100), 0x100, 0);
    comVt.add(0x4c).writePointer(F.c3cx_keep(new NativeCallback(function (self, a, b, c, d) {
      self.add(0x84).writeU32((self.add(0x84).readU32() + 1) >>> 0);
      self.add(0x88).writeU32(a >>> 0);
      self.add(0x8c).writeU32(b >>> 0);
      self.add(0x90).writeU32(c >>> 0);
      self.add(0x94).writeU32(d >>> 0);
      return self.add(0x80).readS32();
    }, 'int', ['pointer', 'uint32', 'uint32', 'uint32', 'uint32'], 'stdcall')));
    const devVt = F.c3cx_fill(__keepAlloc(0x100), 0x100, 0);
    devVt.add(0xec).writePointer(F.c3cx_keep(new NativeCallback(function (self, v) {
      self.add(0x84).writeU32((self.add(0x84).readU32() + 1) >>> 0);
      self.add(0x88).writeU32(v >>> 0);
      return self.add(0x80).readS32();
    }, 'int', ['pointer', 'uint32'], 'thiscall')));
    // multi: channel at +0x70; flag: the byte at channel+0x5c; buf / com: present; code: interface result;
    // dev: device present (MultiSound only) with code devCode.
    const mk = (multi, flag, buf, com, code, dev, devCode) => {
      const blk = F.c3cx_fill(__keepAlloc(0x400), 0x400, 0);
      const ch = multi ? 0x70 : 0;
      for (let o = 0; o < 0x100; o += 4) blk.add(o).writeU32((0x0b000000 + o) >>> 0);   // markers
      blk.add(ch + 0x5c).writeU32((0x5c5c5c00 | flag) >>> 0);
      blk.add(ch + 0x54).writePointer(buf ? blk.add(0x180) : ptr(0));
      if (multi) blk.add(0x40).writePointer(dev ? blk.add(0x100) : ptr(0));
      const d = blk.add(0x100);
      d.writePointer(devVt);
      d.add(0x80).writeS32(devCode);
      const b = blk.add(0x180);
      b.add(0x64).writePointer(com ? blk.add(0x300) : ptr(0));
      b.add(0xc8).writeU32(0x3f000000);
      b.add(0xcc).writeU32(0x40000000);
      b.add(0xd0).writeU32(0x40400000);
      b.add(0xd4).writeU32(0xd4d4d4d4);
      const c = blk.add(0x300);
      c.writePointer(comVt);
      c.add(0x80).writeS32(code);
      return blk;
    };
    const shapes = (m) => ({
      off: mk(m, 0x00, 1, 1, 0, 1, 0),           // 3D flag clear
      offhi: mk(m, 0xdf, 1, 1, 0, 0, 0),         // every other bit set, 3D flag clear
      nobuf: mk(m, 0x20, 0, 1, 0, 1, 7),         // flag set, no Ds3DBuffer
      nocom: mk(m, 0x20, 1, 0, 0, 0, 0),         // flag set, buffer without an interface
      ok0: mk(m, 0x20, 1, 1, 0, 1, 0),           // interface returns 0
      okE: mk(m, 0xff, 1, 1, 0x88780096 | 0, 1, 0x77),   // interface returns a failure code
      okN: mk(m, 0x20, 1, 1, -1, 0, 0),          // interface returns -1
    });
    const out = {};
    const c = shapes(false), ms = shapes(true);
    for (const k of Object.keys(c)) { out['c_' + k] = c[k]; out['m_' + k] = ms[k]; }
    return out;
  },

  // --- the rate/timer family -----------------------------------------------------------------------------------
  // Objects for Seq_scaleTickRate (+0x174 / +0x17c), Sound::scaleTickRate (the Seq at +0x60), Channel_setRateGlide
  // (+0x1c4, +0x1c8, +0x1d0, +0x1d8, +0x1e0, +0x5c), MultiSound::setRateGlide (the same at +0x70) and
  // MultiSound::setReverseRate (the channel at +0x70: +0x58 bit 0 CLEAR in every object, so Channel_setReverseRate
  // never enters its per-voice loop, which calls the live engine 0x100b4a20; its timer part reads +0x208 and
  // +0x1e0 and writes +0x1c0, +0x1c4, +0x1c8). One 0x400 block each.
  // period: the timer period, or null for a null g_soundTimer. Everything is built first; then the first thread is
  // frozen and g_soundTimer is saved and replaced; the flush restores it before the resume.
  c3cx_timer(period) {
    const F = globalThis.DIFF_FIXTURES;
    const blk = () => {
      const p = F.c3cx_fill(__keepAlloc(0x400), 0x400, 0);
      for (let o = 0; o < 0x400; o += 4) p.add(o).writeU32((0x7a000000 + o) >>> 0);
      return p;
    };
    const out = {};
    // Seq objects: +0x17c values.
    [0, 1, 1000, 0x7fffffff, 0xffffffff, 0x80000001].forEach((v, i) => {
      const p = blk(); p.add(0x17c).writeU32(v >>> 0); p.add(0x174).writeU32(0x17417417); out['q' + i] = p;
    });
    // Sound objects: the Seq at +0x60.
    [12345, 0xfffffff0, 7].forEach((v, i) => {
      const p = blk(); p.add(0x60 + 0x17c).writeU32(v >>> 0); out['s' + i] = p;
    });
    // channel objects for Channel_setRateGlide, at base 0 (prefix g) and inside a MultiSound at +0x70 (prefix h).
    // cur: the current rate +0x1d8; flags: +0x5c.
    const glide = (base, cur, flags) => {
      const p = blk();
      p.add(base + 0x1d8).writeS32(cur);
      p.add(base + 0x5c).writeU32(flags >>> 0);
      p.add(base + 0x1c4).writeU32(0x1c41c41c);
      p.add(base + 0x1c8).writeDouble(-7.25);
      p.add(base + 0x1d0).writeDouble(3.5);
      p.add(base + 0x1e0).writeDouble(9.75);
      return p;
    };
    [[0x10000, 0], [0, 0xffffffef], [-0x3000, 0x12345601], [0x7fff, 0x10]].forEach((a, i) => {
      out['g' + i] = glide(0, a[0], a[1]);
      out['h' + i] = glide(0x70, a[0], a[1]);
    });
    // MultiSound objects for setReverseRate: +0x58 of the channel (this+0xc8) has bit 0 clear.
    [[1000, 1.5, 0x00000000], [-1000, -0.25, 0xfffffffe], [0, 0.1, 0x00000040],
     [0x7fffffff, 1e300, 0x10000000]].forEach((a, i) => {
      const p = blk();
      p.add(0x70 + 0x58).writeU32(a[2] >>> 0);
      p.add(0x70 + 0x208).writeS32(a[0]);
      p.add(0x70 + 0x1e0).writeDouble(a[1]);
      p.add(0x70 + 0x1c0).writeU32(0x1c01c01c);
      p.add(0x70 + 0x1c4).writeU32(0x1c41c41c);
      p.add(0x70 + 0x1c8).writeDouble(-1.0);
      out['r' + i] = p;
    });
    const timer = F.c3cx_fill(__keepAlloc(0x40), 0x40, 0);
    timer.writeU32((period === null ? 0x7e7e7e7e : period) >>> 0);
    out.timer = timer;
    const g = F.c3cx_snd(0xb49ec);
    __freeze.thread();
    __freeze.save(g, 4);
    g.writePointer(period === null ? ptr(0) : timer);
    return out;
  },
  c3cx_timer_p10() { return globalThis.DIFF_FIXTURES.c3cx_timer(10); },
  c3cx_timer_p4096() { return globalThis.DIFF_FIXTURES.c3cx_timer(4096); },
  c3cx_timer_p1() { return globalThis.DIFF_FIXTURES.c3cx_timer(1); },
  c3cx_timer_null() { return globalThis.DIFF_FIXTURES.c3cx_timer(null); },

  // --- MultiSound flag and forwarding slots ----------------------------------------------------------------------
  // One 0x1800 block per object: the MultiSound at +0 (the channel at +0x70, so the channel's +0x58 / +0x5c are
  // +0xc8 / +0xcc, its voice blocks start at +0x70 + 0x2d4 - 0x84 and its stream list at +0x13b8), a device at
  // +0x1600 (vtable slot +0x6c, thiscall one argument: bumps +0x84 and records the argument at +0x88; it is what
  // Channel_applyFlags 0x1002c040 forwards to through this+0x40), and the MultiSound's own vtable slot +0x48
  // (thiscall one argument: bumps +0x1780, records the argument at +0x1784 and ORs 0x1000 into +0xcc, so the
  // order of the `or` that follows it in applyFlags is observable). Stream nodes at +0x1400 ({+4 next, +8 element})
  // and elements at +0x1480 ({+0 value, +4 threshold}).
  c3cx_ms() {
    const F = globalThis.DIFF_FIXTURES;
    const vt = F.c3cx_fill(__keepAlloc(0x100), 0x100, 0);
    vt.add(0x48).writePointer(F.c3cx_keep(new NativeCallback(function (self, a) {
      self.add(0x1780).writeU32((self.add(0x1780).readU32() + 1) >>> 0);
      self.add(0x1784).writeU32(a >>> 0);
      self.add(0xcc).writeU32((self.add(0xcc).readU32() | 0x1000) >>> 0);
    }, 'void', ['pointer', 'uint32'], 'thiscall')));
    // slot +0xcc (thiscall, nothing pushed): bumps +0x178c and returns the object's own value at +0x1788.
    vt.add(0xcc).writePointer(F.c3cx_keep(new NativeCallback(function (self) {
      self.add(0x178c).writeU32((self.add(0x178c).readU32() + 1) >>> 0);
      return self.add(0x1788).readS32();
    }, 'int', ['pointer'], 'thiscall')));
    const dvt = F.c3cx_fill(__keepAlloc(0x100), 0x100, 0);
    dvt.add(0x6c).writePointer(F.c3cx_keep(new NativeCallback(function (self, f) {
      self.add(0x84).writeU32((self.add(0x84).readU32() + 1) >>> 0);
      self.add(0x88).writeU32(f >>> 0);
    }, 'void', ['pointer', 'uint32'], 'thiscall')));
    // device slot +0x70 (thiscall, nothing pushed; what Channel_collectFlagBits starts from): bumps +0x90 and
    // returns the device's own value at +0x80.
    dvt.add(0x70).writePointer(F.c3cx_keep(new NativeCallback(function (self) {
      self.add(0x90).writeU32((self.add(0x90).readU32() + 1) >>> 0);
      return self.add(0x80).readS32();
    }, 'int', ['pointer'], 'thiscall')));
    // device slot +0x9c (thiscall, one argument; what Channel_setField5cClamped1200 forwards to): bumps +0x94 and
    // records the argument at +0x98; returns 0x99.
    dvt.add(0x9c).writePointer(F.c3cx_keep(new NativeCallback(function (self, v) {
      self.add(0x94).writeU32((self.add(0x94).readU32() + 1) >>> 0);
      self.add(0x98).writeU32(v >>> 0);
      return 0x99;
    }, 'int', ['pointer', 'uint32'], 'thiscall')));
    // slot +0x3c in COM form (stdcall: the object, one value), the interface DsBuffer::setVolume 0x1000f1c0 calls
    // through the buffer's +0x60 when that is non-null: bumps +0xa0 and records the value at +0xa4.
    dvt.add(0x3c).writePointer(F.c3cx_keep(new NativeCallback(function (self, v) {
      self.add(0xa0).writeU32((self.add(0xa0).readU32() + 1) >>> 0);
      self.add(0xa4).writeU32(v >>> 0);
      return 0;
    }, 'int', ['pointer', 'uint32'], 'stdcall')));
    // c8 / cc: the two flag words; dev: a device at +0x40; voices: per-voice setup for Channel_stopVoices;
    // stream: [position, [[value, threshold] | null, ...]] for Channel_findStreamNode; x: a final adjustment.
    const mk = (c8, cc, dev, voices, stream, x) => {
      const p = F.c3cx_fill(__keepAlloc(0x1800), 0x1800, 0);
      for (let o = 0; o < 0x1400; o += 4) p.add(o).writeU32((0x5a000000 + o) >>> 0);
      p.writePointer(vt);
      p.add(0xc8).writeU32(c8 >>> 0);
      p.add(0xcc).writeU32(cc >>> 0);
      p.add(0x30).writeU32(0x30303030);
      p.add(0x58).writeU32(0x00000000);
      p.add(0x40).writePointer(dev ? p.add(0x1600) : ptr(0));
      p.add(0x1600).writePointer(dvt);
      // voice blocks: the byte flag at channel+0x2d4+i*0x110 and the double at channel+0x2d4+0x54+i*0x110.
      for (let i = 0; i < 16; ++i) {
        const v = p.add(0x70 + 0x2d4 + i * 0x110);
        v.writeU8(voices ? voices[i % voices.length][0] : 0x11);
        v.add(0x54).writeDouble(voices ? voices[i % voices.length][1] : 1.0);
      }
      p.add(0x70 + 0x1e0).writeU32(0x1e01e01e);
      p.add(0x70 + 0x1e4).writeU32(0x1e41e41e);
      if (stream) {
        p.add(0x70 + 0x16c).writeU32(stream[0] >>> 0);
        p.add(0x70 + 0x1350).writeU32(0x13501350);
        const items = stream[1];
        for (let i = 0; i < items.length; ++i) {
          const node = p.add(0x1400 + i * 0x10);
          node.add(4).writePointer(i + 1 < items.length ? p.add(0x1400 + (i + 1) * 0x10) : ptr(0));
          if (items[i] === null) { node.add(8).writePointer(ptr(0)); continue; }
          const el = p.add(0x1480 + i * 0x10);
          el.writeU32(items[i][0] >>> 0);
          el.add(4).writeU32(items[i][1] >>> 0);
          node.add(8).writePointer(el);
        }
        p.add(0x70 + 0x1348).writePointer(items.length ? p.add(0x1400) : ptr(0));
      }
      if (x) x(p);
      return p;
    };
    // Channel_collectFlagBits / MultiSound_getStatus: device value (null = no device), +0x30, +0x58, +0x228.
    const st = (devVal, f30, f58, f228) => mk(0, 0, devVal !== null, null, null, (p) => {
      if (devVal !== null) p.add(0x1680).writeU32(devVal >>> 0);
      p.add(0x30).writeU32(f30 >>> 0);
      p.add(0x58).writeU32(f58 >>> 0);
      p.add(0x228).writeU32(f228 >>> 0);
    });
    // MultiSound::setVolume / MultiSound_setPitchCents: the +0xcc slot's value, +0xc8 (the channel's +0x58; bit 28
    // set or bit 0 clear in every shape, so Channel_setRate never enters its loop over the live engine 0x100b4a20),
    // a device, and for setVolume a DsBuffer at +0x1640 as the channel's +0x54 (this+0xc4) with its level object
    // (+0x5c) at +0x1400 and its interface (+0x60) the device, plus voice 3's buffer pointer (channel+0x318+3*0x110)
    // on a second DsBuffer at +0x14c0. Every other voice buffer pointer is null.
    const vol = (ccVal, c8, dev, buf, lvl, com, voice) => mk(c8, 0, dev, null, null, (p) => {
      p.add(0x1788).writeU32(ccVal >>> 0);
      for (let i = 0; i < 16; ++i) p.add(0x70 + 0x318 + i * 0x110).writePointer(ptr(0));
      p.add(0x70 + 0x54).writePointer(buf ? p.add(0x1640) : ptr(0));
      const b = p.add(0x1640);
      b.add(0x5c).writePointer(lvl ? p.add(0x1400) : ptr(0));
      b.add(0x60).writePointer(com ? p.add(0x1600) : ptr(0));
      b.add(0xc4).writeU32(0xc4c4c4c4);
      p.add(0x1400 + 0x188).writeU32(lvl >>> 0);
      if (voice) {
        p.add(0x70 + 0x318 + 3 * 0x110).writePointer(p.add(0x14c0));
        p.add(0x14c0 + 0x5c).writePointer(ptr(0));
        p.add(0x14c0 + 0x60).writePointer(ptr(0));
      }
      p.add(0x70 + 0x1d8).writeU32(0x1d81d81d);
      p.add(0x70 + 0x224).writeS32(-17);
    });
    const V = [[0x11, -1.0], [0x29, -2.0], [0x00, 0.5], [0x08, -0.0], [0x20, -3.0]];
    return {
      // MultiSound_is3DEnabled
      e0: mk(0, 0, 0), e1: mk(2, 0x20, 0), e2: mk(0xff, 0xff, 0), e3: mk(0xfffffffd, 0xffffffff, 0),
      e4: mk(0x2, 0xffffffdf, 0), e5: mk(0x3, 0x120, 0), e6: mk(0x80000002, 0x20, 0), e7: mk(0x1, 0x20, 0),
      e8: mk(0x2, 0x40, 0), e9: mk(0xffffffff, 0x20, 0), e10: mk(0x10002, 0x2020, 0), e11: mk(0x2, 0x21, 0),
      // MultiSound::applyFlags: bit 1 of +0xc8 clear everywhere (flushMmioBuffer then only sets bit 0).
      a0: mk(0, 0, 1), a1: mk(0x55555555, 0x0f0f0f0f, 0), a2: mk(0xfffffffd, 0, 1), a3: mk(0x40, 0xffffffff, 0),
      // MultiSound_stop (Channel_stopVoices on the channel): no bit 6 / bits 6+4 / bit 6 without bit 0 /
      // bit 6 + bit 0 with busy bits and mixed voices / bit 6 + bit 0 with no busy bits.
      t0: mk(0x00000001, 0, 0), t1: mk(0x00000051, 0, 0), t2: mk(0x00000040, 0x3, 0),
      t3: mk(0x0000f041, 0, 0, V), t4: mk(0x0ffff041, 0x8, 0, V), t5: mk(0x00000041, 0, 0, V),
      t6: mk(0x00001041, 0, 0, [[0x00, -5.0]]),
      // MultiSound_findStreamNode (Channel_findStreamNode on the channel): empty list, null first element, one
      // element below / above the position, three ascending thresholds, a null element after a match.
      f0: mk(0, 0, 0, null, [100, []]), f1: mk(0, 0, 0, null, [100, [null]]),
      f2: mk(0, 0, 0, null, [100, [[0x111, 50]]]), f3: mk(0, 0, 0, null, [100, [[0x222, 150]]]),
      f4: mk(0, 0, 0, null, [100, [[0xa1, 10], [0xa2, 100], [0xa3, 101]]]),
      f5: mk(0, 0, 0, null, [0xffffffff, [[0xb1, 1], [0xb2, 2], null]]),
      f6: mk(0, 0, 0, null, [0, [[0xc1, 0], [0xc2, 0], [0xc3, 1]]]),
      // MultiSound_setVoiceBit1On: bit 1 of +0xc8 clear (0x14) / set with bit 0 (the voice loop) / set without
      // bit 0 (bit 7 of +0xc8).
      v0: mk(0, 0, 0), v1: mk(0xfffffffd, 0, 0), v2: mk(0x3, 0, 0, V), v3: mk(0x2, 0, 0),
      v4: mk(0x82, 0, 0), v5: mk(0xffffffff, 0, 0, V), v6: mk(0x7e, 0, 0), v7: mk(0x80000003, 0, 0, [[0xff, 1.0]]),
      // Channel_collectFlagBits / MultiSound_getStatus
      s0: st(null, 0, 0, 0), s1: st(null, 1, 0x3f, 0), s2: st(0, 0, 0x01, 1), s3: st(0xffffff00, 0x30, 0x08, 0),
      s4: st(0x12345678, 0, 0x02, 0x228), s5: st(null, 0, 0x04, 0), s6: st(null, 0, 0x10, 5), s7: st(0x100, 0, 0x20, 0),
      s8: st(0x80, 0, 0xffffffc0, 0), s9: st(null, 0xffffffff, 0xffffffff, 0xffffffff), s10: st(0x7, 0, 0x2a, 0),
      s11: st(-1, 0, 0x15, 0),
      // Channel_setField5cClamped1200 (with / without a device) and Channel_setField4Masked7f
      c0: mk(0, 0, 1), c1: mk(0, 0, 0),
      // MultiSound::setVolume
      w0: vol(0, 0x00000000, 0, 0, 0, 0, 0), w1: vol(1, 0x10000000, 0, 1, 0, 0, 0),
      w2: vol(1, 0x10000001, 1, 1, 0x40, 1, 1), w3: vol(0, 0x10000009, 0, 1, 0x7f, 1, 0),
      w4: vol(1, 0x00000008, 1, 0, 0, 0, 0), w5: vol(-1, 0xfffffffe, 0, 1, 1, 1, 1),
      // MultiSound_setPitchCents
      p0: vol(0, 0, 0, 0, 0, 0, 0), p1: vol(1, 0, 1, 0, 0, 0, 0), p2: vol(0, 0x10000000, 1, 0, 0, 0, 0),
      p3: vol(7, 0, 0, 0, 0, 0, 0),
    };
  },

  // --- Channel_allocAndInitVoice: 0x1400 channel blocks; the reported index is written at +0x13f8 -----------------
  c3cx_alloc() {
    const F = globalThis.DIFF_FIXTURES;
    const mk = (f58) => {
      const p = F.c3cx_fill(__keepAlloc(0x1400), 0x1400, 0);
      for (let o = 0; o < 0x13f8; o += 4) p.add(o).writeU32((0xa1000000 + o) >>> 0);
      p.add(0x58).writeU32(f58 >>> 0);
      p.add(0x1f0).writeDouble(1.25);
      p.add(0x200).writeDouble(-640.5);
      p.add(0x1388).writeDouble(1e-310);      // a denormal, copied bit for bit by fld/fstp
      p.add(0x1e0).writeDouble(0.1);
      p.add(0x22c).writeU32(0x22c22c22);
      for (let i = 0; i < 16; ++i) {
        const v = p.add(i * 0x110);
        v.add(0x2d4).writeU8(0xff);
        v.add(0x2cc).writeU8(0x80 + i);
      }
      p.add(0x13f8).writeU32(0x13f813f8);
      return p;
    };
    const masks = {
      x0: 0x00000000,       // bit 0 clear
      x1: 0x10000001,       // bit 28 set
      x2: 0x00000001,       // slot 0
      x3: 0x00007011,       // slots 0..2 busy, bit 4 set
      x4: 0x0000b001,       // slots 0, 1, 3 busy: slot 2
      x5: 0x0ffff001,       // all sixteen busy
      x6: 0x07fff001,       // slot 15
      x7: 0xe0000fff,       // slot 0, high bits set, bit 28 clear
      x8: 0x0fffefff,       // bit 12 clear (slot 0) with bits 0..11 and 13..27 set
      x9: 0x0ff7f001,       // slot 7
      x10: 0x00001fd1,      // slot 1 (bit 12 busy, bit 13 clear)
      x11: 0xefffffff,      // all busy, bit 28 clear
    };
    const out = {};
    for (const k of Object.keys(masks)) { out[k] = mk(masks[k]); out[k + 'o'] = out[k].add(0x13f8); }
    return out;
  },

  // --- getDeviceCaps and getNodeByIndex ----------------------------------------------------------------------------
  // getDeviceCaps: 0x200 blocks; the interface at +0x100 (vtable slot +0x10, stdcall (interface, caps): bumps
  // +0x184, records the caps pointer's low 12 bits at +0x188, and when +0x18c is non-zero writes +0x180 to caps+0x48,
  // i.e. the object's +0x54). getNodeByIndex: 0x400 blocks; nodes at +0x200 (0x20 each: +0 string, +0x10 next),
  // strings at +0x300, a destination buffer at +0x380 (0x40 bytes of 0xaa).
  c3cx_dev() {
    const F = globalThis.DIFF_FIXTURES;
    const vt = F.c3cx_fill(__keepAlloc(0x100), 0x100, 0);
    vt.add(0x10).writePointer(F.c3cx_keep(new NativeCallback(function (com, caps) {
      com.add(0x84).writeU32((com.add(0x84).readU32() + 1) >>> 0);
      com.add(0x88).writeU32(caps.toUInt32() & 0xfff);
      if (com.add(0x8c).readU32() !== 0) caps.add(0x48).writeU32(com.add(0x80).readU32());
      return 0;
    }, 'int', ['pointer', 'pointer'], 'stdcall')));
    const caps = (hasCom, pre54, val, writes) => {
      const p = F.c3cx_fill(__keepAlloc(0x200), 0x200, 0);
      for (let o = 4; o < 0x100; o += 4) p.add(o).writeU32((0xca000000 + o) >>> 0);
      p.add(0x54).writeU32(pre54 >>> 0);
      p.writePointer(hasCom ? p.add(0x100) : ptr(0));
      p.add(0x100).writePointer(vt);
      p.add(0x180).writeU32(val >>> 0);
      p.add(0x18c).writeU32(writes);
      return p;
    };
    const out = {
      d0: caps(0, 0x11111111, 5, 1), d1: caps(0, 0xfffffffe, 6, 1),
      d2: caps(1, 0, 0x22, 1), d3: caps(1, 0x33, 0x44, 0), d4: caps(1, 0, 0xffffffff, 1),
      d5: caps(1, 0x80000000, 0x7fffffff, 1), d6: caps(1, 0x55, 0x55, 0), d7: caps(1, 1, 0, 1),
      d8: caps(1, 0x12345678, 0x9abcdef0, 1), d9: caps(1, 0xdeadbeef, 0x10, 0),
      d10: caps(1, 0, 0x400, 1), d11: caps(0, 0, 0x1, 0),
    };
    // getNodeByIndex. count: +0x148; names: node strings (null = no node list at all).
    const list = (count, names) => {
      const p = F.c3cx_fill(__keepAlloc(0x400), 0x400, 0);
      for (let o = 0; o < 0x200; o += 4) p.add(o).writeU32((0x4e000000 + o) >>> 0);
      p.add(0x148).writeU32(count >>> 0);
      F.c3cx_fill(p.add(0x380), 0x40, 0xaa);
      if (names === null) { p.add(4).writePointer(ptr(0)); return p; }
      for (let i = 0; i < names.length; ++i) {
        const node = p.add(0x200 + i * 0x20);
        const s = p.add(0x300 + i * 0x20);
        s.writeUtf8String(names[i]);
        node.writePointer(s);
        node.add(0x10).writePointer(i + 1 < names.length ? p.add(0x200 + (i + 1) * 0x20) : ptr(0));
      }
      p.add(4).writePointer(names.length ? p.add(0x200) : ptr(0));
      return p;
    };
    out.n0 = list(0, ['Alpha']);
    out.n1 = list(3, null);
    out.n2 = list(3, ['Alpha', 'Beta device', 'Gamma 3']);
    out.n3 = list(5, ['One', 'Two']);
    out.n4 = list(1, ['']);
    out.n5 = list(0xffffffff, ['Xy', 'Zw', 'Q']);
    for (const k of ['n0', 'n1', 'n2', 'n3', 'n4', 'n5']) out[k + 'b'] = out[k].add(0x380);
    return out;
  },

  // --- Wave_Device::open's initialisers: ten 0x100 blocks (initStreamBuffer) / 0x200 blocks (initSoundVoice), each
  // filled with its own byte so that a field left unwritten reads differently per vector. initStreamBuffer's
  // operator new 0x100424d7 is replaced for the key by the heap stand-in (re/frida/js/heapstandin.js): both arms get
  // the same 0x20000-byte block from the fixture arena, whose cursor is in the state region.
  c3cx_stream() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    for (let i = 0; i < 10; ++i) out['b' + i] = F.c3cx_fill(__keepAlloc(0x100), 0x100, (0x13 + i * 0x17) & 0xff);
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], arenaSize: 0x20100 });
    out.arena = h.arena;
    return out;
  },
  c3cx_voice() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    for (let i = 0; i < 10; ++i) out['u' + i] = F.c3cx_fill(__keepAlloc(0x200), 0x200, (0x29 + i * 0x1d) & 0xff);
    return out;
  },

  // --- __free_lc_time: six 0xb0-byte structs whose 44 dwords are distinct values (never dereferenced: free
  // 0x10042c07 is replaced for the key by the heap stand-in's recording deallocator, which only logs them), one of
  // them with zero fields. The log (count at +0, pointers from +4) is in the state region.
  c3cx_lctime() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    for (let i = 0; i < 6; ++i) {
      const p = F.c3cx_fill(__keepAlloc(0xb0), 0xb0, 0);
      for (let o = 0; o < 0xb0; o += 4) p.add(o).writeU32(i === 5 && (o & 8) ? 0 : ((0x1c000000 + i * 0x10000 + o) >>> 0));
      out['l' + i] = p;
    }
    const h = __heapStandIn({ module: 'sound.dll', free: [0x42c07] });
    out.freelog = h.log;
    return out;
  },

  // --- Ctrl5b19c_ctor: ten 0x100 blocks of 0xa5 ----------------------------------------------------------------
  c3cx_ctrl() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    for (let i = 0; i < 10; ++i) out['k' + i] = F.c3cx_fill(__keepAlloc(0x100), 0x100, 0xa5);
    return out;
  },

  // --- ostreamResetVtable: 0x100 blocks; `this` is block+0x20, the virtual-base table pointer at block+0x18 points
  // to block+0x80, whose second dword (+0x84) is the offset. The destination is block+0x18+offset; offsets that
  // would land on the table pointer itself or on the offset dword are not used (the tail call re-reads both).
  c3cx_ostream() {
    const F = globalThis.DIFF_FIXTURES;
    const offs = [0x8, 0x10, 0x28, 0x40, 0x60, 0xc, 0xfffffff8, 0x4, 0x30, 0x70, 0x9c, 0x1d];
    const out = {};
    offs.forEach((off, i) => {
      const p = F.c3cx_fill(__keepAlloc(0x100), 0x100, 0);
      for (let o = 0; o < 0x100; o += 4) p.add(o).writeU32((0x05000000 + o) >>> 0);
      p.add(0x18).writePointer(p.add(0x80));
      p.add(0x80).writeU32(0x80808080);
      p.add(0x84).writeU32(off >>> 0);
      out['o' + i] = p;
      out['o' + i + 't'] = p.add(0x20);
    });
    return out;
  },

  // --- the old small-block heap: one 0x5000 block, the region at +0 (+0x10 = the page base, entries from +0x18)
  // and four pages from +0x1000. A page: +0 its map pointer, +4 its run length, +8..+0xf8 its map (one byte per
  // paragraph), the rest 0xee (non-zero, so every zero-run walk stops inside the page).
  //   page 0: A at map 0 (5), free 5..9, B at 10 (3), free 13..19, C at 20 (0x10), D at 36 (0xbc, up to 0xe0),
  //           E at 0xe0 (8), free 0xe8..0xef; map pointer = map+5; entry free count 0x20.
  //   page 1: F at map 0 (6), G at 6 (3), rest D-like; entry free count 0xea (+6 reaches 0xf0).
  //   page 2: entry free count 0xf0 (+0 reaches it); H at map 0 (0, a zero-length block); map pointer = map+0xe9.
  //   page 3: J at map 0xe0 (8), free 0xe8..0xef; map pointer = map+0xe9; entry free count 0xef (+1 is 0xf0
  //           for K at map 0x10, length 1).
  // counter: the value written to the module counter 0x100b5590 (saved first, restored at the flush).
  c3cx_sbh(counter) {
    const F = globalThis.DIFF_FIXTURES;
    const blk = F.c3cx_fill(__keepAlloc(0x5000), 0x5000, 0);
    const R = blk, P = blk.add(0x1000);
    R.add(0x10).writePointer(P);
    for (let o = 0; o < 0x10; o += 4) R.add(o).writeU32((0x7e000000 + o) >>> 0);
    const pg = (i) => P.add(i * 0x1000);
    const map = (i) => pg(i).add(8);
    const entry = (i, free) => { R.add(0x18 + i * 8).writeU32(free); R.add(0x1c + i * 8).writeU32(0x0e0e0e0e); };
    for (let i = 0; i < 4; ++i) {
      F.c3cx_fill(pg(i).add(0xf8), 0x1000 - 0xf8, 0xee);
      pg(i).add(4).writeU32(0x44444444);
    }
    const block = (i, at, len) => { map(i).add(at).writeU8(len); for (let k = 1; k < len; ++k) map(i).add(at + k).writeU8(0xc0); };
    // page 0
    block(0, 0, 5); block(0, 10, 3); block(0, 20, 0x10); block(0, 36, 0xbc); block(0, 0xe0, 8);
    pg(0).writePointer(map(0).add(5)); entry(0, 0x20);
    // page 1
    block(1, 0, 6); block(1, 6, 3); block(1, 9, 0xd7);
    pg(1).writePointer(map(1).add(0xef)); entry(1, 0xea);
    // page 2
    block(2, 1, 0xef);
    pg(2).writePointer(map(2).add(0xe9)); entry(2, 0xf0);
    // page 3
    block(3, 0, 0x10); map(3).add(0x10).writeU8(1); block(3, 0x11, 0xcf); block(3, 0xe0, 8);
    pg(3).writePointer(map(3).add(0xe9)); entry(3, 0xef);
    const cnt = F.c3cx_snd(0xb5590);
    __freeze.setU32(cnt, counter);
    const out = { R: R, cnt: cnt };
    for (let i = 0; i < 4; ++i) out['p' + i] = pg(i);
    // map bytes the vectors name
    out.A = map(0); out.B = map(0).add(10); out.C = map(0).add(20); out.E = map(0).add(0xe0);
    out.F = map(1); out.G = map(1).add(6); out.H = map(2); out.L = map(2).add(1);
    out.J = map(3).add(0xe0); out.K = map(3).add(0x10);
    return out;
  },
  c3cx_sbh_c0() { return globalThis.DIFF_FIXTURES.c3cx_sbh(0); },
  c3cx_sbh_c1f() { return globalThis.DIFF_FIXTURES.c3cx_sbh(0x1f); },

});
