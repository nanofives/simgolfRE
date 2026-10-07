// Fixtures for C3 batch c3y (Window::adjustSizeOuter, comboFindItem, ListModel::ctor, ListBox::ctor,
// viewFieldCtor4a2250). Every object is a private Memory.alloc with only the fields the functions read; the running
// game never walks these. Only synthetic values are written, never game text. diff_hook rewrites Memory.alloc to
// __keepAlloc so blocks referenced only through a pointer field are not freed mid-run.
(function () {
  function fill(p, size, seed) { for (let i = 0; i < size; i += 4) p.add(i).writeU32((i * 2654435761 + seed) >>> 0); }

  // Shared (a, b) scratch for Window::adjustSizeOuter (0x0047ca10): 4 int pairs with varied initial values; the whole
  // 0x20 buffer is the state region. a<i>/b<i> expose the i-th pair's two cells.
  function withScratch(fx) {
    const buf = Memory.alloc(0x20);
    const vals = [100, 200, -50, 40, 0, 1000, -1, -1];
    for (let i = 0; i < 8; i++) buf.add(i * 4).writeS32(vals[i]);
    fx.scratch = buf;
    for (let i = 0; i < 4; i++) { fx['a' + i] = buf.add(i * 8); fx['b' + i] = buf.add(i * 8 + 4); }
    return fx;
  }
  // A window: flags dword at +0x9c, border fields +0x180/+0x184/+0x188, child pointer +0x15c. The border global at
  // 0x0083ff10 is read by the function itself (not seeded here). cornersH !== -1 by default so the +0x188 branch runs.
  function win(flags, opts) {
    opts = opts || {};
    const w = Memory.alloc(0x200);
    w.add(0x9c).writeU32(flags >>> 0);
    w.add(0x180).writeS32('f180' in opts ? opts.f180 : 300);
    w.add(0x184).writeS32('f184' in opts ? opts.f184 : 10);
    w.add(0x188).writeS32('f188' in opts ? opts.f188 : 50);
    if (opts.child) w.add(0x15c).writePointer(opts.child);
    return withScratch({ obj: w });
  }
  // A child whose virtual slot +0x170 is Widget_get (0x00477560): it returns [[child+0x5c]+0x10] with no stack args,
  // so the adjustSizeOuter virtual call (call [vtable+0x170]) adds a deterministic value to *b. child+0 -> a 0x180
  // vtable with the slot, child+0x5c -> a sub-object whose +0x10 is the returned value.
  function childWithGetter(value) {
    const vt = Memory.alloc(0x180);
    vt.add(0x170).writePointer(ptr('0x00477560'));   // Widget_get
    const sub = Memory.alloc(0x20);
    sub.add(0x10).writeS32(value);
    const child = Memory.alloc(0x200);
    child.writePointer(vt);
    child.add(0x5c).writePointer(sub);
    return child;
  }

  // Build a combo object: selector pointer at selOff with [sel+8]=0, so the list base L = obj + baseOff. head = [L+8],
  // count = [L+0x10]. Each node: +4 id, +8 -> {+4 value}, +0xc next. ids 10/20/30/40, values 1010/2020/3030/4040.
  function combo(bit4, head0, count0) {
    const obj = Memory.alloc(0x3000);
    const selOff = bit4 ? 0x1488 : 0x2d98;
    const baseOff = bit4 ? 0x1548 : 0x2e58;
    if (bit4) obj.add(4).writeU8(4);                 // flags bit 4 selects the +0x1488 list
    const sel = Memory.alloc(0x10);
    sel.add(8).writeS32(0);                           // off = 0
    obj.add(selOff).writePointer(sel);
    const ids = [10, 20, 30, 40];
    let next = ptr(0);
    for (let i = ids.length - 1; i >= 0; i--) {       // link backwards so node0 is the head
      const node = Memory.alloc(0x20);
      node.add(4).writeS32(ids[i]);
      const subv = Memory.alloc(0x10); subv.add(4).writeS32((ids[i] + 1) * 100 + ids[i]);
      node.add(8).writePointer(subv);
      node.add(0xc).writePointer(next);
      next = node;
    }
    obj.add(baseOff + 8).writePointer(head0 ? next : ptr(0));   // [L+8] head
    obj.add(baseOff + 0x10).writeS32(count0);                   // [L+0x10] count
    return { obj: obj };
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    // Window::adjustSizeOuter 0x0047ca10 flag variants (each returns via *a/*b in the scratch state region).
    c3y_win_flat()      { return win(0); },                                   // no bits: *a/*b unchanged
    c3y_win_b4()        { return win(0x4); },                                 // bit 4 -> *b += border
    c3y_win_b8()        { return win(0x8); },                                 // bit 8 -> *a += border
    c3y_win_b400()      { return win(0x400, { f184: 10, f188: 50 }); },       // bit 0x400 -> 2*w to both, +(h-w) to *b
    c3y_win_b400h1()    { return win(0x400, { f184: 10, f188: -1 }); },       // h == -1 -> skip the (h-w) add
    c3y_win_b10()       { return win(0x10, { f180: 300, f184: 10 }); },       // bit 0x10 -> 2*w block AND +0x180 block
    c3y_win_child()     { return win(0, { child: childWithGetter(777) }); },  // child vcall runs, adds 777 to *b
    c3y_win_childskip() { return win(0x20000000, { child: childWithGetter(777) }); },  // flag 0x20000000 -> skip vcall

    // comboFindItem 0x004940e0 list variants.
    c3y_combo_a()      { return combo(false, true, 4); },   // bit 4 clear, 4 nodes
    c3y_combo_b()      { return combo(true, true, 4); },    // bit 4 set, 4 nodes
    c3y_combo_empty()  { return combo(false, false, 4); },  // head null -> returns 0
    c3y_combo_count0() { return combo(false, true, 0); },   // count 0 -> returns 0

    // ListModel::ctor 0x00489150: distinct pre-fill + distinct global cookie 0x00839650 per variant, so the preserved
    // regions and the saved-cookie field +0xec differ (the ctor overwrites its own fields identically in both arms).
    c3y_lm_a() { const o = Memory.alloc(0x200); fill(o, 0x200, 0x11); ptr('0x00839650').writeS32(0x1111); return { obj: o }; },
    c3y_lm_b() { const o = Memory.alloc(0x200); fill(o, 0x200, 0x22); ptr('0x00839650').writeS32(0x2222); return { obj: o }; },
    c3y_lm_c() { const o = Memory.alloc(0x200); fill(o, 0x200, 0x33); ptr('0x00839650').writeS32(0x3333); return { obj: o }; },

    // ListBox::ctor 0x00489cb0: 0x800 object (holds the Widget at +0x5c and ListModel at +0x5d4). build=1 runs the
    // sub-ctors; build=0 needs the primary vtable (+0) and Widget sub-vtable (+4) pre-seeded (they are read after the
    // guard). The cookie is seeded because ListModel::ctor touches it.
    c3y_listbox()    { const o = Memory.alloc(0x800); fill(o, 0x800, 0x44); ptr('0x00839650').writeS32(0x4444); return { obj: o }; },
    c3y_listbox_b0() {
      const o = Memory.alloc(0x800); fill(o, 0x800, 0x55);
      o.writePointer(ptr('0x004bb3cc')); o.add(4).writePointer(ptr('0x004bb3d0'));
      ptr('0x00839650').writeS32(0x5555); return { obj: o };
    },

    // viewFieldCtor4a2250 0x004a2250: 0x800 object (Widget at +0x8c, ListModel at +0x604). build=0 needs the primary
    // vtable (+0) pre-seeded (read after the guard).
    c3y_viewfield()    { const o = Memory.alloc(0x800); fill(o, 0x800, 0x66); ptr('0x00839650').writeS32(0x6666); return { obj: o }; },
    c3y_viewfield_b0() {
      const o = Memory.alloc(0x800); fill(o, 0x800, 0x77);
      o.writePointer(ptr('0x004bc214')); ptr('0x00839650').writeS32(0x7777); return { obj: o };
    },
  });
})();
