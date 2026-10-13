// Fixtures for batch c3eu (round 32, 2026-10-13, golf_clean.exe): rows earlier rounds dropped for the heap, an import, file
// I/O or a constant result. Every key combines the shared helpers:
//   - re/frida/js/freeze.js        (the game's first thread is frozen; every live global a key writes is saved first and
//     restored before the resume),
//   - re/frida/js/importstandin.js (the exe's IAT slots point at recording stubs on the A/B thread; the call log is state),
//   - re/frida/js/heapstandin.js   (free 0x004a5007 / operator delete 0x004a4ffc only record; the log is state),
//   - calleeStand (c3ea's helper, copied): a C2 callee replaced on the A/B thread by a recorder whose log is state,
//   - and, for the window-tree rows, c3ef's / c3eg's own fixtures (re/frida/js/fixtures.d/c3ef.js, c3eg.js), wrapped.
// Only our own synthetic values: no game text, no live game object handed to a body.
//
// Reimplementations: shim/src/re/c3eu.cpp. Registry: re/frida/registry.d/c3eu.py. Purposes: log/c3/c3eu_purpose.md.
(function () {
  const F = {};
  const keep = function (x) { globalThis.__diffKeepAlive.push(x); return x; };
  function junk(p, n, tag) { for (let k = 0; k < n; k += 4) p.add(k).writeU32((tag + k) >>> 0); }
  function zero(p, n) { for (let k = 0; k < n; k += 4) p.add(k).writeU32(0); }
  function u32b(v) { v >>>= 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; }
  function blk(n, tag) { const p = keep(Memory.alloc(n)); if (tag === undefined) zero(p, n); else junk(p, n, tag); return p; }

  // ---- the first thread is frozen once per key, before any stand-in is installed (c3dl's standInGuard)
  let frozenNow = false;
  function freezeNow() {
    if (frozenNow) return;
    __freeze.thread();
    frozenNow = true;
    __freeze.atFlush(function () { frozenNow = false; });
    __freeze.negOn();             // the negative-control build's exported switch (no effect on the normal build)
  }
  function importOn(spec) { freezeNow(); return __importStandIn(spec); }
  function heapOn(out) {
    freezeNow();
    const h = __heapStandIn({ module: 'golf_clean.exe', free: [0xa5007, 0xa4ffc], arenaSize: 0x100, exhaust: 'null' });
    out.arena = h.arena;
    out.freelog = h.log;
    return h;
  }
  function saveSet(va, v) { __freeze.save(ptr(va), 4); ptr(va).writeU32(v >>> 0); }

  // ---- recording stubs (machine code, c3dl's design): each call appends {tag, ecx, stack args...} to rec (rec+0 byte
  // cursor, entries from rec+0x10) and returns cursor ^ tag; thiscall stubs pop their arguments.
  function stubBytes(rec, tag, n, thiscall) {
    const r = u32b(rec.toUInt32());
    const b = [];
    b.push(0xA1, ...r);                                              // mov eax, [REC]
    b.push(0x05, ...r);                                              // add eax, REC
    b.push(0xC7, 0x00, ...u32b(tag));                                // mov dword [eax], tag
    if (thiscall) b.push(0x89, 0x48, 0x04);                          // mov [eax+4], ecx
    else b.push(0xC7, 0x40, 0x04, ...u32b(0xC0C0C0C0));
    for (let i = 1; i <= n; i++) b.push(0x8B, 0x54, 0x24, 4 * i, 0x89, 0x50, 4 + 4 * i);
    b.push(0x81, 0x05, ...r, ...u32b(4 * (n + 2)));                  // add dword [REC], 4(n+2)
    b.push(0xA1, ...r);                                              // mov eax, [REC]
    b.push(0x35, ...u32b(tag));                                      // xor eax, tag
    if (thiscall && n > 0) b.push(0xC2, 4 * n, 0x00); else b.push(0xC3);
    return b;
  }
  function recEnv() {
    const size = Process.pageSize * 2;
    const base = keep(Memory.alloc(size));
    Memory.protect(base, size, 'rwx');
    let off = 0;
    function emit(bytes) {
      const p = base.add(off);
      p.writeByteArray(bytes);
      off += (bytes.length + 15) & ~15;
      if (off > size) throw new Error('c3eu: code arena full');
      return p;
    }
    const rec = blk(0x400);
    rec.writeU32(0x10);
    return { rec: rec, stub: function (tag, n) { return emit(stubBytes(rec, tag, n, true)); } };
  }

  // ---- calleeStand (c3ea's): log word 0 = cursor, words 1..16 = call count of stub k, word 17 = FNV-1a hash over every
  // call, entries {va, count, values...} from +0x50 while they fit. ret: number | array (by call number) | fn(n, args).
  const standLive = {};
  let standTok = 1;
  const CLOG = 0x400;
  function newClog() { const l = blk(CLOG); l.writeU32(0x50); l.add(0x44).writeU32(0x811c9dc5); return l; }
  function fnvWord(h, w) {
    for (let k = 0; k < 4; k++) { h ^= (w >>> (8 * k)) & 0xff; h = Math.imul(h, 0x01000193) >>> 0; }
    return h >>> 0;
  }
  // nargs (optional, ours): how many of the argc words a call actually passes (a variadic callee), given the words; only
  // those are logged and hashed, so words beyond the real arguments (the caller's frame) never reach the log.
  function calleeStand(fx, k, va, argc, abi, ret, nargs) {
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
      let a = Array.prototype.slice.call(arguments).map(function (v) { return v >>> 0; });
      if (nargs) a = a.slice(0, nargs(a));
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
      let r;
      if (typeof ret === 'function') r = ret(n, a);
      else if (Array.isArray(ret)) r = ret[Math.min(n, ret.length - 1)];
      else r = ret === undefined ? 0 : ret;
      return r >>> 0;
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
  // Root-window input handlers. The globals they write are the live mouse / modifier words, written by the game's first
  // thread on every input event: that thread is frozen and every word is saved and pre-filled with our own pattern.
  // ====================================================================================================================
  function rootGlobals() {
    freezeNow();
    __freeze.save(ptr(0x00822d58), 0x24);                // 0x00822d58 .. 0x00822d7b (press words)
    junk(ptr(0x00822d58), 0x24, 0x7e570000);
    __freeze.save(ptr(0x00822d8c), 8);                   // 0x00822d8c / 0x00822d90 (point words)
    ptr(0x00822d8c).writeU32(0x7e5a0001); ptr(0x00822d90).writeU32(0x7e5a0002);
  }
  F.c3eu_root_pt = function () { rootGlobals(); return {}; };

  // onPoint / onUnpoint: the deadline 0x005aa774 and the byte 0x0083ab9c, timeGetTime answering T
  const T_NOW = 0x12345678;
  const DEADLINES = { d0: 0, lt: T_NOW - 1, eq: T_NOW, gt: T_NOW + 0x100, one: 1, hi: 0xffffffff };
  Object.keys(DEADLINES).forEach(function (v) {
    F['c3eu_root_tick_' + v] = function () {
      rootGlobals();
      __freeze.save(ptr(0x005aa774), 4);
      ptr(0x005aa774).writeU32(DEADLINES[v] >>> 0);
      __freeze.save(ptr(0x0083ab9c), 1);
      ptr(0x0083ab9c).writeU8(0x5a);
      const s = importOn({ module: 'golf_clean.exe', imports: { 'WINMM.dll!timeGetTime': { args: 0, ret: T_NOW } } });
      return { implog: s.log };
    };
  });

  // onPress1 / onPress2: the scale word 0x00822c8c that scaleX 0x00404970 reads (scaleX(2) = 1, 3, 6, 13 for these), and
  // GetAsyncKeyState answering per virtual key (control 0x11, shift 0x10; any other key 0).
  const PRESS = [[160, 0x8000, 0], [400, 0, 0x8001], [1000, 0xffff8000, 0x00010100], [2000, 0x1234, 0xabcd],
                 [160, 0x0100, 0x7f00]];
  PRESS.forEach(function (row, i) {
    F['c3eu_root_press_' + i] = function () {
      rootGlobals();
      saveSet(0x00822c8c, row[0]);
      const s = importOn({ module: 'golf_clean.exe', imports: { 'USER32.dll!GetAsyncKeyState': { args: 1, impl: function (a) {
        const vk = a[0].toUInt32();
        return (vk === 0x11 ? row[1] : vk === 0x10 ? row[2] : 0) >>> 0;
      } } } });
      return { implog: s.log };
    };
  });

  // ====================================================================================================================
  // Junk-filled objects (twelve, 0x40 or 0x200 bytes) for the vtable store and the NodeList constructor
  // ====================================================================================================================
  function junkObjects(size, tag) {
    const out = {};
    const b = blk(size * 12);
    for (let i = 0; i < 12; i++) {
      junk(b.add(size * i), size, (tag + i * 0x01010101) >>> 0);
      out['o' + i] = b.add(size * i);
    }
    out.oblk = b;
    freezeNow();                  // not needed by the bodies (no global is touched); it arms the negative-control switch
    return out;
  }
  F.c3eu_junk40 = function () { return junkObjects(0x40, 0x5d000000); };
  F.c3eu_junk200 = function () { return junkObjects(0x200, 0x4e000000); };

  // ====================================================================================================================
  // BinkPlayer::scalarDtor 0x00487020: c3dl_bink's rows (+4 Bink handle, +8 buffer handle) with the recording deallocator.
  // _BinkClose@4 returns 1, _BinkBufferClose@4 returns handle * 3 + 1.
  // ====================================================================================================================
  const BINK_ROWS = [[0, 0], [0x7d01, 0], [0, 0x7d02], [0x7d03, 0x7d03], [0xffffffff, 0], [0, 0xffffffff], [0x7d06, 0x7d07],
                     [1, 1], [0x80000000, 0x80000000], [0x7d09, 0], [0, 0], [0x7d0b, 0x7d0c]];
  F.c3eu_bink = function () {
    const out = {};
    const b = blk(0x10 * BINK_ROWS.length);
    BINK_ROWS.forEach(function (r, i) {
      const o = b.add(0x10 * i);
      junk(o, 0x10, 0x6d000000 + i * 0x100);
      o.add(4).writeU32(r[0] >>> 0);
      o.add(8).writeU32(r[1] >>> 0);
      out['b' + i] = o;
    });
    out.bblk = b;
    heapOn(out);
    const s = importOn({ module: 'golf_clean.exe', imports: {
      'binkw32.dll!_BinkClose@4': { args: 1, ret: 1 },
      'binkw32.dll!_BinkBufferClose@4': { args: 1, impl: function (a) { return (Math.imul(a[0].toUInt32(), 3) + 1) >>> 0; } } } });
    out.implog = s.log;
    return out;
  };

  // ====================================================================================================================
  // Cursor_deleteDtor 0x0045c000: c3dl_cursor's rows (+4 cursor handle, +8 a sub-object whose slot 0 is a recording stub)
  // with the recording deallocator. DestroyCursor answers handle ^ 5.
  // ====================================================================================================================
  const CUR_ROWS = [[0, 0], [0x7c01, 0], [0, 1], [0x7c03, 1], [0xffffffff, 0], [0x7c05, 1], [0, 1], [0x80000000, 1],
                    [0x7c08, 0], [1, 1], [0, 0], [0x7c0b, 1]];
  F.c3eu_cursor = function () {
    const out = {};
    const E = recEnv();
    const vt = blk(0x40);
    for (let i = 0; i < 16; i++) vt.add(4 * i).writePointer(E.stub(0xC0DE0000 + i, 1));
    const subs = blk(0x10 * CUR_ROWS.length);
    const b = blk(0x20 * CUR_ROWS.length);
    CUR_ROWS.forEach(function (r, i) {
      const o = b.add(0x20 * i), sub = subs.add(0x10 * i);
      junk(sub, 0x10, 0x5b000000 + i * 0x100);
      sub.writePointer(vt);
      junk(o, 0x20, 0x4c000000 + i * 0x100);
      o.add(4).writeU32(r[0] >>> 0);
      o.add(8).writePointer(r[1] ? sub : ptr(0));
      out['c' + i] = o;
    });
    out.cblk = b;
    out.subs = subs;
    out.rec = E.rec;
    heapOn(out);
    const s = importOn({ module: 'golf_clean.exe', imports: {
      'USER32.dll!DestroyCursor': { args: 1, impl: function (a) { return (a[0].toUInt32() ^ 5) >>> 0; } } } });
    out.implog = s.log;
    return out;
  };

  // ====================================================================================================================
  // vtordisp `this` values. A thunk computes this - [this - 4]; a cell holds d at +0 and the thunk receives cell + 4, with
  // d = (cell + 4) - target, so the thunk lands on the real object while [object - 4] is never read or written. The cells
  // are written before the snapshot (both arms read the same d).
  // ====================================================================================================================
  function vdPointer(cell, target) {
    const p = cell.add(4);
    cell.writeU32((p.toUInt32() - target.toUInt32()) >>> 0);
    return p;
  }

  // Other batches' fixtures, wrapped so the negative-control switch is set after their freeze (every wrapped fixture
  // freezes the first thread itself: c3cz's surfaces under its heap stand-in, c3dl_bink through its import stand-in,
  // c3ef's and c3eg's through uitree.js / freezeNow).
  function foreign(name) {
    const fx = globalThis.DIFF_FIXTURES[name]();
    __freeze.negOn();
    return fx;
  }
  F.c3eu_surf_disp = function () { return foreign('c3cz_surf_disp'); };
  F.c3eu_surf_nodisp = function () { return foreign('c3cz_surf_nodisp'); };
  F.c3eu_dlbink = function () { return foreign('c3dl_bink'); };
  // arrayStaticInit_448160: c3cp's fixtures for its dynamic initializer 0x00448150 (the array 0x0080d840 saved; _live the
  // game's bytes, _pat a dword pattern, _zero zeroed; first thread frozen; _atexit stood in)
  ['live', 'pat', 'zero'].forEach(function (v) {
    F['c3eu_sndarr_' + v] = function () { return foreign('c3cp_00448150_' + v); };
  });

  // ListBox::deletingDtor 0x0048cba0 and its thunk 0x0048cb90 on c3ef's stand-alone ListBox parts (c3ef_parts_<v>: lbN, lbN_w
  // = the Window base +0x5c). Adds lbdN = a vtordisp pointer that resolves to lbN_w.
  ['a', 'b', 'c'].forEach(function (v) {
    F['c3eu_lbparts_' + v] = function () {
      const fx = foreign('c3ef_parts_' + v);
      const cells = blk(8 * 6);
      for (let i = 0; i < 6; i++) fx['lbd' + i] = vdPointer(cells.add(8 * i), fx['lb' + i + '_w']);
      return fx;
    };
  });

  // viewDeletingDtor 0x00491900 and its thunk 0x004918f0 on the composites inside c3ef's TextViews (c3ef_tv_<v>: tvN_lb = the
  // composite at TextView + 0x1460). Adds cwN = composite + 0x1fc (its Window base, the `this` viewDeletingDtor takes) and
  // cwdN = a vtordisp pointer that resolves to cwN.
  ['a', 'b', 'c'].forEach(function (v) {
    F['c3eu_tv_' + v] = function () {
      const fx = foreign('c3ef_tv_' + v);
      const cells = blk(8 * 4);
      for (let i = 0; i < 4; i++) {
        const cw = fx['tv' + i + '_lb'].add(0x1fc);
        fx['cw' + i] = cw;
        fx['cwd' + i] = vdPointer(cells.add(8 * i), cw);
      }
      return fx;
    };
  });

  // ListBox::typeAhead_vtordisp 0x0048ccd0 on c3eg's typeAhead rows (c3eg_ta: tN applies row N and returns the ListBox's
  // Window base, kN its key). tdN applies row N the same way and returns a vtordisp pointer that resolves to it.
  const TA_ROWS = [];
  for (let k = 0; k < 24; k++) TA_ROWS.push(k * 8);              // grid rows 0, 8, .., 184 (arrow keys over the geometries)
  for (let k = 200; k < 228; k++) TA_ROWS.push(k);               // every targeted row and every typed character
  F.c3eu_ta = function () {
    const fx = foreign('c3eg_ta');
    const cells = blk(8 * TA_ROWS.length);
    TA_ROWS.forEach(function (k, j) {
      const cell = cells.add(8 * j);
      Object.defineProperty(fx, 'td' + k, { enumerable: true, configurable: true, get: function () {
        return vdPointer(cell, ptr(fx['t' + k]));
      } });
    });
    return fx;
  };

  // ====================================================================================================================
  // Screen::dtor 0x004853d0 and SndMusic::dtor 0x004848a0 (both end in Snd::dtor 0x004841e0, C2, which runs ORIGINAL).
  // Objects are 0x80 bytes: +0x3c bucket index, +0x40 (Screen: an object whose slot 0 is a recording stub; SndMusic: a
  // handle for 0x00484110), +0x44 flags (bit 1 = linked), +0x48 prev, +0x4c next, +0x50 buffer (recorded by the
  // deallocator, never dereferenced). Neighbours are separate 0x60-byte blocks. Globals (saved, then ours):
  //   0x0083af64 / 0x0083af68 the list ends Snd::unlinkFromList 0x004844e0 rewrites when a neighbour is NULL;
  //   0x0083afc4 the sound-library flag and 0x0083af70 its release function, both read by 0x00484110 (variant lib1: a
  //   recording native callback that answers arg ^ 0x55; variant lib0: the flag is 0 and 0x00484110 answers 1).
  // The hash table object 0x0083ad80 is the game's own, READ only: none of our objects is in it, so HashTable::remove
  // 0x004876c0 walks one bucket and stores 0x10 at object + 0x3c (its no-match path).
  // ====================================================================================================================
  //            idx         +0x40   flags  prev  next  buf
  const SND_ROWS = [[0x10, 0, 0, 0, 0, 0], [0x3, 1, 0, 0, 0, 1], [0x0f, 0, 2, 1, 1, 0], [0x20, 1, 2, 0, 1, 1],
                    [0x05, 0, 2, 1, 0, 1], [0xffffffff, 1, 2, 0, 0, 0], [0, 0, 6, 1, 1, 1], [0x0f, 1, 0xfffffffd, 1, 1, 0],
                    [0x8, 1, 0x2, 1, 1, 1], [0x10, 0, 0x3, 0, 0, 0], [0x1, 1, 0, 0, 0, 0], [0x7fffffff, 1, 0x82, 1, 0, 1]];
  function sndEnv(kind, lib) {
    const out = {};
    const E = recEnv();
    const vt = blk(0x40);
    for (let i = 0; i < 16; i++) vt.add(4 * i).writePointer(E.stub(0x5C000000 + i, 1));
    const sb = blk(0x80 * 12 + 0x60 * 24 + 0x20 * 12 + 0x100, 0x3d000000);
    const data = sb.add(0x80 * 12 + 0x60 * 24 + 0x20 * 12);
    SND_ROWS.forEach(function (r, i) {
      const o = sb.add(0x80 * i);
      const pa = sb.add(0x80 * 12 + 0x60 * (2 * i)), pb = sb.add(0x80 * 12 + 0x60 * (2 * i + 1));
      const sub = sb.add(0x80 * 12 + 0x60 * 24 + 0x20 * i);
      sub.writePointer(vt);
      o.add(0x3c).writeU32(r[0] >>> 0);
      if (kind === 'screen') o.add(0x40).writePointer(r[1] ? sub : ptr(0));
      else o.add(0x40).writeU32(r[1] ? (0x7a000000 + i) >>> 0 : 0);
      o.add(0x44).writeU32(r[2] >>> 0);
      o.add(0x48).writePointer(r[3] ? pa : ptr(0));
      o.add(0x4c).writePointer(r[4] ? pb : ptr(0));
      o.add(0x50).writePointer(r[5] ? data.add(8 * i) : ptr(0));
      out['s' + i] = o;
    });
    out.sblk = sb;
    out.rec = E.rec;
    const librec = blk(0x10);
    out.librec = librec;
    const libfn = keep(new NativeCallback(function (a) {
      librec.writeU32(librec.readU32() + 1);
      librec.add(4).writeU32(a >>> 0);
      return (a ^ 0x55) >>> 0;
    }, 'uint32', ['uint32']));
    heapOn(out);
    saveSet(0x0083af64, 0x64646464);
    saveSet(0x0083af68, 0x68686868);
    saveSet(0x0083afc4, lib ? 1 : 0);
    saveSet(0x0083af70, lib ? libfn.toUInt32() : 0);
    return out;
  }
  F.c3eu_screen_lib0 = function () { return sndEnv('screen', false); };
  F.c3eu_screen_lib1 = function () { return sndEnv('screen', true); };
  F.c3eu_sndmusic_lib0 = function () { return sndEnv('music', false); };
  F.c3eu_sndmusic_lib1 = function () { return sndEnv('music', true); };

  // ====================================================================================================================
  // serviceMovie 0x0049c8e0: the movie table 0x00840698 (19 pointers) and its count 0x008406e4, saved and replaced with
  // fixture "movies" (0x20-byte blocks that are never dereferenced: serviceMovieFrame 0x0049c910 is stood in). The stand-in
  // answers ecx ^ 0x3c3c and, in the shrink / grow variants, rewrites the count when it receives a given movie (a
  // function of its argument and of compared memory only).
  // ====================================================================================================================
  const MOVIES = {
    n0: { count: 0 }, neg: { count: -3 }, n1: { count: 1 }, n3: { count: 3 }, n19: { count: 19 },
    shrink: { count: 6, at: 2, to: 2 }, grow: { count: 2, at: 0, to: 5 }, drop0: { count: 4, at: 1, to: 0 },
  };
  Object.keys(MOVIES).forEach(function (v) {
    F['c3eu_movie_' + v] = function () {
      const m = MOVIES[v];
      const fx = { clog: newClog() };
      const mv = blk(0x20 * 19, 0x6e000000);
      fx.mv = mv;
      freezeNow();
      __freeze.save(ptr(0x00840698), 0x50);
      for (let i = 0; i < 19; i++) ptr(0x00840698 + 4 * i).writePointer(mv.add(0x20 * i));
      ptr(0x008406e4).writeS32(m.count);
      const marker = m.at === undefined ? 0 : mv.add(0x20 * m.at).toUInt32();
      calleeStand(fx, 0, 0x0049c910, 1, 'thiscall', function (n, a) {
        if (marker !== 0 && a[0] === marker) ptr(0x008406e4).writeS32(m.to);
        return (a[0] ^ 0x3c3c) >>> 0;
      });
      return fx;
    };
  });

  // ====================================================================================================================
  // gfxFlushB 0x00497b20: the network object 0x008400b8 and the session word 0x008400b0, saved and set; netServiceControl
  // 0x0049ab40 (C2) is stood in (thiscall, no stack argument) and answers ecx ^ 0x5a5a.
  // ====================================================================================================================
  const GFX = { noobj: [0, 1], nosess: [1, 0], call1: [1, 1], call2: [2, 0x80000000], call3: [3, 0xffffffff], call4: [2, 7] };
  Object.keys(GFX).forEach(function (v) {
    F['c3eu_gfx_' + v] = function () {
      const fx = { clog: newClog() };
      const objs = blk(0x40 * 4, 0x2f000000);
      fx.objs = objs;
      freezeNow();
      const g = GFX[v];
      saveSet(0x008400b8, g[0] ? objs.add(0x40 * g[0]).toUInt32() : 0);
      saveSet(0x008400b0, g[1]);
      calleeStand(fx, 0, 0x0049ab40, 1, 'thiscall', function (n, a) { return (a[0] ^ 0x5a5a) >>> 0; });
      return fx;
    };
  });

  // ====================================================================================================================
  // netFlush 0x00497cc0: twelve objects of 0x100 bytes (+0x64 a sub-object whose vtable slot +0x20 is a recording stub with
  // no stack argument, +0xe4 a value, +0xe8 the flag word). netSendMessages 0x00499140 (C2) is stood in (thiscall, five
  // stack arguments; log values: ecx, then the five) and answers its third stack argument (+0xe4) ^ 0x99.
  // ====================================================================================================================
  const NF_ROWS = [[0x20000000, 0], [0xffffffff, 0x7f], [0x20000001, -1], [0x60000000, 0x1234], [0x20000000, 0x80000000],
                   [0xa0000000, 5], [0x20008000, 0x200], [0x3fffffff, 1],
                   [0, 0x11], [0xdfffffff, 0x22], [0x10000000, 0x33], [0x40000000, 0x44]];
  F.c3eu_netflush = function () {
    const fx = { clog: newClog() };
    const E = recEnv();
    const vt = blk(0x40);
    for (let i = 0; i < 16; i++) vt.add(4 * i).writePointer(E.stub(0x4E000000 + i, 0));
    const ob = blk(0x100 * NF_ROWS.length, 0x1e000000);
    NF_ROWS.forEach(function (r, i) {
      const o = ob.add(0x100 * i);
      o.add(0x64).writePointer(vt);
      o.add(0xe4).writeU32(r[1] >>> 0);
      o.add(0xe8).writeU32(r[0] >>> 0);
      fx['n' + i] = o;
    });
    fx.nblk = ob;
    fx.rec = E.rec;
    calleeStand(fx, 0, 0x00499140, 6, 'thiscall', function (n, a) { return (a[3] ^ 0x99) >>> 0; });
    return fx;
  };

  // ====================================================================================================================
  // lookupThemeText 0x0045b660 (path, key): our own synthetic text files served through filestandin.js's CRT layer (fopen
  // 0x004a614d, fgets 0x004a65ee, fclose 0x004a609f), text mode. The text buffer 0x0051a068 (0x1000 bytes saved) starts
  // as 'pre|'. Files:
  //   th0  the key on line 0, three lines, a '*' line
  //   th1  the key (other case) on line 5 among other names, two lines, '*', more lines
  //   th2  no key (four lines, then end of file: the not-found loop runs 100 reads, the last line cut again on each)
  //   th3  the key, two lines, end of file without '*' (100 appends, the last line cut again each time)
  //   th4  the key on line 99 (the last line the search reads) and the key on line 100 (never reached)
  //   th5  CR LF line ends, an empty line before the key, a line that is only one character
  //   th6  a line longer than fgets' 0xf9 characters right after the key (split across two reads), then '*'
  // Vectors pass (file name, key) string pointers; keys k0..k5.
  // ====================================================================================================================
  function lines(arr, crlf) { return arr.join(crlf ? '\r\n' : '\n') + (crlf ? '\r\n' : '\n'); }
  const LONG = 'L' + 'x'.repeat(0x110) + 'end';
  const NAMES99 = []; for (let i = 0; i < 99; i++) NAMES99.push('name' + i);
  const THEME_FILES = {
    'th0.txt': lines(['alpha', 'first line', 'second line', 'third', '*', 'after']),
    'th1.txt': lines(['beta', 'gamma', 'delta', 'epsilon', 'zeta', 'ALPHA', 'one', 'two', '*end', 'tail1', 'tail2']),
    'th2.txt': lines(['beta', 'gamma', 'delta', 'last line here']),
    'th3.txt': 'alpha\nkept one\nkept two last',
    'th4.txt': lines(NAMES99.concat(['omega', 'psi', 'body', '*'])),
    'th5.txt': lines(['', 'q', 'Alpha', 'crlf body', 'x', '*'], true),
    'th6.txt': lines(['alpha', LONG, 'short', '*']),
  };
  const THEME_KEYS = ['alpha', 'omega', 'psi', '', 'q', 'missing'];
  F.c3eu_theme = function () {
    const fx = {};
    freezeNow();
    __freeze.save(ptr(0x0051a068), 0x1000);
    ptr(0x0051a068).writeUtf8String('pre|');
    const files = {};
    Object.keys(THEME_FILES).forEach(function (n) {
      const s = THEME_FILES[n], b = [];
      for (let i = 0; i < s.length; i++) b.push(s.charCodeAt(i) & 0xff);
      files[n] = b;
    });
    const fs = __fileStandIn({ module: 'golf_clean.exe', kernel32: false, crt: ['fopen', 'fgets', 'fclose'], files: files,
                               size: 0x4000 });
    fx.fsblock = fs.block;
    Object.keys(THEME_FILES).forEach(function (n, i) { fx['f' + i] = keep(Memory.allocUtf8String(n)); });
    THEME_KEYS.forEach(function (k, i) { fx['k' + i] = keep(Memory.allocUtf8String(k)); });
    return fx;
  };

  // ====================================================================================================================
  // loadTopScores 0x0040ad60 (no arguments). FindFirstFileA [0x4ba090] is stood in (only its name argument recorded; the
  // second is a frame buffer). Absent (_s0.._s3): the generator seed 0x00822d9c set per key. Present (_p618, _p100, _p700):
  // a 'top10.sve' of 0x618 / 0x100 / 0x700 synthetic bytes served by a low-level descriptor stand-in for _open 0x004a5d48,
  // _read 0x004a583a and _close 0x004a5a78 (c3ep's lowIo, reduced to these three; its block is state). Saved and pre-filled:
  // the table 0x00541ce0 (0x618 bytes), 0x0051a068 (0x40), the descriptor word 0x00568d08, the seed.
  // ====================================================================================================================
  function crc32(s) {
    let c = 0xffffffff;
    for (let i = 0; i < s.length; i++) {
      c ^= s.charCodeAt(i) & 0xff;
      for (let k = 0; k < 8; k++) c = (c >>> 1) ^ (0xedb88320 & -(c & 1));
    }
    return (c ^ 0xffffffff) >>> 0;
  }
  // Low-level block (0x1000, all state): +0 open count, +4 reads, +8 closes, +0xc log cursor (entries {op, a, b, c} from
  // +0x40 up to +0x400), +0x10 descriptor in use, +0x14 its cursor; the file's bytes from +0x400.
  const FD = 5;
  function lowIo(fx, bytes) {
    const blk = blk0x1000();
    fx.lio = blk;
    if (bytes) blk.add(0x400).writeByteArray(bytes);
    const len = bytes ? bytes.length : 0;
    const tid = Process.getCurrentThreadId();
    const log = function (op, a, b, c) {
      const cur = blk.add(0xc).readU32();
      if (cur + 0x10 <= 0x3c0) {
        const p = blk.add(0x40 + cur);
        p.writeU32(op >>> 0); p.add(4).writeU32(a >>> 0); p.add(8).writeU32(b >>> 0); p.add(0xc).writeU32(c >>> 0);
        blk.add(0xc).writeU32(cur + 0x10);
      }
    };
    const impl = {
      0x004a5d48: [3, function (a) {                                  // _open(path, flags): only 'top10.sve' exists
        const name = ptr(a[0]).readCString().toLowerCase();
        blk.writeU32(blk.readU32() + 1);
        if (!bytes || name !== 'top10.sve' || blk.add(0x10).readU32() !== 0) { log(1, crc32(name), a[1], 0xffffffff); return 0xffffffff; }
        blk.add(0x10).writeU32(1); blk.add(0x14).writeU32(0);
        log(1, crc32(name), a[1], FD);
        return FD;
      }],
      0x004a583a: [3, function (a) {                                  // _read(fd, buf, n)
        blk.add(4).writeU32(blk.add(4).readU32() + 1);
        if (a[0] !== FD || blk.add(0x10).readU32() === 0) { log(2 | 0x80, a[0], a[2], 0); return 0xffffffff; }
        const cur = blk.add(0x14).readU32(), got = Math.max(0, Math.min(a[2] | 0, len - cur));
        if (got > 0) Memory.copy(ptr(a[1]), blk.add(0x400 + cur), got);
        blk.add(0x14).writeU32(cur + got);
        log(2, a[0], a[2], got);
        return got;
      }],
      0x004a5a78: [1, function (a) {                                  // _close(fd)
        blk.add(8).writeU32(blk.add(8).readU32() + 1);
        if (a[0] !== FD || blk.add(0x10).readU32() === 0) { log(4 | 0x80, a[0], 0, 0); return 0xffffffff; }
        blk.add(0x10).writeU32(0);
        log(4, a[0], blk.add(0x14).readU32(), 0);
        return 0;
      }],
    };
    Object.keys(impl).forEach(function (vaKey) {
      const at = ptr(parseInt(vaKey, 10)), k = at.toString();
      if (standLive[k]) { try { Interceptor.revert(at); } catch (e) { /* gone */ } delete standLive[k]; Interceptor.flush(); }
      const argc = impl[vaKey][0], fn = impl[vaKey][1];
      const types = Array(argc).fill('uint32');
      const orig = keep(new NativeFunction(at, 'uint32', types));
      const cb = keep(new NativeCallback(function () {
        if (Process.getCurrentThreadId() !== tid) return orig.apply(null, arguments);
        return fn(Array.prototype.slice.call(arguments).map(function (v) { return v >>> 0; })) >>> 0;
      }, 'uint32', types));
      Interceptor.replace(at, cb);
      const tok = standTok++;
      standLive[k] = tok;
      setTimeout(function () {
        if (standLive[k] !== tok) return;
        Interceptor.revert(at); delete standLive[k]; Interceptor.flush();
      }, 0);
    });
    Interceptor.flush();
  }
  function blk0x1000() { return blk(0x1000); }
  function topScoresEnv(seed, bytes) {
    const fx = {};
    freezeNow();
    __freeze.save(ptr(0x00541ce0), 0x618);
    junk(ptr(0x00541ce0), 0x618, 0x70000000);
    __freeze.save(ptr(0x0051a068), 0x40);
    junk(ptr(0x0051a068), 0x40, 0x41424300);
    ptr(0x0051a068 + 0x3c).writeU32(0);
    saveSet(0x00568d08, 0x0d0d0d0d);
    __freeze.save(ptr(0x00822d9c), 8);
    ptr(0x00822d9c).writeU32(seed >>> 0);
    const s = importOn({ module: 'golf_clean.exe', imports: {
      'KERNEL32.dll!FindFirstFileA': { args: 2, record: [0], ret: bytes ? 0x46460001 : 0xffffffff } } });
    fx.implog = s.log;
    if (bytes) lowIo(fx, bytes);
    else fx.lio = blk0x1000();
    return fx;
  }
  [0x12345678, 1, 0xdeadbeef, 0x00010000].forEach(function (seed, i) {
    F['c3eu_top_s' + i] = function () { return topScoresEnv(seed, null); };
  });
  [[0x618, 'p618'], [0x100, 'p100'], [0x700, 'p700']].forEach(function (r) {
    F['c3eu_top_' + r[1]] = function () {
      const b = [];
      for (let i = 0; i < r[0]; i++) b.push((i * 7 + 0x21) & 0xff);
      return topScoresEnv(0x5555, b);
    };
  });

  // ====================================================================================================================
  // jinit_memory_mgr 0x004aee30 (cinfo) and jpeg_CreateCompress 0x004ae150 (cinfo, version, size): four cinfo blocks of
  // 0x200 bytes (junk-filled, +0 = an error manager whose error_exit (+0) is a recording callback that RETURNS: it appends
  // msg_code +0x14 and msg_parm +0x18 / +0x1c to $errlog), malloc 0x004a504f carved from the heap stand-in's arena (the
  // memory manager block lands there), getenv 0x004ad91d stood in (calleeStand) answering the key's string or NULL.
  // ====================================================================================================================
  const JENV = { none: null, m: '123m', M: '45M', plain: '7', k: '-3k', bad: 'x', empty: '' };
  function jpegEnv(env) {
    const fx = { clog: newClog() };
    const cb = blk(0x200 * 4, 0x6a000000);
    const eb = blk(0x40 * 4, 0x65000000);
    const errlog = blk(0x100);
    const exitFn = keep(new NativeCallback(function (ci) {
      const err = ci.readPointer();
      const n = errlog.readU32();
      if (n < 15) {
        errlog.add(4 + 0x10 * n).writeU32(err.add(0x14).readU32());
        errlog.add(8 + 0x10 * n).writeU32(err.add(0x18).readU32());
        errlog.add(0xc + 0x10 * n).writeU32(err.add(0x1c).readU32());
      }
      errlog.writeU32(n + 1);
    }, 'void', ['pointer']));
    for (let i = 0; i < 4; i++) {
      const e = eb.add(0x40 * i);
      e.writePointer(exitFn);
      cb.add(0x200 * i).writePointer(e);
      fx['c' + i] = cb.add(0x200 * i);
    }
    fx.cblk = cb; fx.eblk = eb; fx.errlog = errlog;
    freezeNow();
    const h = __heapStandIn({ module: 'golf_clean.exe', alloc: [0xa504f], free: [0xa5007, 0xa4ffc], arenaSize: 0x400,
                              exhaust: 'null' });
    fx.arena = h.arena; fx.freelog = h.log;
    const s = env === null ? ptr(0) : keep(Memory.allocUtf8String(env));
    calleeStand(fx, 0, 0x004ad91d, 1, 'default', function () { return s.toUInt32(); });
    return fx;
  }
  Object.keys(JENV).forEach(function (v) { F['c3eu_jmem_' + v] = function () { return jpegEnv(JENV[v]); }; });

  // ====================================================================================================================
  // resolveDataPath 0x00491da0 (name): the three directory strings 0x0083bf04 (A), 0x0083c104 (B), 0x0083be04 (C) and the
  // result buffer 0x0083c004 (0x0083be04 .. 0x0083c203 saved, set per variant, compared). FindFirstFileA / FindClose are
  // stood in: a name in FOUND answers a handle 0x46440000 + index, any other -1; each call records the crc32 of the name
  // (the find-data argument is a frame buffer and is not recorded) and FindClose its handle. Variants: abc (all three
  // directories), b (A and C empty), ab (C empty), bc (A empty).
  // ====================================================================================================================
  const FOUND = ['da\\one.dat', 'db\\two.dat', 'dc\\three.dat', 'db\\one.dat', 'x:abs.dat'];
  const DIRS = { abc: ['dA\\', 'dB\\', 'dC\\'], b: ['', 'dB\\', ''], ab: ['dA\\', 'dB\\', ''], bc: ['', 'dB\\', 'dC\\'] };
  const RNAMES = ['one.dat', 'two.dat', 'three.dat', 'none.dat', 'X:abs.dat', 'Y:no.dat'];
  Object.keys(DIRS).forEach(function (v) {
    F['c3eu_resolve_' + v] = function () {
      const fx = {};
      freezeNow();
      __freeze.save(ptr(0x0083be04), 0x400);
      junk(ptr(0x0083be04), 0x400, 0x2e2e0000);
      const d = DIRS[v];
      ptr(0x0083bf04).writeUtf8String(d[0]);
      ptr(0x0083c104).writeUtf8String(d[1]);
      ptr(0x0083be04).writeUtf8String(d[2]);
      ptr(0x0083c004).writeUtf8String('stale');
      RNAMES.forEach(function (n, i) { fx['n' + i] = keep(Memory.allocUtf8String(n)); });
      const s = importOn({ module: 'golf_clean.exe', imports: {
        'KERNEL32.dll!FindFirstFileA': { args: 2, record: false, impl: function (a, ctx) {
          const name = a[0].readCString();
          const i = FOUND.indexOf(name.toLowerCase());
          ctx.record([crc32(name), i]);
          return i < 0 ? 0xffffffff : (0x46440000 + i) >>> 0;
        } },
        'KERNEL32.dll!FindClose': { args: 1, ret: 1 } } });
      fx.implog = s.log;
      return fx;
    };
  });

  // ====================================================================================================================
  // Second pass (continued session).
  // playHabitatSound 0x00405a00 (kind, a, b): playSoundAt 0x0040c500 (C3, cdecl, four arguments) stood in; it answers a
  // function of its arguments.
  // ====================================================================================================================
  F.c3eu_habitat = function () {
    const fx = { clog: newClog() };
    calleeStand(fx, 0, 0x0040c500, 4, 'default', function (n, a) {
      return (Math.imul(a[0], 0x101) ^ a[1] ^ (a[2] << 3) ^ a[3]) >>> 0;
    });
    return fx;
  };

  // ====================================================================================================================
  // flicNextFrame 0x00404090 (slot) / flicOpenStream 0x00404120 (path, slot, outCount): slots 0..11 of the Flic records
  // 0x004f65c8 (0x84 each) and of the links 0x0050fc20 (0x44 each), saved (the game's first thread is frozen), patterned,
  // and set per row: Flic +0x44 (NULL or not) and +0x58 (our header block, words +0x60 / +0x62), link +4 (Link482::data's
  // test), +0x3c, +0x40. Link482::resolve 0x00482e20, Link482b::send 0x00482e40, Flic::open 0x00481f40 and
  // Flic::createStream 0x00482b90 (C2) are stood in; Link482::data 0x00482e10 (C3) runs. outCount cells: $outs (12 x 4).
  // ====================================================================================================================
  // [has44, w62, w60, pos3c, frame40, link4]
  const FLIC_ROWS = [
    [1, 10, 10, 3, 0, 1], [1, 10, 10, 9, 9, 1], [1, 10, 10, 12, 4, 1], [0, 0, 0, 5, 0, 1],
    [0, 0, 0, 0xffffffff, 0xffffffff, 1], [0, 0, 0, 0xffffffff, 7, 0], [1, 1, 1, 0, 0, 1], [1, 1, 0, 0, 0, 1],
    [1, 0xffff, 0x8000, 0xfffe, 0x7fff, 1], [1, 0xffff, 0x8000, 0x7ffe, 0, 1], [1, 2, 3, 1, 0x80000000, 1],
    [1, 5, 5, 4, 3, 0]];
  function flicEnv(open) {
    const fx = { clog: newClog() };
    freezeNow();
    __freeze.save(ptr(0x004f65c8), 0x84 * 12);
    __freeze.save(ptr(0x0050fc20), 0x44 * 12);
    junk(ptr(0x004f65c8), 0x84 * 12, 0x0f100000);
    junk(ptr(0x0050fc20), 0x44 * 12, 0x0f200000);
    const hdr = blk(0x80 * 12, 0x0f300000);
    const outs = blk(4 * 12, 0x0f400000);
    fx.hdr = hdr; fx.outs = outs;
    FLIC_ROWS.forEach(function (r, i) {
      const f = ptr(0x004f65c8 + 0x84 * i), l = ptr(0x0050fc20 + 0x44 * i), h = hdr.add(0x80 * i);
      f.add(0x44).writeU32(r[0] ? h.add(0x40).toUInt32() : 0);
      f.add(0x58).writePointer(h);
      h.add(0x60).writeU16(r[2]);
      h.add(0x62).writeU16(r[1]);
      l.add(4).writeU32(r[5] ? (0x0f500000 + i) >>> 0 : 0);
      l.add(0x3c).writeU32(r[3] >>> 0);
      l.add(0x40).writeU32(r[4] >>> 0);
      fx['out' + i] = outs.add(4 * i);
    });
    calleeStand(fx, 0, 0x00482e20, 1, 'thiscall', function (n, a) { return (a[0] ^ 0x1e50) >>> 0; });
    calleeStand(fx, 1, 0x00482e40, 4, 'thiscall', function (n, a) { return (a[0] ^ a[1] ^ 0x5e) >>> 0; });
    if (open) {
      // Flic::open fails (answers path ^ 0x77) for an odd path, else 0
      calleeStand(fx, 2, 0x00481f40, 5, 'thiscall', function (n, a) { return (a[1] & 1) ? (a[1] ^ 0x77) >>> 0 : 0; });
      calleeStand(fx, 3, 0x00482b90, 3, 'thiscall', function (n, a) { return (a[0] ^ a[1] ^ 0x33) >>> 0; });
    }
    return fx;
  }
  F.c3eu_flic = function () { return flicEnv(false); };
  F.c3eu_flicopen = function () { return flicEnv(true); };

  // ====================================================================================================================
  // globalObjDtor49c780 0x0049c780 (this): twelve patterned 0x600-byte objects; Timer::dtor 0x00486ce0, Surface::
  // releaseBacking 0x004838b0 and Base4805a0::dtor 0x004805a0 (C3) stood in (thiscall, no stack argument; each answers ecx
  // xor a tag); 0x0049c0a0 (a single store at +0x594) runs.
  // ====================================================================================================================
  F.c3eu_gobj = function () {
    const fx = { clog: newClog() };
    const ob = blk(0x600 * 12, 0x6b000000);
    fx.oblk = ob;
    for (let i = 0; i < 12; i++) fx['o' + i] = ob.add(0x600 * i);
    calleeStand(fx, 0, 0x00486ce0, 1, 'thiscall', function (n, a) { return (a[0] ^ 0x11) >>> 0; });
    calleeStand(fx, 1, 0x004838b0, 1, 'thiscall', function (n, a) { return (a[0] ^ 0x22) >>> 0; });
    calleeStand(fx, 2, 0x004805a0, 1, 'thiscall', function (n, a) { return (a[0] ^ 0x4805) >>> 0; });
    return fx;
  };

  // ====================================================================================================================
  // initSound 0x00483e90 (device, flags): g_soundReady 0x0083afc4 saved and set to 0 (the game's first thread is frozen;
  // CLAUDE.md: it is 0 on a machine without audio output, and Screen::dtor_lib1 sets it the same way). Stood in (C3):
  // soundVersionCheck 0x00483fc0 (answers the variant's value), callSoundHook74 0x00484090, resourceLoadB 0x00487a70,
  // resourceLoad 0x00487c00 (fails, answering device ^ 0x500, for an odd device on the music object 0x0083ad80, and for a
  // device with bit 1 set on 0x0083af98), freeSoundLib 0x00484060.
  // ====================================================================================================================
  [['ok', 0], ['ver3', 3], ['verneg', 0xffffffff]].forEach(function (r) {
    F['c3eu_isnd_' + r[0]] = function () {
      const fx = { clog: newClog() };
      freezeNow();
      saveSet(0x0083afc4, 0);
      calleeStand(fx, 0, 0x00483fc0, 0, 'default', function () { return r[1]; });
      calleeStand(fx, 1, 0x00484090, 2, 'default', function () { return 0x74; });
      calleeStand(fx, 2, 0x00487a70, 3, 'thiscall', function (n, a) { return (a[0] ^ 5) >>> 0; });
      calleeStand(fx, 3, 0x00487c00, 3, 'thiscall', function (n, a) {
        if (a[0] === 0x0083ad80 && (a[1] & 1)) return (a[1] ^ 0x500) >>> 0;
        if (a[0] === 0x0083af98 && (a[1] & 2)) return (a[1] ^ 0x900) >>> 0;
        return 0;
      });
      calleeStand(fx, 4, 0x00484060, 0, 'default', function () { return 0x60; });
      return fx;
    };
  });

  // ====================================================================================================================
  // shutdownSound 0x00483f10 (): our own doubly linked list of 0x80-byte nodes (+0x48 prev, +0x4c next, the layout
  // unlinkSound 0x004843e0 uses: head 0x0083af68, tail 0x0083af64, both saved and set), g_soundReady 0x0083afc4 saved and
  // set, the library pointer [0x0083af78] saved and pointed at a recorder (A/B thread only; other threads go to the saved
  // pointer). unlinkSound (C2) runs. Each node's vtable slot +0x80 is a recorder that unlinks the node the same way (a
  // function of compared memory only), so every arm of the loop ends. releaseSoundObj 0x00484110, releaseWithHookA/B/C
  // 0x00487460 / 0x00487ac0 / 0x00487c40 and freeSoundLib 0x00484060 (C3) are stood in; operator delete records
  // (heapstandin.js). Rows: [flags +0x44, +0x40 non-zero, +0x50 a block].
  // ====================================================================================================================
  const SHUT = {
    off: { ready: 0, rows: [[0x202, 1, 1]] }, empty: { ready: 1, rows: [] },
    n1: { ready: 1, rows: [[0x202, 1, 1]] },
    n4: { ready: 0x80000000, rows: [[0x202, 1, 0], [2, 0, 1], [0x200, 1, 1], [0xffffffff, 0, 0]] },
    n8: { ready: 1, rows: [[0, 0, 0], [0x203, 1, 1], [0x1202, 0, 1], [0xfffffdff, 1, 0], [0x200, 0, 0], [0xfffffffd, 1, 1],
                           [0x202, 0, 0], [2, 1, 1]] },
    v6: { ready: 7, rows: [[1, 1, 1], [0x400, 0, 1], [0x302, 1, 0], [0x2, 0, 0], [0x202, 1, 1], [0x100, 1, 1]] },
  };
  Object.keys(SHUT).forEach(function (v) {
    F['c3eu_shut_' + v] = function () {
      const sv = SHUT[v];
      const fx = { clog: newClog() };
      heapOn(fx);
      const tid = Process.getCurrentThreadId();
      const rec = blk(0x200);
      rec.writeU32(0x10);
      fx.rec = rec;
      const vfn = keep(new NativeCallback(function (self) {
        const c = rec.readU32();
        if (c + 4 <= 0x1f0) { rec.add(c).writePointer(self); rec.writeU32(c + 4); }
        const prev = self.add(0x48).readPointer(), next = self.add(0x4c).readPointer();
        if (!prev.isNull()) prev.add(0x4c).writePointer(next); else ptr(0x0083af68).writePointer(next);
        if (!next.isNull()) next.add(0x48).writePointer(prev); else ptr(0x0083af64).writePointer(prev);
        self.add(0x48).writePointer(ptr(0));
        self.add(0x4c).writePointer(ptr(0));
        return (self.toUInt32() ^ 0x80) >>> 0;
      }, 'uint32', ['pointer'], 'thiscall'));
      const vt = blk(0x100, 0x76000000);
      vt.add(0x80).writePointer(vfn);
      __freeze.save(ptr(0x0083af64), 8);
      __freeze.save(ptr(0x0083af78), 4);
      __freeze.save(ptr(0x0083afc4), 4);
      const prevLib = ptr(0x0083af78).readPointer();
      const prevFn = prevLib.isNull() ? null : new NativeFunction(prevLib, 'void', []);
      const libfn = keep(new NativeCallback(function () {
        if (Process.getCurrentThreadId() !== tid) { if (prevFn !== null) prevFn(); return; }
        rec.add(0x1f0).writeU32(rec.add(0x1f0).readU32() + 1);
      }, 'void', []));
      ptr(0x0083af78).writePointer(libfn);
      ptr(0x0083afc4).writeU32(sv.ready >>> 0);
      const nb = blk(0x80 * Math.max(sv.rows.length, 1), 0x5e000000);
      const dels = blk(0x20 * Math.max(sv.rows.length, 1), 0x5d000000);
      fx.nblk = nb; fx.dels = dels;
      const nodes = sv.rows.map(function (r, i) { return nb.add(0x80 * i); });
      sv.rows.forEach(function (r, i) {
        const n = nodes[i];
        n.writePointer(vt);
        n.add(0x40).writeU32(r[1] ? (0x40400000 + i) >>> 0 : 0);
        n.add(0x44).writeU32(r[0] >>> 0);
        n.add(0x48).writePointer(i > 0 ? nodes[i - 1] : ptr(0));
        n.add(0x4c).writePointer(i + 1 < nodes.length ? nodes[i + 1] : ptr(0));
        n.add(0x50).writePointer(r[2] ? dels.add(0x20 * i) : ptr(0));
      });
      ptr(0x0083af68).writePointer(nodes.length ? nodes[0] : ptr(0));
      ptr(0x0083af64).writePointer(nodes.length ? nodes[nodes.length - 1] : ptr(0));
      calleeStand(fx, 0, 0x00484110, 1, 'default', function (n, a) { return (a[0] ^ 0x4110) >>> 0; });
      calleeStand(fx, 1, 0x00487460, 1, 'thiscall', function (n, a) { return (a[0] ^ 0xa) >>> 0; });
      calleeStand(fx, 2, 0x00487ac0, 1, 'thiscall', function (n, a) { return (a[0] ^ 0xb) >>> 0; });
      calleeStand(fx, 3, 0x00487c40, 1, 'thiscall', function (n, a) { return (a[0] ^ 0xc) >>> 0; });
      calleeStand(fx, 4, 0x00484060, 0, 'default', function () { return 0x60; });
      return fx;
    };
  });

  // ====================================================================================================================
  // logWrite 0x004a0280 (this, s1, s2, d1, d2, d3): logger objects t0..t4 (+4 path string or NULL, +8 flag) and the global
  // 0x00840940 (saved, set per variant: 0 / 1). resolveAndOpenFile 0x004a00f0 (C3), fprintf 0x004a622c and fclose 0x004a609f
  // are stood in: the open answers a fake FILE value for a path not starting with 'x' (a function of compared memory), else
  // NULL; fprintf logs the six or seven words its format string takes (0x004e4a50: 6, 0x004e4a60: 7); nothing touches a file.
  // ====================================================================================================================
  const LOGT = [['log.txt', 0], [null, 0], ['log.txt', 1], ['xfail.log', 0], ['b.log', 0]];
  [['', 0], ['_off', 1]].forEach(function (r) {
    F['c3eu_logw' + r[0]] = function () {
      const fx = { clog: newClog() };
      freezeNow();
      saveSet(0x00840940, r[1]);
      const tb = blk(0x20 * LOGT.length, 0x109a0000);
      const names = blk(0x20 * LOGT.length);
      fx.tblk = tb; fx.names = names;
      LOGT.forEach(function (t, i) {
        const o = tb.add(0x20 * i);
        if (t[0] !== null) names.add(0x20 * i).writeUtf8String(t[0]);
        o.add(4).writePointer(t[0] === null ? ptr(0) : names.add(0x20 * i));
        o.add(8).writeU32(t[1]);
        fx['t' + i] = o;
      });
      calleeStand(fx, 0, 0x004a00f0, 2, 'default', function (n, a) {
        return ptr(a[0]).readU8() === 0x78 ? 0 : (a[0] ^ 0xf11e0000) >>> 0;
      });
      calleeStand(fx, 1, 0x004a622c, 7, 'default', function (n, a) { return (a[2] ^ 0x1f) >>> 0; },
                  function (a) { return a[1] === 0x004e4a50 ? 6 : 7; });
      calleeStand(fx, 2, 0x004a609f, 1, 'default', function (n, a) { return (a[0] ^ 0xc105e) >>> 0; });
      return fx;
    };
  });

  // ====================================================================================================================
  // writeCourseInfo 0x00431fa0 (path): path buffers p0..p3 (0x100 each). The globals it prints (0x005685f0, 0x0059aafc,
  // 0x0058d36c, 0x005a34e0, 0x0056a524, 0x00571fd4, 0x0059ae78, 0x00541cd8, 0x005a882c, 0x0056949c, 0x005a636c) and the two
  // strings 0x00567328 / 0x004d6098 are saved and set per variant; the text buffer 0x0051a068 is saved (appendCourseTitle /
  // appendString, C3, run and write it). fopen 0x004a614d, fprintf 0x004a622c and fclose 0x004a609f are stood in: fprintf
  // formats each call with msvcrt's _snprintf from the words its format takes (a table by format address) and appends the
  // text to the output buffer $fout, so the file's bytes are compared; nothing touches a file.
  // ====================================================================================================================
  const CI_WORDS = { 0x004c7918: 0, 0x004c790c: 1, 0x004c7900: 1, 0x004c78f8: 1, 0x004c78ec: 1, 0x004c78e0: 1, 0x004c78d4: 1,
                     0x004c78c4: 1, 0x004c78b0: 2, 0x004c78a4: 1, 0x004c7894: 1, 0x004c7880: 2, 0x004c786c: 2,
                     0x004c7858: 2, 0x004c7844: 2 };
  const CI_INT = [0x005685f0, 0x0059aafc, 0x0058d36c, 0x0056a524, 0x00571fd4, 0x0059ae78, 0x00541cd8, 0x005a882c,
                  0x0056949c, 0x005a636c];
  const CI_VAR = {
    a: { ints: [19, 72, 6800, 64, 1500, 7, 250, 99, 1, 12345], type: 0, s1: 'ThemeA', s2: 'DesA' },
    b: { ints: [1, -3, 0, -65, -2, 0x7fffffff, -1, 0, 100000, -12345], type: 0xfe, s1: '', s2: 'X' },
    c: { ints: [0, 0, 1, 0, 0x147ae14, 3, 101, 1999, -7, 50], type: 0x13, s1: 'T', s2: '' },
  };
  const CI_PATHS = ['course.crs', 'noext', 'a.b.c', '.hid'];
  let snp1 = null, snp2 = null, snp0 = null;
  function snprintfs() {
    if (snp0 !== null) return;
    const m = Module.load('msvcrt.dll');
    const at = m.getExportByName('_snprintf');
    snp0 = new NativeFunction(at, 'int', ['pointer', 'uint32', 'pointer', '...']);
    snp1 = new NativeFunction(at, 'int', ['pointer', 'uint32', 'pointer', '...', 'uint32']);
    snp2 = new NativeFunction(at, 'int', ['pointer', 'uint32', 'pointer', '...', 'uint32', 'uint32']);
  }
  Object.keys(CI_VAR).forEach(function (v) {
    F['c3eu_cinfo_' + v] = function () {
      const cv = CI_VAR[v];
      snprintfs();
      const fx = { clog: newClog() };
      freezeNow();
      CI_INT.forEach(function (va, i) { saveSet(va, cv.ints[i]); });
      __freeze.save(ptr(0x005a34e0), 4);
      ptr(0x005a34e0).writeU8(cv.type);
      __freeze.save(ptr(0x00567328), 0x40);
      __freeze.save(ptr(0x004d6098), 0x40);
      ptr(0x00567328).writeUtf8String(cv.s1);
      ptr(0x004d6098).writeUtf8String(cv.s2);
      __freeze.save(ptr(0x0051a068), 0x400);
      ptr(0x0051a068).writeUtf8String('stale');
      const pb = blk(0x100 * CI_PATHS.length, 0x70000000);
      CI_PATHS.forEach(function (p, i) { pb.add(0x100 * i).writeUtf8String(p); fx['p' + i] = pb.add(0x100 * i); });
      fx.pblk = pb;
      const out = blk(0x1000);
      out.writeU32(0x10);
      fx.fout = out;
      const tmp = blk(0x200);
      calleeStand(fx, 0, 0x004a614d, 2, 'default', function () { return 0x0f11e000; });
      calleeStand(fx, 1, 0x004a622c, 4, 'default', function (n, a) {
        const w = CI_WORDS[a[1]];
        let r;
        if (w === 0) r = snp0(tmp, 0x1ff, ptr(a[1]));
        else if (w === 1) r = snp1(tmp, 0x1ff, ptr(a[1]), a[2]);
        else r = snp2(tmp, 0x1ff, ptr(a[1]), a[2], a[3]);
        const s = tmp.readCString();
        const c = out.readU32();
        if (c + s.length + 1 <= 0x1000) { out.add(c).writeUtf8String(s); out.writeU32(c + s.length); }
        return r >>> 0;
      }, function (a) { const w = CI_WORDS[a[1]]; return 2 + (w === undefined ? 2 : w); });
      calleeStand(fx, 2, 0x004a609f, 1, 'default', function (n, a) { return (a[0] ^ 0xc105e) >>> 0; });
      return fx;
    };
  });

  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
