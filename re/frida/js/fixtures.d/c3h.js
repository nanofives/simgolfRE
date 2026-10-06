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

  // c3i fix-up (2026-10-06): the three Surf473 methods only reached their null guards with c3h_surface (m_4 == 0), so
  // the A/B never ran the format/fill/blit path and they were held back. c3i_surface gives the surface a non-null
  // DirectDraw object (fake) with ONE fake vtable so the path runs, without any live game surface or DirectDraw object:
  //   * fakeSurf (Surf473): +4 points at fakeDD;  znull (Surf473): +4 == 0, for the "return 7" guard.
  //   * fakeDD: +0 points at vtbl. vtbl has two NativeCallback slots:
  //       - +0x40 fill/blit: called thiscall by Surface_fillRegion (this->m_4 vtable+0x40, args sm4,&rect,&rect) and
  //         Surface_blit3 (args sm4,&r1,&r2). It records this(tm4), sm4 and the two rectangles into `rec` and returns
  //         0x2a, so the function returns 0x2a (distinct from 0x10 and 7) and `rec` changes.
  //       - +0xe4 format: called thiscall by Surface_blit (s->m_4 vtable+0xe4). It records the surface into `rec` and
  //         returns a pointer whose first dword is 16 (not 8), so Surface_blit takes the depth-mismatch branch and
  //         returns 0 (the 8-bit blitTo path would need a real DirectDraw surface and is left to the 100% body match).
  //   * rec (0x40 bytes) is the state region; both A/B arms call the same callback with the same inputs, so the
  //     recorded bytes match and change only on the vectors that reach a virtual slot.
  c3i_surface() {
    const vtbl = __keepAlloc(0x100);
    for (let i = 0; i < 0x100; i += 4) vtbl.add(i).writeS32(0);
    const fakeDD = __keepAlloc(0x10);
    for (let i = 0; i < 0x10; i += 4) fakeDD.add(i).writeS32(0);
    fakeDD.writePointer(vtbl);                       // fakeDD->vtable
    const fakeSurf = __keepAlloc(0x28);
    for (let i = 0; i < 0x28; i += 4) fakeSurf.add(i).writeS32(0);
    fakeSurf.add(4).writePointer(fakeDD);            // fakeSurf->m_4 = fakeDD
    const znull = __keepAlloc(0x28);
    for (let i = 0; i < 0x28; i += 4) znull.add(i).writeS32(0);  // znull->m_4 = 0
    const rec = __keepAlloc(0x40);
    for (let i = 0; i < 0x40; i += 4) rec.add(i).writeS32(0);
    const fmt = __keepAlloc(4);
    fmt.writeS32(16);                                // pixel depth != 8 -> Surface_blit returns 0

    // fill/blit at vtable+0x40: thiscall (tm4, sm4, r1*, r2*). Record contents (identical across both arms).
    const fillCb = new NativeCallback(function (tm4, sm4, r1, r2) {
      rec.writePointer(tm4);
      rec.add(4).writePointer(sm4);
      for (let i = 0; i < 4; i++) rec.add(8 + i * 4).writeS32(r1.add(i * 4).readS32());
      for (let i = 0; i < 4; i++) rec.add(0x18 + i * 4).writeS32(r2.add(i * 4).readS32());
      return 0x2a;
    }, 'int', ['pointer', 'pointer', 'pointer', 'pointer'], 'thiscall');

    // format at vtable+0xe4: thiscall (ddSurface) -> pointer to {depth}. Record the surface.
    const formatCb = new NativeCallback(function (dd) {
      rec.add(0x30).writePointer(dd);
      rec.add(0x34).writeS32(0xf0);
      return fmt;
    }, 'pointer', ['pointer'], 'thiscall');

    vtbl.add(0x40).writePointer(fillCb);
    vtbl.add(0xe4).writePointer(formatCb);
    globalThis.__diffKeepAlive.push(fillCb, formatCb);   // NativeCallbacks are not rewritten to __keepAlloc
    return { obj: fakeSurf, znull: znull, rec: rec };
  },
});
