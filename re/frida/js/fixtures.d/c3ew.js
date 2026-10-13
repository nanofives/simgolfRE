// Fixtures for C3 batch c3ew (sound.dll, round 32). Layouts come from the disassembly cited in shim/src/re/c3ew.cpp.
// Every object, vtable and list node here is allocated by the fixture. Helpers copied from c3dt.js (c3dt's file only loads
// when c3dt is a listed batch): the freeze guard, the timer word preset, swapped module globals and thread-filtered
// recording stubs on incremental-link thunks. All values are synthetic.
(function () {
  const F = globalThis.DIFF_FIXTURES;
  const keep = function (x) { globalThis.__diffKeepAlive.push(x); return x; };
  const snd = function (rva) { return Process.getModuleByName('sound.dll').base.add(rva); };
  function fill(p, n, b) { p.writeByteArray(new Array(n).fill(b)); return p; }
  function marker(p, n, tag) { for (let k = 0; k < n; k += 4) p.add(k).writeU32((tag + k) >>> 0); }
  function u32(p, off, v) { p.add(off).writeU32(v >>> 0); }
  function block(n, tag) { const b = keep(Memory.alloc(n)); marker(b, n, tag); return b; }

  let frozenNow = false;
  function guard() {
    if (frozenNow) return;
    __freeze.thread();
    frozenNow = true;
    __freeze.atFlush(function () { frozenNow = false; });
    __freeze.negOn();
  }
  // timer word 0x100b49a8 = 5: SoundTimer::onTick leaves at 0x10008edf before touching anything
  function quietTimer(out) {
    guard();
    const g = snd(0xb49a8);
    out.g49a8 = g;
    __freeze.setU32(g, 5);
    const sleep = new NativeFunction(Process.getModuleByName('kernel32.dll').getExportByName('Sleep'), 'void', ['uint32'], 'stdcall');
    for (let i = 0; i < 20; i++) { sleep(2); if (g.readU32() === 5) break; g.writeU32(5); }
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
  function stubOn(log, rva, nargs, thiscall, retfn, skipLog) {
    guard();
    const base = snd(rva), key = base.toString();
    if (stubLive[key]) { try { Interceptor.revert(base); } catch (e) { /* gone */ } delete stubLive[key]; Interceptor.flush(); }
    const tid = Process.getCurrentThreadId();
    const argt = (thiscall === true ? ['pointer'] : []).concat(Array(nargs).fill('pointer'));
    const abi = thiscall === true ? 'thiscall' : (thiscall === 'cdecl' ? 'default' : 'stdcall');
    const orig = keep(new NativeFunction(base, 'uint32', argt, abi));
    const cb = keep(new NativeCallback(function () {
      const a = Array.prototype.slice.call(arguments);
      if (Process.getCurrentThreadId() !== tid) return orig.apply(null, a);
      const n = log.readU32();
      // skipLog: the call passes a frame-local pointer (e.g. &out) that differs between the original and reimpl arms, so
      // recording it would read RED on the log alone; the observable effect is compared through memory instead.
      if (!skipLog) logCall(log, rva, a);
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

  F.c3ew_h = { keep: keep, snd: snd, fill: fill, marker: marker, u32: u32, block: block, guard: guard,
               quietTimer: quietTimer, swapGlobal: swapGlobal, stubLog: stubLog, stubOn: stubOn, logCall: logCall,
               recVtable: recVtable };
})();
// ---- A. constructors: twelve marker-filled 0x240-byte blocks --------------------------------------------------------
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3ew_h;
  F.c3ew_objs = function () {
    const out = {};
    // these two constructors need no frozen thread or global swap, but the negctl build's wrong body is dormant until
    // the switch is set, so arm it here (harmless in the normal build: no NegCtl export exists)
    __freeze.negOn();
    for (let i = 0; i < 12; i++) out['o' + i] = H.block(0x240, 0x3e000000 + 0x10000 * i);
    return out;
  };
})();
// ---- B. wave-in device wrappers -------------------------------------------------------------------------------------
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3ew_h;
  // closeCapture: objects with +0x30 a timer word (zero or a fixture block); g_soundTimer and g_waveIn swapped
  F.c3ew_closecap = function (withDev) {
    const out = {};
    H.quietTimer(out);
    out.log = H.stubLog();
    for (let i = 0; i < 12; i++) {
      const o = H.block(0x80, 0x2c000000 + 0x1000 * i);
      if (i % 3 === 0) H.u32(o, 0x30, 0);
      out['o' + i] = o;
    }
    out.tmr = H.block(0x80, 0x7a000000);
    H.swapGlobal(0xb49ec, out.tmr.toUInt32(), out, 'gtmr');
    if (withDev) {
      out.dev = H.block(0x40, 0x6e000000);
      out.dev.writePointer(H.recVtable(out.log, { 0: [1, function (a) { return 0x4321; }] }));
      H.swapGlobal(0xb4a04, out.dev.toUInt32(), out, 'gwi');
    } else H.swapGlobal(0xb4a04, 0, out, 'gwi');
    H.stubOn(out.log, 0x26fd, 0, true, function (a) { return 0x15; });
    H.stubOn(out.log, 0x1ed3, 1, true, function (a) { return 0x16; });
    return out;
  };
  F.c3ew_closecap_null = function () { return F.c3ew_closecap(false); };
  F.c3ew_closecap_dev = function () { return F.c3ew_closecap(true); };

  // Wave_In_Device::start: g_waveIn swapped; the four step thunks return codes[k] (0 = success)
  F.c3ew_wistart = function (codes) {
    const out = {};
    H.quietTimer(out);
    out.log = H.stubLog();
    if (codes === null) { H.swapGlobal(0xb4a04, 0, out, 'gwi'); return out; }
    out.dev = H.block(0x80, 0x6f000000);
    H.swapGlobal(0xb4a04, out.dev.toUInt32(), out, 'gwi');
    [0x213a, 0x219e, 0x24b4, 0x2121].forEach(function (rva, k) {
      H.stubOn(out.log, rva, 0, true, function (a) { return codes[k]; });
    });
    return out;
  };
  F.c3ew_wistart_null = function () { return F.c3ew_wistart(null); };
  F.c3ew_wistart_open = function () { return F.c3ew_wistart([0xc, 0, 0, 0]); };
  F.c3ew_wistart_prep = function () { return F.c3ew_wistart([0, 0x13, 0, 0]); };
  F.c3ew_wistart_add = function () { return F.c3ew_wistart([0, 0, 0x11, 0]); };
  F.c3ew_wistart_ok = function () { return F.c3ew_wistart([0, 0, 0, 0]); };
  F.c3ew_wistart_last = function () { return F.c3ew_wistart([0, 0, 0, 0x2b]); };

  // openCapture: objects with +0x28 zero (most) or set; heap stand-in over operator new 0x100424d7 (failMask 1 = every
  // vector's allocation fails); the constructor stub (thunk 0x10001fa5) writes a recording vtable at +0 and returns this;
  // the buffer allocator stub (thunk 0x10002162) returns `code`.
  F.c3ew_opencap = function (mode, code) {
    const out = {};
    H.quietTimer(out);
    out.log = H.stubLog();
    for (let i = 0; i < 12; i++) {
      const o = H.block(0x80, 0x2d000000 + 0x1000 * i);
      H.u32(o, 0x28, (i === 3 || i === 8) ? 1 + i : 0);
      out['o' + i] = o;
    }
    if (mode === 'busy') { out.dev = H.block(0x40, 0x6d000000); H.swapGlobal(0xb4a04, out.dev.toUInt32(), out, 'gwi'); }
    else H.swapGlobal(0xb4a04, 0, out, 'gwi');
    const vt = H.recVtable(out.log, { 0: [1, function (a) { return 0x77; }] });
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], free: [0x4249a], arenaSize: 0x200, fill: 0x5a,
                              failMask: mode === 'oom' ? 1 : 0 });
    out.arena = h.arena; out.freelog = h.log;
    H.stubOn(out.log, 0x1fa5, 0, true, function (a) { a[0].writePointer(vt); return a[0]; });
    H.stubOn(out.log, 0x2162, 2, true, function (a) { return code; });
    return out;
  };
  F.c3ew_opencap_busy = function () { return F.c3ew_opencap('busy', 0); };
  F.c3ew_opencap_ok = function () { return F.c3ew_opencap('ok', 0); };
  F.c3ew_opencap_err = function () { return F.c3ew_opencap('ok', 0x11); };
  F.c3ew_opencap_oom = function () { return F.c3ew_opencap('oom', 0x13); };
  F.c3ew_opencap_oom0 = function () { return F.c3ew_opencap('oom', 0); };

  // WaveInStream_resetCodec: malloc 0x10042acd under the heap stand-in (codecAllocContextA's 0x2544-byte context); the codec
  // words 0x100b58a0..a8 saved; the stream word 0x100b4a24 swapped; thunk 0x100015b4 stubbed
  F.c3ew_rcodec = function (failMask) {
    const out = {};
    H.quietTimer(out);
    out.log = H.stubLog();
    for (let i = 0; i < 12; i++) out['o' + i] = H.block(0xa0, 0x2e000000 + 0x1000 * i);
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x42acd], arenaSize: 0x2600, fill: 0xcd, failMask: failMask });
    out.arena = h.arena;
    out.gcodec = H.snd(0xb58a0);
    __freeze.save(out.gcodec, 12);
    out.strm = H.block(0x80, 0x6c000000);
    H.swapGlobal(0xb4a24, out.strm.toUInt32(), out, 'gstrm');
    H.stubOn(out.log, 0x15b4, 1, true, function (a) { return 0; });
    return out;
  };
  F.c3ew_rcodec_ok = function () { return F.c3ew_rcodec(0); };
  F.c3ew_rcodec_oom = function () { return F.c3ew_rcodec(1); };
})();
// ---- B2. freeMmioHandleListGetInfo: list blocks with values whose first dword is an mmio handle --------------------------
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3ew_h;
  // Rows: list of value kinds per node: a number = a value block whose handle is that number, null = a NULL value
  const ROWS = [[], [null], [0x101], [0x102, 0x103, 0x104], [0x105, null, 0x106], [null, 0x107], [0x108, 0x109],
                [0], [0x10a, 0x10b, null], [0xffffffff], [0x10c, 0x10d, 0x10e, 0x10f], [0x110, null]];
  F.c3ew_freemmio = function (withStream) {
    const out = {};
    H.quietTimer(out);
    out.log = H.stubLog();
    ROWS.forEach(function (r, i) {
      const b = H.block(0x400, 0x2f000000 + 0x1000 * i);
      const node = (j) => b.add(0x100 + 0x10 * j), val = (j) => b.add(0x200 + 0x20 * j);
      r.forEach(function (v, j) {
        node(j).writePointer(j > 0 ? node(j - 1) : ptr(0));
        node(j).add(4).writePointer(j < r.length - 1 ? node(j + 1) : ptr(0));
        if (v === null) node(j).add(8).writePointer(ptr(0));
        else { node(j).add(8).writePointer(val(j)); H.u32(val(j), 0, v); }
      });
      b.add(0x5c).writePointer(r.length ? node(0) : ptr(0));
      b.add(0x60).writePointer(r.length ? node(r.length - 1) : ptr(0));
      H.u32(b, 0x68, r.length + 0x40);
      H.u32(b, 0x7c, [0xffffffff, 4, 0, 0x55, 5, 0xfb, 0x104, 7, 0x80000004, 0x14, 0xc, 3][i]);
      out['b' + i] = b;
    });
    out.cnt = H.snd(0xb4a08);
    __freeze.setU32(out.cnt, 30);
    if (withStream) { out.strm = H.block(0x80, 0x6b000000); H.swapGlobal(0xb4a24, out.strm.toUInt32(), out, 'gstrm'); }
    else H.swapGlobal(0xb4a24, 0, out, 'gstrm');
    out.freelog = __heapStandIn({ module: 'sound.dll', free: [0x4249a] }).log;
    out.implog = __importStandIn({ module: 'sound.dll', imports: {
      'WINMM.dll!mmioGetInfo': { args: 3, record: [0, 2], ret: 0 },
      'WINMM.dll!mmioClose': { args: 2, impl: function (a) { return (a[0].toUInt32() & 0xff) >>> 0; } } } }).log;
    H.stubOn(out.log, 0x1ba4, 0, true, function (a) { return 0; });
    return out;
  };
  F.c3ew_freemmio_null = function () { return F.c3ew_freemmio(false); };
  F.c3ew_freemmio_strm = function () { return F.c3ew_freemmio(true); };
})();
// ---- C. Wave_Device bodies: one configurable fixture ---------------------------------------------------------------
// o.g: module globals to swap {rva: 'blk' (a fresh 0x200-byte block) | number}; o.dev: options for the g_waveDevice block
// (flag byte +0x6c, flag byte +0x10, count +0x148); o.stubs: {thunkRva: [nargs, retfn]}; o.free: record operator delete.
// The twelve `this` objects (0x200 bytes) carry a recording vtable (slots +0x3c / +0x40 / +0x4c), flag bytes +0x1a4 from
// FLAGS, the timer word +0x1a8 from TMR and a node list at +0x1c8 (front) / +0x1cc (back) / +0x1d4 (count) from LISTS.
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3ew_h;
  const FLAGS = [0, 2, 0x20, 0x22, 0xff, 0xfd, 0xdf, 0x10, 0x01, 0x24, 0x80, 0x02];
  const TMR = [0, 0x1111, 0, 0x2222, 0x3333, 0, 0x4444, 0, 0x5555, 0x6666, 0, 0x7777];
  const LISTS = [[], [1], [1, 2, 3], [0], [1, 0, 2], [0, 1], [4, 5], [], [6, 7, 8, 0], [9], [0, 0], [3, 2, 1]];
  F.c3ew_wd = function (o) {
    const out = {};
    H.quietTimer(out);
    out.log = H.stubLog();
    const vt = H.recVtable(out.log, { 0x3c: [1, function (a) { return 0; }],
                                      0x40: [1, function (a) { return (a[0].add(0x10).readU32() & 0xfff) + a[1].toUInt32(); }],
                                      0x4c: [0, function (a) { return 0; }] });
    for (let i = 0; i < 12; i++) {
      const b = H.block(0x200, 0x2a000000 + 0x1000 * i);
      b.writePointer(vt);
      b.add(0x1a4).writeU8(FLAGS[i]);
      H.u32(b, 0x1a8, TMR[i]);
      const L = LISTS[i];
      const node = (j) => b.add(0x100 + 0x10 * j);
      L.forEach(function (v, j) {
        node(j).writePointer(j > 0 ? node(j - 1) : ptr(0));
        node(j).add(4).writePointer(j < L.length - 1 ? node(j + 1) : ptr(0));
        node(j).add(8).writePointer(v ? b.add(0x180 + 4 * v) : ptr(0));
      });
      b.add(0x1c8).writePointer(L.length ? node(0) : ptr(0));
      b.add(0x1cc).writePointer(L.length ? node(L.length - 1) : ptr(0));
      H.u32(b, 0x1d4, L.length + 0x30);
      out['w' + i] = b;
    }
    Object.keys(o.g || {}).forEach(function (k) {
      const rva = parseInt(k, 10), v = o.g[k];
      let val = v;
      if (v === 'blk') { const b = H.block(0x200, 0x61000000 + rva); out['blk' + rva.toString(16)] = b; val = b.toUInt32(); }
      if (v === 'dev') {
        const d = H.block(0x200, 0x62000000);
        d.add(0x6c).writeU8(o.dev.f6c); d.add(0x10).writeU8(o.dev.f10 || 0); H.u32(d, 0x148, o.dev.count || 0);
        out.dev = d; val = d.toUInt32();
      }
      H.swapGlobal(rva, val, out, 'g' + rva.toString(16));
    });
    if (o.free) out.freelog = __heapStandIn({ module: 'sound.dll', free: [0x4249a] }).log;
    Object.keys(o.stubs || {}).forEach(function (k) {
      const s = o.stubs[k];
      H.stubOn(out.log, parseInt(k, 10), s[0], true, s[1] ? function (a, n) { return s[1](a, n, out); } : null);
    });
    return out;
  };
})();
// ---- C2. the per-function key fixtures over c3ew_wd -----------------------------------------------------------------
(function () {
  const F = globalThis.DIFF_FIXTURES;
  const T = 0xb49ec, DEV = 0xb49f0, STRM = 0xb49f4, B0 = 0xb49f8, B1 = 0xb49fc, B2 = 0xb4a00, MIDI = 0xb4a1c, COOP = 0xb4a3c;
  // the timer stubs: registerClient copies the client's dword +0x10 to the timer's +0x20, SoundTimer::start returns it
  const reg = [1, function (a) { a[0].add(0x20).writeU32(a[1].add(0x10).readU32()); return 0x31; }];
  const start = [0, function (a) { return a[0].add(0x20).readU32(); }];
  const z0 = [0, null], z1 = [1, null];
  const g = function (m) { const r = {}; Object.keys(m).forEach(function (k) { r[k] = m[k]; }); return r; };
  // startOutput
  const so = function (gl, f6c) {
    return F.c3ew_wd({ g: gl, dev: { f6c: f6c }, stubs: { 0x1339: z0, 0x1203: z1, 0x227a: reg, 0x1dfc: start } });
  };
  F.c3ew_so_all = function () { return so(g({ [T]: 'blk', [DEV]: 'dev', [B0]: 'blk', [B1]: 'blk', [B2]: 'blk' }), 0x10); };
  F.c3ew_so_none = function () { return so(g({ [T]: 0, [DEV]: 'dev', [B0]: 0, [B1]: 0, [B2]: 0 }), 0); };
  F.c3ew_so_mix = function () { return so(g({ [T]: 'blk', [DEV]: 'dev', [B0]: 0, [B1]: 0, [B2]: 'blk' }), 0xff); };
  F.c3ew_so_mix2 = function () { return so(g({ [T]: 0, [DEV]: 'dev', [B0]: 'blk', [B1]: 'blk', [B2]: 0 }), 0xef); };
  // stopOutput
  const sto = function (gl, f6c) {
    return F.c3ew_wd({ g: gl, dev: { f6c: f6c }, stubs: { 0x26fd: z0, 0x1ed3: z1, 0x1a2d: z0 } });
  };
  F.c3ew_sto_all = function () { return sto(g({ [T]: 'blk', [DEV]: 'dev', [STRM]: 'blk', [B0]: 'blk', [B1]: 'blk', [B2]: 'blk' }), 0x10); };
  F.c3ew_sto_none = function () { return sto(g({ [T]: 'blk', [DEV]: 'dev', [STRM]: 0, [B0]: 0, [B1]: 0, [B2]: 0 }), 0); };
  F.c3ew_sto_mix = function () { return sto(g({ [T]: 'blk', [DEV]: 'dev', [STRM]: 'blk', [B0]: 0, [B1]: 'blk', [B2]: 0 }), 0xff); };
  F.c3ew_sto_mix2 = function () { return sto(g({ [T]: 'blk', [DEV]: 'dev', [STRM]: 0, [B0]: 'blk', [B1]: 'blk', [B2]: 'blk' }), 0xef); };
  // closeOutput
  const co = function (gl, f6c) {
    const o = F.c3ew_wd({ g: gl, dev: { f6c: f6c || 0 }, stubs: { 0x26fd: z0, 0x1f0f: z0, 0x1677: z1, 0x20f9: z0 } });
    if (o.blkb4a1c) o.blkb4a1c.writePointer(F.c3ew_h.recVtable(o.log, { 0x5c: [0, null] }));
    return o;
  };
  F.c3ew_co_all = function () { return co(g({ [T]: 'blk', [MIDI]: 'blk', [DEV]: 'dev', [STRM]: 'blk', [B0]: 'blk', [B1]: 'blk', [B2]: 'blk' })); };
  F.c3ew_co_none = function () { return co(g({ [T]: 'blk', [MIDI]: 0, [DEV]: 0, [STRM]: 0, [B0]: 'blk', [B1]: 0, [B2]: 'blk' })); };
  F.c3ew_co_part = function () { return co(g({ [T]: 'blk', [MIDI]: 0, [DEV]: 'dev', [STRM]: 'blk', [B0]: 0, [B1]: 'blk', [B2]: 0 })); };
  // ctor: the three globals it clears start non-zero
  F.c3ew_ctor = function () {
    return F.c3ew_wd({ g: g({ [DEV]: 0x11110000, [B0]: 0x22220000, [STRM]: 0x33330000 }), stubs: { 0x21df: z0, 0x10aa: z0 } });
  };
  // dtor: the VoiceBase_dtor stub returns the object's dword +0x1d4 (the list count after the pops)
  const dtorStubs = { 0x1a9b: z0, 0x19a1: z0, 0x1dac: [0, function (a) { return a[0].add(0x1d4).readU32(); }] };
  F.c3ew_dtor_dev = function () { return F.c3ew_wd({ g: g({ [DEV]: 0x44440000 }), free: true, stubs: dtorStubs }); };
  F.c3ew_dtor_null = function () { return F.c3ew_wd({ g: g({ [DEV]: 0 }), free: true, stubs: dtorStubs }); };
  // stop
  const stopStubs = { 0x26fd: z0, 0x1ed3: z1, 0x2757: z0, 0x188e: z0, 0x1677: z1, 0x1b81: z0, 0x1a32: z0, 0x2360: z0 };
  const stp = function (gl, f6c) {
    const o = F.c3ew_wd({ g: gl, dev: { f6c: f6c || 0 }, free: true, stubs: stopStubs });
    o.gmb = F.c3ew_h.snd(0xb49bc);
    __freeze.save(o.gmb, 4);
    o.gmb.writeU8(0xa6);
    return o;
  };
  F.c3ew_stop_all = function () { return stp(g({ [T]: 'blk', [DEV]: 'dev', [STRM]: 'blk', [B0]: 'blk', [B1]: 'blk', [B2]: 'blk' }), 8); };
  F.c3ew_stop_none = function () { return stp(g({ [T]: 'blk', [DEV]: 0, [STRM]: 0, [B0]: 'blk', [B1]: 0, [B2]: 0 })); };
  F.c3ew_stop_dev0 = function () { return stp(g({ [T]: 'blk', [DEV]: 'dev', [STRM]: 'blk', [B0]: 0, [B1]: 'blk', [B2]: 'blk' }), 0xf7); };
  // restart: the device block's count +0x148 = 6; stub results per key
  const rs = function (gl, f10, seek, create, cbuf, word) {
    const out = F.c3ew_wd({ g: gl, dev: { f6c: 0, f10: f10, count: 6 }, stubs: {
      0x1ed3: z1, 0x26fd: z0, 0x1677: z1, 0x245f: z0, 0x104b: z0,
      0x192e: [1, function (a) { return seek; }], 0x180c: [2, function (a) { return create; }],
      0x1046: [3, function (a, n, o) { if (!cbuf) a[1].writePointer(o.buf); return cbuf; }],
      0x182f: z1, 0x1c5d: z0, 0x1203: z1, 0x1311: z1, 0x227a: reg, 0x1dfc: start } });
    out.buf = F.c3ew_h.block(0x100, 0x63000000);
    out.buf.add(0xb4).writeU16(word);
    out.buf.add(0x38).writeU32(0x12345);
    return out;
  };
  const RG = function () { return g({ [T]: 'blk', [DEV]: 'dev', [STRM]: 'blk', [B0]: 'blk', [COOP]: 0x5c00 }); };
  F.c3ew_rs_null = function () { return rs(g({ [T]: 'blk', [DEV]: 0 }), 0, 0, 0, 0, 0); };
  F.c3ew_rs_seek = function () { return rs(RG(), 0, 1, 0, 0, 0); };
  F.c3ew_rs_create = function () { return rs(RG(), 0, 0, 0x88780032, 0, 0); };
  F.c3ew_rs_cbuf = function () { return rs(RG(), 0x20, 0, 0, 0x29, 0); };
  F.c3ew_rs_ok = function () { return rs(RG(), 0x20, 0, 0, 0, 7); };
  F.c3ew_rs_ok0 = function () { return rs(g({ [T]: 'blk', [DEV]: 'dev', [STRM]: 'blk', [B0]: 0, [COOP]: 0x5c01 }), 0, 0, 0, 0, 0); };
})();
// ---- D. the sound timer, a timed channel start; E. enqueueWaveCmd10 ---------------------------------------------------
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3ew_h;
  // The module critical section 0x100b4990 is only entered by SoundTimer methods (log/c3/alt/c3ew/refs.py); with the timer word
  // at 5 SoundTimer::onTick leaves at 0x10008edf before its EnterCriticalSection (0x10008efa), and the main thread is frozen.
  // The section must not be held when the key starts: LockCount (+4) -1 or 0 and RecursionCount (+8) 0, checked here.
  function timerCS(out) {
    H.quietTimer(out);
    const cs = H.snd(0xb4990);
    const sleep = new NativeFunction(Process.getModuleByName('kernel32.dll').getExportByName('Sleep'), 'void', ['uint32'], 'stdcall');
    let ok = false;
    for (let i = 0; i < 50 && !ok; i++) {
      const lc = cs.add(4).readS32(), rc = cs.add(8).readS32();
      ok = (lc === -1 || lc === 0) && rc === 0;
      if (!ok) sleep(2);
    }
    if (!ok) throw new Error('c3ew: the timer critical section 0x100b4990 is held');
    out.cs = cs;
    __freeze.save(cs, 24);
  }
  const TFLAGS = [0, 1, 2, 3, 7, 0xffffffff, 0xb, 0x13, 0x80000003, 4, 0xfffffffc, 3];
  const TLISTS = [[], [1], [1, 2], [0], [1, 0, 2], [3, 4, 5], [0, 1], [], [2], [1, 2, 3, 0], [6], [7, 8]];
  F.c3ew_timer = function () {
    const out = {};
    timerCS(out);
    for (let i = 0; i < 12; i++) {
      const b = H.block(0x100, 0x27000000 + 0x1000 * i);
      H.u32(b, 0x3c, TFLAGS[i]);
      H.u32(b, 0x40, 0x4000 + i);
      H.u32(b, 0x18, 1 + i);
      const L = TLISTS[i], node = (j) => b.add(0x80 + 0x10 * j);
      L.forEach(function (v, j) {
        node(j).writePointer(j > 0 ? node(j - 1) : ptr(0));
        node(j).add(4).writePointer(j < L.length - 1 ? node(j + 1) : ptr(0));
        node(j).add(8).writePointer(v ? b.add(0xe0 + 4 * v) : ptr(0));
      });
      b.add(0x4c).writePointer(L.length ? node(0) : ptr(0));
      b.add(0x50).writePointer(L.length ? node(L.length - 1) : ptr(0));
      H.u32(b, 0x58, L.length + 0x20);
      out['t' + i] = b;
    }
    out.freelog = __heapStandIn({ module: 'sound.dll', free: [0x4249a] }).log;
    out.implog = __importStandIn({ module: 'sound.dll', imports: {
      'WINMM.dll!timeGetDevCaps': { args: 2, impl: function (a) { a[0].writeU32((a[0].add(4).readU32() ^ 0x10) >>> 0); return 0; } },
      'KERNEL32.dll!InitializeCriticalSection': { args: 1, ret: 0 }, 'KERNEL32.dll!DeleteCriticalSection': { args: 1, ret: 0 },
      'KERNEL32.dll!EnterCriticalSection': { args: 1, ret: 0 }, 'KERNEL32.dll!LeaveCriticalSection': { args: 1, ret: 0 },
      'WINMM.dll!timeKillEvent': { args: 1, impl: function (a) { return a[0].toUInt32() & 0xf; } },
      'WINMM.dll!timeEndPeriod': { args: 1, impl: function (a) { return a[0].toUInt32() & 0x7; } } } }).log;
    return out;
  };

  // Channel_startVoiceTimed: objects with flags +0xc8, the byte +0x58, the dword +0x50 and a vtable (slot +0x10 returns its argument
  // & 1, slot +0x7c records); thunks 0x10001866 / 0x10001dfc / 0x10001212 stubbed
  const CF = [0, 1, 3, 1, 1, 0x41, 0x43, 0xffffffff, 0xfffffffe, 0x81, 0x1, 0x40000043];
  const C58 = [0, 0, 0, 2, 2, 2, 0, 0xff, 0xff, 2, 0xfd, 2];
  const C50 = [0, 0, 0, 0, 1, 0, 0, 1, 1, 3, 1, 2];
  F.c3ew_chtimed = function (withTimer) {
    const out = {};
    H.quietTimer(out);
    out.log = H.stubLog();
    const vt = H.recVtable(out.log, { 0x10: [1, function (a) { return a[1].toUInt32() & 1; }], 0x7c: [0, null] });
    for (let i = 0; i < 12; i++) {
      const b = H.block(0x200, 0x26000000 + 0x1000 * i);
      b.writePointer(vt);
      H.u32(b, 0xc8, CF[i]);
      b.add(0x58).writeU8(C58[i]);
      H.u32(b, 0x50, C50[i]);
      out['c' + i] = b;
    }
    if (withTimer) { out.tmr = H.block(0x80, 0x7b000000); H.swapGlobal(0xb49ec, out.tmr.toUInt32(), out, 'gtmr'); }
    else H.swapGlobal(0xb49ec, 0, out, 'gtmr');
    H.stubOn(out.log, 0x1866, 0, true, null);
    H.stubOn(out.log, 0x1dfc, 0, true, function (a) { return 9; });
    H.stubOn(out.log, 0x1212, 1, true, function (a) { return a[1].toUInt32() ^ 0x55; });
    return out;
  };
  F.c3ew_chtimed_tmr = function () { return F.c3ew_chtimed(true); };
  F.c3ew_chtimed_null = function () { return F.c3ew_chtimed(false); };

  // enqueueWaveCmd10: g_soundEngine swapped; objects with +0xf8 NULL or a 0x1500-byte block; operator new under the heap stand-in
  F.c3ew_enq10 = function (eng, failMask) {
    const out = {};
    H.quietTimer(out);
    out.log = H.stubLog();
    for (let i = 0; i < 12; i++) {
      const b = H.block(0x200, 0x25000000 + 0x1000 * i);
      if (i % 2 === 1) { const q = H.block(0x1500, 0x24000000 + 0x10000 * i); out['q' + i] = q; b.add(0xf8).writePointer(q); }
      else H.u32(b, 0xf8, 0);
      out['e' + i] = b;
    }
    if (eng) { out.eng = H.block(0x80, 0x7c000000); H.swapGlobal(0xb4a20, out.eng.toUInt32(), out, 'geng'); }
    else H.swapGlobal(0xb4a20, 0, out, 'geng');
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], arenaSize: 0x100, fill: 0x5b, failMask: failMask });
    out.arena = h.arena;
    H.stubOn(out.log, 0x1299, 2, true, function (a) { a[0].writeU32(a[1].toUInt32()); return a[0]; });
    H.stubOn(out.log, 0x11c7, 1, true, function (a) { return 0; });
    return out;
  };
  F.c3ew_enq10_eng = function () { return F.c3ew_enq10(true, 0); };
  F.c3ew_enq10_oom = function () { return F.c3ew_enq10(true, 1); };
  F.c3ew_enq10_null = function () { return F.c3ew_enq10(false, 0); };
})();
// ---- F. sequencer command handlers ----------------------------------------------------------------------------------
// Sequencer object 0x800 bytes: +0x224 the enable flag, +0x738 (cmdHandler8) a compared id, +0x218 list front, +0x220 cursor.
// Nodes at +0x300 + 0x10 j are {+4 next, +8 value}; values at +0x400 + 0x40 j are {+4 id, +8, +0xc, +0x1c, +0x34}.
// Vectors pass a track id argument. Each row: {en: +0x224, f738, ids: [per-node id | null for a NULL value], find: the arg}
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3ew_h;
  function seqFixture(which) {
    const out = {};
    H.quietTimer(out);
    out.log = H.stubLog();
    const vt = which === 8 ? H.recVtable(out.log, { 0x84: [1, function (a) { return 0x8400; }] }) : null;
    // rows chosen to hit: flag off, id==f738 (8 only), miss, match first / middle / last, NULL value mid-list, value+0x1c zero/nonzero
    const ROWS = [
      { en: 0, ids: [1], find: 1 },
      { en: 1, f738: 0x55, ids: [1, 2], find: 0x55 },
      { en: 1, ids: [1, 2, 3], find: 9 },
      { en: 1, ids: [7, 2, 3], find: 7, c: 0 },
      { en: 1, ids: [1, 8, 3], find: 8, c: 0x11 },
      { en: 1, ids: [1, 2, 6], find: 6, c: 0x22 },
      { en: 1, ids: [1, null, 3], find: 3 },
      { en: 1, ids: [null], find: 5 },
      { en: 1, ids: [0xa, 0xb], find: 0xa, c: 0 },
      { en: 1, ids: [0xc, 0xd, 0xe, 0xf], find: 0xf, c: 0x33 },
      { en: 1, ids: [2, 2], find: 2, c: 0x44 },
      { en: 1, ids: [], find: 1 },
    ];
    out._find = [];
    ROWS.forEach(function (r, i) {
      const b = H.block(0x800, 0x23000000 + 0x1000 * i);
      H.u32(b, 0x224, r.en);
      H.u32(b, 0x738, r.f738 === undefined ? 0x90000000 + i : r.f738);
      const node = (j) => b.add(0x300 + 0x10 * j), val = (j) => b.add(0x400 + 0x40 * j);
      r.ids.forEach(function (id, j) {
        node(j).add(4).writePointer(j < r.ids.length - 1 ? node(j + 1) : ptr(0));
        if (id === null) node(j).add(8).writePointer(ptr(0));
        else {
          node(j).add(8).writePointer(val(j));
          H.u32(val(j), 4, id);
          H.u32(val(j), 8, 0x8000 + j); H.u32(val(j), 0xc, 0x9000 + j);
          H.u32(val(j), 0x1c, r.c !== undefined && j === r.ids.indexOf(r.find) ? r.c : 0x1c00 + j);
          H.u32(val(j), 0x34, 0x3400 + j);
        }
      });
      b.add(0x218).writePointer(r.ids.length ? node(0) : ptr(0));
      if (vt) b.writePointer(vt);
      out['s' + i] = b;
      out._find.push(r.find);
    });
    // terminal callee thunks: cmdHandler8 -> Seq_remarkEventRange 0x1519, cmdHandler9 -> Seq_replayEventRange 0x1c3f
    if (which === 8) H.stubOn(out.log, 0x1519, 1, true, function (a) { return (a[1].toUInt32() & 0xffff) | 0x7e0000; });
    else H.stubOn(out.log, 0x1c3f, 2, true, function (a) { return (a[1].toUInt32() + a[2].toUInt32()) >>> 0; });
    return out;
  }
  F.c3ew_seq8 = function () { return seqFixture(8); };
  F.c3ew_seq9 = function () { return seqFixture(9); };
  // the find-id vectors, read by the registry (same order as the rows above)
  F.c3ew_seqfind = [1, 0x55, 9, 7, 8, 6, 3, 5, 0xa, 0xf, 2, 1];
})();
// ---- G. Seq_allocTrackSlot ------------------------------------------------------------------------------------------
// Sequencer 0x1000 bytes: +0x23c track count, +0x238 -> the identity index table at +0x600, +0x71c a schedule arg, the list
// header at +0x60. A fixture-side track table at +0x800 (16 dwords, 0 = Seq_listGetAt "not found") and the track blocks at
// +0x900 + 0x40 i (byte +0x38 is the occupied flag, bit 2). The Seq_listGetAt stub (thunk 0x15fa; ecx = this+0x60) writes
// trackTable[idx] to *out and returns 0 when present. Seq_scheduleEvent (thunk 0x240a) records (track, +0x71c, 0x20, 0).
(function () {
  const F = globalThis.DIFF_FIXTURES, H = F.c3ew_h;
  // per sequencer: {count, present: [idx...], occupied: [idx...]} ; track idx t exists if in present, bit2 set if in occupied
  const ROWS = [
    { count: 4, present: [0, 1, 2, 3], occupied: [] },             // first pick free -> set bit
    { count: 4, present: [0, 1, 2, 3], occupied: [0] },            // first occupied, next free
    { count: 4, present: [0, 1, 2, 3], occupied: [0, 1, 2, 3] },   // all occupied -> exhaust the scan
    { count: 1, present: [0], occupied: [] },                      // single slot free
    { count: 1, present: [0], occupied: [0] },                     // single slot occupied -> loop ends (ebp>=count)
    { count: 8, present: [0, 1, 2, 3, 4, 5, 6, 7], occupied: [0, 1, 2] },
    { count: 8, present: [], occupied: [] },                       // first not found -> schedule the (null) out
    { count: 6, present: [0, 1, 2, 3], occupied: [0, 1] },         // a not-found mid scan (idx 4/5 absent)
    { count: 0, present: [], occupied: [] },                       // +0x23c == 0 -> returns 0 at once
    { count: 3, present: [0, 1, 2], occupied: [0, 2] },
    { count: 5, present: [0, 1, 2, 3, 4], occupied: [1, 3] },
    { count: 2, present: [0, 1], occupied: [0, 1] },
  ];
  F.c3ew_seqalloc = function () {
    const out = {};
    H.quietTimer(out);
    out.log = H.stubLog();
    ROWS.forEach(function (r, i) {
      const b = H.block(0x1000, 0x24000000 + 0x1000 * i);
      H.u32(b, 0x23c, r.count);
      b.add(0x238).writePointer(b.add(0x600));                          // index table ptr
      for (let k = 0; k < 16; k++) H.u32(b, 0x600 + 4 * k, k);          // identity indices
      H.u32(b, 0x71c, 0x71c00000 + i);
      for (let k = 0; k < 16; k++) {
        if (r.present.indexOf(k) >= 0) {
          const t = b.add(0x900 + 0x40 * k);
          b.add(0x800 + 4 * k).writePointer(t);
          t.add(0x38).writeU8(r.occupied.indexOf(k) >= 0 ? 0xc4 : 0xc0);   // bit2 = occupied
        } else b.add(0x800 + 4 * k).writePointer(ptr(0));
      }
      out['q' + i] = b;
    });
    out.ctr = H.snd(0xb5010);
    __freeze.save(out.ctr, 4);
    out.ctr.writeU32(5);                                                 // start the round-robin at 5 (both arms, restored per vector)
    // Seq_listGetAt: ecx = this+0x60, args (out, idx); write trackTable[idx], return 0 if present
    H.stubOn(out.log, 0x15fa, 2, true, function (a) {
      const self = a[0], outp = a[1], idx = a[2].toUInt32();
      const seq = self.sub(0x60);
      const t = (idx < 16) ? seq.add(0x800 + 4 * idx).readPointer() : ptr(0);
      outp.writePointer(t);
      return t.isNull() ? 1 : 0;
    }, true);   // skipLog: arg1 is &out, a frame-local pointer that differs between the arms
    H.stubOn(out.log, 0x240a, 4, true, function (a) { return (a[1].toUInt32() & 0xffff) | 0x5c0000; });
    return out;
  };
})();
// @@END@@
