// Fixtures for batch c3dl (round 26, 2026-10-10): golf_clean.exe rows earlier batches dropped for the CRT heap or a Win32
// import. Every fixture combines the shared stand-ins:
//   - re/frida/js/heapstandin.js  (malloc 0x004a504f / operator new 0x004a57f1 carve from an arena whose header is state;
//     free 0x004a5007 / operator delete 0x004a4ffc only record),
//   - re/frida/js/importstandin.js (the exe's IAT slots point at recording stubs on the A/B thread; the call log is state),
//   - re/frida/js/freeze.js        (the game's first thread is frozen for keys that touch a live global; every global is
//     saved and restored before it resumes),
// and, for CRT routines that are not IAT imports (fclose 0x004a609f), a private A/B-thread-only replacement (crtStand).
// Only our own synthetic values: no game text, no live game object. Nothing here calls the real API or the real CRT
// file routines with a fixture value.
//
// Reimplementations: shim/src/re/c3dl.cpp. Registry: re/frida/registry.d/c3dl.py. Purposes: log/c3/c3dl_purpose.md.
(function () {
  const F = {};
  const keep = function (x) { globalThis.__diffKeepAlive.push(x); return x; };
  function words(p, vals) { vals.forEach(function (v, i) { p.add(4 * i).writeU32(v >>> 0); }); }
  function junk(p, n, tag) { for (let k = 0; k < n; k += 4) p.add(k).writeU32((tag + k) >>> 0); }
  function u32b(v) { v >>>= 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; }
  function cstr(s) { return Memory.allocUtf8String(s); }

  // ---- TIMERS. The shared stand-ins (heapstandin.js, importstandin.js) revert themselves from setTimeout(0). A pending
  // zero-delay timer can run as soon as ANOTHER thread takes the JavaScript lock, and the game's own threads enter it through
  // the stand-ins' forwarding callbacks (malloc / free are replaced): the revert then sometimes fired in the middle of a key
  // and the real allocator or the real fclose ran on fixture pointers (access violations in the key that follows another
  // stand-in key; c3dl_runall2 / runall3; the same race is the likely origin of the false REDs c3dd saw). setTimeout is
  // read-only, so the timer cannot be stretched; instead every key that installs a stand-in first freezes the game's first
  // thread (the one that allocates all the time) with the shared helper, which leaves no game thread entering the
  // JavaScript runtime while the vectors run. The freeze is released with the key's flush.
  let frozenNow = false;
  function standInGuard() {
    if (frozenNow) return;
    __freeze.thread();
    frozenNow = true;
    __freeze.atFlush(function () { frozenNow = false; });
  }
  function importOn(spec) { standInGuard(); return __importStandIn(spec); }
  function heapStandIn2(spec) { standInGuard(); return __heapStandIn(spec); }

  // ---- heap: the exe's CRT allocation and deallocation entry points (RVAs) on the A/B thread only
  const HEAP_FREE = [0xa5007, 0xa4ffc];
  const HEAP_ALLOC = [0xa504f, 0xa57f1];
  // heapstandin.js reverts a leftover replacement and replaces it again in the SAME JavaScript turn (its install()), and in
  // back-to-back keys that sometimes leaves the new replacement ineffective (the real allocator then runs on fixture
  // blocks: access violations in the second key, c3dl_runall2 / loggerSetPath + loggerCtor). The leftover is reverted and
  // committed here first, so install() finds nothing to revert.
  function heapPreRevert(o) {
    const base = Process.getModuleByName('golf_clean.exe').base;
    const rvas = [].concat(HEAP_FREE, (o && Array.isArray(o.alloc)) ? o.alloc : HEAP_ALLOC, [0xa6980]);
    rvas.forEach(function (r) { try { Interceptor.revert(base.add(r)); } catch (e) { /* not replaced */ } });
    Interceptor.flush();
  }
  function heapOn(out, o) {
    o = o || {};
    heapPreRevert(o);
    const spec = { module: 'golf_clean.exe', free: HEAP_FREE, arenaSize: o.arenaSize || 0x4000, exhaust: 'null' };
    if (Array.isArray(o.alloc)) spec.alloc = o.alloc;
    else if (o.alloc !== false) spec.alloc = HEAP_ALLOC;
    if (o.failMask) spec.failMask = o.failMask;
    const h = heapStandIn2(spec);
    out.arena = h.arena;
    out.freelog = h.log;
    return h;
  }

  // ---- a CRT function that is not an IAT import, replaced for one key on the A/B thread only (the heap stand-in's
  // pattern: a token per install, a timer that only undoes its own). impl runs on the A/B thread; every other thread is
  // forwarded to the real routine.
  const crtLive = {};
  let crtToken = 1;
  function crtStand(va, rt, argt, impl, abi) {
    standInGuard();
    const tid = Process.getCurrentThreadId();
    const at = ptr(va), k = at.toString();
    if (crtLive[k]) {
      try { Interceptor.revert(at); } catch (e) { /* already reverted */ }
      delete crtLive[k];
      Interceptor.flush();            // commit the revert BEFORE the new replace: without it a replace in the same turn can be lost
    }
    const orig = keep(abi ? new NativeFunction(at, rt, argt, abi) : new NativeFunction(at, rt, argt));
    const fn = function () {
      if (Process.getCurrentThreadId() !== tid) return orig.apply(null, arguments);
      return impl.apply(null, arguments);
    };
    const cb = keep(abi ? new NativeCallback(fn, rt, argt, abi) : new NativeCallback(fn, rt, argt));
    Interceptor.replace(at, cb);
    const tok = crtToken++;
    crtLive[k] = tok;
    Interceptor.flush();
    setTimeout(function () {
      if (crtLive[k] !== tok) return;
      Interceptor.revert(at);
      delete crtLive[k];
      Interceptor.flush();
    }, 0);
  }

  // ---- recording stubs (machine code; no JavaScript runs inside a vector): each call appends {tag, ecx or 0xC0C0C0C0,
  // stack args...} to rec (rec+0 byte cursor, entries from rec+0x10) and returns cursor ^ tag; thiscall stubs pop their
  // arguments. c3cu's / c3cz's design.
  function stubBytes(rec, tag, n, thiscall, retOff) {
    const r = u32b(rec.toUInt32());
    const b = [];
    b.push(0xA1, ...r);                                              // mov eax, [REC]
    b.push(0x05, ...r);                                              // add eax, REC
    b.push(0xC7, 0x00, ...u32b(tag));                                // mov dword [eax], tag
    if (thiscall) b.push(0x89, 0x48, 0x04);                          // mov [eax+4], ecx
    else b.push(0xC7, 0x40, 0x04, ...u32b(0xC0C0C0C0));
    for (let i = 1; i <= n; i++) b.push(0x8B, 0x54, 0x24, 4 * i, 0x89, 0x50, 4 + 4 * i);
    b.push(0x81, 0x05, ...r, ...u32b(4 * (n + 2)));                  // add dword [REC], 4(n+2)
    if (retOff !== undefined) {
      b.push(0x8B, 0x41, retOff);                                    // mov eax, [ecx + retOff] (this's result field)
    } else {
      b.push(0xA1, ...r);                                            // mov eax, [REC]
      b.push(0x35, ...u32b(tag));                                    // xor eax, tag
    }
    if (thiscall && n > 0) b.push(0xC2, 4 * n, 0x00); else b.push(0xC3);
    return b;
  }
  function recEnv() {
    const size = Process.pageSize * 4;
    const base = keep(Memory.alloc(size));
    Memory.protect(base, size, 'rwx');
    let off = 0;
    function emit(bytes) {
      const p = base.add(off);
      p.writeByteArray(bytes);
      off += (bytes.length + 15) & ~15;
      if (off > size) throw new Error('c3dl: code arena full');
      return p;
    }
    const rec = keep(Memory.alloc(0x400));
    rec.writeByteArray(new Array(0x400).fill(0));
    rec.writeU32(0x10);
    return { rec: rec, emit: emit, stub: function (tag, n, retOff) { return emit(stubBytes(rec, tag, n, true, retOff)); },
             stubC: function (tag, n) { return emit(stubBytes(rec, tag, n, false)); } };
  }

  // ====================================================================================================================
  // callSoundHook94 0x004840c0: the module handle 0x0083afc0 and the function pointer 0x0083af94 (both saved)
  // The hook is a native callback (cdecl, no arguments) that counts its calls in hookrec+0 and returns the row's value.
  // Row: [handle, hook result]. A zero handle never reaches the pointer; a NULL pointer with a handle is not run (it
  // would fault in both arms).
  // ====================================================================================================================
  const HOOK = [
    [0, 0x111], [1, 0xc00], [0x5000, 0xc01], [1, 0], [0xffffffff, 0x80000000], [2, 0xffffffff], [0, 0], [7, 0x1234abcd],
    [0x10, 0xc0f], [0x83afc0, 7], [0x80000000, 0xff00], [1, 0xc00],
  ];
  HOOK.forEach(function (row, i) {
    F['c3dl_hook_' + i] = function () {
      const rec = keep(Memory.alloc(16));
      rec.writeByteArray(new Array(16).fill(0));
      const cb = keep(new NativeCallback(function () {
        rec.writeU32(rec.readU32() + 1);
        return row[1] >>> 0;
      }, 'uint32', []));
      standInGuard();
      __freeze.save(ptr(0x0083afc0), 4);
      __freeze.save(ptr(0x0083af94), 4);
      __freeze.setU32(ptr(0x0083afc0), row[0]);
      __freeze.setU32(ptr(0x0083af94), cb.toUInt32());
      return { hookrec: rec };
    };
  });

  // ====================================================================================================================
  // Timer::ctor 0x00486c90 / StrList::ctor 0x00487210: twelve objects of 0x40 bytes filled with a different junk pattern
  // each, so a constructor that writes a field the original does not (or misses one it writes) shows in the bytes
  // ====================================================================================================================
  function junkObjects(tag) {
    const out = {};
    const blk = keep(Memory.alloc(0x40 * 12));
    for (let i = 0; i < 12; i++) {
      const o = blk.add(0x40 * i);
      junk(o, 0x40, tag + i * 0x01010101);
      out['o' + i] = o;
    }
    out.oblk = blk;
    return out;
  }
  F.c3dl_timer_ctor = function () { return junkObjects(0x3c000000); };
  F.c3dl_strlist_ctor = function () { return junkObjects(0x5e000000); };

  // ====================================================================================================================
  // MappedFile::scalarDtor 0x00492da0: MappedFile objects (0x20 bytes: +0 vtable, +4 view, +8 file handle (-1 = none), +0xc
  // mapping handle) under the import stand-in (UnmapViewOfFile, CloseHandle) and the recording deallocator
  // ====================================================================================================================
  const MF_ROWS = [
    [0, -1, 0], [0x7a000001, -1, 0], [0, 0x3a02, 0], [0, -1, 0x5a03], [0x7a000004, 0x3a04, 0], [0x7a000005, -1, 0x5a05],
    [0, 0x3a06, 0x5a06], [0x7a000007, 0x3a07, 0x5a07], [0, 0, 0], [0x7a000009, 0, 0x5a09], [0, -1, 0],
    [0x7a00000b, 0x3a0b, 0x5a0b],
  ];
  F.c3dl_mf_scalar = function () {
    const out = {};
    const blk = keep(Memory.alloc(0x20 * MF_ROWS.length));
    MF_ROWS.forEach(function (r, i) {
      const o = blk.add(0x20 * i);
      words(o, [0x004bba78, r[0], r[1], r[2], 0x11110000 + i, 0x22220000 + i, 0x33330000 + i, 0x44440000 + i]);
      out['mf' + i] = o;
    });
    out.mfblk = blk;
    heapOn(out, { alloc: false });
    const s = importOn({ module: 'golf_clean.exe', imports: {
      'KERNEL32.dll!UnmapViewOfFile': { ret: 1 }, 'KERNEL32.dll!CloseHandle': { ret: 1 } } });
    out.implog = s.log;
    return out;
  };

  // ====================================================================================================================
  // Cursor::destroy 0x004884b0: objects (0x20 bytes: +4 cursor handle, +8 a sub-object, +0xc, +0x10); a sub-object is a
  // 0x10-byte block whose vtable slot 0 is a recording stub (thiscall, one argument). DestroyCursor's result is the
  // handle xor 0x5 so the return value follows the argument.
  // ====================================================================================================================
  const CUR_ROWS = [[0, 0], [0x7c01, 0], [0, 1], [0x7c03, 1], [0xffffffff, 0], [0x7c05, 1], [0, 1], [0x80000000, 1],
                    [0x7c08, 0], [1, 1], [0, 0], [0x7c0b, 1]];
  F.c3dl_cursor = function () {
    const out = {};
    const E = recEnv();
    const vt = keep(Memory.alloc(0x40));
    for (let i = 0; i < 16; i++) vt.add(4 * i).writePointer(E.stub(0xC0DE0000 + i, 1));
    const subs = keep(Memory.alloc(0x10 * CUR_ROWS.length));
    const blk = keep(Memory.alloc(0x20 * CUR_ROWS.length));
    CUR_ROWS.forEach(function (r, i) {
      const o = blk.add(0x20 * i), sub = subs.add(0x10 * i);
      junk(sub, 0x10, 0x5b000000 + i * 0x100);
      sub.writePointer(vt);
      junk(o, 0x20, 0x4c000000 + i * 0x100);
      o.add(4).writeU32(r[0] >>> 0);
      o.add(8).writePointer(r[1] ? sub : ptr(0));
      out['c' + i] = o;
    });
    out.cblk = blk;
    out.subs = subs;
    out.rec = E.rec;
    const s = importOn({ module: 'golf_clean.exe', imports: {
      'USER32.dll!DestroyCursor': { impl: function (a) { return (a[0].toUInt32() ^ 5) >>> 0; } } } });
    out.implog = s.log;
    return out;
  };

  // ====================================================================================================================
  // BinkPlayer::close 0x00487060: objects (0x10 bytes: +4 Bink handle, +8 buffer handle); _BinkClose@4 returns 1,
  // _BinkBufferClose@4 returns handle * 3 + 1
  // ====================================================================================================================
  const BINK_ROWS = [[0, 0], [0x7d01, 0], [0, 0x7d02], [0x7d03, 0x7d03], [0xffffffff, 0], [0, 0xffffffff], [0x7d06, 0x7d07],
                     [1, 1], [0x80000000, 0x80000000], [0x7d09, 0], [0, 0], [0x7d0b, 0x7d0c]];
  F.c3dl_bink = function () {
    const out = {};
    const blk = keep(Memory.alloc(0x10 * BINK_ROWS.length));
    BINK_ROWS.forEach(function (r, i) {
      const o = blk.add(0x10 * i);
      junk(o, 0x10, 0x6d000000 + i * 0x100);
      o.add(4).writeU32(r[0] >>> 0);
      o.add(8).writeU32(r[1] >>> 0);
      out['b' + i] = o;
    });
    out.bblk = blk;
    const s = importOn({ module: 'golf_clean.exe', imports: {
      'binkw32.dll!_BinkClose@4': { args: 1, ret: 1 },
      'binkw32.dll!_BinkBufferClose@4': { args: 1, impl: function (a) { return (Math.imul(a[0].toUInt32(), 3) + 1) >>> 0; } } } });
    out.implog = s.log;
    return out;
  };

  // ====================================================================================================================
  // Reader family. A Reader is 0x170 bytes (the object is 0x164): +0 vtable, +4 byte, +0x154 .. +0x160 dwords (+0x158 the
  // FILE*, +0x15c / +0x160 the buffers). fclose 0x004a609f is replaced on the A/B thread: it records the FILE* in fclog
  // (+0 count, pointers from +4) and returns the pointer's low byte.
  // Row: [FILE*, buffer a present, buffer b present].
  // ====================================================================================================================
  const RD_ROWS = [
    [0, 0, 0], [0x3f01, 0, 0], [0, 1, 0], [0, 0, 1], [0x3f04, 1, 0], [0x3f05, 0, 1], [0, 1, 1], [0x3f07, 1, 1],
    [0x3f08, 1, 1], [0, 1, 1], [0x3f0a, 0, 0], [0xffff0003, 1, 0],
  ];
  function fcloseOn(out) {
    const fclog = keep(Memory.alloc(0x40));
    fclog.writeByteArray(new Array(0x40).fill(0));
    out.fclog = fclog;
    crtStand(0x004a609f, 'int', ['pointer'], function (fp) {
      const n = fclog.readU32();
      if (n < 15) { fclog.add(4 + 4 * n).writePointer(fp); fclog.writeU32(n + 1); }
      return fp.toUInt32() & 0xff;
    });
  }
  function readers(out) {
    const blk = keep(Memory.alloc(0x170 * RD_ROWS.length));
    const bufs = keep(Memory.alloc(0x20 * 2 * RD_ROWS.length));
    RD_ROWS.forEach(function (r, i) {
      const o = blk.add(0x170 * i);
      junk(o, 0x170, 0x7e000000 + i * 0x100);
      words(o, [0x004bb084]);
      o.add(0x154).writeU32(0x154000 + i);
      o.add(0x158).writeU32(r[0] >>> 0);
      o.add(0x15c).writePointer(r[1] ? bufs.add(0x40 * i) : ptr(0));
      o.add(0x160).writePointer(r[2] ? bufs.add(0x40 * i + 0x20) : ptr(0));
      out['r' + i] = o;
    });
    out.rblk = blk;
    out.rbufs = bufs;
    return out;
  }
  F.c3dl_reader_close = function () { const out = readers({}); fcloseOn(out); return out; };
  F.c3dl_reader_free = function () { const out = readers({}); heapOn(out, { alloc: false }); fcloseOn(out); return out; };
  // allocBuffers / ctor: the arena (0x4000) is smaller than the largest request, so n = 0x20000000 returns NULL (like the
  // CRT); failMask bit 0 / bit 1 fail the first / second malloc of the call
  function readerAlloc(failMask) {
    const out = readers({});
    heapOn(out, { failMask: failMask, arenaSize: 0x4000 });
    fcloseOn(out);
    return out;
  }
  F.c3dl_reader_alloc = function () { return readerAlloc(0); };
  F.c3dl_reader_alloc_f0 = function () { return readerAlloc(1); };
  F.c3dl_reader_alloc_f1 = function () { return readerAlloc(2); };

  // ====================================================================================================================
  // Critical-section list. A list is 0x30 bytes: +4 head, +8 tail, +0xc count, +0x10 CRITICAL_SECTION (a block the
  // stand-in never enters). A node is 0x18 bytes: +0 / +4 / +8 caller words, +0xc data block, +0x10 its size, +0x14 next.
  // Shapes: [node count, data mask (bit k: node k holds a data block), size of each data block]
  // ====================================================================================================================
  const LIST_SHAPES = [
    [0, 0, 0], [1, 0, 0], [1, 1, 5], [2, 0, 0], [2, 3, 4], [3, 5, 9], [3, 7, 0x21], [4, 15, 8], [1, 1, 1], [2, 2, 3],
    [0, 0, 0], [5, 31, 6],
  ];
  function lists(out, nodeCount) {
    const lblk = keep(Memory.alloc(0x30 * LIST_SHAPES.length));
    const nblk = keep(Memory.alloc(0x18 * 8 * LIST_SHAPES.length));
    const dblk = keep(Memory.alloc(0x40 * 8 * LIST_SHAPES.length));
    LIST_SHAPES.forEach(function (sh, i) {
      const l = lblk.add(0x30 * i);
      junk(l, 0x30, 0x6c000000 + i * 0x100);
      const nodes = [];
      for (let k = 0; k < sh[0]; k++) {
        const nd = nblk.add(0x18 * (8 * i + k));
        junk(nd, 0x18, 0x6e000000 + i * 0x10000 + k * 0x100);
        const has = (sh[1] >> k) & 1;
        const d = dblk.add(0x40 * (8 * i + k));
        for (let b = 0; b < 0x40; b++) d.add(b).writeU8((0x30 + i * 7 + k * 13 + b) & 0xff);
        nd.add(0xc).writePointer(has ? d : ptr(0));
        nd.add(0x10).writeU32(has ? sh[2] : (0x1000 + k));
        nodes.push(nd);
      }
      nodes.forEach(function (nd, k) { nd.add(0x14).writePointer(k + 1 < nodes.length ? nodes[k + 1] : ptr(0)); });
      l.add(4).writePointer(nodes.length ? nodes[0] : ptr(0));
      l.add(8).writePointer(nodes.length ? nodes[nodes.length - 1] : ptr(0));
      l.add(0xc).writeU32(nodes.length);
      out['l' + i] = l;
    });
    out.lblk = lblk;
    out.nblk = nblk;
    out.dblk = dblk;
    return out;
  }
  function csImports() {
    return {
      'KERNEL32.dll!EnterCriticalSection': { ret: 0 },
      'KERNEL32.dll!LeaveCriticalSection': { impl: function (a) { return (a[0].toUInt32() & 0xffff) >>> 0; } },
    };
  }
  function listEnv(opts) {
    const out = lists({});
    heapOn(out, opts);
    out.implog = importOn({ module: 'golf_clean.exe', imports: csImports() }).log;
    // source blocks and output words
    out.src = keep(Memory.alloc(0x80));
    for (let b = 0; b < 0x80; b++) out.src.add(b).writeU8((0x90 + b * 3) & 0xff);
    const outs = keep(Memory.alloc(0x40 * 6));
    outs.writeByteArray(new Array(0x40 * 6).fill(0xee));
    for (let i = 0; i < 6; i++) out['o' + i] = outs.add(0x40 * i);
    out.outs = outs;
    return out;
  }
  F.c3dl_list = function () { return listEnv({}); };
  F.c3dl_list_fnode = function () { return listEnv({ failMask: 1 }); };
  F.c3dl_list_fdata = function () { return listEnv({ failMask: 2 }); };

  // ====================================================================================================================
  // General environment for the keys that need fixture objects, recording stubs, a fake display or heap blocks:
  // take(n, tag) carves a 16-byte aligned object out of the state block $blk (pre-filled with a junk pattern), vt(spec)
  // builds a 0x80-slot vtable outside the state (listed slots record, the rest `xor eax, eax; ret`), rec is the recorder.
  // ====================================================================================================================
  const BLK = 0x10000;
  function fill32(p, n, v) { for (let i = 0; i + 4 <= n; i += 4) p.add(i).writeU32(v >>> 0); }
  function envD(opts) {
    opts = opts || {};
    const E = recEnv();
    const fx = {};
    const blk = keep(Memory.alloc(BLK));
    fill32(blk, BLK, 0x6e6e6e6e);
    let cur = 0;
    function take(n, v) {
      const p = blk.add(cur);
      cur += (n + 15) & ~15;
      if (cur > BLK) throw new Error('c3dl: object block full');
      if (v !== undefined) fill32(p, (n + 3) & ~3, v);
      return p;
    }
    // stub code lives in recEnv's arena; a constant `xor eax, eax; ret` for the unlisted slots
    const nul = E.emit([0x33, 0xC0, 0xC3]);
    function vt(spec) {
      const t = keep(Memory.alloc(0x200));
      for (let i = 0; i < 0x80; i++) t.add(i * 4).writePointer(nul);
      for (const s of spec) t.add(s[0]).writePointer(E.stub(s[1], s[2], s[3]));
      return t;
    }
    fx.blk = blk;
    fx.rec = E.rec;
    return { fx: fx, take: take, vt: vt, stub: E.stub, stubC: E.stubC, rec: E.rec, emit: E.emit };
  }

  // ====================================================================================================================
  // netBeginOp0 0x00497c20 / netBeginOp1 0x00497c70: objects of 0x100 bytes whose sub-object at +0x64 starts with the vtable
  // pointer (slot +0x90 is a recording stub, thiscall, no arguments). Row: [+0xe8 flag word]. The net global 0x008400b0
  // is saved and set to 1 (on) or 0 (off).
  // ====================================================================================================================
  const NET_FLAGS = [0, 0x20000000, 0x40000000, 0x60000000, 0x1fffffff, 0x80000000, 0xffffffff, 0x00000001, 0x9fffffff,
                     0, 0x10000000, 0x5fffffff];
  function netObjs(net) {
    const E = envD();
    const vt = E.vt([[0x90, 0xAE70, 0]]);
    NET_FLAGS.forEach(function (f, i) {
      const o = E.take(0x100, 0x4e450000 + i * 0x100);
      o.add(0x64).writePointer(vt);
      o.add(0xe8).writeU32(f);
      E.fx['n' + i] = o;
    });
    standInGuard();
    __freeze.save(ptr(0x008400b0), 4);
    __freeze.setU32(ptr(0x008400b0), net);
    return E.fx;
  }
  F.c3dl_net_on = function () { return netObjs(1); };
  F.c3dl_net_off = function () { return netObjs(0); };

  // ====================================================================================================================
  // screenFlip 0x00483ba0: the display 0x0083ad50 (saved) is a fake whose slot +0x124 records and returns cursor ^ tag
  // ====================================================================================================================
  for (let v = 0; v < 10; v++) {
    F['c3dl_flip_' + v] = function () {
      const E = envD();
      const disp = E.take(0x10, 0x44440000 + v);
      disp.writePointer(E.vt([[0x124, 0xF11F0000 + v * 0x111, 0]]));
      standInGuard();
      __freeze.save(ptr(0x0083ad50), 4);
      __freeze.setU32(ptr(0x0083ad50), disp.toUInt32());
      return E.fx;
    };
  }

  // ====================================================================================================================
  // DC_calla0 0x00475c20: DC objects (+4 the inner object, NULL for two of them); the inner object's slot +0xa0 records its
  // four arguments. Argument objects `a` carry the value read at +4.
  // ====================================================================================================================
  F.c3dl_dc = function () {
    const E = envD();
    const ivt = E.vt([[0xa0, 0xDCA0, 4]]);
    for (let i = 0; i < 12; i++) {
      const dc = E.take(0x10, 0x44430000 + i);
      const inner = E.take(0x10, 0x49490000 + i);
      inner.writePointer(ivt);
      dc.add(4).writePointer((i === 3 || i === 9) ? ptr(0) : inner);
      E.fx['d' + i] = dc;
      const a = E.take(0x10, 0x41410000 + i);
      a.add(4).writeU32((0xa000 + i * 0x111) >>> 0);
      E.fx['a' + i] = a;
    }
    return E.fx;
  };

  // ====================================================================================================================
  // freeCursors 0x0049ead0: the three global Surfaces 0x00840830 / 0x00840860 / 0x00840890 (0x30 bytes each, saved) take
  // fixture contents {+4 handle, +8 name block, +0xc cache}; the display 0x0083ad50 is a fake whose slots +0x90 / +0x94
  // record (Cache::reset, Surface::free), or NULL.
  // ====================================================================================================================
  function cacheObj(E, i, handle, mask) {
    const c = E.take(0x58, 0x43430000 + i);
    c.add(4).writeU32(handle);
    const ovt = E.vt([[0, 0xD0D00000 + i, 1]]);
    for (let k = 0; k < 5; k++) {
      const e = c.add(8 + 0x10 * k);
      if ((mask >> k) & 1) { const o = E.take(0x10, 0x4f420000 + k); o.writePointer(ovt); e.add(4).writePointer(o); }
      else e.add(4).writePointer(ptr(0));
      e.add(0xc).writePointer(((mask >> (k + 5)) & 1) ? E.take(0x10, 0x42420000 + k) : ptr(0));
    }
    return c;
  }
  function surfaceObj(E, i) {
    const s = E.take(0x30, 0x53550000 + i);
    s.add(4).writeU32((i & 1) ? 0x4800 + i : 0);
    s.add(8).writePointer((i & 2) ? E.take(0x20, 0x4e4d0000 + i) : ptr(0));
    s.add(0xc).writePointer((i & 4) ? cacheObj(E, 0x40 + i, (i & 8) ? 0x4900 + i : 0, 0x3ff ^ (i * 0x55)) : ptr(0));
    return s;
  }
  function cursorsEnv(variant, withDisplay) {
    const E = envD();
    heapOn(E.fx, { alloc: false });
    const sv = [variant & 15, (variant * 5 + 3) & 15, (variant * 7 + 9) & 15].map(function (i) { return surfaceObj(E, i); });
    const bytes = sv.map(function (s) { return s.readByteArray(0x30); });
    const disp = E.take(0x20, 0);
    disp.writePointer(E.vt([[0x90, 0xD1590000, 1], [0x94, 0xD1594000, 1]]));
    standInGuard();
    __freeze.save(ptr(0x0083ad50), 4);
    ptr(0x0083ad50).writePointer(withDisplay ? disp : ptr(0));
    __freeze.save(ptr(0x00840830), 0x90);
    ptr(0x00840830).writeByteArray(bytes[2]);
    ptr(0x00840860).writeByteArray(bytes[0]);
    ptr(0x00840890).writeByteArray(bytes[1]);
    return E.fx;
  }
  F.c3dl_cursors_a = function () { return cursorsEnv(15, true); };
  F.c3dl_cursors_b = function () { return cursorsEnv(6, true); };
  F.c3dl_cursors_c = function () { return cursorsEnv(11, true); };
  F.c3dl_cursors_d = function () { return cursorsEnv(2, true); };
  F.c3dl_cursors_e = function () { return cursorsEnv(13, false); };
  F.c3dl_cursors_f = function () { return cursorsEnv(0, true); };

  // ====================================================================================================================
  // loggerSetPath 0x004a01d0 / loggerCtor 0x004a0180: logger objects (0x10 bytes, +4 the path block or NULL). The path
  // opener 0x004a00f0 and fclose 0x004a609f are replaced on the A/B thread: the opener records (path, mode) in oplog (+0
  // count, entries of two words from +4) and returns NULL for a path starting with 'x', else 0x5f000000 + the first
  // character; fclose records its argument in fclog. malloc / free run under the heap stand-in (failMask bit 0 fails the
  // path block).
  // ====================================================================================================================
  function loggerEnv(failMask) {
    const E = envD();
    const out = E.fx;
    for (let i = 0; i < 12; i++) {
      const lg = E.take(0x10, 0x4c470000 + i);
      lg.add(4).writePointer((i % 3 === 1) ? E.take(0x20, 0x42420000 + i) : ptr(0));
      out['g' + i] = lg;
    }
    ['', 'a', 'log.txt', 'xfail.txt', 'c:\\golf\\game.log', 'x', 'y'.repeat(70), 'x'.repeat(33), 'sim.log'].forEach(function (s, i) {
      out['p' + i] = keep(Memory.allocUtf8String(s));
    });
    heapOn(out, { failMask: failMask });
    const oplog = keep(Memory.alloc(0x60));
    oplog.writeByteArray(new Array(0x60).fill(0));
    out.oplog = oplog;
    crtStand(0x004a00f0, 'pointer', ['pointer', 'pointer'], function (path, mode) {
      const n = oplog.readU32();
      if (n < 10) { oplog.add(4 + 8 * n).writePointer(path); oplog.add(8 + 8 * n).writePointer(mode); oplog.writeU32(n + 1); }
      const c = path.readU8();
      return c === 0x78 ? ptr(0) : ptr(0x5f000000 + c);
    });
    fcloseOn(out);
    return out;
  }
  F.c3dl_logger = function () { return loggerEnv(0); };
  F.c3dl_logger_f0 = function () { return loggerEnv(1); };

  // ====================================================================================================================
  // HotList-bearing objects ("images", 0x2c0 bytes): a HotList at +0xb4 and one at +0x254 (0x60 bytes each), the loop words
  // at +0x114 .. +0x254 and a counter at +0x2b4. The Timer inside each HotList and the entry arrays are built as c3de's
  // fixtures build them (the Timer family needs the fake display and the import stand-in: timerEnv).
  // ====================================================================================================================
  const TROWS = [
    [0, 0, 0, 0, 10, 5, 0], [1, 0x7101, 1, 0, 10, 1, 0], [0, 0x7102, 0, 1, 13, 5, 0], [1, 0x7203, 1, 1, 0x32, 5, 1],
    [0, 0x7204, 1, 0, 1000, 5, 0], [3, 0, 0, 1, 0x31, 0, 0], [0xfffffffe, 0x7206, 0x80000000, 0, 0xffffffff, 5, 0],
    [1, 5, 0, 0, 0, 5, 0], [2, 0x7208, 1, 0, 50, 2, 1], [0xffffffff, 0x7109, 0, 7, 49, 9, 0], [1, 0, 0, 0, 777, 5, 0],
    [0, 0x720b, 2, 2, 0x80000000, 5, 0],
  ];
  function writeTimer(o, r) {
    words(o.add(8), [r[0], r[1], r[2], r[3]]);
    o.add(0x20).writeU32(r[4] >>> 0);
    o.add(0x24).writeU32(r[6]);
    o.add(0x28).writeU32(r[5]);
  }
  function arrayPool(size) {
    const base = keep(Memory.alloc(size));
    base.writeByteArray(new Array(size).fill(0));
    return { base: base, size: size, cur: 0 };
  }
  // an entry array of `cap` constructed elements (0x20 bytes, vtable 0x004bba70 at +0): the header at -4 holds cap; the
  // first `used` elements carry (+0x14, +0x18, text)
  function mkArray(pl, cap, used, spec, tag) {
    const need = (4 + 0x20 * Math.max(cap, 1) + 0x20 + 15) & ~15;
    if (pl.cur + need > pl.size) throw new Error('c3dl: array pool exhausted');
    const blk = pl.base.add(pl.cur);
    pl.cur += need;
    junk(blk, need, 0x5a000000 + tag);
    blk.writeU32(cap);
    const base = blk.add(4);
    base.writeU32(0x004bba70);
    for (let i = 0; i < cap; i++) {
      const e = base.add(0x20 * i);
      e.writeU32(0x004bba70);
      const r = i < used ? spec(i) : [0, 0, null];
      e.add(0x14).writeU32(r[0] >>> 0);
      e.add(0x18).writeU32(r[1] >>> 0);
      e.add(0x1c).writeU32(r[2] ? r[2].toUInt32() : 0);
    }
    return base;
  }
  function placeHL(o, r) {
    junk(o, 0x60, 0x77000000 + (r.t || 0) * 0x100);
    o.writeU32(0x004bba74);
    writeTimer(o.add(0x20), TROWS[(r.t || 0) % TROWS.length]);
    words(o.add(4), [0xffffffff, 0xffffffff]);
    o.add(0x50).writePointer(r.arr || ptr(0));
    o.add(0x54).writeU32(r.cap >>> 0);
    o.add(0x58).writeU32(r.count >>> 0);
    o.add(0x5c).writeU32((r.delay === undefined ? 0x3e8 : r.delay) >>> 0);
  }
  // The Timer family's imports and the display it dispatches through (c3de's design): a fake display (+4 the HWND slot
  // +0x20 returns, +8 calls of slot +0x150, +0xc calls of any other NativeCallback slot) replaces the game's display
  // 0x0083ad50 while the first thread is frozen; 0x0083ad4c is saved; the keep-tips guard 0x0083d348 is set to 0.
  // `slots` overrides vtable slots with machine stubs ([offset, pointer]); E is the caller's envD when it needs one recorder.
  function timerEnv(out, display, extraImports, E0, slots) {
    const E = E0 || envD();
    const disp = E.take(0x20, 0);
    words(disp, [0, 0x0000beef, 0, 0]);
    const hwnd = keep(new NativeCallback(function (self) { return self.add(4).readU32(); }, 'uint32', ['pointer'], 'thiscall'));
    const s150 = keep(new NativeCallback(function (self) { self.add(8).writeU32(self.add(8).readU32() + 1); return 0; },
                                         'uint32', ['pointer'], 'thiscall'));
    const other = keep(new NativeCallback(function (self) { self.add(0xc).writeU32(self.add(0xc).readU32() + 1); return 0; },
                                          'uint32', ['pointer'], 'thiscall'));
    const vt = keep(Memory.alloc(0x200));
    for (let k = 0; k < 0x200; k += 4) vt.add(k).writePointer(other);
    vt.add(0x20).writePointer(hwnd);
    vt.add(0x150).writePointer(s150);
    (slots || []).forEach(function (sl) { vt.add(sl[0]).writePointer(sl[1]); });
    disp.writePointer(vt);
    out.disp = disp;
    const imports = {
      'WINMM.dll!timeSetEvent': function (a) { const d = a[0].toUInt32(); return d === 13 ? 0 : (0x7100 | (d & 0xff)); },
      'WINMM.dll!timeKillEvent': { ret: 0 },
      'USER32.dll!SetTimer': function (a) { const d = a[2].toUInt32(); return d === 777 ? 0 : (0x7200 | (d & 0xff)); },
      'USER32.dll!KillTimer': { ret: 1 } };
    Object.keys(extraImports || {}).forEach(function (k) { imports[k] = extraImports[k]; });
    out.implog = importOn({ module: 'golf_clean.exe', imports: imports }).log;
    standInGuard();
    __freeze.setU32(ptr(0x0083ad50), display === 'fake' ? disp.toUInt32() : 0);
    __freeze.save(ptr(0x0083ad4c), 4);
    __freeze.setU32(ptr(0x0083d348), 0);
    return out;
  }
  // live globals a constructor reads: saved, then set to a deterministic pattern per seed (value = f(address, seed))
  function pat(addr, seed) { return (Math.imul(addr, 0x9E3779B1) + Math.imul(seed + 1, 0x01000193)) >>> 0; }
  function setGlobals(list, seed) {
    list.forEach(function (a) {
      __freeze.save(ptr(a), 4);
      ptr(a).writeU32(pat(a, seed));
    });
  }
  function range4(lo, hi) { const r = []; for (let a = lo; a <= hi; a += 4) r.push(a); return r; }
  const G_IMAGE = [0x004e4210, 0x0083ad44];
  const G_VIEW = range4(0x0083fe78, 0x0083ff0c).concat([0x0083ab64, 0x0083ab68, 0x0083ab6c, 0x0083ab70], range4(0x004e42c8, 0x004e42e8));
  const G_VIEW4804 = G_VIEW.concat([0x0083ac18]);
  const G_EDIT = [0x004e43f0, 0x004e43f4, 0x004e43f8, 0x004e4408, 0x004e440c, 0x0083afd8, 0x0083afdc];

  // Rows: [capacity, count, counter at +0x2b4, tip-timer row]. The first `count` elements carry text.
  const IMG_ROWS = [
    [4, 2, 0, 0], [4, 4, 1, 1], [3, 0, 5, 2], [0, 0, 0x13, 3], [10, 3, 0x14, 4], [2, 1, 0x15, 5], [6, 6, 0x7fffffff, 6],
    [5, 2, 0xffffffff, 7], [1, 1, 0x100, 8], [8, 7, 0x12, 9], [4, 0, 0x14, 10], [3, 3, 3, 11],
  ];
  // opts: failMask, imports, size (bytes per object), rowInit(o, i, E), slots, after(out, E)
  function images(opts) {
    const out = {};
    const E = envD();
    const texts = [];
    for (let i = 0; i < 40; i++) texts.push(cstr('tip' + i));
    const size = opts.size || 0x2c0;
    const blk = keep(Memory.alloc(size * IMG_ROWS.length));
    const pl = arrayPool(0x4000);
    out.apool = pl.base;
    IMG_ROWS.forEach(function (r, i) {
      const o = blk.add(size * i);
      junk(o, size, 0x69000000 + i * 0x100);
      const arrA = r[0] > 0 ? mkArray(pl, r[0], r[1], function (k) { return [k, k + 1, texts[(i * 3 + k) % 40]]; }, i * 0x100) : null;
      const arrB = r[0] > 0 ? mkArray(pl, r[0], r[1], function (k) { return [k + 2, k + 3, (k & 1) ? texts[(i + k) % 40] : null]; }, i * 0x100 + 0x80) : null;
      placeHL(o.add(0xb4), { arr: arrB, cap: r[0], count: r[1], t: r[3] });
      placeHL(o.add(0x254), { arr: arrA, cap: r[0], count: r[1], t: r[3] });
      // the loop words +0x114 .. +0x254: a mix of zero and non-zero (the second half is the one the loop tests)
      for (let k = 0; k < 0x28; k++) {
        o.add(0x114 + 4 * k).writeU32((0x11000000 + k) >>> 0);
        o.add(0x1b4 + 4 * k).writeU32(((k * 7 + i) % 3 === 0) ? 0 : ((0x22000000 + k * 0x101) >>> 0));
      }
      o.add(0x2b4).writeU32(r[2] >>> 0);
      if (opts.rowInit) opts.rowInit(o, i, E);
      out['i' + i] = o;
    });
    out.iblk = blk;
    heapOn(out, { arenaSize: 0x8000, failMask: opts.failMask || 0 });
    timerEnv(out, 'fake', opts.imports, E, opts.slots);
    if (opts.after) opts.after(out, E);
    out.eblk = E.fx.blk;
    out.rec = E.fx.rec;
    return out;
  }
  F.c3dl_img_reset = function () { return images({}); };
  F.c3dl_img_reset_f = function () { return images({ failMask: 1 }); };
  // addDropdownSpot: MessageBoxA's result is the first character of the text xor 0x100 times the caption's
  F.c3dl_img_dropdown = function () {
    return images({ imports: {
      'USER32.dll!MessageBoxA': { args: 4, impl: function (a) {
        const t = a[1].isNull() ? 0x1ff : a[1].readU8(), c = a[2].isNull() ? 0x2ff : a[2].readU8();
        return (t + 0x100 * c) >>> 0;
      } } } });
  };

  // ====================================================================================================================
  // Constructors: junk-filled objects (the whole block is state, so a missing or extra store shows), the live globals the
  // constructor copies set per key. The Timer family runs under timerEnv (fake display, import stand-in).
  // Image_ctor reads 0x004e4210 and 0x0083ad44; View_ctor copies 38 globals and 13 more; the EditBox adds seven.
  // ====================================================================================================================
  function ctorObjects(size, tag, n) {
    const E = envD();
    const out = {};
    const blk = keep(Memory.alloc(size * n));
    for (let i = 0; i < n; i++) {
      const o = blk.add(size * i);
      junk(o, size, tag + i * 0x01010101);
      out['o' + i] = o;
    }
    out.oblk = blk;
    timerEnv(out, 'fake', null, E);
    return out;
  }
  [['image', 0x2c0, G_IMAGE], ['view', 0x280, G_VIEW], ['view4804', 0x580, G_VIEW4804],
   ['editbox', 0x600, G_VIEW4804.concat(G_EDIT)], ['globalobj', 0x600, G_VIEW4804], ['editreset', 0x600, G_EDIT]].forEach(function (c) {
    for (let seed = 0; seed < 3; seed++) {
      F['c3dl_ctor_' + c[0] + '_' + seed] = function () {
        const out = ctorObjects(c[1], 0x71000000, 12);
        setGlobals(c[2], seed * 7);
        return out;
      };
    }
  });

  // ====================================================================================================================
  // resetViewState47cfd0 0x0047cfd0: argument-free. The four globals 0x0083a4e8 .. 0x0083a4f4 take a pattern per key;
  // Image_ctor reads 0x004e4210 / 0x0083ad44.
  // ====================================================================================================================
  for (let seed = 0; seed < 6; seed++) {
    F['c3dl_resetview_' + seed] = function () {
      const out = ctorObjects(0x40, 0x72000000, 1);
      setGlobals(G_IMAGE.concat(range4(0x0083a4e8, 0x0083a4f4)), seed * 5);
      return out;
    };
  }

  // ====================================================================================================================
  // Surface_createFull 0x00474dd0: images as above; the display's slot +0x7c is a recording machine stub returning the
  // dword at display+0x10 (the object), the object's slot +4 a stub returning the dword at object+0x10 (the key's result).
  // ====================================================================================================================
  function createFullKey(result, tgt) {
    let obj = null, dispStub = null, relStub = null;
    const out = images({
      rowInit: function (o, i, E) {
        if (!obj) {
          obj = E.take(0x20, 0x4f4f0000);
          obj.writePointer(E.vt([[4, 0xCF10, 4, 0x10]]));
          obj.add(0x10).writeU32(result >>> 0);
          dispStub = E.stub(0xCF7C, 2, 0x10);
        }
        o.add(8).writeU32((i % 3 === 0) ? 0 : (0x500 + i));
        o.add(4).writeU32(i % 2 ? 0x4400 + i : 0);
        o.add(0xac).writePointer((i % 3 === 1) ? ptr(0) : cacheObj(E, i, 0, 0x3ff ^ (i * 0x55)));
        if (!relStub) relStub = E.stub(0xD8C0, 1);
      },
    });
    out.disp.readPointer().add(0x8c).writePointer(relStub);
    out.disp.readPointer().add(0x7c).writePointer(dispStub);
    out.disp.add(0x10).writePointer(obj);
    __freeze.save(ptr(0x0083ad44), 4);
    // setDrawTarget 0x004762d0 reads the pointer's +4 word: NULL (returns 3), an object with +4 = 0, an object with +4 set
    const tobj = keep(Memory.alloc(0x10));
    tobj.writeByteArray(new Array(16).fill(0));
    if (tgt === 'obj') tobj.add(4).writeU32(0x7a7a7a7a);
    ptr(0x0083ad44).writePointer(tgt === 'null' ? ptr(0) : tobj);
    __freeze.save(ptr(0x00839ab0), 4);
    ptr(0x00839ab0).writeU32(0);
    out.obj = obj;
    return out;
  }
  F.c3dl_createfull_0 = function () { return createFullKey(0, 'obj'); };
  F.c3dl_createfull_0n = function () { return createFullKey(0, 'null'); };
  F.c3dl_createfull_0z = function () { return createFullKey(0, 'zero'); };
  F.c3dl_createfull_5 = function () { return createFullKey(5, 'obj'); };
  F.c3dl_createfull_neg = function () { return createFullKey(0xffffffff, 'zero'); };

  // ====================================================================================================================
  // Sound library loader. The table 0x0083af6c .. 0x0083afc8 (11 function pointers, the module handle 0x0083afc0, the
  // flag 0x0083afc4) is saved and filled with junk under the freeze; LoadLibraryA / GetProcAddress / FreeLibrary / MessageBoxA
  // are the import stand-in's recording stubs. GetProcAddress hands out a distinct dummy pointer per ordinal, except that
  // ordinal 11 (the table slot 0x0083af94 callSoundHook94 jumps through) is a native callback returning the key's version
  // word, and `fail` makes one ordinal return NULL.
  // ====================================================================================================================
  const SND = 0x0083af6c, SND_LEN = 0x5c;
  function soundTable(handle, junkSeed) {
    standInGuard();
    __freeze.save(ptr(SND), SND_LEN);
    junk(ptr(SND), SND_LEN, junkSeed);
    ptr(0x0083afc0).writeU32(handle >>> 0);
  }
  function soundImports(o) {
    const cb = o.version === undefined ? null : keep(new NativeCallback(function () { return o.version >>> 0; }, 'uint32', []));
    const imports = {
      'KERNEL32.dll!LoadLibraryA': { args: 1, ret: o.lib === undefined ? 0x7e000000 : o.lib },
      'KERNEL32.dll!GetProcAddress': { args: 2, impl: function (a) {
        const ord = a[1].toUInt32();
        if (ord === o.fail) return 0;
        return ord === 11 && cb ? cb : ptr(0x7e100000 + ord * 4);
      } },
      'KERNEL32.dll!FreeLibrary': { args: 1, impl: function (a) { return ((a[0].toUInt32() & 0xff) + 1) >>> 0; } },
      'USER32.dll!MessageBoxA': { args: 4, ret: 1 },
    };
    return importOn({ module: 'golf_clean.exe', imports: imports }).log;
  }
  // key: [handle before, LoadLibraryA result, version word or undefined, failing ordinal or undefined]
  const SNDKEYS = {
    loaded: [0x1234, 0x7e000000, 0xc00, undefined], loadfail: [0, 0, 0xc00, undefined], ok: [0, 0x7e000000, 0xc00, undefined],
    okc07: [0, 0x7e000000, 0xc07, undefined], oklo: [0, 0x7e000000, 0xc00 | 0xff, undefined],
    okhi: [0, 0x7e000000, 0x10c00, undefined], old: [0, 0x7e000000, 0x0b00, undefined], newer: [0, 0x7e000000, 0xd00, undefined],
    zero: [0, 0x7e000000, 0, undefined], f1: [0, 0x7e000000, 0xc00, 1], f5: [0, 0x7e000000, 0xc00, 5],
    f11: [0, 0x7e000000, 0xc00, 11],
  };
  Object.keys(SNDKEYS).forEach(function (k) {
    const c = SNDKEYS[k];
    F['c3dl_snd_' + k] = function () {
      const out = {};
      out.implog = soundImports({ lib: c[1], version: c[2], fail: c[3] });
      soundTable(c[0], 0x75000000);
      __freeze.save(ptr(0x0083afc0), 4);
      return out;
    };
  });
  // freeSoundLib / clearSoundPtrs: a handle (or none) and a junk table
  for (let v = 0; v < 10; v++) {
    F['c3dl_sndfree_' + v] = function () {
      const out = {};
      out.implog = soundImports({});
      soundTable(v % 3 === 0 ? 0 : (0x7e000000 + v), 0x76000000 + v * 0x01010101);
      ptr(0x0083afc4).writeU32(0x12340000 + v);
      __freeze.save(ptr(0x0083afc4), 4);
      return out;
    };
  }
  // callSoundHook74: the entry 0x0083af74 is a native cdecl callback (a, b) that counts calls in hookrec+0 and returns
  // a * 3 + b; the handle is zero for the "off" key
  function hook74(on) {
    const out = {};
    const rec = keep(Memory.alloc(16));
    rec.writeByteArray(new Array(16).fill(0));
    const cb = keep(new NativeCallback(function (a, b) {
      rec.writeU32(rec.readU32() + 1);
      rec.add(4).writeS32(a);
      rec.add(8).writeS32(b);
      return (Math.imul(a, 3) + b) | 0;
    }, 'int', ['int', 'int']));
    standInGuard();
    __freeze.save(ptr(0x0083afc0), 4);
    __freeze.save(ptr(0x0083af74), 4);
    ptr(0x0083afc0).writeU32(on ? 0x7e000001 : 0);
    ptr(0x0083af74).writePointer(cb);
    out.hookrec = rec;
    return out;
  }
  F.c3dl_hook74_on = function () { return hook74(true); };
  F.c3dl_hook74_off = function () { return hook74(false); };

  // ====================================================================================================================
  // BinkPlayer::frame 0x00487180: players (0x10 bytes: +4 the Bink structure, +8 the buffer structure). A Bink structure has
  // the frame number at +8 and the frame count at +0xc (equal: the last frame); the buffer structure's +4 / +0x10 / +0x14 /
  // +0x18 are the words passed to _BinkCopyToBuffer. _BinkBufferLock returns the buffer pointer's low bit (0 = no lock).
  // ====================================================================================================================
  const BINKF = [[1, 5, 5], [0, 5, 5], [1, 3, 9], [0, 3, 9], [1, 0, 0], [0, 7, 8], [1, 0x7fffffff, 0x7fffffff], [0, 0xffffffff, 1],
                 [1, 2, 2], [0, 4, 6], [1, 10, 11], [0, 8, 8]];
  F.c3dl_binkframe = function () {
    const out = {};
    const blk = keep(Memory.alloc(0x10 * BINKF.length));
    const bks = keep(Memory.alloc(0x40 * BINKF.length));
    const bfs = keep(Memory.alloc(0x40 * BINKF.length));
    BINKF.forEach(function (r, i) {
      const p = blk.add(0x10 * i), bk = bks.add(0x40 * i), bf = bfs.add(0x40 * i);
      junk(p, 0x10, 0x6b000000 + i * 0x100);
      junk(bk, 0x40, 0x6c000000 + i * 0x100);
      junk(bf, 0x40, 0x6d000000 + i * 0x100);
      bk.add(8).writeU32(r[2] >>> 0);
      bk.add(0xc).writeU32(r[1] >>> 0);
      bf.add(0x14).writeU32(0x300 + i);
      bf.add(0x18).writeU32(0x400 + i);
      bf.add(4).writeU32(0x500 + i);
      bf.add(0x10).writeU32(0x600 + i);
      p.add(4).writePointer(bk);
      p.add(8).writePointer(bf.add(r[0]));       // an odd buffer pointer: the lock stub reports success for it
      out['p' + i] = p;
    });
    out.pblk = blk;
    out.bks = bks;
    out.bfs = bfs;
    out.implog = importOn({ module: 'golf_clean.exe', imports: {
      'binkw32.dll!_BinkDoFrame@4': { args: 1, ret: 0 },
      'binkw32.dll!_BinkBufferLock@4': { args: 1, impl: function (a) { return a[0].toUInt32() & 1; } },
      'binkw32.dll!_BinkCopyToBuffer@28': { args: 7, ret: 0 },
      'binkw32.dll!_BinkBufferUnlock@4': { args: 1, ret: 0 },
      'binkw32.dll!_BinkGetRects@8': { args: 2, impl: function (a) { return (a[1].toUInt32() + 0x77) >>> 0; } },
      'binkw32.dll!_BinkBufferBlit@12': { args: 3, ret: 0x1234 },
      'binkw32.dll!_BinkNextFrame@4': { args: 1, ret: 0x5678 } } }).log;
    return out;
  };

  // ====================================================================================================================
  // setNewHeap 0x004823c0: S0Heap objects (0x20 bytes: +4 flag byte, +8 block, +0xc bump pointer, +0x10 capacity, +0x14 free
  // count) as c3dd builds them, under the heap stand-in with realloc; the global 0x00839650 is saved and set to junk, so
  // the store of the heap (or of 0) shows. The out-of-memory arm (MessageBoxA, exit) is not run: heapAlloc returns NULL only
  // for a full heap with the flag set, and that case is left out of the vectors.
  // Row: {flags, cap, free, noData}
  // ====================================================================================================================
  const NH = [
    { cap: 0x400, free: 0x400 }, { cap: 0x400, free: 0x10 }, { cap: 0x400, free: 0x10 }, { flags: 1, cap: 0x400, free: 0x400 },
    { cap: 0, free: 0, noData: true }, { cap: 0x400, free: 0x300 }, { flags: 0xfe, cap: 0x400, free: 0 },
    { cap: 0x800, free: 0x40 }, { flags: 3, cap: 0x400, free: 0x400 }, { cap: 0x400, free: -0x20 },
  ];
  function newHeapKey(failMask) {
    const E = envD();
    const out = E.fx;
    const spec = { module: 'golf_clean.exe', alloc: HEAP_ALLOC, free: HEAP_FREE, realloc: [0xa6980], arenaSize: 0x8000,
                   exhaust: 'null', failMask: failMask || 0 };
    heapPreRevert(null);
    const h = heapStandIn2(spec);
    out.arena = h.arena;
    out.freelog = h.log;
    NH.forEach(function (o, i) {
      const hp = E.take(0x20, 0x48480000 + i);
      const span = o.cap + 0x800;
      const data = E.take(span, 0x44440000 + i);
      hp.add(4).writeU8(o.flags || 0);
      if (o.noData) {
        hp.add(8).writePointer(ptr(0));
        hp.add(0xc).writePointer(data);
      } else {
        hp.add(8).writePointer(data);
        h.register(data, o.cap);
        hp.add(0xc).writePointer(data.add(o.cap > o.free ? o.cap - o.free : 0));
      }
      hp.add(0x10).writeS32(o.cap);
      hp.add(0x14).writeS32(o.free);
      out['h' + i] = hp;
    });
    out.imp = importOn({ module: 'golf_clean.exe', imports: { 'USER32.dll!MessageBoxA': { args: 4, ret: 1 } } }).log;
    standInGuard();
    __freeze.save(ptr(0x00839650), 4);
    ptr(0x00839650).writeU32(0x1234abcd);
    return out;
  }
  F.c3dl_newheap = function () { return newHeapKey(0); };
  F.c3dl_newheap_f0 = function () { return newHeapKey(1); };

  // ====================================================================================================================
  // Tooltip timers. Tooltip::show 0x00480220 draws through the live display: for the length of a key it is replaced on the
  // A/B thread by a stub that records (this, text, the four rect words) in `$tiplog` (+0 count, 0x20-byte entries from
  // +4) and returns 0x7a7a0000 + its call number, so the arguments and the result of every call are compared state. The
  // Timer family runs under the import stand-in and the fake display (timerEnv); View::toParent and Window::visible run as
  // the originals on fixture windows (fields +0x9c flags, +0xa0 visible byte, +0x130 parent, +0x1ac origin, +0x1bc scroll).
  // The tooltip globals 0x0083aac8 / 0x0083aad8 / 0x004e42c4 (written by showTip) and 0x0083ad44 (read) are saved under the
  // freeze; 0x0083ad44 takes a pattern.
  // ====================================================================================================================
  // the tooltip recorder: `$tiplog` (+0 count, 0x20-byte entries from +4: tag, two pointers, the four words of a rect) and
  // out.tiprec(tag, a0, a1, rectPtr) -> the call number
  function tipLog(out) {
    const log = keep(Memory.alloc(0x210));
    log.writeByteArray(new Array(0x210).fill(0));
    out.tiplog = log;
    out.tiprec = function (tag, a0, a1, rectp) {
      const n = log.readU32();
      if (n < 16) {
        const o = log.add(4 + 0x20 * n);
        o.writeU32(tag);
        o.add(4).writePointer(a0);
        o.add(8).writePointer(a1);
        for (let k = 0; k < 4; k++) o.add(0xc + 4 * k).writeS32(rectp.add(4 * k).readS32());
        log.writeU32(n + 1);
      }
      return n;
    };
    return log;
  }
  function tipsKey() {
    const out = {};
    const E = envD();
    // fixture windows
    const WROWS = [   // [flags, visible byte, parent index or -1, origin x, y, scroll x, y]
      [0x00, 1, -1, 10, 20, 1, 2], [0x20, 1, 0, 5, 6, 0, 0], [0x8020, 1, 0, -7, 9, 3, -4], [0x20, 1, -1, 100, 200, 0, 0],
      [0x00, 0, -1, -30, -40, 5, 6], [0x20, 1, 1, 1, 2, 3, 4], [0x20, 0, 0, 7, 8, 9, 10], [0x20, 1, 6, 0, 0, 0, 0],
    ];
    const wblk = E.take(0x640 * WROWS.length, 0x61000000);
    function win(i) { return wblk.add(0x640 * i); }
    WROWS.forEach(function (r, i) {
      const w = win(i);
      w.add(0x9c).writeU32(r[0]);
      w.add(0xa0).writeU8(r[1]);
      w.add(0x130).writePointer(r[2] < 0 ? ptr(0) : win(r[2]));
      words(w.add(0x1ac), [r[3] >>> 0, r[4] >>> 0]);
      words(w.add(0x1bc), [r[5] >>> 0, r[6] >>> 0]);
      out['w' + i] = w;
    });
    // entry arrays: entries (vtable, rect +4 .. +0x10, keys, text +0x1c)
    const texts = [];
    for (let i = 0; i < 24; i++) texts.push(cstr('tip text ' + i));
    const pl = arrayPool(0x4000);
    out.tapool = pl.base;
    function tipArray(cap, tag, textMask) {
      const need = (4 + 0x20 * Math.max(cap, 1) + 0x20 + 15) & ~15;
      const blk = pl.base.add(pl.cur);
      pl.cur += need;
      junk(blk, need, 0x5b000000 + tag);
      blk.writeU32(cap);
      const base = blk.add(4);
      for (let i = 0; i < cap; i++) {
        const e = base.add(0x20 * i);
        e.writeU32(0x004bba70);
        words(e.add(4), [(tag + 10 * i) >>> 0, (tag * 2 + 20 * i) >>> 0, (tag + 10 * i + 50) >>> 0, (tag * 2 + 20 * i + 15) >>> 0]);
        e.add(0x14).writeU32(i);
        e.add(0x18).writeU32(i + 1);
        e.add(0x1c).writePointer(((textMask >> i) & 1) ? texts[(tag + i) % 24] : ptr(0));
      }
      return base;
    }
    // twelve HotLists (0x80 apart): [owner index or -1, +0x10, +0x18, +0x1c, capacity, text mask, Timer row]
    const LROWS = [
      [0, 0, 5, 6, 3, 7, 0], [1, 0x77, 7, 8, 4, 5, 1], [2, 0, 9, 10, 2, 3, 2], [3, 0x1234, 1, 2, 3, 6, 3],
      [-1, 0, 3, 4, 2, 3, 4], [4, 0, 11, 12, 3, 0, 5], [5, 0x99, 13, 14, 5, 31, 6], [0, 0, 15, 16, 1, 1, 7],
      [6, 0, 17, 18, 2, 3, 8], [7, 0x55, 19, 20, 4, 15, 9], [1, 0, 21, 22, 3, 2, 10], [-1, 0xabc, 23, 24, 1, 1, 11],
    ];
    const hblk = E.take(0x80 * LROWS.length, 0x62000000);
    LROWS.forEach(function (r, i) {
      const o = hblk.add(0x80 * i);
      const arr = tipArray(r[4], i * 0x100, r[5]);
      placeHL(o, { arr: arr, cap: r[4], count: r[4], t: r[6] });
      o.add(0xc).writePointer(r[0] < 0 ? ptr(0) : win(r[0]));
      o.add(0x10).writeU32(r[1] >>> 0);
      o.add(0x18).writeU32(r[2]);
      o.add(0x1c).writeU32(r[3]);
      out['h' + i] = o;
    });
    // eight Buttons (0x640 bytes): vtable with slot +0x78 a recording stub (thiscall, two arguments), Timers at +0x57c / +0x5b4,
    // intervals +0x5ac / +0x5b0, text +0x5f8, rect +0x1ac .. +0x1b8, visibility +0xa0, parent +0x130
    const bvt = E.vt([[0x78, 0xB778, 2]]);
    const BROWS = [   // [visible, text?, parent window, interval 5ac, interval 5b0, rect, Timer row]
      [1, 1, -1, 50, 0, [10, 20, 60, 80], 0], [1, 1, 0, 60, 7, [5, 6, 105, 106], 1], [0, 1, -1, 70, 0, [0, 0, 1, 1], 2],
      [1, 0, -1, 80, 9, [1, 2, 3, 4], 3], [1, 1, 5, 90, -3, [100, 200, 300, 400], 4], [1, 1, 2, 0, 0, [-5, -6, 5, 6], 5],
      [1, 1, 1, 1000, 13, [7, 8, 9, 10], 6], [1, 0, 4, 31, 777, [0, 0, 0, 0], 7], [0, 0, 3, 10, 10, [1, 1, 1, 1], 8],
      [1, 1, 3, 10, 0x80000000, [2, 4, 6, 8], 9], [1, 1, 7, 5, 5, [9, 9, 9, 9], 10], [1, 1, -1, 0x7fffffff, 1, [3, 3, 4, 4], 11],
    ];
    const bblk = E.take(0x640 * BROWS.length, 0x63000000);
    BROWS.forEach(function (r, i) {
      const b = bblk.add(0x640 * i);
      b.writePointer(bvt);
      b.add(0xa0).writeU8(r[0]);
      b.add(0x9c).writeU32(0);
      b.add(0x130).writePointer(r[2] < 0 ? ptr(0) : win(r[2]));
      writeTimer(b.add(0x57c), TROWS[r[6] % TROWS.length]);
      writeTimer(b.add(0x5b4), TROWS[(r[6] + 3) % TROWS.length]);
      b.add(0x5ac).writeU32(r[3] >>> 0);
      b.add(0x5b0).writeU32(r[4] >>> 0);
      b.add(0x5f8).writePointer(r[1] ? texts[i % 24] : ptr(0));
      words(b.add(0x1ac), r[5].map(function (x) { return x >>> 0; }));
      out['b' + i] = b;
    });
    out.wblk = wblk;
    out.hblk = hblk;
    out.bblk = bblk;
    tipLog(out);
    crtStand(0x00480220, 'int', ['pointer', 'pointer', 'pointer'], function (self, text, rect) {
      return 0x7a7a0000 + out.tiprec(0x50, self, text, rect);
    }, 'thiscall');
    timerEnv(out, 'fake', null, E);
    out.tblk = E.fx.blk;
    out.rec = E.fx.rec;
    for (const g of [0x0083aac8, 0x0083aad8, 0x004e42c4, 0x0083ad44]) __freeze.save(ptr(g), 4);
    ptr(0x0083aac8).writeU32(0x11111111);
    ptr(0x0083aad8).writeU32(0x22222222);
    ptr(0x004e42c4).writeU32(0x33333333);
    ptr(0x0083ad44).writeU32(0x4a4a4a4a);
    return out;
  }
  F.c3dl_tips = tipsKey;

  // ====================================================================================================================
  // Tooltip::show 0x00480220 / Tooltip::hide 0x00480360. The two drawing callees (drawTooltip 0x0047cdb0, cdecl one argument;
  // redrawDrawList 0x0047f8e0, cdecl two arguments) are replaced on the A/B thread by stubs that record their arguments and
  // the four words of the rect they receive in `$tiplog`; drawTooltip returns 0x7b7b0000 + its call number. The font object
  // (global 0x0083aac8, saved) is {+4 a measure object whose vtable slot +0x10 is a recording machine stub returning the
  // recorder cursor xor 0x40 (so every call of one vector returns a different, deterministic width), +8 / +0xc / +0x10 the
  // line-height words}. The display (0x0083ad50, saved) is a fake whose slot +0xa8 returns the dword at display+0x14 (the
  // screen width), or NULL. Texts, `this` words and rects live in $blk. The rect globals 0x0083a2c8 .. 0x0083a2d7 and the
  // tooltip globals 0x0083aac0 / aac4 are saved and preset.
  // ====================================================================================================================
  const FONTS = [[3, 5, 14], [-1, 12, 99], [0, 0, 0], [5, 5, 5], [0x7fffffff, 1, 2], [-5, -6, 7]];
  function showEnv(o) {
    const out = {};
    const E = envD();
    tipLog(out);
    crtStand(0x0047cdb0, 'int', ['pointer'], function (rectp) { return 0x7b7b0000 + out.tiprec(0xD0, rectp, ptr(0), rectp); });
    crtStand(0x0047f8e0, 'void', ['pointer', 'int'], function (rectp, a) { out.tiprec(0xD1, rectp, ptr(a), rectp); });
    const fr = FONTS[o.font];
    const font = E.take(0x20, 0x46000000);
    const sub = E.take(0x20, 0x47000000);
    sub.writePointer(E.vt([[0x10, 0x40, 2]]));
    font.add(4).writePointer(sub);
    font.add(8).writeU32(fr[0] >>> 0);
    font.add(0xc).writeU32(fr[1] >>> 0);
    font.add(0x10).writeU32(fr[2] >>> 0);
    const disp = E.take(0x20, 0x48000000);
    disp.writePointer(E.vt([[0xa8, 0xA8A8, 0, 0x14]]));
    disp.add(0x14).writeU32(o.width >>> 0);
    // texts, owners and rects
    const T = ['alpha', 'alpha^beta', 'a^b^c^d', '', '^', 'xyz^', 'a long line of text^second line^third', 'q'];
    T.forEach(function (t, i) {
      const p = E.take(0x40, 0x49000000 + i);
      p.writeUtf8String(t);
      out['t' + i] = p;
    });
    for (let i = 0; i < 6; i++) out['w' + i] = E.take(0x20, 0x4a000000 + i);
    const R = [[100, 200, 140, 220], [10, 5, 30, 25], [700, 300, 780, 320], [0, 0, 0, 0], [-50, 10, -10, 30], [400, 400, 400, 400],
               [300, 100, 500, 130], [5, 300, 790, 340]];
    R.forEach(function (r, i) {
      const p = E.take(0x10, 0x4b000000 + i);
      words(p, r.map(function (x) { return x >>> 0; }));
      out['r' + i] = p;
    });
    standInGuard();
    __freeze.save(ptr(0x0083ad50), 4);
    ptr(0x0083ad50).writePointer(o.display === false ? ptr(0) : disp);
    __freeze.save(ptr(0x0083aac8), 4);
    ptr(0x0083aac8).writePointer(font);
    __freeze.save(ptr(0x0083aac0), 8);
    ptr(0x0083aac0).writeU32(0x5a5a5a5a);
    ptr(0x0083aac4).writeU32(o.aac4 >>> 0);
    __freeze.save(ptr(0x0083a2c8), 0x10);
    words(ptr(0x0083a2c8), [0x1001, 0x1002, 0x1003, 0x1004]);
    out.tblk = E.fx.blk;
    out.rec = E.fx.rec;
    return out;
  }
  const SHOWKEYS = { a: { font: 0, width: 800, aac4: 0 }, b: { font: 0, width: 30, aac4: 0x1234 }, c: { font: 1, width: 0, aac4: 0, display: false },
                     d: { font: 2, width: 200, aac4: 7 }, e: { font: 3, width: 640, aac4: 0 }, f: { font: 5, width: 100, aac4: 0xffffffff } };
  Object.keys(SHOWKEYS).forEach(function (k) { F['c3dl_tipshow_' + k] = function () { return showEnv(SHOWKEYS[k]); }; });
  // hide: no fonts needed; the visible-tip global takes six values
  [0, 1, 0x12345678, 0xffffffff, 0x80000000, 7].forEach(function (v, i) {
    F['c3dl_tiphide_' + i] = function () {
      const out = {};
      tipLog(out);
      crtStand(0x0047cdb0, 'int', ['pointer'], function (rectp) { return 0x7b7b0000 + out.tiprec(0xD0, rectp, ptr(0), rectp); });
      crtStand(0x0047f8e0, 'void', ['pointer', 'int'], function (rectp, a) { out.tiprec(0xD1, rectp, ptr(a), rectp); });
      standInGuard();
      __freeze.save(ptr(0x0083aac0), 8);
      ptr(0x0083aac0).writeU32(0x5a5a0000 + i);
      ptr(0x0083aac4).writeU32(v >>> 0);
      __freeze.save(ptr(0x0083a2c8), 0x10);
      words(ptr(0x0083a2c8), [0x2001 + i, 0x2002 + i, 0x2003 + i, 0x2004 + i]);
      return out;
    };
  });

  // ====================================================================================================================
  // Button::beginPress 0x00488ab0 / Button::onMouseMove 0x00488cf0. Twelve Buttons (0x640 bytes: vtable with slots +0x78 (two
  // arguments) and +0x120 as recording machine stubs; +0x130 a parent window; +0x278 a rect provider whose vtable slot +0xd0
  // is a machine stub that copies the four words at provider+0x20 into the rect it is handed; +0x5ac interval; +0x5b4 and
  // the Timer; +0x5e8 id; +0x614 pressed state; +0x620 / +0x624 flags; +0x62c a cdecl callback stub or NULL). Parents (0x100
  // bytes: vtable slots +0xc8 (returns the dword at parent+0x40), +0xcc, +0xd0 as one-argument stubs; +0x74 / +0x78 / +0x7c
  // objects with a +0x1c stub, or NULL) are shared. The live globals the handlers touch (0x0083ab2c, ab30, ab34, ab40, ab44,
  // ab48 and the tooltip-visible global 0x0083aac4) are saved and set per key; timeGetTime, GetAsyncKeyState and
  // GetSystemMetrics are the import stand-in's.
  // ====================================================================================================================
  const RECT_STUB = [0x8B, 0x44, 0x24, 0x04, 0x8B, 0x51, 0x20, 0x89, 0x10, 0x8B, 0x51, 0x24, 0x89, 0x50, 0x04, 0x8B, 0x51, 0x28,
                     0x89, 0x50, 0x08, 0x8B, 0x51, 0x2C, 0x89, 0x50, 0x0C, 0x33, 0xC0, 0xC2, 0x04, 0x00];
  const BT_PARENTS = [[1, 1, 1, 1], [0, 1, 1, 0], [0, 0, 0, 5], [1, 0, 0, 0xffffffff], [1, 1, 1, 0], [1, 0, 1, 2]];   // [7c, 74, 78 present, c8 result]
  const BT_RECTS = [[10, 10, 60, 40], [0, 0, 100, 20], [-5, -5, 5, 5], [100, 200, 101, 201]];
  const BT_ROWS = [   // [pressed +0x614, +0x620, +0x624, interval +0x5ac, callback, parent, rect provider, id]
    [0, 0, 0, 50, 0, 0, 0, 1], [0, 1, 0, -1, 1, 1, 0, 2], [1, 0, 0, 60, 1, 0, 1, 3], [1, 1, 0, 70, 0, 1, 1, 4],
    [1, 0, 1, 80, 1, 2, 2, 5], [1, 1, 0, -1, 0, 3, 2, 6], [2, 0, 0, 90, 1, 4, 3, 7], [0, 0, 1, 100, 0, 5, 3, 8],
    [1, 0, 0, 110, 1, 2, 0, 9], [1, 1, 0, 120, 0, 3, 1, 10], [0, 1, 1, 130, 1, 4, 2, 11], [1, 0, 0, 140, 0, 5, 3, 12],
  ];
  function btnEnv(sc) {
    const out = {};
    const E = envD();
    const parents = BT_PARENTS.map(function (pp, i) {
      const p = E.take(0x100, 0x64000000 + i * 0x100);
      p.writePointer(E.vt([[0xc8, 0xC800 + i, 1, 0x40], [0xcc, 0xCC00 + i, 1], [0xd0, 0xD000 + i, 1]]));
      p.add(0x40).writeU32(pp[3] >>> 0);
      [[0x7c, 0], [0x74, 1], [0x78, 2]].forEach(function (sl) {
        if (pp[sl[1]]) {
          const o = E.take(0x10, 0x65000000 + i * 0x10 + sl[1]);
          o.writePointer(E.vt([[0x1c, 0x1C00 + 0x10 * i + sl[1], 0]]));
          p.add(sl[0]).writePointer(o);
        } else {
          p.add(sl[0]).writePointer(ptr(0));
        }
      });
      return p;
    });
    const rvt = E.vt([]);
    rvt.add(0xd0).writePointer(E.emit(RECT_STUB));
    const providers = BT_RECTS.map(function (r, i) {
      const q = E.take(0x40, 0x66000000 + i);
      q.writePointer(rvt);
      words(q.add(0x20), r.map(function (x) { return x >>> 0; }));
      return q;
    });
    const bvt = E.vt([[0x78, 0xB778, 2], [0x120, 0xB120, 0]]);
    const cbStub = E.stubC(0xCB00, 1);
    const blk = E.take(0x640 * BT_ROWS.length, 0x67000000);
    BT_ROWS.forEach(function (r, i) {
      const b = blk.add(0x640 * i);
      b.writePointer(bvt);
      b.add(0x130).writePointer(parents[r[5]]);
      b.add(0x278).writePointer(providers[r[6]]);
      b.add(0x5ac).writeU32(r[3] >>> 0);
      writeTimer(b.add(0x5b4), TROWS[i % TROWS.length]);
      b.add(0x5e8).writeU32(r[7]);
      b.add(0x614).writeU32(r[0]);
      b.add(0x620).writeU32(r[1]);
      b.add(0x624).writeU32(r[2]);
      b.add(0x62c).writePointer(r[4] ? cbStub : ptr(0));
      out['b' + i] = b;
    });
    out.bblk = blk;
    const imports = {
      'WINMM.dll!timeGetTime': function (a, ctx) { return (1000 * (ctx.call + 1) + 7) >>> 0; },
      'USER32.dll!GetAsyncKeyState': { args: 1, ret: sc.async },
      'USER32.dll!GetSystemMetrics': { args: 1, ret: sc.swap },
    };
    timerEnv(out, 'fake', imports, E);
    out.tblk = E.fx.blk;
    out.rec = E.fx.rec;
    const G6 = [0x0083ab2c, 0x0083ab30, 0x0083ab34, 0x0083ab40, 0x0083ab44, 0x0083ab48];
    G6.forEach(function (g) { __freeze.save(ptr(g), 4); });
    __freeze.save(ptr(0x0083aac4), 4);
    ptr(0x0083aac4).writeU32(0);
    ptr(0x0083ab2c).writeU32(0x2c2c2c2c);
    ptr(0x0083ab30).writeU32(sc.cap30 >>> 0);
    ptr(0x0083ab34).writeU32(0x34343434);
    ptr(0x0083ab40).writeU32(sc.cap40 === undefined ? 0 : (typeof sc.cap40 === 'string' ? out[sc.cap40].toUInt32() : sc.cap40));
    ptr(0x0083ab44).writeU32(sc.cap44 === undefined ? 0 : (typeof sc.cap44 === 'string' ? out[sc.cap44].toUInt32() : sc.cap44));
    ptr(0x0083ab48).writeU32(sc.down === undefined ? 0 : sc.down);
    return out;
  }
  F.c3dl_btn_bp_a = function () { return btnEnv({ async: 0, swap: 0, cap30: 0 }); };
  F.c3dl_btn_bp_b = function () { return btnEnv({ async: 0, swap: 0, cap30: 0x1234 }); };
  const MMS = [   // [async, swap, cap40, cap44, down]
    [0x8000, 0, 'b2', 0, 1], [0x8000, 1, 0, 'b3', 1], [0, 0, 'b0', 0, 1], [0x8000, 0, 'b11', 0, 0], [0, 1, 'b6', 0, 1],
    [0x8000, 0, 'b9', 0, 1], [0, 0, 'b8', 0, 1], [0x8000, 1, 'b5', 0, 1], [0x8000, 0, 'b1', 0, 1], [0, 1, 0, 'b7', 1],
  ];
  MMS.forEach(function (m, i) {
    F['c3dl_btn_mm_' + i] = function () { return btnEnv({ async: m[0], swap: m[1], cap40: m[2], cap44: m[3], down: m[4], cap30: 0 }); };
  });

  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
