// Fixtures for C3 batch c3r (reverseFindChar, swapInts, appendNewline, listIterValue, listIterNext, readListNode,
// HashTable::bucketEmpty / bucketSet, overlayCharAt, jpeg_abort, jpeg_destroy). Only our own synthetic values (no game
// text). Names are prefixed c3r_. Allocations go through __keepAlloc (diff_hook rewrites Memory.alloc); NativeCallbacks
// are pushed onto __diffKeepAlive by hand.
Object.assign(globalThis.DIFF_FIXTURES, {
  // reverseFindChar 0x004935b0: one 64-byte buffer; o<i> = buffer + i. Bytes: 'a'..'g' cycling ((i % 7) + 0x61), with
  // 'Z' at 0, '/' at 3, 17 and 30, '.' at 9, 'Q' at 40 and NUL at 63. 'z' does not occur.
  c3r_rfind() {
    const b = Memory.alloc(0x40);
    for (let i = 0; i < 0x40; i++) b.add(i).writeU8(0x61 + (i % 7));
    b.writeU8(0x5a);
    for (const i of [3, 17, 30]) b.add(i).writeU8(0x2f);
    b.add(9).writeU8(0x2e);
    b.add(40).writeU8(0x51);
    b.add(63).writeU8(0);
    const fx = { obj: b };
    for (let i = 0; i < 0x40; i++) fx['o' + i] = b.add(i);
    return fx;
  },

  // swapInts 0x00493580: eight dword cells c0..c7 in one 0x20-byte block (the state region): 5, -7, 0x7fffffff,
  // 0x80000000, 0, 123456, 5, 5 (c0, c6 and c7 equal, so some swaps leave the bytes as they were).
  c3r_swap() {
    const b = Memory.alloc(0x20);
    const v = [5, -7, 0x7fffffff, 0x80000000 | 0, 0, 123456, 5, 5];
    const fx = { obj: b };
    for (let i = 0; i < 8; i++) { b.add(i * 4).writeS32(v[i]); fx['c' + i] = b.add(i * 4); }
    return fx;
  },

  // appendNewline 0x004925f0: twelve 0x40-byte buffers s0..s11 in one block (the state region), each holding a string of
  // 'a'..'z' of length 0, 1, 2, 3, 5, 8, 13, 21, 34, 55, 62, 4 and its NUL; every other byte is 0xee, so the '\n' and the
  // new NUL written after the string show in the state.
  c3r_nl() {
    const lens = [0, 1, 2, 3, 5, 8, 13, 21, 34, 55, 62, 4];
    const b = Memory.alloc(12 * 0x40);
    for (let i = 0; i < 12 * 0x40; i++) b.add(i).writeU8(0xee);
    const fx = { obj: b };
    for (let k = 0; k < 12; k++) {
      const s = b.add(k * 0x40);
      for (let j = 0; j < lens[k]; j++) s.add(j).writeU8(0x61 + ((j + k) % 26));
      s.add(lens[k]).writeU8(0);
      fx['s' + k] = s;
    }
    return fx;
  },

  // listIterValue 0x00402160 / listIterNext 0x00402130: a ring of six 0x10-byte nodes n0..n5 ($nodes; +8 value, +0xc
  // next = n((k+1) % 6)); values 0x1000 + k*0x111 except n4 = 0. Twelve iterators i0..i11 (0x18 bytes each, in $obj):
  // +8 list pointer, +0xc current node, +0x10 count, +0x14 index. i0, i4, i8 have a NULL list (and a current pointer
  // 0xdead0000 that must not be read). (count, index) for the others: i1 (6, 0), i2 (6, 4), i3 (6, 5) -> reset,
  // i5 (1, 0) -> reset, i6 (0, -1) -> reset, i7 (3, 7), i9 (-2, -3) -> reset, i10 (0x7fffffff, 0x7ffffffe) -> reset,
  // i11 (5, 0x7fffffff) -> wraps to 0x80000000, no reset.
  c3r_iter() {
    const nodes = Memory.alloc(6 * 0x10);
    for (let k = 0; k < 6; k++) {
      const n = nodes.add(k * 0x10);
      n.writeU32(0xa0a00000 + k); n.add(4).writeU32(0xb0b00000 + k);
      n.add(8).writeU32(k === 4 ? 0 : 0x1000 + k * 0x111);
      n.add(0xc).writePointer(nodes.add(((k + 1) % 6) * 0x10));
    }
    const ci = [[0, 0], [6, 0], [6, 4], [6, 5], [0, 0], [1, 0], [0, -1], [3, 7], [0, 0], [-2, -3],
                [0x7fffffff, 0x7ffffffe], [5, 0x7fffffff]];
    const b = Memory.alloc(12 * 0x18);
    const fx = { obj: b, nodes: nodes };
    for (let i = 0; i < 12; i++) {
      const it = b.add(i * 0x18);
      const nul = i % 4 === 0;
      it.writeU32(0xc0c00000 + i); it.add(4).writeU32(0);
      it.add(8).writePointer(nul ? ptr(0) : nodes);
      it.add(0xc).writePointer(nul ? ptr('0xdead0000') : nodes.add((i % 6) * 0x10));
      it.add(0x10).writeS32(ci[i][0]); it.add(0x14).writeS32(ci[i][1]);
      fx['i' + i] = it;
    }
    return fx;
  },

  // readListNode 0x004a4ea0: holders h0..h3 (8 bytes: +0 garbage, +4 node pointer; h0's is NULL), three nodes of three
  // dwords (0x11110000 + k, 0x22220000 + k, 0x33330000 + k). Out cells f and s ($cells, 8 bytes, the state region)
  // start as 0xf0f0f0f0 and 0x5a5a5a5a.
  c3r_node() {
    const cells = Memory.alloc(8);
    cells.writeU32(0xf0f0f0f0); cells.add(4).writeU32(0x5a5a5a5a);
    const nodes = Memory.alloc(3 * 0xc);
    for (let k = 0; k < 3; k++)
      for (let j = 0; j < 3; j++) nodes.add(k * 0xc + j * 4).writeU32(((j + 1) * 0x11110000 + k + 1) >>> 0);
    const h = Memory.alloc(4 * 8);
    const fx = { obj: h, cells: cells, f: cells, s: cells.add(4) };
    for (let i = 0; i < 4; i++) {
      h.add(i * 8).writeU32(0xd0d00000 + i);
      h.add(i * 8 + 4).writePointer(i === 0 ? ptr(0) : nodes.add((i - 1) * 0xc));
      fx['h' + i] = h.add(i * 8);
    }
    return fx;
  },

  // HashTable::bucketEmpty 0x00487770 / bucketSet 0x004877a0: one 0x200-byte table t (garbage (i*29 + 3) & 0xff), with
  // bucket i's byte +0x24 + i*0x1c = [0, 1, 0x80, 0xff, 0][i % 5] and byte +0x25 + i*0x1c = 0 when i % 3 == 1 or i == 0,
  // else (i * 17) & 0xff.
  c3r_buckets() {
    const t = Memory.alloc(0x200);
    for (let i = 0; i < 0x200; i++) t.add(i).writeU8((i * 29 + 3) & 0xff);
    for (let i = 0; i < 16; i++) {
      t.add(0x24 + i * 0x1c).writeU8([0, 1, 0x80, 0xff, 0][i % 5]);
      t.add(0x25 + i * 0x1c).writeU8(i % 3 === 1 || i === 0 ? 0 : (i * 17) & 0xff);
    }
    return { obj: t, t: t };
  },

  // overlayCharAt 0x00456bb0: tile types 0x005722e8 as tile_types_pattern ((x*7 + y*3) & 0x1f), then cell (0,0) = 0xff
  // (-1) and (1,1) = 0xfe (-2); type byte +2 of the 0x30-byte type entries (0x00578372 + t*0x30) = [0, 1, -1, 0x7f,
  // -128, 2, 0, -3][t % 8] for t = 0..31. The sign extension: type bytes 0xff / 0xfe read entries -1 / -2
  // (0x00578342 / 0x00578312), seeded 1 and 0x7f; a zero-extending read would reach entries 255 / 254 (0x0057b342 /
  // 0x0057b312), which are 0 at the main menu (probed 2026-10-07) and are not written. So cells (0,0) and (1,1) return
  // 3 only through a sign-extending read.
  c3r_overlay() {
    globalThis.DIFF_FIXTURES.tile_types_pattern();
    const t = ptr('0x005722e8');
    for (let k = 0; k < 32; k++) ptr('0x00578372').add(k * 0x30).writeS8([0, 1, -1, 0x7f, -128, 2, 0, -3][k % 8]);
    ptr('0x00578342').writeS8(1);
    ptr('0x00578312').writeS8(0x7f);
    t.writeU8(0xff);
    t.add(51).writeU8(0xfe);
    return { obj: t };
  },

  // jpeg_abort 0x004afa60 / jpeg_destroy 0x004afa90: two fake memory managers memA / memB (0x30 bytes) whose +0x24
  // free_pool(cinfo, pool) and +0x28 self_destruct(cinfo) are cdecl NativeCallbacks logging into rec (0x10 bytes):
  // rec+0 += 1 (free_pool) or 0x100 (self_destruct), rec+4 = tag (1 / 2 for A / B, +0x10 for self_destruct), rec+8 =
  // pool argument (free_pool only), rec+0xc = rec+0xc * 31 + the cinfo's id dword at +0x18. 24 cinfo objects (0x20
  // bytes, garbage elsewhere) in $obj: a0..a11 with memory manager memA (even k) / memB (odd k) and +0xc
  // is_decompressor = [0, 1, -1, 0x100, 0x80000000, 0, 2, 0, 0xff, 0, 1, 0][k]; d0..d11 with NULL (k % 3 == 0), memA
  // (k % 3 == 1) or memB (k % 3 == 2). Every +0x10 starts as 0x1234 + index.
  c3r_jpeg() {
    const rec = Memory.alloc(0x10);
    for (let i = 0; i < 0x10; i += 4) rec.add(i).writeU32(0);
    const mkMem = (tag) => {
      const m = Memory.alloc(0x30);
      for (let i = 0; i < 0x30; i += 4) m.add(i).writeU32(0);
      const fp = new NativeCallback(function (cinfo, pool) {
        rec.writeU32(rec.readU32() + 1);
        rec.add(4).writeU32(tag);
        rec.add(8).writeS32(pool);
        rec.add(0xc).writeU32((Math.imul(rec.add(0xc).readU32(), 31) + cinfo.add(0x18).readU32()) >>> 0);
      }, 'void', ['pointer', 'int'], 'default');
      const sd = new NativeCallback(function (cinfo) {
        rec.writeU32(rec.readU32() + 0x100);
        rec.add(4).writeU32(tag + 0x10);
        rec.add(0xc).writeU32((Math.imul(rec.add(0xc).readU32(), 31) + cinfo.add(0x18).readU32()) >>> 0);
      }, 'void', ['pointer'], 'default');
      globalThis.__diffKeepAlive.push(fp, sd);
      m.add(0x24).writePointer(fp);
      m.add(0x28).writePointer(sd);
      return m;
    };
    const memA = mkMem(1), memB = mkMem(2);
    const dec = [0, 1, -1, 0x100, 0x80000000 | 0, 0, 2, 0, 0xff, 0, 1, 0];
    const b = Memory.alloc(24 * 0x20);
    const fx = { obj: b, rec: rec };
    for (let i = 0; i < 24; i++) {
      const c = b.add(i * 0x20);
      for (let j = 0; j < 0x20; j += 4) c.add(j).writeU32((0xe0e00000 + i * 0x100 + j) >>> 0);
      const k = i % 12;
      let mem;
      if (i < 12) mem = k % 2 ? memB : memA;
      else mem = [ptr(0), memA, memB][k % 3];
      c.add(4).writePointer(mem);
      c.add(0xc).writeS32(i < 12 ? dec[k] : 0);
      c.add(0x10).writeU32(0x1234 + i);
      c.add(0x18).writeU32(i + 1);
      fx[(i < 12 ? 'a' : 'd') + k] = c;
    }
    return fx;
  },
});
