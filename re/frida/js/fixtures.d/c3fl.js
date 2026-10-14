// Fixtures for C3 batch c3fl (sound.dll, round 34). Layouts come from the disassembly cited in shim/src/re/c3fl.cpp.
// Every object, vtable, list node and command here is allocated by the fixture; all values are synthetic.
// The sound engine is LIVE on this PC (log/c3/c3fl_notes.txt section 1): the multimedia-timer thread runs
// SoundTimer::onTick 0x10008ed0 ~32 times/s and through it most rows of this batch. Each key therefore (1) builds its objects,
// (2) freezes the first thread (__freeze), (3) holds the timer off (c3fe's quietTimer, copied): it waits for an idle moment
// (re-entrancy word 0x100b49a8 = 0 and the timer's critical section 0x100b4990 unowned), writes 5 (onTick then leaves at
// 0x10008edf before touching anything) and waits until the word has stayed 5 with the section unowned for 100 ms; the word goes
// back to its idle value at the flush, after every other restore. For SoundTimer::onTick's own keys (section C) the live timer
// g_soundTimer 0x100b49ec (the only user dword the timer passes: log/c3/alt/c3fl_probe_user.txt) also gets bit 2 of its byte
// +0x3c cleared under the hold, so a live tick that reads the word as 0 leaves at 0x10008ef4 without a write; the byte is
// restored first at the flush (newest first), while the word is still 5.
(function () {
  const F = globalThis.DIFF_FIXTURES;
  const keep = function (x) { globalThis.__diffKeepAlive.push(x); return x; };
  const snd = function (rva) { return Process.getModuleByName('sound.dll').base.add(rva); };
  function fill(p, n, b) { p.writeByteArray(new Array(n).fill(b)); return p; }
  function marker(p, n, tag) { for (let k = 0; k + 4 <= n; k += 4) p.add(k).writeU32((tag + k) >>> 0); }
  function u32(p, off, v) { p.add(off).writeU32(v >>> 0); }
  function u16(p, off, v) { p.add(off).writeU16(v & 0xffff); }
  function u8(p, off, v) { p.add(off).writeU8(v & 0xff); }
  function f64(p, off, v) { p.add(off).writeDouble(v); }
  function bits64(p, off, lo, hi) { p.add(off).writeU32(lo >>> 0); p.add(off + 4).writeU32(hi >>> 0); }
  function block(n, tag) { const b = keep(Memory.alloc(n)); marker(b, n, tag); return b; }
  function zero(n) { return fill(keep(Memory.alloc(n)), n, 0); }
  const k32 = Process.getModuleByName('kernel32.dll');
  const sleep = keep(new NativeFunction(k32.getExportByName('Sleep'), 'void', ['uint32'], 'stdcall', { scheduling: 'exclusive' }));

  let frozenNow = false;
  function guard() {
    if (frozenNow) return;
    __freeze.thread();
    frozenNow = true;
    __freeze.atFlush(function () { frozenNow = false; });
    __freeze.negOn();
  }
  // Hold the multimedia timer off for the key (see the header). Throws (an ERROR row) rather than run with a tick in flight.
  function quietTimer(out) {
    guard();
    const g = snd(0xb49a8), owner = snd(0xb4990 + 0xc);   // CRITICAL_SECTION.OwningThread
    let idle = null;
    for (let i = 0; i < 400; i++) {
      if (g.readU32() === 0 && owner.readU32() === 0) { idle = 0; break; }
      sleep(1);
    }
    if (idle === null) {
      // not idle for 400 ms: a non-zero word with the section free also keeps onTick out (0x10008edf)
      if (owner.readU32() !== 0) throw new Error('c3fl: timer section 0x100b4990 stays owned');
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
    if (stable < 50) throw new Error('c3fl: timer did not go quiet');
    out.gtick = g;
  }
  function swapGlobal(rva, value, out, name) {
    const g = snd(rva);
    if (name) out[name] = g;
    __freeze.setU32(g, value);
    return g;
  }

  // Recording stub on a thunk (A/B thread only): each call appends {rva, ecx or 0, args...} (0x20 bytes) to `log` (+0 count,
  // entries from +4, 15 kept) and returns retfn(args, n) (0 without retfn). Other threads reach the real function. Reverted at
  // the freeze flush. opt.skip: argument indices (ecx first for thiscall) logged as 0 because they point into the caller's frame;
  // opt.mask: {index: mask} logs only those bits (a byte flag pushed in a dword whose upper bytes are undefined).
  const stubLive = {};
  function stubLog(n) { const sz = n || 0x200; const l = keep(Memory.alloc(sz)); fill(l, sz, 0); l.cap = Math.floor((sz - 4) / 0x20); return l; }
  function logCall(log, tag, a) {
    const n = log.readU32();
    if (n < (log.cap || 15)) {
      const p = log.add(4 + 0x20 * n);
      p.writeU32(tag >>> 0);
      for (let i = 0; i < a.length && i < 7; i++) p.add(4 + 4 * i).writeU32(ptr(a[i]).toUInt32());
    }
    log.writeU32(n + 1);
  }
  function stubOn(log, rva, nargs, conv, retfn, opt) {
    const base = snd(rva), key = base.toString();
    opt = opt || {};
    if (stubLive[key]) { try { Interceptor.revert(base); } catch (e) { /* gone */ } delete stubLive[key]; Interceptor.flush(); }
    const tid = Process.getCurrentThreadId();
    const thiscall = conv === 'thiscall';
    const argt = (thiscall ? ['pointer'] : []).concat(Array(nargs).fill('pointer'));
    const abi = thiscall ? 'thiscall' : (conv === 'cdecl' ? 'default' : 'stdcall');
    const orig = keep(new NativeFunction(base, 'uint32', argt, abi));
    const cb = keep(new NativeCallback(function () {
      const a = Array.prototype.slice.call(arguments);
      if (Process.getCurrentThreadId() !== tid) return orig.apply(null, a);
      const n = log.readU32();
      logCall(log, rva, a.map(function (x, i) {
        if (opt.skip && opt.skip.indexOf(i) >= 0) return ptr(0);
        if (opt.mask && opt.mask[i] !== undefined) return ptr((ptr(x).toUInt32() & opt.mask[i]) >>> 0);
        return x;
      }));
      const r = retfn ? retfn(a, n) : 0;
      return (typeof r === 'number') ? (r >>> 0) : ptr(r).toUInt32();
    }, 'uint32', argt, abi));
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
  // A fixture vtable whose slots are recording callbacks: slots = {offset: [nargs, retfn]}. thiscall (ecx = this) by default;
  // com = true makes them __stdcall with the interface pointer as the first stack argument (IDirectSoundBuffer methods).
  function recVtable(log, slots, com) {
    const vt = fill(keep(Memory.alloc(0x100)), 0x100, 0);
    Object.keys(slots).forEach(function (k) {
      const off = parseInt(k, 10), nargs = slots[k][0], retfn = slots[k][1];
      const argt = ['pointer'].concat(Array(nargs).fill('pointer'));
      vt.add(off).writePointer(keep(new NativeCallback(function () {
        const a = Array.prototype.slice.call(arguments);
        const n = log.readU32();
        logCall(log, (com ? 0x6100 : 0x5100) + off, a);
        const r = retfn ? retfn(a, n) : 0;
        return (typeof r === 'number') ? (r >>> 0) : ptr(r).toUInt32();
      }, 'uint32', argt, com ? 'stdcall' : 'thiscall')));
    });
    return vt;
  }
  // MSG {hwnd, message, wParam, lParam, time, pt.x, pt.y}
  function writeMsg(p, m) { for (let i = 0; i < 7; i++) p.add(4 * i).writeU32((m[i] || 0) >>> 0); }

  F.c3fl_h = { keep: keep, snd: snd, fill: fill, marker: marker, u32: u32, u16: u16, u8: u8, f64: f64, bits64: bits64,
               block: block, zero: zero, guard: guard, quietTimer: quietTimer, swapGlobal: swapGlobal, stubLog: stubLog,
               stubOn: stubOn, logCall: logCall, recVtable: recVtable, writeMsg: writeMsg };
})();

// ---- A. DsBuffer::restore: twelve DsBuffer objects (+0x60 a fixture IDirectSoundBuffer). The interface's Lock (slot +0x2c)
//      returns its +8 + 4k dword on its k-th call (count at +4) and, when +0x30 is set, first moves the owner's (+0x2c) +0x60 to
//      the interface at +0x30, so the reload at 0x10010208 is visible; Restore (slot +0x50) is recorded ------------------------
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3fl_h;
  const LOST = 0x88780096;
  // [first, second, switch]
  const RS = [
    [0, 0, 0], [LOST, 0, 0], [LOST, LOST, 0], [LOST, 0x88780064, 0], [0x88780064, 0, 0], [0x8878000a, 0, 0],
    [0x8878000b, 0, 0], [0x887800aa, 0, 0], [0x887800ab, 0, 0], [1, 0, 0], [0x80004005, 0, 0], [LOST, LOST, 1],
    [LOST, 0x88780095, 0], [LOST, 1, 0],
  ];
  F.c3fl_dsr = function () {
    const out = {};
    out.log = H.stubLog(0x400);
    const vt = H.recVtable(out.log, {
      0x2c: [7, function (a) {
        const com = a[0], k = com.add(4).readU32();
        com.add(4).writeU32(k + 1);
        const sw = com.add(0x30).readPointer();
        if (!sw.isNull() && k === 0) com.add(0x2c).readPointer().add(0x60).writePointer(sw);
        return com.add(8 + 4 * Math.min(k, 1)).readU32();
      }],
      0x50: [0, function (a) { return 0x50; }],
    }, true);
    for (let i = 0; i < RS.length; i++) {
      const o = H.block(0x80, 0x31000000 + 0x10000 * i);
      const c = H.block(0x40, 0x32000000 + 0x10000 * i);
      c.writePointer(vt);
      H.u32(c, 4, 0);
      H.u32(c, 8, RS[i][0]); H.u32(c, 0xc, RS[i][1]);
      c.add(0x2c).writePointer(o);
      c.add(0x30).writePointer(ptr(0));
      if (RS[i][2]) {
        const c2 = H.block(0x40, 0x33000000 + 0x10000 * i);
        c2.writePointer(vt); H.u32(c2, 4, 1); H.u32(c2, 8, 0); H.u32(c2, 0xc, RS[i][1]);
        c2.add(0x2c).writePointer(o); c2.add(0x30).writePointer(ptr(0));
        c.add(0x30).writePointer(c2);
        out['c2_' + i] = c2;
      }
      o.add(0x60).writePointer(c);
      out['o' + i] = o; out['c' + i] = c;
    }
    out.c2x = out.c2_11;
    H.quietTimer(out);
    return out;
  };
})();

// ---- B. DsBuffer::getPlayPosition: objects {+0x10 old play, +0x24 old write, +0x30 start, +0x38 length, +0x40 alt length, +0x78
//      size S, word +0xb4 align W, +0x60 interface}. GetCurrentPosition (slot +0x10) writes the interface's +8 / +0xc as the play
//      and write cursors and returns its +0x10. Two out dwords per object; object 13's first out pointer is its own +0x78 ---------
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3fl_h;
  // [S, W, oldPlay, oldWrite, start30, len38, alt40, play, write]
  const PS = [
    [0x1000, 4, 0x100, 0x200, 0, 0, 0, 0x80, 0x200],          // 0  play < old play (wrap), write == old write -> 0x15
    [0x1000, 0, 0x100, 0x000, 0x300, 0x400, 0, 0x180, 0x250], // 1  W 0; e < S, s > write, e > s -> 0
    [0x1000, 4, 0x100, 0x000, 0x100, 0x200, 0x80, 0x180, 0x200],   // 2  e < S, s <= write -> recompute, e >= s' -> 0x2d
    [0x1000, 2, 0x100, 0x000, 0x900, 0x100, 0xe80, 0x180, 0x800],  // 3  e < S, s > write, e <= s -> recompute, s' wraps, e < s'
    [0x1000, 4, 0x100, 0x000, 0xf00, 0x400, 0, 0x180, 0xe00],      // 4  e >= S, s > write, e' < s -> S - s + e', 0
    [0x1000, 4, 0x100, 0x000, 0xf00, 0x1200, 0, 0x180, 0xe00],     // 5  e >= S, s > write, e' >= s -> e' - s, 0
    [0x1000, 8, 0x100, 0x000, 0x100, 0x400, 0, 0x180, 0xe00],      // 6  e >= S, s <= write, e' > s -> e' - s, 0
    [0x1000, 4, 0x100, 0x000, 0x300, 0x300, 0x100, 0x180, 0xe00],  // 7  e >= S, s <= write, e' <= s, s' < S, e' < s' -> 0x2d
    [0x1000, 4, 0x100, 0x000, 0x300, 0x300, 0x300, 0x180, 0xe00],  // 8  e >= S, s <= write, e' <= s, s' >= S (wraps), e' >= s'
    [0x1000, 3, 0xf00, 0x000, 0x900, 0x100, 0, 0x100, 0x800],      // 9  e < S, s > write, e <= s: recompute with s < play
    [0x1000, 4, 0x100, 0x000, 0x900, 0x200, 0, 0xa00, 0x700],      // 10 e < S, s > write, e > s: s < play for the gap
    [0x10000, 6, 0x8000, 0x10, 0x20, 0x100, 0x7000, 0x4000, 0xfff0],   // 11 S 0x10000, W 6
    [0x1000, 4, 0x100, 0x000, 0x000, 0x000, 0x000, 0x100, 0x000],   // 12 everything at 0 except play == old play
    [0x1000, 4, 0x100, 0x000, 0x900, 0x100, 0, 0x100, 0x800],       // 13 as 9 but the first out pointer is the object's +0x78
  ];
  F.c3fl_dpp = function () {
    const out = {};
    out.log = H.stubLog(0x200);
    const vt = H.recVtable(out.log, {
      0x10: [2, function (a) {
        const com = a[0];
        a[1].writeU32(com.add(8).readU32());
        a[2].writeU32(com.add(0xc).readU32());
        return com.add(0x10).readU32();
      }],
    }, true);
    for (let i = 0; i < PS.length; i++) {
      const t = PS[i];
      const o = H.block(0x100, 0x41000000 + 0x10000 * i);
      const c = H.block(0x20, 0x42000000 + 0x10000 * i);
      c.writePointer(vt);
      H.u32(c, 8, t[7]); H.u32(c, 0xc, t[8]); H.u32(c, 0x10, 0x8000 + i);
      H.u32(o, 0x78, t[0]); H.u16(o, 0xb4, t[1]); H.u32(o, 0x10, t[2]); H.u32(o, 0x24, t[3]);
      H.u32(o, 0x30, t[4]); H.u32(o, 0x38, t[5]); H.u32(o, 0x40, t[6]);
      o.add(0x60).writePointer(c);
      out['o' + i] = o; out['c' + i] = c;
      out['a' + i] = (i === 13) ? o.add(0x78) : H.block(8, 0x43000000 + 0x100 * i);
      out['b' + i] = H.block(8, 0x44000000 + 0x100 * i);
    }
    const nul = H.block(0x100, 0x45000000);
    nul.add(0x60).writePointer(ptr(0));
    out.onull = nul; out.anull = H.block(8, 0x46000000); out.bnull = H.block(8, 0x47000000);
    H.quietTimer(out);
    return out;
  };
})();

// ---- C. SoundTimer::onTick: timers {+0 period, +4 last time, +8 elapsed, byte +0x3c flags, +0x4c client list, +0x54 cursor};
//      nodes {+4 next, +8 client}; clients with a recording vtable (+0x44 with the elapsed, +0x38); a client whose +0x10 is
//      non-zero sets its timer's (+0x14) cursor +0x54 to that value from slot +0x44 (the reload at 0x10008fab).
//      timeGetTime (slot 0x100b8654) returns the clock dword (compared state) and adds 0x40 to it; Enter- / LeaveCriticalSection
//      recorded (Enter clears bit 2 of the flip target's +0x3c in key _flip: the second test at 0x10008f13); OutputDebugStringA
//      records the first 32 bytes of its string. The word 0x100b49a8 is set per key (0 or 3) and compared ------------------------
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3fl_h;
  const NOW = 0x00100000;
  // [period, last, flag3c, clients] clients: 'none' (+0x4c NULL), 'nullc' (first node's client NULL), 2, 3 clients, 'cut'
  const TS = [
    [10, 0, 0x00, 2],                 // 0 flag clear -> 0
    [10, 0, 0x04, 2],                 // 1 last 0: elapsed 0, two clients
    [100, NOW - 50, 0x07, 2],         // 2 elapsed 50 < period - 7 -> leaves, +4 unchanged
    [10, NOW + 0x200, 0x04, 3],       // 3 now < last: elapsed 0x200 > period + 0x14 -> message, three clients
    [40, NOW - 45, 0x04, 'none'],     // 4 elapsed 45 between period - 7 and period + 0x14, no client list
    [40, NOW - 33, 0x04, 'nullc'],    // 5 elapsed 33 == period - 7 (not below), first client NULL
    [3, NOW - 1000, 0x04, 2],         // 6 period 3: period - 7 wraps; elapsed 1000 < it -> leaves
    [40, NOW - 61, 0x04, 'cut'],      // 7 elapsed 61 > period + 0x14 (60): message; the first client cuts the walk
    [0xfffffff0, NOW - 0x30, 0x04, 2],   // 8 period + 0x14 wraps to 4: elapsed 0x30 >= period - 7? no (below) -> leaves
    [0xfffffff0, 0, 0x04, 3],         // 9 last 0 with a wrapping period: elapsed 0 > 4? no; three clients
    [40, NOW - 60, 0x0c, 2],          // 10 elapsed 60 == period + 0x14 (not above): no message
    [5, NOW - 0x7fffffff, 0x04, 2],   // 11 a large elapsed: message with a big number
  ];
  F.c3fl_tick = function (mode) {
    const out = {};
    out.log = H.stubLog(0x400);
    out.dbg = H.zero(0x80);
    out.clock = H.zero(8);
    H.u32(out.clock, 0, NOW);
    const vt = H.recVtable(out.log, {
      0x44: [1, function (a) {
        const c = a[0], v = c.add(0x10).readU32();
        if (v !== 0) c.add(0x14).readPointer().add(0x54).writeU32(v === 1 ? 0 : v);
        return 0x44;
      }],
      0x38: [0, function () { return 0x38; }],
    });
    for (let i = 0; i < TS.length; i++) {
      const t = TS[i];
      const tm = H.block(0x60, 0x51000000 + 0x10000 * i);
      H.u32(tm, 0, t[0]); H.u32(tm, 4, t[1]); H.u32(tm, 8, 0x0badf00d); H.u32(tm, 0x3c, 0x5a5a5a00 | t[2]);
      H.u32(tm, 0x54, 0x54545454);
      const kinds = t[3];
      if (kinds === 'none') {
        H.u32(tm, 0x4c, 0);
      } else {
        const n = (kinds === 'nullc') ? 2 : (kinds === 'cut' ? 3 : kinds);
        let prev = null;
        for (let k = n - 1; k >= 0; k--) {
          const node = H.block(0x10, 0x52000000 + 0x10000 * i + 0x100 * k);
          let cl = null;
          if (!(kinds === 'nullc' && k === 0)) {
            cl = H.block(0x20, 0x53000000 + 0x10000 * i + 0x100 * k);
            cl.writePointer(vt);
            H.u32(cl, 0x10, (kinds === 'cut' && k === 0) ? 1 : 0);
            cl.add(0x14).writePointer(tm);
            out['cl' + i + '_' + k] = cl;
          }
          node.add(4).writePointer(prev || ptr(0));
          node.add(8).writePointer(cl || ptr(0));
          out['nd' + i + '_' + k] = node;
          prev = node;
        }
        tm.add(0x4c).writePointer(prev);
      }
      out['t' + i] = tm;
    }
    const flip = mode === 'flip' ? out.t1 : null;
    const s = __importStandIn({ module: 'sound.dll', imports: {
      'WINMM.dll!timeGetTime': { args: 0, impl: function () {
        const v = out.clock.readU32(); out.clock.writeU32((v + 0x40) >>> 0); return v;
      } },
      'KERNEL32.dll!EnterCriticalSection': { args: 1, impl: function () {
        if (flip) flip.add(0x3c).writeU8(flip.add(0x3c).readU8() & 0xfb);
        return 0x3e17;
      } },
      'KERNEL32.dll!LeaveCriticalSection': { args: 1, ret: 0x5eed },
      'KERNEL32.dll!OutputDebugStringA': { args: 1, impl: function (a, ctx) {
        // the string only (up to 32 bytes, zero padded): the bytes after its terminator are the caller's frame
        const bytes = new Array(32).fill(0);
        for (let k = 0; k < 32; k++) { const c = a[0].add(k).readU8(); if (c === 0) break; bytes[k] = c; }
        out.dbg.writeByteArray(bytes);
        const u = new Uint32Array(new Uint8Array(bytes).buffer);
        ctx.record(Array.prototype.slice.call(u));
        return 0;
      } },
    } });
    out.implog = s.log;
    H.quietTimer(out);
    // keep the live timer out even while the word is 0 (header): bit 2 of the live timer's byte +0x3c, restored first
    const live = H.snd(0xb49ec).readPointer();
    if (!live.isNull()) {
      __freeze.save(live.add(0x3c), 4);
      live.add(0x3c).writeU8(live.add(0x3c).readU8() & 0xfb);
    }
    out.gtick.writeU32(mode === 'w3' ? 3 : 0);
    __freeze.atFlush(function () { out.gtick.writeU32(5); });   // runs first: the word is 5 again before +0x3c comes back
    return out;
  };
  ['w0', 'w3', 'flip'].forEach(function (m) { F['c3fl_tick_' + m] = function () { return F.c3fl_tick(m); }; });
})();

// ---- D. Wave_Device::serviceTick: wave objects {+0x1b0, +0x1b4 delta}; the device 0x100b49f0 swapped for a fixture device
//      {byte +0x6c flags, +0x168 buffer list {+4 next, +8 buffer {+0x44 countdown, +0x50 play result}}} or NULL; the writer
//      0x100b49f4 and its arguments 0x100b49f8 / 0x100b49fc / 0x100b4a00 swapped; Wave_Device::dispatchCommand (thunk
//      0x10001e83), DsBuffer::play (thunk 0x10001203, returns the buffer's +0x50), writeWaveFile (thunk 0x10001514) and
//      writeWaveFileB (thunk 0x100015a5) are recording stubs. mode: nodev / empty / wr0 (list, no writer) / wa / wb -----------
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3fl_h;
  const COUNTS = [0, -10, -3, 5, 0x80000000, -1, 0x7ffffff0, 0];
  const DELTAS = [0, 3, 10, -4, 0x7fffffff, 1, -0x80000000, 2];
  F.c3fl_st = function (mode) {
    const out = {};
    out.log = H.stubLog(0x400);
    let dev = null;
    if (mode !== 'nodev') {
      dev = H.block(0x200, 0x61000000);
      H.u8(dev, 0x6c, mode === 'wb' ? 0x13 : 0xef);
      let prev = null;
      if (mode !== 'empty') {
        for (let k = COUNTS.length - 1; k >= 0; k--) {
          const n = H.block(0x10, 0x62000000 + 0x100 * k);
          const b = H.block(0x60, 0x63000000 + 0x1000 * k);
          H.u32(b, 0x44, COUNTS[k]); H.u32(b, 0x50, 0x6500 + k);
          n.add(4).writePointer(prev || ptr(0));
          n.add(8).writePointer(b);
          out['n' + k] = n; out['b' + k] = b;
          prev = n;
        }
      }
      dev.add(0x168).writePointer(prev || ptr(0));
      out.dev = dev;
    }
    for (let i = 0; i < DELTAS.length; i++) {
      const w = H.block(0x200, 0x64000000 + 0x10000 * i);
      H.u32(w, 0x1b4, DELTAS[i]);
      out['w' + i] = w;
    }
    out.wr = H.block(0x40, 0x66000000);
    H.quietTimer(out);
    H.swapGlobal(0xb49f0, dev ? dev.toUInt32() : 0, out, 'gdev');
    H.swapGlobal(0xb49f4, (mode === 'wa' || mode === 'wb') ? out.wr.toUInt32() : 0, out, 'gwr');
    H.swapGlobal(0xb49f8, 0x49f80001, out, 'gwa');
    H.swapGlobal(0xb49fc, 0x49fc0002, out, 'gwb');
    H.swapGlobal(0xb4a00, 0x4a000003, out, 'gwc');
    H.stubOn(out.log, 0x1e83, 0, 'thiscall', function () { return 0x1e83; });
    H.stubOn(out.log, 0x1203, 1, 'thiscall', function (a) { return a[0].add(0x50).readU32(); });
    H.stubOn(out.log, 0x1514, 1, 'thiscall', function (a, n) { return 0x1514 + n; });
    H.stubOn(out.log, 0x15a5, 2, 'thiscall', function (a, n) { return 0x15a5 + n; });
    return out;
  };
  ['nodev', 'empty', 'wr0', 'wa', 'wb'].forEach(function (m) { F['c3fl_st_' + m] = function () { return F.c3fl_st(m); }; });
})();

// ---- E. Dll_Midi_Device::create_device: g_midiDevice 0x100b4a1c swapped (0, or a marker for _busy); operator new 0x100424d7
//      through the heap stand-in (failMask 1 for _fail); MidiDeviceBase_ctor (thunk 0x1000260d) and CommandQueue::init (thunk
//      0x100010aa) recording stubs. Twelve out cells ------------------------------------------------------------------------
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3fl_h;
  F.c3fl_cd = function (mode) {
    const out = {};
    out.log = H.stubLog(0x100);
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], arenaSize: 0x4080, failMask: mode === 'fail' ? 1 : 0 });
    out.arena = h.arena;
    for (let i = 0; i < 12; i++) out['p' + i] = H.block(8, 0x71000000 + 0x100 * i);
    H.quietTimer(out);
    H.swapGlobal(0xb4a1c, mode === 'busy' ? 0x7a1c0000 : 0, out, 'gmidi');
    H.stubOn(out.log, 0x260d, 0, 'thiscall', function () { return 0x260d; });
    H.stubOn(out.log, 0x10aa, 0, 'thiscall', function () { return 0x10aa; });
    return out;
  };
  ['null', 'fail', 'busy'].forEach(function (m) { F['c3fl_cd_' + m] = function () { return F.c3fl_cd(m); }; });
})();

// ---- F. soundWindowSubclassProc: c3fe's soundWindowProc fixture (D in fixtures.d/c3fe.js) with CallWindowProcA (slot 0x100b85b8)
//      stood in (result computed from its five arguments) and the saved procedure 0x100b49c4 swapped for a marker; the flag byte
//      0x100b49bc swapped (one key per value 0..3); every callee thunk stubbed (delete_sound 0x10001a87 cdecl 1, serviceDecodeRing
//      0x100022fc, WaveInDevice_requeueBuffer 0x100020a4 (1 arg), SoundGroup_rewind 0x10002568, SoundGroup_stop 0x100016a4,
//      pairClear 0x10001816: thiscall); operator delete recorded by the heap stand-in ----------------------------------------
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3fl_h;
  F.c3fl_wp = function (flagv) {
    const out = {};
    out.log = H.stubLog(0x400);
    out.freelog = __heapStandIn({ module: 'sound.dll', free: [0x4249a] }).log;
    const s = __importStandIn({ module: 'sound.dll', imports: {
      'USER32.dll!CallWindowProcA': { args: 5, impl: function (a) {
        return ((a[0].toUInt32() ^ (a[1].toUInt32() * 3) ^ a[2].toUInt32() ^ (a[3].toUInt32() << 1) ^ (a[4].toUInt32() >>> 3)
                 ^ 0x5a5a0000) >>> 0);
      } },
    } });
    out.implog = s.log;
    const vt = H.recVtable(out.log, {
      0: [1, function (a) { return 0x40; }],
      0x60: [0, function (a) { return a[0].add(0x10).readU32(); }],    // the object's +0x10 decides SoundGroup_stop
      0xb8: [0, function (a) { return 0xb8; }],
    });
    function obj(tag, b58, r10) {
      const o = H.block(0x1430, tag);
      o.writePointer(vt);
      H.u32(o, 0x10, r10);
      H.u32(o, 0x58, b58);
      H.u8(o, 0x1428, 0xff);
      H.u32(o, 0x214, 0xffffffff);
      return o;
    }
    out.oA = obj(0x81000000, 0x0, 1);      // slot 0x60 non-zero, +0x58 bit 1 clear
    out.oB = obj(0x82000000, 0x2, 0);      // slot 0x60 zero, bit 1 set
    out.oC = obj(0x83000000, 0x0, 0);
    out.oD = obj(0x84000000, 0x0, 0);
    function node(tag, kind, o) { const n = H.block(0x20, tag); H.u32(n, 0, kind); n.add(4).writePointer(o || ptr(0)); return n; }
    out.n3a = node(0x85000000, 3, out.oA);
    out.n3b = node(0x86000000, 3, out.oB);
    out.n4a = node(0x87000000, 4, out.oC);
    out.n4n = node(0x88000000, 4, null);
    out.n5 = node(0x89000000, 5, out.oD);
    out.n7 = node(0x8a000000, 7, out.oA);
    out.chA = H.block(0x1400, 0x8b000000); H.u32(out.chA, 0x58, 0x0ffff6f6 | 2); H.u32(out.chA, 0x1370, 5);
    out.chB = H.block(0x1400, 0x8c000000); H.u32(out.chB, 0x58, 0x0ffff6f4); H.u32(out.chB, 0x1370, 0);
    out.wiA = H.block(0x40, 0x8d000000); H.u8(out.wiA, 4, 0x02);
    out.wiB = H.block(0x40, 0x8e000000); H.u8(out.wiB, 4, 0xfd);
    out.snd1 = H.block(0x40, 0x8f000000);
    H.quietTimer(out);
    out.gflag = H.snd(0xb49bc);
    __freeze.save(out.gflag, 4);
    out.gflag.writeU8(flagv);
    H.swapGlobal(0xb49c4, 0x49c40bad, out, 'gprev');
    H.stubOn(out.log, 0x1a87, 1, 'cdecl', function () { return 0x1a87; });
    // serviceDecodeRing flips bits 3 and 10 of the channel's +0x58 so the reload at 0x10028c21 is visible
    H.stubOn(out.log, 0x22fc, 0, 'thiscall', function (a) { const c = a[0]; c.add(0x58).writeU32((c.add(0x58).readU32() ^ 0x408) >>> 0); return 0x22fc; });
    H.stubOn(out.log, 0x20a4, 1, 'thiscall', function () { return 0x20a4; });
    H.stubOn(out.log, 0x2568, 0, 'thiscall', function () { return 0x2568; });
    H.stubOn(out.log, 0x16a4, 0, 'thiscall', function () { return 0x16a4; });
    H.stubOn(out.log, 0x1816, 0, 'thiscall', function () { return 0x1816; });
    return out;
  };
  [0, 1, 2, 3].forEach(function (v) { F['c3fl_wp_f' + v] = function () { return F.c3fl_wp(v); }; });
})();

// ---- G. serviceChannelPlayback: the device 0x100b49f0 swapped for a fixture device (or NULL for _nodev); channels of 0x1400 bytes
//      {+0x54 buffer, +0x58 flags, +0x5c flags, +0x1b8, +0x1e0/+0x1f0/+0x200/+0x1388 doubles, +0x220 frequency, +0x22c, +0x1338
//      buffer list, sixteen voice records at +i*0x110}; buffers {+0x44, +0x48 busy, floats +0xc4 / +0xc8 / +0xcc / +0xd0}.
//      Recording stubs: DsBuffer::play 0x10001203, isBusy 0x10001b9f (returns the buffer's +0x48), duplicateBuffer 0x10002572
//      (writes the channel's +0x100 through its third argument and returns +0x104), freeBufferById 0x10001f7d (writes +0x108
//      through its second argument, returns +0x10c), setVolumeRaw 0x100026e4, setFrequency 0x10001181, Ds3DBuffer::setPosition
//      0x10001889. The frame pointer arguments (&b) are logged as 0 ------------------------------------------------------------
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3fl_h;
  function buf(out, name, tag, busy) {
    const b = H.block(0x100, tag);
    H.u32(b, 0x48, busy);
    H.u32(b, 0xc4, 0x0c40000 + (tag >>> 16));
    H.u32(b, 0xc8, 0x3f800000); H.u32(b, 0xcc, 0x7f800001); H.u32(b, 0xd0, 0x80000001);   // 1.0, a float SNaN, a denormal
    out[name] = b;
    return b;
  }
  // [f5c, f58, bufBusy (null = no buffer), n22c, list (array of busy flags or null), dupRet, freeRet, b1b8]
  const CS = [
    [0x04, 0x00000001, 0, 0, null, 0, 0, 0],                 // 0 +0x5c bit 2 -> 0xb
    [0x40, 0x00000000, 0, 0, null, 0, 0, 0],                 // 1 bit 6 plays +0x54 with 1; +0x58 bit 0 clear
    [0x40, 0x00000010, null, 0, null, 0, 0, 0],              // 2 bit 6 with no buffer; bit 0 clear, bit 4 cleared
    [0x20, 0x10000011, 0, 0, null, 0, 0, 0],                 // 3 hw: not busy, +0x22c 0 -> frequency, 3D position, play 0
    [0x00, 0x10000001, 1, 7, [1, 0], 0, 0, 0],               // 4 hw: busy; list second free; volume; +0x22c 7 -> +0x44 = -7
    [0x20, 0x10000001, 1, 0, [], 0, 0, 0],                   // 5 hw: busy, empty list -> duplicate 0
    [0x00, 0x10000001, 1, 3, [1, 1], 0x25, 0, 0],            // 6 duplicate 0x25 -> freeBufferById 0 -> on
    [0x00, 0x10000001, 1, 0, null, 0x1d, 9, 0],              // 7 duplicate 0x1d -> freeBufferById non-zero -> 6
    [0x00, 0x10000001, 1, 0, null, 7, 0, 0],                 // 8 duplicate 7 -> 6
    [0x20, 0x10000001, 0, 5, [1], 0, 0, 0],                  // 9 hw: not busy but +0x22c 5 -> list search all busy -> duplicate
    [0x00, 0x00007001 | 0x10, 0, 0, null, 0, 0, 1],          // 10 sw: bits 12..14 set -> voice 3, +0x1b8 set
    [0x00, 0x0ffff001, 0, 0, null, 0, 0, 0],                 // 11 sw: all sixteen bits set -> 6
    [0x00, 0x00000001, 0, 0, null, 0, 0, 0],                 // 12 sw: voice 0, +0x1b8 0
    [0x40, 0x07fff001, 0, 0, null, 0, 0, 2],                 // 13 sw: voice 15 (bits 12..26 set), bit 6 plays first
  ];
  F.c3fl_sp = function (mode) {
    const out = {};
    out.log = H.stubLog(0x800);
    let dev = null;
    if (mode !== 'nodev') { dev = H.block(0x200, 0x91000000); out.dev = dev; }
    for (let i = 0; i < CS.length; i++) {
      const t = CS[i];
      const c = H.block(0x1400, 0x92000000 + 0x10000 * i);
      H.u32(c, 0x5c, t[0]); H.u32(c, 0x58, t[1]); H.u32(c, 0x22c, t[3]); H.u32(c, 0x1b8, t[7]);
      H.u32(c, 0x220, 0x5622 + i);
      c.add(0x54).writePointer(t[2] === null ? ptr(0) : buf(out, 'b' + i, 0x93000000 + 0x10000 * i, t[2]));
      let prev = null;
      if (t[4]) {
        for (let k = t[4].length - 1; k >= 0; k--) {
          const n = H.block(0x10, 0x94000000 + 0x10000 * i + 0x100 * k);
          n.add(4).writePointer(prev || ptr(0));
          n.add(8).writePointer(buf(out, 'l' + i + '_' + k, 0x95000000 + 0x10000 * i + 0x1000 * k, t[4][k]));
          out['n' + i + '_' + k] = n;
          prev = n;
        }
      }
      c.add(0x1338).writePointer(prev || ptr(0));
      const d = buf(out, 'd' + i, 0x96000000 + 0x10000 * i, 0);
      c.add(0x100).writePointer(d);
      H.u32(c, 0x104, t[5]);
      const fb = buf(out, 'f' + i, 0x97000000 + 0x10000 * i, 0);
      c.add(0x108).writePointer(fb);
      H.u32(c, 0x10c, t[6]);
      H.f64(c, 0x1e0, 0.1 * (i + 1)); H.f64(c, 0x1f0, -1 / 3); H.bits64(c, 0x200, 1, 0x7ff00000); H.f64(c, 0x1388, 1e300);
      out['c' + i] = c;
    }
    H.quietTimer(out);
    H.swapGlobal(0xb49f0, dev ? dev.toUInt32() : 0, out, 'gdev');
    H.stubOn(out.log, 0x1203, 1, 'thiscall', function () { return 0x1203; });
    H.stubOn(out.log, 0x1b9f, 0, 'thiscall', function (a) { return a[0].add(0x48).readU32(); });
    H.stubOn(out.log, 0x2572, 3, 'thiscall', function (a) {
      a[3].writePointer(a[1].add(0x100).readPointer());
      return a[1].add(0x104).readU32();
    }, { skip: [3] });
    H.stubOn(out.log, 0x1f7d, 2, 'thiscall', function (a) {
      a[2].writePointer(a[1].add(0x108).readPointer());
      return a[1].add(0x10c).readU32();
    }, { skip: [2] });
    H.stubOn(out.log, 0x26e4, 1, 'thiscall', function () { return 0x26e4; });
    H.stubOn(out.log, 0x1181, 1, 'thiscall', function () { return 0x1181; });
    H.stubOn(out.log, 0x1889, 3, 'thiscall', function () { return 0x1889; });
    return out;
  };
  ['nodev', 'dev'].forEach(function (m) { F['c3fl_sp_' + m] = function () { return F.c3fl_sp(m); }; });
})();

// ---- H. flushSoundList (argument-free: one call per key): g_soundEngine 0x100b4a20 swapped for a fixture engine whose list at
//      +0x1c8 {head, tail, cursor, count} holds nodes {prev, next, object}; objects (0x1430 bytes) with a recording vtable: +0x78
//      returns +0x10 (the kind), +0x5c returns +0x14, +0x64 returns +0x18 (the next object), +0x80 / +0x38 / +0 (with 1) recorded.
//      Recording stubs: Channel_clearBackref 0x10001398, postNotification 0x10001b3b, SoundGroup_rewind 0x10002568 (an object
//      whose +0x20 is 1 makes the rewind stub zero the list cursor); operator delete recorded; timeGetTime stood in ----------------
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3fl_h;
  // object spec: {k: kind, b: +0x5c result, f58, c8, cc, b1424, cut, next: name}
  const SETS = {
    empty: { list: [], extra: [] },
    nullobj: { list: [null], extra: [] },
    k1: { list: [
      { n: 'o0', k: 1, b: 1 },
      { n: 'o1', k: 1, b: 0, f58: 0x2 },
      { n: 'o2', k: 1, b: 0, f58: 0x10, b1424: 0, cc: 0x400 },
      { n: 'o3', k: 1, b: 0, f58: 0x10, b1424: 1, c8: 0x41 },
      { n: 'o4', k: 1, b: 0, f58: 0x0, c8: 0, cc: 0x80 },
      { n: 'o5', k: 1, b: 0, f58: 0x0, c8: 0x40, cc: 0x0 },
      { n: 'o6', k: 1, b: 1 },
    ], extra: [] },
    mixed: { list: [
      { n: 'q0', k: 4, b: 1, next: 'q1' },
      { n: 'y', k: 1, b: 1 },
      { n: 'x', k: 1, b: 0, c8: 1, cc: 0x400 },
    ], extra: [
      { n: 'q1', k: 6, b: 0, next: 'q2' },
      { n: 'q2', k: 9, b: 0, next: 'q3' },
      { n: 'q3', k: 4, b: 0, f58: 0x8, next: 'q4' },
      { n: 'q4', k: 4, b: 0, f58: 0x0, next: 'q5' },
      { n: 'q5', k: 6, b: 1, next: 'z' },
      { n: 'z', k: 1, b: 0, cc: 0x400 },
    ] },
    cut: { list: [
      { n: 'x0', k: 1, b: 0, c8: 1, cut: 1 },
      { n: 'x1', k: 1, b: 1 },
      { n: 'x2', k: 1, b: 1 },
    ], extra: [] },
    cut2: { list: [
      { n: 'x0', k: 1, b: 0, c8: 1, cut: 1 },
      { n: 'x1', k: 1, b: 0, f58: 0x2 },
    ], extra: [] },
    other: { list: [
      { n: 'p0', k: 0, b: 0, next: 'p1' },
      { n: 'p9', k: 1, b: 0, f58: 0x10, b1424: 1, cc: 0x80 },
    ], extra: [
      { n: 'p1', k: 2, b: 0, next: 'p2' },
      { n: 'p2', k: 5, b: 0, next: 'p3' },
      { n: 'p3', k: 6, b: 1, next: 'p4' },
      { n: 'p4', k: 0xffffffff, b: 0, next: 'p9' },
    ] },
  };
  F.c3fl_fl = function (set) {
    const out = {};
    out.log = H.stubLog(0x800);
    out.freelog = __heapStandIn({ module: 'sound.dll', free: [0x4249a] }).log;
    const s = __importStandIn({ module: 'sound.dll', imports: { 'WINMM.dll!timeGetTime': { args: 0, ret: 0x7171 } } });
    out.implog = s.log;
    const eng = H.block(0x200, 0xa1000000);
    out.eng = eng;
    const list = eng.add(0x1c8);
    const vt = H.recVtable(out.log, {
      0x78: [0, function (a) { return a[0].add(0x10).readU32(); }],
      0x5c: [0, function (a) { return a[0].add(0x14).readU32(); }],
      0x64: [0, function (a) { return a[0].add(0x18).readU32(); }],
      0x80: [0, function () { return 0x80; }],
      0x38: [0, function () { return 0x38; }],
      0: [1, function () { return 0; }],
    });
    const S = SETS[set], objs = {};
    let tag = 0xa2000000;
    function mk(sp) {
      const o = H.block(0x1430, tag); tag += 0x10000;
      o.writePointer(vt);
      H.u32(o, 0x10, sp.k); H.u32(o, 0x14, sp.b); H.u32(o, 0x18, 0); H.u32(o, 0x20, sp.cut || 0);
      H.u32(o, 0x58, sp.f58 || 0); H.u32(o, 0xc8, sp.c8 === undefined ? 0x40 : sp.c8); H.u32(o, 0xcc, sp.cc || 0);
      H.u8(o, 0x1424, sp.b1424 || 0);
      objs[sp.n] = o; out[sp.n] = o;
      return o;
    }
    S.list.forEach(function (sp) { if (sp) mk(sp); });
    S.extra.forEach(mk);
    S.list.concat(S.extra).forEach(function (sp) {
      if (sp && sp.next) objs[sp.n].add(0x18).writePointer(objs[sp.next]);
    });
    const nodes = [];
    for (let i = 0; i < S.list.length; i++) {
      const n = H.block(0x10, 0xa3000000 + 0x100 * i);
      n.add(8).writePointer(S.list[i] ? objs[S.list[i].n] : ptr(0));
      nodes.push(n); out['n' + i] = n;
    }
    for (let i = 0; i < nodes.length; i++) {
      nodes[i].writePointer(i > 0 ? nodes[i - 1] : ptr(0));
      nodes[i].add(4).writePointer(i + 1 < nodes.length ? nodes[i + 1] : ptr(0));
    }
    list.writePointer(nodes.length ? nodes[0] : ptr(0));
    list.add(4).writePointer(nodes.length ? nodes[nodes.length - 1] : ptr(0));
    H.u32(list, 8, 0x0c0c0c0c);
    H.u32(list, 0xc, nodes.length);
    H.quietTimer(out);
    H.swapGlobal(0xb4a20, eng.toUInt32(), out, 'geng');
    H.stubOn(out.log, 0x1398, 0, 'thiscall', function () { return 0x1398; });
    H.stubOn(out.log, 0x1b3b, 2, 'thiscall', function () { return 0x1b3b; });
    H.stubOn(out.log, 0x2568, 0, 'thiscall', function (a) {
      if (a[0].add(0x20).readU32() === 1) list.add(8).writeU32(0);
      return 0x2568;
    });
    return out;
  };
  Object.keys(SETS).forEach(function (k) { F['c3fl_fl_' + k] = function () { return F.c3fl_fl(k); }; });
})();

// ---- I. Midi_Device::dispatchCommand: devices of 0x4058 bytes {+0x40 channel list {head, tail, cursor, count}, +0x50 ring of 0x1000
//      command pointers, +0x4050 read, +0x4054 write}; commands {+0 kind, +4 channel, +8, +0xc}; channels {+0xac, +0x210 window,
//      +0x214}. scanCommandRing (thunk 0x1000196f) is a recording stub that, for a device whose +0x2c is 1, writes +0x30 / +0x34
//      through its two frame pointers (logged as 0). Every handler thunk is a recording stub; Seq_cmdHandler12's flag argument is
//      logged by its low byte only (the upper bytes of the pushed edx are undefined, 0x1000a917). g_soundEngine swapped per key
//      (_post: bit 5 of +0x1a4 set, _thread: clear, _null: NULL); PostMessageA (slot 0x100b85a0) stood in; operator delete recorded.
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3fl_h;
  // device specs: read, write, scan [flag, read', write'], channels in the list, ring entries {idx: [kind, ch, a8, ac] | null}
  const DS = [
    { r: 0x40, w: 0x40, list: [], ring: {} },                                  // 0 empty ring
    { r: 0x10, w: 0x1d, list: ['A', 'B', 'C'], ring: {
      0x11: [1, 'N', 0x11, 0], 0x12: [1, 'F', 0x12, 0], 0x13: [1, null, 0, 0], 0x14: [2, 'A', 0, 0], 0x15: [3, 'B', 0, 0],
      0x16: [4, null, 0, 0], 0x17: [4, 'C', 0, 0], 0x18: [5, 'N', 0, 0], 0x19: [6, 'N', 0, 0], 0x1a: [0, 'N', 0, 0],
      0x1b: [13, 'N', 0, 0], 0x1c: null, 0x1d: [7, 'N', 0, 0] } },
    { r: 0xffa, w: 0x7, list: ['A'], ring: {
      0xffb: [7, null, 0, 0], 0xffc: [8, 'N', 0x88, 0], 0xffd: [9, 'N', 0x99, 0], 0xffe: [10, 'N', 0xa1, 0xa2],
      0xfff: [11, 'N', 0xb1, 0xb2], 0x000: [12, 'N', 0xc1, 0], 0x001: [12, 'F', 0xc2, 0x55], 0x002: [3, 'X', 0, 0],
      0x003: [8, null, 0, 0], 0x004: [10, null, 0, 0], 0x005: [11, null, 0, 0], 0x006: [12, null, 0, 0], 0x007: [9, null, 0, 0] } },
    { r: 0x20, w: 0x1020, list: [], ring: { 0x21: [7, 'N', 0, 0] } },          // 3 n = 0 after the mask
    { r: 0x100, w: 0x104, scan: [1, 0x200, 0x300], list: ['A', 'B'], ring: {   // 4 scanCommandRing moves both copies
      0x101: [7, 'N', 0, 0], 0x201: [2, 'B', 0, 0], 0x202: [9, 'N', 0x92, 0], 0x203: [3, 'A', 0, 0], 0x204: [1, 'N', 0, 0] } },
    { r: 0x7, w: 0x9, list: ['A', 'B'], ring: { 0x8: [2, 'B', 0, 0], 0x9: [4, 'A', 0, 0] } },   // 5 tail, then the head
  ];
  F.c3fl_md = function (mode) {
    const out = {};
    out.log = H.stubLog(0x1000);
    out.freelog = __heapStandIn({ module: 'sound.dll', free: [0x4249a] }).log;
    const s = __importStandIn({ module: 'sound.dll', imports: {
      'USER32.dll!PostMessageA': { args: 4, impl: function (a) { return (a[0].toUInt32() ^ a[1].toUInt32() ^ 0x1000) >>> 0; } },
    } });
    out.implog = s.log;
    for (let i = 0; i < DS.length; i++) {
      const sp = DS[i];
      const d = H.zero(0x4060);
      H.u32(d, 0x4050, sp.r); H.u32(d, 0x4054, sp.w);
      if (sp.scan) { H.u32(d, 0x2c, sp.scan[0]); H.u32(d, 0x30, sp.scan[1]); H.u32(d, 0x34, sp.scan[2]); }
      const chans = {};
      ['A', 'B', 'C', 'N', 'F', 'X'].forEach(function (nm, k) {
        const c = H.block(0x220, 0xb1000000 + 0x100000 * i + 0x1000 * k);
        H.u32(c, 0xac, nm === 'F' ? 0x20 : 0x0f);
        H.u32(c, 0x210, 0x2100 + 0x10 * i + k);
        H.u32(c, 0x214, 0x0f0f0f00);
        chans[nm] = c; out['ch' + i + nm] = c;
      });
      const nodes = sp.list.map(function (nm, k) {
        const n = H.block(0x10, 0xb2000000 + 0x100000 * i + 0x100 * k);
        n.add(8).writePointer(chans[nm]);
        out['ln' + i + '_' + k] = n;
        return n;
      });
      for (let k = 0; k < nodes.length; k++) {
        nodes[k].writePointer(k > 0 ? nodes[k - 1] : ptr(0));
        nodes[k].add(4).writePointer(k + 1 < nodes.length ? nodes[k + 1] : ptr(0));
      }
      d.add(0x40).writePointer(nodes.length ? nodes[0] : ptr(0));
      d.add(0x44).writePointer(nodes.length ? nodes[nodes.length - 1] : ptr(0));
      H.u32(d, 0x48, 0x48484848);
      H.u32(d, 0x4c, nodes.length);
      Object.keys(sp.ring).forEach(function (ks) {
        const idx = parseInt(ks, 10), e = sp.ring[ks];
        if (e === null) { H.u32(d, 0x50 + 4 * idx, 0); return; }
        const c = H.block(0x10, 0xb3000000 + 0x100000 * i + 0x100 * idx);
        H.u32(c, 0, e[0]);
        c.add(4).writePointer(e[1] ? chans[e[1]] : ptr(0));
        H.u32(c, 8, e[2]); H.u32(c, 0xc, e[3]);
        d.add(0x50 + 4 * idx).writePointer(c);
        out['cmd' + i + '_' + idx.toString(16)] = c;
      });
      out['d' + i] = d;
    }
    let eng = null;
    if (mode !== 'null') {
      eng = H.block(0x200, 0xb4000000);
      H.u32(eng, 0x1a4, mode === 'post' ? 0x20 : 0xffffffdf);
      out.eng = eng;
    }
    H.quietTimer(out);
    H.swapGlobal(0xb4a20, eng ? eng.toUInt32() : 0, out, 'geng');
    H.stubOn(out.log, 0x196f, 2, 'thiscall', function (a) {
      const d = a[0];
      if (d.add(0x2c).readU32() === 1) { a[1].writeU32(d.add(0x30).readU32()); a[2].writeU32(d.add(0x34).readU32()); }
      return 0x196f;
    }, { skip: [1, 2] });
    [[0x26c1, 1], [0x1dd9, 1], [0x190b, 0], [0x1186, 0], [0x1474, 1], [0x25c2, 1], [0x23e7, 2], [0x11db, 2], [0x1816, 0],
     [0x11d6, 3]].forEach(function (e) { H.stubOn(out.log, e[0], e[1], 'thiscall', function (a, n) { return e[0] + n; }); });
    H.stubOn(out.log, 0x1037, 2, 'thiscall', function (a, n) { return 0x1037 + n; }, { mask: { 2: 0xff } });
    return out;
  };
  ['post', 'thread', 'null'].forEach(function (m) { F['c3fl_md_' + m] = function () { return F.c3fl_md(m); }; });
})();

// ---- J. Midi_Device::noteOn: a device of 0xa00 bytes {channel records +0x250 + ch * 0x4c {+4 volume, +0xc program, +0x10 pan},
//      +0x730 master volume, +0x744 program table}; g_midiDevice 0x100b4a1c swapped for a block whose +0x3c is the MIDI-out
//      handle (key _hw) or 0 (_sw, _swbig: +0x730 = 100 or 0x12345678); g_soundEngine swapped for a marker block. Every track,
//      program, bank, instrument and voice lives in one zeroed arena ($arena, compared): tracks {+0x18 note shift, +0x1c
//      Seq_firstTrackData result, byte +0x38, +0x58 note table, +0x258 message table, +0x45c, +0x460} with a vtable whose slot +4
//      returns note + shift; programs {+4 flags, +8 / +0xc / +0x10 / +0x14 ranges, +0x30 bank}; banks {+4 instrument table,
//      +0x214 flags, +0x218 Midi_advanceMsgRing result}; instruments {+0x10 slot +0x60 result, +0x14 slot +0x5c result, +0x18
//      alloc-fails flag, +0x1c voice, +0xc8, +0x210, +0x228, +0x1438 count, +0x1440 sub-instruments} with a recording vtable.
//      Recording stubs: Seq_firstTrackData 0x10001906, Midi_advanceMsgRing 0x10001546, Channel_allocVoiceSlot 0x10001a3c (writes
//      0x51070000 + call number through its frame pointer, logged as 0), randRange 0x1000209a (0x12340000 + 0x1111 * call number +
//      range), Voice_setPanGains 0x1000231a, Voice_setLevel 0x10002040, Channel_startVoice 0x10001212, appendNodeC 0x10002351;
//      midiOutShortMsg (slot 0x100b8638) stood in ------------------------------------------------------------------------------
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3fl_h;
  // [ch, note, vel, track {f38, first, xl, occ, t45c, t460}]
  const HW = [
    [3, 0x40, 0x64, {}], [4, 0x41, 0x10, { f38: 1 }], [0xf, 0x30, 0x7f, { first: 1, xl: 5 }],
    [9, 0x7f, 0x80, { f38: 0xfe }], [0, 0, 0, {}], [0x12, 0x10, 1, { first: 1, xl: 0x100 }],
    [7, 0x55, 0x20, { first: 1, xl: -0x55 }], [1, 0x22, 0x33, { f38: 0x02 }],
  ];
  // [ch, note, vel, track, rec {vol, prog, pan}, program {f4, r8, rc, r10, r14, bank {f214, adv, ins {...}}}]
  const INS = function (o) { return Object.assign({ s60: 1, s5c: 0, c8: 0, i210: 0x30, i228: 0, cnt: 0, subs: null, an: 0 }, o); };
  const SW = [
    [0, 0x10, 0x40, {}, { vol: 50, prog: 120, pan: 0x40 }, null],
    [1, 0x11, 0x40, { occ: 1 }, { vol: 50, prog: 1, pan: 0x40 }, { bank: { ins: INS({}) } }],
    [2, 0x12, 0x40, {}, { vol: 50, prog: 2, pan: 0x40 }, { bank: null }],
    [3, 0x13, 0x40, {}, { vol: 50, prog: 3, pan: 0x40 }, { bank: { f214: 1, adv: 0x85, ins: null } }],
    [4, 0x14, 0x40, {}, { vol: 50, prog: 4, pan: 0x40 }, { bank: { ins: INS({ s60: 0 }) } }],
    [5, 0x3c, 0x50, { t45c: 0x1000, t460: 3 }, { vol: 100, prog: 5, pan: 0x40 }, { f4: 0, bank: { ins: INS({}) } }],
    [6, 0x3d, 0x60, { t460: -2 }, { vol: 90, prog: 6, pan: 0x7f },
      { f4: 0x19, r8: 50, rc: 10, bank: { ins: INS({ s5c: 1, c8: 0x40, cnt: 3, subs: [1, 0] }) } }],
    [7, 0x3e, 0x70, {}, { vol: 80, prog: 7, pan: 0 },
      { f4: 0x02, r10: 1000, r14: 7, bank: { f214: 8, ins: INS({ s5c: 1, cnt: 3, subs: [1, 1] }) } }],
    [8, 0x3f, 0x40, {}, { vol: 50, prog: 8, pan: 0x40 }, { bank: { ins: INS({ c8: 1, an: 1 }) } }],
    [9, 0x40, 0xf0, { t460: -5 }, { vol: 0xffffffff, prog: 9, pan: 0 },
      { f4: 0x12, r10: 0xffffffff, r14: 0, bank: { ins: INS({ c8: 0x09, i228: 1 }) } }],
    [10, 0x41, 0x40, {}, { vol: 60, prog: 10, pan: 0x41 }, { f4: 8, bank: { ins: INS({ s5c: 1, cnt: 1 }) } }],
    [11, 0x42, 0x40, { f38: 1 }, { vol: 60, prog: 11, pan: 0x41 }, { bank: { ins: INS({ c8: 0x41 }) } }],
    [12, 0x43, 0x22, { first: 1, xl: 2 }, { vol: 70, prog: 12, pan: 0x3f }, { bank: { ins: INS({ i210: 0x50 }) } }],
    [13, 0x44, 0x40, {}, { vol: 70, prog: 13, pan: 0x10 },
      { bank: { f214: 9, adv: 0x203, ins: INS({ s5c: 1, c8: 1, cnt: 3, subs: null }) } }],
    [14, 0x45, 0x7f, {}, { vol: 0x7f, prog: 14, pan: 0x50 },
      { f4: 0x1b, r8: 5, rc: 9, r10: 3, r14: 2, bank: { ins: INS({ c8: 1 }) } }],
    [15, 0x46, 0x01, { t460: 0x7fffffff }, { vol: 0x10000, prog: 15, pan: 0x7f },
      { bank: { ins: INS({ s5c: 1, c8: 8, cnt: 2, subs: [0] }) } }],
  ];
  F.c3fl_no_hw = HW; F.c3fl_no_sw = SW;
  F.c3fl_no = function (mode) {
    const out = {};
    out.log = H.stubLog(0x400);
    const s = __importStandIn({ module: 'sound.dll', imports: { 'WINMM.dll!midiOutShortMsg': { args: 2, ret: 0 } } });
    out.implog = s.log;
    const ARENA = 0x28000;
    const arena = H.zero(ARENA);
    out.arena = arena;
    let cur = 0;
    function take(n) { const p = arena.add(cur); cur += (n + 0xf) & ~0xf; if (cur > ARENA) throw new Error('c3fl_no: arena'); return p; }
    const tvt = H.recVtable(out.log, { 4: [1, function (a) { return (a[1].toUInt32() + a[0].add(0x18).readU32()) >>> 0; }] });
    const ivt = H.recVtable(out.log, {
      0x60: [0, function (a) { return a[0].add(0x10).readU32(); }],
      0x5c: [0, function (a) { return a[0].add(0x14).readU32(); }],
      0x9c: [1, function () { return 0x9c; }], 0x48: [1, function () { return 0x48; }], 0x40: [1, function () { return 0x40; }],
    });
    const d = H.zero(0xa00);
    out.d = d;
    H.u32(d, 0x730, mode === 'swbig' ? 0x12345678 : 100);
    const gm = H.block(0x40, 0x7b000000);
    H.u32(gm, 0x3c, mode === 'hw' ? 0x3c3c0001 : 0);
    out.gm = gm;
    const eng = H.block(0x40, 0x7c000000);
    function track(i, t, note) {
      const p = take(0x700);
      p.writePointer(tvt);
      H.u32(p, 0x18, t.xl || 0); H.u32(p, 0x1c, t.first || 0); H.u8(p, 0x38, t.f38 || 0);
      H.u32(p, 0x45c, t.t45c || 0); H.u32(p, 0x460, t.t460 || 0);
      if (t.occ) H.u32(p, 0x58 + 4 * note, 0x0cc00cc0);
      out['t' + i] = p;
      return p;
    }
    function instrument(sp, tag) {
      const p = take(0x1480);
      p.writePointer(ivt);
      H.u32(p, 0x10, sp.s60); H.u32(p, 0x14, sp.s5c); H.u32(p, 0x18, sp.an); H.u32(p, 0xc8, sp.c8);
      H.u32(p, 0x210, sp.i210); H.u32(p, 0x228, sp.i228); H.u32(p, 0x1438, sp.cnt);
      H.u32(p, 0x294, 0x29400000 + tag);
      const v = take(0x100);
      H.marker(v, 0x100, 0x7d000000 + 0x1000 * tag);
      p.add(0x1c).writePointer(v);
      (sp.subs || []).forEach(function (busy, k) {
        const sub = instrument(INS({ s5c: busy, i210: 0x20 + k }), tag + 0x10 * (k + 1));
        p.add(0x1440 + 4 * k).writePointer(sub);
      });
      return p;
    }
    const list = mode === 'hw' ? HW : SW;
    list.forEach(function (e, i) {
      const ch = e[0], note = e[1], vel = e[2];
      const m = take(0x10);
      H.u8(m, 0, note); H.u8(m, 1, vel);
      out['m' + i] = m;
      track(i, e[3], note);
      if (mode === 'hw') return;
      const rec = d.add(0x250 + 0x4c * ch);
      H.u32(rec, 4, e[4].vol); H.u32(rec, 0xc, e[4].prog); H.u32(rec, 0x10, e[4].pan);
      const pgs = e[5];
      if (!pgs) return;
      const pg = take(0x40);
      H.u32(pg, 4, pgs.f4 || 0); H.u32(pg, 8, pgs.r8 || 0); H.u32(pg, 0xc, pgs.rc || 0);
      H.u32(pg, 0x10, pgs.r10 || 0); H.u32(pg, 0x14, pgs.r14 || 0);
      d.add(0x744 + 4 * e[4].prog).writePointer(pg);
      if (!pgs.bank) return;
      const bk = take(0x220);
      H.u32(bk, 0x214, pgs.bank.f214 || 0); H.u32(bk, 0x218, pgs.bank.adv || 0);
      pg.add(0x30).writePointer(bk);
      const key = (pgs.bank.f214 & 1) ? pgs.bank.adv : note;
      if (pgs.bank.ins) bk.add(4 + 4 * (key & 0x7f)).writePointer(instrument(pgs.bank.ins, i + 1));
    });
    H.quietTimer(out);
    H.swapGlobal(0xb4a1c, gm.toUInt32(), out, 'gmidi');
    H.swapGlobal(0xb4a20, eng.toUInt32(), out, 'geng');
    H.stubOn(out.log, 0x1906, 0, 'thiscall', function (a) { return a[0].add(0x1c).readU32(); });
    H.stubOn(out.log, 0x1546, 0, 'thiscall', function (a) { return a[0].add(0x218).readU32(); });
    H.stubOn(out.log, 0x1a3c, 1, 'thiscall', function (a, n) {
      const ins = a[0].sub(0x70);
      if (ins.add(0x18).readU32()) return 0;
      a[1].writeU32((0x51070000 + n) >>> 0);
      return ins.add(0x1c).readPointer();
    }, { skip: [1] });
    H.stubOn(out.log, 0x209a, 1, 'thiscall', function (a, n) { return (0x12340000 + 0x1111 * n + a[1].toUInt32()) >>> 0; });
    [0x231a, 0x2040, 0x1212, 0x2351].forEach(function (r) {
      H.stubOn(out.log, r, 1, 'thiscall', function () { return r; });
    });
    return out;
  };
  ['hw', 'sw', 'swbig'].forEach(function (m) { F['c3fl_no_' + m] = function () { return F.c3fl_no(m); }; });
})();

// ---- K. renderChannelVoice: four channels {byte +0x5c, words +0x182 / +0x18e (format), +0x1c0 rate, double +0x1e0, +0x224 pan,
//      +0x230 step}: S (stereo, +0x182 = 2), E (8-bit, +0x18e = 8), M (mono), O (mono, odd rate, +0x5c bit 2); one voice per vector
//      {+0x18, +0x1c position, +0x30 the mono mixer's scripted result, +0x50, doubles +0x58 / +0x60 / +0xf0 / +0xf8, byte +0x94,
//      byte +0x9c, +0xc0, +0xc4}, a 0x100-byte marker buffer and an out cell per vector. g_soundEngine swapped per key: _eng (slot
//      +0x40 returns 22050), _eng0 (returns 0: q = +inf), _noeng (NULL). The five mixers (cdecl, recording stubs) add 0x100 + count
//      to the position through their frame pointer (logged as 0); the mono ones return the voice's +0x30, the channel ones 0x777.
//      Voice_setPanGains 0x1000231a recorded ------------------------------------------------------------------------------------
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3fl_h;
  const SNAN = [1, 0x7ff00000], QNAN = [0, 0x7ff80000];
  // [ch, 9c, c0, c4, v30 (mono result), v50, v58, v60, vf0, vf8, count, pass]
  const VS = [
    ['M', 0x04, 0, 0, 0, 100, 1.5, 0, 0.5, 2.5, 4, 0x10],       // 0 bit 2: no mix; rendered 0 != 4, big -> clear all, reset
    ['M', 0x00, 5, 100, 0, 100, 1.5, 0, 0.5, 2.5, 3, 0xc],      // 1 +0xc4 above the sum: *out = count, big clear
    ['M', 0x00, 5, 100, 0, 100, 1.5, 0, 0.5, 2.5, 3, 0xb],      // 2 the same without clearing
    ['S', 0x00, 0, 0, 0, 100, 1.5, 0, 0.5, 2.5, 5, 0x20],       // 3 big stereo set; rendered 0: tail cleared, reset
    ['E', 0x00, 0, 0, 0, 100, 1.5, 0, 0.5, 2.5, 5, 0],          // 4 8-bit (not big): no clear, reset
    ['M', 0x00, 0, 0, 6, 100, 1.5, 0, 0.5, 2.5, 6, 0xd],        // 5 big mono set returns count: no tail
    ['M', 0x18, 0, 0, 6, -777, 1 / 3, 0, 0.5, 0.25, 6, 1],      // 6 mono add; flags 8|0x10: reset, pan, rate reset (B > A? no)
    ['S', 0x18, 0, 0, 0, 1000, 0.1, 0, 0.5, 0.75, 2, 0],        // 7 stereo add; reset path A with q = 2
    ['M', 0x28, 0, 0, 4, 1, 5.0, 3.0, 1.0, 2.0, 4, 0],          // 8 B, bit 5: x = 4 >= 3 clears bits 3 / 5
    ['M', 0x28, 0, 0, 4, 1, 5.0, 4.5, 1.0, 2.0, 4, 0],          // 9 B, bit 5: x = 4 < 4.5 keeps them
    ['M', 0x08, 0, 0, 2, 7, 1.0, 0, 2.0, 0.125, 4, 0x30],       // 10 B: x = -1 <= 0: reset, rate reset; rendered 2: tail cleared
    ['M', 0x08, 0, 0, 4, 7, 3.0, 0, 2.0, 0.125, 4, 0],          // 11 B: 0 < x = 1 < 2: +0xf0 = 1, bit 4
    ['M', 0x08, 0, 0, 4, 7, 10.0, 0, 2.0, 0.125, 4, 0],         // 12 B: x = 8 >= 2: nothing
    ['M', 0x08, 0, 0, 4, 7, 10.0, 0, QNAN, 0.125, 4, 0],        // 13 B: x NaN: reset path
    ['M', 0x28, 0, 0, 4, 7, 10.0, QNAN, 2.0, 0.125, 4, 0],      // 14 B, bit 5, +0x60 NaN: keeps the bits
    ['M', 0x1c, 0, 0, 4, 7, 10.0, 0, 2.0, 0.125, 0, 0],         // 15 bit 2 with path A; count 0 = rendered: no final reset
    ['O', 0x18, 0, 0, 3, 0x7fffffff, 1e-3, 0, 0.5, SNAN, 3, 0x40],   // 16 odd rate; path A; +0x5c bit 2: no tail clear
    ['E', 0x08, 0, 0, 0, 3, 0.0, 0, 0.0, 0.0, 1, 0x10],         // 17 B: x = 0 (C3): reset path
    ['O', 0x08, 0, 0, 3, -5, 1e10, 0, -3.0, 9.0, 3, 0],          // 18 B: x > 0 and > +0xf0 (negative): nothing
    ['M', 0x08, 0, 0, 1, 5, 2.0, 0, SNAN, 1.0, 1, 0],           // 19 B: +0xf0 SNaN: x NaN -> reset
  ];
  F.c3fl_rv_vs = VS;
  F.c3fl_rv = function (mode) {
    const out = {};
    out.log = H.stubLog(0x400);
    function d64(p, off, v) { if (Array.isArray(v)) H.bits64(p, off, v[0], v[1]); else H.f64(p, off, v); }
    const chs = {};
    [['S', 2, 0, 44100, 0.1, 0], ['E', 0, 8, 22050, 1 / 3, 0], ['M', 0, 0, 11025, 0.7, 0], ['O', 0, 0, 30000, 1e-7, 4]]
      .forEach(function (c, k) {
        const p = H.block(0x400, 0xc1000000 + 0x10000 * k);
        H.u16(p, 0x182, c[1]); H.u16(p, 0x18e, c[2]); H.u32(p, 0x1c0, c[3]); H.f64(p, 0x1e0, c[4]); H.u8(p, 0x5c, c[5]);
        H.u32(p, 0x224, 0x2240 + k); H.u32(p, 0x230, 3 + k);
        chs[c[0]] = p; out['ch' + c[0]] = p;
      });
    VS.forEach(function (e, i) {
      const v = H.block(0x120, 0xc2000000 + 0x10000 * i);
      H.u8(v, 0x9c, e[1]); H.u32(v, 0xc0, e[2]); H.u32(v, 0xc4, e[3]); H.u32(v, 0x30, e[4]); H.u32(v, 0x50, e[5]);
      d64(v, 0x58, e[6]); d64(v, 0x60, e[7]); d64(v, 0xf0, e[8]); d64(v, 0xf8, e[9]);
      H.u32(v, 0x18, 0x180 + i); H.u32(v, 0x1c, 0x1c0 + i); H.u8(v, 0x94, 0xff);
      out['v' + i] = v;
      out['b' + i] = H.block(0x100, 0xc3000000 + 0x10000 * i);
      out['o' + i] = H.block(8, 0xc4000000 + 0x10000 * i);
    });
    let eng = null;
    if (mode !== 'noeng') {
      eng = H.block(0x40, 0xc5000000);
      H.u32(eng, 0x10, mode === 'eng0' ? 0 : 22050);
      eng.writePointer(H.recVtable(out.log, { 0x40: [0, function (a) { return a[0].add(0x10).readU32(); }] }));
      out.eng = eng;
    }
    H.quietTimer(out);
    H.swapGlobal(0xb4a20, eng ? eng.toUInt32() : 0, out, 'geng');
    [[0x10dc, 0], [0x12ee, 0], [0x2220, 0], [0x1f64, 1], [0x2617, 1]].forEach(function (m) {
      H.stubOn(out.log, m[0], 5, 'cdecl', function (a) {
        a[1].writeU32((a[1].readU32() + 0x100 + a[2].toUInt32()) >>> 0);
        return m[1] ? a[3].add(0x30).readU32() : 0x777;
      }, { skip: [1] });
    });
    H.stubOn(out.log, 0x231a, 1, 'thiscall', function () { return 0x231a; });
    return out;
  };
  ['eng', 'eng0', 'noeng'].forEach(function (m) { F['c3fl_rv_' + m] = function () { return F.c3fl_rv(m); }; });
})();

// ---- L. writeWaveFile: three writers {+4, +0x18 last time, words +0x1e / +0x2a, +0x30 mmio handle, +0x34}; six DsBuffers {+0x18,
//      +0x1c, +0x40 / +0x44 / +0x48 DsBuffer::getPosition's bytes / offset / result, +0x50..+0x5c DsBuffer::restore's p1 / n1 / p2
//      / n2, word +0xb4 block align} with 0x40-byte marker lock buffers; g_soundEngine swapped for an engine whose list +0x1c8
//      ({+4 next, +8 object}, cursor +0x1d0) holds the key's objects (0x300 bytes, recording vtable: +0x78 kind = +0x10, +0x5c =
//      +0x14 (and ORs +0x24 into the byte +0xcc), +0x60 = +0x18, +0xcc = +0x1c; fields +0x58, +0xc4 busy block, +0xc8, +0xcc and the
//      format words), or NULL for _noeng. timeGetTime returns 0x10000, mmioWrite 0x10 (stood in). Every callee thunk is a recording
//      stub; frame pointers are logged as 0 ------------------------------------------------------------------------------------
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3fl_h;
  // [getPos result, bytes, offset, align, +0x18, +0x1c, n1, p2?, n2]
  const BUFS = [[7, 0x40, 0, 4, 0, 0, 0x10, 0, 0], [0, 0x40, 0, 0, 0, 0, 0x10, 0, 0], [0, 3, 0, 4, 0, 0, 0x10, 0, 0],
                [0, 0x40, 0x100, 4, 1, 2, 0x20, 0, 0], [0, 0x80, 0x200, 2, 3, 4, 0x10, 1, 0x18], [0, 0x1000, 0x40, 0x10, 5, 9, 0, 1, 8]];
  // [+0x18 last, +0x30 handle, word +0x1e, word +0x2a]
  const WRS = [[0, 0x3030, 1, 0x10], [0x1000, 0, 2, 8], [0x20000, 0x3031, 1, 0x10]];
  const O = function (o) {
    return Object.assign({ k: 1, s5c: 0, s60: 1, scc: 0, f58: 0, c8: 0, cc: 0, ca: 0, be: 0, e6: 0, da: 0, x1fe: 0, x1f2: 0,
                           c4: null, set: 0 }, o);
  };
  const A = O({ s60: 0 }), B = O({ s5c: 1, x1fe: 8, x1f2: 1 }), C = O({ s5c: 1, scc: 1 }), D = O({ s5c: 1, scc: 1, c8: 1 });
  const E = O({ cc: 4, scc: 1, c4: 0 }), Fo = O({ cc: 4 }), G = O({ cc: 4, c4: 1 }), Ho = O({}), I = O({ s5c: 1, set: 4, c4: 0 });
  const J = O({ s5c: 1, x1fe: 0, x1f2: 0, cc: 0 });
  const SETS = {
    noeng: null, empty: [], nullobj: [null],
    k6: [O({ k: 6 }), O({ k: 6, s5c: 1, ca: 8, be: 1 }), O({ k: 2 }), O({ k: 6, s5c: 1 })],
    k4: [O({ k: 4 }), O({ k: 4, s5c: 1, e6: 8, da: 1 }), O({ k: 4, s5c: 1, f58: 0x10 }), O({ k: 0xffffffff })],
    k1: [A, B, C, D, E, Fo, G, Ho, I],
    k1b: [Ho, C, D, E, A],
    mix: [O({ k: 6, s5c: 1, ca: 8 }), O({ k: 4, s5c: 1, f58: 0x10, e6: 8 }), J, O({ k: 6, s5c: 1, be: 1 })],
  };
  F.c3fl_wf = function (set) {
    const out = {};
    out.log = H.stubLog(0x800);
    const s = __importStandIn({ module: 'sound.dll', imports: {
      'WINMM.dll!timeGetTime': { args: 0, ret: 0x10000 },
      'WINMM.dll!mmioWrite': { args: 3, ret: 0x10 },
    } });
    out.implog = s.log;
    BUFS.forEach(function (e, j) {
      const b = H.block(0x100, 0xd1000000 + 0x10000 * j);
      H.u32(b, 0x48, e[0]); H.u32(b, 0x40, e[1]); H.u32(b, 0x44, e[2]); H.u16(b, 0xb4, e[3]);
      H.u32(b, 0x18, e[4]); H.u32(b, 0x1c, e[5]);
      const p1 = H.block(0x40, 0xd2000000 + 0x10000 * j);
      b.add(0x50).writePointer(p1); H.u32(b, 0x54, e[6]);
      let p2 = null;
      if (e[7]) { p2 = H.block(0x40, 0xd3000000 + 0x10000 * j); out['q' + j] = p2; }
      b.add(0x58).writePointer(p2 || ptr(0)); H.u32(b, 0x5c, e[8]);
      out['b' + j] = b; out['p' + j] = p1;
    });
    WRS.forEach(function (e, i) {
      const w = H.block(0x60, 0xd4000000 + 0x10000 * i);
      H.u32(w, 0x18, e[0]); H.u32(w, 0x30, e[1]); H.u16(w, 0x1e, e[2]); H.u16(w, 0x2a, e[3]);
      out['w' + i] = w;
    });
    let eng = null;
    const S = SETS[set];
    if (S !== null) {
      eng = H.block(0x200, 0xd5000000);
      out.eng = eng;
      const vt = H.recVtable(out.log, {
        0x78: [0, function (a) { return a[0].add(0x10).readU32(); }],
        0x5c: [0, function (a) {
          const o = a[0], x = o.add(0x24).readU32();
          if (x) o.add(0xcc).writeU8(o.add(0xcc).readU8() | x);
          return o.add(0x14).readU32();
        }],
        0x60: [0, function (a) { return a[0].add(0x18).readU32(); }],
        0xcc: [0, function (a) { return a[0].add(0x1c).readU32(); }],
      });
      let prev = null;
      for (let k = S.length - 1; k >= 0; k--) {
        const n = H.block(0x10, 0xd6000000 + 0x100 * k);
        n.add(4).writePointer(prev || ptr(0));
        const sp = S[k];
        if (sp) {
          const o = H.block(0x300, 0xd7000000 + 0x10000 * k);
          o.writePointer(vt);
          H.u32(o, 0x10, sp.k); H.u32(o, 0x14, sp.s5c); H.u32(o, 0x18, sp.s60); H.u32(o, 0x1c, sp.scc); H.u32(o, 0x24, sp.set);
          H.u32(o, 0x58, sp.f58); H.u32(o, 0xc8, sp.c8); H.u8(o, 0xcc, sp.cc);
          H.u16(o, 0xca, sp.ca); H.u16(o, 0xbe, sp.be); H.u16(o, 0xe6, sp.e6); H.u16(o, 0xda, sp.da);
          H.u16(o, 0x1fe, sp.x1fe); H.u16(o, 0x1f2, sp.x1f2);
          if (sp.c4 === null) o.add(0xc4).writePointer(ptr(0));
          else { const c = H.block(0x60, 0xd8000000 + 0x10000 * k); H.u32(c, 0x48, sp.c4); o.add(0xc4).writePointer(c); out['c' + k] = c; }
          n.add(8).writePointer(o);
          out['o' + k] = o;
        } else {
          n.add(8).writePointer(ptr(0));
        }
        out['n' + k] = n;
        prev = n;
      }
      eng.add(0x1c8).writePointer(prev || ptr(0));
      H.u32(eng, 0x1d0, 0x1d01d0);
    }
    H.quietTimer(out);
    H.swapGlobal(0xb4a20, eng ? eng.toUInt32() : 0, out, 'geng');
    H.stubOn(out.log, 0x233d, 3, 'thiscall', function (a) {
      a[1].writeU32(a[0].add(0x40).readU32()); a[2].writeU32(a[0].add(0x44).readU32());
      return a[0].add(0x48).readU32();
    }, { skip: [1, 2] });
    H.stubOn(out.log, 0x213f, 6, 'thiscall', function (a) {
      a[3].writePointer(a[0].add(0x50).readPointer()); a[4].writeU32(a[0].add(0x54).readU32());
      a[5].writePointer(a[0].add(0x58).readPointer()); a[6].writeU32(a[0].add(0x5c).readU32());
      return 0x213f;
    }, { skip: [3, 4, 5, 6] });
    H.stubOn(out.log, 0x1b9f, 0, 'thiscall', function (a) { return a[0].add(0x48).readU32(); });
    [[0x16a9, 4, [4]], [0x1c6c, 4, [4]], [0x119f, 4, [4]], [0x20c7, 3, [3]], [0x1302, 3, [3]], [0x1ec9, 4, []], [0x1b13, 4, []],
     [0x1113, 0, []], [0x154b, 1, []], [0x22f2, 0, []], [0x1f0f, 0, []]].forEach(function (e) {
      H.stubOn(out.log, e[0], e[1], 'thiscall', function () { return e[0]; }, { skip: e[2] });
    });
    return out;
  };
  Object.keys(SETS).forEach(function (k) { F['c3fl_wf_' + k] = function () { return F.c3fl_wf(k); }; });
})();

// ---- M. Wave_Device::dispatchCommand: one device per vector {+0x1a4 flags, +0x1c8 list, +0x1d8 ring of 0x1000 command pointers,
//      +0x41d8 read, +0x41dc write}; commands {+0 kind, +4 channel, +8, +0xc} and channels {byte +0x44 / +0x58 / +0xc8 / +0xcc /
//      +0x1428, +0xc4 decoder buffer, +0xf8 / +0xfc engine objects, +0x1420 window, +0x1430 / +0x1434, +0x1438 sub count, +0x1440
//      sub channels, a vtable with slots 0 (returns 0) and 0xcc (returns the channel's +0x24)} are carved from one zeroed arena
//      ($arena, compared). operator new 0x100424d7 / delete 0x1004249a go through the heap stand-in (its cursor and free log are
//      compared); listNode3Init 0x10002031 is a stub that zeroes the 0x10 block and returns it; every other callee is a recording
//      stub; PostMessageA (slot 0x100b85a0) stood in. Channels unlinked by a handler are pre-linked onto the device list. key _post
//      sets bit 5 of each device's +0x1a4 (PostMessageA), _thread clears it (postSoundThreadMessage thunk 0x100011d6) -------------
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3fl_h;
  // Each device: a list of commands [kind, channelSpec|null, c8, cc]. channelSpec: object with fields; `pre` links it on the list.
  const CH = function (o) {
    return Object.assign({ f44: 0, f58: 0, c8: 0, cc: 0, b1428: 0, c4: 0, f8: 0, fc: 0, cnt: 0, subs: null, pre: false,
                           s0cc: 0 }, o);
  };
  // device specs: array of commands [kind, chIndex (-1 = NULL channel), c8val, ccval]; channels declared per device
  const DEVS = [
    // 0: kind 1 variants
    { chs: [CH({ f44: 4, c8: 0x40 }), CH({ f44: 4, b1428: 1 }), CH({ f44: 4, f58: 0 }),
            CH({ f44: 4, f58: 2, s0cc: 1, c8: 1 }), CH({ f44: 0x100, f58: 0 }), CH({ f44: 0x100, f58: 2 })],
      cmds: [[1, 0, 0, 0], [1, 1, 0, 0], [1, 2, 0, 0], [1, 3, 0, 0], [1, 4, 0, 0], [1, 5, 0, 0]] },
    // 1: kind 2
    { chs: [CH({ f44: 4, f58: 2, pre: true }), CH({ f44: 4, f58: 0, c4: 1, pre: true }),
            CH({ f44: 0x100, f58: 3, pre: true }), CH({ f44: 0, f58: 0 })],
      cmds: [[2, 0, 0, 0], [2, 1, 0, 0], [2, 2, 0, 0], [2, 3, 0, 0]] },
    // 2: kind 3
    { chs: [CH({ f44: 4, cnt: 3, subs: [true, false], pre: true }), CH({ f44: 0x100, pre: true }), CH({ f44: 0 })],
      cmds: [[3, 0, 0, 0], [3, 1, 0, 0], [3, 2, 0, 0]] },
    // 3: kind 4
    { chs: [CH({ f44: 4, c8: 0x40, pre: true }), CH({ f44: 4, c8: 0 }), CH({ f44: 0x100, f58: 2, pre: true }),
            CH({ f44: 0x100, f58: 0 })],
      cmds: [[4, 0, 0, 0], [4, 1, 0, 0], [4, 2, 0, 0], [4, 3, 0, 0]] },
    // 4: kinds 6, 13, 5, null, and a trailing 0-kind (default)
    { chs: [CH({ cc: 4 }), CH({ cc: 0 }), CH({}), CH({})],
      cmds: [[6, 0, 0, 0], [6, 1, 0, 0], [13, 2, 0x1234, 0x5678], [5, 3, 0, 0], [0, 3, 0, 0], [7, -1, 0, 0]] },
    // 5: kinds 14, 15, 16 (channel +0xf8 engine object)
    { chs: [CH({ f8: 1 }), CH({ f8: 1 }), CH({ f8: 1 }), CH({ f8: 0 })],
      cmds: [[14, 0, 0, 0xaa], [14, 0, 1, 0xbb], [15, 1, 0x99, 0], [16, 2, 0, 0], [14, 3, 0, 0]] },
    // 6: kind 17
    { chs: [CH({ f8: 1, fc: 1 }), CH({ f8: 1, fc: 0 }), CH({ f8: 0, fc: 0 })],
      cmds: [[17, 0, 0, 0], [17, 1, 0, 0], [17, 2, 0, 0], [17, -1, 0, 0]] },
    // 7: a NULL command slot in the middle (counts an iteration without advancing), then more
    { chs: [CH({ cc: 4 }), CH({ cc: 4 })], cmds: [[6, 0, 0, 0], null, [6, 1, 0, 0]] },
  ];
  F.c3fl_wd = function (mode) {
    const out = {};
    out.log = H.stubLog(0x1000);
    const hp = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], free: [0x4249a], arenaSize: 0x2000 });
    out.freelog = hp.log;
    out.heaparena = hp.arena;
    const s = __importStandIn({ module: 'sound.dll', imports: { 'USER32.dll!PostMessageA': { args: 4, ret: 1 } } });
    out.implog = s.log;
    const ARENA = 0x80000;
    const arena = H.zero(ARENA);
    out.arena = arena;
    let cur = 0;
    function take(n) { const p = arena.add(cur); cur += (n + 0xf) & ~0xf; if (cur > ARENA) throw new Error('c3fl_wd: arena'); return p; }
    const chvt = H.recVtable(out.log, {
      0: [1, function () { return 0; }],
      0xcc: [0, function (a) { return a[0].add(0x24).readU32(); }],
      0x34: [3, function () { return 0x34; }],
      0x40: [1, function () { return 0x40; }],
    });
    function mkch(sp, tag) {
      const o = take(0x1480);
      o.writePointer(chvt);
      H.u32(o, 0x24, sp.s0cc); H.u32(o, 0x44, sp.f44); H.u32(o, 0x58, sp.f58); H.u32(o, 0xc8, sp.c8);
      H.u8(o, 0xcc, sp.cc); H.u8(o, 0x1428, sp.b1428); H.u32(o, 0x1438, sp.cnt);
      H.u32(o, 0x1420, 0x14200000 + tag);
      if (sp.c4) H.u32(o, 0xc4, take(0x80).toUInt32());
      if (sp.f8) o.add(0xf8).writePointer(mkch(CH({}), tag + 0x1000));
      if (sp.fc) o.add(0xfc).writePointer(mkch(CH({}), tag + 0x2000));
      (sp.subs || []).forEach(function (on, k) { if (on) o.add(0x1440 + 4 * (k + 1)).writePointer(mkch(CH({}), tag + 0x3000 + k)); });
      return o;
    }
    DEVS.forEach(function (ds, di) {
      const dev = take(0x4200);
      H.u32(dev, 0x1a4, mode === 'post' ? 0x20 : 0);
      const chs = ds.chs.map(function (sp, ci) { const o = mkch(sp, di * 0x100 + ci); out['d' + di + 'c' + ci] = o; return o; });
      // pre-link channels flagged pre onto the list dev+0x1c8
      let prev = null, head = null, count = 0;
      ds.chs.forEach(function (sp, ci) {
        if (!sp.pre) return;
        const n = take(0xc);
        n.add(8).writePointer(chs[ci]);
        n.writePointer(prev || ptr(0));
        if (prev) prev.add(4).writePointer(n); else head = n;
        prev = n; count++;
      });
      dev.add(0x1c8).writePointer(head || ptr(0));
      dev.add(0x1cc).writePointer(prev || ptr(0));
      H.u32(dev, 0x1d4, count);
      // ring
      ds.cmds.forEach(function (cm, k) {
        const slot = dev.add(0x1d8 + 4 * ((k + 1) & 0xfff));
        if (cm === null) { slot.writePointer(ptr(0)); return; }
        const c = take(0x10);
        H.u32(c, 0, cm[0]);
        c.add(4).writePointer(cm[1] < 0 ? ptr(0) : chs[cm[1]]);
        H.u32(c, 8, cm[2]); H.u32(c, 0xc, cm[3]);
        slot.writePointer(c);
      });
      H.u32(dev, 0x41d8, 0);
      H.u32(dev, 0x41dc, ds.cmds.length);
      out['dev' + di] = dev;
    });
    H.quietTimer(out);
    H.stubOn(out.log, 0x2257, 1, 'thiscall', function (a) { return a[0].add(4 * (a[1].toUInt32() & 0xfff)).readU32(); });
    H.stubOn(out.log, 0x2031, 0, 'thiscall', function (a) { const p = a[0]; p.writeU32(0); H.u32(p, 4, 0); H.u32(p, 8, 0); return p; });
    H.stubOn(out.log, 0x154b, 1, 'thiscall', function () { return 0x154b; });
    [[0x1f37, 0], [0x11f9, 0], [0x22e8, 0], [0x168b, 0], [0x1e79, 0], [0x272a, 1], [0x1b9f, 0], [0x11d6, 3]].forEach(function (e) {
      H.stubOn(out.log, e[0], e[1], 'thiscall', function () { return e[0]; });
    });
    H.swapGlobal(0xb4a20, 0, out, 'geng');   // not read by dispatchCommand but keep the live engine out of the picture
    return out;
  };
  ['post', 'thread'].forEach(function (m) { F['c3fl_wd_' + m] = function () { return F.c3fl_wd(m); }; });
})();
