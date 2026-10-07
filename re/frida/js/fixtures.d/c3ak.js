// Batch c3ak fixtures (jgld.dll). Every block is allocated here; nothing live in the running jgld.dll is handed
// to the A/B. Layouts come from the functions' own field offsets (see shim/src/re/c3ak.cpp).
Object.assign(globalThis.DIFF_FIXTURES, {
  // 12 RECTs {left, top, right, bottom} in one block ($r0 is its base, $rN = base + 0x10*N) plus two scratch RECTs
  // for the writers. The set covers zero area, positive and negative coordinates, inverted rects (right < left,
  // bottom < top) and the extremes of int32 so right-left / bottom-top wrap in both directions.
  c3ak_rects() {
    const vals = [
      [0, 0, 0, 0],
      [0, 0, 1, 1],
      [0, 0, 800, 600],
      [10, 20, 10, 20],
      [10, 20, 9, 19],
      [-5, -7, 5, 7],
      [-100, -200, -50, -150],
      [5, 5, -5, -5],
      [0x7fffffff, 0x7fffffff, -0x80000000, -0x80000000],
      [-0x80000000, -0x80000000, 0x7fffffff, 0x7fffffff],
      [0, 0, 0x7fffffff, 0x7fffffff],
      [1, 2, 3, 4],
    ];
    const blk = Memory.alloc(vals.length * 0x10);
    const out = {};
    vals.forEach((v, i) => {
      const p = blk.add(i * 0x10);
      for (let k = 0; k < 4; k++) p.add(k * 4).writeS32(v[k]);
      out['r' + i] = p;
    });
    out.w = Memory.alloc(0x10);
    out.w2 = Memory.alloc(0x10);
    for (let k = 0; k < 4; k++) { out.w.add(k * 4).writeS32(-1); out.w2.add(k * 4).writeS32(-1); }
    return out;
  },

  // 12 objects of 0x40 bytes in one block, used both as Sprites (getFlag18 reads [this+0x18]) and as the target of
  // setField28 ([this+0x28]). $s0 / $t0 are the block base, so the single state region ("$t0", 0, 12*0x40) covers
  // every object. The [+0x18] words alternate bit 0 so the masked result is 1 for half the vectors and 0 for the
  // rest, with other bits set to show that only bit 0 survives the mask.
  c3ak_objs() {
    const flags = [0, 1, 2, 3, 0xfffffffe, 0xffffffff, 0x80000000, 0x80000001, 0x55555555, 0xaaaaaaaa, 0x10, 0x11];
    const blk = Memory.alloc(flags.length * 0x40);
    const out = {};
    flags.forEach((f, i) => {
      const p = blk.add(i * 0x40);
      p.add(0x18).writeU32(f);
      p.add(0x28).writePointer(ptr(0x11110000 + i));   // a distinct value setField28 must overwrite
      out['s' + i] = p;
      out['t' + i] = p;
    });
    return out;
  },

  // A 16-byte pattern buffer for the two big-endian loads: $bN = base + N, so the 12 vectors read 12 different
  // byte quadruples (0x00, 0xff, 0x80 and 0x7f appear as the top byte, which is the sign bit of png_get_int_32).
  // Plus the libpng info/png scratch blocks: $png is only tested for null, $infoP and $infoL are written at
  // +8, +0x10, +0x14, +0x70, +0x74 and +0x78, all inside the 0x80-byte state region. $pal is a dummy palette
  // pointer that png_set_PLTE only stores.
  c3ak_png() {
    const bytes = [0x00, 0x01, 0x02, 0x7f, 0x80, 0xff, 0xfe, 0x12, 0x34, 0x56, 0x78, 0x00, 0xff, 0x80, 0x01, 0x7f];
    const buf = Memory.alloc(bytes.length);
    bytes.forEach((b, i) => buf.add(i).writeU8(b));
    const out = {};
    for (let i = 0; i < 12; i++) out['b' + i] = buf.add(i);
    out.png = Memory.alloc(0x40);
    out.pal = Memory.alloc(0x40);
    out.infoP = Memory.alloc(0x100);
    out.infoL = Memory.alloc(0x100);
    // non-zero starting contents, so a missed write or a wrong field shows up in the state hash
    for (const p of [out.infoP, out.infoL]) for (let o = 0; o < 0x100; o += 4) p.add(o).writeU32(0x33333333);
    return out;
  },

  // Random seeds: 12 four-byte seeds for next() in one block ($g0 is its base) and 4 more for range() ($h0 base).
  // The seeds include 0 (the value Random's constructor writes) and values whose next LCG step crosses the 32-bit
  // wrap, so the returned [0, 1) value differs per vector.
  c3ak_random() {
    const next = [0, 1, 2, 12345, 0x3039, 0x7fffffff, 0x80000000, 0xffffffff, 0x41c64e6d, 0xdeadbeef,
                  0x12345678, 0xcafebabe];
    const g = Memory.alloc(next.length * 4);
    const out = {};
    next.forEach((s, i) => { g.add(i * 4).writeU32(s); out['g' + i] = g.add(i * 4); });
    // $h0 drives the n sweep of range(), so its seed must advance to a non-zero next(): seed 0 steps to 0x3039,
    // whose bits 16..30 are 0, and every n would return 0. $h1 keeps that zero case on purpose.
    const rng = [12345, 0, 0x7fffffff, 0xdeadbeef];
    const h = Memory.alloc(rng.length * 4);
    rng.forEach((s, i) => { h.add(i * 4).writeU32(s); out['h' + i] = h.add(i * 4); });
    return out;
  },
});
