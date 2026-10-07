// Fixtures for C3 batch c3v (ListModel findId/indexOfId/idAt, scrollbarResetState, Window grow/shrinkRect,
// Widget::setValue, EditBox::setText, nearestCharControl). Only our own synthetic values (no game text). Names are
// prefixed c3v_. Allocations go through __keepAlloc (diff_hook rewrites Memory.alloc) so blocks referenced only from a
// field are not freed mid-run; NativeCallbacks are pushed onto __diffKeepAlive by hand. nearestCharControl needs no
// fixture: it reads the constant control-spot table at 0x004c7b38 that is already in the binary.
(function () {
  function zero(p, size) { for (let i = 0; i < size; i += 4) p.add(i).writeU32(0); }
  function keep(cb) { globalThis.__diffKeepAlive.push(cb); return cb; }
  // rec layout shared by the virtual-call recorders: +0 call count, +4 last object id, +8 running hash of every
  // recorded value (hash = hash * 31 + v), +0xc.. the last call's arguments (one dword each).
  function record(rec, id, vals) {
    rec.writeU32(rec.readU32() + 1);
    rec.add(4).writeU32(id);
    let h = rec.add(8).readU32();
    h = (Math.imul(h, 31) + id) >>> 0;
    for (let i = 0; i < vals.length; i++) {
      const v = vals[i] >>> 0;
      h = (Math.imul(h, 31) + v) >>> 0;
      rec.add(0xc + 4 * i).writeU32(v);
    }
    rec.add(8).writeU32(h);
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    // ListModel (findId 0x004899d0 / indexOfId 0x004898d0 / idAt 0x00489a30): a circular doubly-linked list of 6 nodes
    // (0x14 bytes: data +4, next +0xc, prev +0x10) with data 100,200,...,600. The model object has head +0xc8,
    // current node +0xcc (init head), count +0xd0 = 6, index +0xd4 = 0, mirror index +0xf0 = 0.
    c3v_list() {
      const N = 6;
      const nodes = [];
      for (let i = 0; i < N; i++) { const n = Memory.alloc(0x14); zero(n, 0x14); n.add(4).writeS32((i + 1) * 100); nodes.push(n); }
      for (let i = 0; i < N; i++) {
        nodes[i].add(0xc).writePointer(nodes[(i + 1) % N]);       // next
        nodes[i].add(0x10).writePointer(nodes[(i - 1 + N) % N]);  // prev
      }
      const lm = Memory.alloc(0x120); zero(lm, 0x120);
      lm.add(0xc8).writePointer(nodes[0]);   // head
      lm.add(0xcc).writePointer(nodes[0]);   // current (init head)
      lm.add(0xd0).writeS32(N);              // count
      return { obj: lm, lm: lm };
    },

    // Empty ListModel (head +0xc8 = 0, count +0xd0 = 0): the head-null / count<=0 branches of findId (0x004899dd),
    // indexOfId (0x004898dd) and idAt (0x00489a9d). Current +0xcc is left 0; idAt never dereferences it here because
    // the head-null test returns 0 first.
    c3v_list_empty() {
      const lm = Memory.alloc(0x120); zero(lm, 0x120);
      return { obj: lm, lm: lm };
    },

    // scrollbarResetState 0x004979a0: twelve scrollbar objects with varied max +0x5c0 and anchor +0x57c (six with
    // +0x57c = -1, the inactive branch; six with +0x57c != -1, the active branch). The function writes +0x5ac, +0x5b0,
    // +0x5b4, +0x5b8.
    c3v_scroll() {
      const maxes = [0, 1, 5, 10, -1, -5, 100, 7, 50, 3, 0x7fffffff, -0x80000000];
      const fx = { obj: null };
      for (let i = 0; i < maxes.length; i++) {
        const s = Memory.alloc(0x5c8); zero(s, 0x5c8);
        s.add(0x5c0).writeS32(maxes[i]);
        s.add(0x57c).writeS32(i & 1 ? 3 : -1);  // odd -> active (!= -1), even -> inactive
        if (i === 0) fx.obj = s;
        fx['sb' + i] = s;
      }
      return fx;
    },

    // Window grow/shrinkRect (0x0047cc10 / 0x0047cce0): window objects with flags +0x9c and margins +0x180, +0x184,
    // +0x188 (-1 = none), optional caption frame +0x15c. Three input rectangles r0..r2 (int[4] = left,top,right,bottom)
    // that the function transforms. ONE fake vtable: slot +0x170 is a thiscall height() returning the frame's +4.
    c3v_rect() {
      const vt = Memory.alloc(0x200); zero(vt, 0x200);
      vt.add(0x170).writePointer(keep(new NativeCallback(function (self) {
        return self.add(4).readS32();
      }, 'int', ['pointer'], 'thiscall')));
      const frames = [];
      for (let k = 0; k < 2; k++) { const f = Memory.alloc(8); f.writePointer(vt); f.add(4).writeS32(k ? 7 : 12); frames.push(f); }
      // [flags, m180, m184, m188, frameIndex(-1 none)]
      const W = [
        [0x00000000, 0, 0, -1, -1],         // w0: nothing set
        [0x00000004, 0, 0, -1, -1],         // w1: bit 4 -> bottom += inset
        [0x00000008, 0, 0, -1, -1],         // w2: bit 8 -> right += inset
        [0x0000000c, 0, 0, -1, -1],         // w3: bits 4 and 8
        [0x00000400, 0, 5, -1, -1],         // w4: bit 0x400, m188 == -1
        [0x00000400, 0, 5, 9, -1],          // w5: bit 0x400, m188 != -1
        [0x00000001, 0, 4, -1, -1],         // w6: bit 0x1 (in 0x11 mask)
        [0x00000010, 3, 6, -1, -1],         // w7: bit 0x10 -> inset-all and top shift
        [0x20000400, 0, 5, -1, 0],          // w8: 0x400 inset, frame present but 0x20000000 suppresses it
        [0x00000000, 0, 0, -1, 0],          // w9: only the frame branch (height 12)
        [0x00000000, 0, 0, -1, 1],          // w10: only the frame branch (height 7)
        [0x00000011, 2, 6, 9, 0],           // w11: 0x1|0x10, m188 != -1, m180 set, frame present
      ];
      const fx = { obj: null };
      for (let i = 0; i < W.length; i++) {
        const w = Memory.alloc(0x190); zero(w, 0x190);
        w.add(0x9c).writeU32(W[i][0] >>> 0);
        w.add(0x180).writeS32(W[i][1]);
        w.add(0x184).writeS32(W[i][2]);
        w.add(0x188).writeS32(W[i][3]);
        if (W[i][4] >= 0) w.add(0x15c).writePointer(frames[W[i][4]]); else w.add(0x15c).writeU32(0);
        if (i === 0) fx.obj = w;
        fx['w' + i] = w;
      }
      const rects = [[100, 100, 200, 200], [0, 0, 50, 80], [-10, -20, 10, 20]];
      for (let j = 0; j < rects.length; j++) {
        const r = Memory.alloc(0x10);
        for (let k = 0; k < 4; k++) r.add(k * 4).writeS32(rects[j][k]);
        fx['r' + j] = r;
      }
      return fx;
    },

    // Widget::setValue 0x0047d020: objects with flag byte +0xa0 (bit 2 = active), geometry +0x1bc/+0x1c0/+0x1c4/+0x1c8
    // (width = +0x1c4 - +0x1bc, height = +0x1c8 - +0x1c0), value slot +0x184, vtable at +0 whose slot +0xc records the
    // invalidate call (width, height, 0, 0). sv1 has bit 2 clear (the early-return branch).
    c3v_setvalue() {
      const rec = Memory.alloc(0x40); zero(rec, 0x40);
      const vt = Memory.alloc(0x20); zero(vt, 0x20);
      vt.add(0xc).writePointer(keep(new NativeCallback(function (self, a, b, c, d) {
        record(rec, self.add(0x184).readU32(), [a, b, c, d]);
      }, 'void', ['pointer', 'uint32', 'uint32', 'uint32', 'uint32'], 'thiscall')));
      // [a0 bit2, 1bc, 1c0, 1c4, 1c8]
      const O = [
        [1, 10, 20, 110, 220],   // sv0: active, w=100 h=200
        [0, 10, 20, 110, 220],   // sv1: inactive (bit 2 clear)
        [1, -5, 0, 45, 300],     // sv2: active, w=50 h=300
        [1, 0, 0, 0, 0],         // sv3: active, w=0 h=0
      ];
      const fx = { obj: rec, rec: rec };
      for (let i = 0; i < O.length; i++) {
        const o = Memory.alloc(0x1d0); zero(o, 0x1d0);
        o.writePointer(vt);
        o.add(0xa0).writeU8(O[i][0] ? 2 : 0);
        o.add(0x1bc).writeS32(O[i][1]); o.add(0x1c0).writeS32(O[i][2]);
        o.add(0x1c4).writeS32(O[i][3]); o.add(0x1c8).writeS32(O[i][4]);
        fx['sv' + i] = o;
      }
      return fx;
    },

    // EditBox::setText 0x00486200: an edit box with a 32-byte buffer at +0x574 (capacity +0x578 = 16), a length slot
    // +0x5a4, and a vtable at +0 whose slot +0x120 records the notify. st1 has a null buffer (the early-return branch).
    // Texts t0..t4 are our own ASCII strings, all shorter than the capacity; a vector may also pass 0 (null text).
    c3v_settext() {
      const rec = Memory.alloc(0x40); zero(rec, 0x40);
      const vt = Memory.alloc(0x200); zero(vt, 0x200);
      vt.add(0x120).writePointer(keep(new NativeCallback(function (self) {
        record(rec, self.add(0x5a4).readU32(), [self.add(0x574).readU32()]);
      }, 'void', ['pointer'], 'thiscall')));
      const buf = Memory.alloc(0x20); zero(buf, 0x20);
      const st0 = Memory.alloc(0x5b0); zero(st0, 0x5b0);
      st0.writePointer(vt); st0.add(0x574).writePointer(buf); st0.add(0x578).writeU32(16);
      const st1 = Memory.alloc(0x5b0); zero(st1, 0x5b0);
      st1.writePointer(vt); st1.add(0x574).writeU32(0); st1.add(0x578).writeU32(16);
      const fx = { obj: buf, buf: buf, rec: rec, st0: st0, st1: st1 };
      const texts = ['', 'a', 'hello', 'abc123', 'fifteen-chars!!'];
      for (let i = 0; i < texts.length; i++) fx['t' + i] = Memory.allocUtf8String(texts[i]);
      return fx;
    },
  });
})();
