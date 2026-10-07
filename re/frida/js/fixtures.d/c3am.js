// Batch c3am fixtures (Terrain.dll). Our own Tile buffers: the live Terrain object is never used.
// Tile stride 0x248; the fields seeded here are the ones the hooked functions read or write:
//   +0x24 type, +0x2c x, +0x30 y, +0x34..+0x40 four neighbour pointers, +0x208..+0x20e PathInfo,
//   +0x210..+0x233 nine wall heights (dwords), +0x234..+0x23c nine wall flag bytes.
// Every other byte gets a deterministic filler so the byte-offset reads of Tile::getWall (this + side + 0x234)
// return something different for each (tile, side) pair instead of zero.
Object.assign(globalThis.DIFF_FIXTURES, {
  c3am_tiles() {
    const N = 14, STRIDE = 0x248;
    const TYPES = [0x14, 4, 0, 9, 7, 0x11, 6, 0x0d, -1, 0x7fffffff, 0x14, 2, 0x13, 0x15];
    const XS = [0, 1, -1, 5, 49, -0x80000000, 0x7fffffff, 12, 7, 33, 2, 0, 100, -7];
    const YS = [0, -1, 3, 8, 50, 0x7fffffff, -0x80000000, 4, 19, 0, 6, 11, -3, 64];
    const base = Memory.alloc(N * STRIDE);

    // filler first: byte (i*37 + j*13 + 0x5b) & 0xff at tile i, offset j
    const fill = [];
    for (let i = 0; i < N; i++)
      for (let j = 0; j < STRIDE; j++) fill.push((i * 37 + j * 13 + 0x5b) & 0xff);
    base.writeByteArray(fill);

    const out = { base };
    for (let i = 0; i < N; i++) {
      const t = base.add(i * STRIDE);
      t.add(0x24).writeS32(TYPES[i]);
      t.add(0x2c).writeS32(XS[i]);
      t.add(0x30).writeS32(YS[i]);
      for (let k = 0; k < 4; k++) t.add(0x34 + k * 4).writePointer(base.add(((i + k + 1) % N) * STRIDE));
      for (let j = 0; j < 7; j++) t.add(0x208 + j).writeU8(((i * 7 + j * 3) & 0x7f) + 1);   // PathInfo: never 0
      for (let j = 0; j < 9; j++) t.add(0x210 + j * 4).writeS32(i * 100 + j);               // wall heights
      for (let j = 0; j < 9; j++) t.add(0x234 + j).writeU8((i * 5 + j * 3) % 9);            // wall flags 0..8
      out["t" + i] = t;
      out["p" + i] = t.add(0x208);   // PathInfo block (PathInfo::clear's this)
      out["w" + i] = t.add(0x210);   // WallInfo block (WallInfo::clear's this)
    }
    // Terrain::getWall and Tile::isSolidType store this in their frame and never read it: a scratch block.
    out.ter = Memory.alloc(0x40);
    return out;
  },
});
