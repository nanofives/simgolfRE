// Batch c3bf fixtures (Terrain.dll, round 15). Every Tile, Face and neighbour here is our own allocation; the
// live Terrain object and its tile array are never written. The module memory the bodies under test read or
// write is:
//
//   g_vertices    0x100b28c8   three floats per entry, stride 0xc   (read by Terrain::faceNormal)
//   g_normals     0x10070a18   three floats per entry, stride 0xc   (written by Tile::smoothNormals)
//   g_textures    0x100687f8   0x384 per type, 0x24 per variation   (READ ONLY, by Tile::setFaceTexture)
//   g_courseType  0x10070a0c   dword                                (read by Tile::blendFaceTextures)
//   g_collarInfo  0x10106b48   pointer to a dword table             (read by Tile::edgeKind)
//
// That is safe at the main menu, where diff_hook runs, and it is measured, not assumed:
// log/c3/alt/c3bf_probe.py attached to Tile::blendFaceTextures 0x10013670, Tile::reblendNeighbourhood
// 0x10013500, Tile::smoothNormals 0x10011ef0, Tile::setTypeId 0x10014020, Tile::edgeKind 0x10015650,
// Terrain::rebuild 0x10003980, Terrain::relight 0x100076e0, Terrain::loadNewCourseType 0x10001af0 and
// Terrain::Terrain 0x10002ae0 - the whole set of functions that read or write g_courseType and g_collarInfo,
// plus the three under test - and counted zero calls into any of them at the menu. The registry declares every
// window the vectors touch as a state region, so diff_hook snapshots it before each vector, compares it after
// each arm and writes it back.
//
// g_collarInfo lives in .bss (zero at load) and is only ever set by Terrain::loadNewCourseType; the fixtures
// install a table of their own in it, exactly as batch c3as's c3as_tiles does for Tile::edgeKind, and the
// registry lists the pointer slot ($collarSlot) in the state regions. g_courseType is written by the same
// function and read by the four above plus Tile::blendFaceTextures; each fixture writes the value its key
// needs, so no key depends on what an earlier one left behind.
Object.assign(globalThis.DIFF_FIXTURES, (function () {
  const TSIZE = 0x250;              // a Tile ends at +0x244 (stride 0x248); 0x250 keeps the blocks aligned
  const VSLOTS = 24;                // eight faces x three vertex slots

  function terrain() { return Process.getModuleByName('Terrain.dll').base; }

  // One block of `n` tiles filled with a deterministic non-zero pattern, so a field a body does NOT write
  // keeps a value the state hash would notice if one arm wrote it.
  function tileBlock(n) {
    const blk = Memory.alloc(n * TSIZE);
    const fill = [];
    for (let i = 0; i < n * TSIZE; i++) fill.push((i * 37 + 0x5b) & 0xff);
    blk.writeByteArray(fill);
    const t = [];
    for (let i = 0; i < n; i++) t.push(blk.add(i * TSIZE));
    return { blk, t };
  }

  function setType(p, v) { p.add(0x24).writeS32(v); }
  function setSub(p, v) { p.add(0x28).writeS32(v); }
  function setNeighbour(p, i, q) { p.add(0x34 + i * 4).writePointer(q || ptr(0)); }
  function setFaceCount(p, v) { p.add(0x44).writeS32(v); }
  function setVariation(p, v) { p.add(0x240).writeS32(v); }
  function setDirty(p, v) { p.add(0x244).writeU8(v); }

  // Face f, vertex slot v of tile `p`: Tile + 0x48 + f * 0x38 + v * 4.
  function setFaceVertex(p, f, v, idx) { p.add(0x48 + f * 0x38 + v * 4).writeS32(idx); }
  // Give tile number k of a block the 24 vertex indices k * 24 .. k * 24 + 23.
  function wireVertices(p, k) {
    for (let f = 0; f < 8; f++)
      for (let v = 0; v < 3; v++) setFaceVertex(p, f, v, k * VSLOTS + f * 3 + v);
  }

  // The three floats of g_vertices / g_normals entry i.
  function vtxPtr(i, c) { return terrain().add(0xb28c8 + i * 12 + c * 4); }
  function nrmPtr(i, c) { return terrain().add(0x70a18 + i * 12 + c * 4); }

  // g_collarInfo: 0x100 dwords, entry = (type % 5) * 100 (the shape batch c3as used), so types 4 and 9 share
  // an entry, 4 and 5 do not, and 0x11 (17 % 5 = 2) differs from both. Installed in the module pointer slot.
  function installCollar(out) {
    const collar = Memory.alloc(0x100 * 4);
    for (let i = 0; i < 0x100; i++) collar.add(i * 4).writeS32((i % 5) * 100);
    const slot = terrain().add(0x106b48);
    out.collarOld = slot.readPointer();
    slot.writePointer(collar);
    out.collar = collar;
    out.collarSlot = slot;
  }

  // g_courseType, written into the module and handed back as $courseSlot so the registry can snapshot it.
  function setCourseType(out, v) {
    const slot = terrain().add(0x70a0c);
    out.courseOld = slot.readS32();
    slot.writeS32(v);
    out.courseSlot = slot;
  }

  // A deterministic value generator. Every number is k / 173, a rational whose binary expansion does not
  // terminate, so no product, sum or difference below is exact by construction.
  function gen(seed) {
    let s = seed >>> 0;
    return function () {
      s = (Math.imul(s, 1664525) + 1013904223) >>> 0;
      return ((s >>> 7) % 40001) / 173.0 - 115.0;
    };
  }

  // ----------------------------------------------------------------------------------------------
  // c3bf_smooth: Tile::smoothNormals. Thirteen tiles; the five subjects cover the five combinations of the
  // three Faces* locals the body builds (fa = n[a]->getFaces(), fb = n[b]->getFaces(),
  // fc = n[b]->n[a]->getFaces()) for every (a, b) in {0,1} x {2,3}:
  //     s0  no neighbour at all                                  fa = fb = fc = 0
  //     s1  n[0] = a0, n[1] = a1                                 fa only (for a = 0 and a = 1)
  //     s2  n[2] = b0, n[3] = b1                                 fb only (n[a] == 0, so fc is skipped at
  //                                                              the second test, je 0x10011f6b)
  //     s3  n[0] = a0, n[1] = a1, n[2] = b0, n[3] = b1           fa and fb; b0/b1 have no n[0] or n[1], so
  //                                                              fc = 0 through je 0x10011f7f
  //     s4  n[0] = a0, n[1] = a1, n[2] = c0, n[3] = c1           fa, fb and fc; c0/c1 have n[0] = d0 and
  //                                                              n[1] = d1
  // Every tile gets the 24 vertex indices k*24.. of its own, so g_vertices and g_normals entries 0..311 are
  // the whole working set. The vertex coordinates and the eight face normals of every tile are seeded from
  // the generator above: the body sums PAIRS of face normals, and the sum of two unit-length floats needs
  // more than 24 bits of mantissa, so a reimplementation that rounded the intermediate to float instead of to
  // the measured 53 bits would produce a different float. The seeded face normals also matter in themselves:
  // the a = 0, b = 3 arm sums fb's face 1 normal although it only refreshes fb's faces 0 and 4
  // (fld 0x10012481 against the faceNormal calls at 0x10012354 and 0x10012369), so that value comes from the
  // fixture and not from Terrain::faceNormal.
  // ----------------------------------------------------------------------------------------------
  function smoothFixture(seed) {
    const N = 13;
    const b = tileBlock(N);
    const rnd = gen(seed);
    const S = { s0: 0, s1: 1, s2: 2, s3: 3, s4: 4, a0: 5, a1: 6, b0: 7, b1: 8, c0: 9, c1: 10, d0: 11, d1: 12 };
    for (let k = 0; k < N; k++) {
      wireVertices(b.t[k], k);
      setType(b.t[k], 4);
      setSub(b.t[k], 0);
      setFaceCount(b.t[k], 8);
      setVariation(b.t[k], 0);
      setDirty(b.t[k], 0);
      for (let i = 0; i < 4; i++) setNeighbour(b.t[k], i, null);
    }
    // vertices and normals of every entry the faces of these tiles name
    for (let i = 0; i < N * VSLOTS; i++) {
      vtxPtr(i, 0).writeFloat(rnd());
      vtxPtr(i, 1).writeFloat(rnd());
      vtxPtr(i, 2).writeFloat(rnd());
      nrmPtr(i, 0).writeFloat(rnd() / 97.0);
      nrmPtr(i, 1).writeFloat(rnd() / 97.0);
      nrmPtr(i, 2).writeFloat(rnd() / 97.0);
    }
    // the eight face normals of every tile (Tile + 0x74 + f * 0x38 + c * 4)
    for (let k = 0; k < N; k++)
      for (let f = 0; f < 8; f++)
        for (let c = 0; c < 3; c++)
          b.t[k].add(0x74 + f * 0x38 + c * 4).writeFloat(rnd() / 113.0);

    setNeighbour(b.t[S.s1], 0, b.t[S.a0]); setNeighbour(b.t[S.s1], 1, b.t[S.a1]);
    setNeighbour(b.t[S.s2], 2, b.t[S.b0]); setNeighbour(b.t[S.s2], 3, b.t[S.b1]);
    setNeighbour(b.t[S.s3], 0, b.t[S.a0]); setNeighbour(b.t[S.s3], 1, b.t[S.a1]);
    setNeighbour(b.t[S.s3], 2, b.t[S.b0]); setNeighbour(b.t[S.s3], 3, b.t[S.b1]);
    setNeighbour(b.t[S.s4], 0, b.t[S.a0]); setNeighbour(b.t[S.s4], 1, b.t[S.a1]);
    setNeighbour(b.t[S.s4], 2, b.t[S.c0]); setNeighbour(b.t[S.s4], 3, b.t[S.c1]);
    // c0 and c1 carry the diagonal tile for a = 0 and a = 1; b0 and b1 deliberately do not
    setNeighbour(b.t[S.c0], 0, b.t[S.d0]); setNeighbour(b.t[S.c0], 1, b.t[S.d1]);
    setNeighbour(b.t[S.c1], 0, b.t[S.d0]); setNeighbour(b.t[S.c1], 1, b.t[S.d1]);

    const out = { blk: b.blk, obj: b.blk, vtx: terrain().add(0xb28c8), nrm: terrain().add(0x70a18) };
    for (const name in S) out[name] = b.t[S[name]];
    return out;
  }

  // ----------------------------------------------------------------------------------------------
  // c3bf_blend: Tile::blendFaceTextures. Thirty-three tiles. p0..p17 are the subjects, one per arm of the
  // eight open-neighbour flags and of the two early returns; the rest are neighbours whose type decides
  // Tile::isOpenType (true for 0, 1, 2, 3, 7, 8, 9) and whose +0x28 decides Tile::edgeKind's water path.
  //     p0   no neighbour                        every `je (n == 0)` taken
  //     p1   n[0] = NO  (type 2, open)           b0 set
  //     p2   n[0] = NC  (type 4, closed)         b0 clear
  //     p3   n[1] = NO                           b1 set
  //     p4   n[1] = NC                           b1 clear
  //     p5   n[2] = NO                           b2 set
  //     p6   n[2] = NC                           b2 clear
  //     p7   n[3] = NO                           b3 set
  //     p8   n[3] = NC                           b3 clear
  //     p9   all four = NO, NO->n[2] = NO2 (3), NO->n[3] = NO3 (7)   b0..b7 all set, d0..d3 all computed
  //     p10  all four = NC, NC->n[2] = NC2 (5), NC->n[3] = NC3 (6)   b0..b3 and b4..b7 all clear, d0..d3 set
  //     p11  n[0] = NM0 (open; n[2] closed, n[3] open), n[1] = NM1 (open; n[2] open, n[3] closed),
  //          n[2] = NW, n[3] = NW2, own +0x28 = 1                    b4 clear, b5 set, b6 set, b7 clear
  //     p12  own type 4, so Tile::edgeKind takes the collar path; neighbours 4, 9, 5 and 0x11 give it the
  //          three results 0, 2 and 1 against the table installed above
  //     p13  own type 0x14                       the early return at 0x100136d6
  //     p14  own type 0                          Tile::isSolidType false, the early return at 0x10013754
  //     p15  own type 9                          the same, through the second value isSolidType rejects
  //     p16  own +0x28 = 1 with four water neighbours whose +0x28 are 2, 1, 5, 0: Tile::edgeKind's water
  //          path returns 2, 0, 1 and 1
  //     p17  n[0] and n[1] have a NULL n[2] and n[3]: used ONLY by the c3bf_blend_ct0 key, where the
  //          course-type guard fails, so those pointers reach Tile::edgeKind (which returns 0 for NULL)
  //          and never Tile::isOpenType (which would fault)
  // Every neighbour that a subject with n[2] or n[3] set reaches has its own n[2] and n[3] filled (with the
  // filler tile F), because the inner tests at 0x10013790 and 0x100137d8 read THIS tile's n[2] / n[3] and
  // then dereference the OTHER tile's (0x100137a8-0x100137ab).
  // ----------------------------------------------------------------------------------------------
  function blendFixture(courseType) {
    const IDX = { p0: 0, p1: 1, p2: 2, p3: 3, p4: 4, p5: 5, p6: 6, p7: 7, p8: 8, p9: 9, p10: 10, p11: 11,
                  p12: 12, p13: 13, p14: 14, p15: 15, p16: 16, p17: 17,
                  NO: 18, NO2: 19, NO3: 20, NC: 21, NC2: 22, NC3: 23, NW: 24, NW2: 25, NW3: 26, NW4: 27,
                  NM0: 28, NM1: 29, N9: 30, F: 31, HOLLOW: 32 };
    //          type, +0x28
    const DESC = { p0: [0x11, 0], p1: [0x11, 0], p2: [0x11, 0], p3: [0x11, 0], p4: [0x11, 0], p5: [0x11, 0],
                   p6: [0x11, 0], p7: [0x11, 0], p8: [0x11, 0], p9: [0x11, 0], p10: [0x11, 0], p11: [0x11, 1],
                   p12: [4, 0], p13: [0x14, 0], p14: [0, 0], p15: [9, 0], p16: [0x11, 1], p17: [0x11, 0],
                   NO: [2, 0], NO2: [3, 5], NO3: [7, 2], NC: [4, 0], NC2: [5, 1], NC3: [6, 0x11],
                   NW: [0x11, 0], NW2: [0x11, 5], NW3: [0x11, 2], NW4: [0x11, 1], NM0: [2, 0], NM1: [2, 0],
                   N9: [9, 0], F: [9, 0], HOLLOW: [2, 0] };
    const N = 33;
    const b = tileBlock(N);
    const out = { blk: b.blk, obj: b.blk };
    installCollar(out);
    setCourseType(out, courseType);
    for (const name in IDX) {
      const p = b.t[IDX[name]];
      setType(p, DESC[name][0]);
      setSub(p, DESC[name][1]);
      setFaceCount(p, 8);
      setVariation(p, IDX[name] % 0x19);     // every row of g_textures the vectors can reach exists
      setDirty(p, 0);
      for (let i = 0; i < 4; i++) setNeighbour(p, i, null);
      out[name] = p;
    }
    const T = (name) => b.t[IDX[name]];
    // the filler every neighbour points at, so no inner dereference is NULL (F is its own n[2] and n[3])
    setNeighbour(T('F'), 2, T('F')); setNeighbour(T('F'), 3, T('F'));
    for (const name of ['NO2', 'NO3', 'NC2', 'NC3', 'NW', 'NW2', 'NW3', 'NW4', 'N9']) {
      setNeighbour(T(name), 2, T('F')); setNeighbour(T(name), 3, T('F'));
    }
    setNeighbour(T('NO'), 2, T('NO2')); setNeighbour(T('NO'), 3, T('NO3'));
    setNeighbour(T('NC'), 2, T('NC2')); setNeighbour(T('NC'), 3, T('NC3'));
    setNeighbour(T('NM0'), 2, T('NC2')); setNeighbour(T('NM0'), 3, T('NO3'));
    setNeighbour(T('NM1'), 2, T('NO2')); setNeighbour(T('NM1'), 3, T('NC3'));
    // HOLLOW keeps n[2] and n[3] NULL on purpose (the c3bf_blend_ct0 key)

    setNeighbour(T('p1'), 0, T('NO'));
    setNeighbour(T('p2'), 0, T('NC'));
    setNeighbour(T('p3'), 1, T('NO'));
    setNeighbour(T('p4'), 1, T('NC'));
    setNeighbour(T('p5'), 2, T('NO'));
    setNeighbour(T('p6'), 2, T('NC'));
    setNeighbour(T('p7'), 3, T('NO'));
    setNeighbour(T('p8'), 3, T('NC'));
    for (let i = 0; i < 4; i++) setNeighbour(T('p9'), i, T('NO'));
    for (let i = 0; i < 4; i++) setNeighbour(T('p10'), i, T('NC'));
    setNeighbour(T('p11'), 0, T('NM0')); setNeighbour(T('p11'), 1, T('NM1'));
    setNeighbour(T('p11'), 2, T('NW'));  setNeighbour(T('p11'), 3, T('NW2'));
    setNeighbour(T('p12'), 0, T('NC'));  setNeighbour(T('p12'), 1, T('N9'));
    setNeighbour(T('p12'), 2, T('NC2')); setNeighbour(T('p12'), 3, T('NW'));
    for (let i = 0; i < 4; i++) setNeighbour(T('p13'), i, T('NO'));
    for (let i = 0; i < 4; i++) setNeighbour(T('p14'), i, T('NO'));
    for (let i = 0; i < 4; i++) setNeighbour(T('p15'), i, T('NO'));
    setNeighbour(T('p16'), 0, T('NW3')); setNeighbour(T('p16'), 1, T('NW4'));
    setNeighbour(T('p16'), 2, T('NW2')); setNeighbour(T('p16'), 3, T('NW'));
    setNeighbour(T('p17'), 0, T('HOLLOW')); setNeighbour(T('p17'), 1, T('HOLLOW'));
    setNeighbour(T('p17'), 2, T('NO'));     setNeighbour(T('p17'), 3, T('NO'));
    return out;
  }

  // ----------------------------------------------------------------------------------------------
  // c3bf_reblend: Tile::reblendNeighbourhood with g_courseType 0, so Tile::blendFaceTextures never reaches
  // Tile::isOpenType and a NULL inner neighbour is safe (it only reaches Tile::edgeKind, which returns 0 for
  // NULL). Twenty-seven tiles; r0..r10 are the subjects, one per combination of the four neighbour pointers
  // the nine guarded calls test:
  //     r0  none            r1  n[0]            r2  n[2]            r3  n[0] + n[2]
  //     r4  n[1]            r5  n[1] + n[2]     r6  n[3]            r7  n[1] + n[3]
  //     r8  n[0] + n[3]     r9  all four, with four distinct diagonals (the nine calls hit nine tiles)
  //     r10 all four, with the diagonals G (type 0x14) and G2 (type 9), so two of the nine calls take
  //         Tile::blendFaceTextures' two early returns
  // Each tile that a diagonal call reaches has its own n[2] and n[3] filled, because the call sites at
  // 0x1001355f, 0x10013597, 0x100135cf and 0x10013607 dereference the OTHER tile's pointer after testing
  // THIS tile's.
  // ----------------------------------------------------------------------------------------------
  function reblendFixture() {
    const IDX = { r0: 0, r1: 1, r2: 2, r3: 3, r4: 4, r5: 5, r6: 6, r7: 7, r8: 8, r9: 9, r10: 10,
                  M0: 11, M1: 12, M2: 13, M3: 14, G: 15, G2: 16, K0: 17, K1: 18, K2: 19, K3: 20,
                  L0: 21, L1: 22, L2: 23, L3: 24, MG0: 25, MG1: 26 };
    const TYPE = { r0: 4, r1: 5, r2: 6, r3: 7, r4: 8, r5: 10, r6: 0x11, r7: 0x12, r8: 0x13, r9: 0x15,
                   r10: 2, M0: 4, M1: 5, M2: 6, M3: 0x11, G: 0x14, G2: 9, K0: 2, K1: 3, K2: 7, K3: 8,
                   L0: 10, L1: 0x12, L2: 0x13, L3: 0x15, MG0: 4, MG1: 5 };
    const N = 27;
    const b = tileBlock(N);
    const out = { blk: b.blk, obj: b.blk };
    installCollar(out);
    setCourseType(out, 0);
    for (const name in IDX) {
      const p = b.t[IDX[name]];
      setType(p, TYPE[name]);
      setSub(p, IDX[name] & 3);
      setFaceCount(p, 8);
      setVariation(p, IDX[name] % 0x19);
      setDirty(p, 0);
      for (let i = 0; i < 4; i++) setNeighbour(p, i, null);
      out[name] = p;
    }
    const T = (name) => b.t[IDX[name]];
    setNeighbour(T('G'), 2, T('G')); setNeighbour(T('G'), 3, T('G'));
    setNeighbour(T('G2'), 2, T('G2')); setNeighbour(T('G2'), 3, T('G2'));
    for (const name of ['M0', 'M1']) { setNeighbour(T(name), 2, T('G')); setNeighbour(T(name), 3, T('G')); }
    setNeighbour(T('K0'), 2, T('L0')); setNeighbour(T('K0'), 3, T('L1'));
    setNeighbour(T('K1'), 2, T('L2')); setNeighbour(T('K1'), 3, T('L3'));
    setNeighbour(T('MG0'), 2, T('G')); setNeighbour(T('MG0'), 3, T('G2'));
    setNeighbour(T('MG1'), 2, T('G2')); setNeighbour(T('MG1'), 3, T('G'));

    setNeighbour(T('r1'), 0, T('M0'));
    setNeighbour(T('r2'), 2, T('M2'));
    setNeighbour(T('r3'), 0, T('M0')); setNeighbour(T('r3'), 2, T('M2'));
    setNeighbour(T('r4'), 1, T('M1'));
    setNeighbour(T('r5'), 1, T('M1')); setNeighbour(T('r5'), 2, T('M2'));
    setNeighbour(T('r6'), 3, T('M3'));
    setNeighbour(T('r7'), 1, T('M1')); setNeighbour(T('r7'), 3, T('M3'));
    setNeighbour(T('r8'), 0, T('M0')); setNeighbour(T('r8'), 3, T('M3'));
    setNeighbour(T('r9'), 0, T('K0')); setNeighbour(T('r9'), 1, T('K1'));
    setNeighbour(T('r9'), 2, T('K2')); setNeighbour(T('r9'), 3, T('K3'));
    setNeighbour(T('r10'), 0, T('MG0')); setNeighbour(T('r10'), 1, T('MG1'));
    setNeighbour(T('r10'), 2, T('K2'));  setNeighbour(T('r10'), 3, T('K3'));
    return out;
  }

  // ----------------------------------------------------------------------------------------------
  // c3bf_reblend_ct1: Tile::reblendNeighbourhood with g_courseType 1. A 4x4 torus in which every tile has all
  // four neighbours set (n[0] = (x, y-1), n[1] = (x, y+1), n[2] = (x-1, y), n[3] = (x+1, y), all modulo 4), so
  // no inner dereference of Tile::blendFaceTextures can be NULL while the course-type guard holds. The types
  // alternate 0x11 (water, Tile::isOpenType false) and 2 (open) in a checkerboard, so a water subject has four
  // open neighbours (b0..b3 set) whose own diagonals are water (b4..b7 clear), and an open subject takes the
  // collar path of Tile::edgeKind instead.
  // ----------------------------------------------------------------------------------------------
  function reblendCt1Fixture() {
    const N = 16;
    const b = tileBlock(N);
    const out = { blk: b.blk, obj: b.blk };
    installCollar(out);
    setCourseType(out, 1);
    const at = (x, y) => b.t[((y + 4) % 4) * 4 + ((x + 4) % 4)];
    for (let y = 0; y < 4; y++) {
      for (let x = 0; x < 4; x++) {
        const p = at(x, y);
        setType(p, (x + y) % 2 === 0 ? 0x11 : 2);
        setSub(p, (x * 3 + y) % 4);
        setFaceCount(p, 8);
        setVariation(p, (y * 4 + x) % 0x19);
        setDirty(p, 0);
        out['w' + (y * 4 + x)] = p;
      }
    }
    for (let y = 0; y < 4; y++) {
      for (let x = 0; x < 4; x++) {
        const p = at(x, y);
        setNeighbour(p, 0, at(x, y - 1));
        setNeighbour(p, 1, at(x, y + 1));
        setNeighbour(p, 2, at(x - 1, y));
        setNeighbour(p, 3, at(x + 1, y));
      }
    }
    return out;
  }

  return {
    c3bf_smooth() { return smoothFixture(0x5bf00001); },
    // the same tiles with a second, unrelated seed: a different set of vertex positions and face normals
    c3bf_smooth2() { return smoothFixture(0x13ef0002); },
    c3bf_blend() { return blendFixture(1); },
    // g_courseType 0: the `jne 0x1001376d` side of all eight open-neighbour tests
    c3bf_blend_ct0() { return blendFixture(0); },
    c3bf_reblend() { return reblendFixture(); },
    c3bf_reblend_ct1() { return reblendCt1Fixture(); },
  };
})());
