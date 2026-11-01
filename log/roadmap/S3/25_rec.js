// S3.25 recorder (appended to a pinned scenario's agent by log/roadmap/S3/25_rec.py; built on S3.5's scene_rec.js).
// Per FRAME of the ORIGINAL (a frame starts at resetSpriteQueue 0x004627d0, called once by mainLoop at 0x004105cf, before the
// ground passes; renderSpriteQueue 0x00463100 runs at 0x00415b6d, the present at 0x0041e29b, the checkpoint waits in H1):
//  - CALL LOG: every main-thread entry of the configured exe functions (and Terrain.dll bodies by RVA) as one row
//    [seq, hook id, return address, ecx, stack args..., {s: strings}, {ret: eax}, {out: derefs}], cleared at each frame start;
//  - BOX WATCH: after renderSpriteQueue returns, at every hooked onLeave the 16-bit root pixels of the configured boxes are read;
//    a change is logged as [row seq of the returning call, hook id, pixels]. Inner calls return first, so the first event after
//    a write names the innermost hooked function that made it;
//  - QUEUE DUMP: at renderSpriteQueue's entry, the configured .data ranges (the sprite-queue arrays, the sheet tables) copied
//    natively; at its return the root DIB copied (as scene_rec.js);
//  r25SnapNow(label) (called by the checkpoint, the main thread waiting in the clock stub) keeps the current frame's data;
//  r25Take() sends it. Config: globalThis.R25 = {hooks: [[va, nargs, name, flags]], thooks: [[rva, nargs, name, flags]],
//  boxes: [[x0, y0, x1, y1]], qranges: [[va, size]], rootq: bool, maxRows, postOnly: rows only after renderSpriteQueue,
//  watchFrom: 'rsq' = box watch from renderSpriteQueue's entry, 'frame' = from resetSpriteQueue (default: from its return);
//  with any hook flagged 'w' only those hooks check the boxes; watchT: [[lo, hi]] virtual T ranges (T() - t0) whose frames
//  watch the boxes (a frame is judged at its start; default every frame)}. flags: 's' strings of pointer args, 'r' eax,
//  'o' u32 at each stack-pointer argument on leave, 'ra:<lo>-<hi>' only calls returning into [lo, hi).
(function () {
  const C = globalThis.R25 || {};
  const RESET = ptr(0x004627d0), RSQ = ptr(0x00463100), INIT_SYSTEM_RVA = 0x9c80;
  const maxRows = C.maxRows || 200000;
  let main = null, frame = 0, rows = [], boxEv = [], post = false, watch = false, wok = true, boxLast = null, seq = 0, dropped = 0;
  function vt() { try { return T() - CFG.t0; } catch (e) { return -1; } }   // scenario_det.js's virtual clock, as the run log prints it
  let rootHdc = null, dib = null, rootq = null, qbuf = null, qsize = 0, qframe = -1, rqframe = -1, rsqCalls = 0;
  const snaps = [];
  if (C.qranges && C.qranges.length) {
    for (const r of C.qranges) qsize += r[1];
    qbuf = Memory.alloc(qsize);
  }
  function dibInfo() {
    if (dib || !rootHdc) return dib;
    const gdi = Process.getModuleByName('gdi32.dll');
    const GetCurrentObject = new NativeFunction(gdi.getExportByName('GetCurrentObject'), 'pointer', ['pointer', 'uint32'], 'stdcall');
    const GetObjectA = new NativeFunction(gdi.getExportByName('GetObjectA'), 'int', ['pointer', 'int', 'pointer'], 'stdcall');
    const hbm = GetCurrentObject(rootHdc, 7);
    if (hbm.isNull()) return null;
    const ds = Memory.alloc(96);
    if (GetObjectA(hbm, 84, ds) < 84) return null;
    const bits = ds.add(20).readPointer();
    if (bits.isNull()) return null;
    const wb = ds.add(12).readS32(), bh = ds.add(32).readS32();
    dib = { bits: bits, n: wb * Math.abs(bh), wb: wb, meta: { w: ds.add(4).readS32(), h: ds.add(8).readS32(), wb: wb,
            bpp: ds.add(18).readU16(), biHeight: bh } };
    if (C.rootq) rootq = Memory.alloc(dib.n);
    return dib;
  }
  function readBoxes() {
    if (!C.boxes || !C.boxes.length || !dibInfo()) return null;
    const out = [];
    for (const b of C.boxes) {
      const px = [], w = b[2] - b[0] + 1;
      for (let y = b[1]; y <= b[3]; y++) {
        const r = new Uint16Array(dib.bits.add(y * dib.wb + b[0] * 2).readByteArray(w * 2));
        for (let i = 0; i < w; i++) px.push(r[i]);
      }
      out.push(px);
    }
    return out;
  }
  function same(a, b) {
    if (!a || !b) return a === b;
    for (let i = 0; i < a.length; i++) for (let j = 0; j < a[i].length; j++) if (a[i][j] !== b[i][j]) return false;
    return true;
  }
  function cstr(p) {
    try {
      if (p.isNull() || p.compare(ptr(0x10000)) < 0) return null;
      const by = new Uint8Array(p.readByteArray(96));
      let n = by.indexOf(0);
      if (n < 1) return null;
      for (let i = 0; i < n; i++) if (by[i] < 9 || by[i] > 126) return null;
      return String.fromCharCode.apply(null, Array.from(by.subarray(0, n)));
    } catch (e) { return null; }
  }
  function parseFlags(f) {
    const o = { s: false, r: false, o: false, w: false, lo: 0, hi: 0 };
    for (const t of (f || '').split(',')) {
      if (t === 's') o.s = true; else if (t === 'r') o.r = true; else if (t === 'o') o.o = true; else if (t === 'w') o.w = true;
      else if (t.startsWith('ra:')) { const p = t.slice(3).split('-'); o.lo = parseInt(p[0], 16); o.hi = parseInt(p[1], 16); }
    }
    return o;
  }
  function attach(addr, id, n, fl) {
    Interceptor.attach(addr, {
      onEnter(a) {
        if (main === null || Process.getCurrentThreadId() !== main) return;
        if (C.postOnly && !post) return;
        const ra = this.returnAddress.toUInt32();
        if (fl.hi && (ra < fl.lo || ra >= fl.hi)) return;
        if (rows.length >= maxRows) { dropped++; return; }
        const row = [seq++, id, ra, this.context.ecx.toUInt32()];
        const args = [];
        for (let i = 0; i < n; i++) { const v = a[i]; args.push(v); row.push(v.toUInt32()); }
        if (fl.s) {
          const s = {};
          for (let i = 0; i < n; i++) { const t = cstr(args[i]); if (t !== null) s[i] = t; }
          row.push({ s: s });
        }
        rows.push(row);
        this.r25 = row;
        if (fl.o) { this.r25args = args; this.r25sp = this.context.esp.toUInt32(); }
      },
      onLeave(r) {
        const row = this.r25;
        if (!row) return;
        if (fl.r) row.push({ ret: r.toUInt32() });
        if (fl.o && this.r25args) {
          const out = {};
          for (let i = 0; i < this.r25args.length; i++) {
            const v = this.r25args[i].toUInt32();
            if (v >= this.r25sp - 0x4000 && v < this.r25sp + 0x40000) { try { out[i] = this.r25args[i].readU32(); } catch (e) { } }
          }
          row.push({ out: out });
        }
        if (watch && C.boxes && C.boxes.length && (fl.w || !anyW)) {
          const b = readBoxes();
          if (!same(b, boxLast)) { boxEv.push([row[0], id, b]); boxLast = b; }
        }
      }
    });
  }
  Interceptor.attach(RESET, {
    onEnter() {
      if (main === null) main = Process.getCurrentThreadId();
      if (Process.getCurrentThreadId() !== main) return;
      frame += 1; rows = []; boxEv = []; post = false; watch = false; boxLast = null; seq = 0;
      if (C.watchT && C.watchT.length) { const tt = vt(); wok = C.watchT.some(function (w) { return tt >= w[0] && tt <= w[1]; }); }
      if (wok && C.watchFrom === 'frame' && dibInfo()) {
        watch = true;
        boxLast = readBoxes();
        boxEv.push([-1, -3, boxLast]);
      }
    }
  });
  Interceptor.attach(RSQ, {
    onEnter() {
      if (main === null || Process.getCurrentThreadId() !== main) return;
      rows.push([seq++, -1, this.returnAddress.toUInt32(), 0]);
      if (qbuf) {
        let o = 0;
        for (const r of C.qranges) { Memory.copy(qbuf.add(o), ptr(r[0]), r[1]); o += r[1]; }
        qframe = frame;
      }
      if (wok && (C.watchFrom === 'rsq' || C.watchFrom === 'frame') && dibInfo()) {
        watch = true;
        const b = readBoxes();
        if (b && !same(b, boxLast)) boxEv.push([seq - 1, -1, b]);
        boxLast = b;
      }
    },
    onLeave() {
      if (main === null || Process.getCurrentThreadId() !== main) return;
      rsqCalls += 1;
      rows.push([seq++, -2, 0, 0]);
      if (dibInfo()) {
        if (rootq) { Memory.copy(rootq, dib.bits, dib.n); rqframe = frame; }
        post = true;
        watch = wok;
        if (wok) {
          const b = readBoxes();
          if (b && (!C.watchFrom || !same(b, boxLast))) boxEv.push([seq - 1, -2, b]);
          boxLast = b;
        }
      }
    }
  });
  const anyW = (C.hooks || []).concat(C.thooks || []).some(function (h) { return parseFlags(h[3]).w; });
  (C.hooks || []).forEach(function (h, i) { attach(ptr(h[0]), i, h[1], parseFlags(h[3])); });
  let hookedT = false;
  function hookTerrain() {
    if (hookedT) return;
    const tm = Process.findModuleByName('Terrain.dll');
    if (!tm) return;
    hookedT = true;
    Interceptor.attach(tm.base.add(INIT_SYSTEM_RVA), { onEnter(a) { rootHdc = a[2]; } });
    const base = (C.hooks || []).length;
    (C.thooks || []).forEach(function (h, i) { attach(tm.base.add(h[0]), base + i, h[1], parseFlags(h[3])); });
  }
  try { Process.attachModuleObserver({ onAdded: function () { hookTerrain(); } }); } catch (e) { }
  const iv = setInterval(function () { hookTerrain(); if (hookedT) clearInterval(iv); }, 5);
  rpc.exports.r25SnapNow = function (label) {
    const s = { label: label, frame: frame, rsqCalls: rsqCalls, rows: rows, boxEv: boxEv, dropped: dropped,
                qframe: qframe, rqframe: rqframe, meta: dib ? dib.meta : null, post: post };
    const parts = [];
    if (qbuf && qframe === frame) { s.qsize = qsize; parts.push(qbuf.readByteArray(qsize)); } else s.qsize = 0;
    if (rootq && rqframe === frame) { s.rqsize = dib.n; parts.push(rootq.readByteArray(dib.n)); } else s.rqsize = 0;
    let total = 0;
    for (const p of parts) total += p.byteLength;
    const all = new Uint8Array(total);
    let o = 0;
    for (const p of parts) { all.set(new Uint8Array(p), o); o += p.byteLength; }
    s.data = all.buffer;
    snaps.push(s);
    rows = rows.slice();      // the snapshot keeps its own array; this frame's later rows go on in a copy
    boxEv = boxEv.slice();
    return true;
  };
  rpc.exports.r25Take = function () {
    const n = snaps.length;
    while (snaps.length) {
      const s = snaps.shift();
      const d = s.data;
      delete s.data;
      send({ r25: 'snap', meta: s }, d.byteLength ? d : null);
    }
    send({ r25: 'done' });
    return n;
  };
})();
