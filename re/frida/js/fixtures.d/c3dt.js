// Fixtures for C3 batch c3dt (sound.dll, round 28): reachable C2 rows earlier batches left at C2. Layouts come from the
// disassembly cited in shim/src/re/c3dt.cpp. Every object, vtable, COM-shaped interface, list node and string here is
// allocated by the fixture; no live sound object, device, list, window or file is handed to an arm. Stand-ins used (one call
// per fixture): __heapStandIn (re/frida/js/heapstandin.js) over sound.dll's operator delete 0x4249a, __importStandIn
// (re/frida/js/importstandin.js) over sound.dll's IAT slots, and a thread-filtered recording stub on an incremental-link
// thunk (stubOn below) for forwarders whose callee has a large tree. Module globals a fixture writes are saved first and
// restored by re/frida/js/freeze.js before the game's first thread is resumed. All values are synthetic.
// NativeCallbacks are kept alive through __diffKeepAlive.
(function () {
  const F = globalThis.DIFF_FIXTURES;
  const keep = function (x) { globalThis.__diffKeepAlive.push(x); return x; };
  const snd = function (rva) { return Process.getModuleByName('sound.dll').base.add(rva); };
  function fill(p, n, b) { p.writeByteArray(new Array(n).fill(b)); return p; }
  function marker(p, n, tag) { for (let k = 0; k < n; k += 4) p.add(k).writeU32((tag + k) >>> 0); }
  function u32(p, off, v) { p.add(off).writeU32(v >>> 0); }

  // ---- freezing and the multimedia timer -----------------------------------------------------------------------
  // The first thread is frozen once per key (idempotent through the flag). The module re-entrancy word 0x100b49a8 is preset
  // to 5 (SoundTimer::onTick then leaves at 0x10008edf, c3dh_tickword) so the multimedia timer thread does not read the
  // globals this key swaps; the word is saved first, restored at the flush, and returned so the registry can compare it.
  let frozenNow = false;
  function guard() {
    if (frozenNow) return;
    __freeze.thread();
    frozenNow = true;
    __freeze.atFlush(function () { frozenNow = false; });
    // the negative-control build (log/c3/alt/c3dt/negctl_gen.py) keeps its wrong detail of Channel_serviceBuffer dormant until its exported NegCtl switch is set;
    // in the normal build the address is unmapped and nothing is written
    __freeze.negOn();   // every exported <Batch>NegCtl switch (re/frida/js/freeze.js)
  }
  function quietTimer(out) {
    guard();
    const g = snd(0xb49a8);
    out.g49a8 = g;
    __freeze.setU32(g, 5);
    // a tick already inside the handler writes 0 back when it ends: set it again until it reads 5 after a short wait
    const sleep = new NativeFunction(Process.getModuleByName('kernel32.dll').getExportByName('Sleep'), 'void', ['uint32'], 'stdcall');
    for (let i = 0; i < 20; i++) { sleep(2); if (g.readU32() === 5) break; g.writeU32(5); }
  }
  // a module global swapped for the key: saved, set, restored at the flush (call after quietTimer)
  function swapGlobal(rva, value, out, name) {
    const g = snd(rva);
    if (name) out[name] = g;
    __freeze.setU32(g, value);
    return g;
  }

  // ---- recording stub on a thunk or function entry (A/B thread only) -------------------------------------------
  // Each call on the A/B thread appends {rva, ecx or 0, args...} (0x20 bytes) to `log` (+0 count, entries from +4) and returns
  // retfn(args, callIndex) (0 without retfn). Calls from any other thread are forwarded to the real function. Reverted at the
  // flush, before the first thread resumes.
  let stubLive = {};
  function stubLog() { const l = keep(Memory.alloc(0x200)); fill(l, 0x200, 0); return l; }
  function stubOn(log, rva, nargs, thiscall, retfn) {
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
      if (n < 15) {
        const p = log.add(4 + 0x20 * n);
        p.writeU32(rva >>> 0);
        for (let i = 0; i < a.length && i < 7; i++) p.add(4 + 4 * i).writeU32(a[i].toUInt32());
      }
      log.writeU32(n + 1);
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

  // ===================================================================================================================
  // A. wave-in capture device
  // ===================================================================================================================
  // Device block (0x300 bytes, markers everywhere): +4 flag dword, +0x44 the winmm handle, +0x48 the format area, +0x5c buffer
  // list head, +0x64 cursor, +0x6c sub-object. Nodes (+0x100 + 0x10 j) are {+0 prev, +4 next, +8 header pointer}; headers
  // (+0x180 + 0x20 j) are 0x20-byte blocks whose first dword is the result the stood-in winmm call returns for that header.
  function wiBlock(i, o) {
    const b = fill(keep(Memory.alloc(0x300)), 0x300, 0x3e);
    marker(b, 0x300, 0x4c000000 + 0x100 * i);
    const hd = o.hd || [];
    const hdr = function (j) { return b.add(0x180 + 0x20 * j); };
    const node = function (j) { return b.add(0x100 + 0x10 * j); };
    hd.forEach(function (v, j) {
      hdr(j).writeU32(v >>> 0); u32(hdr(j), 4, 0xa11a0000 + j); u32(hdr(j), 0x10, 0x5e5e0000 + j);
      node(j).writePointer(j > 0 ? node(j - 1) : ptr(0));
      node(j).add(4).writePointer(j < hd.length - 1 ? node(j + 1) : ptr(0));
      node(j).add(8).writePointer(hdr(j));
    });
    if (o.nullHdr !== undefined) node(o.nullHdr).add(8).writePointer(ptr(0));
    u32(b, 4, o.flags === undefined ? 0xa5a5a5a5 : o.flags);
    u32(b, 0x44, o.h === undefined ? 0x7001 + i : o.h);
    b.add(0x5c).writePointer(hd.length ? node(0) : ptr(0));
    u32(b, 0x64, 0x64646464);
    b.add(0x6c).writePointer(o.sub ? o.sub : ptr(0));
    if (o.fmt !== undefined) u32(b, 0x48, o.fmt);
    return b;
  }

  // WaveInDevice_start: twelve handles
  F.c3dt_wistart = function () {
    const out = {};
    [0, 1, 0x7001, 0x70000001, 0xffffffff, 0x80000000, 0x1234, 0x5555, 2, 0x100, 0xdeadbeef, 0x41].forEach(function (h, i) {
      out['w' + i] = wiBlock(i, { h: h });
    });
    out.implog = __importStandIn({ module: 'sound.dll', imports: { 'WINMM.dll!waveInStart': { args: 1, ret: 0 } } }).log;
    return out;
  };
  // WaveInDevice_reset: the same handles; waveInStop then waveInReset
  F.c3dt_wireset = function () {
    const out = {};
    [0, 1, 0x7001, 0x70000001, 0xffffffff, 0x80000000, 0x1234, 0x5555, 2, 0x100, 0xdeadbeef, 0x41].forEach(function (h, i) {
      out['w' + i] = wiBlock(i, { h: h });
    });
    out.implog = __importStandIn({ module: 'sound.dll', imports: {
      'WINMM.dll!waveInStop': { args: 1, ret: 0 }, 'WINMM.dll!waveInReset': { args: 1, ret: 0 } } }).log;
    return out;
  };
  // WaveInDevice_prepareHeaders / addBuffer. Header result codes: 0 ok; 2..0x23 and 0xffff the error table. Rows:
  //  0 no list head; 1 head without header pointer; 2 zero handle; 3 one header ok; 4 three ok; 5 error in the first;
  //  6 error in the second of three; 7 error in the third; 8 a node without header in the middle; 9..21 single header with an error code
  function wiRows() {
    return [
      { hd: [] }, { hd: [0], nullHdr: 0 }, { hd: [0], h: 0 }, { hd: [0] }, { hd: [0, 0, 0] }, { hd: [3, 0, 0] }, { hd: [0, 4, 0] },
      { hd: [0, 0, 5] }, { hd: [0, 0, 0], nullHdr: 1 }, { hd: [2] }, { hd: [6] }, { hd: [7] }, { hd: [8] }, { hd: [0x1f] },
      { hd: [0x20] }, { hd: [0x21] }, { hd: [0x22] }, { hd: [0x23] }, { hd: [0xffff] }, { hd: [0, 0], nullHdr: 1 }, { hd: [0, 9] },
    ];
  }
  function wiLoopFixture(api) {
    const out = {};
    wiRows().forEach(function (r, i) { out['w' + i] = wiBlock(i, r); });
    const spec = {};
    spec['WINMM.dll!' + api] = { args: 3, impl: function (a) { return a[1].readU32(); } };
    out.implog = __importStandIn({ module: 'sound.dll', imports: spec }).log;
    return out;
  }
  F.c3dt_wiprep = function () { return wiLoopFixture('waveInPrepareHeader'); };
  F.c3dt_wiadd = function () { return wiLoopFixture('waveInAddBuffer'); };
  // WaveInDevice_requeueBuffer: rows [sub-object present, header]; the sub-object stub is the thunk 0x1000176c (loadWaveResource)
  F.c3dt_wirequeue = function () {
    const out = {};
    const rows = [[0, 1], [1, 1], [0, 0], [1, 0], [0, 2], [1, 3], [1, 2], [0, 4], [1, 5], [0, 6], [1, 7], [1, 1]];
    const sub = fill(keep(Memory.alloc(0x40)), 0x40, 0x77);
    rows.forEach(function (r, i) {
      out['w' + i] = wiBlock(i, { sub: r[0] ? sub : null, h: i === 11 ? 0 : undefined });
      if (r[1]) {
        const h = fill(keep(Memory.alloc(0x40)), 0x40, 0x90 + i);
        marker(h, 0x40, 0x5d000000 + 0x100 * i);
        out['h' + i] = h;
      }
    });
    out.log = stubLog();
    stubOn(out.log, 0x176c, 1, true, function (a) { return 0x1000 + a[1].toUInt32(); });
    out.implog = __importStandIn({ module: 'sound.dll', imports: {
      'WINMM.dll!waveInUnprepareHeader': { args: 3, ret: 33 }, 'WINMM.dll!waveInPrepareHeader': { args: 3, ret: 0 },
      'WINMM.dll!waveInAddBuffer': { args: 3, ret: 0 } } }).log;
    return out;
  };
  // WaveInDevice_open: the format area's first dword is the code waveInOpen returns; on success it stores a handle
  F.c3dt_wiopen = function () {
    const out = {};
    const codes = [0, 2, 3, 4, 5, 6, 7, 8, 0x1f, 0x20, 0x21, 0x22, 0xffff, 1];
    codes.forEach(function (c, i) {
      out['w' + i] = wiBlock(i, { h: i === 13 ? 0x4242 : 0, fmt: c, flags: i === 0 ? 0x10 : 0x40000000 + i });
    });
    out.implog = __importStandIn({ module: 'sound.dll', imports: {
      'WINMM.dll!waveInOpen': { args: 6, impl: function (a) {
        const code = a[2].readU32();
        if (code === 0) a[0].writeU32(0x7e570000);
        return code;
      } } } }).log;
    return out;
  };

  // ===================================================================================================================
  // B. mmio handle close and forwarders
  // ===================================================================================================================
  // object blocks of 0x60 bytes with the mmio handle at +0x30
  function mmBlock(i, h) {
    const b = fill(keep(Memory.alloc(0x60)), 0x60, 0x3d);
    marker(b, 0x60, 0x6d000000 + 0x100 * i);
    u32(b, 0x30, h);
    return b;
  }
  const MMH = [0, 1, 0x1234, 0xffffffff, 0x80000000, 2, 0x7777, 0x41, 0x100, 0, 0x55, 3];
  F.c3dt_closemmio = function () {
    const out = {};
    guard();
    MMH.forEach(function (h, i) { out['m' + i] = mmBlock(i, h); });
    const cnt = snd(0xb4a08);
    out.cnt = cnt;
    __freeze.setU32(cnt, 20);
    out.implog = __importStandIn({ module: 'sound.dll', imports: { 'WINMM.dll!mmioClose': { args: 2, ret: 0 } } }).log;
    return out;
  };
  // closeStream: g_waveStreamDev 0x100b49f4 swapped for the key (null, or a block whose handle is `h`); the chain runs
  // closeMmioHandle (mmioClose stood in, counter 0x100b4a08 preset)
  F.c3dt_closestream = function (h) {
    const out = {};
    quietTimer(out);
    const cnt = snd(0xb4a08);
    out.cnt = cnt;
    __freeze.setU32(cnt, 20);
    if (h === null) swapGlobal(0xb49f4, 0, out, 'gdev');
    else { out.dev = mmBlock(0, h); swapGlobal(0xb49f4, out.dev.toUInt32(), out, 'gdev'); }
    out.implog = __importStandIn({ module: 'sound.dll', imports: { 'WINMM.dll!mmioClose': { args: 2, ret: 0 } } }).log;
    return out;
  };
  F.c3dt_closestream_null = function () { return F.c3dt_closestream(null); };
  F.c3dt_closestream_h0 = function () { return F.c3dt_closestream(0); };
  F.c3dt_closestream_h1 = function () { return F.c3dt_closestream(0x1234); };
  F.c3dt_closestream_h2 = function () { return F.c3dt_closestream(0xffffffff); };
  // openWaveStreamFile: g_waveStreamDev swapped; the callee thunk 0x10002211 (openMmioFile) is a recording stub that returns arg + 7
  F.c3dt_openstream = function (withDev) {
    const out = {};
    quietTimer(out);
    if (withDev) { out.dev = mmBlock(0, 0x11); swapGlobal(0xb49f4, out.dev.toUInt32(), out, 'gdev'); }
    else swapGlobal(0xb49f4, 0, out, 'gdev');
    out.log = stubLog();
    stubOn(out.log, 0x2211, 1, true, function (a) { return (a[1].toUInt32() + 7) >>> 0; });
    return out;
  };
  F.c3dt_openstream_null = function () { return F.c3dt_openstream(false); };
  F.c3dt_openstream_dev = function () { return F.c3dt_openstream(true); };
  // Seq_closeFile: objects of 0x100 bytes; byte +0xac (bit 0 selects the call), dword +0x44; thunk 0x10001014 stubbed
  // (returns the dword at ecx+0x10, i.e. this+0x70)
  F.c3dt_seqclose = function () {
    const out = {};
    const rows = [[0x00, 0x1], [0x01, 0x1], [0x01, 0x0], [0x02, 0xffffffff], [0x03, 0xfffffffe], [0xff, 0x80000001], [0x80, 0x3],
                  [0x81, 0x55], [0x00, 0x0], [0x7f, 0x100], [0x01, 0x101], [0x10, 0x2]];
    rows.forEach(function (r, i) {
      const b = fill(keep(Memory.alloc(0x100)), 0x100, 0x2c);
      marker(b, 0x100, 0x5e000000 + 0x100 * i);
      b.add(0xac).writeU8(r[0]);
      u32(b, 0x44, r[1]);
      out['s' + i] = b;
    });
    out.log = stubLog();
    stubOn(out.log, 0x1014, 0, true, function (a) { return a[0].add(0x10).readU32(); });
    return out;
  };

  // ===================================================================================================================
  // C. deleting-destructor wrappers and the sound window object
  // ===================================================================================================================
  // Twelve 0x40-byte objects (markers) and the flag arguments used by the vectors in the registry. The member destructor
  // (thunk RVA given) is a recording stub that returns the dword at this+8; operator delete 0x1004249a only records.
  function delFixture(thunkRva) {
    const out = {};
    for (let i = 0; i < 12; i++) {
      const b = fill(keep(Memory.alloc(0x40)), 0x40, 0x31);
      marker(b, 0x40, 0x7d000000 + 0x100 * i);
      out['o' + i] = b;
    }
    out.log = stubLog();
    stubOn(out.log, thunkRva, 0, true, function (a) { return a[0].add(8).readU32(); });
    out.freelog = __heapStandIn({ module: 'sound.dll', free: [0x4249a] }).log;
    return out;
  }
  F.c3dt_del_sound = function () { return delFixture(0x2446); };
  F.c3dt_del_wavedev = function () { return delFixture(0x17da); };
  F.c3dt_del_ctrla = function () { return delFixture(0x1019); };
  F.c3dt_del_s5b6a8 = function () { return delFixture(0x1893); };
  F.c3dt_del_s5c450 = function () { return delFixture(0x19c4); };
  F.c3dt_del_streamrd = function () { return delFixture(0x108c); };
  F.c3dt_del_wavein = function () { return delFixture(0x113b); };
  F.c3dt_del_multi = function () { return delFixture(0x1d39); };
  // SoundWindowObj_dtor / destroySoundWindowObj: objects {+0 vtable, +4 flag byte, +8 window, +0xc previous procedure}
  const SWO = [
    [0x00, 0, 0x11], [0x04, 0, 0x22], [0x00, 0x1111, 0x33], [0x04, 0x1111, 0x44], [0x05, 0x2222, 0x55], [0xfb, 0x3333, 0x66],
    [0xff, 0x4444, 0x77], [0x04, 0xffffffff, 0x88], [0x0c, 0x5555, 0x99], [0x84, 0x6666, 0], [0x08, 0x7777, 0xaa], [0x04, 1, 0xbb],
  ];
  function swoBlocks(out) {
    SWO.forEach(function (r, i) {
      const b = fill(keep(Memory.alloc(0x20)), 0x20, 0x5c);
      marker(b, 0x20, 0x4b000000 + 0x100 * i);
      b.add(4).writeU8(r[0]);
      u32(b, 8, r[1]);
      u32(b, 0xc, 0x0ad00000 + r[2] * 0x101);
      out['o' + i] = b;
    });
  }
  F.c3dt_swodtor = function () {
    const out = {};
    swoBlocks(out);
    out.implog = __importStandIn({ module: 'sound.dll', imports: {
      'USER32.dll!SetWindowLongA': { args: 3, impl: function (a) { return (0xca110000 + (a[2].toUInt32() & 0xffff) * 0x1001) >>> 0; } } } }).log;
    return out;
  };
  F.c3dt_swodestroy = function () {
    const out = F.c3dt_swodtor();
    out.freelog = __heapStandIn({ module: 'sound.dll', free: [0x4249a] }).log;
    return out;
  };

  // ===================================================================================================================
  // D. forwarders that test a module global
  // ===================================================================================================================
  // A recorded call from a fixture callback (virtual slot, COM method): the same 0x20-byte entry stubOn writes
  function logCall(log, tag, a) {
    const n = log.readU32();
    if (n < 15) {
      const p = log.add(4 + 0x20 * n);
      p.writeU32(tag >>> 0);
      for (let i = 0; i < a.length && i < 7; i++) p.add(4 + 4 * i).writeU32(a[i].toUInt32());
    }
    log.writeU32(n + 1);
  }
  function devBlock(i, tag) {
    const b = fill(keep(Memory.alloc(0x200)), 0x200, 0x3b);
    marker(b, 0x200, (tag || 0x6e000000) + 0x1000 * i);
    return b;
  }
  const ARGS12 = [0, 1, 0x1234, 0xffffffff, 0x80000000, 0x5555, 2, 0x100, 0xdeadbeef, 0x41, 7, 0x7fffffff];

  // Wave_Device::caps: g_waveDevice 0x100b49f0 swapped (null, or a device block); thunk 0x10001c26 returns the dword at this+8
  function capsFixture(withDev) {
    const out = {};
    quietTimer(out);
    if (withDev) { out.dev = devBlock(0); swapGlobal(0xb49f0, out.dev.toUInt32(), out, 'gdev'); }
    else swapGlobal(0xb49f0, 0, out, 'gdev');
    out.log = stubLog();
    stubOn(out.log, 0x1c26, 0, true, function (a) { return a[0].add(8).readU32(); });
    return out;
  }
  F.c3dt_caps_null = function () { return capsFixture(false); };
  F.c3dt_caps_dev = function () { return capsFixture(true); };
  // Wave_Device::deviceName (a1, a2 = name buffer, a3): thunk 0x1000222f returns a1 + a3 and writes one byte of a2
  function nameFixture(withDev) {
    const out = {};
    quietTimer(out);
    for (let i = 0; i < 12; i++) out['b' + i] = fill(keep(Memory.alloc(0x20)), 0x20, 0x41 + i);
    if (withDev) { out.dev = devBlock(0); swapGlobal(0xb49f0, out.dev.toUInt32(), out, 'gdev'); }
    else swapGlobal(0xb49f0, 0, out, 'gdev');
    out.log = stubLog();
    stubOn(out.log, 0x222f, 3, true, function (a) { a[2].writeU8(0x6e); return (a[1].toUInt32() + a[3].toUInt32()) >>> 0; });
    return out;
  }
  F.c3dt_name_null = function () { return nameFixture(false); };
  F.c3dt_name_dev = function () { return nameFixture(true); };
  // serviceWaveDevice: the flag byte at device+0x6c selects the call (bit 4)
  function svcFixture(flags) {
    const out = {};
    quietTimer(out);
    if (flags === null) swapGlobal(0xb49f0, 0, out, 'gdev');
    else { out.dev = devBlock(0); u32(out.dev, 0x6c, flags); swapGlobal(0xb49f0, out.dev.toUInt32(), out, 'gdev'); }
    out.log = stubLog();
    stubOn(out.log, 0x1672, 1, true, function (a) { return (a[1].toUInt32() + 0x40) >>> 0; });
    return out;
  }
  F.c3dt_svc_null = function () { return svcFixture(null); };
  F.c3dt_svc_clear = function () { return svcFixture(0xffffffef); };
  F.c3dt_svc_set = function () { return svcFixture(0x10); };
  F.c3dt_svc_setall = function () { return svcFixture(0xffffffff); };
  // Wave_Device::setCooperativeLevel: thunk 0x100010a0 returns arg ^ 0x77
  function coopFixture(withDev) {
    const out = {};
    quietTimer(out);
    if (withDev) { out.dev = devBlock(0); swapGlobal(0xb49f0, out.dev.toUInt32(), out, 'gdev'); }
    else swapGlobal(0xb49f0, 0, out, 'gdev');
    out.log = stubLog();
    stubOn(out.log, 0x10a0, 1, true, function (a) { return (a[1].toUInt32() ^ 0x77) >>> 0; });
    return out;
  }
  F.c3dt_coop_null = function () { return coopFixture(false); };
  F.c3dt_coop_dev = function () { return coopFixture(true); };
  // DirectSoundDevice::setCooperativeLevel: device blocks (0x1a0 bytes): +0 IDirectSound pointer (NULL or a COM object whose vtable slot +0x18
  // is a recording stdcall callback returning its third argument, the level), +0x190 the level. Thunks 0x1000268a (cdecl) and 0x1000104b are stubs.
  const DSROWS = [null, 0, 0, 1, 2, 0x80004005, 0xffffffff, 0, 3, null, 0x7fffffff, 0x10];
  F.c3dt_dscoop = function () {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    const vt = fill(keep(Memory.alloc(0x40)), 0x40, 0);
    const com = keep(new NativeCallback(function (self, a, lvl) {
      logCall(out.log, 0x18, [self, a, lvl]);
      return lvl.toUInt32() >>> 0;
    }, 'uint32', ['pointer', 'pointer', 'pointer'], 'stdcall'));
    vt.add(0x18).writePointer(com);
    DSROWS.forEach(function (lvl, i) {
      const b = devBlock(i, 0x5d000000);
      if (lvl === null) { b.writePointer(ptr(0)); u32(b, 0x190, 0x190 + i); }
      else {
        const o = fill(keep(Memory.alloc(0x20)), 0x20, 0x66);
        o.writePointer(vt);
        b.writePointer(o);
        u32(b, 0x190, lvl);
      }
      out['d' + i] = b;
    });
    out.gcoop = snd(0xb4a3c);
    __freeze.setU32(out.gcoop, 0x1234abcd);
    stubOn(out.log, 0x268a, 1, 'cdecl', function (a) { return 0x55; });
    stubOn(out.log, 0x104b, 0, true, function (a) { return 0; });
    return out;
  };
  // Wave_In_Device::op1 / op2 / WaveInStream_start: g_waveIn 0x100b4a04 and the stream word 0x100b4a24 swapped
  function wiOpFixture(which, withDev) {
    const out = {};
    quietTimer(out);
    const rva = which === 'stream' ? 0xb4a24 : 0xb4a04;
    if (withDev) { out.dev = devBlock(1); swapGlobal(rva, out.dev.toUInt32(), out, 'gdev'); }
    else swapGlobal(rva, 0, out, 'gdev');
    out.log = stubLog();
    if (which === 'op1') stubOn(out.log, 0x177b, 1, true, function (a) { return (a[1].toUInt32() + 0x11) >>> 0; });
    if (which === 'op2') stubOn(out.log, 0x1253, 0, true, function (a) { return a[0].add(8).readU32(); });
    if (which === 'stream') {
      stubOn(out.log, 0x15b4, 1, true, function (a) { return (a[1].toUInt32() & 0xffff) + 5; });
      for (let i = 0; i < 12; i++) out['t' + i] = devBlock(i, 0x7a000000);
    }
    return out;
  }
  F.c3dt_op1_null = function () { return wiOpFixture('op1', false); };
  F.c3dt_op1_dev = function () { return wiOpFixture('op1', true); };
  F.c3dt_op2_null = function () { return wiOpFixture('op2', false); };
  F.c3dt_op2_dev = function () { return wiOpFixture('op2', true); };
  F.c3dt_wstream_null = function () { return wiOpFixture('stream', false); };
  F.c3dt_wstream_dev = function () { return wiOpFixture('stream', true); };
  // Wave_In_Device::stop: g_waveIn swapped for a wave-in device block; the chain runs WaveInDevice_reset and WaveInDevice_close with the winmm
  // and KERNEL32 critical-section imports stood in. Header blocks: first dword 0xbad makes waveInUnprepareHeader fail (33).
  function stopFixture(kind) {
    const out = {};
    quietTimer(out);
    const o = kind === 'null' ? null : (
      kind === 'noh' ? { h: 0, hd: [] } : kind === 'ok' ? { hd: [] } : kind === 'hdr' ? { hd: [1, 2] } :
      kind === 'hdrbad' ? { hd: [1, 0xbad, 3] } : { h: 0x70000005, hd: [1] });
    if (o === null) swapGlobal(0xb4a04, 0, out, 'gdev');
    else { out.w = wiBlock(0, o); swapGlobal(0xb4a04, out.w.toUInt32(), out, 'gdev'); }
    out.implog = __importStandIn({ module: 'sound.dll', imports: {
      'KERNEL32.dll!EnterCriticalSection': { ret: 0 }, 'KERNEL32.dll!LeaveCriticalSection': { ret: 0 },
      'WINMM.dll!waveInStop': { args: 1, ret: 0 }, 'WINMM.dll!waveInReset': { args: 1, ret: 0 },
      'WINMM.dll!waveInUnprepareHeader': { args: 3, impl: function (a) { return a[1].readU32() === 0xbad ? 33 : 0; } },
      'WINMM.dll!waveInClose': { args: 1, impl: function (a) { const h = a[0].toUInt32(); return h >= 0x70000000 ? (h & 0xffff) : 0; } } } }).log;
    return out;
  }
  F.c3dt_stop_null = function () { return stopFixture('null'); };
  F.c3dt_stop_noh = function () { return stopFixture('noh'); };
  F.c3dt_stop_ok = function () { return stopFixture('ok'); };
  F.c3dt_stop_hdr = function () { return stopFixture('hdr'); };
  F.c3dt_stop_hdrbad = function () { return stopFixture('hdrbad'); };
  F.c3dt_stop_err = function () { return stopFixture('err'); };

  // ===================================================================================================================
  // E. small channel / sequencer / list bodies
  // ===================================================================================================================
  // Voice_stopAndFlag: objects of 0x80 bytes (markers; the dword at +0x58 is the flag word); thunk 0x10001efb is a recording stub
  F.c3dt_vstop = function () {
    const out = {};
    for (let i = 0; i < 12; i++) {
      const b = devBlock(i, 0x4a000000);
      u32(b, 0x58, [0, 8, 0xffffffff, 0xfffffff7, 0x7, 0x80000000, 0x100, 0xf, 0x1, 0x55555555, 0xaaaaaaaa, 0x78][i]);
      out['v' + i] = b;
    }
    out.log = stubLog();
    stubOn(out.log, 0x1efb, 1, true, function (a) { return a[1].toUInt32(); });
    return out;
  };
  // SoundGroup_rewind: objects whose dword +0 points at a vtable block whose slot +0x40 is a recording thiscall(1 argument) callback
  F.c3dt_grewind = function () {
    const out = {};
    out.log = stubLog();
    const vt = fill(keep(Memory.alloc(0x80)), 0x80, 0);
    vt.add(0x40).writePointer(keep(new NativeCallback(function (self, a) {
      logCall(out.log, 0x40, [self, a]);
      return 0x1357;
    }, 'uint32', ['pointer', 'pointer'], 'thiscall')));
    for (let i = 0; i < 12; i++) {
      const b = devBlock(i, 0x3c000000);
      b.writePointer(vt);
      u32(b, 4, [0, 1, 0xffffffff, 0x80000000, 0x1234, 7, 0x100, 0x55, 2, 3, 0x7fffffff, 0xdead][i]);
      out['g' + i] = b;
    }
    stubOn(out.log, 0x1de8, 0, true, function (a) { return 0; });
    return out;
  };
  // Channel_serviceBuffer: rows [buffer: 0 none / 1 idle (dword +8 = 0) / 2 busy, flag word]
  const CSROWS = [[0, 0], [0, 8], [0, 0xffffffff], [1, 0], [1, 8], [1, 0xffffffe7], [2, 0], [2, 8], [2, 0xf8], [1, 0x10], [2, 0x18], [0, 0x10]];
  F.c3dt_chansvc = function () {
    const out = {};
    quietTimer(out);
    CSROWS.forEach(function (r, i) {
      const b = devBlock(i, 0x2b000000);
      if (r[0] === 0) b.add(0x54).writePointer(ptr(0));
      else {
        const buf = fill(keep(Memory.alloc(0x20)), 0x20, 0x66);
        u32(buf, 8, r[0] === 1 ? 0 : 0x1000 + i);
        b.add(0x54).writePointer(buf);
        out['u' + i] = buf;
      }
      u32(b, 0x58, r[1]);
      out['c' + i] = b;
    });
    out.log = stubLog();
    stubOn(out.log, 0x1b9f, 0, true, function (a) { return a[0].add(8).readU32(); });
    stubOn(out.log, 0x2716, 0, true, function (a) { return (a[0].add(0x20).readU32() ^ 0x1234) >>> 0; });
    return out;
  };
  // MultiSound_enqueueCmd3 / release: g_soundEngine 0x100b4a20 swapped (a fixture block or NULL); objects whose vtable slot +0x60 returns the dword at this+8
  function msFixture(kind, eng) {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    const vt = fill(keep(Memory.alloc(0x80)), 0x80, 0);
    vt.add(0x60).writePointer(keep(new NativeCallback(function (self) {
      logCall(out.log, 0x60, [self]);
      return self.add(8).readU32();
    }, 'uint32', ['pointer'], 'thiscall')));
    for (let i = 0; i < 12; i++) {
      const b = devBlock(i, 0x1e000000);
      b.writePointer(vt);
      u32(b, 8, [0, 1, 0xffffffff, 0, 0x80000000, 5, 0, 7, 0x100, 0, 0x55, 2][i]);
      b.add(0xcc).writeU8([0, 4, 0xff, 0xfb, 4, 1, 0x04, 0, 0x84, 2, 0x44, 0xfc][i]);
      out['m' + i] = b;
    }
    if (eng) { out.eng = devBlock(7, 0x1f000000); swapGlobal(0xb4a20, out.eng.toUInt32(), out, 'gdev'); }
    else swapGlobal(0xb4a20, 0, out, 'gdev');
    if (kind === 'enq') stubOn(out.log, 0x20b3, 2, true, function (a) { return ((a[2].toUInt32() & 0xffff) + 1) >>> 0; });
    else {
      stubOn(out.log, 0x20b3, 2, true, function (a) { return 0x2222; });
      stubOn(out.log, 0x1bf4, 0, true, function (a) { return a[0].add(4).readU32(); });
    }
    return out;
  }
  F.c3dt_enq_eng = function () { return msFixture('enq', true); };
  F.c3dt_enq_null = function () { return msFixture('enq', false); };
  F.c3dt_msrel = function () { return msFixture('rel', true); };
  // ListValues_callSlot18: one 0x200-byte block per row holds the list header at +0 (head +0xc, cursor +0x14), nodes {+0 prev, +4 next, +8 value}
  // at +0x40 + 0x10 j and value objects {+0 vtable, +4 action, +8 the header, +0xc a node} at +0x100 + 0x20 j. The vtable's slot +0x18 logs the value
  // object and acts: 1 clears the header's cursor, 2 sets it to the node named at +0xc.
  const LROWS = [
    { n: 0 }, { n: 1, nov: [0] }, { n: 1 }, { n: 3 }, { n: 3, nov: [1] }, { n: 3, act: { 0: 1 } }, { n: 2, act: { 0: 2 }, tgt: 0 },
    { n: 3, act: { 0: 2 }, tgt: 2 }, { n: 3, nov: [2] }, { n: 2, nov: [1] }, { n: 3, act: { 1: 1 } }, { n: 1, act: { 0: 2 }, tgt: 0 },
  ];
  F.c3dt_listcall = function () {
    const out = {};
    out.log = stubLog();
    const vt = fill(keep(Memory.alloc(0x40)), 0x40, 0);
    vt.add(0x18).writePointer(keep(new NativeCallback(function (self) {
      logCall(out.log, 0x18, [self]);
      const act = self.add(4).readU32();
      const hdr = self.add(8).readPointer();
      if (act === 1) hdr.add(0x14).writePointer(ptr(0));
      else if (act === 2) hdr.add(0x14).writePointer(self.add(0xc).readPointer());
      return 0x99;
    }, 'uint32', ['pointer'], 'thiscall')));
    LROWS.forEach(function (r, i) {
      const b = fill(keep(Memory.alloc(0x200)), 0x200, 0x2d);
      marker(b, 0x40, 0x1a000000 + 0x1000 * i);
      u32(b, 0x14, 0x14141414);
      const node = (j) => b.add(0x40 + 0x10 * j), obj = (j) => b.add(0x100 + 0x20 * j);
      for (let j = 0; j < r.n; j++) {
        node(j).writePointer(j > 0 ? node(j - 1) : ptr(0));
        node(j).add(4).writePointer(j < r.n - 1 ? node(j + 1) : ptr(0));
        const noVal = r.nov && r.nov.indexOf(j) >= 0;
        node(j).add(8).writePointer(noVal ? ptr(0) : obj(j));
        obj(j).writePointer(vt);
        u32(obj(j), 4, (r.act && r.act[j]) || 0);
        obj(j).add(8).writePointer(b);
        obj(j).add(0xc).writePointer(r.tgt !== undefined ? node(r.tgt) : ptr(0));
        u32(obj(j), 0x10, 0x3a3a0000 + j);
      }
      b.add(0xc).writePointer(r.n ? node(0) : ptr(0));
      out['l' + i] = b;
    });
    return out;
  };
  // Seq_setPlayOrderMode: sequencer blocks of 0x240 bytes; thunk 0x10001bcc (Seq_buildPlayOrder) is a recording stub
  F.c3dt_seqorder = function () {
    const out = {};
    for (let i = 0; i < 12; i++) {
      const b = fill(keep(Memory.alloc(0x240)), 0x240, 0x57);
      marker(b, 0x240, 0x6c000000 + 0x1000 * i);
      u32(b, 0x214, [0, 0xffffffff, 0x180, 0x7f, 0x100, 0xfffffe7f, 0x55555555, 0x80, 0xaaaaaaaa, 0x1, 0x200, 0x300][i]);
      u32(b, 0x238, 0x2380000 + i);
      out['q' + i] = b;
    }
    out.log = stubLog();
    stubOn(out.log, 0x1bcc, 1, true, function (a) { return a[1].toUInt32(); });
    return out;
  };

  // ===================================================================================================================
  // F. parser forwarders and the MIDI output switch
  // ===================================================================================================================
  // Sound5c450::openMmio: thunk 0x1000259f (openMmioSound) is a stub returning arg ^ the dword at this+8
  F.c3dt_openmmio = function () {
    const out = {};
    for (let i = 0; i < 12; i++) out['o' + i] = devBlock(i, 0x5c000000);
    out.log = stubLog();
    stubOn(out.log, 0x259f, 1, true, function (a) { return (a[1].toUInt32() ^ a[0].add(8).readU32()) >>> 0; });
    return out;
  };
  // Channel_parseWaveHeader / adjPlus70: the stub (thunk 0x10001e51, parseRiffWave) returns the flag dword at ecx+0x58 AFTER the bit-1 clear
  const PFLAGS = [0, 2, 3, 0xffffffff, 0x80000002, 0x100, 0x102, 0xfffffffd, 0x55555555, 0xaaaaaaaa, 0x7, 0x4];
  function parseFixture(adj) {
    const out = {};
    for (let i = 0; i < 12; i++) {
      const b = devBlock(i, 0x5b000000);
      u32(b, adj ? 0x70 + 0x58 : 0x58, PFLAGS[i]);
      out['p' + i] = b;
    }
    out.log = stubLog();
    stubOn(out.log, 0x1e51, 1, true, function (a) { return a[0].add(0x58).readU32(); });
    return out;
  }
  F.c3dt_parsehdr = function () { return parseFixture(false); };
  F.c3dt_parseadj = function () { return parseFixture(true); };
  // serviceDecodeRing: thunk 0x10002022 (fillDecodeRing) stub
  F.c3dt_decring = function () {
    const out = {};
    [0, 2, 3, 0x22, 0x23, 0x21, 0x20, 0xffffffff, 0x82, 0x42, 1, 0xfffffffe].forEach(function (f, i) {
      const b = devBlock(i, 0x4d000000);
      u32(b, 0x58, f);
      out['r' + i] = b;
    });
    out.log = stubLog();
    stubOn(out.log, 0x2022, 0, true, function (a) { return 0x77; });
    return out;
  };
  // Midi_Device::enableOutput: objects with a vtable whose slot +0x4c is a recording thiscall callback; the module byte 0x100b49bc is saved and
  // preset; thunk 0x10001997 (pumpWaveMessages, ecx = 0x100b49b8) is a stub
  F.c3dt_midien = function () {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    const vt = fill(keep(Memory.alloc(0x80)), 0x80, 0);
    vt.add(0x4c).writePointer(keep(new NativeCallback(function (self) {
      logCall(out.log, 0x4c, [self]);
      return 0x31;
    }, 'uint32', ['pointer'], 'thiscall')));
    [0, 1, 2, 0xfffffffe, 0xffffffff, 0x100, 0x101, 0x80, 0x81, 0xfe, 0x40000000, 0x7].forEach(function (f, i) {
      const b = devBlock(i, 0x4c000000);
      b.writePointer(vt);
      u32(b, 0x24, f);
      out['d' + i] = b;
    });
    out.gmidi = snd(0xb49bc);
    __freeze.setU32(out.gmidi, 0x40);
    stubOn(out.log, 0x1997, 0, true, function (a) { return 0; });
    return out;
  };

  // ===================================================================================================================
  // G. the idle-sound reaper and the CRT lock wrappers
  // ===================================================================================================================
  // Sound_reapIdle: twelve scenarios, one key each. The module words 0x100b4fe8 (head), 0x100b4fec (tail), 0x100b4ff0 (cursor) and 0x100b4ff4 (count)
  // are saved and set for the key (first thread frozen, timer quiet). A scenario block (0x1000 bytes) holds nodes {+0 prev, +4 next, +8 sound} at
  // +0x100 + 0x10 j and sound objects (0x240 bytes: +0x210 the signed counter, +0x214 the busy flag the stub of thunk 0x10001942 returns) at +0x400 + 0x240 j.
  // Rows per sound: [counter, busy]; 'x' = a node whose sound pointer is NULL.
  const REAP = [
    [],                                                                // empty list
    ['x'],                                                             // sound pointer NULL: the node is the result
    [[5, 0]],                                                          // counter above 0: kept, no next node
    [[0, 1]],                                                          // busy: kept
    [[0, 0]],                                                          // reaped, list empties
    [[0, 0], [0, 0], [0, 0]],                                          // all three reaped in turn
    [[7, 0], [0, 0], [3, 1]],                                          // keep, reap (middle), keep
    [[0, 0], [2, 1], [-4, 0]],                                         // reap head, keep, reap tail (negative counter)
    [[1, 0], [0, 9], [100, 0]],                                        // all kept
    [[1, 0], 'x', [0, 0]],                                             // kept, then a NULL sound ends the walk
    [[0, 0], 'x'],                                                     // reap, then a NULL sound
    [[-1, 0]],                                                         // negative counter: reaped
    [[0, 0, 1]],                                                       // the busy-test callee clears the cursor word: reaped without unlinking, walk ends
    [[0, 1, 1]],                                                       // the same with a busy sound: kept, cursor NULL, walk ends
  ];
  function reapFixture(i) {
    const out = {};
    quietTimer(out);
    const b = fill(keep(Memory.alloc(0x1000)), 0x1000, 0x3a);
    marker(b, 0x100, 0x6a000000 + 0x100 * i);
    const rows = REAP[i];
    const node = (j) => b.add(0x100 + 0x10 * j), sobj = (j) => b.add(0x400 + 0x240 * j);
    rows.forEach(function (r, j) {
      node(j).writePointer(j > 0 ? node(j - 1) : ptr(0));
      node(j).add(4).writePointer(j < rows.length - 1 ? node(j + 1) : ptr(0));
      if (r === 'x') node(j).add(8).writePointer(ptr(0));
      else {
        node(j).add(8).writePointer(sobj(j));
        marker(sobj(j), 0x240, 0x6b000000 + 0x1000 * j);
        u32(sobj(j), 0x210, r[0]);
        u32(sobj(j), 0x214, r[1]);
        u32(sobj(j), 0x218, r[2] || 0);
      }
    });
    out.s = b;
    out.glist = snd(0xb4fe8);
    __freeze.save(out.glist, 0x10);
    out.glist.writePointer(rows.length ? node(0) : ptr(0));                    // head
    out.glist.add(4).writePointer(rows.length ? node(rows.length - 1) : ptr(0));   // tail
    out.glist.add(8).writePointer(ptr(0x77770000));                            // cursor (overwritten by the function)
    out.glist.add(0xc).writeU32(rows.length);                                  // count
    out.freelog = __heapStandIn({ module: 'sound.dll', free: [0x4249a] }).log;
    out.log = stubLog();
    stubOn(out.log, 0x1942, 0, true, function (a) {
      if (a[0].add(0x218).readU32() !== 0) snd(0xb4ff0).writeU32(0);      // a scenario whose callee clears the walk cursor
      return a[0].add(0x214).readU32() & 0xff;
    });
    stubOn(out.log, 0x1d66, 0, true, function (a) { return 0; });
    return out;
  }
  for (let i = 0; i < REAP.length; i++) F['c3dt_reap_' + i] = function () { return reapFixture(i); };

  // the CRT lock wrappers: the import stand-in records the critical-section argument and returns arg ^ 0x5ca1ab1e
  function csFixture(api) {
    const out = {};
    const spec = {};
    // all four critical-section imports are stood in (each with its own result mask), so a wrapper that calls the wrong one reads RED instead of
    // reaching the real API with a fixture pointer
    ['InitializeCriticalSection', 'DeleteCriticalSection', 'EnterCriticalSection', 'LeaveCriticalSection'].forEach(function (n, i) {
      spec['KERNEL32.dll!' + n] = { args: 1, impl: function (a) { return (a[0].toUInt32() ^ (0x5ca1ab1e + 0x1111 * i)) >>> 0; } };
    });
    out.implog = __importStandIn({ module: 'sound.dll', imports: spec }).log;
    return out;
  }
  F.c3dt_cs_init = function () { return csFixture('InitializeCriticalSection'); };
  F.c3dt_cs_delete = function () { return csFixture('DeleteCriticalSection'); };
  F.c3dt_cs_enter = function () { return csFixture('EnterCriticalSection'); };
  F.c3dt_cs_leave = function () { return csFixture('LeaveCriticalSection'); };
  // _unlock and the funclets: LeaveCriticalSection stood in; the lock table at 0x10067400 holds 0 for unused locks and the live critical-section addresses
  // 0x100b5598 (1), 0x100b55c8 (9), 0x100b55e0 (13), 0x100b55b0 (17)
  F.c3dt_unlock = function () { return csFixture('LeaveCriticalSection'); };

  // ---- hooks for later groups are appended below -------------------------------------------------------------------
  F.c3dt_helpers = { guard: guard, quietTimer: quietTimer, swapGlobal: swapGlobal, stubOn: stubOn, stubLog: stubLog, wiBlock: wiBlock,
                     mmBlock: mmBlock, fill: fill, marker: marker, u32: u32, keep: keep, snd: snd };
})();
