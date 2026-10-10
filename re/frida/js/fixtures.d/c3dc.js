// Fixtures for batch c3dc (round 24, jgld.dll). Reimplementations: shim/src/re/c3dc.cpp. Registry:
// re/frida/registry.d/c3dc.py. Purposes and branch tables: log/c3/c3dc_purpose.md.
//
// Everything a vector touches is the fixture's own memory, built on jgld's own vtables where a body or a callee
// dispatches through one. Module globals the bodies read or write (the tracked-object list 0x10128428, its
// teardown flag 0x10128444, g_display 0x1012870c, the Display pointer 0x10128420, the sprite memory counters
// 0x101284c8 / 0x101284cc and the import slot [0x1012c42c]) are only written with the game's first thread frozen
// (re/frida/js/freeze.js), which writes the game's values back before it resumes the thread. c3cw measured that
// only that thread touches the tracked list and jgld's allocator (log/c3/alt/c3cw_probe.py, c3cn_probe.py).
//
// The heap stand-in (re/frida/js/heapstandin.js) serves jgld's operator new 0x1007f0f0, malloc 0x1007fb40,
// calloc 0x1007ffa0, operator delete 0x1007e6d0 and free 0x100806d0 (c3cn's set; jalloc/jfree end in malloc /
// free) for every key, so every allocation comes from the compared arena and every free is a compared log entry.
//
// jgld.dll is relocated at run time, so every module address is built from the running base.

(function () {
  const jgld = () => Process.getModuleByName('jgld.dll').base;
  const fill = (p, n, seed) => { for (let i = 0; i < n; i += 4) p.add(i).writeU32(((seed << 16) + i) >>> 0); };

  function standIn(size) {
    return __heapStandIn({ module: 'jgld.dll',
                           alloc: [0x7f0f0, 0x7fb40, { rva: 0x7ffa0, args: 2, sizeArgs: 2, fill: 0 }],
                           free: [0x7e6d0, 0x806d0], arenaSize: size || 0x400, fill: 0xcd });
  }

  // c3cw's settle guard: a freezing key that ran inside the start-up burst of Tracked::ctor (thousands of jgld
  // operator new calls a second through the stand-in's forwarding path) hung the game once. Wait until the
  // tracked list's count (+0xc) has not moved for 0.5 s before installing anything.
  function settle() {
    const glist = jgld().add(0x128428);
    let last = glist.add(0xc).readS32(), still = 0;
    for (let t = 0; still < 5; t++) {
      if (t >= 600) throw new Error('c3dc: tracked list still changing after 60 s; not freezing');
      Thread.sleep(0.1);
      const c = glist.add(0xc).readS32();
      still = c === last ? still + 1 : 0;
      last = c;
    }
  }

  // Executable stubs. Each gets its own page-sized allocation, kept alive.
  function code(bytes) {
    const page = Process.pageSize;
    const p = __keepAlloc(page);
    Memory.protect(p, page, 'rwx');
    p.writeByteArray(bytes);
    return p;
  }
  const le = (v) => { const a = []; for (let i = 0; i < 4; i++) a.push((v >>> (8 * i)) & 0xff); return a; };
  const addrBytes = (p) => le(p.toUInt32());

  // __thiscall(int) node "deleting destructor" (c3cw's): eax = ++*ctr; node[+0x14] = eax; ret 4. Which node a
  // removeCurrent / clear deleted, and in what order, is then compared state and nothing reaches the CRT.
  function nodeVtable(ctr) {
    ctr.writeU32(0);
    const c = code([0xA1].concat(addrBytes(ctr), [0x40, 0xA3], addrBytes(ctr), [0x89, 0x41, 0x14, 0xC2, 0x04, 0x00]));
    const vt = __keepAlloc(0x20);
    for (let i = 0; i < 8; i++) vt.add(i * 4).writePointer(c);
    return vt;
  }

  // Lays out a circular list of n nodes at `nodes` (stride 0x20) holding datas[k], current = node `cur`
  // (-1 = none), and writes the 0x1c-byte LinkedList header at `list` (vtable word left as `vtWord`).
  function buildList(list, nodes, datas, cur, ordered, vt, vtWord) {
    const n = datas.length, ps = [];
    for (let k = 0; k < n; k++) {
      const p = nodes.add(k * 0x20);
      p.writePointer(vt);
      p.add(0xc).writePointer(ptr(datas[k]));
      p.add(0x10).writeU8(0x40 + k);
      ps.push(p);
    }
    for (let k = 0; k < n; k++) {
      ps[k].add(4).writePointer(ps[(k + n - 1) % n]);
      ps[k].add(8).writePointer(ps[(k + 1) % n]);
    }
    const c = n && cur >= 0 ? ps[cur] : ptr(0);
    if (vtWord !== undefined) list.writeU32(vtWord);
    list.add(4).writePointer(n && cur >= 0 ? ptr(datas[cur]) : ptr(0x5a5a0001));
    list.add(8).writeU32(n && cur >= 0 ? (0x40 + cur) : 0x5a5a0002);
    list.add(0xc).writeS32(n);
    list.add(0x10).writePointer(n ? ps[0] : ptr(0));
    list.add(0x14).writePointer(c);
    list.add(0x18).writeU32(ordered);
    return ps;
  }

  // ============================================================================ LinkedList::add (synthetic lists)
  // Eight lists of our own in one block ($blk, 0x2000 bytes):
  //   0x0000 + i*0x40  list i header          0x0800 + i*0x100  list i's nodes (stride 0x20)
  //   L0 empty, ordered 0            L1 empty, ordered 1 (count 0: no message, +0x18 still cleared)
  //   L2 one node, ordered 0         L3 one node, ordered 1: the debug message
  //   L4 three nodes, current 1      L5 three nodes, ordered 0x7fffffff: the message
  //   L6 five nodes, ordered 0       L7 count 2 with no head: the head arm with a non-zero count
  // The message goes through jgld's import slot [0x1012c42c] (OutputDebugStringA). OutputDebugStringA raises
  // DBG_PRINTEXCEPTION_C, which Frida's NativeFunction turns into a failed rpc (c3ci), so for this key the slot is
  // pointed at a stdcall recorder of our own (`$dbg`: +0 count, +4 last argument; `ret 4`) with the first thread
  // frozen, and the freeze helper writes the import back before resuming. Only jgld's own code reads jgld's slot.
  function lists() {
    const base = jgld();
    const h = standIn(0x400);
    const blk = __keepAlloc(0x2000);
    fill(blk, 0x2000, 0x41dd);
    const dbg = __keepAlloc(0x10);
    dbg.writeU32(0); dbg.add(4).writeU32(0);
    const rec = code([0x8B, 0x44, 0x24, 0x04, 0xA3].concat(addrBytes(dbg.add(4)), [0xFF, 0x05], addrBytes(dbg),
                     [0xC2, 0x04, 0x00]));
    const vt = nodeVtable(blk.add(0x1f00));
    const out = { arena: h.arena, freelog: h.log, blk: blk, obj: blk, dbg: dbg };
    const SPEC = [[[], -1, 0], [[], -1, 1], [[0x1001], 0, 0], [[0x1001], 0, 1],
                  [[0x2001, 0x2002, 0x2003], 1, 0], [[0x2001, 0x2002, 0x2003], 2, 0x7fffffff],
                  [[0x3001, 0x3002, 0x3003, 0x3004, 0x3005], 0, 0], [[], -1, 0]];
    SPEC.forEach(function (s, i) {
      const l = blk.add(i * 0x40);
      buildList(l, blk.add(0x800 + i * 0x100), s[0], s[1], s[2], vt, 0x0eadbe00 + i);
      out['L' + i] = l;
    });
    out.L7.add(0xc).writeS32(2);            // count 2, head NULL
    const slot = base.add(0x12c42c);
    Memory.protect(slot, 4, 'rw-');
    __freeze.thread();
    __freeze.save(slot, 4);
    slot.writePointer(rec);
    return out;
  }

  // ============================================================================ the swapped tracked list
  // c3cw's recipe: freeze the first thread, save the 0x1c-byte header of 0x10128428 and write our own list.
  //   shape: empty | one | five (current = node 2) | fivehead (current = head)
  // Twelve plain objects $o0..$o11 (stride 0x40) are the candidates for Tracked::ctor / SurfaceBase::dtor /
  // SpriteBase::~SpriteBase; the list's nodes hold $o0..$o4, so $o5..$o11 are "not found" for find().
  function tracked(shape) {
    settle();
    const base = jgld();
    const glist = base.add(0x128428);
    const h = standIn(0x400);
    const blk = __keepAlloc(0x1000);
    fill(blk, 0x1000, 0x7a3e);
    const vt = nodeVtable(blk.add(0xf00));
    const out = { arena: h.arena, freelog: h.log, blk: blk, obj: blk, glist: glist };
    for (let i = 0; i < 12; i++) out['o' + i] = blk.add(i * 0x40);
    const n = { empty: 0, one: 1, five: 5, fivehead: 5 }[shape];
    const datas = [];
    for (let k = 0; k < n; k++) datas.push(out['o' + k]);
    __freeze.thread();
    __freeze.save(glist, 0x1c);
    buildList(glist, blk.add(0xc00), datas, shape === 'five' ? 2 : 0, 0, vt);
    return out;
  }

  // ============================================================================ deleteAll / destroyDisplay
  // Real objects of the Tracked classes in the swapped list, so deleteAll's slot-0 call runs each class's own
  // scalar deleting destructor (which unregisters it through Tracked::dtor's find + removeCurrent, and frees it
  // through the stand-in's operator delete). Objects are 0x840 bytes (Palette::~Palette reads +0x80c and
  // Palette::reset 0x1006a380 writes +0x810 / +0x814); a Palette's +0x80c points at a 0x10-byte sub-object.
  //   kinds: T Tracked 0x1011d058, B PaletteBase 0x1011db3c, P Palette 0x1011db10, S SurfaceBase 0x1011d220,
  //          R SpriteBase 0x1011d444
  // A non-Tracked object (DisplayBase) is never put in the list: its destructor does not unregister, so the
  // count never drops and the original loops forever; a NULL data word does the same (see the cpp).
  const KIND = { T: 0x11d058, B: 0x11db3c, P: 0x11db10, S: 0x11d220, R: 0x11d444 };
  const SHAPES = {
    empty: ['', -1], one: ['T', 0], three: ['TBR', 1], five: ['TPSRT', 0], fivecur3: ['TPSRT', 3],
  };
  function realList(shapeName, out, blk) {
    const base = jgld();
    const glist = base.add(0x128428);
    const vt = nodeVtable(blk.add(0x5f00));
    const spec = SHAPES[shapeName];
    const datas = [];
    for (let k = 0; k < spec[0].length; k++) {
      const o = blk.add(k * 0x840);
      o.writePointer(base.add(KIND[spec[0][k]]));
      o.add(0x80c).writePointer(spec[0][k] === 'P' ? blk.add(0x5800 + k * 0x10) : ptr(0));
      datas.push(o);
      out['k' + k] = o;
    }
    out.glist = glist;
    __freeze.save(glist, 0x1c);
    buildList(glist, blk.add(0x5c00), datas, spec[1], 0, vt);
  }
  function deleteAllFx(shapeName) {
    settle();
    const h = standIn(0x400);
    const blk = __keepAlloc(0x6000);
    fill(blk, 0x6000, 0x6d11);
    const out = { arena: h.arena, freelog: h.log, blk: blk, obj: blk };
    __freeze.thread();
    realList(shapeName, out, blk);
    return out;
  }
  // display: null | base (a DisplayBase object: slot 0 is DisplayBase::deletingDtor 0x100674f0) | stub (a vtable
  // of our own whose slot 0 is c3cw's counting stub, which marks the object's +0x14 and returns)
  function destroyFx(display, shapeName) {
    settle();
    const base = jgld();
    const h = standIn(0x400);
    const blk = __keepAlloc(0x6000);
    fill(blk, 0x6000, 0x5e22);
    const out = { arena: h.arena, freelog: h.log, blk: blk, obj: blk };
    const disp = blk.add(0x5e00);
    if (display === 'base') disp.writePointer(base.add(0x11d7dc));
    if (display === 'stub') disp.writePointer(nodeVtable(blk.add(0x5ef0)));
    out.disp = disp;
    const gdisp = base.add(0x12870c);
    out.gdisp = gdisp;
    __freeze.thread();
    realList(shapeName, out, blk);
    __freeze.save(gdisp, 4);
    gdisp.writePointer(display === 'null' ? ptr(0) : disp);
    return out;
  }

  // ============================================================================ atexitDestroyGlobalList
  // flag: the byte at 0x10128444 (bit 0 decides); shape: the swapped list (its nodes use the counting stub, which
  // LinkedList::clear 0x10006df0 calls for every node). LinkedList::dtor 0x10006cf0 also writes the LinkedList
  // vtable over the header's first dword, which $glist records and the freeze helper writes back.
  function atexitFx(flag, shape) {
    settle();
    const base = jgld();
    const glist = base.add(0x128428);
    const gflag = base.add(0x128444);
    const h = standIn(0x400);
    const blk = __keepAlloc(0x1000);
    fill(blk, 0x1000, 0x2b47);
    const vt = nodeVtable(blk.add(0xf00));
    const out = { arena: h.arena, freelog: h.log, blk: blk, obj: blk, glist: glist, gflag: gflag };
    const n = { empty: 0, one: 1, five: 5 }[shape];
    const datas = [];
    for (let k = 0; k < n; k++) datas.push(blk.add(k * 0x40));
    __freeze.thread();
    __freeze.save(glist, 0x1c);
    __freeze.save(gflag, 4);
    buildList(glist, blk.add(0xc00), datas, n ? 1 % n : -1, 0, vt, 0x0eadbeef);
    gflag.writeU8(flag);
    return out;
  }

  // ============================================================================ DisplayBase destructors
  // Twelve objects at an odd stride (0x24) in one block; no list is involved.
  function displayObjs() {
    const h = standIn(0x100);
    const blk = __keepAlloc(0x400);
    fill(blk, 0x400, 0x3c77);
    const out = { arena: h.arena, freelog: h.log, blk: blk, obj: blk };
    for (let i = 0; i < 12; i++) out['d' + i] = blk.add(i * 0x24);
    return out;
  }

  // ============================================================================ Sprite::release
  // Twelve sprites (stride 0x40) and their blocks in one region; the two module counters 0x101284c8 /
  // 0x101284cc are set to values of our own with the first thread frozen (Sprite::create8 / create16 on that
  // thread are their other writers) and are a state region.
  //   payload, +0x18, second block, +0x24, +0x2c, +0x30 (w), +0x34 (h)
  const RSPEC = [
    [1, 0x00, 0, 0x10, 16, 16, 8],     // r0  payload, flat form
    [1, 0x01, 0, 0x30, 16, 16, 8],     // r1  payload, span form (bit 0): both counters
    [1, 0x03, 1, 0x44, 20, 20, 10],    // r2  span form, other bits set, second block too
    [0, 0x01, 1, 0x50, 8, 8, 4],       // r3  no payload: counters and the first free skipped
    [0, 0x00, 0, 0x00, 8, 8, 4],       // r4  nothing to free: only the five clears
    [1, 0x02, 1, 0x20, 4, 3, 7],       // r5  bit 0 clear with bit 1 set
    [1, 0x01, 0, 0x7fffffff, 9, 9, 9], // r6  a huge span count: the difference wraps
    [1, 0x01, 0, 0, 0x10000, 0x10000, 0x8000], // r7  width * height overflows 32 bits
    [1, 0xffffffff, 1, 5, 1, 1, 1],    // r8  every flag bit
    [1, 0x00, 1, 0x11, 7, -3, 5],      // r9  negative width
    [0, 0xfe, 0, 0x12, 3, 4, 5],       // r10 no payload, bit 0 clear
    [1, 0x01, 1, 0x13, 100, 640, 480], // r11 large sprite
  ];
  function releaseFx() {
    settle();
    const base = jgld();
    const h = standIn(0x100);
    const blk = __keepAlloc(0x1000);
    fill(blk, 0x1000, 0x5151);
    const out = { arena: h.arena, freelog: h.log, blk: blk, obj: blk };
    RSPEC.forEach(function (s, i) {
      const p = blk.add(i * 0x40);
      p.add(0x14).writePointer(s[0] ? blk.add(0x400 + i * 0x40) : ptr(0));
      p.add(0x18).writeU32(s[1] >>> 0);
      p.add(0x1c).writePointer(s[2] ? blk.add(0x800 + i * 0x40) : ptr(0));
      p.add(0x24).writeU32(s[3] >>> 0);
      p.add(0x2c).writeS32(s[4]);
      p.add(0x30).writeS32(s[5]);
      p.add(0x34).writeS32(s[6]);
      out['r' + i] = p;
    });
    const g = base.add(0x1284c8);
    out.gcount = g;
    __freeze.thread();
    __freeze.save(g, 8);
    g.writeU32(0x00123456);
    g.add(4).writeU32(0x00654321);
    return out;
  }

  // ============================================================================ the pixel-format helpers
  // A Display of our own for the module pointer 0x10128420: +0 a vtable whose slot +0xb4 is the stub
  // `mov eax, [ecx+4]; inc dword [ecx+8]; ret` (thiscall, no argument), +4 the format value it returns, +8 the
  // call counter, so the dispatch itself is compared state ($disp). Swapped in with the first thread frozen.
  function fmtFx(value) {
    settle();
    const base = jgld();
    const disp = __keepAlloc(0x10);
    const vt = __keepAlloc(0x100);
    const stub = code([0x8B, 0x41, 0x04, 0xFF, 0x41, 0x08, 0xC3]);
    for (let i = 0; i < 0x40; i++) vt.add(i * 4).writePointer(stub);
    disp.writePointer(vt);
    disp.add(4).writeU32(value >>> 0);
    disp.add(8).writeU32(0);
    const g = base.add(0x128420);
    __freeze.thread();
    __freeze.save(g, 4);
    g.writePointer(disp);
    return { obj: disp, disp: disp };
  }

  // ============================================================================ the Surface copy dispatchers
  // c3bh's surface set (its fixture c3bh_surfaces, copied: 8-bit and 16-bit surfaces on jgld's Surface vtable
  // 0x1011d0b0 with Palettes on 0x1011db10, the same-surface m* surfaces and the planted row-count bytes of the
  // blitT_8to16 last body) plus c3bc's shade table and in-place RECTs for blit16from16. The blitters' module loop
  // globals are not state regions (c3bc / c3bh: 0 changes at the menu, 0 entries into the dispatchers); what
  // they hold is verified through $pix. No surface without a Palette is used, so the client global 0x1012873c is
  // never read.
  function surfaces() {
    if (globalThis.__c3dcSurf) return globalThis.__c3dcSurf;
    const base = jgld();
    const vtSurface = base.add(0x11d0b0);
    const vtPalette = base.add(0x11db10);
    const mkPal = (a565, b565, a555, b555) => {
      const p = __keepAlloc(0x820);
      for (let o = 0; o < 0x820; o += 4) p.add(o).writeU32(0);
      p.writePointer(vtPalette);
      for (let i = 0; i < 256; i++) {
        p.add(0x40c + i * 2).writeU16((i * a565 + b565) & 0xffff);
        p.add(0x60c + i * 2).writeU16((i * a555 + b555) & 0xffff);
      }
      return p;
    };
    const pal = mkPal(0x0105, 0x1234, 0x0203, 0x5678);
    const pal2 = mkPal(0x0307, 0x9abc, 0x0409, 0xdef0);
    //       depth fmt pitch bounds            clip              base   palette
    const SPECS = [
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal],    // 0  s8a   8-bit 32 x 16
      [8, 0, 24, [0, 0, 24, 12], [0, 0, 24, 12], true, pal2],   // 1  s8b   8-bit 24 x 12, the other palette
      [8, 0, 24, [0, 0, 24, 12], [2, 1, 18, 9], true, pal],     // 2  s8c   8-bit, clip strictly inside
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], false, pal],   // 3  s8nb  8-bit, no pixel base
      [12, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal],   // 4  s12   depth 12
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal2],  // 5  d16a  16-bit, clip == bounds
      [16, 0, 32, [0, 0, 32, 16], [4, 2, 20, 12], true, pal2],  // 6  d16b  clip strictly inside
      [16, 1, 19, [0, 0, 19, 11], [0, 0, 19, 11], true, pal2],  // 7  d16c  format 1
      [16, 2, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal2],  // 8  d16d  format 2
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], false, pal2], // 9  d16f  no pixel base
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal],    // 10 m1    same-surface 8-bit
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal2],  // 11 m16   same-surface 16-bit
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal],    // 12 mb1   blitT_8to16's last body, one row
      [24, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal],   // 13 x24   depth 24
      [0, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal],    // 14 x0    depth 0
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal],   // 15 d16p  16-bit, the first palette
    ];
    const NAMES = ['s8a', 's8b', 's8c', 's8nb', 's12', 'd16a', 'd16b', 'd16c', 'd16d', 'd16f', 'm1', 'm16', 'mb1',
                   'x24', 'x0', 'd16p'];
    const NSURF = SPECS.length, SLICE = 0x1000;
    const pix = __keepAlloc(NSURF * SLICE);
    // Pixel alphabets chosen so the KEYED blitters have something to match (c3ay, c3bc): 8-bit slices hold
    // 0x40..0x46 (blit8from8 writes the argument surface where its byte equals the key), 16-bit slices hold the
    // first Palette's colours for indices 0x40..0x46 in the table their format code selects (blitKey8to16 writes
    // where the destination word equals the source's palette colour of the key), with one word in thirteen the
    // 0x7c1f blit16from16 tests for (VA 0x1000f7c6).
    const t565 = (i) => (i * 0x0105 + 0x1234) & 0xffff, t555 = (i) => (i * 0x0203 + 0x5678) & 0xffff;
    for (let i = 0; i < NSURF; i++) {
      const b0 = i * SLICE;
      if (SPECS[i][0] === 16) {
        const t = SPECS[i][1] === 1 ? t555 : t565;
        for (let k = 0; k < SLICE / 2; k++) {
          const v = ((k + i) % 13) === 5 ? 0x7c1f : t(0x40 + ((k * 3 + i * 5) % 7));
          pix.add(b0 + k * 2).writeU16(v);
        }
      } else {
        for (let j = 0; j < SLICE; j++) pix.add(b0 + j).writeU8(0x40 + ((j * 3 + i * 2) % 7));
      }
    }
    pix.add(12 * SLICE + 79).writeU8(1);     // mb1: blitT_8to16's last body stops after one row (c3bh)
    const fx = { obj: pix, pix: pix, pal: pal, pal2: pal2 };
    const surf = __keepAlloc(NSURF * 0x820);
    SPECS.forEach((sp, i) => {
      const s = surf.add(i * 0x820);
      for (let o = 0; o < 0x820; o += 4) s.add(o).writeU32(0);
      s.writePointer(vtSurface);
      s.add(0x24).writeS32(sp[0]);
      s.add(0x28).writeS32(sp[1]);
      s.add(0x40).writeS32(sp[2]);
      for (let j = 0; j < 4; j++) s.add(0x44 + j * 4).writeS32(sp[4][j]);
      for (let j = 0; j < 4; j++) s.add(0x54 + j * 4).writeS32(sp[3][j]);
      s.add(0x7c).writePointer(sp[6]);
      s.add(0x4c0).writePointer(sp[5] ? pix.add(i * SLICE) : ptr(0));
      fx[NAMES[i]] = s;
      fx['l' + i] = s.add(0x4c0);
    });
    fx.nsurf = NSURF;
    // c3bc's blit16from16 shade table (0x80000 words indexed by (15 - n) << 15 | colour) and $tbl2 0x80 in.
    const TBL = 0x80000, tbl = __keepAlloc(TBL * 2 + 0x100);
    for (let i = 0; i < TBL + 0x80; i++) tbl.add(i * 2).writeU16((i * 0x3b9 + (i >>> 15) * 0x1d7 + 0x51ed) & 0xffff);
    fx.tbl = tbl;
    fx.tbl2 = tbl.add(0x80);
    const RECTS = { br_full: [0, 0, 32, 16], br_in: [4, 2, 20, 12], br_odd: [3, 1, 14, 10], br_neg: [-6, -4, 10, 7],
                    br_over: [20, 8, 50, 30], br_out: [40, 40, 50, 50], br_s19: [0, 0, 19, 11] };
    const names = Object.keys(RECTS);
    const rects = __keepAlloc(names.length * 0x10);
    names.forEach((name, i) => {
      RECTS[name].forEach((v, j) => rects.add(i * 0x10 + j * 4).writeS32(v));
      fx[name] = rects.add(i * 0x10);
    });
    fx.rects = rects;
    globalThis.__c3dcSurf = fx;
    return fx;
  }

  // ============================================================================ the Sprite draw dispatchers
  // c3ci's object set (re/frida/js/fixtures.d/c3ci.js c3ci_objects, copied): fourteen surfaces on jgld's Surface
  // vtable 0x1011d0b0, two Palettes on 0x1011db10, Sprites on 0x1011d380 with plain 8-bit payloads in the middle
  // of 0x4000-byte slices, and the lookup tables the blitters are handed. Added here:
  //   - run-length row tables at +0x1c for the busy sprites (bit 0 of +0x18), the form the drawRle16* blitters
  //     read (re/match/jgld_blitY6..12.cpp: struct Row { BYTE start; BYTE len; WORD off; } per row, `off` a byte
  //     offset into the payload); four extra rows with len 0 follow each table, because the stepped body indexes
  //     the table with a 16.16 row counter;
  //   - a fifth payload slice of 16-bit words drawn from {0, 1, 2, 3, 0x00ff} for the 16-bit sprites: draw16t
  //     0x1001b060 indexes its table with (source << 16) | destination (c3bm), so only small words stay inside it.
  function spriteObjects() {
    if (globalThis.__c3dcSpr) return globalThis.__c3dcSpr;
    const base = jgld();
    const vtSurface = base.add(0x11d0b0), vtPalette = base.add(0x11db10), vtSprite = base.add(0x11d380);
    const mkPal = (a565, b565, a555, b555) => {
      const p = __keepAlloc(0x820);
      for (let o = 0; o < 0x820; o += 4) p.add(o).writeU32(0);
      p.writePointer(vtPalette);
      for (let i = 0; i < 256; i++) {
        p.add(0x40c + i * 2).writeU16((i * a565 + b565) & 0x1fff);
        p.add(0x60c + i * 2).writeU16((i * a555 + b555) & 0x1fff);
      }
      return p;
    };
    const pal = mkPal(0x0105, 0x0234, 0x0203, 0x0678);
    const pal2 = mkPal(0x0307, 0x0abc, 0x0409, 0x0ef0);
    const NSURF = 14, SLICE = 0x1000;
    const pix = __keepAlloc(NSURF * SLICE);
    //      depth fmt pitch bounds            clip              base
    const SPECS = [
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   //  0 d0
      [16, 0, 32, [0, 0, 32, 16], [4, 2, 20, 12], true],   //  1 d1  clip inside
      [16, 1, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   //  2 d2  format 1
      [16, 2, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   //  3 d3  format 2
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], false],  //  4 d4  no pixel base
      [16, 0, 24, [0, 0, 24, 12], [0, 0, 24, 12], true],   //  5 d5  stride 24
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   //  6 d6
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],    //  7 e0  8-bit
      [8, 0, 32, [0, 0, 32, 16], [4, 2, 20, 12], true],    //  8 e1  8-bit, clip inside
      [8, 0, 24, [0, 0, 24, 12], [0, 0, 24, 12], true],    //  9 e2
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], false],   // 10 e3  8-bit, no pixel base
      [24, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   // 11 x0  depth 24
      [12, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   // 12 x1  depth 12
      [0, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],    // 13 x2  depth 0
    ];
    const SNAMES = ['d0', 'd1', 'd2', 'd3', 'd4', 'd5', 'd6', 'e0', 'e1', 'e2', 'e3', 'x0', 'x1', 'x2'];
    for (let i = 0; i < NSURF; i++) {
      const b0 = i * SLICE;
      if (SPECS[i][0] === 16) {
        for (let k = 0; k < SLICE / 2; k++) pix.add(b0 + k * 2).writeU16((k * 4099 + i * 311 + 7) & 0x1fff);
      } else {
        for (let j = 0; j < SLICE; j++) pix.add(b0 + j).writeU8((j * 37 + i * 13 + 5) & 0xff);
      }
    }
    const fx = { obj: pix, pix: pix, pal: pal, pal2: pal2 };
    const surf = __keepAlloc(NSURF * 0x820);
    SPECS.forEach((sp, i) => {
      const s = surf.add(i * 0x820);
      for (let o = 0; o < 0x820; o += 4) s.add(o).writeU32(0);
      s.writePointer(vtSurface);
      s.add(0x24).writeS32(sp[0]);
      s.add(0x28).writeS32(sp[1]);
      s.add(0x40).writeS32(sp[2]);
      for (let j = 0; j < 4; j++) s.add(0x44 + j * 4).writeS32(sp[4][j]);
      for (let j = 0; j < 4; j++) s.add(0x54 + j * 4).writeS32(sp[3][j]);
      s.add(0x4c0).writePointer(sp[5] ? pix.add(i * SLICE) : ptr(0));
      fx[SNAMES[i]] = s;
      fx['l' + i] = s.add(0x4c0);
    });
    // payload slices 0..3: c3ci's alphabet (0xf8..0xff drive the blitters' special arms); slice 4: 16-bit words
    const NPAY = 6, PAY = 0x4000;
    const pay = __keepAlloc(NPAY * PAY);
    const ALPH = [0x00, 0x01, 0x05, 0x0f, 0x10, 0x3c, 0x7f, 0xa5, 0xf8, 0xf9, 0xfd, 0xfe, 0xff];
    const STEP = [5, 3, 7, 11];
    for (let i = 0; i < 4; i++) {
      for (let j = 0; j < PAY; j++) pay.add(i * PAY + j).writeU8(ALPH[(j * STEP[i] + i * 4) % 13]);
    }
    const W16 = [0, 1, 2, 3, 0xff, 2, 1];
    for (let k = 0; k < PAY / 2; k++) pay.add(4 * PAY + k * 2).writeU16(W16[(k * 3) % 7]);
    // slice 5: only 0xf8..0xff. Sprite::draw_1002ab90 (drawM's 8 bpp arm) indexes its table with
    // ((byte - 0xf8) & 0xff) << 15 | destination for EVERY byte below 0xfe (0x1002ae36..0x1002ae46 in
    // re/match/jgld_blit16d.cpp), so any lower byte reads megabytes past the table.
    for (let j = 0; j < PAY; j++) pay.add(5 * PAY + j).writeU8(0xf8 + ((j * 5) % 8));
    // run-length row tables
    const rows = __keepAlloc(0x400);
    const mkRows = (off, w, h, pitch) => {
      const t = rows.add(off);
      for (let r = 0; r < h + 4; r++) {
        const e = t.add(r * 4);
        if (r < h) {
          const st = (r * 3) % 4, ln = w - st - ((r * 5) % 3);
          e.writeU8(st); e.add(1).writeU8(ln); e.add(2).writeU16(r * pitch + st);
        } else {
          e.writeU32(0);
        }
      }
      return t;
    };
    //   palette, payload slice (-1 none), payload offset, m_18, m_20, pitch, width, height, rows (-1 none)
    const BSP = [
      [pal, 0, 0x1000, 0, 8, 16, 16, 8, -1],      //  0 bA     the main sprite
      [pal2, 1, 0x1000, 0, 8, 20, 20, 10, -1],    //  1 bB     wider, the other Palette
      [0, 2, 0x1000, 0, 8, 16, 16, 8, -1],        //  2 bNP    no Palette of its own: the client's
      [pal, 0, 0x1040, 0, 8, 4, 1, 1, -1],        //  3 bD     1 x 1
      [pal, -1, 0, 0, 8, 16, 16, 8, -1],          //  4 bNB    no payload: slot +0x20 returns 0
      [pal, 3, 0x1000, 0, 8, 16, 16, 8, -1],      //  5 bW
      [pal, 1, 0x1000, 1, 8, 16, 16, 8, 0x000],   //  6 bBsy   busy (run-length), 16 x 8
      [pal, 2, 0x1000, 0, 0x10, 16, 16, 8, -1],   //  7 b16    16 bpp, 8-bit alphabet slice
      [pal, 3, 0x1000, 0, 24, 16, 16, 8, -1],     //  8 bX     24 bpp: the outer default
      [pal2, 3, 0x1000, 3, 8, 20, 20, 10, 0x100], //  9 bBsy2  busy, 20 x 10, the other Palette, bit 1 too
      [pal, 4, 0x1000, 0, 0x10, 16, 16, 8, -1],   // 10 b16w   16 bpp, 16-bit word payload
      [pal, 4, 0x1000, 1, 0x10, 16, 16, 8, 0x200],// 11 b16Bsy 16 bpp, busy
      [0, 1, 0x1000, 1, 8, 16, 16, 8, 0x300],     // 12 bBnp   busy with no Palette: the client's
      [pal, 5, 0x1000, 0, 8, 16, 16, 8, -1],      // 13 bP     8 bpp, payload 0xf8..0xff only (drawM)
      [pal2, 5, 0x1000, 0, 8, 20, 20, 10, -1],    // 14 bP2    the same, 20 x 10, the other Palette
    ];
    const BSPN = ['bA', 'bB', 'bNP', 'bD', 'bNB', 'bW', 'bBsy', 'b16', 'bX', 'bBsy2', 'b16w', 'b16Bsy', 'bBnp', 'bP', 'bP2'];
    const spr = __keepAlloc(BSP.length * 0x80);
    BSP.forEach((s, i) => {
      const p = spr.add(i * 0x80);
      for (let o = 0; o < 0x80; o += 4) p.add(o).writeU32(0);
      p.writePointer(vtSprite);
      p.add(0x10).writePointer(s[0] ? s[0] : ptr(0));
      p.add(0x14).writePointer(s[1] < 0 ? ptr(0) : pay.add(s[1] * PAY + s[2]));
      p.add(0x18).writeS32(s[3]);
      p.add(0x1c).writePointer(s[8] < 0 ? ptr(0) : mkRows(s[8], s[6], s[7], s[5]));
      p.add(0x20).writeS32(s[4]);
      p.add(0x24).writeS32(0x40);
      p.add(0x28).writeS32(i === 10 || i === 11 ? 0xff : 0);
      p.add(0x2c).writeS32(s[5]);
      p.add(0x30).writeS32(s[6]);
      p.add(0x34).writeS32(s[7]);
      fx[BSPN[i]] = p;
    });
    fx.spr = spr;
    fx.spix = pay;
    fx.rows = rows;
    // lookup tables (c3ci): $t 16-bit entries masked to 0x1fff (0xe4100 bytes), $m a 256-byte index remap,
    // $b a 64 KB byte table, each with a second pointer 0x80 in
    const t = __keepAlloc(0xe4100);
    for (let i = 0; i < 0x72080; i++) t.add(i * 2).writeU16((i * 7919 + 13) & 0x1fff);
    const m = __keepAlloc(0x200);
    for (let i = 0; i < 0x200; i++) m.add(i).writeU8((i * 37 + 11) & 0xff);
    const b = __keepAlloc(0x10100);
    for (let i = 0; i < 0x10100; i++) b.add(i).writeU8((i * 97 + 29) & 0xff);
    fx.t = t; fx.t2 = t.add(0x80); fx.m = m; fx.m2 = m.add(0x80); fx.b = b; fx.b2 = b.add(0x80);
    const c8 = __keepAlloc(0x100);
    for (let i = 0; i < 0x100; i++) c8.add(i).writeU8((i * 151 + 71) & 0xff);
    fx.c8 = c8;
    const client = __keepAlloc(8);
    client.writeU32(0);
    client.add(4).writePointer(pal2);
    fx.client = client;
    // the module globals that are state regions (c3ci's list plus the run-length block)
    fx.gmask = base.add(0x128530); fx.gvstep = base.add(0x128540); fx.gtbl = base.add(0x128548);
    fx.gpos = base.add(0x128554); fx.glevel = base.add(0x128568); fx.gcols = base.add(0x128570);
    fx.gxstep = base.add(0x128580);
    fx.grle = base.add(0x1285a8);       // 0x101285a8..0x10128603, the drawRle16* block
    fx.g8vstep = base.add(0x128624); fx.g8tbl = base.add(0x12862c); fx.g8pos = base.add(0x12863c);
    fx.g8cols = base.add(0x128660); fx.g8colour = base.add(0x128664); fx.g8pix = base.add(0x12866c);
    fx.g8step = base.add(0x12867c); fx.g8xstep = base.add(0x128688);
    const rowPtr = base.add(0x122e20).readPointer();
    fx.growpair = rowPtr.isNull() ? base.add(0x12854c) : rowPtr.sub(4);
    const rowPtr8 = base.add(0x122e58).readPointer();
    fx.g8rowpair = rowPtr8.isNull() ? base.add(0x128634) : rowPtr8.sub(4);
    globalThis.__c3dcSpr = fx;
    return fx;
  }

  // c3ci's freeze wrapper on the shared helper: suspends the first thread, saves 0x101284d0..0x10128698 (every
  // loop global of the blitters reached here, the run-length block included), the scale globals 0x10122dc0..c8,
  // the palette client 0x1012873c, the colour-table slot 0x10128658, the two row-step pairs and jgld's
  // _CrtDbgMode[_CRT_WARN] (RVA 0x123378), then writes the scale, our client and 0 into the debug mode (c3ci: with
  // mode 2 the report's OutputDebugStringA raises DBG_PRINTEXCEPTION_C and Frida fails the rpc; with 0 the report
  // takes none of its output arms and returns 0, as in mode 2).
  function spriteFreeze(scale) {
    settle();
    const fx = spriteObjects();
    const base = jgld();
    const RANGES = [[base.add(0x1284d0), 0x1c8], [base.add(0x122dc0), 0xc], [base.add(0x12873c), 4],
                    [fx.growpair, 8], [fx.g8rowpair, 8], [base.add(0x123378), 4]];
    __freeze.thread();
    RANGES.forEach((r) => __freeze.save(r[0], r[1]));
    base.add(0x122dc0).writeS32(scale[0]);
    base.add(0x122dc4).writeS32(scale[1]);
    base.add(0x122dc8).writeS32(scale[2]);
    base.add(0x12873c).writePointer(fx.client);
    base.add(0x128658).writePointer(fx.c8);
    base.add(0x123378).writeS32(0);
    return fx;
  }

  // ============================================================================ libpng: info struct and text
  // A png_struct of our own ($png, 0x220 bytes, only tested for NULL / handed to png_malloc and png_free), info
  // structs with chosen num_text (+0x30), max_text (+0x34) and text (+0x38) words, caller text arrays of 16-byte
  // png_text {compression, key, text, text_length} and our own ASCII strings. png_malloc ends in jgld's malloc
  // 0x1007fb40 and png_free in free 0x100806d0, both served by the stand-in, so the arrays come from the compared
  // arena and the old array's release is a compared log entry. No module global is written: no freeze.
  //   I0 no array          I1 array (2 used of 4)    I2 array (1 used of 10)   I3 array (3 used of 6)
  //   T0 three texts ("alpha", NULL, "")   T1 one long text   T2 five texts   T3 two texts, both NULL
  function pngFx() {
    const h = standIn(0x800);
    const blk = __keepAlloc(0x1000);
    fill(blk, 0x1000, 0x6e67);
    const out = { arena: h.arena, freelog: h.log, blk: blk, obj: blk, png: blk };
    const str = (off, s) => { blk.add(off).writeUtf8String(s); return blk.add(off); };
    const sA = str(0xe00, 'alpha'), sB = str(0xe10, 'key-b'), sC = str(0xe20, 'a longer comment of forty-one chars....'),
          sE = str(0xe60, ''), sK = str(0xe70, 'k'), sZ = str(0xe80, 'zz');
    const infos = [[0, 0, false], [2, 4, true], [1, 10, true], [3, 6, true]];
    infos.forEach(function (s, i) {
      const info = blk.add(0x300 + i * 0x100);
      info.add(0x30).writeS32(s[0]);
      info.add(0x34).writeS32(s[1]);
      info.add(0x38).writePointer(s[2] ? blk.add(0x700 + i * 0x80) : ptr(0));
      out['I' + i] = info;
    });
    const mkText = (off, entries) => {
      const t = blk.add(off);
      entries.forEach(function (e, k) {
        t.add(k * 16).writeS32(e[0]);
        t.add(k * 16 + 4).writePointer(e[1]);
        t.add(k * 16 + 8).writePointer(e[2]);
        t.add(k * 16 + 12).writeU32(0x7777);
      });
      return t;
    };
    out.T0 = mkText(0xb00, [[0, sK, sA], [1, sB, ptr(0)], [-1, sZ, sE]]);
    out.T1 = mkText(0xb40, [[2, sB, sC]]);
    out.T2 = mkText(0xb60, [[0, sK, sA], [3, sB, sC], [-1, sZ, sB], [1, sK, sE], [5, sA, sZ]]);
    out.T3 = mkText(0xbc0, [[7, sK, ptr(0)], [8, sZ, ptr(0)]]);
    return out;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3dc_surfaces() { return surfaces(); },
    c3dc_png() { return pngFx(); },
    c3dc_spr_s111() { return spriteFreeze([1, 1, 1]); },
    c3dc_spr_s223() { return spriteFreeze([2, 2, 3]); },
    c3dc_lists() { return lists(); },
    c3dc_tracked_empty() { return tracked('empty'); },
    c3dc_tracked_one() { return tracked('one'); },
    c3dc_tracked_five() { return tracked('five'); },
    c3dc_tracked_fivehead() { return tracked('fivehead'); },
    c3dc_delall_empty() { return deleteAllFx('empty'); },
    c3dc_delall_one() { return deleteAllFx('one'); },
    c3dc_delall_three() { return deleteAllFx('three'); },
    c3dc_delall_five() { return deleteAllFx('five'); },
    c3dc_delall_fivecur3() { return deleteAllFx('fivecur3'); },
    c3dc_destroy_null_empty() { return destroyFx('null', 'empty'); },
    c3dc_destroy_base_empty() { return destroyFx('base', 'empty'); },
    c3dc_destroy_base_three() { return destroyFx('base', 'three'); },
    c3dc_destroy_null_five() { return destroyFx('null', 'five'); },
    c3dc_destroy_stub_one() { return destroyFx('stub', 'one'); },
    c3dc_atexit_set_five() { return atexitFx(0x01, 'five'); },
    c3dc_atexit_setff_one() { return atexitFx(0xff, 'one'); },
    c3dc_atexit_clear_empty() { return atexitFx(0x00, 'empty'); },
    c3dc_atexit_clear_one() { return atexitFx(0x80, 'one'); },
    c3dc_atexit_clear_five() { return atexitFx(0xfe, 'five'); },
    c3dc_display_objs() { return displayObjs(); },
    c3dc_release() { return releaseFx(); },
    c3dc_fmt1() { return fmtFx(1); },
    c3dc_fmt0() { return fmtFx(0); },
    c3dc_fmt2() { return fmtFx(2); },
    c3dc_fmtm1() { return fmtFx(-1); },
  });
})();
