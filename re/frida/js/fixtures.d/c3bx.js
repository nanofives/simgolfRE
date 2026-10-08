// Fixtures for C3 batch c3bx (round 18, golf_clean.exe): dump_buffer 0x004b19e0, jpeg_set_quality 0x004ae740,
// StrList::dtor 0x00487260, releaseWithHookC 0x00487c40, comboListQuery 0x00493ed0, closeHookA 0x00487430,
// closeHookB 0x004879f0 and the four Window forwarders 0x0047b9f0 / 0x0047ba10 / 0x0047ba30 / 0x0047ba50.
// Only our own synthetic values (no game text, no live game object). Names are prefixed c3bx_.
//
// Allocations go through Memory.alloc (diff_hook rewrites it to __keepAlloc so blocks referenced only from a
// field survive the run); NativeCallbacks are pushed onto __diffKeepAlive by hand.
//
// Every callback is a pure function of its arguments or of memory diff_hook snapshots and restores between the
// two arms, so the second arm sees exactly the world the first one saw.
//
// Two live globals are written by these fixtures, both restored by diff_hook because they are state regions:
//   0x0083af80 / 0x0083af88  the close hooks closeHookA / closeHookB call. Their only reader is those two
//                            functions, reached only from shutdownSound 0x00483f10 (game shutdown), so no
//                            running menu code calls them while the A/B runs.
//   0x0083ab2c               g_curWidget, which setWidgetScalar 0x004967f0 overwrites with its widget's
//                            field +0x130. Every synthetic widget below carries the CURRENT value of
//                            g_curWidget in +0x130, so that store writes back the value the game already had.
(function () {
  function keep(cb) { globalThis.__diffKeepAlive.push(cb); return cb; }
  function zero(p, n) { for (let i = 0; i < n; i += 4) p.add(i).writeU32(0); }
  function fill(p, n, v) { for (let i = 0; i < n; i += 4) p.add(i).writeU32(v); }
  function alloc(n, v) { const p = Memory.alloc(n); if (v === undefined) zero(p, n); else fill(p, n, v); return p; }

  // A rolling hash recorder: rec+0 the hash, rec+4 the number of calls, rec+8 the last value pushed. It makes
  // the ORDER and the arguments of every dispatched callback part of the compared state.
  function mkRec() { return alloc(0x40); }
  function push(rec, v) {
    let h = rec.readU32();
    h = (Math.imul(h, 33) + (v >>> 0)) >>> 0;
    rec.writeU32(h);
    rec.add(4).writeU32((rec.add(4).readU32() + 1) >>> 0);
    rec.add(8).writeU32(v >>> 0);
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    // ---- dump_buffer 0x004b19e0 ----------------------------------------------------------------------------
    // One working_state (0x24: next_output_byte +0, free_in_buffer +4, cur.put_buffer +8, cur.put_bits +0xc,
    // cur.last_dc_val[4] +0x10, cinfo +0x20), one cinfo (only +0x14 is read), one destination manager
    // (next_output_byte +0, free_in_buffer +4, empty_output_buffer +0xc, our buffer at +0x18 and our per-vector
    // seed at +0x1c) and one 0x40-byte output buffer per vector. The working state is pre-filled 0x6b so the two
    // copies at 0x004b19fc / 0x004b1a01 are visible, and the cur / last_dc_val bytes the function must NOT
    // touch are compared too.
    _c3bxDump(rows, ok) {
      // empty_output_buffer(cinfo) = TRUE: refill the manager from the buffer stashed at dest+0x18, by the
      // amount stashed at dest+0x1c, so every vector gets a different pair of values copied back.
      const EMPTY_OK = keep(new NativeCallback(function (cinfo) {
        const d = cinfo.add(0x14).readPointer();
        const k = d.add(0x1c).readU32() & 0x1f;
        d.writePointer(d.add(0x18).readPointer().add(k));
        d.add(4).writeU32(0x40 - k);
        return 1;
      }, 'int', ['pointer']));
      // empty_output_buffer(cinfo) = FALSE: record the call in the manager's spare dword at +0x20 and refuse.
      const EMPTY_FAIL = keep(new NativeCallback(function (cinfo) {
        const d = cinfo.add(0x14).readPointer();
        d.add(0x20).writeU32((d.add(0x20).readU32() + 0x101) >>> 0);
        return 0;
      }, 'int', ['pointer']));
      const fx = {};
      rows.forEach(function (r, i) {
        const buf = alloc(0x40, 0xcccccccc);
        const dest = alloc(0x30, 0);
        dest.writePointer(buf.add(r.dnext));
        dest.add(4).writeU32(r.dfree);
        dest.add(0xc).writePointer(ok ? EMPTY_OK : EMPTY_FAIL);
        dest.add(0x18).writePointer(buf);
        dest.add(0x1c).writeU32(r.k);
        const c = alloc(0x40, 0);
        c.add(0x14).writePointer(dest);
        const ws = alloc(0x24, 0x6b6b6b6b);
        ws.writePointer(buf.add(r.wnext));
        ws.add(4).writeU32(r.wfree);
        ws.add(0x20).writePointer(c);
        fx['w' + i] = ws;
        fx['d' + i] = dest;
        if (i === 0) fx.obj = ws;
      });
      return fx;
    },
    // empty_output_buffer returns TRUE: the jne at 0x004b19f5 is taken and both copies run.
    c3bx_dump_ok() {
      return this._c3bxDump([
        { wnext: 0, wfree: 0, dnext: 0, dfree: 0, k: 0 },
        { wnext: 4, wfree: 1, dnext: 8, dfree: 0x38, k: 1 },
        { wnext: 0x3f, wfree: 0, dnext: 0x20, dfree: 0x20, k: 7 },
        { wnext: 0x10, wfree: 0x30, dnext: 0, dfree: 0x40, k: 0x1f },
        { wnext: 0, wfree: 0xffffffff, dnext: 0x3f, dfree: 1, k: 0x10 },
        { wnext: 0x20, wfree: 0x12345678, dnext: 0x10, dfree: 0x30, k: 3 },
        { wnext: 8, wfree: 2, dnext: 4, dfree: 0x3c, k: 0x1e },
        { wnext: 0x3c, wfree: 4, dnext: 0x3c, dfree: 4, k: 0x1a },
      ], true);
    },
    // empty_output_buffer returns FALSE: the jne at 0x004b19f5 is not taken, the function returns 0 at
    // 0x004b19f9 and the working state keeps its 0x6b fill in +0 and +4.
    c3bx_dump_fail() {
      return this._c3bxDump([
        { wnext: 0, wfree: 0, dnext: 0, dfree: 0, k: 0 },
        { wnext: 0x10, wfree: 5, dnext: 0x20, dfree: 0x20, k: 2 },
        { wnext: 0x3f, wfree: 1, dnext: 0, dfree: 0x40, k: 0x11 },
        { wnext: 0, wfree: 0xffffffff, dnext: 0x3f, dfree: 1, k: 0x1f },
        { wnext: 0x18, wfree: 0x55, dnext: 0x18, dfree: 0x28, k: 9 },
        { wnext: 0x24, wfree: 0x7fffffff, dnext: 4, dfree: 0x3c, k: 0x15 },
      ], false);
    },

    // ---- jpeg_set_quality 0x004ae740 ------------------------------------------------------------------------
    // One jpeg_compress_struct (err +0, mem +4, global_state +0x10, quant_tbl_ptrs[4] +0x40), one error manager
    // and the two JQUANT_TBLs (quantval[64] UINT16 at +0, sent_table at +0x80, 0x84 bytes) the two
    // jpeg_add_quant_table 0x004ae600 calls fill. All of them are state regions, so each vector starts from the
    // same snapshot; the tables are pre-filled 0x77 so every one of the 128 writes shows.
    // error_exit(cinfo) records the message code and its first parameter and returns, so the instructions after
    // the ERREXIT run in both arms.
    _c3bxQuality(opts) {
      const ERR_REC = keep(new NativeCallback(function (cinfo) {
        const e = cinfo.readPointer();
        let h = e.add(0x30).readU32();
        h = (Math.imul(h, 33) + e.add(0x14).readU32()) >>> 0;
        h = (Math.imul(h, 33) + e.add(0x18).readU32()) >>> 0;
        e.add(0x30).writeU32(h);
        e.add(0x34).writeU32((e.add(0x34).readU32() + 1) >>> 0);
      }, 'void', ['pointer']));
      // alloc_small(cinfo, pool_id, size): a bump allocator over this cinfo's pool (cursor at pool+0, blocks
      // from pool+0x10, 0x10-aligned). Both arms get the same pointers because the pool is a state region and
      // is restored between them.
      const ALLOC_BUMP = keep(new NativeCallback(function (cinfo, pool, size) {
        const p = cinfo.add(4).readPointer().add(0x10).readPointer();
        const cur = p.readU32();
        p.writeU32((cur + ((size + 15) & ~15)) >>> 0);
        return p.add(0x10 + cur);
      }, 'pointer', ['pointer', 'int', 'int']));

      const err = alloc(0x84, 0x22222222);
      err.writePointer(ERR_REC);
      zero(err.add(0x30), 0x10);
      const pool = alloc(0x400, 0);
      const mem = alloc(0x20, 0);
      mem.writePointer(ALLOC_BUMP);
      mem.add(0x10).writePointer(pool);
      const c = alloc(0x180, 0);
      c.writePointer(err);
      c.add(4).writePointer(mem);
      c.add(0x10).writeS32(opts.state);
      const fx = { obj: c, c: c, err: err, pool: pool };
      if (opts.preset) {
        const q0 = alloc(0x84, 0x77777777);
        const q1 = alloc(0x84, 0x77777777);
        c.add(0x40).writePointer(q0);
        c.add(0x44).writePointer(q1);
        fx.q0 = q0;
        fx.q1 = q1;
      }
      return fx;
    },
    // global_state 100 (CSTATE_START) with both tables already allocated: the je at 0x004ae612 is taken (no
    // ERREXIT) and the jne at 0x004ae633 is taken (no allocation).
    c3bx_quality() { return this._c3bxQuality({ state: 100, preset: true }); },
    // The same with quant_tbl_ptrs[0] and [1] NULL: the jne at 0x004ae633 is not taken, so
    // jpeg_alloc_quant_table 0x004afab0 runs twice and the two pointers are stored at 0x004ae63e.
    c3bx_quality_alloc() { return this._c3bxQuality({ state: 100, preset: false }); },
    // global_state 0x63: the je at 0x004ae612 is not taken, so both jpeg_add_quant_table calls raise
    // ERREXIT1 0x12 (0x004ae617 / 0x004ae623 / 0x004ae628) before filling the table.
    c3bx_quality_badstate() { return this._c3bxQuality({ state: 0x63, preset: true }); },

    // ---- StrList::dtor 0x00487260 ---------------------------------------------------------------------------
    // Twelve objects of 0x20 bytes pre-filled 0x3b (so the vtable store of 0x004baeac at 0x00487260 is a visible
    // change in every one of them) with a child pointer at +0x14. Each child carries a vtable whose slot +0x14
    // is our recorder; the children themselves are never written, so only the objects and the recorder are
    // compared.
    c3bx_strlist() {
      const rec = mkRec();
      const RELEASE = keep(new NativeCallback(function (self) {
        push(rec, self.add(4).readS32());
        return (self.add(4).readS32() * 7 + 1) | 0;
      }, 'int', ['pointer'], 'thiscall'));
      const vt = alloc(0x40, 0);
      vt.add(0x14).writePointer(RELEASE);
      const child = function (id) { const k = alloc(0x20, 0); k.writePointer(vt); k.add(4).writeS32(id); return k; };
      const kids = [child(1), child(2), child(3), child(-7), child(0x1234), child(0)];
      const obj = function (k) {
        const o = alloc(0x20, 0x3b3b3b3b);
        o.add(0x14).writePointer(k === null ? ptr(0) : k);
        return o;
      };
      const fx = { obj: rec, rec: rec };
      // Three with a null child (the je at 0x0048726b taken) and nine with children of six distinct ids.
      [null, kids[0], kids[1], null, kids[2], kids[3], kids[4], null, kids[5], kids[0], kids[3], kids[4]]
        .forEach(function (k, i) { fx['o' + i] = obj(k); });
      return fx;
    },

    // ---- releaseWithHookC 0x00487c40 -------------------------------------------------------------------------
    // Twelve objects of 0x20 bytes: vtable at +0, an id at +4 and the child at +0x14. The child's vtable slot
    // +0x14 is our release callback; the object's own vtable slot +8 is our notify callback. Two vtables carry
    // DIFFERENT notify callbacks, and a child whose flag at +0xc is set swaps its parent's vtable pointer (the
    // parent is stashed at child+8) to the second one: that is how the re-read of [esi] at 0x00487c4f, after the
    // first call has returned, is made observable.
    c3bx_relc() {
      const rec = mkRec();
      const vtMain = alloc(0x40, 0);
      const vtAlt = alloc(0x40, 0);
      const NOTIFY_MAIN = keep(new NativeCallback(function (self) {
        push(rec, 0x1000 + self.add(4).readS32());
      }, 'void', ['pointer'], 'thiscall'));
      const NOTIFY_ALT = keep(new NativeCallback(function (self) {
        push(rec, 0x2000 + self.add(4).readS32());
      }, 'void', ['pointer'], 'thiscall'));
      vtMain.add(8).writePointer(NOTIFY_MAIN);
      vtAlt.add(8).writePointer(NOTIFY_ALT);
      const RELEASE = keep(new NativeCallback(function (self) {
        push(rec, self.add(4).readS32());
        if (self.add(0xc).readS32() !== 0) {
          const parent = self.add(8).readPointer();
          if (!parent.isNull()) parent.writePointer(vtAlt);
        }
        return 0;
      }, 'int', ['pointer'], 'thiscall'));
      const vtChild = alloc(0x40, 0);
      vtChild.add(0x14).writePointer(RELEASE);
      const fx = { obj: rec, rec: rec };
      // [child id, swap-the-parent-vtable flag] or null for no child.
      [null, [1, 0], [2, 0], [3, 1], null, [4, 1], [5, 0], [-9, 0], null, [6, 1], [7, 0], [0x55, 0]]
        .forEach(function (r, i) {
          const o = alloc(0x20, 0x8c8c8c8c);
          o.writePointer(vtMain);
          o.add(4).writeS32(i);
          if (r === null) {
            o.add(0x14).writePointer(ptr(0));
          } else {
            const k = alloc(0x20, 0);
            k.writePointer(vtChild);
            k.add(4).writeS32(r[0]);
            k.add(8).writePointer(o);
            k.add(0xc).writeS32(r[1]);
            o.add(0x14).writePointer(k);
          }
          fx['o' + i] = o;
        });
      return fx;
    },

    // ---- comboListQuery 0x00493ed0 ---------------------------------------------------------------------------
    // Twelve objects of 0x23c0 bytes with the sub-object pointer at +0x23b4. Nine sub-objects carry a vtable
    // whose slot +0xd8 returns a value derived from the sub-object's id, so the tail jump at 0x00493edc gives
    // nine distinct return values; three objects hold a null pointer and return 0.
    c3bx_combo() {
      const rec = mkRec();
      const QUERY = keep(new NativeCallback(function (self) {
        const id = self.add(4).readS32();
        push(rec, id);
        return (Math.imul(id, 1000) + 7) | 0;
      }, 'int', ['pointer'], 'thiscall'));
      const vt = alloc(0x100, 0);
      vt.add(0xd8).writePointer(QUERY);
      const sub = function (id) { const s = alloc(0x20, 0); s.writePointer(vt); s.add(4).writeS32(id); return s; };
      const subs = [sub(1), sub(2), sub(-3), sub(0x7f), sub(0), sub(0x123), sub(-0x4000), sub(9), sub(0x2a)];
      const fx = { obj: rec, rec: rec };
      [null, subs[0], subs[1], subs[2], null, subs[3], subs[4], subs[5], null, subs[6], subs[7], subs[8]]
        .forEach(function (s, i) {
          const o = alloc(0x23c0, 0);
          o.add(0x23b4).writePointer(s === null ? ptr(0) : s);
          fx['o' + i] = o;
        });
      return fx;
    },

    // ---- closeHookA 0x00487430 / closeHookB 0x004879f0 -------------------------------------------------------
    // Objects of 0x20 bytes pre-filled 0xa5 with the handle at +0x14. The global hook (0x0083af80 for closeHookA,
    // 0x0083af88 for closeHookB) is written here and is a state region of both keys, so diff_hook restores it
    // between the two arms and after the run. The hook is called with no arguments and no stack cleanup
    // (0x00487443 / 0x00487a03), so the callback takes none.
    _c3bxClose(global, hooked) {
      const rec = mkRec();
      const HOOK = keep(new NativeCallback(function () { push(rec, 0x5a5a); }, 'void', []));
      ptr(global).writePointer(hooked ? HOOK : ptr(0));
      const fx = { obj: rec, rec: rec };
      // Handle 0 takes the je at 0x00487438 / 0x004879f8; every other value falls through to the hook test.
      const handles = hooked ? [1, 0, -1, 0x1234, 0x7fffffff, 0] : [1, 0, -1, 0x55aa, 0];
      handles.forEach(function (h, i) {
        const o = alloc(0x20, 0xa5a5a5a5);
        o.add(0x14).writeS32(h);
        fx['o' + i] = o;
      });
      return fx;
    },
    c3bx_closeA_set() { return this._c3bxClose(0x0083af80, true); },
    c3bx_closeA_null() { return this._c3bxClose(0x0083af80, false); },
    c3bx_closeB_set() { return this._c3bxClose(0x0083af88, true); },
    c3bx_closeB_null() { return this._c3bxClose(0x0083af88, false); },

    // ---- the four Window forwarders 0x0047b9f0 / 0x0047ba10 / 0x0047ba30 / 0x0047ba50 -------------------------
    // Six synthetic widgets and six synthetic windows, shared by all four keys.
    // A widget is 0x5a0 bytes: vtable +0, an id +4, g_curWidget's current value at +0x130 (setWidgetScalar
    // 0x004967f0 copies it to 0x0083ab2c at 0x004967fb, so that store writes back what the game already had),
    // the low bound +0x580, the high bound +0x584, the reverse flag +0x588 and the result +0x58c. Vtable slot
    // +0x120 is the redraw both callees dispatch through (0x0049684c, 0x004967c3) with the widget in ecx and no
    // arguments; our callback records the widget id and all four of +0x580..+0x58c, so the order of the calls
    // and everything the callee computed is part of the compared state.
    // A window is 0x300 bytes with the two child slots at +0x26c and +0x270.
    c3bx_fwd() {
      const rec = alloc(0x80, 0);
      const REDRAW = keep(new NativeCallback(function (self) {
        let h = rec.readU32();
        [4, 0x580, 0x584, 0x588, 0x58c].forEach(function (off) {
          h = (Math.imul(h, 33) + self.add(off).readU32()) >>> 0;
        });
        rec.writeU32(h);
        rec.add(4).writeU32((rec.add(4).readU32() + 1) >>> 0);
        rec.add(8).writeU32(self.add(0x58c).readU32());
        rec.add(0xc).writeU32(self.add(0x588).readU32());
        return 0;
      }, 'int', ['pointer'], 'thiscall'));
      const vt = alloc(0x140, 0);
      vt.add(0x120).writePointer(REDRAW);
      const curWidget = ptr(0x0083ab2c).readPointer();
      const widget = function (id, lo, hi, flag) {
        const w = alloc(0x5a0, 0);
        w.writePointer(vt);
        w.add(4).writeS32(id);
        w.add(0x130).writePointer(curWidget);
        w.add(0x580).writeS32(lo);
        w.add(0x584).writeS32(hi);
        w.add(0x588).writeS32(flag);
        w.add(0x58c).writeU32(0x11111111);
        return w;
      };
      const g0 = widget(1, 10, 90, 0);              // ordinary range, reverse flag clear
      const g1 = widget(2, -5, 5, 0);               // range across zero
      const g2 = widget(3, 0, 0, 1);                // empty range, reverse flag set
      const g3 = widget(4, 100, 50, 0);             // low bound above high bound
      const g4 = widget(5, -1000, 1000, 1);         // wide range, reverse flag set
      const g5 = widget(6, 0, 0x7fffffff, 0);       // high bound at INT_MAX
      const win = function (a, b) {
        const w = alloc(0x300, 0);
        w.add(0x26c).writePointer(a === null ? ptr(0) : a);
        w.add(0x270).writePointer(b === null ? ptr(0) : b);
        return w;
      };
      return { obj: rec, rec: rec, g0: g0, g1: g1, g2: g2, g3: g3, g4: g4, g5: g5,
               wa: win(g0, g1), wb: win(g2, g3), wc: win(g4, g5),
               wn: win(null, null), w26: win(g1, null), w27: win(null, g0) };
    },
  });
})();
