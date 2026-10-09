// Fixtures for batch c3cr (round 22, golf_clean.exe UI rows below 0x491b70).
// Reimplementations: shim/src/re/c3cr.cpp. Registry: re/frida/registry.d/c3cr.py. Purposes: log/c3/c3cr_purpose.md.
//
// Every object here is our own allocation; none is linked into the game's window list, draw list or focus chain.
// The virtual calls land in RECORDING STUBS written as machine code (no JavaScript runs inside a vector, so the
// fireSelectChanged keys can freeze the game's first thread without any lock or allocation while it is down).
//
// A recording stub for (tag, n stack arguments, thiscall or cdecl):
//     mov eax, [REC]            ; record cursor (byte offset from REC)
//     add eax, REC
//     mov dword [eax], tag
//     mov [eax+4], ecx          ; thiscall: the object the slot was called on (cdecl: the constant 0xC0C0C0C0,
//                               ;   because ecx is not an input of a cdecl call and differs between the arms)
//     mov edx, [esp+4i]; mov [eax+4+4i], edx   for each argument i = 1..n
//     mov edx, [0x0083b654]; mov [eax+8+4n], edx   ; the "marked" flag at the moment of the call
//     add dword [REC], 4*(n+3)
//     mov eax, [REC]; xor eax, tag   ; a return value that differs per call
//     ret 4n  (thiscall)  |  ret  (cdecl)
// The recorder block REC (0x400 bytes, cursor at +0, entries from +0x10) is part of every key's state, so the
// order, the target object, every argument and the flag value of each dispatched call are compared.
// Optional prefixes for the focus stub (slot +0xf0) let the "lose focus" call change the container, which is the
// only way the re-reads in Container::cycleSelection at 0x0047b04c..0x0047b067 can see a different list:
//     mode 1: when the argument is 0: mov eax,[ecx+0x130]; mov dword [eax+0x138], 0        (clear the list head)
//     mode 2: when the argument is 0: mov eax,[ecx+0x130]; mov eax,[eax+0x13c]; mov dword [eax+4], 0
//                                                                              (clear the cursor node's item)
(function () {
  const G_MARKED = 0x0083b654;
  const G_CURWIDGET = 0x0083ab2c;

  function u32(v) { v >>>= 0; return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]; }
  function fill(p, n, v) { for (let i = 0; i < n; i += 4) p.add(i).writeU32(v >>> 0); }
  function alloc(n, v) { const p = Memory.alloc(n); fill(p, n, v === undefined ? 0 : v); return p; }

  // A code page plus a bump pointer for the stubs.
  function codeArena() {
    const page = Process.pageSize;
    const base = Memory.alloc(page * 2);
    Memory.protect(base, page * 2, 'rwx');
    let off = 0;
    return function (bytes) {
      const p = base.add(off);
      p.writeByteArray(bytes);
      off += (bytes.length + 15) & ~15;
      if (off > page * 2) throw new Error('c3cr: code arena full');
      return p;
    };
  }

  function stubBytes(rec, tag, n, thiscall, mode) {
    const r = u32(rec.toUInt32());
    const b = [];
    if (mode === 1 || mode === 2) {
      // cmp dword [esp+4], 0 ; jne skip
      const body = mode === 1
        ? [0x8B, 0x81, 0x30, 0x01, 0x00, 0x00,                       // mov eax, [ecx+0x130]
           0xC7, 0x80, 0x38, 0x01, 0x00, 0x00, 0, 0, 0, 0]           // mov dword [eax+0x138], 0
        : [0x8B, 0x81, 0x30, 0x01, 0x00, 0x00,                       // mov eax, [ecx+0x130]
           0x8B, 0x80, 0x3C, 0x01, 0x00, 0x00,                       // mov eax, [eax+0x13c]
           0xC7, 0x40, 0x04, 0, 0, 0, 0];                            // mov dword [eax+4], 0
      b.push(0x83, 0x7C, 0x24, 0x04, 0x00, 0x75, body.length);
      b.push(...body);
    }
    b.push(0xA1, ...r);                                              // mov eax, [REC]
    b.push(0x05, ...r);                                              // add eax, REC
    b.push(0xC7, 0x00, ...u32(tag));                                 // mov dword [eax], tag
    if (thiscall) b.push(0x89, 0x48, 0x04);                          // mov [eax+4], ecx
    else b.push(0xC7, 0x40, 0x04, ...u32(0xC0C0C0C0));              // mov dword [eax+4], 0xC0C0C0C0
    for (let i = 1; i <= n; i++) b.push(0x8B, 0x54, 0x24, 4 * i, 0x89, 0x50, 4 + 4 * i);
    b.push(0x8B, 0x15, ...u32(G_MARKED));                            // mov edx, [0x0083b654]
    b.push(0x89, 0x50, 8 + 4 * n);                                   // mov [eax+8+4n], edx
    b.push(0x81, 0x05, ...r, ...u32(4 * (n + 3)));                   // add dword [REC], 4(n+3)
    b.push(0xA1, ...r);                                              // mov eax, [REC]
    b.push(0x35, ...u32(tag));                                       // xor eax, tag
    if (thiscall && n > 0) b.push(0xC2, 4 * n, 0x00); else b.push(0xC3);
    return b;
  }

  function newRec() { const rec = alloc(0x400); rec.writeU32(0x10); return rec; }

  // A vtable of 0x80 slots whose unused slots point at `xor eax, eax; ret` and whose used slots are recording
  // stubs. spec: [[slot offset, tag, n, mode], ...] (thiscall).
  function vtable(emit, rec, spec) {
    const nul = emit([0x33, 0xC0, 0xC3]);
    const vt = alloc(0x200);
    for (let i = 0; i < 0x80; i++) vt.add(i * 4).writePointer(nul);
    for (const [slot, tag, n, mode] of spec) vt.add(slot).writePointer(emit(stubBytes(rec, tag, n, true, mode || 0)));
    return vt;
  }

  // Suspend the process's first thread (the one that owns the window and runs the UI dispatch that rewrites
  // 0x0083ab2c) for the length of one key; `undo` runs before it is resumed (c3cd's pattern).
  // Shared frozen-thread helper (re/frida/js/freeze.js): suspends the process's first thread at a safe point;
  // `undo` runs at the flush, before the thread is resumed.
  function freezeFirstThread(undo) {
    __freeze.thread();
    __freeze.atFlush(undo);
  }

  // ---- ListBox objects ---------------------------------------------------------------------------------------
  // Each object is one 0x800-byte block pre-filled 0x5a5a5a5a. P = block+0x40 holds the vbtable pointer and
  // `this` = P + 0x58 (so this-0x58 = P). vbtable [0] 0, [1] d1, [2] d2: the Window base W = P + d1 and the
  // list-model base M = P + d2. Fields (only those the functions read):
  //   W+0     the W vtable (slots +0x78, +0x98, +0xe8 and +0x120 record; 2, 2, 2 and 0 arguments)
  //   W-4     the vtordisp displacement W - this (what `sub ecx, [ecx-4]` subtracts), so a thunk gets W
  //   W+0x9c  flag byte (bit 3 scrollBy, bit 6 fwdInnerVfn20/24)       = this + d1 + 0x44
  //   W+0x130 parent (a 0x200-byte object on the parent vtable: +0x20 0 args, +0x24 1, +0x74 2, +0x104 2,
  //           +0x108 1) or 0                                          = this + d1 + 0xd8
  //   W+0x26c 0: Window::fwdScroll26c 0x0047b9f0 then calls nothing (its setWidgetScalar path writes 0x0083ab2c)
  //   M+0x5c visible rows (this + d2 + 4), M+0xd0 row count (this + d2 + 0x78), M+0xf4 selection (this + d2 + 0x9c)
  //   M+0xc8 list head, +0xcc cursor, +0xd4, +0xf0 selected index: what ListModel::selectedId 0x00489950 walks
  //   P+0x24  top row (this-0x34); P+0x50 the cdecl selection callback (this-8) or 0
  // The parent's +0x98 is a child on the child vtable (+0x1c, 0 args) or 0.
  //            d1      d2     flags  parent child  cb   top  rows  vis  idx
  const LB = [[0x100, 0x400, 0x48, 1, 1, 1, 3, 10, 4, 2],
              [0x180, 0x480, 0x40, 1, 0, 0, 0, 5, 5, -1],
              [0x200, 0x500, 0x08, 1, 1, 1, 7, 10, 3, 12],
              [0x500, 0x100, 0x00, 0, 0, 1, 1, 2, 0, 0],
              [0x120, 0x420, 0x4c, 1, 1, 0, -2, 0, 0, -3],
              [0x100, 0x400, 0x48, 0, 0, 0, 5, 5, -3, 3],
              [0x140, 0x440, 0x40, 1, 1, 1, 1, 20, 7, -4],
              [0x160, 0x460, 0x08, 0, 0, 1, 0, 3, 1, 5]];

  function buildListBoxes(fx, emit, rec) {
    const vtW = vtable(emit, rec, [[0x78, 0xA0780000, 2], [0x98, 0xA0980000, 2], [0xe8, 0xA0E80000, 2],
                                   [0x120, 0xA1200000, 0]]);
    const vtP = vtable(emit, rec, [[0x20, 0xB0200000, 0], [0x24, 0xB0240000, 1], [0x74, 0xB0740000, 2],
                                   [0x104, 0xB1040000, 2], [0x108, 0xB1080000, 1]]);
    const vtC = vtable(emit, rec, [[0x1c, 0xC01C0000, 0]]);
    const cb = emit(stubBytes(rec, 0xD0000000, 1, false, 0));
    LB.forEach(function (r, i) {
      const [d1, d2, flags, hasParent, hasChild, hasCb, top, rows, vis, idx] = r;
      const blk = alloc(0x800, 0x5a5a5a5a);
      const P = blk.add(0x40);
      const self = P.add(0x58);
      const vbt = alloc(0x10);
      vbt.writeU32(0); vbt.add(4).writeS32(d1); vbt.add(8).writeS32(d2);
      P.writePointer(vbt);
      P.add(0x24).writeS32(top);
      P.add(0x50).writePointer(hasCb ? cb : ptr(0));
      const W = P.add(d1);
      W.writePointer(vtW);
      W.sub(4).writeS32(d1 - 0x58);
      W.add(0x9c).writeU32(flags);
      W.add(0x26c).writePointer(ptr(0));
      if (hasParent) {
        const par = alloc(0x200, 0x3c3c3c3c);
        par.writePointer(vtP);
        par.add(0x98).writePointer(ptr(0));
        if (hasChild) {
          const ch = alloc(0x40, 0x2d2d2d2d);
          ch.writePointer(vtC);
          par.add(0x98).writePointer(ch);
          fx['ch' + i] = ch;
        }
        W.add(0x130).writePointer(par);
        fx['par' + i] = par;
      } else {
        W.add(0x130).writePointer(ptr(0));
      }
      const M = P.add(d2);
      M.add(0x5c).writeS32(vis);
      M.add(0xd0).writeS32(rows);
      M.add(0xf0).writeS32(idx);
      M.add(0xd4).writeS32(0x77);
      // a four-node circular list with values 0x100*i + k at +4, next at +0xc, prev at +0x10
      const nodes = [];
      for (let k = 0; k < 4; k++) { const nd = alloc(0x20, 0x1e1e1e1e); nd.add(4).writeU32(0x100 * i + k); nodes.push(nd); }
      for (let k = 0; k < 4; k++) {
        nodes[k].add(0xc).writePointer(nodes[(k + 1) % 4]);
        nodes[k].add(0x10).writePointer(nodes[(k + 3) % 4]);
      }
      // object 7: no list and an index past the row count, so selectedId skips its walk (0x00489963) and returns 0
      // (a null head with an index it does walk would fault in the original at 0x004899a5)
      M.add(0xc8).writePointer(i === 7 ? ptr(0) : nodes[0]);
      M.add(0xcc).writePointer(nodes[1]);
      fx['blk' + i] = blk;
      fx['lb' + i] = self;
      fx['w' + i] = W;
    });
  }

  // ---- Container / items for cycleSelection, cycleSelectionIfOwned, isFocusChild ----------------------------
  // A container C (0x200 bytes): +0xa0 bit 0 set and +0x130 0 (so Window::visible of an item reduces to the
  // item's own bit 0 of +0xa0), list head +0x138, cursor +0x13c, child count +0x140, index +0x144.
  // Items (0x180 bytes) on the item vtable whose slot +0xf0 (1 argument) records; +0x9c flag dword, +0xa0 flag
  // byte, +0x130 parent. Nodes (0x20 bytes): item at +4, next at +0xc; circular over the container's items.
  //   container: [count, cursor node, index, visibility bit of each of the 4 items, item flag9c bit 12 mask]
  const CT = [[4, 0, 0, [1, 0, 1, 1]],     // node 1 hidden, node 2 visible: two steps
              [4, 3, 3, [1, 1, 1, 1]],     // cursor on the last node with index 3: the wrap at 0x0047afdb
              [4, 0, 0, [0, 0, 0, 0]],     // nothing visible: the walk comes back to the saved item (0x0047b03e)
              [4, 1, 1, [1, 0, 0, 0]],     // only node 0 visible: three steps
              [1, 0, 0, [1, 1, 1, 1]],     // count 1: the jle at 0x0047af78
              [2, 0, 0, [1, 0, 0, 1]],     // count 2 over a 4-node ring: two hidden steps end the loop (0x0047b019)
              [3, 2, 5, [0, 1, 0, 1]],     // an index that is not the node position
              [0, 0, 0, [1, 1, 1, 1]]];    // count 0

  function buildContainers(fx, emit, rec, mode) {
    const vtI = vtable(emit, rec, [[0xf0, 0xE0F00000, 1, mode]]);
    CT.forEach(function (r, c) {
      const [count, cursor, index, vis] = r;
      const C = alloc(0x200, 0x4b4b4b4b);
      C.writePointer(vtI);
      C.add(0x9c).writeU32(0);
      C.add(0xa0).writeU32(1);
      C.add(0x130).writePointer(ptr(0));
      const items = [], nodes = [];
      for (let k = 0; k < 4; k++) {
        const it = alloc(0x180, 0x69696969);
        it.writePointer(vtI);
        it.add(0x9c).writeU32(k === 3 && c % 2 === 1 ? 0x1000 : 0x20);  // bit 12 on item 3 of odd containers
        it.add(0xa0).writeU32(vis[k] ? 0x41 : 0x40);
        it.add(0x130).writePointer(C);
        items.push(it);
        fx['it' + c + '_' + k] = it;
      }
      for (let k = 0; k < 4; k++) {
        const nd = alloc(0x20, 0x0f0f0f0f);
        nd.add(4).writePointer(items[k]);
        nodes.push(nd);
        fx['nd' + c + '_' + k] = nd;
      }
      for (let k = 0; k < 4; k++) nodes[k].add(0xc).writePointer(nodes[(k + 1) % 4]);
      C.add(0x138).writePointer(nodes[0]);
      C.add(0x13c).writePointer(nodes[cursor]);
      C.add(0x140).writeS32(count);
      C.add(0x144).writeS32(index);
      fx['ct' + c] = C;
    });
    // Orphans: items with no parent (+0x130 = 0), one with bit 12 set; and an item whose parent has a null head
    // (only isFocusChild uses it: cycleSelection would hand Window::visible a NULL item there).
    const o0 = alloc(0x180, 0x69696969); o0.writePointer(vtI); o0.add(0x9c).writeU32(0); o0.add(0x130).writePointer(ptr(0));
    const o1 = alloc(0x180, 0x69696969); o1.writePointer(vtI); o1.add(0x9c).writeU32(0x1000); o1.add(0x130).writePointer(ptr(0));
    const nh = alloc(0x200, 0x4b4b4b4b); nh.add(0x138).writePointer(ptr(0)); nh.add(0x13c).writePointer(ptr(0));
    nh.add(0x140).writeS32(4);
    const o2 = alloc(0x180, 0x69696969); o2.writePointer(vtI); o2.add(0x9c).writeU32(0xefff); o2.add(0x130).writePointer(nh);
    fx.orph0 = o0; fx.orph1 = o1; fx.orph2 = o2; fx.nullhead = nh;
  }

  // ---- Buttons for timerCallback ------------------------------------------------------------------------------
  // Four 0x80-byte buttons on a vtable whose slot +0x78 (2 arguments) records: Button::onTimer 0x00488fd0 with
  // any `which` other than 0 and 1 reaches only that call (0x004890c7..0x004890cd).
  function buildButtons(fx, emit, rec) {
    const vtB = vtable(emit, rec, [[0x78, 0xF0780000, 2]]);
    for (let i = 0; i < 4; i++) {
      const b = alloc(0x80, 0x0b0b0b0b + i);
      b.writePointer(vtB);
      fx['btn' + i] = b;
    }
  }

  function listBoxFixture(freeze) {
    const fx = {};
    const emit = codeArena();
    const rec = newRec();
    fx.rec = rec; fx.obj = rec;
    buildListBoxes(fx, emit, rec);
    if (freeze) {
      const g = ptr(G_CURWIDGET);
      const saved = g.readPointer();
      freezeFirstThread(function () { g.writePointer(saved); });
    }
    return fx;
  }

  function containerFixture(mode) {
    const fx = {};
    const emit = codeArena();
    const rec = newRec();
    fx.rec = rec; fx.obj = rec;
    buildContainers(fx, emit, rec, mode);
    return fx;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3cr_listbox() { return listBoxFixture(false); },
    // fireSelectChanged writes 0x0083ab2c, which the game's UI thread rewrites: freeze that thread for the key.
    c3cr_listbox_frozen() { return listBoxFixture(true); },
    c3cr_focus() { return containerFixture(0); },
    c3cr_focus_cb_head() { return containerFixture(1); },
    c3cr_focus_cb_item() { return containerFixture(2); },
    c3cr_buttons() {
      const fx = {};
      const emit = codeArena();
      const rec = newRec();
      fx.rec = rec; fx.obj = rec;
      buildButtons(fx, emit, rec);
      return fx;
    },
  });
})();
