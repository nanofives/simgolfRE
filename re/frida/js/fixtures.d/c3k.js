// Fixtures for the c3k batch (club-name builder, window inner-size, draw-target setter, record placer, corner
// heights, connected-path flood). Each seeds only the tables/globals the function and its callees read, with our
// own synthetic numbers; no live game list, device or string is handed to an A/B. Loaded after diff_fixtures.js;
// Memory.alloc is rewritten to __keepAlloc so allocations survive the run.
Object.assign(globalThis.DIFF_FIXTURES, {
  // appendClubName (0x0040a9a0): the text buffer 0x0051a068 is the state region, reset to empty per vector. The
  // club-name strings are read from the binary by the reimplementation, so nothing is seeded there.
  c3k_text_empty() { ptr('0x0051a068').writeUtf8String(''); return {}; },

  // Window::innerSize (0x0047cb10): ten fake window objects, each 0x200 bytes with flags at +0x9c, m180/m184/m188
  // at +0x180/+0x184/+0x188 and the child pointer +0x15c cleared so the vtable branch never runs. $w/$h are the
  // shared input cells (seeded 100/200) and the state regions.
  c3k_windows() {
    const win = (flags, m180, m184, m188) => {
      const o = Memory.alloc(0x200);
      o.add(0x9c).writeU32(flags);
      o.add(0x180).writeS32(m180);
      o.add(0x184).writeS32(m184);
      o.add(0x188).writeS32(m188);
      o.add(0x15c).writePointer(ptr(0));   // no child -> no vtable call
      return o;
    };
    const o0 = win(0x04, 0, 10, 7);     // bit 0x04: *h -= g
    const o1 = win(0x08, 0, 10, 7);     // bit 0x08: *w -= g
    const o2 = win(0x0c, 0, 10, 7);     // both
    const o3 = win(0x400, 0, 5, 7);     // 0x400 block, m188 != -1
    const o4 = win(0x400, 0, 5, -1);    // 0x400 block, m188 == -1 (no extra term)
    const o5 = win(0x01, 0, 8, 3);      // 0x11 block via bit 0
    const o6 = win(0x10, 2, 9, 5);      // 0x10: enters the 0x11 block and adds m184 - m180
    const o7 = win(0x11, 1, 6, 4);      // 0x11 block + m188 term + 0x10 term
    const o8 = win(0x00, 0, 3, 9);      // no flags: *w/*h unchanged
    const o9 = win(0x0c, 0, 25, 7);     // both, different border math
    const w = Memory.alloc(4), h = Memory.alloc(4);
    w.writeS32(100); h.writeS32(200);
    return { o0, o1, o2, o3, o4, o5, o6, o7, o8, o9, w, h };
  },

  // setDrawTarget (0x004762d0): a fake draw-context object (this) whose +0x5c..0x68 window is the state region
  // (seeded to 0 so writes show), plus two fake surface descriptors: $s0 with surf+4 == 0 (the +0x5c store is
  // skipped) and $s1 with surf+4 != 0 (stored).
  c3k_drawtarget() {
    const obj = Memory.alloc(0x100);
    for (let o = 0x5c; o < 0x6c; o += 4) obj.add(o).writeU32(0);
    const s0 = Memory.alloc(0x10); s0.add(4).writeS32(0);
    const s1 = Memory.alloc(0x10); s1.add(4).writeS32(1);
    return { obj, s0, s1 };
  },

  // placeRecord (0x00401040): footprint-definition bytes at 0x004c11e0 (stride 0x27) for types 0/1/2, and the
  // record banks at 0x004e6d20 (0x74 bytes/bank; 16 id words/bank). Bank 0 all free (id == -1), bank 1 slot 0
  // taken, bank 2 full. The whole 0x200 region is cleared first so the row/col/occupancy writes are observable.
  c3k_records() {
    const defs = [[0x01, 0x02, 0x04, 0x08, 0x10, 0x20],   // type 0: diagonal
                  [0x3f, 0x00, 0x3f, 0x00, 0x3f, 0x00],   // type 1: stripes
                  [0x07, 0x07, 0x07, 0x00, 0x00, 0x00]];  // type 2: 3x3 block
    for (let t = 0; t < defs.length; t++)
      for (let r = 0; r < 6; r++)
        ptr('0x004c11e0').add(t * 0x27 + r).writeU8(defs[t][r]);
    const banks = ptr('0x004e6d20');
    for (let i = 0; i < 0x200; i++) banks.add(i).writeU8(0);
    const id = (bank, slot) => banks.add(bank * 0x74 + slot * 2);
    for (let s = 0; s < 16; s++) id(0, s).writeS16(-1);          // bank 0: all free
    id(1, 0).writeS16(5);                                        // bank 1: slot 0 taken
    for (let s = 1; s < 16; s++) id(1, s).writeS16(-1);
    for (let s = 0; s < 16; s++) id(2, s).writeS16(0);           // bank 2: full (none == -1)
    return {};
  },

  // cornerHeights (0x0040bfe0): tile types at 0x005722e8 (mostly type 1 -> heightBlend; (6,6)..(10,10) take the
  // other flag branches), type flag dwords at 0x0057837c (stride 0x30), per-cell bytes at 0x00543018, the corner
  // cache at 0x0051b770 (8 bytes/cell), and heightBlend's inputs: 0x0059e7b8 = 0, 0x00834170 = 1 (so heightBlend
  // returns the 0x005a4998[x*51+y] byte) seeded with a varied pattern.
  c3k_corners() {
    ptr('0x0059e7b8').writeU32(0);
    ptr('0x00834170').writeU32(1);
    const type = ptr('0x005722e8');
    for (let i = 0; i < 2500; i++) type.add(i).writeS8(1);      // default type 1
    const setType = (x, y, t) => type.add(x * 50 + y).writeS8(t);
    setType(6, 6, 2); setType(7, 7, 3); setType(8, 8, 4); setType(9, 9, 5); setType(10, 10, 6);
    const flags = ptr('0x0057837c');
    const setFlags = (t, f) => flags.add(t * 0x30).writeU32(f);
    setFlags(0, 0); setFlags(1, 0); setFlags(2, 2); setFlags(3, 4); setFlags(4, 8); setFlags(5, 3); setFlags(6, 5);
    ptr('0x00543018').add(9 * 50 + 9).writeS8(42);
    ptr('0x00543018').add(10 * 50 + 10).writeS8(-7);
    const cache = ptr('0x0051b770');
    for (let i = 0; i < 2500 * 8; i++) cache.add(i).writeU8(0);
    cache.add((5 * 50 + 5) * 8 + 1).writeS8(7);                 // (5,5) corner 1 cache hit
    const blend = ptr('0x005a4998');
    for (let x = 0; x < 52; x++)
      for (let y = 0; y < 51; y++)
        blend.add(x * 51 + y).writeS8((x * 3 + y * 5) & 0x3f);
    return {};
  },

  // propagateType11 (0x0042f1c0): the path grid at 0x0056988c (50x50 bytes, the state region) is all 0 except cell
  // (22,22) = 5; the tile types at 0x005722e8 are type 2 everywhere except a 5x5 block (x,y in 20..24) of type 1;
  // the type +2 flag bytes at 0x00578376 (stride 0x30) are 0x11 for type 1 and 0 for type 2. Neither type is 0x14,
  // so tileBlocked reports every in-bounds cell open.
  c3k_pathgrid() {
    const path = ptr('0x0056988c');
    for (let i = 0; i < 2500; i++) path.add(i).writeS8(0);
    path.add(22 * 50 + 22).writeS8(5);
    const type = ptr('0x005722e8');
    for (let i = 0; i < 2500; i++) type.add(i).writeS8(2);
    for (let x = 20; x <= 24; x++)
      for (let y = 20; y <= 24; y++)
        type.add(x * 50 + y).writeS8(1);
    ptr('0x00578376').add(1 * 0x30).writeS8(0x11);   // type 1: +2 == 0x11
    ptr('0x00578376').add(2 * 0x30).writeS8(0x00);   // type 2: +2 == 0
    return {};
  },
});
