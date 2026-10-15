// Fixtures for batch c3fu (round 35, 2026-10-14). Reimplementations: shim/src/re/c3fu.cpp. Registry:
// re/frida/registry.d/c3fu.py. Purposes and per-jcc tables: log/c3/c3fu_purpose.md. Notes: log/c3/c3fu_notes.txt.
// Only our own synthetic values: no game text, no live game object handed to a body.
//
// PER-VECTOR STATE (this batch's method). Several bodies take no argument and read or write fixed globals, so a key's
// vectors would all be the same call. diff_hook.js resolves every "$name" argument as fx[name] BEFORE it snapshots the
// state regions (re/frida/js/diff_hook.js: `vec.map(... resolve ...)` precedes `snap()`), so a property of the fixture
// object defined with a GETTER runs once per vector, before both arms. The getters below write that vector's values into
// the globals (each global saved once with __freeze.save, written back at the flush) and return a fixture pointer that
// the registry passes as `this` (ecx). Every body that uses it never reads ecx (cited in c3fu.cpp per function). Both arms
// of a vector start from the snapshot taken after the getter ran; the next vector's getter overwrites the same words.
//
// Shared helpers: re/frida/js/freeze.js (first thread frozen, globals restored newest first before the resume, negOn arms
// the negative-control switch), importstandin.js (sound.dll's SetWindowLongA, golf_clean.exe's FreeLibrary),
// recdisplay.js (the recording display swapped into 0x0083ad50 forwards other threads' calls to the real one).
// Copied helpers: c3eu's recording stubs and calleeStand (c3ea's), c3fe's quietTimer (sound.dll's multimedia timer held at
// SoundTimer::onTick's first test for the key: this PC has an audio device, so the sound engine runs).
// Wrapped fixtures: c3ct_edit / c3ct_edit_b (c3ct's EditBoxes) and c3db_a (c3db's synthetic widgets): c3ct and c3db must be
// listed in the batch list (log/c3/c3fu_batches.txt lists them; the main build links every batch).
(function () {
  const F = globalThis.DIFF_FIXTURES;
  const keep = function (x) { globalThis.__diffKeepAlive.push(x); return x; };
  const snd = function (rva) { return Process.getModuleByName('sound.dll').base.add(rva); };
  function u32b(v) { v >>>= 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; }
  function zero(p, n) { for (let k = 0; k < n; k += 4) p.add(k).writeU32(0); }
  function junk(p, n, tag) { for (let k = 0; k + 4 <= n; k += 4) p.add(k).writeU32((tag + k) >>> 0); }
  function blk(n, tag) { const p = keep(Memory.alloc(n)); if (tag === undefined) zero(p, n); else junk(p, n, tag); return p; }
  const k32 = Process.getModuleByName('kernel32.dll');
  const sleep = keep(new NativeFunction(k32.getExportByName('Sleep'), 'void', ['uint32'], 'stdcall', { scheduling: 'exclusive' }));

  // ---- the first thread is frozen once per key; the negative-control switch is armed after the freeze
  let frozenNow = false;
  function freezeNow() {
    if (frozenNow) return;
    __freeze.thread();
    frozenNow = true;
    __freeze.atFlush(function () { frozenNow = false; });
    __freeze.negOn();
  }

  // ---- per-vector selectors: fx.<prefix><k> runs apply(k) and returns fx.sel
  function selectors(fx, prefix, n, apply) {
    fx.sel = blk(0x10, 0x5e1e0000);
    for (let k = 0; k < n; k++) {
      (function (kk) {
        Object.defineProperty(fx, prefix + kk, { enumerable: false, configurable: true,
          get: function () { apply(kk); return fx.sel; } });
      })(k);
    }
  }

  // ---- c3fe's quietTimer (copied): wait for an idle moment (re-entrancy word 0x100b49a8 = 0 and the timer's critical
  // section 0x100b4990 unowned), write 5 (SoundTimer::onTick then leaves at 0x10008edf before touching anything), wait until
  // the word has stayed 5 with the section unowned for 100 ms; the idle value goes back at the flush. Throws rather than run
  // with a tick in flight.
  function quietTimer() {
    freezeNow();
    const g = snd(0xb49a8), owner = snd(0xb4990 + 0xc);   // CRITICAL_SECTION.OwningThread
    let idle = null;
    for (let i = 0; i < 400; i++) {
      if (g.readU32() === 0 && owner.readU32() === 0) { idle = 0; break; }
      sleep(1);
    }
    if (idle === null) {
      if (owner.readU32() !== 0) throw new Error('c3fu: timer section 0x100b4990 stays owned');
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
    if (stable < 50) throw new Error('c3fu: timer did not go quiet');
  }

  // ---- recording stubs (c3eu's design, machine code): each call appends {tag, ecx, stack args...} at the cursor
  // (rec+0, entries from rec+0x10) and returns cursor ^ tag; thiscall stubs pop their n arguments.
  function stubBytes(rec, tag, n) {
    const r = u32b(rec.toUInt32());
    const b = [];
    b.push(0xA1, ...r);                                              // mov eax, [REC]
    b.push(0x05, ...r);                                              // add eax, REC
    b.push(0xC7, 0x00, ...u32b(tag));                                // mov dword [eax], tag
    b.push(0x89, 0x48, 0x04);                                        // mov [eax+4], ecx
    for (let i = 1; i <= n; i++) b.push(0x8B, 0x54, 0x24, 4 * i, 0x89, 0x50, 4 + 4 * i);
    b.push(0x81, 0x05, ...r, ...u32b(4 * (n + 2)));                  // add dword [REC], 4(n+2)
    b.push(0xA1, ...r);                                              // mov eax, [REC]
    b.push(0x35, ...u32b(tag));                                      // xor eax, tag
    if (n > 0) b.push(0xC2, 4 * n, 0x00); else b.push(0xC3);
    return b;
  }
  function recEnv(recSize) {
    const size = Process.pageSize * 2;
    const base = keep(Memory.alloc(size));
    Memory.protect(base, size, 'rwx');
    let off = 0;
    function emit(bytes) {
      const p = base.add(off);
      p.writeByteArray(bytes);
      off += (bytes.length + 15) & ~15;
      if (off > size) throw new Error('c3fu: code arena full');
      return p;
    }
    const rec = blk(recSize);
    rec.writeU32(0x10);
    return { rec: rec, stub: function (tag, n) { return emit(stubBytes(rec, tag, n || 0)); } };
  }
  // a vtable of 0x100 slots, each `xor eax, eax; ret` unless given: slots = {offset: tag} -> recording stub (no argument)
  function recVtable(env, slots) {
    const vt = blk(0x400);
    const nul = env.stub(0x0BAD0000, 0);
    for (let i = 0; i < 0x100; i++) vt.add(i * 4).writePointer(nul);
    Object.keys(slots).forEach(function (k) { vt.add(parseInt(k, 10)).writePointer(env.stub(slots[k], 0)); });
    return vt;
  }

  // ---- calleeStand (c3ea's, copied from c3eu.js): a callee replaced on the A/B thread by a recorder whose log is state.
  // log word 0 = cursor, words 1..16 = call count of stub k, word 17 = FNV-1a hash over every call, entries
  // {va, count, values...} from +0x50 while they fit. Other threads reach the real callee.
  const standLive = {};
  let standTok = 1;
  const CLOG = 0x400;
  function newClog() { const l = blk(CLOG); l.writeU32(0x50); l.add(0x44).writeU32(0x811c9dc5); return l; }
  function fnvWord(h, w) {
    for (let k = 0; k < 4; k++) { h ^= (w >>> (8 * k)) & 0xff; h = Math.imul(h, 0x01000193) >>> 0; }
    return h >>> 0;
  }
  function calleeStand(fx, k, va, argc, abi, ret) {
    freezeNow();
    const tid = Process.getCurrentThreadId();
    const at = ptr(va), key = at.toString();
    if (standLive[key]) {
      try { Interceptor.revert(at); } catch (e) { /* already reverted */ }
      delete standLive[key];
      Interceptor.flush();
    }
    const argt = Array(argc).fill('uint32');
    const orig = new NativeFunction(at, 'uint32', argt, abi);
    const log = fx.clog;
    const fn = function () {
      if (Process.getCurrentThreadId() !== tid) return orig.apply(null, arguments);
      const a = Array.prototype.slice.call(arguments).map(function (v) { return v >>> 0; });
      const cell = log.add(4 + 4 * k);
      const n = cell.readU32();
      cell.writeU32(n + 1);
      let h = log.add(0x44).readU32();
      h = fnvWord(h, va);
      a.forEach(function (v) { h = fnvWord(h, v); });
      log.add(0x44).writeU32(h);
      const curs = log.readU32();
      const need = 8 + 4 * a.length;
      if (curs + need <= CLOG) {
        log.add(curs).writeU32(va >>> 0);
        log.add(curs + 4).writeU32(a.length);
        a.forEach(function (v, i) { log.add(curs + 8 + 4 * i).writeU32(v); });
        log.writeU32(curs + need);
      }
      return (ret === undefined ? 0 : ret) >>> 0;
    };
    const cb = new NativeCallback(fn, 'uint32', argt, abi);
    keep(cb); keep(orig);
    Interceptor.replace(at, cb);
    const tok = standTok++;
    standLive[key] = tok;
    Interceptor.flush();
    setTimeout(function () {
      if (standLive[key] !== tok) return;
      Interceptor.revert(at);
      delete standLive[key];
      Interceptor.flush();
    }, 0);
  }

  // ====================================================================================================================
  // resetSpriteQueue 0x004627d0: the five globals and their neighbours, prefilled per vector ($p0..$p11).
  //   regions (saved once): 0x00838200 (4), 0x0083416c..0x0083417f (g_date 0x00834170 and 0x00834174 inside),
  //   0x008371ec..0x00837203 (0x008371f4 and 0x008371fc inside), 0x0082c15c..0x0082c167 (0x0082c160 inside)
  // ====================================================================================================================
  const RSQ = [[0x00838200, 4], [0x0083416c, 0x14], [0x008371ec, 0x18], [0x0082c15c, 0xc]];
  const RSQ_FILL = [0, 0xffffffff, 0x11110000, 0x7fffffff, 0x80000000, 0x5a5a5a5a, 1, 0xfffffffe, 0x00400000, 0x12345678,
                    0x400, 0xcccccccc];
  F.c3fu_rsq = function () {
    const fx = {};
    freezeNow();
    RSQ.forEach(function (r) { __freeze.save(ptr(r[0]), r[1]); });
    selectors(fx, 'p', RSQ_FILL.length, function (k) {
      const v = RSQ_FILL[k];
      RSQ.forEach(function (r, j) {
        for (let o = 0; o < r[1]; o += 4) ptr(r[0] + o).writeU32((k < 2 ? v : (v + j * 0x1000 + o)) >>> 0);
      });
    });
    return fx;
  };

  // ====================================================================================================================
  // NodeListB::dtor 0x00487a60 / NodeListC::dtor 0x00487b80 over StrList::dtor 0x00487260: twelve objects (0x40 bytes each,
  // junk), +0x14 NULL or one of four held objects whose vtable slot +0x14 records (tag 0x51400000 + object, ecx).
  // ====================================================================================================================
  F.c3fu_nl = function () {
    const fx = {};
    const env = recEnv(0x400);
    fx.rec = env.rec;
    const held = [];
    for (let h = 0; h < 4; h++) {
      const o = blk(0x20, 0x48000000 + h * 0x100);
      o.writePointer(recVtable(env, { 0x14: 0x51400000 + h, 0x10: 0x51000000 + h, 0x18: 0x51800000 + h }));
      held.push(o);
    }
    fx.held = blk(0x10);           // (unused name kept free of getters)
    const b = blk(0x40 * 12);
    for (let i = 0; i < 12; i++) {
      const o = b.add(0x40 * i);
      junk(o, 0x40, (0x4e000000 + i * 0x01010101) >>> 0);
      // objects 0, 3, 6, 9 hold nothing; the others hold object (i % 4)
      o.add(0x14).writePointer(i % 3 === 0 ? ptr(0) : held[i % 4]);
      fx['o' + i] = o;
    }
    fx.oblk = b;
    fx.hblk = held[0];
    freezeNow();                   // no global is touched; it arms the negative-control switch
    return fx;
  };

  // ====================================================================================================================
  // shutdownSoundSystem 0x004490b0: the Snd array 0x0080d840..0x008156cf (300 records of 0x6c) saved once and refilled per
  // vector ($p0..$p5) with our records; Snd::stop484f00 0x00484f00 runs on them (the record's vtable slot +0x80 and the
  // device's slot +0x14 record); shutdownSound 0x00483f10 is stood in (calleeStand, returns 0x5d5d).
  //   record r: +0 our vtable (slot +0x80 tag 0x58800000), +0x40 NULL or device d (slot +0x14 tag 0x51400000 + d),
  //   +0x44 junk with bit 0 set or clear, +0x58 a byte with bit 1 set or clear, the rest junk.
  // ====================================================================================================================
  const SND = 0x0080d840, SND_N = 300, SND_SZ = 0x6c;
  F.c3fu_sss = function () {
    const fx = {};
    const env = recEnv(0x2400);
    fx.rec = env.rec;
    fx.clog = newClog();
    const vtR = recVtable(env, { 0x80: 0x58800000 });
    const devs = [];
    for (let d = 0; d < 3; d++) {
      const o = blk(0x20, 0x44000000 + d * 0x100);
      o.writePointer(recVtable(env, { 0x14: 0x51400000 + d }));
      devs.push(o);
    }
    freezeNow();
    __freeze.save(ptr(SND), SND_N * SND_SZ);
    calleeStand(fx, 0, 0x00483f10, 0, 'default', 0x5d5d);
    // pattern k: device of record i = (i * 7 + k) % 5 (0, 1 -> none; 2..4 -> device 0..2), bit 0 of +0x44 = (i + k) & 1,
    // bit 1 of +0x58 = ((i >> 1) + k) & 1; pattern 0 has no device and both bits clear, pattern 5 every bit set.
    selectors(fx, 'p', 6, function (k) {
      for (let i = 0; i < SND_N; i++) {
        const r = ptr(SND + i * SND_SZ);
        junk(r, SND_SZ, (0x53000000 + (k << 16) + (i << 4)) >>> 0);
        r.writePointer(vtR);
        let dev = -1, b0 = 0, b1 = 0;
        if (k === 0) { dev = -1; b0 = 0; b1 = 0; }
        else if (k === 5) { dev = i % 3; b0 = 1; b1 = 1; }
        else { const s = (i * 7 + k) % 5; dev = s < 2 ? -1 : s - 2; b0 = (i + k) & 1; b1 = ((i >> 1) + k) & 1; }
        r.add(0x40).writePointer(dev < 0 ? ptr(0) : devs[dev]);
        const w44 = r.add(0x44).readU32();
        r.add(0x44).writeU32(((w44 & ~1) | b0) >>> 0);
        const b58 = r.add(0x58).readU8();
        r.add(0x58).writeU8((b58 & ~2) | (b1 << 1));
      }
    });
    return fx;
  };

  // ====================================================================================================================
  // engineShutdown 0x00485740: twelve callees stood in (calleeStand, call order and arguments logged); the empty 0x004a0890
  // (one `ret`, too short to intercept) runs for real; freeGraphsyObject 0x004a0060 runs for real on our globals, set per
  // vector ($p0..$p7): 0x0083ad50 = a guarded recording display (slot +0xc records, tag 0xD000000C) or 0, 0x0084092c = one
  // of our handle values or 0; KERNEL32!FreeLibrary [0x4ba0ac] stood in (returns handle ^ 0x00f0f0f0). 0x0083aff0 is set to
  // our dummy object for the key, so the multimedia-timer callback 0x00486f90 skips the display (0x00486fa6..0x00486fb1:
  // a non-NULL 0x0083aff0 that is not the firing timer jumps to 0x00486fdb) while 0x0083ad50 is 0 between an arm and the
  // restore.
  // ====================================================================================================================
  const ES_STANDS = [[0x00492470, 0, 'default'], [0x00486ff0, 0, 'default'], [0x0049ff30, 0, 'default'],
                     [0x0049ead0, 0, 'default'], [0x00483b10, 0, 'default'], [0x0049d280, 0, 'default'],
                     [0x0049d460, 0, 'default'], [0x00490c30, 0, 'default'], [0x0047d010, 0, 'default'],
                     [0x00483340, 0, 'default'], [0x0049d0e0, 1, 'thiscall']];
  //                display  handle
  const ES_ROWS = [[1, 0x00a10000], [0, 0x00a20000], [1, 0], [0, 0], [1, 0x7ffe0000], [0, 1], [1, 0xfffffffe], [1, 0x00a10000]];
  F.c3fu_es = function () {
    const fx = {};
    const env = recEnv(0x400);
    fx.rec = env.rec;
    fx.clog = newClog();
    const dvt = recVtable(env, { 0x0c: 0xD000000C });
    const disp = blk(0x40, 0x44490000);
    disp.writePointer(dvt);
    fx.disp = disp;
    const gate = blk(0x40, 0x47410000);
    freezeNow();
    __freeze.setU32(ptr(0x0083aff0), gate.toUInt32());
    __freeze.save(ptr(0x0083ad50), 4);
    __freeze.save(ptr(0x0084092c), 4);
    const guarded = __recDisplay.guard(disp);
    const s = __importStandIn({ module: 'golf_clean.exe', imports: {
      'KERNEL32.dll!FreeLibrary': { args: 1, impl: function (a) { return (a[0].toUInt32() ^ 0x00f0f0f0) >>> 0; } } } });
    fx.implog = s.log;
    ES_STANDS.forEach(function (st, k) { calleeStand(fx, k, st[0], st[1], st[2], 0x3c000000 + k); });
    selectors(fx, 'p', ES_ROWS.length, function (k) {
      ptr(0x0083ad50).writeU32(ES_ROWS[k][0] ? guarded.toUInt32() : 0);
      ptr(0x0084092c).writeU32(ES_ROWS[k][1] >>> 0);
      fx.disp.add(4).writeU32((0x44490000 + k) >>> 0);       // marks the vector in the display block
    });
    return fx;
  };

  // ====================================================================================================================
  // EditBox::charPosToPixel 0x00486360 on c3ct's EditBoxes (c3ct_edit / c3ct_edit_b) plus two output blocks ox / oy
  // (0x10 bytes each, junk) and their alias (ox passed twice).
  // ====================================================================================================================
  // The wrapped fixtures freeze the first thread themselves (c3ct.js freezeAndSave, c3db.js build); negOn() arms the
  // negative-control switch after that freeze (a normal build has no switch, so it does nothing there).
  function cpp(base) {
    const fx = F[base]();
    fx.ox = blk(0x10, 0x0f0f0000);
    fx.oy = blk(0x10, 0x0e0e0000);
    __freeze.negOn();
    return fx;
  }
  F.c3fu_cpp = function () { return cpp('c3ct_edit'); };
  // c3ct_edit_b gives EditBoxes 0, 2 and 5 equal margins (+0x598 = +0x59c); 2 and 5 get unequal ones here (3 / 1, 6 / 2), so
  // a body that adds one margin twice (NEG9) differs on this key too.
  F.c3fu_cpp_b = function () {
    const fx = cpp('c3ct_edit_b');
    fx.eb2.add(0x598).writeS32(3);
    fx.eb5.add(0x598).writeS32(6);
    return fx;
  };

  // ====================================================================================================================
  // Widget_writePixel 0x00478ea0 on c3db's widgets (c3db_a: synthetic Surfaces on jgld's own Surface vtable, all in $arena)
  // plus three widgets whose Surface is ours with a RECORDING vtable (wr8 / wr16 / wr24: depth 8, 16, 24; bounds 32 x 16):
  // every slot the two bodies call logs (tag, ecx) at $wrrec and returns a constant (+0xd8 width 32, +0xdc height 16, +0x10
  // the pixel base $wrpix + 0x40 * k, +0xe4 a pointer to the depth dword, +0xe0 pitch 32, +0x24 0 popping its argument).
  // jgld's own release slot leaves no trace when nothing was acquired; these show each slot call, including the +0x24 call
  // a wrong height test (NEG10) adds at y = 16, where Surface_pixelPtr returns NULL before its slot +0x10 (0x004796de).
  // ====================================================================================================================
  function constStub(emit, rec, tag, value, nargs) {
    const r = u32b(rec.toUInt32());
    const b = [0xA1, ...r, 0x05, ...r, 0xC7, 0x00, ...u32b(tag), 0x89, 0x48, 0x04,
               0x81, 0x05, ...r, ...u32b(8), 0xB8, ...u32b(value)];
    if (nargs) b.push(0xC2, 4 * nargs, 0x00); else b.push(0xC3);
    return emit(b);
  }
  F.c3fu_wp = function () {
    const fx = F.c3db_a();
    const size = Process.pageSize;
    const code = keep(Memory.alloc(size));
    Memory.protect(code, size, 'rwx');
    let off = 0;
    const emit = function (bytes) { const p = code.add(off); p.writeByteArray(bytes); off += (bytes.length + 15) & ~15; return p; };
    const rec = blk(0x400);
    rec.writeU32(0x10);
    fx.wrrec = rec;
    const pix = blk(0x400 * 3, 0x70000000);
    fx.wrpix = pix;
    const nul = emit([0x33, 0xC0, 0xC3]);
    [8, 16, 24].forEach(function (depth, k) {
      const dw = blk(0x10);
      dw.writeU32(depth);
      const base = pix.add(0x400 * k);
      const t = 0x5f000000 + (depth << 16);
      const vt = blk(0x400);
      for (let i = 0; i < 0x100; i++) vt.add(i * 4).writePointer(nul);
      vt.add(0xd8).writePointer(constStub(emit, rec, t + 0xd8, 32, 0));
      vt.add(0xdc).writePointer(constStub(emit, rec, t + 0xdc, 16, 0));
      vt.add(0x10).writePointer(constStub(emit, rec, t + 0x10, base.toUInt32(), 0));
      vt.add(0xe4).writePointer(constStub(emit, rec, t + 0xe4, dw.toUInt32(), 0));
      vt.add(0xe0).writePointer(constStub(emit, rec, t + 0xe0, 32, 0));
      vt.add(0x24).writePointer(constStub(emit, rec, t + 0x24, 0, 1));
      const s = blk(0x40, 0x53550000 + k * 0x100);
      s.writePointer(vt);
      const w = blk(0x10);
      w.writeU32(0x57520000 + depth);
      w.add(4).writePointer(s);
      fx['wr' + depth] = w;
    });
    __freeze.negOn();
    return fx;
  };

  // ====================================================================================================================
  // sound.dll
  // ====================================================================================================================
  // getDsCoopLevel 0x3a770: the global 0x100b4a3c set per vector ($c0..$c11).
  const COOP = [0, 1, 2, 3, 4, 0xffffffff, 0x7fffffff, 0x80000000, 0x12345678, 6, 0x100, 5];
  F.c3fu_coop = function () {
    const fx = {};
    freezeNow();
    const g = snd(0xb4a3c);
    __freeze.save(g, 4);
    fx.g = g;
    selectors(fx, 'c', COOP.length, function (k) { g.writeU32(COOP[k] >>> 0); });
    return fx;
  };

  // getWaveDeviceInterface 0xe2d0: g_waveDevice 0x100b49f0 set per vector ($d0..$d11) to 0 or to one of our blocks whose
  // first dword differs; the sound timer is held off for the key.
  F.c3fu_wdi = function () {
    const fx = {};
    const devs = [];
    const FIRST = [0x00000000, 0x00000001, 0xffffffff, 0x1005b304, 0x80000000, 0x7fffffff, 0x12345678, 0xdeadbee0, 0x00010000,
                   0x0000ff00];
    FIRST.forEach(function (v, i) { const d = blk(0x40, 0x57440000 + i * 0x100); d.writeU32(v >>> 0); devs.push(d); });
    quietTimer();
    const g = snd(0xb49f0);
    __freeze.save(g, 4);
    fx.g = g;
    const ROWS = [-1, 0, 1, 2, 3, -1, 4, 5, 6, 7, 8, 9];
    selectors(fx, 'd', ROWS.length, function (k) { g.writePointer(ROWS[k] < 0 ? ptr(0) : devs[ROWS[k]]); });
    return fx;
  };

  // cfltcvtInit 0x42506: the six pointers 0x100651b8..0x100651cf and one dword on each side (0x100651b4, 0x100651d0)
  // prefilled per vector ($p0..$p11): zeros, the values the body stores (vector 1), the same values rotated, our tags.
  F.c3fu_cflt = function () {
    const fx = {};
    quietTimer();
    const lo = snd(0x651b4);
    __freeze.save(lo, 0x20);
    fx.cvt = lo;
    const right = [0x4477c, 0x44426, 0x4448c, 0x443cc, 0x44474, 0x4477c].map(function (r) { return snd(r).toUInt32(); });
    selectors(fx, 'p', 12, function (k) {
      for (let i = 0; i < 8; i++) {
        let v;
        if (k === 0) v = 0;
        else if (k === 1) v = (i >= 1 && i <= 6) ? right[i - 1] : 0x0c0c0c0c;
        else if (k === 2) v = (i >= 1 && i <= 6) ? right[i % 6] : 0x0d0d0d0d;
        else v = (0x43000000 + (k << 16) + i * 0x11) >>> 0;
        lo.add(4 * i).writeU32(v >>> 0);
      }
    });
    return fx;
  };

  // atexitDtor_100b49b8 0x91f0 over SoundWindowObj_dtor 0x286a0: the module's window object 0x100b49b8 (0x10 bytes:
  // +0 vtable, +4 flag byte, +8 window handle, +0xc saved window procedure) prefilled per vector ($s0..$s11); USER32
  // SetWindowLongA [0x100b85a4] stood in (returns hwnd ^ proc ^ 0x1234), so no real window is touched. The live vtable
  // value is kept at +0 except in vectors 0 and 1.
  //            +4 byte  +8 hwnd       +0xc proc
  const SWO = [[0x04, 0x00010010, 0x00401000], [0x00, 0x00010010, 0x00401000], [0x04, 0, 0x00402000],
               [0x05, 0x00020020, 0x00403000], [0xff, 0x7fff0000, 0xffffffff], [0xfb, 0x00030030, 0x00404000],
               [0x04, 0xffffffff, 0], [0x0c, 0x00040040, 0x12345678], [0x00, 0, 0], [0x06, 0x00050050, 0x80000000],
               [0x84, 0x00060060, 0x00405000], [0x03, 0x00070070, 0x00406000]];
  F.c3fu_swo = function () {
    const fx = {};
    quietTimer();
    const o = snd(0xb49b8);
    __freeze.save(o, 0x10);
    fx.swo = o;
    const vt = o.readU32();
    const s = __importStandIn({ module: 'sound.dll', imports: {
      'USER32.dll!SetWindowLongA': { args: 3, impl: function (a) {
        return (a[0].toUInt32() ^ a[2].toUInt32() ^ 0x1234) >>> 0; } } } });
    fx.implog = s.log;
    selectors(fx, 's', SWO.length, function (k) {
      const r = SWO[k];
      o.writeU32(k < 2 ? (0x56540000 + k) >>> 0 : vt);
      o.add(4).writeU32((0x5a5a5a00 | r[0]) >>> 0);
      o.add(8).writeU32(r[1] >>> 0);
      o.add(0xc).writeU32(r[2] >>> 0);
    });
    return fx;
  };
})();
