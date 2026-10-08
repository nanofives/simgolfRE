// Batch c3bt fixtures (Terrain.dll, round 18): the exported Terrain methods golf_clean.exe calls. Every Tile,
// every Terrain object and every neighbour here is our own allocation; the live Terrain instance
// (Terrain::getInstance 0x100031a0) is never called and never touched. The only module memory the bodies under
// test reach, all of it through their callees, is:
//
//   g_vertices        0x100b28c8  stride 0xc, three floats per entry  (written by Tile::elevateCorner /
//                                                                      Tile::lowerCorner / the type walks)
//   g_normals         0x10070a18  stride 0xc                          (written by Tile::calcNormals)
//   g_textures        0x100687f8  0x384 per type, 0x24 per variation  (READ ONLY, under Tile::setTypeId)
//   g_courseType      0x10070a0c  dword                               (read by Tile::blendFaceTextures)
//   g_collarInfo      0x10106b48  pointer to a dword table            (read by Tile::edgeKind)
//   g_tileW / g_tileH / g_splineHeight  0x10063e4c..0x10063e54        (read by the corner pair)
//   g_viewAngleIndex  0x10070a14  dword                               (written by Terrain::setViewAngle under
//                                                                      Terrain::lowerEdge)
//   the CRT per-thread random seed at _getptd() + 0x14                (read and written by rand 0x10018ce0
//                                                                      under Terrain::setType)
//
// That is the working set batches c3ba and c3bf measured as idle at the main menu, where diff_hook runs
// (log/c3/alt/c3ba_probe.py: zero calls into thirteen Terrain.dll entry points over 8 s and no byte of
// g_vertices / g_normals / g_textures changed in 1843 samples; log/c3/alt/c3bf_probe.py: g_courseType 0 and
// g_collarInfo NULL unchanged in 2467 samples). The random seed is per THREAD (rand reads it through
// _getptd 0x100219c0 at 0x10018ce4, then [ptd+0x14] at 0x10018cef), so the slot used here belongs to the
// Frida thread that runs both arms and no game thread can touch it. The registry declares every window the
// vectors touch as a state region, so diff_hook snapshots it before each vector, compares it after each arm
// and writes it back.
//
// g_collarInfo is NULL at the menu and Tile::edgeKind dereferences it on its non-0x11 path (0x100156fb,
// 0x10015701), so the setType fixture installs a table of its own exactly as batches c3as, c3bf and c3bk do,
// and the registry lists the pointer slot in the state regions. g_courseType is set to 0 there too: with it at
// 0 Tile::blendFaceTextures never calls Tile::isOpenType (jne 0x1001376d), the only path that would
// dereference a neighbour's NULL n[2] or n[3].
//
// All tile types used below are <= 0x24 and all variation / sub words <= 0x18, so every g_textures and
// g_collarInfo index the bodies form stays inside those tables.
Object.assign(globalThis.DIFF_FIXTURES, (function () {
  const TSIZE = 0x250;              // stride of a STANDALONE tile block (a Tile ends at +0x244)
  const TSTRIDE = 0x248;            // stride INSIDE a Terrain object (Terrain::tileAt, imul 0x10001da2)
  const VSLOTS = 24;                // eight faces x three vertex slots

  function terrain() { return Process.getModuleByName('Terrain.dll').base; }

  function fillBytes(p, n, seed) {
    const fill = [];
    for (let i = 0; i < n; i++) fill.push((i * 53 + seed) & 0xff);
    p.writeByteArray(fill);
  }

  // One block of `n` standalone tiles filled with a deterministic non-zero pattern, so a field a body does NOT
  // write keeps a value the state hash would notice if one arm wrote it.
  function tileBlock(n, seed) {
    const blk = Memory.alloc(n * TSIZE);
    fillBytes(blk, n * TSIZE, seed === undefined ? 0x2d : seed);
    const t = [];
    for (let i = 0; i < n; i++) t.push(blk.add(i * TSIZE));
    return { blk, t };
  }

  // A Terrain object of our own: width at +0x14, height at +0x18, the calcAllNormals shift count at +0x1c and
  // the tile array at +0x3a4 with stride 0x248 (all from Terrain::tileAt 0x10001d73 / 0x10001d84 / 0x10001d99 /
  // 0x10001da2 / 0x10001dab and Terrain::calcAllNormals 0x1000a765).
  function terrainObject(w, h, shift, seed) {
    const size = 0x3a4 + w * h * TSTRIDE;
    const obj = Memory.alloc(size);
    fillBytes(obj, size, seed === undefined ? 0x11 : seed);
    obj.add(0x14).writeS32(w);
    obj.add(0x18).writeS32(h);
    obj.add(0x1c).writeS32(shift);
    const t = [];
    for (let k = 0; k < w * h; k++) t.push(obj.add(0x3a4 + k * TSTRIDE));
    return { obj, t, size, w, h };
  }

  function setType(p, v) { p.add(0x24).writeS32(v); }
  function setSub(p, v) { p.add(0x28).writeS32(v); }
  function setX(p, v) { p.add(0x2c).writeS32(v); }
  function setY(p, v) { p.add(0x30).writeS32(v); }
  function setNeighbour(p, i, q) { p.add(0x34 + i * 4).writePointer(q || ptr(0)); }
  function setFaceCount(p, v) { p.add(0x44).writeS32(v); }
  function setHasPath(p, v) { p.add(0x208).writeU8(v); }
  function setConnected(p, v) { p.add(0x209).writeU8(v); }
  function setPathFlag(p, v) { p.add(0x20a).writeU8(v); }
  function setVariation(p, v) { p.add(0x240).writeS32(v); }
  function setDirty(p, v) { p.add(0x244).writeU8(v); }
  function setCorner(p, c, v) { p.add(c * 4).writeS32(v); }

  // Face f, vertex slot v of tile `p`: Tile + 0x48 + f * 0x38 + v * 4.
  function setFaceVertex(p, f, v, idx) { p.add(0x48 + f * 0x38 + v * 4).writeS32(idx); }
  // Tile number k gets the 24 vertex indices k * 24 .. k * 24 + 23, so no two tiles share a g_vertices entry.
  function wireVertices(p, k) {
    for (let f = 0; f < 8; f++)
      for (let v = 0; v < 3; v++) setFaceVertex(p, f, v, k * VSLOTS + f * 3 + v);
  }
  // The eight face normals (face + 0x2c = Tile + 0x74 + f * 0x38), overwritten by Terrain::faceNormal on the
  // faces Tile::smoothNormals refreshes and summed as they stand on the others.
  function seedFaceNormals(p, k) {
    for (let f = 0; f < 8; f++)
      for (let c = 0; c < 3; c++)
        p.add(0x74 + f * 0x38 + c * 4).writeFloat((f + 1) * 0.125 - c * 0.3125 + k * 0.0625);
  }

  function vtxPtr(i, c) { return terrain().add(0xb28c8 + i * 12 + c * 4); }
  function nrmPtr(i, c) { return terrain().add(0x70a18 + i * 12 + c * 4); }

  // g_tileW, g_tileH, g_splineHeight (0x10063e4c, 0x10063e50, 0x10063e54)
  function setConfig(tileW, tileH, spline) {
    terrain().add(0x63e4c).writeFloat(tileW);
    terrain().add(0x63e50).writeFloat(tileH);
    terrain().add(0x63e54).writeFloat(spline);
  }

  // Positions for g_vertices with a spread that is not a multiple of a power of two, so every subtraction and
  // product inside Terrain::faceNormal is a real float operation; the normals are zeroed.
  function seedVertices(count) {
    for (let i = 0; i < count; i++) {
      vtxPtr(i, 0).writeFloat((i % 7) * 13.5 - 20.25);
      vtxPtr(i, 1).writeFloat((i % 11) * 7.5 - 30.0);
      vtxPtr(i, 2).writeFloat((i % 5) * 9.75 + 3.125);
      nrmPtr(i, 0).writeFloat(0.0);
      nrmPtr(i, 1).writeFloat(0.0);
      nrmPtr(i, 2).writeFloat(0.0);
    }
  }

  // Heights at three magnitudes (around 1, 1e5 and 1e8, where a float ULP is 8) with non-terminating binary
  // expansions, as batch c3bk used, so a rounding difference in the corner walks cannot hide in one magnitude.
  function heights(seed) {
    let s = seed >>> 0;
    const scale = [1.0, 1.0, 97.0, 4099.0, 262144.0 * 3.0];
    let i = 0;
    return function () {
      s = (Math.imul(s, 1664525) + 1013904223) >>> 0;
      const k = ((s >>> 9) % 40001) / 173.0 - 115.0;
      const m = scale[(i++) % scale.length];
      return k * m + 1.0 / 29.0;
    };
  }
  function seedHeights(count, seed) {
    const h = heights(seed);
    for (let i = 0; i < count; i++) {
      vtxPtr(i, 0).writeFloat(h() / 7.0);
      vtxPtr(i, 1).writeFloat(h());
      vtxPtr(i, 2).writeFloat(h() / 11.0);
    }
  }

  // g_collarInfo: 0x100 dwords of our own, entry = (i % 5) * 100 (the shape batches c3as, c3bf and c3bk used).
  function installCollar(out) {
    const collar = Memory.alloc(0x100 * 4);
    for (let i = 0; i < 0x100; i++) collar.add(i * 4).writeS32((i % 5) * 100);
    const slot = terrain().add(0x106b48);
    slot.writePointer(collar);
    out.collar = collar;
    out.collarSlot = slot;
  }
  function setCourseType(out, v) {
    const slot = terrain().add(0x70a0c);
    slot.writeS32(v);
    out.courseSlot = slot;
  }

  // The CRT per-thread random seed rand 0x10018ce0 uses: _getptd 0x100219c0 returns the per-thread block and
  // rand reads and rewrites the dword at +0x14 (0x10018cef, 0x10018d01). Called here on the same thread that
  // will run both arms, so the slot is theirs alone; seeded to a fixed value so the key is reproducible.
  function installRandSlot(out) {
    const getptd = new NativeFunction(terrain().add(0x219c0), 'pointer', [], 'default');
    const slot = getptd().add(0x14);
    slot.writeS32(0x4d2);
    out.randSlot = slot;
  }

  // ----------------------------------------------------------------------------------------------
  // c3bt_tiles: twelve standalone tiles for the four pure field wrappers - Terrain::getVariation 0x3390
  // (reads the byte at +0x28 and sign-extends it), Terrain::hasPath 0xa510 (byte at +0x208),
  // Terrain::layPath 0xa3e0 (writes +0x208, +0x209 and +0x20a from the tile's own type) and
  // Terrain::setWall 0xa560 (writes +0x234 + side and +0x210 + side * 4).
  // The twelve differ in:
  //   +0x28   0, 1, 0x7f, 0x80, 0xff, 0x12, 0x100, 0x1234, 0x55, 0xaa, 0x7fffff80, -1
  //           (the four with a high bit in the low byte are the negative results of the sign extension, and
  //            0x100 is the one whose low byte is 0 while the dword is not)
  //   +0x208  0 and 1 alternating, plus 0xff on two of them
  //   +0x24   the nine values Tile::layPath tests against (0x16, 0, 2, 1, 3, 7, 9) and three it does not
  //   +0x209 / +0x20a  both starting values
  // The `this` of all four is stored in the frame slot and never read (0x100033aa, 0x1000a52a, 0x1000a3fa,
  // 0x1000a57a), so $terr is a block of our own that no body can reach.
  // ----------------------------------------------------------------------------------------------
  function tilesFixture() {
    //         +0x28       type  hasPath connected flag
    const D = [[0,          0x16, 0, 0, 0],
               [1,          0,    1, 1, 1],
               [0x7f,       2,    0, 1, 0],
               [0x80,       1,    1, 0, 1],
               [0xff,       3,    1, 1, 0],
               [0x12,       7,    0, 0, 1],
               [0x100,      9,    1, 0, 0],
               [0x1234,     4,    0xff, 1, 1],
               [0x55,       0x11, 1, 0, 1],
               [0xaa,       0x14, 0, 1, 0],
               [0x7fffff80, 5,    0xff, 0, 0],
               [-1,         0x24, 1, 1, 1]];
    const b = tileBlock(D.length, 0x2d);
    const out = { blk: b.blk, obj: b.blk, terr: Memory.alloc(0x40) };
    D.forEach((d, k) => {
      const p = b.t[k];
      setSub(p, d[0]);
      setType(p, d[1]);
      setHasPath(p, d[2]);
      setConnected(p, d[3]);
      setPathFlag(p, d[4]);
      setVariation(p, k);
      setFaceCount(p, 8);
      setDirty(p, 0);
      for (let i = 0; i < 4; i++) setNeighbour(p, i, null);
      out['t' + k] = p;
    });
    return out;
  }

  // ----------------------------------------------------------------------------------------------
  // c3bt_grid: a 5 x 4 Terrain object of our own for Terrain::hasConnectedPath 0xa450 and
  // Terrain::updatePath 0xa4b0, which both run Terrain::tileAt(x, y) and then a Tile method on the result
  // with NO null test (0x1000a47d, 0x1000a4e1). Only in-grid coordinates are used: tileAt answers 0 for
  // anything else (xor eax, eax at 0x10001d8f) and the originals would then read or write through a null
  // pointer at 0x10013380 / 0x100133c6.
  // The twenty tiles carry three different values of the connected byte at +0x209 (0, 1 and 0xff), so the
  // return of hasConnectedPath varies over the grid and the write of updatePath is visible on tiles that
  // already hold the value as well as on tiles that do not.
  // ----------------------------------------------------------------------------------------------
  function gridFixture() {
    const g = terrainObject(5, 4, 0, 0x11);
    const out = { terr: g.obj, obj: g.obj, size: g.size };
    for (let y = 0; y < g.h; y++) {
      for (let x = 0; x < g.w; x++) {
        const k = x + y * g.w;
        const p = g.t[k];
        setType(p, 4);
        setSub(p, k % 5);
        setX(p, x);
        setY(p, y);
        setFaceCount(p, 8);
        setVariation(p, k % 0x19);
        setHasPath(p, k & 1);
        setConnected(p, [0, 1, 0xff][k % 3]);
        setPathFlag(p, (k >> 1) & 1);
        setDirty(p, 0);
        for (let i = 0; i < 4; i++) setNeighbour(p, i, null);
      }
    }
    return out;
  }

  // ----------------------------------------------------------------------------------------------
  // c3bt_corner: the tile wiring batch c3ba used for Tile::elevateCorner / Tile::lowerCorner, reused for the
  // two Terrain wrappers 0xa5c0 and 0xa620. Sixteen standalone tiles of type 4 (so Tile::lowerCorner's
  // type-7 block at 0x1000cfc5 is skipped) in eight subjects plus eight neighbours, wired so the eight
  // normals-dirty tests at the end of both callees are taken and not taken; every neighbour carries an n[2]
  // and an n[3] because that tail dereferences n[0]->n[2] after testing only this tile's n[2] (0x1000cad1
  // then 0x1000cada).
  // ----------------------------------------------------------------------------------------------
  function cornerFixture(spline) {
    const N = 16;
    const b = tileBlock(N, 0x47);
    setConfig(100.0, 100.0, spline);
    seedVertices(N * VSLOTS);
    for (let k = 0; k < N; k++) {
      wireVertices(b.t[k], k);
      setType(b.t[k], 4);
      setSub(b.t[k], 0);
      setVariation(b.t[k], 0);
      setFaceCount(b.t[k], 8);
      setDirty(b.t[k], 0);
      for (let i = 0; i < 4; i++) setNeighbour(b.t[k], i, null);
      for (const c of [1, 3, 5, 7]) setCorner(b.t[k], c, k * 3 + c);
    }
    setNeighbour(b.t[1], 0, b.t[8]);
    setNeighbour(b.t[2], 2, b.t[9]);
    setNeighbour(b.t[3], 3, b.t[10]);
    setNeighbour(b.t[4], 1, b.t[11]);
    setNeighbour(b.t[5], 0, b.t[12]); setNeighbour(b.t[5], 2, b.t[9]);
    setNeighbour(b.t[6], 0, b.t[12]); setNeighbour(b.t[6], 3, b.t[10]);
    setNeighbour(b.t[7], 0, b.t[12]); setNeighbour(b.t[7], 1, b.t[15]);
    setNeighbour(b.t[7], 2, b.t[9]);  setNeighbour(b.t[7], 3, b.t[10]);
    setNeighbour(b.t[12], 2, b.t[13]); setNeighbour(b.t[12], 3, b.t[14]);
    setNeighbour(b.t[15], 2, b.t[13]); setNeighbour(b.t[15], 3, b.t[14]);
    for (const k of [0, 1, 2]) vtxPtr(k * VSLOTS + 2, 1).writeFloat(-1000.0);
    for (const k of [4, 5, 6, 7]) vtxPtr(k * VSLOTS + 2, 1).writeFloat(1.0e6);
    const out = { blk: b.blk, obj: b.blk, vtx: terrain().add(0xb28c8), cfg: terrain().add(0x63e4c),
                  terr: Memory.alloc(0x40) };
    for (let k = 0; k < N; k++) out['t' + k] = b.t[k];
    return out;
  }

  // ----------------------------------------------------------------------------------------------
  // c3bt_normals: batch c3ba's ten-tile set for Tile::calcNormals, reused for the wrapper 0xa6f0. No tile has
  // a neighbour, so Tile::smoothNormals takes its NULL paths and only this tile's own face normals are
  // recomputed through Terrain::faceNormal before the sum into g_normals. Tiles 8 and 9 have the
  // normals-dirty byte clear, which is Tile::calcNormals' early return (jne 0x10012d1a): the wrapper then
  // writes nothing at all.
  // ----------------------------------------------------------------------------------------------
  function normalsFixture() {
    const N = 10;
    const b = tileBlock(N, 0x47);
    setConfig(100.0, 100.0, 15.0);
    seedVertices(N * VSLOTS);
    for (let k = 0; k < N; k++) {
      wireVertices(b.t[k], k);
      setType(b.t[k], 4);
      setSub(b.t[k], 0);
      setVariation(b.t[k], 0);
      setFaceCount(b.t[k], 8);
      for (let i = 0; i < 4; i++) setNeighbour(b.t[k], i, null);
      setDirty(b.t[k], k < 8 ? 1 : 0);
      seedFaceNormals(b.t[k], k);
    }
    const out = { blk: b.blk, obj: b.blk, nrm: terrain().add(0x70a18), vtx: terrain().add(0xb28c8),
                  terr: Memory.alloc(0x40) };
    for (let k = 0; k < N; k++) out['c' + k] = b.t[k];
    return out;
  }

  // ----------------------------------------------------------------------------------------------
  // c3bt_allnormals: Terrain::calcAllNormals 0xa740 over a 5 x 5 Terrain object of our own. The window is
  // 26 << shift coordinates wide in each direction and is centred on the ARGUMENT tile's +0x2c / +0x30,
  // which the body only reads: the argument is never handed to a callee. So the subjects are twelve
  // standalone PROBE tiles whose coordinates place the window over different parts of the grid, which is how
  // the two loop bounds (jge 0x1000a7a6, jle 0x1000a7cf) and the null test (je 0x1000a7e8) get both sides and
  // how each vector ends in a different state:
  //   shift 0, window x in [X-12, X+13] and y in [Y-13, Y+12]
  //     p0  (2, 2)     every one of the 25 tiles
  //     p1  (16, 2)    the column x = 4 only
  //     p2  (-11, 2)   the columns x = 0..2
  //     p3  (-13, 2)   the column x = 0 only
  //     p4  (17, 2)    no column: tileAt answers 0 on every x of the window
  //     p5  (2, 16)    the rows y = 3, 4
  //     p6  (2, -11)   the rows y = 0, 1
  //     p7  (2, 18)    no row: the inner loop runs with every tileAt null
  //     p8  (14, 16)   x = 2..4 and y = 3, 4
  //     p9  (-12, -12) x = 0, 1 and y = 0
  //     p10 (0, 0)     every tile (the window reaches far outside the grid on two sides)
  //     p11 (4, 3)     every tile from the opposite corner
  // Only the tiles whose normals-dirty byte at +0x244 is set do any work (jne 0x10012d1a inside
  // Tile::calcNormals), and the pattern here sets it on the tiles with an even x + y, so a window that
  // covers a different set of tiles clears a different set of flags and writes a different set of g_normals
  // entries. No tile has a neighbour, so Tile::smoothNormals takes its NULL paths.
  // ----------------------------------------------------------------------------------------------
  const PROBES0 = [[2, 2], [16, 2], [-11, 2], [-13, 2], [17, 2], [2, 16], [2, -11], [2, 18], [14, 16],
                   [-12, -12], [0, 0], [4, 3]];
  // shift 1: the window is x in [X-25, X+26] and y in [Y-26, Y+25]
  //     p0 (2, 2)    every one of the 25 tiles      p1 (29, 2)   the column x = 4 only
  //     p2 (30, 2)   no column                      p3 (-24, 2)  the columns x = 0..2
  //     p4 (-26, 2)  the column x = 0 only          p5 (2, 31)   no row
  const PROBES1 = [[2, 2], [29, 2], [30, 2], [-24, 2], [-26, 2], [2, 31]];

  function allNormalsFixture(shift, probes) {
    const g = terrainObject(5, 5, shift, 0x11);
    setConfig(100.0, 100.0, 15.0);
    seedVertices(g.w * g.h * VSLOTS);
    for (let y = 0; y < g.h; y++) {
      for (let x = 0; x < g.w; x++) {
        const k = x + y * g.w;
        const p = g.t[k];
        wireVertices(p, k);
        setType(p, 4);
        setSub(p, 0);
        setX(p, x);
        setY(p, y);
        setFaceCount(p, 8);
        setVariation(p, 0);
        setHasPath(p, 0);
        setDirty(p, ((x + y) & 1) === 0 ? 1 : 0);
        for (let i = 0; i < 4; i++) setNeighbour(p, i, null);
        seedFaceNormals(p, k);
      }
    }
    const b = tileBlock(probes.length, 0x6b);
    const out = { terr: g.obj, obj: g.obj, size: g.size, blk: b.blk, nrm: terrain().add(0x70a18) };
    probes.forEach((c, k) => {
      const p = b.t[k];
      setType(p, 4);
      setX(p, c[0]);
      setY(p, c[1]);
      setDirty(p, 0);
      for (let i = 0; i < 4; i++) setNeighbour(p, i, null);
      out['p' + k] = p;
    });
    return out;
  }

  // ----------------------------------------------------------------------------------------------
  // c3bt_edge: Terrain::lowerEdgeCorner 0xa680 over a 5 x 5 Terrain object of our own. The body calls
  // Terrain::lowerEdge 0x10038900 and then Tile::lowerCorner, so the fixture has to satisfy both.
  //   * lowerEdge reads the subject tile's and the pivot tile's coordinates and looks up three neighbours by
  //     coordinate for each of its four angle arms (0x10038900, re/match/terrain_edge.cpp). Every subject
  //     here is an INTERIOR tile (x and y in 1..3), so all three lookups of all four arms are inside the
  //     grid, and every tile has type 4, so Tile::isHidden 0x10015460 (type == 0x14) is false for all of
  //     them: the draw block of lowerEdge, which calls Tile::render and OpenGL, is never entered.
  //   * Tile::lowerCorner needs the corner values at +4, +0xc, +0x14, +0x1c and the height step
  //     g_splineHeight; no tile has a neighbour, so its normals-dirty tail takes every `je`.
  // g_viewAngleIndex 0x10070a14, which Terrain::setViewAngle writes from the angle argument, is handed back
  // as $viewSlot and is a state region.
  // ----------------------------------------------------------------------------------------------
  function edgeFixture() {
    const g = terrainObject(5, 5, 0, 0x23);
    setConfig(100.0, 100.0, 15.0);
    seedVertices(g.w * g.h * VSLOTS);
    const out = { terr: g.obj, obj: g.obj, size: g.size, vtx: terrain().add(0xb28c8),
                  cfg: terrain().add(0x63e4c), viewSlot: terrain().add(0x70a14) };
    for (let y = 0; y < g.h; y++) {
      for (let x = 0; x < g.w; x++) {
        const k = x + y * g.w;
        const p = g.t[k];
        wireVertices(p, k);
        setType(p, 4);
        setSub(p, 0);
        setX(p, x);
        setY(p, y);
        setFaceCount(p, 8);
        setVariation(p, 0);
        setHasPath(p, 0);
        setDirty(p, 0);
        for (let i = 0; i < 4; i++) setNeighbour(p, i, null);
        for (const c of [1, 3, 5, 7]) setCorner(p, c, k * 3 + c);
        out['g' + k] = p;
      }
    }
    // the four interior subjects and two pivots, named so the vectors read clearly
    out.e0 = out.g6;    // (1, 1)
    out.e1 = out.g8;    // (3, 1)
    out.e2 = out.g16;   // (1, 3)
    out.e3 = out.g18;   // (3, 3)
    out.pv0 = out.g0;   // (0, 0)
    out.pv1 = out.g24;  // (4, 4)
    return out;
  }

  // ----------------------------------------------------------------------------------------------
  // c3bt_settype: Terrain::setType 0x32f0. Two separate objects, because the body never mixes them:
  //   * $terr is a Terrain object of our own holding only the 0x25 records of 0x18 bytes at +0x40 whose
  //     first dword the body tests and divides by (0x10003316, 0x1000332c). COUNTS below sets the count of
  //     each of the twelve types the vectors use: four of them are 0 or negative (the `jle 0x1000331b` side,
  //     and the test is signed, so -3 and -1 belong there), and the other eight are 1, 2, 3, 5, 7, 11, 2 and
  //     0x7fffffff, so the remainder rand() % count takes a different value on each of them.
  //   * a 21-tile block in the shape batch c3bk used for Tile::setTypeId: four subjects, each in a ring of
  //     four neighbours of one type (6 or 7) whose own diagonals are of that type too, plus a filler that is
  //     its own n[2] and n[3] so no inner dereference is NULL. The dispatch of Tile::setTypeId calls at most
  //     one entry function per call and the type-6 and type-7 ones look at different neighbour types, which
  //     is why there are two rings.
  //         a0  old type 6     in the type-6 ring   (the leave-6 arm, cmp 0x1001406a)
  //         a1  old type 0x11  in the type-6 ring   (the 0x11 arm, cmp 0x1001407d)
  //         b0  old type 7     in the type-7 ring   (the leave-7 arm, cmp 0x10014059)
  //         b1  old type 5     in the type-7 ring   (none of the three)
  // The collar table, g_courseType 0 and the per-thread random seed are installed as described at the top.
  // ----------------------------------------------------------------------------------------------
  function setTypeFixture() {
    // the count of every type the vectors use; the rest of the table is t + 1 and is never read
    const COUNTS = { 0: 0, 1: -3, 4: 0, 5: -1,
                     6: 1, 7: 2, 9: 3, 0xd: 5, 0x10: 7, 0x11: 11, 0x14: 0x7fffffff, 0x24: 2 };
    const terrObj = Memory.alloc(0x400);
    fillBytes(terrObj, 0x400, 0x11);
    for (let t = 0; t < 0x25; t++)
      terrObj.add(0x40 + t * 0x18).writeS32(t in COUNTS ? COUNTS[t] : t + 1);

    const SUBJ = { a0: [6, 6], a1: [0x11, 6], b0: [7, 7], b1: [5, 7] };
    const RING = ['R0', 'R1', 'R2', 'R3', 'D0', 'D1', 'D2', 'D3'];
    const names = Object.keys(SUBJ)
      .concat(RING.map((s) => s + '_6'), RING.map((s) => s + '_7'), ['F']);
    const N = names.length;                       // 4 subjects + 8 + 8 ring tiles + the filler = 21
    const b = tileBlock(N, 0x2d);
    const out = { blk: b.blk, obj: b.blk, terr: terrObj, n: N };
    installCollar(out);
    setCourseType(out, 0);
    installRandSlot(out);
    const T = {};
    names.forEach((name, k) => {
      const p = b.t[k];
      wireVertices(p, k);
      const s = SUBJ[name];
      setType(p, s ? s[0] : (name === 'F' ? 2 : parseInt(name.slice(-1), 10)));
      setSub(p, k % 5);
      setFaceCount(p, 8);
      setVariation(p, k % 0x19);
      setHasPath(p, k & 1);
      setPathFlag(p, (k >> 1) & 1);
      setDirty(p, 0);
      for (let i = 0; i < 4; i++) setNeighbour(p, i, null);
      T[name] = p;
      out[name] = p;
    });
    setNeighbour(T.F, 2, T.F); setNeighbour(T.F, 3, T.F);
    for (const tt of ['6', '7']) {
      setNeighbour(T['R0_' + tt], 2, T['D0_' + tt]); setNeighbour(T['R0_' + tt], 3, T['D1_' + tt]);
      setNeighbour(T['R1_' + tt], 2, T['D2_' + tt]); setNeighbour(T['R1_' + tt], 3, T['D3_' + tt]);
      for (const name of ['R2_', 'R3_', 'D0_', 'D1_', 'D2_', 'D3_']) {
        setNeighbour(T[name + tt], 2, T.F); setNeighbour(T[name + tt], 3, T.F);
      }
    }
    for (const name in SUBJ) {
      const tt = String(SUBJ[name][1]);
      setNeighbour(T[name], 0, T['R0_' + tt]); setNeighbour(T[name], 1, T['R1_' + tt]);
      setNeighbour(T[name], 2, T['R2_' + tt]); setNeighbour(T[name], 3, T['R3_' + tt]);
    }
    seedHeights(N * VSLOTS, 0x3bc01801);
    out.vtx = terrain().add(0xb28c8);
    return out;
  }

  return {
    c3bt_tiles() { return tilesFixture(); },
    c3bt_grid() { return gridFixture(); },
    c3bt_corner() { return cornerFixture(15.0); },
    // the same tiles with a height step that is not exact in binary, so the roundings inside the corner
    // callees are real roundings
    c3bt_corner_odd() { return cornerFixture(0.1); },
    c3bt_normals() { return normalsFixture(); },
    c3bt_allnormals() { return allNormalsFixture(0, PROBES0); },
    c3bt_allnormals_shift1() { return allNormalsFixture(1, PROBES1); },
    c3bt_edge() { return edgeFixture(); },
    c3bt_settype() { return setTypeFixture(); },
  };
})());
