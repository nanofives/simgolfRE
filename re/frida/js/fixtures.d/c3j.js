// Fixtures for C3 batch c3j (golfer stat trend, two tile/object scans, two Snd field writers, a Snd ctor).
// Each seeds only the tables/globals its function (and its callees) read, with our own synthetic numbers; the
// running game's live lists and devices are never handed to an A/B. Constructor/field-writer arenas are fresh
// Memory.alloc blocks. Loaded after diff_fixtures.js; Memory.alloc is rewritten to __keepAlloc so it survives.
Object.assign(globalThis.DIFF_FIXTURES, {
  // Non-zero per-byte garbage so a reimplementation that omits a field (leaving the garbage) reads RED.
  _c3jGarbage(base, size, seed) {
    for (let i = 0; i < size; i++) base.add(i).writeU8(((seed * 131 + i * 37 + 0x5b) & 0xfe) | 1);
  },
  _c3jZero(base, size) {
    let i = 0;
    for (; i + 4 <= size; i += 4) base.add(i).writeU32(0);
    for (; i < size; i++) base.add(i).writeU8(0);
  },

  // computeRatingTrend 0x004060a0: for idx 0..3, the signed byte at 0x00575ab0 + idx*0x208 and the signed words at
  // 0x00575ada + idx*0x208 + (outer-1)*2 + j*0x16 (outer 1..9, j 0..7). Every word is 1..13 so each l20[j] (= 8 +
  // sum) is positive and the sel 1/2/4 branches all proceed; the per-idx pattern makes the result differ by idx.
  c3j_rating() {
    for (let idx = 0; idx < 4; idx++) {
      const rec = idx * 0x208;
      ptr('0x00575ab0').add(rec).writeS8((idx * 7 + 3) & 0x7f);
      const sbase = ptr('0x00575ada').add(rec);
      for (let outer = 1; outer <= 9; outer++)
        for (let j = 0; j < 8; j++)
          sbase.add((outer - 1) * 2 + j * 0x16).writeS16(((idx * 5 + outer * 3 + j * 7) % 13) + 1);
    }
    return {};
  },

  // holeQuadrants 0x00407000: the 256 placed-object records at 0x0058bcb8 (stride 0x10). Record fields read: type
  // word +0 (4 = counted), objY word +2, objX word +4, field int +8 (< 0x10). Only our synthetic objects are
  // written; the rest of the table is zeroed. tileDistance(a, b, objY, objX) measures (a, b) against each object.
  c3j_holes() {
    const base = ptr('0x0058bcb8');
    globalThis.DIFF_FIXTURES._c3jZero(base, 256 * 0x10);
    const put = (i, type, objY, objX, field) => {
      const r = base.add(i * 0x10);
      r.writeS16(type); r.add(2).writeS16(objY); r.add(4).writeS16(objX); r.add(8).writeS32(field);
    };
    put(0, 4, 10, 10, 0);   // bit 1
    put(1, 4, 10, 20, 1);   // bit 2
    put(2, 4, 20, 10, 2);   // bit 4
    put(3, 4, 20, 20, 3);   // bit 8
    put(4, 0, 0, 0, 0);     // not type 4 -> ignored
    put(5, 4, 10, 11, 1);   // bit 2, close to object 0
    put(6, 4, 30, 30, 0);   // bit 1, isolated
    return {};
  },

  // findNearestTargetTile 0x0040de70: the tile-type grid 0x005722e8 (byte[x*50+y]; 0x14 = blocked) and the tile
  // flags grid 0x0053caf0 (word[x*50+y]; bit 0x200 = target). All tiles are type 0 (unblocked) and flag 0 except a
  // handful of target cells; one target is also made blocked (type 0x14) to show it is skipped.
  c3j_nearest() {
    globalThis.DIFF_FIXTURES._c3jZero(ptr('0x005722e8'), 50 * 50);      // types: all 0 (unblocked)
    globalThis.DIFF_FIXTURES._c3jZero(ptr('0x0053caf0'), 50 * 50 * 2);  // flags: all 0
    const flag = (x, y) => ptr('0x0053caf0').add((x * 50 + y) * 2).writeU16(0x200);
    const block = (x, y) => ptr('0x005722e8').add(x * 50 + y).writeU8(0x14);
    [[8, 8], [12, 12], [16, 16], [20, 20], [10, 20], [20, 10]].forEach(([x, y]) => flag(x, y));
    block(16, 16);  // a flagged tile that is blocked -> never chosen
    return {};
  },

  // Snd arenas: fresh 0x80-byte slots filled with garbage; the whole arena is the state region, so a forgotten
  // field keeps its garbage and reads RED, and the tail past the written fields varies the final state per slot.
  _c3jSndArena(n, prep) {
    const SLOT = 0x80;
    const arena = Memory.alloc(SLOT * n);
    const fx = { obj: arena };
    for (let i = 0; i < n; i++) {
      const o = arena.add(i * SLOT);
      globalThis.DIFF_FIXTURES._c3jGarbage(o, SLOT, i + 1);
      if (prep) prep(o);
      fx['o' + i] = o;
    }
    return fx;
  },
  // setVolume 0x00485140: device pointer this+0x40 held null and this+0x3c >= 0x10, so only this+4 is written.
  c3j_snd_vol() {
    return globalThis.DIFF_FIXTURES._c3jSndArena(10, o => { o.add(0x40).writeU32(0); o.add(0x3c).writeU32(0x7f); });
  },
  // setField34 0x004846b0: device pointer this+0x40 held null, so only this+0x34 is written.
  c3j_snd_f34() {
    return globalThis.DIFF_FIXTURES._c3jSndArena(10, o => { o.add(0x40).writeU32(0); });
  },
  // ctor485260 0x00485260: no device dependency; writes this+0..this+0x30 and returns this.
  c3j_snd_ctor() {
    return globalThis.DIFF_FIXTURES._c3jSndArena(10, null);
  },
});
