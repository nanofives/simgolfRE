// Fixtures for C3 batch c3cz (round 24, golf_clean.exe): functions earlier batches dropped for the CRT heap.
// Reimplementations: shim/src/re/c3cz.cpp. Registry: re/frida/registry.d/c3cz.py. Purposes: log/c3/c3cz_purpose.md.
// Only our own synthetic values: no game text, no live game object (the few live globals a function writes are
// swapped under the shared freeze helper and restored before the game's first thread resumes).
//
// HEAP. czHeap() is re/frida/js/heapstandin.js's method with scheduled failures added. For the length of one key it
// replaces golf_clean.exe's malloc 0x004a504f and operator new 0x004a57f1 with an arena allocator and free 0x004a5007
// (operator delete 0x004a4ffc tail-calls it) with a recorder, on the A/B thread only; every other thread is
// forwarded to the real CRT. The arena header is state, so both arms see the same sequence:
//   arena+0 cursor, +4 allocation call counter, +8 fail mask (call k returns NULL when bit k is set), +0xc fail size
//   (a request of exactly that size returns NULL), blocks from arena+0x10, 16-byte aligned, filled 0xcd.
// A request larger than what is left returns NULL, as the CRT does for an impossible size (Vec_release's capacity
// 0 asks for 0xffffffd8 bytes). free log: +0 count, pointers from +4.
//
// RECORDING STUBS (machine code, c3cu's design): each appends {tag, ecx (thiscall) or 0xC0C0C0C0, stack args...} to
// the recorder `rec` (rec+0 byte cursor, entries from rec+0x10) and returns rec-cursor ^ tag; thiscall stubs pop
// their arguments. No JavaScript runs inside a vector except the heap callbacks and the libjpeg memory callbacks.
//
// Every key's state: $blk (objects, 0x10000 bytes, pre-filled 0x6e6e6e6e), $arena (0x10 + arena size), $freelog
// (0x404) and $rec (0x400), plus the live globals a function writes (listed per key in the registry).
(function () {
  const BLK = 0x10000;
  const keep = (x) => { globalThis.__diffKeepAlive.push(x); return x; };
  function u32(v) { v >>>= 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; }
  function fill32(p, n, v) { for (let i = 0; i + 4 <= n; i += 4) p.add(i).writeU32(v >>> 0); }

  // ---- heap stand-in with scheduled failures -------------------------------------------------------------------
  const live = {};
  function replaceAt(at, cb) {
    const k = at.toString();
    if (live[k]) { try { Interceptor.revert(at); } catch (e) { /* already reverted */ } delete live[k]; }
    Interceptor.replace(at, cb);
    live[k] = true;
  }
  function revertAt(at) {
    const k = at.toString();
    if (!live[k]) return;
    Interceptor.revert(at);
    delete live[k];
  }
  function czHeap(o) {
    const tid = Process.getCurrentThreadId();
    const size = o.arenaSize || 0x4000;
    const arena = __keepAlloc(size + 0x10);
    fill32(arena, size + 0x10, 0xcdcdcdcd);
    arena.writeU32(0);
    arena.add(4).writeU32(0);
    arena.add(8).writeU32((o.failMask || 0) >>> 0);
    arena.add(0xc).writeU32(o.failSize === undefined ? 0xffffffff : (o.failSize >>> 0));
    const log = __keepAlloc(0x404);
    fill32(log, 0x404, 0);
    const reverts = [];
    [0x004a504f, 0x004a57f1].forEach(function (va) {
      const at = ptr(va);
      const orig = new NativeFunction(at, 'pointer', ['uint32']);
      const cb = new NativeCallback(function (sz) {
        if (Process.getCurrentThreadId() !== tid) return orig(sz);
        const k = arena.add(4).readU32();
        arena.add(4).writeU32(k + 1);
        const mask = arena.add(8).readU32();
        if ((k < 32 && ((mask >>> k) & 1)) || sz === arena.add(0xc).readU32()) return ptr(0);
        const cur = arena.readU32();
        const need = ((sz + 15) & ~15) >>> 0;
        if (sz > size || cur + need > size) return ptr(0);
        const p = arena.add(0x10 + cur);
        if (sz) p.writeByteArray(new Array(sz).fill(0xcd));
        arena.writeU32(cur + need);
        return p;
      }, 'pointer', ['uint32']);
      keep(cb); keep(orig);
      replaceAt(at, cb);
      reverts.push(at);
    });
    {
      const at = ptr(0x004a5007);
      const orig = new NativeFunction(at, 'void', ['pointer']);
      const cb = new NativeCallback(function (p) {
        if (Process.getCurrentThreadId() !== tid) { orig(p); return; }
        const n = log.readU32();
        if (n < 0x100) { log.add(4 + 4 * n).writePointer(p); log.writeU32(n + 1); }
      }, 'void', ['pointer']);
      keep(cb); keep(orig);
      replaceAt(at, cb);
      reverts.push(at);
    }
    Interceptor.flush();
    setTimeout(function () { reverts.forEach(revertAt); Interceptor.flush(); }, 0);
    return { arena: arena, log: log };
  }

  // ---- recording stubs -------------------------------------------------------------------------------------------
  function codeArena() {
    const size = Process.pageSize * 32;
    const base = keep(Memory.alloc(size));
    Memory.protect(base, size, 'rwx');
    let off = 0;
    return function (bytes) {
      const p = base.add(off);
      p.writeByteArray(bytes);
      off += (bytes.length + 15) & ~15;
      if (off > size) throw new Error('c3cz: code arena full');
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

  // ---- the environment every key builds --------------------------------------------------------------------------
  // opts.heap: czHeap options or false; returns { fx, take(n, fill), stub(tag, n), vt(spec), str(text) }.
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
      if (cur > BLK) throw new Error('c3cz: object block full');
      if (v !== undefined) fill32(p, (n + 3) & ~3, v);
      return p;
    }
    const rec = keep(Memory.alloc(0x400));
    fill32(rec, 0x400, 0);
    rec.writeU32(0x10);
    const nul = emit([0x33, 0xC0, 0xC3]);
    function stub(tag, n, cdecl) { return emit(stubBytes(rec, tag, n, !cdecl)); }
    // A 0x80-slot vtable outside $blk: listed slots record, the rest `xor eax,eax; ret`.
    function vt(spec) {
      const t = keep(Memory.alloc(0x200));
      for (let i = 0; i < 0x80; i++) t.add(i * 4).writePointer(nul);
      for (const [slot, tag, n] of spec) t.add(slot).writePointer(stub(tag, n));
      return t;
    }
    // Input strings: outside the state (the functions only read them).
    function str(text) { return keep(Memory.allocUtf8String(text)); }
    fx.blk = blk; fx.rec = rec; fx.obj = blk;
    if (opts.heap !== false) {
      const h = czHeap(opts.heap || {});
      fx.arena = h.arena; fx.freelog = h.log;
    } else {
      // keys without the stand-in still list $arena / $freelog in their state: give them empty regions
      fx.arena = keep(Memory.alloc(0x4010)); fill32(fx.arena, 0x4010, 0);
      fx.freelog = keep(Memory.alloc(0x404)); fill32(fx.freelog, 0x404, 0);
    }
    return { fx: fx, take: take, stub: stub, vt: vt, str: str, rec: rec };
  }
  // The shared input strings: s0 "" (1 byte), s1 4 bytes, s2 32 bytes, s3 0x333 bytes (the fail size of the keys
  // that schedule one), s4 100 bytes, s5 15 bytes.
  const FAIL_SIZE = 0x333;
  function strings(E) {
    const t = ['', 'tip', 'synthetic text of 31 characters', 'x'.repeat(FAIL_SIZE - 1), 'y'.repeat(99), 'fourteen chars'];
    t.forEach(function (s, i) { E.fx['s' + i] = E.str(s); });
  }

  // Delete-through-virtual-base objects: [obj] = a vbtable whose entry +4 is `off`; at obj+off sits a vtable whose
  // slot 0 records (thiscall, 1 argument: the 1 every caller pushes).
  function vbObj(E, size, off, tag) {
    const o = E.take(size, 0x5e5e5e5e);
    const vbt = E.take(0x10, 0);
    vbt.add(4).writeS32(off);
    o.writePointer(vbt);
    o.add(off).writePointer(E.vt([[0, tag, 1]]));
    return o;
  }

  // ======================================================================== Part 1: CRT heap leaves
  // ---- ChildArray: {items +0x224, capacity +0x228, count +0x22c}; children with the vtable slot 8 recording and
  // the flag word +0x9c (bit 0x2000000 on the even children: appended, not put first).
  const ARRAYS = [
    [0, 0, false], [0, 0, true], [10, 0, true], [10, 3, true], [10, 10, true], [20, 5, true], [20, 10, true],
    [30, 15, true], [5, 2, true], [-5, 0, false], [1, 1, true], [12, 2, true], [40, 25, true], [10, -1, false],
    [11, 1, true], [9, 9, true],
  ];
  function arrays() {
    const E = env({ heap: { arenaSize: 0x4000 } });
    const fx = E.fx;
    const kids = [];
    const kvt = E.vt([[8, 0xC8110000, 0]]);
    for (let i = 0; i < 12; i++) {
      const c = E.take(0x100, 0x4b4b0000 + i);
      c.writePointer(kvt);
      c.add(0x9c).writeU32((i % 2 === 0 ? 0x2000000 : 0) | (i * 0x11));
      kids.push(c);
      fx['c' + i] = c;
    }
    ARRAYS.forEach(function ([cap, count, hasItems], i) {
      const a = E.take(0x230, 0x41410000 + i);
      const items = E.take(0x80, 0x49490000 + i);
      for (let k = 0; k < Math.max(count, 0) && k < 32; k++) items.add(4 * k).writePointer(kids[(k * 5 + i) % 12]);
      a.add(0x224).writePointer(hasItems ? items : ptr(0));
      a.add(0x228).writeS32(cap);
      a.add(0x22c).writeS32(count);
      fx['a' + i] = a;
    });
    return fx;
  }

  // ---- Buf objects {vtable +0, flag byte +4, data +8, +0xc, +0x10, +0x14}: the odd ones hold a buffer.
  function bufs(heap) {
    const E = env({ heap: heap });
    for (let i = 0; i < 12; i++) {
      const b = E.take(0x20, 0x42420000 + i);
      b.writeU32(0x004ba84c);
      b.add(8).writePointer(i % 2 ? E.take(0x20, 0x44440000 + i) : ptr(0));
      E.fx['b' + i] = b;
    }
    return E.fx;
  }

  // ---- EditBoxes {buffer +0x574, size +0x578}; Buttons {tooltip +0x5f8}; string-field holders {fields +0x2c..+0x38}
  function texts(heap) {
    const E = env({ heap: heap });
    strings(E);
    const olds = ['old text one', 'second old string', '', 'x', 'an old text of thirty characters'];
    for (let i = 0; i < 12; i++) {
      const eb = E.take(0x580, 0x45450000 + i);
      const has = i % 3 !== 0;
      if (has) {
        const s = olds[i % olds.length];
        const p = E.take(0x40, 0x5a5a5a5a);
        p.writeUtf8String(s);
        eb.add(0x574).writePointer(p);
      } else {
        eb.add(0x574).writePointer(ptr(0));
      }
      eb.add(0x578).writeS32([0, 5, 12, 3, 20, 7, 0, 9, 31, 1, 2, 16][i]);
      E.fx['e' + i] = eb;

      const bt = E.take(0x600, 0x42540000 + i);
      bt.add(0x5f8).writePointer(i % 2 ? E.take(0x20, 0x54540000 + i) : ptr(0));
      E.fx['t' + i] = bt;

      const sf = E.take(0x40, 0x53460000 + i);
      for (let k = 0; k < 4; k++) {
        const on = ((i + k) % 3) !== 0;
        if (on) { const p = E.take(0x20, 0x5a5a5a5a); p.writeUtf8String('field ' + i + '.' + k); sf.add(0x2c + 4 * k).writePointer(p); }
        else sf.add(0x2c + 4 * k).writePointer(ptr(0));
      }
      E.fx['f' + i] = sf;
    }
    return E.fx;
  }

  // ---- listPush lists {head +4, tail +8, count +0xc, limit +0x10}, nodes {a +0, b +4, data +8, size +0xc,
  // next +0x10}. L0 empty, L1 one node, L2 three nodes, L3 full (count == limit), L4 empty with limit 0 (full),
  // L5 two nodes with payloads.
  function pushLists(heap) {
    const E = env({ heap: heap });
    const shapes = [[0, 8], [1, 8], [3, 8], [4, 4], [0, 0], [2, 10]];
    shapes.forEach(function ([n, limit], i) {
      const L = E.take(0x20, 0x4c4c0000 + i);
      const nodes = [];
      for (let k = 0; k < n; k++) {
        const nd = E.take(0x20, 0x4e4e0000 + i * 16 + k);
        nd.add(8).writePointer(k % 2 ? E.take(0x10, 0x50500000 + k) : ptr(0));
        nodes.push(nd);
      }
      nodes.forEach(function (nd, k) { nd.add(0x10).writePointer(k + 1 < n ? nodes[k + 1] : ptr(0)); });
      L.add(4).writePointer(n ? nodes[0] : ptr(0));
      L.add(8).writePointer(n ? nodes[n - 1] : ptr(0));
      L.add(0xc).writeS32(n);
      L.add(0x10).writeS32(limit);
      E.fx['L' + i] = L;
    });
    E.fx.out = E.take(0x10, 0x0f0f0f0f);
    return E.fx;
  }

  // ---- listEraseRange: a std::list {size +8} with a sentinel S and five nodes n1..n5 {next +0, prev +4}, circular.
  function eraseList() {
    const E = env({ heap: {} });
    const L = E.take(0x10, 0x4c450000);
    const nodes = [];
    for (let k = 0; k < 6; k++) nodes.push(E.take(0x10, 0x4e450000 + k));
    for (let k = 0; k < 6; k++) {
      nodes[k].writePointer(nodes[(k + 1) % 6]);
      nodes[k].add(4).writePointer(nodes[(k + 5) % 6]);
    }
    L.add(4).writePointer(nodes[0]);
    L.add(8).writeS32(5);
    E.fx.L = L;
    nodes.forEach(function (n, k) { E.fx['n' + k] = n; });
    E.fx.out = E.take(0x10, 0x0f0f0f0f);
    return E.fx;
  }

  // ---- allocTileBuffer: the std::list globals 0x00820f18 (allocator byte), 0x00820f1c (head), 0x00820f20 (size)
  // belong to the running game; the first thread is frozen and the 12 bytes are restored at the flush.
  function tileBuffer() {
    const E = env({ heap: { arenaSize: 0x400 } });
    __freeze.thread();
    __freeze.save(ptr(0x00820f18), 12);
    ptr(0x00820f18).writeByteArray([0x77, 0x77, 0x77, 0x77, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88]);
    return E.fx;
  }

  // ---- View_unlinkFromParent: views {parent +0x130}; parents {head +0x138, cursor +0x13c, count +0x140,
  // last +0x144, keep +0x148}; nodes {view +4, data +8, next +0xc, prev +0x10}, circular.
  function views() {
    const E = env({ heap: {} });
    const fx = E.fx;
    function view(i) { const v = E.take(0x140, 0x56560000 + i); v.add(0x130).writePointer(ptr(0)); return v; }
    function parent(i, keepFlag) {
      const p = E.take(0x150, 0x50410000 + i);
      p.add(0x138).writePointer(ptr(0));
      p.add(0x148).writeU32(keepFlag || 0);
      return p;
    }
    // ring(parent, members, cursorIndex, count): members are views or null (a node for someone else)
    function ring(p, members, ci, count, dataMask) {
      const nodes = members.map(function (m, k) {
        const n = E.take(0x20, 0x4e560000 + k);
        n.add(4).writePointer(m || ptr(0x13579bdf));
        n.add(8).writePointer(((dataMask >> k) & 1) ? E.take(0x10, 0x44410000 + k) : ptr(0));
        return n;
      });
      nodes.forEach(function (n, k) {
        n.add(0xc).writePointer(nodes[(k + 1) % nodes.length]);
        n.add(0x10).writePointer(nodes[(k + nodes.length - 1) % nodes.length]);
      });
      p.add(0x138).writePointer(nodes[0]);
      p.add(0x13c).writePointer(nodes[ci]);
      p.add(0x140).writeS32(count);
      p.add(0x144).writeS32(0x777);
      members.forEach(function (m) { if (m) m.add(0x130).writePointer(p); });
      return nodes;
    }
    const V = [];
    for (let i = 0; i < 16; i++) V.push(view(i));
    // v0: no parent. v1: parent without a head.
    const p1 = parent(1); V[1].add(0x130).writePointer(p1);
    // ring A (3 nodes: v2, v3, v4), cursor at the head, frees on; data on nodes 0 and 2
    ring(parent(2), [V[2], V[3], V[4]], 0, 3, 0b101);
    // ring B (3 nodes: v5, other, v6), cursor at node 2 (v6's): v5 is found after wrapping; keep flag set
    ring(parent(3, 1), [V[5], null, V[6]], 2, 3, 0b111);
    // ring C (1 node: v7): the count reaches 0, so the head is cleared
    ring(parent(4), [V[7]], 0, 1, 0b1);
    // ring D (2 nodes: others), v8 points at it but is not in it: not found after count steps
    ring(parent(5), [null, null], 0, 2, 0);
    V[8].add(0x130).writePointer(V[2].add(0x130).readPointer()); // v8 -> ring A's parent, not a member
    // ring E: count 0 with a head (head cleared, last = -1), member v9
    ring(parent(6), [V[9], null], 0, 0, 0b11);
    // ring F: count -1 (the walk is skipped, nothing cleared, last = -2), member v10
    ring(parent(7), [V[10], null], 0, -1, 0);
    // ring G: count 5 over 3 nodes, v11 absent (the walk wraps); ring H: v12 is the head but the cursor is past it
    const pg = parent(9); ring(pg, [null, null, null], 1, 5, 0); V[11].add(0x130).writePointer(pg);
    ring(parent(10), [V[12], null, V[13], null], 3, 4, 0b1111);
    // v14: ring of 2 with a NULL data on the found node and the found node not the head
    ring(parent(11), [null, V[14]], 0, 2, 0b01);
    V.forEach(function (v, i) { fx['v' + i] = v; });
    return fx;
  }

  // ---- Label::setText: labels with a vtable whose slots 0x134 (1 arg) and 0x13c (2 args) record, the old text
  // +0x14c, the visibility bit +0xa0 bit 0 and the parent +0x130 (Window::visible 0x004801f0 walks it).
  function labels(heap) {
    const E = env({ heap: heap });
    strings(E);
    const lvt = E.vt([[0x134, 0x1AB30000, 1], [0x13c, 0x1AB3C000, 2]]);
    const shown = E.take(0x140, 0x50500000); shown.add(0xa0).writeU32(0x31); shown.add(0x130).writePointer(ptr(0));
    const hidden = E.take(0x140, 0x50510000); hidden.add(0xa0).writeU32(0x30); hidden.add(0x130).writePointer(ptr(0));
    for (let i = 0; i < 12; i++) {
      const l = E.take(0x160, 0x4c420000 + i);
      l.writePointer(lvt);
      l.add(0x14c).writePointer(i % 2 ? E.take(0x20, 0x4f4f0000 + i) : ptr(0));
      l.add(0xa0).writeU32((i % 4 === 3) ? 0x40 : 0x41);
      l.add(0x130).writePointer([ptr(0), shown, hidden][i % 3]);
      E.fx['l' + i] = l;
    }
    return E.fx;
  }

  // ---- MsgBox labels: the globals 0x0083b994 (A) and 0x0083b998 (B) belong to the game; frozen thread, saved.
  // mode 'old' gives both an old block, 'null' none.
  function msgbox(mode, heap) {
    const E = env({ heap: heap });
    strings(E);
    const oldA = E.take(0x20, 0x4f410000), oldB = E.take(0x20, 0x4f420000);
    __freeze.thread();
    __freeze.save(ptr(0x0083b994), 0x38);
    ptr(0x0083b994).writePointer(mode === 'old' ? oldA : ptr(0));
    ptr(0x0083b998).writePointer(mode === 'old' ? oldB : ptr(0));
    return E.fx;
  }
  // MsgBox::initDefaults also reads 0x0083ad44 and writes 0x0083b99c, 0x0083b9a0, 0x0083b9b4, 0x0083b9c4.
  function msginit(mode, src, heap) {
    const fx = msgbox(mode, heap);
    __freeze.save(ptr(0x0083ad44), 4);
    ptr(0x0083ad44).writeU32(src);
    for (const g of [0x0083b99c, 0x0083b9a0, 0x0083b9b4, 0x0083b9c4]) ptr(g).writeU32(0x6b6b0000 + (g & 0xff));
    return fx;
  }

  // ---- HotList::freeEntryTip: entries {tip +0x1c}; the guard global 0x0083d348 = g (frozen, saved).
  function tips(g) {
    const E = env({ heap: {} });
    for (let i = 0; i < 12; i++) {
      const e = E.take(0x20, 0x54490000 + i);
      e.add(0x1c).writePointer(i % 3 ? E.take(0x10, 0x54500000 + i) : ptr(0));
      E.fx['h' + i] = e;
    }
    __freeze.thread();
    __freeze.setU32(ptr(0x0083d348), g);
    return E.fx;
  }

  // ---- freeFileWinBuf / freeResampleTable: the global block pointer = a fixture block or NULL (frozen, saved).
  function globalBlock(va, mode) {
    const E = env({ heap: {} });
    const b = E.take(0x20, 0x47420000);
    __freeze.thread();
    __freeze.save(ptr(va), 4);
    ptr(va).writePointer(mode === 'null' ? ptr(0) : (mode === 'odd' ? b.add(8) : b));
    return E.fx;
  }

  // ======================================================================== Part 2: Surface and caches
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
  // The display at 0x0083ad50 (frozen, saved): mode 'disp' a fixture display whose slots +0x90 and +0x94 record,
  // 'nodisp' NULL.
  function makeDisplay(E) {
    const d = E.take(0x20, 0);
    d.writePointer(E.vt([[0x90, 0xD1590000, 1], [0x94, 0xD1594000, 1]]));
    return d;
  }
  function swapDisplay(d, mode) {           // first thread frozen
    __freeze.save(ptr(0x0083ad50), 4);
    ptr(0x0083ad50).writePointer(mode === 'disp' ? __recDisplay.guard(d) : ptr(0));
  }
  // Surfaces {vtable +0, handle +4, name block +8, cache +0xc, +0x10 .. +0x24}: 12 combinations.
  function surface(E, i) {
    const s = E.take(0x30, 0x53550000 + i);
    s.add(4).writeU32((i & 1) ? 0x4800 + i : 0);
    s.add(8).writePointer((i & 2) ? E.take(0x20, 0x4e4d0000 + i) : ptr(0));
    s.add(0xc).writePointer((i & 4) ? cache(E, 0x40 + i, (i & 8) ? 0x4900 + i : 0, 0x3ff ^ (i * 0x55)) : ptr(0));
    return s;
  }
  function surfaces(mode, heap) {
    const E = env({ heap: heap });
    strings(E);
    for (let i = 0; i < 16; i++) E.fx['S' + i] = surface(E, i);
    for (let i = 0; i < 12; i++) E.fx['C' + i] = cache(E, i, (i % 3) ? 0x4a00 + i : 0, [0x3ff, 0, 0x155, 0x2aa, 0x01f, 0x3e0, 0x0f0, 0x30c, 0x1, 0x200, 0x2c3, 0x3c0][i]);
    const d = makeDisplay(E);
    __freeze.thread();
    swapDisplay(d, mode);
    return E.fx;
  }
  // freeFonts: the two global Surfaces 0x008408c8 and 0x008408f8 (0x60 bytes, saved) get fixture contents.
  function fonts(mode, variant) {
    const E = env({ heap: {} });
    const a = surface(E, variant), b = surface(E, variant ^ 0xf);
    const d = makeDisplay(E);
    const ba = a.readByteArray(0x30), bb = b.readByteArray(0x30);
    __freeze.thread();
    swapDisplay(d, mode);
    __freeze.save(ptr(0x008408c8), 0x60);
    ptr(0x008408c8).writeByteArray(ba);
    ptr(0x008408f8).writeByteArray(bb);
    return E.fx;
  }

  // ---- createCache: `this` objects {+0x1c}, holders {+4}; operator new from the stand-in (failMask for NULL).
  function creators(heap) {
    const E = env({ heap: heap });
    for (let i = 0; i < 12; i++) {
      E.fx['k' + i] = E.take(0x40, 0x4b430000 + i);
      E.fx['o' + i] = E.take(0x10, 0x4f480000 + i);
    }
    return E.fx;
  }

  // ---- Cache482f60::ctor / scalarDtorAdj20: the global 0x00839650 (frozen, saved) = g.
  function c482(g) {
    const E = env({ heap: {} });
    const offs = [0x1c, 0x30, 0x40, 0x8];
    for (let i = 0; i < 12; i++) {
      const o = E.take(0x60, 0x43360000 + i);
      const vbt = E.take(0x10, 0);
      vbt.add(4).writeS32(offs[i % 4]);
      o.add(4).writePointer(vbt);
      E.fx['q' + i] = o;
      // a full object for the deleting destructor: the real vbtable 0x004babb4 (entry +4 = 0x1c, so the base is at
      // +0x20), an empty node list (+8 = 0), and the dword at base+4 (full+0x24) 0 on even objects
      const f = E.take(0x60, 0x46440000 + i);
      f.add(4).writeU32(0x004babb4);
      f.add(8).writeU32(0);
      f.add(0x24).writeU32(i % 2 ? 0x24240000 + i : 0);
      E.fx['d' + i] = f.add(0x20);
    }
    __freeze.thread();
    __freeze.setU32(ptr(0x00839650), g);
    return E.fx;
  }

  // ---- allocPairArray objects; initPairArrayGlobal's global object 0x008406e8 (0x20 bytes, frozen, saved) gets
  // a prefill per variant (the bytes +5..+7 and +0x1c the body never writes keep it).
  function pairs() {
    const E = env({ heap: false });
    for (let i = 0; i < 12; i++) E.fx['p' + i] = E.take(0x20, 0x50410000 + i * 0x0101);
    return E.fx;
  }
  function pairGlobal(v) {
    const E = env({ heap: false });
    __freeze.thread();
    __freeze.fill(ptr(0x008406e8), 0x20, [0x00, 0x5a, 0xa5][v]);
    return E.fx;
  }

  // ======================================================================== Part 3: node lists
  // A node list {vtable +0, vbtable +4, current +8, next +0xc, count +0x10, +0x14}; nodes {vbtable +0, data +8,
  // next +0xc} and data objects are deleted through their virtual bases (recording stubs, tags 0xDA.. / 0xDE..).
  // shapes: [count, nodes, data mask]
  const NL = [[0, 0, 0], [0, 2, 3], [1, 1, 1], [2, 2, 1], [3, 3, 7], [4, 4, 5], [2, 4, 0], [-1, 1, 1], [3, 3, 2],
              [1, 2, 0], [4, 4, 15], [2, 2, 2]];
  function nodeList(E, at, i, listVt, allData) {
    const [count, n, m0] = NL[i];
    const mask = allData ? 0xf : m0;
    at.writePointer(listVt);
    const nodes = [];
    for (let k = 0; k < n; k++) {
      const nd = vbObj(E, 0x20, 0x10, 0xDE000000 + i * 0x100 + k);
      nd.add(8).writePointer(((mask >> k) & 1) ? vbObj(E, 0x20, 0x18, 0xDA000000 + i * 0x100 + k) : ptr(0));
      nodes.push(nd);
    }
    nodes.forEach(function (nd, k) { nd.add(0xc).writePointer(k + 1 < n ? nodes[k + 1] : ptr(0)); });
    at.add(8).writePointer(n ? nodes[0] : ptr(0));
    at.add(0xc).writeU32(0x0c0c0000 + i);
    at.add(0x10).writeS32(count);
    at.add(0x14).writeU32(0x14140000 + i);
  }
  function lists(opts) {
    opts = opts || {};
    const E = env({ heap: {} });
    const lvt = E.vt([[4, 0x5107A000, 1]]);
    for (let i = 0; i < 12; i++) {
      // List402_clear: the list itself
      const l = E.take(0x20, 0x4c490000 + i);
      nodeList(E, l, i, lvt);
      E.fx['l' + i] = l;
      // ListModel: Buf at +4 (buffer at +0xc on odd models), node list at +0xc0, vbtable at +0xc4 with entry +4 =
      // 0x24 (the virtual base at +0xe8), +0xec a saved dword
      const m = E.take(0x100, 0x4d4d0000 + i);
      m.writeU32(0x004bb21c);
      m.add(4).writeU32(0x004ba84c);
      m.add(0xc).writePointer(i % 2 ? E.take(0x20, 0x4d420000 + i) : ptr(0));
      // the real vtable's slot 1 is bufferFree 0x004a05d0, which reads record+4 with no null test: every node of
      // that variant carries a data object
      nodeList(E, m.add(0xc0), i, opts.realVt ? ptr(0x004bb22c) : lvt, opts.realVt);
      const vbt = E.take(0x10, 0);
      vbt.add(4).writeS32(0x24);
      m.add(0xc4).writePointer(vbt);
      m.add(0xec).writeU32(0xecec0000 + i);
      E.fx['m' + i] = m;
      // panelSubDtor: `this` is list + 0x28; the list's vbtable +4 has entry +4 = 0x40 (the base at list+0x44)
      const p = E.take(0x60, 0x50530000 + i);
      nodeList(E, p, i, lvt);
      const pvb = E.take(0x10, 0);
      pvb.add(4).writeS32(0x40);
      p.add(4).writePointer(pvb);
      E.fx['ps' + i] = p.add(0x28);
    }
    if (opts.frozen) {
      __freeze.thread();
      __freeze.setU32(ptr(0x00839650), opts.frozen);
    }
    return E.fx;
  }

  // ======================================================================== Part 4: libjpeg
  // error_exit records (msg_code +0x14, msg_parm.i[0] +0x18) into err+0x30 (hash) / +0x34 (count) and RETURNS.
  const ERR_REC = keep(new NativeCallback(function (cinfo) {
    const e = cinfo.readPointer();
    let h = e.add(0x30).readU32();
    h = (Math.imul(h, 33) + e.add(0x14).readU32()) >>> 0;
    h = (Math.imul(h, 33) + e.add(0x18).readU32()) >>> 0;
    e.add(0x30).writeU32(h);
    e.add(0x34).writeU32((e.add(0x34).readU32() + 1) >>> 0);
  }, 'void', ['pointer']));
  // A jpeg_compress_struct (0x180 bytes) with its error manager; mem filled by the caller.
  function cinfoOf(E, i) {
    const c = E.take(0x180, 0x63690000 + i);
    const err = E.take(0x40, 0x65720000 + i);
    err.writePointer(ERR_REC);
    err.add(0x30).writeU32(0);
    err.add(0x34).writeU32(0);
    c.writePointer(err);
    return c;
  }
  // The real memory manager layout (jmemmgr.c my_memory_mgr, 0x50 bytes): pools from the stand-in arena.
  // spec: { small: [[used, left], ...] chain for pool 0 / 1, large: [...], total }
  function memOf(E, i, spec) {
    const m = E.take(0x50, 0x6d6d0000 + i);
    for (let s = 0; s < 0x30; s += 4) m.add(s).writeU32(0x6d000000 + s);
    // max_memory_to_use (+0x2c) is 0: a pool id of -1 reads it as small_list[-1] (0x004aefd3), so the negative-id
    // vectors find an empty chain there instead of a pattern pointer
    m.add(0x2c).writeU32(0);
    function chain(list, base) {
      let first = ptr(0), prev = null;
      (list || []).forEach(function ([used, left], k) {
        const h = E.take(0x10 + used + left, 0x70700000 + base + k);
        h.writePointer(ptr(0)); h.add(4).writeU32(used); h.add(8).writeU32(left);
        if (prev) prev.writePointer(h); else first = h;
        prev = h;
      });
      return first;
    }
    m.add(0x30).writePointer(chain(spec.small0, 0x00));
    m.add(0x34).writePointer(chain(spec.small1, 0x10));
    m.add(0x38).writePointer(chain(spec.large0, 0x20));
    m.add(0x3c).writePointer(chain(spec.large1, 0x30));
    m.add(0x40).writePointer(ptr(0));
    m.add(0x44).writePointer(ptr(0));
    m.add(0x48).writeU32(spec.total || 0x1000);
    m.add(0x4c).writeU32(0x4c4c4c4c);
    return m;
  }
  // close_backing_store(cinfo, info): cdecl recorder.
  function virtList(E, n, openMask, tag) {
    let first = ptr(0), prev = null;
    for (let k = 0; k < n; k++) {
      const v = E.take(0x40, 0x76610000 + k);
      v.add(0x28).writeU32(((openMask >> k) & 1) ? 1 : 0);
      v.add(0x2c).writePointer(ptr(0));
      v.add(0x38).writePointer(E.stub(tag + k, 2, true));
      if (prev) prev.add(0x2c).writePointer(v); else first = v;
      prev = v;
    }
    return first;
  }
  const MEMS = [
    {}, { small0: [[0x40, 0x100]] }, { small0: [[0x40, 0x10], [0x80, 0x400]] }, { small1: [[0, 0x20]] },
    { small0: [[0x10, 0x8]], small1: [[0x20, 0x8], [0x30, 0x8]] }, { large0: [[0x100, 0]] },
    { large1: [[0x40, 0], [0x80, 0]], small1: [[0x40, 0x40]] }, { small0: [[0, 0x600]] },
  ];
  function jmem(heap) {
    const E = env({ heap: heap || { arenaSize: 0x30000 } });
    for (let i = 0; i < 12; i++) {
      const c = cinfoOf(E, i);
      const m = memOf(E, i, MEMS[i % MEMS.length]);
      c.add(4).writePointer(m);
      if (i >= 8) {
        m.add(0x40).writePointer(virtList(E, 2 + (i & 1), 0b101 ^ i, 0xC1050000 + i * 0x10));
        m.add(0x44).writePointer(virtList(E, 2, 0b11 ^ (i & 1), 0xC1B00000 + i * 0x10));
      }
      E.fx['j' + i] = c;
    }
    return E.fx;
  }
  // jinit_* / std_huff_tables: cinfo->mem is a fixture manager whose alloc_small (+0), alloc_large (+4) and
  // request_virt_barray (+0x14) are JS callbacks: allocations come from the block at mem+0x50 (cursor at its +0,
  // records (pool, size) into its +4 hash), blocks pre-filled 0xcc; request_virt_barray records its six arguments
  // into the same hash and returns a value derived from them.
  const J_ALLOC = keep(new NativeCallback(function (cinfo, pool, size) {
    const p = cinfo.add(4).readPointer().add(0x50).readPointer();
    const cur = p.readU32();
    p.writeU32((cur + ((size + 15) & ~15)) >>> 0);
    p.add(4).writeU32((Math.imul(p.add(4).readU32(), 33) + pool * 0x10000 + size) >>> 0);
    return p.add(0x10 + cur);
  }, 'pointer', ['pointer', 'int', 'uint32']));
  const J_VIRT = keep(new NativeCallback(function (cinfo, pool, zero, cols, rows, maxacc) {
    const p = cinfo.add(4).readPointer().add(0x50).readPointer();
    let h = p.add(4).readU32();
    for (const v of [0xbaaa, pool, zero, cols, rows, maxacc]) h = (Math.imul(h, 33) + (v >>> 0)) >>> 0;
    p.add(4).writeU32(h);
    return ptr(((cols * 0x10000 + rows * 0x100 + maxacc) >>> 0) || 1);
  }, 'pointer', ['pointer', 'int', 'int', 'uint32', 'uint32', 'int']));
  // components: [h, v, width_in_blocks, height_in_blocks]
  const COMPS = [[1, 1, 10, 7], [2, 2, 13, 9], [2, 1, 1, 1], [1, 2, 0, 5], [4, 4, 31, 17], [3, 1, 6, 4]];
  function jinit() {
    const E = env({ heap: false });
    // huffman tables for std_huff_tables' "already allocated" side
    for (let i = 0; i < 12; i++) {
      const c = cinfoOf(E, i);
      const m = E.take(0x60, 0x6a6d0000 + i);
      m.writePointer(J_ALLOC);
      m.add(4).writePointer(J_ALLOC);
      m.add(0x14).writePointer(J_VIRT);
      const pool = E.take(0x10 + 0x900, 0xcccccccc);
      pool.writeU32(0); pool.add(4).writeU32(0);
      m.add(0x50).writePointer(pool);
      c.add(4).writePointer(m);
      const nc = [0, 1, 2, 3, 4, 1, 3, 2, -1, 4, 1, 3][i];
      const comp = E.take(0x54 * 4, 0x99999999);
      for (let k = 0; k < 4; k++) {
        const [h, v, w, hh] = COMPS[(i + k) % COMPS.length];
        comp.add(k * 0x54 + 8).writeS32(h); comp.add(k * 0x54 + 0xc).writeS32(v);
        comp.add(k * 0x54 + 0x1c).writeS32(w); comp.add(k * 0x54 + 0x20).writeS32(hh);
      }
      c.add(0x34).writeS32(nc);
      c.add(0x3c).writePointer(comp);
      // std_huff_tables: table pointers NULL (allocated) or an existing 0x118-byte table (overwritten), per bit
      for (let t = 0; t < 4; t++) {
        const off = [0x50, 0x60, 0x54, 0x64][t];
        c.add(off).writePointer(((i >> t) & 1) ? E.take(0x118, 0x48540000 + t) : ptr(0));
      }
      E.fx['c' + i] = c;
    }
    return E.fx;
  }
  // finish_pass_phuff: an encoder {gather +0xc, next_output_byte +0x10, free_in_buffer +0x14, put_buffer +0x18,
  // put_bits +0x1c, cinfo +0x20, ac_tbl_no +0x34, EOBRUN +0x38, BE +0x3c, bit_buffer +0x40, derived_tbls +0x4c}, a
  // destination {next_output_byte +0, free_in_buffer +4, empty_output_buffer +0xc} whose empty_output_buffer
  // records and points the destination at a spare buffer, and a derived table set.
  const EMPTY = keep(new NativeCallback(function (cinfo) {
    const d = cinfo.add(0x14).readPointer();
    d.add(0x10).writeU32((d.add(0x10).readU32() + 1) >>> 0);
    d.writePointer(d.add(0x18).readPointer());
    d.add(4).writeU32(d.add(0x14).readU32());
    return 1;
  }, 'int', ['pointer']));
  const PH = [ // [EOBRUN, BE, put_bits, put_buffer, gather, free, ac_tbl]
    [0, 0, 0, 0, 0, 0x80, 0], [1, 0, 3, 0x00e00000, 0, 0x80, 0], [5, 2, 0, 0, 0, 0x80, 1], [0x7fff, 7, 5, 0x00f80000, 0, 0x80, 2],
    [0, 0, 7, 0x00fe0000, 0, 0x80, 3], [3, 4, 1, 0x00800000, 1, 0x80, 0], [2, 0, 6, 0x00fc0000, 0, 2, 1], [9, 9, 2, 0x00c00000, 0, 1, 2],
    [0x100, 1, 4, 0x00f00000, 0, 0x80, 3], [0, 3, 0, 0, 0, 0x80, 0], [0x40, 30, 3, 0x00a00000, 0, 3, 1], [1, 1, 0, 0, 1, 0x80, 2],
    [0x8000, 2, 1, 0x00800000, 0, 0x80, 1], [0x12345, 0, 0, 0, 1, 0x80, 3],
  ];
  function phuff() {
    const E = env({ heap: false });
    const tabs = [];
    for (let t = 0; t < 4; t++) {
      const d = E.take(0x500, 0);
      for (let s = 0; s < 256; s++) {
        d.add(s * 4).writeU32((s % 5 === 0) ? 0xffff : ((s * 0x9e37 + t * 0x101) & 0xffff));
        d.add(0x400 + s).writeU8(1 + ((s * 7 + t) % 16));
      }
      tabs.push(d);
    }
    const counts = [];
    for (let t = 0; t < 4; t++) {
      const k = E.take(0x404, 0);
      for (let s = 0; s < 0x101; s++) k.add(s * 4).writeU32(t * 0x1000 + s);
      counts.push(k);
    }
    PH.forEach(function ([eob, be, bits, pb, gather, free, act], i) {
      const c = cinfoOf(E, i);
      const d = E.take(0x20, 0);
      const buf = E.take(0x100, 0x5b5b5b5b), spare = E.take(0x100, 0x6c6c6c6c);
      d.writePointer(buf); d.add(4).writeU32(free); d.add(0xc).writePointer(EMPTY);
      d.add(0x14).writeU32(0x80); d.add(0x18).writePointer(spare);
      c.add(0x14).writePointer(d);
      const e = E.take(0x80, 0x55555555);
      e.add(0xc).writeU32(gather);
      e.add(0x10).writeU32(0x10101010); e.add(0x14).writeU32(0x14141414);   // overwritten from the destination
      e.add(0x18).writeU32(pb); e.add(0x1c).writeS32(bits);
      e.add(0x20).writePointer(c);
      e.add(0x34).writeS32(act);
      e.add(0x38).writeU32(eob); e.add(0x3c).writeU32(be);
      const bb = E.take(0x40, 0);
      for (let k = 0; k < 0x40; k++) bb.add(k).writeU8(((k * 7 + i) >> 2) & 1);
      e.add(0x40).writePointer(bb);
      for (let t = 0; t < 4; t++) {
        e.add(0x4c + 4 * t).writePointer(tabs[(t + i) % 4]);
        e.add(0x5c + 4 * t).writePointer(counts[(t + i) % 4]);
      }
      c.add(0x164).writePointer(e);
      E.fx['c' + i] = c;
    });
    return E.fx;
  }

  // ======================================================================== Part 5: the timer lock counter
  function lock(v) {
    const E = env({ heap: false });
    __freeze.thread();
    __freeze.setU32(ptr(0x0083afec), v >>> 0);
    return E.fx;
  }
  const LOCKS = [0, 1, 2, -1, 0x7fffffff, -0x80000000, 5, 100, -7, 0xffff, 0x10000, 3];

  const F = {
    c3cz_arrays: arrays,
    c3cz_bufs() { return bufs({ failSize: 0x333 }); },
    c3cz_texts() { return texts({}); },
    c3cz_texts_fail() { return texts({ failSize: FAIL_SIZE }); },
    c3cz_push() { return pushLists({}); },
    c3cz_push_fail0() { return pushLists({ failMask: 1 }); },
    c3cz_push_fail1() { return pushLists({ failMask: 2 }); },
    c3cz_erase: eraseList,
    c3cz_tilebuf: tileBuffer,
    c3cz_views: views,
    c3cz_labels() { return labels({ failSize: FAIL_SIZE }); },
    c3cz_msgbox_old() { return msgbox('old', { failSize: FAIL_SIZE }); },
    c3cz_msgbox_null() { return msgbox('null', { failSize: FAIL_SIZE }); },
    c3cz_msginit_a() { return msginit('null', 0x11112222, {}); },
    c3cz_msginit_b() { return msginit('old', 0x33334444, {}); },
    c3cz_msginit_c() { return msginit('old', 0x55556666, { failMask: 1 }); },
    c3cz_msginit_d() { return msginit('null', 0x77778888, { failMask: 2 }); },
    c3cz_msginit_e() { return msginit('old', 0, { failMask: 3 }); },
    c3cz_tips_g0() { return tips(0); },
    c3cz_tips_g1() { return tips(1); },
    c3cz_fwb_block() { return globalBlock(0x00840820, 'block'); },
    c3cz_fwb_odd() { return globalBlock(0x00840820, 'odd'); },
    c3cz_fwb_null() { return globalBlock(0x00840820, 'null'); },
    c3cz_frt_block() { return globalBlock(0x0083ac10, 'block'); },
    c3cz_frt_odd() { return globalBlock(0x0083ac10, 'odd'); },
    c3cz_frt_null() { return globalBlock(0x0083ac10, 'null'); },
    c3cz_surf_disp() { return surfaces('disp', { failSize: FAIL_SIZE }); },
    c3cz_surf_nodisp() { return surfaces('nodisp', { failSize: FAIL_SIZE }); },
    c3cz_fonts_disp_a() { return fonts('disp', 0xd); },
    c3cz_fonts_disp_b() { return fonts('disp', 0x6); },
    c3cz_fonts_disp_c() { return fonts('disp', 0x0); },
    c3cz_fonts_nodisp() { return fonts('nodisp', 0xf); },
    c3cz_create() { return creators({}); },
    c3cz_create_fail() { return creators({ failMask: 1 }); },
    c3cz_c482_g0() { return c482(0); },
    c3cz_c482_g1() { return c482(0x39650000); },
    c3cz_pairs: pairs,
    c3cz_pairg_0() { return pairGlobal(0); },
    c3cz_pairg_1() { return pairGlobal(1); },
    c3cz_pairg_2() { return pairGlobal(2); },
    c3cz_lists() { return lists({}); },
    c3cz_lists_realvt() { return lists({ realVt: true }); },
    c3cz_lists_frozen() { return lists({ frozen: 0x96500000 }); },
    c3cz_jmem() { return jmem(); },
    c3cz_jmem_fail() { return jmem({ arenaSize: 0x30000, failMask: 0x7 }); },
    c3cz_jinit: jinit,
    c3cz_phuff: phuff,
  };
  LOCKS.forEach(function (v, i) { F['c3cz_lock_' + i] = function () { return lock(v); }; });
  Object.assign(globalThis.DIFF_FIXTURES, F);
})();
