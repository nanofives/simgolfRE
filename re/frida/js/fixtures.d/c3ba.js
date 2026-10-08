// Batch c3ba fixtures (Terrain.dll). Every Tile, Face and neighbour here is our own allocation; the only
// module memory touched is the three arrays the bodies under test are built around:
//
//   g_vertices  0x100b28c8   stride 0xc, three floats per entry   (written by elevateCorner, lowerCorner,
//                                                                  initGrid; read by faceNormal)
//   g_normals   0x10070a18   stride 0xc                           (written by initGrid, calcNormals)
//   g_textures  0x100687f8   0x384 per type, 0x24 per variation   (read by setFaceTexture, written by
//                                                                  setTextureId, in the type-0x23/0x24 rows)
//   g_tileW / g_tileH / g_splineHeight  0x10063e4c..0x10063e54     (read by initGrid / the corner pair)
//
// That is safe at the main menu, where diff_hook runs, and it is measured, not assumed:
// log/c3/alt/c3ba_probe.py attached to thirteen Terrain.dll entry points (render, drawTile, Tile::render,
// calcNormals, smoothNormals, initGrid, setTextureId, buildArrays, Tile::reset, elevateCorner, lowerCorner,
// setFaceTexture, getInstance) for 8 s at the menu and counted ZERO calls into any of them, and sampled the
// first 0x600 bytes of the three arrays 1843 times without one byte changing (g_vertices and g_normals read
// all zero there). The registry declares every window the vectors touch as a state region, so diff_hook
// snapshots it before each vector, compares it after each arm and writes it back.
//
// g_tileW, g_tileH and g_splineHeight are referenced by three functions in the whole module - initGrid
// 0x1000c560 (both sizes), Terrain::setSplineHeight 0x1000a840, Tile::elevateCorner 0x1000c966 and
// Tile::lowerCorner 0x1000ce76 (the height step) - found by scanning .text for the three addresses. The
// fixtures below that need a particular value write it explicitly, so no key depends on what an earlier one
// left behind; the live values at the menu are 100.0, 100.0 and 15.0.
Object.assign(globalThis.DIFF_FIXTURES, (function () {
  const TSIZE = 0x250;              // a Tile ends at +0x244; 0x250 keeps the blocks aligned
  const VSLOTS = 24;                // eight faces x three vertex slots

  function terrain() { return Process.getModuleByName('Terrain.dll').base; }

  // One block of `n` tiles filled with a deterministic non-zero pattern, so a field a body does NOT write
  // keeps a value the state hash would notice if one arm wrote it.
  function tileBlock(n) {
    const blk = Memory.alloc(n * TSIZE);
    const fill = [];
    for (let i = 0; i < n * TSIZE; i++) fill.push((i * 31 + 0x47) & 0xff);
    blk.writeByteArray(fill);
    const t = [];
    for (let i = 0; i < n; i++) t.push(blk.add(i * TSIZE));
    return { blk, t };
  }

  // Face f, vertex slot v of tile `p`: Tile + 0x48 + f * 0x38 + v * 4.
  function setFaceVertex(p, f, v, idx) { p.add(0x48 + f * 0x38 + v * 4).writeS32(idx); }
  // Give tile number k of a block the 24 vertex indices k * 24 .. k * 24 + 23.
  function wireVertices(p, k) {
    for (let f = 0; f < 8; f++)
      for (let v = 0; v < 3; v++) setFaceVertex(p, f, v, k * VSLOTS + f * 3 + v);
  }
  function setNeighbour(p, i, q) { p.add(0x34 + i * 4).writePointer(q || ptr(0)); }
  function setType(p, v) { p.add(0x24).writeS32(v); }
  function setSub(p, v) { p.add(0x28).writeS32(v); }
  function setVariation(p, v) { p.add(0x240).writeS32(v); }
  function setDirty(p, v) { p.add(0x244).writeU8(v); }
  function setCorner(p, c, v) { p.add(c * 4).writeS32(v); }

  // The three floats of g_vertices / g_normals entry i.
  function vtxPtr(i, c) { return terrain().add(0xb28c8 + i * 12 + c * 4); }
  function nrmPtr(i, c) { return terrain().add(0x70a18 + i * 12 + c * 4); }

  // Zero `count` entries of both arrays, then seed the vertex positions with a spread that is not a
  // multiple of a power of two, so every subtraction and product in faceNormal is a real float operation.
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

  // g_tileW, g_tileH, g_splineHeight
  function setConfig(tileW, tileH, spline) {
    terrain().add(0x63e4c).writeFloat(tileW);
    terrain().add(0x63e50).writeFloat(tileH);
    terrain().add(0x63e54).writeFloat(spline);
  }

  // ----------------------------------------------------------------------------------------------
  // c3ba_corner: eight subject tiles (type 4, so Tile::lowerCorner's `cmp [this+0x24], 7` at 0x1000cfbb
  // falls through) plus eight neighbours, wired so that the eight normals-dirty tests at the end of both
  // bodies (0x1000cab9 .. 0x1000cb80) are taken and not taken:
  //     t0  no neighbour                      every je taken
  //     t1  n[0]                              n[0] alone
  //     t2  n[2]                              n[2] alone
  //     t3  n[3]                              n[3] alone
  //     t4  n[1]                              n[1] alone
  //     t5  n[0], n[2]                        the n[0] && n[2] pair (n[0]->n[2] must exist: T12->n[2])
  //     t6  n[0], n[3]                        the n[0] && n[3] pair
  //     t7  all four                          all four pairs
  // The centre vertex height decides the max/min compare (0x1000c9eb / 0x1000cefb): t0..t2 hold -1000,
  // t4..t7 hold 1e6 and t3 holds exactly the mean elevateCorner computes for corner 1, which is the
  // C3-set (equal) side of `test ah, 0x41`.
  // ----------------------------------------------------------------------------------------------
  function cornerFixture(spline) {
    const N = 16;
    const b = tileBlock(N);
    setConfig(100.0, 100.0, spline);
    seedVertices(N * VSLOTS);
    for (let k = 0; k < N; k++) {
      wireVertices(b.t[k], k);
      setType(b.t[k], 4);
      setSub(b.t[k], 0);
      setVariation(b.t[k], 0);
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
    // the neighbours the pair tests dereference
    setNeighbour(b.t[12], 2, b.t[13]); setNeighbour(b.t[12], 3, b.t[14]);
    setNeighbour(b.t[15], 2, b.t[13]); setNeighbour(b.t[15], 3, b.t[14]);

    for (const k of [0, 1, 2]) vtxPtr(k * VSLOTS + 2, 1).writeFloat(-1000.0);
    for (const k of [4, 5, 6, 7]) vtxPtr(k * VSLOTS + 2, 1).writeFloat(1.0e6);
    // t3, corner 1: f0 = 4, v0 = 1 -> slot 13. newH = (corner[1] + 1) * spline, m = (newH + a) * 0.5.
    const a3 = vtxPtr(3 * VSLOTS + 13, 1).readFloat();
    const newH3 = Math.fround((3 * 3 + 1 + 1) * spline);
    vtxPtr(3 * VSLOTS + 2, 1).writeFloat(Math.fround((newH3 + a3) * 0.5));

    const out = { blk: b.blk, obj: b.blk, vtx: terrain().add(0xb28c8), cfg: terrain().add(0x63e4c) };
    for (let k = 0; k < N; k++) out['t' + k] = b.t[k];
    return out;
  }

  // ----------------------------------------------------------------------------------------------
  // c3ba_corner7: Tile::lowerCorner's type-7 block. Twenty-five tiles; u0..u11 are the subjects (type 7),
  // the rest are neighbours whose Tile::getType is 7 or 3 (S2 and S3 are the type-3 default n[2]/n[3] of
  // every neighbour: the normals-dirty tail dereferences n[0]->n[2] after testing only THIS tile's n[2],
  // 0x1000d205, so no neighbour may have a NULL n[2] or n[3]).
  //     u0   no neighbour                                            only the centre is sunk
  //     u1   n[0] = A(3)                                             getType(n[0]) != 7 (jne 0x1000cff8)
  //     u2   n[0] = B(7), n[1] = B(7)                                both outer writes, inner skipped (no n2/n3)
  //     u3   n[0] = B(7), n[1] = A(3), n[2] = C(3)                   inner n[2] fails on type (jne 0x1000d030)
  //     u4   n[0] = D(7, ->n2 = E(3)), n[2] = F(7)                   n[0]->n[2] fails on type (jne 0x1000d043)
  //     u5   n[0] = G(7, ->n2 = H(7), ->n3 = I(7)), n[2] = F(7), n[3] = J(7)  both n[0] inner writes
  //     u6   n[1] = G(7), n[2] = F(7), n[3] = J(7)                   both n[1] inner writes
  //     u7   n[0] = G, n[1] = K(7, ->n2 = H, ->n3 = I), n[2] = F, n[3] = J   every write
  //     u8   n[0] = B(7), n[3] = C(3)              getType(n[3]) != 7 in the n[0] block (jne 0x1000d074)
  //                                                and in the tail (jne 0x1000d1b6)
  //     u9   n[0] = D(7, ->n3 = S3(3)), n[3] = J(7)  getType(n[0]->n[3]) != 7 (jne 0x1000d087)
  //     u10  n[1] = B(7, ->n2 = S2(3), ->n3 = S3(3)), n[2] = F(7), n[3] = J(7)
  //                                                getType(n[1]->n[2]) != 7 (jne 0x1000d10a) and
  //                                                getType(n[1]->n[3]) != 7 (jne 0x1000d151)
  //     u11  n[1] = B(7), n[2] = C(3), n[3] = C(3)   getType(n[2]) != 7 in the n[1] block (jne 0x1000d0f7)
  //                                                and getType(n[3]) != 7 (jne 0x1000d13e)
  // ----------------------------------------------------------------------------------------------
  function corner7Fixture() {
    const N = 25;
    const b = tileBlock(N);
    setConfig(100.0, 100.0, 15.0);
    seedVertices(N * VSLOTS);
    const name = {A: 12, B: 13, C: 14, D: 15, E: 16, F: 17, G: 18, H: 19, I: 20, J: 21, K: 22,
                  S2: 23, S3: 24};
    const type7 = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
                   name.B, name.D, name.F, name.G, name.H, name.I, name.J, name.K];
    for (let k = 0; k < N; k++) {
      wireVertices(b.t[k], k);
      setType(b.t[k], type7.indexOf(k) >= 0 ? 7 : 3);
      setSub(b.t[k], 0);
      setVariation(b.t[k], 0);
      setDirty(b.t[k], 0);
      for (let i = 0; i < 4; i++) setNeighbour(b.t[k], i, null);
      for (const c of [1, 3, 5, 7]) setCorner(b.t[k], c, k * 2 + c);
    }
    // every neighbour tile gets an n[2] and an n[3] so the dirty tail never writes through NULL
    for (let k = 12; k < N; k++) { setNeighbour(b.t[k], 2, b.t[name.S2]); setNeighbour(b.t[k], 3, b.t[name.S3]); }
    setNeighbour(b.t[name.D], 2, b.t[name.E]);
    setNeighbour(b.t[name.G], 2, b.t[name.H]); setNeighbour(b.t[name.G], 3, b.t[name.I]);
    setNeighbour(b.t[name.K], 2, b.t[name.H]); setNeighbour(b.t[name.K], 3, b.t[name.I]);

    setNeighbour(b.t[1], 0, b.t[name.A]);
    setNeighbour(b.t[2], 0, b.t[name.B]); setNeighbour(b.t[2], 1, b.t[name.B]);
    setNeighbour(b.t[3], 0, b.t[name.B]); setNeighbour(b.t[3], 1, b.t[name.A]);
    setNeighbour(b.t[3], 2, b.t[name.C]);
    setNeighbour(b.t[4], 0, b.t[name.D]); setNeighbour(b.t[4], 2, b.t[name.F]);
    setNeighbour(b.t[5], 0, b.t[name.G]); setNeighbour(b.t[5], 2, b.t[name.F]);
    setNeighbour(b.t[5], 3, b.t[name.J]);
    setNeighbour(b.t[6], 1, b.t[name.G]); setNeighbour(b.t[6], 2, b.t[name.F]);
    setNeighbour(b.t[6], 3, b.t[name.J]);
    setNeighbour(b.t[7], 0, b.t[name.G]); setNeighbour(b.t[7], 1, b.t[name.K]);
    setNeighbour(b.t[7], 2, b.t[name.F]); setNeighbour(b.t[7], 3, b.t[name.J]);
    setNeighbour(b.t[8], 0, b.t[name.B]); setNeighbour(b.t[8], 3, b.t[name.C]);
    setNeighbour(b.t[9], 0, b.t[name.D]); setNeighbour(b.t[9], 3, b.t[name.J]);
    setNeighbour(b.t[10], 1, b.t[name.B]); setNeighbour(b.t[10], 2, b.t[name.F]);
    setNeighbour(b.t[10], 3, b.t[name.J]);
    setNeighbour(b.t[11], 1, b.t[name.B]); setNeighbour(b.t[11], 2, b.t[name.C]);
    setNeighbour(b.t[11], 3, b.t[name.C]);

    const out = { blk: b.blk, obj: b.blk, vtx: terrain().add(0xb28c8), cfg: terrain().add(0x63e4c) };
    for (let k = 0; k < N; k++) out['u' + k] = b.t[k];
    return out;
  }

  // ----------------------------------------------------------------------------------------------
  // c3ba_reset: eight tiles left exactly as tileBlock filled them (every field non-zero), so each of
  // Tile::reset's eleven stores and its two callees' eleven byte stores show in the state hash.
  // ----------------------------------------------------------------------------------------------
  function resetFixture() {
    const N = 8;
    const b = tileBlock(N);
    const out = { blk: b.blk, obj: b.blk };
    for (let k = 0; k < N; k++) out['r' + k] = b.t[k];
    return out;
  }

  // ----------------------------------------------------------------------------------------------
  // c3ba_face: twenty-four tiles, one per arm of Tile::setFaceTexture's type-override switch. Nothing
  // outside the tiles is written; g_textures is only read, and every (type, variation, piece) a vector can
  // produce stays inside the table (type <= 0x24, variation <= 0x16, piece <= 8).
  // ----------------------------------------------------------------------------------------------
  function faceFixture() {
    //        type  sub     variation
    const S = [[1, 0x80, 0], [1, 0x7f, 1], [6, 0, 2], [21, 0, 3], [7, 5, 4], [7, -6, 5], [7, -8, 6],
               [13, 0, 7], [17, 1, 8], [17, 2, 9], [17, 5, 10], [22, 0x40, 11], [22, 0x47, 12],
               [22, 0x48, 13], [22, 0x3f, 14], [0, 0, 15], [23, 0, 16], [4, 0, 17], [7, 6, 18],
               [7, 7, 19], [16, 0, 20], [14, 0, 21], [15, 0, 22], [7, 8, 23]];
    const b = tileBlock(S.length);
    const out = { blk: b.blk, obj: b.blk };
    for (let k = 0; k < S.length; k++) {
      setType(b.t[k], S[k][0]);
      setSub(b.t[k], S[k][1]);
      setVariation(b.t[k], S[k][2]);
      out['f' + k] = b.t[k];
    }
    return out;
  }

  // ----------------------------------------------------------------------------------------------
  // c3ba_tex: setTextureId writes the texture table itself, so the vectors stay in the two highest type
  // rows, 0x23 and 0x24, which span g_textures + 0x7b0c .. + 0x8214 (type stride 0x384). The state region
  // is exactly those 1800 bytes.
  // ----------------------------------------------------------------------------------------------
  function texFixture() {
    return { tex: terrain().add(0x70304), obj: terrain().add(0x70304) };
  }

  // ----------------------------------------------------------------------------------------------
  // c3ba_grid: initGrid lays out the start of both arrays. The largest vector is (4, 4), whose highest
  // entry is 143, so the state regions are the first 0x780 bytes of each array (160 entries).
  // ----------------------------------------------------------------------------------------------
  function gridFixture(tileW, tileH) {
    setConfig(tileW, tileH, 15.0);
    for (let i = 0; i < 160; i++) {
      vtxPtr(i, 0).writeFloat(-7.125 - i);
      vtxPtr(i, 1).writeFloat(11.5 + i);
      vtxPtr(i, 2).writeFloat(3.75 - i);
      nrmPtr(i, 0).writeFloat(0.25 + i);
      nrmPtr(i, 1).writeFloat(-0.5 - i);
      nrmPtr(i, 2).writeFloat(2.125 + i);
    }
    return { vtx: terrain().add(0xb28c8), nrm: terrain().add(0x70a18), cfg: terrain().add(0x63e4c),
             obj: terrain().add(0xb28c8) };
  }

  // ----------------------------------------------------------------------------------------------
  // c3ba_normals: ten tiles with no neighbour, so Tile::smoothNormals (the original, called by
  // calcNormals) takes its three NULL paths and only recomputes this tile's own face normals through
  // Tile::faceNormal, which reads g_vertices and writes face + 0x2c. Tiles 8 and 9 have the normals-dirty
  // byte clear, which is calcNormals' early return (jne 0x10012d1a).
  // ----------------------------------------------------------------------------------------------
  function normalsFixture() {
    const N = 10;
    const b = tileBlock(N);
    setConfig(100.0, 100.0, 15.0);
    seedVertices(N * VSLOTS);
    for (let k = 0; k < N; k++) {
      wireVertices(b.t[k], k);
      setType(b.t[k], 4);
      setSub(b.t[k], 0);
      setVariation(b.t[k], 0);
      for (let i = 0; i < 4; i++) setNeighbour(b.t[k], i, null);
      setDirty(b.t[k], k < 8 ? 1 : 0);
      // a spread of face normals, overwritten by faceNormal on the dirty tiles
      for (let f = 0; f < 8; f++)
        for (let c = 0; c < 3; c++)
          b.t[k].add(0x74 + f * 0x38 + c * 4).writeFloat((f + 1) * 0.125 - c * 0.3125 + k * 0.0625);
    }
    const out = { blk: b.blk, obj: b.blk, nrm: terrain().add(0x70a18), vtx: terrain().add(0xb28c8) };
    for (let k = 0; k < N; k++) out['c' + k] = b.t[k];
    return out;
  }

  return {
    c3ba_corner() { return cornerFixture(15.0); },
    // the same tiles with a height step that is not exact in binary, so the `fstp dword` after
    // `fild; fmul [0x10063e54]` (0x1000c96a) and the mean's rounding are real roundings
    c3ba_corner_odd() { return cornerFixture(0.1); },
    c3ba_corner7() { return corner7Fixture(); },
    c3ba_reset() { return resetFixture(); },
    c3ba_face() { return faceFixture(); },
    c3ba_tex() { return texFixture(); },
    c3ba_grid() { return gridFixture(100.0, 100.0); },
    c3ba_grid_odd() { return gridFixture(0.1, 1.0 / 3.0); },
    c3ba_normals() { return normalsFixture(); },
  };
})());
