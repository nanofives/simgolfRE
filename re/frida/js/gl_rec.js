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
  const state = { armed: false, flushLeft: 0, n: 0, dropped: 0, installed: 0, tid: 0 };
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

  function hook(mod, name, spec) {
    let addr = null;
    try { addr = Process.getModuleByName(mod).getExportByName(name); } catch (e) { return; }
    const nargs = spec[0], ptrs = spec[1] || {};
    Interceptor.attach(addr, {
      onEnter(a) {
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
            } else if (k === 'tex') {   // glTexImage2D(..., w=3,h=4,border=5,fmt=6,type=7,pixels=8): hash 4 bytes per pixel bound
              const wd = w[3], ht = w[4];
              const px = w[w.length - 1];
              if (px && wd > 0 && ht > 0 && wd * ht < 4194304) line += ' tex=' + fnv(ptr(px), wd * ht * 4);
            } else {
              const p = ptr(w[+k]); const fl = [];
              for (let i = 0; i < ptrs[k]; i++) fl.push(hex(p.add(4 * i).readU32()));
              line += ' v=' + fl.join(',');
            }
          } catch (e) { line += ' v=?'; }
        }
        buf.push(line);
        state.n++;
        if (name === 'glFlush' && --state.flushLeft <= 0) {
          state.armed = false;
          const lines = buf; buf = [];
          for (let i = 0; i < lines.length; i += 20000) send({ glrec: 'chunk', i: i, lines: lines.slice(i, i + 20000) });
          send({ glrec: 'done', calls: lines.length, dropped: state.dropped });
        }
      }
    });
    state.installed++;
  }
  // the script loads at spawn, before the loader has mapped Terrain.dll / opengl32.dll: install when they are there
  const iv = setInterval(function () {
    tm = Process.findModuleByName('Terrain.dll');
    if (!tm || !Process.findModuleByName('opengl32.dll')) return;
    clearInterval(iv);
    for (const n in T) hook('opengl32.dll', n, T[n]);
    if (Process.findModuleByName('glu32.dll')) for (const n in GLU) hook('glu32.dll', n, GLU[n]);
    send({ glrec: 'installed', hooks: state.installed, terrain: !!tm });
  }, 100);
  rpc.exports.glrecArm = function (n) { buf = []; state.n = 0; state.dropped = 0; state.flushLeft = n; state.armed = true; return true; };
  rpc.exports.glrecSeen = function () { return seen; };
  rpc.exports.glrecState = function () { return { armed: state.armed, n: state.n, dropped: state.dropped, hooks: state.installed, flushLeft: state.flushLeft }; };
})();
