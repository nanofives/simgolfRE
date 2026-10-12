// Fixtures for C3 batch c3dd (round 25, golf_clean.exe): the functions that reach the heap through heapAlloc 0x00474860
// and its _realloc 0x004a6980 growth step. Reimplementations: shim/src/re/c3dd.cpp. Registry: re/frida/registry.d/c3dd.py.
// Purposes: log/c3/c3dd_purpose.md. Only our own synthetic values: no game text, no live game object (the one live global
// a function writes, 0x00839650, is saved and cleared under the shared freeze helper and restored before the game's
// first thread resumes).
//
// HEAP. ddHeap() is re/frida/js/heapstandin.js's __heapStandIn over golf_clean.exe's malloc 0x004a504f, operator new
// 0x004a57f1, free 0x004a5007 and _realloc 0x004a6980, on the A/B thread only. Arena header (all state): +0 cursor,
// +4 allocation call counter, +8 fail mask (call k returns NULL when bit k is set), +0xc fail size. _realloc(p, n)
// carves a new block, copies min(old size, n) bytes, records the old pointer in the free log and returns the new block;
// the blocks the fixture makes itself (the heaps' data blocks, in $blk) are declared with register(ptr, size).
//
// S0Heap objects (heapAlloc's `this`): +4 flag byte, +8 block, +0xc bump pointer, +0x10 capacity, +0x14 free count. The
// block of an object lives in $blk, with `span` bytes behind it so the bump pointer can write past the capacity the
// object claims (the bump pointer is not moved when _realloc moves the block).
//
// RECORDING STUBS (machine code): each appends {tag, ecx, stack args...} to the recorder `rec` (rec+0 byte cursor, entries
// from rec+0x10) and returns rec-cursor ^ tag; thiscall stubs pop their arguments.
//
// Every key's state: $blk (objects), $arena (0x10 + arena size), $freelog (0x404), $rec (0x400), plus 0x00839650 where
// the function writes it.
(function () {
  const BLK = 0x20000;
  const ARENA = 0x8000;
  const keep = (x) => { globalThis.__diffKeepAlive.push(x); return x; };
  function u32(v) { v >>>= 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; }
  function fill32(p, n, v) { for (let i = 0; i + 4 <= n; i += 4) p.add(i).writeU32(v >>> 0); }

  function codeArena() {
    const size = Process.pageSize * 16;
    const base = keep(Memory.alloc(size));
    Memory.protect(base, size, 'rwx');
    let off = 0;
    return function (bytes) {
      const p = base.add(off);
      p.writeByteArray(bytes);
      off += (bytes.length + 15) & ~15;
      if (off > size) throw new Error('c3dd: code arena full');
      return p;
    };
  }
  function stubBytes(rec, tag, n, thiscall) {
    const r = u32(rec.toUInt32());
    const b = [];
    b.push(0xA1, ...r);                                              // mov eax, [REC]
    b.push(0x05, ...r);                                              // add eax, REC
    b.push(0xC7, 0x00, ...u32(tag));                                 // mov dword [eax], tag
    if (thiscall) b.push(0x89, 0x48, 0x04);                          // mov [eax+4], ecx
    else b.push(0xC7, 0x40, 0x04, ...u32(0xC0C0C0C0));
    for (let i = 1; i <= n; i++) b.push(0x8B, 0x54, 0x24, 4 * i, 0x89, 0x50, 4 + 4 * i);
    b.push(0x81, 0x05, ...r, ...u32(4 * (n + 2)));                   // add dword [REC], 4(n+2)
    b.push(0xA1, ...r);                                              // mov eax, [REC]
    b.push(0x35, ...u32(tag));                                       // xor eax, tag
    if (thiscall && n > 0) b.push(0xC2, 4 * n, 0x00); else b.push(0xC3);
    return b;
  }

  // opts: arenaSize, failMask, failSize, noHeap. Returns the builders and the fixture object.
  function env(opts) {
    opts = opts || {};
    const fx = {};
    const emit = codeArena();
    const blk = keep(Memory.alloc(BLK));
    fill32(blk, BLK, 0x6e6e6e6e);
    let cur = 0;
    function take(n, v) {
      const p = blk.add(cur);
      cur += (n + 15) & ~15;
      if (cur > BLK) throw new Error('c3dd: object block full');
      if (v !== undefined) fill32(p, (n + 3) & ~3, v);
      return p;
    }
    const rec = keep(Memory.alloc(0x400));
    fill32(rec, 0x400, 0);
    rec.writeU32(0x10);
    const nul = emit([0x33, 0xC0, 0xC3]);
    function stub(tag, n) { return emit(stubBytes(rec, tag, n, true)); }
    function vt(spec) {
      const t = keep(Memory.alloc(0x200));
      for (let i = 0; i < 0x80; i++) t.add(i * 4).writePointer(nul);
      for (const [slot, tag, n] of spec) t.add(slot).writePointer(stub(tag, n));
      return t;
    }
    function str(text) { return keep(Memory.allocUtf8String(text)); }
    fx.blk = blk; fx.rec = rec; fx.obj = blk;
    const h = __heapStandIn({
      module: 'golf_clean.exe',
      alloc: [0x004a504f - 0x400000, 0x004a57f1 - 0x400000],
      free: [0x004a5007 - 0x400000],
      realloc: [0x004a6980 - 0x400000],
      arenaSize: opts.arenaSize || ARENA,
      failMask: opts.failMask || 0,
      failSize: opts.failSize,
      exhaust: 'null',
    });
    fx.arena = h.arena; fx.freelog = h.log;
    return { fx: fx, take: take, stub: stub, vt: vt, str: str, rec: rec, h: h, n: 0 };
  }

  // An S0Heap: o = {flags, cap, free, noData, span}. The bump pointer is block + (cap - free) (the block start for
  // noData). Returns the object.
  function mkHeap(E, o) {
    const hp = E.take(0x20, 0x48480000 + E.n);
    const span = o.span || (o.cap + 0x800);
    const data = E.take(span, 0x44440000 + E.n);
    E.n++;
    hp.add(4).writeU8(o.flags || 0);
    if (o.noData) {
      hp.add(8).writePointer(ptr(0));
      hp.add(0xc).writePointer(data);
    } else {
      hp.add(8).writePointer(data);
      E.h.register(data, o.cap);
      hp.add(0xc).writePointer(data.add(o.cap > o.free ? o.cap - o.free : 0));
    }
    hp.add(0x10).writeS32(o.cap);
    hp.add(0x14).writeS32(o.free);
    return hp;
  }

  // Shared input strings: s0 "" (1 byte), s1 "tip" (4), s2 31 characters (32), s3 0x332 characters (0x333, the fail
  // size), s4 99 characters (100), s5 14 characters (15), s6 0x4ff characters (0x500), s7 0x7ff characters (0x800).
  const FAIL_SIZE = 0x333;
  function strings(E) {
    const t = ['', 'tip', 'synthetic text of 31 characters', 'x'.repeat(FAIL_SIZE - 1), 'y'.repeat(99), 'fourteen chars',
               'z'.repeat(0x4ff), 'w'.repeat(0x7ff)];
    t.forEach(function (s, i) { E.fx['s' + i] = E.str(s); });
  }

  // ======================================================================== heapAlloc
  // Heaps h0..h12 (see the registry for the vectors): flags, capacity, free count.
  const HEAPS = [
    { cap: 0x400, free: 0x400 },                    // h0  room
    { cap: 0x400, free: 0x10 },                     // h1  one growth step for n = 0x100
    { cap: 0x400, free: 0x10 },                     // h2  three steps for n = 0x900
    { flags: 1, cap: 0x400, free: 0x10 },           // h3  flag set, too small: NULL
    { flags: 1, cap: 0x400, free: 0x400 },          // h4  flag set, room (and too small for 0x500)
    { cap: 0, free: 0, noData: true },              // h5  no block yet: _realloc(NULL, 0x400)
    { cap: 0x400, free: 0x400 },                    // h6  room (n = 0 and n = 0x400)
    { cap: 0x400, free: 0x400 },                    // h7  one step for n = 0x401
    { flags: 0xfe, cap: 0x400, free: 0 },           // h8  bit 0 clear, other bits set: grows
    { cap: 0x400, free: -0x20 },                    // h9  negative free count
    { cap: 0x400, free: 0x300, flags: 0 },          // h10 room, then two steps
    { cap: 0x800, free: 0x40 },                     // h11 second heap size
    { flags: 3, cap: 0x400, free: 0x400 },          // h12 flag bit 0 with bit 1
  ];
  function heaps() {
    const E = env({});
    HEAPS.forEach(function (o, i) { E.fx['h' + i] = mkHeap(E, o); });
    return E.fx;
  }

  // ======================================================================== strDup / bufferCopyTo / Surface_get
  // Heaps: hf room (0x400 free), hs 4 free (growth for 32 bytes), hz 0 free, hflag flag set with room, hflagz flag set
  // with 0 free, hnull no block (cursor in a scratch block), hq 0x500 free (the 0x500 string just fits).
  function strHeaps(E) {
    const fx = E.fx;
    fx.hf = mkHeap(E, { cap: 0x400, free: 0x400, span: 0x1800 });
    fx.hs = mkHeap(E, { cap: 0x400, free: 4, span: 0x1800 });
    fx.hz = mkHeap(E, { cap: 0x400, free: 0, span: 0x1800 });
    fx.hflag = mkHeap(E, { flags: 1, cap: 0x400, free: 0x400, span: 0x1800 });
    fx.hflagz = mkHeap(E, { flags: 1, cap: 0x400, free: 0, span: 0x1800 });
    fx.hnull = mkHeap(E, { cap: 0, free: 0, noData: true, span: 0x1800 });
    fx.hq = mkHeap(E, { cap: 0x600, free: 0x500, span: 0x1800 });
  }
  function strdup() {
    const E = env({ failSize: FAIL_SIZE });
    strings(E);
    strHeaps(E);
    return E.fx;
  }

  // Records {+0x18 heap, +0x1c source, +0x20 value or second source, +0x24 value}; destination records r0..r3.
  // For bufferCopyTo the source string is +0x1c, for Surface_get it is +0x20 and +0x1c is a value.
  function records(E, mode) {
    const fx = E.fx;
    const heaps = [ptr(0), fx.hf, fx.hs, fx.hz, fx.hflag, fx.hflagz, fx.hnull, fx.hq];
    const srcs = [ptr(0), fx.s1, fx.s2, fx.s3, fx.s4, fx.s6, fx.s0, fx.s5];
    // r_i = heap i % 8, source i / 8 ... laid out explicitly so the registry can name each
    const spec = [
      [0, 0], [0, 2], [0, 3], [1, 2], [1, 1], [2, 2], [3, 5], [4, 2], [5, 2], [6, 1], [6, 5], [7, 5], [1, 0], [0, 6],
      [3, 3], [2, 7],
    ];
    spec.forEach(function (sp, i) {
      const r = E.take(0x30, 0x52450000 + i);
      r.add(0x18).writePointer(heaps[sp[0]]);
      if (mode === 'bufferCopyTo') {
        r.add(0x1c).writePointer(srcs[sp[1]]);
        r.add(0x20).writeU32(0x11110000 + i);
      } else {
        r.add(0x1c).writeU32(0x22220000 + i);
        r.add(0x20).writePointer(srcs[sp[1]]);
      }
      r.add(0x24).writeU32(0x33330000 + i);
      fx['r' + i] = r;
      const d = E.take(0x20, 0x44520000 + i);
      fx['d' + i] = d;
    });
    return fx;
  }
  function recs(mode) {
    const E = env({ failSize: FAIL_SIZE });
    strings(E);
    strHeaps(E);
    return records(E, mode);
  }

  // ======================================================================== strAssign
  // Objects {+4 flag byte, +8 block, +0xc, +0x10, +0x14, +0x18 owned flag}: a0 clean, a1 owned flag with a block, a2 a
  // block without the flag, a3 owned flag without a block, a4 flag byte 0xfe.
  function strassign() {
    const E = env({ failSize: FAIL_SIZE });
    const fx = E.fx;
    for (let i = 0; i < 6; i++) {
      const a = E.take(0x20, 0x41530000 + i);
      a.add(4).writeU8(i === 4 ? 0xfe : 0);
      a.add(0x18).writeU32(i === 1 || i === 3 ? 1 : 0);
      const owned = i === 1 || i === 2 || i === 5;
      a.add(8).writePointer(owned ? E.take(0x40, 0x4f420000 + i) : ptr(0));
      a.add(0xc).writePointer(owned ? a.add(8).readPointer() : ptr(0));
      a.add(0x10).writeU32(owned ? 0x40 : 0);
      a.add(0x14).writeU32(owned ? 0x40 : 0);
      fx['a' + i] = a;
    }
    return fx;
  }

  // ======================================================================== S3List_add / S3List_add2 / addImage /
  // postMessageLine / listAddEntry
  // A list: +0 vptr (slot 0 records), +4 vbptr -> vbtable whose entry +4 is 0x40, +8 head, +0xc cursor, +0x10 count,
  // +0x14 last, +0x18 heap; the virtual base at +0x40 holds a heap pointer at +0x48. o = {count, heap, vheap}.
  function mkList(E, o) {
    const L = E.take(0x60, 0x4c530000 + E.n++);
    L.writePointer(E.lvt);
    const vbt = E.take(0x10, 0);
    vbt.add(4).writeS32(0x40);
    L.add(4).writePointer(vbt);
    L.add(0x18).writePointer(o.heap || ptr(0));
    L.add(0x48).writePointer(o.vheap || ptr(0));
    const n = o.count || 0;
    const nodes = [];
    for (let k = 0; k < n; k++) {
      const nd = E.take(0x20, 0x4e530000 + 16 * E.n + k);
      nd.add(4).writeU32(0x1000 + k);
      nd.add(8).writePointer(E.take(0x20, 0x50530000 + 16 * E.n + k));
      nodes.push(nd);
    }
    nodes.forEach(function (nd, k) {
      nd.add(0xc).writePointer(nodes[(k + 1) % n]);
      nd.add(0x10).writePointer(nodes[(k + n - 1) % n]);
    });
    L.add(8).writePointer(n ? nodes[0] : ptr(0));
    L.add(0xc).writePointer(n ? nodes[n - 1] : ptr(0));
    L.add(0x10).writeS32(o.rawCount !== undefined ? o.rawCount : n);
    L.add(0x14).writeS32(o.n === undefined ? -1 : o.n);
    return L;
  }
  // Lists l0..: shapes (count, heap kind): kinds 0 malloc (no heap), 1 direct heap, 2 virtual-base heap.
  const LIST_SHAPES = [
    [0, 0], [0, 1], [0, 2], [1, 0], [1, 1], [1, 2], [2, 0], [3, 1], [4, 2], [2, 2], [3, 0], [5, 1],
  ];
  function lists(opts, hs) {
    const E = env(opts || {});
    E.lvt = E.vt([[0, 0xD0530000, 1]]);
    strings(E);
    const fx = E.fx;
    fx.hf = mkHeap(E, { cap: 0x400, free: 0x400, span: 0x1800 });
    fx.hv = mkHeap(E, { cap: 0x400, free: 0x400, span: 0x1800 });
    // hs: 0x30 free, so a node (0x1c) fits but the payload (0x18 or 0x10) after it forces one growth step
    fx.hs = mkHeap(E, { cap: 0x400, free: 0x30, span: 0x1800 });
    fx.hz = mkHeap(E, { cap: 0x400, free: 0, span: 0x1800 });
    LIST_SHAPES.forEach(function (sp, i) {
      const heap = sp[1] === 1 ? (i % 2 ? fx.hs : fx.hf) : null;
      const vheap = sp[1] === 2 ? (i % 2 ? fx.hz : fx.hv) : null;
      fx['l' + i] = mkList(E, { count: sp[0], heap: heap, vheap: vheap });
    });
    // lists with a heap that grows more than once (hz: 0 free) and with the owned global in use
    fx.lz = mkList(E, { count: 0, heap: fx.hz });
    fx.lz2 = mkList(E, { count: 2, heap: fx.hz });
    __freeze.thread();
    __freeze.save(ptr(0x00839650), 4);
    ptr(0x00839650).writeU32(0);
    return fx;
  }

  // TextView with the embedded list at +0x13e0 and the counter at +0x137c.
  function textviews(opts) {
    const E = env(opts || {});
    E.lvt = E.vt([[0, 0xD0540000, 1]]);
    const fx = E.fx;
    fx.hf = mkHeap(E, { cap: 0x400, free: 0x400, span: 0x1800 });
    fx.hs = mkHeap(E, { cap: 0x400, free: 0x30, span: 0x1800 });
    const kinds = [[0, 0], [0, 1], [1, 0], [2, 1], [0, 0], [3, 0]];
    kinds.forEach(function (sp, i) {
      const tv = E.take(0x1500, 0x54560000 + i);
      tv.add(0x137c).writeS32(i * 3);
      const L = E.take(0x60, 0x4c540000 + i);
      // the list lives inside the TextView: build it in place
      const lp = tv.add(0x13e0);
      lp.writePointer(E.lvt);
      const vbt = E.take(0x10, 0);
      vbt.add(4).writeS32(0x40);
      lp.add(4).writePointer(vbt);
      lp.add(0x18).writePointer(sp[1] === 1 ? (i % 2 ? fx.hs : fx.hf) : ptr(0));
      lp.add(0x48).writePointer(ptr(0));
      const n = sp[0];
      const nodes = [];
      for (let k = 0; k < n; k++) {
        const nd = E.take(0x20, 0x4e540000 + 16 * i + k);
        nd.add(4).writeU32(0x2000 + k);
        nd.add(8).writePointer(E.take(0x20, 0x50540000 + 16 * i + k));
        nodes.push(nd);
      }
      nodes.forEach(function (nd, k) {
        nd.add(0xc).writePointer(nodes[(k + 1) % n]);
        nd.add(0x10).writePointer(nodes[(k + n - 1) % n]);
      });
      lp.add(8).writePointer(n ? nodes[0] : ptr(0));
      lp.add(0xc).writePointer(n ? nodes[n - 1] : ptr(0));
      lp.add(0x10).writeS32(n);
      lp.add(0x14).writeS32(-1);
      lp.add(0x1c).writeU32(0x77770000 + i);
      lp.add(0x20).writeU32(0x78780000 + i);
      lp.add(0x24).writeU32(0x79790000 + i);
      fx['t' + i] = tv;
    });
    __freeze.thread();
    __freeze.save(ptr(0x00839650), 4);
    ptr(0x00839650).writeU32(0);
    return fx;
  }

  // listAddEntry objects: [this] points at a table whose +8 is the virtual-base offset 0x100; the list is at this +
  // 0x1c0, its count (this + 0x1d0) is the slot index; the owned strings are at this + 0x2c + 4 * index, the values at
  // this + 0x54 + 4 * index. e_i: slot index i (the count), with the first `i` nodes in the ring.
  function entries(opts) {
    const E = env(opts || {});
    E.lvt = E.vt([[0, 0xD0550000, 1]]);
    const fx = E.fx;
    strings(E);
    fx.hf = mkHeap(E, { cap: 0x400, free: 0x400, span: 0x1800 });
    const tbl = E.take(0x10, 0);
    tbl.add(8).writeS32(0x100);
    const counts = [0, 1, 2, 3, 9, 10, 0, 5, 0, 4];
    counts.forEach(function (c, i) {
      const e = E.take(0x2c0, 0x45550000 + i);
      e.writePointer(tbl);
      const lp = e.add(0x100 + 0xc0);
      lp.writePointer(E.lvt);
      const vbt = E.take(0x10, 0);
      vbt.add(4).writeS32(0x40);
      lp.add(4).writePointer(vbt);
      lp.add(0x18).writePointer(i % 3 === 2 ? fx.hf : ptr(0));
      lp.add(0x48).writePointer(ptr(0));
      const nodes = [];
      for (let k = 0; k < c; k++) {
        const nd = E.take(0x20, 0x4e550000 + 16 * i + k);
        nd.add(4).writeU32(0x3000 + k);
        nd.add(8).writePointer(E.take(0x20, 0x50550000 + 16 * i + k));
        nodes.push(nd);
      }
      nodes.forEach(function (nd, k) {
        nd.add(0xc).writePointer(nodes[(k + 1) % c]);
        nd.add(0x10).writePointer(nodes[(k + c - 1) % c]);
      });
      lp.add(8).writePointer(c ? nodes[0] : ptr(0));
      lp.add(0xc).writePointer(c ? nodes[c - 1] : ptr(0));
      lp.add(0x10).writeS32(c);
      lp.add(0x14).writeS32(-1);
      // the ten owned-string slots start empty; the odd objects own a string in slot `c`
      for (let k = 0; k < 10; k++) e.add(0x2c + 4 * k).writePointer(ptr(0));
      if (i % 2 === 1 && c < 10) {
        const old = E.take(0x20, 0x4f550000 + i);
        e.add(0x2c + 4 * c).writePointer(old);
      }
      fx['e' + i] = e;
    });
    __freeze.thread();
    __freeze.save(ptr(0x00839650), 4);
    ptr(0x00839650).writeU32(0);
    return fx;
  }

  // ======================================================================== View_attach
  // Views {+0x9c flags, +0xa0 flag byte, +0x130 parent, list +0x138 head, +0x13c cursor, +0x140 count, +0x144 last, +0x148
  // heap}. u0..u3 attach (flags clear), u4 has 0x200000, u5 has +0xa0 bit 3, u6 and u7 have 0x1000 (u7 is also a target
  // with 0x1000 set), v0.. targets: v0 empty/no heap, v1 empty/heap, v2 one node/no heap, v3 two nodes/heap, v4 flags
  // 0x1000, v5 three nodes/no heap, v6 count 1 with no nodes (corrupt head), v7 empty/heap with room only for the node.
  function views(opts) {
    const E = env(opts || {});
    const fx = E.fx;
    fx.hf = mkHeap(E, { cap: 0x400, free: 0x400, span: 0x1800 });
    fx.hs = mkHeap(E, { cap: 0x400, free: 0x1c, span: 0x1800 });
    function view(i, flags, f2) {
      const v = E.take(0x160, 0x56570000 + i);
      v.add(0x9c).writeU32(flags);
      v.add(0xa0).writeU8(f2 || 0);
      v.add(0x130).writePointer(ptr(0));
      v.add(0x138).writePointer(ptr(0));
      v.add(0x13c).writePointer(ptr(0));
      v.add(0x140).writeS32(0);
      v.add(0x144).writeS32(0x777);
      v.add(0x148).writePointer(ptr(0));
      return v;
    }
    const U = [view(0, 0), view(1, 0x11), view(2, 0x2000), view(3, 0x4), view(4, 0x200000), view(5, 0, 8),
               view(6, 0x1000), view(7, 0x1000)];
    U.forEach(function (u, i) { fx['u' + i] = u; });
    function target(i, flags, count, heap) {
      const v = view(20 + i, flags);
      v.add(0x148).writePointer(heap || ptr(0));
      const nodes = [];
      for (let k = 0; k < count; k++) {
        const nd = E.take(0x20, 0x4e570000 + 16 * i + k);
        nd.add(4).writePointer(ptr(0x1000 + k));
        nd.add(8).writePointer(E.take(0x10, 0x50570000 + 16 * i + k));
        nodes.push(nd);
      }
      nodes.forEach(function (nd, k) {
        nd.add(0xc).writePointer(nodes[(k + 1) % count]);
        nd.add(0x10).writePointer(nodes[(k + count - 1) % count]);
      });
      v.add(0x138).writePointer(count ? nodes[0] : ptr(0));
      v.add(0x13c).writePointer(count ? nodes[count - 1] : ptr(0));
      v.add(0x140).writeS32(count);
      return v;
    }
    const T = [target(0, 0, 0, null), target(1, 0, 0, fx.hf), target(2, 0, 1, null), target(3, 0, 2, fx.hf),
               target(4, 0x1000, 0, null), target(5, 0, 3, null), target(6, 0, 0, fx.hs), target(7, 0, 2, fx.hs)];
    T.forEach(function (v, i) { fx['v' + i] = v; });
    return fx;
  }

  // ======================================================================== Part 2: destructors and constructors
  // A cache {vtable +0, handle +4, five entries from +8: {index +0, object +4, byte +8, byte +9, block +0xc}}; the
  // objects' vtable slot 0 records (a scalar deleting destructor, 1 argument).
  function cache(E, i, handle, mask) {
    const c = E.take(0x58, 0x43430000 + i);
    c.add(4).writeU32(handle);
    const ovt = E.vt([[0, 0xD0D00000 + i, 1]]);
    for (let k = 0; k < 5; k++) {
      const e = c.add(8 + 0x10 * k);
      if ((mask >> k) & 1) { const o = E.take(0x10, 0x4f420000 + k); o.writePointer(ovt); e.add(4).writePointer(o); }
      else e.add(4).writePointer(ptr(0));
      e.add(0xc).writePointer(((mask >> (k + 5)) & 1) ? E.take(0x10, 0x42420000 + k) : ptr(0));
    }
    return c;
  }
  // The display at 0x0083ad50 (first thread frozen, saved): a fixture display whose slots +0x8c, +0x90 and +0x94 record.
  function makeDisplay(E) {
    const d = E.take(0x20, 0);
    d.writePointer(E.vt([[0x8c, 0xD18C0000, 1], [0x90, 0xD1590000, 1], [0x94, 0xD1594000, 1]]));
    return d;
  }
  function swapDisplay(d, mode) {
    __freeze.save(ptr(0x0083ad50), 4);
    ptr(0x0083ad50).writePointer(mode === 'disp' ? __recDisplay.guard(d) : ptr(0));
  }

  // R2C / Flic / Stream objects (0x100 bytes): see the bit list in the registry. The Surface sub-object at +8 has its
  // handle at +0xc, block at +0x10, cache at +0x14.
  const FLIC_MASKS = [0, 0x7fff, 0x1, 0x2, 0xe, 0x10, 0x12, 0x70, 0x380, 0xc00, 0x1000, 0x2000, 0x4000, 0x5555, 0x2aaa,
                      0x7ffe];
  function flics(mode, heap) {
    const E = env(heap || {});
    const fx = E.fx;
    const kvt = E.vt([[0, 0xD2000000, 1], [0xc, 0xD2000001, 1]]);
    function obj(tagBase) { const o = E.take(0x20, 0x4f4f0000 + tagBase); o.writePointer(kvt); return o; }
    FLIC_MASKS.forEach(function (m, i) {
      const o = E.take(0x100, 0x46460000 + i);
      const bit = (b) => (m >> b) & 1;
      o.add(4).writeU32(0x1111 + i);
      o.add(0x38).writeU32(0x2222 + i);
      o.add(0x3c).writeU32(0x3333 + i);
      o.add(0x40).writeU32(0x4444 + i);
      o.add(0x34).writePointer(bit(0) ? obj(1) : ptr(0));
      o.add(0x44).writePointer(bit(1) ? E.take(0x10, 0x42440000 + i) : ptr(0));
      o.add(0x48).writePointer(bit(2) ? E.take(0x10, 0x42480000 + i) : ptr(0));
      o.add(0x4c).writePointer(bit(3) ? E.take(0x10, 0x424c0000 + i) : ptr(0));
      o.add(0x54).writePointer(bit(4) ? obj(5) : ptr(0));
      o.add(0x5c).writePointer(bit(5) ? obj(6) : ptr(0));
      o.add(0x60).writePointer(bit(6) ? obj(7) : ptr(0));
      o.add(0x64).writePointer(bit(7) ? cache(E, i, (i & 1) ? 0x4a00 + i : 0, 0x3ff ^ (i * 0x55)) : ptr(0));
      o.add(0x68).writePointer(bit(8) ? cache(E, i + 16, (i & 2) ? 0x4b00 + i : 0, 0x1f ^ (i * 0x21)) : ptr(0));
      o.add(0x78).writePointer(bit(9) ? cache(E, i + 32, 0x4c00 + i, 0x3e0) : ptr(0));
      o.add(0x6c).writePointer(bit(10) ? obj(11) : ptr(0));
      o.add(0x70).writePointer(bit(11) ? obj(12) : ptr(0));
      // the Surface at +8: handle +0xc, name block +0x10, cache +0x14
      o.add(0xc).writeU32(bit(12) ? 0x4d00 + i : 0);
      o.add(0x10).writePointer(bit(13) ? E.take(0x20, 0x4e4d0000 + i) : ptr(0));
      o.add(0x14).writePointer(bit(14) ? cache(E, i + 48, (i & 4) ? 0x4e00 + i : 0, 0x3ff ^ (i * 0x33)) : ptr(0));
      o.add(0x74).writeU32(0x7474 + i);
      o.add(0x50).writeU32(0x5050 + i);
      o.add(0x58).writeU32(0x5858 + i);
      o.add(0x7c).writeU32(0x7c7c + i);
      fx['f' + i] = o;
    });
    const d = makeDisplay(E);
    __freeze.thread();
    swapDisplay(d, mode);
    return fx;
  }

  // Image objects (0x300 bytes): handle +4, cache +0xac, forty owned pointers from +0x1b4. The globals 0x00839ab0 (g),
  // 0x0083ad44 (a value the function copies to +0x5c) and the display 0x0083ad50 are frozen and saved.
  const IMAGE_MASKS = [0, 0xffffffffff, 0x1, 0x8000000000, 0x5555555555, 0xaaaaaaaaaa, 0x3ff, 0xf0f0f0f0f0, 0x100000, 0x2];
  function images(g) {
    const E = env({});
    const fx = E.fx;
    for (let i = 0; i < 12; i++) {
      const o = E.take(0x300, 0x49490000 + i);
      o.add(4).writeU32(i % 2 ? 0x4f00 + i : 0);
      o.add(0xac).writePointer(i % 3 === 1 ? cache(E, i, (i & 4) ? 0x4a00 + i : 0, 0x3ff ^ (i * 0x55)) : ptr(0));
      const m = IMAGE_MASKS[i % IMAGE_MASKS.length];
      for (let k = 0; k < 40; k++) {
        const on = k < 30 ? (((m / Math.pow(2, k)) & 1) === 1) : ((i + k) % 3 === 0);
        o.add(0x1b4 + 4 * k).writePointer(on ? E.take(0x10, 0x4b4b0000 + 64 * i + k) : ptr(0));
      }
      fx['i' + i] = o;
    }
    const d = makeDisplay(E);
    __freeze.thread();
    swapDisplay(d, 'disp');
    __freeze.save(ptr(0x00839ab0), 4);
    ptr(0x00839ab0).writeU32(g);
    __freeze.save(ptr(0x0083ad44), 4);
    ptr(0x0083ad44).writeU32(0x7a7a7a00 + g);
    return fx;
  }

  // Objects for the static Surface-array constructors: six 0x900-byte blocks with distinct fill.
  function gsurf() {
    const E = env({ noHeapNeeded: true });
    for (let i = 0; i < 6; i++) E.fx['g' + i] = E.take(0x900, 0x5a5a0000 + 0x111 * i);
    return E.fx;
  }
  // The global Surface 0x00839a68 (0x30 bytes, first thread frozen, saved) gets a fill per variant.
  function gsurfGlobal(v) {
    const E = env({});
    __freeze.thread();
    __freeze.save(ptr(0x00839a68), 0x30);
    fill32(ptr(0x00839a68), 0x30, 0x6c6c0000 + v * 0x1111);
    return E.fx;
  }

  // VoiceRx objects (0x70 bytes, distinct fill): the constructor writes +0 .. +0x58 and the 10-byte name block at +0x50.
  function voice() {
    const E = env({});
    for (let i = 0; i < 6; i++) E.fx['o' + i] = E.take(0x70, 0x56520000 + 0x1111 * i);
    return E.fx;
  }

  const F = {
    c3dd_heaps: heaps,
    c3dd_strdup: strdup,
    c3dd_bcopy() { return recs('bufferCopyTo'); },
    c3dd_sget() { return recs('Surface_get'); },
    c3dd_strassign: strassign,
    c3dd_lists() { return lists({}); },
    c3dd_lists_fail0() { return lists({ failMask: 1 }); },
    c3dd_lists_fail1() { return lists({ failMask: 2 }); },
    c3dd_views() { return views({}); },
    c3dd_views_fail0() { return views({ failMask: 1 }); },
    c3dd_views_fail1() { return views({ failMask: 2 }); },
    c3dd_textviews() { return textviews({}); },
    c3dd_textviews_fail() { return textviews({ failMask: 1 }); },
    c3dd_entries() { return entries({}); },
    c3dd_entries_fail0() { return entries({ failMask: 1 }); },
    c3dd_entries_fail1() { return entries({ failMask: 2 }); },
    c3dd_flic_disp() { return flics('disp'); },
    c3dd_images_a() { return images(0); },
    c3dd_images_b() { return images(1); },
    c3dd_gsurf: gsurf,
    c3dd_voice: voice,
    c3dd_gsurfg_a() { return gsurfGlobal(0); },
    c3dd_gsurfg_b() { return gsurfGlobal(1); },
    c3dd_gsurfg_c() { return gsurfGlobal(2); },
  };
  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
