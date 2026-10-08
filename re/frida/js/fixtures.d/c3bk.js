// Batch c3bk fixtures (Terrain.dll, round 16): Tile::setTypeId and its six callees. Every Tile here is our
// own allocation; the live Terrain object and its tile array are never written. The module memory the bodies
// under test read or write is:
//
//   g_vertices    0x100b28c8   three floats per entry, stride 0xc   (component 1 written by the four walks)
//   g_textures    0x100687f8   0x384 per type, 0x24 per variation   (READ ONLY, by Tile::resetFaceTextures
//                                                                    and by Tile::setFaceTexture)
//   g_courseType  0x10070a0c   dword                                (read by Tile::blendFaceTextures)
//   g_collarInfo  0x10106b48   pointer to a dword table             (read by Tile::edgeKind)
//
// That is the same working set batch c3bf measured as idle at the main menu, where diff_hook runs
// (log/c3/alt/c3bf_probe.py: zero calls into Tile::setTypeId, Tile::reblendNeighbourhood,
// Tile::blendFaceTextures, Tile::edgeKind, Terrain::rebuild, Terrain::relight, Terrain::loadNewCourseType and
// Terrain::Terrain over 8 s, g_courseType 0 and g_collarInfo NULL unchanged in 2467 samples). The registry
// declares every window the vectors touch as a state region, so diff_hook snapshots it before each vector,
// compares it after each arm and writes it back.
//
// g_collarInfo is NULL at the menu and Tile::edgeKind dereferences it on its non-0x11 path (0x100156fb,
// 0x10015701) with this->type and the other tile's type as dword indices, so every fixture installs a table
// of its own, exactly as batch c3as's c3as_tiles and c3bf's blend fixtures do, and the registry lists the
// pointer slot ($collarSlot) in the state regions. g_courseType is set to 0 by every fixture here: with it
// at 0 Tile::blendFaceTextures never calls Tile::isOpenType (jne 0x1001376d), which is the only path that
// would dereference a neighbour's NULL n[2] or n[3].
//
// All tile types used below are <= 0x24 and all variation / sub words <= 0x18, so every g_textures and
// g_collarInfo index the bodies form is inside those tables.
Object.assign(globalThis.DIFF_FIXTURES, (function () {
  const TSIZE = 0x250;              // a Tile ends at +0x244 (stride 0x248); 0x250 keeps the blocks aligned
  const VSLOTS = 24;                // eight faces x three vertex slots

  function terrain() { return Process.getModuleByName('Terrain.dll').base; }

  // One block of `n` tiles filled with a deterministic non-zero pattern, so a field a body does NOT write
  // keeps a value the state hash would notice if one arm wrote it.
  function tileBlock(n) {
    const blk = Memory.alloc(n * TSIZE);
    const fill = [];
    for (let i = 0; i < n * TSIZE; i++) fill.push((i * 53 + 0x2d) & 0xff);
    blk.writeByteArray(fill);
    const t = [];
    for (let i = 0; i < n; i++) t.push(blk.add(i * TSIZE));
    return { blk, t };
  }

  function setType(p, v) { p.add(0x24).writeS32(v); }
  function setSub(p, v) { p.add(0x28).writeS32(v); }
  function setNeighbour(p, i, q) { p.add(0x34 + i * 4).writePointer(q || ptr(0)); }
  function setFaceCount(p, v) { p.add(0x44).writeS32(v); }
  function setHasPath(p, v) { p.add(0x208).writeU8(v); }
  function setPathFlag(p, v) { p.add(0x20a).writeU8(v); }
  function setVariation(p, v) { p.add(0x240).writeS32(v); }
  function setDirty(p, v) { p.add(0x244).writeU8(v); }

  // Face f, vertex slot v of tile `p`: Tile + 0x48 + f * 0x38 + v * 4.
  function setFaceVertex(p, f, v, idx) { p.add(0x48 + f * 0x38 + v * 4).writeS32(idx); }
  // Give tile number k of a block the 24 vertex indices k * 24 .. k * 24 + 23, so no two tiles of a block
  // share an entry of g_vertices and every one of the walk's 25 stores lands on an entry of its own.
  function wireVertices(p, k) {
    for (let f = 0; f < 8; f++)
      for (let v = 0; v < 3; v++) setFaceVertex(p, f, v, k * VSLOTS + f * 3 + v);
  }

  // The three floats of g_vertices entry i. The array is the 3w x 3h vertex grid initGrid 0x1000c560 lays
  // out, so the first few hundred entries are deep inside it.
  function vtxPtr(i, c) { return terrain().add(0xb28c8 + i * 12 + c * 4); }

  // g_collarInfo: 0x100 dwords, entry = (type % 5) * 100 (the shape batches c3as and c3bf used), so two
  // types are "the same collar" only when they are congruent modulo 5. Installed in the module pointer slot.
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

  // Heights for g_vertices[i][1]. Three magnitudes on purpose: around 1 (where +20.0 / -13.0 changes every
  // bit of the mantissa), around 1e5 (where the delta is a few ULP) and around 1e8 (where a float ULP is 8,
  // so the delta is absorbed into one or two steps and the rounding of the add is what decides the result).
  // Every value has a non-terminating binary expansion (k / 173 and k / 29), so no sum below is exact by
  // construction.
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

  // Fill g_vertices entries 0 .. n*24-1: component 1 is the height the walks add to, components 0 and 2 are
  // seeded too and are never written by any body here (a write by either arm would show in the state hash).
  function seedVertices(n, seed) {
    const h = heights(seed);
    for (let i = 0; i < n * VSLOTS; i++) {
      vtxPtr(i, 0).writeFloat(h() / 7.0);
      vtxPtr(i, 1).writeFloat(h());
      vtxPtr(i, 2).writeFloat(h() / 11.0);
    }
  }

  // ----------------------------------------------------------------------------------------------
  // c3bk_walk6 / c3bk_walk7: the four corner walks (Tile::enterType6 0xde60, Tile::enterType7 0xd540,
  // Tile::leaveType6 0x14190, Tile::leaveType7 0x149e0). The walk's guards all compare a neighbour's type
  // against ONE value, 6 or 7, so the block is built twice: `tt` is the type the guards accept and `ot` (2)
  // is a type they reject. A function run against the block of the OTHER type takes the "not taken" side of
  // every type test at once, which is what the second key of each function is for.
  //
  // 32 tiles. e0..e14 are the subjects, one per combination of the four neighbour pointers and of the three
  // tests inside each diagonal block:
  //     e0   no neighbour at all           every `je (n == 0)` taken, only the centre vertex moves
  //     e1   n0 = AT (type tt)             the n0 block runs, its two inner blocks do not (no n2, no n3)
  //     e2   n0 = AX (type ot)             the n0 block's type test fails
  //     e3   n1 = BT       e4  n1 = BX
  //     e5   n2 = CT       e6  n2 = CX
  //     e7   n3 = DT       e8  n3 = DX
  //     e9   n0 = P0, n1 = P1, n2 = P2, n3 = P3, all type tt, and P0 / P1 carry DIAGONALS of type tt
  //          (Q02, Q03, Q12, Q13): every one of the four inner blocks runs in full
  //     e10  n0 = AT, n1 = BT, n2 = CT, n3 = DT: all four neighbours are type tt but their diagonals are the
  //          filler F (type ot), so each inner block fails on its THIRD test only
  //     e11  n0 = AT, n2 = CT        the n0/n2 inner block alone, third test failing
  //     e12  n0 = AT, n2 = CX        the n0/n2 inner block failing on its SECOND test
  //     e13  n0 = AX, n1 = BX, n2 = CX, n3 = DX: all four pointers set, every type test failing, so the
  //          vertex walk writes only the centre vertex while the dirty walk still visits all nine tiles
  //     e14  n0 = AT, n1 = BT (type tt), n2 = CX, n3 = DX (type ot): the n0 and n1 blocks run and all FOUR
  //          inner blocks fail on their SECOND test, the one on the n2 / n3 type
  // Every neighbour carries an n2 and an n3 (the filler F, which is its own n2 and n3), because both the
  // inner type tests and the dirty walk read THIS tile's n2 / n3 and then dereference the OTHER tile's
  // (e.g. 0x1000df22 against 0x1000df46, and 0x1000e431 against 0x1000e43d).
  // ----------------------------------------------------------------------------------------------
  function walkFixture(tt, seed) {
    const ot = 2;                                  // a type neither 6 nor 7
    const IDX = { e0: 0, e1: 1, e2: 2, e3: 3, e4: 4, e5: 5, e6: 6, e7: 7, e8: 8, e9: 9, e10: 10, e11: 11,
                  e12: 12, e13: 13, e14: 14,
                  AT: 15, AX: 16, BT: 17, BX: 18, CT: 19, CX: 20, DT: 21, DX: 22, F: 23,
                  P0: 24, P1: 25, P2: 26, P3: 27, Q02: 28, Q03: 29, Q12: 30, Q13: 31 };
    const TT = ['AT', 'BT', 'CT', 'DT', 'P0', 'P1', 'P2', 'P3', 'Q02', 'Q03', 'Q12', 'Q13'];
    const N = 32;
    const b = tileBlock(N);
    const out = { blk: b.blk, obj: b.blk, n: N };
    installCollar(out);
    setCourseType(out, 0);
    for (const name in IDX) {
      const k = IDX[name];
      const p = b.t[k];
      wireVertices(p, k);
      setType(p, name[0] === 'e' ? tt : (TT.indexOf(name) >= 0 ? tt : ot));
      setSub(p, k % 5);
      setFaceCount(p, 8);
      setVariation(p, k % 0x19);
      setHasPath(p, k & 1);
      setPathFlag(p, (k >> 1) & 1);
      setDirty(p, 0);
      for (let i = 0; i < 4; i++) setNeighbour(p, i, null);
      out[name] = p;
    }
    const T = (name) => b.t[IDX[name]];
    // F is its own n2 and n3, and every neighbour tile points at it, so no inner dereference is NULL
    setNeighbour(T('F'), 2, T('F')); setNeighbour(T('F'), 3, T('F'));
    for (const name of ['AT', 'AX', 'BT', 'BX', 'CT', 'CX', 'DT', 'DX',
                        'P2', 'P3', 'Q02', 'Q03', 'Q12', 'Q13']) {
      setNeighbour(T(name), 2, T('F')); setNeighbour(T(name), 3, T('F'));
    }
    // the only two neighbours whose diagonals are themselves of type tt
    setNeighbour(T('P0'), 2, T('Q02')); setNeighbour(T('P0'), 3, T('Q03'));
    setNeighbour(T('P1'), 2, T('Q12')); setNeighbour(T('P1'), 3, T('Q13'));

    setNeighbour(T('e1'), 0, T('AT'));
    setNeighbour(T('e2'), 0, T('AX'));
    setNeighbour(T('e3'), 1, T('BT'));
    setNeighbour(T('e4'), 1, T('BX'));
    setNeighbour(T('e5'), 2, T('CT'));
    setNeighbour(T('e6'), 2, T('CX'));
    setNeighbour(T('e7'), 3, T('DT'));
    setNeighbour(T('e8'), 3, T('DX'));
    setNeighbour(T('e9'), 0, T('P0')); setNeighbour(T('e9'), 1, T('P1'));
    setNeighbour(T('e9'), 2, T('P2')); setNeighbour(T('e9'), 3, T('P3'));
    setNeighbour(T('e10'), 0, T('AT')); setNeighbour(T('e10'), 1, T('BT'));
    setNeighbour(T('e10'), 2, T('CT')); setNeighbour(T('e10'), 3, T('DT'));
    setNeighbour(T('e11'), 0, T('AT')); setNeighbour(T('e11'), 2, T('CT'));
    setNeighbour(T('e12'), 0, T('AT')); setNeighbour(T('e12'), 2, T('CX'));
    setNeighbour(T('e13'), 0, T('AX')); setNeighbour(T('e13'), 1, T('BX'));
    setNeighbour(T('e13'), 2, T('CX')); setNeighbour(T('e13'), 3, T('DX'));
    setNeighbour(T('e14'), 0, T('AT')); setNeighbour(T('e14'), 1, T('BT'));
    setNeighbour(T('e14'), 2, T('CX')); setNeighbour(T('e14'), 3, T('DX'));

    seedVertices(N, seed);
    out.vtx = terrain().add(0xb28c8);
    return out;
  }

  // ----------------------------------------------------------------------------------------------
  // c3bk_path: Tile::setPathFlagForType 0xd480. Six subjects crossing the two sides of its only guard
  // (byte [this+0x208], je 0x1000d4aa) with the two starting values of the flag it writes
  // (byte [this+0x20a]), and with and without a neighbourhood for the re-blend it always ends in:
  //     pa  hasPath 1, flag 0, no neighbour, type 4
  //     pb  hasPath 1, flag 1, no neighbour, type 5
  //     pc  hasPath 0, flag 1, no neighbour, type 2       (the flag must survive)
  //     pd  hasPath 1, flag 0, four neighbours, type 0x11
  //     pe  hasPath 1, flag 1, four neighbours, type 9    (Tile::isSolidType rejects 9, so the re-blend
  //                                                        returns early for the subject itself)
  //     pf  hasPath 0, flag 0, four neighbours, type 0x14 (the re-blend's other early return)
  // The `type` argument never reaches g_textures (the re-blend uses the tile's OWN type at +0x24), so the
  // vectors may pass -1 and 0x7fffffff.
  // ----------------------------------------------------------------------------------------------
  function pathFixture() {
    const IDX = { pa: 0, pb: 1, pc: 2, pd: 3, pe: 4, pf: 5, PN0: 6, PN1: 7, PN2: 8, PN3: 9, PF: 10 };
    const DESC = { pa: [4, 1, 0], pb: [5, 1, 1], pc: [2, 0, 1],
                   pd: [0x11, 1, 0], pe: [9, 1, 1], pf: [0x14, 0, 0],
                   PN0: [3, 0, 0], PN1: [7, 1, 0], PN2: [0x11, 0, 1], PN3: [0x24, 1, 1], PF: [8, 0, 0] };
    const N = 11;
    const b = tileBlock(N);
    const out = { blk: b.blk, obj: b.blk, n: N };
    installCollar(out);
    setCourseType(out, 0);
    for (const name in IDX) {
      const k = IDX[name];
      const p = b.t[k];
      wireVertices(p, k);
      setType(p, DESC[name][0]);
      setSub(p, k % 5);
      setFaceCount(p, 8);
      setVariation(p, k % 0x19);
      setHasPath(p, DESC[name][1]);
      setPathFlag(p, DESC[name][2]);
      setDirty(p, 0);
      for (let i = 0; i < 4; i++) setNeighbour(p, i, null);
      out[name] = p;
    }
    const T = (name) => b.t[IDX[name]];
    setNeighbour(T('PF'), 2, T('PF')); setNeighbour(T('PF'), 3, T('PF'));
    for (const name of ['PN0', 'PN1', 'PN2', 'PN3']) {
      setNeighbour(T(name), 2, T('PF')); setNeighbour(T(name), 3, T('PF'));
    }
    for (const name of ['pd', 'pe', 'pf']) {
      setNeighbour(T(name), 0, T('PN0')); setNeighbour(T(name), 1, T('PN1'));
      setNeighbour(T(name), 2, T('PN2')); setNeighbour(T(name), 3, T('PN3'));
    }
    seedVertices(N, 0x3bc0010b);
    out.vtx = terrain().add(0xb28c8);
    return out;
  }

  // ----------------------------------------------------------------------------------------------
  // c3bk_reset: Tile::resetFaceTextures 0x15230. It takes no argument, so one subject is one vector; the
  // fourteen below cross its three tests - hasPath (je 0x1001525a), type == 0 (jne 0x1001526d) and the two
  // loop bounds against faceCount (jge 0x1001528a and 0x100152c7):
  //     q0  type 0, faceCount 8, hasPath 1, flag 1, sub 0        the type-0 loop, full
  //     q1  type 0, faceCount 8, hasPath 0, flag 1, sub 7        the same with the flag left alone
  //     q2  type 0, faceCount 3, hasPath 1, flag 0, sub 0x18     a partial type-0 loop
  //     q3  type 0, faceCount 0, hasPath 1, flag 1, sub 4        the type-0 loop body never runs
  //     q4  type 9, faceCount 8, hasPath 1, flag 1, variation 0  the else loop, full
  //     q5  type 9, faceCount 0, hasPath 0, flag 0               the else loop body never runs
  //     q6  type 9, faceCount 5, hasPath 1, flag 1, variation 0x18
  //     q7  type 0x24, faceCount 8, variation 0x18               the last row of g_textures
  //     q8  type 4, faceCount 8, hasPath 0                       the re-blend rewrites all eight faces
  //     q9  type 0x11, faceCount 8, hasPath 1
  //     q10 type 0x14, faceCount 8                               the re-blend's 0x14 early return
  //     q11 type 2, faceCount 8, with four neighbours            the re-blend touches nine tiles
  //     q12 type 0, faceCount 8, with four neighbours            type-0 loop plus a nine-tile re-blend
  //     q13 type 9, faceCount 8, with four neighbours
  // Types 0 and 9 are the only two Tile::setTypeId routes here and both are rejected by Tile::isSolidType
  // (0x1001374b), so the re-blend returns before touching the subject's faces and the loop's writes are what
  // the A/B compares.
  // ----------------------------------------------------------------------------------------------
  function resetFixture() {
    //              type, faceCount, hasPath, flag, sub, variation, ring
    const DESC = {
      q0:  [0,    8, 1, 1, 0,    3, 0], q1:  [0,    8, 0, 1, 7,    3, 0],
      q2:  [0,    3, 1, 0, 0x18, 3, 0], q3:  [0,    0, 1, 1, 4,    3, 0],
      q4:  [9,    8, 1, 1, 2,    0, 0], q5:  [9,    0, 0, 0, 2,    5, 0],
      q6:  [9,    5, 1, 1, 2, 0x18, 0], q7:  [0x24, 8, 1, 0, 1, 0x18, 0],
      q8:  [4,    8, 0, 1, 1,    2, 0], q9:  [0x11, 8, 1, 0, 1,    6, 0],
      q10: [0x14, 8, 1, 1, 1,    6, 0], q11: [2,    8, 0, 0, 1,    9, 1],
      q12: [0,    8, 1, 1, 6,    9, 1], q13: [9,    8, 1, 0, 6,   11, 1],
      QN0: [3,    8, 0, 0, 2,    4, 0], QN1: [0x11, 8, 1, 1, 2,    4, 0],
      QN2: [7,    8, 0, 1, 2,    4, 0], QN3: [0x24, 8, 1, 0, 2,    4, 0],
      QF:  [8,    8, 0, 0, 2,    4, 0],
    };
    const names = Object.keys(DESC);
    const N = names.length;
    const b = tileBlock(N);
    const out = { blk: b.blk, obj: b.blk, n: N };
    installCollar(out);
    setCourseType(out, 0);
    const T = {};
    names.forEach((name, k) => {
      const p = b.t[k];
      const d = DESC[name];
      wireVertices(p, k);
      setType(p, d[0]);
      setFaceCount(p, d[1]);
      setHasPath(p, d[2]);
      setPathFlag(p, d[3]);
      setSub(p, d[4]);
      setVariation(p, d[5]);
      setDirty(p, 0);
      for (let i = 0; i < 4; i++) setNeighbour(p, i, null);
      T[name] = p;
      out[name] = p;
    });
    setNeighbour(T.QF, 2, T.QF); setNeighbour(T.QF, 3, T.QF);
    for (const name of ['QN0', 'QN1', 'QN2', 'QN3']) {
      setNeighbour(T[name], 2, T.QF); setNeighbour(T[name], 3, T.QF);
    }
    for (const name of names) {
      if (DESC[name][6] !== 1) continue;
      setNeighbour(T[name], 0, T.QN0); setNeighbour(T[name], 1, T.QN1);
      setNeighbour(T[name], 2, T.QN2); setNeighbour(T[name], 3, T.QN3);
    }
    seedVertices(N, 0x3bc0020d);
    out.vtx = terrain().add(0xb28c8);
    return out;
  }

  // ----------------------------------------------------------------------------------------------
  // c3bk_settype: Tile::setTypeId 0x14020. Eight subjects in two groups, because the dispatch calls at most
  // one of the two entry functions per call and their guards look at different types:
  //     group A (a0..a3) sits in a ring of FOUR type-6 neighbours whose own diagonals are type 6 too, so a
  //             new type of 6 runs Tile::enterType6's full walk, and a subject whose OLD type is 6 runs
  //             Tile::leaveType6's full walk first;
  //     group B (b0..b3) sits in the same ring built of type-7 tiles, for Tile::enterType7 and
  //             Tile::leaveType7.
  // The subjects' own starting types cover the three cases of the leave step (cmp 0x10014059 against 7,
  // cmp 0x1001406a against 6, cmp 0x1001407d against 0x11) with both values of the path byte at +0x208:
  //     a0  type 6,    hasPath 0, flag 1       a1  type 6,    hasPath 1, flag 0
  //     a2  type 0x11, hasPath 1, flag 1       a3  type 4,    hasPath 0, flag 0
  //     b0  type 7,    hasPath 0, flag 0       b1  type 7,    hasPath 1, flag 1
  //     b2  type 0x11, hasPath 0, flag 1       b3  type 5,    hasPath 1, flag 0
  // ----------------------------------------------------------------------------------------------
  function setTypeFixture() {
    const SUBJ = { a0: [6, 0, 1, 6], a1: [6, 1, 0, 6], a2: [0x11, 1, 1, 6], a3: [4, 0, 0, 6],
                   b0: [7, 0, 0, 7], b1: [7, 1, 1, 7], b2: [0x11, 0, 1, 7], b3: [5, 1, 0, 7] };
    const RING = ['R0', 'R1', 'R2', 'R3', 'D0', 'D1', 'D2', 'D3'];
    const names = Object.keys(SUBJ)
      .concat(RING.map((s) => s + '_6'), RING.map((s) => s + '_7'), ['F']);
    const N = names.length;                       // 8 subjects + 8 + 8 ring tiles + the filler = 25
    const b = tileBlock(N);
    const out = { blk: b.blk, obj: b.blk, n: N };
    installCollar(out);
    setCourseType(out, 0);
    const T = {};
    names.forEach((name, k) => {
      const p = b.t[k];
      wireVertices(p, k);
      const s = SUBJ[name];
      setType(p, s ? s[0] : (name === 'F' ? 2 : parseInt(name.slice(-1), 10)));
      setSub(p, k % 5);
      setFaceCount(p, 8);
      setVariation(p, k % 0x19);
      setHasPath(p, s ? s[1] : (k & 1));
      setPathFlag(p, s ? s[2] : ((k >> 1) & 1));
      setDirty(p, 0);
      for (let i = 0; i < 4; i++) setNeighbour(p, i, null);
      T[name] = p;
      out[name] = p;
    });
    setNeighbour(T.F, 2, T.F); setNeighbour(T.F, 3, T.F);
    for (const tt of ['6', '7']) {
      // R0 and R1 are the two neighbours whose diagonals the inner blocks read, so they get type-tt ones;
      // R2, R3 and the four diagonals point at the filler (type 2), which no guard accepts.
      setNeighbour(T['R0_' + tt], 2, T['D0_' + tt]); setNeighbour(T['R0_' + tt], 3, T['D1_' + tt]);
      setNeighbour(T['R1_' + tt], 2, T['D2_' + tt]); setNeighbour(T['R1_' + tt], 3, T['D3_' + tt]);
      for (const name of ['R2_', 'R3_', 'D0_', 'D1_', 'D2_', 'D3_']) {
        setNeighbour(T[name + tt], 2, T.F); setNeighbour(T[name + tt], 3, T.F);
      }
    }
    for (const name in SUBJ) {
      const tt = String(SUBJ[name][3]);
      setNeighbour(T[name], 0, T['R0_' + tt]); setNeighbour(T[name], 1, T['R1_' + tt]);
      setNeighbour(T[name], 2, T['R2_' + tt]); setNeighbour(T[name], 3, T['R3_' + tt]);
    }
    seedVertices(N, 0x3bc00311);
    out.vtx = terrain().add(0xb28c8);
    return out;
  }

  return {
    c3bk_walk6() { return walkFixture(6, 0x3bc00001); },
    c3bk_walk7() { return walkFixture(7, 0x3bc00002); },
    // the same 31 tiles with a second, unrelated set of heights
    c3bk_walk6b() { return walkFixture(6, 0x3bc00003); },
    c3bk_walk7b() { return walkFixture(7, 0x3bc00004); },
    c3bk_path() { return pathFixture(); },
    c3bk_reset() { return resetFixture(); },
    c3bk_settype() { return setTypeFixture(); },
  };
})());
