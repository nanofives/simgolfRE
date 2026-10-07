// Fixtures for C3 batch c3t (fillSpanDown, fillSpanUp, fill16, mapToScreen456b70, polyEdgeAdvance, Widget_drawHLine,
// Widget_drawVLine, Stream4838f0::puts, Surface::clear, Surface::copyRaw, Surface_fill). Only our own synthetic values
// (no game text). Names are prefixed c3t_. Allocations go through __keepAlloc (diff_hook rewrites Memory.alloc) so
// blocks referenced only from a field are not freed mid-run; NativeCallbacks are pushed onto __diffKeepAlive by hand.
(function () {
  function fillPattern(p, size, seed) {
    for (let i = 0; i < size; i++) p.add(i).writeU8((i * 29 + seed) & 0xff);
  }
  function zero(p, size) {
    for (let i = 0; i < size; i += 4) p.add(i).writeU32(0);
  }
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
  function keep(cb) { globalThis.__diffKeepAlive.push(cb); return cb; }

  // Polygon span context (globals read by fillSpanDown 0x00493000 / fillSpanUp 0x00493080): clip min x 0x0083d35c,
  // clip max x 0x0083d38c, row 0x0083d384, pitch 0x0083d388, 16-bit base 0x0083d37c, 8-bit base 0x0083d380, colour
  // word 0x0083d34c. Both bases point at one 0x200-byte buffer pre-filled with a pattern; row 3 x pitch 0x40 puts the
  // row at +0xc0, and x up to 40 stays inside the buffer for both widths.
  function span(minX, maxX) {
    const buf = Memory.alloc(0x200);
    fillPattern(buf, 0x200, 7);
    ptr('0x0083d35c').writeS32(minX);
    ptr('0x0083d38c').writeS32(maxX);
    ptr('0x0083d384').writeS32(3);
    ptr('0x0083d388').writeS32(0x40);
    ptr('0x0083d37c').writePointer(buf);
    ptr('0x0083d380').writePointer(buf);
    ptr('0x0083d34c').writeU16(0xbeef);
    return { obj: buf, buf: buf };
  }

  function mapFlag(flag) {
    ptr('0x00822b80').writeS32(flag);
    const out = Memory.alloc(0x10);
    out.writeU32(0x11111111); out.add(4).writeU32(0x22222222);
    return { obj: out, out: out, ox: out, oy: out.add(4) };
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3t_span() { return span(4, 40); },
    // min >= max: the only setting where the post-clamp length test (0x0049304a / 0x004930ca) is taken.
    c3t_span_inv() { return span(30, 10); },

    // fill16 0x00493630: one 0x100-byte buffer pre-filled with a pattern; dst is buf+0x40 (a) or buf+0x42 (b, not
    // dword-aligned). Counts stay below 0x40 words so every write lands inside the buffer.
    c3t_fill16() {
      const buf = Memory.alloc(0x100);
      fillPattern(buf, 0x100, 3);
      return { obj: buf, buf: buf, a: buf.add(0x40), b: buf.add(0x42) };
    },

    // mapToScreen456b70 0x00456b70: two output dwords x at out+0, y at out+4; the flag 0x00822b80 selects the +-1 term.
    c3t_map_f0() { return mapFlag(0); },
    c3t_map_f1() { return mapFlag(1); },
    c3t_map_fneg() { return mapFlag(0x80000000 | 0); },

    // polyEdgeAdvance 0x00492fa0 and its callee polyEdgeStep 0x00492ed0: vertex table (x, y dword pairs) at
    // 0x0083d358, vertex count 0x0083d378 = 6, end vertex index 0x0083d350 = 3. Vertices: v0 (10,0) v1 (33,5)
    // v2 (25,5) v3 (5,20) v4 (40,20) v5 (8,2). Edge records (0x24 bytes: +0 direction, +4 rows, +8 vertex index,
    // +0xc x, +0x10 x step, +0x14 sign step, +0x18 error, +0x1c error step, +0x20 error reset) e0..e13 in one block.
    c3t_poly() {
      const verts = Memory.alloc(6 * 8);
      const v = [[10, 0], [33, 5], [25, 5], [5, 20], [40, 20], [8, 2]];
      for (let i = 0; i < 6; i++) { verts.add(i * 8).writeS32(v[i][0]); verts.add(i * 8 + 4).writeS32(v[i][1]); }
      ptr('0x0083d358').writePointer(verts);
      ptr('0x0083d378').writeS32(6);
      ptr('0x0083d350').writeS32(3);
      // [dir, rows, idx, x, xstep, sign, err, errstep, reset]
      const E = [
        [1, 5, 0, 100, 2, 1, -10, 3, 7],          // e0: error stays <= 0
        [1, 5, 0, 100, 2, 1, -2, 3, 7],           // e1: error becomes 1 > 0 -> sign step and reset
        [1, 0, 0, 50, -3, -1, 0, 0, 4],           // e2: rows 0 -> -1 (non-zero), error 0 not > 0
        [1, 2, 0, -20, -1, -1, 5, 5, 9],          // e3: error 10 > 0, negative sign step
        [1, 0x80000000 | 0, 0, 7, 1, 1, 0, -1, 3],  // e4: rows wrap to 0x7fffffff
        [1, 9, 0, 7, 1, 1, 0x7fffffff, 1, 3],     // e5: error wraps to 0x80000000 (negative, not > 0)
        [1, 1, 3, 1, 1, 1, 1, 1, 1],              // e6: rows -> 0, index 3 == end -> step returns 0
        [1, 1, 0, 0, 0, 0, 0, 0, 0],              // e7: 0 -> 1, height 5, dx 23 (positive arm) -> 1
        [1, 1, 1, 0, 0, 0, 0, 0, 0],              // e8: 1 -> 2 height 0, 2 -> 3 height 15, dx -20 -> 1
        [-1, 1, 0, 0, 0, 0, 0, 0, 0],             // e9: 0 -> wraps to 5, height 2, dx -2 -> 1
        [1, 1, 5, 0, 0, 0, 0, 0, 0],              // e10: 5 -> wraps to 0, height -2 -> 0
        [1, 1, 4, 0, 0, 0, 0, 0, 0],              // e11: 4 -> 5, height -18 -> 0
        [-1, 1, 4, 0, 0, 0, 0, 0, 0],             // e12: 4 -> 3 height 0 and 3 == end -> 0
        [-1, 1, 2, 0, 0, 0, 0, 0, 0],             // e13: 2 -> 1 height 0, 1 -> 0 height -5 -> 0
      ];
      const block = Memory.alloc(E.length * 0x24);
      const fx = { obj: block, edges: block };
      for (let i = 0; i < E.length; i++) {
        const e = block.add(i * 0x24);
        for (let k = 0; k < 9; k++) e.add(k * 4).writeS32(E[i][k]);
        fx['e' + i] = e;
      }
      return fx;
    },

    // Widget_drawHLine 0x00478bb0 / Widget_drawVLine 0x00478be0: twelve widgets w0..w11 (8 bytes: +0 tag, +4 drawing
    // object); +4 is null for w0, w3, w7 and one of three drawing objects d1..d3 (+0 vtable, +4 id) otherwise. ONE
    // fake vtable: slot +0x64 is a thiscall NativeCallback taking six stack dwords that records them in rec.
    c3t_wline() {
      const rec = Memory.alloc(0x40); zero(rec, 0x40);
      const vt = Memory.alloc(0x80); zero(vt, 0x80);
      vt.add(0x64).writePointer(keep(new NativeCallback(function (self, a, b, c, d, e, f) {
        record(rec, self.add(4).readU32(), [a, b, c, d, e, f]);
      }, 'void', ['pointer', 'uint32', 'uint32', 'uint32', 'uint32', 'uint32', 'uint32'], 'thiscall')));
      const ds = [null];
      for (let k = 1; k <= 3; k++) { const d = Memory.alloc(8); d.writePointer(vt); d.add(4).writeU32(k); ds.push(d); }
      const which = [0, 1, 2, 0, 3, 1, 2, 0, 3, 1, 2, 3];
      const fx = { obj: rec, rec: rec };
      for (let i = 0; i < 12; i++) {
        const w = Memory.alloc(8);
        w.writeU32(0xc3700000 + i);
        if (which[i]) w.add(4).writePointer(ds[which[i]]); else w.add(4).writeU32(0);
        fx['w' + i] = w;
      }
      return fx;
    },

    // Stream4838f0::puts 0x004838f0: objects p0..p7 (+4 null for p0, p4; else stream k1..k3 with +4 id). ONE fake
    // vtable: slot +0x10 is a thiscall NativeCallback (text, len) that records (len, first byte) and returns
    // id * 1000 + len * 7 + first byte. Texts: t0 "", t1 "a", t2 "hello", t3 a 40-character string, t4 "xy".
    c3t_puts() {
      const rec = Memory.alloc(0x40); zero(rec, 0x40);
      const vt = Memory.alloc(0x20); zero(vt, 0x20);
      vt.add(0x10).writePointer(keep(new NativeCallback(function (self, text, len) {
        const id = self.add(4).readU32();
        const first = text.readU8();
        record(rec, id, [len, first]);
        return (id * 1000 + len * 7 + first) | 0;
      }, 'int', ['pointer', 'pointer', 'uint32'], 'thiscall')));
      const ks = [null];
      for (let k = 1; k <= 3; k++) { const s = Memory.alloc(8); s.writePointer(vt); s.add(4).writeU32(k); ks.push(s); }
      const which = [0, 1, 2, 3, 0, 1, 2, 3];
      const fx = { obj: rec, rec: rec };
      for (let i = 0; i < which.length; i++) {
        const p = Memory.alloc(8);
        p.writeU32(0);
        if (which[i]) p.add(4).writePointer(ks[which[i]]); else p.add(4).writeU32(0);
        fx['p' + i] = p;
      }
      const texts = ['', 'a', 'hello', 'abcdefghijklmnopqrstuvwxyz0123456789ABCD', 'xy'];
      for (let i = 0; i < texts.length; i++) fx['t' + i] = Memory.allocUtf8String(texts[i]);
      return fx;
    },

    // Surface::clear 0x00482940 / Surface::copyRaw 0x00482a80: holders h0..h8 (+4 = surface s_k). Each surface is
    // {+0 vtable, +4 id, +8 buffer, +0xc dimension A, +0x10 dimension B}; the buffers are 0x40-byte slices of one
    // pattern-filled block `bufs`. ONE fake vtable: +0x20 returns the buffer, +0x34 returns A, +0x30 returns B, +0x24
    // (one argument) records it. Every callback folds its slot number into rec's hash, so the call order is state.
    // Sources src0 / src1 (0x50 bytes of distinct patterns) feed copyRaw (it reads from src + 6).
    c3t_surf() {
      const rec = Memory.alloc(0x40); zero(rec, 0x40);
      const vt = Memory.alloc(0x40); zero(vt, 0x40);
      const getter = function (slot, off) {
        return keep(new NativeCallback(function (self) {
          record(rec, self.add(4).readU32(), [slot]);
          return self.add(off).readU32();
        }, 'uint32', ['pointer'], 'thiscall'));
      };
      vt.add(0x20).writePointer(getter(0x20, 8));
      vt.add(0x34).writePointer(getter(0x34, 0xc));
      vt.add(0x30).writePointer(getter(0x30, 0x10));
      vt.add(0x24).writePointer(keep(new NativeCallback(function (self, arg) {
        record(rec, self.add(4).readU32(), [0x24, arg]);
        return 0;
      }, 'int', ['pointer', 'uint32'], 'thiscall')));
      const dims = [[0, 9], [1, 1], [2, 1], [1, 3], [2, 2], [1, 5], [3, 3], [7, 9], [8, 8]];
      const bufs = Memory.alloc(dims.length * 0x40);
      fillPattern(bufs, dims.length * 0x40, 11);
      const fx = { obj: bufs, bufs: bufs, rec: rec };
      for (let i = 0; i < dims.length; i++) {
        const s = Memory.alloc(0x14);
        s.writePointer(vt); s.add(4).writeU32(i + 1); s.add(8).writePointer(bufs.add(i * 0x40));
        s.add(0xc).writeU32(dims[i][0]); s.add(0x10).writeU32(dims[i][1]);
        const h = Memory.alloc(8);
        h.writeU32(0); h.add(4).writePointer(s);
        fx['h' + i] = h;
      }
      for (let k = 0; k < 2; k++) {
        const src = Memory.alloc(0x50);
        fillPattern(src, 0x50, 100 + k * 50);
        fx['src' + k] = src;
      }
      return fx;
    },

    // Surface_fill 0x00475da0: objects f0..f7 (+4 null for f0, f5; else drawing object d1..d3 with +4 id). ONE fake
    // vtable: slot +0x5c is a thiscall NativeCallback (rect, a5) that records the four rect dwords and a5 and returns
    // a hash of them.
    c3t_sfill() {
      const rec = Memory.alloc(0x40); zero(rec, 0x40);
      const vt = Memory.alloc(0x80); zero(vt, 0x80);
      vt.add(0x5c).writePointer(keep(new NativeCallback(function (self, rect, a5) {
        const id = self.add(4).readU32();
        const r = [rect.readU32(), rect.add(4).readU32(), rect.add(8).readU32(), rect.add(12).readU32()];
        record(rec, id, r.concat([a5]));
        return (Math.imul(rec.add(8).readU32(), 1) & 0x7fffffff) | 0;
      }, 'int', ['pointer', 'pointer', 'uint32'], 'thiscall')));
      const ds = [null];
      for (let k = 1; k <= 3; k++) { const d = Memory.alloc(8); d.writePointer(vt); d.add(4).writeU32(k); ds.push(d); }
      const which = [0, 1, 2, 3, 1, 0, 2, 3];
      const fx = { obj: rec, rec: rec };
      for (let i = 0; i < which.length; i++) {
        const f = Memory.alloc(8);
        f.writeU32(0);
        if (which[i]) f.add(4).writePointer(ds[which[i]]); else f.add(4).writeU32(0);
        fx['f' + i] = f;
      }
      return fx;
    },
  });
})();
