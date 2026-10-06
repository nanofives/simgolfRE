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
  // Rect {x0 = 10, y0 = 20, x1 = 30, y1 = 40} for pointInRect 0x00492610.
  rect_10_20_30_40() {
    const obj = Memory.alloc(16);
    [10, 20, 30, 40].forEach((v, i) => obj.add(i * 4).writeS32(v));
    return { obj };
  },
};
