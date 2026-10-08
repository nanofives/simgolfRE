// Fixtures for C3 batch c3be (round 14, golf_clean.exe): seven IJG libjpeg 6a compress helpers and four
// engine forwarders that dispatch through one object pointer. Only our own synthetic values (no game text,
// no live game object). Names are prefixed c3be_.
//
// Allocations go through Memory.alloc (diff_hook rewrites it to __keepAlloc so blocks referenced only from a
// field survive the run); NativeCallbacks are pushed onto __diffKeepAlive by hand.
//
// Every callback must be a pure function of the memory diff_hook snapshots and restores between the two arms,
// otherwise the second arm would see a different world than the first. The ones here read only fields that are
// declared as state regions (a destination manager's next_output_byte / free_in_buffer) or are constant.
//
// libjpeg structures used: cinfo+0 = err (jpeg_error_mgr: error_exit +0, msg_code +0x14), cinfo+4 = mem
// (jpeg_memory_mgr: alloc_small +0), cinfo+0x14 = dest (jpeg_destination_mgr: next_output_byte +0,
// free_in_buffer +4, init_destination +8, empty_output_buffer +0xc, term_destination +0x10).
(function () {
  function keep(cb) { globalThis.__diffKeepAlive.push(cb); return cb; }
  function zero(p, n) { for (let i = 0; i < n; i += 4) p.add(i).writeU32(0); }
  function fill(p, n, v) { for (let i = 0; i < n; i += 4) p.add(i).writeU32(v); }
  function alloc(n, v) { const p = Memory.alloc(n); if (v === undefined) zero(p, n); else fill(p, n, v); return p; }

  // alloc_small(cinfo, pool, size): hands out the buffer stashed at cinfo->mem+8, so both A/B arms get the SAME
  // pointer (a fresh Memory.alloc per call would differ between the arms and read RED). __cdecl.
  const ALLOC_CB = keep(new NativeCallback(function (cinfo, pool, size) {
    return cinfo.add(4).readPointer().add(8).readPointer();
  }, 'pointer', ['pointer', 'int', 'int']));

  // empty_output_buffer(cinfo) = TRUE: reset the destination manager to the buffer stashed at dest+0x18 with
  // 0x40 free. Deterministic (it writes constants into a region that is part of the state).
  const EMPTY_OK = keep(new NativeCallback(function (cinfo) {
    const d = cinfo.add(0x14).readPointer();
    d.writePointer(d.add(0x18).readPointer());
    d.add(4).writeU32(0x40);
    return 1;
  }, 'int', ['pointer']));
  // empty_output_buffer(cinfo) = FALSE: the suspension case libjpeg's own sinks never return.
  const EMPTY_FAIL = keep(new NativeCallback(function (cinfo) { return 0; }, 'int', ['pointer']));
  // empty_output_buffer(cinfo) = TRUE for the progressive flush: advance the manager by one byte and give back
  // 0x10 more free bytes, both derived from the manager's current (restored) contents, so the result differs
  // per vector and is identical in both arms.
  const EMPTY_STEP = keep(new NativeCallback(function (cinfo) {
    const d = cinfo.add(0x14).readPointer();
    d.writePointer(d.readPointer().add(1));
    d.add(4).writeU32((d.add(4).readU32() + 0x10) >>> 0);
    return 1;
  }, 'int', ['pointer']));
  // error_exit(cinfo): libjpeg's never returns; ours records the message code the caller set and returns, so
  // the instructions after the ERREXIT are exercised too.
  const ERR_EXIT = keep(new NativeCallback(function (cinfo) {
    const e = cinfo.readPointer();
    e.add(0x18).writeU32((e.add(0x14).readU32() ^ 0x5a5a0000) >>> 0);
  }, 'void', ['pointer']));

  // A destination manager {next, free, init, empty, term} plus our own output buffer at dest+0x18 (past the
  // structure; EMPTY_OK reads it from there).
  function mkDest(emptyCb, free, bufSize) {
    const buf = alloc(bufSize, 0xcccccccc);
    const dest = alloc(0x20);
    dest.writePointer(buf);
    dest.add(4).writeU32(free);
    dest.add(0xc).writePointer(emptyCb);
    dest.add(0x18).writePointer(buf);
    return { dest: dest, buf: buf };
  }

  // A compress struct with err and dest; `mem` only when a buffer to hand out is given.
  function mkCinfo(size, err, dest, allocBuf) {
    const c = alloc(size);
    if (err) c.writePointer(err);
    if (dest) c.add(0x14).writePointer(dest);
    if (allocBuf) {
      const mem = alloc(0x10);
      mem.writePointer(ALLOC_CB);
      mem.add(8).writePointer(allocBuf);
      c.add(4).writePointer(mem);
    }
    return c;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    // ---- jpeg_std_error 0x004aec80 -----------------------------------------------------------------------
    // Twelve error managers of 0x84 bytes, pre-filled 0xAA so every field the function writes is an observable
    // change and the msg_parm union at +0x18..+0x64 (which it must NOT touch) is checked too. The return is the
    // argument, so the twelve buffers give twelve distinct return values.
    c3be_err() {
      const fx = {};
      for (let i = 0; i < 12; i++) {
        const e = alloc(0x84, 0xaaaaaaaa);
        e.add(0x18).writeU32(0x1000 + i);     // distinct marker inside msg_parm
        fx['e' + i] = e;
        if (i === 0) fx.obj = e;
      }
      return fx;
    },

    // ---- start_iMCU_row 0x004b0860 -----------------------------------------------------------------------
    // One cinfo + coef + component per scenario. coef is pre-filled 0x55 so the three stores are visible.
    // [comps_in_scan, total_iMCU_rows, iMCU_row_num, v_samp_factor, last_row_height]:
    //   0,1,2 take the signed `comps_in_scan > 1` branch (2, 3, 0x7FFFFFFF)
    //   3,6,8,11,12 take v_samp_factor; 4,5,7,9,10 take last_row_height
    //   6 (0) and 7 (-1) and 12 (-0x80000000) prove the compare at 0x004b0876 is signed
    //   8 (total 0) and 11 (total 0x80000000) prove the compare at 0x004b088a is unsigned
    c3be_coef() {
      const rows = [[2, 10, 3, 2, 5], [3, 10, 3, 2, 5], [0x7fffffff, 10, 3, 2, 5],
                    [1, 10, 3, 2, 5], [1, 10, 9, 2, 5], [1, 10, 100, 3, 7],
                    [0, 5, 0, 4, 6], [-1, 5, 4, 4, 6], [1, 0, 5, 8, 9],
                    [1, 1, 0, 11, 12], [1, 0xffffffff, 0xfffffffe, 13, 14], [1, 0x80000000, 3, 15, 16],
                    [-0x80000000, 2, 0, 17, 18]];
      const fx = {};
      rows.forEach(function (r, i) {
        const coef = alloc(0x20, 0x55555555);
        coef.add(8).writeU32(r[2] >>> 0);          // iMCU_row_num
        const comp = alloc(0x50);
        comp.add(0x0c).writeS32(r[3]);             // v_samp_factor
        comp.add(0x48).writeS32(r[4]);             // last_row_height
        const c = mkCinfo(0x180);
        c.add(0xe8).writeU32(r[1] >>> 0);          // total_iMCU_rows
        c.add(0xec).writeS32(r[0]);                // comps_in_scan
        c.add(0xf0).writePointer(comp);            // cur_comp_info[0]
        c.add(0x150).writePointer(coef);
        fx['c' + i] = c;
        fx['k' + i] = coef;
        if (i === 0) fx.obj = coef;
      });
      return fx;
    },

    // ---- add_huff_table 0x004ae8b0 -----------------------------------------------------------------------
    // Six (htblptr cell, bits, val) triples. `preset` points each cell at a pre-filled 0x118 table (the
    // 0x004ae8ba jne is taken); `alloc` leaves the cells NULL so the allocation at 0x004ae8c1 runs and stores
    // the buffer our alloc_small hands out. The tables are pre-filled 0xAA, so clearing sent_table at +0x114
    // and the two copies are observable; each triple carries different synthetic bytes.
    c3be_huff_preset() { return this._c3beHuff(false); },
    c3be_huff_alloc() { return this._c3beHuff(true); },
    _c3beHuff(nullCells) {
      const fx = {};
      for (let i = 0; i < 6; i++) {
        const tb = alloc(0x118, 0xaaaaaaaa);
        const cell = alloc(4);
        if (!nullCells) cell.writePointer(tb);
        const bits = alloc(0x14);
        for (let j = 0; j < 0x11; j++) bits.add(j).writeU8((i * 7 + j * 11) & 0xff);
        const val = alloc(0x100);
        for (let j = 0; j < 0x100; j++) val.add(j).writeU8((i * 31 + j * 3) & 0xff);
        fx['tb' + i] = tb;
        fx['hp' + i] = cell;
        fx['b' + i] = bits;
        fx['v' + i] = val;
        fx['c' + i] = mkCinfo(0x40, null, null, tb);
        if (i === 0) fx.obj = tb;
      }
      return fx;
    },

    // ---- emit_jfif_app0 0x004afc80 / emit_adobe_app14 0x004afd20 ------------------------------------------
    // One destination manager with 0x100 free bytes (the 18-byte APP0 and the 16-byte APP14 never reach the
    // empty_output_buffer path) and one cinfo per field combination.
    c3be_jfif() {
      const d = mkDest(EMPTY_OK, 0x100, 0x40);
      const fx = { obj: d.buf, buf: d.buf, dest: d.dest };
      const rows = [[0, 0, 0], [1, 72, 72], [2, 300, 300], [1, 1, 0xffff], [0xff, 0xffff, 0xffff],
                    [3, 0x1234, 0x5678], [0, 1, 2], [2, 0x100, 0x200], [0x80, 0x8000, 0x8001],
                    [7, 96, 0], [0, 0, 1], [0x55, 0xaaaa, 0x5555]];
      rows.forEach(function (r, i) {
        const c = mkCinfo(0x100, null, d.dest);
        c.add(0xcc).writeU8(r[0]);            // density_unit
        c.add(0xce).writeU16(r[1]);           // X_density
        c.add(0xd0).writeU16(r[2]);           // Y_density
        fx['c' + i] = c;
      });
      return fx;
    },
    c3be_adobe() {
      const d = mkDest(EMPTY_OK, 0x100, 0x40);
      const fx = { obj: d.buf, buf: d.buf, dest: d.dest };
      [0, 1, 2, 3, 4, 5, 6, 7, -1, 100].forEach(function (space, i) {
        const c = mkCinfo(0x100, null, d.dest);
        c.add(0x38).writeS32(space);          // jpeg_color_space
        fx['c' + i] = c;
      });
      return fx;
    },

    // ---- flush_bits 0x004b1b70 ---------------------------------------------------------------------------
    // working_state = {next_output_byte +0, free_in_buffer +4, cur.put_buffer +8, cur.put_bits +0xc,
    // cinfo +0x20}. One state object per (put_bits, put_buffer) pair; `free` and the destination manager's
    // empty_output_buffer select which branch of the buffer bookkeeping runs.
    _c3beWs(pairs, free, emptyCb) {
      const d = mkDest(emptyCb, 0x40, 0x40);
      const cinfo = mkCinfo(0x40, alloc(0x30), d.dest);
      const fx = { obj: d.buf, buf: d.buf, dest: d.dest };
      pairs.forEach(function (p, i) {
        const ws = alloc(0x24);
        ws.writePointer(d.buf.add(4 * i));     // next_output_byte: a slot of its own in the buffer
        ws.add(4).writeU32(free);
        ws.add(8).writeU32(p[1] >>> 0);        // cur.put_buffer
        ws.add(0xc).writeS32(p[0]);            // cur.put_bits
        ws.add(0x20).writePointer(cinfo);
        fx['w' + i] = ws;
      });
      return fx;
    },
    // Twelve states with room to spare: free_in_buffer = 0x40, so no vector reaches dump_buffer.
    //   put_bits 0 -> put_bits + 7 = 7 and the loop at 0x004b1b98 is skipped
    //   put_bits 1 with put_buffer 0x00800000 and put_bits 3 with 0x00ff0000 emit 0xFF (stuffed zero)
    c3be_ws() {
      return this._c3beWs([[0, 0x00123456], [0, 0xffffffff], [1, 0x00000000], [1, 0x00800000],
                           [2, 0x00000000], [3, 0x00ff0000], [4, 0x00800000], [5, 0x12345678],
                           [6, 0x00abcdef], [7, 0x00000000], [7, 0xffff0000], [3, 0x00010000]],
                          0x40, EMPTY_OK);
    },
    // free_in_buffer = 1: the first byte empties the buffer and dump_buffer 0x004b19e0 succeeds.
    c3be_ws_full() {
      return this._c3beWs([[1, 0x00000000], [2, 0x00000000], [3, 0x00000000], [4, 0x00120000],
                           [5, 0x00340000], [7, 0x00560000]], 1, EMPTY_OK);
    },
    // free_in_buffer = 2 with an emitted 0xFF: the stuffed zero empties the buffer (0x004b1bd8 path).
    c3be_ws_stuff() {
      return this._c3beWs([[1, 0x00800000], [3, 0x00ff0000], [2, 0x00c00000], [5, 0x00f80000]], 2, EMPTY_OK);
    },
    // empty_output_buffer returns FALSE: dump_buffer returns 0 and flush_bits returns 0 (0x004b1bbd).
    c3be_ws_fail() {
      return this._c3beWs([[1, 0x00000000], [2, 0x00000000], [4, 0x00120000], [7, 0x00560000]], 1, EMPTY_FAIL);
    },
    // Same, through the stuffed-zero check at 0x004b1be5 (free = 2, emitted byte 0xFF).
    c3be_ws_fail2() {
      return this._c3beWs([[1, 0x00800000], [3, 0x00ff0000], [5, 0x00f80000], [2, 0x00c00000]], 2, EMPTY_FAIL);
    },
    // put_bits >= 9: the loop runs more than once (back edge 0x004b1bf4). 18 and 25 also take the shift count
    // 24 - (put_bits + 7) negative, which `shl ebp, cl` resolves modulo 32.
    c3be_ws_loop() {
      return this._c3beWs([[9, 0x00ab0000], [10, 0x00000000], [12, 0x12345678], [16, 0x00ff0000],
                           [17, 0x00000000], [18, 0x0000abcd], [25, 0x00000000], [9, 0xffffffff]],
                          0x40, EMPTY_OK);
    },

    // ---- dump_buffer_4b2420 0x004b2420 -------------------------------------------------------------------
    // Twelve entropy objects, each with its own cinfo / destination manager / error manager, and each manager
    // seeded with different next_output_byte and free_in_buffer so the twelve results differ. The entropy
    // object keeps its copy of the two values at +0x10 and +0x14 (pre-filled 0x55).
    _c3bePhuff(emptyCb) {
      const fx = {};
      for (let i = 0; i < 12; i++) {
        const buf = alloc(0x40, 0xcccccccc);
        const dest = alloc(0x20);
        dest.writePointer(buf.add(i));                 // next_output_byte
        dest.add(4).writeU32(i * 7 + 1);               // free_in_buffer
        dest.add(0xc).writePointer(emptyCb);
        const err = alloc(0x30, 0x33333333);
        err.writePointer(ERR_EXIT);                    // error_exit
        const cinfo = mkCinfo(0x40, err, dest);
        const ent = alloc(0x30, 0x55555555);
        ent.add(0x20).writePointer(cinfo);
        fx['e' + i] = ent;
        fx['d' + i] = dest;
        fx['x' + i] = err;
        if (i === 0) fx.obj = ent;
      }
      return fx;
    },
    c3be_phuff() { return this._c3bePhuff(EMPTY_STEP); },
    c3be_phuff_err() { return this._c3bePhuff(EMPTY_FAIL); },

    // ---- netNameOf 0x0049aa30 ----------------------------------------------------------------------------
    // One object with the 16-slot table at +0x170 (stride 0x58, id at +0, name at +0x15). Slot 15 repeats
    // slot 1's id so the first match wins. The names are our own synthetic bytes.
    c3be_net() {
      const ids = [10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 20];
      const o = alloc(0x170 + 16 * 0x58, 0x77777777);
      ids.forEach(function (id, i) {
        const slot = o.add(0x170 + i * 0x58);
        slot.writeS32(id);
        for (let j = 0; j < 8; j++) slot.add(0x15 + j).writeU8(0x41 + ((i + j) % 26));
        slot.add(0x15 + 8).writeU8(0);
      });
      return { obj: o };
    },

    // ---- Surface_drawRegion 0x00473df0 -------------------------------------------------------------------
    // Three device objects on one fake vtable whose slot +0x5c records its arguments in `rec` and returns a
    // value built from them, so the two offsets this+0x20 / this+0x24 and the argument order are checked.
    // Surfaces: t* are the `this` side (m_4 device, m_8 lock flag, m_20/m_24 origin), s* the source side.
    c3be_surf() {
      const rec = alloc(0x20);
      const vt = alloc(0x80);
      vt.add(0x5c).writePointer(keep(new NativeCallback(function (self, src, x, y, mask) {
        const sid = self.add(4).readS32();
        const did = src.isNull() ? 0 : src.add(4).readS32();
        const mid = mask.isNull() ? 0 : mask.add(4).readS32();
        rec.writeS32(x); rec.add(4).writeS32(y);
        rec.add(8).writeS32(did); rec.add(0xc).writeS32(mid);
        rec.add(0x10).writeS32(sid);
        return (Math.imul(x, 1000) + y + sid * 7 + did * 13 + mid * 17) | 0;
      }, 'int', ['pointer', 'pointer', 'int', 'int', 'pointer'], 'thiscall')));
      const dev = function (id) { const d = alloc(0x10); d.writePointer(vt); d.add(4).writeS32(id); return d; };
      const d0 = dev(1), d1 = dev(2), d2 = dev(3);
      const surf = function (m4, m8, m20, m24) {
        const s = alloc(0x30);
        if (m4) s.add(4).writePointer(m4);
        s.add(8).writeS32(m8);
        s.add(0x20).writeS32(m20);
        s.add(0x24).writeS32(m24);
        return s;
      };
      return { obj: rec, rec: rec,
               t_ok: surf(d0, 0, 10, 20), t_ok2: surf(d0, 0, -5, 7),
               t_busy: surf(d0, 1, 10, 20), t_busy2: surf(d0, -1, 10, 20), t_nodev: surf(null, 0, 10, 20),
               s_ok: surf(d1, 0, 0, 0), s_ok2: surf(d2, 0, 0, 0), s_nodev: surf(null, 0, 0, 0),
               m_ok: surf(d2, 0, 0, 0) };
    },

    // ---- DC_call84 0x00475b60 ----------------------------------------------------------------------------
    // One fake device vtable with slot +0x84; the handles carry an integer payload at +4, which the original
    // passes as the first argument.
    c3be_dc() {
      const rec = alloc(0x20);
      const vt = alloc(0x100);
      vt.add(0x84).writePointer(keep(new NativeCallback(function (self, payload, a, b) {
        const id = self.add(4).readS32();
        rec.writeS32(payload); rec.add(4).writeS32(a); rec.add(8).writeS32(b); rec.add(0xc).writeS32(id);
        return (Math.imul(a, 31) + b + id * 7 + payload) | 0;
      }, 'int', ['pointer', 'int', 'int', 'int'], 'thiscall')));
      const dev = function (id) { const d = alloc(0x10); d.writePointer(vt); d.add(4).writeS32(id); return d; };
      const holder = function (m4) { const o = alloc(0x10); if (m4) o.add(4).writePointer(m4); return o; };
      const handle = function (v) { const h = alloc(0x10); h.add(4).writeS32(v); return h; };
      return { obj: rec, rec: rec,
               d_ok: holder(dev(1)), d_ok2: holder(dev(2)), d_nodev: holder(null),
               h0: handle(0x11), h1: handle(0x22), h2: handle(-3), h3: handle(0), h4: handle(0x1234) };
    },

    // ---- Surface_box 0x00475b20 --------------------------------------------------------------------------
    // Slot +0x34 of the device vtable receives the RECT the function builds on its stack; the callback reads
    // all four members, so left/top/right/bottom and the two additions are checked.
    c3be_box() {
      const rec = alloc(0x20);
      const vt = alloc(0x80);
      vt.add(0x34).writePointer(keep(new NativeCallback(function (self, r) {
        const l = r.readS32(), t = r.add(4).readS32(), ri = r.add(8).readS32(), b = r.add(0xc).readS32();
        rec.writeS32(l); rec.add(4).writeS32(t); rec.add(8).writeS32(ri); rec.add(0xc).writeS32(b);
        return (l + Math.imul(t, 3) + Math.imul(ri, 5) + Math.imul(b, 7)) | 0;
      }, 'int', ['pointer', 'pointer'], 'thiscall')));
      const d = alloc(0x10); d.writePointer(vt); d.add(4).writeS32(9);
      const holder = function (m4) { const o = alloc(0x10); if (m4) o.add(4).writePointer(m4); return o; };
      return { obj: rec, rec: rec, b_ok: holder(d), b_nodev: holder(null) };
    },
  });
})();
