// Fixtures for diff_hook.js: each returns { obj: NativePointer, ... } built in game memory.
globalThis.DIFF_FIXTURES = {
  // Fake Terrain: only the fields tileAt reads are meaningful (+0x14 width = 10, +0x18 height = 8).
  terrain_10x8() {
    const obj = Memory.alloc(0x3a4 + 10 * 8 * 0x248);
    obj.add(0x14).writeS32(10);
    obj.add(0x18).writeS32(8);
    return { obj };
  },
  // Tile type table of golf_clean.exe (0x005722e8, [x][y] bytes, 50x50; read by tileBlocked 0x0040bf60):
  // pattern (x*7 + y*3) & 0x1f, so type 0x14 (the blocked value) occurs at many cells. Main-menu state only.
  tile_types_pattern() {
    const t = ptr('0x005722e8');
    for (let x = 0; x < 50; x++)
      for (let y = 0; y < 50; y++)
        t.add(x * 50 + y).writeU8((x * 7 + y * 3) & 0x1f);
    return { obj: t };
  },
  // Seeds the tables the batch-2 lookups read (main-menu state only), so every branch is reachable:
  // tile types as tile_types_pattern; wall masks 0x005619a0 = (x*5 + y*11) & 0xff; wall heights per type
  // (0x00578378 + t*0x30) = 0 when t % 3 == 0, else +t / -t; golfers 0..15 (0x0057956e + g*0x100): type word
  // g % 12, flags word g; golfer-type byte 0x004d60a9 + t*0x230 gets bit 7 set for odd t (t < 12); cell tables
  // 0..3: base (0x005a9370) = -1, 0, 10, 100 and width (0x0053f3e8) = 5, 0, 7, 3.
  golf_tables() {
    globalThis.DIFF_FIXTURES.tile_types_pattern();
    const mask = ptr('0x005619a0'), wall = ptr('0x00578378');
    for (let x = 0; x < 50; x++)
      for (let y = 0; y < 50; y++)
        mask.add(x * 50 + y).writeU8((x * 5 + y * 11) & 0xff);
    for (let t = 0; t < 32; t++)
      wall.add(t * 0x30).writeS8(t % 3 === 0 ? 0 : (t % 2 ? t : -t));
    for (let g = 0; g < 16; g++) {
      ptr('0x0057956e').add(g * 0x100).writeS16(g % 12);
      ptr('0x00579570').add(g * 0x100).writeS16(g);
    }
    for (let t = 0; t < 12; t++) {
      const b = ptr('0x004d60a9').add(t * 0x230);
      Memory.protect(b, 1, 'rw-');
      b.writeU8(t % 2 ? (b.readU8() | 0x80) : (b.readU8() & 0x7f));
    }
    [-1, 0, 10, 100].forEach((v, i) => ptr('0x005a9370').add(i * 4).writeS32(v));
    [5, 0, 7, 3].forEach((v, i) => ptr('0x0053f3e8').add(i * 4).writeS32(v));
    return { obj: ptr('0x005722e8') };
  },
  // 16 Random objects (a seed dword each) for Random::next 0x0045c1a0: s0..s15 cover 0, 1, sign/carry edges and
  // arbitrary values. State region: $obj + 0, 64 bytes.
  rng_seeds() {
    const seeds = [0, 1, 0x3039, 0x7fff, 0x8000, 0xffff, 0x10000, 0x41c64e6d, 0x7fffffff, 0x80000000, 0xfffffffe,
                   0xffffffff, 0x12345678, 0xdeadbeef, 0x0badf00d, 0xa5a5a5a5];
    const obj = Memory.alloc(seeds.length * 4);
    const fx = { obj };
    seeds.forEach((s, i) => { obj.add(i * 4).writeU32(s >>> 0); fx['s' + i] = obj.add(i * 4); });
    return fx;
  },
  // Current course (dword 0x0059bf90) = 0 with kind byte (0x00571ff7) 2 / 0, plus the tile pattern and a flags
  // pattern (0x0053caf0 words = 0xffff ^ cell) so clearTile 0x00470a10's writes are visible.
  course_kind2() { return globalThis.DIFF_FIXTURES._course(2); },
  course_kind0() { return globalThis.DIFF_FIXTURES._course(0); },
  _course(kind) {
    globalThis.DIFF_FIXTURES.tile_types_pattern();
    ptr('0x0059bf90').writeS32(0);
    ptr('0x00571ff7').writeS8(kind);
    for (let i = 0; i < 2500; i++) ptr('0x0053caf0').add(i * 2).writeU16((0xffff ^ i) & 0xffff);
    return { obj: ptr('0x005722e8') };
  },
  // Batch 3 writers. Our own ASCII in the text buffer 0x0051a068 (no game text is written or recorded).
  _text(s) { ptr('0x0051a068').writeUtf8String(s); },
  // Message ring (queueMessage 0x0040c720 / clearMatching 0x0040c860): a = i % 3, b = i % 2, ttl = 0x30 - i,
  // write index 0x0053bba8 = n.
  _msgs(n) {
    globalThis.DIFF_FIXTURES._text('ab-msg');
    for (let i = 0; i < 8; i++) {
      ptr('0x0056c770').add(i * 4).writeS32(i % 3);
      ptr('0x0056c794').add(i * 4).writeS32(i % 2);
      ptr('0x0056a924').add(i * 4).writeS32(0x30 - i);
    }
    ptr('0x0053bba8').writeS32(n);
    return { obj: ptr('0x0056c770') };
  },
  msg_ring_n7() { return globalThis.DIFF_FIXTURES._msgs(7); },
  msg_ring_n3() { return globalThis.DIFF_FIXTURES._msgs(3); },
  // Popup ring (pointsPopup 0x0040c890): flags 0x0059e7b8 bit 0x1000000 set or clear, write index 0x0059abb0 = n,
  // the who records 0x00575ca0 + who*0x208 (who 0..3) = 1000 + who.
  _pops(flag, n) {
    const fl = ptr('0x0059e7b8');
    fl.writeU32(flag ? (fl.readU32() | 0x1000000) : (fl.readU32() & ~0x1000000) >>> 0);
    ptr('0x0059abb0').writeS32(n);
    for (let w = 0; w < 4; w++) ptr('0x00575ca0').add(w * 0x208).writeS32(1000 + w);
    return { obj: ptr('0x00542dd8') };
  },
  pop_ring_n7() { return globalThis.DIFF_FIXTURES._pops(false, 7); },
  pop_ring_n2() { return globalThis.DIFF_FIXTURES._pops(false, 2); },
  pop_ring_off() { return globalThis.DIFF_FIXTURES._pops(true, 4); },
  // logTick 0x0040c6f0: tick dword 0x00834170 = 0x12345 (slot (0x12345 / 1024) % 500 = 72).
  tick_12345() { ptr('0x00834170').writeS32(0x12345); return { obj: ptr('0x00568600') }; },
  // resetGolfer 0x00426670: golfers as golf_tables (type g % 12) and the 100 queue entries at 0x005689e8
  // (word +0 = i, word +4 = i % 13).
  golfer_queue() {
    globalThis.DIFF_FIXTURES.golf_tables();
    for (let i = 0; i < 100; i++) {
      ptr('0x005689e8').add(i * 8).writeS16(i);
      ptr('0x005689e8').add(i * 8 + 4).writeS16(i % 13);
    }
    return { obj: ptr('0x005689e8') };
  },
  text_ab() { globalThis.DIFF_FIXTURES._text('ab'); return { obj: ptr('0x0051a068') }; },
  // Terrain readers (slopeX/slopeY/slopeMix/heightAt): golf_tables, plus corner heights 1..7 in the 8-byte-per-cell
  // table cornerHeights 0x0040bfe0 returns first (0x0051b770 + (x*50 + y)*8 + corner = (x*3 + y*5 + c*3) % 7 + 1),
  // type flags dwords 0x0057837c + t*0x30 by t % 6: 8 (flat), 2 and 4 (ramp paths), 0, 0, 1; bit 0 of 0x0059e7b8
  // cleared.
  terrain_slopes() {
    globalThis.DIFF_FIXTURES.golf_tables();
    const c = ptr('0x0051b770');
    for (let x = 0; x < 50; x++)
      for (let y = 0; y < 50; y++)
        for (let k = 0; k < 8; k++)
          c.add((x * 50 + y) * 8 + k).writeS8((x * 3 + y * 5 + k * 3) % 7 + 1);
    for (let t = 0; t < 32; t++) ptr('0x0057837c').add(t * 0x30).writeU32([8, 2, 4, 0, 0, 1][t % 6]);
    const fl = ptr('0x0059e7b8');
    fl.writeU8(fl.readU8() & 0xfe);
    return { obj: ptr('0x005722e8') };
  },
  // rateTile 0x00422530: golf_tables, then golfer records i = 0..0x9f (0x005794c0 + i*0x100) with varied fields
  // (+0x11 flag = i & 1, +0xb2 = (i*7) % 11 - 3, position +0xcc/+0xd0 on tiles (i % 50, (i*7) % 50), +0x10 = i % 3,
  // +0x0e = i % 4, +0xe8 = i % 9, +0xe9 = i % 11, +0x2e = i % 7 - 2, +0x1a = i % 2), type byte +2 (0x00578372 +
  // t*0x30) = t % 5 - 1 for t < 32, 0x00543cc8 = 2, mode dword 0x00822c88 = mode.
  _ratings(mode) {
    globalThis.DIFF_FIXTURES.golf_tables();
    for (let i = 0; i < 0xa0; i++) {
      const g = ptr('0x005794c0').add(i * 0x100);
      g.add(0x11).writeU8(i & 1); g.add(0xb2).writeS8((i * 13) % 50 - 10);
      g.add(0xcc).writeS32((i % 50) * 1024 + 300); g.add(0xd0).writeS32(((i * 7) % 50) * 1024 + 700);
      g.add(0x10).writeU8(i % 3); g.add(0x0e).writeU16(i % 4); g.add(0xe8).writeU8(i % 9);
      g.add(0xe9).writeU8(i % 11); g.add(0x2e).writeS8(i % 7 - 2); g.add(0x1a).writeU8(i % 2);
    }
    for (let k = 0; k < 32; k++) ptr('0x00578372').add(k * 0x30).writeS8(k % 5 - 1);
    ptr('0x00543cc8').writeS32(2);
    ptr('0x00822c88').writeS32(mode);
    return { obj: ptr('0x005794c0') };
  },
  ratings_mode0() { return globalThis.DIFF_FIXTURES._ratings(0); },
  ratings_mode1() { return globalThis.DIFF_FIXTURES._ratings(1); },
  // heightBlend 0x0040c170: golf_tables; bit 0 of 0x0059e7b8 cleared; dword 0x00834170 = tick; current course 0 with
  // bytes +2 / +4 (0x00571ff6 / 0x00571ff8) = m2 / m4; 0x00822c88 = 1; table 0x005a4998 (51 per row) = (x + y) % 13;
  // height grid 0x00838c1c = (i * 13) % 50 - 10.
  _blend(tick, m2, m4) {
    globalThis.DIFF_FIXTURES.golf_tables();
    const fl = ptr('0x0059e7b8'); fl.writeU8(fl.readU8() & 0xfe);
    ptr('0x00834170').writeS32(tick);
    ptr('0x0059bf90').writeS32(0);
    ptr('0x00571ff6').writeS8(m2); ptr('0x00571ff8').writeS8(m4);
    ptr('0x00822c88').writeS32(1);
    for (let x = 0; x < 50; x++) for (let y = 0; y < 51; y++) ptr('0x005a4998').add(x * 51 + y).writeU8((x + y) % 13);
    // the 16 x 16 (19-byte rows) signed height grid bilinearSample 0x004674c0 reads for sampleHeight 0x0042dba0
    for (let i = 0; i < 19 * 18; i++) ptr('0x00838c1c').add(i).writeS8((i * 13) % 50 - 10);
    return { obj: ptr('0x005722e8') };
  },
  blend_c2() { return globalThis.DIFF_FIXTURES._blend(0, 2, 1); },
  blend_c0() { return globalThis.DIFF_FIXTURES._blend(0, 0, 0); },
  blend_tick() { return globalThis.DIFF_FIXTURES._blend(5, 2, 1); },
  // evalPlacementArea 0x0040db90: golf_tables; flags words 0x0053caf0 = 0x400 every 17th cell, 0x8000 every 41st,
  // 0x0080 every 53rd, else 0; course type byte 0x005a34e0 = ct; type bytes +0 / +2 (0x00578374 / 0x00578376) =
  // t % 4 - 1 / (t % 3 ? 13 : 0); placed objects: record r < 8 at 16-byte records 0x0058bcb8 = {type r % 5, x 5r, y 3r},
  // the rest type -1 (empty).
  _place(ct) {
    globalThis.DIFF_FIXTURES.golf_tables();
    for (let i = 0; i < 2500; i++)
      ptr('0x0053caf0').add(i * 2).writeU16(i % 17 === 0 ? 0x400 : i % 41 === 0 ? 0x8000 : i % 53 === 0 ? 0x80 : 0);
    ptr('0x005a34e0').writeS8(ct);
    for (let t = 0; t < 32; t++) {
      ptr('0x00578374').add(t * 0x30).writeS8(t % 4 - 1);
      ptr('0x00578376').add(t * 0x30).writeS8(t % 3 ? 13 : 0);
    }
    for (let r = 0; r < 256; r++) {
      const o = ptr('0x0058bcb8').add(r * 16);
      o.writeS16(r < 8 ? r % 5 : -1); o.add(2).writeS16(5 * r); o.add(4).writeS16(3 * r);
    }
    return { obj: ptr('0x005722e8') };
  },
  place_ct0() { return globalThis.DIFF_FIXTURES._place(0); },
  place_ct1() { return globalThis.DIFF_FIXTURES._place(1); },
  // Rect {x0 = 10, y0 = 20, x1 = 30, y1 = 40} for pointInRect 0x00492610.
  rect_10_20_30_40() {
    const obj = Memory.alloc(16);
    [10, 20, 30, 40].forEach((v, i) => obj.add(i * 4).writeS32(v));
    return { obj };
  },
};
