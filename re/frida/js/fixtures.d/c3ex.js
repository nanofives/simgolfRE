// Fixtures for C3 batch c3ex (sound.dll, round 32). Layouts come from the disassembly cited in shim/src/re/c3ex.cpp. Every
// object, vtable and log here is allocated by the fixture; no live sound object, device, list, window or file is handed to an
// arm. Helpers copied from re/frida/js/fixtures.d/c3dt.js (freeze guard, quiet timer word, swapped module global, recording
// stub on an incremental-link thunk that only the A/B thread takes). Module globals a fixture writes are saved first and
// restored by re/frida/js/freeze.js before the game's first thread is resumed. All values are synthetic.
//
// Recording objects (group B): a block of 0x200 bytes (larger where the body reads further) whose vtable slots are thiscall
// NativeCallbacks that log {0x5100 + slot, this, arguments} and return the dword at this + 0x100 + slot (the row's answer for that
// slot, inside the compared object). A block may carry a poke at +0x1e0..+0x1ef (target pointer, trigger tag, value, target
// offset): when the callback or stub named by the trigger runs on that block, the value is written to target + offset, which is
// how a callee changes its caller's state in a row. Thunk stubs read their configuration from the same block area
// (+0x1f0 .. +0x1ff), so every result depends only on the arguments and compared memory.
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
  // The re-entrancy word 0x100b49a8 is preset to 5 so SoundTimer::onTick leaves at once (0x10008edf, c3dh notes D) and the
  // multimedia-timer thread does not read the globals a key swaps; saved first, restored at the flush, compared as state.
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
    const sleep = new NativeFunction(Process.getModuleByName('kernel32.dll').getExportByName('Sleep'), 'void', ['uint32'], 'stdcall');
    for (let i = 0; i < 20; i++) { sleep(2); if (g.readU32() === 5) break; g.writeU32(5); }
  }
  function swapGlobal(rva, value, out, name) {
    const g = snd(rva);
    if (name) out[name] = g;
    __freeze.setU32(g, value);
    return g;
  }

  // ---- call log and recording stub (A/B thread only) ------------------------------------------------------------
  const LOGSZ = 0x420;                       // count at +0, then 0x20-byte entries (33 of them)
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
  // mask: indices of arguments (0 = ecx for thiscall) that are the caller's stack slots; they are logged as 0x5ac0 because the two
  // arms' frames differ (the value written through them is what the comparison sees)
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
  // a vtable whose listed slots ({offset: stack argument count}) log and return the object's answer dword this+0x100+slot
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
  // a recording object: vtable at +0, then cfg {offset: value or pointer}
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

  // ===================================================================================================================
  // A. field stores and a forwarder on fixture objects
  // ===================================================================================================================
  // setField6cReturn0 / clearField6cReturn0: twelve 0x100-byte objects, each with its own marker words and a distinct prior
  // value at +0x6c (zero in two of them, so a clear that writes nothing is one of the rows).
  const PRIOR6C = [0, 1, 0x1234, 0xffffffff, 0x80000000, 0, 0x5555, 2, 0x100, 0xdeadbeef, 0x41, 0x7fffffff];
  // (every fixture freezes the first thread through guard(), which also arms the negative-control switch; the three fixtures
  //  without a stub or a swapped global call it directly)
  F.c3ex_f6c = function () {
    const out = {};
    guard();
    PRIOR6C.forEach(function (v, i) {
      const b = block(i, 0x6c000000, 0x100);
      u32(b, 0x6c, v);
      out['o' + i] = b;
    });
    return out;
  };

  // Ch5c2ec_forwardSlot37: twelve outer objects (+0x40 = NULL in rows 0, 4, 8) and their child objects; the child vtable's
  // slot +0x94 logs the child and returns the child's dword +8 (a per-row value).
  const CHILDV = [0, 1, 0x1234, 0xffffffff, 0, 0x80000000, 7, 0x100, 0, 0x55aa55aa, 0x13, 2];
  F.c3ex_fwd37 = function () {
    const out = {};
    guard();
    out.log = stubLog();
    const vt = fill(keep(Memory.alloc(0xa0)), 0xa0, 0);
    vt.add(0x94).writePointer(keep(new NativeCallback(function (self) {
      logCall(out.log, 0x94, [self]);
      return self.add(8).readU32();
    }, 'uint32', ['pointer'], 'thiscall')));
    for (let i = 0; i < 12; i++) {
      const o = block(i, 0x40000000, 0x100);
      const c = block(i, 0x41000000, 0x100);
      c.writePointer(vt);
      u32(c, 8, CHILDV[i]);
      o.add(0x40).writePointer(i % 4 === 0 ? ptr(0) : c);
      out['o' + i] = o;
      out['c' + i] = c;
    }
    return out;
  };

  // Channel_start: g_soundEngine 0x100b4a20 swapped (NULL, or a fixture engine whose slot +0x6c logs and returns `slot`);
  // WaveCmdQueue::pushNode's thunk 0x100020b3 is a recording stub.
  const FLAGS58 = [0, 1, 0x24, 0xffffffde, 0x20, 0x25, 4, 0x80000000, 0xffffffff, 0x101, 0x7ffffffe, 0xdb];
  function chStartFixture(withEng, slot) {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    for (let i = 0; i < 12; i++) {
      const b = block(i, 0x58000000, 0x100);
      u32(b, 0x58, FLAGS58[i]);
      out['ch' + i] = b;
    }
    if (withEng) {
      const vt = fill(keep(Memory.alloc(0x80)), 0x80, 0);
      vt.add(0x6c).writePointer(keep(new NativeCallback(function (self) {
        logCall(out.log, 0x6c, [self]);
        return slot >>> 0;
      }, 'uint32', ['pointer'], 'thiscall')));
      out.eng = block(0, 0x5e000000, 0x200);
      out.eng.writePointer(vt);
      swapGlobal(0xb4a20, out.eng.toUInt32(), out, 'gdev');
    } else {
      swapGlobal(0xb4a20, 0, out, 'gdev');
    }
    stubOn(out.log, 0x20b3, 2, true, function (a) { return ((a[1].toUInt32() & 0xffff) + a[2].toUInt32()) >>> 0; });
    return out;
  }
  F.c3ex_chstart_ok = function () { return chStartFixture(true, 0); };
  F.c3ex_chstart_busy = function () { return chStartFixture(true, 7); };
  F.c3ex_chstart_null = function () { return chStartFixture(false, 0); };

  // ===================================================================================================================
  // B. channel / device wrappers over create_sound, Engine_playSound and their objects' virtual slots
  // ===================================================================================================================
  // Thunk stubs shared by group B (A/B thread only, recorded in the key's log):
  //   create_sound 0x1000137a (cdecl out, a, type): the owner is out - 0x40; a non-NULL owner+0x1f0 is written to *out, and
  //     owner+0x1f4 is returned.
  //   Engine_playSound 0x100026e9 (thiscall, a): returns this+0x1f8.
  //   delete_sound 0x10001a87 (cdecl, the object): returns 0.
  function soundStubs(log) {
    stubOn(log, 0x137a, 3, 'cdecl', function (a) {
      const owner = a[0].sub(0x40);
      const child = owner.add(0x1f0).readPointer();
      if (!child.isNull()) a[0].writePointer(child);
      return owner.add(0x1f4).readU32();
    });
    stubOn(log, 0x26e9, 1, true, function (a) { return a[0].add(0x1f8).readU32(); });
    stubOn(log, 0x1a87, 1, 'cdecl', function () { return 0; });
  }
  // rows: {pre: child already in +0x40, mk: child create_sound writes, cr: create result, play: Engine_playSound result,
  //        ans: {slot: answer} on the owner, cans: on the child, cfg: other owner dwords, ccfg: child dwords, cpoke: poke on the child}
  function ownerRows(out, rows, ownerSlots, childSlots, ownerSize) {
    out.log = stubLog();
    const ovt = recVtable(out.log, ownerSlots), cvt = recVtable(out.log, childSlots);
    rows.forEach(function (r, i) {
      const cans = {};
      Object.keys(r.cans || {}).forEach(function (k) { cans[0x100 + parseInt(k, 10)] = r.cans[k]; });
      const child = recObj(i, 0x2c000000, cvt, merge(cans, r.ccfg || {}));
      const ans = {};
      Object.keys(r.ans || {}).forEach(function (k) { ans[0x100 + parseInt(k, 10)] = r.ans[k]; });
      const cfg = merge({ 0x40: r.pre ? child : 0, 0x1f0: r.mk ? child : 0, 0x1f4: r.cr || 0, 0x1f8: r.play || 0 }, ans, r.cfg || {});
      const o = recObj(i, 0x2b000000, ovt, cfg, ownerSize);
      if (r.cpoke) Object.keys(r.cpoke).forEach(function (k) { u32(child, parseInt(k, 10), k === '480' ? o : r.cpoke[k]); });
      out['o' + i] = o;
      out['c' + i] = child;
    });
  }

  // SubChannel_openSourceType2 (one argument per row in the registry)
  F.c3ex_src2 = function () {
    const out = {};
    ownerRows(out, [
      { mk: 1, cfg: { 0x58: 2, 0x5c: 0x77 } },
      { cr: 5 },
      { pre: 1 },
      { pre: 1 },
      { mk: 1, play: 3, cfg: { 0x58: 2 } },
      { mk: 1, cfg: { 0x58: 2, 0x5c: 0 } },
      { pre: 1, cfg: { 0x58: 0x02, 0x5c: 0x1234 } },
      { mk: 1, cfg: { 0x58: 0xfd } },
      { pre: 1, play: 9 },
      { cr: 0x80000000 },
      { mk: 1, cfg: { 0x58: 0xff, 0x5c: 0xffffffff }, ccfg: { 0x58: 0x55 } },
      { pre: 1 },
    ], {}, {});
    soundStubs(out.log);
    return out;
  };

  // openSourceType4 / 6 / 7: owner slots +0x7c and +0x14, child slot +0x84 with 4 (type 4) or 3 arguments
  const SRC_ROWS = [
    { pre: 1, cans: { 0x84: 0 } },
    { pre: 1, cans: { 0x84: 5 } },
    { mk: 1, cans: { 0x84: 0 } },
    { cr: 3 },
    {},
    { pre: 1, cans: { 0x84: 0x10 } },
    { mk: 1, cans: { 0x84: 0xffffffff } },
    { pre: 1, cans: { 0x84: 1 } },
    { cr: 0x80000000 },
    { mk: 1, cans: { 0x84: 0x10 } },
    { pre: 1, cans: { 0x84: 0 }, ans: { 0x7c: 0x7c7c } },
    { mk: 1, cans: { 0x84: 7 } },
  ];
  function srcFixture(nargs) {
    const out = {};
    ownerRows(out, SRC_ROWS, { 0x7c: 0, 0x14: 0 }, { 0x84: nargs });
    soundStubs(out.log);
    return out;
  }
  F.c3ex_src4 = function () { return srcFixture(4); };
  F.c3ex_src3 = function () { return srcFixture(3); };

  // Device_createStartQueue: owner slots +0x58, +0x7c; child slots +0x6c (1), +0x84 (0), +0x48 (1)
  F.c3ex_startq = function () {
    const out = {};
    ownerRows(out, [
      { cfg: { 0x50: 0 } },
      { cfg: { 0x50: 5 }, cr: 2 },
      { pre: 1, cfg: { 0x50: 5, 0x58: 1, 0x44: 0, 0x30: 0 } },
      { pre: 1, cfg: { 0x50: 5, 0x58: 0, 0x44: 0 }, ans: { 0x58: 1 }, cans: { 0x84: 4 } },
      { pre: 1, cfg: { 0x50: 6, 0x44: 1 } },
      { mk: 1, cfg: { 0x50: 7, 0x58: 1, 0x44: 0x10, 0x30: 1 }, ans: { 0x58: 7 } },
      { pre: 1, cfg: { 0x50: 8, 0x44: 0xfffffffe, 0x30: 5 } },
      { pre: 1, cfg: { 0x50: 9, 0x58: 0xff }, cans: { 0x84: 0x80000000 } },
      { mk: 1, cfg: { 0x50: 0xa, 0x44: 0, 0x30: 0 } },
      { pre: 1, cfg: { 0x50: 0xffffffff, 0x58: 0xfe, 0x44: 2, 0x30: 1 }, ans: { 0x58: 1 } },
      { pre: 1, cfg: { 0x50: 0xb, 0x44: 1 } },
      { cfg: { 0x50: 0xc }, cr: 0x13 },
    ], { 0x58: 0, 0x7c: 0 }, { 0x6c: 1, 0x84: 0, 0x48: 1 });
    soundStubs(out.log);
    return out;
  };

  // Device_lazyCreateSound: owner slot +0x58; child slots +0x6c (1), +0xc8 (0). The +0x58 dword covers bits 0..6.
  F.c3ex_lazy = function () {
    const out = {};
    ownerRows(out, [
      { cfg: { 0x50: 0 } },
      { cfg: { 0x50: 3 }, cr: 6 },
      { pre: 1, cfg: { 0x50: 3, 0x58: 0 }, cans: { 0xc8: 0x64 } },
      { pre: 1, cfg: { 0x50: 3, 0x58: 1 }, ans: { 0x58: 1 } },
      { pre: 1, cfg: { 0x50: 3, 0x58: 4 }, play: 3 },
      { pre: 1, cfg: { 0x50: 3, 0x58: 8 }, cans: { 0xc8: 0x1111 } },
      { pre: 1, cfg: { 0x50: 3, 0x58: 0x28 } },
      { mk: 1, cfg: { 0x50: 4, 0x58: 0x10 }, cans: { 0xc8: 0x2222 } },
      { pre: 1, cfg: { 0x50: 3, 0x58: 0x20 } },
      { pre: 1, cfg: { 0x50: 3, 0x58: 0x40 }, ans: { 0x58: 9 } },
      { pre: 1, cfg: { 0x50: 3, 0x58: 0x7f }, play: 0x80000000 },
      { pre: 1, cfg: { 0x50: 0xffffffff, 0x58: 0xffffffff }, ans: { 0x58: 0 }, cans: { 0xc8: 0xffffffff } },
    ], { 0x58: 0 }, { 0x6c: 1, 0xc8: 0 });
    soundStubs(out.log);
    return out;
  };

  // Device_prepareSoundFlags: owner slot +0x58; child slots +0x6c, +0xc8, +0x40, +0x9c, +0x44. Row 10's child slot +0x6c
  // sets bit 1 of the owner's +0x58 byte, so the re-read at 0x1002bcca takes its other side.
  F.c3ex_prep = function () {
    const out = {};
    ownerRows(out, [
      { cr: 4 },
      { pre: 1, cfg: { 0x58: 0, 4: 0x44, 8: 0x88, 0x5c: 0x5c5c }, cans: { 0xc8: 0x300 } },
      { pre: 1, cfg: { 0x58: 2 } },
      { pre: 1, cfg: { 0x58: 1 }, ans: { 0x58: 1 }, play: 5 },
      { pre: 1, cfg: { 0x58: 8 } },
      { pre: 1, cfg: { 0x58: 0x10 } },
      { pre: 1, cfg: { 0x58: 0x40 } },
      { mk: 1, cfg: { 0x58: 0x59 } },
      { pre: 1, cfg: { 0x58: 0xff } },
      { pre: 1, cfg: { 0x58: 0xfd }, ans: { 0x58: 3 } },
      { pre: 1, cfg: { 0x58: 0 }, cpoke: { 480: 1, 0x1e4: 0x5100 + 0x6c, 0x1e8: 2, 0x1ec: 0x58 } },
      { mk: 1, cfg: { 0x58: 0 }, play: 0x80000000 },
    ], { 0x58: 0 }, { 0x6c: 1, 0xc8: 0, 0x40: 1, 0x9c: 1, 0x44: 1 });
    soundStubs(out.log);
    return out;
  };

  // startDevicePlayback: owner slot +0x80; child slot +0x1c. Device_lazyCreateSound's thunk 0x100016f4 is a stub that stores the
  // owner's +0x1fc in +0x40; timeGetTime is stood in (one value). Rows 9 / 10 poke the owner's +0x58 from the child's slot +0x1c.
  F.c3ex_devplay = function () {
    const out = {};
    ownerRows(out, [
      { pre: 1, cfg: { 0x58: 0 } },
      { pre: 1, cfg: { 0x58: 0x10 }, cans: { 0x1c: 5 } },
      { cfg: { 0x58: 0 } },
      { cfg: { 0x58: 0x10, 0x1fc: 1 }, cans: { 0x1c: 7 } },
      { cfg: { 0x58: 0x10 } },
      { pre: 1, cfg: { 0x58: 0xef }, cans: { 0x1c: 2 } },
      { cfg: { 0x58: 0xff, 0x1fc: 1 } },
      { pre: 1, cfg: { 0x58: 0xff }, cans: { 0x1c: 0x80000000 } },
      { cfg: { 0x58: 0xef } },
      { pre: 1, cfg: { 0x58: 0 }, cpoke: { 480: 1, 0x1e4: 0x5100 + 0x1c, 0x1e8: 0x10, 0x1ec: 0x58 } },
      { cfg: { 0x58: 0x10, 0x1fc: 1 }, cpoke: { 480: 1, 0x1e4: 0x5100 + 0x1c, 0x1e8: 0, 0x1ec: 0x58 } },
      { pre: 1, cfg: { 0x58: 0x30 }, cans: { 0x1c: 1 } },
    ], { 0x80: 0 }, { 0x1c: 0 });
    // +0x1fc = 1 marks rows whose lazy-create stub installs the row's child
    for (let i = 0; i < 12; i++) {
      const o = out['o' + i];
      if (o.add(0x1fc).readU32() === 1) o.add(0x1fc).writePointer(out['c' + i]);
      else o.add(0x1fc).writeU32(0);
    }
    stubOn(out.log, 0x16f4, 0, true, function (a) { a[0].add(0x40).writePointer(a[0].add(0x1fc).readPointer()); return 0x16f4; });
    out.implog = __importStandIn({ module: 'sound.dll', imports: { 'WINMM.dll!timeGetTime': { args: 0, ret: 0x7f3a1234 } } }).log;
    return out;
  };

  // SoundGroup_stop: groups of 0x1500 bytes (count +0x1438, members from +0x1440, flag byte +0xc8, +0x44) whose slots +0x5c and
  // 0 (the deleting destructor, one argument) are recorded. A member is itself a group (count 0, flag byte alternating) reached
  // through the thunk 0x100016a4, which jumps to this function's hooked entry, so the nested call runs the same body in both arms
  // (Frida's Interceptor follows that thunk's `jmp` to the hook's detour, so the thunk cannot be stubbed apart from the function).
  // Channel_releaseBuffers' thunk 0x10001d11 (ecx = group+0x70) is a stub. A member's slot +0x5c applies its poke (rows 4, 7, 11
  // change the owner's count or clear the member's own slot while the owner's walk is at it).
  F.c3ex_gstop = function () {
    const out = {};
    out.log = stubLog();
    const ovt = recVtable(out.log, { 0x5c: 0, 0: 1 }), mvt = ovt;
    const rows = [
      { n: 0, c8: 2 }, { n: 1, c8: 0 }, { n: 3, m: [1, 1], c8: 2 }, { n: 4, m: [1, 0, 1], c8: 0 },
      { n: 2, m: [1], c8: 2, pk: [0, 0x1438, 5] }, { n: 0xffffffff, c8: 0xff }, { n: 6, m: [0, 0, 0, 0, 0], c8: 2 },
      { n: 3, m: [1, 1], c8: 0, pk: [0, 0x1438, 1] }, { n: 0x80000000, c8: 2 }, { n: 5, m: [1, 1, 1, 1], c8: 3 },
      { n: 2, m: [0], c8: 2 }, { n: 3, m: [1, 1], c8: 0, pk: [1, 0x1444, 0] },
    ];
    rows.forEach(function (r, i) {
      const o = recObj(i, 0x2a000000, ovt, { 0x1438: r.n, 0xc8: r.c8 | 0x3b3b3b00, 0x44: 0xffffffff - i }, 0x1500);
      out['o' + i] = o;
      for (let j = 0; j < 8; j++) u32(o, 0x1440 + 4 * j, 0);   // slots past the members (a poke can raise the count) are NULL
      (r.m || []).forEach(function (has, j) {
        let m = ptr(0);
        if (has) {
          const cfg = { 0x1438: 0, 0xc8: (j & 1) ? 0x3b3b3b02 : 0x3b3b3b00, 0x44: 0x44440000 + j };
          if (r.pk && r.pk[0] === j) Object.assign(cfg, pokeCfg(o, 0x5100 + 0x5c, r.pk[2], r.pk[1]));
          m = recObj(i * 8 + j, 0x29000000, mvt, cfg, 0x1500);
          out['m' + i + '_' + j] = m;
        }
        o.add(0x1440 + 4 * j).writePointer(m);
      });
    });
    stubOn(out.log, 0x1d11, 0, true, function (a) { return a[0].add(0x10).readU32(); });
    return out;
  };

  // Channel_setVolume: 0x1500-byte channels (+0x58 flags, +0x54 buffer, 16 slots from +0x318 every 0x110); DsBuffer::setVolume's
  // thunk 0x100018f7 is a stub (thiscall, one argument) that only records.
  F.c3ex_chvol = function () {
    const out = {};
    out.log = stubLog();
    const bufs = [];
    for (let k = 0; k < 18; k++) bufs.push(block(k, 0x18000000, 0x40));
    const rows = [
      { f: 0 }, { f: 0x10000000 }, { f: 0x10000000, b: 1 }, { f: 0x10000000, b: 1, s: [0, 5, 15] },
      { f: 0xefffffff, b: 1, s: [1] }, { f: 0xffffffff, b: 1, s: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15] },
      { f: 0x10000000, s: [3] }, { f: 0x30000000, b: 1, s: [7, 8] }, { f: 0x1000000f, b: 1, s: [15] },
      { f: 0x90000000, b: 1, s: [0, 2, 4, 6, 8, 10, 12, 14] }, { f: 0x10000000, b: 1, s: [1, 3, 5, 7, 9, 11, 13, 15] }, { f: 0x80000000, b: 1 },
    ];
    rows.forEach(function (r, i) {
      const c = block(i, 0x35000000, 0x1500);
      u32(c, 0x58, r.f);
      u32(c, 0x54, r.b ? bufs[i % 18] : 0);
      for (let k = 0; k < 16; k++) u32(c, 0x318 + 0x110 * k, (r.s || []).indexOf(k) >= 0 ? bufs[(k + i) % 18] : 0);
      out['ch' + i] = c;
    });
    stubOn(out.log, 0x18f7, 1, true, function () { return 0; });
    return out;
  };

  // Channel_set3DBufferStub: +0x5c byte, +0x54 object; stubReturnBC's thunk 0x10002522 is a stub returning object+0x10 + argument
  F.c3ex_3dstub = function () {
    const out = {};
    out.log = stubLog();
    const bytes = [0, 0x20, 0x20, 0xff, 0xdf, 0x20, 0x21, 0x60, 0xa0, 0x20, 0xff, 0x10];
    const has = [1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1];
    for (let i = 0; i < 12; i++) {
      const b = block(i, 0x37000000, 0x40);
      const c = block(i, 0x5c000000, 0x100);
      c.add(0x5c).writeU8(bytes[i]);
      u32(c, 0x54, has[i] ? b : 0);
      out['ch' + i] = c;
      out['b' + i] = b;
    }
    stubOn(out.log, 0x2522, 1, true, function (a) { return (a[0].add(0x10).readU32() + a[1].toUInt32()) >>> 0; });
    return out;
  };

  // DsBuffer::getPosition: +0x5c descriptor (byte +0x10); getTimedPosition 0x1000115e (thiscall, 3) and getPlayPosition
  // 0x10001695 (thiscall, 2) are stubs whose results mix their arguments with this+0x20.
  F.c3ex_dspos = function () {
    const out = {};
    out.log = stubLog();
    const db = [null, 0x20, 0, 0xff, 0xdf, 0x20, 0x21, null, 0x10, 0x60, 0, 0xa0];
    for (let i = 0; i < 12; i++) {
      const d = block(i, 0x5d000000, 0x40);
      const s = block(i, 0x10000000, 0x100);
      if (db[i] !== null) d.add(0x10).writeU8(db[i]);
      u32(s, 0x5c, db[i] === null ? 0 : d);
      out['s' + i] = s;
      out['d' + i] = d;
    }
    stubOn(out.log, 0x115e, 3, true, function (a) {
      return (a[0].add(0x20).readU32() ^ a[1].toUInt32() ^ (a[2].toUInt32() << 1) ^ (a[3].toUInt32() << 2)) >>> 0;
    });
    stubOn(out.log, 0x1695, 2, true, function (a) { return (a[0].add(0x24).readU32() + a[1].toUInt32() - a[2].toUInt32()) >>> 0; });
    return out;
  };

  // Device::serviceVoices: devices (+0x40 list head, +0x48 cursor, +0x34 argument); nodes {+4 next, +8 voice}; voices with the
  // +0xac flag byte. Midi_Device::dispatchCommand's thunk 0x10001f4b is a recording stub; Voice::update's thunk 0x10001c4e is a
  // stub applying the voice's poke (rows 6 / 9 clear or move the device's cursor).
  F.c3ex_svc = function () {
    const out = {};
    out.log = stubLog();
    // rows: list of voices per node (0 = node without voice, else the flag byte), optional poke [node index, value: 'null' | node]
    const rows = [
      [], [0], [0x20], [0x20, 0x20, 0x20], [0xdf, 0x20], [0x20, 0, 0x20], [0x20, 0x20, 0x20], [0xff],
      [0x00, 0x00], [0x20, 0x20, 0x20, 0x20], [0x21, 0x60, 0x10], [0x20, 0x20],
    ];
    const pokes = { 6: [0, 'null'], 9: [1, 3] };
    rows.forEach(function (vs, i) {
      const d = block(i, 0x0a000000, 0x100);
      u32(d, 0x34, 0x3434 + i);
      const nodes = vs.map(function (f, j) { return block(i * 8 + j, 0x0b000000, 0x40); });
      nodes.forEach(function (n, j) {
        u32(n, 4, j + 1 < nodes.length ? nodes[j + 1] : 0);
        if (vs[j]) {
          const v = block(i * 8 + j, 0x0c000000, 0x200);
          v.add(0xac).writeU8(vs[j]);
          if (pokes[i] && pokes[i][0] === j) {
            const val = pokes[i][1] === 'null' ? 0 : nodes[pokes[i][1]].toUInt32();
            Object.keys(pokeCfg(d, 0xffffffff, val, 0x48)).forEach(function (k) { u32(v, parseInt(k, 10), pokeCfg(d, 0xffffffff, val, 0x48)[k]); });
          }
          u32(n, 8, v);
          out['v' + i + '_' + j] = v;
        } else {
          u32(n, 8, 0);
        }
        out['n' + i + '_' + j] = n;
      });
      u32(d, 0x40, nodes.length ? nodes[0] : 0);
      out['d' + i] = d;
    });
    stubOn(out.log, 0x1f4b, 0, true, function () { return 0x1f4b; });
    stubOn(out.log, 0x1c4e, 1, true, function (a) { poke(a[0], 0x1c4e); return 0; });
    return out;
  };

  // Sound::ctor: twelve 0xb00-byte blocks with distinct prior contents (Sound_setSourceType 0x1002ad10 runs for real and ORs into
  // +0x44); Sound5b6a8_ctor's thunk 0x10002513 is a recording stub.
  F.c3ex_sctor = function () {
    const out = {};
    out.log = stubLog();
    for (let i = 0; i < 12; i++) {
      const b = block(i, 0x3f000000, 0xb00);
      u32(b, 0x44, [0, 1, 0x10, 0xffffffff, 0x80000000, 0x100, 0xeeeeeeef, 2, 0x8, 0x28, 0x55, 0x7fffffff][i]);
      out['s' + i] = b;
    }
    stubOn(out.log, 0x2513, 0, true, function (a) { return a[0]; });
    return out;
  };

  // enqueueWaveCmdE: operator new 0x100424d7 carves from a fixture arena (heap stand-in); g_soundEngine swapped (a fixture engine
  // or NULL); WaveCmdQueue::pushValue's thunk 0x100011c7 is a stub returning a mix of the node's four dwords. pairStore 0x10008810
  // runs for real.
  function enqFixture(withEng) {
    const out = {};
    quietTimer(out);
    out.log = stubLog();
    for (let i = 0; i < 12; i++) out['o' + i] = block(i, 0x2f000000, 0x40);
    out.eng = block(0, 0x5e000000, 0x100);
    swapGlobal(0xb4a20, withEng ? out.eng.toUInt32() : 0, out, 'gdev');
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], arenaSize: 0x400 });
    out.arena = h.arena;
    stubOn(out.log, 0x11c7, 1, true, function (a) {
      const n = a[1];
      return (n.readU32() + n.add(4).readU32() * 3 + n.add(8).readU32() * 5 + n.add(0xc).readU32() * 7) >>> 0;
    });
    return out;
  }
  F.c3ex_enqe_eng = function () { return enqFixture(true); };
  F.c3ex_enqe_null = function () { return enqFixture(false); };

  // Track_setData: tracks (+0x3c old data, +0x34 flags, +0x2c, +0x40); operator delete 0x1004249a only records (heap stand-in);
  // Midi_scanTrack's thunk 0x1000215d is a stub returning the track's +0x1f0.
  F.c3ex_track = function () {
    const out = {};
    out.log = stubLog();
    const olds = [0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 0];
    const fl = [4, 4, 0, 0xff, 0, 4, 0xfb, 4, 0x0c, 4, 0, 0xff];
    for (let i = 0; i < 12; i++) {
      const t = block(i, 0x1e000000, 0x200);
      const d = block(i, 0x1d000000, 0x40);
      u32(t, 0x3c, olds[i] ? d : 0);
      u32(t, 0x34, fl[i]);
      u32(t, 0x1f0, 0x5ca00000 + i);
      out['t' + i] = t;
      out['data' + i] = d;
    }
    const h = __heapStandIn({ module: 'sound.dll', free: [0x4249a], arenaSize: 0x100 });
    out.freelog = h.log;
    stubOn(out.log, 0x215d, 0, true, function (a) { return a[0].add(0x1f0).readU32(); });
    return out;
  };

  // SoundWindowObj_subclass: objects (+4 flag byte, +8 subclassed window, +0xc old procedure); SetWindowLongA is stood in
  // (result 0 for a zero window, else the window xor 0x5a5a0000)
  F.c3ex_subclass = function () {
    const out = {};
    guard();
    const w8 = [0, 0, 0x1234, 0, 0, 0, 0xffffffff, 0, 0, 0, 0, 0];
    for (let i = 0; i < 12; i++) {
      const b = block(i, 0x28000000, 0x40);
      u32(b, 8, w8[i]);
      b.add(4).writeU8([0, 4, 0, 0xff, 0xfb, 1, 0, 0x80, 0, 2, 0x7f, 0][i]);
      out['w' + i] = b;
    }
    out.implog = __importStandIn({ module: 'sound.dll', imports: {
      'USER32.dll!SetWindowLongA': { args: 3, impl: function (a) { const h = a[0].toUInt32(); return h === 0 ? 0 : (h ^ 0x5a5a0000) >>> 0; } } } }).log;
    return out;
  };

  // startSignalThread: objects (+0 flag byte, +0x20 thread handle, +0x24 event handle); ResetEvent and SetThreadPriority are
  // stood in (results mix their arguments); _beginthread 0x10042984 is a recording stub returning a fixed handle.
  F.c3ex_sigthread = function () {
    const out = {};
    out.log = stubLog();
    const f = [0, 1, 0xfe, 0xff, 0x80, 0x81, 0, 3, 0x10, 0x11, 0x7e, 0x7f];
    for (let i = 0; i < 12; i++) {
      const b = block(i, 0x2a900000, 0x40);
      b.writeU8(f[i]);
      u32(b, 0x24, 0x24000000 + 0x111 * i);
      out['t' + i] = b;
    }
    out.implog = __importStandIn({ module: 'sound.dll', imports: {
      'KERNEL32.dll!ResetEvent': { args: 1, impl: function (a) { return (a[0].toUInt32() ^ 0x11110000) >>> 0; } },
      'KERNEL32.dll!SetThreadPriority': { args: 2, impl: function (a) { return (a[0].toUInt32() + a[1].toUInt32() + 1) >>> 0; } } } }).log;
    stubOn(out.log, 0x42984, 3, 'cdecl', function () { return 0x7e00beef; });
    return out;
  };

  // ===================================================================================================================
  // C. DirectSound buffer list
  // ===================================================================================================================
  // The list words 0x100b4a34 (head) / 0x100b4a38 (tail) are swapped for a fixture list of `n` buffers (0x200 bytes, prev +0xbc,
  // next +0xc0, +0x98 a flag dword, +0x1f0 the answer of the releaseIfOwned stub) under the frozen first thread. Stubs:
  // DsBuffer::releaseIfOwned 0x100012d5 (returns +0x1f0, applies the buffer's poke) and DsBuffer::destroy 0x10001910 (records);
  // operator delete 0x1004249a only records (heap stand-in). Devices are 0x200-byte blocks (+0x6c flag byte, +0x158 counter,
  // +0x198 extra buffer, +0 interface, +8, +0x18c counter).
  function dsList(out, n, owned) {
    quietTimer(out);
    const bufs = [];
    for (let j = 0; j < n; j++) {
      const b = block(j, 0x0b000000, 0x200);
      u32(b, 0x98, [0, 0xfffffffb, 4, 0x80000000, 1][j % 5]);
      u32(b, 0x1f0, owned ? owned[j % owned.length] : 0);
      fill(b.add(0x1e0), 0x10, 0);
      bufs.push(b);
      out['b' + j] = b;
    }
    bufs.forEach(function (b, j) {
      u32(b, 0xbc, j > 0 ? bufs[j - 1] : 0);
      u32(b, 0xc0, j + 1 < n ? bufs[j + 1] : 0);
    });
    swapGlobal(0xb4a34, n ? bufs[0].toUInt32() : 0, out, 'ghead');
    swapGlobal(0xb4a38, n ? bufs[n - 1].toUInt32() : 0, out, 'gtail');
    out.nb = block(9, 0x0c000000, 0x200);      // a buffer that is not in the list
    fill(out.nb.add(0x1e0), 0x20, 0);
    u32(out.nb, 0x98, 0);
    out.log = stubLog();
    out.freelog = __heapStandIn({ module: 'sound.dll', free: [0x4249a], arenaSize: 0x100 }).log;
    stubOn(out.log, 0x12d5, 0, true, function (a) { poke(a[0], 0x12d5); return a[0].add(0x1f0).readU32(); });
    stubOn(out.log, 0x1910, 0, true, function () { return 0; });
    return bufs;
  }
  const DSFLAGS = [0, 2, 2, 0xff, 0xfd, 2, 0x02, 0x42, 0, 2, 0x12, 0x7f];
  function dsDevices(out, extra) {
    const ivt = fill(keep(Memory.alloc(0x40)), 0x40, 0);
    ivt.add(8).writePointer(keep(new NativeCallback(function (i) { logCall(out.log, 0x1c08, [i]); return 0; }, 'uint32', ['pointer'], 'stdcall')));
    for (let i = 0; i < 12; i++) {
      const d = block(i, 0x0d000000, 0x200);
      d.add(0x6c).writeU8(DSFLAGS[i]);
      u32(d, 0x158, 0x158 + i);
      u32(d, 0x18c, [0, 1, 0xffffffff, 7, 0x100, 2, 3, 4, 5, 6, 0x7fffffff, 9][i]);
      u32(d, 0x198, 0);
      u32(d, 0, 0);
      if (extra) {
        if (i % 3 !== 0) {
          const x = block(i, 0x0e000000, 0x200);
          fill(x.add(0x1e0), 0x20, 0);
          if (i % 3 === 2) Object.keys(pokeCfg(d, 0x12d5, 0, 0x198)).forEach(function (k) { u32(x, parseInt(k, 10), pokeCfg(d, 0x12d5, 0, 0x198)[k]); });
          u32(d, 0x198, x);
          out['x' + i] = x;
        }
        if (i % 2 === 1) {
          const it = block(i, 0x0f000000, 0x40);
          it.writePointer(ivt);
          u32(d, 0, it);
        }
      }
      out['d' + i] = d;
    }
  }
  F.c3ex_dsrel_0 = function () { const out = {}; dsList(out, 0); dsDevices(out); return out; };
  F.c3ex_dsrel_1 = function () { const out = {}; dsList(out, 1); dsDevices(out); return out; };
  F.c3ex_dsrel_4 = function () { const out = {}; dsList(out, 4, [0, 1, 0x80000000, 0]); dsDevices(out); return out; };
  F.c3ex_dsrm_4 = function () { const out = {}; dsList(out, 4, [0, 1, 0, 0x20]); dsDevices(out); return out; };
  F.c3ex_dsrm_1 = function () { const out = {}; dsList(out, 1, [5]); dsDevices(out); return out; };
  F.c3ex_dsall_0 = function () { const out = {}; dsList(out, 0); dsDevices(out, true); return out; };
  F.c3ex_dsall_3 = function () { const out = {}; dsList(out, 3, [0, 7, 0]); dsDevices(out, true); return out; };

  // ===================================================================================================================
  // D. MultiSound start paths
  // ===================================================================================================================
  // MultiSound blocks of 0x1500 bytes (fields +4, +0x50, +0x58, +0xc8, +0xcc, +0x1428) on a recording vtable (slots +0x10 with one
  // argument, +0xcc). Stubs: Channel_resetPanState 0x10001866, serviceChannelPlayback 0x10001f37, Channel_setVolume 0x100015aa
  // (record), Channel_allocAndInitVoice 0x10001839 (ecx = owner+0x70: writes owner+0x1f4 to *out, returns owner+0x1f0),
  // WaveCmdQueue::pushNode 0x100020b3 (record). g_soundEngine is swapped (a recording engine whose slot +0x6c returns `eng`, or NULL).
  function msRows(out, rows, eng) {
    quietTimer(out);
    out.log = stubLog();
    const vt = recVtable(out.log, { 0x10: 1, 0xcc: 0 });
    rows.forEach(function (r, i) {
      const cfg = { 4: 0x400 + i, 0x50: 0x500 + i, 0x58: r.f58 || 0, 0xc8: r.c8, 0xcc: r.cc || 0, 0x1428: r.b1428 || 0,
                    0x110: r.a10 || 0, 0x1cc: r.acc || 0, 0x1f0: r.alloc || 0, 0x1f4: 0x1f4000 + i };
      out['m' + i] = recObj(i, 0x39000000, vt, cfg, 0x1500);
      out['out' + i] = fill(keep(Memory.alloc(0x10)), 0x10, 0x5a);
    });
    if (eng === null) swapGlobal(0xb4a20, 0, out, 'gdev');
    else {
      out.eng = recObj(0, 0x5e000000, recVtable(out.log, { 0x6c: 0 }), { 0x16c: eng });
      swapGlobal(0xb4a20, out.eng.toUInt32(), out, 'gdev');
    }
    [0x1866, 0x1f37].forEach(function (rva) { stubOn(out.log, rva, 0, true, function () { return 0; }); });
    stubOn(out.log, 0x15aa, 1, true, function () { return 0; });
    stubOn(out.log, 0x20b3, 2, true, function () { return 0; });
    stubOn(out.log, 0x1839, 1, true, function (a) {
      const owner = a[0].sub(0x70);
      a[1].writeU32(owner.add(0x1f4).readU32());
      return owner.add(0x1f0).readU32();
    });
  }
  F.c3ex_msvoice = function () {
    const out = {};
    msRows(out, [
      { c8: 0 }, { c8: 1, f58: 0 }, { c8: 1, f58: 2, a10: 5 }, { c8: 1, f58: 2 }, { c8: 3, alloc: 7, acc: 1 },
      { c8: 0x43 }, { c8: 0x43, alloc: 9 }, { c8: 3, acc: 1, f58: 0x10 }, { c8: 3, alloc: 4, acc: 1, cc: 0x40 },
      { c8: 0xff, alloc: 2 }, { c8: 0x0b, alloc: 3, acc: 2, f58: 0xef, cc: 0xffffffbf }, { c8: 1, f58: 2, alloc: 0x80000000 },
    ], 0);
    return out;
  };
  const MSSTART_ROWS = [
    { c8: 0x40 }, { c8: 0x41, b1428: 1 }, { c8: 0, f58: 0 }, { c8: 0, f58: 2, a10: 6 }, { c8: 1, f58: 2, cc: 4 },
    { c8: 3, acc: 1, f58: 2 }, { c8: 0x43 }, { c8: 2, cc: 0x40, f58: 0x10, acc: 1 }, { c8: 3, cc: 0x44 },
    { c8: 0x41, f58: 2, acc: 1 }, { c8: 2, acc: 1 }, { c8: 3, b1428: 0xfe, acc: 1, f58: 0x12, cc: 0xffffffff },
  ];
  F.c3ex_msstart_ok = function () { const out = {}; msRows(out, MSSTART_ROWS, 0); return out; };
  F.c3ex_msstart_busy = function () { const out = {}; msRows(out, MSSTART_ROWS, 3); return out; };
  F.c3ex_msstart_null = function () { const out = {}; msRows(out, MSSTART_ROWS, null); return out; };

  // ===================================================================================================================
  // E. sequencer track walks
  // ===================================================================================================================
  // Sequencers: 0x800-byte recording objects (slot +0xb4 = the track count, answer at +0x1b4) with +0x20c, +0x214, +0x224, the list
  // word +0xac, +0x1f0 (the load stubs' result) and at +0x400 a lookup table of eight {track, result} pairs that the Seq_listGetAt stub
  // reads (index >= 8 returns 0x24). Tracks: 0x300-byte blocks (+0x2c read pointer, +0x34 flags, +0x38 byte, +0x54 key, +0x58..+0x257
  // filled with markers). Event streams are 4-byte records {delta, status, value, end}: the Midi_readVarLen stub writes the delta and
  // returns 1 when end is 0xff; Track_advancePos moves +0x2c by 4; Midi_Device::playEvent writes the value of the record at +0x2c to the
  // count slot and returns 0x29 for status 0xb0. Stubs only on the A/B thread; every call recorded.
  // (Frida's Interceptor follows a thunk's `jmp` to the hooked entry's detour, so the key of a function never stubs its own thunk:
  //  Seq_preRollTrack's key passes own = 0x1956.)
  function seqStubs(log, own) {
    stubOn(log, 0x15fa, 2, true, function (a) {
      const s = a[0].sub(0x60), i = a[2].toUInt32();
      if (i >= 8) return 0x24;
      const r = s.add(0x404 + 8 * i).readU32();
      if (r === 0) a[1].writePointer(s.add(0x400 + 8 * i).readPointer());
      return r;
    }, [1]);
    stubOn(log, 0x139d, 2, 'cdecl', function (a) { a[1].writeU32(a[0].readU8()); return a[0].add(3).readU8() === 0xff ? 1 : 0; }, [1]);
    stubOn(log, 0x1c7b, 1, true, function (a) { a[0].add(0x2c).writePointer(a[0].add(0x2c).readPointer().add(4)); return 0; });
    stubOn(log, 0x2068, 2, true, function (a) {
      const p = a[1].add(0x2c).readPointer();
      a[2].writeU32(p.add(2).readU8());
      return p.add(1).readU8() === 0xb0 ? 0x29 : 0;
    }, [2]);
    stubOn(log, 0x2298, 1, true, function () { return 0; });
    if (own !== 0x1956) stubOn(log, 0x1956, 1, true, function () { return 0; });
    stubOn(log, 0x270c, 1, true, function (a) { return a[0].sub(0x60).add(0x1f0).readU32(); });
    stubOn(log, 0x169f, 1, true, function (a) { return a[0].sub(0x60).add(0x1f0).readU32(); });
    stubOn(log, 0x1997, 0, true, function () { return 0; });
  }
  function stream(recs) {
    const b = fill(keep(Memory.alloc(0x80)), 0x80, 0);
    recs.forEach(function (r, k) { b.add(4 * k).writeByteArray([r[0], r[1], r[2] || 0, r[3] || 0]); });
    b.add(4 * recs.length).writeByteArray([0, 0, 0, 0xff]);   // every stream ends with a record whose delta read fails
    return b;
  }
  function trackObj(i, j, tr) {
    const t = block(i * 8 + j, 0x7c000000, 0x300);
    u32(t, 0x34, tr.fl || 0);
    t.add(0x38).writeU8(tr.b38 || 0);
    u32(t, 0x54, tr.key || 0);
    t.add(0x2c).writePointer(stream(tr.recs || [[1, 0x90]]));
    return t;
  }
  function seqObj(out, i, vt, o) {
    const s = recObj(i, 0x5e900000, vt, { 0x1b4: o.count || 0, 0x214: o.f214 || 0, 0x224: o.f224 || 0, 0x20c: o.f20c || 0,
                                          0x1f0: o.load || 0, 0xac: 0x0ac0ac00 + i }, 0x800);
    for (let j = 0; j < 8; j++) {
      const tr = (o.tracks || [])[j];
      let t = ptr(0), res = 0x24;
      if (tr) {
        res = tr.res || 0;
        if (!tr.none) { t = trackObj(i, j, tr); out['t' + i + '_' + j] = t; }
      }
      s.add(0x400 + 8 * j).writePointer(t);
      s.add(0x404 + 8 * j).writeU32(res);
    }
    out['s' + i] = s;
    return s;
  }

  // Seq_stepTrackMeta: one sequencer, twelve tracks (vector 0 passes NULL)
  F.c3ex_stepmeta = function () {
    const out = {};
    out.log = stubLog();
    seqObj(out, 0, recVtable(out.log, { 0xb4: 0 }), {});
    const rows = [
      {}, { fl: 2 }, { recs: [[5, 0x90]] }, { recs: [[1, 0x80, 0, 0xff]] }, { recs: [[2, 0x80], [0, 0xb0, 7]] },
      { recs: [[3, 0x80], [0, 0x80, 4], [1, 0xc0]] }, { recs: [[1, 0xa0], [0, 0x80, 2], [5, 0xe0], [0, 0x80, 1], [0, 0x90]] },
      { recs: [[1, 0x80], [0, 0x80]] }, { recs: [[1, 0x91]] }, { recs: [[1, 0xcf]] },
      { fl: 0xfd, recs: [[7, 0xf0], [0, 0x80, 3], [2, 0x80], [0, 0xb0]] }, { fl: 0xff },
    ];
    rows.forEach(function (r, j) { out['k' + j] = trackObj(9, j, r); });
    seqStubs(out.log);
    return out;
  };

  // Seq_preRollTrack: one sequencer, count 10, table entries 0..7 (index 8, 9 not in the table: 0x24; 10, 11 at or above the count)
  F.c3ex_preroll = function () {
    const out = {};
    out.log = stubLog();
    seqObj(out, 0, recVtable(out.log, { 0xb4: 0 }), { count: 10, tracks: [
      { recs: [[1, 0x90]] }, { fl: 2 }, { recs: [[2, 0x80], [0, 0xb0, 7]] }, { res: 0x24, none: true },
      { recs: [[1, 0xa0], [0, 0x80, 2], [5, 0xe0], [0, 0x80, 1], [0, 0xc0]] }, { recs: [[1, 0x80, 0, 0xff]] },
      { fl: 0xfd, recs: [[3, 0x80], [0, 0x80, 9]] }, { recs: [[1, 0x80], [0, 0x80, 1], [1, 0x81], [0, 0x80, 2], [0, 0x9f]] },
    ] });
    seqStubs(out.log, 0x1956);
    return out;
  };

  // Seq_selectTrack: twelve sequencers, each with its own table; vectors (sequencer, data, target index)
  F.c3ex_seltrack = function () {
    const out = {};
    out.log = stubLog();
    const vt = recVtable(out.log, { 0xb4: 0 });
    const rows = [
      { f20c: 0, tracks: [{ key: 5 }, { key: 9 }] }, { f20c: 0, tracks: [{ key: 9 }, { key: 5 }] },
      { f20c: 6, tracks: [{ key: 1 }, { key: 2 }] }, { f20c: 0, tracks: [{ key: 1 }, { res: 0x24, none: true }] },
      { f20c: 0, tracks: [{ key: 1 }, { res: 7, none: true }] }, { f20c: 0, tracks: [{ key: 4 }, { key: 4, fl: 4 }] },
      { f20c: 0, tracks: [{ key: 0xffffffff }, { key: 0 }] }, { f20c: 0, tracks: [{ key: 0 }, { key: 0x80000000, fl: 0xff }] },
      { f20c: 1, tracks: [{ key: 3 }, { key: 3, fl: 6 }] }, { f20c: 0, tracks: [{ key: 8 }, { key: 2, fl: 0xffffffff }] },
      { f20c: 0, tracks: [{ res: 3, none: true }, { key: 2 }] },
      { f20c: 0, tracks: [{ key: 0 }, { key: 0 }, { key: 0 }, { key: 0 }, { key: 0 }, { key: 0 }, { key: 0 }, { key: 1 }] },
    ];
    rows.forEach(function (r, i) { seqObj(out, i, vt, r); });
    seqStubs(out.log);
    return out;
  };

  // Seq::loadFile and Midi_loadMemoryAndPreRoll: twelve sequencers (count, +0x214, +0x224, the load result, tables)
  const LOAD_ROWS = [
    { f214: 0x40, count: 3, tracks: [{}, {}, {}] }, { load: 5, count: 2, tracks: [{}, {}] }, { count: 0 },
    { f214: 0xff, count: 1, tracks: [{}] }, { load: 0x80000000, count: 1, f224: 1 }, { count: 4, tracks: [{}, {}, { res: 0x24, none: true }, {}] },
    { f214: 0xbf, count: 2, f224: 1, tracks: [{}, {}] }, { load: 6, count: 3, f224: 5, tracks: [{}, {}, {}] },
    { count: 2, tracks: [{ res: 9, none: true }, { b38: 0xf7 }] }, { f214: 0x40, load: 1, count: 0 },
    { count: 8, tracks: [{}, {}, {}, {}, {}, {}, {}, {}] }, { f214: 0x40, count: 1, tracks: [{ b38: 0xff }] },
  ];
  function loadFixture() {
    const out = {};
    out.log = stubLog();
    const vt = recVtable(out.log, { 0xb4: 0 });
    LOAD_ROWS.forEach(function (r, i) { seqObj(out, i, vt, r); });
    seqStubs(out.log);
    return out;
  }
  F.c3ex_seqload = loadFixture;
})();
