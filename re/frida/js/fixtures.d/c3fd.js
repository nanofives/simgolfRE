// Fixtures for C3 batch c3fd (sound.dll, round 33). Layouts come from the disassembly cited in shim/src/re/c3fd.cpp. Every
// object, vtable and log here is allocated by the fixture; no live sound object, device, list, window or file is handed to an
// arm. Helpers copied from re/frida/js/fixtures.d/c3ex.js (freeze guard, quiet timer word, swapped module global, recording
// stub on an incremental-link thunk that only the A/B thread takes, recording vtables and objects). Module globals a fixture
// writes are saved first and restored by re/frida/js/freeze.js before the game's first thread is resumed. All values are
// synthetic.
//
// Recording objects: a block (0x200 bytes or larger) whose vtable slots are thiscall NativeCallbacks that log
// {0x5100 + slot, this, arguments} and return the dword at this + 0x100 + slot (the row's answer for that slot, inside the
// compared object). A block may carry a poke at +0x1e0..+0x1ef (target pointer, trigger tag, value, target offset): when the
// callback named by the trigger runs on that block, the value is written to target + offset.
(function () {
  const F = globalThis.DIFF_FIXTURES;
  const keep = function (x) { globalThis.__diffKeepAlive.push(x); return x; };
  const snd = function (rva) { return Process.getModuleByName('sound.dll').base.add(rva); };
  function fill(p, n, b) { p.writeByteArray(new Array(n).fill(b)); return p; }
  function marker(p, n, tag) { for (let k = 0; k < n; k += 4) p.add(k).writeU32((tag + k) >>> 0); }
  function u32(p, off, v) {
    if (v instanceof NativePointer) p.add(off).writePointer(v); else p.add(off).writeU32(v >>> 0);
  }
  function block(i, tag, n) {
    const sz = n || 0x200;
    const b = fill(keep(Memory.alloc(sz)), sz, 0x3b);
    marker(b, sz, (tag >>> 0) + 0x10000 * i);
    return b;
  }

  // ---- freezing and the multimedia timer (c3dt's method) --------------------------------------------------------
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
    const g = snd(0xb49a8);
    out.g49a8 = g;
    __freeze.setU32(g, 5);
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
  function poke(self, tag) {
    const t = self.add(0x1e0).readPointer();
    if (t.isNull()) return;
    const trig = self.add(0x1e4).readU32();
    if (trig !== (tag >>> 0) && trig !== 0xffffffff) return;
    t.add(self.add(0x1ec).readU32()).writeU32(self.add(0x1e8).readU32());
  }
  let stubLive = {};
  // mask: indices of arguments (0 = ecx for thiscall) that are the caller's stack slots, logged as 0x5ac0
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
  function recVtable(log, slots) {
    const vt = fill(keep(Memory.alloc(0x100)), 0x100, 0);
    Object.keys(slots).forEach(function (k) {
      const off = parseInt(k, 10), n = slots[k];
      const argt = ['pointer'].concat(Array(n).fill('uint32'));
      vt.add(off).writePointer(keep(new NativeCallback(function () {
        const a = Array.prototype.slice.call(arguments);
        logCall(log, 0x5100 + off, a);
        poke(a[0], 0x5100 + off);
        return a[0].add(0x100 + off).readU32();
      }, 'uint32', argt, 'thiscall')));
    });
    return vt;
  }
  function recObj(i, tag, vt, cfg, size) {
    const b = block(i, tag, size);
    if (vt) {
      b.writePointer(vt);
      fill(b.add(0x100), 0xe0, 0);   // unset slot answers are 0
    }
    Object.keys(cfg || {}).forEach(function (k) { u32(b, parseInt(k, 10), cfg[k]); });
    return b;
  }
  function pokeCfg(target, trig, value, off) { const c = {}; c[0x1e0] = target; c[0x1e4] = trig; c[0x1e8] = value; c[0x1ec] = off; return c; }
  function merge() { const o = {}; for (let i = 0; i < arguments.length; i++) Object.assign(o, arguments[i]); return o; }
  function heap(out, size, opts) {
    const h = __heapStandIn(Object.assign({ module: 'sound.dll', alloc: [0x424d7], free: [0x4249a], arenaSize: size || 0x400 },
                                          opts || {}));
    out.arena = h.arena;
    out.freelog = h.log;
    return h;
  }

  // ===================================================================================================================
  // A. GetProcAddress exports
  // ===================================================================================================================
  // Dll_*::delete_device: the global (g_midiDevice 0x100b4a1c, 0x100b4a20, 0x100b4a24) swapped for the key. Variant 0 leaves it
  // NULL; variants 1..9 point it at a recording object whose slot 0 (the scalar deleting destructor, one argument) answers a
  // per-variant value; variants 3 and 7 poke the global itself from inside slot 0 (the store of 0 after the call still wins),
  // variants 5 and 9 poke the object's own +0x40.
  [[0xb4a1c, 'midi'], [0xb4a20, 'wave'], [0xb4a24, 'wavein']].forEach(function (g) {
    for (let v = 0; v < 10; v++) {
      F['c3fd_del_' + g[1] + '_v' + v] = (function (rva, v) {
        return function () {
          const out = {};
          quietTimer(out);
          out.log = stubLog();
          const vt = recVtable(out.log, { 0: 1 });
          const cfg = {};
          cfg[0x100] = (0x11 * v) >>> 0;
          if (v === 3 || v === 7) Object.assign(cfg, pokeCfg(snd(rva), 0x5100, 0x7a7a0000 + v, 0));
          out.o = recObj(v, 0xde000000, vt, cfg);
          if (v === 5 || v === 9) Object.assign(cfg, pokeCfg(out.o, 0x5100, 0x40400000 + v, 0x40));
          Object.keys(cfg).forEach(function (k) { u32(out.o, parseInt(k, 10), cfg[k]); });
          swapGlobal(rva, v === 0 ? 0 : out.o, out, 'g');
          return out;
        };
      })(g[0], v);
    }
  });

  // init_sound_timer: 0x100b49d8 and g_soundTimer 0x100b49ec swapped (prior marker words), operator new 0x100424d7 carved from a
  // fixture arena, SoundTimer::init's thunk 0x100022b1 a recording stub that marks the block (+0 = 0x7173) and returns it
  // (`_alt`: returns the fixture block out.alt instead, so the +0x24 store lands there).
  function itimer(alt) {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    swapGlobal(0xb49d8, 0x5a5a5a5a, out, 'gfl');
    swapGlobal(0xb49ec, 0x5b5b5b5b, out, 'gtm');
    out.alt = block(1, 0xa1000000, 0x40);
    heap(out, 0x200);
    stubOn(out.log, 0x22b1, 0, true, function (a) {
      a[0].writeU32(0x7173);
      return alt ? out.alt : a[0];
    });
    return out;
  }
  F.c3fd_itimer = function () { return itimer(false); };
  F.c3fd_itimer_alt = function () { return itimer(true); };

  // Dll_Wave_Device::create_device / Dll_Wave_In_Device::create_device: the device global swapped (`busy`: a fixture object;
  // otherwise NULL), the out slots s0..s11 hold prior markers, operator new carved from the arena (`oom`: every allocation fails
  // through the fail mask), the constructor's thunk a recording stub.
  function createDev(rva, ctorThunk, size, mode) {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    out.dev = block(2, 0xd0000000, 0x40);
    out.alt = block(3, 0xa2000000, 0x80);
    for (let i = 0; i < 12; i++) out['s' + i] = block(i, 0x50000000, 0x10);
    swapGlobal(rva, mode === 'busy' ? out.dev : 0, out, 'g');
    heap(out, size + 0x100, mode === 'oom' ? { failMask: 0xffffffff } : {});
    stubOn(out.log, ctorThunk, 0, true, function (a) {
      for (let k = 4; k < 0x48; k += 4) a[0].add(k).writeU32(0xc7000000 + k);
      return mode === 'alt' ? out.alt : (ctorThunk === 0x2761 ? 0x1234 : a[0]);
    });
    return out;
  }
  ['busy', 'ok', 'alt', 'oom'].forEach(function (m) {
    F['c3fd_wcreate_' + m] = function () { return createDev(0xb4a20, 0x1f82, 0x41e0, m); };
    F['c3fd_wicreate_' + m] = function () { return createDev(0xb4a24, 0x2761, 0x4c, m); };
  });

  // crtValidatePointer (c3cm's hook, released from re/c3_hold.tsv by this key): KERNEL32!IsBadWritePtr stood in at sound.dll's
  // IAT. Its answer mixes both arguments (bit 4 of the pointer xor bit 0 of the length), so both results occur; the pointers
  // are plain numbers the stand-in never dereferences, and every call (pointer, length) is compared in the import log.
  F.c3fd_valid = function () {
    const out = {};
    guard();                       // freezes the first thread and arms the negative-control switch
    out.implog = __importStandIn({ module: 'sound.dll', imports: {
      'KERNEL32.dll!IsBadWritePtr': { args: 2, impl: function (a) { return ((a[0].toUInt32() >>> 4) ^ a[1].toUInt32()) & 1; } } } }).log;
    return out;
  };

  // ===================================================================================================================
  // B. MultiSound constructors / destructor, Seq_dtor
  // ===================================================================================================================
  // Constructors: twelve 0x1500-byte blocks with marker words. SubChannel_ctor's thunk 0x10001465 is a recording stub that writes
  // the fields the MultiSound constructors later overwrite or leave (+0x1420, +0x142c, +0x1430, +0x143c, +0x1ec), the channel
  // constructor's thunk 0x10001005 marks this+0x70, Sound_setSourceType's thunk 0x100018a2 is recorded.
  F.c3fd_msctor = function () {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    for (let i = 0; i < 12; i++) out['o' + i] = block(i, 0x39000000, 0x1500);
    stubOn(out.log, 0x1465, 0, true, function (a) {
      [0x1420, 0x142c, 0x1430, 0x143c, 0x1ec, 0x1438].forEach(function (off, k) { a[0].add(off).writeU32(0x5c000000 + 0x100 * k + off); });
      return a[0];
    });
    stubOn(out.log, 0x1005, 0, true, function (a) { a[0].writeU32(0x70707070); return a[0]; });
    stubOn(out.log, 0x18a2, 1, true, function (a) { return (a[1].toUInt32() ^ 0x55) >>> 0; });
    return out;
  };

  // MultiSound_dtor: groups with count +0x1438 (rows: 0, 1, 2, 3, 4, 5, -1, 2, 3, 1, 6, 2), members at +0x1440 (recording objects,
  // some NULL), bit 1 of +0xc8, +0x1430 a fixture string or NULL. Channel_releaseBuffers 0x10001d11, Channel_closeAndFree
  // 0x100011b8 and Ch5bb90_dtor 0x10002108 are recording stubs; operator delete records.
  const MSD = [
    { n: 0, f: 0, s: 0 }, { n: 1, f: 2, s: 1 }, { n: 2, f: 0, s: 1, m: [1] }, { n: 3, f: 2, s: 0, m: [1, 0] },
    { n: 4, f: 0xff, s: 1, m: [1, 1, 1] }, { n: 5, f: 0xfd, s: 0, m: [0, 1, 0, 1] }, { n: -1, f: 2, s: 1 },
    { n: 2, f: 0x02, s: 0, m: [0] }, { n: 3, f: 0x40, s: 1, m: [1, 1] }, { n: 1, f: 0, s: 0 },
    { n: 6, f: 0x22, s: 1, m: [1, 1, 0, 1, 1] }, { n: 2, f: 3, s: 1, m: [1] },
  ];
  F.c3fd_msdtor = function () {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    const vt = recVtable(out.log, { 0: 1 });
    heap(out, 0x100);
    MSD.forEach(function (r, i) {
      const o = block(i, 0x39800000, 0x1500);
      u32(o, 0x1438, r.n);
      o.add(0xc8).writeU8(r.f);
      const s = block(i, 0x14300000, 0x20);
      out['str' + i] = s;
      u32(o, 0x1430, r.s ? s : 0);
      (r.m || []).forEach(function (has, j) {
        const m = recObj(j, 0x3a000000 + 0x100000 * i, vt, { 0x100: 0x100 + j });
        out['m' + i + '_' + j] = m;
        u32(o, 0x1440 + 4 * j, has ? m : 0);
      });
      out['o' + i] = o;
    });
    stubOn(out.log, 0x1d11, 0, true, function () { return 0x1d11; });
    stubOn(out.log, 0x11b8, 0, true, function () { return 0x11b8; });
    stubOn(out.log, 0x2108, 0, true, function () { return 0x2108; });
    return out;
  };

  // Seq_dtor: objects with the flag byte +0x4c (bit 0 runs Midi_Device::closeFile, its thunk 0x10001014 a recording stub) and a
  // node list head +0x19c / tail +0x1a0 / count +0x1a8; nodes are {prev, next, payload} blocks; a payload 0 ends the teardown.
  const SQD = [
    { f: 0, p: [] }, { f: 1, p: [] }, { f: 0, p: [5] }, { f: 1, p: [5, 6] }, { f: 0, p: [5, 0, 7] }, { f: 3, p: [0, 5] },
    { f: 0xfe, p: [1, 2, 3, 4] }, { f: 1, p: [9, 9, 0] }, { f: 0, p: [0] }, { f: 0x81, p: [1, 2, 3, 4, 5] },
    { f: 0, p: [7, 8] }, { f: 1, p: [3, 0, 0] },
  ];
  F.c3fd_seqdtor = function () {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    heap(out, 0x100);
    SQD.forEach(function (r, i) {
      const o = block(i, 0x1e000000, 0x200);
      o.add(0x4c).writeU8(r.f);
      const nodes = r.p.map(function (pl, j) { return block(j, 0x1e800000 + 0x100000 * i, 0x10); });
      nodes.forEach(function (n, j) {
        u32(n, 0, j ? nodes[j - 1] : 0);
        u32(n, 4, j + 1 < nodes.length ? nodes[j + 1] : 0);
        u32(n, 8, r.p[j] ? (0x9a000000 + 0x10 * i + j) : 0);
        out['n' + i + '_' + j] = n;
      });
      u32(o, 0x19c, nodes.length ? nodes[0] : 0);
      u32(o, 0x1a0, nodes.length ? nodes[nodes.length - 1] : 0);
      u32(o, 0x1a8, nodes.length + 0x100);
      out['o' + i] = o;
    });
    stubOn(out.log, 0x1014, 0, true, function () { return 0x1014; });
    return out;
  };

  // ===================================================================================================================
  // C. command queue, voice notification, sequencer and MIDI device rows
  // ===================================================================================================================
  // Wave_Device::flushCommandQueue: 0x4200-byte devices; the ring is the 0x1000 dwords at +0x1d8, head +0x41d8, tail +0x41dc.
  // CommandQueue::at 0x100088c0 and pairClear 0x10008840 run for real (pure: a masked ring read and two stores); entries are
  // fixture blocks {type, object}; type-4 objects are recording objects (slot 0, one argument) or NULL; operator delete records.
  const FQ = [
    { h: 5, t: 5, e: [] }, { h: 0, t: 1, e: [1] }, { h: 10, t: 14, e: [1, 2, 3, 4] }, { h: 0xffe, t: 3, e: [4, 0, 2, 5, 9] },
    { h: 7, t: 9, e: [0, 4] }, { h: 0x20, t: 0x26, e: [6, 1, -1, 4, 3, 0] }, { h: 0, t: 0xfff, e: [2, 3] },
    { h: 0x100, t: 0x103, e: [4, 4, 1] }, { h: 3, t: 2, e: [1, 1] }, { h: 0xfff, t: 0, e: [3] },
    { h: 1, t: 4, e: [0x80000001, 4, 2] }, { h: 9, t: 0xc, e: [4, 0, 0] },
    { h: 1, t: 0x1001, e: [1, 2] },        // head != tail but (tail - head) & 0xfff = 0: the walk is skipped (0x1000cca6)
  ];
  F.c3fd_flush = function () {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    const vt = recVtable(out.log, { 0: 1 });
    heap(out, 0x100);
    FQ.forEach(function (r, i) {
      const o = block(i, 0x0cc00000, 0x4200);
      fill(o.add(0x1d8), 0x4000, 0);
      u32(o, 0x41d8, r.h);
      u32(o, 0x41dc, r.t);
      r.e.forEach(function (ty, j) {
        const idx = (r.h + 1 + j) & 0xfff;
        if (ty === 0) return;                         // an empty ring slot
        const e = block(j, 0x0e000000 + 0x100000 * i, 0x10);
        u32(e, 0, ty);
        if ((ty >>> 0) === 4 && (j & 1) === 0) {
          const ob = recObj(j, 0x0f000000 + 0x100000 * i, vt, { 0x100: 0x40 + j });
          out['x' + i + '_' + j] = ob;
          u32(e, 4, ob);
        } else {
          u32(e, 4, (ty >>> 0) === 4 ? 0 : 0x44000000 + j);
        }
        out['e' + i + '_' + j] = e;
        u32(o, 0x1d8 + 4 * idx, e);
      });
      out['o' + i] = o;
    });
    return out;
  };

  // Voice_notifyChannelNotes: twelve 0x204-byte voices (slots +4 .. +0x200); a few slots per voice hold groups (0x1500 bytes:
  // count +0x1438, members +0x1440.., flag byte +0xc8). removeSoundNode's thunk 0x10002243 answers the offered node's +0x1f0,
  // SoundGroup_rewind 0x10002568 and postNotification 0x10001b3b are recorded; the device 0x100b4a20 is a fixture block.
  const NV = [
    [], [[0, 0, 0, 0]], [[3, 1, 1, 0]], [[0x7f, 2, 0, 1, [0, 1]]], [[5, 3, 0, 0, [1, 0, 1]], [6, 0, 1, 1]],
    [[1, 1, 1, 1], [0x40, 2, 0, 0, [1, 1]]], [[0, 4, 0, 0, [0, 0, 1, 1]]], [[2, -1, 1, 0]], [[9, 2, 0, 0, [0, 0]], [10, 2, 1, 1, [1, 1]]],
    [[0x7e, 3, 1, 0, [1, 1, 0]]], [[4, 1, 0, 1], [8, 1, 0, 0], [12, 1, 1, 0]], [[0, 5, 0, 1, [1, 0, 0, 1, 0]]],
  ];
  // a slot spec: [slot index, count, own flag bit 0, own removeSoundNode answer, member specs: bit 0 flag, bit 1 answer]
  F.c3fd_notify = function () {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    out.dev = block(0, 0xdd000000, 0x40);
    swapGlobal(0xb4a20, out.dev, out, 'gdev');
    NV.forEach(function (slots, i) {
      const v = block(i, 0x1ac00000, 0x204);
      fill(v.add(4), 0x200, 0);
      slots.forEach(function (s, k) {
        const g = block(k, 0x1ad00000 + 0x100000 * i, 0x1500);
        u32(g, 0x1438, s[1]);
        g.add(0xc8).writeU8(s[2] ? 0x81 : 0x80);
        u32(g, 0x1f0, s[3]);
        (s[4] || []).forEach(function (ms, j) {
          const m = block(j, 0x1ae00000 + 0x100000 * i + 0x10000 * k, 0x200);
          m.add(0xc8).writeU8(ms & 1 ? 0x41 : 0x40);
          u32(m, 0x1f0, ms & 2 ? 1 : 0);
          out['m' + i + '_' + k + '_' + j] = m;
          u32(g, 0x1440 + 4 * j, m);
        });
        out['g' + i + '_' + k] = g;
        u32(v, 4 + 4 * s[0], g);
      });
      out['v' + i] = v;
    });
    stubOn(out.log, 0x2243, 1, true, function (a) { return a[1].add(0x1f0).readU32(); });
    stubOn(out.log, 0x2568, 0, true, function () { return 0x2568; });
    stubOn(out.log, 0x1b3b, 2, true, function () { return 0x1b3b; });
    return out;
  };

  // Voice_allNotesOff: g_midiDevice 0x100b4a1c swapped for a fixture device whose +0x3c is non-zero (`_h`) or zero (`_n`), the
  // device 0x100b4a20 a fixture block (`_nodev`: NULL). 0xa00-byte voices: vtable slot +0x48 (one argument) recorded, +0x58 flag
  // byte, +0xac / +0x214 / +0x740 marker words, the list +0x1fc of {_, next, payload} nodes, 0x7f voice slots at +0x744 whose
  // +0x30 objects are notified through the thunk 0x100020f4 (a recording stub). Midi_Device::flushMessages's thunk 0x10001bbd is
  // a recording stub.
  const AO = [
    { f: 0, list: [], vs: [] }, { f: 2, list: [1], vs: [[0, 1]] }, { f: 0, list: [1, 1, 1], vs: [[3, 1], [7, 0]] },
    { f: 2, list: [1, 0, 1], vs: [[0x7e, 1]] }, { f: 0xff, list: [0], vs: [[1, 1], [2, 1], [0x40, 1]] }, { f: 0xfd, list: [1, 1], vs: [[5, 0]] },
    { f: 2, list: [], vs: [[9, 1]] }, { f: 0, list: [1, 1, 1, 1], vs: [] }, { f: 0x42, list: [1], vs: [[0x10, 1], [0x11, 0]] },
    { f: 0x40, list: [0, 1], vs: [[0x20, 1]] }, { f: 2, list: [1, 1], vs: [[0x30, 1]] }, { f: 0x10, list: [1], vs: [[0x7e, 1], [0, 1]] },
  ];
  function allOff(h, dev) {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    out.md = block(0, 0x3d000000, 0x40);
    u32(out.md, 0x3c, h ? 0x3c3c0001 : 0);
    swapGlobal(0xb4a1c, out.md, out, 'gmd');
    out.dev = block(0, 0xdd000000, 0x40);
    swapGlobal(0xb4a20, dev ? out.dev : 0, out, 'gdev');
    const vt = recVtable(out.log, { 0x48: 1 });
    AO.forEach(function (r, i) {
      const o = recObj(i, 0x20d00000, vt, {}, 0xa00);
      o.add(0x58).writeU8(r.f);
      fill(o.add(0x744), 0x7f * 4, 0);
      const nodes = r.list.map(function (pl, j) { return block(j, 0x20e00000 + 0x100000 * i, 0x10); });
      nodes.forEach(function (n, j) {
        u32(n, 4, j + 1 < nodes.length ? nodes[j + 1] : 0);
        u32(n, 8, r.list[j] ? block(j, 0x20f00000 + 0x100000 * i, 0x10) : 0);
        out['n' + i + '_' + j] = n;
      });
      u32(o, 0x1fc, nodes.length ? nodes[0] : 0);
      r.vs.forEach(function (s, k) {
        const v = block(k, 0x21000000 + 0x100000 * i, 0x40);
        u32(v, 0x30, s[1] ? block(k, 0x21100000 + 0x100000 * i, 0x10) : 0);
        out['w' + i + '_' + k] = v;
        u32(o, 0x744 + 4 * s[0], v);
      });
      if (i === 10) { u32(o, 0x1e0, o); u32(o, 0x1e4, 0x5148); u32(o, 0x1e8, 0x0f0f0000); u32(o, 0x1ec, 0xac); }
      out['o' + i] = o;
    });
    stubOn(out.log, 0x1bbd, 0, true, function () { return 0x1bbd; });
    stubOn(out.log, 0x20f4, 0, true, function () { return 0x20f4; });
    return out;
  }
  F.c3fd_alloff_h = function () { return allOff(true, true); };
  F.c3fd_alloff_n = function () { return allOff(false, true); };
  F.c3fd_alloff_nodev = function () { return allOff(true, false); };

  // Seq_listGetAt's thunk 0x100015fa as a recording stub for the sequencer rows: the list is this+0x60; entry i < 16 writes the
  // track pointer at this+0x300+4i into the out slot (on success AND failure, so the failure arm of Seq_replayEventRange writes
  // through a fixture track) and returns the code at this+0x340+4i; i >= 16 returns 0x24 and writes nothing. The out slot is the
  // caller's stack slot and is logged as 0x5ac0.
  function listStub(log, tOff, cOff) {
    const to = tOff || 0x300, co = cOff || 0x340;
    stubOn(log, 0x15fa, 2, true, function (a) {
      const self = a[0].sub(0x60), i = a[2].toUInt32();
      if (i >= 16) return 0x24;
      a[1].writePointer(self.add(to + 4 * i).readPointer());
      return self.add(co + 4 * i).readU32();
    }, [1]);
  }
  // Seq_replayEventRange: twelve 0x400-byte sequencers (vtable: slot +0xb4 the track count, slot +0x5c the mode; both answers in
  // the object), sixteen track entries each (0x40-byte tracks with the byte +0x38) and their result codes. Midi_Device::emitEvent
  // 0x100021ee and Seq_rewindTracks 0x10001ca3 are recording stubs. Rows: {count, mode, track bits +0x38, result codes}.
  const RP = [
    { n: 8, m: 0, b: [8, 8, 0, 8, 8, 0, 8, 8], c: [0, 0, 0, 0, 0, 0, 0, 0] },
    { n: 8, m: 1, b: [8, 8, 0, 8, 8, 0, 8, 8], c: [0, 0, 0, 0, 0, 0, 0, 0] },
    { n: 5, m: 1, b: [0, 8, 8, 0xff, 0], c: [3, 0, 0x24, 0, 0] },
    { n: 16, m: 0, b: [0xff, 0xf7, 8, 0, 8, 8, 8, 8, 1, 2, 3, 4, 5, 6, 7, 8], c: [1, 0, 0, 0, 0x24, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0] },
    { n: 16, m: 7, b: [8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8], c: [0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0] },
    { n: 0, m: 0, b: [8], c: [0] },
    { n: 3, m: 1, b: [0, 0, 0], c: [0, 0, 0] },
    { n: 4, m: 0xffffffff, b: [8, 0xc, 0x48, 9], c: [0, 0, 0, 2] },
    { n: 2, m: 0, b: [0x08, 0x88], c: [5, 0] },
    { n: 10, m: 1, b: [8, 8, 8, 8, 8, 8, 8, 8, 8, 8], c: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0] },
    { n: 6, m: 1, b: [0x80, 8, 0x80, 8, 0x80, 8], c: [0, 0x24, 0, 0, 0, 0] },
    { n: 0xffffffff, m: 0, b: [8, 8, 8], c: [0, 0, 0] },
  ];
  function seqObjs(out, vt, rows, tag) {
    rows.forEach(function (r, i) {
      const cfg = {};
      cfg[0x100 + 0xb4] = r.n;
      cfg[0x100 + 0x5c] = r.m;
      const o = recObj(i, tag, vt, cfg, 0x400);
      for (let k = 0; k < 16; k++) {
        const t = block(k, (tag + 0x800000 + 0x10000 * i) >>> 0, 0x40);
        t.add(0x38).writeU8(r.b[k] === undefined ? 0x18 : r.b[k]);
        out['t' + i + '_' + k] = t;
        u32(o, 0x300 + 4 * k, t);
        u32(o, 0x340 + 4 * k, r.c[k] === undefined ? 0 : r.c[k]);
      }
      out['o' + i] = o;
    });
  }
  F.c3fd_replay = function () {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    const vt = recVtable(out.log, { 0xb4: 0, 0x5c: 0 });
    seqObjs(out, vt, RP, 0x23900000);
    listStub(out.log);
    stubOn(out.log, 0x21ee, 1, true, function (a) { return 0x21ee; });
    stubOn(out.log, 0x1ca3, 0, true, function () { return 0x1ca3; });
    return out;
  };

  // Midi_Device::emitEvent: the same sequencer objects (track lookups through the list stub) with 0x460-byte tracks. `_h`:
  // g_midiDevice's +0x3c non-zero, midiOutShortMsg stood in at sound.dll's IAT (every (handle, message) recorded); the track's
  // dwords +0x258 .. +0x454 hold messages in a few slots. `_n`: +0x3c zero; the track's +0x58 .. +0x254 hold sound objects
  // (0x1500 bytes, slot +0x28 recorded, count +0x1438, members +0x1440).
  function emitFixture(h) {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    out.md = block(0, 0x3d000000, 0x40);
    u32(out.md, 0x3c, h ? 0x3c3c0002 : 0);
    swapGlobal(0xb4a1c, out.md, out, 'gmd');
    const vt = recVtable(out.log, { 0x28: 0 });
    const ovt = recVtable(out.log, {});
    for (let i = 0; i < 4; i++) {
      const o = recObj(i, 0x24600000, ovt, {}, 0x400);
      for (let k = 0; k < 16; k++) {
        const t = block(k, 0x24700000 + 0x10000 * i, 0x460);
        if (h) {
          fill(t.add(0x258), 0x200, 0);
          for (let j = 0; j < ((i * 5 + k) % 6); j++) u32(t, 0x258 + 4 * ((k * 7 + j * 13 + i) % 0x80), 0x00a09001 + 0x10101 * (j + k));
          if (k === 3) u32(t, 0x258 + 4 * 0x7f, 0x00000100);   // low byte 0: skipped
        } else {
          fill(t.add(0x58), 0x200, 0);
          for (let j = 0; j < ((i + k) % 4); j++) {
            const s = recObj(j, 0x24800000 + 0x10000 * i + 0x100 * k, vt, { 0x1438: (j + k) % 4 }, 0x1500);
            for (let q = 0; q < 3; q++) u32(s, 0x1440 + 4 * q, recObj(q, 0x24900000 + 0x1000 * i + 0x100 * k + 0x10 * j, vt, {}, 0x200));
            u32(t, 0x58 + 4 * ((k * 11 + j * 29 + i) % 0x80), s);
          }
        }
        out['t' + i + '_' + k] = t;
        u32(o, 0x300 + 4 * k, t);
        u32(o, 0x340 + 4 * k, (k === 5 || k === 11) ? 0x24 : 0);
      }
      out['o' + i] = o;
    }
    listStub(out.log);
    if (h) out.implog = __importStandIn({ module: 'sound.dll', imports: { 'WINMM.dll!midiOutShortMsg': { args: 2, ret: 0 } } }).log;
    return out;
  }
  F.c3fd_emit_h = function () { return emitFixture(true); };
  F.c3fd_emit_n = function () { return emitFixture(false); };

  // Midi_Device::sendShortMsg: a 0x1000-byte device (+0x25c + 0x4c*channel program words, +0x744 + 4v entries with the flag
  // byte +4 and a +0x30 object whose +0x218 is passed on), twelve 0x500-byte tracks (vtable slot +4 remaps the note: its answer
  // is the track's +0x104; +0x4f0 = Seq_firstTrackData's answer; +0x58 + 4*note channel objects; +0x258 + 4*note' words), 0x1500-byte
  // channel objects (flag byte +0xc8, slot +0x28 recorded, +0x200 = the voice Channel_findVoiceByTag answers). Thunks
  // 0x10001906, 0x1000187a, 0x100014a6, 0x1000272a are recording stubs; midiOutShortMsg is stood in at the IAT.
  function shortMsgFixture(h) {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    out.md = block(0, 0x3d000000, 0x40);
    u32(out.md, 0x3c, h ? 0x3c3c0003 : 0);
    swapGlobal(0xb4a1c, out.md, out, 'gmd');
    out.dev = block(0, 0x21a00000, 0x1000);
    for (let ch = 0; ch < 16; ch++) u32(out.dev, 0x25c + 0x4c * ch, ch % 4);
    for (let v = 0; v < 4; v++) {
      const e = block(v, 0x21b00000, 0x40);
      e.add(4).writeU8(v === 2 ? 0 : 8);
      const a = block(v, 0x21c00000, 0x220);
      u32(e, 0x30, v === 3 ? 0 : a);
      out['e' + v] = e;
      out['ea' + v] = a;
      u32(out.dev, 0x744 + 4 * v, e);
    }
    const cvt = recVtable(out.log, { 0x28: 0 });
    const tvt = recVtable(out.log, { 4: 1 });
    for (let i = 0; i < 12; i++) {
      const t = recObj(i, 0x21d00000, tvt, {}, 0x500);
      u32(t, 0x4f0, i % 3 === 0 ? 1 : 0);
      u32(t, 0x104, (0x20 + 5 * i) & 0x7f);   // slot +4's answer: the remapped note
      out['t' + i] = t;
      const c = recObj(i, 0x21e00000, cvt, {}, 0x1500);
      c.add(0xc8).writeU8((i % 2) && i !== 11 ? 0x81 : 0x80);   // row 11 (program 3: e3's +0x30 is NULL) takes slot +0x28
      u32(c, 0x200, i % 4 === 1 ? 0 : block(i, 0x21f00000, 0x20));
      out['c' + i] = c;
      const m = block(i, 0x22000000, 0x10);
      m.writeU8((0x10 + 7 * i) & 0x7f);
      m.add(1).writeU8((0x40 + 9 * i) & 0xff);
      out['msg' + i] = m;
      if (i !== 4) u32(t, 0x58 + 4 * m.readU8(), c);
    }
    stubOn(out.log, 0x1906, 0, true, function (a) { return a[0].add(0x4f0).readU32(); });
    stubOn(out.log, 0x187a, 1, true, function (a) { return a[0].sub(0x70).add(0x200).readU32(); });
    stubOn(out.log, 0x14a6, 0, true, function () { return 0x14a6; });
    stubOn(out.log, 0x272a, 1, true, function () { return 0x272a; });
    if (h) out.implog = __importStandIn({ module: 'sound.dll', imports: { 'WINMM.dll!midiOutShortMsg': { args: 2, ret: 0 } } }).log;
    return out;
  }
  F.c3fd_short_h = function () { return shortMsgFixture(true); };
  F.c3fd_short_n = function () { return shortMsgFixture(false); };
  // Midi_Device::playEvent: a 0x800-byte device (+0x25c + 0x4c*channel words), sixteen 0x60-byte tracks (+0x2c the event bytes,
  // +0x30 running status, +0x48 channel) and count slots. The four handler thunks are recording stubs with fixed answers
  // (sendShortMsg 0x100016bd: 3, noteOn 0x10001a14: 5, setChannelVolume 0x100022ca: 7, handleMetaEvent 0x1000124e: 4).
  const PE = [
    [0x83, 0x40, 0x10], [0x95, 0x3c, 0x64], [0x95, 0x3c, 0x00], [0xa2, 1, 2], [0xb7, 7, 100], [0xc9, 0x21],
    [0xd1, 5], [0xef, 0, 0x40], [0xf0, 5, 1], [0xff, 0x2f, 0], [0xff, 0x51, 3], [0x40, 0x7f],
    [0x10, 0], [0x7f, 1], [0xf7, 2], [0x00, 0x00], [0x05, 0x06],
  ];
  // running-status rows; row 16's status byte 0 (a track with no status yet) makes kind - 0x80 wrap above 0x7f (0x100208e1)
  const PE_RUN = { 11: [0x90, 2], 12: [0xc0, 5], 13: [0xe0, 9], 15: [0xb0, 0xf], 16: [0x00, 3] };
  F.c3fd_playev = function () {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    out.dev = block(0, 0x20800000, 0x800);
    PE.forEach(function (ev, i) {
      const b = block(i, 0x20900000, 0x10);
      b.writeByteArray(ev);
      const t = block(i, 0x20a00000, 0x60);
      u32(t, 0x2c, b);
      const run = PE_RUN[i] || [0x80 + 0x10 * (i % 7), i % 16];
      t.add(0x30).writeU8(run[0]);
      u32(t, 0x48, run[1]);
      out['b' + i] = b;
      out['t' + i] = t;
      const c = block(i, 0x20b00000, 4);
      u32(c, 0, 0x100 * i);
      out['c' + i] = c;
    });
    stubOn(out.log, 0x16bd, 3, true, function () { return 3; });
    stubOn(out.log, 0x1a14, 3, true, function () { return 5; });
    stubOn(out.log, 0x22ca, 3, true, function () { return 7; });
    stubOn(out.log, 0x124e, 2, true, function () { return 4; });
    return out;
  };
  // Engine_playSound: the word 0x100b507c swapped (`_off`: 0). Twelve 0x200-byte owners (vtable slot +0x7c recorded, +0x30,
  // +0x44 flags, +0x50 an old string or NULL, +0x40 a sound object or NULL) and their sound objects (slots +0x60, +0x10, +0x48
  // recorded, answers per row). create_sound's thunk 0x1000137a (cdecl out, name, type) is a recording stub: the owner is
  // out - 0x40; its +0x1f0 (a sound object or 0) is written to *out and its +0x1f4 returned. Names are fixture strings;
  // operator new / delete through the heap stand-in.
  const PS = [
    { pre: 0, mk: 1, cr: 0, a10: 0, f: 0, a30: 1, old: 1 }, { pre: 0, mk: 0, cr: 7, f: 0 }, { pre: 1, a60: 1, f: 1 },
    { pre: 1, a60: 0, a10: 0, f: 0, a30: 0, old: 0 }, { pre: 1, a10: 3, f: 0xff, old: 1 }, { pre: 1, a10: 0, f: 1, old: 1 },
    { pre: 0, mk: 1, cr: 0, a10: 9, f: 0x10 }, { pre: 1, a60: 0x80000000, f: 0 }, { pre: 0, mk: 1, cr: 0, a10: 0, f: 0xfe, a30: 5 },
    { pre: 1, a10: 0, f: 0, a30: 2, old: 1 }, { pre: 0, mk: 0, cr: 0x80000001, f: 3 }, { pre: 1, a10: 0xffffffff, f: 2, old: 0 },
  ];
  const NAMES = ['a', 'bc', 'snd01', '', 'longer_name_x', 'q', 'zz', 'm3', 'abcdefghijklmnop', 'k', 'x9', 'yy'];
  function playSnd(on) {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    swapGlobal(0xb507c, on ? 1 : 0, out, 'gon');
    const ovt = recVtable(out.log, { 0x7c: 0 });
    const svt = recVtable(out.log, { 0x60: 0, 0x10: 1, 0x48: 1 });
    heap(out, 0x400);
    PS.forEach(function (r, i) {
      const sa = {};
      sa[0x160] = r.a60 || 0;
      sa[0x110] = r.a10 || 0;
      const s = recObj(i, 0x2ae00000, svt, sa);
      const old = block(i, 0x2af00000, 0x10);
      const o = recObj(i, 0x2ad00000, ovt, { 0x30: r.a30 || 0, 0x44: r.f, 0x50: r.old ? old : 0, 0x40: r.pre ? s : 0,
                                             0x1f0: r.mk ? s : 0, 0x1f4: r.cr || 0 });
      const nm = keep(Memory.allocUtf8String(NAMES[i]));
      out['s' + i] = s;
      out['old' + i] = old;
      out['o' + i] = o;
      out['nm' + i] = nm;
    });
    stubOn(out.log, 0x137a, 3, 'cdecl', function (a) {
      const owner = a[0].sub(0x40);
      const child = owner.add(0x1f0).readPointer();
      if (!child.isNull()) a[0].writePointer(child);
      return owner.add(0x1f4).readU32();
    });
    return out;
  }
  F.c3fd_playsnd = function () { return playSnd(true); };
  F.c3fd_playsnd_off = function () { return playSnd(false); };

  // buildVoiceTxName: twelve 0x100-byte blocks with marker words (their +0x40 is zeroed by the body before it is tested), operator
  // new through the heap stand-in, Sound_setSourceType's thunk 0x100018a2 a recording stub.
  F.c3fd_txname = function () {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    heap(out, 0x200);
    for (let i = 0; i < 12; i++) out['o' + i] = block(i, 0x2f200000, 0x100);
    stubOn(out.log, 0x18a2, 1, true, function (a) { return (a[1].toUInt32() + 0x100) >>> 0; });
    return out;
  };

  // ===================================================================================================================
  // D. second group
  // ===================================================================================================================
  // scanCommandRing (Midi: ring at +0x50, object flag byte +0xac bit 5; Wave: ring at +0x1d8, flag byte +0xc8 bit 6). Twelve
  // devices; the (first, last) index pairs live in one block `idx` (8 bytes per row); ring entries {kind, object, w2, w3} come
  // from one pool `ents` (0x10 bytes each) and objects from `objs` (0x100 bytes each: 0 no flag, 1 flag, 2 a second object
  // without the flag, -1 NULL). CommandQueue::at and pairClear run for real; operator delete records.
  const SCAN = [
    { f: 5, l: 5, e: [] }, { f: 5, l: 6, e: [[1, 0]] }, { f: 0, l: 3, e: [[1, 0, 7, 8], [1, 0, 7, 8], [1, 0, 7, 9]] },
    { f: 10, l: 13, e: [[1, 1], [2, 1], [3, 0]] }, { f: 10, l: 13, e: [[1, 0], [2, 0], [3, 0]] },
    { f: 20, l: 24, e: [[1, -1], [2, -1], [1, 2], [2, 2]] }, { f: 0, l: 4, e: [[2, 0], [1, 0], [2, 1], [1, 1]] },
    { f: 0, l: 3, e: [[2, -1], [1, -1], 0] }, { f: 0xffe, l: 2, e: [[1, 0, 1, 1], 0, [1, 0, 1, 1], [3, 0]] },
    { f: 0, l: 5, e: [[3, 0], [3, 0, 5, 5], [3, 0, 5, 6], [2, 2], [4, 2]] }, { f: 0, l: 4, e: [0, [1, 0], [2, 0], [1, 0]] },
    { f: 0, l: 0x10, e: [[1, 1], [2, 1], [2, 0], [1, 0], [1, 2, 3], [1, 2, 3], [2, 2], [1, 1, 4], [2, 1, 4], [1, 2, 3, 3]] },
  ];
  function scanFixture(ringOff, devSize, flagOff, flagBit) {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    heap(out, 0x100);
    out.idx = block(0, 0x1d000000, 8 * SCAN.length);
    out.ents = fill(keep(Memory.alloc(0x10 * 0x80)), 0x10 * 0x80, 0x5e);
    out.objs = block(0, 0x0b000000, 0x100 * 3);
    [0, 1, 2].forEach(function (k) { out.objs.add(0x100 * k + flagOff).writeU8(k === 1 ? (0x81 | flagBit) : (0x7e & ~flagBit)); });
    let ne = 0;
    SCAN.forEach(function (r, i) {
      const d = block(i, 0x5c000000, devSize);
      fill(d.add(ringOff), 0x4000, 0);
      u32(out.idx, 8 * i, r.f);
      u32(out.idx, 8 * i + 4, r.l);
      out['fa' + i] = out.idx.add(8 * i);
      out['la' + i] = out.idx.add(8 * i + 4);
      r.e.forEach(function (en, k) {
        if (!en) return;
        const e = out.ents.add(0x10 * ne++);
        u32(e, 0, en[0]);
        u32(e, 4, en[1] < 0 ? 0 : out.objs.add(0x100 * en[1]));
        u32(e, 8, en[2] || 0);
        u32(e, 0xc, en[3] || 0);
        u32(d, ringOff + 4 * ((r.f + k) & 0xfff), e);
      });
      out['d' + i] = d;
    });
    return out;
  }
  F.c3fd_scan_midi = function () { return scanFixture(0x50, 0x4100, 0xac, 0x20); };
  F.c3fd_scan_wave = function () { return scanFixture(0x1d8, 0x4200, 0xc8, 0x40); };

  // Sound::dtor / streamReaderDtor: twelve objects whose node list (Sound: head +0xa4c, tail +0xa50, count +0xa58; stream reader:
  // +0x5c, +0x60, +0x68) holds nodes {prev, next, payload} from one pool `nodes`; payloads are blocks from the pool `pays`
  // (their dword 0 is the mmio handle the stream reader closes) or NULL. mmioClose is stood in at sound.dll's IAT, the open-file
  // count 0x100b4a08 and the wave-in device 0x100b4a24 are swapped; the tail calls' thunks are recording stubs.
  const LISTS = [[], [1], [0], [1, 2], [1, 0, 3], [0, 1, 2], [1, 2, 0, 3, 4], [1, 0, 0, 2], [3, 4, 5, 6], [0, 0], [1, 2, 3, 0], [2, 0, 1, 0, 5]];
  function listFixture(out, objSize, hOff, cOff) {
    out.nodes = fill(keep(Memory.alloc(0x10 * 0x40)), 0x10 * 0x40, 0x4e);
    out.pays = block(0, 0x9a000000, 0x10 * 0x40);
    let nn = 0;
    LISTS.forEach(function (ps, i) {
      const o = block(i, 0x0d000000, objSize);
      const ns = ps.map(function () { return out.nodes.add(0x10 * nn++); });
      ns.forEach(function (n, j) {
        u32(n, 0, j ? ns[j - 1] : 0);
        u32(n, 4, j + 1 < ns.length ? ns[j + 1] : 0);
        u32(n, 8, ps[j] ? out.pays.add(0x10 * (nn - ns.length + j)) : 0);
      });
      u32(o, hOff, ns.length ? ns[0] : 0);
      u32(o, hOff + 4, ns.length ? ns[ns.length - 1] : 0);
      u32(o, cOff, 0x100 + ns.length);
      out['o' + i] = o;
    });
  }
  F.c3fd_sounddtor = function () {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    heap(out, 0x100);
    listFixture(out, 0xa60, 0xa4c, 0xa58);
    stubOn(out.log, 0x1893, 0, true, function () { return 0x1893; });
    return out;
  };
  function srFixture(wi) {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    heap(out, 0x100);
    listFixture(out, 0x80, 0x5c, 0x68);
    swapGlobal(0xb4a08, 0x40, out, 'gopen');
    out.wi = block(0, 0x0ea00000, 0x40);
    swapGlobal(0xb4a24, wi ? out.wi : 0, out, 'gwi');
    stubOn(out.log, 0x1ba4, 0, true, function () { return 0x1ba4; });
    stubOn(out.log, 0x2612, 0, true, function () { return 0x2612; });
    out.implog = __importStandIn({ module: 'sound.dll', imports: { 'WINMM.dll!mmioClose': { args: 2, ret: 0 } } }).log;
    return out;
  }
  F.c3fd_srdtor_wi = function () { return srFixture(true); };
  F.c3fd_srdtor_nowi = function () { return srFixture(false); };

  // MultiSound_setSlotSource: twelve 0x200-byte channels (+0xfc current object, +0xf8 a source object or NULL, slots +0x58 ..
  // +0xf4). operator new carves the 0x1450-byte object from the arena; MultiSound_ctor's thunk 0x100025ea is a recording stub that
  // gives the block a recording vtable (slots 0x6c, 0x10, 0 with one argument). Slot +0x10 derives everything from its argument
  // (the slot value): it returns bits 8..15 and writes bits 0..7 to the object's byte +0xcc. The constant routine's thunk
  // 0x10001dde and Channel_setStreamSource's thunk 0x1000122b are recording stubs.
  const SS = [
    { cur: 1, idx: 0 }, { idx: 0x28 }, { idx: 0xffffffff }, { idx: 3, v: 0 }, { idx: 3, v: 0x10000 }, { idx: 5, v: 0x10004, src: 1 },
    { idx: 0x27, v: 0x10504 }, { idx: 0, v: 0x10700, src: 1 }, { idx: 9, v: 0x100fb, src: 1 }, { idx: 1, v: 0x20004 },
    { idx: 0x10, v: 0x1ff00, src: 0 }, { idx: 2, v: 0x10014, src: 1 },
  ];
  F.c3fd_setslot = function () {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    heap(out, 0x1500);
    const vt = fill(keep(Memory.alloc(0x100)), 0x100, 0);
    vt.add(0x6c).writePointer(keep(new NativeCallback(function (self, a) { logCall(out.log, 0x516c, [self, a]); return 0x6c; },
      'uint32', ['pointer', 'uint32'], 'thiscall')));
    vt.add(0x10).writePointer(keep(new NativeCallback(function (self, a) {
      logCall(out.log, 0x5110, [self, a]);
      self.add(0xcc).writeU8(a & 0xff);
      return (a >>> 8) & 0xff;
    }, 'uint32', ['pointer', 'uint32'], 'thiscall')));
    vt.add(0).writePointer(keep(new NativeCallback(function (self, a) { logCall(out.log, 0x5100, [self, a]); return 0; },
      'uint32', ['pointer', 'uint32'], 'thiscall')));
    SS.forEach(function (r, i) {
      const o = block(i, 0x2f800000, 0x200);
      fill(o.add(0x58), 0xa0, 0);
      if (r.idx < 0x28 && r.v !== undefined) u32(o, 0x58 + 4 * r.idx, r.v);
      if (r.idx < 0x28 && r.v === undefined) u32(o, 0x58 + 4 * r.idx, 0x10000 + i);
      const src = block(i, 0x2f900000, 0x100);
      u32(o, 0xf8, r.src ? src : 0);
      u32(o, 0xfc, r.cur ? block(i, 0x2fa00000, 0x10) : 0);
      out['o' + i] = o;
      out['src' + i] = src;
      out['idx' + i] = r.idx;
    });
    stubOn(out.log, 0x25ea, 0, true, function (a) { a[0].writePointer(vt); a[0].add(0xcc).writeU8(0); return a[0]; });
    stubOn(out.log, 0x1dde, 0, true, function () { return 0xb; });
    stubOn(out.log, 0x122b, 2, true, function () { return 0x122b; });
    return out;
  };

  // Voice_startPlayback: twelve 0x800-byte sequencers (vtable slots +0x5c busy, +0x48 one argument, +0xb4 track count; answers in
  // the object), +0xac flags, +0x23c / +0x240 / +0x238 the marked-track list (index arrays in the pool `ix`), tracks from the pool
  // `trk` through the list stub (this+0x300 / +0x340 tables; entries 3 and 6 are NULL tracks, index >= 16 is a failed lookup that
  // writes nothing), +0x3f0 the answer of CommandQueue::push's recording stub (thunk 0x100015e1). Track_resetReadState 0x1000239c,
  // Track_clearFlag8 0x100010be and Seq_preRollTrack 0x10001956 are recording stubs; timeGetTime is stood in; g_midiDevice is a
  // fixture block.
  const SP = [
    { ac: 1, busy: 1 }, { ac: 0x04 }, { ac: 0x20 }, { ac: 0, cnt: 0, push: 0 },
    { ac: 9, f: 1, ix: [0, 1, 2], cnt: 2, push: 1 }, { ac: 8, f: 1, ix: [20, 4], cnt: 3, push: 0 }, { ac: 0, f: 1, ix: [], cnt: 1, push: 2 },
    { ac: 0x108, f: 1, n: -1, ix: [1], cnt: 0, push: 0 }, { ac: 0x81, f: 1, ix: [5, 6, 7, 3], cnt: 1, push: 0 }, { ac: 2, f: 0, n: 5, ix: [1], cnt: 2, push: 3 },
    { ac: 0x408, f: 1, ix: [15, 16], cnt: 4, push: 0 }, { ac: 0xffffffdb, f: 1, ix: [2, 2], cnt: 1, push: 0 },
  ];
  F.c3fd_startplay = function () {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    out.md = block(0, 0x3d000000, 0x40);
    swapGlobal(0xb4a1c, out.md, out, 'gmd');
    const vt = recVtable(out.log, { 0x5c: 0, 0x48: 1, 0xb4: 0 });
    out.trk = block(0, 0x7a000000, 0x40 * 16 * SP.length);
    out.ix = block(0, 0x1c000000, 0x10 * SP.length);
    SP.forEach(function (r, i) {
      const cfg = { 0xac: r.ac, 0x23c: r.f || 0, 0x240: r.n !== undefined ? r.n : (r.ix || []).length, 0x238: out.ix.add(0x10 * i),
                    0x3f0: r.push || 0 };
      cfg[0x100 + 0x5c] = r.busy || 0;
      cfg[0x100 + 0xb4] = r.cnt || 0;
      const o = recObj(i, 0x20b00000, vt, cfg, 0x800);
      (r.ix || []).forEach(function (v, k) { u32(out.ix, 0x10 * i + 4 * k, v); });
      for (let k = 0; k < 16; k++) {
        u32(o, 0x300 + 4 * k, (k === 3 || k === 6) ? 0 : out.trk.add(0x40 * (16 * i + k)));
        u32(o, 0x340 + 4 * k, 0);
      }
      out['o' + i] = o;
    });
    listStub(out.log);
    stubOn(out.log, 0x239c, 0, true, function () { return 0x239c; });
    stubOn(out.log, 0x10be, 0, true, function () { return 0x10be; });
    stubOn(out.log, 0x1956, 1, true, function () { return 0x1956; });
    stubOn(out.log, 0x15e1, 4, true, function (a) { return a[1].add(0x3f0).readU32(); });
    out.implog = __importStandIn({ module: 'sound.dll', imports: { 'WINMM.dll!timeGetTime': { args: 0, ret: 0x7f3a1234 } } }).log;
    return out;
  };

  // Seq_dispatchChannelCmd: twelve 0x800-byte sequencers (vtable slot +0xb4 the track count; records at +0x264 + 0x4c*n; the list
  // stub's tables at +0x740 / +0x780: eight 0x470-byte tracks from the pool `trk`, entries 8..15 NULL with result code 0x24 at 9),
  // fifteen 0x40-byte commands in the pool `cmd` (+0x1c type, +0x28 n, +0x2c flag byte, +0x30 value, +0x3c the answer of
  // Seq_updateChannel's recording stub, thunk 0x10001550). Ctrl_appendIfAccepted's thunk 0x10001654 answers the track's +0x3c.
  const DC = [
    { t: 0 }, { t: 3 }, { t: 0 }, { t: 1, n: 2, fl: 2 }, { t: 1, n: 5 }, { t: 2, n: 3 }, { t: 3 }, { t: 5 }, { t: 3 }, { t: 5 },
    { t: 4 }, { t: 6 }, { t: 1, n: 0, fl: 0xfd }, { t: 2, n: 0 }, { t: 5 },
  ];
  F.c3fd_dispatch = function () {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    const vt = recVtable(out.log, { 0xb4: 0 });
    out.trk = block(0, 0x7b000000, 0x470 * 8 * 12);
    out.cmd = block(0, 0xcd000000, 0x40 * DC.length);
    DC.forEach(function (c, k) {
      const p = out.cmd.add(0x40 * k);
      u32(p, 0x1c, c.t);
      u32(p, 0x28, c.n || 0);
      p.add(0x2c).writeU8(c.fl || 0);
      u32(p, 0x3c, 0x3c00 + k);
      out['c' + k] = p;
    });
    for (let i = 0; i < 12; i++) {
      const cfg = {};
      cfg[0x100 + 0xb4] = [4, 4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4][i];
      const o = recObj(i, 0x24000000, vt, cfg, 0x800);
      for (let k = 0; k < 16; k++) {
        u32(o, 0x740 + 4 * k, k < 8 ? out.trk.add(0x470 * (8 * i + k)) : 0);
        u32(o, 0x780 + 4 * k, k === 9 ? 0x24 : 0);
      }
      out['o' + i] = o;
    }
    listStub(out.log, 0x740, 0x780);
    stubOn(out.log, 0x1550, 1, true, function (a) { return a[1].add(0x3c).readU32(); });
    stubOn(out.log, 0x1654, 1, true, function (a) { return a[0].add(0x3c).readU32(); });
    return out;
  };

  // DsBuffer::create: recording COM objects. A COM vtable slot is a __stdcall NativeCallback (interface pointer first) that logs
  // {0x5200 + slot, arguments}, writes the object's dword +0x140 + slot through argument `out` when the slot has one (the method's
  // out parameter) and returns the object's +0x100 + slot. ds (slot +0xc CreateSoundBuffer, out = argument 2), the buffer (slots 0
  // QueryInterface out 2, +0xc GetCaps, +0x14 GetFormat, +0x30 Play, +0x38 SetFormat) and the queried interfaces (slot 0) are such
  // objects; `this` is a 0x100-byte block (+0 flags, +0x60 buffer, +0x64 / +0x68 / +0x6c interfaces, +0x74 flag byte, +0x78 n,
  // +0x80 format pointer, +0xa8 format copy).
  function recCom(log, slots) {
    const vt = fill(keep(Memory.alloc(0x100)), 0x100, 0);
    Object.keys(slots).forEach(function (k) {
      const off = parseInt(k, 10), n = slots[k][0], outIdx = slots[k][1];
      vt.add(off).writePointer(keep(new NativeCallback(function () {
        const a = Array.prototype.slice.call(arguments);
        logCall(log, 0x5200 + off, a);
        if (outIdx !== undefined && !a[outIdx].isNull()) a[outIdx].writeU32(a[0].add(0x140 + off).readU32());
        return a[0].add(0x100 + off).readU32();
      }, 'uint32', Array(n + 1).fill('pointer'), 'stdcall')));
    });
    return vt;
  }
  const DSB_HR = [0x8878000a, 0x8007000e, 0x80004001, 0x80070057, 0x88780064, 0x88780078, 0x88780096, 0x887800aa, 0x88780065,
                  0x887800ab, 0x80000000, 0x8007000f, 0, 0];
  const DSB_OK = [
    { f0: 0 }, { f0: 2, n: 7 }, { f0: 0, n: 1, f74: 0x10, qout: 1, q2out: 5 }, { f0: 2, n: 3, f74: 0x10, qa: 0x80004002, qout: 1 },
    { f0: 0, n: 0xffffffff, f74: 0x10, qout: 0, pre68: 1 }, { f0: 2, f74: 0x10, qout: 1, q2a: 0x80000001 },
    { f0: 0, f74: 0x30, qa: 1, qout: 1 }, { f0: 3, n: 0x10, f74: 0xef }, { f0: 0, fmt: 1 }, { f0: 2, fmt: 1, n: 5 },
    { f0: 0, f74: 0x10, qa: 0x80000000, pre64: 1 }, { f0: 0, n: 2, f74: 0x10, qout: 0 },
  ];
  function dsbFixture(failing) {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    const dvt = recCom(out.log, { 0xc: [3, 2] });
    const bvt = recCom(out.log, { 0: [2, 2], 0xc: [1], 0x14: [3], 0x30: [3], 0x38: [1] });
    const qvt = recCom(out.log, { 0: [2, 2] });
    out.fmt = block(0, 0xf0000000, 0x20);
    out.desc = block(0, 0xde500000, 0x10);
    const rows = failing ? DSB_HR.map(function (hr, i) { return { hr: hr, f0: i & 2, fmt: i % 3 === 0 ? 1 : 0, pre60: i === 13 }; }) : DSB_OK;
    rows.forEach(function (r, i) {
      const q = recObj(i, 0x5d000000, qvt, {});
      const q2 = recObj(i, 0x5e000000, qvt, {});
      u32(q, 0x100, r.q2a || 0);
      u32(q, 0x140, r.q2out ? 0x6c000000 + i : 0);
      const b = recObj(i, 0x5b000000, bvt, {});
      u32(b, 0x100, r.qa || 0);
      u32(b, 0x140, r.qout ? q : 0);
      const ds = recObj(i, 0x5a000000, dvt, {});
      u32(ds, 0x10c, r.hr || 0);
      u32(ds, 0x14c, r.hr ? 0 : b);
      const o = block(i, 0xb0000000, 0x100);
      u32(o, 0, 0x7ffffff8 | (r.f0 || 0));
      u32(o, 0x60, r.pre60 ? b : 0);
      u32(o, 0x64, r.pre64 ? q2 : 0);
      u32(o, 0x68, r.pre68 ? q2 : 0);
      u32(o, 0x6c, 0x6c6c6c6c);
      o.add(0x74).writeU8(r.f74 || 0);
      u32(o, 0x78, r.n || 0);
      u32(o, 0x80, r.fmt ? out.fmt : 0);
      out['o' + i] = o;
      out['ds' + i] = ds;
      out['b' + i] = b;
      out['q' + i] = q;
      out['q2_' + i] = q2;
    });
    return out;
  }
  F.c3fd_dsb_hr = function () { return dsbFixture(true); };
  F.c3fd_dsb_ok = function () { return dsbFixture(false); };

  // ===================================================================================================================
  // E. third group
  // ===================================================================================================================
  // Channel_releaseBuffers / Channel_closeAndFree: twelve 0x1400-byte channels (flag bytes +0x58 / +0x5c, +0x54 DirectSound
  // buffer, +0 / +4 mmio handles, +8 a stream block {+0x18 buffer, +0x44 handle}, +0x1b8, +0x13a4, node lists +0x1338 / +0x1348
  // with payload blocks), all node and payload blocks from pools. The module's open-mmio list (head 0x100b49c8, tail 0x100b49cc,
  // cursor 0x100b49d0, count 0x100b49d4) is swapped for a fixture list whose payloads are channel+0xc of some rows (as head, in the
  // middle twice, as tail) and two unrelated nodes; the open count 0x100b4a08 is swapped; mmioClose is stood in; operator delete and
  // free 0x10042c07 record. Channel_releaseBuffers: DirectSoundDevice::removeBuffer's thunk 0x10001677 and Channel_resetPanState's
  // thunk 0x10001866 are recording stubs, g_waveDevice 0x100b49f0 a fixture block. Channel_closeAndFree: the eh vector destructor
  // 0x1004280e runs for real and calls the element destructor's thunk 0x10001b45 sixteen times; its body 0x10037df0 is a bare
  // `ret` (Frida refuses to patch it: "unable to intercept function"), so it runs for real and changes nothing.
  function chanPools(out) {
    out.cnodes = fill(keep(Memory.alloc(0x10 * 0x80)), 0x10 * 0x80, 0x4e);
    out.cpays = block(0, 0x9b000000, 0x20 * 0x80);
    out.streams = block(0, 0x5f000000, 0x50 * 12);
    out.mnodes = fill(keep(Memory.alloc(0x10 * 0x10)), 0x10 * 0x10, 0x4d);
    let nn = 0, np = 0;
    out.addList = function (o, hOff, cOff, ps) {
      const ns = ps.map(function () { return out.cnodes.add(0x10 * nn++); });
      ns.forEach(function (n, j) {
        u32(n, 0, j ? ns[j - 1] : 0);
        u32(n, 4, j + 1 < ns.length ? ns[j + 1] : 0);
        u32(n, 8, ps[j] ? out.cpays.add(0x20 * np++) : 0);
      });
      u32(o, hOff, ns.length ? ns[0] : 0);
      u32(o, hOff + 4, ns.length ? ns[ns.length - 1] : 0);
      u32(o, cOff, 0x200 + ns.length);
    };
  }
  // the open-mmio list: keys are row numbers (-1 = an unrelated payload)
  function mmioList(out, rowsInOrder) {
    const ns = rowsInOrder.map(function (r, j) { return out.mnodes.add(0x10 * j); });
    ns.forEach(function (n, j) {
      u32(n, 0, j ? ns[j - 1] : 0);
      u32(n, 4, j + 1 < ns.length ? ns[j + 1] : 0);
      const r = rowsInOrder[j];
      u32(n, 8, r < 0 ? 0x77700000 + j : out['o' + r].add(0xc));
    });
    swapGlobal(0xb49c8, ns[0], out, 'gmh');
    swapGlobal(0xb49cc, ns[ns.length - 1], out, 'gmt');
    swapGlobal(0xb49d0, 0x5c5c5c5c, out, 'gmc');
    swapGlobal(0xb49d4, 0x100 + ns.length, out, 'gmn');
    swapGlobal(0xb4a08, 0x40, out, 'gopen');
  }
  // rows: f58, f5c, b54 (DirectSound buffer), h0, h4, st (stream at +8: 0 none, 1 block, 2 = this+0xc), b1b8, a4 (+0x13a4),
  // l38 / l48 (payload patterns of the +0x1338 / +0x1348 lists)
  const CH = [
    { f58: 0 }, { f58: 2, f5c: 4 }, { f58: 2, b54: 0x5400, l38: [1, 1] }, { f58: 2, b54: 0x5401, l38: [1, 0, 1] },
    { f58: 2, b54: 0x5402 }, { f58: 3, h0: 0x11, st: 1, b1b8: 1, l48: [1, 1], a4: 5 }, { f58: 2, h0: 0x12, h4: 0x22, st: 1, l48: [1] },
    { f58: 2, h0: 0x13, h4: 0x23, st: 1, l48: [1, 0, 1], l38: [1, 1] }, { f58: 0xff, h0: 0x14, st: 2, l38: [0], a4: 7 },
    { f58: 2, h0: 0x15, h4: 0x25, st: 1, b1b8: 1 }, { f58: 0xfd, h0: 0x16, st: 1, l48: [1], l38: [1] },
    { f58: 6, f5c: 0xfb, h0: 0, h4: 0x27, st: 1, l48: [1, 1, 1], l38: [1, 0], a4: 9 },
  ];
  // two layouts of the open-mmio list (row numbers, -1 unrelated): l1 puts row 8 at the head and row 9 at the tail; l2 puts row
  // 6's two nodes at the head and row 11's two nodes at the tail, so a second unlink also removes a head and a tail node
  const MMIO_L = { l1: [8, -1, 5, 6, 6, 11, -1, 11, 9], l2: [6, 6, 5, -1, 9, 8, 11, 11] };
  function chanFixture(release, layout) {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    heap(out, 0x100, { free: [0x4249a, 0x42c07] });
    chanPools(out);
    CH.forEach(function (r, i) {
      const o = block(i, 0x34000000, 0x1400);
      o.add(0x58).writeU8(r.f58 || 0);
      o.add(0x5c).writeU8(r.f5c || 0);
      u32(o, 0x54, r.b54 || 0);
      u32(o, 0, r.h0 || 0);
      u32(o, 4, r.h4 || 0);
      const s = out.streams.add(0x50 * i);
      u32(s, 0x18, 0x0b000000 + 0x100 * i);
      u32(s, 0x44, 0x44000000 + i);
      u32(o, 8, r.st === 1 ? s : (r.st === 2 ? o.add(0xc) : 0));
      u32(o, 0x1b8, r.b1b8 ? out.cpays.add(0x20 * (0x70 + i)) : 0);
      u32(o, 0x13a4, r.a4 || 0);
      out.addList(o, 0x1338, 0x1344, r.l38 || []);
      out.addList(o, 0x1348, 0x1354, r.l48 || []);
      out['o' + i] = o;
    });
    mmioList(out, MMIO_L[layout]);
    out.implog = __importStandIn({ module: 'sound.dll', imports: { 'WINMM.dll!mmioClose': { args: 2, ret: 0 } } }).log;
    if (release) {
      out.dev = block(0, 0xdd400000, 0x40);
      swapGlobal(0xb49f0, out.dev, out, 'gwd');
      stubOn(out.log, 0x1677, 1, true, function () { return 0x1677; });
      stubOn(out.log, 0x1866, 0, true, function () { return 0x1866; });
    } else {
      guard();   // the element destructor 0x10037df0 is a bare `ret` Frida cannot patch: it runs for real (no effect)
    }
    return out;
  }
  ['l1', 'l2'].forEach(function (l) {
    F['c3fd_relbufs_' + l] = function () { return chanFixture(true, l); };
    F['c3fd_closefree_' + l] = function () { return chanFixture(false, l); };
  });

  // Midi_Device::drainCommandQueue: twelve 0x4100-byte devices (ring at +0x50, head +0x4050, tail +0x4054, the device list +0x40
  // head / +0x44 tail / +0x48 cursor / +0x4c count, +0x40f0 the amount the scanCommandRing stub adds to *first). Entries {kind,
  // object, a, b} from the pool `ents`; objects are recording voices (vtable slots +0xb8 none and 0 one argument; +0xac flags) from
  // `vox`; device-list nodes from `dnodes`. scanCommandRing's thunk 0x1000196f, Voice_allNotesOff's thunk 0x10001dd9,
  // Seq_closeFile 0x1000246e and the command handlers 7 (0x10001186), 9 (0x10001474), 8 (0x100025c2), 10 (0x100023e7) and 11
  // (0x100011db) are recording stubs; pairClear runs for real; operator delete records.
  // rows: h, t, scan adds `adv` to *first, entries [kind, voice index (-1 NULL), a, b], listed: voices in the device list
  const DQ = [
    { h: 4, t: 4, e: [] }, { h: 0, t: 1, e: [[1, 0]] }, { h: 10, t: 15, e: [[2, 0], [3, 1], [4, 2], [5, 3], [6, 4]], listed: [1, 2] },
    { h: 0xffd, t: 3, e: [[7, 0], [8, 1, 3], [9, 2, 4], [10, 3, 5, 6], [11, 4, 7, 8], [12, 0]] },
    { h: 1, t: 6, e: [[4, -1], [7, -1], [8, -1], [9, -1], [10, -1]] }, { h: 1, t: 4, e: [[11, -1], 0, [0, 0]] },
    { h: 20, t: 24, adv: 1, e: [[1, 0], [2, 1], [3, 2], [4, 3]], listed: [3, 2, 5] },
    { h: 30, t: 33, e: [[4, 0], [4, 1], [3, 2]], listed: [0, 2] }, { h: 2, t: 2, e: [[1, 0]] },
    { h: 5, t: 0x1005, e: [[1, 0]] }, { h: 7, t: 9, adv: 2, e: [[2, 4], [9, 5, 1]] },
    { h: 0x40, t: 0x48, e: [[4, 2], [3, 0], [3, 1], [4, 3], [0x80000001, 4], [8, 5, 2], [1, 0]], listed: [0, 1, 2, 3, 4, 5] },
  ];
  F.c3fd_drain = function () {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    heap(out, 0x100);
    const vt = recVtable(out.log, { 0xb8: 0, 0: 1 });
    out.ents = fill(keep(Memory.alloc(0x10 * 0x60)), 0x10 * 0x60, 0x5e);
    out.vox = block(0, 0x0f000000, 0x200 * 6 * 12);
    out.dnodes = fill(keep(Memory.alloc(0x10 * 0x60)), 0x10 * 0x60, 0x4c);
    let ne = 0, nd = 0;
    DQ.forEach(function (r, i) {
      const d = block(i, 0xa3300000, 0x4100);
      fill(d.add(0x50), 0x4000, 0);
      u32(d, 0x4050, r.h);
      u32(d, 0x4054, r.t);
      u32(d, 0x40f0, r.adv || 0);
      const vx = [];
      for (let k = 0; k < 6; k++) {
        const v = out.vox.add(0x200 * (6 * i + k));
        v.writePointer(vt);
        fill(v.add(0x100), 0xe0, 0);
        u32(v, 0xac, [0x20, 0x21, 0xff, 0x0, 0x60, 0x2f][k]);
        vx.push(v);
      }
      const listed = r.listed || [];
      const ns = listed.map(function () { return out.dnodes.add(0x10 * nd++); });
      ns.forEach(function (n, j) {
        u32(n, 0, j ? ns[j - 1] : 0);
        u32(n, 4, j + 1 < ns.length ? ns[j + 1] : 0);
        u32(n, 8, vx[listed[j]]);
      });
      u32(d, 0x40, ns.length ? ns[0] : 0);
      u32(d, 0x44, ns.length ? ns[ns.length - 1] : 0);
      u32(d, 0x48, 0x48484848);
      u32(d, 0x4c, 0x100 + ns.length);
      r.e.forEach(function (en, k) {
        if (!en) return;
        const e = out.ents.add(0x10 * ne++);
        u32(e, 0, en[0]);
        u32(e, 4, en[1] < 0 ? 0 : vx[en[1]]);
        u32(e, 8, en[2] || 0);
        u32(e, 0xc, en[3] || 0);
        u32(d, 0x50 + 4 * ((r.h + 1 + k) & 0xfff), e);
      });
      out['d' + i] = d;
    });
    stubOn(out.log, 0x196f, 2, true, function (a) {
      a[1].writeU32((a[1].readU32() + a[0].add(0x40f0).readU32()) >>> 0);
      return 0;
    }, [1, 2]);
    stubOn(out.log, 0x1dd9, 1, true, function () { return 0x1dd9; });
    stubOn(out.log, 0x246e, 0, true, function () { return 0x246e; });
    stubOn(out.log, 0x1186, 0, true, function () { return 0x1186; });
    stubOn(out.log, 0x1474, 1, true, function () { return 0x1474; });
    stubOn(out.log, 0x25c2, 1, true, function () { return 0x25c2; });
    stubOn(out.log, 0x23e7, 2, true, function () { return 0x23e7; });
    stubOn(out.log, 0x11db, 2, true, function () { return 0x11db; });
    return out;
  };

  // Midi_Device::shutdown: twelve 0x100-byte devices (+0x2c timer or NULL, +0x3c MIDI-out handle or 0). The track list (head
  // 0x100b5000, tail 0x100b5004, cursor 0x100b5008, count 0x100b500c) holds recording voices (vtable slot 0 one argument, +0xac
  // flags) and the channel list (head 0x100b4fe8, tail 0x100b4fec, count 0x100b4ff4) payload blocks; both are swapped per key (the
  // lists are the same for every vector of a key; the devices vary). g_soundTimer 0x100b49ec is a fixture block. killEvent
  // 0x100026fd, unregisterClient 0x10001ed3, drainCommandQueue 0x1000259a and Seq_freeChannelObjects 0x10001d66 are recording stubs;
  // midiOutClose is stood in; operator delete records.
  const SD = {
    a: { tr: [], ch: [] },
    b: { tr: [0, 1, 2], ch: [1, 1] },
    c: { tr: [0, 1, 2, 3, 4], ch: [1, 0, 1] },
    d: { tr: [0, 1, 1, 2], ch: [0] },
  };
  function shutFixture(k) {
    const cfg = SD[k];
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    heap(out, 0x100);
    const vt = recVtable(out.log, { 0: 1 });
    out.vox = block(0, 0x0e000000, 0x200 * 5);
    const vx = [];
    for (let j = 0; j < 5; j++) {
      const v = out.vox.add(0x200 * j);
      v.writePointer(vt);
      fill(v.add(0x100), 0xe0, 0);
      u32(v, 0xac, 0x7f + j);
      vx.push(v);
    }
    out.tn = fill(keep(Memory.alloc(0x10 * 8)), 0x80, 0x4b);
    const tns = cfg.tr.map(function (x, j) { return out.tn.add(0x10 * j); });
    tns.forEach(function (n, j) {
      u32(n, 0, j ? tns[j - 1] : 0);
      u32(n, 4, j + 1 < tns.length ? tns[j + 1] : 0);
      u32(n, 8, vx[cfg.tr[j]]);
    });
    swapGlobal(0xb5000, tns.length ? tns[0] : 0, out, 'gth');
    swapGlobal(0xb5004, tns.length ? tns[tns.length - 1] : 0, out, 'gtt');
    swapGlobal(0xb5008, 0x58585858, out, 'gtc');
    swapGlobal(0xb500c, 0x100 + tns.length, out, 'gtn');
    out.cn = fill(keep(Memory.alloc(0x10 * 8)), 0x80, 0x4a);
    out.cp = block(0, 0x9c000000, 0x20 * 8);
    const cns = cfg.ch.map(function (x, j) { return out.cn.add(0x10 * j); });
    cns.forEach(function (n, j) {
      u32(n, 0, j ? cns[j - 1] : 0);
      u32(n, 4, j + 1 < cns.length ? cns[j + 1] : 0);
      u32(n, 8, cfg.ch[j] ? out.cp.add(0x20 * j) : 0);
    });
    swapGlobal(0xb4fe8, cns.length ? cns[0] : 0, out, 'gch');
    swapGlobal(0xb4fec, cns.length ? cns[cns.length - 1] : 0, out, 'gct');
    swapGlobal(0xb4ff4, 0x100 + cns.length, out, 'gcn');
    out.tmr = block(0, 0x54000000, 0x40);
    swapGlobal(0xb49ec, out.tmr, out, 'gtm');
    for (let i = 0; i < 12; i++) {
      const d = block(i, 0x9e400000, 0x100);
      u32(d, 0x2c, i % 3 === 0 ? 0 : block(i, 0x2c000000, 0x20));
      u32(d, 0x3c, i % 4 === 1 ? 0 : 0x3c000000 + i);
      out['d' + i] = d;
    }
    stubOn(out.log, 0x26fd, 0, true, function () { return 0x26fd; });
    stubOn(out.log, 0x1ed3, 1, true, function () { return 0x1ed3; });
    stubOn(out.log, 0x259a, 0, true, function () { return 0x259a; });
    stubOn(out.log, 0x1d66, 0, true, function () { return 0x1d66; });
    out.implog = __importStandIn({ module: 'sound.dll', imports: { 'WINMM.dll!midiOutClose': { args: 1, ret: 0 } } }).log;
    return out;
  }
  ['a', 'b', 'c', 'd'].forEach(function (k) { F['c3fd_shutdown_' + k] = function () { return shutFixture(k); }; });
})();
