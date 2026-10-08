// Batch c3aw fixtures (Terrain.dll). Every buffer here is our own allocation: no list object, node,
// iterator or destination of the running game is handed to either arm, and no module global is written
// (none of the twelve bodies reads one).
//
// c3aw_lists   one 0x400 block carved into
//                +0x000  16 list nodes of 12 bytes ({next, prev, value}) linked into a ring, so
//                        _Acc::_Next 0x1000b600 and the operator++ family always land on a mapped node
//                        and never on NULL ($n0..$n15);
//                +0x0c0  16 list objects of 12 bytes ({allocator, _Head at +4, _Size at +8}) whose head
//                        pointers walk the ring and whose counts include 0 three times and the extremes
//                        of the signed dword, so size 0x1000b430 returns many values and empty
//                        0x1000b040 takes both sides of its `neg; sbb; inc` ($L0..$L15);
//                +0x180  24 one-dword iterators ($i0..$i23): iterator k holds node k % 16, so $i16..$i19
//                        and $i22, $i23 repeat nodes and const_iterator::operator!= 0x1000b3d0 has both
//                        equal and unequal pairs for the same node; $i20 and $i21 are overwritten with
//                        NULL afterwards, which operator* 0x1000b2d0 and operator== accept (neither
//                        dereferences the node) and which the operator++ keys deliberately never use
//                        (0x1000b7ce dereferences it);
//                +0x1e0  16 four-byte slots for the iterators begin/end/operator++(int) return by value
//                        ($r0..$r15), seeded with 0xdead0000 + k so a missing store would show;
//                +0x220  16 destinations for allocator::construct ($d0..$d15), seeded 0xcafe0000 + k;
//                +0x260  16 sources for allocator::construct ($s0..$s15), seeded 0x11110000 + k * 7.
//              `obj` is the block base, so every pointer result is reported as obj+0x... and the two arms
//              are compared on the same scale. The whole 0x400 block is the state region of every key
//              that writes, so diff_hook restores it between the arms and after each vector.
Object.assign(globalThis.DIFF_FIXTURES, {
  c3aw_lists() {
    const SIZE = 0x400;
    const blk = Memory.alloc(SIZE);
    const zero = [];
    for (let i = 0; i < SIZE; i++) zero.push(0);
    blk.writeByteArray(zero);
    const out = { blk, obj: blk };

    // 16 nodes of 12 bytes in a ring: next, prev, stored Tile pointer slot
    const NN = 16;
    for (let i = 0; i < NN; i++) {
      const n = blk.add(i * 12);
      n.writePointer(blk.add(((i + 1) % NN) * 12));
      n.add(4).writePointer(blk.add(((i + NN - 1) % NN) * 12));
      n.add(8).writeS32(0x7e000 + i * 3);
      out['n' + i] = n;
    }

    // 16 list objects: +0 the allocator member (never read), +4 the head node, +8 the count
    const COUNT = [0, 1, 2, 3, 7, 16, 255, 1000, 0x7fffffff, -1, -1000, -0x80000000, 0, 5, 0, 42];
    for (let i = 0; i < 16; i++) {
      const L = blk.add(0xc0 + i * 12);
      L.writeS32(0);
      L.add(4).writePointer(blk.add(((i * 5 + 1) % NN) * 12));
      L.add(8).writeS32(COUNT[i] | 0);
      out['L' + i] = L;
    }

    // 24 iterators, then two of them set to NULL
    for (let i = 0; i < 24; i++) {
      const it = blk.add(0x180 + i * 4);
      it.writePointer(blk.add((i % NN) * 12));
      out['i' + i] = it;
    }
    out.i20.writePointer(ptr(0));
    out.i21.writePointer(ptr(0));

    // 16 return slots, 16 construct destinations, 16 construct sources
    for (let i = 0; i < 16; i++) {
      const r = blk.add(0x1e0 + i * 4);
      r.writeU32(0xdead0000 + i);
      out['r' + i] = r;
      const d = blk.add(0x220 + i * 4);
      d.writeU32(0xcafe0000 + i);
      out['d' + i] = d;
      const s = blk.add(0x260 + i * 4);
      s.writeU32(0x11110000 + i * 7);
      out['s' + i] = s;
    }
    return out;
  },

  // c3aw_faces  16 Faces buffers of 0x200 bytes ($F0..$F15) for Faces::build 0x10002060. 0x200 is more than
  //             the 0x1c4 bytes the body can touch: eight records of 0x38 bytes plus the `+0x38` store of the
  //             eighth record, which lands at 0x1c0 (the stride is 0x38 but the store is at +0x38, so each
  //             record zeroes the next record's offset 0), and the twelve tail copies, whose highest address
  //             is +0x13c. The buffers are filled with a deterministic non-zero pattern so that any field the
  //             body does NOT write keeps a value the state hash would notice if one arm wrote it.
  c3aw_faces() {
    const N = 16, STRIDE = 0x200;
    const blk = Memory.alloc(N * STRIDE);
    const fill = [];
    for (let i = 0; i < N * STRIDE; i++) fill.push((i * 37 + 0x5d) & 0xff);
    blk.writeByteArray(fill);
    const out = { blk, obj: blk };
    for (let i = 0; i < N; i++) out['F' + i] = blk.add(i * STRIDE);
    return out;
  },
});
