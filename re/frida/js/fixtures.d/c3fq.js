// Fixtures for C3 batch c3fq (sound.dll, round 35). Layouts come from the disassembly cited in shim/src/re/c3fq.cpp. Every
// object, vtable, list node, file and log here is allocated by the fixture; no live sound object, device, list, window or file is
// handed to an arm. Helpers copied from re/frida/js/fixtures.d/c3fk.js (freeze guard, quiet multimedia timer, swapped module
// global, recording stub on an incremental-link thunk that only the A/B thread takes, recording vtables and objects, heap
// stand-in). Module globals a fixture writes are saved first and restored by re/frida/js/freeze.js before the game's first
// thread is resumed. All values are synthetic.
//
// Recording objects: a block whose vtable slots are thiscall NativeCallbacks that log {0x5100 + slot offset, this, arguments}
// and return the dword at this + 0x100 + slot offset (the row's answer for that slot, inside the compared object).
(function () {
  const F = globalThis.DIFF_FIXTURES;
  const keep = function (x) { globalThis.__diffKeepAlive.push(x); return x; };
  const snd = function (rva) { return Process.getModuleByName('sound.dll').base.add(rva); };
  function fill(p, n, b) { p.writeByteArray(new Array(n).fill(b)); return p; }
  function marker(p, n, tag) { for (let k = 0; k + 4 <= n; k += 4) p.add(k).writeU32((tag + k) >>> 0); }
  function u32(p, off, v) {
    if (v instanceof NativePointer) p.add(off).writePointer(v); else p.add(off).writeU32(v >>> 0);
  }
  function block(i, tag, n) {
    const sz = n || 0x200;
    const b = fill(keep(Memory.alloc(sz)), sz, 0x3b);
    marker(b, sz, (tag >>> 0) + 0x10000 * i);
    return b;
  }
  function zero(n) { return fill(keep(Memory.alloc(n)), n, 0); }
  function cstr(s) { const p = keep(Memory.alloc(s.length + 8)); fill(p, s.length + 8, 0x5f); p.writeUtf8String(s); return p; }
  const k32 = Process.getModuleByName('kernel32.dll');
  const sleep = keep(new NativeFunction(k32.getExportByName('Sleep'), 'void', ['uint32'], 'stdcall', { scheduling: 'exclusive' }));

  // ---- freezing and the multimedia timer ------------------------------------------------------------------------
  let frozenNow = false;
  function guard() {
    if (frozenNow) return;
    __freeze.thread();
    frozenNow = true;
    __freeze.atFlush(function () { frozenNow = false; });
    __freeze.negOn();
  }
  // c3fe's method (as c3fk): wait for an idle moment (re-entrancy word 0x100b49a8 = 0 and the timer's critical section
  // 0x100b4990 unowned), write 5 (SoundTimer::onTick then leaves at 0x10008edf before touching anything), wait until the word
  // has stayed 5 with the section unowned for 100 ms, and put the idle value back at the flush.
  function quietTimer(out) {
    guard();
    const g = snd(0xb49a8), owner = snd(0xb4990 + 0xc);   // CRITICAL_SECTION.OwningThread
    let idle = null;
    for (let i = 0; i < 400; i++) {
      if (g.readU32() === 0 && owner.readU32() === 0) { idle = 0; break; }
      sleep(1);
    }
    if (idle === null) {
      if (owner.readU32() !== 0) throw new Error('c3fq: timer section 0x100b4990 stays owned');
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
    if (stable < 50) throw new Error('c3fq: timer did not go quiet');
    out.g49a8 = g;
  }
  function swapGlobal(rva, value, out, name) {
    const g = snd(rva);
    if (name) out[name] = g;
    __freeze.setU32(g, (value instanceof NativePointer) ? value.toUInt32() : value);
    return g;
  }

  // ---- call log and recording stub (A/B thread only) ------------------------------------------------------------
  const LOGSZ = 0x820;                       // count at +0, then 0x20-byte entries (65 of them)
  function stubLog() { const l = keep(Memory.alloc(LOGSZ)); fill(l, LOGSZ, 0); return l; }
  function logCall(log, tag, a) {
    const n = log.readU32();
    if (4 + 0x20 * (n + 1) <= LOGSZ) {
      const p = log.add(4 + 0x20 * n);
      p.writeU32(tag >>> 0);
      for (let i = 0; i < a.length && i < 7; i++) p.add(4 + 4 * i).writeU32(ptr(a[i]).toUInt32());
    }
    log.writeU32(n + 1);
  }
  let stubLive = {};
  // a thread-filtered replacement of the incremental-link thunk at `rva`: the A/B thread logs {rva, args} and gets retfn's
  // value; every other thread runs the original. mask: argument indices (0 = ecx for thiscall) logged as 0x5ac0.
  function stubOn(log, rva, nargs, thiscall, retfn, mask) {
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
      logCall(log, rva, mask ? a.map(function (v, k) { return mask.indexOf(k) >= 0 ? ptr(0x5ac0) : v; }) : a);
      const r = retfn ? retfn(a) : 0;
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
  // slots: {offset: nargs | {n: nargs, ret: function (args) -> value}}; default answer = dword at this + 0x100 + offset
  function recVtable(log, slots) {
    const vt = fill(keep(Memory.alloc(0x100)), 0x100, 0);
    Object.keys(slots).forEach(function (k) {
      const off = parseInt(k, 10), s = slots[k];
      const n = typeof s === 'number' ? s : s.n;
      const argt = ['pointer'].concat(Array(n).fill('pointer'));
      vt.add(off).writePointer(keep(new NativeCallback(function () {
        const a = Array.prototype.slice.call(arguments);
        logCall(log, 0x5100 + off, a);
        const r = (typeof s === 'object' && s.ret) ? s.ret(a) : a[0].add(0x100 + off).readU32();
        return (typeof r === 'number') ? (r >>> 0) : ptr(r).toUInt32();
      }, 'uint32', argt, 'thiscall')));
    });
    return vt;
  }
  function recObj(i, tag, vt, cfg, size) {
    const b = block(i, tag, size);
    if (vt) {
      b.writePointer(vt);
      fill(b.add(0x100), 0xf0, 0);   // unset slot answers are 0
    }
    Object.keys(cfg || {}).forEach(function (k) { u32(b, parseInt(k, 10), cfg[k]); });
    return b;
  }
  function heap(out, size, opts) {
    const h = __heapStandIn(Object.assign({ module: 'sound.dll', alloc: [0x424d7], free: [0x4249a], arenaSize: size || 0x400 },
                                          opts || {}));
    out.arena = h.arena;
    out.freelog = h.log;
    return h;
  }
  globalThis.__c3fq = { fill: fill, marker: marker, u32: u32, block: block, zero: zero, guard: guard, quietTimer: quietTimer,
                        swapGlobal: swapGlobal, stubLog: stubLog, logCall: logCall, stubOn: stubOn, recVtable: recVtable,
                        recObj: recObj, heap: heap, snd: snd, keep: keep };

  // ===================================================================================================================
  // A. ChannelTable_releaseObject / ChannelTable_loadRange
  // ===================================================================================================================
  // A table is a 0x220-byte block: slots +4 .. +0x200 hold 0 or a fixture object, +0x204 .. +0x21c keep their markers
  // (+0x218 is the value loadRange passes to slot +4). Objects are 0x1460-byte recording objects (vtable slot 0 = the
  // deleting destructor, one argument) with their four member slots +0x1440 .. +0x144c set explicitly; members are 0x200-byte
  // recording objects.
  function table(i, tag) {
    const t = block(i, tag, 0x220);
    fill(t.add(4), 0x200, 0);
    return t;
  }
  function objWithMembers(out, vt, name, tag, mem) {
    const o = recObj(0, tag, vt, {}, 0x1460);
    for (let j = 0; j < 4; j++) {
      let m = 0;
      if (mem[j]) { m = recObj(j + 1, tag + 0x1000000, vt, {}, 0x200); out[name + '_' + j] = m; }
      u32(o, 0x1440 + 4 * j, m);
    }
    out[name] = o;
    return o;
  }
  // releaseObject rows: {s: slots holding obj, x: {slot: other object id}, m: member pattern, at204: obj also at +0x204}
  const RO = [
    { s: [0], m: [0, 0, 0, 0] },
    { s: [3, 7, 0x7f], m: [1, 0, 0, 0] },
    { s: [], m: [1, 1, 0, 0] },
    { s: 'all', m: [1, 1, 1, 0] },
    { s: [9], m: [1, 1, 1, 1] },
    { s: [2, 4], m: [0, 1, 1, 0] },
    { s: [0x10], m: [1, 0, 1, 1] },
    { s: [0x20, 0x21], m: [1, 1, 0, 1] },
    { s: [0x40], x: { 0x3f: 0, 0x41: 1 }, m: [1, 1, 1, 1] },
    { s: 'even', m: [1, 0, 0, 0] },
    { s: [0x7f], at204: true, m: [1, 1, 0, 0] },
    { s: [1], x: { 0: 2, 2: 3 }, m: [1, 1, 1, 0] },
  ];
  F.c3fq_relobj = function () {
    const out = {};
    guard();                       // pure fixture memory (no freeze needed for correctness), but arms the negctl switch
    const log = stubLog();
    out.log = log;
    const vt = recVtable(log, { 0: 1 });
    RO.forEach(function (r, i) {
      const t = table(i, 0x7b000000);
      const o = objWithMembers(out, vt, 'o' + i, 0x0b000000 + 0x10000 * i, r.m);
      const slots = r.s === 'all' ? Array.from({ length: 0x80 }, function (_, k) { return k; })
        : r.s === 'even' ? Array.from({ length: 0x40 }, function (_, k) { return 2 * k; }) : r.s;
      slots.forEach(function (k) { u32(t, 4 + 4 * k, o); });
      Object.keys(r.x || {}).forEach(function (k) {
        const x = recObj(parseInt(r.x[k], 10), 0x0c000000 + 0x10000 * i, vt, {}, 0x200);
        out['x' + i + '_' + r.x[k]] = x;
        u32(t, 4 + 4 * parseInt(k, 10), x);
      });
      if (r.at204) u32(t, 0x204, o);
      out['t' + i] = t;
    });
    return out;
  };

  // loadRange rows: rec = {last, first, count, r (the dword at rec+0xc that slot +0x10 of every new object returns)}; tbl:
  // {slot: old object id}; old objects: {id: member pattern}; t218: the table's +0x218.
  const LR = [
    { last: 5, first: 2, count: 1, r: 0, tbl: {}, old: {}, t218: 0x218a0000 },
    { last: 0, first: 0, count: 0, r: 0, tbl: {}, old: {}, t218: 0 },
    { last: 3, first: 3, count: 2, r: 0, tbl: { 3: 'A' }, old: { A: [1, 1, 0, 0] }, t218: 7 },
    { last: 6, first: 1, count: 4, r: 0, tbl: { 0: 'D', 1: 'A', 2: 'A', 3: 'B', 5: 'B', 6: 'C' },
      old: { A: [1, 0, 0, 0], B: [0, 0, 0, 0], C: [1, 1, 1, 1], D: [1, 0, 0, 0] }, t218: 0xffffffff },
    { last: 4, first: 0, count: 3, r: 0x26, tbl: { 1: 'A' }, old: { A: [1, 0, 0, 0] }, t218: 9 },
    { last: 12, first: 10, count: 3, r: 0, tbl: { 5: 'E', 11: 'F' }, old: { E: [1, 1, 0, 0], F: [0, 0, 0, 0] }, t218: 0x55 },
    { last: 4, first: 8, count: 2, r: 0, tbl: { 6: 'G' }, old: { G: [1, 0, 0, 0] }, t218: 0x66 },
    { last: 0x7f, first: 0x7f, count: 5, r: 0, tbl: { 0x7f: 'H' }, old: { H: [1, 1, 1, 1] }, t218: 0x7f },
    { last: 0x7f, first: 0, count: 3, r: 0, tbl: 'ijk', old: { I: [1, 0, 0, 0], J: [0, 0, 0, 0], K: [1, 1, 0, 0] }, t218: 0x80 },
    { last: 2, first: 1, count: 3, r: 1, tbl: {}, old: {}, t218: 0x99 },
    { last: 9, first: 0, count: 2, r: 0xffffffff, tbl: { 0: 'A', 9: 'B' }, old: { A: [0, 0, 0, 0], B: [1, 0, 0, 0] }, t218: 0xaa },
    { last: 2, first: 0, count: 2, r: 0, tbl: { 1: 'L', 9: 'L', 0x40: 'M' }, old: { L: [1, 1, 1, 0], M: [1, 0, 0, 0] }, t218: 0xbb },
  ];
  F.c3fq_loadrange = function () {
    const out = {};
    const log = stubLog();
    out.log = log;
    const oldVt = recVtable(log, { 0: 1 });
    // the vtable the MultiSound_ctorWithType stand-in gives every new object: slot 0 (deleting destructor), slot +4 (one
    // argument, answer 0), slot +0x10 (the load: answers the dword its argument points to)
    const newVt = recVtable(log, { 0: 1, 4: 1, 0x10: { n: 1, ret: function (a) { return a[1].readU32(); } } });
    LR.forEach(function (r, i) {
      const t = table(i, 0x7c000000);
      u32(t, 0x218, r.t218);
      const olds = {};
      Object.keys(r.old).forEach(function (id, k) {
        olds[id] = objWithMembers(out, oldVt, 'o' + i + id, 0x0d000000 + 0x10000 * i + 0x1000 * k, r.old[id]);
      });
      if (r.tbl === 'ijk') {
        for (let k = 0; k < 0x80; k++) u32(t, 4 + 4 * k, olds['IJK'[k % 3]]);
      } else {
        Object.keys(r.tbl).forEach(function (k) { u32(t, 4 + 4 * parseInt(k, 10), olds[r.tbl[k]]); });
      }
      const rec = block(i, 0x7d000000, 0x40);
      u32(rec, 0, r.last); u32(rec, 4, r.first); u32(rec, 8, r.count); u32(rec, 0xc, r.r);
      out['t' + i] = t;
      out['r' + i] = rec;
    });
    heap(out, 0x6800, { fill: 0x5d });
    // MultiSound_ctorWithType through its thunk 0x100026ad: this = the new block, one argument (the type global)
    stubOn(log, 0x26ad, 1, true, function (a) {
      a[0].writePointer(newVt);
      fill(a[0].add(0x100), 0xf0, 0);
      a[0].add(4).writeU32(a[1].toUInt32());
      return a[0];
    });
    return out;
  };

  // ===================================================================================================================
  // B. rewindChannel
  // ===================================================================================================================
  // A channel is a 0x1380-byte marker block: +0 the mmio handle (0 or one of the three handles the stand-in serves), +8 a
  // 0x40-byte stream object, +0xc .. +0x53 the MMIOINFO mmioGetInfo / mmioAdvance fill, the fields the body reads set per row.
  // Three handles are shared by every row (each vector starts from the same snapshot of the stand-in block): h0 a buffered
  // DOS file (MMIO_ALLOCBUF, 0x100-byte buffer), h1 the same file unbuffered, h2 a 0x200-byte memory file.
  // Stubs: fillDecodeRing (thunk 0x10002022, this) logs; Channel_recomputeVoiceGains (0x10001efb, this, v) logs;
  // Channel_setPanGains (0x10001a7d, this, v) logs and, when the channel's +0x1370 is non-zero, writes it to +0x58 (so the
  // re-read of +0x58 at 0x10033ad9 after the calls is observable).
  const RW = [
    { h: 0, f58: 0x00000000, f5c: 0x00000000 },                    // 0x14 return
    { h: 0, f58: 0xffffffff, f5c: 0xfffffffb },                    // 0x14 return, other bits set
    { h: 0, f58: 0x00000120, f5c: 0x00000004 },                    // no handle, bit 2: no mmio, no loop
    { h: 0, f58: 0x00000001, f5c: 0x00000007 },                    // bit 0: stream copy, voice loop
    { h: 0, f58: 0x1ff, f5c: 0xff, poke: 0x0 },                   // bit 0 set, setPanGains clears +0x58: no loop
    { h: 1, f58: 0xfffffffe, f5c: 0x00000003, w18c: 2, f164: 0x40, f174: 0x10 },     // buffered, seek 0x90
    { h: 2, f58: 0x00000000, f5c: 0x00000000, w18c: 1, f164: 0x80, f174: 0 },        // unbuffered (advance 266)
    { h: 3, f58: 0x80000000, f5c: 0x00000010, w18c: 4, f164: 0x20, f174: 0x8 },      // memory file
    { h: 1, f58: 0x00000000, f5c: 0x00000000, w18c: 0x10, f164: 0x100, f174: 0x7 },  // seek past the end
    { h: 1, f58: 0x00000020, f5c: 0x00000000, w18c: 3, f164: 0x55, f174: 0x3, poke: 0x00000001 },   // poke sets bit 0: loop
    { h: 1, f58: 0xf0000f67, f5c: 0x00000004 },                    // handle with bit 2: no mmio, loop
    { h: 2, f58: 0x00000101, f5c: 0x00000008, f24: 0x1234 },       // handle, bit 0: stream copy wins
  ];
  function rwFile() {
    const a = new Uint8Array(0x400);
    for (let k = 0; k < a.length; k++) a[k] = (k * 37 + 11) & 0xff;
    return a;
  }
  F.c3fq_rewind = function () {
    const out = {};
    const log = stubLog();
    out.log = log;
    const m = __mmioStandIn({ module: 'sound.dll', files: { 'c3fq_rw.raw': rwFile() }, bufSlots: 3, bufCap: 0x200,
                              size: 0x1800 });
    out.mmblock = m.block;
    out.implog = m.implog;
    const hs = [ptr(0), m.open('c3fq_rw.raw', 0x10000, 0x100), m.open('c3fq_rw.raw', 0, 0), m.openMem(0x200, 0, null, 0)];
    RW.forEach(function (r, i) {
      const c = block(i, 0x0e000000, 0x1380);
      const st = block(i, 0x0f000000, 0x40);
      u32(c, 0, hs[r.h]);
      u32(c, 8, st);
      u32(c, 0x58, r.f58);
      u32(c, 0x5c, r.f5c);
      if (r.w18c !== undefined) c.add(0x18c).writeU16(r.w18c);
      if (r.f164 !== undefined) u32(c, 0x164, r.f164);
      if (r.f174 !== undefined) u32(c, 0x174, r.f174);
      if (r.f24 !== undefined) u32(c, 0x24, r.f24);
      u32(c, 0x1370, r.poke === undefined ? 0 : (r.poke || 0x80000000));
      if (r.poke === 0) u32(c, 0x1370, 0x80000000);     // non-zero answer with bit 0 clear
      out['c' + i] = c;
      out['s' + i] = st;
    });
    stubOn(log, 0x2022, 0, true, function () { return 0x2022; });
    stubOn(log, 0x1efb, 1, true, function () { return 0x1efb; });
    stubOn(log, 0x1a7d, 1, true, function (a) {
      const p = a[0].add(0x1370).readU32();
      if (p) a[0].add(0x58).writeU32(p);
      return 0x1a7d;
    });
    return out;
  };

  // ===================================================================================================================
  // C. The buffer queue: Wave_Device::appendBuffer / Wave_Device::freeBufferList
  // ===================================================================================================================
  // Both write the module counter [0x100b4a08] (lowered after each mmioClose): the first thread is frozen, the multimedia
  // timer held quiet (quietTimer) and the counter is a state region, restored after the key. mmioClose is stood in through
  // importstandin.js (answer 0, both arguments recorded); handles are synthetic values, never real ones. Queue nodes are
  // 0x10-byte blocks {prev, next, payload}; voices 0x100-byte blocks.
  function node(i, tag, prev, next, payload) {
    const n = block(i, tag, 0x10);
    u32(n, 0, prev || 0); u32(n, 4, next || 0); u32(n, 8, payload || 0);
    return n;
  }
  // a queue on `self` from a list of payloads (null = a node with a NULL payload); returns the nodes
  function queue(out, self, name, payloads, tag) {
    const ns = payloads.map(function (p, k) { return node(k, tag + 0x100 * k, 0, 0, p); });
    ns.forEach(function (n, k) {
      u32(n, 0, k ? ns[k - 1] : 0);
      u32(n, 4, k + 1 < ns.length ? ns[k + 1] : 0);
      out[name + k] = n;
    });
    u32(self, 0x5c, ns.length ? ns[0] : 0);
    u32(self, 0x60, ns.length ? ns[ns.length - 1] : 0);
    u32(self, 0x68, ns.length);
    return ns;
  }
  function closeStandIn(out) {
    const s = __importStandIn({ module: 'sound.dll', imports: { 'WINMM.dll!mmioClose': { args: 2, ret: 0 } } });
    out.implog = s.log;
  }

  // appendBuffer: a 0x400 Wave_Device. Only buf == this+0x6c is accepted; the source range is buf+0x18 / buf+0x20
  // (d = end - start); the format at +0xd8 has nBlockAlign at +0xc (must be non-zero: appendBuffer divides d by it,
  // 0x10006975). Voice_resetDefaults 0x10037c40 and MmioBuffer_init 0x10037e10 are C3 and run for real on fixture memory;
  // the device global [0x100b4a20] is NULL under the freeze so Voice_resetDefaults skips its live-device call (je 0x10037cc6).
  // The voice (0x110) and node (0xc) come from the heap stand-in; mmioClose (the d==0 arm) is recorded by the import stand-in.
  // Prior voices (0x110) are fixture-owned so their addresses are stable in both arms; the back-patch at 0x10006998 writes
  // the previous tail voice's +0x98 / +0x9c / +0xb4 / +0xcc.
  const AP = [
    { d: 0, f58: 0x00000000, q: 0 },                           // mmioClose arm (d==0), returns 0x26
    { d: 0, f58: 0x00000005, q: 1 },                           // mmioClose arm, bits set
    { d: 0x800, f58: 0x00000000, q: 0, align: 4 },             // first append, empty queue, no prevVoice
    { d: 0x800, f58: 0x00000004, q: 1, align: 4 },             // append with one prior voice (back-patch), bit 2 set, bit 0 clear
    { d: 0x1234, f58: 0x00000005, q: 1, align: 2 },            // bit 2 and bit 0 set: no flag change
    { d: 0x1000, f58: 0x00000004, q: 2, align: 8 },            // two prior voices (tail back-patch)
    { d: 0x40, f58: 0xfffffffb, q: 1, align: 0x10 },           // bit 2 clear: no flag change
    { d: 0x7fff, f58: 0x00000006, q: 1, align: 1 },            // bit 1+2 set, bit 0 clear: flag change
    { d: 0x200, f58: 0x00000000, q: 0, align: 0x20 },          // empty, flag unchanged (bit 2 clear)
    { d: 3, f58: 0x00000004, q: 2, align: 3 },                 // small d, two prior
    { d: 0x10000, f58: 0x00000007, q: 1, align: 0x40 },        // bit 0,1,2 set
    { d: 0x55, f58: 0x00000004, q: 1, align: 0x80 },           // append, back-patch, bit 2 only
  ];
  F.c3fq_append = function () {
    const out = {};
    quietTimer(out);                                          // freeze the main thread and hold the live multimedia timer:
                                                              // the engine reads the Wave_Device globals swapped below
    swapGlobal(0xb4a08, 7, out, 'g4a08');                      // the open-handle counter (lowered on the d==0 arm)
    swapGlobal(0xb4a20, 0, out, 'g4a20');                      // device NULL: Voice_resetDefaults skips the live arm
    heap(out, 0x4000, { fill: 0 });                            // operator new voices (0x110) and nodes (0xc)
    AP.forEach(function (r, i) {
      const dev = block(i, 0x11000000, 0x400);
      u32(dev, 0x58, r.f58);
      u32(dev, 0x6c + 0x18, 0x1000);
      u32(dev, 0x6c + 0x20, (0x1000 + r.d) >>> 0);
      dev.add(0xd8 + 0xc).writeU16(r.align || 1);
      const pvs = [];
      for (let k = 0; k < r.q; k++) {
        pvs.push(recObj(k, 0x12000000 + 0x10000 * i, 0, {}, 0x110));
        out['pv' + i + '_' + k] = pvs[k];
      }
      queue(out, dev, 'ap' + i + '_', pvs, 0x13000000 + 0x10000 * i);
      u32(dev, 0xb0, 0x41410000 + i);    // the synthetic mmio handle the d==0 arm closes (recorded, not real)
      out['d' + i] = dev;
      out['b' + i] = dev.add(0x6c);      // the only accepted argument is this+0x6c
    });
    closeStandIn(out);
    return out;
  };

  // freeBufferList(dst, count, fmt, outN): converts the head voice through the converter table 0x1005b4a4[fmt] and drains /
  // trims the queue. The converter (mixRated* at table slot fmt; all C3) runs for real on a fixture voice; VoiceSlot_dtorEmpty
  // 0x10037df0 (C2) and the two vtable slots +0x80 / +0x20 of the device are recorded stubs. Rows vary +0x58 bit 3 (the first
  // drain), fmt (< 0xc no pad, >= 0xc pad with a zero fill of (count - got) * 8), the queue (empty / voices with NULL and
  // non-NULL payloads), each voice's +0xcc (the extra-rounds count) and +0x1c / +0x20 / +0x9c (the finish test). The voice
  // the converter reads is seeded so mixRated* reads defined fields; its result `got` is the dword at &pos the converter
  // writes, compared as state.
  function freeVoice(out, i, tag, cc, cur, end, flag) {
    const v = recObj(0, tag, 0, {}, 0x220);
    u32(v, 0xcc, cc || 0);                 // extra rounds (freeBufferList reads +0xcc at 0x1000662b)
    u32(v, 0x1c, cur || 0);                // current position (converter reads and advances it through &pos)
    u32(v, 0x20, end === undefined ? 0x400 : end);   // end position (0x100066ce: finished when pos >= +0x20)
    u32(v, 0x44, 0x42420000 + i);          // mmio handle (recorded close)
    u32(v, 0x9c, flag || 0);               // byte +0x9c bit 0 gates the finish (0x100066d7)
    out['fv' + i] = v;
    return v;
  }
  // The converter table entries 0x1005b4a4[fmt] are the mixRated* resamplers (all C3), which need a fully formed source
  // voice and buffer. For freeBufferList's A/B only the call itself matters (both arms take it identically), so each used
  // slot is swapped under the freeze for a recording cdecl stub (ptr d, ptr pos, count, voice, 0) that records its call and
  // returns count - 4 (clamped to >= 0), leaving *pos and *d: freeBufferList's own `got != count` pad and `pos >= end`
  // finish logic is then exercised by the row's seeded fields, not by the resampler's output.
  const vprotect = keep(new NativeFunction(k32.getExportByName('VirtualProtect'), 'int',
                                           ['pointer', 'uint32', 'uint32', 'pointer'], 'stdcall'));
  // The converter table 0x1005b4a4 lives in a read-only page and its entries (the mixRated* resamplers) are shim-hooked at
  // C3, so neither the slot nor the body can be rewritten normally. For freeBufferList's A/B only the call matters (both
  // arms take it identically), so the used slots are made writable with VirtualProtect, swapped under the freeze for one
  // recording cdecl stub (ptr d, ptr pos, count, voice, 0) that records its call and returns count - 4 (>= 0), leaving *pos
  // and *d: freeBufferList's own `got != count` pad and `pos >= end` finish logic is exercised by the row's seeded fields,
  // not the resampler's output. The original value is saved/restored by __freeze.setU32.
  function convStub(log, fmts) {
    guard();
    const cb = keep(new NativeCallback(function (d, pos, count, voice, z) {
      // &d and &pos are the CALLER's stack slots (freeBufferList's locals): their addresses differ between the two arms'
      // frames, so log them as a constant and keep only count / voice / z (stable) as compared state.
      logCall(log, 0x5c00, [ptr(0x5ac0), ptr(0x5ac0), count, voice, z]);
      const c = count.toUInt32();
      return (c > 4 ? c - 4 : 0) >>> 0;
    }, 'uint32', ['pointer', 'pointer', 'uint32', 'pointer', 'uint32'], 'default'));
    const oldp = Memory.alloc(4);
    fmts.forEach(function (f) {
      const slot = snd(0x5b4a4 + 4 * f);
      if (!vprotect(slot, 4, 0x04, oldp)) throw new Error('c3fq: VirtualProtect failed on the converter slot');
      const saved = oldp.readU32();
      // __freeze restores run newest-first: queue the page-protection restore FIRST so the value restore (queued by setU32
      // after it) runs while the page is still writable.
      __freeze.atFlush(function () { vprotect(slot, 4, saved, oldp); });
      __freeze.setU32(slot, cb.toUInt32());
    });
  }
  const FR = [
    { drain: 0, fmt: 0, payloads: ['v'], cc: 0, cur: 0, end: 0x40, flag: 0 },         // one voice, no extra rounds, not finished
    { drain: 0, fmt: 0, payloads: [], cc: 0 },                                         // empty queue -> *outN = 0, no pad, 0x26
    { drain: 0, fmt: 0xc, payloads: [], cc: 0 },                                       // empty queue, pad: zero count*8 bytes, 0x26
    { drain: 8, fmt: 0, payloads: ['v', null, 'v'], cc: 0, cur: 0, end: 0x40 },        // bit 3: drain the whole queue first
    { drain: 0, fmt: 0, payloads: ['v'], cc: 1, cur: 0, end: 0x40 },                   // one extra round (pops a node)
    { drain: 0, fmt: 0xc, payloads: ['v'], cc: 0, cur: 0x400, end: 0x40, flag: 0 },    // finished (pos >= end, flag clear): trim
    { drain: 0, fmt: 0xc, payloads: ['v'], cc: 0, cur: 0x400, end: 0x40, flag: 1 },    // pos >= end but flag set: no trim
    { drain: 0, fmt: 5, payloads: ['v', 'v'], cc: 2, cur: 0, end: 0x40 },              // two extra rounds
    { drain: 0, fmt: 0, payloads: ['v'], cc: 0, cur: 0x10, end: 0x40 },                // not finished (pos < end)
    { drain: 8, fmt: 0, payloads: [], cc: 0 },                                          // bit 3 set but empty: slot +0x80 only
    { drain: 0, fmt: 0xd, payloads: ['v'], cc: 0, cur: 0x400, end: 0x40, flag: 0 },    // pad fmt, finished
    { drain: 0, fmt: 1, payloads: ['v'], cc: 0, cur: 0x20, end: 0x40 },                // fmt 1 converter, mid
  ];
  F.c3fq_free = function () {
    const out = {};
    const log = stubLog();
    out.log = log;
    quietTimer(out);                                         // freeze the main thread and hold the live multimedia timer
    const counter = swapGlobal(0xb4a08, 7, out, 'g4a08');
    const dst = zero(0x800); keep(dst); out.dst = dst;        // the destination buffer (count*8 bytes max)
    const outN = zero(4); keep(outN); out.outN = outN;
    FR.forEach(function (r, i) {
      const dev = block(i, 0x14000000, 0x400);
      u32(dev, 0x58, r.drain ? (r.drain | 0) : 0);
      // the vtable with slots +0x80 (drain finish) and +0x20 (trim finish) as recording stubs
      const vt = recVtable(log, { 0x80: 0, 0x20: 0 });
      dev.writePointer(vt);
      fill(dev.add(0x100), 0x120, 0);
      const pays = r.payloads.map(function (p, k) {
        return p === 'v' ? freeVoice(out, i * 10 + k, 0x15000000 + 0x10000 * i + 0x1000 * k, r.cc, r.cur, r.end, r.flag) : 0;
      });
      queue(out, dev, 'fq' + i + '_', pays, 0x16000000 + 0x10000 * i);
      out['d' + i] = dev;
    });
    // VoiceSlot_dtorEmpty 0x10037df0 is a single `ret` (empty destructor): it runs for real, harmlessly, in both arms.
    convStub(log, [0, 1, 5, 0xc, 0xd]);
    closeStandIn(out);
    heap(out, 0x400, { fill: 0 });     // operator delete recorder only (no alloc)
    return out;
  };

  // ===================================================================================================================
  // D. create_sound
  // ===================================================================================================================
  // create_sound(out, name, type) picks a Sound class by `type` (if non-zero) or by strstr of `name` against "wav" / "mid" /
  // "aif" / "rmi", allocates it (heap stand-in) and builds it. The seven constructors (thunks 0x100026ad, 0x10002513,
  // 0x1000138e, 0x10001163, 0x100018c5, 0x1000140b, 0x1000237e) and the two name setters (0x100021b7, 0x1000105f) are all
  // C3; they are recording stubs that return `this`, so only the dispatch (which type, which size, which constructor/name
  // call, in which order) is compared. strstr 0x10042690 (C3) runs for real on the fixture names. The gate global
  // [0x100b4a1c] is swapped under the freeze (set / zero) so idx 1 and 4 reach both their build arm and their `return 3`.
  // Rows exercise every jump-table index through an explicit `type` and the strstr path through names.
  const CS_V = [   // [name, type]  -- type 0 means use the strstr path
    ['menu.wav', 0], ['song.mid', 0], ['clip.aif', 0], ['bank.rmi', 0], ['noext', 0], [null, 0],
    ['x', 1], ['x', 2], ['x', 3], ['x', 4], ['x', 5], ['x', 6], ['x', 7], ['x', 8], ['x', 0x80],
  ];
  function createSound(gate) {
    return function () {
      const out = {};
      const log = stubLog();
      out.log = log;
      swapGlobal(0xb4a1c, gate, out, 'g4a1c');       // the gate for idx 1 / 4
      heap(out, 0x8000, { fill: 0 });                // operator new for every class size
      CS_V.forEach(function (r, i) {
        out['out' + i] = zero(4); keep(out['out' + i]);
        out['nm' + i] = r[0] === null ? ptr(0) : cstr(r[0]);
      });
      // the seven constructors: recording stubs returning `this` (arg 0)
      [[0x26ad, 1], [0x2513, 0], [0x138e, 0], [0x1163, 0], [0x18c5, 0], [0x140b, 0], [0x237e, 0]].forEach(function (c) {
        stubOn(log, c[0], c[1], true, function (a) { return a[0]; });
      });
      // Object_setName / Sound_assignName: recording stubs, one stack argument (name); return value discarded
      stubOn(log, 0x21b7, 1, true, function () { return 0; });
      stubOn(log, 0x105f, 1, true, function () { return 0; });
      return out;
    };
  }
  F.c3fq_create = createSound(1);        // gate set: idx 1 / 4 build
  F.c3fq_create_gate0 = createSound(0);  // gate 0: idx 1 / 4 return 3
})();
