// Fixtures for batch c3dx (round 28, golf_clean.exe UI and text rows): scrollbarOnDown / scrollbarScrollBy /
// scrollbarOnKey, TextView::updateSelection / select, buildStatusText. Reimplementations: shim/src/re/c3dx.cpp.
// Registry: re/frida/registry.d/c3dx.py. Purposes and per-jcc branch coverage: log/c3/c3dx_purpose.md.
//
// Every object is our own allocation; none is linked into the game's window list, draw list or focus chain. Virtual
// calls land in RECORDING STUBS written as machine code (no JavaScript runs inside a vector), the method of c3ct:
//     mov eax, [REC]; add eax, REC            ; REC+0 is the record cursor (byte offset)
//     mov dword [eax], tag; mov [eax+4], ecx  ; (cdecl stubs store the constant 0xC0C0C0C0 instead of ecx)
//     mov edx, [esp+4i]; mov [eax+4+4i], edx  ; each stack argument
//     mov edx, [WATCH]; mov [eax+8+4n], edx   ; a global the function under test writes, as it is at the call
//     add dword [REC], 4(n+3)
//     mov eax, [REC]; xor eax, tag            ; a return value that differs per call
//       or  mov eax, [ecx+off]                ; (value stubs: a field of the called object)
//     ret 4n (thiscall) | ret (cdecl)
// The recorder block is part of every key's state, so the order, target, arguments and the watched global of each
// dispatched call are compared.
//
// Every key freezes the game's first thread with the shared helper re/frida/js/freeze.js: the functions write the
// drag-capture globals 0x0083ab30 / 0x0083ab34, the current widget 0x0083ab2c and 0x0083ff18, and buildStatusText
// writes the shared text buffer 0x0051a068, which the running menu rewrites. They are saved and written back before
// the thread resumes, and they are state regions of the keys.
(function () {
  const G_DRAG = 0x0083ab30, G_DRAG2 = 0x0083ab34, G_CUR = 0x0083ab2c, G_KEY = 0x0083ff18;

  function u32(v) { v >>>= 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; }
  function fill(p, n, v) { for (let i = 0; i < n; i += 4) p.add(i).writeU32(v >>> 0); }
  function alloc(n, v) { const p = Memory.alloc(n); fill(p, n, v === undefined ? 0 : v); return p; }

  // The negative-control build (log/c3/alt/c3dx_negctl_gen.py) keeps its wrong bodies dormant until its exported NegCtl switch is set;
  // in the normal build there is no such export and nothing is written. Always called right after the first thread froze.
  function negOn() {
    __freeze.negOn();   // every exported <Batch>NegCtl switch (re/frida/js/freeze.js)
  }
  function codeArena() {
    const size = Process.pageSize * 4;
    const base = Memory.alloc(size);
    Memory.protect(base, size, 'rwx');
    let off = 0;
    return function (bytes) {
      const p = base.add(off);
      p.writeByteArray(bytes);
      off += (bytes.length + 15) & ~15;
      if (off > size) throw new Error('c3dx: code arena full');
      return p;
    };
  }

  // A recording stub (see the header). retOff === undefined: cursor ^ tag; otherwise [ecx + retOff].
  function stubBytes(rec, watch, tag, n, thiscall, retOff) {
    const r = u32(rec.toUInt32());
    const b = [];
    b.push(0xA1, ...r);                                              // mov eax, [REC]
    b.push(0x05, ...r);                                              // add eax, REC
    b.push(0xC7, 0x00, ...u32(tag));                                 // mov dword [eax], tag
    if (thiscall) b.push(0x89, 0x48, 0x04);                          // mov [eax+4], ecx
    else b.push(0xC7, 0x40, 0x04, ...u32(0xC0C0C0C0));
    for (let i = 1; i <= n; i++) b.push(0x8B, 0x54, 0x24, 4 * i, 0x89, 0x50, 4 + 4 * i);
    b.push(0x8B, 0x15, ...u32(watch));                               // mov edx, [WATCH]
    b.push(0x89, 0x50, 8 + 4 * n);                                   // mov [eax+8+4n], edx
    b.push(0x81, 0x05, ...r, ...u32(4 * (n + 3)));                   // add dword [REC], 4(n+3)
    if (retOff === undefined) {
      b.push(0xA1, ...r);                                            // mov eax, [REC]
      b.push(0x35, ...u32(tag));                                     // xor eax, tag
    } else {
      b.push(0x8B, 0x81, ...u32(retOff));                            // mov eax, [ecx+retOff]
    }
    if (thiscall && n > 0) b.push(0xC2, 4 * n, 0x00); else b.push(0xC3);
    return b;
  }

  function newRec(size) { const rec = alloc(size); rec.writeU32(0x10); return rec; }

  // A vtable of 0x80 slots: unused slots `xor eax, eax; ret`; spec [[slot offset, tag, n, retOff], ...] (thiscall).
  function vtable(emit, rec, watch, spec) {
    const nul = emit([0x33, 0xC0, 0xC3]);
    const vt = alloc(0x200);
    for (let i = 0; i < 0x80; i++) vt.add(i * 4).writePointer(nul);
    for (const [slot, tag, n, retOff] of spec) vt.add(slot).writePointer(emit(stubBytes(rec, watch, tag, n, true, retOff)));
    return vt;
  }

  // Freeze the first thread and save the globals the key writes (restored before the thread resumes).
  function freezeAndSave(globals) {
    __freeze.thread(); negOn();
    for (const g of globals) __freeze.save(ptr(g), 4);
  }

  // ---- scrollbars ---------------------------------------------------------------------------------------------------
  // Scrollbar objects (0x1400 bytes, pre-filled 0x5a5a5a5a):
  //   +0      the scrollbar vtable: slot +0x6c (2 arguments) and +0x120 (none) record
  //   +0x130  the parent (0x100 bytes): vtable slot +0xe4 (2 arguments) records; +0x84 a grandchild with vtable slot +0x1c
  //           (records) or NULL
  //   +0x278  the child (0x80 bytes) on a vtable whose slot +0xd8 returns [child+0x40] (the width) and +0xdc returns
  //           [child+0x44] (the height), or NULL
  //   +0x13ac a cdecl callback taking 2 arguments (records) or NULL
  //   +0x578 id   +0x580 min   +0x584 max   +0x588 invert flag   +0x58c position   +0x598 / +0x59c / +0x5a4 written
  //   +0x5a0 page step   +0x5ac / +0x5b4 x bounds   +0x5b0 / +0x5b8 y bounds
  //          width height min max inv pos step cb grand child
  const SB = [[40, 10, 0, 20, 0, 5, 4, 1, 1, 1],       // wide, position inside, callback and grandchild present
              [10, 40, -5, 30, 1, 30, 7, 0, 0, 1],     // tall, inverted, position at the maximum, neither present
              [25, 25, 0, 2, 0, 0, 3, 1, 1, 1],        // equal extents (the not-above side), position at the minimum
              [0, 0, 3, 9, 1, 6, 10, 1, 0, 0],         // no child: both extents 0
              [30, 12, 0, 20, 1, 19, 100, 0, 1, 1],    // wide, inverted, a page step larger than the range
              [8, 21, -3, -3, 0, -3, 2, 1, 1, 1],      // tall, one-point range
              [-5, -9, 10, 12, 0, 10, 1, 1, 0, 1],     // negative extents, width above height
              [-9, -5, 0, 10, 1, 4, 6, 0, 1, 1]];      // negative extents, height above width
  const SB_LO_X = 10, SB_HI_X = 50, SB_LO_Y = 8, SB_HI_Y = 40;

  function scrollFixture(drag, second) {
    const fx = {};
    const emit = codeArena();
    const rec = newRec(0x400);
    fx.rec = rec; fx.obj = rec;
    const vtS = vtable(emit, rec, G_DRAG, [[0x6c, 0x5B6C0000, 2], [0x120, 0x5B120000, 0]]);
    const vtP = vtable(emit, rec, G_CUR, [[0xe4, 0x5BE40000, 2]]);
    const vtG = vtable(emit, rec, G_CUR, [[0x1c, 0x5B1C0000, 0]]);
    const vtC = vtable(emit, rec, G_DRAG, [[0xd8, 0xC0D80000, 0, 0x40], [0xdc, 0xC0DC0000, 0, 0x44]]);
    const cb = emit(stubBytes(rec, G_CUR, 0x5BCB0000, 2, false));
    SB.forEach(function (r, i) {
      const [w, h, mn, mx, inv, pos, step, hasCb, hasGrand, hasChild] = r;
      const s = alloc(0x1400, 0x5a5a5a5a);
      s.writePointer(vtS);
      const par = alloc(0x100, 0x3b3b3b3b);
      par.writePointer(vtP);
      if (hasGrand) {
        const g = alloc(0x40, 0x3c3c3c3c);
        g.writePointer(vtG);
        par.add(0x84).writePointer(g);
        fx['gr' + i] = g;
      } else {
        par.add(0x84).writePointer(ptr(0));
      }
      s.add(0x130).writePointer(par);
      if (hasChild) {
        const c = alloc(0x80, 0x2c2c2c2c);
        c.writePointer(vtC);
        c.add(0x40).writeS32(w);
        c.add(0x44).writeS32(h);
        s.add(0x278).writePointer(c);
        fx['ch' + i] = c;
      } else {
        s.add(0x278).writePointer(ptr(0));
      }
      s.add(0x13ac).writePointer(hasCb ? cb : ptr(0));
      s.add(0x578).writeS32(0x40 + i);
      s.add(0x580).writeS32(mn);
      s.add(0x584).writeS32(mx);
      s.add(0x588).writeS32(inv);
      s.add(0x58c).writeS32(pos);
      s.add(0x598).writeS32(0x77);
      s.add(0x59c).writeS32(0x66);
      s.add(0x5a0).writeS32(step);
      s.add(0x5a4).writeS32(0x55);
      s.add(0x5ac).writeS32(SB_LO_X);
      s.add(0x5b4).writeS32(SB_HI_X);
      s.add(0x5b0).writeS32(SB_LO_Y);
      s.add(0x5b8).writeS32(SB_HI_Y);
      fx['sb' + i] = s;
      fx['par' + i] = par;
    });
    freezeAndSave([G_DRAG, G_DRAG2, G_CUR, G_KEY]);
    // The drag-capture pair: 'free' = 0 (the capture is taken), 'held' = another widget (left alone).
    ptr(G_DRAG).writePointer(drag === 'free' ? ptr(0) : fx.sb7);
    ptr(G_DRAG2).writePointer(second === undefined ? fx.sb6 : second === 0 ? ptr(0) : fx.sb5);
    ptr(G_CUR).writePointer(fx.sb4);
    ptr(G_KEY).writeU32(0x4b4b4b4b);
    return fx;
  }

  // ---- TextView ---------------------------------------------------------------------------------------------------
  // TextView objects (0x3000 bytes, pre-filled 0x5a5a5a5a). V is the object:
  //   V+0       vtable: slot +8 and +0x110 record (no arguments)
  //   V+0x590   another object (0x40 bytes) with vtable slot +0x110 recording, or NULL
  //   V+0x1460  the sub-object getFieldValueByKind reads: kind dword at V+0x1654; for kinds 1, 2, 4 and 16 it returns
  //             the model's count, for kind 8 the dword V+0x1578, otherwise 0
  //   V+0x1464  the virtual-base table (entry +8 = d): the ListModel is at M = V + d + 0x1464
  //   V+0x14d8  a second table (entry +8 = e): the id of kind 1 is the dword V+e+0x15c8
  //   M+0xc8 head  M+0xcc cursor  M+0xd0 count  M+0xd4 index  M+0xf0 selected index; nodes (0x20): +4 id, +0xc next,
  //   +0x10 prev
  //   V+0x1f68  flags (bits 0x4000, 0x100 and 0x40)       V+0x5c0 and V+0x1fc0 are written
  //            kind flags   d     count sel other ids
  const TV = [[4, 0x0000, 0x000, 3, 1, 1, [11, 22, 33]],
              [1, 0x0000, 0x200, 2, 0, 1, [44, 55]],
              [-1, 0x4040, 0x000, 2, 0, 0, [66, 77]],
              [4, 0x0100, 0x000, 0, 0, 0, []],
              [8, 0x4100, 0x000, 2, -1, 0, [88, 99]],
              [2, 0x0000, 0x300, 4, 6, 0, [101, 102, 103, 104]],
              [-1, 0x0000, 0x000, 1, 0, 1, [111]],
              [3, 0x0000, 0x000, 3, -2, 0, [121, 122, 123]],
              [16, 0x4000, 0x000, 3, 2, 0, [131, 132, 133]],
              [1, 0x0000, 0x000, 0, 0, 0, []],
              [-1, 0x0100, 0x200, 3, 1, 1, [141, 142, 143]],
              [4, 0x0040, 0x000, 2, 1, 1, [151, 152]],
              [-1, 0x0140, 0x000, 2, 0, 0, [161, 162]]];
  const TV_E = 0x20, TV_ID1 = 0x7a7a;

  function textFixture() {
    const fx = {};
    const emit = codeArena();
    const rec = newRec(0x400);
    fx.rec = rec; fx.obj = rec;
    const vtV = vtable(emit, rec, G_DRAG, [[0x8, 0x7A080000, 0], [0x110, 0x7A110000, 0]]);
    const vtO = vtable(emit, rec, G_DRAG, [[0x110, 0x7B110000, 0]]);
    TV.forEach(function (r, i) {
      const [kind, flags, d, count, sel, other, ids] = r;
      const v = alloc(0x3000, 0x5a5a5a5a);
      v.writePointer(vtV);
      if (other) {
        const o = alloc(0x40, 0x4d4d4d4d);
        o.writePointer(vtO);
        v.add(0x590).writePointer(o);
        fx['oth' + i] = o;
      } else {
        v.add(0x590).writePointer(ptr(0));
      }
      v.add(0x5c0).writeS32(0x1234);
      const t1 = alloc(0x20, 0x6c6c6c6c);
      t1.add(8).writeS32(d);
      v.add(0x1464).writePointer(t1);
      const t2 = alloc(0x20, 0x6d6d6d6d);
      t2.add(8).writeS32(TV_E);
      v.add(0x14d8).writePointer(t2);
      v.add(TV_E + 0x15c8).writeS32(TV_ID1 + i);
      v.add(0x1578).writeS32(0x77 + i);
      v.add(0x1654).writeS32(kind);
      v.add(0x1f68).writeU32(flags);
      v.add(0x1fc0).writeS32(0x3333);
      const m = v.add(d + 0x1464);
      const nodes = [];
      for (let k = 0; k < Math.max(ids.length, 1); k++) {
        const nd = alloc(0x20, 0x7e7e7e7e);
        nodes.push(nd);
      }
      ids.forEach(function (id, k) {
        const nd = nodes[k];
        nd.add(4).writeS32(id);
        nd.add(0xc).writePointer(nodes[(k + 1) % ids.length]);
        nd.add(0x10).writePointer(nodes[(k + ids.length - 1) % ids.length]);
      });
      m.add(0xc8).writePointer(ids.length ? nodes[0] : ptr(0));
      m.add(0xcc).writePointer(ids.length ? nodes[ids.length - 1] : ptr(0));
      m.add(0xd0).writeS32(count);
      m.add(0xd4).writeS32(0x2222);
      m.add(0xf0).writeS32(sel);
      fx['tv' + i] = v;
      fx['nodes' + i] = nodes[0];
    });
    freezeAndSave([G_DRAG]);
    return fx;
  }

  // ---- buildStatusText ------------------------------------------------------------------------------------------------
  // Golfers 20..29 get a kind byte (0x005794d0 + g*0x100: the form buildGolferName takes), a hole byte (0x005794d9,
  // signed) and the type word (0x0057956e). The text buffer holds a synthetic text of a different length per variant, the
  // status buffer 0x005695a8 a marker, and the priority byte 0x005a9cd0 the variant's value.
  const GOLFERS = [[0x80, 3], [0x81, -3], [0x00, 18], [0x24, 0], [0x40, 127], [0x60, -128], [0x63, 55], [0x69, 100],
                   [0x61, 7], [0xa0, 1]];
  const SAVED_TEXT = ['', 'abc', 'saved text of the message line, longer than a word', 'x'];

  function statusFixture(cur, textVariant) {
    const fx = {};
    __freeze.thread(); negOn();
    __freeze.save(ptr(0x0051a068), 0x200);
    __freeze.save(ptr(0x005695a8), 0x80);
    __freeze.save(ptr(0x0058a528), 0x20);
    __freeze.save(ptr(0x005a9cd0), 4);
    GOLFERS.forEach(function (r, i) {
      const g = 20 + i;
      const base = 0x005794d0 + g * 0x100;
      __freeze.save(ptr(base), 0x10);
      __freeze.save(ptr(0x0057956e + g * 0x100), 4);
      ptr(base).writeU8(r[0]);
      ptr(base + 9).writeS8(r[1]);
      ptr(0x0057956e + g * 0x100).writeU16(i % 4);
    });
    const t = Array.from(SAVED_TEXT[textVariant], (c) => c.charCodeAt(0)).concat([0]);
    ptr(0x0051a068).writeByteArray(t);
    ptr(0x005695a8).writeByteArray(Array.from('marker', (c) => c.charCodeAt(0)).concat([0]));
    ptr(0x005a9cd0).writeS8(cur);
    return fx;
  }

  // ---- snapshotTables and initRecords ---------------------------------------------------------------------------------
  // Both take no argument, so a key is one call; the fixtures give the tables they touch a different pattern per key
  // (the tables are saved and written back before the first thread resumes), so the copies / clears have something to
  // move and a missing, short or misdirected copy shows in the bytes.
  //   snapshotTables: the whole span 0x0082415c .. 0x00838204 (ten 0x1000-byte sources and destinations, the two gap
  //   dwords 0x0082b15c / 0x008371f4 and the source dword 0x00838200).
  //   initRecords: the position-record table 0x0056d1d8 .. 0x0056e948 and the record banks 0x004e6d20 .. 0x004e71b0.
  function patternFill(base, size, kind) {
    __freeze.save(ptr(base), size);
    const buf = new Uint8Array(size);
    for (let i = 0; i < size; i++) {
      let b;
      if (kind === 0) b = (i * 7 + (i >> 8) * 13 + 1) & 0xff;
      else if (kind === 1) b = ((i >> 2) * 31 + 0x55) & 0xff;
      else if (kind === 2) b = (i & 3) === 0 ? 0xa5 : (i >> 4) & 0xff;
      else b = (Math.imul(i + kind * 0x1357, (0x9e3779b1 + kind * 2) | 0) >>> (8 + (kind % 3) * 4)) & 0xff;
      buf[i] = b;
    }
    ptr(base).writeByteArray(buf.buffer);
  }
  function snapshotFixture(kind) {
    const fx = {};
    __freeze.thread(); negOn();
    patternFill(0x0082415c, 0x140a8, kind);
    return fx;
  }
  function recordsFixture(kind) {
    const fx = {};
    __freeze.thread(); negOn();
    patternFill(0x0056d1d8, 0x1770, kind);
    patternFill(0x004e6d20, 0x490, kind + 1);
    return fx;
  }

  // ---- ListBox::selectById --------------------------------------------------------------------------------------------
  // Objects S (0xa00 bytes, pre-filled 0x5a5a5a5a): S+4 the virtual-base table (entry +4 = dw, entry +8 = ds), S+0x10 the
  // force flag, S+0x28 top, S+0x2c visible rows, S+0x50 a cdecl callback (records) or NULL. The window base W = S + dw + 4
  // keeps a vptr (slot +0x120 records); the parent pointer is the dword S + dw + 0x134 (vtable slots +0x100 (1 argument) and
  // +0xfc (2) record; +0x94 a child with slot +0x1c). The model M = S + ds: M+0xf4 the selected row, M+0xcc head, M+0xd0
  // cursor, M+0xd4 count, M+0xd8 index, nodes (0x20 bytes) with the key at +4 and next / prev at +0xc / +0x10. The visible
  // rows are never below the count, so ListBox::setSelection takes no scroll path.
  //   dw     ds     keys          sel force parent cb  child
  const SI = [[0x100, 0x400, [10, 20, 30], 1, 0, 1, 0, 0],
              [0x100, 0x400, [10, 20, 30], -1, 0, 1, 0, 1],
              [0x140, 0x440, [5, 5, 7], 2, 0, 0, 1, 0],
              [0x100, 0x400, [], 4, 0, 1, 0, 0],
              [0x140, 0x440, [], -1, 0, 1, 1, 0],
              [0x100, 0x400, [8, 9], 0, 0, 1, 0, 0],
              [0x140, 0x440, [1, 2], 0, 1, 1, 1, 1],
              [0x100, 0x400, [3, 4, 5, 6], 2, 0, 1, 1, 1],
              [0x140, 0x440, [7, 8, 9], 5, 0, 0, 0, 0]];
  const SI_ZERO_COUNT = 5;      // object 5 keeps its ring but has count 0
  function selectByIdFixture() {
    const fx = {};
    __freeze.thread(); negOn();
    const emit = codeArena();
    const rec = newRec(0x1000);
    fx.rec = rec; fx.obj = rec;
    const vtW = vtable(emit, rec, G_DRAG, [[0x120, 0x7F120000, 0]]);
    const vtP = vtable(emit, rec, G_DRAG, [[0x100, 0x7F100000, 1], [0xfc, 0x7F0FC000, 2]]);
    const vtC = vtable(emit, rec, G_DRAG, [[0x1c, 0x7F1C0000, 0]]);
    const cb = emit(stubBytes(rec, G_DRAG, 0x7FCB0000, 1, false));
    SI.forEach(function (r, i) {
      const [dw, ds, keys, sel, force, hasPar, hasCb, hasChild] = r;
      const s = alloc(0xa00, 0x5a5a5a5a);
      const vbt = alloc(0x20, 0x6e6e6e6e);
      vbt.add(4).writeS32(dw);
      vbt.add(8).writeS32(ds);
      s.add(4).writePointer(vbt);
      s.add(0x10).writeS32(force);
      s.add(0x28).writeS32(0);
      s.add(0x2c).writeS32(8);
      s.add(0x50).writePointer(hasCb ? cb : ptr(0));
      s.add(dw + 4).writePointer(vtW);
      if (hasPar) {
        const par = alloc(0x100, 0x3b3b3b3b);
        par.writePointer(vtP);
        if (hasChild) {
          const ch = alloc(0x40, 0x3c3c3c3c);
          ch.writePointer(vtC);
          par.add(0x94).writePointer(ch);
        } else {
          par.add(0x94).writePointer(ptr(0));
        }
        s.add(dw + 0x134).writePointer(par);
      } else {
        s.add(dw + 0x134).writePointer(ptr(0));
      }
      const m = s.add(ds);
      m.add(0x5c).writeS32(1);
      m.add(0x60).writeS32(1);
      const nodes = keys.map(function (k) {
        const n = alloc(0x20, 0x7e7e7e7e);
        n.add(4).writeS32(k);
        return n;
      });
      nodes.forEach(function (n, j) {
        n.add(0xc).writePointer(nodes[(j + 1) % nodes.length]);
        n.add(0x10).writePointer(nodes[(j + nodes.length - 1) % nodes.length]);
      });
      m.add(0xcc).writePointer(nodes.length ? nodes[0] : ptr(0));
      m.add(0xd0).writePointer(nodes.length ? nodes[nodes.length - 1] : ptr(0));
      m.add(0xd4).writeS32(i === SI_ZERO_COUNT ? 0 : nodes.length);
      m.add(0xd8).writeS32(0x55);
      m.add(0xf4).writeS32(sel);
      fx['sx' + i] = s;
    });
    return fx;
  }

  // ---- List401c70_dtor / C4021e0_dtor (twin list destructors) -----------------------------------------------------------
  // The method's ecx is P = O + 0x1c with O the list (0x80 bytes, pre-filled 0x5a5a5a5a): O+4 the virtual-base table pointer
  // (entry +4 = 0x20 or 0x28, so the store of the base vtable lands inside O), O+8 head, O+0xc cursor, O+0x10 count, O+0x14
  // index. Nodes (0x60 bytes) and items (0x20 bytes) are deleted through the virtual-base pattern: the vtable dword +4 is the
  // offset (0 or 0x30; at +0x30 the object keeps a vptr to the same vtable) and slot 0 of the vtable at node+offset records
  // (argument 1). The vtable stored at O+0 is the game's own (its slot +4 is `ret 4`).
  //   nodes, items present flags, offset, count override, vbase offset
  const DL = [[3, [1, 1, 1], 0, null, 0x20],
              [1, [0], 0, null, 0x20],
              [2, [1, 0], 0x30, null, 0x28],
              [0, [], 0, null, 0x20],
              [2, [1, 1], 0, 0, 0x20],
              [4, [1, 1, 1, 1], 0, 2, 0x28],
              [2, [0, 1], 0x30, null, 0x20],
              [3, [0, 0, 0], 0x30, null, 0x28],
              [1, [1], 0, null, 0x20],
              [5, [1, 0, 1, 0, 1], 0, null, 0x20],
              [2, [1, 1], 0x30, 1, 0x28],
              [3, [1, 0, 1], 0, null, 0x28]];
  function dtorListFixture() {
    const fx = {};
    const emit = codeArena();
    const rec = newRec(0x1000);
    fx.rec = rec; fx.obj = rec;
    __freeze.thread(); negOn();
    const mkVt = function (tag, off) {
      const vt = vtable(emit, rec, G_DRAG, [[0x0, tag, 1]]);
      vt.add(4).writeU32(off);
      return vt;
    };
    const vtNode0 = mkVt(0x7D010000, 0), vtNode30 = mkVt(0x7D020000, 0x30);
    const vtItem0 = mkVt(0x7D030000, 0), vtItem30 = mkVt(0x7D040000, 0x30);
    DL.forEach(function (r, g) {
      const [n, items, off, cnt, vbo] = r;
      const o = alloc(0x80, 0x5a5a5a5a);
      const vbt = alloc(0x20, 0x6e6e6e6e);
      vbt.add(4).writeS32(vbo);
      o.add(4).writePointer(vbt);
      const nodes = [];
      for (let i = 0; i < n; i++) {
        const nd = alloc(0x60, 0x4e4e4e4e);
        nd.writePointer(off ? vtNode30 : vtNode0);
        if (off) nd.add(off).writePointer(vtNode30);
        if (items[i]) {
          const it = alloc(0x20, 0x4f4f4f4f);
          it.writePointer(off ? vtItem30 : vtItem0);
          if (off) it.add(off).writePointer(vtItem30);
          nd.add(8).writePointer(it);
        } else {
          nd.add(8).writePointer(ptr(0));
        }
        nodes.push(nd);
        fx['dn' + g + '_' + i] = nd;
      }
      nodes.forEach(function (nd, i) {
        nd.add(0xc).writePointer(nodes[(i + 1) % n]);
        nd.add(0x10).writePointer(nodes[(i + n - 1) % n]);
      });
      o.add(8).writePointer(n ? nodes[0] : ptr(0));
      o.add(0xc).writePointer(n ? nodes[n - 1] : ptr(0));
      o.add(0x10).writeS32(cnt === null ? n : cnt);
      o.add(0x14).writeS32(0x66);
      fx['dl' + g] = o;
      fx['dp' + g] = o.add(0x1c);
    });
    return fx;
  }

  // ---- Window::doLayout -------------------------------------------------------------------------------------------------
  // Windows (0x700 bytes, pre-filled 0x5a5a5a5a): +0 vtable (slot +0x58 records), +0xa0 = 0 (not visible, so
  // Window::invalidate returns after Window::visible), +0x220 flags, +0x570 a cdecl function pointer (records) or NULL.
  //   flags, callback
  const WL = [[0, 1], [0, 0], [1, 1], [3, 0], [2, 1], [0xfffffffe, 0], [0x101, 1], [0x100, 1], [0x80000000, 0], [0xfffffffc, 1]];
  function layoutFixture() {
    const fx = {};
    const emit = codeArena();
    const rec = newRec(0x800);
    fx.rec = rec; fx.obj = rec;
    const vtW = vtable(emit, rec, G_CUR, [[0x58, 0x7D580000, 0]]);
    const cb = emit(stubBytes(rec, G_CUR, 0x7D5C0000, 0, false));
    WL.forEach(function (r, i) {
      const w = alloc(0x700, 0x5a5a5a5a);
      w.writePointer(vtW);
      w.add(0xa0).writeU32(0);
      w.add(0x220).writeU32(r[0]);
      w.add(0x570).writePointer(r[1] ? cb : ptr(0));
      fx['wl' + i] = w;
    });
    freezeAndSave([G_CUR]);
    ptr(G_CUR).writePointer(fx.wl9);
    return fx;
  }

  // ---- panelSubSlot60 -----------------------------------------------------------------------------------------------------
  // Objects Q = B + 0xe0 with B (0x400 bytes, pre-filled 0x5a5a5a5a): the dword at B is a virtual-base table (entry +4 = the
  // offset); the target B + offset keeps a vptr to a vtable of its own (slot +0x120 records, tag per object).
  const PS = [0x40, 0x80, 0x100, 0x140, 0x200, 0x2c0, 0x18, 0x24, 0x60, 0x90, 0x1c0, 0x300];
  function slot60Fixture() {
    const fx = {};
    const emit = codeArena();
    const rec = newRec(0x800);
    fx.rec = rec; fx.obj = rec;
    PS.forEach(function (off, i) {
      const b = alloc(0x400, 0x5a5a5a5a);
      const vbt = alloc(0x20, 0x6e6e6e6e);
      vbt.add(4).writeS32(off);
      b.writePointer(vbt);
      const vt = vtable(emit, rec, G_DRAG, [[0x120, (0x7D600000 + (i << 16)) >>> 0, 0]]);
      b.add(off).writePointer(vt);
      fx['pb' + i] = b;
      fx['pq' + i] = b.add(0xe0);
    });
    __freeze.thread(); negOn();
    return fx;
  }

  // ---- listPanelReadValue ---------------------------------------------------------------------------------------------
  // List panels: a 0x1400-byte block S (pre-filled 0x5a5a5a5a) whose first dword is the virtual-base table (entry +4 = dw,
  // entry +8 = ds). The method's this is T = S + 0x24. The window base is W = S + dw: W+0 a vtable (slot +0x120 records),
  // W+0x4c a pointer to an object with vtable slot +0x1c recording or NULL, W+0x2d0 the font pointer Widget_value reads
  // (font +8 >= 0: +0x10 + +8, else +0xc). The model base is R = T + ds: R+0x24, +0x38, +0x44, +0x48 numbers, the ring
  // head R+0xa4, cursor R+0xa8, count R+0xac, index R+0xb0, selected R+0xcc, hover R+0xd0, previous hover R+0xd4; nodes
  // (0x20 bytes): +8 item, +0xc next, +0x10 prev; items (0x20 bytes): +8 the row weight.
  //   dw    ds     weights        cursor idx sel hover prev r24 r44 r48 r38 font obj
  const LP = [[0x60, 0x400, [3, 5, 2, 4], 2, 1, 1, 7, 7, 4, 20, 6, 3, 0, 1],
              [0x80, 0x440, [2, 2, 2], 0, 0, 0, -1, 3, 4, 20, 6, 2, 1, 0],
              [0xa0, 0x480, [7], 0, 0, 5, 5, 2, 4, 20, 6, 1, 0, 1],
              [0x60, 0x400, [], 0, -1, 0, 3, 3, 4, 20, 6, 3, 0, 1],
              [0x80, 0x440, [1, 4, 1, 4, 1, 4], 3, 2, 2, -1, -1, 2, 10, 0, 1, 1, 1],
              [0xa0, 0x480, [2, 3, 4], 1, 1, 2, -1, 5, 0, 1, 0, 1, 0, 0],
              [0x60, 0x400, [5, 6, 7, 8], 1, 0, 3, 3, -1, 3, 12, 2, 2, 1, 1],
              [0x80, 0x440, [1, 1], 1, 1, 6, 6, 6, 1, 7, 4, 0, 0, 1],
              [0x60, 0x400, [], 0, -2, 0, 3, 3, 4, 20, 6, 3, 0, 1, -1]];
  function listPanelFixture() {
    const fx = {};
    __freeze.thread(); negOn();
    const emit = codeArena();
    const rec = newRec(0x800);
    fx.rec = rec; fx.obj = rec;
    const vtW = vtable(emit, rec, G_DRAG, [[0x120, 0x7E120000, 0]]);
    const vtO = vtable(emit, rec, G_DRAG, [[0x1c, 0x7E1C0000, 0]]);
    const fontA = alloc(0x20, 0x4a4a4a4a);
    fontA.add(8).writeS32(2); fontA.add(0xc).writeS32(77); fontA.add(0x10).writeS32(10);   // value 12
    const fontB = alloc(0x20, 0x4b4b4b4b);
    fontB.add(8).writeS32(-1); fontB.add(0xc).writeS32(9); fontB.add(0x10).writeS32(55);   // value 9
    LP.forEach(function (r, i) {
      const [dw, ds, weights, cur, idx, sel, hov, prev, r24, r44, r48, r38, font, hasObj, cnt] = r;
      const S = alloc(0x1400, 0x5a5a5a5a);
      const vbt = alloc(0x20, 0x6e6e6e6e);
      vbt.add(4).writeS32(dw);
      vbt.add(8).writeS32(ds);
      S.writePointer(vbt);
      const W = S.add(dw);
      W.writePointer(vtW);
      if (hasObj) {
        const o = alloc(0x40, 0x4c4c4c4c);
        o.writePointer(vtO);
        W.add(0x4c).writePointer(o);
        fx['lpo' + i] = o;
      } else {
        W.add(0x4c).writePointer(ptr(0));
      }
      W.add(0x2d0).writePointer(font ? fontB : fontA);
      const R = S.add(0x24 + ds);
      R.add(0x24).writeS32(r24);
      R.add(0x38).writeS32(r38);
      R.add(0x44).writeS32(r44);
      R.add(0x48).writeS32(r48);
      const nodes = weights.map(function (w) {
        const n = alloc(0x20, 0x7e7e7e7e);
        const it = alloc(0x20, 0x7f7f7f7f);
        it.add(8).writeS32(w);
        n.add(8).writePointer(it);
        return n;
      });
      nodes.forEach(function (n, j) {
        n.add(0xc).writePointer(nodes[(j + 1) % nodes.length]);
        n.add(0x10).writePointer(nodes[(j + nodes.length - 1) % nodes.length]);
      });
      R.add(0xa4).writePointer(nodes.length ? nodes[0] : ptr(0));
      R.add(0xa8).writePointer(nodes.length ? nodes[cur] : ptr(0));
      R.add(0xac).writeS32(cnt === undefined ? nodes.length : cnt);
      R.add(0xb0).writeS32(idx);
      R.add(0xcc).writeS32(sel);
      R.add(0xd0).writeS32(hov);
      R.add(0xd4).writeS32(prev);
      fx['lp' + i] = S.add(0x24);     // the method's this
      fx['lps' + i] = S;
      // The vtordisp thunk's input: this + 0x600 with the displacement 0x600 stored in the dword in front of it.
      S.add(0x24 + 0x600 - 4).writeS32(0x600);
      fx['lpu' + i] = S.add(0x24 + 0x600);
    });
    return fx;
  }

  // ---- movieSelectStream ----------------------------------------------------------------------------------------------
  // Movie objects (0xa00 bytes, pre-filled 0x5a5a5a5a): +0xbc enable dword, +0x938 count, +0x948 the record array (16-byte
  // records: type dword, flag byte at +4), +0x94c written, +0x1a4 the wanted type. Records are given as [type, flags];
  // a record of the wanted type counts, the others are skipped.
  //   enable count type  records
  const MV = [[1, 4, 7, [[7, 0], [3, 0], [7, 0x10], [5, 0], [7, 0], [7, 0x10]]],
              [1, 1, 7, [[7, 0x10]]],
              [1, 1, 7, [[7, 0]]],
              [0, 3, 7, [[7, 0x10], [7, 0x10], [7, 0x10]]],
              [1, 3, 7, [[7, 0x10], [7, 0x10], [7, 0x10]]],
              [1, 3, 7, [[7, 0], [7, 0], [7, 0]]],
              [1, 3, 7, [[1, 0], [2, 0x10], [7, 0], [9, 0], [7, 0x10], [7, 0x10]]],
              [1, 5, 4, [[4, 0], [4, 0], [8, 0x10], [4, 0], [4, 0x13], [4, 0], [4, 0x10]]],
              [1, 2, 4, [[4, 0x01], [4, 0x20]]],
              [1, 3, 4, [[4, 0x21], [4, 0x0f], [4, 0x10]]],
              [1, 0, 7, [[7, 0x10]]],
              [-1, 2, 7, [[7, 0], [7, 0x10]]],
              [1, 3, 7, null]];
  function movieFixture() {
    const fx = {};
    __freeze.thread(); negOn();
    MV.forEach(function (r, i) {
      const [en, count, type, recs] = r;
      const m = alloc(0xa00, 0x5a5a5a5a);
      m.add(0xbc).writeS32(en);
      m.add(0x938).writeS32(count);
      m.add(0x1a4).writeS32(type);
      m.add(0x94c).writeU32(0x77777777);
      if (recs === null) {
        m.add(0x948).writePointer(ptr(0));           // object 12: enabled, NULL array
      } else {
        const arr = alloc(0x10 * (recs.length + 1), 0x6f6f6f6f);
        recs.forEach(function (rc, k) {
          arr.add(k * 0x10).writeS32(rc[0]);
          arr.add(k * 0x10 + 4).writeU32(rc[1]);
        });
        m.add(0x948).writePointer(arr);
        fx['mva' + i] = arr;
      }
      fx['mv' + i] = m;
    });
    return fx;
  }

  // ---- listCurrentIndex -----------------------------------------------------------------------------------------------
  // Objects of 0x3000 bytes (pre-filled 0x5a5a5a5a). Byte +4 bit 2 picks the list: set -> the model at V + d + 0x1548 with
  // d = [[V+0x1488]+8]; clear -> the model at V + d + 0x2e58 with d = [[V+0x2d98]+8]. Model: +8 head, +0xc cursor, +0x10
  // count, +0x14 index. Nodes (0x20): +4 key, +8 item (0x10 bytes, +4 the value returned), +0xc next, +0x10 prev.
  //   list A?  d     keys          cursor count  values
  const LC = [[1, 0x00, [1, 2, 3], 1, null, [10, 20, 30]],
              [1, 0x40, [4, 5, 6, 7], 3, null, [41, 42, 43, 44]],
              [1, 0x00, [8, 8, 9], 1, null, [51, 52, 53]],
              [1, 0x80, [3, 4, 5], 2, 0, [61, 62, 63]],
              [1, 0x00, [6, 7, 9, 10], 3, 2, [71, 72, 73, 74]],
              [1, 0x00, [2], 0, null, [81]],
              [1, 0x20, [5, 5, 5], 2, null, [91, 92, 93]],
              [0, 0x00, [1, 2], 1, null, [101, 102]],
              [0, 0x80, [3], 0, null, [111]],
              [0, 0x40, [4, 5, 6], 2, null, [121, 122, 123]],
              [1, 0x100, [11, 12, 13, 14, 15], 4, null, [131, 132, 133, 134, 135]],
              [0, 0x00, [7, 7], 1, 1, [141, 142]]];
  function listCurrentFixture() {
    const fx = {};
    __freeze.thread(); negOn();
    LC.forEach(function (r, i) {
      const [isA, d, keys, cur, cnt, vals] = r;
      const v = alloc(0x3000, 0x5a5a5a5a);
      v.add(4).writeU32(isA ? 4 : 0);
      const vbt = alloc(0x20, 0x6e6e6e6e);
      vbt.add(8).writeS32(d);
      v.add(isA ? 0x1488 : 0x2d98).writePointer(vbt);
      const m = v.add(d + (isA ? 0x1548 : 0x2e58));
      const nodes = keys.map(function (k, j) {
        const n = alloc(0x20, 0x7e7e7e7e);
        n.add(4).writeS32(k);
        const it = alloc(0x10, 0x7f7f7f7f);
        it.add(4).writeS32(vals[j]);
        n.add(8).writePointer(it);
        return n;
      });
      nodes.forEach(function (n, j) {
        n.add(0xc).writePointer(nodes[(j + 1) % nodes.length]);
        n.add(0x10).writePointer(nodes[(j + nodes.length - 1) % nodes.length]);
      });
      m.add(8).writePointer(nodes[0]);
      m.add(0xc).writePointer(nodes[cur]);
      m.add(0x10).writeS32(cnt === null ? nodes.length : cnt);
      m.add(0x14).writeS32(0x99);
      fx['lc' + i] = v;
    });
    return fx;
  }

  // ---- Snd::start and SndStream::open484c20 ----------------------------------------------------------------------------
  // createSound4840e0 returns 1 when the dword 0x0083afc4 (sound library loaded) is 0 and otherwise calls the cdecl
  // function in the table slot 0x0083af6c with its three arguments; the fixture saves both and installs a recording stub
  // (arguments 1..3 recorded) that stores the 'created' sound object into *arg1 and returns 0 ('ok'), or returns 5
  // without storing ('fail'). Sound objects (0x40 bytes) have vtable slots +0x6c (1 argument) and +0x48 (1) recording and
  // +0x84 returning [obj+0x30]. The Snd objects (0x100 bytes): +0x30 flag, +0x40 sound object, +0x44 flags, +0x50 name,
  // +0x58 byte flags, +0xf0 / +0xf4 the values slots +0x58 / +0x70 return; vtable slots +0x58, +0x70 return those fields
  // and +0x7c records.
  //   name, sound object, flags44, flags58, v58, v84, f30, v70
  const SN = [[0, 1, 0, 1, 7, 0, 1, 3],
              [5, 1, 0, 1, 7, 0, 1, 4],
              [5, 1, 1, 0, 0, 0, 1, 0],
              [5, 1, 0, 0, 0, 9, 1, 8],
              [5, 1, 0, 1, 0, 0, 0, 2],
              [6, 0, 0, 0, 1, 0, 1, 5],
              [6, 0, 1, 1, 0, 0, 0, 0],
              [0, 0, 0, 0, 0, 0, 1, 6],
              [9, 0, 0, 1, 3, 0, 1, 7],
              [5, 1, 3, 3, 2, 4, 0, 1]];
  function sndFixture(mode) {
    const fx = {};
    const emit = codeArena();
    const rec = newRec(0x800);
    fx.rec = rec; fx.obj = rec;
    const vtS = vtable(emit, rec, G_DRAG, [[0x58, 0x5D580000, 0, 0xf0], [0x70, 0x5D700000, 0, 0xf4], [0x7c, 0x5D7C0000, 0]]);
    const vtO = vtable(emit, rec, G_DRAG, [[0x6c, 0x5D6C0000, 1], [0x84, 0x5D840000, 0, 0x30], [0x48, 0x5D480000, 1]]);
    const made = alloc(0x40, 0x5a5a5a5a);
    made.writePointer(vtO);
    made.add(0x30).writeS32(0);
    fx.made = made;
    SN.forEach(function (r, i) {
      const [name, hasSnd, f44, f58, v58, v84, f30, v70] = r;
      const o = alloc(0x100, 0x5a5a5a5a);
      o.writePointer(vtS);
      o.add(0x30).writeS32(f30);
      if (hasSnd) {
        const so = alloc(0x40, 0x5b5b5b5b);
        so.writePointer(vtO);
        so.add(0x30).writeS32(v84);
        o.add(0x40).writePointer(so);
        fx['so' + i] = so;
      } else {
        o.add(0x40).writePointer(ptr(0));
      }
      o.add(0x44).writeU32(f44);
      o.add(0x50).writeS32(name);
      o.add(0x58).writeU32(f58);
      o.add(0xf0).writeS32(v58);
      o.add(0xf4).writeS32(v70);
      fx['sn' + i] = o;
    });
    __freeze.thread(); negOn();
    __freeze.save(ptr(0x0083afc4), 4);
    __freeze.save(ptr(0x0083af6c), 44);
    if (mode === 'nolib') {
      ptr(0x0083afc4).writeU32(0);
    } else {
      ptr(0x0083afc4).writeU32(1);
      const r = u32(rec.toUInt32());
      const b = [0xA1, ...r, 0x05, ...r, 0xC7, 0x00, ...u32(0x5E000000)];
      for (let k = 1; k <= 3; k++) b.push(0x8B, 0x54, 0x24, 4 * k, 0x89, 0x50, 4 * k);
      b.push(0x81, 0x05, ...r, ...u32(16));
      if (mode === 'ok') {
        b.push(0x8B, 0x54, 0x24, 0x04, 0xC7, 0x02, ...u32(made.toUInt32()), 0xB8, ...u32(0));
      } else {
        b.push(0xB8, ...u32(5));
      }
      b.push(0xC3);
      ptr(0x0083af6c).writePointer(emit(b));
    }
    return fx;
  }

  // ---- netListRemove --------------------------------------------------------------------------------------------------
  // Lists (0x40 bytes, pre-filled 0x5a5a5a5a): +0 vtable (slot +4 records the item pointer), +8 head, +0xc cursor, +0x10
  // count, +0x14 index. Nodes (0x60 bytes): +0 vtable (slot 0 records with the argument 1; the dword at +4 is the
  // virtual-base offset 0 or 0x30; at +0x30 a vptr to the same vtable), +4 key, +8 item or NULL, +0xc next, +0x10 prev.
  // Items (0x20 bytes): vtable slot 0 records (argument 1).
  //   keys, cursor index, offset, count override, items present flags
  const NL = [[[5, 6, 7], 1, 0, null, [1, 1, 1]],
              [[5, 6, 7], 2, 0, null, [1, 0, 1]],
              [[9], 0, 0, null, [1]],
              [[3, 4], 0, 0, 0, [1, 1]],
              [[], 0, 0, null, []],
              [[1, 2, 3, 4], 1, 0x30, null, [0, 1, 1, 0]],
              [[8, 9, 10], 0, 0, null, [1, 1, 1]],
              [[2, 2, 3], 0, 0x30, null, [1, 1, 0]],
              [[6, 7], 1, 0, null, [0, 0]]];
  function nodeListFixture() {
    const fx = {};
    __freeze.thread(); negOn();
    const emit = codeArena();
    const rec = newRec(0x800);
    fx.rec = rec; fx.obj = rec;
    const vtL = vtable(emit, rec, G_DRAG, [[0x4, 0x7C040000, 1]]);
    const vtI = vtable(emit, rec, G_DRAG, [[0x0, 0x7C050000, 1]]);
    const vtN0 = vtable(emit, rec, G_DRAG, [[0x0, 0x7C060000, 1]]);
    vtN0.add(4).writeU32(0);
    const vtN30 = vtable(emit, rec, G_DRAG, [[0x0, 0x7C070000, 1]]);
    vtN30.add(4).writeU32(0x30);
    NL.forEach(function (r, g) {
      const [keys, cur, off, cnt, items] = r;
      const l = alloc(0x40, 0x5a5a5a5a);
      l.writePointer(vtL);
      const nodes = keys.map(function (k, i) {
        const n = alloc(0x60, 0x4e4e4e4e);
        n.writePointer(off ? vtN30 : vtN0);
        if (off) n.add(off).writePointer(vtN30);
        n.add(4).writeS32(k);
        if (items[i]) {
          const it = alloc(0x20, 0x4f4f4f4f);
          it.writePointer(vtI);
          n.add(8).writePointer(it);
          fx['nit' + g + '_' + i] = it;
        } else {
          n.add(8).writePointer(ptr(0));
        }
        fx['nn' + g + '_' + i] = n;
        return n;
      });
      nodes.forEach(function (n, i) {
        n.add(0xc).writePointer(nodes[(i + 1) % nodes.length]);
        n.add(0x10).writePointer(nodes[(i + nodes.length - 1) % nodes.length]);
      });
      l.add(8).writePointer(nodes.length ? nodes[0] : ptr(0));
      l.add(0xc).writePointer(nodes.length ? nodes[cur] : ptr(0));
      l.add(0x10).writeS32(cnt === null ? nodes.length : cnt);
      l.add(0x14).writeS32(0x22);
      fx['nl' + g] = l;
    });
    return fx;
  }

  // ---- radioSelect ----------------------------------------------------------------------------------------------------
  // Radio groups (0x100 bytes, pre-filled 0x5a5a5a5a): +4+4i the button pointers, +0x84 count, +0x88 selected key, +0x8c
  // flags, +0x90 data, +0x94 mute. A button (0x700 bytes) has its vtable slot +0x120 recording (no arguments), +0x130 the
  // shared parent (0x100 bytes; vtable slots +0xd8 (2 arguments), +0xe0 (2) and +0xdc (3) record) or NULL, +0x578 the
  // mode dword Button::setMode writes, +0x5e4 the dirty flag and +0x5e8 the key.
  //   keys, selected, flags, data, mute, dirty flags, parent present
  const RG = [[[10, 20, 30], 20, 0, 0x55, 0, [0, 1, 0], 1],
              [[10, 20, 30, 20], 20, 1, 0x56, 0, [0, 0, 0, 1], 1],
              [[5, 6], -1, 2, 0x57, 0, [0, 0], 1],
              [[7, 8, 9], 8, 3, 0x58, 0, [1, 0, 1], 1],
              [[1, 2], 2, 0, 0x59, 1, [1, 1], 1],
              [[], 3, 0, 0x5a, 0, [], 1],
              [[4, 4, 4], 4, 2, 0x5b, 0, [0, 0, 0], 1],
              [[11, 12], 99, 0, 0x5c, 1, [0, 1], 0],
              [[21, 22, 23], 21, 3, 0x5d, 0, [0, 0, 1], 1],
              [[31, 32, 33], 32, 1, 0x5e, 0, [1, 0, 0], 1]];
  function radioFixture() {
    const fx = {};
    __freeze.thread(); negOn();
    const emit = codeArena();
    const rec = newRec(0x800);
    fx.rec = rec; fx.obj = rec;
    const vtB = vtable(emit, rec, G_DRAG, [[0x120, 0x4B120000, 0]]);
    const vtP = vtable(emit, rec, G_DRAG, [[0xd8, 0x4BD80000, 2], [0xe0, 0x4BE00000, 2], [0xdc, 0x4BDC0000, 3]]);
    RG.forEach(function (r, g) {
      const [keys, cur, flags, data, mute, dirty, hasPar] = r;
      const par = alloc(0x100, 0x3b3b3b3b);
      par.writePointer(vtP);
      const grp = alloc(0x100, 0x5a5a5a5a);
      keys.forEach(function (k, i) {
        const b = alloc(0x700, 0x6a6a6a6a);
        b.writePointer(vtB);
        b.add(0x130).writePointer(hasPar ? par : ptr(0));
        b.add(0x578).writeS32(7);
        b.add(0x5e4).writeS32(dirty[i]);
        b.add(0x5e8).writeS32(k);
        grp.add(4 + 4 * i).writePointer(b);
        fx['rb' + g + '_' + i] = b;
      });
      grp.add(0x84).writeS32(keys.length);
      grp.add(0x88).writeS32(cur);
      grp.add(0x8c).writeU32(flags);
      grp.add(0x90).writeS32(data);
      grp.add(0x94).writeS32(mute);
      fx['rg' + g] = grp;
      fx['rp' + g] = par;
    });
    return fx;
  }

  // ---- scrollbarThumbRect ---------------------------------------------------------------------------------------------
  // Thumb objects (0x700 bytes, pre-filled 0x5a5a5a5a): +0x278 the child (0x80 bytes) on a vtable of PURE getters
  // (slot +0xd8 returns [child+0x40], slot +0xdc [child+0x44]; nothing is recorded, so the number of getter calls is not
  // part of the comparison) or NULL, +0x574 flags (bit 2), +0x57c border, +0x580 min, +0x584 max, +0x58c position, +0x59c
  // drag offset, +0x5ac the rectangle.
  //   bit2 border  min max pos  off   cw  ch  rect(l,t,r,b)
  const TH = [[2, -1, 0, 10, 4, -1, 100, 50, [10, 20, 30, 40]],      // A: bit 2, no border, wide child
              [0, -1, 0, 10, 4, -1, 100, 50, [10, 20, 30, 40]],      // A: bit 2 clear
              [2, 3, 0, 10, 4, -1, 100, 50, [10, 20, 30, 40]],       // A: bit 2, border
              [0, 3, 0, 10, 4, -1, 100, 50, [10, 20, 30, 40]],       // A: bit 2 clear, border
              [2, -1, 0, 10, 4, -1, 30, 90, [10, 20, 30, 40]],       // A: tall child
              [0, 7, 5, 25, -3, -1, 30, 90, [0, 0, 16, 60]],         // A: tall child, bit 2 clear, border, other range
              [2, -1, 7, 7, 7, -1, 100, 50, [10, 20, 30, 40]],       // A: min == max, wide
              [0, -1, 7, 7, 7, -1, 30, 90, [10, 20, 30, 40]],        // A: min == max, tall
              [2, -1, 0, 10, 4, -1, 0, 0, [10, 20, 30, 40]],         // A: no child (extents 0, not wide)
              [0, 3, -4, 20, 9, -1, 60, 60, [3, 5, 9, 17]],          // A: equal extents (not wide)
              [2, -1, 0, 10, 4, -5, 100, 50, [10, 20, 30, 40]],      // B: bit 2, offset -5 -> 5 (inside)
              [2, -1, 0, 10, 4, 90, 100, 50, [10, 20, 30, 40]],      // B: bit 2, offset 90 -> 100 (clamped to 80)
              [2, -1, 0, 10, 4, -50, 100, 50, [10, 20, 30, 40]],     // B: bit 2, offset -50 -> -40 (raised to 0)
              [0, -1, 0, 10, 4, -5, 100, 50, [10, 20, 30, 40]],      // B: bit 2 clear, 5 -> raised to 20
              [0, -1, 0, 10, 4, 25, 100, 50, [10, 20, 30, 40]],      // B: bit 2 clear, 35 (inside)
              [0, -1, 0, 10, 4, 90, 100, 50, [10, 20, 30, 40]],      // B: bit 2 clear, 100 -> 60
              [2, 3, 0, 10, 4, 0, 30, 90, [10, 20, 30, 40]],         // B: bit 2, border, tall child, 10 -> 10
              [2, -1, 0, 10, 4, 200, 30, 90, [10, 20, 30, 40]],      // B: bit 2, tall child, 210 -> 70
              [2, -1, 0, 10, 4, -60, 30, 90, [10, 20, 30, 40]],      // B: bit 2, tall child, negative -> 0
              [0, 3, 0, 10, 4, 3, 30, 90, [10, 20, 30, 40]],         // B: bit 2 clear, border, tall child, raised
              [0, -1, 0, 10, 4, 40, 30, 90, [10, 20, 30, 40]],       // B: bit 2 clear, tall child, 50 (inside)
              [0, -1, 0, 10, 4, 300, 30, 90, [10, 20, 30, 40]],      // B: bit 2 clear, tall child, clamped to 50
              [2, -1, 0, 10, 4, 5, 0, 0, [10, 20, 30, 40]],          // B: bit 2, no child (extent 0): negative -> 0
              [0, -1, 0, 10, 4, 5, 0, 0, [10, 20, 30, 40]],          // B: bit 2 clear, no child
              [2, 3, 0, 10, 4, 30, 60, 60, [3, 5, 9, 17]]];          // B: bit 2, equal extents
  function thumbFixture() {
    const fx = {};
    __freeze.thread(); negOn();
    const emit = codeArena();
    const vtC = alloc(0x200);
    const nul = emit([0x33, 0xC0, 0xC3]);
    for (let i = 0; i < 0x80; i++) vtC.add(i * 4).writePointer(nul);
    vtC.add(0xd8).writePointer(emit([0x8B, 0x81, ...u32(0x40), 0xC3]));
    vtC.add(0xdc).writePointer(emit([0x8B, 0x81, ...u32(0x44), 0xC3]));
    TH.forEach(function (r, i) {
      const [bit2, border, mn, mx, pos, off, cw, ch, rc] = r;
      const t = alloc(0x700, 0x5a5a5a5a);
      if (cw === 0 && ch === 0) {
        t.add(0x278).writePointer(ptr(0));
      } else {
        const c = alloc(0x80, 0x2c2c2c2c);
        c.writePointer(vtC);
        c.add(0x40).writeS32(cw);
        c.add(0x44).writeS32(ch);
        t.add(0x278).writePointer(c);
        fx['thch' + i] = c;
      }
      t.add(0x574).writeU32(bit2);
      t.add(0x57c).writeS32(border);
      t.add(0x580).writeS32(mn);
      t.add(0x584).writeS32(mx);
      t.add(0x58c).writeS32(pos);
      t.add(0x59c).writeS32(off);
      rc.forEach(function (v, k) { t.add(0x5ac + 4 * k).writeS32(v); });
      fx['th' + i] = t;
    });
    fx.out = alloc(0x20, 0x3d3d3d3d);
    return fx;
  }

  // ---- stampCourseTiles ----------------------------------------------------------------------------------------------
  // Every table the function and rebuildHeightfield 0x0042f7a0 read or write is saved (written back before the thread
  // resumes) and seeded with a pattern; rebuildHeightfield's own seeds follow c3ah's clearObjectFootprint fixture: the
  // tile-type records at 0x00578370 (0x30 each) get a zero behaviour dword at +0xc so its convergence loop ends after one
  // pass, the corner cache 0x0051b770 holds 1..7. The footprint side bytes (0x004c26c0 + kind*0x14) and the extra-size
  // dwords (0x005a8c38 + kind*4) are our own values, as in c3ah.
  //   list: 'empty' (every record free), 'one' (record 0 used), 'some' (records 0..2 used), 'full' (all 256 used and the
  //   word after the table is -1, so the scan stops at index 0x100)
  //   ct: the course-type byte 0x005a34e0
  const SIDES = { 0: 1, 3: 2, 5: 2, 6: 1, 7: 3, 10: 4, 12: 2, 15: 2, 16: 0, 17: 0 };
  function stampFixture(list, ct, rngSeed) {
    const fx = {};
    __freeze.thread(); negOn();
    const regions = [[0x0053caf0, 5000], [0x005722e8, 2564], [0x0056988c, 2500], [0x0053bbac, 2500], [0x005a4998, 2700],
                     [0x0051b770, 20000], [0x00543018, 2500], [0x005619a0, 2500], [0x00578370, 0x26 * 0x30],
                     [0x004c26c0, 0x26 * 0x14], [0x005a8c38, 0x26 * 4], [0x0058bcb8, 0x1010], [0x00822d9c, 4],
                     [0x00585860, 2], [0x005a34e0, 4], [0x005a34f0, 4], [0x00578795, 4], [0x0059e7b8, 4], [0x004c2e04, 4]];
    regions.forEach(function (r) { __freeze.save(ptr(r[0]), r[1]); });
    for (let i = 0; i < 2500; i++) {
      ptr(0x0053caf0).add(i * 2).writeU16((0xffff ^ (i * 7)) & 0xffff);
      ptr(0x005722e8).add(i).writeU8((i * 11) % 0x24);       // tile types stay inside the 0x26-entry type table
      ptr(0x0056988c).add(i).writeU8((i * 5) & 0xff);
      ptr(0x0053bbac).add(i).writeU8((i * 3) & 0xff);
    }
    for (let i = 0; i < 2700; i++) ptr(0x005a4998).add(i).writeU8((i * 13 + 5) & 0xff);
    const ch = ptr(0x0051b770);
    for (let i = 0; i < 2500; i++)
      for (let k = 0; k < 8; k++) ch.add(i * 8 + k).writeS8(((i * 3 + k * 5) % 7) + 1);
    Memory.protect(ptr(0x00578370), 0x26 * 0x30, 'rw-');
    for (let t = 0; t < 0x26; t++) {
      ptr(0x0057837c).add(t * 0x30).writeU32(0);
      ptr(0x00578372).add(t * 0x30).writeS8((t % 5) - 1);
      ptr(0x00578373).add(t * 0x30).writeS8((t % 7) + 1);
      ptr(0x00578376).add(t * 0x30).writeS8(t % 3);
      ptr(0x00578377).add(t * 0x30).writeS8(t % 4);
    }
    Memory.protect(ptr(0x004c26c0), 0x26 * 0x14, 'rw-');
    Memory.protect(ptr(0x005a8c38), 0x26 * 4, 'rw-');
    for (let t = 0; t < 0x26; t++) {
      ptr(0x004c26c0).add(t * 0x14).writeS8(SIDES[t] === undefined ? 1 : SIDES[t]);
      ptr(0x005a8c38).add(t * 4).writeS32(t === 16 ? 1 : t === 17 ? 0 : (t % 3) + 1);
    }
    const po = ptr(0x0058bcb8);
    for (let i = 0; i < 0x101; i++) {
      const used = list === 'full' ? i < 256 : list === 'some' ? i < 3 : list === 'one' ? i < 1 : false;
      po.add(i * 0x10).writeS16(used ? i % 7 : -1);
    }
    ptr(0x00822d9c).writeU32(rngSeed);
    ptr(0x00585860).writeU16(0x1111);
    ptr(0x005a34e0).writeU8(ct);
    ptr(0x005a34f0).writeU8(0x2f);
    ptr(0x00578795).writeU8(0x5c);
    ptr(0x0059e7b8).writeU32(2);
    return fx;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3dx_stamp_empty() { return stampFixture('empty', 0, 0x12345678); },
    c3dx_stamp_one() { return stampFixture('one', 0, 0x0badcafe); },
    c3dx_stamp_some() { return stampFixture('some', 0, 0x31415926); },
    c3dx_stamp_full() { return stampFixture('full', 0, 0x27182818); },
    c3dx_stamp_ct1() { return stampFixture('empty', 1, 0x01020304); },
    c3dx_stamp_ct2() { return stampFixture('some', 2, 0x0a0b0c0d); },
    c3dx_snap0() { return snapshotFixture(0); },
    c3dx_snap1() { return snapshotFixture(1); },
    c3dx_snap2() { return snapshotFixture(2); },
    c3dx_snap3() { return snapshotFixture(3); },
    c3dx_snap9() { return snapshotFixture(9); },
    c3dx_snap8() { return snapshotFixture(8); },
    c3dx_snap7() { return snapshotFixture(7); },
    c3dx_snap6() { return snapshotFixture(6); },
    c3dx_snap5() { return snapshotFixture(5); },
    c3dx_snap4() { return snapshotFixture(4); },
    c3dx_records0() { return recordsFixture(0); },
    c3dx_records1() { return recordsFixture(1); },
    c3dx_records2() { return recordsFixture(2); },
    c3dx_selectbyid() { return selectByIdFixture(); },
    c3dx_dtorlist() { return dtorListFixture(); },
    c3dx_layout() { return layoutFixture(); },
    c3dx_slot60() { return slot60Fixture(); },
    c3dx_listpanel() { return listPanelFixture(); },
    c3dx_movie() { return movieFixture(); },
    c3dx_listcur() { return listCurrentFixture(); },
    c3dx_snd_ok() { return sndFixture('ok'); },
    c3dx_snd_fail() { return sndFixture('fail'); },
    c3dx_snd_nolib() { return sndFixture('nolib'); },
    c3dx_nodelist() { return nodeListFixture(); },
    c3dx_radio() { return radioFixture(); },
    c3dx_thumb() { return thumbFixture(); },
    c3dx_sb_free() { return scrollFixture('free'); },
    c3dx_sb_held() { return scrollFixture('held'); },
    c3dx_sb_free2() { return scrollFixture('free', 0); },
    c3dx_text() { return textFixture(); },
    c3dx_status_low() { return statusFixture(-128, 2); },
    c3dx_status_mid() { return statusFixture(5, 1); },
    c3dx_status_high() { return statusFixture(100, 0); },
    c3dx_status_one() { return statusFixture(-1, 3); },
  });
})();

// ====================================================================================================================
// Second block of fixtures: ListModel::append, TextView::addText, ListBox::addItem and sendCommand all end in S3List_add
// 0x00401d10 (C3, c3dd), which allocates nodes and payloads through heapAlloc / malloc. This block follows c3dd's method:
// the heap stand-in (re/frida/js/heapstandin.js) over golf_clean.exe's malloc 0x004a504f, operator new 0x004a57f1, free
// 0x004a5007 and _realloc 0x004a6980 on the A/B thread only, an arena whose header (cursor, call counter, fail mask) is
// state, every object carved from one block $blk (state), recording stubs, and the global 0x00839650 saved and cleared
// under the shared freeze helper. Own scope: nothing here is shared with the first block.
//
// A list (the embedded S3List): +0 vptr (slot 0 records, argument 1), +4 vbptr -> table whose entry +4 is 0x40, +8 head,
// +0xc cursor, +0x10 count, +0x14 last, +0x18 heap pointer (S0Heap, or NULL = malloc), +0x48 the virtual base's heap pointer.
// An S0Heap: +4 flag byte, +8 block, +0xc bump pointer, +0x10 capacity, +0x14 free count.
// ====================================================================================================================
(function () {
  const BLK = 0x20000;
  const ARENA = 0x8000;
  const keep = (x) => { globalThis.__diffKeepAlive.push(x); return x; };
  function u32(v) { v >>>= 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; }
  function fill32(p, n, v) { for (let i = 0; i + 4 <= n; i += 4) p.add(i).writeU32(v >>> 0); }

  // The negative-control build (log/c3/alt/c3dx_negctl_gen.py) keeps its wrong bodies dormant until its exported NegCtl switch is set;
  // in the normal build there is no such export and nothing is written. Always called right after the first thread froze.
  function negOn() {
    __freeze.negOn();   // every exported <Batch>NegCtl switch (re/frida/js/freeze.js)
  }
  function codeArena() {
    const size = Process.pageSize * 16;
    const base = keep(Memory.alloc(size));
    Memory.protect(base, size, 'rwx');
    let off = 0;
    return function (bytes) {
      const p = base.add(off);
      p.writeByteArray(bytes);
      off += (bytes.length + 15) & ~15;
      if (off > size) throw new Error('c3dx: list code arena full');
      return p;
    };
  }
  // A recording stub: appends {tag, ecx or 0xC0C0C0C0, stack args} to rec and returns rec-cursor ^ tag.
  function stubBytes(rec, tag, n, thiscall, retOff) {
    const r = u32(rec.toUInt32());
    const b = [];
    b.push(0xA1, ...r, 0x05, ...r, 0xC7, 0x00, ...u32(tag));
    if (thiscall) b.push(0x89, 0x48, 0x04);
    else b.push(0xC7, 0x40, 0x04, ...u32(0xC0C0C0C0));
    for (let i = 1; i <= n; i++) b.push(0x8B, 0x54, 0x24, 4 * i, 0x89, 0x50, 4 + 4 * i);
    b.push(0x81, 0x05, ...r, ...u32(4 * (n + 2)));
    if (retOff === undefined) b.push(0xA1, ...r, 0x35, ...u32(tag));      // eax = cursor ^ tag
    else b.push(0x8B, 0x81, ...u32(retOff));                             // eax = [ecx + retOff]
    if (thiscall && n > 0) b.push(0xC2, 4 * n, 0x00); else b.push(0xC3);
    return b;
  }

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
      if (cur > BLK) throw new Error('c3dx: list object block full');
      if (v !== undefined) fill32(p, (n + 3) & ~3, v);
      return p;
    }
    const rec = keep(Memory.alloc(0x400));
    fill32(rec, 0x400, 0);
    rec.writeU32(0x10);
    const nul = emit([0x33, 0xC0, 0xC3]);
    function vt(spec) {
      const t = keep(Memory.alloc(0x200));
      for (let i = 0; i < 0x80; i++) t.add(i * 4).writePointer(nul);
      for (const [slot, tag, n, retOff] of spec) t.add(slot).writePointer(emit(stubBytes(rec, tag, n, true, retOff)));
      return t;
    }
    fx.blk = blk; fx.rec = rec; fx.obj = blk;
    const h = __heapStandIn({
      module: 'golf_clean.exe',
      alloc: [0x004a504f - 0x400000, 0x004a57f1 - 0x400000],
      free: [0x004a5007 - 0x400000],
      realloc: [0x004a6980 - 0x400000],
      arenaSize: ARENA,
      failMask: opts.failMask || 0,
      exhaust: 'null',
    });
    fx.arena = h.arena; fx.freelog = h.log;
    return { fx: fx, take: take, vt: vt, rec: rec, h: h, n: 0, emit: emit, str: (t) => keep(Memory.allocUtf8String(t)) };
  }

  function mkHeap(E, o) {
    const hp = E.take(0x20, 0x48480000 + E.n);
    const span = o.cap + 0x800;
    const data = E.take(span, 0x44440000 + E.n);
    E.n++;
    hp.add(4).writeU8(o.flags || 0);
    hp.add(8).writePointer(data);
    E.h.register(data, o.cap);
    hp.add(0xc).writePointer(data.add(o.cap > o.free ? o.cap - o.free : 0));
    hp.add(0x10).writeS32(o.cap);
    hp.add(0x14).writeS32(o.free);
    return hp;
  }

  // Fills the list object at L in place. o = {count, heap, vheap}.
  function fillList(E, L, o) {
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
    E.n++;
    nodes.forEach(function (nd, k) {
      nd.add(0xc).writePointer(nodes[(k + 1) % n]);
      nd.add(0x10).writePointer(nodes[(k + n - 1) % n]);
    });
    L.add(8).writePointer(n ? nodes[0] : ptr(0));
    L.add(0xc).writePointer(n ? nodes[n - 1] : ptr(0));
    L.add(0x10).writeS32(n);
    L.add(0x14).writeS32(-1);
  }

  // shapes (count, heap kind): kinds 0 malloc (no heap), 1 direct heap, 2 virtual-base heap
  const SHAPES = [[0, 0], [0, 1], [0, 2], [1, 0], [1, 1], [1, 2], [2, 0], [3, 1], [4, 2], [2, 2], [3, 0], [5, 1]];

  function begin(opts, tag) {
    const E = env(opts);
    E.lvt = E.vt([[0, tag, 1]]);
    const fx = E.fx;
    fx.hf = mkHeap(E, { cap: 0x400, free: 0x400 });
    fx.hv = mkHeap(E, { cap: 0x400, free: 0x400 });
    fx.hs = mkHeap(E, { cap: 0x400, free: 0x30 });
    fx.hz = mkHeap(E, { cap: 0x400, free: 0 });
    const texts = ['plain text', '^marked', '^^double', '^^^triple', '^', '^^'];
    texts.forEach(function (t, i) { fx['t' + i] = E.str(t); });
    return E;
  }
  function heapsOf(fx, sp, i) {
    return {
      count: sp[0],
      heap: sp[1] === 1 ? (i % 2 ? fx.hs : fx.hf) : null,
      vheap: sp[1] === 2 ? (i % 2 ? fx.hz : fx.hv) : null,
    };
  }
  function finish(E, extraGlobals) {
    __freeze.thread(); negOn();
    __freeze.save(ptr(0x00839650), 4);
    ptr(0x00839650).writeU32(0);
    (extraGlobals || []).forEach(function (g) { __freeze.save(ptr(g[0]), 4); ptr(g[0]).writeU32(g[1] >>> 0); });
    return E.fx;
  }

  // ListModel objects: the list at +0xc0.
  function listModels(opts) {
    const E = begin(opts, 0xD0600000);
    SHAPES.forEach(function (sp, i) {
      const lm = E.take(0x200, 0x4c4d0000 + i);
      fillList(E, lm.add(0xc0), heapsOf(E.fx, sp, i));
      E.fx['lm' + i] = lm;
    });
    return finish(E);
  }

  // TextView objects: the list at +0x1410.
  function textViews(opts) {
    const E = begin(opts, 0xD0610000);
    SHAPES.forEach(function (sp, i) {
      const tv = E.take(0x1500, 0x54560000 + i);
      fillList(E, tv.add(0x1410), heapsOf(E.fx, sp, i));
      E.fx['tv' + i] = tv;
    });
    return finish(E);
  }

  // List boxes: S (0x800 bytes): S+4 the virtual-base table (entry +4 = dw, entry +8 = dm), S+0x30 the running weight,
  // S+0x1f4 the kind sendCommand switches on, W = S + dw: W+0x294 the row weight the wide rows add; M = S + dm: M+0x34
  // the wrap width (room = M+0x34 - [0x0083ff10]; the global is set to 0x40), the list at M+0xc4 (so M+0xe0 is its +0x1c).
  //   shape, dw, dm, width delta (room), kind
  const LB = [[0, 0x100, 0x400, 0, 1], [1, 0x140, 0x440, -8, 2], [3, 0x100, 0x400, 0, 2], [4, 0x140, 0x440, -2, 4],
              [5, 0x100, 0x400, 0, 8], [6, 0x140, 0x440, -1, 16], [7, 0x100, 0x400, -30, 2], [8, 0x140, 0x440, 0, 3],
              [9, 0x100, 0x400, -5, 0], [10, 0x140, 0x440, 0, 5], [11, 0x100, 0x400, -9, 17], [2, 0x140, 0x440, -3, 2]];
  const G_WIDTH = 0x40;
  function listBoxes(opts) {
    const E = begin(opts, 0xD0620000);
    LB.forEach(function (r, i) {
      const [shape, dw, dm, delta, kind] = r;
      const s = E.take(0x800, 0x4c420000 + i);
      const vbt = E.take(0x10, 0);
      vbt.add(4).writeS32(dw);
      vbt.add(8).writeS32(dm);
      s.add(4).writePointer(vbt);
      s.add(0x30).writeS32(i);
      s.add(0x1f4).writeS32(kind);
      s.add(dw + 0x294).writeS32(3 + (i % 4));
      s.add(dm + 0x34).writeS32(G_WIDTH + delta);
      fillList(E, s.add(dm + 0xc4), heapsOf(E.fx, SHAPES[shape], i));
      E.fx['lb' + i] = s;
    });
    return finish(E, [[0x0083ff10, G_WIDTH]]);
  }

  // Bitmaps for flipBmpRows: a header (+4 width, +8 height, +0xe bit count) and a pixel buffer filled with a pattern, three
  // bytes per pixel wide plus 32 guard bytes of 0xee that a wrong row count would disturb. operator new for the scratch row
  // and the free behind operator delete are the heap stand-in's (the free log is state).
  //   width, height, bit count
  const IMGS = [[1, 2, 8], [2, 2, 8], [3, 3, 8], [4, 4, 24], [3, 5, 24], [2, -2, 8], [2, -3, 24], [5, 6, 16], [3, 4, 32],
                [0, 3, 8], [4, 1, 8], [4, 0, 8], [2, 7, 24], [6, 8, 8], [1, -5, 24], [3, -1, 24], [5, 2, 24], [3, 6, 24]];
  function bitmaps(opts) {
    const E = begin(opts, 0xD0630000);
    IMGS.forEach(function (r, i) {
      const [w, h, bc] = r;
      const hdr = E.take(0x20, 0x68640000 + i);
      hdr.add(4).writeS32(w);
      hdr.add(8).writeS32(h);
      hdr.add(0xe).writeU16(bc);
      const used = Math.abs(h) * w * 3;
      const px = E.take(used + 32, 0xeeeeeeee);
      for (let k = 0; k < used; k++) px.add(k).writeU8((k * 5 + 7 + i) & 0xff);
      E.fx['bh' + i] = hdr;
      E.fx['bp' + i] = px;
    });
    return finish(E);
  }

  // Objects whose name pointer TextView::setName (+0x5c8, malloc / free) and Snd::setName (+0x50, operator new / delete) replace:
  // the odd TextViews and every third Snd already own a (fixture-made) block, the others hold NULL.
  function nameObjs(opts) {
    const E = begin(opts, 0xD0640000);
    const fx = E.fx;
    fx.t6 = E.str('');
    fx.t7 = E.str('w'.repeat(70));
    for (let i = 0; i < 6; i++) {
      const tv = E.take(0x600, 0x74760000 + i);
      tv.add(0x5c8).writePointer(i % 2 ? E.take(0x20, 0x4f4f4f4f) : ptr(0));
      fx['sv' + i] = tv;
      const sn = E.take(0x100, 0x73740000 + i);
      sn.add(0x50).writePointer(i % 3 === 1 ? E.take(0x20, 0x4e4e4e4e) : ptr(0));
      fx['sd' + i] = sn;
    }
    return finish(E);
  }

  // Flic objects for Flic::indexFrames (0x100 bytes each): +0x44 the byte offset of the first chunk (0 = no stream, result 7),
  // +0x48 / +0x4c the two tables (the odd objects already own a fixture-made one), +0x58 a block whose +0x50 is the stream base.
  // The stream is 200 chunks of 16 or 32 bytes (size dword at +0, type word at +4): chunks with k % 3 == 2 or k % 5 == 0 are
  // frame chunks (0xf1fa), the others type 0xf1fb, so the walk skips non-frame chunks between frame chunks.
  function flics(opts) {
    const E = begin(opts, 0xD0650000);
    const stream = E.take(200 * 32 + 64, 0x77777777);
    const offs = [];
    let off = 0;
    for (let k = 0; k < 200; k++) {
      const size = (k % 4 === 1) ? 32 : 16;
      stream.add(off).writeS32(size);
      stream.add(off + 4).writeU16((k % 3 === 2 || k % 5 === 0) ? 0xf1fa : 0xf1fb);
      offs.push(off);
      off += size;
    }
    const starts = [offs[1], offs[2], offs[4], 0, offs[7], offs[3], offs[10], offs[6]];
    const base = E.take(0x80, 0x78780000);
    base.add(0x50).writePointer(stream);
    starts.forEach(function (st, i) {
      const f = E.take(0x100, 0x46460000 + i);
      f.add(0x44).writeS32(st);
      f.add(0x48).writePointer(i % 2 ? E.take(0x20, 0x4f4f4f4f) : ptr(0));
      f.add(0x4c).writePointer(i % 3 === 1 ? E.take(0x20, 0x4e4e4e4e) : ptr(0));
      f.add(0x58).writePointer(base);
      E.fx['fl' + i] = f;
    });
    return finish(E);
  }

  // SndMusic objects for SndMusic::setFile (0x100 bytes): +0x30 flag, +0x40 sound object, +0x44 flags, +0x50 the name (a
  // fixture-made block on the odd objects). Sound objects (0x40 bytes) have vtable slots +0x60 (returns [obj+0x38]), +0x10
  // (returns [obj+0x3c], 1 argument) and +0x48 (1 argument) recording; the SndMusic vtable has slot +0x7c recording. As for
  // Snd::start, createSound's table slot 0x0083af6c holds a recording stub that stores the 'made' sound object ('ok'), fails
  // with 5 ('fail') or the library flag 0x0083afc4 is 0 ('nolib').
  //   sound object present, v60, v10, flags44, flag30
  const SM = [[0, 0, 0, 0, 1], [1, 1, 0, 0, 1], [1, 0, 0, 0, 1], [1, 0, 5, 3, 1], [1, 0, 0, 1, 1], [1, 0, 0, 0, 0],
              [0, 0, 0, 2, 0], [0, 0, 0, 1, 1]];
  function musicObjs(mode) {
    const E = begin({}, 0xD0660000);
    const fx = E.fx;
    fx.t6 = E.str('');
    fx.t7 = E.str('w'.repeat(70));
    const vtM = E.vt([[0x7c, 0x7D7C0000, 0]]);
    const vtS = E.vt([[0x60, 0x7D600000, 0, 0x38], [0x10, 0x7D100000, 1, 0x3c], [0x48, 0x7D480000, 1]]);
    const made = E.take(0x40, 0x5a5a5a5a);
    made.writePointer(vtS);
    made.add(0x38).writeS32(0);
    made.add(0x3c).writeS32(0);
    fx.made = made;
    SM.forEach(function (r, i) {
      const [has, v60, v10, f44, f30] = r;
      const o = E.take(0x100, 0x5a5a5a5a);
      o.writePointer(vtM);
      o.add(0x30).writeS32(f30);
      if (has) {
        const so = E.take(0x40, 0x5b5b5b5b);
        so.writePointer(vtS);
        so.add(0x38).writeS32(v60);
        so.add(0x3c).writeS32(v10);
        o.add(0x40).writePointer(so);
      } else {
        o.add(0x40).writePointer(ptr(0));
      }
      o.add(0x44).writeU32(f44);
      o.add(0x50).writePointer(i % 2 ? E.take(0x20, 0x4f4f4f4f) : ptr(0));
      fx['sm' + i] = o;
    });
    __freeze.thread(); negOn();
    __freeze.save(ptr(0x0083afc4), 4);
    __freeze.save(ptr(0x0083af6c), 44);
    __freeze.save(ptr(0x00839650), 4);
    ptr(0x00839650).writeU32(0);
    if (mode === 'nolib') {
      ptr(0x0083afc4).writeU32(0);
    } else {
      ptr(0x0083afc4).writeU32(1);
      const r = u32(E.rec.toUInt32());
      const b = [0xA1, ...r, 0x05, ...r, 0xC7, 0x00, ...u32(0x5E000000)];
      for (let k = 1; k <= 3; k++) b.push(0x8B, 0x54, 0x24, 4 * k, 0x89, 0x50, 4 * k);
      b.push(0x81, 0x05, ...r, ...u32(16));
      if (mode === 'ok') b.push(0x8B, 0x54, 0x24, 0x04, 0xC7, 0x02, ...u32(made.toUInt32()), 0xB8, ...u32(0));
      else b.push(0xB8, ...u32(5));
      b.push(0xC3);
      ptr(0x0083af6c).writePointer(E.emit(b));
    }
    // $tnl: the name 'plain text'; resolving it clears the library flag 0x0083afc4 (saved above, put back at the flush), so
    // the last vectors of a key take the no-library arm (the function then returns 1 before touching anything).
    Object.defineProperty(fx, 'tnl', { enumerable: true, get: function () { ptr(0x0083afc4).writeU32(0); return fx.t0; } });
    return fx;
  }

  // Snd objects for Snd::configure / Snd::open / Snd::reopen (and through them Snd::setName, SndStream::open484c20 and
  // SndMusic::setFile as original callees). Snd object (0x100 bytes): +4 / +8 / +0x5c ints the sound object receives, +0x30 flag,
  // +0x40 sound object, +0x44 SndMusic flags, +0x50 the name, +0x58 the state dword, +0x64 written, +0xf0 the value slot +0x58
  // returns; its vtable has slots +0x58 (returns [this+0xf0]), +0x48 and +0x7c recording. Sound objects (0x40 bytes): slots
  // +0x6c, +0x10 (returns [obj+0x3c]), +0x48, +0x40, +0x9c, +0x44 take one argument and record, +0x60 returns [obj+0x38], +0xc8
  // returns [obj+0x34]. createSound's table slot 0x0083af6c and releaseSoundObj's 0x0083af70 hold recording stubs; the first
  // stores the 'made' sound object ('ok') or fails with 5 ('fail'); the library flag 0x0083afc4 is 1. Both slots are put back
  // at the flush.
  //   has sound, v60, v10, v58, state58, f44, f30, ret c8, name
  const SF = [[0, 0, 0, 0, 0x00, 0, 1, 0x11, 0],
              [0, 0, 0, 1, 0x79, 0, 1, 0x12, 1],
              [1, 0, 0, 0, 0x01, 0, 1, 0x13, 1],
              [1, 0, 0, 1, 0x02, 0, 0, 0x14, 1],
              [1, 1, 0, 0, 0x18, 3, 1, 0x15, 1],
              [1, 0, 5, 1, 0x4c, 1, 1, 0x16, 1],
              [0, 0, 0, 0, 0x7f, 0, 0, 0x17, 1],
              [1, 0, 0, 0, 0x08, 0, 1, 0x18, 0],
              [1, 0, 0, 1, 0x28, 2, 1, 0x19, 1],
              [0, 0, 0, 1, 0x04, 0, 1, 0x1a, 1],
              [1, 0, 0, 0, 0x20, 0, 1, 0x1b, 1],
              [1, 0, 0, 1, 0x10, 0, 0, 0x1c, 1]];
  function soundFamily(mode) {
    const E = begin({}, 0xD0670000);
    const fx = E.fx;
    fx.t6 = E.str('');
    fx.t7 = E.str('w'.repeat(70));
    const vtM = E.vt([[0x58, 0x7D580000, 0, 0xf0], [0x48, 0x7D480000, 1], [0x7c, 0x7D7C0000, 0]]);
    const vtS = E.vt([[0x6c, 0x7D6C0000, 1], [0x10, 0x7D100000, 1, 0x3c], [0x48, 0x7D480001, 1], [0x40, 0x7D400000, 1],
                      [0x9c, 0x7D9C0000, 1], [0x44, 0x7D440000, 1], [0x60, 0x7D600000, 0, 0x38], [0xc8, 0x7DC80000, 0, 0x34]]);
    const made = E.take(0x40, 0x5a5a5a5a);
    made.writePointer(vtS);
    made.add(0x34).writeS32(0x77);
    made.add(0x38).writeS32(0);
    made.add(0x3c).writeS32(0);
    fx.made = made;
    SF.forEach(function (r, i) {
      const [has, v60, v10, v58, st, f44, f30, c8, nm] = r;
      const o = E.take(0x100, 0x5a5a5a5a);
      o.writePointer(vtM);
      o.add(4).writeS32(0x1000 + i);
      o.add(8).writeS32(0x2000 + i);
      o.add(0x30).writeS32(f30);
      if (has) {
        const so = E.take(0x40, 0x5b5b5b5b);
        so.writePointer(vtS);
        so.add(0x34).writeS32(c8);
        so.add(0x38).writeS32(v60);
        so.add(0x3c).writeS32(v10);
        o.add(0x40).writePointer(so);
      } else {
        o.add(0x40).writePointer(ptr(0));
      }
      o.add(0x44).writeU32(f44);
      o.add(0x50).writePointer(nm ? fx.t0 : ptr(0));
      o.add(0x58).writeU32(st);
      o.add(0x5c).writeS32(0x3000 + i);
      o.add(0x64).writeS32(0x66);
      o.add(0xf0).writeS32(v58);
      fx['sf' + i] = o;
    });
    __freeze.thread(); negOn();
    __freeze.save(ptr(0x0083afc4), 4);
    __freeze.save(ptr(0x0083af6c), 44);
    __freeze.save(ptr(0x00839650), 4);
    ptr(0x00839650).writeU32(0);
    ptr(0x0083afc4).writeU32(1);
    const r = u32(E.rec.toUInt32());
    const b = [0xA1, ...r, 0x05, ...r, 0xC7, 0x00, ...u32(0x5E000000)];
    for (let k = 1; k <= 3; k++) b.push(0x8B, 0x54, 0x24, 4 * k, 0x89, 0x50, 4 * k);
    b.push(0x81, 0x05, ...r, ...u32(16));
    if (mode === 'ok') b.push(0x8B, 0x54, 0x24, 0x04, 0xC7, 0x02, ...u32(made.toUInt32()), 0xB8, ...u32(0));
    else b.push(0xB8, ...u32(5));
    b.push(0xC3);
    ptr(0x0083af6c).writePointer(E.emit(b));
    // releaseSoundObj: records its one argument (cdecl) and returns 0
    const rel = [0xA1, ...r, 0x05, ...r, 0xC7, 0x00, ...u32(0x5E100000), 0x8B, 0x54, 0x24, 0x04, 0x89, 0x50, 0x04,
                 0x81, 0x05, ...r, ...u32(8), 0x33, 0xC0, 0xC3];
    ptr(0x0083af70).writePointer(E.emit(rel));
    return fx;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3dx_sndfam_ok() { return soundFamily('ok'); },
    c3dx_sndfam_fail() { return soundFamily('fail'); },
    c3dx_music_ok() { return musicObjs('ok'); },
    c3dx_music_fail() { return musicObjs('fail'); },
    c3dx_flic() { return flics({}); },
    c3dx_flic_fail0() { return flics({ failMask: 1 }); },
    c3dx_names() { return nameObjs({}); },
    c3dx_names_fail0() { return nameObjs({ failMask: 1 }); },
    c3dx_bmp() { return bitmaps({}); },
    c3dx_lm() { return listModels({}); },
    c3dx_lm_fail0() { return listModels({ failMask: 1 }); },
    c3dx_lm_fail1() { return listModels({ failMask: 2 }); },
    c3dx_tvadd() { return textViews({}); },
    c3dx_tvadd_fail0() { return textViews({ failMask: 1 }); },
    c3dx_lbadd() { return listBoxes({}); },
    c3dx_lbadd_fail0() { return listBoxes({ failMask: 1 }); },
    c3dx_lbadd_fail1() { return listBoxes({ failMask: 2 }); },
  });
})();
