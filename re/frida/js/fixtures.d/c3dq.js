// Fixtures for C3 batch c3dq (round 27, 2026-10-11): golf_clean.exe drawing rows. Reimplementations:
// shim/src/re/c3dq.cpp. Registry: re/frida/registry.d/c3dq.py. Purposes and per-jcc coverage: log/c3/c3dq_purpose.md.
//
// Everything a vector touches is OURS (one arena per fixture call, the state region of the keys); nothing is linked into
// a window list, draw list or focus chain. Machine code stubs (a private rwx page) record into $rec, so no JavaScript
// runs inside a vector. Keys that read live globals freeze the game's first thread (re/frida/js/freeze.js), point the
// globals at our objects and let the flush restore them.
//
// Recording stub (c3da's builder): thiscall, n stack arguments, appends {tag, ecx, each argument} at the recorder
// cursor ($rec+0, entries from $rec+0x10) and returns (cursor ^ tag) or a constant.
(function () {
  function u32(v) { v >>>= 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; }
  function alloc(n) {
    const p = __keepAlloc(n);
    for (let i = 0; i < n; i += 4) p.add(i).writeU32(0);
    return p;
  }
  // The negative-control build (log/c3/alt/c3dq_negctl_gen.py) keeps its wrong bodies dormant until this flag page is set;
  // in the normal build the address is unmapped and the write is skipped.
  function negOn() {
    try {
      if (ptr('0x7A110000').add(4).readU32() === 0x5147454e) __freeze.setU32(ptr('0x7A110000'), 1);
    } catch (e) { /* normal build: no flag page */ }
  }
  // Freeze the game's first thread, then (only then) switch the negative-control flag on: the game calls several of these
  // functions on its own thread, so a wrong body must not be live while that thread still runs.
  function freezeNow() {
    __freeze.thread();
    negOn();
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
      if (off > size) throw new Error('c3dq: code arena full');
      return p;
    };
  }
  // n value arguments are recorded; ret: undefined -> cursor ^ tag, a number -> that constant
  function stubBytes(rec, tag, n, ret, body, skip) {
    const r = u32(rec.toUInt32());
    const b = [0x53];                                                  // push ebx
    b.push(0x8B, 0x1D, ...r);                                          // mov ebx, [REC]
    b.push(0x81, 0xC3, ...r);                                          // add ebx, REC
    b.push(0xC7, 0x03, ...u32(tag));                                   // mov dword [ebx], tag
    b.push(0x89, 0x4B, 0x04);                                          // mov [ebx+4], ecx
    let off = 8;
    for (let i = 0; !skip && i < n; i++) {
      b.push(0x8B, 0x44, 0x24, 8 + 4 * i, 0x89, 0x43, off); off += 4;  // mov eax,[esp+8+4i]; mov [ebx+off],eax
    }
    b.push(0x81, 0x05, ...r, ...u32(off));                             // add dword [REC], off
    if (body) b.push(...body);                                         // extra effect (argument at [esp+8+4i])
    if (ret === undefined) b.push(0xA1, ...r, 0x35, ...u32(tag));      // mov eax, [REC]; xor eax, tag
    else b.push(0xB8, ...u32(ret));                                    // mov eax, ret
    b.push(0x5B);                                                      // pop ebx
    if (n > 0) b.push(0xC2, 4 * n, 0x00); else b.push(0xC3);
    return b;
  }

  // A recording display: 0x100 slots; the slot offsets in ARGS take that many stack arguments (callee pops).
  const DISP_ARGS = { 0x88: 1, 0x94: 1 };
  function makeDisplay(emit, rec, retOverride, bodies, argsOverride, skipMap) {
    const dvt = alloc(0x400);
    for (let i = 0; i < 0x100; i++) {
      const off = i * 4;
      const ret = retOverride && retOverride[off] !== undefined ? retOverride[off] : undefined;
      const body = bodies && bodies[off] ? bodies[off] : undefined;
      const nargs = argsOverride && argsOverride[off] !== undefined ? argsOverride[off] : (DISP_ARGS[off] || 0);
      dvt.add(off).writePointer(emit(stubBytes(rec, 0xD15A0000 + off, nargs, ret, body, skipMap ? !!skipMap[off] : off === 0x38)));   // 0x38: stack pointer
    }
    const disp = alloc(0x40);
    disp.writePointer(dvt);
    disp.add(4).writeU32(0xD15D15D1);
    return disp;
  }

  const REC_SIZE = 0x2000;
  function newRec(size) {
    const rec = alloc(size || REC_SIZE);
    rec.writeU32(0x10);
    return rec;
  }

  // ---- screen / modal family -------------------------------------------------------------------------------
  // opts: disp (display present), b8 ('obj' | 'none' -> [0x008400b8]), b0 (value of [0x008400b0]), ret (slot overrides),
  // junk (0x0083ab40 / 0x0083ab44 start value). The movie list count [0x008406e4] is set to 0 so serviceMovie 0x0049c8e0
  // loops over nothing; [0x008400b0..0x008400c8] and the net object are ours.
  function buildScreen(opts) {
    const emit = codeArena();
    const rec = newRec();
    const fx = { rec: rec };
    fx.disp = makeDisplay(emit, rec, opts.ret);
    const net = alloc(0x400);
    net.add(0xec).writeU32(0xffffffff);                 // netServiceControl 0x0049ab40: timeGetTime - 2000 <= this+0xec
    fx.net = net;
    fx.obj = rec;
    freezeNow();
    __freeze.setU32(ptr('0x0083ad50'), opts.disp === false ? 0 : fx.disp.toUInt32());
    __freeze.save(ptr('0x008400b0'), 0x18);
    ptr('0x008400b8').writePointer(opts.b8 ? net : ptr(0));
    ptr('0x008400b0').writeU32((opts.b0 || 0) >>> 0);
    __freeze.setU32(ptr('0x008406e4'), 0);
    __freeze.save(ptr('0x0083ab40'), 8);
    ptr('0x0083ab40').writeU32((opts.junk === undefined ? 0x11112222 : opts.junk) >>> 0);
    ptr('0x0083ab44').writeU32((opts.junk === undefined ? 0x33334444 : opts.junk + 1) >>> 0);
    return fx;
  }

  // ---- objects with a display handle: freeGfxGlobal (the Surface at 0x00840710) and Surface::ctorVtbl483b90 -------
  // The Surface records are 0x28 bytes: +4 handle, +8 file name, +0xc cache, +0x10..+0x24 fields cleared by
  // Surface_free 0x00473ae0. +8 and +0xc stay 0 here (they are freed through the CRT heap).
  function buildSurfRec(opts) {
    const emit = codeArena();
    const rec = newRec();
    const fx = { rec: rec };
    fx.disp = makeDisplay(emit, rec);
    const objs = alloc(0x400);
    // object k at objs + k * 0x40: +4 handle, +8 .. +0x24 junk
    const spec = [[0xA1A1A1A1, 0x77], [0, 0x66], [0xB2B2B2B2, 0x55], [0xC3C3C3C3, 0], [0xD4D4D4D4, 0x1234]];
    spec.forEach(function (s, k) {
      const o = objs.add(k * 0x40);
      o.writeU32(0x53000000 + k);
      o.add(4).writeU32(s[0]);
      o.add(8).writeU32(opts.keep8 ? 0 : 0);
      o.add(0xc).writeU32(0);
      for (let j = 0x10; j <= 0x24; j += 4) o.add(j).writeU32((s[1] + j) >>> 0);
      fx['o' + k] = o;
    });
    fx.objs = objs;
    fx.obj = objs;
    freezeNow();
    __freeze.setU32(ptr('0x0083ad50'), fx.disp.toUInt32());
    if (opts.global) {
      __freeze.save(ptr('0x00840710'), 0x28);
      const g = ptr('0x00840710');
      g.writeU32(0x53ffffff);
      g.add(4).writeU32(opts.handle >>> 0);
      g.add(8).writeU32(0);
      g.add(0xc).writeU32(0);
      for (let j = 0x10; j <= 0x24; j += 4) g.add(j).writeU32((0x9100 + j) >>> 0);
    }
    return fx;
  }

  // ---- recording stub with argument kinds (c3da's builder): 'v' = value, 'r' = the four dwords behind a pointer, 's' =
  // skipped. ret: undefined -> cursor ^ tag, a number -> that constant. thiscall: ecx is recorded.
  function stubK(rec, tag, kinds, ret, noEcx) {
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
    if (off > 0x7f) throw new Error('c3dq: entry too long');
    b.push(0x81, 0x05, ...r, ...u32(off));                             // add dword [REC], off
    if (ret === undefined) b.push(0xA1, ...r, 0x35, ...u32(tag));      // mov eax, [REC]; xor eax, tag
    else b.push(0xB8, ...u32(ret));                                    // mov eax, ret
    b.push(0x5B);                                                      // pop ebx
    const n = kinds.length;
    if (n > 0) b.push(0xC2, 4 * n, 0x00); else b.push(0xC3);
    return b;
  }

  // ---- windows with a Surface handle: Window::paint / Window::paintBackground ----------------------------------
  // A handle is an object whose vtable has recording stubs: +0xd8 returns its width, +0xdc its height, +0x40 (src handle,
  // dest RECT*, src RECT*) records the rect contents, +0x44 (0, colour) and +0x5c (RECT*, colour) record. A window is a
  // 0x300-byte object with a vtable at +0 whose slot +0x11c returns 1 (parents P0) or 0 (P1); the window's Surface
  // sub-object is at +0x274 (+4 = the handle pointer, i.e. window + 0x278).
  function buildPaint(opts) {
    const emit = codeArena();
    const rec = newRec();
    const fx = { rec: rec };
    const nul = emit([0x33, 0xC0, 0xC3]);
    const mkHandle = function (W, H, id) {
      const vt = alloc(0x400);
      for (let i = 0; i < 0x100; i++) vt.add(i * 4).writePointer(nul);
      vt.add(0xd8).writePointer(emit([0xB8, ...u32(W), 0xC3]));
      vt.add(0xdc).writePointer(emit([0xB8, ...u32(H), 0xC3]));
      vt.add(0x40).writePointer(emit(stubK(rec, 0x4ac30040 + id * 0x1000, 'vrr')));
      vt.add(0x44).writePointer(emit(stubK(rec, 0x4ac30044 + id * 0x1000, 'vv')));
      vt.add(0x5c).writePointer(emit(stubK(rec, 0x4ac3005c + id * 0x1000, 'rv')));
      const h = alloc(0x20);
      h.writePointer(vt);
      h.add(4).writeU32(0x48000000 + id);
      return h;
    };
    const mkVt = function (ret11c) {
      const vt = alloc(0x400);
      for (let i = 0; i < 0x100; i++) vt.add(i * 4).writePointer(nul);
      vt.add(0x11c).writePointer(emit([0xB8, ...u32(ret11c), 0xC3]));
      return vt;
    };
    const vt1 = mkVt(1), vt0 = mkVt(0), vtN = mkVt(0x55);
    const mkWin = function (id, o) {
      const w = alloc(0x300);
      w.writePointer(o.vt || vt1);
      w.add(0x9c).writeU32(o.f9c >>> 0);
      if (o.parent) w.add(0x130).writePointer(o.parent);
      (o.A || [0, 0, 0, 0]).forEach(function (v, i) { w.add(0x1ac + 4 * i).writeS32(v); });
      (o.B || [0, 0, 0, 0]).forEach(function (v, i) { w.add(0x1bc + 4 * i).writeS32(v); });
      if (o.handle) w.add(0x278).writePointer(mkHandle(o.handle[0], o.handle[1], id));
      w.add(0x274).writeU32(0x53000000 + id);
      return w;
    };
    fx.p0 = mkWin(10, { f9c: 0, handle: [200, 150], vt: vt1 });     // parent whose slot +0x11c returns 1
    fx.p1 = mkWin(11, { f9c: 0, handle: [200, 150], vt: vt0 });     // parent whose slot +0x11c returns 0
    fx.pN = mkWin(12, { f9c: 0, handle: [200, 150], vt: vtN });     // parent whose slot +0x11c returns 0x55
    fx.w0 = mkWin(0, { f9c: 0x80000, parent: fx.p0, handle: [64, 48], A: [10, 20, 74, 68], B: [3, 4, 67, 52] });
    fx.w1 = mkWin(1, { f9c: 0x80000, parent: fx.p1, handle: [64, 48], A: [10, 20, 74, 68], B: [3, 4, 67, 52] });
    fx.w2 = mkWin(2, { f9c: 0x00000, parent: fx.p0, handle: [64, 48], A: [10, 20, 74, 68], B: [3, 4, 67, 52] });
    fx.w3 = mkWin(3, { f9c: 0x80000, handle: [64, 48], A: [10, 20, 74, 68], B: [3, 4, 67, 52] });
    fx.w4 = mkWin(4, { f9c: 0x80000, parent: fx.p0, A: [-7, 9, 0, 0], B: [100, -30, 0, 0] });
    fx.w5 = mkWin(5, { f9c: 0x80001, parent: fx.pN, handle: [1, 1], A: [0, 0, 1, 1], B: [0, 0, 1, 1] });
    fx.w6 = mkWin(6, { f9c: 0xfff7ffff, parent: fx.p0, handle: [0, 0], A: [5, 5, 5, 5], B: [-5, -5, 5, 5] });
    fx.w7 = mkWin(7, { f9c: 0x80000, parent: fx.p0, handle: [320, 240], A: [1, 2, 3, 4], B: [5, 6, 7, 8] });
    const rect = function (l, t, r, b) { const p = alloc(0x10); [l, t, r, b].forEach(function (v, i) { p.add(4 * i).writeS32(v); }); return p; };
    fx.pr0 = rect(2, 3, 40, 30); fx.pr1 = rect(0, 0, 0, 0); fx.pr2 = rect(-5, -6, 10, 12); fx.pr3 = rect(8, 9, 8, 9);
    fx.obj = fx.w0;
    freezeNow();
    __freeze.save(ptr('0x0083abe0'), 4);
    ptr('0x0083abe0').writeU32(opts.color >>> 0);
    return fx;
  }

  // ---- blitter, screen objects, primary surface -----------------------------------------------------------------
  // buildBlit: the blitter objects [0x0083c328] / [0x0083c32c] (vtables of recording stubs: slot +0x80 (obj, ctx) and
  // +0x4c / +8 (obj), all stdcall) and the context [0x0083c330]. opts: o328, o32c, ctx (booleans).
  function buildBlit(opts) {
    const emit = codeArena();
    const rec = newRec();
    const fx = { rec: rec };
    const vt = alloc(0x400);
    const nul = emit([0x33, 0xC0, 0xC2, 0x04, 0x00]);               // xor eax, eax; ret 4: the unused stdcall slots (a wrong body may call one)
    for (let i = 0; i < 0x100; i++) vt.add(i * 4).writePointer(nul);
    vt.add(0x80).writePointer(emit(stubK(rec, 0xB1170080, 'vv', opts.r80, true)));
    vt.add(0x4c).writePointer(emit(stubK(rec, 0xB117004C, 'v', undefined, true)));
    vt.add(0x08).writePointer(emit(stubK(rec, 0xB1170008, 'v', undefined, true)));
    const mk = function (id) { const o = alloc(0x20); o.writePointer(vt); o.add(4).writeU32(0xB1100000 + id); return o; };
    fx.a = mk(1); fx.b = mk(2); fx.ctx = alloc(0x10);
    fx.obj = fx.a;
    freezeNow();
    __freeze.save(ptr('0x0083c328'), 0x10);
    ptr('0x0083c328').writePointer(opts.o328 ? fx.a : ptr(0));
    ptr('0x0083c32c').writePointer(opts.o32c ? fx.b : ptr(0));
    ptr('0x0083c330').writePointer(opts.ctx ? fx.ctx : ptr(0));
    return fx;
  }

  // buildClr: the screen-object list [0x0083ac90] (head), [0x0083ac94] (scratch), [0x0083ac98] (count), [0x0083ac9c], and
  // the allocator object at 0x0083ac88 (only its vtable pointer is replaced; slot +4 records the payload). Nodes have
  // +8 payload and +0xc next. Every object's vptr points at VT0 (slot 0: scalar-deleting destructor stub recording
  // (ecx, flag); [vptr+4] = 0, the virtual base offset) except those built with vbase 8: their vptr points at VT8
  // ([vptr+4] = 8) and the word at +8 is a vptr to VT0, the way a class with a virtual base lays out.
  function buildClr(opts) {
    const emit = codeArena();
    const rec = newRec();
    const fx = { rec: rec };
    const nul = emit([0x33, 0xC0, 0xC3]);
    const dtor = emit(stubK(rec, 0xDE570000, 'v'));
    const vt0 = alloc(0x40), vt8 = alloc(0x40), vt16 = alloc(0x40);
    vt0.writePointer(dtor); vt0.add(4).writeU32(0);
    vt8.writePointer(nul); vt8.add(4).writeU32(8);
    vt16.writePointer(nul); vt16.add(4).writeU32(0x10);
    const mkObj = function (id, vb) {
      const o = alloc(0x20);
      if (vb) { o.writePointer(vt8); o.add(8).writePointer(vt0); } else { o.writePointer(vt0); }
      o.add(4).writeU32(0xC1000000 + id);
      return o;
    };
    const gvt = alloc(0x400);
    for (let i = 0; i < 0x100; i++) gvt.add(i * 4).writePointer(nul);
    gvt.add(4).writePointer(emit(stubK(rec, 0x6C000004, 'v')));
    fx.gvt = gvt;
    const nodes = alloc(0x100);
    fx.nodes = nodes;
    const n = [];
    for (let k = 0; k < 3; k++) {
      const nd = nodes.add(k * 0x30);
      const vb = opts.vb && opts.vb.indexOf(k) >= 0;
      if (vb) { nd.writePointer(vt16); nd.add(0x10).writePointer(vt0); }   // +8 stays the payload slot
      else nd.writePointer(vt0);
      n.push(nd);
    }
    // payload objects P0..P2
    const pl = (opts.payload || [0, 1, 2]).map(function (p) { return p < 0 ? ptr(0) : mkObj(p, p === 2); });
    for (let k = 0; k < 3; k++) {
      n[k].add(8).writePointer(pl[k] || ptr(0));
      n[k].add(0xc).writePointer(k + 1 < 3 ? n[k + 1] : ptr(0));
    }
    fx.n0 = n[0];
    fx.obj = nodes;
    freezeNow();
    __freeze.setU32(ptr('0x0083ad50'), opts.disp === false ? 0 : makeDisplay(emit, rec).toUInt32());
    __freeze.save(ptr('0x0083ac88'), 0x20);
    ptr('0x0083ac88').writePointer(gvt);
    ptr('0x0083ac90').writePointer(opts.head === false ? ptr(0) : n[0]);
    ptr('0x0083ac94').writeU32(0x6677);
    ptr('0x0083ac98').writeS32(opts.count);
    ptr('0x0083ac9c').writeU32(0x7766);
    return fx;
  }

  // buildPrim: setPrimarySurface. Display slot +0xb8 returns opts.b8, slot +0x74 (&surf->+4) returns a handle object
  // whose slot +8 (id, a, b) returns opts.r.
  function buildPrim(opts) {
    const emit = codeArena();
    const rec = newRec();
    const fx = { rec: rec };
    const nul = emit([0x33, 0xC0, 0xC3]);
    const hvt = alloc(0x400);
    for (let i = 0; i < 0x100; i++) hvt.add(i * 4).writePointer(nul);
    hvt.add(8).writePointer(emit(stubK(rec, 0x41D00008, 'vvv', opts.r)));
    const h = alloc(0x20);
    h.writePointer(hvt);
    const rets = {}; rets[0xb8] = opts.b8; rets[0x74] = h.toUInt32();
    const args = {}; args[0x74] = 1;
    const disp = makeDisplay(emit, rec, rets, undefined, args);
    const surfs = alloc(0x100);
    for (let k = 0; k < 4; k++) surfs.add(k * 0x40).writeU32(0x53000000 + k);
    surfs.add(1 * 0x40 + 4).writeU32(0x5A5A5A5A);      // s1 already has a handle
    surfs.add(2 * 0x40 + 4).writeU32(0);               // s2, s3 have none
    fx.s0 = surfs; fx.s1 = surfs.add(0x40); fx.s2 = surfs.add(0x80); fx.s3 = surfs.add(0xc0);
    fx.surfs = surfs;
    fx.obj = surfs;
    freezeNow();
    __freeze.setU32(ptr('0x0083ad50'), disp.toUInt32());
    __freeze.save(ptr('0x0083ad44'), 4);
    ptr('0x0083ad44').writeU32(0x4444);
    return fx;
  }

  // ---- tiled blits and the nine-patch compositor ---------------------------------------------------------------
  // The destination D is a 0x30-byte Surface record whose +4 handle hD has a vtable of recording stubs: slot +0xc8 (RECT*)
  // writes the clip rectangle opts.clip through its argument, slot +0x34 (RECT*) records the rectangle it is given
  // (the clip the tilers set and restore). Tile / piece images are 0x30-byte records: +4 handle, +8 = 0 (no file
  // name), +0x18 / +0x1c width / height read by blitRight and drawComposite, +0x20 / +0x24 the hot spot Image_draw adds;
  // their handles answer slots +0xd8 / +0xdc (the dimensions blitLeft / blitMid read) and record slot +0x38 (dest handle,
  // 0, 0, x, y, w, h, flag) and slot +0x44 (dest handle, x, y, mask handle). The record dimensions and the handle
  // dimensions differ on purpose where noted, so a body that reads the wrong source reads RED.
  // The display (present unless opts.disp === false) stores the triple of slot +0x9c at +0x10..+0x18 and answers slot +0xa0.
  function buildTiles(opts) {
    const emit = codeArena();
    const rec = newRec(0x10000);
    const fx = { rec: rec };
    const nul = emit([0x33, 0xC0, 0xC3]);
    const st = {}; st[0x9c] = [0, 1, 2].reduce(function (a, j) { return a.concat([0x8B, 0x44, 0x24, 8 + 4 * j, 0x89, 0x41, 0x10 + 4 * j]); }, []);
    st[0xa0] = [0, 1, 2].reduce(function (a, j) { return a.concat([0x8B, 0x54, 0x24, 8 + 4 * j, 0x8B, 0x41, 0x10 + 4 * j, 0x89, 0x02]); }, []);
    const dargs = {}; dargs[0x9c] = 3; dargs[0xa0] = 3;
    const dskip = {}; dskip[0xa0] = true;
    const disp = makeDisplay(emit, rec, undefined, st, dargs, dskip);
    disp.add(0x10).writeS32(2); disp.add(0x14).writeS32(3); disp.add(0x18).writeS32(2);
    fx.disp = disp;
    const arena = alloc(0x3000);
    fx.arena = arena;
    fx.obj = arena;
    let off = 0;
    const take = function (n) { const p = arena.add(off); off += (n + 15) & ~15; if (off > 0x3000) throw new Error('c3dq: tiles arena full'); return p; };
    const cl = opts.clip || [0, 0, 320, 240];
    const hvtD = alloc(0x400);
    for (let i = 0; i < 0x100; i++) hvtD.add(i * 4).writePointer(nul);
    hvtD.add(0xc8).writePointer(emit(stubBytes(rec, 0x4ac300c8, 1, 0, rectFill(cl[0], cl[1], cl[2], cl[3]), true)));
    hvtD.add(0x34).writePointer(emit(stubK(rec, 0x4ac30034, 'r')));
    const hD = alloc(0x20); hD.writePointer(hvtD); hD.add(4).writeU32(0x48440000);
    fx.D = take(0x30); fx.D.writeU32(0x53440000); fx.D.add(4).writePointer(hD);
    fx.D2 = take(0x30); fx.D2.writeU32(0x53440001);                       // no handle: blitLeft / Mid / Right return 7
    const mkHandle = function (hw, hh, id) {
      const vt = alloc(0x400);
      for (let i = 0; i < 0x100; i++) vt.add(i * 4).writePointer(nul);
      vt.add(0xd8).writePointer(emit([0xB8, ...u32(hw), 0xC3]));
      vt.add(0xdc).writePointer(emit([0xB8, ...u32(hh), 0xC3]));
      vt.add(0x38).writePointer(emit(stubK(rec, 0x4ac30038 + id * 0x10000, 'vvvvvvvv')));
      vt.add(0x40).writePointer(emit(stubK(rec, 0x4ac30040 + id * 0x10000, 'vrr')));   // Surface_blit3 (blitMid cells)
      vt.add(0x44).writePointer(emit(stubK(rec, 0x4ac30044 + id * 0x10000, 'vvvv')));
      const h = alloc(0x20); h.writePointer(vt); h.add(4).writeU32(0x48000000 + id);
      return h;
    };
    // image(id, handle dims | null, record dims, hot spot)
    const image = function (id, hd, rd, hs) {
      const im = take(0x30);
      im.writeU32(0x53000000 + id);
      if (hd) im.add(4).writePointer(mkHandle(hd[0], hd[1], id));
      im.add(0x18).writeS32(rd[0]); im.add(0x1c).writeS32(rd[1]);
      im.add(0x20).writeS32(hs[0]); im.add(0x24).writeS32(hs[1]);
      return im;
    };
    fx.T0 = image(1, [8, 6], [8, 6], [1, 2]);
    fx.T1 = image(2, [5, 7], [9, 4], [3, 1]);             // handle 5 x 7, record 9 x 4
    fx.T2 = image(3, [0, 0], [6, 6], [0, 0]);             // zero handle dimensions
    fx.T3 = image(4, null, [6, 6], [0, 0]);               // no handle
    fx.T4 = image(5, [4, 4], [0, 5], [0, 0]);             // zero record width
    fx.T5 = image(6, [32, 24], [32, 24], [-2, 3]);
    fx.T6 = image(7, [3, 0], [6, 0], [0, 0]);             // zero height (handle and record)
    // the nine-patch pieces: TL, TR, BL, BR, top, bottom, left, right (record dims differ from the handle dims)
    const PD = [[6, 5], [7, 4], [5, 6], [8, 7], [3, 4], [4, 5], [5, 3], [6, 3]];
    for (let k = 0; k < 8; k++) fx['P' + k] = image(10 + k, [PD[k][0] + 1, PD[k][1] + 2], PD[k], [k, -k]);
    fx.M = image(20, [20, 15], [20, 15], [0, 0]);          // the fill / mask image
    const parts = function (list, mask, flag) {
      const p = take(0x28);
      for (let k = 0; k < 8; k++) if (list.indexOf(k) >= 0) p.add(4 * k).writePointer(fx['P' + k]);
      if (mask) p.add(0x20).writePointer(fx.M);
      p.add(0x24).writeS32(flag);
      return p;
    };
    fx.pfull = parts([0, 1, 2, 3, 4, 5, 6, 7], true, 5);
    fx.pnomask = parts([0, 1, 2, 3, 4, 5, 6, 7], false, 5);
    fx.pcorners = parts([0, 1, 2, 3], true, 0);
    fx.pedges = parts([4, 5, 6, 7], true, 9);
    fx.ptopbot = parts([4, 5], true, 1);
    fx.pmask = parts([], true, 2);
    fx.pleft = parts([0, 2, 6], true, 3);
    fx.pright = parts([1, 3, 7], true, 4);
    fx.pnone = parts([], false, 0);
    fx.pmix = parts([0, 3, 4, 7], true, 6);
    const rect = function (l, t, r, b) { const p = take(0x10); [l, t, r, b].forEach(function (v, i) { p.add(4 * i).writeS32(v); }); return p; };
    fx.R0 = rect(10, 10, 110, 90); fx.R1 = rect(0, 0, 40, 30); fx.R2 = rect(-5, -5, 60, 50);
    fx.R3 = rect(20, 20, 20, 20); fx.R4 = rect(5, 5, 30, 12); fx.R5 = rect(400, 300, 500, 400);
    freezeNow();
    __freeze.setU32(ptr('0x0083ad50'), opts.disp === false ? 0 : disp.toUInt32());
    return fx;
  }

  // ---- Screen::close: screen objects (vtable slot +0x80 records) holding a sound object at +0x40 (slot +0x5c returns
  // opts-selected values, slot +0x38 records). releaseSoundObj 0x00484110 runs ORIGINAL: with [0x0083afc4] = 0 it returns 1;
  // with it non-zero it calls the pointer [0x0083af70] with the object: here a cdecl recorder.
  function buildScreenClose(opts) {
    const emit = codeArena();
    const rec = newRec();
    const fx = { rec: rec };
    const nul = emit([0x33, 0xC0, 0xC3]);
    const svt = alloc(0x400);
    for (let i = 0; i < 0x100; i++) svt.add(i * 4).writePointer(nul);
    svt.add(0x80).writePointer(emit(stubK(rec, 0x5C000080, '')));
    const mkO = function (ret5c, id) {
      const vt = alloc(0x400);
      for (let i = 0; i < 0x100; i++) vt.add(i * 4).writePointer(nul);
      vt.add(0x5c).writePointer(emit([0xB8, ...u32(ret5c), 0xC3]));
      vt.add(0x38).writePointer(emit(stubK(rec, 0x50000038, '')));
      const o = alloc(0x20); o.writePointer(vt); o.add(4).writeU32(0x50B00000 + id);
      return o;
    };
    const O = [mkO(0, 0), mkO(1, 1), null, mkO(0x55, 3), mkO(0, 4)];
    // screen objects are 0x80 apart; the sound object pointer lives at +0x40
    const scr2 = alloc(0x400);
    fx.scr = scr2;
    O.forEach(function (o, k) {
      const s = scr2.add(k * 0x80);
      s.writePointer(svt);
      s.add(4).writeU32(0x5C000000 + k);
      if (o) s.add(0x40).writePointer(o);
      fx['s' + k] = s;
    });
    freezeNow();
    __freeze.save(ptr('0x0083af70'), 4);
    __freeze.save(ptr('0x0083afc4'), 4);
    if (opts.lib) {
      ptr('0x0083afc4').writeU32(1);
      ptr('0x0083af70').writePointer(emit(libStub(rec)));
    } else {
      ptr('0x0083afc4').writeU32(0);
    }
    return fx;
  }
  // cdecl recorder with one argument: appends {tag, 0, arg}, returns 7, pops nothing
  function libStub(rec) {
    const r = u32(rec.toUInt32());
    const b = [0x53];
    b.push(0x8B, 0x1D, ...r, 0x81, 0xC3, ...r);                        // ebx = REC + [REC]
    b.push(0xC7, 0x03, ...u32(0x11B00001), 0xC7, 0x43, 0x04, 0, 0, 0, 0);
    b.push(0x8B, 0x44, 0x24, 0x08, 0x89, 0x43, 0x08);                  // [ebx+8] = arg
    b.push(0x81, 0x05, ...r, ...u32(12));
    b.push(0xB8, ...u32(7), 0x5B, 0xC3);
    return b;
  }

  // ---- window tree ---------------------------------------------------------------------------------------------
  // Eight windows (0x300 bytes each, one block $wins): +0x9c / +0xa0 flag words, +0xb0 owner pointer, +0x130 parent,
  // +0x1ac / +0x1bc RECTs, +0x1dc clip RECT (written by relayoutClip), +0x224 child array / +0x22c count. Window::visible
  // 0x004801f0 is byte[+0xa0] & 1 and every ancestor's the same.
  const NWIN = 8, WSIZE = 0x300;
  //        f9c     fa0  owner parent A (+0x1ac)           B (+0x1bc)          children
  const WSPEC = [
    [0x0000, 3, 0, -1, [10, 20, 310, 220], [5, 6, 305, 206], [1, 2, 3, 4]],
    [0x0000, 1, 0, 0, [20, 30, 120, 130], [3, 4, 103, 104], [5]],
    [0x0020, 1, 2, 0, [40, 50, 140, 150], [1, 2, 101, 102], [6]],
    [0x0000, 0, 3, 0, [60, 70, 160, 170], [7, 8, 107, 108], [7]],
    [0x8000, 1, 0, 0, [80, 90, 180, 190], [9, 1, 109, 101], []],
    [0x8020, 1, 5, 1, [11, 12, 51, 62], [2, 3, 52, 63], []],
    [0x0000, 1, 2, 2, [13, 14, 53, 64], [4, 5, 54, 65], []],
    [0x0020, 1, 3, 3, [15, 16, 55, 66], [6, 7, 56, 67], []],
  ];
  // body of the rect object's slot +0xd0 (fills the RECT at [esp+8])
  function rectFill(l, t, r, b) {
    return [0x8B, 0x44, 0x24, 0x08, 0xC7, 0x00, ...u32(l), 0xC7, 0x40, 0x04, ...u32(t), 0xC7, 0x40, 0x08, ...u32(r),
            0xC7, 0x40, 0x0C, ...u32(b)];
  }
  // opts: arr (window indices of the top-level array 0x0083a2d8), focus (index into the windows, -1: a stranger, undefined:
  // 0), pre (initial draw count), cur (index for [0x0083aa9c], -1 none), disp
  function buildWin(opts) {
    const emit = codeArena();
    const rec = newRec();
    const fx = { rec: rec };
    const bodies = {};
    bodies[0x2c] = [0x8B, 0x44, 0x24, 0x08, 0x83, 0x00, 0x03, 0x8B, 0x44, 0x24, 0x0C, 0x83, 0x00, 0x05];   // *x += 3; *y += 5
    bodies[0x38] = [0x8B, 0x44, 0x24, 0x08, 0xC7, 0x00, 0x07, 0x00, 0x00, 0x00, 0xC7, 0x40, 0x04, 0x0B, 0x00, 0x00, 0x00];
    const args = {}; args[0x2c] = 2; args[0x38] = 1;
    const drets = {};
    if (opts.dw !== undefined) { drets[0xa8] = opts.dw; drets[0xac] = opts.dh; }
    fx.disp = makeDisplay(emit, rec, drets, bodies, args);
    // the object at [0x0083a7bc]: slot +0xd0 (RECT*) fills 0, 0, 640, 480; slot +0xd4 returns a RECT {1, 2, 300, 200}
    const rr = alloc(0x10);
    [1, 2, 300, 200].forEach(function (v, i) { rr.add(4 * i).writeS32(v); });
    const rvt = alloc(0x400);
    for (let i = 0; i < 0x100; i++) rvt.add(i * 4).writePointer(emit(stubBytes(rec, 0x7EC70000 + i * 4, 0, 0)));
    rvt.add(0xd0).writePointer(emit(stubBytes(rec, 0x7EC700D0, 1, 0, rectFill(0, 0, 640, 480), true)));   // arg: stack RECT
    rvt.add(0xd4).writePointer(emit(stubBytes(rec, 0x7EC700D4, 0, rr.toUInt32())));
    const robj = alloc(0x20);
    robj.writePointer(rvt);
    fx.robj = robj;
    const wins = alloc(NWIN * WSIZE);
    fx.wins = wins;
    // every window has a vtable (hitTestTree 0x0047f340 calls slot +0x11c at 0x0047f473 / 0x0047f683): slot +0x11c returns 1
    const wnul = emit([0x33, 0xC0, 0xC3]);
    const wvt = alloc(0x400);
    for (let i = 0; i < 0x100; i++) wvt.add(i * 4).writePointer(wnul);
    wvt.add(0x11c).writePointer(emit([0xB8, 1, 0, 0, 0, 0xC3]));
    for (let k = 0; k < NWIN; k++) {
      const sp = WSPEC[k];
      const w = wins.add(k * WSIZE);
      w.writePointer(wvt);
      w.add(4).writeU32(0x57490000 + k);
      w.add(0x9c).writeU32(sp[0]); w.add(0xa0).writeU32(sp[1]);
      fx['w' + k] = w;
    }
    for (let k = 0; k < NWIN; k++) {
      const sp = WSPEC[k];
      const w = fx['w' + k];
      w.add(0xb0).writePointer(fx['w' + sp[2]]);
      if (sp[3] >= 0) w.add(0x130).writePointer(fx['w' + sp[3]]);
      sp[4].forEach(function (v, i) { w.add(0x1ac + 4 * i).writeS32(v); });
      sp[5].forEach(function (v, i) { w.add(0x1bc + 4 * i).writeS32(v); });
      const arr = alloc(0x40);
      sp[6].forEach(function (c, i) { arr.add(4 * i).writePointer(fx['w' + c]); });
      w.add(0x224).writePointer(arr);
      w.add(0x22c).writeS32(sp[6].length);
    }
    fx.stray = alloc(0x20);
    // cells for View_toLocal: (x_k, y_k) at cells + 8k
    const cells = alloc(0x80);
    for (let k = 0; k < 8; k++) {
      cells.add(8 * k).writeS32(100 + 7 * k);
      cells.add(8 * k + 4).writeS32(-40 + 13 * k);
      fx['cx' + k] = cells.add(8 * k);
      fx['cy' + k] = cells.add(8 * k + 4);
    }
    fx.cells = cells;
    // points for hitTestScreen: (px_k, py_k) at pcells + 8k
    const PTS = [[30, 40], [100, 100], [15, 25], [5, 5], [1000, 1000], [11, 12], [200, 150], [60, 70], [130, 140], [135, 60], [125, 145], [170, 180]];
    const pcells = alloc(0x80);
    PTS.forEach(function (pt, k) {
      pcells.add(8 * k).writeS32(pt[0]);
      pcells.add(8 * k + 4).writeS32(pt[1]);
      fx['px' + k] = pcells.add(8 * k);
      fx['py' + k] = pcells.add(8 * k + 4);
    });
    fx.pcells = pcells;
    fx.obj = wins;
    freezeNow();
    __freeze.setU32(ptr('0x0083ad50'), opts.disp === false ? 0 : fx.disp.toUInt32());
    __freeze.save(ptr('0x0083a7bc'), 4);
    ptr('0x0083a7bc').writePointer(robj);
    const focus = opts.focus === undefined ? ptr(0) : (opts.focus < 0 ? fx.stray : fx['w' + opts.focus]);
    __freeze.save(ptr('0x0083aae0'), 4);
    ptr('0x0083aae0').writePointer(focus);
    __freeze.save(ptr('0x0083aaec'), 4);
    ptr('0x0083aaec').writeU32(0x7788);
    __freeze.save(ptr('0x0083a2d8'), 0x40);
    const arr = opts.arr || [];
    for (let i = 0; i < 16; i++) ptr('0x0083a2d8').add(4 * i).writePointer(i < arr.length ? fx['w' + arr[i]] : ptr(0x5a5a0000 + i));
    __freeze.save(ptr('0x0083ab94'), 4);
    ptr('0x0083ab94').writeS32(arr.length);
    __freeze.save(ptr('0x0083ab90'), 4);
    ptr('0x0083ab90').writeS32(opts.pre || 0);
    __freeze.save(ptr('0x00839ac0'), 0x80);
    for (let i = 0; i < 0x80; i += 4) ptr('0x00839ac0').add(i).writeU32(0xA5A50000 + i);
    __freeze.save(ptr('0x0083aa9c'), 4);
    ptr('0x0083aa9c').writePointer(opts.cur === undefined || opts.cur < 0 ? ptr(0) : fx['w' + opts.cur]);
    if (opts.caps) {
      // hitTestScreen: the capture globals 0x0083ab30 / 34 / 40 / 44 / 50 / 54 (window indices, -1 none), the flag 0x0083ab18
      // and the saved point 0x00839ab8 / 0x00839abc
      __freeze.save(ptr('0x0083ab18'), 0x44);
      __freeze.save(ptr('0x00839ab8'), 8);
      const cap = opts.caps;
      [['g30', 0x30], ['g34', 0x34], ['c40', 0x40], ['c44', 0x44], ['g50', 0x50], ['g54', 0x54]].forEach(function (e) {
        const idx = cap[e[0]];
        ptr('0x0083ab00').add(e[1]).writePointer(idx === undefined || idx < 0 ? ptr(0) : fx['w' + idx]);
      });
      ptr('0x0083ab18').writeS32(cap.f18 === undefined ? 5 : cap.f18);
      ptr('0x00839ab8').writeS32(0x1111); ptr('0x00839abc').writeS32(0x2222);
    }
    return fx;
  }

  // ---- text objects (c3df's model: +0xc draw callback, +0x5c.. fonts, +4 handle) ------------------------------
  // The draw callback at object +0xc (cdecl (text, x, y, len), called first by drawRichText 0x004775b0 through
  // 0x004767a0) appends {tag, 0xC0C0C0C0, text, x, y, len, [watch]} to the recorder and returns (cursor ^ tag), always
  // non-zero, so drawRichText returns at once and every call's arguments are compared through $rec. The font measurer
  // (slot +0x10 of its vtable) returns the sum over the count characters of (W + (char & 3)), W = measurer + 4.
  function cbBytes(rec, watch, tag, n) {
    const r = u32(rec.toUInt32());
    const b = [];
    b.push(0xA1, ...r);                                              // mov eax, [REC]
    b.push(0x05, ...r);                                              // add eax, REC
    b.push(0xC7, 0x00, ...u32(tag));                                 // mov dword [eax], tag
    b.push(0xC7, 0x40, 0x04, ...u32(0xC0C0C0C0));                    // mov dword [eax+4], 0xC0C0C0C0
    for (let i = 1; i <= n; i++) b.push(0x8B, 0x54, 0x24, 4 * i, 0x89, 0x50, 4 + 4 * i);
    b.push(0x8B, 0x15, ...u32(watch));                               // mov edx, [WATCH]
    b.push(0x89, 0x50, 8 + 4 * n);                                   // mov [eax+8+4n], edx
    b.push(0x81, 0x05, ...r, ...u32(4 * (n + 3)));                   // add dword [REC], 4(n+3)
    b.push(0xA1, ...r);                                              // mov eax, [REC]
    b.push(0x35, ...u32(tag));                                       // xor eax, tag
    b.push(0xC3);
    return b;
  }
  const MEASURE = [0x56, 0x57, 0x8B, 0x74, 0x24, 0x0C, 0x8B, 0x7C, 0x24, 0x10, 0x33, 0xC0, 0x85, 0xFF, 0x7E, 0x0F,
                   0x0F, 0xB6, 0x16, 0x83, 0xE2, 0x03, 0x03, 0x51, 0x04, 0x03, 0xC2, 0x46, 0x4F, 0x75, 0xF1,
                   0x5F, 0x5E, 0xC2, 0x08, 0x00];
  function bytesOf(s) { return Array.from(s, function (c) { return c.charCodeAt(0); }); }
  const TEXTS = {
    s_plain: 'aaa bbb ccc ddd eee', s_one: 'abcdefgh', s_braces: 'ab {cd ef} gh ij', s_long:
      'the quick brown fox jumps over the lazy dog again and again', s_spaces: '  lead  and trail  ', s_empty: '',
    s_short: 'xy',
  };

  // opts.global: swap the tgt object into [0x004c1570] (freezing the first thread); opts.h: its handle's slot +0xdc
  // result (undefined: no handle); opts.fonts: false -> no fonts; opts.scale: value for [0x00822c8c].
  function buildText(opts) {
    opts = opts || {};
    const emit = codeArena();
    const rec = newRec();
    const watch = alloc(16);
    const fx = { rec: rec, watch: watch };
    const cb = emit(cbBytes(rec, watch.toUInt32(), 0xC3D90001, 4));
    const nul = emit([0x33, 0xC0, 0xC3]);
    const meas = emit(MEASURE);
    const mvt = alloc(0x200);
    for (let i = 0; i < 0x80; i++) mvt.add(i * 4).writePointer(nul);
    mvt.add(0x10).writePointer(meas);
    const font = function (w, h, p8, pc, nomeas) {
      const m = alloc(0x10);
      m.writePointer(mvt); m.add(4).writeS32(w);
      const f = alloc(0x20);
      if (!nomeas) f.add(4).writePointer(m);
      f.add(8).writeS32(p8 || 0); f.add(0xc).writeS32(pc || 0); f.add(0x10).writeS32(h);
      return f;
    };
    const F = [font(3, 12), font(5, 14), font(7, 16), font(11, 18)];
    fx.f0 = F[0]; fx.f1 = F[1]; fx.f2 = F[2]; fx.f3 = F[3];
    fx.fneg = font(3, 12, -1, 21);           // Widget_value: +8 < 0 returns +0xc (21)
    fx.fnomeas = font(3, 12, 0, 0, true);    // measurer NULL: the font swap is refused, the draw wrappers return 3
    // handle objects: slot +0xdc returns H
    const handle = function (H) {
      const vt = alloc(0x400);
      for (let i = 0; i < 0x100; i++) vt.add(i * 4).writePointer(nul);
      vt.add(0xdc).writePointer(emit([0xB8, ...u32(H), 0xC3]));
      const h = alloc(0x20);
      h.writePointer(vt);
      return h;
    };
    const tobj = function (o) {
      const t = alloc(0x300);
      t.writeU32(0x54580000 + (o.id || 0));
      if (o.h !== undefined) t.add(4).writePointer(handle(o.h));
      if (o.cb !== false) t.add(0xc).writePointer(cb);
      t.add(0x14).writeS32(o.indent || 0);
      t.add(0x34).writeS32(o.style || 0);
      t.add(0x40).writeS32(o.markup === undefined ? 1 : o.markup);
      t.add(0x48).writeS32(o.drop || 0);
      t.add(0x54).writeS32(o.narrow || 0);
      const fonts = o.fonts === false ? [] : (o.fontlist || F);
      for (let i = 0; i < 4; i++) if (fonts[i]) t.add(0x5c + 4 * i).writePointer(fonts[i]);
      return t;
    };
    fx.tm = tobj({ id: 1, h: 100 });                                   // markup on, all four fonts, handle height 100
    fx.th0 = tobj({ id: 2, h: 0 });                                    // handle whose height is 0
    fx.tnoh = tobj({ id: 3 });                                         // no handle
    fx.tnofont = tobj({ id: 4, h: 100, fonts: false, markup: 0 });     // no fonts: the wrappers return 3
    fx.tplain = tobj({ id: 5, h: 100, markup: 0, fontlist: [F[1]] });
    fx.tneg = tobj({ id: 6, h: 100, fontlist: [fx.fneg, F[1], F[2], F[3]] });
    fx.tnomeas = tobj({ id: 7, h: 100, fontlist: [fx.fnomeas] });
    fx.tind = tobj({ id: 8, h: 100, indent: 20 });                     // first-line indent 20
    fx.tind.add(0x18).writeS32(20);                                    // pending indent (measureText4 reads it without resetting)
    fx.tind2 = tobj({ id: 9, h: 100, indent: 3, fontlist: [F[1]], markup: 0 });
    fx.lines = alloc(0x80); fx.cnt = alloc(4); fx.cnt.writeS32(0x7777);
    fx.tgt = opts.tgt ? fx[opts.tgt] : fx.tm;
    // strings: NUL-terminated, 16 zero bytes after each
    let size = 0;
    Object.keys(TEXTS).forEach(function (k) { size += TEXTS[k].length + 1 + 16; });
    const buf = alloc((size + 3) & ~3);
    let off = 0;
    Object.keys(TEXTS).forEach(function (k) {
      if (TEXTS[k].length) buf.add(off).writeByteArray(bytesOf(TEXTS[k]));
      fx[k] = buf.add(off);
      off += TEXTS[k].length + 1 + 16;
    });
    fx.buf = buf;
    fx.obj = buf;
    const rect = function (l, t, r, b) { const p = alloc(0x10); [l, t, r, b].forEach(function (v, i) { p.add(4 * i).writeS32(v); }); return p; };
    fx.r0 = rect(0, 0, 100, 40); fx.r1 = rect(10, 20, 60, 25); fx.r2 = rect(-5, -8, 30, 50); fx.r3 = rect(0, 0, 0, 0);
    fx.r4 = rect(7, 3, 200, 160); fx.r5 = rect(10, 4, 11, 5);
    if (!opts.glob && !opts.global) freezeNow();
    if (opts.glob) {
      // text layout globals: 0x00839a9c (x cursor), 0x00839aa0 (y accumulator), 0x00839aa4 (limit), 0x00839aa8 (accumulator)
      // and 0x00839a5c (last draw result)
      freezeNow();
      __freeze.save(ptr('0x00839a9c'), 0x10);
      __freeze.save(ptr('0x00839a5c'), 4);
      ptr('0x00839a9c').writeS32(opts.glob[0]);
      ptr('0x00839aa0').writeS32(opts.glob[1]);
      ptr('0x00839aa4').writeS32(opts.glob[2]);
      ptr('0x00839aa8').writeS32(opts.glob[3]);
      ptr('0x00839a5c').writeS32(0x5c5c);
    }
    if (opts.global) {
      freezeNow();
      __freeze.setU32(ptr('0x004c1570'), fx.tgt.toUInt32());
      if (opts.scale !== undefined) __freeze.setU32(ptr('0x00822c8c'), opts.scale);
    }
    return fx;
  }

  const F = {};
  F.c3dq_tx_obj = function () { return buildText({}); };
  F.c3dq_ml_base = function () { return buildText({ glob: [11, 7, 60, 0] }); };
  F.c3dq_ml_lim0 = function () { return buildText({ glob: [11, 7, 0, 0] }); };
  F.c3dq_ml_wide = function () { return buildText({ glob: [11, 7, 200, 5] }); };
  F.c3dq_ml_pre = function () { return buildText({ glob: [11, 7, 60, 40] }); };
  F.c3dq_tg_h100 = function () { return buildText({ global: true, tgt: 'tm', scale: 800 }); };
  F.c3dq_tg_h100b = function () { return buildText({ global: true, tgt: 'tm', scale: 1000 }); };
  F.c3dq_tg_h0 = function () { return buildText({ global: true, tgt: 'th0', scale: 800 }); };
  F.c3dq_tg_noh = function () { return buildText({ global: true, tgt: 'tnoh', scale: 640 }); };
  F.c3dq_tg_nofont = function () { return buildText({ global: true, tgt: 'tnofont', scale: 800 }); };
  const ARRS = { a1: [0], a2: [0, 3, 1, 2], a3: [4, 1], a4: [], a5: [0, 1, 2, 4] };
  const FOC = { f0: undefined, fw1: 1, fw6: 6, fw2: 2, fw5: 5, fs: -1, fw4: 4 };
  Object.keys(ARRS).forEach(function (a) {
    Object.keys(FOC).forEach(function (f) {
      F['c3dq_wt_' + a + '_' + f] = function () { return buildWin({ arr: ARRS[a], focus: FOC[f], pre: 2 }); };
    });
  });
  [-1, 0, 1, 2, 5].forEach(function (c) {
    F['c3dq_wc_' + (c < 0 ? 'n' : c)] = function () { return buildWin({ arr: [0], cur: c }); };
  });
  F.c3dq_wc_nodisp = function () { return buildWin({ arr: [0], cur: -1, disp: false }); };
  F.c3dq_pt_c = function () { return buildPaint({ color: 0x1234 }); };
  F.c3dq_pt_0 = function () { return buildPaint({ color: 0 }); };
  F.c3dq_bl_obj = function () { return buildBlit({ o328: true, o32c: true, ctx: true }); };
  F.c3dq_bl_noctx = function () { return buildBlit({ o328: true, o32c: true, ctx: false }); };
  F.c3dq_bl_n32c = function () { return buildBlit({ o328: true, o32c: false, ctx: true }); };
  F.c3dq_bl_n328 = function () { return buildBlit({ o328: false, o32c: true, ctx: true }); };
  F.c3dq_bl_only328 = function () { return buildBlit({ o328: true, o32c: false, ctx: false }); };
  F.c3dq_bl_none = function () { return buildBlit({ o328: false, o32c: false, ctx: false }); };
  // flushBlitSurface is a leaf: four more vectors with different slot +0x80 results (the function returns that result)
  [0, 1, -1, 0x1234].forEach(function (v, i) {
    F['c3dq_bl_r' + i] = function () { return buildBlit({ o328: true, o32c: true, ctx: true, r80: v >>> 0 }); };
  });
  F.c3dq_cl_empty = function () { return buildClr({ head: false, count: 3 }); };
  F.c3dq_cl_n0 = function () { return buildClr({ count: 0 }); };
  F.c3dq_cl_n1 = function () { return buildClr({ count: 1 }); };
  F.c3dq_cl_n3 = function () { return buildClr({ count: 3, payload: [0, -1, 2], vb: [1] }); };
  F.c3dq_cl_n3b = function () { return buildClr({ count: 3, payload: [-1, 1, 2], vb: [0, 2] }); };
  F.c3dq_cl_n2 = function () { return buildClr({ count: 2, payload: [1, 2, -1] }); };
  F.c3dq_cl_n3c = function () { return buildClr({ count: 3, payload: [-1, -1, -1] }); };
  F.c3dq_cl_n3d = function () { return buildClr({ count: 3, payload: [0, 1, 2], vb: [0, 1, 2] }); };
  F.c3dq_cl_n1b = function () { return buildClr({ count: 1, payload: [2, -1, -1], vb: [0] }); };
  F.c3dq_cl_n3_nodisp = function () { return buildClr({ count: 3, payload: [0, 1, 2], disp: false }); };
  F.c3dq_ps_ok = function () { return buildPrim({ b8: 0, r: 0x33 }); };
  F.c3dq_ps_r0 = function () { return buildPrim({ b8: 0, r: 0 }); };
  F.c3dq_ps_b8 = function () { return buildPrim({ b8: 0x77, r: 0x33 }); };
  F.c3dq_tl_full = function () { return buildTiles({ clip: [0, 0, 320, 240] }); };
  F.c3dq_tl_small = function () { return buildTiles({ clip: [30, 20, 70, 60] }); };
  F.c3dq_tl_none = function () { return buildTiles({ clip: [600, 400, 700, 500] }); };
  F.c3dq_tl_nodisp = function () { return buildTiles({ clip: [0, 0, 320, 240], disp: false }); };
  F.c3dq_tl_nodispnone = function () { return buildTiles({ clip: [600, 400, 700, 500], disp: false }); };
  F.c3dq_wm_big = function () { return buildWin({ arr: [], dw: 640, dh: 480 }); };
  F.c3dq_wm_odd = function () { return buildWin({ arr: [], dw: 3, dh: 5 }); };
  F.c3dq_wm_nodisp = function () { return buildWin({ arr: [], dw: 0, dh: 0, disp: false }); };
  F.c3dq_sc_nolib = function () { return buildScreenClose({ lib: false }); };
  F.c3dq_sc_lib = function () { return buildScreenClose({ lib: true }); };
  const CAPS = {
    none: {}, c40: { c40: 1 }, c40bad: { c40: 3 }, c40dead: { c40: 3, c44: 1 }, c44: { c44: 1 }, c44bad: { c44: 3, g30: 4 },
    g30: { g30: 4 }, g30bad: { g30: 3, g34: 2 }, g34: { g34: 2 }, g34bad: { g34: 3 }, f: { g54: 0 }, fe: { g54: 3, g50: 1 },
    fenone: { g54: 0, g50: -1 }, fo: { g54: 3, g50: 2 }, flag0: { f18: 0 }, flag1: { f18: 1 }
  };
  Object.keys(CAPS).forEach(function (c) {
    F['c3dq_hs_' + c] = function () { return buildWin({ arr: [0, 1, 2], caps: CAPS[c] }); };
  });
  F.c3dq_hs_empty = function () { return buildWin({ arr: [], caps: {} }); };
  F.c3dq_scr_none = function () { return buildScreen({ b8: false, b0: 0 }); };
  F.c3dq_scr_b8 = function () { return buildScreen({ b8: true, b0: 0 }); };
  F.c3dq_scr_b0 = function () { return buildScreen({ b8: false, b0: 1 }); };
  F.c3dq_scr_both = function () { return buildScreen({ b8: true, b0: 1 }); };   // netServiceControl arm (modal tails only)
  F.c3dq_scr_nodisp = function () { return buildScreen({ disp: false, b8: false, b0: 0 }); };
  // screenTick47d010: slot +8 returns a constant per key (ten values) so the leaf has ten distinct vectors
  const TICK = [0, 1, -1, 0x7fffffff, 0x80000000, 0x1234, 0xdeadbeef, 7, 0x100, 0x55aa55aa];
  TICK.forEach(function (v, i) {
    F['c3dq_tick_' + i] = function () { return buildScreen({ b8: false, b0: 0, ret: { 8: v >>> 0 } }); };
  });
  F.c3dq_free_h = function () { return buildSurfRec({ global: true, handle: 0xE5E5E5E5 }); };
  F.c3dq_free_h2 = function () { return buildSurfRec({ global: true, handle: 0x1000 }); };
  F.c3dq_free_nohandle = function () { return buildSurfRec({ global: true, handle: 0 }); };
  F.c3dq_objs = function () { return buildSurfRec({}); };

  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
