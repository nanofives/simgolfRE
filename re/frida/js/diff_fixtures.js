// Fixtures for diff_hook.js: each returns { obj: NativePointer, ... } built in game memory.
globalThis.DIFF_FIXTURES = {
  // Fake Terrain: only the fields tileAt reads are meaningful (+0x14 width = 10, +0x18 height = 8).
  terrain_10x8() {
    const obj = Memory.alloc(0x3a4 + 10 * 8 * 0x248);
    obj.add(0x14).writeS32(10);
    obj.add(0x18).writeS32(8);
    return { obj };
  },
};
