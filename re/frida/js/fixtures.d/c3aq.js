// Batch c3aq fixtures (Terrain.dll). Every buffer here is our own: the live Terrain object, its tile array and
// the module globals are never touched.
//
// c3aq_tiles  16 Tile buffers (stride 0x248) seeded so that each hooked reader has a branch-covering spread:
//   +0x00..+0x20  nine dwords read by Tile::getCorner (index 0..8); the four at +0x04, +0x0c, +0x14, +0x1c are
//                 the corner heights Tile::maxHeight compares, chosen to take both sides of all three of its
//                 jle (0x10015529, 0x10015551, 0x10015573), with equal, negative and INT_MIN/INT_MAX rows;
//   +0x24         type: tiles 0..6 are the seven types Tile::layPath treats as "collar off"
//                 (0x16, 0, 2, 1, 3, 7, 9, in the order the original compares them), tiles 7..15 are other types;
//   +0x28         rotation dword; its low byte is what Tile::getVariation returns, so the 16 values cover
//                 0, 1, 0x7f, 0x80, 0xff and bytes coming from negative and large dwords;
//   +0x44         Faces count, seeded non-zero so Faces::Faces' store of 0 changes the state;
//   +0x208/+0x209 hasPath / connected bytes, seeded with 0, 1 and other non-zero values;
//   +0x240        variation dword (Tile::setVariation's target).
// Everything else is a deterministic filler so out-of-range Tile::getCorner indices are still reproducible.
//
// c3aq_blocks  16 standalone PathInfo blocks (16 bytes each) and 16 standalone WallInfo blocks (0x30 bytes each),
//   all bytes seeded non-zero so the clears called by the two constructors are observable. They are kept out of
//   the Tile buffers on purpose: the PathInfo bytes a constructor must zero are the same bytes Tile::hasPath and
//   Tile::isConnected need to read as 0 on some tiles.
//
// c3aq_images  6 pixel buffers of 0x100 bytes each plus 12 ImageInfo records {bpp, width, height} for
//   swapRedBlue: sizes whose width*height is 1, 2, 3, 4, 6, 9 and 16 pixels, plus zero and negative products.
Object.assign(globalThis.DIFF_FIXTURES, {
  c3aq_tiles() {
    const N = 16, STRIDE = 0x248;
    // (c0 = +0x04, c1 = +0x0c, c2 = +0x14, c3 = +0x1c) for Tile::maxHeight: a = max(c0, c3), b = max(c1, c2)
    const CORNERS = [
      [10, 8, 2, 3],        //  0  c0>c3, c1>c2, a>b   -> 10
      [10, 20, 5, 3],       //  1  c0>c3, c1>c2, a<=b  -> 20
      [10, 2, 8, 3],        //  2  c0>c3, c1<=c2, a>b  -> 10
      [10, 5, 20, 3],       //  3  c0>c3, c1<=c2, a<=b -> 20
      [3, 8, 2, 10],        //  4  c0<=c3, c1>c2, a>b  -> 10
      [3, 20, 5, 10],       //  5  c0<=c3, c1>c2, a<=b -> 20
      [3, 2, 8, 10],        //  6  c0<=c3, c1<=c2, a>b -> 10
      [3, 5, 20, 10],       //  7  c0<=c3, c1<=c2, a<=b-> 20
      [7, 7, 7, 7],         //  8  all equal (every jle taken) -> 7
      [-5, -9, -3, -7],     //  9  negatives -> -3
      [0x7fffffff, -0x80000000, 0, -0x80000000],  // 10 -> 0x7fffffff
      [-0x80000000, -0x80000000, -0x80000000, -0x80000000],  // 11 -> -0x80000000
      [0, 0, 0, 1],         // 12 -> 1
      [1, 0, 0, 0],         // 13 -> 1
      [-1, 100, -100, -2],  // 14 -> 100
      [0x40000000, 0x3fffffff, 0x3ffffffe, -1],   // 15 -> 0x40000000
    ];
    const TYPES = [0x16, 0, 2, 1, 3, 7, 9, 4, 0x11, 0x14, 5, 6, 8, 0x15, 0x0a, -1];
    const ROT = [0, 1, 0x7f, 0x80, 0xff, 0x100, 0x1ff, -1, -0x80, 0x12345678, 0xaa, 2, 3,
                 0x7fffffff, -0x80000000, 0x55];
    const HASPATH = [0, 1, 2, 0xff, 0, 1, 0, 1, 3, 0, 0x7f, 0x80, 1, 0, 1, 0];
    const CONNECTED = [1, 0, 0xff, 0, 1, 0, 2, 0, 1, 1, 0, 0x80, 0, 1, 0, 1];
    const base = Memory.alloc(N * STRIDE);

    const fill = [];
    for (let i = 0; i < N; i++)
      for (let j = 0; j < STRIDE; j++) fill.push((i * 29 + j * 11 + 0x3d) & 0xff);
    base.writeByteArray(fill);

    const out = { base, obj: base };             // `obj` makes pointer returns read as obj+0x....
    for (let i = 0; i < N; i++) {
      const t = base.add(i * STRIDE);
      // the nine dwords Tile::getCorner indexes; slots 1, 3, 5, 7 are the corner heights
      for (let k = 0; k < 9; k++) t.add(k * 4).writeS32(i * 1000 + k * 100 + 7);
      t.add(0x04).writeS32(CORNERS[i][0]);
      t.add(0x0c).writeS32(CORNERS[i][1]);
      t.add(0x14).writeS32(CORNERS[i][2]);
      t.add(0x1c).writeS32(CORNERS[i][3]);
      t.add(0x24).writeS32(TYPES[i]);
      t.add(0x28).writeS32(ROT[i] | 0);
      t.add(0x44).writeS32(0x5a000 + i);         // Faces count, non-zero
      t.add(0x208).writeU8(HASPATH[i]);
      t.add(0x209).writeU8(CONNECTED[i]);
      t.add(0x20a).writeU8((i & 1) ? 1 : 0);
      t.add(0x240).writeS32(-(i + 1) * 17);
      out['t' + i] = t;
      out['f' + i] = t.add(0x44);                // the Faces block (Faces::Faces' this)
    }
    return out;
  },

  c3aq_blocks() {
    const N = 16, PSZ = 0x10, WSZ = 0x30;
    const pbase = Memory.alloc(N * PSZ);
    const wbase = Memory.alloc(N * WSZ);
    const pf = [], wf = [];
    for (let i = 0; i < N; i++) {
      for (let j = 0; j < PSZ; j++) pf.push(((i * 13 + j * 7) & 0x7f) + 1);   // never 0
      for (let j = 0; j < WSZ; j++) wf.push(((i * 23 + j * 5) & 0x7f) + 1);   // never 0
    }
    pbase.writeByteArray(pf);
    wbase.writeByteArray(wf);
    const out = { pbase, wbase };
    for (let i = 0; i < N; i++) {
      out['p' + i] = pbase.add(i * PSZ);
      out['w' + i] = wbase.add(i * WSZ);
    }
    return out;
  },

  // 16 four-byte destination slots (the list node payload std::_Construct writes) plus 16 source dwords.
  // No heap and no list of the game's: the placement form of operator new only hands the pointer back.
  c3aq_nodes() {
    const N = 16;
    const dest = Memory.alloc(N * 4);
    const src = Memory.alloc(N * 4);
    const SRC = [0, 1, -1, 0x7fffffff, -0x80000000, 0x248, 0x10001234, -7, 0x55555555, 0xaaaa,
                 2, 3, 0x1000, -0x1000, 0x7f, 0x80];
    const out = { dest, src, obj: dest };
    for (let i = 0; i < N; i++) {
      dest.add(i * 4).writeS32(0x4d000 + i);     // seeded non-zero, so a copy always changes the state
      src.add(i * 4).writeS32(SRC[i] | 0);
      out['d' + i] = dest.add(i * 4);
      out['s' + i] = src.add(i * 4);
    }
    return out;
  },

  c3aq_images() {
    const NB = 6, BSZ = 0x100;
    const imgbase = Memory.alloc(NB * BSZ);
    const fill = [];
    for (let i = 0; i < NB; i++)
      for (let j = 0; j < BSZ; j++) fill.push((i * 97 + j * 31 + 0x11) & 0xff);
    imgbase.writeByteArray(fill);
    // {bpp, width, height}: the function reads only +4 and +8, and its counter is width*height
    const SIZES = [[24, 1, 1], [24, 2, 1], [24, 1, 3], [24, 4, 4], [24, 3, 2], [24, 9, 1],
                   [8, 2, 3], [24, 0, 5], [24, 5, 0], [24, -1, 2], [24, 2, -3], [24, 16, 1]];
    const infobase = Memory.alloc(SIZES.length * 12);
    const out = { imgbase, infobase };
    for (let i = 0; i < NB; i++) out['b' + i] = imgbase.add(i * BSZ);
    for (let i = 0; i < SIZES.length; i++) {
      const p = infobase.add(i * 12);
      p.writeS32(SIZES[i][0]);
      p.add(4).writeS32(SIZES[i][1]);
      p.add(8).writeS32(SIZES[i][2]);
      out['i' + i] = p;
    }
    return out;
  },
});
