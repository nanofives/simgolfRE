// Fixtures for batch c3eg (round 29, 2026-10-12): golf_clean.exe window plumbing. Reimplementations: shim/src/re/c3eg.cpp.
// Registry: re/frida/registry.d/c3eg.py. Purposes and per-jcc tables: log/c3/c3eg_purpose.md.
//
// Everything a vector touches is OURS (one arena per fixture call, the state regions of the keys). Keys that read live
// globals freeze the game's first thread (re/frida/js/freeze.js), point the globals at our objects and let the flush restore
// them. Machine-code recording stubs (c3dq's builder) sit in a private rwx page: no JavaScript runs inside a vector.
// Per-vector variation of live globals uses c3dr's getter trick: diff_hook resolves a "$rN" argument at each vector BEFORE the
// state snapshot, so the property getter of fx.rN sets that row's globals.
//
// This file also holds the batch's user32 preset for the import stand-in (user32Preset): ShowCursor, ClipCursor,
// GetForegroundWindow, SetCapture, ReleaseCapture recorded with their arguments (ClipCursor with the RECT it points at),
// IntersectRect and UnionRect forwarded (pure) with the destination RECT recorded.
(function () {
  const F = {};
  function u32(v) { v >>>= 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; }
  function alloc(n) {
    const p = __keepAlloc(n);
    for (let i = 0; i < n; i += 4) p.add(i).writeU32(0);
    return p;
  }
  let frozen = false;
  function freezeNow() {
    if (frozen) return;
    __freeze.thread();
    frozen = true;
    __freeze.atFlush(function () { frozen = false; });
    __freeze.negOn();   // the negative-control build keeps its wrong bodies dormant until the thread is frozen
  }
  function codeArena() {
    const size = Process.pageSize * 16;
    const base = __keepAlloc(size);
    Memory.protect(base, size, 'rwx');
    let off = 0;
    return function (bytes) {
      const p = base.add(off);
      p.writeByteArray(bytes);
      off += (bytes.length + 15) & ~15;
      if (off > size) throw new Error('c3eg: code arena full');
      return p;
    };
  }
  function newRec(size) {
    const rec = alloc(size || 0x2000);
    rec.writeU32(0x10);
    return rec;
  }
  // Recording stub with argument kinds (c3da's builder): 'v' = value, 'r' = the four dwords behind a pointer, 's' = skipped.
  // ret: undefined -> cursor ^ tag, a number -> that constant. thiscall: ecx is recorded.
  function stubK(rec, tag, kinds, ret, noEcx, cdecl) {
    const r = u32(rec.toUInt32());
    const b = [0x53];                                                  // push ebx
    b.push(0x8B, 0x1D, ...r);                                          // mov ebx, [REC]
    b.push(0x81, 0xC3, ...r);                                          // add ebx, REC
    b.push(0xC7, 0x03, ...u32(tag));                                   // mov dword [ebx], tag
    if (noEcx) b.push(0xC7, 0x43, 0x04, 0, 0, 0, 0);                   // stdcall slot: ecx is not an argument
    else b.push(0x89, 0x4B, 0x04);                                     // mov [ebx+4], ecx
    let off = 8;
    kinds.split('').forEach(function (k, i) {
      const sp = 8 + 4 * i;
      if (k === 'v') {
        b.push(0x8B, 0x44, 0x24, sp, 0x89, 0x43, off); off += 4;
      } else if (k === 'r') {
        b.push(0x8B, 0x54, 0x24, sp);                                  // mov edx,[esp+sp]
        for (let j = 0; j < 4; j++) { b.push(0x8B, 0x42, 4 * j, 0x89, 0x43, off); off += 4; }
      }
    });
    if (off > 0x7f) throw new Error('c3eg: entry too long');
    b.push(0x81, 0x05, ...r, ...u32(off));                             // add dword [REC], off
    if (ret === undefined) b.push(0xA1, ...r, 0x35, ...u32(tag));      // mov eax, [REC]; xor eax, tag
    else if (typeof ret === 'object') b.push(0xA1, ...u32(ret.mem.toUInt32()));   // mov eax, [cell] (a cell a row sets)
    else b.push(0xB8, ...u32(ret));                                    // mov eax, ret
    b.push(0x5B);                                                      // pop ebx
    const n = kinds.length;
    if (n > 0 && !cdecl) b.push(0xC2, 4 * n, 0x00); else b.push(0xC3);
    return b;
  }
  // A recording object: 0x100 slots; spec {offset: [kinds, ret]} overrides the default (returns 0, pops nothing).
  // defKinds: argument kinds of the default slot (a recording stub that pops them) instead of a bare `xor eax, eax; ret`
  function makeObj(emit, rec, spec, size, defKinds) {
    const nul = emit([0x33, 0xC0, 0xC3]);
    const vt = alloc(0x400);
    const def = defKinds === undefined ? nul : emit(stubK(rec, 0xC3E50000, defKinds));
    for (let i = 0; i < 0x100; i++) vt.add(i * 4).writePointer(def);
    Object.keys(spec || {}).forEach(function (o) {
      const s = spec[o];
      if (s.bytes) vt.add(+o).writePointer(emit(s.bytes));
      else vt.add(+o).writePointer(emit(stubK(rec, 0xC3E60000 + (+o), s[0], s[1])));
    });
    const obj = alloc(size || 0x40);
    obj.writePointer(vt);
    return obj;
  }
  // slot code that copies the RECT at `rr` into the RECT* argument (thiscall, one stack argument): eax = rr afterwards
  function copyRectSlot(rr) {
    const b = [0x8B, 0x54, 0x24, 0x04, 0xB8, ...u32(rr.toUInt32())];    // mov edx,[esp+4]; mov eax, rr
    for (let j = 0; j < 4; j++) b.push(0x8B, 0x48, 4 * j, 0x89, 0x4A, 4 * j);   // mov ecx,[eax+4j]; mov [edx+4j],ecx
    b.push(0xC2, 0x04, 0x00);                                          // ret 4
    return b;
  }
  function putRect(p, r) { r.forEach(function (v, i) { p.add(4 * i).writeS32(v); }); }
  function zero(p, n) { p.writeByteArray(new Array(n).fill(0)); }

  // ---- row values: a numeric key writes a dword, 'b<addr>' a byte (the key is the address as a decimal string)
  function putRow(row) {
    Object.keys(row).forEach(function (a) {
      if (a.charAt(0) === 'b') ptr(+a.slice(1)).writeU8(row[a] & 0xff); else ptr(+a).writeU32(row[a] >>> 0);
    });
  }
  function saveRows(rows) {
    const seen = {};
    rows.forEach(function (row) {
      Object.keys(row).forEach(function (a) {
        if (seen[a]) return; seen[a] = 1;
        __freeze.save(ptr(a.charAt(0) === 'b' ? +a.slice(1) : +a), a.charAt(0) === 'b' ? 1 : 4);
      });
    });
  }
  function rowsOn(fx, rows, rets) {
    saveRows(rows);
    rows.forEach(function (row, i) {
      Object.defineProperty(fx, 'r' + i, { enumerable: true, configurable: true, get: function () { putRow(row); return rets ? rets[i] : 0; } });
    });
  }

  // ---- a function of the exe replaced for one key on the A/B thread only (c3dw's calleeStand): the call is appended to the
  // call log fx.clog (word 0 = cursor, entries {va, count, args...} from +0x50; word 1 + k = the call count of stub k); other
  // threads reach the real function. deref {argIndex: words}: the pointed-to words are logged instead of the pointer.
  const standLive = {};
  let standTok = 1;
  const CLOG = 0x400;
  function newClog(n) { const l = alloc(n || CLOG); l.writeU32(0x50); return l; }
  function calleeStand(fx, k, va, argc, abi, ret, deref) {
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
      const a = Array.prototype.slice.call(arguments);
      const raw = a.slice();
      const cell = log.add(4 + 4 * k);
      const n = cell.readU32();
      cell.writeU32(n + 1);
      const vals = a.slice();
      if (deref) Object.keys(deref).forEach(function (ix) {
        vals[ix] = 0xfeedf00d;
        const d = deref[ix];
        if (typeof d === 'object') {                       // {str: words}: the C string up to its NUL, zero padded
          const p = ptr(raw[ix]), words = new Array(d.str).fill(0);
          let end = false;
          for (let k = 0; k < 4 * d.str; k++) {
            const c = end ? 0 : p.add(k).readU8();
            if (c === 0) end = true;
            words[k >> 2] |= c << (8 * (k & 3));
          }
          words.forEach(function (x) { vals.push(x >>> 0); });
        } else for (let w = 0; w < d; w++) vals.push(ptr(raw[ix]).add(4 * w).readU32());
      });
      const cur = log.readU32();
      const need = 8 + 4 * vals.length;
      if (cur + need <= (fx.clogCap || CLOG)) {
        log.add(cur).writeU32(va >>> 0);
        log.add(cur + 4).writeU32(vals.length);
        vals.forEach(function (v, i) { log.add(cur + 8 + 4 * i).writeU32(v >>> 0); });
        log.writeU32(cur + need);
      }
      let r;
      if (typeof ret === 'function') r = ret(n, a);
      else if (Array.isArray(ret)) r = ret[Math.min(n, ret.length - 1)];
      else r = ret === undefined ? 0 : ret;
      return r >>> 0;
    };
    const cb = new NativeCallback(fn, 'uint32', argt, abi);
    __diffKeepAlive.push(cb, orig);
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

  // ---- user32 preset for the import stand-in (golf_clean.exe's USER32 slots). `over` replaces a spec by name.
  // ClipCursor: the RECT behind its argument (or 0 for NULL) is what gets recorded. IntersectRect / UnionRect run the real
  // (pure) API and record the result and the destination RECT.
  function rectWords(p) {
    const q = ptr(p);
    if (q.isNull()) return [0xdead0000];
    return [q.readU32(), q.add(4).readU32(), q.add(8).readU32(), q.add(12).readU32()];
  }
  function user32Preset(over) {
    const imp = {
      'USER32.dll!ShowCursor': { args: 1, ret: [1, 0, 2, -1] },
      'USER32.dll!ClipCursor': { args: 1, impl: function (a, ctx) { ctx.record(rectWords(a[0])); return ctx.call + 1; } },
      'USER32.dll!GetForegroundWindow': { args: 0, ret: [0x5151, 0x7777] },
      'USER32.dll!SetCapture': { args: 1, ret: 0x1111 },
      'USER32.dll!ReleaseCapture': { args: 0, ret: 1 },
      'USER32.dll!IntersectRect': { args: 3, impl: function (a, ctx) { const r = ctx.orig(a[0], a[1], a[2]); ctx.record([r].concat(rectWords(a[0]))); return r; } },
      'USER32.dll!UnionRect': { args: 3, impl: function (a, ctx) { const r = ctx.orig(a[0], a[1], a[2]); ctx.record([r].concat(rectWords(a[0]))); return r; } },
    };
    Object.keys(over || {}).forEach(function (k) {
      if (over[k] === null) delete imp['USER32.dll!' + k]; else imp['USER32.dll!' + k] = over[k];
    });
    return imp;
  }
  function importsOn(fx, names, over) {
    const all = user32Preset(over);
    const imp = {};
    names.forEach(function (n) { imp['USER32.dll!' + n] = all['USER32.dll!' + n]; });
    fx.implog = __importStandIn({ module: 'golf_clean.exe', imports: imp }).log;
  }

  // ====================================================================================================================
  // Windows: 0x300-byte blocks. Fields read by the chain (from the instructions of the functions and of the original
  // callees View::toParent 0x0047b170 and Window::visible 0x004801f0): +0x9c flag word (0x20: offsets accumulate through the
  // parent, 0x8000: the parent's origin is subtracted again), +0xa0 flags (bit 0 visible), +0x130 parent, +0x1ac / +0x1b0 the
  // RECT origin, +0x1bc / +0x1c0 the client origin, +0x278 the surface handle (slot +0xd0 writes the window's size RECT).
  // ====================================================================================================================
  const WSIZE = 0x300;
  //        f9c     fa0 parent A (+0x1ac)           B (+0x1bc)          handle RECT
  const WSPEC = [
    [0x0000, 1, -1, [10, 20, 310, 220], [5, 6, 305, 206], [0, 0, 300, 200]],
    [0x0020, 1, 0, [20, 30, 120, 130], [3, 4, 103, 104], [0, 0, 100, 100]],
    [0x0000, 0, -1, [40, 50, 140, 150], [1, 2, 101, 102], [0, 0, 60, 60]],
    [0x0000, 1, 2, [60, 70, 160, 170], [7, 8, 107, 108], [2, 1, 72, 51]],
    [0x0020, 1, 1, [80, 90, 180, 190], [9, 1, 109, 101], [4, 6, 54, 46]],
    [0x8020, 1, 0, [11, 12, 51, 62], [2, 3, 52, 63], [0, 0, 20, 10]],
    [0x0020, 1, -1, [-7, -9, 33, 41], [4, 5, 44, 55], [-3, -4, 5, 6]],
    [0x8020, 3, 1, [100, 110, 150, 160], [12, 13, 62, 63], [1, 1, 49, 49]],
    [0x0000, 5, 4, [15, 25, 95, 85], [6, 7, 86, 67], [0, 0, 80, 60]],
    [0x0020, 1, 5, [0, 0, 640, 480], [0, 0, 640, 480], [0, 0, 640, 480]],
    [0x8000, 1, 0, [30, 40, 90, 100], [8, 9, 68, 69], [5, 5, 25, 25]],
    [0x0020, 1, 9, [-20, -30, 20, 30], [-1, -2, 39, 58], [0, 0, 40, 60]],
  ];
  function buildWins(env) {
    const wins = alloc(WSPEC.length * WSIZE);
    env.wins = wins;
    WSPEC.forEach(function (sp, k) {
      const w = wins.add(k * WSIZE);
      env['w' + k] = w;
      w.add(0x9c).writeU32(sp[0]);
      w.add(0xa0).writeU32(sp[1]);
      putRect(w.add(0x1ac), sp[3]);
      putRect(w.add(0x1bc), sp[4]);
    });
    WSPEC.forEach(function (sp, k) {
      const w = env['w' + k];
      if (sp[2] >= 0) w.add(0x130).writePointer(env['w' + sp[2]]);
      const rr = alloc(0x10);
      putRect(rr, sp[5]);
      const h = makeObj(env.emit, env.rec, { 0xd0: { bytes: copyRectSlot(rr) } }, 0x40, 'v');
      w.add(0x278).writePointer(h);
    });
  }
  function winEnv() {
    const emit = codeArena();
    const rec = newRec(0x2000);
    const fx = { rec: rec, emit: emit, clog: newClog() };
    buildWins(fx);
    fx.obj = fx.wins;
    return fx;
  }
  // The live display [0x0083ad50] is swapped for ours under a frozen first thread (restored by the flush).
  function swapDisplay(fx, spec, defKinds) {
    freezeNow();
    fx.disp = makeObj(fx.emit, fx.rec, spec, 0x40, defKinds);
    __freeze.setU32(ptr('0x0083ad50'), __recDisplay.guard(fx.disp).toUInt32());
  }

  // ---- Window::invalidate 0x00480ce0: display slot +0x30 (RECT*) records the four dwords and returns cursor ^ tag
  F.c3eg_inv = function () {
    const fx = winEnv();
    swapDisplay(fx, { 0x30: ['r', undefined] }, 'v');
    return fx;
  };

  // ---- Surface_clip 0x00479a80: display slot +0x20 returns 0x5151; GetForegroundWindow answers 0x5151, 0x7777 alternately;
  // ClipCursor records the RECT behind 0x0083aa78
  function rectBlk(fx) {
    const R = [[2, 3, 40, 30], [-5, -6, 10, 12], [0, 0, 0, 0], [100, 100, 400, 300], [7, 8, 7, 8], [0x7fffffff, -1, 0, 5]];
    R.forEach(function (r, i) { const p = alloc(0x10); putRect(p, r); fx['q' + i] = p; });
  }
  F.c3eg_clip = function () {
    const fx = winEnv();
    rectBlk(fx);
    swapDisplay(fx, { 0x20: ['', 0x5151] });
    __freeze.save(ptr('0x0083aa78'), 0x10);
    __freeze.save(ptr('0x0083aa98'), 4);
    ptr('0x0083aa98').writeU32(0x13572468);
    importsOn(fx, ['GetForegroundWindow', 'ClipCursor'], {
      // the answer follows the window under test (stored at 0x0083aa98 by the function before it calls Window::visible)
      GetForegroundWindow: { args: 0, impl: function () {
        const k = ptr(0x0083aa98).readPointer().sub(fx.wins).toInt32() / WSIZE;
        return k % 2 === 0 ? 0x5151 : 0x7777;
      } },
    });
    return fx;
  };
  F.c3eg_clip_fg = function () {       // the foreground window never matches: the ClipCursor arm is not taken
    const fx = winEnv();
    rectBlk(fx);
    swapDisplay(fx, { 0x20: ['', 0x5151] });
    __freeze.save(ptr('0x0083aa78'), 0x10);
    __freeze.save(ptr('0x0083aa98'), 4);
    importsOn(fx, ['GetForegroundWindow', 'ClipCursor'], { GetForegroundWindow: { args: 0, ret: [0, 0x7777] } });
    return fx;
  };

  // ---- Window::layoutScrollbars 0x0047d570: Window::moveTo 0x0047b420 stood in (thiscall, this + 2 arguments)
  const LS = [
    // n   0x1c4 0x1bc 0x184 0x180  c158 c154 c150  A has a handle
    [6, 300, 4, 2, 3, 1, 1, 1, 1], [6, 300, 4, 2, 3, 1, 0, 0, 1], [6, 300, 4, 2, 3, 0, 1, 0, 0], [6, 300, 4, 2, 3, 0, 0, 1, 0],
    [6, 300, 4, 2, 3, 0, 0, 0, 0], [0, 100, 0, 0, 0, 1, 1, 1, 1], [-3, -50, 7, -2, 9, 1, 0, 1, 0], [0x1000, 0x7fff, 1, 1, 1, 0, 1, 1, 0],
    [5, 640, 10, 20, 30, 1, 1, 0, 1], [5, 640, 10, 20, 30, 1, 1, 0, 0], [9, 480, 0, 0, 0, 0, 1, 1, 0], [2, 20, 1, 1, 1, 1, 0, 0, 1],
  ];
  F.c3eg_lsb = function () {
    const emit = codeArena();
    const rec = newRec(0x2000);
    const fx = { rec: rec, emit: emit, clog: newClog() };
    const blk = alloc(LS.length * WSIZE);
    fx.wblk = blk;
    const kids = alloc(4 * WSIZE);
    fx.kids = kids;
    // the sub-object behind [child + 0x278]: slot +0xd8 returns 11 / -4 / 0 (per child A)
    const hs = [11, -4, 0, 25].map(function (v) { return makeObj(emit, rec, { 0xd8: { bytes: [0xB8, ...u32(v >>> 0), 0xC3] } }); });
    LS.forEach(function (sp, k) {
      const w = blk.add(k * WSIZE);
      fx['w' + k] = w;
      w.add(0x194).writeS32(sp[0]);
      w.add(0x1c4).writeS32(sp[1]);
      w.add(0x1bc).writeS32(sp[2]);
      w.add(0x184).writeS32(sp[3]);
      w.add(0x180).writeS32(sp[4]);
      const a = kids.add((k % 4) * WSIZE);
      if (sp[5]) {
        w.add(0x158).writePointer(a);
        if (sp[8]) a.add(0x278).writePointer(hs[k % 4]); else a.add(0x278).writePointer(ptr(0));
      }
      if (sp[6]) w.add(0x154).writePointer(kids.add(((k + 1) % 4) * WSIZE));
      if (sp[7]) w.add(0x150).writePointer(kids.add(((k + 2) % 4) * WSIZE));
    });
    fx.obj = blk;
    // moveTo's result changes with the call number (it is the last eax of the original)
    calleeStand(fx, 0, 0x0047b420, 3, 'thiscall', function (n) { return (0x61 + 3 * n) >>> 0; });
    return fx;
  };

  // ---- EditBox_setCursorChar 0x0047ab00: drawCursor 0x0047d130 stood in (cdecl, four arguments)
  F.c3eg_ecc = function () {
    const fx = { clog: newClog() };
    const blk = alloc(0x300);
    for (let i = 0x200; i < 0x220; i += 4) blk.add(i).writeU32(0xa5a5a500 + i);     // sentinel contents of +0x208 / +0x214 / +0x218
    fx.eblk = blk;
    fx.obj = blk;
    calleeStand(fx, 0, 0x0047d130, 4, 'default', function (n) { return (0x71 + n) >>> 0; });
    return fx;
  };

  // ---- addDirtyRect 0x0047d060: Surface_blitThunk 0x00475c60 (thiscall, this + 7 arguments) and drawTooltip 0x0047cdb0
  // (cdecl, RECT*: the RECT's words are logged) stood in; UnionRect forwarded
  const ADR = [
    // aa0 (cursor image) aa4  aa8  ab4  ab8   dirty rect before (l, t, r, b)        f
    [0, 5, 6, 7, 8, [0, 0, 0, 0], 0], [0, 5, 6, 7, 8, [1, 2, 3, 4], 1], [0x1234, 10, 20, 30, 40, [0, 0, 0, 0], 0],
    [0x1234, 10, 20, 30, 40, [0, 0, 0, 0], 1], [0x1234, 10, 20, 30, 40, [50, 60, 90, 100], 1], [0x1234, 10, 20, 30, 40, [50, 60, 90, 100], 0],
    [0x1234, 0, 0, 0, 0, [0, 0, 0, 0], 1], [0x1234, 0, 0, 0, 0, [5, 5, 15, 15], 1], [1, -8, -9, 20, 21, [-20, -20, 0, 0], 7],
    [0x80000000, 100, 120, 5, 6, [90, 110, 95, 118], -1], [2, 300, 200, 64, 48, [250, 180, 300, 200], 1], [0xffffffff, 7, 7, 1, 1, [7, 7, 8, 8], 0x10],
  ];
  F.c3eg_adr = function () {
    const fx = { clog: newClog() };
    freezeNow();
    __freeze.save(ptr('0x0083a4d8'), 0x10);
    __freeze.save(ptr('0x0083aaa0'), 0x30);
    const rows = ADR.map(function (r) {
      const o = { 0x0083aaa0: r[0], 0x0083aaa4: r[1], 0x0083aaa8: r[2], 0x0083aab4: r[3], 0x0083aab8: r[4] };
      for (let i = 0; i < 4; i++) o[0x0083a4d8 + 4 * i] = r[5][i];
      return o;
    });
    rowsOn(fx, rows, ADR.map(function (r) { return r[6]; }));
    calleeStand(fx, 0, 0x00475c60, 8, 'thiscall', function (n) { return (0x41 + n) >>> 0; });
    calleeStand(fx, 1, 0x0047cdb0, 1, 'default', function (n, a) {   // the result follows the RECT it was handed
      const p = ptr(a[0]);
      return (0x51 + p.readU32() + 3 * p.add(4).readU32() + 5 * p.add(8).readU32() + 7 * p.add(12).readU32() + 11 * ptr(0x0083aaa0).readU32()) >>> 0;
    }, { 0: 4 });
    importsOn(fx, ['UnionRect']);
    return fx;
  };

  // ====================================================================================================================
  // Handler environments: 0x500-byte windows with a shared recording vtable whose result slots read a CELL (a dword the row
  // sets), exe callees stood in by calleeStand, the globals of the row saved by the freeze helper.
  // ====================================================================================================================
  const HW = 0x500;
  function hEnv(nWin) {
    const emit = codeArena();
    const rec = newRec(0x2000);
    const fx = { rec: rec, emit: emit, clog: newClog() };
    fx.cells = alloc(0x100);
    fx.cell = function (k) { return fx.cells.toUInt32() + 4 * k; };
    fx.wblk = alloc(nWin * HW);
    fx.W = function (i) { return fx.wblk.add(i * HW); };
    fx.A = function (i, off) { return fx.W(i).toUInt32() + off; };
    fx.obj = fx.wblk;
    return fx;
  }
  // rows [{w: windowIndex, set: {address: value}}]: the getter fx.t<i> applies the row (before the state snapshot) and returns
  // the row's window (the vector's `this`)
  function tRows(fx, rows) {
    saveRows(rows.map(function (r) { return r.set; }));
    rows.forEach(function (r, i) {
      Object.defineProperty(fx, 't' + i, { enumerable: true, configurable: true, get: function () { putRow(r.set); return r.ret !== undefined ? r.ret : fx.W(r.w); } });
    });
  }
  const KB = function (addr) { return 'b' + addr; };

  // ---- Window::onPressHot 0x0047f010: windows W0 (parent W1), W1, W2 (no parent). Cells: 0 slot +0x11c, 1 slot +0x15c,
  // 2 slot +0x160, 3 HotList::hitTest result, 4 its index, 5 Window::click result.
  //        w  f9c    c0 c1 c2 hit idx t388 t428 click
  const PRESS = [
    [0, 0x000, 1, 0, 0, 0, 2, 7, 0, 0x31],     // slot +0x15c answers 0: returns 0
    [0, 0x000, 1, 5, 0, 0, 2, 7, 0, 0x32],     // slot +0x15c answers 5: falls into click
    [0, 0x000, 1, 0, 0, 3, 1, -1, 9, 0x33],    // table word -1: slot +0x160 answers 0
    [0, 0x000, 1, 0, 8, 3, 1, -1, 9, 0x34],    // slot +0x160 answers 8: click
    [0, 0x000, 1, 4, 4, -1, 0, 0, 0, 0x35],    // hit < 0: click
    [0, 0x000, 0, 4, 4, 0, 0, 0, 0, 0x36],     // slot +0x11c answers 0: click
    [0, 0x000, 1, 0, 0, 0, 3, 5, 0, 0x37],     // a0 = 0 in the registry: no mouseDispatch23c
    [0, 0x200, 1, 0, 0, 0, 3, 5, 0, 0x38],     // bit 0x200 of +0x9c: no mouseDispatch23c
    [2, 0x000, 1, 6, 0, 0, 0, 2, 0, 0x39],     // no parent
    [2, 0x100, 0, 0, 0, 0, 0, 0, 0, 0x3a],     // no parent, 0x100 does not hide the dispatch
    [0, 0x000, 1, 1, 0, 2, 3, 0x7fffffff, 0, 0x3b],
    [2, 0x300, 1, 0, 3, 1, 2, -1, -7, 0x3c],
  ];
  function pressEnv() {
    const fx = hEnv(3);
    const o = makeObj(fx.emit, fx.rec, {
      0x12c: ['v', undefined], 0x130: ['v', undefined], 0x11c: ['', { mem: fx.cells.add(0) }], 0x15c: ['vv', { mem: fx.cells.add(4) }],
      0x160: ['vv', { mem: fx.cells.add(8) }] }, 0x40, '');
    const vt = o.readPointer();
    for (let i = 0; i < 3; i++) fx.W(i).writePointer(vt);
    fx.W(0).add(0x130).writePointer(fx.W(1));
    freezeNow();
    __freeze.save(ptr('0x0083ab40'), 0x10);
    tRows(fx, PRESS.map(function (r) {
      const set = {};
      set[fx.A(r[0], 0x9c)] = r[1];
      for (let k = 0; k < 6; k++) set[fx.cell(k)] = [r[2], r[3], r[4], r[5], r[6], r[9]][k];
      set[fx.A(r[0], 0x388 + 4 * r[6])] = r[7];
      set[fx.A(r[0], 0x428 + 4 * r[6])] = r[8];
      return { w: r[0], set: set };
    }));
    calleeStand(fx, 0, 0x0047e120, 1, 'thiscall', 0x61);
    calleeStand(fx, 1, 0x0047bf40, 4, 'thiscall', 0x62);
    calleeStand(fx, 2, 0x00492a90, 5, 'thiscall', function (n, a) {
      ptr(a[3]).writeU32(ptr(fx.cell(4)).readU32());
      return ptr(fx.cell(3)).readU32();
    }, { 3: 0 });
    calleeStand(fx, 3, 0x0047c1b0, 4, 'thiscall', function () { return (0x700 + ptr(fx.cell(5)).readU32()) >>> 0; });
    return fx;
  }
  F.c3eg_press = pressEnv;

  // ---- Window::onReleaseInner150 0x0047f0f0 / onReleaseInner158 0x0047f230: window rect +0x1ac {10, 20, 110, 70} (w 100, h 50),
  // +0xa0 bit 0 = visible; W1 is visible but its parent W2 is not. pointInRect 0x00492610 and Window::visible run ORIGINAL.
  //        w  h40 h44 vis(a0 of the window)   (x, y, a2 are the registry's)
  const REL = [
    [0, 0, -1, 1], [0, 0, -1, 1], [0, -1, 0, 1], [0, -1, 0, 1], [0, 1, 0, 1], [0, 0, -1, 0], [0, -1, 0, 0], [0, 1, -1, 0],
    [0, 0, -1, 1], [0, 0, -1, 1], [0, 0, -1, 1], [1, 1, -1, 1], [0, 0, -1, 1], [0, 2, -1, 1], [0, -1, -1, 1],
  ];
  function relEnv(stands) {
    const fx = hEnv(3);
    freezeNow();
    __freeze.save(ptr('0x0083ab40'), 8);
    const W = function (i) { return i < 0 ? 0 : fx.W(i).toUInt32(); };
    [0, 1, 2].forEach(function (i) {
      putRect(fx.W(i).add(0x1ac), [10, 20, 110, 70]);
      fx.W(i).add(0xa0).writeU32(1);
    });
    fx.W(1).add(0x130).writePointer(fx.W(2));
    fx.W(2).add(0xa0).writeU32(0);
    const rows = REL.map(function (r) {
      const set = {};
      set[0x0083ab40] = r[1] < 0 ? 0 : W(r[1]);
      set[0x0083ab44] = r[2] < 0 ? 0 : W(r[2]);
      set[fx.A(r[0], 0xa0)] = r[3];
      return { w: r[0], set: set };
    });
    // rows 1 and 3 hold W0 in the other word; the held window can also be a window that is not `this`
    tRows(fx, rows);
    stands.forEach(function (va, k) { calleeStand(fx, k, va, 4, 'thiscall', function (n, a) { return (0x80 + 7 * k + a[1] + 3 * a[2] + 5 * a[3]) >>> 0; }); });
    return fx;
  }
  F.c3eg_rel150 = function () { return relEnv([0x0047c0e0, 0x0047c290]); };
  F.c3eg_rel158 = function () { return relEnv([0x0047c360, 0x0047c500]); };

  // ---- Window::onKeyChar 0x0047ef30: the key ring 0x0083aba4..0x0083abad (ten bytes) with its pointer [0x004e42ec], the fixed
  // object 0x008400c8 (flags +0xa0, parent +0x130), [0x0083ab88]. The objects behind +0x26c / +0x270 answer slot +0x8c from the
  // cells 0 / 1. Window::detach 0x0047b820, Window::show 0x0047b670 and Window::key 0x0047c5d0 are stood in; matchInputCode
  // 0x0047eee0 and Window::visible 0x004801f0 run ORIGINAL.
  const RING = 0x0083aba4, RING_END = 0x0083abad;
  // [window, ring pointer, ring bytes {addr: byte}, visible(0x008400c8), cell0, cell1, [0x0083ab88]]
  const MD = { 'm': 0x6d, 'd': 0x64, 'e': 0x65, 'b': 0x62, 'u': 0x75 };
  const KEYS = [
    [0, 0x0083aba9, { 0x0083aba4: 0x6d, 0x0083aba5: 0x64, 0x0083aba6: 0x65, 0x0083aba7: 0x62, 0x0083aba8: 0x75 }, 1, 0, 0, 0x11],
    [0, 0x0083aba9, { 0x0083aba4: 0x6d, 0x0083aba5: 0x64, 0x0083aba6: 0x65, 0x0083aba7: 0x62, 0x0083aba8: 0x75 }, 0, 5, 0, 0x12],
    [0, 0x0083aba3 + 4, {}, 0, 0, 7, 0x13],
    [1, 0x0083aba5, {}, 1, 0, 0, 0x14],
    [2, 0x0083aba5, {}, 0, 3, 0, 0x15],
    [3, 0x0083aba5, {}, 0, 0, 0, 0x16],
    [0, 0x0083aba6, { 0x0083aba4: 0x62, 0x0083aba5: 0x75, 0x0083abab: 0x6d, 0x0083abac: 0x64, 0x0083abad: 0x65 }, 1, 0, 0, 0x17],
    [0, 0x0083abad, {}, 0, 0, 0, 0x18],
    [0, 0x0083abac, {}, 1, 0, 0, 0x19],
    [0, 0x0083abad, { 0x0083aba8: 0x6d, 0x0083aba9: 0x64, 0x0083abaa: 0x65, 0x0083abab: 0x62, 0x0083abac: 0x75 }, 0, 0, 0, 0x1a],
    [1, 0x0083aba6, {}, 0, 0, 4, 0x1b],
    [2, 0x0083aba6, {}, 1, 6, 0, 0x1c],
  ];
  F.c3eg_key = function () {
    const fx = hEnv(4);
    const o1 = makeObj(fx.emit, fx.rec, { 0x8c: ['vv', { mem: fx.cells.add(0) }], 0x90: ['vv', undefined] }, 0x40, '');
    const o2 = makeObj(fx.emit, fx.rec, { 0x8c: ['vv', { mem: fx.cells.add(4) }], 0x90: ['vv', undefined] }, 0x40, '');
    fx.W(0).add(0x26c).writePointer(o1); fx.W(0).add(0x270).writePointer(o2);
    fx.W(1).add(0x270).writePointer(o2);
    fx.W(2).add(0x26c).writePointer(o1);
    freezeNow();
    __freeze.save(ptr(RING), 0xc);
    __freeze.save(ptr('0x004e42ec'), 4);
    __freeze.save(ptr('0x008400c8').add(0xa0), 4);
    __freeze.save(ptr('0x008400c8').add(0x130), 4);
    __freeze.save(ptr('0x0083ab88'), 4);
    ptr('0x008400c8').add(0x130).writePointer(ptr(0));
    tRows(fx, KEYS.map(function (r) {
      const set = {};
      for (let a = RING; a <= RING_END; a++) set[KB(a)] = r[2][a] === undefined ? 0 : r[2][a];
      set[0x004e42ec] = r[1];
      set[0x008400c8 + 0xa0] = r[3];
      set[fx.cell(0)] = r[4];
      set[fx.cell(1)] = r[5];
      set[0x0083ab88] = r[6];
      return { w: r[0], set: set };
    }));
    calleeStand(fx, 0, 0x0047b820, 1, 'thiscall', 0x91);
    calleeStand(fx, 1, 0x0047b670, 2, 'thiscall', 0x92);
    calleeStand(fx, 2, 0x0047c5d0, 3, 'thiscall', function (n, a) { return (0x900 + (a[1] & 0xff) + a[2]) >>> 0; });
    return fx;
  };

  // ---- relayoutClip 0x0047e140: eight windows (a tree: W0 -> W1, W2, W3; W1 -> W4, W5; W3 -> W6; W5 -> W7), the object at
  // [0x0083a7bc] whose slot +0xd4 returns the RECT* in cell 0. View::offsetRectToLocal 0x0047b120, View::offsetRectToParent
  // 0x0047b0d0 and Window::visible 0x004801f0 run ORIGINAL; IntersectRect is forwarded (user32 preset).
  //        f9c    fa0 parent A                    B                      children
  const RL = [
    [0x0000, 1, -1, [10, 20, 310, 220], [5, 6, 305, 206], [1, 2, 3]],
    [0x0020, 1, 0, [20, 30, 120, 130], [3, 4, 103, 104], [4, 5]],
    [0x8000, 1, 0, [40, 50, 140, 150], [1, 2, 101, 102], []],
    [0x0000, 0, 0, [60, 70, 160, 170], [7, 8, 107, 108], [6]],
    [0x8020, 1, 1, [80, 90, 180, 190], [9, 1, 109, 101], []],
    [0x0000, 1, 1, [11, 12, 51, 62], [2, 3, 52, 63], [7]],
    [0x0020, 1, 3, [0, 0, 100, 100], [0, 0, 100, 100], []],
    [0x0000, 1, 5, [-5, -6, 35, 34], [3, 3, 43, 43], []],
  ];
  // row: [window, display object present, RECT of the slot (0 none, 1 = {1, 2, 300, 200}, 2 = {0, 0, 50, 50})]
  const RLROWS = [[0, 1, 1], [0, 1, 2], [1, 1, 1], [2, 1, 1], [3, 1, 1], [4, 1, 1], [5, 1, 1], [6, 1, 1], [7, 0, 0], [5, 1, 0], [0, 0, 0], [2, 1, 2]];
  F.c3eg_rl = function () {
    const fx = hEnv(RL.length);
    rectBlk(fx);
    const rects = [0, 1, 2].map(function (k) {
      const p = alloc(0x10);
      if (k === 1) putRect(p, [1, 2, 300, 200]); else if (k === 2) putRect(p, [0, 0, 50, 50]);
      return p;
    });
    const obj = makeObj(fx.emit, fx.rec, { 0xd4: ['', { mem: fx.cells.add(0) }], 0xd8: { bytes: [0xB8, ...u32(rects[2].toUInt32()), 0xC3] } }, 0x40, '');
    fx.dobj = obj;
    RL.forEach(function (sp, k) {
      const w = fx.W(k);
      w.add(0x9c).writeU32(sp[0]);
      w.add(0xa0).writeU32(sp[1]);
      if (sp[2] >= 0) w.add(0x130).writePointer(fx.W(sp[2]));
      putRect(w.add(0x1ac), sp[3]);
      putRect(w.add(0x1bc), sp[4]);
      for (let j = 0; j < 4; j++) w.add(0x1dc + 4 * j).writeS32(0x7000 + j);
      const arr = alloc(0x40);
      sp[5].forEach(function (c, i) { arr.add(4 * i).writePointer(fx.W(c)); });
      w.add(0x224).writePointer(arr);
      w.add(0x22c).writeS32(sp[5].length);
    });
    freezeNow();
    __freeze.save(ptr('0x0083a7bc'), 4);
    importsOn(fx, ['IntersectRect']);
    tRows(fx, RLROWS.map(function (r) {
      const set = {};
      set[0x0083a7bc] = r[1] ? obj.toUInt32() : 0;
      set[fx.cell(0)] = r[2] === 0 ? 0 : rects[r[2]].toUInt32();
      return { w: r[0], set: set };
    }));
    return fx;
  };

  // ---- redrawDrawList 0x0047f8e0: six windows (W0 visible + slot +0x13c, owner W1; W1 owner itself; W2 owner W3; W3 slot +0x13c,
  // owner itself; W4 invisible; W5 visible but its parent W4 is not). The draw list 0x00839ac0 / count 0x0083ab90 / focus
  // 0x0083aae0 / cursor image block 0x0083aaa0 are the live globals, saved. Window::visible runs ORIGINAL, IntersectRect is
  // forwarded, Surface_blitThunk 0x00475c60 is stood in.
  //        +0xa0 owner parent
  const RD = [[3, 1, -1], [1, 1, -1], [1, 3, -1], [3, 3, -1], [0, 0, -1], [1, 0, 4]];
  // row: [list, count, focus window (-1 none), rect (-1 NULL, else q index), cursor image, aa4, aa8, ab4, ab8]
  const RDROWS = [
    [[], 0, 1, 0, 0, 0, 0, 0, 0],
    [[0, 1, 2], 3, 1, 0, 0, 0, 0, 0, 0],
    [[0, 1, 2], 3, 1, 1, 0, 0, 0, 0, 0],
    [[0, 1, 2], 3, -1, -1, 0, 0, 0, 0, 0],
    [[0, 1, 2], 3, 1, 0, 0, 0, 0, 0, 0],
    [[0, 1, 2], -1, -1, 0, 0, 0, 0, 0, 0],
    [[0, 1, 2], -1, 1, 0, 0, 0, 0, 0, 0],
    [[3, 0, 1], 3, -1, -1, 0x1234, 10, 20, 30, 40],
    [[3, 0, 1], 3, -1, 0, 0x1234, 10, 20, 30, 40],
    [[3, 0, 1], 3, -1, 3, 0x1234, 10, 20, 30, 40],
    [[0, 1, 2, 3, 4, 5], 6, -1, 1, 0, 0, 0, 0, 0],
    [[2, 3], 2, 3, 0, 1, 5, 6, 7, 8],
    [[2, 3], 2, 3, 2, 1, 5, 6, 7, 8],
    [[0, 4, 5, 2], 4, -1, -1, 0x55, 1, 2, 300, 200],
  ];
  F.c3eg_rdl = function () {
    const fx = hEnv(RD.length);
    rectBlk(fx);
    const o = makeObj(fx.emit, fx.rec, { 0x13c: ['vv', undefined], 0x138: ['v', undefined], 0x134: ['v', undefined] }, 0x40, '');
    const vt = o.readPointer();
    RD.forEach(function (sp, k) {
      const w = fx.W(k);
      w.writePointer(vt);
      w.add(0xa0).writeU32(sp[0]);
      w.add(0xb0).writePointer(fx.W(sp[1]));
      if (sp[2] >= 0) w.add(0x130).writePointer(fx.W(sp[2]));
      fx['w' + k] = w;
    });
    freezeNow();
    __freeze.save(ptr('0x00839ac0'), 0x40);
    __freeze.save(ptr('0x0083ab90'), 4);
    __freeze.save(ptr('0x0083aae0'), 4);
    __freeze.save(ptr('0x0083aaa0'), 0x30);
    importsOn(fx, ['IntersectRect']);
    const rows = RDROWS.map(function (r) {
      const set = {};
      for (let i = 0; i < 16; i++) set[0x00839ac0 + 4 * i] = 0;
      r[0].forEach(function (wk, i) { set[0x00839ac0 + 4 * i] = fx.W(wk).toUInt32(); });
      set[0x0083ab90] = r[1];
      set[0x0083aae0] = r[2] < 0 ? 0 : fx.W(r[2]).toUInt32();
      set[0x0083aaa0] = r[4];
      set[0x0083aaa4] = r[5]; set[0x0083aaa8] = r[6]; set[0x0083aab4] = r[7]; set[0x0083aab8] = r[8];
      return { w: 0, set: set, ret: r[3] < 0 ? 0 : fx['q' + r[3]] };
    });
    tRows(fx, rows);
    calleeStand(fx, 0, 0x00475c60, 8, 'thiscall', function (n, a) {
      return (0x41 + a[2] + 3 * a[3] + 5 * a[4] + 7 * a[5] + 11 * a[6] + 13 * a[7]) >>> 0;
    });
    return fx;
  };

  // ---- Window::show 0x0047b670: Panel::notify 0x0047e120, rebuildDrawList 0x0047e450, Window::resized 0x0047bc60 and
  // Surface_clip 0x00479a80 are stood in (they act on the live window lists and the clip globals); View::offsetRectToLocal /
  // offsetRectToParent run ORIGINAL on the fixture windows. Slots: window +0x58, [+0x15c] +0x120, [[+0x13c] + 4] +0xf0 (one
  // argument), [+0xc] +0x1c, the display's +0x30 (RECT*). Rows: [window, +0xa0, +0x9c, +0x15c object, chain, +0xc object,
  // [0x0083aa98] is this, display present]; chain 0 = +0x140 zero, 1 = +0x138 zero, 2 = [+0x13c]+4 zero, 3 = complete.
  const SHOWROWS = [
    [0, 4, 0, 1, 3, 1, 1, 1], [0, 1, 0, 1, 3, 1, 1, 1], [1, 0, 0, 1, 3, 1, 1, 1], [1, 6, 2, 0, 0, 0, 0, 1],
    [2, 4, 0, 1, 2, 0, 0, 1], [2, 6, 0, 0, 3, 0, 0, 1], [3, 4, 0, 0, 1, 1, 0, 0], [3, 4, 2, 0, 0, 0, 0, 0],
    [4, 4, 2, 1, 3, 1, 0, 1], [4, 4, 0, 1, 0, 0, 0, 1], [5, 14, 0, 1, 3, 1, 1, 1], [5, 4, 0, 0, 1, 1, 1, 1],
    [1, 5, 0, 1, 3, 1, 1, 1],
  ];
  F.c3eg_show = function () {
    const fx = hEnv(6);
    const w58 = makeObj(fx.emit, fx.rec, { 0x58: ['', undefined] }, 0x40, '');
    const vt = w58.readPointer();
    const o15c = makeObj(fx.emit, fx.rec, { 0x120: ['', undefined] }, 0x40, '');
    const oF = makeObj(fx.emit, fx.rec, { 0xf0: ['v', undefined] }, 0x40, '');
    const oC = makeObj(fx.emit, fx.rec, { 0x1c: ['', undefined] }, 0x40, '');
    const chain = alloc(0x20); chain.add(4).writePointer(oF);
    const chain0 = alloc(0x20);
    for (let k = 0; k < 6; k++) {
      const w = fx.W(k);
      w.writePointer(vt);
      putRect(w.add(0x1ac), [10 + 10 * k, 20, 110 + 10 * k, 70 + k]);
      putRect(w.add(0x1bc), [5, 6 + k, 55 + 3 * k, 56]);
    }
    fx.disp = makeObj(fx.emit, fx.rec, { 0x30: ['r', undefined] }, 0x40, 'v');
    freezeNow();
    __freeze.save(ptr('0x0083ab40'), 8);
    __freeze.save(ptr('0x0083aa98'), 4);
    __freeze.save(ptr('0x0083ad50'), 4);
    tRows(fx, SHOWROWS.map(function (r) {
      const A = function (off) { return fx.A(r[0], off); };
      const set = {};
      set[A(0xa0)] = r[1];
      set[A(0x9c)] = r[2];
      set[A(0x220)] = 0;
      set[A(0x15c)] = r[3] ? o15c.toUInt32() : 0;
      set[A(0x140)] = r[4] === 0 ? 0 : 1;
      set[A(0x138)] = r[4] === 1 ? 0 : 1;
      set[A(0x13c)] = (r[4] === 2 ? chain0 : chain).toUInt32();
      set[A(0xc)] = r[5] ? oC.toUInt32() : 0;
      set[0x0083aa98] = r[6] ? fx.W(r[0]).toUInt32() : 0x12345678;
      set[0x0083ab40] = 0x77; set[0x0083ab44] = 0x78;
      set[0x0083ad50] = r[7] ? __recDisplay.guard(fx.disp).toUInt32() : 0;
      return { w: r[0], set: set };
    }));
    calleeStand(fx, 0, 0x0047e120, 1, 'thiscall', 0x61);
    calleeStand(fx, 1, 0x0047e450, 0, 'default', 0x62);
    calleeStand(fx, 2, 0x0047bc60, 3, 'thiscall', function (n, a) { return (0x70 + a[1] + a[2]) >>> 0; });
    calleeStand(fx, 3, 0x00479a80, 2, 'thiscall', 0x64);
    return fx;
  };

  // ---- Window::mouseEvent238 0x0047be40: windows with flags +0x9c / +0xa0, the cdecl callback at +0x238 (records (x, y)), the
  // objects behind +0x50 and +0x1c (slot +0x1c), own slots +0xa0 (x, y), +0x50 (a, b, rect*) and +0x6c (x, y) answering from the
  // cells (0, 1, 2). HotList::hitTestRect 0x00492b10 (writes a and b, answers cell 3), HotList::onMouseMove 0x00492cc0 and HotList::
  // cancelTip 0x00492d40 are stood in. Rows: [window, +0x9c, +0xa0, callback, +0x50 object, +0x1c object, c0 (0xa0), c1 (0x50),
  // c2 (0x6c), hit result, a, b]
  const ME = [
    [0, 0, 1, 1, 1, 1, 7, 0, 3, 0, 5, 6], [0, 0, 1, 1, 1, 1, 7, 9, 3, 0, 5, 6], [0, 0, 1, 1, 0, 0, 7, 0, 3, -1, 0, 0],
    [1, 0, 1, 1, 1, 1, 7, 0, 3, 2, 8, 9], [1, 0, 1, 1, 0, 1, 11, 0, 3, 2, 8, 9], [2, 0x200000, 1, 1, 1, 1, 7, 0, 3, 0, 1, 1],
    [2, 0, 8, 1, 1, 1, 7, 0, 3, 0, 1, 1], [3, 0, 1, 0, 0, 0, 7, 4, 3, 1, 2, 3], [3, 0, 1, 0, 0, 1, 7, 0, 3, -5, 2, 3],
    [0, 0x400000, 0, 1, 0, 0, 7, 0, 3, 0, 4, 4], [1, 0, 1, 0, 1, 0, 12, 0, 13, 0, 4, 4], [2, 0x100, 0x10, 1, 0, 1, 7, 0, 3, 0, 4, 4],
  ];
  F.c3eg_me = function () {
    const fx = hEnv(4);
    const oB = makeObj(fx.emit, fx.rec, { 0x1c: ['', undefined] }, 0x40, '');
    const o50 = makeObj(fx.emit, fx.rec, { 0x1c: ['', undefined] }, 0x40, '');
    const wo = makeObj(fx.emit, fx.rec, {
      0xa0: ['vv', { mem: fx.cells.add(0) }], 0x50: ['vvr', { mem: fx.cells.add(4) }], 0x6c: ['vv', { mem: fx.cells.add(8) }] }, 0x40, '');
    const vt = wo.readPointer();
    const cb = fx.emit(stubK(fx.rec, 0xCB230000, 'vv', undefined, true, true));
    for (let k = 0; k < 4; k++) fx.W(k).writePointer(vt);
    freezeNow();
    __freeze.save(ptr('0x0083ab2c'), 4);
    tRows(fx, ME.map(function (r) {
      const A = function (off) { return fx.A(r[0], off); };
      const set = {};
      set[A(0x9c)] = r[1];
      set[A(0xa0)] = r[2];
      set[A(0x238)] = r[3] ? cb.toUInt32() : 0;
      set[A(0x50)] = r[4] ? o50.toUInt32() : 0;
      set[A(0x1c)] = r[5] ? oB.toUInt32() : 0;
      set[fx.cell(0)] = r[6]; set[fx.cell(1)] = r[7]; set[fx.cell(2)] = r[8]; set[fx.cell(3)] = r[9];
      set[fx.cell(4)] = r[10]; set[fx.cell(5)] = r[11];
      return { w: r[0], set: set };
    }));
    calleeStand(fx, 0, 0x00492b10, 6, 'thiscall', function (n, a) {
      ptr(a[3]).writeU32(ptr(fx.cell(4)).readU32());
      ptr(a[4]).writeU32(ptr(fx.cell(5)).readU32());
      ptr(a[5]).writeU32(0x1111); ptr(a[5]).add(4).writeU32(0x2222); ptr(a[5]).add(8).writeU32(0x3333); ptr(a[5]).add(12).writeU32(0x4444);
      return ptr(fx.cell(3)).readU32();
    }, { 3: 0, 4: 0, 5: 0 });
    calleeStand(fx, 1, 0x00492cc0, 4, 'thiscall', function (n, a) { return (0x500 + a[1] + 3 * a[2]) >>> 0; });
    calleeStand(fx, 2, 0x00492d40, 1, 'thiscall', 0x93);
    return fx;
  };

  // ---- ListBox::onMouse 0x0048c6e0: list boxes are 0x800-byte blocks with a virtual-base table at +4 (vb[1] = 0x100 the window
  // part A, vb[2] = 0x300 the list-model part B). A4 = this + 0x104 holds the window vtable (slot +0x120) and its flags +0xa0
  // sit at this + 0x1a4; [A + 0xa0] (this + 0x1a0) is the mode byte; [A + 0x134] (this + 0x234) is also the parent window that
  // Window::visible follows. ListModel::selectedId 0x00489950, ListBox::measure 0x0048a120 (it writes the cell-0 value into
  // [this + 0x14]), ListBox::build 0x0048a5d0, setSelection 0x00489f50, Window::show 0x0047b670 and the four range/field
  // forwarders 0x0047ba30/70/50/90 are stood in; ReleaseCapture goes through the user32 preset; Window::visible runs ORIGINAL.
  // Row: [v14, measure writes, build result, word f4, mode byte, visible flags, callback, parent, parent object (+0x94), parent
  // visible flags, [B+0x60], [B+0xd4]]
  const LBM = [
    [5, 5, 0, 3, 0, 1, 1, 1, 1, 1, 4, 10], [5, 6, 0, -1, 8, 1, 1, 1, 1, 1, 4, 10], [5, 6, 7, 2, 0, 1, 1, 1, 1, 1, 4, 10],
    [5, 6, 0, 2, 4, 0, 1, 1, 1, 1, 3, 10], [5, 6, 0, -1, 0, 1, 0, 0, 0, 0, 4, 10], [5, 5, 0, 1, 12, 0, 0, 1, 0, 1, 4, 10],
    [5, 5, 0, 1, 4, 1, 1, 1, 1, 1, 5, 1000], [5, 6, 0, 0, 1, 0, 1, 1, 1, 1, 4, 10], [5, 5, 0, 7, 0x24, 1, 1, 1, 1, 1, -2, 9],
    [5, 6, 0, -1, 0, 1, 1, 1, 1, 0, 4, 10], [5, 5, 0, 4, 8, 1, 1, 0, 0, 0, 4, 10], [5, 6, 0, 5, 4, 1, 0, 1, 1, 1, 7, 100],
  ];
  const LBW = 0x800;
  F.c3eg_lbm = function () {
    const emit = codeArena();
    const rec = newRec(0x2000);
    const fx = { rec: rec, emit: emit, clog: newClog() };
    fx.cells = alloc(0x100);
    fx.cell = function (k) { return fx.cells.toUInt32() + 4 * k; };
    fx.lb = alloc(LBM.length * LBW);
    fx.pb = alloc(LBM.length * 0x200);
    fx.obj = fx.lb;
    const vb = alloc(0x10); vb.add(4).writeU32(0x100); vb.add(8).writeU32(0x300);
    const wo = makeObj(emit, rec, { 0x120: ['', undefined] }, 0x40, '');
    const po = makeObj(emit, rec, { 0x100: ['v', undefined], 0x104: ['v', undefined] }, 0x40, '');
    const so = makeObj(emit, rec, { 0x1c: ['', undefined] }, 0x40, '');
    const cb = emit(stubK(rec, 0xCB500000, 'v', undefined, true, true));
    LBM.forEach(function (r, k) {
      const t = fx.lb.add(k * LBW), p = fx.pb.add(k * 0x200);
      fx['t' + k + 'raw'] = t;
      t.add(4).writePointer(vb);
      t.add(0x104).writePointer(wo.readPointer());
      t.add(0x300 + 0x2c).writeU32(0x2c0 + k); t.add(0x300 + 0x30).writeU32(0x300 + k); t.add(0x300 + 0x5c).writeU32(0x5c0 + k);
      p.writePointer(po.readPointer());
      p.add(0x94).writePointer(r[8] ? so : ptr(0));
      p.add(0xa0).writeU32(r[9]);
      t.add(0x14).writeU32(r[0]);
      t.add(0x50).writePointer(r[6] ? cb : ptr(0));
      t.add(0x1a0).writeU32(r[4]);
      t.add(0x1a4).writeU32(r[5]);
      t.add(0x234).writePointer(r[7] ? p : ptr(0));
      t.add(0x300 + 0xf4).writeS32(r[3]);
      t.add(0x300 + 0x60).writeS32(r[10]);
      t.add(0x300 + 0xd4).writeS32(r[11]);
    });
    freezeNow();
    importsOn(fx, ['ReleaseCapture']);
    const rows = LBM.map(function (r, k) {
      const set = {};
      set[fx.cell(0)] = r[1]; set[fx.cell(1)] = r[2];
      return { w: 0, set: set, ret: fx.lb.add(k * LBW) };
    });
    tRows(fx, rows);
    calleeStand(fx, 0, 0x00489950, 1, 'thiscall', function (n) { return (0x40 + n) >>> 0; });
    calleeStand(fx, 1, 0x0048a120, 1, 'thiscall', function (n, a) {
      ptr(a[0]).add(0x14).writeU32(ptr(fx.cell(0)).readU32());
      return 0x71;
    });
    calleeStand(fx, 2, 0x0048a5d0, 5, 'thiscall', function () { return ptr(fx.cell(1)).readU32(); });
    calleeStand(fx, 3, 0x00489f50, 3, 'thiscall', 0x72);
    calleeStand(fx, 4, 0x0047b670, 2, 'thiscall', 0x73);
    calleeStand(fx, 5, 0x0047ba30, 3, 'thiscall', 0x74);
    calleeStand(fx, 6, 0x0047ba70, 2, 'thiscall', 0x75);
    calleeStand(fx, 7, 0x0047ba50, 3, 'thiscall', 0x76);
    calleeStand(fx, 8, 0x0047ba90, 2, 'thiscall', 0x77);
    return fx;
  };

  // ---- drawTooltip 0x0047cdb0: the text pointer [0x0083aac4] (writable strings of ours; '^' separates lines), the font record
  // [0x0083aac8] (+8, +0xc, +0x10), the box 0x0083a2c8 (left, top, right, bottom), the callback [0x0083aabc], the cursor image
  // block 0x0083aaa0.. and the object [0x0083a7bc] are live globals saved and set per row; the live display is swapped for a
  // recording one (slots +0x9c with 3 arguments, +0xa4 with 1) or removed. Stood in ($clog, 0x1000 bytes): Widget_fillRectR
  // 0x00475b00, Widget_drawBevelBoxR 0x004795d0, Widget_fillArea 0x00478b50, setDrawTarget 0x004762d0, Widget_setQuad 0x00476310,
  // drawCentered2 0x00477da0 (the piece is logged as text), drawCentered3 0x00477eb0, Image_draw 0x00473e60. The CRT strchr
  // 0x004a6170 runs ORIGINAL.
  const TIPTEXT = ['plain tip', 'ab^cde^f', 'x^', '^lead', '', 'one^two^three^four', 'ab^cde^f'];
  // row: [text index (-1 none), font (0/1), callback, rect (-1 NULL, else q index), image, aa4, aa8, display, object, box, e1, e0, fill]
  const TIPROWS = [
    [-1, 0, 0, -1, 0, 0, 0, 1, 0, [10, 20, 110, 40], 3, 4, 5], [-1, 0, 1, 0, 0x1234, 10, 20, 1, 1, [10, 20, 110, 40], 3, 4, 5],
    [0, 0, 0, 0, 0, 0, 0, 1, 1, [10, 20, 110, 40], 3, 4, 5], [1, 0, 0, -1, 0, 0, 0, 1, 0, [10, 20, 110, 80], 6, 7, 8],
    [2, 1, 0, 1, 0, 0, 0, 1, 1, [0, 0, 60, 30], 6, 7, 8], [3, 0, 0, 0, 0x77, 4, 5, 0, 1, [5, 6, 105, 66], 1, 2, 3],
    [4, 1, 0, -1, 0, 0, 0, 1, 0, [10, 20, 110, 40], 1, 2, 3], [6, 1, 1, -1, 0, 0, 0, 0, 0, [10, 20, 110, 40], 1, 2, 3],
    [0, 0, 0, 0, 0x99, 8, 9, 0, 0, [10, 20, 110, 40], 1, 2, 3], [5, 0, 0, 2, 0, 0, 0, 1, 1, [0, 0, 200, 100], 9, 8, 7],
    [-1, 0, 1, -1, 0, 0, 0, 0, 0, [10, 20, 110, 40], 1, 2, 3], [1, 1, 0, 0, 0x55, 1, 2, 1, 0, [30, 40, 130, 100], 2, 3, 4],
  ];
  F.c3eg_tip = function () {
    const fx = { rec: newRec(0x2000), emit: codeArena(), clogCap: 0x1000 };
    fx.clog = newClog(0x1000);
    rectBlk(fx);
    fx.txtblk = alloc(0x400);
    const txt = TIPTEXT.map(function (s, i) {
      const p = fx.txtblk.add(0x80 * i);
      for (let k = 0; k < s.length; k++) p.add(k).writeU8(s.charCodeAt(k));
      return p;
    });
    const fonts = [[-1, 14, 20], [3, 14, 20]].map(function (f) {
      const p = alloc(0x40); p.add(8).writeS32(f[0]); p.add(0xc).writeS32(f[1]); p.add(0x10).writeS32(f[2]); return p;
    });
    fx.fontblk = fonts[0];
    const cbk = fx.emit(stubK(fx.rec, 0xCB700000, '', undefined, true, true));
    const dobj = makeObj(fx.emit, fx.rec, { 0x9c: ['vvv', undefined], 0xa4: ['v', undefined], 0xa8: ['v', undefined] }, 0x40, '');
    const sobj = makeObj(fx.emit, fx.rec, { 0xc4: ['', undefined] }, 0x40, '');
    fx.obj = fx.txtblk;
    freezeNow();
    __freeze.save(ptr('0x0083aabc'), 4);
    __freeze.save(ptr('0x0083aac4'), 0x18);        // text, font, 0x83aacc.., 0x83aad8
    __freeze.save(ptr('0x0083aaa0'), 0x30);
    __freeze.save(ptr('0x0083a2c8'), 0x10);
    __freeze.save(ptr('0x0083a7bc'), 4);
    __freeze.save(ptr('0x0083ad50'), 4);
    __freeze.save(ptr('0x004e42c4'), 4);
    __freeze.save(ptr('0x004e42c0'), 4);
    const rows = TIPROWS.map(function (r) {
      const set = {};
      set[0x0083aabc] = r[2] ? cbk.toUInt32() : 0;
      set[0x0083aac4] = r[0] < 0 ? 0 : txt[r[0]].toUInt32();
      set[0x0083aac8] = fonts[r[1]].toUInt32();
      set[0x0083aad8] = r[12];
      set[0x0083aaa0] = r[4]; set[0x0083aaa4] = r[5]; set[0x0083aaa8] = r[6];
      set[0x0083ad50] = r[7] ? __recDisplay.guard(dobj).toUInt32() : 0;
      set[0x0083a7bc] = r[8] ? sobj.toUInt32() : 0;
      for (let i = 0; i < 4; i++) set[0x0083a2c8 + 4 * i] = r[9][i];
      set[0x004e42c4] = r[10]; set[0x004e42c0] = r[11];
      return { w: 0, set: set, ret: r[3] < 0 ? 0 : fx['q' + r[3]] };
    });
    tRows(fx, rows);
    fx.disp = dobj;
    calleeStand(fx, 0, 0x00475b00, 2, 'thiscall', 0x51, { 1: 4 });
    calleeStand(fx, 1, 0x004795d0, 4, 'thiscall', 0x52, { 1: 4 });
    calleeStand(fx, 2, 0x00478b50, 3, 'thiscall', 0x53, { 1: 4 });
    calleeStand(fx, 3, 0x004762d0, 5, 'thiscall', 0x54);
    calleeStand(fx, 4, 0x00476310, 5, 'thiscall', 0x55);
    calleeStand(fx, 5, 0x00477da0, 6, 'thiscall', function (n, a) { return (0x560 + a[3] + 3 * a[4] + 5 * a[5]) >>> 0; }, { 1: { str: 6 } });
    calleeStand(fx, 6, 0x00477eb0, 4, 'thiscall', 0x57, { 1: { str: 6 }, 2: 4 });
    calleeStand(fx, 7, 0x00473e60, 5, 'thiscall', function (n, a) { return (0x580 + a[0] + a[2] + a[3]) >>> 0; });
    return fx;
  };

  // ---- Window::dispatchAccel 0x0047c6c0: three windows (W0 top, W1 its focus child through [+0x13c] + 4, W2 the child's own
  // child). Per window: +0x9c, +0xa0, +0x140, +0x138, the objects at +0x26c / +0x270 (slot +0x8c answers a cell), the callback +0x260
  // (cdecl, answers a cell), the object +0x3c (slot +0x1c), own slot +0x90 (answers a cell). Window::key 0x0047c5d0 is stood in (it
  // answers the cell of the window it was called on); MapVirtualKeyA goes through the import stand-in: odd codes map to 0, even ones to
  // 0x41. The recursion enters the hook in both arms. Cells: window k uses 4k (+0x26c answer), 4k + 1 (+0x270), 4k + 2 (slot +0x90),
  // 16 + k (Window::key), 24 + k (callback).
  // W(f9c, a0, e140, e138, h26, h70, hcb, h3c, r26, r70, s90, keyr, cbr)
  const DW = function () { return Array.prototype.slice.call(arguments); };
  const E = [0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
  const mk = function (o) { const r = E.slice(); Object.keys(o).forEach(function (i) { r[+i] = o[i]; }); return r; };
  // index map: 0 f9c, 1 a0, 2 e140, 3 e138, 4 h26, 5 h70, 6 hcb, 7 h3c, 8 r26, 9 r70, 10 s90, 11 keyr, 12 cbr
  const DA = [
    [mk({ 10: 3 }), mk({}), mk({})], [mk({ 11: 5, 10: 3 }), mk({}), mk({})], [mk({ 10: 4 }), mk({}), mk({})],
    [mk({ 10: 2 }), mk({}), mk({})], [mk({ 4: 1, 11: 7, 10: 1 }), mk({}), mk({})], [mk({ 4: 1, 8: 9 }), mk({}), mk({})],
    [mk({ 5: 1, 11: 6, 10: 2 }), mk({}), mk({})], [mk({ 4: 1, 5: 1, 11: 8, 6: 1, 12: 100, 7: 1, 10: 3 }), mk({}), mk({})],
    [mk({ 5: 1, 9: 5 }), mk({}), mk({})], [mk({ 0: 0x200000 }), mk({}), mk({})], [mk({ 1: 8 }), mk({}), mk({})],
    [mk({ 2: 1, 3: 1, 10: 1 }), mk({}), mk({})], [mk({ 2: 1, 3: 1 }), mk({ 10: 5 }), mk({})],
    [mk({ 2: 1, 3: 1 }), mk({ 11: 3 }), mk({})], [mk({ 2: 1, 3: 1 }), mk({ 4: 1, 8: 4 }), mk({})],
    [mk({ 2: 1, 3: 1, 4: 1, 11: 9, 10: 6 }), mk({ 5: 1, 11: 2 }), mk({})],
    [mk({ 2: 1, 3: 1 }), mk({ 2: 1, 3: 1 }), mk({ 10: 7 })], [mk({ 4: 1, 5: 1, 11: 0, 10: 5 }), mk({}), mk({})],
    [mk({ 10: 1 }), mk({}), mk({})], [mk({ 10: 1 }), mk({}), mk({})], [mk({ 10: 1 }), mk({}), mk({})],
    [mk({ 2: 1, 3: 1 }), mk({ 4: 1, 8: 0, 11: 3 }), mk({})], [mk({ 2: 1, 3: 1 }), mk({ 4: 1, 8: 0, 11: 3 }), mk({})],
    [mk({ 2: 1, 3: 1 }), mk({ 5: 1, 9: 7 }), mk({})], [mk({ 2: 1, 3: 1 }), mk({ 4: 1, 11: 5, 5: 1 }), mk({})],
    [mk({ 2: 1, 3: 1 }), mk({ 5: 1 }), mk({})], [mk({ 2: 1, 3: 1 }), mk({}), mk({})],
    [mk({ 4: 1, 10: 2 }), mk({}), mk({})], [mk({ 5: 1, 10: 2 }), mk({}), mk({})],
    [mk({ 4: 1, 11: 2, 5: 1, 10: 3 }), mk({}), mk({})], [mk({ 4: 1, 10: 4 }), mk({}), mk({})],
  ];
  F.c3eg_acc = function () {
    const fx = hEnv(3);
    const set0 = {};
    fx.blk0 = alloc(0x20); fx.blk1 = alloc(0x20);
    const objs = [0, 1, 2].map(function (k) {
      const o26 = makeObj(fx.emit, fx.rec, { 0x8c: ['vv', { mem: fx.cells.add(16 * k) }] }, 0x40, '');
      const o70 = makeObj(fx.emit, fx.rec, { 0x8c: ['vv', { mem: fx.cells.add(16 * k + 4) }] }, 0x40, '');
      const o3c = makeObj(fx.emit, fx.rec, { 0x1c: ['', undefined] }, 0x40, '');
      const wv = makeObj(fx.emit, fx.rec, { 0x90: ['v', { mem: fx.cells.add(16 * k + 8) }], 0x94: ['v', undefined] }, 0x40, '');
      const cb = fx.emit(stubK(fx.rec, 0xAC000000 + k, 'v', { mem: fx.cells.add(0xe0 + 4 * k) }, true, true));
      fx.W(k).writePointer(wv.readPointer());
      return { o26: o26, o70: o70, o3c: o3c, cb: cb };
    });
    fx.blk0.add(4).writePointer(fx.W(1)); fx.blk1.add(4).writePointer(fx.W(2));
    freezeNow();
    __freeze.save(ptr('0x0083ab2c'), 4);
    const keyCell = function (k) { return fx.cells.toUInt32() + 0xc0 + 4 * k; };
    tRows(fx, DA.map(function (r) {
      const set = {};
      r.forEach(function (w, k) {
        const A = function (off) { return fx.A(k, off); };
        set[A(0x9c)] = w[0]; set[A(0xa0)] = w[1]; set[A(0x140)] = w[2]; set[A(0x138)] = w[3];
        set[A(0x13c)] = k === 0 ? fx.blk0.toUInt32() : k === 1 ? fx.blk1.toUInt32() : 0;
        set[A(0x26c)] = w[4] ? objs[k].o26.toUInt32() : 0;
        set[A(0x270)] = w[5] ? objs[k].o70.toUInt32() : 0;
        set[A(0x260)] = w[6] ? objs[k].cb.toUInt32() : 0;
        set[A(0x3c)] = w[7] ? objs[k].o3c.toUInt32() : 0;
        const cb0 = fx.cells.toUInt32();
        set[cb0 + 16 * k] = w[8]; set[cb0 + 16 * k + 4] = w[9]; set[cb0 + 16 * k + 8] = w[10];
        set[keyCell(k)] = w[11];
        set[fx.cells.toUInt32() + 0xe0 + 4 * k] = w[12];
      });
      return { w: 0, set: set };
    }));
    importsOn(fx, ['MapVirtualKeyA'], {
      MapVirtualKeyA: { args: 2, impl: function (a) { return (a[0].toUInt32() & 1) ? 0 : 0x41; } },
    });
    calleeStand(fx, 0, 0x0047c5d0, 3, 'thiscall', function (n, a) {
      const idx = Math.floor((a[0] - fx.wblk.toUInt32()) / HW);
      return ptr(keyCell(idx)).readU32();
    });
    return fx;
  };

  // ---- ListBox::typeAhead 0x0048aea0 (this = ListBox + 0x5c): one 0x800-byte list box per row (L; vb[1] = 0x100, vb[2] = 0x300, so
  // the window part A2 is L + 0x104, the parent pointer sits at L + 0x234 and the list-model part Bp at L + 0x35c). Fields: top
  // L + 0x28, vis L + 0x2c, callback L + 0x50, M Bp + 0, S Bp + 4, head Bp + 0x70, cursor node Bp + 0x74, N Bp + 0x78, index Bp + 0x7c,
  // sel Bp + 0x98. The list is a circular chain of N nodes (+8 item, +0xc next, +0x10 prev; the item's +4 is its text). Stood in
  // ($clog): 0x0047b9f0, 0x0047ba10, ListModel::selectedId 0x00489950, setSelection 0x00489f50, Window::invalidate 0x00480ce0.
  // MapVirtualKeyA answers the key itself for digits and letters and for space, else 0. CRT tolower 0x004a71d2 runs ORIGINAL.
  // Row: [N, S, M, vis, top, sel, key, callback, parent, parent object]
  const TA = [];
  const TA_KEYS = [];
  (function () {
    // arrow keys over a deterministic walk of geometries (seed (seed * 75 + 74) % 65537, rnd(n) = seed % n), then the type-ahead
    // rows below; the registry reaches each row's key through the getter $k<i>
    const keys = [0x25, 0x26, 0x27, 0x28, 0x64, 0x68, 0x66, 0x62];
    let seed = 12345;
    const rnd = function (n) { seed = (seed * 75 + 74) % 65537; return seed % n; };
    for (let i = 0; i < 200; i++) {
      const N = [1, 2, 5, 6, 8][rnd(5)], S = [1, 2, 3][rnd(3)], M = [1, 2, 3][rnd(3)];
      const vis = [1, 2, 3, 4, 9][rnd(5)];
      const top = rnd(Math.max(1, N));
      const sel = [0, 1, N - 1, rnd(N), -1, rnd(N)][rnd(6)];
      const key = keys[rnd(8)];
      TA.push([N, S, M, vis, top, sel, key, rnd(2), rnd(3) !== 0 ? 1 : 0, rnd(2)]);
      TA_KEYS.push(key);
    }
  })();
  const TA_CHAR = [
    [5, 1, 1, 3, 0, 0, 0x41, 0, 1, 1], [5, 1, 1, 3, 0, 2, 0x42, 1, 1, 1], [5, 1, 1, 3, 0, 4, 0x44, 0, 0, 0],
    [5, 1, 1, 3, 0, 1, 0x48, 1, 1, 0], [5, 1, 1, 3, 0, 3, 0x5a, 0, 1, 1], [5, 1, 1, 3, 0, 0, 0x69, 0, 1, 1],
    [5, 1, 1, 3, 0, 0, 0x29, 0, 1, 1], [5, 1, 1, 3, 0, 0, 0x20, 0, 1, 1], [8, 1, 1, 3, 0, -3, 0x47, 1, 1, 1],
    [8, 1, 1, 3, 0, -100, 0x46, 0, 1, 1], [8, 1, 1, 3, 0, 8, 0x45, 0, 1, 1], [8, 1, 1, 3, 0, 20, 0x43, 0, 1, 1],
    [8, 1, 1, 3, 0, 7, 0x41, 0, 1, 1], [8, 1, 1, 3, 0, -8, 0x48, 0, 1, 1], [8, 1, 1, 3, 0, 5, 0x31, 0, 1, 1],
    [1, 1, 1, 3, 0, 0, 0x41, 0, 1, 1], [3, 1, 1, 3, 0, 1, 0x61, 0, 1, 1], [8, 1, 1, 3, 0, 2, 0x44, 0, 1, 1],
    [8, 1, 1, 3, 0, -0x80000000, 0x44, 0, 1, 1], [0, 1, 1, 3, 0, 0, 0x41, 0, 1, 1], [5, 1, 1, 3, 0, 1, 0x6a, 0, 1, 1],
    [5, 1, 1, 3, 0, 3, 0x7a, 0, 1, 1],
  ];
  // targeted rows for sides the grid does not reach (UP with M == 1; DOWN with M == 1 and M != 1 at the end of the list; RIGHT)
  const TA_EXTRA = [
    [5, 1, 1, 2, 3, 2, 0x26, 0, 1, 1], [6, 1, 1, 2, 0, 2, 0x28, 0, 1, 1], [5, 3, 2, 2, 3, 4, 0x28, 0, 1, 1],
    [4, 2, 3, 2, 3, 5, 0x27, 0, 1, 1], [4, 2, 2, 2, 3, 5, 0x27, 0, 1, 1], [5, 2, 4, 2, 1, 5, 0x27, 0, 1, 1],
  ];
  TA_EXTRA.forEach(function (r) { TA_KEYS.push(r[6]); });
  TA_CHAR.forEach(function (r) { TA_KEYS.push(r[6]); });
  const TALL = TA.concat(TA_EXTRA).concat(TA_CHAR);
  const TASTR = ['apple', 'Banana', 'cherry', 'Date', 'elder', 'Fig', 'grape', 'Hat'];
  F.c3eg_ta = function () {
    const emit = codeArena();
    const rec = newRec(0x4000);
    const fx = { rec: rec, emit: emit, clog: newClog(0x1000), clogCap: 0x1000 };
    fx.lb = alloc(TALL.length * LBW);
    fx.pb = alloc(TALL.length * 0x200);
    fx.nodes = alloc(TALL.length * 8 * 0x20);
    fx.obj = fx.lb;
    const strs = alloc(0x200), items = alloc(8 * 0x10);
    TASTR.forEach(function (s, i) {
      const p = strs.add(0x20 * i);
      for (let k = 0; k < s.length; k++) p.add(k).writeU8(s.charCodeAt(k));
      items.add(0x10 * i + 4).writePointer(p);
    });
    const vb = alloc(0x10); vb.add(4).writeU32(0x100); vb.add(8).writeU32(0x300);
    const lvt = makeObj(emit, rec, { 0x0: ['v', undefined] }, 0x40, 'v');
    const wo = makeObj(emit, rec, { 0x120: ['', undefined] }, 0x40, '');
    const po = makeObj(emit, rec, { 0x100: ['v', undefined], 0x104: ['v', undefined] }, 0x40, '');
    const so = makeObj(emit, rec, { 0x1c: ['', undefined] }, 0x40, '');
    const cb = emit(stubK(rec, 0xCB5A0000, 'v', undefined, true, true));
    TALL.forEach(function (r, k) {
      const L = fx.lb.add(k * LBW), P = fx.pb.add(k * 0x200), ND = fx.nodes.add(k * 8 * 0x20);
      fx['l' + k] = L;
      L.writePointer(lvt.readPointer());
      L.add(4).writePointer(vb);
      L.add(0x104).writePointer(wo.readPointer());
      L.add(0x28).writeS32(r[4]); L.add(0x2c).writeS32(r[3]);
      L.add(0x50).writePointer(r[7] ? cb : ptr(0));
      L.add(0x234).writePointer(r[8] ? P : ptr(0));
      P.writePointer(po.readPointer());
      P.add(0x94).writePointer(r[9] ? so : ptr(0));
      P.add(0xa0).writeU32(1);
      const Bp = L.add(0x35c);
      Bp.writeS32(r[2]); Bp.add(4).writeS32(r[1]);
      Bp.add(0x78).writeS32(r[0]); Bp.add(0x98).writeS32(r[5]);
      for (let i = 0; i < r[0]; i++) {
        const n = ND.add(0x20 * i);
        n.add(8).writePointer(items.add(0x10 * (i % 8)));
        n.add(0xc).writePointer(ND.add(0x20 * ((i + 1) % r[0])));
        n.add(0x10).writePointer(ND.add(0x20 * ((i + r[0] - 1) % r[0])));
      }
      Bp.add(0x70).writePointer(r[0] > 0 ? ND : ptr(0));
      Bp.add(0x74).writePointer(ND.add(0x20 * 3 % Math.max(1, r[0] * 0x20)));
    });
    freezeNow();
    __freeze.save(ptr('0x0083ab2c'), 4);
    __freeze.save(ptr('0x0083b650'), 4);
    importsOn(fx, ['MapVirtualKeyA'], {
      MapVirtualKeyA: { args: 2, impl: function (a) {
        const c = a[0].toUInt32();
        return ((c >= 0x30 && c <= 0x5a) || c === 0x20) ? c : 0;
      } },
    });
    tRows(fx, TALL.map(function (r, k) { return { w: 0, set: {}, ret: fx.lb.add(k * LBW + 0x5c) }; }));
    TA_KEYS.forEach(function (key, k) { fx['k' + k] = key; });
    calleeStand(fx, 0, 0x0047b9f0, 2, 'thiscall', 0x91);
    calleeStand(fx, 1, 0x0047ba10, 2, 'thiscall', 0x92);
    calleeStand(fx, 2, 0x00489950, 1, 'thiscall', function (n) { return (0x40 + n) >>> 0; });
    calleeStand(fx, 3, 0x00489f50, 3, 'thiscall', 0x93);
    calleeStand(fx, 4, 0x00480ce0, 1, 'thiscall', 0x94);
    return fx;
  };

  // ---- drawCursor 0x0047d130 (w, a1, x, y): windows with +0x9c, +0x208 (image: +0x18 width, +0x1c height), +0x20c / +0x210 (hot
  // spot), +0x214 (cursor object whose +4 word is the handle), +0x218 (cursor id), slot +0x140 (px, py) answering cell 6. The live
  // display is ours (slot +0x2c writes the position cells 7 / 8 through its two pointers); the surface object [0x0083a504] has slots
  // +0xd8 / +0xdc answering cells 4 / 5. Stood in ($clog, 0x1000 bytes): hitTestScreen 0x0047f700 (answers the window pointer of
  // cell 1, writes px + 0x100 / py + 0x200), View::toGlobal 0x0047b2d0 (px += 0x100, py += 0x200), Surface_createFull 0x00474dd0,
  // Surface_blitThunk 0x00475c60, drawTooltip 0x0047cdb0 (the box is logged), addDirtyRect 0x0047d060 (my hook). User32 preset:
  // ShowCursor answers cell 2 on its first call and cell 3 on the following ones, LoadCursorA answers 0x1000 + id, SetCursor answers
  // its argument + 1, UnionRect is forwarded.
  const DCI = [{ w: 16, h: 16 }, { w: 32, h: 24 }, { w: 0, h: 0 }, { w: 0, h: 5 }];
  // row defaults, then the row's own fields: w (window index, -1: a0 = 0), hit (window the hit test finds, -1 none), g18, f9c,
  // img (0 none, 1, 2), cur (0 none, 1 = image 1, 2 = image 2), v140, sf / sn (ShowCursor first / next result), sw / sh (surface
  // object width / height, 0 = no object), hc (0 none, 1 handle 0x55, 2 +4 word 0), id (w+0x218), a1, x, y, disp, px / py (display)
  const DCR = [];
  (function () {
    const base = { w: 0, hit: -1, g18: 0, f9c: 0, img: 0, cur: 0, v140: 0, sf: -1, sn: -1, sn2: -9, sw: 0, sh: 0, hc: 0, id: 0, a1: 1,
                   x: 40, y: 50, disp: 1, px: 11, py: 12 };
    const R = function (o) { DCR.push(Object.assign({}, base, o)); };
    R({ img: 1, cur: 0, sf: -1, sw: 16, sh: 16, a1: 0 });
    R({ img: 1, cur: 0, sf: 0, sn: 0, sn2: -1, sw: 0, sh: 0, a1: 1 });
    R({ img: 1, cur: 1, sw: 16, sh: 16, a1: 1 });
    R({ img: 2, cur: 1, sw: 16, sh: 24, a1: 1 });
    R({ img: 2, cur: 0, sf: 1, sn: -1, sw: 32, sh: 8, a1: 0 });
    R({ img: 2, cur: 0, sf: -1, sw: 32, sh: 24, a1: 1 });
    R({ w: -1, hit: 1, img: 1, cur: 0, sw: 16, sh: 16, a1: 0, x: -1, y: -1, px: 70, py: 80 });
    R({ w: -1, hit: -1, x: -1, y: 7, a1: 0 });
    R({ w: -1, hit: 2, x: 5, y: -1, img: 2, cur: 2, sw: 32, sh: 24, a1: 1 });
    R({ w: -1, hit: 0, x: 3, y: 4, img: 0, cur: 0, id: 0x7f81 });
    [-2, -1, 0, 1, 2, 3, 4, 5, 6, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 - 2 + 2, 100, -5].forEach(function (r, i) {
      R({ w: i % 3, g18: i % 2, f9c: (i % 2) ? 0 : 0x4000000, v140: r, img: i % 2 });
    });
    R({ img: 0, cur: 0, hc: 0, id: 0 });
    R({ img: 0, cur: 1, sf: 1, sn: 1, hc: 0, id: 0x7f81 });
    R({ img: 0, cur: 2, sf: 0, sn: 1, hc: 1, id: 0x7f81 });
    R({ img: 0, cur: 1, sf: 1, hc: 2, id: 0x7f89 });
    R({ img: 0, cur: 1, sf: 0, sn: 0, sn2: 1, hc: 0, id: 0x7f88 });
    R({ w: 1, img: 0, cur: 0, hc: 1 });
    R({ img: 3, cur: 0, sf: -1, sw: 0, sh: 0, a1: 0 });     // a zero-width image and no surface object: both sizes read 0
    R({ img: 4, cur: 0, sf: -1, sw: 0, sh: 0, a1: 1 });     // zero width, height 5: the height differs
  })();
  F.c3eg_dc = function () {
    const emit = codeArena();
    const rec = newRec(0x2000);
    const fx = { rec: rec, emit: emit, clog: newClog(0x1000), clogCap: 0x1000 };
    fx.cells = alloc(0x100);
    fx.cell = function (k) { return fx.cells.toUInt32() + 4 * k; };
    fx.wblk = alloc(3 * HW);
    fx.W = function (i) { return fx.wblk.add(i * HW); };
    fx.A = function (i, off) { return fx.W(i).toUInt32() + off; };
    fx.obj = fx.wblk;
    const wv = makeObj(emit, rec, { 0x140: ['vv', { mem: fx.cells.add(24) }] }, 0x40, '');
    for (let i = 0; i < 3; i++) fx.W(i).writePointer(wv.readPointer());
    const imgs = DCI.map(function (d) { const p = alloc(0x40); p.add(0x18).writeS32(d.w); p.add(0x1c).writeS32(d.h); return p; });
    const hcs = [0, 1, 2].map(function (k) { const p = alloc(0x20); if (k === 1) p.add(4).writeU32(0x55); return p; });
    const surf = makeObj(emit, rec, { 0xd8: ['', { mem: fx.cells.add(16) }], 0xdc: ['', { mem: fx.cells.add(20) }] }, 0x40, '');
    const cx = fx.cells.toUInt32() + 28, cy = fx.cells.toUInt32() + 32;
    const posSlot = [0x8B, 0x54, 0x24, 0x04, 0xA1, ...u32(cx), 0x89, 0x02, 0x8B, 0x54, 0x24, 0x08, 0xA1, ...u32(cy), 0x89, 0x02, 0xC2, 0x08, 0x00];
    const disp = makeObj(emit, rec, { 0x2c: { bytes: posSlot }, 0x30: { bytes: [0x33, 0xC0, 0xC2, 0x08, 0x00] } }, 0x40, '');
    freezeNow();
    [['0x0083ab18', 4], ['0x0083aaa0', 0x30], ['0x0083a4d8', 0x10], ['0x0083ab4c', 4], ['0x0083aba0', 4], ['0x0083a504', 4],
     ['0x0083ad50', 4]].forEach(function (g) { __freeze.save(ptr(g[0]), g[1]); });
    const rows = DCR.map(function (r, i) {
      const set = {};
      const w = r.w < 0 ? -1 : r.w;
      set[0x0083ab18] = r.g18;
      set[0x0083aaa0] = r.cur === 0 ? 0 : imgs[r.cur - 1].toUInt32();
      set[0x0083ab4c] = 0x1234 + i;
      set[0x0083aba0] = 0;
      set[0x0083a504] = r.sw === 0 && r.sh === 0 ? 0 : surf.toUInt32();
      set[0x0083ad50] = r.disp ? __recDisplay.guard(disp).toUInt32() : 0;
      set[fx.cell(1)] = r.hit < 0 ? 0 : fx.W(r.hit).toUInt32();
      set[fx.cell(2)] = r.sf; set[fx.cell(3)] = r.sn; set[fx.cell(9)] = r.sn2 === -9 ? r.sn : r.sn2;
      set[fx.cell(4)] = r.sw; set[fx.cell(5)] = r.sh; set[fx.cell(6)] = r.v140;
      set[fx.cell(7)] = r.px; set[fx.cell(8)] = r.py;
      [0, 1, 2].forEach(function (k) {
        set[fx.A(k, 0x9c)] = k === Math.max(w, 0) ? r.f9c : 0;
        set[fx.A(k, 0x208)] = k === Math.max(w, 0) ? (r.img === 0 ? 0 : imgs[r.img - 1].toUInt32()) : 0;
        set[fx.A(k, 0x20c)] = 3 + k; set[fx.A(k, 0x210)] = 4 + k;
        set[fx.A(k, 0x214)] = k === Math.max(w, 0) && r.hc ? hcs[r.hc].toUInt32() : 0;
        set[fx.A(k, 0x218)] = k === Math.max(w, 0) ? r.id : 0;
      });
      // when the hit test finds a window other than the row's, give that window the row's fields too
      if (r.w < 0 && r.hit >= 0) {
        const k = r.hit;
        set[fx.A(k, 0x9c)] = r.f9c;
        set[fx.A(k, 0x208)] = r.img === 0 ? 0 : imgs[r.img - 1].toUInt32();
        set[fx.A(k, 0x214)] = r.hc ? hcs[r.hc].toUInt32() : 0;
        set[fx.A(k, 0x218)] = r.id;
      }
      return { w: Math.max(w, 0), set: set, ret: w < 0 ? 0 : fx.W(w) };
    });
    tRows(fx, rows);
    DCR.forEach(function (r, i) { fx['b' + i] = r.a1; fx['x' + i] = r.x; fx['y' + i] = r.y; });
    importsOn(fx, ['ShowCursor', 'UnionRect', 'LoadCursorA', 'SetCursor'], {
      ShowCursor: { args: 1, impl: function (a, ctx) { return ptr(fx.cell(ctx.call === 0 ? 2 : (ctx.call === 1 ? 3 : 9))).readS32(); } },
      LoadCursorA: { args: 2, impl: function (a) { return (0x1000 + a[1].toUInt32()) >>> 0; } },
      SetCursor: { args: 1, impl: function (a) { return (a[0].toUInt32() + 1) >>> 0; } },
    });
    calleeStand(fx, 0, 0x0047f700, 2, 'default', function (n, a) {
      ptr(a[0]).writeS32(ptr(a[0]).readS32() + 0x100); ptr(a[1]).writeS32(ptr(a[1]).readS32() + 0x200);
      return ptr(fx.cell(1)).readU32();
    }, { 0: 1, 1: 1 });      // the pointers are stack addresses: the words they point at (px, py) are logged instead
    calleeStand(fx, 1, 0x0047b2d0, 3, 'thiscall', function (n, a) {
      ptr(a[1]).writeS32(ptr(a[1]).readS32() + 0x100); ptr(a[2]).writeS32(ptr(a[2]).readS32() + 0x200);
      return 0x91;
    }, { 1: 1, 2: 1 });
    calleeStand(fx, 2, 0x00474dd0, 7, 'thiscall', function (n, a) { return (0x920 + a[1] + a[2]) >>> 0; });
    calleeStand(fx, 3, 0x00475c60, 8, 'thiscall', function (n, a) { return (0x930 + a[2] + a[3]) >>> 0; });
    calleeStand(fx, 4, 0x0047cdb0, 1, 'default', function () {
      ptr(fx.cell(10)).writeU32(ptr(0x0083ab4c).readU32());   // the flag word drawCursor sets around the call is observed
      return 0x94;
    }, { 0: 4 });
    calleeStand(fx, 5, 0x0047d060, 1, 'default', function (n, a) { return (0x950 + a[0]) >>> 0; });
    return fx;
  };

  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
