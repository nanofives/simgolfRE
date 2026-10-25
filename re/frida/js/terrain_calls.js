// Terrain.dll call recorder (phase 5 step 3, the state side; re/PHASE5_TERRAIN_PLAN.md section 13). Driven by re/tools/terrain_calls.py.
//
// Logs every call of an exported Terrain method from the moment Terrain.dll is mapped (so getInstance, the constructor's state,
// initSystem and initTerrain are in the log), on any thread: one line per call
//   seq tid depth name ecx a0 a1 a2 a3 a4 ret [extra]
// raw dwords (floats as bit patterns), eax at return. Pure getters (tileAt, getType, getElevation, getWall, getVariation, hasPath,
// hasConnectedPath, tileHit) are counted, not logged. Extras: setType -> var=<tile +0x240 after the call> (the variation the DLL's
// rand() drew); passCollarInfo -> tbl=<the n ints>.
//
// Snapshots, always ON THE GAME THREAD at a function boundary (the fuzz-race lesson of section 9):
//   tcSnap(label)          at the next Terrain::render entry: the Terrain object (0x164ad0 bytes), Terrain.dll's .rdata+.data image
//                          (RVA 0x5f000 .. 0x113000: vertex / normal arrays, texture table, course type, view globals) and the
//                          collar table g_collarInfo points to; with the log position (seq of the next call).
//   tcDetail(name, n)      before and after each of the next n calls of `name` (outermost exported call only).
//   tcTake()               sends the queued snapshots ({tc: 'snap', ...} + data messages); tcDump() sends the log.
(function () {
  const BODY = {   // exported method -> body RVA (the exports point at incremental-link thunks), stack dword count
    '??1Terrain@@QAE@XZ': [0x3090, 0], calcAllNormals: [0xa740, 1], calcNormals: [0xa6f0, 1], changeLighting: [0x3540, 1],
    closeSystem: [0x9ba0, 0], drawBezierSpline: [0x5230, 9], drawCardinalSpline: [0x4c70, 11], drawCircle: [0x42a0, 2],
    drawLine: [0x48a0, 7], elevateCorner: [0xa5c0, 2], getElevation: [0x1de0, 2], getInstance: [0x31a0, 0], getType: [0x1f10, 1],
    getVariation: [0x3390, 1], getWall: [0x1e80, 2], hasConnectedPath: [0xa450, 2], hasPath: [0xa510, 1], initSystem: [0x9c80, 4],
    initTerrain: [0xa970, 0], layPath: [0xa3e0, 3], loadNewCourseType: [0x1af0, 1], localRender: [0x89e0, 3], lowerCorner: [0xa620, 2],
    lowerEdgeCorner: [0xa680, 4], passCollarInfo: [0xa880, 2], pathUpdateRender: [0x6410, 2], render: [0x5990, 2],
    renderTile: [0x80e0, 5], resetTerrain: [0xaa10, 0], resize: [0x9ed0, 2], setSplineHeight: [0xa840, 1], setType: [0x32f0, 3],
    setWall: [0xa560, 4], setZoomLevel: [0x8fc0, 1], stripRender: [0x9270, 3], tileAt: [0x1d50, 2], tileHit: [0xab30, 2],
    updatePath: [0xa4b0, 3]
  };
  const GETTERS = { tileAt: 1, getType: 1, getElevation: 1, getWall: 1, getVariation: 1, hasPath: 1, hasConnectedPath: 1, tileHit: 1 };
  const OBJ = 0x164ad0, DATA0 = 0x5f000, DATAN = 0xb4000, COLLAR_PTR = 0x106b48, TERRAIN_PTR = 0x106b4c;
  let tm = null, seq = 0, log = [], done = false, collarN = 0;
  const counts = {};
  const depth = {};          // per thread: exported-call nesting
  let snapReq = null;        // label of a pending tcSnap
  const detail = {};         // name -> calls left to snapshot around
  const queue = [];          // snapshots waiting for tcTake

  function hex(v) { return '0x' + (v >>> 0).toString(16); }
  function terrain() { const p = tm.base.add(TERRAIN_PTR).readPointer(); return p.isNull() ? null : p; }
  function capture(label, where) {
    const t = terrain();
    const cp = tm.base.add(COLLAR_PTR).readPointer();
    queue.push({ label: label, where: where, seq: seq, tobjAddr: t ? t.toUInt32() : 0,
                 tobj: t ? t.readByteArray(OBJ) : null, tdata: tm.base.add(DATA0).readByteArray(DATAN),
                 collarAddr: cp.toUInt32(), collar: (!cp.isNull() && collarN > 0) ? cp.readByteArray(collarN * 4) : null });
  }

  // ---- injection (tcInject): calls of the ORIGINAL's exported methods with chosen arguments, made on the game thread at the next
  // outermost Terrain::render entry (before that render is logged), each logged as an ordinary depth-0 call, with a snapshot before
  // the first and after each one (labels <method>#<seq>, like tcDetail). A tile argument is given as ['T', x, y].
  let injecting = false, injectQ = null, injected = 0;
  const NF = {};
  function nf(name) {
    if (!NF[name]) {
      const n = BODY[name][1];
      const at = [];
      for (let i = 0; i < n; i++) at.push('uint32');
      NF[name] = new NativeFunction(tm.base.add(BODY[name][0]), 'uint32', ['pointer'].concat(at), 'thiscall');
    }
    return NF[name];
  }
  function runInject(tid) {
    const q = injectQ;
    injectQ = null;
    const t = terrain();
    const tileVa = function (x, y) { return t.add(0x3a4 + (x + y * 50) * 0x248).toUInt32(); };
    injecting = true;
    try {
      for (let k = 0; k < q.length; k++) {
        const name = q[k][0];
        const args = q[k].slice(1).map(function (v) { return Array.isArray(v) ? tileVa(v[1], v[2]) : (v >>> 0); });
        const s = seq++;
        if (k === 0) capture(name + '#' + s, 'before');
        const r = nf(name).apply(null, [t].concat(args));
        const shown = [];
        for (let i = 0; i < 5; i++) shown.push(i < args.length ? hex(args[i]) : '-');
        let extra = '';
        if (name === 'setType') extra = ' var=' + ptr(args[0]).add(0x240).readS32();
        log.push([s, tid, 0, name, hex(t.toUInt32())].concat(shown).join(' ') + ' ' + hex(r) + extra);
        capture(name + '#' + s, 'after');
        injected++;
      }
    } finally {
      injecting = false;
    }
  }

  // ---- frames (tcFrames(true), roadmap S3.2): the root surface the GL context draws into, around every terrain pass ----
  // initSystem's third argument is the DC of the root window's surface (initTerrain passes slot +0x28 of the jgld surface at
  // g_rootWindow + 0x278); its bitmap's bits are the pixels the passes draw. For every call of the exe's updateTerrainRender on the
  // game thread the bits are copied (native memcpy into preallocated buffers) at its entry and its return, and at the entry and
  // the return of each depth-0 render / localRender / stripRender / pathUpdateRender inside it (the first PASS_CAP passes). Only the
  // last completed frame is kept; tcSnapNow sends it with the checkpoint (message tc 'frame').
  const UTR = ptr(0x004498a0), PASS_CAP = 8;
  const PASSES = { render: 1, localRender: 1, stripRender: 1, pathUpdateRender: 1 };
  let framesOn = !!globalThis.TC_FRAMES, rootHdc = null, dib = null, frameNo = 0, inFrame = false, fcur = null, flast = null;
  const LITE = !!globalThis.TC_FRAMES_LITE;   // only the last frame's entry and exit (the census recordings)
  let flastPass = null, lastIdx = 1, flastStrip = null, flastPath = null;
  const sentExtra = {};   // frame ordinals already sent as an extra frame (.pass / .strip / .path)
  // three buffer sets of [entry, exit, pass pre / post x PASS_CAP]: the frame being written, the last completed frame (lastIdx),
  // the last completed frame that had a pass (set 2)
  let bufs = null;
  function dibInfo() {
    if (dib || !rootHdc) return dib;
    const gdi = Process.getModuleByName('gdi32.dll');
    const GetCurrentObject = new NativeFunction(gdi.getExportByName('GetCurrentObject'), 'pointer', ['pointer', 'uint32'], 'stdcall');
    const GetObjectA = new NativeFunction(gdi.getExportByName('GetObjectA'), 'int', ['pointer', 'int', 'pointer'], 'stdcall');
    const hbm = GetCurrentObject(rootHdc, 7);                 // OBJ_BITMAP
    if (hbm.isNull()) return null;
    const ds = Memory.alloc(96);
    if (GetObjectA(hbm, 84, ds) < 84) return null;           // sizeof(DIBSECTION) on x86
    const bits = ds.add(20).readPointer();                   // BITMAP.bmBits
    if (bits.isNull()) return null;
    const wb = ds.add(12).readS32(), bh = ds.add(32).readS32();
    dib = { bits: bits, n: wb * Math.abs(bh), meta: { w: ds.add(4).readS32(), h: ds.add(8).readS32(), wb: wb, bpp: ds.add(18).readU16(),
            biHeight: bh, comp: ds.add(40).readU32(), m0: ds.add(64).readU32(), m1: ds.add(68).readU32(), m2: ds.add(72).readU32() } };
    const mk = function () { const a = []; for (let i = 0; i < 2 + 2 * PASS_CAP; i++) a.push(Memory.alloc(dib.n)); return a; };
    bufs = [mk(), mk(), mk(), mk(), mk()];   // + 3 the last frame with a stripRender, 4 with a pathUpdateRender
    return dib;
  }
  function fcopy(slot) {
    if (!dibInfo()) return;
    Memory.copy(bufs[0][slot], dib.bits, dib.n);
  }
  function passEnter(name, a, ecx) {
    if (!framesOn || !inFrame || fcur.passes.length >= PASS_CAP) return -1;   // (LITE: the pass is listed, not copied)
    const k = fcur.passes.length;
    const args = [];
    for (let i = 0; i < BODY[name][1]; i++) args.push(a[i].toUInt32());
    fcur.passes.push({ name: name, seq: seq, args: args, ecx: ecx });
    if (LITE) return -1;
    fcopy(2 + 2 * k);
    return k;
  }
  function passLeave(k) {
    if (k >= 0) fcopy(3 + 2 * k);
  }
  function hookFrames() {
    Interceptor.attach(UTR, {
      onEnter(a) {
        if (!framesOn || inFrame) { this.no = true; return; }
        inFrame = true;
        this.no = false;
        fcur = { frame: frameNo++, seq: seq, args: [a[0].toInt32(), a[1].toInt32(), a[2].toInt32(), a[3].toInt32()], passes: [] };
        fcopy(0);
      },
      onLeave(r) {
        if (this.no) return;
        fcopy(1);
        fcur.ret = r.toInt32();
        fcur.seqEnd = seq;
        inFrame = false;
        // a frame with a pass also becomes the last frame with a pass (buffer set 2); any frame becomes the last one
        const k = fcur.passes.length > 0 ? 2 : 1;
        const has = function (nm) { return fcur.passes.some(function (p) { return p.name === nm; }); };
        if (bufs) {
          // a frame with a stripRender / pathUpdateRender pass is also copied into set 3 / 4 (rare: native copies of the set)
          if (has('stripRender')) { for (let i = 0; i < bufs[0].length; i++) Memory.copy(bufs[3][i], bufs[0][i], dib.n); flastStrip = fcur; }
          if (has('pathUpdateRender')) { for (let i = 0; i < bufs[0].length; i++) Memory.copy(bufs[4][i], bufs[0][i], dib.n); flastPath = fcur; }
          const t = bufs[0]; bufs[0] = bufs[k]; bufs[k] = t;
        }
        if (k === 2) flastPass = fcur;
        flast = fcur;
        lastIdx = k;
        fcur = null;
      }
    });
  }
  hookFrames();
  rpc.exports.tcFrames = function (on) { framesOn = !!on; return { on: framesOn, hdc: rootHdc ? rootHdc.toUInt32() : 0 }; };
  // the checkpoint's frames: the last frame with a pass (label + '.pass', when it is not the last frame itself), then the last frame
  function sendFrame(label) {
    if (!flast || !dib) { send({ tc: 'frame', label: label, none: 1 }); return; }
    const extra = function (f, suffix, b) {
      if (!f || f === flast || sentExtra[f.frame]) return;
      sentExtra[f.frame] = 1;
      sendOne(label + suffix, f, b);
    };
    if (!LITE) extra(flastStrip, '.strip', bufs[3]);
    if (!LITE) extra(flastPath, '.path', bufs[4]);
    if (!LITE) extra(flastPass, '.pass', bufs[2]);
    sendOne(label, flast, bufs[lastIdx]);
  }
  function sendOne(label, f, b) {
    send({ tc: 'frame', label: label, frame: f.frame, seq: f.seq, seqEnd: f.seqEnd, args: f.args, ret: f.ret, passes: f.passes,
           dib: dib.meta });
    send({ tc: 'fdata', kind: 'entry' }, b[0].readByteArray(dib.n));
    send({ tc: 'fdata', kind: 'exit' }, b[1].readByteArray(dib.n));
    for (let k = 0; !LITE && k < f.passes.length; k++) {
      send({ tc: 'fdata', kind: 'pass' + k + '_pre' }, b[2 + 2 * k].readByteArray(dib.n));
      send({ tc: 'fdata', kind: 'pass' + k + '_post' }, b[3 + 2 * k].readByteArray(dib.n));
    }
    send({ tc: 'frameend', label: label });
  }

  function install() {
    if (done) return;
    tm = Process.findModuleByName('Terrain.dll');
    if (!tm) return;
    done = true;
    for (const name in BODY) {
      const rva = BODY[name][0], nargs = BODY[name][1];
      Interceptor.attach(tm.base.add(rva), {
        onEnter(a) {
          if (injecting) { this.skip = true; this.inj = true; return; }
          const tid = this.threadId;
          if (name === 'render' && !(depth[tid] > 0) && injectQ !== null) runInject(tid);
          const d = depth[tid] || 0;
          depth[tid] = d + 1;
          this.d = d;
          counts[name] = (counts[name] || 0) + 1;
          if (name === 'render' && d === 0 && snapReq !== null) { capture(snapReq, 'render'); snapReq = null; }
          if (GETTERS[name]) { this.skip = true; return; }
          if (name === 'initSystem' && d === 0) rootHdc = a[2];
          this.pk = (PASSES[name] && d === 0) ? passEnter(name, a, this.context.ecx.toUInt32()) : -1;
          this.seq = seq++;
          const args = [];
          for (let i = 0; i < 5; i++) args.push(i < nargs ? hex(a[i].toUInt32()) : '-');
          this.line = [this.seq, tid, d, name, hex(this.context.ecx.toUInt32())].concat(args);
          this.extra = '';
          if (name === 'passCollarInfo') {
            const n = a[1].toInt32();
            const v = [];
            for (let i = 0; i < n; i++) v.push(a[0].add(4 * i).readS32());
            this.extra = 'tbl=' + v.join(',');
            collarN = n;
          }
          if (name === 'setType') this.tile = a[0];
          this.det = d === 0 && detail[name] > 0;
          if (this.det) { detail[name]--; capture(name + '#' + this.seq, 'before'); }
        },
        onLeave(r) {
          if (this.inj) return;
          const tid = this.threadId;
          depth[tid] = (depth[tid] || 1) - 1;
          if (this.skip) return;
          passLeave(this.pk);
          let extra = this.extra;
          if (name === 'setType' && !this.tile.isNull()) extra += 'var=' + this.tile.add(0x240).readS32();
          log.push(this.line.join(' ') + ' ' + hex(r.toUInt32()) + (extra ? ' ' + extra : ''));
          if (this.det) capture(name + '#' + this.seq, 'after');
        }
      });
    }
    send({ tc: 'installed', base: tm.base.toUInt32() });
  }
  try { Process.attachModuleObserver({ onAdded: function () { install(); } }); } catch (e) { }
  install();
  const iv = setInterval(function () { install(); if (done) clearInterval(iv); }, 5);

  rpc.exports.tcInject = function (calls) { injectQ = calls; return true; };
  rpc.exports.tcInjected = function () { return { pending: injectQ !== null, done: injected }; };
  rpc.exports.tcSnap = function (label) { snapReq = label; return true; };
  // a snapshot NOW, for a caller that knows the game thread is stopped outside every Terrain call (a pinned scenario's checkpoint:
  // the main thread waits in the clock stub); refused (false) while any thread is inside an exported Terrain method
  rpc.exports.tcSnapNow = function (label) {
    for (const t in depth) if (depth[t] > 0) return false;
    capture(label, 'now');
    if (framesOn) sendFrame(label);
    return true;
  };
  rpc.exports.tcPending = function () { return snapReq !== null; };
  rpc.exports.tcDetail = function (name, n) { detail[name] = n; return true; };
  rpc.exports.tcState = function () { return { seq: seq, logged: log.length, queued: queue.length, counts: counts, installed: done }; };
  rpc.exports.tcTake = function () {
    const n = queue.length;
    while (queue.length) {
      const s = queue.shift();
      send({ tc: 'snap', label: s.label, where: s.where, seq: s.seq, tobjAddr: s.tobjAddr, collarAddr: s.collarAddr });
      if (s.tobj) send({ tc: 'data', kind: 'tobj' }, s.tobj);
      send({ tc: 'data', kind: 'tdata' }, s.tdata);
      if (s.collar) send({ tc: 'data', kind: 'collar' }, s.collar);
      send({ tc: 'snapend' });
    }
    return n;
  };
  rpc.exports.tcDump = function () {
    const lines = log;
    log = [];
    for (let i = 0; i < lines.length; i += 20000) send({ tc: 'chunk', i: i, lines: lines.slice(i, i + 20000) });
    send({ tc: 'done', n: lines.length });
    return lines.length;
  };
})();
