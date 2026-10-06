// Fixtures for C3 batch c3h (render leaves, writers, table lookups). Each seeds only our own synthetic values
// (no game text) so every tested branch is reachable. Names are prefixed c3h_. The surface fixture builds a fake
// Surf473 whose DirectDraw surface pointer (m_4 at +4) is 0, so the A/B exercises the null guards and never calls
// into DirectDraw.
Object.assign(globalThis.DIFF_FIXTURES, {
  // Table483::find 0x004833f0 scans 5 entries (stride 0x10) whose key is at entry+8. c3h_table has a free slot
  // (-1) at index 4, so keys 10..40 hit the key-match branch (indices 0..3) and anything else returns the free
  // slot (4). The other fields are left 0 (unread by find).
  c3h_table() {
    const p = __keepAlloc(5 * 0x10);
    const keys = [10, 20, 30, 40, -1];
    for (let i = 0; i < 5; i++) {
      p.add(i * 0x10).writeS32(0);          // a
      p.add(i * 0x10 + 4).writeS32(0);      // b
      p.add(i * 0x10 + 8).writeS32(keys[i]); // key
      p.add(i * 0x10 + 0xc).writeS32(0);    // c
    }
    return { obj: p };
  },

  // c3h_table_full has no free slot (-1), so keys 10..50 hit indices 0..4 and an absent key reaches the
  // "none found" return of 5.
  c3h_table_full() {
    const p = __keepAlloc(5 * 0x10);
    const keys = [10, 20, 30, 40, 50];
    for (let i = 0; i < 5; i++) {
      p.add(i * 0x10 + 8).writeS32(keys[i]);
    }
    return { obj: p };
  },

  // tileToScreen 0x0042f940 caches are the short grids g_tileSX 0x0055eb40 and g_tileSY 0x0055fec8 (cell = x*50 + y,
  // stride 2). Seed a mix: cached-valid cells (nonzero, not -99) return the cached position; -99 cells return 0
  // without recomputing; 0 cells are uncached and go through worldToScreen (0x0042fb90, called on both arms with
  // identical global state at the menu). sx/sy are the out-param buffers (also state regions).
  c3h_tiles() {
    const sxg = ptr('0x0055eb40'), syg = ptr('0x0055fec8');
    for (let x = 0; x < 50; x++)
      for (let y = 0; y < 50; y++) {
        const cell = x * 50 + y;
        const m = (x + y) % 4;
        if (m === 0) { sxg.add(cell * 2).writeS16(100 + x); syg.add(cell * 2).writeS16(50 + y); }  // cached valid
        else if (m === 1) { sxg.add(cell * 2).writeS16(-99); }                                     // off-screen
        else { sxg.add(cell * 2).writeS16(0); syg.add(cell * 2).writeS16(0); }                      // uncached
      }
    return { obj: sxg, sx: Memory.alloc(4), sy: Memory.alloc(4) };
  },

  // Surface_blit / Surface_fillRegion / Surface_blit3: a fake Surf473 with m_0 and m_4 (+4) both 0. Passed as
  // `this` and as the surface argument; with m_4 == 0 the only reachable returns are 0x10 (null argument) and 7.
  c3h_surface() {
    const p = __keepAlloc(0x28);
    for (let i = 0; i < 0x28; i += 4) p.add(i).writeS32(0);
    return { obj: p };
  },
});
