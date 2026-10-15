// Fixtures for C3 batch c3fp (sound.dll renderChannelStreamed 0x10036660, round 35). Layouts come from the disassembly cited
// in shim/src/re/c3fp.cpp and the frame map in log/c3/c3fp_notes.txt section 1. Every object, buffer and stub here belongs to
// the fixture; all values are synthetic.
//
// Per vector one arena of 0x7000 bytes (the vector's state region $v<i>), marker-filled, holding:
//   +0x0000 ch   the channel (0x1400): +8 obj, +0x54 ds, +0x58 / +0x5c flags, doubles +0x1c8 step, +0x1d0 target, +0x1e0 value,
//                +0x210 bit-3 bound, +0x1370 counter, +0x1374 mask, +0x1378 base, +0x13a0 P, +0x13a4 Q, records +0x2d4 + 0x110 k
//   +0x1400 X    the successor (0x1480): byte +0xcc (bit 2: mmio stream), +0x78 ring o2; its channel part sub = X + 0x70:
//                +0xc8 = sub+0x58 flags, +0x13e4 = sub+0x1374 mask, +0x13e8 = sub+0x1378 base, +0x1400 = sub+0x1390 stream M
//   +0x2900 P    the holder (0x1380): +0 X, +0x58 flags, +0x1370 counter
//   +0x3c80 obj  the source ring {+0x14 size, +0x18 start, +0x1c read, +0x20 limit, +0x24 end} over s1
//   +0x3cc0 o2   the successor's ring over s2
//   +0x3d00 M    the successor's mmio stream {+0x1c read, +0x20 limit} over s2, +0xb4 = M2 (what closeMmioSound's stub installs)
//   +0x3e00 M2   the next stream over s3
//   +0x3f00 ds   the DsBuffer: word +0xb4 block align; what the stubs return: +0x40 want, +0x44 offset, +0x48 getPosition's
//                result, +0x4c bit 0 = leave the out cells alone, +0x50 / +0x54 / +0x58 / +0x5c restore's p1 / n1 / p2 / n2
//   +0x4000 Q    a cell; +0x4010 out (a3)
//   +0x4100 s1, +0x4900 s2, +0x5100 s3 (sources, 0x800 each), +0x5900 p1, +0x6100 p2 (locked segments, 0x800 each)
// Each key: build, freeze the first thread and hold the multimedia timer off (c3fl's quietTimer, copied: 0x100b49a8 = 5),
// swap g_soundEngine 0x100b4a20 for a marker block, stub the seven thunks the body calls (A/B thread only; recording into $log,
// frame pointers logged as 0), then set the x87 control word last (c3fh's x87State, copied: fninit + fldcw), so both arms enter
// with it. Restores run at the flush, newest first.
(function () {
  const F = globalThis.DIFF_FIXTURES;
  const keep = function (x) { globalThis.__diffKeepAlive.push(x); return x; };
  const snd = function (rva) { return Process.getModuleByName('sound.dll').base.add(rva); };
  function fill(p, n, b) { p.writeByteArray(new Array(n).fill(b)); return p; }
  function marker(p, n, tag) { for (let k = 0; k + 4 <= n; k += 4) p.add(k).writeU32((tag + k * 0x01010101) >>> 0); return p; }
  const dv = new DataView(new ArrayBuffer(8));
  function wd(p, v) {
    if (typeof v === 'object') { p.writeU32(v.lo >>> 0); p.add(4).writeU32(v.hi >>> 0); return; }   // raw bits
    dv.setFloat64(0, v, true);
    p.writeU32(dv.getUint32(0, true));
    p.add(4).writeU32(dv.getUint32(4, true));
  }
  const k32 = Process.getModuleByName('kernel32.dll');
  const sleep = keep(new NativeFunction(k32.getExportByName('Sleep'), 'void', ['uint32'], 'stdcall', { scheduling: 'exclusive' }));

  // ---- x87 control word (c3fh's x87State, copied) ----
  let cw = null;
  function cwFns() {
    if (cw) return cw;
    const page = keep(Memory.alloc(Process.pageSize));
    // getcw: sub esp, 4; fnstcw [esp]; movzx eax, word [esp]; add esp, 4; ret
    page.writeByteArray([0x83, 0xEC, 0x04, 0xD9, 0x3C, 0x24, 0x0F, 0xB7, 0x04, 0x24, 0x83, 0xC4, 0x04, 0xC3]);
    // setcw(cw): fninit; fldcw [esp+4]; ret
    page.add(16).writeByteArray([0xDB, 0xE3, 0xD9, 0x6C, 0x24, 0x04, 0xC3]);
    Memory.protect(page, Process.pageSize, 'rwx');
    cw = { get: keep(new NativeFunction(page, 'uint32', [])), set: keep(new NativeFunction(page.add(16), 'void', ['uint32'])) };
    return cw;
  }
  function x87State(word) {
    const f = cwFns();
    const saved = f.get();
    f.set(word);
    __freeze.atFlush(function () { f.set(saved); });
  }

  // ---- freeze + quiet timer (c3fl's, copied) ----
  let frozenNow = false;
  function guard() {
    if (frozenNow) return;
    __freeze.thread();
    frozenNow = true;
    __freeze.atFlush(function () { frozenNow = false; });
    __freeze.negOn();
  }
  function quietTimer(out) {
    guard();
    const g = snd(0xb49a8), owner = snd(0xb4990 + 0xc);   // CRITICAL_SECTION.OwningThread
    let idle = null;
    for (let i = 0; i < 400; i++) {
      if (g.readU32() === 0 && owner.readU32() === 0) { idle = 0; break; }
      sleep(1);
    }
    if (idle === null) {
      if (owner.readU32() !== 0) throw new Error('c3fp: timer section 0x100b4990 stays owned');
      idle = g.readU32();
    }
    g.writeU32(5);
    __freeze.atFlush(function () { g.writeU32(idle); });
    let stable = 0;
    for (let i = 0; i < 1000 && stable < 50; i++) {
      sleep(2);
      if (g.readU32() !== 5) { g.writeU32(5); stable = 0; continue; }
      if (owner.readU32() !== 0) { stable = 0; continue; }
      stable++;
    }
    if (stable < 50) throw new Error('c3fp: timer did not go quiet');
    out.gtick = g;
  }

  // ---- recording thunk stubs (c3fl's stubOn, copied): A/B thread only; entry {rva, ecx, args...} (0x20 bytes) ----
  const stubLive = {};
  function stubLog(sz) { const l = keep(Memory.alloc(sz)); fill(l, sz, 0); l.cap = Math.floor((sz - 4) / 0x20); return l; }
  function logCall(log, tag, a) {
    const n = log.readU32();
    if (n < log.cap) {
      const p = log.add(4 + 0x20 * n);
      p.writeU32(tag >>> 0);
      for (let i = 0; i < a.length && i < 7; i++) p.add(4 + 4 * i).writeU32(ptr(a[i]).toUInt32());
    }
    log.writeU32(n + 1);
  }
  function stubOn(log, rva, nargs, retfn, skip) {
    const base = snd(rva), key = base.toString();
    if (stubLive[key]) { try { Interceptor.revert(base); } catch (e) { /* gone */ } delete stubLive[key]; Interceptor.flush(); }
    const tid = Process.getCurrentThreadId();
    const argt = ['pointer'].concat(Array(nargs).fill('pointer'));
    const orig = keep(new NativeFunction(base, 'uint32', argt, 'thiscall'));
    const cb = keep(new NativeCallback(function () {
      const a = Array.prototype.slice.call(arguments);
      if (Process.getCurrentThreadId() !== tid) return orig.apply(null, a);
      const n = log.readU32();
      logCall(log, rva, a.map(function (x, i) { return (skip && skip.indexOf(i) >= 0) ? ptr(0) : x; }));
      const r = retfn ? retfn(a, n) : 0;
      return (typeof r === 'number') ? (r >>> 0) : ptr(r).toUInt32();
    }, 'uint32', argt, 'thiscall'));
    Interceptor.replace(base, cb);
    Interceptor.flush();
    stubLive[key] = true;
    __freeze.atFlush(function () {
      if (!stubLive[key]) return;
      Interceptor.revert(base);
      Interceptor.flush();
      delete stubLive[key];
    });
  }

  // ---- one vector's object graph ----
  const NAN = { lo: 0, hi: 0x7ff80000 };
  const ARENA = 0x7000;
  const O = { ch: 0, X: 0x1400, P: 0x2900, obj: 0x3c80, o2: 0x3cc0, M: 0x3d00, M2: 0x3e00, ds: 0x3f00, Q: 0x4000, out: 0x4010,
              s1: 0x4100, s2: 0x4900, s3: 0x5100, p1: 0x5900, p2: 0x6100 };
  const DEF = {
    ds: 1, gp: [0, 0x40, 0x80, 0], al: 4, n1: null, n2: 0, p2: 0,
    rd: 0x10, lim: 0x400, end: 0x600, size: 0x200, fl: 0, g: 0, P: 0, Q: 1,
    cc: 4, xf: 0, xmask: 0x7ff, xbase: 0, mrd: 0x10, mlim: 0x100, m2rd: 0x08, m2lim: 0x200,
    ord: 0x10, olim: 0x200, oend: 0x300, osize: 0x100,
    V: 0.5, A: 0.125, T: 0.25, T2: 1.0, base: 0, mask: 0x7ff,
  };
  function build(out, i, spec, tag) {
    const s = Object.assign({}, DEF, spec);
    const v = marker(keep(Memory.alloc(ARENA)), ARENA, tag);
    const at = function (k) { return v.add(O[k]); };
    const ch = at('ch'), X = at('X'), P = at('P'), obj = at('obj'), o2 = at('o2'), M = at('M'), M2 = at('M2'), ds = at('ds');
    // ch
    ch.add(8).writePointer(obj);
    ch.add(0x54).writePointer(s.ds ? ds : ptr(0));
    ch.add(0x58).writeU32(s.fl >>> 0); ch.add(0x5c).writeU32(s.g >>> 0);
    wd(ch.add(0x1c8), s.A); wd(ch.add(0x1d0), s.T); wd(ch.add(0x1e0), s.V); wd(ch.add(0x210), s.T2);
    ch.add(0x1370).writeU32(0x1370 + i); ch.add(0x1374).writeU32(s.mask >>> 0);
    ch.add(0x1378).writePointer(at('s1').add(s.base));
    ch.add(0x13a0).writePointer(s.P ? P : ptr(0));
    ch.add(0x13a4).writePointer(s.Q ? at('Q') : ptr(0));
    // the sixteen record bytes the hand-over rewrites ((b & 0xfd) | 4, 0x10036ec3-0x10036edd): bits 1 and 2 both ways
    for (let k = 0; k < 16; k++) ch.add(0x2d4 + 0x110 * k).writeU8((0x33 + 0x2b * k + i) & 0xff);
    // obj (main ring over s1)
    obj.add(0x14).writeU32(s.size >>> 0); obj.add(0x18).writePointer(at('s1'));
    obj.add(0x1c).writePointer(at('s1').add(s.rd)); obj.add(0x20).writePointer(at('s1').add(s.lim));
    obj.add(0x24).writePointer(at('s1').add(s.end));
    // ds
    const want = s.gp[1];
    ds.add(0xb4).writeU16(s.al & 0xffff);
    ds.add(0x40).writeU32(want >>> 0); ds.add(0x44).writeU32(s.gp[2] >>> 0); ds.add(0x48).writeU32(s.gp[0] >>> 0);
    ds.add(0x4c).writeU32(s.gp[3] ? 1 : 0);
    ds.add(0x50).writePointer(at('p1')); ds.add(0x54).writeU32((s.n1 === null ? want : s.n1) >>> 0);
    ds.add(0x58).writePointer(s.p2 ? at('p2') : ptr(0)); ds.add(0x5c).writeU32(s.n2 >>> 0);
    // successor
    P.writePointer(X); P.add(0x58).writeU32(0x5858 + i); P.add(0x1370).writeU32(0x1370 + 0x10 * i);
    X.add(0xcc).writeU8(s.cc & 0xff); X.add(0x78).writePointer(o2);
    X.add(0xc8).writeU32(s.xf >>> 0); X.add(0x13e4).writeU32(s.xmask >>> 0); X.add(0x13e8).writePointer(at('s2').add(s.xbase));
    X.add(0x1400).writePointer(M);
    M.add(0x1c).writePointer(at('s2').add(s.mrd)); M.add(0x20).writePointer(at('s2').add(s.mlim)); M.add(0xb4).writePointer(M2);
    M2.add(0x1c).writePointer(at('s3').add(s.m2rd)); M2.add(0x20).writePointer(at('s3').add(s.m2lim));
    o2.add(0x14).writeU32(s.osize >>> 0); o2.add(0x18).writePointer(at('s2'));
    o2.add(0x1c).writePointer(at('s2').add(s.ord)); o2.add(0x20).writePointer(at('s2').add(s.olim));
    o2.add(0x24).writePointer(at('s2').add(s.oend));
    out['v' + i] = v; out['c' + i] = ch; out['o' + i] = at('out');
  }

  // ---- vector tables (counts mirrored in re/frida/registry.d/c3fp.py) ----
  // COPY: the copy loop and the unchained end; the fade is not entered (flag 3 of +0x58 and bit 4 of +0x5c clear).
  const COPY = [
    { ds: 0 },                                                                    //  0 no DsBuffer: 0x13
    { gp: [7, 0x40, 0x80, 0] },                                                   //  1 getPosition 7: returned
    { gp: [0x2d, 0, 0, 1], fl: 0x02, rd: 0x100, size: 0x200 },                    //  2 0x2d, nothing locked; notice (0x100 > 0x80)
    { gp: [0x2d, 0x40, 0x80, 0], al: 3, fl: 0x02 },                               //  3 one segment, q = 0x15; no notice (0x50 <= 0x80)
    { gp: [0, 0x60, 0x80, 0], al: 0, n1: 0x20, p2: 1, n2: 0x40, fl: 0x402 },      //  4 two segments, q = 0; +0x58 bit 10 set
    { gp: [0, 0x30, 0x80, 0], n1: 0x40, fl: 0x23 },                               //  5 step shorter than the segment, no flag 0x10
    { gp: [0, 0x40, 0x80, 0], n1: 0, p2: 1, n2: 0x40 },                           //  6 empty first segment
    { gp: [0, 0x70, 0x80, 0], n1: 0x20, p2: 1, n2: 0x20 },                        //  7 more than both segments: p2 taken three times
    { gp: [0, 0x100, 0x80, 0], rd: 0x300, lim: 0x80, end: 0x380, fl: 0x20 },      //  8 wrapped ring: bit 0 of +0x5c, wrap to start
    { gp: [0, 0x40, 0x80, 0], g: 1 },                                             //  9 bit 0 of +0x5c preset: wrap after the step
    { gp: [0, 0x40, 0x80, 0], rd: 0x40, lim: 0x60, fl: 0x20 },                    // 10 underrun: flag 0x10 set, rest zero-filled
    { gp: [0, 0x80, 0x80, 0], rd: 0x300, lim: 0x80, end: 0x340, fl: 0x10 },       // 11 flag 0x10 preset, clamp at the end: zero fill
    { gp: [0, 0x40, 0x80, 0], rd: 0x40, lim: 0x60, fl: 0x14, P: 1 },              // 12 flags 4 + 0x10, successor, no 0x20: fill
    { gp: [0, 0x40, 0x80, 0], rd: 0x40, lim: 0x60, fl: 0x24, n1: 0x20, p2: 1, n2: 0x20 },   // 13 flags 4 + 0x20, no successor
    { gp: [0, 0x40, 0x80, 0], fl: 0x10 },                                         // 14 flag 0x10, segment used up: no fill
    { gp: [0, 0x40, 0x80, 0], rd: 0x40, lim: 0x60, fl: 0, n1: 0x20, p2: 1, n2: 0x20 },      // 15 clamp with no flag
    { gp: [0, 0x37, 0x80, 0], rd: 0x13, al: 7, n1: 0x15, p2: 1, n2: 0x22, fl: 0x02, size: 0x20 },   // 16 odd sizes; notice
    { gp: [0, 0x40, 0x80, 0], rd: 0x40, lim: 0x60, fl: 0x04, P: 1, n1: 0x20, p2: 1, n2: 0x20 },     // 17 flag 4, successor, no 0x20
    { gp: [0, 0x48, 0x80, 0], rd: 0x700, lim: 0x20, end: 0x710, fl: 0x22, n1: 0x48 },             // 18 wrap with flag 0x20 and 2
  ];
  // CHAIN: the hand-over to the successor (P set, flag 0x20, an underrun at 0x60 that sets flag 0x10), then the successor end.
  const CH0 = { gp: [0, 0x80, 0x80, 0], rd: 0x40, lim: 0x60, fl: 0x20, P: 1 };
  const CHAIN = [
    { cc: 4, mrd: 0x10, mlim: 0x100 },                                            //  0 stream, whole segment: cursor stored
    { cc: 4, mrd: 0x10, mlim: 0x60 },                                             //  1 stream clamp, cursor 0x20 stored, one more pass
    { cc: 4, mrd: 0x10, mlim: 0x30 },                                             //  2 stream ends: closeMmioSound, next stream M2
    { cc: 0, ord: 0x10, olim: 0x200, xf: 0x02 },                                  //  3 ring: +0x1c advanced; notice (0x70 > 0x40)
    { cc: 0, ord: 0x300, olim: 0x80, oend: 0x380, xf: 0 },                        //  4 wrapped ring; successor flag 2 clear
    { cc: 0, xf: 0x22 },                                                          //  5 successor flags 0x21
    { cc: 0, xf: 0x402 },                                                         //  6 successor bit 10 set
    { cc: 0, xf: 0x02, xbase: 0x40 },                                             //  7 no notice (0x30 <= 0x40)
    { cc: 0, gp: [0, 0xa0, 0x80, 0], n1: 0x80, p2: 1, n2: 0x20, xf: 0x02 },       //  8 ring, second segment after the hand-over
    { cc: 4, fl: 0x24, mrd: 0x10, mlim: 0x100 },                                  //  9 flags 4 + 0x20 with a successor
    { cc: 4, mrd: 0x10, mlim: 0x48 },                                             // 10 two hand-overs in one call
    { cc: 4, mrd: 0x10, mlim: 0x30, fl: 0x20, g: 0x80 },                          // 11 as 2 with +0x5c bit 7 preset
    { cc: 0, ord: 0x13, osize: 0x40, xf: 0x02, xmask: 0x3f, gp: [0, 0x83, 0x80, 0], n1: 0x83 },   // 12 odd sizes, masked notice
  ].map(function (o) { return Object.assign({}, CH0, o); });
  // FADE: nothing locked (getPosition 0x2d, out cells untouched), the fade (0x10036a82-0x10036c63) per vector.
  const FD0 = { gp: [0x2d, 0, 0, 1], Q: 0 };
  const FADE = [
    { fl: 0, g: 0 },                                                    //  0 no fade
    { fl: 8, g: 2, Q: 1 },                                              //  1 stop request, Q cleared
    { fl: 0, g: 0x12 },                                                 //  2 stop request through +0x5c bit 4, no Q
    { fl: 8, g: 8, V: 1.0, T2: 2.0 },                                   //  3 bit 3, V below the bound
    { fl: 8, g: 8, V: 3.0, T2: 2.0 },                                   //  4 bit 3, bound reached: bits cleared
    { fl: 8, g: 8, V: NAN, T2: 2.0 },                                   //  5 bit 3, V unordered
    { g: 0x10, A: 0.25, V: 0.5, T: 1.0 },                               //  6 step > 0, V below the target: bit 4 cleared
    { g: 0x10, A: 0.25, V: 0.5, T: 0.5 },                               //  7 step > 0, V at the target
    { g: 0x10, A: 0.75, V: 0.7, T: 0.1 },                               //  8 step > 0, V < step: V = T
    { g: 0x10, A: 0.1, V: 0.9, T: 0.2 },                                //  9 step > 0: V - A
    { g: 0x10, A: 1e-17, V: 1.0, T: 0.5 },                              // 10 V - A rounds differently at 53 / 64 bits
    { g: 0x10, A: -0.25, V: 0.6, T: 0.5 },                              // 11 step <= 0, V above the target: cleared
    { g: 0x10, A: -0.1, V: 0.2, T: 0.9 },                               // 12 step <= 0, |A| < T - V: V - A
    { g: 0x10, A: -0.5, V: 0.2, T: 0.4 },                               // 13 step <= 0, |A| >= T - V: V = T
    { g: 0x10, A: 0.0, V: 0.2, T: 0.4 },                                // 14 step 0.0
    { g: 0x10, A: NAN, V: 0.2, T: 0.4 },                                // 15 step unordered
    { g: 0x10, A: 0.5, V: NAN, T: 0.4 },                                // 16 step > 0, V unordered: cleared
    { g: 0x10, A: -0.5, V: NAN, T: 0.4 },                               // 17 step <= 0, V unordered
    { fl: 8, g: 0, V: 0.0, A: 0.5, Q: 1 },                              // 18 V == 0: stop, Q cleared, step captured
    { fl: 8, g: 0, V: 0.0, A: 0.5 },                                    // 19 V == 0 without Q
    { fl: 8, g: 0, V: 0.75, A: 0.25 },                                  // 20 V > A: V - A
    { fl: 8, g: 0, V: 1.0, A: 1e-17 },                                  // 21 unrounded V - A into the volume (53 / 64 bits)
    { fl: 8, g: 0, V: NAN, A: 1.0, Q: 1 },                              // 22 V unordered
    { g: 0x10, A: 1.0, V: 1e300, T: 0.0 },                              // 23 volume out of __ftol's range
    { fl: 0, g: 0x18, V: 3.0, T2: 2.0 },                                // 24 bits 3 and 4: bit 3 first
    { fl: 8, g: 0x0a, Q: 1 },                                           // 25 bits 1 and 3: bit 1 first
    { g: 0x10, A: -1e-16, V: 1 - Math.pow(2, -53), T: 1.0 },            // 26 V - A rounds up at 53 bits, down at 64 toward zero
    { g: 0x10, A: 0.1, V: 0.7, T: 0.0 },                                // 27 odd fractions
    { fl: 8, g: 0, V: -0.5, A: 0.25 },                                  // 28 V < 0 <= A: step captured
    { fl: 8, g: 0, V: 2.0, A: 0.5 },                                    // 29 V - A = 1.5
    { g: 0x10, A: 0.5, V: 5.0, T: 1.0 },                                // 30 volume above 127
    { g: 0x10, A: 2.0, V: 1.5, T: 1.0 },                                // 31 V < A: V = T
    { fl: 8, g: 0, V: 1 / 3, A: 1e-9 },                                 // 32 1/3 - 1e-9 unrounded
  ].map(function (o) { return Object.assign({}, FD0, o); });
  const GROUPS = { copy: COPY, chain: CHAIN, fade: FADE };
  F.c3fp_counts = { copy: COPY.length, chain: CHAIN.length, fade: FADE.length };

  function buildKey(group, word) {
    const out = {};
    const tbl = GROUPS[group];
    const gtag = { copy: 0xa1000000, chain: 0xa2000000, fade: 0xa3000000 }[group];
    tbl.forEach(function (spec, i) { build(out, i, spec, gtag + 0x10000 * i); });
    out.log = stubLog(0x2000);
    const eng = marker(keep(Memory.alloc(0x40)), 0x40, 0xae000000);
    out.eng = eng;
    quietTimer(out);
    out.geng = snd(0xb4a20);
    __freeze.setU32(out.geng, eng.toUInt32());
    const L = out.log;
    // DsBuffer::getPosition(want*, off*, 0): fills the cells from ds+0x40 / +0x44 unless ds+0x4c bit 0, returns ds+0x48
    stubOn(L, 0x233d, 3, function (a) {
      const d = a[0];
      if (!(d.add(0x4c).readU32() & 1)) { a[1].writeU32(d.add(0x40).readU32()); a[2].writeU32(d.add(0x44).readU32()); }
      return d.add(0x48).readU32();
    }, [1, 2]);
    // DsBuffer::restore(off, want, &p1, &n1, &p2, &n2): the segments from ds+0x50..+0x5c
    stubOn(L, 0x213f, 6, function (a) {
      const d = a[0];
      a[3].writePointer(d.add(0x50).readPointer()); a[4].writeU32(d.add(0x54).readU32());
      a[5].writePointer(d.add(0x58).readPointer()); a[6].writeU32(d.add(0x5c).readU32());
      return 0x213f;
    }, [3, 4, 5, 6]);
    stubOn(L, 0x1ec9, 4, function () { return 0x1ec9; });                       // DsBuffer::unlock(p1, n1, p2, n2)
    stubOn(L, 0x18f7, 1, function () { return 0x18f7; });                       // DsBuffer::setVolume(vol)
    // closeMmioSound(sub): installs the next stream, as the original's 0x10035eb4 does (sub+0x1390 = old stream's +0xb4)
    stubOn(L, 0x1b18, 0, function (a) {
      const s = a[0], m = s.add(0x1390).readPointer();
      s.add(0x1390).writePointer(m.add(0xb4).readPointer());
      return 0;
    });
    stubOn(L, 0x11d6, 3, function () { return 1; });                            // postSoundThreadMessage(0x7eb, obj, 0)
    stubOn(L, 0x23f1, 1, function () { return 0x23f1; });                       // addToSoundList(X), ecx = g_soundEngine
    x87State(word);
    return out;
  }
  [['copy', 'w27f', 0x027f], ['chain', 'w27f', 0x027f], ['fade', 'w27f', 0x027f], ['fade', 'wf7f', 0x0f7f],
   ['copy', 'wf7f', 0x0f7f], ['chain', 'wf7f', 0x0f7f]].forEach(function (k) {
    F['c3fp_' + k[0] + '_' + k[1]] = function () { return buildKey(k[0], k[2]); };
  });
})();
