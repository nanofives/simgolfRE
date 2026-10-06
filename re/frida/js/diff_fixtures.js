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
};
