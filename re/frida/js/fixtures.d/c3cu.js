// Fixtures for batch c3cu (round 23, golf_clean.exe composite-view layer from 0x004a0c90 up).
// Reimplementations: shim/src/re/c3cu.cpp. Registry: re/frida/registry.d/c3cu.py. Purposes: log/c3/c3cu_purpose.md.
//
// Every object is carved from ONE fixture arena (0x30000 bytes, pre-filled 0x5a5a5a5a). None of these objects is
// linked into the game's window list, draw list or focus chain. The state regions are the call recorder ($rec,
// 0x400 bytes) plus, for the dispatcher keys, every field a callee writes: each ListBox's span from its top row
// L-0x34 to the end of its model ($lbz_<v>, 0x6b0 bytes: scrollBy's top row, selectedId's cursor +0xcc and +0xd4)
// and each listPanel model ($lpz_<v>, 0x100 bytes). Hashing the whole arena per vector made a key take minutes.
// Virtual slots and callbacks land in RECORDING STUBS written as machine code into a separate rwx page (no
// JavaScript runs inside a vector, so the selectChanged key can freeze the game's first thread):
//     mov eax, [REC]; add eax, REC                  ; REC+0 = byte cursor (entries from REC+0x10)
//     mov dword [eax], tag
//     mov [eax+4], ecx            (thiscall: the object the slot was called on)
//       | mov dword [eax+4], 0xC0C0C0C0   (cdecl: ecx is not an input of the call and differs between the arms)
//     mov edx, [esp+4i]; mov [eax+4+4i], edx        for each stack argument i = 1..n
//     add dword [REC], 4*(n+2)
//     mov eax, [REC]; xor eax, tag                  ; a return value that differs per call
//     ret 4n (thiscall) | ret (cdecl)
//
// The view object (tag at D-8) and the five sub-objects its dispatchers select (c3cu.cpp header):
//   every object is a 0x1000-byte block with D = block + 0x800 and [D-8] = tag. [D-0x1f8] is a vbtable whose
//   entry +4 gives the Window base Container::cycleSelection 0x0047af60 runs on (viewDispatchCycleSelection, tags 1,
//   2, 8, 0x10); that base's +0x130 is null or a container whose count +0x140 is 0 or 1, so cycleSelection returns at
//   0x0047af6b / 0x0047af78 without its walk (its walk is c3cr's evidence) and without writing 0x0083ab98.
(function () {
  const G_CURWIDGET = 0x0083ab2c;
  const ARENA = 0x30000;

  function u32(v) { v >>>= 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; }

  function codeArena() {
    const size = Process.pageSize * 4;
    const base = Memory.alloc(size);
    Memory.protect(base, size, 'rwx');
    let off = 0;
    return function (bytes) {
      const p = base.add(off);
      p.writeByteArray(bytes);
      off += (bytes.length + 15) & ~15;
      if (off > size) throw new Error('c3cu: code arena full');
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
    else b.push(0xC7, 0x40, 0x04, ...u32(0xC0C0C0C0));              // mov dword [eax+4], 0xC0C0C0C0
    for (let i = 1; i <= n; i++) b.push(0x8B, 0x54, 0x24, 4 * i, 0x89, 0x50, 4 + 4 * i);
    b.push(0x81, 0x05, ...r, ...u32(4 * (n + 2)));                   // add dword [REC], 4(n+2)
    b.push(0xA1, ...r);                                              // mov eax, [REC]
    b.push(0x35, ...u32(tag));                                       // xor eax, tag
    if (thiscall && n > 0) b.push(0xC2, 4 * n, 0x00); else b.push(0xC3);
    return b;
  }

  function build() {
    const fx = {};
    const emit = codeArena();
    const arena = Memory.alloc(ARENA);
    for (let i = 0; i < ARENA; i += 4) arena.add(i).writeU32(0x5a5a5a5a);
    let cur = 0;
    function take(n, fillv) {
      const p = arena.add(cur);
      cur += (n + 15) & ~15;
      if (cur > ARENA) throw new Error('c3cu: arena full');
      if (fillv !== undefined) for (let i = 0; i < n; i += 4) p.add(i).writeU32(fillv >>> 0);
      return p;
    }
    const rec = take(0x400, 0);
    rec.writeU32(0x10);
    fx.arena = arena; fx.rec = rec; fx.obj = arena;

    const nul = emit([0x33, 0xC0, 0xC3]);                            // xor eax, eax; ret
    // A 0x80-slot vtable: listed slots record, the rest return 0. spec: [[slot offset, tag, n stack args], ...]
    function vtable(spec) {
      const vt = take(0x200);
      for (let i = 0; i < 0x80; i++) vt.add(i * 4).writePointer(nul);
      for (const [slot, tag, n] of spec) vt.add(slot).writePointer(emit(stubBytes(rec, tag, n, true)));
      return vt;
    }
    function cdecl1(tag) { return emit(stubBytes(rec, tag, 1, false)); }
    function vbt(d1, d2) { const t = take(0x10, 0); t.add(4).writeS32(d1); t.add(8).writeS32(d2 || 0); return t; }
    function block(tag) {
      const blk = take(0x1000, 0x5a5a5a5a);
      const D = blk.add(0x800);
      D.sub(8).writeS32(tag);
      return D;
    }
    // Containers for the cycleSelection fallbacks: count 0 or 1 (no walk).
    function container(count) {
      const c = take(0x200, 0x4b4b4b4b);
      c.add(0x140).writeS32(count);
      return c;
    }
    // The generic vbtable at D-0x1f8: entry +4 = 0x600, so the cycleSelection base is D+0x408 and its parent
    // pointer sits at D+0x538.
    function genericCycle(D, parent) {
      D.sub(0x1f8).writePointer(vbt(0x600, 0));
      D.add(0x538).writePointer(parent);
    }

    // ---- panelSub (tag 8, P = D-0x8c) ------------------------------------------------------------------------
    // HotList at P-0xd0: entries pointer +0x50, count +0x58; entries of 0x20 bytes with rect {l,t,r,b} at +4, the
    // "b" dword at +0x14 and the "a" dword at +0x18 (HotList::hitTest 0x00492a90, c3d). Entries 2 [50,50,90,90] and
    // 4 [60,60,100,100] overlap (the scan runs high to low, so entry 4 wins). The seven callbacks P-0x20 .. P-0x8 are
    // cdecl recorders (tag 0xCB0000s0 + slot) or 0. [P+0x3c] = 0x40, so the vtordisp thunk input is T = P+0x40.
    // [P-0xe0] is a vbtable with entry +4 = 0x300: the base P+0x220 has a vtable whose slot +0x120 records (the
    // body 0x004a4870 of panelSubSlot60_vtordisp and of viewDispatchSlot60's tag 8).
    const RECTS = [[0, 0, 10, 10], [20, 20, 40, 40], [50, 50, 90, 90], [100, 0, 120, 20], [60, 60, 100, 100],
                   [200, 200, 210, 210]];
    const vt120 = vtable([[0x120, 0xA1200000, 0]]);
    //               count  callbacks set (bit k = slot k)
    const PS = [[6, 0x7f], [6, 0x00], [0, 0x7f], [6, 0x55], [3, 0x2a]];
    PS.forEach(function (r, v) {
      const [count, mask] = r;
      const D = block(8);
      const P = D.sub(0x8c);
      const ent = take(RECTS.length * 0x20, 0);
      RECTS.forEach(function (rc, i) {
        const e = ent.add(i * 0x20);
        rc.forEach(function (x, j) { e.add(4 + 4 * j).writeS32(x); });
        e.add(0x14).writeS32(0x2000 + 0x10 * v + i);
        e.add(0x18).writeS32(0x1000 + 0x10 * v + i);
      });
      const H = P.sub(0xd0);
      H.add(0x50).writePointer(ent);
      H.add(0x58).writeS32(count);
      for (let k = 0; k < 7; k++) {
        P.sub(0x20 - 4 * k).writePointer((mask >> k) & 1 ? cdecl1(0xCB000000 + 0x100 * v + k) : ptr(0));
      }
      P.add(0x3c).writeS32(0x40);
      P.sub(0xe0).writePointer(vbt(0x300, 0));
      P.add(0x220).writePointer(vt120);
      genericCycle(D, v % 2 ? container(1) : ptr(0));
      fx['d8_' + v] = D; fx['p8_' + v] = P; fx['t8_' + v] = P.add(0x40);
    });

    // ---- panelRow (tag 4, R = D-4) ---------------------------------------------------------------------------
    // Ten child pointers R-0x88 .. R-0x64 (objects on a vtable whose slot +0xf0, one argument, records; or 0).
    // [R-0x8c] is a vbtable with entry +4 = 0x100, so X = R+0x100: X+0xb4, X+0xac, X+0xb0 -> Y, Y+4 -> Z (an object
    // whose slot +0x128 records, or 0) and the fallback base X-0x8c with its parent at X+0xa4.
    // [R+0x3c] = 0x40: the vtordisp thunk input is T = R+0x40.
    const vtF0 = vtable([[0xf0, 0xF0F00000, 1]]);
    const vt128 = vtable([[0x128, 0x91280000, 0]]);
    //               children mask  b4  ac  Z    fallback parent (-1 none, else count)
    const PR = [[0x3ff, 1, 1, 1, -1],
                [0x025, 0, 1, 1, 1],
                [0x000, 1, 0, 1, -1],
                [0x3fe, 1, 1, 0, 0],
                [0x200, 1, 1, 1, 1],
                [0x0f0, 7, 9, 0, 1]];
    PR.forEach(function (r, v) {
      const [mask, b4, ac, hasZ, par] = r;
      const D = block(4);
      const R = D.sub(4);
      for (let i = 0; i < 10; i++) {
        let c = ptr(0);
        if ((mask >> i) & 1) { c = take(0x40, 0x2d2d2d2d); c.writePointer(vtF0); }
        R.sub(0x88 - 4 * i).writePointer(c);
      }
      R.sub(0x8c).writePointer(vbt(0x100, 0));
      const X = R.add(0x100);
      X.add(0xb4).writeS32(b4);
      X.add(0xac).writeS32(ac);
      const Y = take(0x10, 0);
      let Z = ptr(0);
      if (hasZ) { Z = take(0x40, 0x3e3e3e3e); Z.writePointer(vt128); }
      Y.add(4).writePointer(Z);
      X.add(0xb0).writePointer(Y);
      X.add(0xa4).writePointer(par < 0 ? ptr(0) : container(par));
      R.add(0x3c).writeS32(0x40);
      genericCycle(D, ptr(0));   // not read for tag 4 (the tag-4 case uses R's own vbtable)
      fx['d4_' + v] = D; fx['r4_' + v] = R; fx['t4_' + v] = R.add(0x40);
    });

    // A four-node circular list for ListModel::selectedId 0x00489950 (value 0x100*v + k at +4, next +0xc, prev +0x10).
    function ring(v) {
      const nodes = [];
      for (let k = 0; k < 4; k++) { const nd = take(0x20, 0x1e1e1e1e); nd.add(4).writeU32(0x100 * v + k); nodes.push(nd); }
      for (let k = 0; k < 4; k++) {
        nodes[k].add(0xc).writePointer(nodes[(k + 1) % 4]);
        nodes[k].add(0x10).writePointer(nodes[(k + 3) % 4]);
      }
      return nodes;
    }
    // model fields at M: head +0xc8, cursor +0xcc, count +0xd0, +0xd4, selected index +0xf0.
    function model(M, v, idx, rows, hasList) {
      const nodes = ring(v);
      M.add(0xc8).writePointer(hasList ? nodes[0] : ptr(0));
      M.add(0xcc).writePointer(nodes[1]);
      M.add(0xd0).writeS32(rows);
      M.add(0xd4).writeS32(0x77);
      M.add(0xf0).writeS32(idx);
    }

    // ---- ListBox (tag 2, L = D-0x1a0) ------------------------------------------------------------------------
    // c3cr's ListBox layout: the vbtable pointer at L-0x58 = D-0x1f8 (so it is also the cycleSelection vbtable),
    // entry +4 = 0x300 (Window base W = D+0x108) and entry +8 = 0x580 (list-model base M = D+0x388).
    //   W+0 vtable (slot +0x120 records), W+0x9c flag byte (bit 3 scrollBy, bit 6 fwdInnerVfn20 / viewDispatchSlot6),
    //   W+0x130 parent (vtable slots +0x20 0 args, +0x24 1, +0x74 2, +0x104 2, +0x108 1; +0x98 a child whose +0x1c
    //   records; +0x140 count 0 or 1 for cycleSelection) or 0, W+0x26c 0 (Window::fwdScroll26c 0x0047b9f0 then
    //   calls nothing), M+0x5c visible rows, the model fields above; L-0x34 top row; L-8 a cdecl callback or 0.
    const vtW = vtable([[0x120, 0xA2200000, 0]]);
    const vtP = vtable([[0x20, 0xB0200000, 0], [0x24, 0xB0240000, 1], [0x74, 0xB0740000, 2],
                        [0x104, 0xB1040000, 2], [0x108, 0xB1080000, 1]]);
    const vtC = vtable([[0x1c, 0xC01C0000, 0]]);
    //            flags parent child cb  top rows vis idx list pcount
    const LB = [[0x48, 1, 1, 1, 3, 10, 4, 2, 1, 1],
                [0x40, 1, 0, 0, 0, 5, 5, -1, 1, 0],
                [0x08, 0, 0, 1, 7, 10, 3, 1, 1, 0],
                [0x00, 1, 1, 1, 1, 2, 0, 0, 1, 1],
                [0x4c, 1, 1, 0, -2, 0, 0, 9, 0, 0],
                [0x08, 1, 0, 1, 0, 3, 1, 3, 1, 1],
                [0x48, 0, 0, 0, 5, 8, 2, 0, 1, 0]];
    LB.forEach(function (r, v) {
      const [flags, hasParent, hasChild, hasCb, top, rows, vis, idx, hasList, pcount] = r;
      const D = block(2);
      const L = D.sub(0x1a0);
      L.sub(0x58).writePointer(vbt(0x300, 0x580));
      const W = D.add(0x108);
      const M = D.add(0x388);
      W.writePointer(vtW);
      W.add(0x9c).writeU32(flags);
      W.add(0x26c).writePointer(ptr(0));
      let par = ptr(0);
      if (hasParent) {
        par = take(0x200, 0x3c3c3c3c);
        par.writePointer(vtP);
        let ch = ptr(0);
        if (hasChild) { ch = take(0x40, 0x2c2c2c2c); ch.writePointer(vtC); }
        par.add(0x98).writePointer(ch);
        par.add(0x140).writeS32(pcount);
      }
      W.add(0x130).writePointer(par);
      M.add(0x5c).writeS32(vis);
      model(M, 0x20 + v, idx, rows, hasList);
      L.sub(0x34).writeS32(top);
      L.sub(8).writePointer(hasCb ? cdecl1(0xD2000000 + 0x100 * v) : ptr(0));
      fx['d2_' + v] = D;
      fx['lbz_' + v] = D.sub(0x1d4);   // L-0x34 .. M+0xfc (D+0x484): 0x658 bytes, region size 0x6b0
    });

    // ---- listPanel (tag 0x10, Q = D-0x180) -------------------------------------------------------------------
    // vbtable at Q-0x24 (entry +4 = 0x300, entry +8 = 0x500): the base Q-0x24+0x300 = D+0x15c has a vtable whose
    // slot +0x120 records (0x0049e7b0, viewDispatchSlot60's tag 0x10); [Q+0x300+0x10c] = D+0x28c is the value
    // listSelectionChanged 0x0049e640 stores to 0x0083ab2c; the model Q-0x24+0x500 = D+0x35c feeds
    // ListModel::selectedId; the cdecl callback is at Q-0x10.
    const vtQ = vtable([[0x120, 0xA3200000, 0]]);
    //            cb  idx rows list
    const LP = [[1, 1, 4, 1], [0, 0, 4, 1], [1, 7, 2, 0], [1, -2, 4, 1]];
    LP.forEach(function (r, v) {
      const [hasCb, idx, rows, hasList] = r;
      const D = block(0x10);
      const Q = D.sub(0x180);
      Q.sub(0x24).writePointer(vbt(0x300, 0x500));
      D.add(0x15c).writePointer(vtQ);
      D.add(0x28c).writeU32(0x16160000 + v);
      model(D.add(0x35c), 0x30 + v, idx, rows, hasList);
      Q.sub(0x10).writePointer(hasCb ? cdecl1(0xD1000000 + 0x100 * v) : ptr(0));
      genericCycle(D, v % 2 ? container(1) : container(0));
      fx['d16_' + v] = D;
      fx['lpz_' + v] = D.add(0x35c);   // the model D+0x35c .. +0xfc
    });

    // ---- optionDialog (tag 1, O = D-0x168) -------------------------------------------------------------------
    // vbtable at O-0x1c (entry +4 = 0x300, entry +8 = 0x480): the base O-0x1c+0x300 = D+0x17c has a vtable whose
    // slot +0x120 records (0x0049fd80, viewDispatchSlot60's tag 1); [O+0x300+0x114] = D+0x2ac is the value 0x0049fc30
    // stores to 0x0083ab2c; [O+0x480+0xd4] = D+0x3ec is the argument of the cdecl callback at O-0x10.
    const vtO = vtable([[0x120, 0xA4200000, 0]]);
    [[1, 0x1234], [0, 0x5678], [1, -7]].forEach(function (r, v) {
      const [hasCb, arg] = r;
      const D = block(1);
      const O = D.sub(0x168);
      O.sub(0x1c).writePointer(vbt(0x300, 0x480));
      D.add(0x17c).writePointer(vtO);
      D.add(0x2ac).writeU32(0x01010000 + v);
      D.add(0x3ec).writeS32(arg);
      O.sub(0x10).writePointer(hasCb ? cdecl1(0xD0000000 + 0x100 * v) : ptr(0));
      genericCycle(D, v ? container(1) : ptr(0));
      fx['d1_' + v] = D;
    });

    // ---- tags no case handles ---------------------------------------------------------------------------------
    [0, 3, 5, 0x11, -1, 0x7fffffff, 0x10000, 16 + 0x100].forEach(function (tag, v) {
      const D = block(tag);
      genericCycle(D, ptr(0));
      fx['dx_' + v] = D;
    });
    return fx;
  }

  // ---- Surface aggregates for the gSurfaceDtor_* keys ------------------------------------------------------------
  // Three 0xa00-byte aggregates ($sd0..2) tiled with 0x2c-byte Surfaces (slot i at +0x2c*i): +0 0x11110000+i (any
  // value: the destructors store 0x004ba2d8 there), +4 the handle Surface_free 0x00473ae0 releases through the
  // display's slot +0x94 when it is non-zero (0x00473af0..0x00473afa), +8 and +0xc 0 (so Surface_free never reaches
  // its free 0x004a5007 or 0x00483010 + operator delete), +0x10..+0x28 a pattern unique per dword (Surface_free clears
  // +0x10..+0x24, 0x00473b2e..0x00473b3d). Handles: $sd0 every slot, $sd1 odd slots, $sd2 none.
  // The live display pointer 0x0083ad50 is swapped, with the game's first thread frozen (freeze.js), for a fixture
  // display whose slot +0x94 records (mode 'disp') or for 0 (mode 'nodisp': Surface_free returns at 0x00473aee).
  function surfaces(mode) {
    const fx = {};
    const emit = codeArena();
    const rec = Memory.alloc(0x400);
    for (let i = 0; i < 0x400; i += 4) rec.add(i).writeU32(0);
    rec.writeU32(0x10);
    fx.rec = rec; fx.obj = rec;
    const nul = emit([0x33, 0xC0, 0xC3]);
    const vt = Memory.alloc(0x200);
    for (let i = 0; i < 0x80; i++) vt.add(i * 4).writePointer(nul);
    vt.add(0x94).writePointer(emit(stubBytes(rec, 0xD1590000, 1, true)));
    const disp = Memory.alloc(0x40);
    disp.writePointer(vt);
    for (let o = 0; o < 3; o++) {
      const a = Memory.alloc(0xa00);
      for (let i = 0; i < 0xa00; i += 4) a.add(i).writeU32(0x5a5a5a5a);
      for (let i = 0; (i + 1) * 0x2c <= 0xa00; i++) {
        const sp = a.add(0x2c * i);
        sp.writeU32(0x11110000 + i);
        const hnd = o === 0 ? 0x4800 + i : (o === 1 && (i & 1) ? 0x4900 + i : 0);
        sp.add(4).writeU32(hnd);
        sp.add(8).writeU32(0);
        sp.add(0xc).writeU32(0);
        for (let j = 0x10; j < 0x2c; j += 4) sp.add(j).writeU32(0x7e000000 + o * 0x100000 + i * 0x100 + j);
      }
      fx['sd' + o] = a;
    }
    __freeze.thread();
    const slot = ptr('0x0083ad50');
    __freeze.save(slot, 4);
    slot.writePointer(mode === 'disp' ? disp : ptr(0));
    return fx;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3cu_view() { return build(); },
    // viewDispatchSelectChanged's tags 1, 2 and 0x10 write 0x0083ab2c, which the game's first thread rewrites from its
    // own UI dispatch: that key freezes the thread (shared helper re/frida/js/freeze.js) and restores the global.
    c3cu_view_frozen() {
      const fx = build();
      __freeze.thread();
      __freeze.save(ptr(G_CURWIDGET), 4);
      return fx;
    },
    c3cu_surfdtor() { return surfaces('disp'); },
    c3cu_surfdtor_nodisp() { return surfaces('nodisp'); },
  });
})();
