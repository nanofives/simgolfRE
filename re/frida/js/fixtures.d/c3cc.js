// Fixtures for C3 batch c3cc (round 19, golf_clean.exe): releaseWithHookA 0x00487460,
// releaseWithHookB 0x00487ac0, closeHookC 0x00487bd0, orderPair 0x00496770,
// scrollbarResetThumb 0x004967d0, jpeg_set_linear_quality 0x004ae6c0, out_of_memory 0x004af0a0,
// jpeg_open_backing_store 0x004b0490, jpeg_mem_available 0x004b0480 and jpeg_destroy_compress 0x004ae210.
// Only our own synthetic values (no game text, no live game object). Names are prefixed c3cc_.
//
// Allocations go through Memory.alloc (diff_hook rewrites it to __keepAlloc so blocks referenced only from a
// field survive the run); NativeCallbacks are pushed onto __diffKeepAlive by hand.
//
// Every callback is a pure function of its arguments or of memory diff_hook snapshots and restores between
// the two arms, so the second arm sees exactly the world the first one saw.
//
// Three live globals are written by these fixtures, all three state regions of the keys that use them:
//   0x0083af80 / 0x0083af88  the close hooks closeHookA 0x00487430 / closeHookB 0x004879f0 call. Their only
//                            readers are those two functions, reached only from releaseWithHookA /
//                            releaseWithHookB <- shutdownSound 0x00483f10 (game shutdown), so no running menu
//                            code calls them while the A/B runs.
//   0x0083af90               the hook closeHookC 0x00487bd0 calls. 0x00487bdb is the ONLY instruction in the
//                            image that references this global (scan of golf_clean.exe for the literal
//                            0x0083af90), so nothing else reads or writes it.
// The three error-raising libjpeg keys install an error_exit that RECORDS and RETURNS, so the instructions
// after the ERREXIT run in both arms (round 19: c3aa's six libjpeg ERREXIT calls were wrong for a week
// because no fixture ever let error_exit return).
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
    // ---- releaseWithHookA 0x00487460 / releaseWithHookB 0x00487ac0 ------------------------------------------
    // Eight objects of 0x20 bytes pre-filled 0xa7 with the child pointer at +0x14. Three hold a null child (the
    // je at 0x00487468 / 0x00487ac8 taken); the other five hold children of five distinct ids whose vtable slot
    // +0x14 is our release callback. The callback records its id, so the ORDER of the two dispatched calls is
    // part of the compared state: the child's slot +0x14 first, then the global close hook that
    // closeHookA 0x00487430 / closeHookB 0x004879f0 calls at 0x00487443 / 0x00487a03 - which still sees the
    // child pointer in +0x14 (its handle test at 0x00487433 / 0x004879f3) because +0x14 is only cleared
    // afterwards, at 0x00487476 / 0x00487ad6.
    _c3ccRelease(global, hooked) {
      const rec = mkRec();
      const RELEASE = keep(new NativeCallback(function (self) {
        const id = self.add(4).readS32();
        push(rec, id);
        return (id * 3 + 1) | 0;   // the caller discards it (0x0048746c / 0x00487acc)
      }, 'int', ['pointer'], 'thiscall'));
      // The global close hook takes no arguments and the caller does not clean the stack (0x00487443).
      const HOOK = keep(new NativeCallback(function () { push(rec, 0x7e7e); }, 'void', []));
      ptr(global).writePointer(hooked ? HOOK : ptr(0));
      const vtChild = alloc(0x40, 0);
      vtChild.add(0x14).writePointer(RELEASE);
      const fx = { obj: rec, rec: rec };
      [null, 1, 2, null, -9, 0x1234, null, 0x55]
        .forEach(function (id, i) {
          const o = alloc(0x20, 0xa7a7a7a7);
          if (id === null) {
            o.add(0x14).writePointer(ptr(0));
          } else {
            const k = alloc(0x20, 0);
            k.writePointer(vtChild);
            k.add(4).writeS32(id);
            o.add(0x14).writePointer(k);
          }
          fx['o' + i] = o;
        });
      return fx;
    },
    // The global hook is set: the non-null-child vectors run both dispatched calls and clear +0x14 twice (once
    // inside closeHook* at 0x00487445 / 0x00487a05, once at 0x00487476 / 0x00487ad6).
    c3cc_relA_set() { return this._c3ccRelease(0x0083af80, true); },
    // The global hook is NULL: closeHookA / closeHookB take the je at 0x00487441 / 0x00487a01 and return 0x14
    // without clearing +0x14, which releaseWithHook* then clears itself.
    c3cc_relA_null() { return this._c3ccRelease(0x0083af80, false); },
    c3cc_relB_set() { return this._c3ccRelease(0x0083af88, true); },
    c3cc_relB_null() { return this._c3ccRelease(0x0083af88, false); },

    // ---- closeHookC 0x00487bd0 -------------------------------------------------------------------------------
    // Objects of 0x20 bytes pre-filled 0xa5 with the handle at +0x14. The global 0x0083af90 is written here and
    // is a state region of both keys, so diff_hook restores it between the two arms and after the run. The hook
    // is called with no arguments and no stack cleanup (0x00487be3), so the callback takes none.
    _c3ccCloseC(hooked) {
      const rec = mkRec();
      const HOOK = keep(new NativeCallback(function () { push(rec, 0x3c3c); }, 'void', []));
      ptr(0x0083af90).writePointer(hooked ? HOOK : ptr(0));
      const fx = { obj: rec, rec: rec };
      // Handle 0 takes the je at 0x00487bd8; every other value falls through to the hook test at 0x00487bdf.
      const handles = hooked ? [1, 0, -1, 0x1234, 0x7fffffff, 0] : [1, 0, -1, 0x55aa, 0];
      handles.forEach(function (h, i) {
        const o = alloc(0x20, 0xa5a5a5a5);
        o.add(0x14).writeS32(h);
        fx['o' + i] = o;
      });
      return fx;
    },
    c3cc_closeC_set() { return this._c3ccCloseC(true); },
    c3cc_closeC_null() { return this._c3ccCloseC(false); },

    // ---- orderPair 0x00496770 --------------------------------------------------------------------------------
    // Three synthetic widgets of 0x5a0 bytes: vtable +0, an id +4, the low bound +0x580, the high bound +0x584,
    // the swapped flag +0x588 and the result +0x58c, all four pre-filled 0x11111111 so every store shows.
    // Vtable slot +0x120 is the redraw orderPair dispatches through at 0x004967c3 with the widget in ecx and no
    // arguments; our callback records the widget id and all four dwords (so the values the function computed are
    // part of the compared state) and returns a value derived from them, which is orderPair's own return value.
    c3cc_order() {
      const rec = mkRec();
      const REDRAW = keep(new NativeCallback(function (self) {
        [4, 0x580, 0x584, 0x588, 0x58c].forEach(function (off) { push(rec, self.add(off).readU32()); });
        return (Math.imul(self.add(0x580).readS32(), 31) + self.add(0x588).readS32()) | 0;
      }, 'int', ['pointer'], 'thiscall'));
      const vt = alloc(0x140, 0);
      vt.add(0x120).writePointer(REDRAW);
      const widget = function (id) {
        const w = alloc(0x5a0, 0);
        w.writePointer(vt);
        w.add(4).writeS32(id);
        [0x580, 0x584, 0x588, 0x58c].forEach(function (off) { w.add(off).writeU32(0x11111111); });
        return w;
      };
      return { obj: rec, rec: rec, g0: widget(1), g1: widget(2), g2: widget(3) };
    },

    // ---- scrollbarResetThumb 0x004967d0 -----------------------------------------------------------------------
    // Six synthetic widgets of 0x600 bytes. +0x57c decides scrollbarResetState's branch at 0x004979c0 (-1 ends
    // it at 0x004979f7, anything else runs the second half); +0x5ac..+0x5c0 are pre-filled 0x9b so both the
    // store of the caller's dword into +0x5c0 (0x004967d4) and the four stores the callee makes show.
    c3cc_scroll() {
      const fx = {};
      [-1, -1, 0, 1, -2, 0x7fffffff].forEach(function (mode, i) {
        const w = alloc(0x600, 0);
        fill(w.add(0x57c), 0x48, 0x9b9b9b9b);
        w.add(0x57c).writeS32(mode);
        fx['w' + i] = w;
        if (i === 0) fx.obj = w;
      });
      return fx;
    },

    // ---- jpeg_set_linear_quality 0x004ae6c0 -------------------------------------------------------------------
    // One jpeg_compress_struct (err +0, mem +4, global_state +0x10, quant_tbl_ptrs[4] +0x40), one error manager
    // and the two JQUANT_TBLs (quantval[64] UINT16 at +0, sent_table at +0x80, 0x84 bytes) the two
    // jpeg_add_quant_table 0x004ae600 calls fill. All of them are state regions, so each vector starts from the
    // same snapshot; the tables are pre-filled 0x77 so every one of the 128 writes shows.
    // error_exit(cinfo) records the message code and its first parameter and RETURNS, so the instructions after
    // the ERREXIT at 0x004ae628 run in both arms.
    _c3ccQuality(opts) {
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
    c3cc_linq() { return this._c3ccQuality({ state: 100, preset: true }); },
    // The same with quant_tbl_ptrs[0] and [1] NULL: the jne at 0x004ae633 is not taken, so
    // jpeg_alloc_quant_table 0x004afab0 runs twice and the two pointers are stored at 0x004ae63e.
    c3cc_linq_alloc() { return this._c3ccQuality({ state: 100, preset: false }); },
    // global_state 0x63: the je at 0x004ae612 is not taken, so both jpeg_add_quant_table calls raise
    // ERREXIT1 0x12 (0x004ae617 / 0x004ae623 / 0x004ae628) and, with this returning handler, go on to fill
    // the table anyway.
    c3cc_linq_badstate() { return this._c3ccQuality({ state: 0x63, preset: true }); },

    // ---- out_of_memory 0x004af0a0 ------------------------------------------------------------------------------
    // One compress object and one error manager pre-filled 0x22 (so the stores of 0x35 into err+0x14 at
    // 0x004af0a7 and of the caller's second dword into err+0x18 at 0x004af0b4 both show). error_exit at err+0
    // records the message code and the parameter and RETURNS; the whole error manager is the state region, so
    // each of the twelve `which` values leaves a different final state.
    c3cc_oom() {
      const ERR_REC = keep(new NativeCallback(function (cinfo) {
        const e = cinfo.readPointer();
        let h = e.add(0x30).readU32();
        h = (Math.imul(h, 33) + e.add(0x14).readU32()) >>> 0;
        h = (Math.imul(h, 33) + e.add(0x18).readU32()) >>> 0;
        e.add(0x30).writeU32(h);
        e.add(0x34).writeU32((e.add(0x34).readU32() + 1) >>> 0);
      }, 'void', ['pointer']));
      const err = alloc(0x84, 0x22222222);
      err.writePointer(ERR_REC);
      zero(err.add(0x30), 0x10);
      const c = alloc(0x40, 0);
      c.writePointer(err);
      return { obj: c, c: c, err: err };
    },

    // ---- jpeg_open_backing_store 0x004b0490 ---------------------------------------------------------------------
    // Ten compress objects, each with its own error manager carrying a distinct seed at +0x40 and a distinct
    // pre-fill of +0x14, so the store of 0x30 at 0x004b0497 shows against a different background every time and
    // every vector leaves a different final state. error_exit at err+0 records the message code and the seed and
    // RETURNS, so the `pop ecx` at 0x004b04a2 runs in both arms. The second and third arguments are given
    // distinct values although no instruction of the function reads them.
    c3cc_obs() {
      const ERR_REC = keep(new NativeCallback(function (cinfo) {
        const e = cinfo.readPointer();
        let h = e.add(0x30).readU32();
        h = (Math.imul(h, 33) + e.add(0x14).readU32()) >>> 0;
        h = (Math.imul(h, 33) + e.add(0x40).readU32()) >>> 0;
        e.add(0x30).writeU32(h);
        e.add(0x34).writeU32((e.add(0x34).readU32() + 1) >>> 0);
      }, 'void', ['pointer']));
      const fx = {};
      for (let i = 0; i < 10; i++) {
        const err = alloc(0x84, (0x11111111 * (i + 1)) >>> 0);
        err.writePointer(ERR_REC);
        zero(err.add(0x30), 0x10);
        err.add(0x14).writeS32(i * 7 - 3);
        err.add(0x40).writeS32(0x5a00 + i);
        const c = alloc(0x40, 0);
        c.writePointer(err);
        fx['c' + i] = c;
        fx['e' + i] = err;
        if (i === 0) fx.obj = c;
      }
      return fx;
    },

    // ---- jpeg_destroy_compress 0x004ae210 ------------------------------------------------------------------------
    // Ten compress objects of 0x20 bytes pre-filled 0x4d: the memory manager at +4 and the state word at +0x10,
    // both of which jpeg_destroy 0x004afa90 clears (0x004afaa6 / 0x004afaa9). Three hold a null manager (the je
    // at 0x004afa9d taken); the other seven hold managers whose self_destruct at +0x28 is our callback, called
    // with the compress object (0x004afaa0) and cleaned by the caller (0x004afaa3). The callback records the
    // manager's id and the state word it is called with, so both the dispatch and its ORDER relative to the two
    // stores are part of the compared state.
    c3cc_destroy() {
      const rec = mkRec();
      const SELF_DESTRUCT = keep(new NativeCallback(function (cinfo) {
        const m = cinfo.add(4).readPointer();
        push(rec, m.add(4).readS32());
        push(rec, cinfo.add(0x10).readS32());
        m.add(8).writeU32((m.add(8).readU32() + 1) >>> 0);
      }, 'void', ['pointer']));
      const fx = { obj: rec, rec: rec };
      [null, 1, 2, null, -4, 0x300, 7, null, 0x7fff, 0x29]
        .forEach(function (id, i) {
          const c = alloc(0x20, 0x4d4d4d4d);
          c.add(0x10).writeS32(i * 11 + 100);
          if (id === null) {
            c.add(4).writePointer(ptr(0));
          } else {
            const m = alloc(0x40, 0);
            m.add(4).writeS32(id);
            m.add(0x28).writePointer(SELF_DESTRUCT);
            c.add(4).writePointer(m);
            fx['m' + i] = m;
          }
          fx['c' + i] = c;
        });
      return fx;
    },
  });
})();
