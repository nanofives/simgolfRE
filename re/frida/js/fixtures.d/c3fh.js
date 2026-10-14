// C3 batch c3fh (round 34): fixtures for the x87-faithful pilot (shim/src/re/c3fh.cpp, re/frida/registry.d/c3fh.py).
// Synthetic voice records, sample buffers, cursors and output buffers only; nothing here touches the running engine.
// The probe (log/c3/alt/c3fh_probe_menu.txt) found no game thread entering any Voice_mix* leaf in 60 s at the menu, so
// the keys need no frozen thread; the wrong-body switch (__freeze.negOn) is armed per key and restored at the flush.
//
// x87 state: every key first sets the A/B thread to the state the mixers leave behind (fninit, control word 0x0f7f), so
// the first original arm and every later arm enter with the same control word (the ramp division before `fninit` runs at
// the entering word). The thread's word from before the key is put back (with fninit, which also drops the registers the
// mixers leave on the stack) at the flush.
(function () {
  const PAGE = Process.pageSize;
  let cw = null;
  function cwFns() {
    if (cw) return cw;
    const page = Memory.alloc(PAGE);
    // getcw: sub esp, 4; fnstcw [esp]; movzx eax, word [esp]; add esp, 4; ret
    page.writeByteArray([0x83, 0xEC, 0x04, 0xD9, 0x3C, 0x24, 0x0F, 0xB7, 0x04, 0x24, 0x83, 0xC4, 0x04, 0xC3]);
    // setcw(cw): fninit; fldcw [esp+4]; ret
    page.add(16).writeByteArray([0xDB, 0xE3, 0xD9, 0x6C, 0x24, 0x04, 0xC3]);
    Memory.protect(page, PAGE, 'rwx');
    cw = { get: new NativeFunction(page, 'uint32', []), set: new NativeFunction(page.add(16), 'void', ['uint32']) };
    return cw;
  }
  function x87State(word) {
    const f = cwFns();
    const saved = f.get();
    f.set(word);
    __freeze.atFlush(function () { f.set(saved); });
    return saved;
  }
  globalThis.__c3fhX87 = { cwFns: cwFns, x87State: x87State };

  // doubles through a DataView (NativePointer.writeDouble(Infinity) stores a quiet NaN: c3fe notes, Frida 17.22.1)
  const dv = new DataView(new ArrayBuffer(8));
  function wd(p, v) {
    if (typeof v === 'object') { p.writeU32(v.lo >>> 0); p.add(4).writeU32(v.hi >>> 0); return; }   // raw bits
    dv.setFloat64(0, v, true);
    p.writeU32(dv.getUint32(0, true));
    p.add(4).writeU32(dv.getUint32(4, true));
  }

  const OUTSZ = 0x1000, OBJSZ = 0x1400, SSZ = 0x40, LEAD = 64, TOTAL = 256;
  // deterministic sample words: a mix of LCG values and the extremes
  function word16(k) {
    const special = { 3: 32767, 4: -32768, 9: -1, 10: 0, 17: 32767, 18: 32767, 30: -32768, 31: 1 };
    if (special[k] !== undefined) return special[k];
    let x = (Math.imul(k + 77, 0x9E3779B1) >>> 0) ^ 0x5bd1e995;
    x = Math.imul(x ^ (x >>> 15), 0x2c1b3c6d) >>> 0;
    return ((x >>> 8) & 0xffff) << 16 >> 16;
  }
  function byte8(k) {
    const special = { 3: 255, 4: 0, 9: 128, 10: 127, 17: 255, 30: 0, 31: 1 };
    if (special[k] !== undefined) return special[k];
    const x = Math.imul(k + 31, 0x9E3779B1) >>> 0;
    return (x >>> 13) & 0xff;
  }
  function mkSamples(sb) {
    const buf = Memory.alloc(TOTAL * sb + 16);
    for (let i = 0; i < TOTAL; i++) {
      const k = i - LEAD;
      if (sb === 2) buf.add(i * 2).writeS16(word16(k)); else buf.add(i).writeU8(byte8(k));
    }
    return buf.add(LEAD * sb);           // sample index 0
  }
  function outInit(out, seed) {
    const vals = [1e-17, 3.3, -2.2e15, 0.1, -0.0, 7.5e-310, 123456.789, -1.0000000000000002, 2.5, -4096.125];
    for (let i = 0; i < OUTSZ / 8; i++) wd(out.add(i * 8), vals[(i * 7 + seed) % vals.length] * (1 + (i % 5)));
  }

  // Mono resampler records (Voice_mixSet/AddMono16, Voice_mixSet/Add8bit). Indices are sample positions from index 0.
  // mode 1 reads the record's +0x1f0 / +0x200 gains, +0x1380 position, +0x1388 step, +0x58 flags (8 ramp, 0x20 wrap),
  // the byte +0x5c (bit 0 set = no loop), +0x1c8 the ramp volume, +0x1378 the wrap value, and the sub-object at +8
  // (+0x18 loop start, +0x1c / +0x20 / +0x24: end = +0x20 unless +0x1c is above it, then +0x24). Other modes read
  // +0x80 / +0x70 gains, +0x100 position, +0x108 step, the byte +0x9c (8 ramp, 2 no loop), +0xf0 ramp volume, the byte
  // +0x98 (bit 0: loop), +0xd8 / +0xd4 loop start / end or +0x18 / +0x20 start / end.
  const MONO = [
    { m: 1, n: 12, step: 0.1, pos: 0.3, A: 0.7, B: 1.3, f58: 0, b5c: 1, cur: 0, s20: 40, s1c: 0, ls: 4 },                     // 0 count exit
    { m: 1, n: 200, step: 1 / 3, pos: 0, A: 0.25, B: -0.6, f58: 0, b5c: 1, cur: 0, s20: 20, s1c: 0, ls: 4 },                 // 1 end exit
    { m: 1, n: 100, step: 0.77, pos: 0.5, A: 0.9, B: 0.45, f58: 0, b5c: 0, cur: 2, s20: 30, s1c: 0, ls: 4 },                 // 2 loop wraps
    { m: 1, n: 60, step: 1.1, pos: 0.2, A: 0.6, B: 0.8, f58: 0x20, b5c: 0, cur: 10, s20: 5, s1c: 50, s24: 25, ls: 3, wrap: 60 },   // 3 wrap once
    { m: 1, n: 20, step: 0.9, pos: 0.0, A: 1.1, B: 0.3, f58: 0x20, b5c: 0, cur: 0, s20: 30, s1c: 0, ls: 2, wrap: 61 },       // 4 no loop (cursor <= +0x20)
    { m: 1, n: 30, step: 1.75, pos: 0.125, A: 0.5, B: 0.5, f58: 0, b5c: 1, cur: 0, s20: 10, s1c: 11, s24: 35, ls: 0 },       // 5 end = +0x24
    { m: 1, n: 10, step: 0.5, pos: 0.0, A: 1.0, B: 1.0, f58: 8, b5c: 1, vol: 1.0, cur: 0, s20: 40, s1c: 0, ls: 0 },         // 6 ramp 1/10
    { m: 1, n: 37, step: 1.0, pos: 0.0, A: 0.8, B: 0.6, f58: 8, b5c: 0, vol: 0.7, cur: 0, s20: 9, s1c: 0, ls: 2 },          // 7 ramp, loop, pad from loop start
    { m: 1, n: 10, step: 1.0, pos: 0.0, A: 0.8, B: 0.6, f58: 0, b5c: 1, cur: 0, s20: 6, s1c: 0, ls: 2 },                    // 8 pad 0, end exit
    { m: 2, n: 50, step: 0.6, pos: 0.0, A: 0.35, B: 0.65, c9c: 8, b98: 1, vol: 0.3, cur: 0, ls: 1, end: 12 },                // 9 mode 2 loop + ramp (fidiv)
    { m: 2, n: 60, step: 0.45, pos: 0.25, A: 0.35, B: 0.65, c9c: 2, b98: 1, cur: 0, ls18: 1, end20: 15 },                    // 10 mode 2 loop off (+0x9c bit 1)
    { m: 2, n: 7, step: 2.5, pos: 0.75, A: 0.2, B: 1.9, c9c: 8, b98: 0, vol: -2.5, cur: 0, ls18: 0, end20: 40 },            // 11 mode 2 ramp, no loop
    { m: 0, n: 9, step: 0.5, pos: -2.6, A: 0.3, B: 0.7, c9c: 0, b98: 0, cur: 10, ls18: 0, end20: 40 },                       // 12 negative position
    { m: 1, n: 5, step: 0.25, pos: 3e9, A: 1e-9, B: 1e-9, f58: 0, b5c: 1, cur: 0, s20: 40, s1c: 0, ls: 0, only16: true },   // 13 position past int32 (fist 0x80000000)
    { m: 1, n: 6, step: 1.1102230246251565e-16, pos: 2.9999999999999996, A: 0.75, B: 1.25, f58: 0, b5c: 1, cur: 0, s20: 40, s1c: 0, ls: 0 },   // 14 rounding at an integer
    { m: 1, n: 8, step: 0.37, pos: 0.1, A: -1.7976931348623157e308, B: 1e308, f58: 0, b5c: 1, cur: 15, s20: 40, s1c: 0, ls: 0 },   // 15 overflow
    { m: 1, n: 8, step: 0.1, pos: 0.0, A: 3.0e-310, B: 4.9406564584124654e-324, f58: 8, vol: 1e-320, b5c: 1, cur: 0, s20: 40, s1c: 0, ls: 0 },   // 16 denormals
    { m: 1, n: 3, step: { lo: 1, hi: 0x7ff80000 }, pos: 0.5, A: 1.0, B: Infinity, f58: 0, b5c: 1, cur: 0, s20: 40, s1c: 0, ls: 0, only16: true },   // 17 NaN step, inf gain
    { m: 1, n: 0, step: 1.5, pos: 0.0, A: 0.5, B: -0.5, f58: 0, b5c: 1, cur: 0, s20: 6, s1c: 0, ls: 0 },                    // 18 n = 0 runs to the end
    { m: 1, n: 0x80000003, step: 2.0, pos: 0.0, A: 0.5, B: 0.25, f58: 8, vol: 1.0, b5c: 1, cur: 0, s20: 8, s1c: 0, ls: 0 },   // 19 negative n, ramp
    { m: 1, n: 10, step: -0.75, pos: 20.0, A: 0.4, B: 0.6, f58: 0, b5c: 1, cur: 10, s20: 60, s1c: 0, ls: 0 },               // 20 backwards
    { m: 1, n: 64, step: 0.7071067811865476, pos: 0.123456789, A: 0.9999999999999999, B: 1.0000000000000002, f58: 8, vol: 0.3333333333333333, b5c: 0, cur: 1, s20: 23, s1c: 0, ls: 5 },   // 21 long odd-fraction run
    { m: 1, n: 4, step: 0.6, pos: 0.1, A: 0.6, B: 0.8, f58: 0x20, b5c: 0, cur: 10, s20: 5, s1c: 50, s24: 25, ls: 3, wrap: 60 },   // 22 wrap armed, count exit before the end
  ];

  function buildMono(sb, keep) {
    const base = mkSamples(sb);
    const at = (i) => base.add(i * sb);
    const fx = {};
    MONO.forEach(function (c, i) {
      const o = Memory.alloc(OBJSZ), s = Memory.alloc(SSZ), p = Memory.alloc(16), out = Memory.alloc(OUTSZ);
      outInit(out, i);
      // both mode groups are filled, so a body reading the wrong group reads different values
      const other = { A: 0.0625 + i, B: -0.03125 - i, pos: 7.0 + i / 8, step: 3.0 + i / 16 };
      const g1 = c.m === 1 ? c : other, g2 = c.m === 1 ? other : c;
      wd(o.add(0x1f0), g1.B); wd(o.add(0x200), g1.A); wd(o.add(0x1380), g1.pos); wd(o.add(0x1388), g1.step);
      wd(o.add(0x80), g2.B); wd(o.add(0x70), g2.A); wd(o.add(0x100), g2.pos); wd(o.add(0x108), g2.step);
      o.add(8).writePointer(s);
      o.add(0x58).writeU32(c.f58 || 0);
      o.add(0x5c).writeU32(c.b5c === undefined ? 0x10 : (c.b5c | 0x10));   // bit 4 is not read by the mixers
      wd(o.add(0x1c8), c.m === 1 && c.vol !== undefined ? c.vol : 0.5);
      wd(o.add(0xf0), c.m !== 1 && c.vol !== undefined ? c.vol : -0.75);
      o.add(0x1378).writePointer(at(c.wrap !== undefined ? c.wrap : 63));
      o.add(0x9c).writeU8(c.c9c || 0);
      o.add(0x98).writeU8(c.b98 || 0);
      o.add(0xd8).writePointer(at(c.m !== 1 && c.ls !== undefined ? c.ls : 0));
      o.add(0xd4).writePointer(at(c.m !== 1 && c.end !== undefined ? c.end : 44));
      o.add(0x18).writePointer(at(c.ls18 !== undefined ? c.ls18 : 3));
      o.add(0x20).writePointer(at(c.end20 !== undefined ? c.end20 : 45));
      s.add(0x18).writePointer(at(c.m === 1 ? c.ls : 7));
      s.add(0x1c).writePointer(at(c.s1c !== undefined ? c.s1c : 0));
      s.add(0x20).writePointer(at(c.s20 !== undefined ? c.s20 : 46));
      s.add(0x24).writePointer(at(c.s24 !== undefined ? c.s24 : 47));
      p.writePointer(at(c.cur));
      const use = !(c.only16 && sb === 1);
      fx['o' + i] = o; fx['s' + i] = s; fx['p' + i] = p; fx['out' + i] = out;
      // 8-bit: the two rows whose fist leaves 0x80000000 would step the byte cursor 2 GB away; the record runs as row 0
      fx['n' + i] = use ? c.n : MONO[0].n;
      fx['m' + i] = c.m;
      if (!use) { wd(o.add(0x1380), MONO[0].pos); wd(o.add(0x1388), MONO[0].step); }
    });
    x87State(0x0f7f);
    __freeze.negOn();
    return fx;
  }

  // Stereo 16-bit records: positions are frames (4 bytes); no step or position field.
  const STEREO = [
    { m: 1, n: 10, A: 0.3, B: -1.7, f58: 0, b5c: 1, cur: 0, s20: 40, s1c: 0, ls: 0 },                 // 0 count exit
    { m: 1, n: 50, A: 0.6, B: 0.4, f58: 0, b5c: 1, cur: 0, s20: 12, s1c: 0, ls: 0 },                  // 1 end exit
    { m: 1, n: 30, A: 0.9, B: 1.1, f58: 0, b5c: 0, cur: 0, s20: 9, s1c: 0, ls: 3 },                   // 2 loop wraps
    { m: 1, n: 40, A: 0.7, B: 0.2, f58: 0x20, b5c: 0, cur: 10, s20: 5, s1c: 50, s24: 20, ls: 2 },    // 3 wrap once, then end
    { m: 1, n: 20, A: 0.15, B: 0.85, f58: 0x20, b5c: 0, cur: 0, s20: 30, s1c: 0, ls: 2 },             // 4 cursor <= +0x20: no loop
    { m: 1, n: 10, A: 1.0, B: 1.0, f58: 8, vol: 1.0, b5c: 1, cur: 0, s20: 40, s1c: 0, ls: 0 },        // 5 ramp 1/10
    { m: 1, n: 25, A: 0.5, B: 0.75, f58: 8, vol: -0.3, b5c: 0, cur: 0, s20: 7, s1c: 0, ls: 1 },       // 6 ramp + loop
    { m: 2, n: 33, A: 0.25, B: 0.125, c9c: 8, b98: 1, vol: 0.9, cur: 0, ls: 2, end: 11 },            // 7 mode 2 loop + ramp
    { m: 2, n: 40, A: 0.45, B: 0.55, c9c: 2, b98: 1, cur: 0, ls18: 1, end20: 15 },                    // 8 mode 2 loop off
    { m: 0, n: 6, A: 1.5, B: -0.5, c9c: 0, b98: 0, cur: 3, ls18: 0, end20: 40 },                      // 9 mode 0
    { m: 1, n: 8, A: -1e308, B: 1e308, f58: 0, b5c: 1, cur: 15, s20: 40, s1c: 0, ls: 0 },             // 10 overflow
    { m: 1, n: 3, A: 3e-310, B: 4.9406564584124654e-324, f58: 8, vol: 1e-320, b5c: 1, cur: 0, s20: 40, s1c: 0, ls: 0 },   // 11 denormals
    { m: 1, n: 0, A: 0.5, B: 0.5, f58: 0, b5c: 1, cur: 0, s20: 5, s1c: 0, ls: 0 },                    // 12 n = 0 runs to the end
    { m: 1, n: 9, A: 0.33, B: 0.66, f58: 0, b5c: 1, cur: 0, s20: 0, s1c: 0, ls: 0, endOdd: true },     // 13 end two bytes into a frame
    { m: 1, n: 7, A: 1.0, B: 1.0, f58: 8, vol: 3.3, b5c: 1, cur: 0, s20: 40, s1c: 0, ls: 0 },         // 14 ramp through zero
    { m: 1, n: 4, A: 0.7, B: 0.2, f58: 0x20, b5c: 0, cur: 10, s20: 5, s1c: 50, s24: 30, ls: 2 },     // 15 wrap armed, count exit before the end
  ];

  function buildStereo() {
    const base = mkSamples(2);
    const at = (i) => base.add(i * 4);
    const fx = {};
    STEREO.forEach(function (c, i) {
      const o = Memory.alloc(OBJSZ), s = Memory.alloc(SSZ), p = Memory.alloc(16), out = Memory.alloc(OUTSZ);
      outInit(out, i + 3);
      const other = { A: 0.0625 + i, B: -0.03125 - i };
      const g1 = c.m === 1 ? c : other, g2 = c.m === 1 ? other : c;
      wd(o.add(0x1f0), g1.B); wd(o.add(0x200), g1.A);
      wd(o.add(0x80), g2.B); wd(o.add(0x70), g2.A);
      o.add(8).writePointer(s);
      o.add(0x58).writeU32(c.f58 || 0);
      o.add(0x5c).writeU32(c.b5c === undefined ? 0x10 : (c.b5c | 0x10));
      wd(o.add(0x1c8), c.m === 1 && c.vol !== undefined ? c.vol : 0.5);
      wd(o.add(0xf0), c.m !== 1 && c.vol !== undefined ? c.vol : -0.75);
      o.add(0x9c).writeU8(c.c9c || 0);
      o.add(0x98).writeU8(c.b98 || 0);
      o.add(0xd8).writePointer(at(c.m !== 1 && c.ls !== undefined ? c.ls : 0));
      o.add(0xd4).writePointer(at(c.m !== 1 && c.end !== undefined ? c.end : 44));
      o.add(0x18).writePointer(at(c.ls18 !== undefined ? c.ls18 : 3));
      o.add(0x20).writePointer(at(c.end20 !== undefined ? c.end20 : 45));
      s.add(0x18).writePointer(at(c.m === 1 ? c.ls : 7));
      s.add(0x1c).writePointer(at(c.s1c !== undefined ? c.s1c : 0));
      s.add(0x20).writePointer(c.endOdd ? at(5).add(2) : at(c.s20 !== undefined ? c.s20 : 46));
      s.add(0x24).writePointer(at(c.s24 !== undefined ? c.s24 : 47));
      p.writePointer(at(c.cur));
      fx['o' + i] = o; fx['s' + i] = s; fx['p' + i] = p; fx['out' + i] = out;
      fx['n' + i] = c.n; fx['m' + i] = c.m;
    });
    x87State(0x0f7f);
    __freeze.negOn();
    return fx;
  }

  // Stream resamplers (streamRenderBlock / streamRenderTail): 16-bit samples, the step is a constant of the function.
  // Mode 1 reads the sub-object at +8 like the mono mixers (no cursor test on the wrap arm); other modes read +0x100 pos,
  // +0x98 / +0x9c loop bits, and a chain when +0x9c bit 0 is set: +0xb4 -> buffer {+0x18 first start, +0xd4 end, +0xb4 next,
  // +0xd8 start used when hopping into it}. Both gain groups are filled (Block reads +0x80 / +0x70, Tail +0x1f0 / +0x200).
  const STREAM = [
    { m: 1, n: 12, pos: 0.3, A: 0.7, B: 1.3, f58: 0, b5c: 1, cur: 0, s20: 40, s1c: 0, ls: 4 },                       // 0 count exit
    { m: 1, n: 200, pos: 0, A: 0.25, B: -0.6, f58: 0, b5c: 1, cur: 0, s20: 20, s1c: 0, ls: 4 },                      // 1 end exit
    { m: 1, n: 100, pos: 0.5, A: 0.9, B: 0.45, f58: 0, b5c: 0, cur: 2, s20: 30, s1c: 0, ls: 4 },                     // 2 loop wraps
    { m: 1, n: 60, pos: 0.2, A: 0.6, B: 0.8, f58: 0x20, b5c: 0, cur: 1, s20: 10, s1c: 0, ls: 3, wrap: 60 },          // 3 wrap once, then end
    { m: 1, n: 30, pos: 0.0, A: 1.1, B: 0.3, f58: 8, vol: 0.7, b5c: 1, cur: 0, s20: 40, s1c: 0, ls: 0 },             // 4 ramp
    { m: 1, n: 30, pos: 0.125, A: 0.5, B: 0.5, f58: 0, b5c: 1, cur: 0, s20: 10, s1c: 11, s24: 35, ls: 0 },           // 5 end = +0x24
    { m: 1, n: 0, pos: 0.0, A: 0.5, B: -0.5, f58: 0, b5c: 1, cur: 0, s20: 6, s1c: 0, ls: 0 },                        // 6 n = 0 runs to the end
    { m: 1, n: 64, pos: 0.123456789, A: 0.9999999999999999, B: 1.0000000000000002, f58: 8, vol: 0.3333333333333333, b5c: 0, cur: 1, s20: 23, s1c: 0, ls: 5 },   // 7 odd, ramp + loop
    { m: 1, n: 5, pos: 3e9, A: 1e-9, B: 1e-9, f58: 0, b5c: 1, cur: 0, s20: 40, s1c: 0, ls: 0 },                     // 8 fist 0x80000000
    { m: 1, n: 40, pos: 0.0, A: 0.8, B: 0.6, f58: 0, b5c: 0, cur: 0, s20: 9, s1c: 0, ls: 2 },                        // 9 pad from the loop start
    { m: 2, n: 50, pos: 0.25, A: 0.35, B: 0.65, b98: 0, c9c: 0, cur: 0, ls18: 0, end20: 20 },                        // 10 no loop, no chain
    { m: 2, n: 60, pos: 0.0, A: 0.35, B: 0.65, b98: 1, c9c: 0, cur: 0, ls: 2, end: 12 },                             // 11 loop
    { m: 2, n: 40, pos: 0.0, A: 0.2, B: 1.9, b98: 1, c9c: 2, cur: 0, ls18: 1, end20: 15 },                           // 12 loop off by +0x9c bit 1
    { m: 2, n: 90, pos: 0.5, A: 0.45, B: 0.55, b98: 0, c9c: 1, cur: 0, end20: 10, chain: [[15, 25], [30, 38]] },      // 13 two hops, then the chain ends
    { m: 2, n: 40, pos: 0.0, A: 1.5, B: -0.5, b98: 0, c9c: 1, cur: 3, end20: 9, chain: [[20, 26]] },                  // 14 one buffer, then the chain ends
    { m: 2, n: 50, pos: 0.0, A: 0.7, B: 0.3, b98: 1, c9c: 1, cur: 0, ls: 1, end: 30, end20: 8, chain: [[12, 20], [3, 9]] },   // 15 chain with loop bit
    { m: 0, n: 9, pos: -2.6, A: 0.3, B: 0.7, b98: 0, c9c: 0, cur: 10, ls18: 0, end20: 40 },                          // 16 mode 0, negative position
    { m: 2, n: 14, pos: 0.0, A: 0.6, B: 0.4, b98: 0, c9c: 1, cur: 0, end20: 6, chain: [[10, 30], [31, 40]] },          // 17 count exit inside the chain
    { m: 1, n: 8, pos: 0.1, A: -1.7976931348623157e308, B: 1e308, f58: 0, b5c: 1, cur: 15, s20: 40, s1c: 0, ls: 0 },  // 18 overflow
    { m: 1, n: 8, pos: 0.0, A: 3.0e-310, B: 4.9406564584124654e-324, f58: 8, vol: 1e-320, b5c: 1, cur: 0, s20: 40, s1c: 0, ls: 0 },   // 19 denormals
    { m: 1, n: 5, pos: 0.25, A: 0.6, B: 0.8, f58: 0x20, b5c: 0, cur: 0, s20: 30, s1c: 0, ls: 3, wrap: 61 },          // 20 wrap armed, count exit before the end
  ];
  function buildStream() {
    const base = mkSamples(2);
    const at = (i) => base.add(i * 2);
    const fx = {};
    STREAM.forEach(function (c, i) {
      const o = Memory.alloc(OBJSZ), s = Memory.alloc(SSZ), p = Memory.alloc(16), out = Memory.alloc(OUTSZ);
      outInit(out, i + 5);
      fill0(o, OBJSZ);
      wd(o.add(0x1f0), c.B); wd(o.add(0x200), c.A); wd(o.add(0x80), c.B); wd(o.add(0x70), c.A);
      wd(o.add(0x1380), c.m === 1 ? c.pos : 9.5); wd(o.add(0x100), c.m === 1 ? 4.25 : c.pos);
      o.add(8).writePointer(s);
      o.add(0x58).writeU32(c.f58 || 0);
      o.add(0x5c).writeU32(c.b5c === undefined ? 0x10 : (c.b5c | 0x10));
      wd(o.add(0x1c8), c.vol !== undefined ? c.vol : 0.5);
      o.add(0x1378).writePointer(at(c.wrap !== undefined ? c.wrap : 63));
      o.add(0x98).writeU8(c.b98 || 0);
      o.add(0x9c).writeU8(c.c9c || 0);
      o.add(0xcc).writeU32(0x100 + i);
      o.add(0xd8).writePointer(at(c.ls !== undefined && c.m !== 1 ? c.ls : 0));
      o.add(0xd4).writePointer(at(c.end !== undefined ? c.end : 44));
      o.add(0x18).writePointer(at(c.ls18 !== undefined ? c.ls18 : 3));
      o.add(0x20).writePointer(at(c.end20 !== undefined ? c.end20 : 45));
      s.add(0x18).writePointer(at(c.m === 1 ? c.ls : 7));
      s.add(0x1c).writePointer(at(c.s1c !== undefined ? c.s1c : 0));
      s.add(0x20).writePointer(at(c.s20 !== undefined ? c.s20 : 46));
      s.add(0x24).writePointer(at(c.s24 !== undefined ? c.s24 : 47));
      let prev = o;
      (c.chain || []).forEach(function (b, k) {
        const blk = Memory.alloc(0x100);
        fill0(blk, 0x100);
        blk.add(0x18).writePointer(at(b[0]));       // the start used for the first hop (read through +0xb4 of the record)
        blk.add(0xd8).writePointer(at(b[0] + 1));   // the start used when hopping in from the previous buffer
        blk.add(0xd4).writePointer(at(b[1]));
        prev.add(0xb4).writePointer(blk);
        fx['b' + i + '_' + k] = blk;
        prev = blk;
      });
      if (!c.chain) o.add(0xb4).writePointer(ptr(0));
      p.writePointer(at(c.cur));
      fx['o' + i] = o; fx['s' + i] = s; fx['p' + i] = p; fx['out' + i] = out;
      fx['n' + i] = c.n; fx['m' + i] = c.m;
    });
    x87State(0x0f7f);
    __freeze.negOn();
    return fx;
  }
  function fill0(p, n) { p.writeByteArray(new Array(n).fill(0)); }

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3fh_stream() { return buildStream(); },
    c3fh_mono16() { return buildMono(2); },
    c3fh_mono8() { return buildMono(1); },
    c3fh_stereo16() { return buildStereo(); },
  });
  globalThis.__c3fhCounts = { mono: MONO.length, stereo: STEREO.length };
})();

// ---- C. The caller-word helpers (section C of shim/src/re/c3fh.cpp) --------------------------------------------------
// Every key sets the A/B thread's control word first (0x027f, the thread's own, or 0x0f7f, what a mixer leaves), so the
// same vectors run at 53 bits to nearest and at 64 bits toward zero. Keys whose body reads g_soundEngine 0x100b4a20 or
// g_soundTimer 0x100b49ec swap those globals for fixture objects with the first thread frozen and the multimedia timer
// held off (c3fe's quietTimer, copied: the engine is live on this PC, log/c3/alt/c3fh_probe_menu.txt, g_soundReady = 1).
// The engine's rate slot +0x40 and the DirectSound buffer's SetVolume (+0x3c) / Stop (+0x48) are native stubs that count
// their calls inside the fixture object (no JS on the call path, so nothing runs between two x87 instructions of a body).
(function () {
  const F = globalThis.DIFF_FIXTURES;
  const keep = function (x) { globalThis.__diffKeepAlive.push(x); return x; };
  const snd = function (rva) { return Process.getModuleByName('sound.dll').base.add(rva); };
  function fill(p, n, b) { p.writeByteArray(new Array(n).fill(b)); return p; }
  function marker(p, n, tag) { for (let k = 0; k + 4 <= n; k += 4) p.add(k).writeU32((tag + k * 0x01010101) >>> 0); return p; }
  function block(n, tag) { return marker(keep(Memory.alloc(n)), n, tag); }
  const dv = new DataView(new ArrayBuffer(8));
  function wd(p, v) {
    if (typeof v === 'object') { p.writeU32(v.lo >>> 0); p.add(4).writeU32(v.hi >>> 0); return; }   // raw bits
    dv.setFloat64(0, v, true);
    p.writeU32(dv.getUint32(0, true));
    p.add(4).writeU32(dv.getUint32(4, true));
  }
  const NAN = { lo: 0, hi: 0x7ff80000 }, SNAN = { lo: 1, hi: 0x7ff00000 }, INF = { lo: 0, hi: 0x7ff00000 };
  const k32 = Process.getModuleByName('kernel32.dll');
  const sleep = keep(new NativeFunction(k32.getExportByName('Sleep'), 'void', ['uint32'], 'stdcall', { scheduling: 'exclusive' }));

  let code = null;
  function stubs() {
    if (code) return code;
    const page = keep(Memory.alloc(Process.pageSize));
    // +0x00 engine slot +0x40 (thiscall, none): inc dword [ecx+8]; mov eax, [ecx+4]; ret
    page.writeByteArray([0xff, 0x41, 0x08, 0x8b, 0x41, 0x04, 0xc3]);
    // +0x10 SetVolume (stdcall this, vol): log vol at [this+8+4*k] (k = [this+4], 0..7), k = (k+1) & 7, return [this+0x28]
    page.add(0x10).writeByteArray([0x8b, 0x44, 0x24, 0x04, 0x8b, 0x48, 0x04, 0x8b, 0x54, 0x24, 0x08, 0x89, 0x54, 0x88, 0x08,
                                   0x41, 0x83, 0xe1, 0x07, 0x89, 0x48, 0x04, 0x8b, 0x40, 0x28, 0xc2, 0x08, 0x00]);
    // +0x30 Stop (stdcall this): inc dword [this+0x2c]; xor eax, eax; ret 4
    page.add(0x30).writeByteArray([0x8b, 0x44, 0x24, 0x04, 0xff, 0x40, 0x2c, 0x33, 0xc0, 0xc2, 0x04, 0x00]);
    Memory.protect(page, Process.pageSize, 'rwx');
    const engVt = fill(keep(Memory.alloc(0x100)), 0x100, 0);
    engVt.add(0x40).writePointer(page);
    const comVt = fill(keep(Memory.alloc(0x100)), 0x100, 0);
    comVt.add(0x3c).writePointer(page.add(0x10));
    comVt.add(0x48).writePointer(page.add(0x30));
    code = { engVt: engVt, comVt: comVt };
    return code;
  }
  function engine(rate) {
    const e = fill(keep(Memory.alloc(0x10)), 0x10, 0);
    e.writePointer(stubs().engVt); e.add(4).writeU32(rate >>> 0);
    return e;
  }
  function comBuffer(ret) {
    const c = fill(keep(Memory.alloc(0x30)), 0x30, 0);
    c.writePointer(stubs().comVt); c.add(0x28).writeU32(ret >>> 0);
    return c;
  }

  let frozenNow = false;
  function guard() {
    if (frozenNow) return;
    __freeze.thread();
    frozenNow = true;
    __freeze.atFlush(function () { frozenNow = false; });
  }
  function quietTimer(out) {                      // copied from re/frida/js/fixtures.d/c3fe.js (round 33)
    guard();
    const g = snd(0xb49a8), owner = snd(0xb4990 + 0xc);
    let idle = null;
    for (let i = 0; i < 400; i++) {
      if (g.readU32() === 0 && owner.readU32() === 0) { idle = 0; break; }
      sleep(1);
    }
    if (idle === null) {
      if (owner.readU32() !== 0) throw new Error('c3fh: timer section 0x100b4990 stays owned');
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
    if (stable < 50) throw new Error('c3fh: timer did not go quiet');
  }
  // eng / tmr: null (NULL global) or a number (engine rate / timer divisor). Builds first, then freezes and swaps.
  function swapGlobals(fx, eng, tmr) {
    fx.eng = engine(eng === null ? 0 : eng);
    fx.tmr = fill(keep(Memory.alloc(0x10)), 0x10, 0);
    if (tmr !== null) fx.tmr.writeU32(tmr >>> 0);
    globalThis.__c3fhX87.cwFns();
    quietTimer(fx);
    __freeze.setU32(snd(0xb4a20), eng === null ? 0 : fx.eng.toUInt32());
    __freeze.setU32(snd(0xb49ec), tmr === null ? 0 : fx.tmr.toUInt32());
  }
  function finish(fx, word) {
    globalThis.__c3fhX87.x87State(word);
    __freeze.negOn();
    return fx;
  }

  // -- Seq_setTempo: (tempo, +0x1bc, old +0x718 (signed), +0x720 double) per vector
  const TEMPO = [
    [120, 96, 500000, 0.1], [1, 480, 600000, 1 / 3], [7, 7, -1, 2.5], [3, 0xffffffff, 0x7fffffff, 1e300],
    [0x7fffffff, 1, 0x80000000, -0.75], [0xffffffff, 120, 3, 123456.789], [0, 96, 500000, 0.1], [1000003, 0, 0, 7.0],
    [97, 389, 12345, NAN], [13, 11, 977, 1e-310], [0x80000000, 3, -7, 1e18], [44100, 0x10, 0x7ffffff0, 3e9],
  ];
  function buildTempo(word) {
    const fx = {};
    TEMPO.forEach(function (r, i) {
      const o = block(0x800, 0x51000000 + i * 0x100);
      o.add(0x1bc).writeU32(r[1] >>> 0); o.add(0x718).writeU32(r[2] >>> 0); wd(o.add(0x720), r[3]);
      fx['o' + i] = o; fx['t' + i] = r[0];
    });
    return finish(fx, word);
  }

  // -- DsBuffer::setVolume: (volume, master +0x188 or null, COM SetVolume's return or null for no buffer)
  const DSVOL = [
    [0x7f, null, 0], [64, 0x7f, 0x88780096], [0, 0x7f, 0], [1, 1, 5], [0x80, 0x40, 0], [0xffffffff, 100, null],
    [100, 0xffffffff, 7], [50, 50, null], [127, 0, 0], [33, 77, 1], [10, 126, null], [2, 3, 9], [127, 127, 0],
    [126, 127, 3], [127, 1, null],
  ];
  function buildDsVol(word) {
    const fx = {};
    DSVOL.forEach(function (r, i) {
      const o = block(0x100, 0x52000000 + i * 0x100);
      const m = block(0x200, 0x53000000 + i * 0x100);
      m.add(0x188).writeU32(r[1] === null ? 0 : r[1] >>> 0);
      o.add(0x5c).writePointer(r[1] === null ? ptr(0) : m);
      const c = comBuffer(r[2] === null ? 0 : r[2]);
      o.add(0x60).writePointer(r[2] === null ? ptr(0) : c);
      fx['o' + i] = o; fx['m' + i] = m; fx['c' + i] = c; fx['v' + i] = r[0];
    });
    return finish(fx, word);
  }

  // -- Channel objects (Channel_recomputeVoiceGains / setReverseRate / setRate). Per channel: flag word +0x58, +0x1c0,
  //    +0x1d8, +0x1e0 / +0x1f0 / +0x200 doubles, +0x208, +0x224 pan; per voice s = +0x324 + k * 0x110: [s-0x9c] int,
  //    [s-0x94] double, byte [s-0x50] (bit 3 skips the voice in setRate).
  const CH_F58 = [1, 1, 0, 9, 0x10000001, 1, 9, 1, 0x11, 1];
  const CH_RATE = [48000, 22050, 22049, 44101, 1, 0xfffffff0, 96001, 8000, 0x7fffffff, 3];
  const CH_IRATE = [-5, 1, 1, 0x10000, 0x10001, 0x7fffffff, 32767, 777, (0x8000ffff | 0), 12345];
  const CH_1C0 = [88211, 48000, 100003, 96001, 52919, 0x7fffffff, 45007, 48001, 61001, 99991];   // setRate's inner rate
  const CH_D = [0.1, 1 / 3, 0.7071067811865476, 2.5, 1e-310, -0.6, 65535.99999999999, 1e300, 0.30000000000000004, 7.25];
  function buildChannels(fx) {
    CH_F58.forEach(function (f, i) {
      const o = block(0x1500, 0x54000000 + i * 0x10000);
      o.add(0x58).writeU32(f >>> 0);
      o.add(0x1c0).writeU32(CH_1C0[i] >>> 0);
      o.add(0x1d8).writeU32((0x8000 + 0x111 * i) >>> 0);
      wd(o.add(0x1e0), CH_D[i]); wd(o.add(0x1f0), CH_D[(i + 1) % 10]); wd(o.add(0x200), CH_D[(i + 2) % 10]);
      o.add(0x208).writeU32((0x10000 + 0x1235 * i) >>> 0);
      o.add(0x224).writeU32(((i * 13) % 0x81 - 0x40) >>> 0);
      for (let k = 0; k < 16; k++) {
        const s = o.add(0x324 + k * 0x110);
        s.sub(0x9c).writeU32((0x8000 + 0x3a7 * k * (i + 1)) >>> 0);
        wd(s.sub(0x94), CH_D[(k + i) % 10] * (k + 1));
        s.sub(0x50).writeU8(((k + i) % 3 === 0) ? 8 : 0);
      }
      fx['o' + i] = o; fx['r' + i] = CH_RATE[i]; fx['ir' + i] = CH_IRATE[i];
    });
    return fx;
  }
  function buildChan(word, eng, tmr) {
    const fx = buildChannels({});
    swapGlobals(fx, eng, tmr);
    return finish(fx, word);
  }

  // -- Voice_resetDefaults: three voice records with different markers per key; the engine rate around the constant
  //    1000.0 at 0x1005b280.
  function buildVoice(word, eng) {
    const fx = {};
    for (let i = 0; i < 3; i++) fx['o' + i] = block(0x120, 0x55000000 + i * 0x1000 + (eng === null ? 0 : eng & 0xfff));
    swapGlobals(fx, eng, null);
    return finish(fx, word);
  }

  // -- Channel_stepVolumeFade: (f58, f5c, e +0x1e0, target +0x1d0, step +0x1c8, +0x210, +0x13a4); each channel's +0x54 is a
  //    DsBuffer with no master (+0x5c NULL) and a COM buffer at +0x60.
  const FADE = [
    [0, 0, 0.5, 0.3, 0.05, 0.7, 0],              //  0 no flag: returns at once
    [8, 2, 0.5, 0.3, 0.05, 0.7, 1],              //  1 stop request, +0x13a4 set
    [0, 0x12, 0.5, 0.3, 0.05, 0.7, 0],           //  2 stop request via +0x5c bit 4, +0x13a4 clear
    [8, 8, 0.5, 0.3, 0.05, 0.7, 0],              //  3 bit 3: e below +0x210, nothing
    [8, 8, 0.9, 0.3, 0.05, 0.7, 0],              //  4 bit 3: e reached +0x210, both bits cleared
    [8, 0x10, 0.8, 0.3, 0.05, 0.7, 0],           //  5 fade down: e - step
    [8, 0x10, 0.04, 0.01, 0.05, 0.7, 0],         //  6 fade down: e below step, lands on the target
    [8, 0x10, 0.2, 0.5, 0.05, 0.7, 0],           //  7 step > 0, e <= target: straight to the target
    [8, 0x10, 0.2, 0.5, -0.05, 0.7, 0],          //  8 step <= 0, e < target, |step| < gap: e - step
    [8, 0x10, 0.48, 0.5, -0.05, 0.7, 0],         //  9 step <= 0, |step| >= gap: the target
    [8, 0x10, 0.6, 0.5, -0.05, 0.7, 0],          // 10 step <= 0, e >= target: straight to the target
    [8, 0, 0.0, 0.5, 0.05, 0.7, 1],              // 11 no flag, e == 0.0: stop
    [8, 0, 0.9, 0.5, 0.1, 0.7, 0],               // 12 no flag, e > step: e - step (unrounded into the volume)
    [8, 0, 0.05, 0.5, 0.1, 0.7, 0],              // 13 no flag, e <= step: step = e, bit 1 set, volume 0
    [8, 0, NAN, 0.5, 0.1, 0.7, 1],               // 14 no flag, e unordered: stop
    [8, 0x10, 0.8, 0.3, NAN, 0.7, 0],            // 15 step unordered: the step <= 0 arm
    [8, 0, 1 / 3, 0.5, 0.1, 0.7, 0],             // 16 odd fractions, no flag
    [0, 0x10, 0.7, 0.3, 1e-17, 0.7, 0],          // 17 tiny step
    [8, 0x10, 0.7, 0.30000000000000004, 0.39999999999999997, 0.7, 0],   // 18 e - step near the target
    [8, 0, -0.25, 0.5, 0.1, 0.7, 0],             // 19 no flag, e negative (<= step)
    [0, 0x18, 0.9, 0.3, 0.05, 0.7, 0],           // 20 bits 3 and 4 in +0x5c: bit 3 wins
    [8, 0x10, 0.5, 0.5, 0.05, 0.7, 0],           // 21 e == target with step > 0: to the target
  ];
  function buildFade(word) {
    const fx = {};
    FADE.forEach(function (r, i) {
      const o = block(0x1400, 0x56000000 + i * 0x10000);
      o.add(0x58).writeU32(r[0]); o.add(0x5c).writeU32(r[1] | 0x100);
      wd(o.add(0x1e0), r[2]); wd(o.add(0x1d0), r[3]); wd(o.add(0x1c8), r[4]); wd(o.add(0x210), r[5]);
      o.add(0x13a4).writeU32(r[6]);
      const d = block(0x100, 0x57000000 + i * 0x100);
      d.add(0x5c).writePointer(ptr(0));
      const c = comBuffer(0x40 + i);
      d.add(0x60).writePointer(c);
      o.add(0x54).writePointer(d);
      fx['o' + i] = o; fx['d' + i] = d; fx['c' + i] = c;
    });
    return finish(fx, word);
  }

  // -- Sound5b6a8_ctor: objects with markers; g_soundTimer per key (its callee MidiReader_init 0x1001d380 reads it).
  function buildSnd5(word, tmr) {
    const fx = {};
    for (let i = 0; i < 3; i++) {
      const o = block(0xa00, 0x59000000 + i * 0x10000);
      o.add(0x718).writeU32([0x78, 0xffffffff, 0x12345][i]);
      fx['o' + i] = o;
    }
    swapGlobals(fx, null, tmr);
    return finish(fx, word);
  }

  const W = { w27f: 0x027f, wf7f: 0x0f7f };
  Object.keys(W).forEach(function (w) {
    F['c3fh_tempo_' + w] = function () { return buildTempo(W[w]); };
    F['c3fh_dsvol_' + w] = function () { return buildDsVol(W[w]); };
    F['c3fh_fade_' + w] = function () { return buildFade(W[w]); };
    F['c3fh_chanA_' + w] = function () { return buildChan(W[w], 22050, 7); };
    F['c3fh_chanB_' + w] = function () { return buildChan(W[w], 44100, 1000); };
    F['c3fh_snd5_' + w] = function () { return buildSnd5(W[w], 7); };
  });
  F.c3fh_chanN_w27f = function () { return buildChan(0x027f, null, 7); };          // no engine: the voice loop runs empty
  F.c3fh_chanT0_w27f = function () { return buildChan(0x027f, 22050, null); };     // no timer: returns after the voice loop
  F.c3fh_chanC_wf7f = function () { return buildChan(0x0f7f, 11025, 0xffffffff); };
  F.c3fh_chanD_w27f = function () { return buildChan(0x027f, 0x7fffffff, 3); };
  [['N', null], ['r999', 999], ['r1000', 1000], ['r1001', 1001], ['r3', 3], ['r0', 0], ['r22050', 22050]].forEach(function (v) {
    F['c3fh_voice_' + v[0] + '_w27f'] = function () { return buildVoice(0x027f, v[1]); };
    F['c3fh_voice_' + v[0] + '_wf7f'] = function () { return buildVoice(0x0f7f, v[1]); };
  });
  F.c3fh_snd5N_w27f = function () { return buildSnd5(0x027f, null); };

  // -- D. golf_clean.exe allocResampleTable 0x00481870. The first thread is frozen; g_83ac10 / g_83ac14 / g_83abb0 and the
  //    pixel format words the function reads through blitErrorPtr 0x00492450 (0x0083c310 +4, +8 .. +0x12) are saved, set
  //    and restored at the flush; _malloc 0x004a504f carves from a heap stand-in arena (both arms get the same addresses),
  //    `failMask` bit k makes the k-th allocation return NULL.
  const PIXFMT = {                                 // [+4 flag, shifts R G B (+8 +0xa +0xc), masks R G B (+0xe +0x10 +0x12)]
    f555: [0, 10, 5, 0, 0x7c00, 0x3e0, 0x1f],
    f565: [1, 11, 5, 0, 0xf800, 0x7e0, 0x1f],
    fodd: [0, 3, 9, 1, 0xf8, 0x7e00, 0x6],         // a blue shift of 1: the recombination puts blue back unshifted
  };
  function buildResample(fmt, word, failMask) {
    const f = PIXFMT[fmt];
    const rows = f[0] ? 0x10000 : 0x8000, maxLen = f[0] ? 6 : 10;
    const h = __heapStandIn({ module: 'golf_clean.exe', alloc: [0xa504f], arenaSize: rows * maxLen * 2 + maxLen * 8 + 0x100,
                              failMask: failMask || 0 });
    const fx = { arena: h.arena, freelog: h.log, g10: ptr(0x0083ac10), g14: ptr(0x0083ac14), gb0: ptr(0x0083abb0) };
    globalThis.__c3fhX87.cwFns();
    __freeze.thread();
    __freeze.save(ptr(0x0083ac10), 8);
    __freeze.save(ptr(0x0083abb0), 4);
    __freeze.save(ptr(0x0083c310), 0x14);
    const p = ptr(0x0083c310);
    p.add(4).writeU32(f[0]);
    [8, 0xa, 0xc, 0xe, 0x10, 0x12].forEach(function (off, k) { p.add(off).writeU16(f[1 + k]); });
    return finish(fx, word);
  }
  F.c3fh_resample_f555_w27f = function () { return buildResample('f555', 0x027f, 0); };
  F.c3fh_resample_f555_wf7f = function () { return buildResample('f555', 0x0f7f, 0); };
  F.c3fh_resample_fodd_w27f = function () { return buildResample('fodd', 0x027f, 0); };
  F.c3fh_resample_fodd_wf7f = function () { return buildResample('fodd', 0x0f7f, 0); };
  F.c3fh_resample_f565_w27f = function () { return buildResample('f565', 0x027f, 0); };
  F.c3fh_resample_fail0_w27f = function () { return buildResample('f555', 0x027f, 1); };
  F.c3fh_resample_fail1_w27f = function () { return buildResample('f555', 0x027f, 2); };

  // -- E. jgld.dll png_init_read_transformations 0x100702a0: one png_struct of ours per vector (0x220 bytes, patterned),
  //    with its palette (256 entries), trans array (256) and the gamma tables png_build_gamma_table 0x10077d30 makes
  //    through png_malloc, carved by the heap stand-in (allocation entry points as c3dk's png_build_gamma_table keys:
  //    0x7f0f0, 0x7fb40, calloc 0x7ffa0; frees 0x7e6d0, 0x806d0). No module global is written: no freeze.
  //    [color_type, bit_depth, transformations, bg gamma type, gamma, screen, bg gamma, bg {index, r, g, b, gray},
  //     num_palette, num_trans, sig_bit r g b]
  const PNGROWS = [
    [3, 8, 0x2080, 2, 0.45455, 2.2, 0.0, [0, 30, 140, 250, 0], 16, 6, [8, 8, 8]],          //  0 palette, gamma + bg, file
    [3, 8, 0x2080, 1, 0.45455, 2.2, 0.0, [0, 31, 141, 251, 0], 16, 6, [8, 8, 8]],          //  1 screen: |gs - 1| < 0.05
    [3, 8, 0x2080, 3, 0.45455, 2.2, 0.7, [0, 32, 142, 252, 0], 16, 6, [8, 8, 8]],          //  2 unique: pow on both
    [3, 8, 0x2080, 0, 0.45455, 2.2, 0.0, [0, 33, 143, 253, 0], 16, 6, [8, 8, 8]],          //  3 unknown: g = gs = 1
    [3, 8, 0x82180, 2, 0.6, 1.7, 0.0, [5, 0, 0, 0, 0], 12, 5, [8, 8, 8]],                  //  4 expand from palette, invert alpha
    [3, 8, 0x81180, 0, 0.0, 0.0, 0.0, [3, 0, 0, 0, 0], 12, 5, [8, 8, 8]],                  //  5 expand, EXPAND set: no invert; no gamma
    [3, 8, 0x80, 0, 0.0, 0.0, 0.0, [0, 200, 100, 50, 0], 20, 9, [8, 8, 8]],               //  6 no gamma, background composite
    [3, 8, 0x2000, 0, 0.5, 2.0, 0.0, [0, 1, 2, 3, 0], 20, 0, [8, 8, 8]],                   //  7 gamma palette, no background
    [3, 8, 0x2008, 0, 0.7, 1.3, 0.0, [0, 1, 2, 3, 0], 20, 0, [5, 6, 9]],                   //  8 gamma + shift (9 -> -1 -> 0)
    [0, 1, 0x2180, 1, 0.45455, 2.2, 0.0, [0, 0, 0, 0, 1], 0, 0, [8, 8, 8]],                //  9 gray 1-bit: * 0xff
    [0, 2, 0x2180, 2, 0.45455, 2.2, 0.0, [0, 0, 0, 0, 2], 0, 0, [8, 8, 8]],                // 10 gray 2-bit: * 0x55
    [4, 4, 0x2180, 3, 0.45455, 2.2, 1.3, [0, 0, 0, 0, 7], 0, 0, [8, 8, 8]],                // 11 gray-alpha 4-bit: * 0x11
    [0, 8, 0x2180, 0, 0.45455, 2.2, 0.0, [0, 0, 0, 0, 77], 0, 0, [8, 8, 8]],               // 12 gray 8-bit: copied
    [0, 16, 0x2180, 2, 0.3333333, 3.0, 0.0, [0, 0, 0, 0, 40000], 0, 0, [8, 8, 8]],         // 13 gray 16-bit
    [0, 3, 0x2180, 1, 0.45455, 2.2, 0.0, [0, 9, 9, 9, 5], 0, 0, [8, 8, 8]],                // 14 bit depth 3: no case
    [2, 8, 0x2180, 2, 0.45455, 2.2, 0.0, [0, 10, 128, 245, 0], 0, 0, [8, 8, 8]],           // 15 RGB 8-bit
    [6, 16, 0x2080, 3, 0.8, 1.9, 0.6, [0, 1000, 33333, 65000, 0], 0, 0, [8, 8, 8]],        // 16 RGBA 16-bit, unique
    [2, 8, 0x600080, 1, 0.45455, 2.2, 0.0, [0, 50, 60, 70, 0], 0, 0, [8, 8, 8]],           // 17 RGB to gray instead of gamma
    [3, 8, 0x0, 0, 0.45455, 2.2, 0.0, [0, 1, 2, 3, 4], 16, 6, [8, 8, 8]],                  // 18 nothing but the copy
    [3, 8, 0x2080, 3, 0.45455, 2.2, 0.4370629, [0, 90, 91, 92, 0], 16, 16, [8, 8, 8]],     // 19 |gs - 1| just below 0.05
    [3, 8, 0x2080, 3, 0.45455, 2.2, NaN, [0, 90, 91, 92, 0], 16, 6, [8, 8, 8]],            // 20 NaN background gamma
    [2, 4, 0x2080, 2, 0.45455, 2.2, 0.0, [0, 3, 7, 15, 0], 0, 0, [8, 8, 8]],               // 21 RGB 4-bit: m = 15
    [3, 8, 0x2180, 2, 0.45455, 2.2, 0.0, [4, 0, 0, 0, 0], 12, 5, [8, 8, 8]],               // 22 expand from palette, no alpha inversion
    [2, 8, 0x2080, 4, 0.45455, 2.2, 0.0, [0, 10, 20, 30, 0], 0, 0, [8, 8, 8]],             // 23 RGB, background gamma type 4: 1.0 / 1.0
    [0, 8, 0x2000, 0, 0.45455, 2.2, 0.0, [0, 0, 0, 0, 9], 0, 0, [8, 8, 8]],                // 24 gray, gamma without background
    [2, 8, 0x0088, 0, 0.0, 0.0, 0.0, [0, 1, 2, 3, 0], 0, 0, [8, 8, 8]],                    // 25 RGB, background and shift without gamma
    [3, 8, 0x0008, 0, 0.0, 0.0, 0.0, [0, 1, 2, 3, 0], 20, 0, [10, 12, 0]],                 // 26 shift only: red / green below 0, blue 8
  ];
  function buildPng(word) {
    const fx = {};
    const h = __heapStandIn({ module: 'jgld.dll', alloc: [0x7f0f0, 0x7fb40, { rva: 0x7ffa0, args: 2, sizeArgs: 2, fill: 0 }],
                              free: [0x7e6d0, 0x806d0], arenaSize: 0x4000, fill: 0xcd });
    fx.arena = h.arena; fx.freelog = h.log;
    PNGROWS.forEach(function (r, i) {
      const p = block(0x220, 0x5a000000 + i * 0x1000);
      const pal = keep(Memory.alloc(0x300)), tr = keep(Memory.alloc(0x100));
      for (let k = 0; k < 0x300; k++) pal.add(k).writeU8((k * 37 + i * 11 + 5) & 0xff);
      const TR = [0, 255, 128, 1, 254, 77, 200, 3, 0, 99, 255, 17, 60, 0, 250, 5];
      for (let k = 0; k < 0x100; k++) tr.add(k).writeU8(TR[(k + i) % TR.length]);
      p.add(0x70).writeU32(r[2] >>> 0);
      p.add(0x114).writePointer(pal);
      p.add(0x118).writeU16(r[8]);
      p.add(0x11a).writeU16(r[9]);
      p.add(0x126).writeU8(r[0]);
      p.add(0x127).writeU8(r[1]);
      p.add(0x130).writeU8(r[3]);
      if (r[6] !== r[6]) p.add(0x134).writeU32(0x7fc00000); else p.add(0x134).writeFloat(r[6]);
      p.add(0x138).writeU8(r[7][0]);
      [0x13a, 0x13c, 0x13e, 0x140].forEach(function (off, k) { p.add(off).writeU16(r[7][k + 1]); });
      p.add(0x15c).writeFloat(r[4]);
      p.add(0x160).writeFloat(r[5]);
      p.add(0x17c).writeU8(r[10][0]); p.add(0x17d).writeU8(r[10][1]); p.add(0x17e).writeU8(r[10][2]); p.add(0x17f).writeU8(8);
      p.add(0x188).writePointer(tr);
      fx['g' + i] = p; fx['pal' + i] = pal; fx['tr' + i] = tr;
    });
    return finish(fx, word);
  }
  F.c3fh_png_w27f = function () { return buildPng(0x027f); };
  F.c3fh_png_wf7f = function () { return buildPng(0x0f7f); };
  globalThis.__c3fhCountsC = { tempo: TEMPO.length, dsvol: DSVOL.length, chan: CH_F58.length, fade: FADE.length };
})();
