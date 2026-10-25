// GL call-stream recorder (Phase 5 step 3 measurement, re/PHASE5_TERRAIN_PLAN.md section 2.3).
// Hooks the OPENGL32 / GLU32 exports that Terrain.dll imports and records every call made FROM Terrain.dll code
// (caller address inside the module) while armed, with the raw stack dwords of each argument (floats / doubles as bit
// patterns), the float vectors behind pointer arguments of the glXXXfv calls, and a hash of texture / array memory.
// rpc: glrec_arm(nFlush) starts recording and stops after nFlush glFlush calls; glrec_state() returns the counters.
// Messages: {glrec: 'done', calls: [...]} in chunks of {glrec: 'chunk', i, lines}.
(function () {
  // name: [dwords of stack args, pointer-arg spec]  spec: {argIndex: floatCount} reads floats behind that pointer
  const T = {
    glEnableClientState: [1], glHint: [2], glFrontFace: [1], glEnable: [1], glDisable: [1],
    glNormal3fv: [1, { 0: 3 }], glVertex3fv: [1, { 0: 3 }], glTexCoord2fv: [1, { 0: 2 }], glVertex2fv: [1, { 0: 2 }],
    glArrayElement: [1], glTexParameterf: [3], glTexParameteri: [3], glTexEnvi: [3],
    glGenTextures: [2], glDeleteTextures: [2], glBindTexture: [2],
    glTexImage2D: [9, { tex: 1 }], glVertexPointer: [4, { ptr: 1 }], glNormalPointer: [3, { ptr: 1 }],
    glViewport: [4], glScalef: [3], glNormal3f: [3], glTexCoord2f: [2], glVertex3f: [3],
    glRotated: [8], glRotatef: [4], glTranslatef: [3], glMatrixMode: [1], glPushMatrix: [0], glLoadIdentity: [0],
    glOrtho: [12], glBlendFunc: [2], glLineWidth: [1], glColor4f: [4], glBegin: [1], glVertex2i: [2], glEnd: [0],
    glPopMatrix: [0], glFlush: [0], glMaterialfv: [3, { 2: 4 }], glMateriali: [3], glLightfv: [3, { 2: 4 }],
    wglCreateContext: [1], wglMakeCurrent: [2], wglDeleteContext: [1]
  };
  const GLU = { gluBuild2DMipmaps: [7, { tex: 6 }], gluBuild1DMipmaps: [6, { tex: 5 }] };
  let tm = null;
  const state = { armed: true, flushLeft: 1e9, n: 0, dropped: 0, installed: 0, tid: 0 };
  let buf = [];
  const seen = {};
  const CAP = 4000000;
  // injection state (glrecInject, below), declared first: the module observer can run tryInstall while this script evaluates
  let injectQ = null, injecting = false, injCur = 0, injSeq = 0, injDone = 0, injLog = [], replaced = false, injectCond = null;
  const injSnaps = [], entryDepth = {}, NF = {};

  function hex(v) { return '0x' + (v >>> 0).toString(16); }
  function fnv(p, n) {
    let h = 0x811c9dc5;
    const b = new Uint8Array(p.readByteArray(n));
    for (let i = 0; i < n; i++) { h ^= b[i]; h = Math.imul(h, 0x01000193) >>> 0; }
    return hex(h);
  }

  // ---- snapshot mode (glrecSnap(true)): everything a replay needs for the LAST draw pass (glPushMatrix .. glFlush) ----
  // texture bytes of every glTexImage2D (once per FNV hash, sent as they happen), the glGenTextures / glDeleteTextures ids, and for
  // each pass the DIB the context draws into BEFORE (at glPushMatrix) and AFTER (glFlush returned), the vertex / normal array
  // memory up to the largest glArrayElement index, and the stream line numbers the pass spans.
  // multi mode (glrecSnap(true, true)): a pass (first glPushMatrix .. the glFlush after the matching glPopMatrix, plus the glLoadIdentity
  // before it) is kept OUT of the stream in `passLines`, so one boot can snapshot many passes without the stream growing; the
  // snapshot carries the pass lines and `stateIdx` = the number of stream lines before it (state calls only).
  let snapMode = false, multi = false, passActive = false, inDraw = false, depth = 0, passLines = null, hdc = 0, snap = null, cur = null, vp = 0, np = 0, wantT = false, entryRec = null;
  const lastByKind = {};
  const sentTex = {};
  const gdi = Process.getModuleByName('gdi32.dll');
  const GetCurrentObject = new NativeFunction(gdi.getExportByName('GetCurrentObject'), 'pointer', ['pointer', 'uint32'], 'stdcall');
  const GetObjectA = new NativeFunction(gdi.getExportByName('GetObjectA'), 'int', ['pointer', 'int', 'pointer'], 'stdcall');
  function dibRead() {
    if (!hdc) return null;
    const hbm = GetCurrentObject(ptr(hdc), 7);              // OBJ_BITMAP
    if (hbm.isNull()) return null;
    const ds = Memory.alloc(96);
    if (GetObjectA(hbm, 84, ds) < 84) return null;           // sizeof(DIBSECTION) on x86
    const bits = ds.add(20).readPointer();                   // BITMAP.bmBits
    const wb = ds.add(12).readS32(), w = ds.add(4).readS32(), bh = ds.add(32).readS32();   // bmWidthBytes, bmWidth, dsBmih.biHeight
    const meta = { w: w, h: ds.add(8).readS32(), wb: wb, bpp: ds.add(18).readU16(), biHeight: bh, comp: ds.add(40).readU32(),
                   m0: ds.add(64).readU32(), m1: ds.add(68).readU32(), m2: ds.add(72).readU32() };
    if (bits.isNull()) return null;
    return { meta: meta, bytes: bits.readByteArray(wb * Math.abs(bh)), bits: bits };
  }

  function hook(mod, name, spec) {
    let addr = null;
    try { addr = Process.getModuleByName(mod).getExportByName(name); } catch (e) { return; }
    const nargs = spec[0], ptrs = spec[1] || {};
    Interceptor.attach(addr, {
      onEnter(a) {
        this.rec = false;
        const key = name + '@' + (Process.findModuleByAddress(this.returnAddress) || { name: '?' }).name;
        seen[key] = (seen[key] || 0) + 1;
        if (!state.armed) return;
        if (tm && !(this.returnAddress >= tm.base && this.returnAddress < tm.base.add(tm.size))) return;
        if (buf.length >= CAP) { state.dropped++; return; }
        const w = [];
        for (let i = 0; i < nargs; i++) w.push(a[i].toUInt32());
        let line = name + ' ' + w.map(hex).join(',');
        for (const k in ptrs) {
          try {
            if (k === 'ptr') {          // glVertexPointer(size,type,stride,ptr) / glNormalPointer(type,stride,ptr): keep the pointer
              line += ' p=' + hex(w[w.length - 1]);
            } else if (k === 'tex') {   // glTexImage2D(..., w=3,h=4,border=5,fmt=6,type=7,pixels=8): hash bytes per pixel bound
              const wd = w[3], ht = w[4];
              const px = w[w.length - 1];
              const bpp = w[6] === 0x1907 ? 3 : w[6] === 0x1908 ? 4 : 0;   // GL_RGB / GL_RGBA, unsigned byte (fmt = w[6], type = w[7])
              if (px && bpp && wd > 0 && ht > 0 && wd * ht < 4194304) {
                const hs = fnv(ptr(px), wd * ht * bpp);
                line += ' tex=' + hs;
                if (snapMode && !sentTex[hs]) { sentTex[hs] = 1; send({ glrec: 'tex', hash: hs, w: wd, h: ht, fmt: w[6] }, ptr(px).readByteArray(wd * ht * bpp)); }
              }
            } else {
              const p = ptr(w[+k]); const fl = [];
              for (let i = 0; i < ptrs[k]; i++) fl.push(hex(p.add(4 * i).readU32()));
              line += ' v=' + fl.join(',');
            }
          } catch (e) { line += ' v=?'; }
        }
        if (snapMode) {
          if (name === 'wglMakeCurrent' && w[0]) hdc = w[0];
          else if (name === 'glVertexPointer') vp = w[3];
          else if (name === 'glNormalPointer') np = w[2];
          else if (name === 'glDeleteTextures' && w[1]) {
            const ids = []; for (let i = 0; i < w[0] && i < 64; i++) ids.push(hex(ptr(w[1]).add(4 * i).readU32()));
            line += ' ids=' + ids.join(',');
          } else if (name === 'glPushMatrix') {
            const afterIdent = buf.length > 0 && buf[buf.length - 1].startsWith('glLoadIdentity');
            if (!multi) {
              cur = { pre: dibRead(), startIdx: buf.length - (afterIdent ? 1 : 0), maxIdx: -1 };
            } else {
              if (!passActive) {   // nested pushes of one pass keep the first pre-pass DIB
                passActive = true; depth = 0; passLines = [];
                cur = { pre: dibRead(), stateIdx: buf.length - (afterIdent ? 1 : 0), maxIdx: -1 };
                if (afterIdent) passLines.push(buf[buf.length - 1]);
              }
              depth++;
            }
          } else if (name === 'glPopMatrix' && multi) depth--;
          else if (name === 'glArrayElement' && cur && w[0] > cur.maxIdx) cur.maxIdx = w[0];
        }
        this.rec = true;
        this.w = w;
        // multi mode: the stream keeps EVERY call except the bodies of glBegin..glEnd blocks (their vertex data is the pass's own),
        // because passes also change state that outlasts them (glMatrixMode, glOrtho, colour, blending of overlay drawing);
        // passLines holds the complete pass, draw blocks included.
        let toStream = true;
        if (multi) { if (name === 'glBegin') inDraw = true; toStream = !inDraw; if (name === 'glEnd') inDraw = false; }
        this.toStream = toStream;
        this.idx = buf.length;
        if (toStream) buf.push(line);
        this.pidx = -1;
        if (multi && passActive) { this.pidx = passLines.length; passLines.push(line); }
        state.n++;
        if (name === 'glFlush' && !snapMode && --state.flushLeft <= 0) {
          state.armed = false;
          const lines = buf; buf = [];
          for (let i = 0; i < lines.length; i += 20000) send({ glrec: 'chunk', i: i, lines: lines.slice(i, i + 20000) });
          send({ glrec: 'done', calls: lines.length, dropped: state.dropped });
        }
      },
      onLeave(r) {
        if (!this.rec || !snapMode) return;
        if (name === 'glGenTextures' && this.w[0] > 0 && this.w[0] <= 64) {   // the ids the driver handed out
          const ids = []; for (let i = 0; i < this.w[0]; i++) ids.push(hex(ptr(this.w[1]).add(4 * i).readU32()));
          if (this.toStream) buf[this.idx] += ' ids=' + ids.join(',');
          if (this.pidx >= 0) passLines[this.pidx] += ' ids=' + ids.join(',');
        } else if (name === 'glFlush' && (!multi || depth <= 0)) {
          if (cur && cur.maxIdx >= 0) {
            const post = dibRead();
            snap = { pre: cur.pre, post: post, startIdx: cur.startIdx, endIdx: this.idx, maxIdx: cur.maxIdx, stateIdx: multi ? cur.stateIdx : buf.length,
                     passLines: multi ? passLines.slice() : null,
                     vtx: vp ? ptr(vp).readByteArray((cur.maxIdx + 1) * 12) : null, nrm: np ? ptr(np).readByteArray((cur.maxIdx + 1) * 12) : null,
                     entry: entryRec, tobj: null, tdata: null, tobjAddr: 0 };
            if (wantT && tm) {   // the Terrain object (g_terrain 0x106b4c, 0x164ad0 bytes) and the DLL's .rdata + .data image
              const op = tm.base.add(0x106b4c).readPointer();
              if (!op.isNull()) { snap.tobjAddr = op.toUInt32(); snap.tobj = op.readByteArray(0x164ad0); }
              snap.tdata = tm.base.add(0x5f000).readByteArray(0xb4000);
            }
            if (snap.entry && snap.entry.inj) { injSnaps.push(snap); snap = null; }   // an injected pass (glrecInject): its own list
            else lastByKind[snap.entry ? snap.entry.kind : 0] = snap;
            if (fuzzLive && snap) lastByKind[22] = snap;   // pseudo-kind 22: the pass that ran on fuzzed tiles (a later plain pass must not replace it)
          }
          if (fuzzLive) { restoreFuzz(); fuzzLive = false; fuzzDone++; }
          entryRec = null;
          if (multi && passActive) { passActive = false; passLines = null; cur = null; }
        }
      }
    });
    state.installed++;
  }
  // the script loads at spawn, before the loader has mapped Terrain.dll / opengl32.dll: install when they are there
  let done = false;
  function tryInstall() {
    if (done) return;
    tm = Process.findModuleByName('Terrain.dll');
    if (!tm || !Process.findModuleByName('opengl32.dll')) return;
    done = true;
    for (const n in T) hook('opengl32.dll', n, T[n]);
    if (Process.findModuleByName('glu32.dll')) for (const n in GLU) hook('glu32.dll', n, GLU[n]);
    if (injectQ !== null) installReplace();
    // the exported render entry points: which one starts a pass and with what (this, pivot / tile, direction / pivot, angle)
    const ENTRY = { stripRender: [0x9270, 1], render: [0x5990, 2], localRender: [0x89e0, 3], pathUpdateRender: [0x6410, 4] };
    for (const en in ENTRY) {
      Interceptor.attach(tm.base.add(ENTRY[en][0]), { onEnter(a) {
        const tid = this.threadId;
        const d = entryDepth[tid] || 0;
        entryDepth[tid] = d + 1;
        // a queued fuzz is applied here, ON THE GAME THREAD, just before a Terrain::render pass starts, and undone right after
        // that pass's glFlush (onLeave below): the rpc thread never touches the tiles while the game thread reads them
        if (en === 'render' && pendingFuzz && !fuzzLive) { applyFuzz(pendingFuzz.seed, pendingFuzz.p); pendingFuzz = null; fuzzLive = true; }
        if (snapMode && !entryRec) entryRec = { kind: ENTRY[en][1], name: en, ecx: this.context.ecx.toUInt32(), a0: a[0].toUInt32(), a1: a[1].toUInt32(), a2: a[2].toUInt32(), inj: injecting ? injCur : 0 };
      }, onLeave() { entryDepth[this.threadId] = (entryDepth[this.threadId] || 1) - 1; } });
    }
    send({ glrec: 'installed', hooks: state.installed, terrain: !!tm });
  }
  // Process.attachModuleObserver fires as each module is mapped, i.e. before the loader reaches the exe's entry point, so the
  // first GL call is seen; the timer is the fallback for a Frida without it.
  try { Process.attachModuleObserver({ onAdded: function () { tryInstall(); } }); } catch (e) { }
  tryInstall();
  const iv = setInterval(function () { tryInstall(); if (done) clearInterval(iv); }, 5);
  // init mode: recording is armed from the moment the hooks are installed (flushLeft huge); glrecMark inserts a '#mark' line,
  // glrecDump sends everything recorded so far (chunks + done) and disarms. glrecArm(n) (frame mode) resets the buffer.
  rpc.exports.glrecMark = function (label) { if (state.armed) buf.push('#mark ' + label + ' n=' + buf.length); return buf.length; };
  rpc.exports.glrecDump = function () {
    state.armed = false;
    const lines = buf; buf = [];
    for (let i = 0; i < lines.length; i += 20000) send({ glrec: 'chunk', i: i, lines: lines.slice(i, i + 20000) });
    send({ glrec: 'done', calls: lines.length, dropped: state.dropped });
    return lines.length;
  };
  rpc.exports.glrecSnap = function (on, mult, tstate) { snapMode = !!on; multi = !!mult; wantT = !!tstate; return snapMode; };
  // ---- injection (glrecInject, S3.1): passes of the ORIGINAL's draw-list entry points that the scenarios never (or rarely)
  // reach, called with chosen arguments ON THE GAME THREAD at the next outermost Terrain::render / localRender entry, before
  // that call starts. Each entry is [name, args...]: name stripRender (pivot, dir, angle), pathUpdateRender (pivot, angle),
  // localRender (tile, pivot, angle) or render (tile, angle), or ['zoom', z]: the Terrain object's zoom shift (+0x1c) set to z for
  // the injected calls after it (written back when the injection ends; the GL projection stays the current one, like glrecFuzz's
  // state changes: the passes are the original's for that object state); an argument 'P' is the interrupted call's pivot (render's tile,
  // localRender's pivot = the camera tile), 'A' its angle bits, ['T', x, y] a tile, a number a raw dword (floats as bits).
  // Each injected pass is kept in injSnaps (snapshot form, entry.inj = its sequence number); after the last one the back
  // buffer is written back to what it held before the first, so the game's frame and everything after it are unchanged.
  // The calls are made from an Interceptor.replace callback of render / localRender (installed at the first glrecInject, while
  // the scenario's main thread waits at a checkpoint outside every Terrain call): Frida runs no hook for a call made from inside
  // an Interceptor.attach callback, so injected from onEnter the passes drew without one GL call recorded (g1, S3.1 notes).
  const BODY = { stripRender: [0x9270, 3], render: [0x5990, 2], localRender: [0x89e0, 3], pathUpdateRender: [0x6410, 2] };
  function nf(name) {
    if (!NF[name]) {
      const at = ['pointer'];
      for (let i = 0; i < BODY[name][1]; i++) at.push('uint32');
      NF[name] = new NativeFunction(tm.base.add(BODY[name][0]), 'void', at, 'thiscall');
    }
    return NF[name];
  }
  function runInject(ecx, pivot, angle, where) {
    const q = injectQ;
    injectQ = null;
    const dib = dibRead();
    const t = ptr(ecx);
    const w = t.add(0x14).readS32();
    const tileVa = function (x, y) { return t.add(0x3a4 + (x + y * w) * 0x248).toUInt32(); };
    const zoom0 = t.add(0x1c).readS32();
    let fuzzedHere = false;
    injecting = true;
    try {
      for (let k = 0; k < q.length; k++) {
        const name = q[k][0];
        if (name === 'zoom') { t.add(0x1c).writeS32(q[k][1]); injLog.push({ seq: 0, name: 'zoom', args: [hex(q[k][1])], at: where }); continue; }
        // ['fuzz', seed, p]: glrecFuzz's mutation of the tiles' draw fields for the injected calls after it (undone at the end)
        if (name === 'fuzz') { if (!fuzzSaved) { applyFuzz(q[k][1], q[k][2]); fuzzedHere = true; } injLog.push({ seq: 0, name: 'fuzz', args: [hex(q[k][1])], at: where }); continue; }
        const args = q[k].slice(1).map(function (v) {
          if (v === 'P') return pivot.toUInt32();
          if (v === 'A') return angle.toUInt32();
          if (Array.isArray(v)) return tileVa(v[1], v[2]);
          return v >>> 0;
        });
        injCur = ++injSeq;
        entryRec = null;
        nf(name).apply(null, [t].concat(args));
        injLog.push({ seq: injCur, name: name, args: args.map(hex), at: where });
      }
    } finally {
      injecting = false;
      t.add(0x1c).writeS32(zoom0);
      if (fuzzedHere) restoreFuzz();
      injCur = 0;
      entryRec = null;
      if (dib) dib.bits.writeByteArray(dib.bytes);
      injDone++;
    }
  }
  function installReplace() {
    if (replaced || !tm) return;
    replaced = true;
    const REPL = { render: [0x5990, 2, 0, 1], localRender: [0x89e0, 3, 1, 2] };   // body, args, pivot index, angle index
    for (const en in REPL) {
      const r = REPL[en];
      const orig = new NativeFunction(tm.base.add(r[0]), 'void', r[1] === 2 ? ['pointer', 'uint32', 'uint32'] : ['pointer', 'uint32', 'uint32', 'uint32'], 'thiscall');
      const cb = function (self, x0, x1, x2) {
        const args = [x0, x1, x2];
        // the attach listener of this entry ran first (depth 1 = outermost) and set entryRec for this call: keep it aside
        if (injectQ !== null && !injecting && snapMode && multi && (entryDepth[Process.getCurrentThreadId()] || 0) === 1 &&
            (injectCond === null || self.add(0x1c).readS32() === injectCond.zoom)) {
          const saved = entryRec;
          entryRec = null;
          runInject(self, ptr(args[r[2]]), ptr(args[r[3]]), en);
          entryRec = saved;
        }
        if (r[1] === 2) orig(self, x0, x1); else orig(self, x0, x1, x2);
      };
      Interceptor.replace(tm.base.add(r[0]), r[1] === 2
        ? new NativeCallback(function (s, a0, a1) { cb(s, a0, a1, 0); }, 'void', ['pointer', 'uint32', 'uint32'], 'thiscall')
        : new NativeCallback(function (s, a0, a1, a2) { cb(s, a0, a1, a2); }, 'void', ['pointer', 'uint32', 'uint32', 'uint32'], 'thiscall'));
    }
  }
  // cond (optional) {zoom: z}: run only at a call whose Terrain object has that zoom shift (a pass at a zoom the scenario holds
  // between two checkpoints); a list queued before Terrain.dll is mapped is installed with the other hooks
  rpc.exports.glrecInject = function (list, cond) { injectQ = list; injectCond = cond || null; installReplace(); return true; };
  rpc.exports.glrecInjected = function () { return { pending: injectQ !== null, done: injDone, snaps: injSnaps.length, log: injLog }; };
  function sendOne(s, tag) {
    if (s.passLines) for (let i = 0; i < s.passLines.length; i += 20000) send({ glrec: 'passlines', i: i, lines: s.passLines.slice(i, i + 20000) });
    send({ glrec: 'snapmeta', startIdx: s.startIdx, endIdx: s.endIdx, maxIdx: s.maxIdx, stateIdx: s.stateIdx, meta: s.pre.meta, postMeta: s.post.meta, entry: s.entry, tobjAddr: s.tobjAddr, tag: tag });
    send({ glrec: 'snapdata', kind: 'pre' }, s.pre.bytes);
    send({ glrec: 'snapdata', kind: 'post' }, s.post.bytes);
    if (s.vtx) send({ glrec: 'snapdata', kind: 'vtx' }, s.vtx);
    if (s.nrm) send({ glrec: 'snapdata', kind: 'nrm' }, s.nrm);
    if (s.tobj) send({ glrec: 'snapdata', kind: 'tobj' }, s.tobj);
    if (s.tdata) send({ glrec: 'snapdata', kind: 'tdata' }, s.tdata);
    send({ glrec: 'snapend', tag: tag });
  }
  // every injected pass completed so far, in order (then forgotten); returns how many were sent
  rpc.exports.glrecSendInjected = function () {
    const l = injSnaps.splice(0, injSnaps.length);
    for (let i = 0; i < l.length; i++) if (l[i].pre && l[i].post) sendOne(l[i], 'inj' + l[i].entry.inj);
    return l.length;
  };
  rpc.exports.glrecSendSnap = function (kind) {
    // kind 0 = the most recent pass, else the most recent pass started by that entry point (1 stripRender, 2 render,
    // 3 localRender, 4 pathUpdateRender)
    const pick = kind ? lastByKind[kind] : snap;
    if (!pick || !pick.pre || !pick.post) return false;
    const s = pick;
    if (snap === s) snap = null;
    lastByKind[s.entry ? s.entry.kind : 0] = null;   // each snapshot is sent once; the next call reports false until a new pass completes
    if (kind === 22) lastByKind[22] = null;
    snap = null;
    if (s.passLines) for (let i = 0; i < s.passLines.length; i += 20000) send({ glrec: 'passlines', i: i, lines: s.passLines.slice(i, i + 20000) });
    snap = s;
    send({ glrec: 'snapmeta', startIdx: snap.startIdx, endIdx: snap.endIdx, maxIdx: snap.maxIdx, stateIdx: snap.stateIdx, meta: snap.pre.meta, postMeta: snap.post.meta, entry: snap.entry, tobjAddr: snap.tobjAddr });
    send({ glrec: 'snapdata', kind: 'pre' }, snap.pre.bytes);
    send({ glrec: 'snapdata', kind: 'post' }, snap.post.bytes);
    if (snap.vtx) send({ glrec: 'snapdata', kind: 'vtx' }, snap.vtx);
    if (snap.nrm) send({ glrec: 'snapdata', kind: 'nrm' }, snap.nrm);
    if (snap.tobj) send({ glrec: 'snapdata', kind: 'tobj' }, snap.tobj);
    if (snap.tdata) send({ glrec: 'snapdata', kind: 'tdata' }, snap.tdata);
    send({ glrec: 'snapend' });
    snap = null;
    return true;
  };
  // State fuzzing of the ORIGINAL: randomise the draw-relevant fields of the live Terrain object's 2,500 tiles so the render path's
  // rarely taken branches run (path bands toward connected neighbours, every wall side, hidden neighbours for skirts, type-7
  // rotated textures, rotation and variation), then glrecRestore() writes the original bytes back. Fields (Tile): type +0x24,
  // rotation +0x28, path flags +0x208 (has) +0x209 (connected) +0x20a (draw), wall kinds +0x210 (9 ints), wall flags +0x234 (9
  // bytes), variation +0x240. p = [path, wall, hidden, type7] probabilities.
  let fuzzSaved = null, pendingFuzz = null, fuzzLive = false, fuzzDone = 0;
  function terrainObj() { const op = tm.base.add(0x106b4c).readPointer(); return op.isNull() ? null : op; }
  // glrecFuzz queues one mutation for the next Terrain::render pass (applied and undone on the game thread);
  // glrecFuzzDone() counts the passes that ran mutated
  rpc.exports.glrecFuzz = function (seed, p) { pendingFuzz = { seed: seed, p: p }; return true; };
  rpc.exports.glrecFuzzDone = function () { return fuzzDone; };
  function applyFuzz(seed, p) {
    const op = terrainObj();
    if (!op) return -1;
    const base = op.add(0x3a4);
    if (!fuzzSaved) fuzzSaved = { ptr: base, bytes: base.readByteArray(2500 * 0x248) };
    let s = seed >>> 0;
    const rnd = function () { s = (s + 0x6d2b79f5) >>> 0; let t = s; t = Math.imul(t ^ (t >>> 15), t | 1); t ^= t + Math.imul(t ^ (t >>> 7), t | 61); return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
    let n = 0;
    for (let i = 0; i < 2500; i++) {
      const t = base.add(i * 0x248);
      if (rnd() < p[0]) {   // a path tile; neighbours often have one too, so bands and overlay masks vary
        t.add(0x208).writeU8(1); t.add(0x209).writeU8(rnd() < 0.5 ? 1 : 0); t.add(0x20a).writeU8(rnd() < 0.85 ? 1 : 0); n++;
      }
      for (let k = 0; k < 9; k++) {
        if (rnd() < p[1]) { t.add(0x234 + k).writeU8(1); t.add(0x210 + 4 * k).writeS32(rnd() < 0.5 ? 1 : 2); }
      }
      const r = rnd();
      if (r < p[2]) t.add(0x24).writeS32(0x14);
      else if (r < p[2] + p[3]) t.add(0x24).writeS32(7);
      t.add(0x28).writeS32(Math.floor(rnd() * 4));
      t.add(0x240).writeS32(Math.floor(rnd() * 3));
    }
    return n;
  };
  function restoreFuzz() {
    if (!fuzzSaved) return false;
    fuzzSaved.ptr.writeByteArray(fuzzSaved.bytes);
    fuzzSaved = null;
    return true;
  }
  rpc.exports.glrecRestore = restoreFuzz;
  rpc.exports.glrecSeen = function () { return seen; };
  rpc.exports.glrecState = function () { return { armed: state.armed, n: state.n, dropped: state.dropped, hooks: state.installed, flushLeft: state.flushLeft }; };
})();
