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
  let snapMode = false, multi = false, passActive = false, inDraw = false, depth = 0, passLines = null, hdc = 0, snap = null, cur = null, vp = 0, np = 0;
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
    return { meta: meta, bytes: bits.readByteArray(wb * Math.abs(bh)) };
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
                     vtx: vp ? ptr(vp).readByteArray((cur.maxIdx + 1) * 12) : null, nrm: np ? ptr(np).readByteArray((cur.maxIdx + 1) * 12) : null };
          }
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
  rpc.exports.glrecSnap = function (on, mult) { snapMode = !!on; multi = !!mult; return snapMode; };
  rpc.exports.glrecSendSnap = function () {
    if (!snap || !snap.pre || !snap.post) return false;
    const s = snap; snap = null;      // each snapshot is sent once; the next call reports false until a new pass completes
    if (s.passLines) for (let i = 0; i < s.passLines.length; i += 20000) send({ glrec: 'passlines', i: i, lines: s.passLines.slice(i, i + 20000) });
    snap = s;
    send({ glrec: 'snapmeta', startIdx: snap.startIdx, endIdx: snap.endIdx, maxIdx: snap.maxIdx, stateIdx: snap.stateIdx, meta: snap.pre.meta, postMeta: snap.post.meta });
    send({ glrec: 'snapdata', kind: 'pre' }, snap.pre.bytes);
    send({ glrec: 'snapdata', kind: 'post' }, snap.post.bytes);
    if (snap.vtx) send({ glrec: 'snapdata', kind: 'vtx' }, snap.vtx);
    if (snap.nrm) send({ glrec: 'snapdata', kind: 'nrm' }, snap.nrm);
    send({ glrec: 'snapend' });
    snap = null;
    return true;
  };
  rpc.exports.glrecSeen = function () { return seen; };
  rpc.exports.glrecState = function () { return { armed: state.armed, n: state.n, dropped: state.dropped, hooks: state.installed, flushLeft: state.flushLeft }; };
})();
