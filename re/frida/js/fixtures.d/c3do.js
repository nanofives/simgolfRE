// Fixtures for C3 batch c3do (sound.dll, round 27): reachable C2 rows round 26 listed without a blocker, plus the mixer family.
// Layouts come from the disassembly cited in shim/src/re/c3do.cpp. Every object, vtable, list node and buffer here is allocated by
// the fixture; no live sound object, device, list, window or file is handed to an arm. Stand-ins used (one call per fixture):
// __heapStandIn (re/frida/js/heapstandin.js) over sound.dll's operator new 0x424d7 / operator delete 0x4249a. All values are synthetic.
// NativeCallbacks are kept alive through __diffKeepAlive.
Object.assign(globalThis.DIFF_FIXTURES, {

  c3do_fill(p, n, b) { p.writeByteArray(new Array(n).fill(b)); return p; },
  c3do_keep(cb) { globalThis.__diffKeepAlive.push(cb); return cb; },
  c3do_snd(rva) { return Process.getModuleByName('sound.dll').base.add(rva); },
  c3do_stub(nargs, fn) {
    return globalThis.DIFF_FIXTURES.c3do_keep(new NativeCallback(fn, 'uint32', ['pointer'].concat(Array(nargs).fill('uint32')), 'thiscall'));
  },
  // `size`-byte block whose dwords are distinct markers (base + row << 16 + offset)
  c3do_mark(size, base, row) {
    const b = __keepAlloc(size);
    for (let o = 0; o < size; o += 4) b.add(o).writeU32((base + (row << 16) + o) >>> 0);
    return b;
  },
  // n objects of `size` bytes named o0..o(n-1); every dword is a distinct marker
  c3do_objs(n, size, base) {
    const out = {};
    for (let i = 0; i < n; i++) out['o' + i] = globalThis.DIFF_FIXTURES.c3do_mark(size, base === undefined ? 0x5c000000 : base, i);
    return out;
  },
  // twelve 0x100-byte objects; the dword at +0 and the byte at +4 differ by row (low bits 0..3 and (0x11 i) & 0xff), so the flag-bit
  // updates of DsBuffer::ctor / initDesc read both ways
  c3do_objs12() {
    const out = globalThis.DIFF_FIXTURES.c3do_objs(12, 0x100);
    for (let i = 0; i < 12; i++) {
      out['o' + i].writeU32((0x5c000000 + (i << 16) + (i % 4) + 4 * (i >> 2)) >>> 0);
      out['o' + i].add(4).writeU8((0x11 * i) & 0xff);
    }
    out.o4.writeU32(0xffffffff); out.o8.writeU32(0);
    return out;
  },
  // the same with the operator new arena (the base constructors allocate their name strings)
  c3do_objs12_arena() {
    const out = globalThis.DIFF_FIXTURES.c3do_objs12();
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], arenaSize: 0x200 });
    out.arena = h.arena;
    return out;
  },

  // --- list-owning destructor bodies (ChainObject_dtor, Ctrl5b58c_dtor(Embedded), MidiReader_freeContent) ------------------------
  // Ten objects o0..o9 (0x80 bytes) whose list header {head +0xc, tail +0x10, count +0x18} holds the row's nodes {prev +0, next +4, value +8}
  // (nodes in nb0..nb9, 0x10 each). A value is NULL ('N') or a 0x20-byte object in vb0..vb9 whose vtable slot 0 is a recording
  // thiscall(1 arg) stub: +8 counts the calls, +0xc keeps this, +0x10 the argument. s_i = o_i + 0x2c (the sub-object the Chain bodies take).
  // Rows: 0 empty; 1 O; 2 N; 3 OOO; 4 ONO; 5 NON; 6 NN; 7 OONONO; 8 OOOOOOOO; 9 NOON
  // operator delete 0x1004249a is recorded (the nodes are the fixture's own and never freed).
  c3do_drain_rows() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const vt = F.c3do_fill(__keepAlloc(0x20), 0x20, 0);
    vt.writePointer(F.c3do_stub(1, function (self, a) {
      self.add(8).writeU32((self.add(8).readU32() + 1) >>> 0);
      self.add(0xc).writeU32(self.toUInt32());
      self.add(0x10).writeU32(a >>> 0);
      return 0x6d;
    }));
    const shapes = ['', 'O', 'N', 'OOO', 'ONO', 'NON', 'NN', 'OONONO', 'OOOOOOOO', 'NOON'];
    shapes.forEach(function (sh, i) {
      const o = F.c3do_mark(0x80, 0x5c100000, i);
      const nb = F.c3do_mark(0x80, 0x5c200000, i);
      const vb = F.c3do_mark(0x100, 0x5c300000, i);
      const n = sh.length;
      for (let k = 0; k < n; k++) {
        const nd = nb.add(0x10 * k);
        nd.writePointer(k > 0 ? nb.add(0x10 * (k - 1)) : ptr(0));
        nd.add(4).writePointer(k < n - 1 ? nb.add(0x10 * (k + 1)) : ptr(0));
        if (sh[k] === 'O') {
          const v = vb.add(0x20 * k);
          v.writePointer(vt);
          v.add(8).writeU32(0);
          v.add(0xc).writeU32(0);
          v.add(0x10).writeU32(0);
          nd.add(8).writePointer(v);
        } else nd.add(8).writePointer(ptr(0));
      }
      o.add(0xc).writePointer(n ? nb : ptr(0));
      o.add(0x10).writePointer(n ? nb.add(0x10 * (n - 1)) : ptr(0));
      o.add(0x18).writeU32(n);
      out['o' + i] = o;
      out['s' + i] = o.add(0x2c);
      out['nb' + i] = nb;
      out['vb' + i] = vb;
    });
    return out;
  },
  c3do_drain() {
    const out = globalThis.DIFF_FIXTURES.c3do_drain_rows();
    const h = __heapStandIn({ module: 'sound.dll', free: [0x4249a] });
    out.freelog = h.log;
    return out;
  },
  // MidiReader_dtor: the same ten list rows with [+0x3c buffer, +0x34 flag byte]: rows 0 (0,0) 1 (0,4) 2 (buf,0) 3 (buf,4) 4 (buf,0xfb)
  // 5 (0,0xff) 6 (buf,0x04) 7 (buf,0x24) 8 (0,0x04) 9 (buf,0x00)
  c3do_mrd() {
    const out = globalThis.DIFF_FIXTURES.c3do_drain();
    const rows = [[0, 0], [0, 4], [1, 0], [1, 4], [1, 0xfb], [0, 0xff], [1, 4], [1, 0x24], [0, 4], [1, 0]];
    rows.forEach(function (r, i) {
      const o = out['o' + i];
      o.add(0x3c).writePointer(r[0] ? ptr(0x7e910000 + 0x1000 * i) : ptr(0));
      o.add(0x34).writeU8(r[1]);
    });
    return out;
  },

  // --- Channel_propagateFlag0: chains linked through +0x40 ------------------------------------------------------------------
  // Twelve 0x60-byte channels c0..c11: byte +0x58 = (37 i + 1) & 0xff; chains c1 -> c2 -> c3 -> c4, c6 -> c7, c9 -> c10 -> c11; the rest
  // end at once (+0x40 = 0).
  c3do_chain() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    for (let i = 0; i < 12; i++) {
      const c = F.c3do_mark(0x60, 0x5c400000, i);
      c.add(0x58).writeU8((37 * i + 1) & 0xff);
      c.add(0x40).writePointer(ptr(0));
      out['c' + i] = c;
    }
    const link = [[1, 2], [2, 3], [3, 4], [6, 7], [9, 10], [10, 11]];
    link.forEach(function (l) { out['c' + l[0]].add(0x40).writePointer(out['c' + l[1]]); });
    return out;
  },

  // A list {prev +0, next +4, value +8} of `vals` (pointers or null) in a node block `nb`; returns the head node (or null)
  c3do_list(nb, vals) {
    const n = vals.length;
    for (let k = 0; k < n; k++) {
      const nd = nb.add(0x10 * k);
      nd.writePointer(k > 0 ? nb.add(0x10 * (k - 1)) : ptr(0));
      nd.add(4).writePointer(k < n - 1 ? nb.add(0x10 * (k + 1)) : ptr(0));
      nd.add(8).writePointer(vals[k]);
    }
    return n ? nb : ptr(0);
  },

  // --- Seq_cmdHandler7: sequencer objects z0..z11 (0x780 bytes) -----------------------------------------------------------------
  // The vtable slot +0x84 (1 argument) counts at self+0x740 and keeps its argument at +0x744; slot +0x94 (2 arguments) counts at +0x748,
  // keeps the arguments at +0x74c / +0x750 and returns the dword at +0x754. +0x738 is the wanted track id (0x77). Tracks are 0x30-byte
  // blocks {+4 id, +8 a, +0xc b, +0x1c c} in tb_i; the list nodes are in nb_i.
  // Rows [dword +0x224, shape] (shape: M = the id 0x77 track, x = another id, N = NULL value):
  //   0 {0, Mc0}   1 {1, empty}   2 {1, N}   3 {1, M c=0}   4 {1, M c=7}   5 {1, xM c=9}   6 {1, xxM c=0xffffffff}
  //   7 {1, xx}    8 {1, xNM}     9 {5, M c=1}  10 {1, xxxxM c=3}  11 {0xffffffff, x}
  c3do_h7() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const vt = F.c3do_fill(__keepAlloc(0x100), 0x100, 0);
    vt.add(0x84).writePointer(F.c3do_stub(1, function (self, a) {
      self.add(0x740).writeU32((self.add(0x740).readU32() + 1) >>> 0);
      self.add(0x744).writeU32(a >>> 0);
      return 0;
    }));
    vt.add(0x94).writePointer(F.c3do_stub(2, function (self, a, b) {
      self.add(0x748).writeU32((self.add(0x748).readU32() + 1) >>> 0);
      self.add(0x74c).writeU32(a >>> 0);
      self.add(0x750).writeU32(b >>> 0);
      return self.add(0x754).readU32();
    }));
    // element codes: 'N' NULL value, 'x' track with another id, ['M', c] the id 0x77 track whose dword +0x1c is c
    const rows = [[0, [['M', 0]]], [1, []], [1, ['N']], [1, [['M', 0]]], [1, [['M', 7]]], [1, ['x', ['M', 9]]],
                  [1, ['x', 'x', ['M', 0xffffffff]]], [1, ['x', 'x']], [1, ['x', 'N', ['M', 0]]], [5, [['M', 1]]],
                  [1, ['x', 'x', 'x', 'x', ['M', 3]]], [0xffffffff, ['x']]];
    rows.forEach(function (r, i) {
      const z = F.c3do_mark(0x780, 0x5c500000, i);
      z.writePointer(vt);
      z.add(0x738).writeU32(0x77);
      z.add(0x740).writeU32(0); z.add(0x744).writeU32(0); z.add(0x748).writeU32(0); z.add(0x74c).writeU32(0); z.add(0x750).writeU32(0);
      z.add(0x754).writeU32((0x1357 + i) >>> 0);
      z.add(0x224).writeU32(r[0] >>> 0);
      const tb = F.c3do_mark(0x200, 0x5c600000, i);
      const nb = F.c3do_mark(0x100, 0x5c700000, i);
      const vals = [];
      let k = 0;
      r[1].forEach(function (e, j) {
        if (e === 'N') { vals.push(ptr(0)); return; }
        const t = tb.add(0x30 * k++);
        const m = (e !== 'x');
        t.add(4).writeU32(m ? 0x77 : (0x11 * (j + 1)));
        t.add(8).writeU32((0xa0 + i * 16 + j) >>> 0);
        t.add(0xc).writeU32((0xb0 + i * 16 + j) >>> 0);
        t.add(0x1c).writeU32((m ? e[1] : 0x55) >>> 0);
        vals.push(t);
      });
      z.add(0x218).writePointer(F.c3do_list(nb, vals));
      z.add(0x220).writeU32(0xdeadbe00 + i);
      out['z' + i] = z; out['tb' + i] = tb; out['nb' + i] = nb;
    });
    return out;
  },

  // --- Seq_rewindTracks: sequencer objects q0..q9 (0x80 bytes) whose track list (head +0xc) holds the row's values ---------------
  // A value is NULL ('N'), a type-5 track ['T', b19, f20] (dword +0x1c = 5, dword +0x20 = f20, byte +0x19 = b19) or another track 'U'
  // (+0x1c = 3). The vtable slot +0x10 (1 argument) copies the low byte of the dword at +0x20 into the byte at +0x19 and counts at +8,
  // keeping its argument at +0xc; the slot +0x18 (no argument) counts at +0x24. Rows: 0 empty; 1 N; 2 T(0x77,1); 3 T(0x77,0);
  // 4 U; 5 T1 U T0; 6 T0 N T1; 7 U U; 8 T2 T1 T7; 9 T1 T1 T1 U. +0x34 / +0x38 / +0x3c carry row-dependent flag patterns.
  c3do_rew() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const vt = F.c3do_fill(__keepAlloc(0x40), 0x40, 0);
    vt.add(0x10).writePointer(F.c3do_stub(1, function (self, a) {
      self.add(8).writeU32((self.add(8).readU32() + 1) >>> 0);
      self.add(0xc).writeU32(a >>> 0);
      self.add(0x19).writeU8(self.add(0x20).readU32() & 0xff);
      return 0;
    }));
    vt.add(0x18).writePointer(F.c3do_stub(0, function (self) {
      self.add(0x24).writeU32((self.add(0x24).readU32() + 1) >>> 0);
      return 0;
    }));
    const T = (b, f) => ['T', b, f];
    const rows = [[], ['N'], [T(0x77, 1)], [T(0x77, 0)], ['U'], [T(0x77, 1), 'U', T(0x77, 0)], [T(0x77, 0), 'N', T(0x77, 1)],
                  ['U', 'U'], [T(0x77, 2), T(0x77, 1), T(0x77, 7)], [T(0x77, 1), T(0x77, 1), T(0x77, 1), 'U']];
    const f34 = [0xffffffff, 0, 2, 0xfffffffd, 0x5, 0x2, 0xfffffffe, 0x80000002, 0x3, 0x12345678];
    const f38 = [0xff, 0x00, 0x04, 0xfb, 0x01, 0x05, 0xfe, 0x04, 0x00, 0x55];
    rows.forEach(function (r, i) {
      const q = F.c3do_mark(0x80, 0x5c800000, i);
      const vb = F.c3do_mark(0x200, 0x5c900000, i);
      const nb = F.c3do_mark(0x100, 0x5ca00000, i);
      const vals = [];
      r.forEach(function (e, j) {
        if (e === 'N') { vals.push(ptr(0)); return; }
        const v = vb.add(0x40 * j);
        v.writePointer(vt);
        v.add(8).writeU32(0); v.add(0xc).writeU32(0); v.add(0x24).writeU32(0);
        if (e === 'U') { v.add(0x1c).writeU32(3); v.add(0x19).writeU8(0x33); v.add(0x20).writeU32(1); } else {
          v.add(0x1c).writeU32(5); v.add(0x19).writeU8(e[1]); v.add(0x20).writeU32(e[2]);
        }
        vals.push(v);
      });
      q.add(0xc).writePointer(F.c3do_list(nb, vals));
      q.add(0x14).writeU32(0xdead0000 + i);
      q.add(0x34).writeU32(f34[i] >>> 0);
      q.add(0x38).writeU8(f38[i]);
      out['q' + i] = q; out['vb' + i] = vb; out['nb' + i] = nb;
    });
    return out;
  },

  // --- resizeDynArray: arrays a0..a9 (0x20 bytes): data pointer +4 (NULL or a synthetic block that operator delete only records),
  // flags +8, count +0xc. operator new / delete are stood in (arena 0x400).
  c3do_resize() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    // [data?, flags, count]
    const rows = [[0, 0, 0], [1, 0, 5], [0, 1, 0], [1, 1, 3], [1, 0, 1], [1, 0xffffffff, 0x10], [0, 0x80000001, 2], [1, 2, 0], [1, 3, 7], [0, 0, 17]];
    rows.forEach(function (r, i) {
      const a = F.c3do_mark(0x20, 0x5cb00000, i);
      a.add(4).writePointer(r[0] ? ptr(0x7ea00000 + 0x1000 * i) : ptr(0));
      a.add(8).writeU32(r[1] >>> 0);
      a.add(0xc).writeU32(r[2] >>> 0);
      out['a' + i] = a;
    });
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], free: [0x4249a], arenaSize: 0x400 });
    out.arena = h.arena;
    out.freelog = h.log;
    return out;
  },

  // --- appendNamedNode: owners w0..w5 (0x160 bytes: head +4, count +0x148) with 0..3 existing 0x14-byte nodes (in nb_i), GUIDs g0..g3,
  // strings t0..t7 (kept alive), the module counter 0x100b4a44 as `ctr` (saved and restored at the freeze flush; no code but this function
  // reads it). operator new is stood in (arena 0x400).
  c3do_appnode() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const counts = [0, 1, 2, 3, 0, 2];
    for (let i = 0; i < 6; i++) {
      const w = F.c3do_mark(0x160, 0x5cc00000, i);
      const nb = F.c3do_mark(0x80, 0x5cd00000, i);
      const n = counts[i];
      for (let k = 0; k < n; k++) {
        const nd = nb.add(0x20 * k);
        nd.add(0x10).writePointer(k < n - 1 ? nb.add(0x20 * (k + 1)) : ptr(0));
      }
      w.add(4).writePointer(n ? nb : ptr(0));
      w.add(0x148).writeU32(i === 4 ? 0xffffffff : n);
      out['w' + i] = w; out['nb' + i] = nb;
    }
    for (let i = 0; i < 4; i++) {
      const g = F.c3do_mark(0x10, 0x5ce00000, i);
      out['g' + i] = g;
    }
    const strs = ['', 'a', 'ab', 'abc', 'abcd', 'hello world', 'abcdefghijklmnopqrstuvwxyz', 'x'.repeat(37)];
    strs.forEach(function (s, i) { out['t' + i] = Memory.allocUtf8String(s); globalThis.__diffKeepAlive.push(out['t' + i]); });
    const ctr = F.c3do_snd(0xb4a44);
    __freeze.save(ctr, 4);
    ctr.writeU32(0x41);
    out.ctr = ctr;
    const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], arenaSize: 0x400 });
    out.arena = h.arena;
    return out;
  },

  // --- dynInit_ctrl*: the object (0xe8 bytes at the module global) is saved, filled with a per-dword pattern and restored at the
  // freeze flush; __onexit (RVA 0x425c9, the only callee of _atexit 0x42647) records its argument on the A/B thread and returns it
  // ('ok': _atexit returns 0) or NULL ('fail': _atexit returns -1); other threads are forwarded to the real function.
  c3do_dyn(grva, mode) {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const g = F.c3do_snd(grva);
    __freeze.save(g, 0xe8);
    const u = new Uint32Array(0xe8 >>> 2);
    for (let i = 0; i < u.length; i++) u[i] = (0xC3000000 | (((grva >>> 2) + i) & 0x00ffffff)) ^ 0x5a17c3c0;
    g.writeByteArray(u.buffer);
    out.g = g;
    const log = __keepAlloc(0x40);
    log.writeByteArray(new Array(0x40).fill(0));
    out.log = log;
    const tid = Process.getCurrentThreadId();
    const at = F.c3do_snd(0x425c9);
    const orig = new NativeFunction(at, 'pointer', ['pointer']);
    const cb = new NativeCallback(function (fn) {
      if (Process.getCurrentThreadId() !== tid) return orig(fn);
      const n = log.readU32();
      if (n < 15) { log.add(4 + 4 * n).writePointer(fn); log.writeU32(n + 1); }
      return mode === 'ok' ? fn : ptr(0);
    }, 'pointer', ['pointer']);
    __diffKeepAlive.push(cb, orig);
    Interceptor.replace(at, cb);
    Interceptor.flush();
    __freeze.atFlush(function () { Interceptor.revert(at); Interceptor.flush(); });
    return out;
  },
  c3do_dyn_b50_ok() { return globalThis.DIFF_FIXTURES.c3do_dyn(0xb4b50, 'ok'); },
  c3do_dyn_b50_fail() { return globalThis.DIFF_FIXTURES.c3do_dyn(0xb4b50, 'fail'); },
  c3do_dyn_c38_ok() { return globalThis.DIFF_FIXTURES.c3do_dyn(0xb4c38, 'ok'); },
  c3do_dyn_c38_fail() { return globalThis.DIFF_FIXTURES.c3do_dyn(0xb4c38, 'fail'); },
  c3do_dyn_d20_ok() { return globalThis.DIFF_FIXTURES.c3do_dyn(0xb4d20, 'ok'); },
  c3do_dyn_d20_fail() { return globalThis.DIFF_FIXTURES.c3do_dyn(0xb4d20, 'fail'); },
  c3do_dyn_a68_ok() { return globalThis.DIFF_FIXTURES.c3do_dyn(0xb4a68, 'ok'); },
  c3do_dyn_a68_fail() { return globalThis.DIFF_FIXTURES.c3do_dyn(0xb4a68, 'fail'); },
  c3do_dyn_e08_ok() { return globalThis.DIFF_FIXTURES.c3do_dyn(0xb4e08, 'ok'); },
  c3do_dyn_e08_fail() { return globalThis.DIFF_FIXTURES.c3do_dyn(0xb4e08, 'fail'); },
  // >>> DS

  // --- DsBuffer methods over a COM-shaped interface ----------------------------------------------------------------------------------
  // A COM object is 0x100 bytes: +0 vtable (shared); configuration: +0x20 GetCurrentPosition result, +0x24 Lock result, +0x28 SetFormat result,
  // +0x2c Unlock result, +0x30 play cursor, +0x34 write cursor, +0x40 ptr1, +0x44 bytes1, +0x48 ptr2, +0x4c bytes2 (what Lock hands out); records:
  // +0x80 GetCurrentPosition calls, +0x84 Lock calls with +0x88 offset, +0x8c bytes, +0x90 flags, +0x94 SetFormat calls with +0x98 the format pointer, +0xa0
  // Unlock calls with +0xa4 ptr1, +0xa8 bytes1, +0xac ptr2, +0xb0 bytes2. Stdcall stubs, the interface pointer first.
  c3do_comvt() {
    const F = globalThis.DIFF_FIXTURES;
    const vt = F.c3do_fill(__keepAlloc(0x80), 0x80, 0);
    const inc = (p) => p.writeU32((p.readU32() + 1) >>> 0);
    const put = (off, n, fn) => vt.add(off).writePointer(F.c3do_keep(new NativeCallback(fn, 'uint32', ['pointer'].concat(Array(n).fill('uint32')), 'stdcall')));
    put(0x10, 2, function (self, pPlay, pWrite) {
      inc(self.add(0x80));
      if (pPlay !== 0) ptr(pPlay).writeU32(self.add(0x30).readU32());
      if (pWrite !== 0) ptr(pWrite).writeU32(self.add(0x34).readU32());
      return self.add(0x20).readU32();
    });
    put(0x2c, 7, function (self, off, bytes, pp1, pb1, pp2, pb2, flags) {
      inc(self.add(0x84));
      self.add(0x88).writeU32(off >>> 0); self.add(0x8c).writeU32(bytes >>> 0); self.add(0x90).writeU32(flags >>> 0);
      if (pp1 !== 0) ptr(pp1).writeU32(self.add(0x40).readU32());
      if (pb1 !== 0) ptr(pb1).writeU32(self.add(0x44).readU32());
      if (pp2 !== 0) ptr(pp2).writeU32(self.add(0x48).readU32());
      if (pb2 !== 0) ptr(pb2).writeU32(self.add(0x4c).readU32());
      return self.add(0x24).readU32();
    });
    put(0x38, 1, function (self, fmt) {
      inc(self.add(0x94));
      self.add(0x98).writeU32(fmt >>> 0);
      return self.add(0x28).readU32();
    });
    put(0x4c, 4, function (self, p1, b1, p2, b2) {
      inc(self.add(0xa0));
      self.add(0xa4).writeU32(p1 >>> 0); self.add(0xa8).writeU32(b1 >>> 0); self.add(0xac).writeU32(p2 >>> 0); self.add(0xb0).writeU32(b2 >>> 0);
      return self.add(0x2c).readU32();
    });
    return vt;
  },
  c3do_comobj(vt, cfg) {
    const F = globalThis.DIFF_FIXTURES;
    const c = F.c3do_fill(__keepAlloc(0x100), 0x100, 0);
    c.writePointer(vt);
    const w = (off, v) => c.add(off).writeU32((v || 0) >>> 0);
    w(0x20, cfg.cur); w(0x24, cfg.lock); w(0x28, cfg.fmt); w(0x2c, cfg.unl); w(0x30, cfg.play); w(0x34, cfg.write);
    w(0x40, cfg.p1); w(0x44, cfg.b1); w(0x48, cfg.p2); w(0x4c, cfg.b2);
    return c;
  },
  // DsBuffer objects d0..d(n-1) (0x100 bytes, markers) with the dword +0 = flags, byte +4, +0x24 / +0x28 / +0x30 / +0x40 markers, the interface at +0x60 (a COM object
  // c_i, or NULL when row.noif) and +0x78 = size
  c3do_dsrows(rows) {
    const F = globalThis.DIFF_FIXTURES;
    const vt = F.c3do_comvt();
    const out = {};
    rows.forEach(function (r, i) {
      const d = F.c3do_mark(0x100, 0x5d300000, i);
      const a = F.c3do_mark(0x80, 0x5d400000, i);
      const b = F.c3do_mark(0x80, 0x5d500000, i);
      out['a' + i] = a; out['b' + i] = b;
      const cfg = Object.assign({}, r.com || {});
      if (cfg.p1 === undefined) cfg.p1 = a.toUInt32();
      if (cfg.p2 === undefined) cfg.p2 = b.toUInt32();
      const c = F.c3do_comobj(vt, cfg);
      out['c' + i] = c;
      d.writeU32((r.flags === undefined ? 0x11110000 + i : r.flags) >>> 0);
      d.add(4).writeU8(r.b4 === undefined ? 0x22 : r.b4);
      d.add(0x60).writePointer(r.noif ? ptr(0) : c);
      d.add(0x78).writeU32((r.size === undefined ? 0x1000 : r.size) >>> 0);
      if (r.f40 !== undefined) d.add(0x40).writeU32(r.f40 >>> 0);
      if (r.f30 !== undefined) d.add(0x30).writeU32(r.f30 >>> 0);
      out['d' + i] = d;
      const f = F.c3do_mark(0x20, 0x5d600000, i);
      out['f' + i] = f;
      out['o' + i] = F.c3do_mark(0x10, 0x5d700000, i);
    });
    return out;
  },
  // setPrimaryFormat: SetFormat result codes; rows 0..3 cover the four (interface, bit 1) combinations
  c3do_dsfmt() {
    const rows = [{ noif: 1, flags: 0 }, { noif: 1, flags: 2 }, { flags: 0 }, { flags: 2 }];
    [0, 0x80004001, 0x8007000e, 0x80070057, 0x8878000a, 0x88780064, 0x88780078, 0x88780096, 0x887800aa, 0x88780065, 0x887800ab, 0x88780063,
     0x80004005, 1, 0x7fffffff, 0xffffffff, 0x80070058, 0x8007000d, 0x88780009, 0x8878000b, 0x88780000, 0x88780050].forEach(function (c) {
      rows.push({ flags: 0x103, com: { fmt: c } });
    });
    return globalThis.DIFF_FIXTURES.c3do_dsrows(rows);
  },
  // lockEntire: rows 0 no interface; 1 Lock fails; 2.. Lock hands out (b1, b2) bytes in the two 0x80-byte blocks
  c3do_dslockall() {
    const R = (b1, b2, extra) => Object.assign({ com: { b1: b1, b2: b2 } }, extra || {});
    const rows = [{ noif: 1 }, { com: { lock: 0x80004005, b1: 0x10, b2: 0 } }, R(0x10, 0), R(0x13, 0), R(0, 0), R(0x20, 0x14), R(7, 5), R(1, 1), R(0x40, 0x40, { size: 0x2000 }),
                  { com: { lock: 1, b1: 4, b2: 4 } }, R(0x80, 0), R(0, 0x11), R(0x7c, 3, { com: { b1: 0x7c, b2: 3, unl: 0x88780096 } })];
    rows[12].com = { b1: 0x7c, b2: 3, unl: 0x88780096 };
    return globalThis.DIFF_FIXTURES.c3do_dsrows(rows);
  },
  // lockCaptureRegion: rows [flags byte +4, GetCurrentPosition result, write cursor, size +0x78, +0x40, +0x30]
  c3do_dslockcap() {
    const T = [[1, 0, 0x100, 0x1000, 0x20, 0x30], [0, 0, 0x100, 0x1000, 0x20, 0x30], [1, 0, 0xf00, 0x1000, 0x400, 0x30], [0, 0, 0x100, 0x1000, 0x20, 0xff0],
               [1, 0x80004005, 0x100, 0x1000, 0x20, 0x30], [0xff, 0, 0x7f0, 0x800, 0x10, 0x10], [0xfe, 0, 0x7f0, 0x800, 0x10, 0x10], [1, 0, 0, 0x10, 0, 0],
               [3, 0, 0xffffff00, 0x40, 0xffffff00, 0x8], [0, 0, 5, 0x100, 0, 0xf0], [1, 1, 0x100, 0x1000, 0x20, 0x30], [0x81, 0, 0x1000, 0x1000, 0x1000, 0x1000]];
    const rows = T.map(function (t) { return { b4: t[0], size: t[3], f40: t[4], f30: t[5], com: { cur: t[1], write: t[2], play: 0x55 } }; });
    return globalThis.DIFF_FIXTURES.c3do_dsrows(rows);
  },
  // <<< DS
  // >>> LIST

  // --- removal from the doubly linked value lists ------------------------------------------------------------------------------
  // Objects o_i (`sz` bytes) with a list header {head, back, cursor, count} at `hoff`; nodes in n_i; four targets ta_i..td_i of 0xd0 bytes in the block t_i (letters in the shape
  // name them, 'N' is a node whose value is NULL, 'd' is never in a list: the "not found" argument). The cursor starts as a marker 0xc0de00ii.
  c3do_lst(sz, hoff, shapes) {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    shapes.forEach(function (sh, i) {
      const o = F.c3do_mark(sz, 0x5da00000, i);
      const t = F.c3do_mark(0x340, 0x5db00000, i);
      const nb = F.c3do_mark(0x80, 0x5dc00000, i);
      const tp = (ch) => (ch === 'N' ? ptr(0) : t.add(0xd0 * (ch.charCodeAt(0) - 97)));
      ['a', 'b', 'c', 'd'].forEach(function (ch, k) {
        t.add(0xd0 * k + 0xc8).writeU32(k % 2 ? 0xffffffff : 0x5a5a5a5a);
        out['t' + ch + i] = tp(ch);
      });
      const n = sh.length;
      const head = F.c3do_list(nb, sh.map(tp));
      const h = o.add(hoff);
      h.writePointer(head);
      h.add(4).writePointer(n ? nb.add(0x10 * (n - 1)) : ptr(0));
      h.add(8).writePointer(ptr(0xc0de0000 + i));
      h.add(0xc).writeU32(n);
      out['o' + i] = o; out['t' + i] = t; out['n' + i] = nb;
    });
    return out;
  },
  c3do_lst_shapes() { return [[], ['a'], ['a', 'b'], ['a', 'b', 'c'], ['a', 'b', 'c', 'd'], ['N'], ['a', 'N', 'b']]; },
  c3do_vlr() {
    const F = globalThis.DIFF_FIXTURES;
    const out = F.c3do_lst(0x1400, 0x1338, F.c3do_lst_shapes());
    out.freelog = __heapStandIn({ module: 'sound.dll', free: [0x4249a] }).log;
    return out;
  },
  c3do_rsn() {
    const F = globalThis.DIFF_FIXTURES;
    const out = F.c3do_lst(0x240, 0x1c8, F.c3do_lst_shapes());
    out.freelog = __heapStandIn({ module: 'sound.dll', free: [0x4249a] }).log;
    return out;
  },
  // the module registry list 0x100b5000 (header {head, back, cursor, count}; read by registerSound / unregisterSound / Midi_Device::shutdown / Voice::update, none of them entered at
  // the menu) is saved and replaced by the row's list for the length of the key; unregisterSound's rows are the shapes above, registerSound's start from 0, 1 or 2 nodes plus sound objects
  c3do_regl(sh) {
    const F = globalThis.DIFF_FIXTURES;
    const out = F.c3do_lst(0x100, 0x20, [sh]);          // only the targets / nodes / the header block are used
    const g = F.c3do_snd(0xb5000);
    __freeze.save(g, 0x10);
    const o = out.o0;
    g.writeByteArray(o.add(0x20).readByteArray(0x10));
    out.reg = g;
    out.freelog = __heapStandIn({ module: 'sound.dll', free: [0x4249a] }).log;
    return out;
  },
  c3do_unreg_s3() { return globalThis.DIFF_FIXTURES.c3do_regl(['a', 'b', 'c']); },
  c3do_unreg_s1() { return globalThis.DIFF_FIXTURES.c3do_regl(['a']); },
  c3do_unreg_s0() { return globalThis.DIFF_FIXTURES.c3do_regl([]); },
  c3do_unreg_sn() { return globalThis.DIFF_FIXTURES.c3do_regl(['a', 'N', 'b']); },
  // registerSound: the registry list keeps `sh`; sound objects s0..s3 (0x240 bytes, flag dword at +0x214); operator new is stood in
  c3do_regs(sh) {
    const F = globalThis.DIFF_FIXTURES;
    const out = F.c3do_regl(sh);
    for (let i = 0; i < 4; i++) out['s' + i] = F.c3do_mark(0x240, 0x5dd00000, i);
    out.s1.add(0x214).writeU32(0xffffffff); out.s2.add(0x214).writeU32(0);
    out.arena = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7], arenaSize: 0x200 }).arena;
    return out;
  },
  c3do_reg_s0() { return globalThis.DIFF_FIXTURES.c3do_regs([]); },
  c3do_reg_s1() { return globalThis.DIFF_FIXTURES.c3do_regs(['a']); },
  c3do_reg_s2() { return globalThis.DIFF_FIXTURES.c3do_regs(['a', 'b']); },
  // <<< LIST
  // >>> RMTRK

  // --- Seq_removeTrackById: sequencer objects q0..q8 (0x240 bytes: list header head +0x218, back +0x21c, cursor +0x220, count +0x224) whose track list holds the row's tracks (0x30-byte blocks
  // in tb_i with the id at +4: 'x' tracks have the id 0x11 (position + 1), 'M' the id 0x77, 'N' is a node with a NULL value). operator delete 0x1004249a is recorded.
  // Rows: 0 empty; 1 N; 2 M; 3 xM; 4 Mx; 5 xMx; 6 xx; 7 xNM; 8 xxxM
  c3do_rmtrk() {
    const F = globalThis.DIFF_FIXTURES;
    const out = {};
    const shapes = ['', 'N', 'M', 'xM', 'Mx', 'xMx', 'xx', 'xNM', 'xxxM'];
    shapes.forEach(function (sh, i) {
      const q = F.c3do_mark(0x240, 0x5de00000, i);
      const tb = F.c3do_mark(0x200, 0x5df00000, i);
      const nb = F.c3do_mark(0x80, 0x5e000000, i);
      const vals = [];
      let k = 0;
      for (let j = 0; j < sh.length; j++) {
        if (sh[j] === 'N') { vals.push(ptr(0)); continue; }
        const t = tb.add(0x30 * k++);
        t.add(4).writeU32(sh[j] === 'M' ? 0x77 : 0x11 * (j + 1));
        vals.push(t);
      }
      const n = sh.length;
      q.add(0x218).writePointer(F.c3do_list(nb, vals));
      q.add(0x21c).writePointer(n ? nb.add(0x10 * (n - 1)) : ptr(0));
      q.add(0x220).writePointer(ptr(0xc0de0000 + i));
      q.add(0x224).writeU32(n);
      out['q' + i] = q; out['tb' + i] = tb; out['nb' + i] = nb;
    });
    out.freelog = __heapStandIn({ module: 'sound.dll', free: [0x4249a] }).log;
    return out;
  },
  // <<< RMTRK
  // >>> MIX GENERATED

  // --- software mixer fixtures (mixRenderA, mixCopyF, mixCopyInnerC and the rated variants) ---------------------------------------------
  // One case = a voice v (0x1400 bytes, marker-filled), the stream sample record bo (0x40 bytes, v+8 in mode 1), up to three chain voices
  // cv1..cv3, 16-bit data blocks sd / cd1..cd3 (48 shorts each, extremes mixed in), an output block out (256 dwords, saturation edges mixed
  // in), the cells po / pi (the two in/out pointers). Case spec (indices are shorts inside the block, undefined fields take the default):
  //   mode 0 (voice is the record): f98, f9c (flag bytes +0x98 / +0x9c), st / en / ls / le -> +0x18 / +0x20 / +0xd8 / +0xd4
  //   mode 1 (stream layout): st / en / ln / le -> bo +0x18 / +0x20 / +0x1c / +0x24, f58 -> byte v+0x58
  //   chain: [{st,en,ls,le}, ...] up to three chain voices linked through +0xb4 (voice +0xb4 -> cv1 -> cv2 -> cv3)
  //   in: index of *ppIn in sd, o: index (in dwords) of *ppOut in out
  //   rates: [rate, left gain, right gain] and ring {frac, ip, hi} (dword +2, dword +4, word +6 of the 48-bit position) of the gain / rated variants;
  //   fade: dword v+0xec (mode 0)
  c3do_mixcase(i, sp) {
    const F = globalThis.DIFF_FIXTURES;
    const NSH = 48;
    const lcg = (s) => (Math.imul(s, 1103515245) + 12345) >>> 0;
    const data = (seed) => {
      const b = __keepAlloc(NSH * 2 + 16);
      let s = (0x1234567 + seed * 977) >>> 0;
      for (let k = 0; k < NSH; k++) {
        s = lcg(s);
        let w = (s >>> 12) & 0xffff;
        if (k % 7 === 3) w = 0x7fff; else if (k % 11 === 5) w = 0x8000; else if (k % 13 === 6) w = 0;
        b.add(2 * k).writeU16(w);
      }
      b.add(NSH * 2).writeU32(0xa5a5a5a5);
      return b;
    };
    const out = {};
    const sd = data(i * 4 + 1);
    out['sd' + i] = sd;
    const v = F.c3do_mark(0x1400, 0x5d000000, i);
    out['v' + i] = v;
    const P = (blk, idx) => blk.add(2 * idx);
    const bo = F.c3do_mark(0x40, 0x5d100000, i);
    out['bo' + i] = bo;
    const d = (x, y) => (x === undefined ? y : x);
    if (sp.mode === 1) {
      v.add(8).writePointer(bo);
      bo.add(0x18).writePointer(P(sd, d(sp.st, 0)));
      bo.add(0x1c).writePointer(P(sd, d(sp.ln, 0)));
      bo.add(0x20).writePointer(P(sd, d(sp.en, 40)));
      bo.add(0x24).writePointer(P(sd, d(sp.le, 40)));
      v.add(0x58).writeU8(d(sp.f58, 0));
    } else {
      v.add(0x98).writeU8(d(sp.f98, 0));
      v.add(0x9c).writeU8(d(sp.f9c, 0));
      v.add(0x18).writePointer(P(sd, d(sp.st, 0)));
      v.add(0x20).writePointer(P(sd, d(sp.en, 40)));
      v.add(0xd8).writePointer(P(sd, d(sp.ls, 8)));
      v.add(0xd4).writePointer(P(sd, d(sp.le, 40)));
      v.add(0xb4).writePointer(ptr(0));
    }
    const ch = sp.chain || [];
    let prev = null;
    const cvs = [];
    for (let c = 0; c < ch.length; c++) {
      const cv = F.c3do_mark(0x1400, 0x5d200000 + (c << 20), i);
      const cd = data(i * 4 + 2 + c);
      out['cv' + (c + 1) + '_' + i] = cv;
      out['cd' + (c + 1) + '_' + i] = cd;
      cv.add(0x18).writePointer(P(cd, d(ch[c].st, 0)));
      cv.add(0x20).writePointer(P(cd, d(ch[c].en, 40)));
      cv.add(0xd8).writePointer(P(cd, d(ch[c].ls, 4)));
      cv.add(0xd4).writePointer(P(cd, d(ch[c].le, 40)));
      cv.add(0xb4).writePointer(ptr(0));
      cvs.push(cv);
    }
    for (let c = 0; c < cvs.length; c++) {
      if (c === 0) { if (sp.mode !== 1) v.add(0xb4).writePointer(cvs[0]); }
      if (c + 1 < cvs.length) cvs[c].add(0xb4).writePointer(cvs[c + 1]);
    }
    if (sp.fade !== undefined) v.add(0xec).writeU32(sp.fade >>> 0);
    if (sp.rates) {
      // [rate, left gain, right gain]: mode 1 +0x220 / +0x1e8 / +0x1f8, ring at +0x1358; mode 0 +0x4c / +0x78 / +0x68, ring at +0xb8
      const R = sp.rates;
      const lay = (sp.mode === 1) ? [0x220, 0x1e8, 0x1f8, 0x1358] : [0x4c, 0x78, 0x68, 0xb8];
      v.add(lay[0]).writeU32(R[0] >>> 0);
      v.add(lay[1]).writeU32(R[1] >>> 0);
      v.add(lay[2]).writeU32(R[2] >>> 0);
      const rg = v.add(lay[3]);
      const r = sp.ring || { frac: 0, ip: 0, hi: 0 };
      rg.add(2).writeU32(r.frac >>> 0);
      rg.add(4).writeU32(r.ip >>> 0);
      rg.add(6).writeU16(r.hi & 0xffff);
    }
    const ob = __keepAlloc(0x400);
    let s = (0x7654321 + i * 131) >>> 0;
    for (let k = 0; k < 256; k++) {
      s = lcg(s);
      let w = s >>> 0;
      if (k % 5 === 1) w = 0x7ffff000 + (k & 0xfff);
      else if (k % 9 === 4) w = (0x80000800 - (k & 0x7ff)) >>> 0;
      else if (k % 6 === 0) w = (w & 0x00ffffff);
      ob.add(4 * k).writeU32(w >>> 0);
    }
    out['out' + i] = ob;
    const pi = __keepAlloc(8), po = __keepAlloc(8);
    pi.writePointer(P(sd, d(sp.in, 0)));
    po.writePointer(ob.add(4 * d(sp.o, 0)));
    out['pi' + i] = pi; out['po' + i] = po;
    return out;
  },
  // builds all cases of `rows` into one fixture object
  c3do_mixrows(rows) {
    const F = globalThis.DIFF_FIXTURES;
    const all = {};
    rows.forEach(function (sp, i) { Object.assign(all, F.c3do_mixcase(i, sp)); });
    return all;
  },
  c3do_mixA() { return globalThis.DIFF_FIXTURES.c3do_mixrows([{"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 0, "o": 0}, {"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 34, "o": 3}, {"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 36, "o": 6}, {"mode": 0, "f98": 1, "f9c": 0, "st": 0, "en": 40, "ls": 8, "le": 40, "in": 30, "o": 1}, {"mode": 0, "f98": 1, "f9c": 0, "st": 0, "en": 36, "ls": 10, "le": 38, "in": 34, "o": 0}, {"mode": 0, "f98": 1, "f9c": 2, "st": 0, "en": 40, "ls": 8, "le": 40, "in": 34, "o": 2}, {"mode": 0, "f98": 2, "f9c": 0, "st": 0, "en": 40, "in": 38, "o": 4}, {"mode": 0, "f98": 0, "f9c": 1, "st": 0, "en": 40, "in": 32, "o": 0, "chain": [{"st": 2, "en": 44, "ls": 6, "le": 44}]}, {"mode": 0, "f98": 0, "f9c": 1, "st": 0, "en": 40, "in": 34, "o": 5, "chain": [{"st": 2, "en": 44, "ls": 6, "le": 42}, {"st": 4, "en": 44, "ls": 10, "le": 46}]}, {"mode": 0, "f98": 0, "f9c": 1, "st": 0, "en": 38, "in": 30, "o": 0, "chain": [{"st": 0, "en": 44, "ls": 2, "le": 40}, {"st": 6, "en": 44, "ls": 8, "le": 42}, {"st": 3, "en": 44, "ls": 12, "le": 44}]}, {"mode": 0, "f98": 1, "f9c": 1, "st": 0, "en": 40, "ls": 8, "le": 40, "in": 36, "o": 7, "chain": [{"st": 2, "en": 44, "ls": 6, "le": 44}]}, {"mode": 0, "f98": 3, "f9c": 3, "st": 0, "en": 40, "ls": 8, "le": 40, "in": 38, "o": 0, "chain": [{"st": 0, "en": 44, "ls": 4, "le": 40}, {"st": 0, "en": 44, "ls": 4, "le": 40}]}, {"mode": 1, "st": 0, "ln": 44, "en": 40, "le": 44, "f58": 0, "in": 36, "o": 0}, {"mode": 1, "st": 0, "ln": 10, "en": 40, "le": 44, "f58": 0, "in": 30, "o": 2}, {"mode": 1, "st": 2, "ln": 44, "en": 40, "le": 44, "f58": 32, "in": 38, "o": 1}, {"mode": 1, "st": 4, "ln": 10, "en": 40, "le": 40, "f58": 32, "in": 36, "o": 0}, {"mode": 1, "st": 0, "ln": 10, "en": 40, "le": 44, "f58": 32, "in": 36, "o": 4}, {"mode": 1, "st": 0, "ln": 40, "en": 40, "le": 44, "f58": 223, "in": 34, "o": 0}]); },
  c3do_mixGA() { return globalThis.DIFF_FIXTURES.c3do_mixrows([{"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 0, "o": 0, "rates": [65536, 65536, 65536]}, {"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 34, "o": 3, "rates": [65536, 16384, 32768]}, {"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 36, "o": 6, "rates": [65536, 131072, 2147483647]}, {"mode": 0, "f98": 1, "f9c": 2, "st": 0, "en": 40, "ls": 8, "le": 40, "in": 34, "o": 2, "rates": [65536, 4294901760, 4660]}, {"mode": 0, "f98": 2, "f9c": 0, "st": 0, "en": 40, "in": 38, "o": 4, "rates": [65536, 0, 65536]}, {"mode": 0, "f98": 0, "f9c": 1, "st": 0, "en": 40, "in": 32, "o": 0, "rates": [65536, 32768, 32768], "chain": [{"st": 2, "en": 44, "ls": 6, "le": 44}]}, {"mode": 0, "f98": 0, "f9c": 1, "st": 0, "en": 40, "in": 34, "o": 5, "rates": [65536, 65536, 32767], "chain": [{"st": 2, "en": 44, "ls": 6, "le": 42}, {"st": 4, "en": 44, "ls": 10, "le": 46}]}, {"mode": 0, "f98": 0, "f9c": 1, "st": 0, "en": 38, "in": 30, "o": 0, "rates": [65536, 256, 4294967280], "chain": [{"st": 0, "en": 44, "ls": 2, "le": 40}, {"st": 6, "en": 44, "ls": 8, "le": 42}, {"st": 3, "en": 44, "ls": 12, "le": 44}]}, {"mode": 0, "f98": 1, "f9c": 1, "st": 0, "en": 40, "ls": 8, "le": 40, "in": 36, "o": 7, "rates": [65536, 2147483647, 2147483647], "chain": [{"st": 2, "en": 44, "ls": 6, "le": 44}]}, {"mode": 0, "f98": 3, "f9c": 3, "st": 0, "en": 40, "ls": 8, "le": 40, "in": 38, "o": 0, "rates": [65536, 65536, 65536], "chain": [{"st": 0, "en": 44, "ls": 4, "le": 40}, {"st": 0, "en": 44, "ls": 4, "le": 40}]}, {"mode": 1, "st": 0, "ln": 44, "en": 40, "le": 44, "f58": 0, "in": 36, "o": 0, "rates": [65536, 65536, 65536]}, {"mode": 1, "st": 0, "ln": 10, "en": 40, "le": 44, "f58": 0, "in": 30, "o": 2, "rates": [65536, 24576, 40960]}, {"mode": 1, "st": 2, "ln": 44, "en": 40, "le": 44, "f58": 32, "in": 38, "o": 1, "rates": [65536, 131072, 131072]}, {"mode": 1, "st": 0, "ln": 10, "en": 40, "le": 44, "f58": 32, "in": 36, "o": 4, "rates": [65536, 4294967295, 1]}, {"mode": 0, "f98": 1, "f9c": 0, "st": 0, "en": 40, "ls": 8, "le": 40, "in": 0, "o": 0, "rates": [65536, 32768, 16384]}]); },
  c3do_mixGB() { return globalThis.DIFF_FIXTURES.c3do_mixrows([{"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 0, "o": 0, "rates": [65536, 65536, 32768]}, {"mode": 0, "f98": 0, "f9c": 8, "st": 0, "en": 40, "in": 0, "o": 2, "fade": 16384, "rates": [65536, 65536, 32768]}, {"mode": 0, "f98": 0, "f9c": 8, "st": 0, "en": 40, "in": 3, "o": 0, "fade": 0, "rates": [65536, 256, 512]}, {"mode": 0, "f98": 0, "f9c": 8, "st": 0, "en": 40, "in": 0, "o": 1, "fade": 47, "rates": [65536, 8192, 4096]}, {"mode": 0, "f98": 0, "f9c": 8, "st": 0, "en": 40, "in": 5, "o": 0, "fade": 1048576, "rates": [65536, 4096, 8192]}, {"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 10, "o": 4, "rates": [65536, 4294963200, 2147483647]}, {"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 36, "o": 6, "rates": [65536, 2147483647, 131072]}, {"mode": 0, "f98": 1, "f9c": 0, "st": 0, "en": 40, "ls": 8, "le": 40, "in": 30, "o": 3, "rates": [65536, 24576, 24576]}, {"mode": 0, "f98": 1, "f9c": 8, "st": 0, "en": 36, "ls": 10, "le": 38, "in": 34, "o": 0, "fade": 768, "rates": [65536, 24576, 28672]}, {"mode": 0, "f98": 1, "f9c": 2, "st": 0, "en": 40, "ls": 8, "le": 40, "in": 34, "o": 2, "rates": [65536, 16384, 16384]}, {"mode": 0, "f98": 0, "f9c": 9, "st": 0, "en": 40, "in": 32, "o": 0, "fade": 768, "rates": [65536, 24576, 12288], "chain": [{"st": 2, "en": 44, "ls": 6, "le": 44}]}, {"mode": 0, "f98": 0, "f9c": 1, "st": 0, "en": 40, "in": 34, "o": 5, "rates": [65536, 65536, 65536], "chain": [{"st": 2, "en": 44, "ls": 6, "le": 42}, {"st": 4, "en": 44, "ls": 10, "le": 46}]}, {"mode": 0, "f98": 0, "f9c": 1, "st": 0, "en": 38, "in": 30, "o": 0, "rates": [65536, 256, 4294967280], "chain": [{"st": 0, "en": 44, "ls": 2, "le": 40}, {"st": 6, "en": 44, "ls": 8, "le": 42}, {"st": 3, "en": 44, "ls": 12, "le": 44}]}, {"mode": 0, "f98": 1, "f9c": 1, "st": 0, "en": 40, "ls": 8, "le": 40, "in": 36, "o": 7, "rates": [65536, 2147483647, 2147483647], "chain": [{"st": 2, "en": 44, "ls": 6, "le": 44}]}, {"mode": 1, "st": 0, "ln": 44, "en": 40, "le": 44, "f58": 0, "in": 36, "o": 0, "rates": [65536, 65536, 32768]}, {"mode": 1, "st": 0, "ln": 10, "en": 40, "le": 44, "f58": 0, "in": 30, "o": 2, "rates": [65536, 4294934528, 2147483647]}, {"mode": 1, "st": 2, "ln": 44, "en": 40, "le": 44, "f58": 32, "in": 38, "o": 1, "rates": [65536, 131072, 131072]}, {"mode": 1, "st": 4, "ln": 10, "en": 40, "le": 40, "f58": 32, "in": 36, "o": 0, "rates": [65536, 196608, 1]}]); },
  c3do_mixR() { return globalThis.DIFF_FIXTURES.c3do_mixrows([{"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 0, "o": 0, "rates": [32768, 65536, 65536]}, {"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 30, "o": 3, "rates": [98304, 16384, 32768]}, {"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 34, "o": 5, "rates": [131072, 4294934528, 2147483647]}, {"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 0, "o": 1, "rates": [16384, 24576, 28672], "ring": {"frac": 49152, "ip": 0, "hi": 0}}, {"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 2, "o": 0, "rates": [65536, 32767, 16384], "ring": {"frac": 32768, "ip": 3, "hi": 0}}, {"mode": 0, "f98": 1, "f9c": 0, "st": 0, "en": 40, "ls": 8, "le": 40, "in": 30, "o": 0, "rates": [65536, 65536, 65536]}, {"mode": 0, "f98": 1, "f9c": 0, "st": 0, "en": 40, "ls": 8, "le": 40, "in": 20, "o": 2, "rates": [163840, 24576, 20480]}, {"mode": 0, "f98": 1, "f9c": 0, "st": 0, "en": 40, "ls": 6, "le": 38, "in": 28, "o": 4, "rates": [24576, 2147483647, 2147483647], "ring": {"frac": 61440, "ip": 0, "hi": 0}}, {"mode": 0, "f98": 0, "f9c": 1, "st": 0, "en": 40, "in": 32, "o": 0, "rates": [73728, 65536, 32768], "chain": [{"st": 2, "en": 44, "ls": 6, "le": 44}]}, {"mode": 0, "f98": 0, "f9c": 1, "st": 0, "en": 40, "in": 34, "o": 6, "rates": [196608, 20480, 36864], "chain": [{"st": 2, "en": 44, "ls": 6, "le": 42}, {"st": 4, "en": 44, "ls": 10, "le": 46}]}, {"mode": 0, "f98": 0, "f9c": 1, "st": 0, "en": 38, "in": 30, "o": 0, "rates": [36864, 256, 4294967280], "chain": [{"st": 0, "en": 44, "ls": 2, "le": 40}, {"st": 6, "en": 44, "ls": 8, "le": 42}, {"st": 3, "en": 44, "ls": 12, "le": 44}]}, {"mode": 0, "f98": 1, "f9c": 1, "st": 0, "en": 40, "ls": 8, "le": 40, "in": 36, "o": 7, "rates": [81920, 2147483647, 131072], "chain": [{"st": 2, "en": 44, "ls": 6, "le": 44}]}, {"mode": 0, "f98": 1, "f9c": 2, "st": 0, "en": 40, "ls": 8, "le": 40, "in": 34, "o": 2, "rates": [81920, 16384, 16384]}, {"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 0, "o": 0, "rates": [65536, 65536, 65536]}, {"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 4, "o": 3, "rates": [65536, 32768, 65536]}, {"mode": 0, "f98": 0, "f9c": 8, "st": 0, "en": 40, "in": 0, "o": 0, "fade": 4096, "rates": [65536, 65536, 65536]}, {"mode": 0, "f98": 0, "f9c": 8, "st": 0, "en": 40, "in": 0, "o": 1, "fade": 1024, "rates": [32768, 65536, 32768]}, {"mode": 0, "f98": 0, "f9c": 8, "st": 0, "en": 40, "in": 2, "o": 2, "fade": 0, "rates": [32768, 8192, 12288]}, {"mode": 0, "f98": 0, "f9c": 9, "st": 0, "en": 40, "in": 28, "o": 0, "fade": 8192, "rates": [90112, 28672, 28672], "chain": [{"st": 2, "en": 44, "ls": 6, "le": 44}]}, {"mode": 1, "st": 0, "ln": 44, "en": 40, "le": 44, "f58": 0, "in": 36, "o": 0, "rates": [32768, 65536, 32768]}, {"mode": 1, "st": 0, "ln": 44, "en": 40, "le": 44, "f58": 32, "in": 36, "o": 2, "rates": [98304, 24576, 20480]}, {"mode": 1, "st": 4, "ln": 10, "en": 40, "le": 40, "f58": 32, "in": 36, "o": 0, "rates": [65536, 65536, 65536]}, {"mode": 1, "st": 2, "ln": 44, "en": 40, "le": 44, "f58": 32, "in": 30, "o": 1, "rates": [65536, 12288, 28672]}, {"mode": 1, "st": 0, "ln": 10, "en": 40, "le": 44, "f58": 32, "in": 30, "o": 3, "rates": [81920, 16384, 49152]}, {"mode": 1, "st": 0, "ln": 44, "en": 40, "le": 44, "f58": 0, "in": 34, "o": 0, "rates": [65536, 65536, 65536]}, {"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 0, "o": 0, "rates": [65536, 65536, 32768]}, {"mode": 1, "st": 0, "ln": 10, "en": 40, "le": 44, "f58": 0, "in": 30, "o": 0, "rates": [32768, 65536, 65536]}, {"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 0, "o": 1, "rates": [16384, 28672, 20480], "ring": {"frac": 4294963200, "ip": 65535, "hi": 0}}, {"mode": 0, "f98": 0, "f9c": 8, "st": 0, "en": 40, "in": 0, "o": 2, "fade": 20, "rates": [32768, 65536, 32768]}]); },
  c3do_mixRG() { return globalThis.DIFF_FIXTURES.c3do_mixrows([{"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 0, "o": 0, "rates": [32768, 65536, 65536]}, {"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 30, "o": 3, "rates": [98304, 16384, 32768]}, {"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 34, "o": 5, "rates": [131072, 4294934528, 2147483647]}, {"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 0, "o": 1, "rates": [16384, 24576, 28672], "ring": {"frac": 49152, "ip": 0, "hi": 0}}, {"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 2, "o": 0, "rates": [65536, 32767, 16384], "ring": {"frac": 32768, "ip": 3, "hi": 0}}, {"mode": 0, "f98": 0, "f9c": 1, "st": 0, "en": 40, "in": 32, "o": 0, "rates": [73728, 65536, 32768], "chain": [{"st": 2, "en": 44, "ls": 6, "le": 44}]}, {"mode": 0, "f98": 0, "f9c": 1, "st": 0, "en": 40, "in": 34, "o": 6, "rates": [196608, 20480, 36864], "chain": [{"st": 2, "en": 44, "ls": 6, "le": 42}, {"st": 4, "en": 44, "ls": 10, "le": 46}]}, {"mode": 0, "f98": 0, "f9c": 1, "st": 0, "en": 38, "in": 30, "o": 0, "rates": [36864, 256, 4294967280], "chain": [{"st": 0, "en": 44, "ls": 2, "le": 40}, {"st": 6, "en": 44, "ls": 8, "le": 42}, {"st": 3, "en": 44, "ls": 12, "le": 44}]}, {"mode": 0, "f98": 1, "f9c": 2, "st": 0, "en": 40, "ls": 8, "le": 40, "in": 34, "o": 2, "rates": [81920, 16384, 16384]}, {"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 0, "o": 0, "rates": [65536, 65536, 65536]}, {"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 4, "o": 3, "rates": [65536, 32768, 65536]}, {"mode": 0, "f98": 0, "f9c": 8, "st": 0, "en": 40, "in": 0, "o": 0, "fade": 4096, "rates": [65536, 65536, 65536]}, {"mode": 0, "f98": 0, "f9c": 8, "st": 0, "en": 40, "in": 0, "o": 1, "fade": 1024, "rates": [32768, 65536, 32768]}, {"mode": 0, "f98": 0, "f9c": 8, "st": 0, "en": 40, "in": 2, "o": 2, "fade": 0, "rates": [32768, 8192, 12288]}, {"mode": 0, "f98": 0, "f9c": 9, "st": 0, "en": 40, "in": 28, "o": 0, "fade": 8192, "rates": [90112, 28672, 28672], "chain": [{"st": 2, "en": 44, "ls": 6, "le": 44}]}, {"mode": 1, "st": 0, "ln": 44, "en": 40, "le": 44, "f58": 32, "in": 36, "o": 2, "rates": [98304, 24576, 20480]}, {"mode": 1, "st": 4, "ln": 10, "en": 40, "le": 40, "f58": 32, "in": 36, "o": 0, "rates": [65536, 65536, 65536]}, {"mode": 1, "st": 2, "ln": 44, "en": 40, "le": 44, "f58": 32, "in": 30, "o": 1, "rates": [65536, 12288, 28672]}, {"mode": 1, "st": 0, "ln": 10, "en": 40, "le": 44, "f58": 32, "in": 30, "o": 3, "rates": [81920, 16384, 49152]}, {"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 0, "o": 0, "rates": [65536, 65536, 32768]}, {"mode": 1, "st": 0, "ln": 10, "en": 40, "le": 44, "f58": 0, "in": 30, "o": 0, "rates": [32768, 65536, 65536]}, {"mode": 0, "f98": 0, "f9c": 0, "st": 0, "en": 40, "in": 0, "o": 1, "rates": [16384, 28672, 20480], "ring": {"frac": 4294963200, "ip": 65535, "hi": 0}}, {"mode": 0, "f98": 0, "f9c": 8, "st": 0, "en": 40, "in": 0, "o": 2, "fade": 20, "rates": [32768, 65536, 32768]}]); },
  // <<< MIX GENERATED
});
