// Batch c3as fixtures (Terrain.dll). Every buffer here is our own allocation; the live Terrain object, its tile
// array and the module's vertex array are never written.
//
// c3as_tiles   28 Tile buffers (stride 0x248) for Tile::isOpenType 0x100155b0 and Tile::edgeKind 0x10015650,
//              seeded only at +0x24 (type) and +0x28 (rotation/variation), plus a 0x100-entry collar table and
//              the module pointer that selects it.
//              THE ONE MODULE GLOBAL THIS BATCH WRITES: Tile::edgeKind's non-0x11 path reads the DWORD table
//              through the pointer at Terrain.dll+0x106b48 (VA 0x10106b48, loads at 0x100156fb and 0x10015701).
//              That pointer lives in .bss (zero at load) and is only ever set by Terrain::loadNewCourseType and
//              read by the Terrain constructor/destructor, Terrain::passCollarInfo and Tile::edgeKind itself
//              (re/analysis/terrain/*.md), none of which run while the game sits in the main menu, where the
//              A/B runs. The fixture saves the value it found in `collarOld` and installs a table of our own,
//              and the registry lists the pointer slot ($collarSlot) in edgeKind's state regions so diff_hook
//              snapshots and restores it around every vector.
//
// c3as_math    one 0x900 block holding, for the x87 keys: 24 mutable 3-float vectors $v0..$v23 (normalize and
//              rotateAxis), 10 two-float output points $o0..$o9 and 12 two-float control points $c0..$c11
//              (Terrain::hermitePoint), and 16 face records of 0x40 bytes $f0..$f15 (Terrain::faceNormal:
//              three vertex indices at +0, +4, +8 and the normal at +0x2c).
//              Terrain::faceNormal indexes the module array at Terrain.dll+0xb28c8 with no bounds check, as
//              index*12 computed in 32-bit arithmetic, so the fixture does NOT touch that array: it allocates
//              its own 24 vertices at an address whose distance from the array is a multiple of 12 and stores
//              the matching (wrapping) indices in the face records. Both arms then read the same vertices of
//              ours and the module's own data is left alone.
//              The operands are deliberately inexact in float (0.1, 1/3, 1e-7, 3e5, 1.0000001, 16777215) so a
//              body computed in float instead of at the measured 53-bit x87 precision returns a different float.
//
// c3as_nodes   16 list nodes of 12 bytes ({next, prev, value}) and 20 one-pointer iterators, for the five
//              std::list<Tile*> helpers. None of them is dereferenced by the hooked accessors, and no node is
//              ever linked into a list of the game's.
Object.assign(globalThis.DIFF_FIXTURES, {
  c3as_tiles() {
    const N = 28, STRIDE = 0x248;
    // (type at +0x24, rotation at +0x28)
    //  0..6   the seven types Tile::isOpenType returns 1 for, in the order it compares them
    //  7..15  types it returns 0 for, including the extremes of the signed dword
    //  16..27 the pairs Tile::edgeKind needs: 0x11 with rotations 0, 1, 2, 3, 5, 7, -128, 0x180, 0x101
    //         and the three non-0x11 types 4, 9, 5 (collar[4] == collar[9], collar[4] != collar[5])
    const TYPE = [2, 7, 1, 0, 9, 8, 3, 4, 5, 6, 0x11, -1, 0x7fffffff, 10, 0x16, -0x80000000,
                  0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 4, 9, 5, 0x11];
    const ROT = [0, 1, 2, 3, 0, 1, 5, 0, 2, 7, 0, 0, 1, 2, 0, 3,
                 0, 3, 1, 2, -128, 0x180, 5, 7, 0, 0, 0, 0x101];
    const base = Memory.alloc(N * STRIDE);
    const fill = [];
    for (let i = 0; i < N; i++)
      for (let j = 0; j < STRIDE; j++) fill.push((i * 31 + j * 13 + 0x27) & 0xff);
    base.writeByteArray(fill);
    const out = { base, obj: base };
    for (let i = 0; i < N; i++) {
      const t = base.add(i * STRIDE);
      t.add(0x24).writeS32(TYPE[i] | 0);
      t.add(0x28).writeS32(ROT[i] | 0);
      out['t' + i] = t;
    }
    // collar table: 0x100 dwords, entry = (type % 5) * 100, so types 4 and 9 share an entry and 4 and 5 do not
    const collar = Memory.alloc(0x100 * 4);
    for (let i = 0; i < 0x100; i++) collar.add(i * 4).writeS32((i % 5) * 100);
    const slot = Process.getModuleByName('Terrain.dll').base.add(0x106b48);
    out.collarOld = slot.readPointer();
    slot.writePointer(collar);
    out.collar = collar;
    out.collarSlot = slot;
    return out;
  },

  c3as_math() {
    const blk = Memory.alloc(0x900);
    const out = { blk, obj: blk };
    let p = 0;

    // 24 mutable 3-float vectors (normalize, rotateAxis). No all-zero vector: normalize divides by the length.
    // $v16..$v23 were chosen so that computing normalize's sum of squares and its three divisions in FLOAT
    // instead of at the measured 53-bit x87 precision returns a different float: they are the negative control
    // for the rounding model (9 of the 24 separate the two models).
    const VEC = [
      [1.0, 0.0, 0.0], [0.1, 0.3333333333, 1e-7], [3e5, 0.7, 1234.567], [1.0000001, 0.9999999, 123456.79],
      [-0.1, 0.3, 7.0], [1e-7, 1e7, 0.1], [1e7, 1e-7, -0.1], [16777215.0, 1.0, 0.5],
      [1.0000001, 16777215.0, 3.0], [0.2, 0.30000001192092896, 0.7], [-0.8, 0.6, -0.9], [-2.5e-5, 1.5e6, 123.456],
      [0.0077, -33000.0, 1e-7], [2.0, 3.0, 6.0], [-1.0, -1.0, -1.0], [1e-20, 1e-20, 3e-20],
      [-2.5653823, 0.215292, -0.8058665], [-0.3981259, -2.5808675, -2.4557219],
      [-1.6605662, 0.7645993, 2.6862537], [-2.7205039, 2.1508108, -1.2623443],
      [1.0823998, -0.4344462, -1.115117], [1.0092951, 1.5874252, 0.4381556],
      [0.8827731, 2.9585756, 1.9315487], [-2.8646224, -0.2298283, -1.9917097],
    ];
    for (let i = 0; i < VEC.length; i++) {
      const q = blk.add(p); p += 12;
      for (let k = 0; k < 3; k++) q.add(k * 4).writeFloat(VEC[i][k]);
      out['v' + i] = q;
    }

    // 10 two-float output points (Terrain::hermitePoint), seeded non-zero so a missing store would show
    for (let i = 0; i < 10; i++) {
      const q = blk.add(p); p += 8;
      q.writeFloat(-1234.5 - i); q.add(4).writeFloat(9876.5 + i);
      out['o' + i] = q;
    }

    // 12 two-float control points
    const CTRL = [[0.0, 0.0], [1.0, 0.0], [0.0, 1.0], [1.0, 1.0], [0.1, 0.3333333333], [3e5, -1234.567],
                  [1.0000001, 0.9999999], [16777215.0, 0.5], [-7.25, 1e-7], [1e7, -1e7],
                  [0.30000001192092896, 0.2], [-2.5e-5, 1.5e6]];
    for (let i = 0; i < CTRL.length; i++) {
      const q = blk.add(p); p += 8;
      q.writeFloat(CTRL[i][0]); q.add(4).writeFloat(CTRL[i][1]);
      out['c' + i] = q;
    }

    // Our own vertex array for Terrain::faceNormal, placed so that (address - (Terrain.dll+0xb28c8)) is a
    // multiple of 12; the indices stored in the faces are that distance divided by 12, taken as 32-bit values,
    // which is exactly what `imul idx, 0xc` + the indexed load reconstruct.
    const vbase = Process.getModuleByName('Terrain.dll').base.add(0xb28c8);
    const NV = 24;
    const raw = Memory.alloc(NV * 12 + 16);
    const d0 = raw.sub(vbase).toUInt32();
    const pad = (12 - (d0 % 12)) % 12;
    const delta = (d0 + pad) >>> 0;
    if (delta % 12 !== 0) throw new Error('c3as_math: vertex array offset ' + delta + ' is not a multiple of 12');
    const verts = raw.add(pad);
    const index0 = delta / 12;
    const VX = [];
    for (let i = 0; i < NV; i++) {
      // deterministic, with inexact-in-float magnitudes and a few exact ones
      const x = [0.0, 1.0, 0.1, 3e5, -0.3333333333, 1.0000001, 16777215.0, 1e-7][i % 8] * (1 + (i % 3));
      const y = [0.0, 0.5, -0.7, 1e7, 0.30000001192092896, -2.5e-5, 123.456, 1e-20][(i + 3) % 8] * (1 + (i % 5));
      const z = [1.0, -1.0, 0.0077, -33000.0, 2.0, 1e-7, 0.9999999, 6.0][(i + 5) % 8] * (1 + (i % 7));
      const q = verts.add(i * 12);
      q.writeFloat(x); q.add(4).writeFloat(y); q.add(8).writeFloat(z);
      VX.push(index0 + i);
    }
    out.verts = verts;

    // 16 face records of 0x40 bytes: three vertex indices and a normal at +0x2c seeded non-zero
    const TRI = [[0, 1, 2], [2, 1, 0], [3, 4, 5], [6, 7, 8], [9, 10, 11], [12, 13, 14], [15, 16, 17],
                 [18, 19, 20], [21, 22, 23], [0, 5, 10], [1, 7, 13], [2, 9, 16], [4, 11, 18], [3, 12, 21],
                 [8, 14, 20], [0, 1, 1]];
    const faceBase = Memory.alloc(16 * 0x40);
    const ffill = [];
    for (let i = 0; i < 16 * 0x40; i++) ffill.push((i * 7 + 0x5b) & 0xff);
    faceBase.writeByteArray(ffill);
    for (let i = 0; i < 16; i++) {
      const f = faceBase.add(i * 0x40);
      f.writeS32(VX[TRI[i][0]] | 0);
      f.add(4).writeS32(VX[TRI[i][1]] | 0);
      f.add(8).writeS32(VX[TRI[i][2]] | 0);
      f.add(0x2c).writeFloat(-1.5); f.add(0x30).writeFloat(2.5); f.add(0x34).writeFloat(-3.5);
      out['f' + i] = f;
    }
    out.faces = faceBase;
    return out;
  },

  c3as_nodes() {
    const NN = 16, NI = 20;
    const nodes = Memory.alloc(NN * 12);
    for (let i = 0; i < NN; i++) {
      const n = nodes.add(i * 12);
      n.writePointer(nodes.add(((i + 1) % NN) * 12));        // next
      n.add(4).writePointer(nodes.add(((i + NN - 1) % NN) * 12));  // prev
      n.add(8).writeS32(0x7e000 + i);                        // the stored Tile pointer slot
    }
    // 20 iterators; iterator k holds node k % 16, so iterators 16..19 repeat nodes 0..3 and
    // const_iterator::operator== has both an equal and an unequal pair for the same node
    const iters = Memory.alloc(NI * 4);
    const out = { nodes, iters, obj: nodes };
    for (let i = 0; i < NN; i++) out['n' + i] = nodes.add(i * 12);
    for (let i = 0; i < NI; i++) {
      iters.add(i * 4).writePointer(nodes.add((i % NN) * 12));
      out['i' + i] = iters.add(i * 4);
    }
    // one iterator holding NULL, for _Mynode and for an operator== pair that compares two NULL nodes
    const nullIters = Memory.alloc(8);
    nullIters.writePointer(ptr(0));
    nullIters.add(4).writePointer(ptr(0));
    out.z0 = nullIters;
    out.z1 = nullIters.add(4);
    return out;
  },
});
