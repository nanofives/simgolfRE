// Fixtures for C3 batch c3q (postEvent, relaxTile42f630, freeRecordAtPos, pushTripleEntry, appendEndearment,
// appendRatingLabel, addHabitatSlot, canStep, nearestHole; rankValue needs none). Only our own synthetic values (no
// game text). Names are prefixed c3q_. Every fixture returns {obj}, the first table it seeds.
Object.assign(globalThis.DIFF_FIXTURES, {
  // postEvent 0x0046e7b0. Flags dword 0x0059e7b8: bit 0x4000000 set (flagSet) or cleared, other bits kept. Event
  // records 0x004c15a0 + id*0x30 for id 0..15: +0 = 0 for even ids and id*3 + 1 for odd ids, +4 = 0x1000 + id. Tick
  // dword 0x00834170 = 0x2468, course dword 0x0059bf90 = 3. Outputs pre-set so every store is visible:
  // 0x004e3db8 = 0x7777, 0x004e3dbc = 0x6666, 0x008392a4 = 0x5555, 0x00839338 = 0x4444.
  _c3q_event(flagSet) {
    const fl = ptr('0x0059e7b8');
    const v = fl.readU32();
    fl.writeU32(flagSet ? (v | 0x4000000) >>> 0 : (v & ~0x4000000) >>> 0);
    const t = ptr('0x004c15a0');
    for (let id = 0; id < 16; id++) {
      t.add(id * 0x30).writeS32(id & 1 ? id * 3 + 1 : 0);
      t.add(id * 0x30 + 4).writeS32(0x1000 + id);
    }
    ptr('0x00834170').writeS32(0x2468);
    ptr('0x0059bf90').writeS32(3);
    ptr('0x004e3db8').writeS32(0x7777);
    ptr('0x004e3dbc').writeS32(0x6666);
    ptr('0x008392a4').writeS32(0x5555);
    ptr('0x00839338').writeS32(0x4444);
    return { obj: t };
  },
  c3q_event_on() { return globalThis.DIFF_FIXTURES._c3q_event(false); },
  c3q_event_off() { return globalThis.DIFF_FIXTURES._c3q_event(true); },

  // relaxTile42f630 0x0042f630 and canStep 0x00407400 share the tile tables (50 x 50, index x*50 + y).
  // relax: type bytes 0x005722e8 = 0x14 (blocked) where (x*3 + y*5) % 11 == 0, else ((x >> 2) + (y >> 2)) % 3 + 1,
  // so 4x4 blocks of one type with borders between them; signed heights 0x00543018 = (x*5 + y*7) % 23 - 11.
  c3q_relax() {
    const ty = ptr('0x005722e8'), h = ptr('0x00543018');
    for (let x = 0; x < 50; x++) for (let y = 0; y < 50; y++) {
      const i = x * 50 + y;
      ty.add(i).writeU8((x * 3 + y * 5) % 11 === 0 ? 0x14 : ((x >> 2) + (y >> 2)) % 3 + 1);
      h.add(i).writeS8((x * 5 + y * 7) % 23 - 11);
    }
    return { obj: h };
  },
  // step: type bytes 0x005722e8 = (x*7 + y*3) % 24 (types 0..23, including 0x14, 0x15, 0x16); type byte +2
  // (0x00578372 + t*0x30) for t = 0..23: 0 when t % 4 == 0 (except 0x14, which gets 5 so tileBlocked decides it),
  // -1 when t % 4 == 3, else t + 1; flag words 0x0053caf0 = 0x100 where i % 7 == 0, else 0x20 where i % 11 == 0, else
  // 0x40 where i % 13 == 0, else 0 (i = x*50 + y).
  c3q_step() {
    const ty = ptr('0x005722e8'), fw = ptr('0x0053caf0'), tr = ptr('0x00578372');
    for (let x = 0; x < 50; x++) for (let y = 0; y < 50; y++) {
      const i = x * 50 + y;
      ty.add(i).writeU8((x * 7 + y * 3) % 24);
      fw.add(i * 2).writeU16(i % 7 === 0 ? 0x100 : i % 11 === 0 ? 0x20 : i % 13 === 0 ? 0x40 : 0);
    }
    for (let t = 0; t < 24; t++)
      tr.add(t * 0x30).writeS8(t === 0x14 ? 5 : t % 4 === 0 ? 0 : t % 4 === 3 ? -1 : t + 1);
    return { obj: ty };
  },

  // freeRecordAtPos 0x004011b0: the 100 records of 0x3c bytes at 0x0056d1d8: word +0 = 0x100 + i, word +6 = i % 10,
  // word +8 = (i * 3) % 7, except record 99 (+6 = +8 = -1) and record 98 (+6 = 0x7fff, +8 = -0x8000).
  c3q_records() {
    const b = ptr('0x0056d1d8');
    for (let i = 0; i < 100; i++) {
      const r = b.add(i * 0x3c);
      r.writeU16(0x100 + i);
      r.add(6).writeS16(i === 99 ? -1 : i === 98 ? 0x7fff : i % 10);
      r.add(8).writeS16(i === 99 ? -1 : i === 98 ? -0x8000 : (i * 3) % 7);
    }
    return { obj: b };
  },

  // pushTripleEntry 0x00409cb0: count dword 0x005a9cd4 = n.
  _c3q_triple(n) { ptr('0x005a9cd4').writeS32(n); return { obj: ptr('0x00586b50') }; },
  c3q_triple_n0() { return globalThis.DIFF_FIXTURES._c3q_triple(0); },
  c3q_triple_n7() { return globalThis.DIFF_FIXTURES._c3q_triple(7); },
  c3q_triple_n255() { return globalThis.DIFF_FIXTURES._c3q_triple(255); },
  c3q_triple_n256() { return globalThis.DIFF_FIXTURES._c3q_triple(256); },
  c3q_triple_big() { return globalThis.DIFF_FIXTURES._c3q_triple(0x7fffffff); },

  // appendEndearment 0x00467560 / appendRatingLabel 0x004532a0: our own text in the buffer 0x0051a068, either empty
  // or "q3" (the whole 0x40-byte region is zeroed first).
  _c3q_text(s) {
    const b = ptr('0x0051a068');
    for (let i = 0; i < 0x40; i++) b.add(i).writeU8(0);
    b.writeUtf8String(s);
    return { obj: b };
  },
  c3q_text_empty() { return globalThis.DIFF_FIXTURES._c3q_text(''); },
  c3q_text_q3() { return globalThis.DIFF_FIXTURES._c3q_text('q3'); },

  // addHabitatSlot 0x00405970: the 128 slots of 0x14 bytes at 0x00572cb0, every byte = (i*7 + k) & 0x7f (k = byte
  // offset in the slot), then the marker byte +0x12 = 0xff for the free slots `free` and 0 for the others; generator
  // seed dword 0x00822d9c = seed.
  _c3q_habitat(free, seed) {
    const b = ptr('0x00572cb0');
    for (let i = 0; i < 128; i++) {
      const s = b.add(i * 0x14);
      for (let k = 0; k < 0x14; k++) s.add(k).writeU8((i * 7 + k) & 0x7f);
      s.add(0x12).writeU8(free.indexOf(i) >= 0 ? 0xff : 0);
    }
    ptr('0x00822d9c').writeU32(seed);
    return { obj: b };
  },
  c3q_habitat_5() { return globalThis.DIFF_FIXTURES._c3q_habitat([5, 9, 60], 0x1234567); },
  c3q_habitat_0() { return globalThis.DIFF_FIXTURES._c3q_habitat([0, 1], 0x89abcdef); },
  c3q_habitat_127() { return globalThis.DIFF_FIXTURES._c3q_habitat([127], 0x2468ace); },
  c3q_habitat_full() { return globalThis.DIFF_FIXTURES._c3q_habitat([], 0x13579bd); },

  // nearestHole 0x00407340: hole records 0x00575cc0 + k*0x208 (k = 0..17, hole k + 1): dwords +0/+4 = (5 + 2k,
  // 40 - 2k) and +0x10/+0x14 = (8 + 2k, 6 + k); holes 7 and 8 (k = 6, 7) share their +0/+4 point (17, 28) so a tie
  // keeps the earlier one. Pin entries 0x0059aea8 + k*0x18: +0 = -1 except k = 3 ((30 << 10) + 0x200, +4 =
  // (44 << 10) + 0x200) and k = 12 ((2 << 10) + 0x3ff, +4 = (3 << 10)). Variant far: every point at 200000 + k and
  // every pin -1, so no distance is below 0xffff.
  _c3q_holes(far) {
    const r = ptr('0x00575cc0'), p = ptr('0x0059aea8');
    for (let k = 0; k < 18; k++) {
      const h = r.add(k * 0x208);
      const tx = k === 7 ? 17 : 5 + 2 * k, tyy = k === 7 ? 28 : 40 - 2 * k;
      h.writeS32(far ? 200000 + k : tx); h.add(4).writeS32(far ? 200000 + k : tyy);
      h.add(0x10).writeS32(far ? 200000 + k : 8 + 2 * k); h.add(0x14).writeS32(far ? 200000 + k : 6 + k);
      const q = p.add(k * 0x18);
      q.writeS32(-1); q.add(4).writeS32(0);
      if (!far && k === 3) { q.writeS32((30 << 10) + 0x200); q.add(4).writeS32((44 << 10) + 0x200); }
      if (!far && k === 12) { q.writeS32((2 << 10) + 0x3ff); q.add(4).writeS32(3 << 10); }
    }
    return { obj: r };
  },
  c3q_holes() { return globalThis.DIFF_FIXTURES._c3q_holes(false); },
  c3q_holes_far() { return globalThis.DIFF_FIXTURES._c3q_holes(true); },
});
