// S4.11 fixtures (log/roadmap/S4/11_keys.py): constructed states for the functions of port/src/sim/audio_crt.cpp that no other
// step's key reaches with every branch. Evaluated into each key's fixture-VM context after the shared scripts (11_replay.py appends
// this file to sound_replay's fixture sources). All values are synthetic; every block, string, code stub and log is the fixture's
// own; module globals the keys change are saved and restored with __freeze. Code stubs are machine code the emulator runs in both
// arms (no NativeCallback): rec(L) is a thiscall recorder that appends ecx to the log L {+0 count, +4 entries} and returns ecx.
(function () {
  const F = globalThis.DIFF_FIXTURES;
  const keep = function (x) { globalThis.__diffKeepAlive.push(x); return x; };
  const snd = function (rva) { return Process.getModuleByName('sound.dll').base.add(rva); };
  function fill(p, n, b) { p.writeByteArray(new Array(n).fill(b)); return p; }
  function block(n, tag) {
    const b = fill(keep(Memory.alloc(n)), n, 0);
    for (let k = 0; k + 4 <= n; k += 4) b.add(k).writeU32((((tag & 0xffff) << 16) | k) >>> 0);
    return b;
  }
  function zero(n) { return fill(keep(Memory.alloc(n)), n, 0); }
  function u32le(v) { v = v >>> 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, v >>> 24]; }
  let page = null, pageOff = 0;
  function code(bytes) {
    if (!page) {
      page = keep(Memory.alloc(Process.pageSize));
      fill(page, Process.pageSize, 0xcc);
      Memory.protect(page, Process.pageSize, 'rwx');
      pageOff = 0;
    }
    const p = page.add(pageOff);
    p.writeByteArray(bytes);
    pageOff += (bytes.length + 0xf) & ~0xf;
    return p;
  }
  function rec(L) {
    const l = L.toUInt32(), l4 = (l + 4) >>> 0;
    return code([0xa1].concat(u32le(l), [0x89, 0x0c, 0x85], u32le(l4), [0xff, 0x05], u32le(l), [0x8b, 0xc1, 0xc3]));
  }
  function str(s) {
    const p = keep(Memory.alloc(s.length + 0x20));
    fill(p, s.length + 0x20, 0);
    p.writeAnsiString(s);
    return p;
  }
  function strings(out, prefix, list) { list.forEach(function (s, i) { out[prefix + i] = str(s); }); }

  // ---- the system-heap imports of sound.dll stood in (HeapAlloc / HeapFree, optionally HeapSize / HeapReAlloc), so the original
  // arm and the port get the same block addresses (the emulator's own heap is not restored between the arms: the second arm saw
  // addresses 0x10 higher). HeapAlloc returns arena block (call number & 15) * 0x40; with `after` it fails until the new-handler
  // stub (hret != 0) has set the ok cell, then succeeds once and clears it (so every vector is self-contained). HeapReAlloc:
  // blocks of 0x100 at arena + 0x400 + 0x100 * (call & 7); `reallocFail(n)` decides which sizes fail; HeapSize: o.size(block).
  // State: $implog (the stand-in's log), $arena (0xc00), $cell (0x10: +0 ok flag, +4 handler result).
  function heapSI(o) {
    o = o || {};
    const arena = zero(0xc00), cell = zero(0x10);
    const imps = {
      'KERNEL32.dll!HeapAlloc': { args: 3, impl: function (a, ctx) {
        if (o.after) { if (cell.readU32() === 0) return 0; cell.writeU32(0); }
        return arena.add(0x40 * (ctx.call & 15));
      } },
      'KERNEL32.dll!HeapFree': { args: 3, ret: 1 },
    };
    if (o.size) imps['KERNEL32.dll!HeapSize'] = { args: 3, impl: function (a) { return o.size(a[2].toUInt32()); } };
    if (o.realloc) imps['KERNEL32.dll!HeapReAlloc'] = { args: 4, impl: function (a, ctx) {
      if (o.after) { if (cell.readU32() === 0) return 0; cell.writeU32(0); }
      if (o.reallocFail && o.reallocFail(a[3].toUInt32())) return 0;
      return arena.add(0x400 + 0x100 * (ctx.call & 7));
    } };
    if (o.extra) Object.assign(imps, o.extra(arena, cell));
    const s = __importStandIn({ module: 'sound.dll', imports: imps });
    return { arena: arena, cell: cell, implog: s.log };
  }
  // the new handler: returns cell+4; when that is non-zero it also sets the ok flag (cell+0)
  function handlerStub(cell, ret) {
    cell.add(4).writeU32(ret >>> 0);
    const ok = cell.toUInt32(), hr = (ok + 4) >>> 0;
    return code([0xa1].concat(u32le(hr), [0x85, 0xc0, 0x74, 0x0a, 0xc7, 0x05], u32le(ok), [1, 0, 0, 0, 0xc3]));
  }

  // ---- GlblHeader::init 0x2e3c0: ten 12-byte headers with marker words and chosen +8 mode words
  F.s411_glbl = function () {
    const out = {};
    [0, 1, 2, 0xfffffffe, 0x80000000, 0x7fffffff, 0x10, 3, 0xffffffff, 0x1234].forEach(function (m, i) {
      const b = block(0x10, 0x4c00 + i);
      b.add(8).writeU32(m >>> 0);
      out['b' + i] = b;
    });
    return out;
  };
  // ---- Sound::getField1b8 0x5f40: objects with marker words around +0x1b8
  F.s411_f1b8 = function () {
    const out = {};
    for (let i = 0; i < 10; i++) {
      const b = block(0x200, 0x4d00 + i);
      if (i === 3) b.add(0x1b8).writeU32(0);
      if (i === 4) b.add(0x1b8).writeU32(0xffffffff);
      out['b' + i] = b;
    }
    return out;
  };
  // ---- Sound::queueMidiCommand3 0x20480: g_midiDevice (rva 0xb4a1c) set to a device whose command ring sits at +0x50 (read index
  // +0x4050, write index +0x4054), or NULL; the ring's node comes from the real operator new chain
  function midi(mode) {
    __freeze.thread();
    const out = { gmidi: snd(0xb4a1c) };
    __freeze.save(snd(0xb4a1c), 4);
    const dev = zero(0x4060);
    out.dev = dev;
    if (mode === 'full') {
      dev.add(0x50 + 0x4000).writeU32(5);
      dev.add(0x50 + 0x4004).writeU32(4);
    } else {
      dev.add(0x50 + 0x4000).writeU32(7);
      dev.add(0x50 + 0x4004).writeU32(3);
    }
    __freeze.setU32(snd(0xb4a1c), mode === 'nodev' ? 0 : dev.toUInt32());
    for (let i = 0; i < 6; i++) out['s' + i] = block(0x100, 0x4e00 + i);
    Object.assign(out, heapSI());
    return out;
  }
  F.s411_midi_dev = function () { return midi('dev'); };
  F.s411_midi_full = function () { return midi('full'); };
  F.s411_midi_nodev = function () { return midi('nodev'); };
  // ---- MixBuffer::deletingDtor 0x2cf50: 0x208-byte objects; the 0x80 slots at +4 hold runs of one record each, a record being
  // {last slot, first slot}; +0x204 an extra buffer or NULL. o0 no runs, o1 one run of one slot, o2 three runs, o3 a run at the end
  // (slots 0x7d..0x7f), o4 a record whose first slot lies after its last (no zeroing), o5 every slot one run, o6 runs + extra
  F.s411_mixdtor = function () {
    const out = {};
    const specs = [
      { runs: [], extra: false },
      { runs: [[5, 5]], extra: false },
      { runs: [[0, 2], [4, 4], [10, 20]], extra: true },
      { runs: [[0x7d, 0x7f]], extra: false },
      { runs: [[6, 6]], skew: true, extra: true },
      { runs: [[0, 0x7f]], extra: false },
      { runs: [[1, 1], [2, 3], [0x40, 0x41]], extra: true },
      { runs: [[0x7f, 0x7f]], extra: true },
    ];
    specs.forEach(function (sp, i) {
      const o = zero(0x208);
      o.writeU32(0x11110000 + i);
      sp.runs.forEach(function (run, k) {
        const r = block(0x10, 0x4f00 + 0x10 * i + k);
        r.writeU32(run[1]);
        r.add(4).writeU32(sp.skew ? run[1] + 3 : run[0]);
        out['r' + i + '_' + k] = r;
        for (let s = run[0]; s <= run[1]; s++) o.add(4 + 4 * s).writePointer(r);
      });
      if (sp.extra) {
        const x = block(0x40, 0x4fe0 + i);
        out['x' + i] = x;
        o.add(0x204).writePointer(x);
      }
      out['o' + i] = o;
    });
    Object.assign(out, heapSI());
    return out;
  };
  // ---- _strrchr 0x424b0 / strstr 0x42690: strings
  F.s411_strings = function () {
    const out = {};
    strings(out, 's', ['', 'a', 'abc', 'a\\b\\c', '\\\\', 'Sounds\\Effects\\Sheep 2.wav', 'no slash here', 'xax', 'aaaa', 'mid',
                       'track.rmi', 'z']);
    // strstr: a haystack with a byte after its NUL that matches, so the unrolled second-character test reads past the end
    const h = zero(0x40);
    h.writeByteArray([0x61, 0x62, 0x00, 0x63, 0x64, 0x00]);
    out.h = h;
    strings(out, 'p', ['', 'a', 'ab', 'bc', 'abc', 'abcd', 'Sheep', '.wav', 'b\\c', 'aa', 'aaa', 'aaaaa', 'cd', 'x', 'ax']);
    return out;
  };
  // ---- _sprintf 0x42577: one 0x80-byte output buffer per vector (filled with 0x5a so trailing bytes are compared), the formats,
  // a %s argument string
  F.s411_sprintf = function () {
    const out = {};
    const fmts = ['%d', '%5d', '%-5d|', '%05d', '%+d', '% d', '%x', '%#x', '%X', '%#X', '%o', '%#o', '%u', '%.3d', '%.0d', '%s',
                  '%.2s', '%8s|', '%-8s|', '%c', '%%d', 'ab%dcd', '%ld', '%hd', '%hu', '%i', 'timer Late!!! by %d ms\n', '%08x',
                  '%#.0o', '%+5d', '%-+6d|', '% 05d', 'plain', '%3c|', '%#5o'];
    strings(out, 'f', fmts);
    const buf = fill(keep(Memory.alloc(0x80 * fmts.length)), 0x80 * fmts.length, 0x5a);
    out.buf = buf;
    for (let i = 0; i < fmts.length; i++) out['o' + i] = buf.add(0x80 * i);
    out.str = str('Sheep 2');
    return out;
  };
  // ---- the allocation chain: g_newMode (rva 0xb5588) and g_newHandler (rva 0xb558c) set per key; the handler stub returns its
  // argument & 1 (cdecl)
  function heapKey(newMode, handler, opts) {
    __freeze.thread();
    const out = { gmode: snd(0xb5588) };
    __freeze.save(snd(0xb5588), 8);
    __freeze.setU32(snd(0xb5588), newMode >>> 0);
    let h = 0;
    if (handler === 'stub') h = code([0x8b, 0x44, 0x24, 0x04, 0x83, 0xe0, 0x01, 0xc3]).toUInt32();
    const si = opts ? heapSI(opts.si) : null;
    if (si) {
      Object.assign(out, si);
      if (handler === 'yes' || handler === 'no') h = handlerStub(si.cell, handler === 'yes' ? 1 : 0).toUInt32();
    }
    __freeze.setU32(snd(0xb558c), h >>> 0);
    for (let i = 0; i < 4; i++) out['b' + i] = block(0x40, 0x5000 + i);
    return out;
  }
  F.s411_heap0 = function () { return heapKey(0, 'none', { si: {} }); };
  F.s411_heap1 = function () { return heapKey(1, 'none', { si: {} }); };
  F.s411_newh = function () { return heapKey(0, 'stub'); };
  // HeapAlloc fails until the new handler (returns 1 / 0) has run: g_newMode 1 with a handler that agrees to retry, with one
  // that gives up, and g_newMode 0 with the agreeing handler (a mode-0 _malloc never asks it)
  F.s411_heapf_yes = function () { return heapKey(1, 'yes', { si: { after: true } }); };
  F.s411_heapf_no = function () { return heapKey(1, 'no', { si: { after: true } }); };
  F.s411_heapf_mode0 = function () { return heapKey(0, 'yes', { si: { after: true } }); };
  // ---- the CRT locks: the lock table (rva 0x67400, 0x30 slots) saved; `create` empties ten slots the start-up never filled,
  // `enter` puts free fixture critical sections there (LockCount -1), `pair` puts sections this thread already holds once
  // (_unlock leaves them, __lock re-enters them)
  const LOCKS = [0x14, 0x15, 0x16, 0x18, 0x1a, 0x1c, 0x20, 0x22, 0x24, 0x26];
  function locks(mode, list) {
    list = list || LOCKS;
    __freeze.thread();
    const out = { gtab: snd(0x67400) };
    __freeze.save(snd(0x67400), 0xc0);
    if (snd(0x67400).add(4 * 0x11).readU32() === 0) throw new Error('s411_locks: lock 0x11 is not initialised');
    list.forEach(function (n, i) {
      let v = 0;
      if (mode !== 'create') {
        const cs = zero(0x18);
        cs.writeU32(0xffffffff);
        cs.add(4).writeU32(mode === 'pair' ? 0xfffffffe : 0xffffffff);
        if (mode === 'pair') {
          cs.add(8).writeU32(1);
          cs.add(0xc).writeU32(Process.getCurrentThreadId());
        }
        out['cs' + i] = cs;
        v = cs.toUInt32();
      }
      __freeze.setU32(snd(0x67400).add(4 * n), v);
    });
    if (mode === 'create') Object.assign(out, heapSI());
    return out;
  }
  F.s411_lock_create = function () { return locks('create'); };
  F.s411_lockexit_enter = function () { return locks('enter', [0xd, 0xe]); };
  F.s411_lockexit_pair = function () { return locks('pair', [0xd, 0xe]); };

  // ---- __lock 0x47710 racing another thread: the nested __lock(0x11) of the creation path (the first EnterCriticalSection of a
  // vector) fills the first still-empty slot of LOCKS with a section of the fixture's, as a second thread that created it first
  // would; the final Enter (cell+8 = 1) disarms it. The vectors go in LOCKS order, so the empty slot is the vector's own.
  F.s411_lock_race = function () {
    __freeze.thread();
    const out = { gtab: snd(0x67400) };
    __freeze.save(snd(0x67400), 0xc0);
    if (snd(0x67400).add(4 * 0x11).readU32() === 0) throw new Error('s411_lock_race: lock 0x11 is not initialised');
    LOCKS.forEach(function (n) { __freeze.setU32(snd(0x67400).add(4 * n), 0); });
    const racers = [];
    for (let i = 0; i < LOCKS.length; i++) {
      const cs = zero(0x18);
      cs.writeU32(0xffffffff);
      cs.add(4).writeU32(0xffffffff);
      out['cs' + i] = cs;
      racers.push(cs);
    }
    Object.assign(out, heapSI({ extra: function (arena, cell) {
      return {
        'KERNEL32.dll!EnterCriticalSection': { args: 1, impl: function () {
          if (cell.add(8).readU32() === 0) {
            for (let i = 0; i < LOCKS.length; i++) {
              const slot = snd(0x67400).add(4 * LOCKS[i]);
              if (slot.readU32() === 0) { slot.writeU32(racers[i].toUInt32()); break; }
            }
            cell.add(8).writeU32(1);
          } else {
            cell.add(8).writeU32(0);
          }
          return 0;
        } },
        'KERNEL32.dll!LeaveCriticalSection': { args: 1, ret: 0 },
        'KERNEL32.dll!InitializeCriticalSection': { args: 1, ret: 0 },
      };
    } }));
    return out;
  };
  // ---- the small-block heaps (heap types 3 and 2: g_heapType rva 0xb5d20, thresholds 0xb5d0c / 0x673fc). No code of this build
  // selects them (setSbhThreshold 0x45e34 has no caller), so the arms are judged with recording stand-ins for the helpers they
  // call: __lock / _unlock and the heap routines 0x46283 / 0x45f2f / 0x45f5a (type 3) and 0x47148 / 0x470ac / 0x47103 (type 2).
  const SLOGSZ = 0x800;
  function slog(log, tag, args) {
    const n = log.readU32();
    if (4 + 0x20 * (n + 1) <= SLOGSZ) {
      const q = log.add(4 + 0x20 * n);
      q.writeU32(tag >>> 0);
      for (let i = 0; i < args.length && i < 7; i++) q.add(4 + 4 * i).writeU32(ptr(args[i]).toUInt32());
    }
    log.writeU32(n + 1);
  }
  function cstub(log, rva, nargs, fn) {
    const base = snd(rva);
    const cb = new NativeCallback(function () {
      const a = Array.prototype.slice.call(arguments);
      slog(log, rva, a);
      return fn ? (fn(a) >>> 0) : 0;
    }, 'uint32', Array(nargs).fill('pointer'), 'default');
    Interceptor.replace(base, cb);
    Interceptor.flush();
    __freeze.atFlush(function () { Interceptor.revert(base); Interceptor.flush(); });
  }
  // a block's bit 16 of its first dword (block() writes (tag << 16) | 0 there) decides found / not found
  function owned(p) { return ((ptr(p).readU32() >>> 16) & 1) === 0; }
  function sbh(type) {
    __freeze.thread();
    const out = { gtype: snd(0xb5d20), glim3: snd(0xb5d0c), glim2: snd(0x673fc) };
    __freeze.save(snd(0xb5d20), 4);
    __freeze.save(snd(0xb5d0c), 4);
    __freeze.save(snd(0x673fc), 4);
    __freeze.setU32(snd(0xb5d20), type);
    __freeze.setU32(snd(0xb5d0c), 0x3f8);
    __freeze.setU32(snd(0x673fc), 0x780);
    out.slog = zero(SLOGSZ);
    cstub(out.slog, 0x47710, 1, null);                                    // __lock
    cstub(out.slog, 0x47771, 1, null);                                    // _unlock
    cstub(out.slog, 0x46283, 1, function (a) { return (ptr(a[0]).toUInt32() & 0x10) ? 0 : 0x60000000 + ptr(a[0]).toUInt32(); });
    cstub(out.slog, 0x45f2f, 1, function (a) { return owned(a[0]) ? 0x6000 : 0; });
    cstub(out.slog, 0x45f5a, 2, null);
    cstub(out.slog, 0x47148, 1, function (a) { return (ptr(a[0]).toUInt32() & 1) ? 0 : 0x70000000 + ptr(a[0]).toUInt32(); });
    cstub(out.slog, 0x470ac, 3, function (a) {
      ptr(a[1]).writeU32(0x1100 + (ptr(a[0]).readU32() >>> 16 & 0xff));
      ptr(a[2]).writeU32(0x2200);
      return owned(a[0]) ? 0x7000 : 0;
    });
    cstub(out.slog, 0x47103, 3, null);
    for (let i = 0; i < 4; i++) out['b' + i] = block(0x40, 0x5000 + i);
    Object.assign(out, heapSI());
    return out;
  }
  F.s411_sbh3 = function () { return sbh(3); };
  F.s411_sbh2 = function () { return sbh(2); };
  F.s411_lock_enter = function () { return locks('enter'); };
  F.s411_lock_pair = function () { return locks('pair'); };
  // ---- the exit table (begin rva 0xb5d3c, end 0xb5d38; the block the start-up's 0x42659 allocated): `room` leaves the end where
  // the start-up left it, `full` moves it to the block's size so __onexit grows the table through msize / crtRealloc
  function onexit(mode) {
    __freeze.thread();
    const out = { gex: snd(0xb5d38) };
    __freeze.save(snd(0xb5d38), 8);
    const begin = snd(0xb5d3c).readU32();
    if (begin === 0) throw new Error('s411_onexit: no exit table');
    out.tbl = ptr(begin);
    if (mode === 'full' || mode === 'fail') __freeze.setU32(snd(0xb5d38), (begin + 0x80) >>> 0);
    if (mode === 'full' || mode === 'fail') {
      // the table's block reports 0x80 bytes (so it is full); the grown block is the stand-in's, or the growth fails
      Object.assign(out, heapSI({ size: function () { return 0x80; }, realloc: true,
                                  reallocFail: mode === 'fail' ? function () { return true; } : null }));
      // the old table was saved by __freeze.save: onexit rewrites g_onexitBegin, so save that too
      __freeze.save(snd(0xb5d3c), 4);
    }
    return out;
  }
  F.s411_onexit_room = function () { return onexit('room'); };
  F.s411_onexit_full = function () { return onexit('full'); };
  F.s411_onexit_fail = function () { return onexit('fail'); };
  // ---- crtRealloc / msize on the exit table's own heap block (the only block of the emulator's heap the fixture knows):
  // HeapSize answers (block & 0xf0) + 0x10, HeapReAlloc fails for a rounded size of 0x400 and more
  F.s411_realloc = function () {
    const out = onexit('room');
    out.b0 = block(0x40, 0x5100);
    Object.assign(out, heapSI({ size: function (p) { return (p & 0xf0) + 0x10; }, realloc: true,
                                reallocFail: function (n) { return n >= 0x400; } }));
    return out;
  };
  // crtRealloc under g_newMode 1 with a handler that agrees / refuses: the first HeapReAlloc fails, the handler runs, the retry
  // succeeds (or the call gives up)
  function reallocH(yes) {
    __freeze.thread();
    const out = { gex: snd(0xb5d38) };
    __freeze.save(snd(0xb5588), 8);
    const si = heapSI({ realloc: true, after: true });
    Object.assign(out, si);
    __freeze.setU32(snd(0xb5588), 1);
    __freeze.setU32(snd(0xb558c), handlerStub(si.cell, yes ? 1 : 0).toUInt32());
    out.gmode = snd(0xb5588);
    out.tbl = snd(0xb5d3c).readPointer();
    out.b0 = block(0x40, 0x5100);
    return out;
  }
  F.s411_realloc_yes = function () { return reallocH(true); };
  F.s411_realloc_no = function () { return reallocH(false); };
  // ---- the vector iterators: arrays of 0x10 elements, recording constructor / destructor stubs, and the real
  // VoiceBase_initListSlot (thunk 0x20db) / VoiceBase_clearList (thunk 0x256d) as a second pair
  F.s411_iter = function () {
    const out = {};
    out.log = zero(0x100);
    out.recf = rec(out.log);
    out.a0 = block(0x110, 0x5200);
    out.a1 = block(0x4c * 4, 0x5201);
    out.a2 = zero(0x18 * 0x10);
    out.ctorReal = snd(0x20db);
    out.dtorReal = snd(0x256d);
    return out;
  };
  // ---- ChannelDesc_dtor 0x244a0 / VoiceBase_initListSlot 0x29b80: records
  F.s411_records = function () {
    const out = {};
    for (let i = 0; i < 10; i++) out['b' + i] = block(0x60, 0x5300 + i);
    return out;
  };
  // ---- Ch5bb90_set3DBuffer 0x2c270: o0 no child, o1 a child whose slot 0xec is a stub (stores its argument at +0x64, returns
  // 0x77), o2 a child that is itself a Ch5bb90 (its slot 0xec is the real thunk 0x1e7e) with the stub child below it
  F.s411_set3d = function () {
    const out = {};
    const stub = code([0x8b, 0x44, 0x24, 0x04, 0x89, 0x41, 0x64, 0xb8, 0x77, 0x00, 0x00, 0x00, 0xc2, 0x04, 0x00]);
    const vtStub = zero(0x100);
    vtStub.add(0xec).writePointer(stub);
    const vtReal = zero(0x100);
    vtReal.add(0xec).writePointer(snd(0x1e7e));
    out.vts = vtStub;
    out.vtr = vtReal;
    const leaf = block(0x80, 0x5400);
    leaf.writePointer(vtStub);
    const mid = block(0x80, 0x5401);
    mid.writePointer(vtReal);
    mid.add(0x40).writePointer(leaf);
    const o0 = block(0x80, 0x5402);
    o0.add(0x40).writeU32(0);
    const o1 = block(0x80, 0x5403);
    o1.add(0x40).writePointer(leaf);
    const o2 = block(0x80, 0x5404);
    o2.add(0x40).writePointer(mid);
    Object.assign(out, { leaf: leaf, mid: mid, o0: o0, o1: o1, o2: o2 });
    return out;
  };
})();
