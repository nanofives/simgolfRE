// Fixtures for C3 batch c3fe (sound.dll, round 33). Layouts come from the disassembly cited in shim/src/re/c3fe.cpp.
// Every object, vtable, list node and message here is allocated by the fixture; all values are synthetic.
// The sound engine is LIVE on this PC (log/c3/c3fe_notes.txt section 1): the multimedia-timer thread runs
// SoundTimer::onTick 0x10008ed0 ~32 times/s. Each key therefore (1) builds its objects, (2) freezes the first thread
// (__freeze), (3) holds the timer off: it waits for an idle moment (re-entrancy word 0x100b49a8 = 0 and the timer's
// critical section 0x100b4990 unowned), writes 5 (onTick then leaves at 0x10008edf before touching anything) and waits
// until the word has stayed 5 with the section unowned for 100 ms, so a tick that passed the test just before the
// write has finished; the word goes back to its idle value at the flush, after every other restore. Helpers copied
// from c3ew.js (thread-filtered recording stubs on incremental-link thunks, recording vtables), plus the x87 control
// word read once per boot into log/c3/alt/c3fe_cw.txt.
(function () {
  const F = globalThis.DIFF_FIXTURES;
  const keep = function (x) { globalThis.__diffKeepAlive.push(x); return x; };
  const snd = function (rva) { return Process.getModuleByName('sound.dll').base.add(rva); };
  function fill(p, n, b) { p.writeByteArray(new Array(n).fill(b)); return p; }
  function marker(p, n, tag) { for (let k = 0; k + 4 <= n; k += 4) p.add(k).writeU32((tag + k) >>> 0); }
  function u32(p, off, v) { p.add(off).writeU32(v >>> 0); }
  function u8(p, off, v) { p.add(off).writeU8(v & 0xff); }
  function f64(p, off, v) { p.add(off).writeDouble(v); }
  function bits64(p, off, lo, hi) { p.add(off).writeU32(lo >>> 0); p.add(off + 4).writeU32(hi >>> 0); }
  function block(n, tag) { const b = keep(Memory.alloc(n)); marker(b, n, tag); return b; }
  function zero(n) { return fill(keep(Memory.alloc(n)), n, 0); }
  const k32 = Process.getModuleByName('kernel32.dll');
  const sleep = keep(new NativeFunction(k32.getExportByName('Sleep'), 'void', ['uint32'], 'stdcall', { scheduling: 'exclusive' }));

  // x87 control word of the A/B thread (fnstcw): 83 ec 04 / d9 3c 24 / 0f b7 04 24 / 83 c4 04 / c3
  let cwDone = false;
  function recordCw() {
    if (cwDone) return;
    cwDone = true;
    const code = keep(Memory.alloc(Process.pageSize));
    code.writeByteArray([0x83, 0xec, 0x04, 0xd9, 0x3c, 0x24, 0x0f, 0xb7, 0x04, 0x24, 0x83, 0xc4, 0x04, 0xc3]);
    Memory.protect(code, Process.pageSize, 'rwx');
    const cw = new NativeFunction(code, 'uint32', [])();
    try {
      const f = new File('C:\\Users\\maria\\Desktop\\Proyectos\\SimGolf\\log\\c3\\alt\\c3fe_cw.txt', 'a');
      f.write('A/B thread ' + Process.getCurrentThreadId() + ' x87 control word 0x' + cw.toString(16) + '\n');
      f.close();
    } catch (e) { /* the record is a diagnostic only */ }
  }

  let frozenNow = false;
  function guard() {
    if (frozenNow) return;
    recordCw();
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
      if (owner.readU32() !== 0) throw new Error('c3fe: timer section 0x100b4990 stays owned');
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
    if (stable < 50) throw new Error('c3fe: timer did not go quiet');
    out.g49a8 = g;
  }
  function swapGlobal(rva, value, out, name) {
    const g = snd(rva);
    if (name) out[name] = g;
    __freeze.setU32(g, value);
    return g;
  }

  // Recording stub on a thunk (A/B thread only): each call appends {rva, ecx or 0, args...} (0x20 bytes) to `log` (+0 count,
  // entries from +4, 15 kept) and returns retfn(args, n) (0 without retfn). Other threads reach the real function. Reverted at
  // the freeze flush.
  const stubLive = {};
  function stubLog() { const l = keep(Memory.alloc(0x200)); fill(l, 0x200, 0); return l; }
  function logCall(log, tag, a) {
    const n = log.readU32();
    if (n < 15) {
      const p = log.add(4 + 0x20 * n);
      p.writeU32(tag >>> 0);
      for (let i = 0; i < a.length && i < 7; i++) p.add(4 + 4 * i).writeU32(ptr(a[i]).toUInt32());
    }
    log.writeU32(n + 1);
  }
  // skip: argument indices (in the callback's argument list, ecx first for thiscall) logged as 0 because they point into the
  // caller's frame, which differs between the two arms; the stub still reads and writes through them.
  function stubOn(log, rva, nargs, conv, retfn, skip) {
    const base = snd(rva), key = base.toString();
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
      logCall(log, rva, skip ? a.map(function (x, i) { return skip.indexOf(i) >= 0 ? ptr(0) : x; }) : a);
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
  // A fixture vtable whose slots are recording thiscall callbacks: slots = {offset: [nargs, retfn]}
  function recVtable(log, slots) {
    const vt = fill(keep(Memory.alloc(0x100)), 0x100, 0);
    Object.keys(slots).forEach(function (k) {
      const off = parseInt(k, 10), nargs = slots[k][0], retfn = slots[k][1];
      const argt = ['pointer'].concat(Array(nargs).fill('pointer'));
      vt.add(off).writePointer(keep(new NativeCallback(function () {
        const a = Array.prototype.slice.call(arguments);
        const n = log.readU32();
        logCall(log, 0x5100 + off, a);
        const r = retfn ? retfn(a, n) : 0;
        return (typeof r === 'number') ? (r >>> 0) : ptr(r).toUInt32();
      }, 'uint32', argt, 'thiscall')));
    });
    return vt;
  }
  // MSG {hwnd, message, wParam, lParam, time, pt.x, pt.y} written by a PeekMessageA stand-in
  function writeMsg(p, m) { for (let i = 0; i < 7; i++) p.add(4 * i).writeU32((m[i] || 0) >>> 0); }

  F.c3fe_h = { keep: keep, snd: snd, fill: fill, marker: marker, u32: u32, u8: u8, f64: f64, bits64: bits64, block: block,
               zero: zero, guard: guard, quietTimer: quietTimer, swapGlobal: swapGlobal, stubLog: stubLog, stubOn: stubOn,
               logCall: logCall, recVtable: recVtable, writeMsg: writeMsg };
})();

// ---- A. release_sound: g_soundTimer 0x100b49ec swapped; stopKeepLock (thunk 0x100024e6) and stop (thunk 0x1000218a) stubbed;
//      operator delete 0x1004249a recorded by the heap stand-in. mode: null / tmr / clr (the stopKeepLock stub zeroes the global) /
//      swap (it writes a second timer there) ------------------------------------------------------------------------------
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3fe_h;
  F.c3fe_rel = function (mode) {
    const out = {};
    out.log = H.stubLog();
    out.t1 = H.block(0x80, 0x71000000);
    out.t2 = H.block(0x80, 0x72000000);
    out.freelog = __heapStandIn({ module: 'sound.dll', free: [0x4249a] }).log;
    H.quietTimer(out);
    const g = H.swapGlobal(0xb49ec, mode === 'null' ? 0 : out.t1.toUInt32(), out, 'gtmr');
    H.stubOn(out.log, 0x24e6, 0, 'thiscall', function (a) {
      if (mode === 'clr') g.writeU32(0);
      if (mode === 'swap') g.writeU32(out.t2.toUInt32());
      return 0x24e6;
    });
    H.stubOn(out.log, 0x218a, 0, 'thiscall', function (a) { return 0x218a; });
    return out;
  };
  ['null', 'tmr', 'clr', 'swap'].forEach(function (m) { F['c3fe_rel_' + m] = function () { return F.c3fe_rel(m); }; });
})();

// ---- B. pumpWaveMessages: twelve objects with a window number at +8; PeekMessageA (IAT 0x100b85b4) stood in: for window w it
//      returns (w & 3) messages, numbered by the call count of this arm (the stand-in log, compared state), each with lParam =
//      the object whose window it is; soundWindowSubclassProc (thunk 0x100025ef, stdcall 4) recorded; for message 0x7ef it adds
//      0x10 to the object's window number, so the next peek of that object asks for another window ---------------------------
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3fe_h;
  F.c3fe_pump = function () {
    const out = {}, byWin = {};
    out.log = H.stubLog();
    for (let i = 0; i < 12; i++) {
      const o = H.block(0x20, 0x2a000000 + 0x1000 * i);
      const w = 0x5000 + 0x10 * i + (i % 4);                     // i % 4 = 0..3 messages for the first window
      H.u32(o, 8, w);
      out['o' + i] = o;
      byWin[w] = o; byWin[w + 0x10] = o; byWin[w + 0x20] = o;
    }
    const s = __importStandIn({ module: 'sound.dll', imports: {
      'USER32.dll!PeekMessageA': { args: 5, record: [1, 2, 3, 4], impl: function (a, ctx) {
        const w = a[1].toUInt32(), k = ctx.call, o = byWin[w];
        // messages left for this window: (w & 3) minus the ones already taken from it (tracked in the object's +0x10)
        const taken = o ? o.add(0x10).readU32() & 0xff : 3;
        if (!o || taken >= (w & 3)) return 0;
        o.add(0x10).writeU32(((o.add(0x10).readU32() & 0xffffff00) | (taken + 1)) >>> 0);
        const msg = 0x7e8 + ((w >> 4) + taken) % 8;
        H.writeMsg(a[0], [w ^ (taken + 1), msg, 0x100 * k + taken, o.toUInt32(), 0x1234 + k, 7, 9]);
        return 1;
      } },
    } });
    out.implog = s.log;
    for (let i = 0; i < 12; i++) H.u32(out['o' + i], 0x10, 0);
    H.quietTimer(out);
    H.stubOn(out.log, 0x25ef, 4, 'stdcall', function (a, n) {
      if (a[1].toUInt32() === 0x7ef) { const o = a[3]; o.add(8).writeU32((o.add(8).readU32() + 0x10) >>> 0); }
      return 0x700 + n;
    });
    return out;
  };
})();

// ---- C. soundThreadPump: the signal object 0x100b5038 (flags, critical section, thread id, thread handle, event, running byte)
//      saved and seeded; GetCurrentThreadId / MsgWaitForMultipleObjects / Enter- and LeaveCriticalSection / PeekMessageA stood in
//      (results by call number of this arm); soundWindowProc (thunk 0x10001780, cdecl 4) recorded; in mode `drop` it clears the
//      flag bit when it sees message 0x7ee ------------------------------------------------------------------------------------
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3fe_h;
  F.c3fe_tp = function (o) {
    const out = {};
    out.log = H.stubLog();
    const msgs = (o.peeks || []).map(function (m) { return m; });
    const s = __importStandIn({ module: 'sound.dll', imports: {
      'KERNEL32.dll!GetCurrentThreadId': { args: 0, ret: 0x4d2 },
      'USER32.dll!MsgWaitForMultipleObjects': { args: 5, ret: o.waits },
      'KERNEL32.dll!EnterCriticalSection': { ret: 0x3e17 },
      'KERNEL32.dll!LeaveCriticalSection': { ret: 0x5eed },
      'USER32.dll!PeekMessageA': { args: 5, record: [1, 2, 3, 4], impl: function (a, ctx) {
        const m = msgs[ctx.call];
        if (!m) return 0;
        H.writeMsg(a[0], m);
        return 1;
      } },
    } });
    out.implog = s.log;
    H.quietTimer(out);
    out.sig = H.snd(0xb5038);
    __freeze.save(out.sig, 0x2c);
    H.fill(out.sig, 0x2c, 0);
    H.u8(out.sig, 0, o.flag);
    H.u32(out.sig, 0x24, 0x0e0e);                                // the event handle the wait is given (by address)
    const flagp = out.sig;
    H.stubOn(out.log, 0x1780, 4, 'cdecl', function (a, n) {
      if (o.drop && a[1].toUInt32() === 0x7ee) flagp.writeU8(flagp.readU8() & 0xfe);
      return 0x900 + n;
    });
    return out;
  };
  const M = function (h, m, w, l) { return [h, m, w, l, 0x77, 1, 2]; };
  F.c3fe_tp_event = function () { return F.c3fe_tp({ waits: [0], flag: 1 }); };
  F.c3fe_tp_flag0 = function () { return F.c3fe_tp({ waits: [1, 0], flag: 0, peeks: [M(1, 0x7ea, 2, 3)] }); };
  F.c3fe_tp_one = function () { return F.c3fe_tp({ waits: [2, 0], flag: 1, peeks: [M(0x11, 0x7ea, 1, 2), M(0x12, 0x7eb, 3, 4)] }); };
  F.c3fe_tp_two = function () {
    return F.c3fe_tp({ waits: [1, 1, 0], flag: 3, peeks: [M(0x21, 0x7ec, 5, 6), null, M(0x22, 0x7ed, 7, 8), M(0x23, 0x10, 9, 10)] });
  };
  F.c3fe_tp_drop = function () {
    return F.c3fe_tp({ waits: [1, 1, 1, 0], flag: 1, drop: true, peeks: [M(0x31, 0x7ee, 1, 1), M(0x32, 0x7ef, 2, 2)] });
  };
})();

// ---- D. soundWindowProc: the flag byte 0x100b49bc swapped (one key per value 0..3); channel objects for 0x7eb, nodes
//      {kind, object} and recording-vtable objects for 0x7ec / 0x7ee / 0x7ef, wave-in objects for 0x7f0; every callee thunk
//      stubbed (delete_sound 0x10001a87 cdecl 1, serviceDecodeRing 0x100022fc, WaveInDevice_requeueBuffer 0x100020a4 (1 arg),
//      SoundGroup_rewind 0x10002568, SoundGroup_stop 0x100016a4, pairClear 0x10001816: thiscall); operator delete recorded;
//      DefWindowProcA (IAT 0x100b85bc) stood in with a result computed from its four arguments --------------------------------
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3fe_h;
  F.c3fe_wp = function (flagv) {
    const out = {};
    out.log = H.stubLog();
    out.freelog = __heapStandIn({ module: 'sound.dll', free: [0x4249a] }).log;
    const s = __importStandIn({ module: 'sound.dll', imports: {
      'USER32.dll!DefWindowProcA': { args: 4, impl: function (a) {
        return ((a[0].toUInt32() * 3) ^ a[1].toUInt32() ^ (a[2].toUInt32() << 1) ^ (a[3].toUInt32() >>> 3) ^ 0x5a5a0000) >>> 0;
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
    out.oA = obj(0x41000000, 0x0, 1);      // slot 0x60 non-zero, +0x58 bit 1 clear
    out.oB = obj(0x42000000, 0x2, 0);      // slot 0x60 zero, bit 1 set
    out.oC = obj(0x43000000, 0x0, 0);
    out.oD = obj(0x44000000, 0x0, 0);
    function node(tag, kind, o) { const n = H.block(0x20, tag); H.u32(n, 0, kind); n.add(4).writePointer(o || ptr(0)); return n; }
    out.n3a = node(0x51000000, 3, out.oA);
    out.n3b = node(0x52000000, 3, out.oB);
    out.n4a = node(0x53000000, 4, out.oC);
    out.n4n = node(0x54000000, 4, null);
    out.n5 = node(0x55000000, 5, out.oD);
    out.n7 = node(0x56000000, 7, out.oA);
    out.chA = H.block(0x1400, 0x61000000); H.u32(out.chA, 0x58, 0x0ffff6f6 | 2); H.u32(out.chA, 0x1370, 5);
    out.chB = H.block(0x1400, 0x62000000); H.u32(out.chB, 0x58, 0x0ffff6f4); H.u32(out.chB, 0x1370, 0);
    out.wiA = H.block(0x40, 0x63000000); H.u8(out.wiA, 4, 0x02);
    out.wiB = H.block(0x40, 0x64000000); H.u8(out.wiB, 4, 0xfd);
    out.snd1 = H.block(0x40, 0x65000000);
    H.quietTimer(out);
    out.gflag = H.snd(0xb49bc);
    __freeze.save(out.gflag, 4);
    out.gflag.writeU8(flagv);
    H.stubOn(out.log, 0x1a87, 1, 'cdecl', function () { return 0x1a87; });
    // serviceDecodeRing flips bits 3 and 10 of the channel's +0x58 so the reload at 0x10028941 is visible
    H.stubOn(out.log, 0x22fc, 0, 'thiscall', function (a) { const c = a[0]; c.add(0x58).writeU32((c.add(0x58).readU32() ^ 0x408) >>> 0); return 0x22fc; });
    H.stubOn(out.log, 0x20a4, 1, 'thiscall', function () { return 0x20a4; });
    H.stubOn(out.log, 0x2568, 0, 'thiscall', function () { return 0x2568; });
    H.stubOn(out.log, 0x16a4, 0, 'thiscall', function () { return 0x16a4; });
    H.stubOn(out.log, 0x1816, 0, 'thiscall', function () { return 0x1816; });
    return out;
  };
  [0, 1, 2, 3].forEach(function (v) { F['c3fe_wp_f' + v] = function () { return F.c3fe_wp(v); }; });
})();

// ---- E. updateVoiceRate / Channel_startVoice: g_soundEngine 0x100b4a20 swapped for a fixture engine whose slot +0x40 returns
//      the engine's +0x10 (R), or NULL. Twelve channels with a base rate B at +0x1c0 and, for updateVoiceRate, a voice at +0x1390
//      with n (+0x50), D (+0x58), the old +0xf0; for Channel_startVoice, nineteen voice records (index -2..16, 0x110 apart from
//      this + 0x250) with n (+0x288) and D (+0x290), and a double at +0x1388 ------------------------------------------------
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3fe_h;
  const BASES = [11025, 22050, 22049, 22051, 44100, 0, 8000, 48000, 0xffffffff, 1, 32000, 96000];
  const NS = [1000, -1000, 0, 0x7fffffff, -0x80000000, 7, 44100, -3, 123456789, 1, -1, 65536];
  // D: plain values, 0.1 and 1/3 (inexact), -0.0, +0.0, a quiet NaN, a signaling NaN (bits 0x7ff00000_00000001), 1e300, -7.25,
  // +infinity, 1e-300, 3e5. Infinity is written as bits: Frida 17.22.1's writeDouble(Infinity) stores the quiet NaN
  // 0x7ff8000000000000 (checked 2026-10-13).
  const DS = [1.0, 0.1, 1 / 3, -0.0, 0.0, NaN, 'snan', 1e300, -7.25, 'inf', 1e-300, 3e5];
  function setD(p, off, v) {
    if (v === 'snan') H.bits64(p, off, 1, 0x7ff00000);
    else if (v === 'inf') H.bits64(p, off, 0, 0x7ff00000);
    else H.f64(p, off, v);
  }
  function engine(out, rate) {
    const e = H.block(0x40, 0x7e000000);
    e.writePointer(H.recVtable(out.log, { 0x40: [0, function (a) { return a[0].add(0x10).readU32(); }] }));
    H.u32(e, 0x10, rate);
    out.eng = e;
    return e;
  }
  F.c3fe_ur = function (rate) {
    const out = {};
    out.log = H.stubLog();
    const e = rate === null ? null : engine(out, rate);
    for (let i = 0; i < 12; i++) {
      const c = H.block(0x1400, 0x30000000 + 0x10000 * i);
      H.u32(c, 0x1c0, BASES[i]);
      if (i === 5) { H.u32(c, 0x1390, 0); out['c' + i] = c; out['v' + i] = H.block(0x100, 0x3f000000); continue; }
      const v = H.block(0x100, 0x38000000 + 0x10000 * i);
      H.u32(v, 0x50, NS[i]);
      setD(v, 0x58, DS[i]);
      setD(v, 0xf0, i % 3 === 0 ? 0.0 : (i % 3 === 1 ? 4.5 : 'snan'));
      H.u32(v, 0xec, 0xeceeceec);
      c.add(0x1390).writePointer(v);
      out['c' + i] = c; out['v' + i] = v;
    }
    H.quietTimer(out);
    H.swapGlobal(0xb4a20, e ? e.toUInt32() : 0, out, 'geng');
    return out;
  };
  F.c3fe_ur_null = function () { return F.c3fe_ur(null); };
  F.c3fe_ur_22050 = function () { return F.c3fe_ur(22050); };
  F.c3fe_ur_44100 = function () { return F.c3fe_ur(44100); };
  F.c3fe_ur_0 = function () { return F.c3fe_ur(0); };
  F.c3fe_ur_max = function () { return F.c3fe_ur(0xffffffff); };

  // Channel_startVoice: twelve channels of 0x1500 bytes behind 0x300 bytes of slack (index -2 reaches this - 0x220 + 0x250)
  F.c3fe_cs = function (rate) {
    const out = {};
    out.log = H.stubLog();
    const e = rate === null ? null : engine(out, rate);
    for (let i = 0; i < 12; i++) {
      const blk = H.block(0x1800, 0x20000000 + 0x10000 * i);
      const s = blk.add(0x300);
      H.u32(s, 0x58, i === 7 ? 0xfffffffe : (0x0ffff0f1 ^ (i << 4)));
      H.u32(s, 0x1c0, BASES[i]);
      setD(s, 0x1388, DS[(i + 5) % 12]);
      for (let k = -2; k <= 16; k++) {
        const r = s.add(k * 0x110);
        H.u32(r, 0x288, NS[(i + k + 12) % 12]);
        setD(r, 0x290, DS[(i * 3 + k + 24) % 12]);
      }
      out['b' + i] = blk; out['s' + i] = s;
    }
    H.quietTimer(out);
    H.swapGlobal(0xb4a20, e ? e.toUInt32() : 0, out, 'geng');
    return out;
  };
  F.c3fe_cs_null = function () { return F.c3fe_cs(null); };
  F.c3fe_cs_22050 = function () { return F.c3fe_cs(22050); };
  F.c3fe_cs_44100 = function () { return F.c3fe_cs(44100); };
})();

// ---- F. renderChannel: twelve channel objects (0x1400 bytes: +0x58, +0x5c, the fade step +0x1c4 and value +0x1d8, 16 voice
//      records at +0x238 + k * 0x110); the three render callees stubbed at their thunks (Single 0x10002649 thiscall 4, Streamed
//      0x100020c7 thiscall 3, Voice 0x100023f6 thiscall 5); the Voice stub writes *a3 = a1 when bit 0 of the voice's byte +0x10
//      is set, else a1 + its byte +0x11; timeGetTime (IAT 0x100b8654) stood in --------------------------------------------------
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3fe_h;
  // {f58, b5c, step, fade}
  const CH = [
    [0x00000000, 0x00, 0, 0],                // 0 single (bit 0 clear, +0x5c without 0x44)
    [0x00000008, 0x00, -5, 0xfff0],          // 1 fade, negative step under the cap, then single
    [0x00000008, 0x10, -0x100, 0xfff0],      // 2 fade, negative step over the cap 0x10000
    [0x00000001, 0x10, 0x30, 0x100],         // 3 fade step < value; bit 0 -> no streamed bit, no voice bits: nothing
    [0x00000009, 0x40, 0x200, 0x100],        // 4 fade step >= value -> 0; streamed (+0x5c bit 6)
    [0x00000001, 0x04, 0, 0],                // 5 +0x5c 0x04: not single; no voice bits
    [0x0000f001, 0x00, 0, 0],                // 6 voices 0-3
    [0x0ffff001, 0x00, 0, 0],                // 7 all sixteen voices
    [0x10a5a001, 0x44, 0, 0],                // 8 every other voice; bit 28
    [0x10000001, 0x04, 0, 0],                // 9 bit 28 with +0x5c bit 2, no voices (the zero-fill test, bit 6 is clear here)
    [0x00001000, 0x40, 0, 0],                // 10 bit 0 clear but +0x5c has 0x40: not single, streamed
    [0x08001011, 0x00, 0, 0],                // 11 voices 0, 15 and bit 4 already set
  ];
  F.c3fe_rc = function () {
    const out = {};
    out.log = H.stubLog();
    out.buf = H.block(0x400, 0x0b000000);
    for (let i = 0; i < 12; i++) {
      const c = H.block(0x1400, 0x10000000 + 0x10000 * i);
      H.u32(c, 0x58, CH[i][0]);
      H.u8(c, 0x5c, CH[i][1]);
      H.u32(c, 0x1c4, CH[i][2]);
      H.u32(c, 0x1d8, CH[i][3]);
      for (let k = 0; k < 16; k++) {
        const v = c.add(0x238 + k * 0x110);
        H.u8(v, 0x10, (i + k) % 3 === 0 ? 1 : 0);
        H.u8(v, 0x11, (k * 7 + i) & 0x1f);
        H.u8(v, 0x9c, (k % 4 === 1) ? 0x04 : 0x00);
      }
      out['c' + i] = c;
      out['out' + i] = H.block(0x10, 0x0c000000 + 0x100 * i);
    }
    const s = __importStandIn({ module: 'sound.dll', imports: { 'WINMM.dll!timeGetTime': { args: 0, ret: 0x7171, record: false } } });
    out.implog = s.log;
    H.quietTimer(out);
    H.stubOn(out.log, 0x2649, 4, 'thiscall', function () { return 0x2649; });
    H.stubOn(out.log, 0x20c7, 3, 'thiscall', function () { return 0x20c7; });
    H.stubOn(out.log, 0x23f6, 5, 'thiscall', function (a) {
      const v = a[1], a1 = a[3].toUInt32(), p = a[5];
      p.writeU32(((v.add(0x10).readU8() & 1) ? a1 : (a1 + v.add(0x11).readU8())) >>> 0);
      return 0x23f6;
    });
    return out;
  };
})();

// ---- G. renderChannelSingle: one channel object (0x1400 bytes) per vector, each with its own object at +8 (+0x14, +0x1c); the six
//      Voice_mix* thunks stubbed (cdecl 5: a0, &local, a1, this, 1; the &local argument is logged as 0): each adds 2 * a1 to the
//      local and returns a1, a1 - 2 or a1 + 1 by the channel's dword +0x30 (0 / 1 / 2); Channel_setPanGains (thunk 0x10001a7d) and
//      postSoundThreadMessage (thunk 0x100011d6) stubbed; PostMessageA (IAT 0x100b85a0) stood in; g_soundEngine swapped for an
//      engine with bit 5 of +0x1a4 set (key _post), clear (_bit0) or NULL (_null) -------------------------------------------------
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3fe_h;
  // [w182, w18e, f58, d5c, r30, lim, p228, p230, x14, x1c, m1374, m1378, d1c8, d1d0, d1e0, d210]
  const NAN = 'nan';
  const RS = [
    [2, 0, 0x002, 0x00, 0, 0, 0, 0, 0x40, 0x100, 0xffff, 0x10, 0, 0, 0, 0],            // 0  a1 > 0x7d0 (vector arg)
    [2, 0, 0x002, 0x00, 0, 0x100, 0x10, 0x20, 0x40, 0x100, 0xffff, 0x10, 0, 0, 0, 0],    // 1  limit path, flag (zero fill)
    [2, 0, 0x002, 0x00, 0, 0x100, 0x10, 0x20, 0x40, 0x100, 0xffff, 0x10, 0, 0, 0, 0],    // 2  limit path, no flag
    [2, 0, 0x002, 0x00, 0, 0x30, 0x10, 0x20, 0x40, 0x100, 0xffff, 0x10, 0, 0, 0, 0],     // 3  SetStereo16, equal, post
    [0, 8, 0x002, 0x00, 1, 0, 0, 0, 0x40, 0x100, 0xffff, 0x10, 0, 0, 0, 0],              // 4  Set8bit, short (zero tail)
    [0, 0, 0x002, 0x00, 2, 0, 0, 0, 0x40, 0x100, 0xffff, 0x10, 0, 0, 0, 0],              // 5  SetMono16, long -> 0xa
    [2, 0, 0x00a, 0x02, 0, 0, 0, 0, 0x40, 0x100, 0xffff, 0x10, 1, 2, 3, 4],              // 6  AddStereo16, fade +0x5c bit 1
    [0, 8, 0x00a, 0x08, 0, 0, 0, 0, 0x40, 0x100, 0xffff, 0x10, 0.5, 0, 5.0, 1.0],        // 7  Add8bit, fade bit 3, d >= +0x210
    [0, 0, 0x00a, 0x08, 0, 0, 0, 0, 0x40, 0x100, 0xffff, 0x10, 0.5, 0, 1.0, 4.0],        // 8  AddMono16, fade bit 3, d < +0x210
    [2, 0, 0x002, 0x10, 0, 0, 0, 0, 0x40, 0x100, 0xffff, 0x10, -0.25, 2.0, 3.0, 0],      // 9  bit 4, +0x1c8 <= 0, d >= +0x1d0
    [2, 0, 0x002, 0x10, 0, 0, 0, 0, 0x40, 0x100, 0xffff, 0x10, 0.25, 2.0, 1.0, 0],       // 10 bit 4, +0x1c8 > 0, d <= +0x1d0
    [2, 0, 0x002, 0x10, 0, 0, 0, 0, 0x40, 0x100, 0xffff, 0x10, 0.25, 2.0, 7.0, 0],       // 11 bit 4, +0x1c8 > 0, d > +0x1d0
    [0, 0, 0x00a, 0x00, 0, 0, 0, 0, 0x40, 0x100, 0xffff, 0x10, 0.75, 0, 0.75, 0],        // 12 neither: d == 0, d <= +0x1c8
    [0, 0, 0x00a, 0x00, 0, 0, 0, 0, 0x40, 0x100, 0xffff, 0x10, 0.1, 0, 0.7, 0],          // 13 neither: d > +0x1c8
    [2, 0, 0x023, 0x00, 0, 0, 0, 0, 0x40, 0x100, 0xffff, 0x10, 0, 0, 0, 0],              // 14 equal, bits 0 / 5 set
    [2, 0, 0x002, 0x00, 0, 0, 0, 0, 0x400, 0x100, 0xffff, 0x10, 0, 0, 0, 0],             // 15 equal, distance not above
    [2, 0, 0x402, 0x00, 0, 0, 0, 0, 0x40, 0x100, 0xffff, 0x10, 0, 0, 0, 0],              // 16 equal, bit 10 already set
    [2, 0, 0x000, 0x00, 0, 0, 0, 0, 0x40, 0x100, 0xffff, 0x10, 0, 0, 0, 0],              // 17 equal, bit 1 clear
    [0, 0, 0x00a, 0x00, 0, 0, 0, 0, 0x40, 0x100, 0xffff, 0x10, 0.5, 0, NAN, 0],          // 18 neither: d a NaN
    [0, 0, 0x00a, 0x10, 0, 0, 0, 0, 0x40, 0x100, 0xffff, 0x10, NAN, 1.0, 2.0, 0],        // 19 bit 4, +0x1c8 a NaN
  ];
  function setD(p, off, v) { if (v === NAN) H.bits64(p, off, 0, 0x7ff80000); else H.f64(p, off, v); }
  F.c3fe_rs = function (mode) {
    const out = {};
    out.log = H.stubLog();
    out.buf = H.block(0x800, 0x0d000000);
    for (let i = 0; i < RS.length; i++) {
      const t = RS[i];
      const c = H.block(0x1400, 0x18000000 + 0x10000 * i);
      const x = H.block(0x20, 0x19000000 + 0x100 * i);
      c.add(8).writePointer(x);
      H.u32(x, 0x14, t[8]); H.u32(x, 0x1c, t[9]);
      c.add(0x182).writeU16(t[0]); c.add(0x18e).writeU16(t[1]);
      H.u32(c, 0x58, t[2]); H.u32(c, 0x5c, t[3]); H.u32(c, 0x30, t[4]);
      H.u32(c, 0x22c, t[5]); H.u32(c, 0x228, t[6]); H.u32(c, 0x230, t[7]);
      H.u32(c, 0x1374, t[10]); H.u32(c, 0x1378, t[11]); H.u32(c, 0x1370, 3);
      setD(c, 0x1c8, t[12]); setD(c, 0x1d0, t[13]); setD(c, 0x1e0, t[14]); setD(c, 0x210, t[15]);
      H.u32(c, 0x224, 0x2240 + i); H.u32(c, 0x17c, 0x17c0 + i);
      out['c' + i] = c; out['x' + i] = x; out['out' + i] = H.block(0x10, 0x0e000000 + 0x100 * i);
    }
    if (mode !== 'null') {
      out.eng = H.block(0x200, 0x7d000000);
      H.u32(out.eng, 0x1a4, mode === 'post' ? 0x20 : 0xffffffdf);
    }
    const s = __importStandIn({ module: 'sound.dll', imports: { 'USER32.dll!PostMessageA': { ret: 1 } } });
    out.implog = s.log;
    H.quietTimer(out);
    H.swapGlobal(0xb4a20, out.eng ? out.eng.toUInt32() : 0, out, 'geng');
    const mix = function (a) {
      const p = a[1], a1 = a[2].toUInt32(), sel = a[3].add(0x30).readU32();
      p.writeU32((p.readU32() + 2 * a1) >>> 0);
      return sel === 1 ? a1 - 2 : (sel === 2 ? a1 + 1 : a1);
    };
    [0x10dc, 0x2220, 0x1f64, 0x12ee, 0x267b, 0x2617].forEach(function (rva) { H.stubOn(out.log, rva, 5, 'cdecl', mix, [1]); });
    H.stubOn(out.log, 0x1a7d, 1, 'thiscall', function () { return 0x1a7d; });
    H.stubOn(out.log, 0x11d6, 3, 'thiscall', function () { return 0x11d6; });
    return out;
  };
  ['post', 'bit0', 'null'].forEach(function (m) { F['c3fe_rs_' + m] = function () { return F.c3fe_rs(m); }; });
})();
