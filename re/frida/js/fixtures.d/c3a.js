// Fixtures for C3 batch c3a. Each seeds the tables the function (and its original callees) read so every branch is
// reachable; only our own synthetic values are written (no game text). Names are prefixed c3a_. Base fixtures
// (tile_types_pattern, golf_tables, terrain_slopes, _blend) are reused where they already seed a needed table.
Object.assign(globalThis.DIFF_FIXTURES, {
  // tileByte 0x004492f0 reads the byte table 0x0056988c (row stride 50); tileFlag20 0x004493b0 reads bit 0x20 of the
  // words at 0x0053caf0 (cell*2); typeAtPos 0x0040bfa0 reads tile types 0x005722e8 and calls tileBlocked.
  c3a_tiles() {
    globalThis.DIFF_FIXTURES.tile_types_pattern();           // 0x005722e8 (tileBlocked / typeAtPos)
    const b = ptr('0x0056988c'), fl = ptr('0x0053caf0');
    for (let x = 0; x < 50; x++)
      for (let y = 0; y < 50; y++) {
        b.add(x * 50 + y).writeU8((x * 7 + y * 3) & 0xff);
        fl.add((x * 50 + y) * 2).writeU16(((x * 13 + y * 5) & 0x1fff) | ((x + y) % 3 ? 0x20 : 0));
      }
    return { obj: ptr('0x005722e8') };
  },

  // thoughtFlag 0x004675d0 / thoughtBalance 0x0045c420: 5 thought words per golfer at 0x00579540 + g*0x100. The top
  // two bits of each word cycle 0x0000 / 0x4000 / 0x8000 / 0xc000 so both branches of both functions are hit.
  c3a_golfer_flags() {
    const base = ptr('0x00579540');
    for (let g = 0; g < 32; g++)
      for (let j = 0; j < 5; j++)
        base.add(g * 0x100 + j * 2).writeU16((((g + j) % 4) << 14) | ((g * 3 + j) & 0x13ff));
    return { obj: base };
  },

  // Shared terrain seed for sampleHeight / tileQuery449310 (cornerHeights) / raiseFromNeighbours / relaxEdges. It
  // layers the base fixtures that already seed the heightBlend cluster, then adds the tile-byte and level tables.
  _c3a_terrain() {
    globalThis.DIFF_FIXTURES.golf_tables();      // tile types 0x005722e8, wall masks 0x005619a0, wall heights, golfers
    globalThis.DIFF_FIXTURES.terrain_slopes();   // cornerHeights table 0x0051b770, type flags 0x0057837c, clears 0x0059e7b8 bit0
    globalThis.DIFF_FIXTURES._blend(0, 2, 1);    // height grid 0x00838c1c, course rec, 0x005a4998, 0x00822c88=1, 0x00834170=0
    const tb = ptr('0x0056988c'), lv = ptr('0x00543018');
    for (let x = 0; x < 50; x++)
      for (let y = 0; y < 50; y++) {
        tb.add(x * 50 + y).writeU8((x * 5 + y * 7) & 3);          // tile byte (raiseFromNeighbours, sameByte != 0)
        lv.add(x * 50 + y).writeS8(((x * 5 + y * 7) % 23) - 11);  // signed per-corner level 0x00543018
      }
  },
  c3a_terrain_read() { globalThis.DIFF_FIXTURES._c3a_terrain(); return { obj: ptr('0x005722e8') }; },

  // cornerRange 0x0042f4b0 writes a max and a min through two out-pointers; it is driven as heightBlend over
  // (x,y),(x+1,y),(x+1,y-1),(x,y-1). The out buffers are the state regions the A/B compares and restores.
  c3a_cornerrange() {
    globalThis.DIFF_FIXTURES._c3a_terrain();
    return { obj: ptr('0x005722e8'), mx: Memory.alloc(4), mn: Memory.alloc(4) };
  },

  // freeAtTile 0x00402930: the 256 records at 0x005736b0 (stride 0x24; x +0, y +4, id +8). Positions vary across many
  // tiles so some rows match a given (tx, ty) and some do not; every record is live (id != -1).
  c3a_records() {
    const base = ptr('0x005736b0');
    for (let i = 0; i < 256; i++) {
      const r = base.add(i * 0x24);
      // x -> tile i % 50; records 250..255 get negative x (tile -1..-6 through the arithmetic >> 10) with a
      // nonzero low part, so a logical shift or a division would select a different tile
      r.writeS32(i >= 250 ? -((i - 249) << 10) + 5 : ((i % 50)) << 10);
      r.add(4).writeS32(((i * 3) % 50) << 10); // y -> tile (i*3) % 50
      r.add(8).writeS32(i);                   // id (live)
    }
    return { obj: base };
  },

  // nearestPlaced 0x0040ddb0: the 256 placed-object records at 0x0058bcb8 (stride 0x10; type word +0, tx +2, ty +4,
  // flags byte +7). Records 0..39 carry types 0..7 (so vectors with those types match several); the rest are empty
  // (type -1). Half the matching records set flag 0x40 so the type>=6 early-return path is reachable. The objDef size
  // table at 0x004c26c0 is left as the game's own constants (size feeds arithmetic, not a branch).
  c3a_placed() {
    const base = ptr('0x0058bcb8');
    for (let i = 0; i < 256; i++) {
      const o = base.add(i * 0x10);
      o.writeS16(i < 40 ? (i % 8) : -1);
      o.add(2).writeS16(i % 50);
      o.add(4).writeS16((i * 3) % 50);
      o.add(7).writeU8(i % 2 ? 0x40 : 0);
    }
    return { obj: base };
  },

  // shotPower 0x00422430: the 10-slot cache (dist 0x005a47b8, lie 0x005685c8, value 0x0053fd20) seeded to dist -1 so
  // positive-distance vectors miss and recompute; write index 0x005a9ce0 = 0. puttReach 0x004223c0 reads the shift
  // byte 0x005783a1, set to 3 so the putt branch terminates.
  c3a_shot() {
    for (let i = 0; i < 10; i++) {
      ptr('0x005a47b8').add(i * 4).writeS32(-1);
      ptr('0x005685c8').add(i * 4).writeS32(-1);
      ptr('0x0053fd20').add(i * 4).writeS32(0);
    }
    ptr('0x005a9ce0').writeS32(0);
    const sh = ptr('0x005783a1');
    Memory.protect(sh, 1, 'rw-');
    sh.writeS8(3);
    return { obj: ptr('0x005a47b8') };
  },
});

// c3i fix-up (2026-10-06): a dedicated fixture for raiseFromNeighbours 0x0042f6e0 so the raise actually fires. The
// shared c3a_terrain_read left neighbours with mismatched tile types and a level grid that never let a neighbour win,
// so every vector returned 0 with no state change (held back as single-valued). c3i_raise seeds:
//   * tile types 0x005722e8 uniformly to 2 (non-blocked: tileBlocked 0x0040bf60 only blocks type 0x14), so every
//     orthogonal neighbour passes the type gate;
//   * tile bytes 0x0056988c to the column parity x&1, so when sameByte != 0 only the vertical neighbours (same column,
//     same byte) qualify and the horizontal ones (opposite parity) are excluded -- the sameByte branch then changes
//     the result for cells whose tallest neighbour is horizontal;
//   * the signed level grid 0x00543018 to a non-monotonic pattern, so most interior cells have a taller qualifying
//     neighbour (raise -> return 1, level byte written) while the local maxima do not (return 0, no write).
// Only our own synthetic values are written; the three tables are the same ones the function and tileBlocked read.
Object.assign(globalThis.DIFF_FIXTURES, {
  c3i_raise() {
    const ty = ptr('0x005722e8'), by = ptr('0x0056988c'), lv = ptr('0x00543018');
    for (let x = 0; x < 50; x++)
      for (let y = 0; y < 50; y++) {
        const cell = x * 50 + y;
        ty.add(cell).writeU8(2);                               // uniform non-blocked type
        by.add(cell).writeU8(x & 1);                           // column parity -> sameByte keeps vertical neighbours
        lv.add(cell).writeS8(((x * 17 + y * 13) % 31) - 15);   // non-monotonic signed levels
      }
    return { obj: lv };
  },
});
