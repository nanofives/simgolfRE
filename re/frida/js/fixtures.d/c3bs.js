// Fixtures for C3 batch c3bs (round 17, golf_clean.exe): the two IJG libjpeg 6a master methods with no call
// xref (pass_startup, finish_pass), the shading spreader pair (spreadShade, spreadAllShade), the RLE surface
// blitter (Surface::blitRLE), the object spawner (spawnObject) and the roster name builder (pickGolferName).
// Only our own synthetic values (no game text, no live game object). Names are prefixed c3bs_.
//
// Allocations go through Memory.alloc (diff_hook rewrites it to __keepAlloc so blocks referenced only from a
// field survive the run); NativeCallbacks are pushed onto __diffKeepAlive by hand.
//
// Every callback is a pure function of its arguments or of memory diff_hook snapshots and restores between the
// two arms, so the second arm sees exactly the world the first one saw.
//
// libjpeg structures (as c3bj / c3bo): cinfo+0xb0 = optimize_coding, cinfo+0x144 = master, cinfo+0x154 =
// marker, cinfo+0x164 = entropy. my_comp_master is 0x24 bytes (prepare_for_pass +0, pass_startup +4,
// finish_pass +8, call_pass_startup +0xc, is_last_pass +0x10, pass_type +0x14, pass_number +0x18,
// total_passes +0x1c, scan_number +0x20). A module object's method slots are at +8 (write_frame_header /
// finish_pass) and +0xc (write_scan_header). The vector's call recorder is kept at cinfo+0x170, past the end
// of jpeg_compress_struct (0x168): the method callbacks get only cinfo, so that is how each one finds it.
(function () {
  function keep(cb) { globalThis.__diffKeepAlive.push(cb); return cb; }
  function zero(p, n) { for (let i = 0; i < n; i += 4) p.add(i).writeU32(0); }
  function fill(p, n, v) { for (let i = 0; i < n; i += 4) p.add(i).writeU32(v); }
  function alloc(n, v) { const p = Memory.alloc(n); if (v === undefined) zero(p, n); else fill(p, n, v); return p; }
  function fillBytes(p, n, v) { for (let i = 0; i < n; i++) p.add(i).writeU8(v); }

  // ---- call recorder -------------------------------------------------------------------------------------
  // rec+0 is the cursor, then pairs of dwords (id, argument). Making the recorder a compared region turns the
  // ORDER of the dispatched methods and their arguments into evidence, not only the struct fields.
  const REC = 0x100;
  function recorder(id, hasArg, extra) {
    const types = hasArg ? ['pointer', 'int'] : ['pointer'];
    return keep(new NativeCallback(function (cinfo, arg) {
      const rec = cinfo.add(0x170).readPointer();
      if (!rec.isNull()) {
        const n = rec.readU32();
        if (n < 0x1e) {
          rec.add(4 + n * 8).writeS32(id);
          rec.add(8 + n * 8).writeS32(hasArg ? arg : -0x55);
          rec.writeU32(n + 1);
        }
      }
      if (extra) extra(cinfo);
    }, 'void', types));
  }

  // ---- pass_startup 0x004b5ca0 ---------------------------------------------------------------------------
  // Two marker objects per vector. The plain frame-header callback records id 1; the SWAPPING one also
  // rewrites cinfo+0x154 to the second marker, whose scan-header callback records id 4 instead of 2. A body
  // that cached cinfo->marker across the two calls would read RED on those vectors (the original re-reads
  // cinfo+0x154 at 0x004b5cbc).
  const M_FRAME = recorder(1, false);
  const M_SCAN = recorder(2, false);
  const M_SCAN_ALT = recorder(4, false);
  let MARKER_ALT = null;
  const M_FRAME_SWAP = recorder(3, false, function (cinfo) { cinfo.add(0x154).writePointer(MARKER_ALT); });

  function mkModule(slots) {
    const m = alloc(0x20, 0x6b6b6b6b);
    Object.keys(slots).forEach(function (off) { m.add(parseInt(off, 10)).writePointer(slots[off]); });
    return m;
  }

  // ---- Surface::blitRLE 0x00482990 ------------------------------------------------------------------------
  // The display is {+0: unused, +4: surface}. The surface is {+0: vtable, +4: destination buffer, +8:
  // recorder}: slot +0x20 (index 8) locks and returns the buffer, slot +0x24 (index 9) unlocks. Both record
  // into the surface's own recorder, so the number of locks, the order and the unlock argument are compared.
  const RLE_LOCK = keep(new NativeCallback(function (surf) {
    const rec = surf.add(8).readPointer();
    const n = rec.readU32();
    if (n < 0xe) { rec.add(4 + n * 8).writeS32(1); rec.add(8 + n * 8).writeS32(-0x55); rec.writeU32(n + 1); }
    return surf.add(4).readPointer();
  }, 'pointer', ['pointer'], 'thiscall'));
  const RLE_UNLOCK = keep(new NativeCallback(function (surf, flag) {
    const rec = surf.add(8).readPointer();
    const n = rec.readU32();
    if (n < 0xe) { rec.add(4 + n * 8).writeS32(2); rec.add(8 + n * 8).writeS32(flag); rec.writeU32(n + 1); }
  }, 'void', ['pointer', 'int'], 'thiscall'));
  const RLE_VTBL = (function () {
    const v = alloc(0x30, 0x9d9d9d9d);
    v.add(0x20).writePointer(RLE_LOCK);
    v.add(0x24).writePointer(RLE_UNLOCK);
    return v;
  })();

  // ---- live tables the terrain, sim and roster fixtures seed ----------------------------------------------
  const TILE_TYPE = ptr(0x005722e8);     // 50x50 tile type bytes; tileBlocked 0x0040bf60 rejects 0x14
  const SHADE_POS = ptr(0x0082376c);     // 50x50 positive shading accumulator
  const SHADE_NEG = ptr(0x00822da0);     // 50x50 negative shading accumulator
  const SHADE_WEIGHT = ptr(0x0053ea24);  // 50x50 weights spreadAllShade sums
  const SHADE_SRC_POS = ptr(0x005a6378); // 50x50 positive sources
  const SHADE_SRC_NEG = ptr(0x0056c7e4); // 50x50 negative sources
  const OBJ_TABLE = ptr(0x0056d1b8);     // 100 object records of 0x3c
  const KIND_TABLE = ptr(0x0055d758);    // kind records of 0x25; spawnObject reads byte +0
  const LOT_MAP = ptr(0x004e6d70);       // per-lot 19x6 cell map, stride 0x74
  const RNG_SEED = ptr(0x00822d9c);
  const ROSTER = ptr(0x00585850);        // walker / roster records, stride 0x4c
  const TEXT = ptr(0x0051a068);          // shared text buffer

  // A deterministic synthetic pattern: 0x14 (blocked) on one cell in seven, otherwise a type byte that varies.
  function seedTiles() {
    for (let i = 0; i < 2500; i++) TILE_TYPE.add(i).writeU8((i % 7) === 3 ? 0x14 : (i % 13));
  }
  // Pre-fill both accumulators so a clamp at 0 and a clamp at 0xff are both reachable and any write shows.
  function seedShade() {
    for (let i = 0; i < 2500; i++) {
      SHADE_POS.add(i).writeU8((i * 7 + 11) & 0xff);
      SHADE_NEG.add(i).writeU8((i * 13 + 29) & 0xff);
    }
  }
  function writeCell(base, cells) {
    for (let i = 0; i < 2500; i++) base.add(i).writeU8(0);
    (cells || []).forEach(function (c) { base.add(c[0] * 50 + c[1]).writeU8(c[2]); });
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    // ---- pass_startup -------------------------------------------------------------------------------------
    // 12 cinfos. Vectors 0..7 use the plain marker (recorded ids 1 then 2); vectors 8..11 use the swapping
    // frame-header callback (ids 3 then 4), which also replaces cinfo+0x154. Each master is pre-filled with a
    // different dword so the clear of call_pass_startup (+0xc) and the untouched neighbours are both visible.
    c3bs_pass() {
      const fx = {};
      const marker = mkModule({ 8: M_FRAME, 12: M_SCAN });
      const markerSwap = mkModule({ 8: M_FRAME_SWAP, 12: M_SCAN });
      MARKER_ALT = mkModule({ 8: M_FRAME, 12: M_SCAN_ALT });
      const fills = [0x00000000, 0x11111111, 0xffffffff, 0x0000000c, 0x7fffffff, 0x80000000,
                     0xa5a5a5a5, 0x00000001, 0x33333333, 0xcafe0001, 0x0000ffff, 0xdeadbeef];
      for (let i = 0; i < 12; i++) {
        const c = alloc(0x180);
        const m = alloc(0x24, fills[i]);
        const r = alloc(REC, 0x3c3c3c3c);
        r.writeU32(0);
        c.add(0x144).writePointer(m);
        c.add(0x154).writePointer(i < 8 ? marker : markerSwap);
        c.add(0x170).writePointer(r);
        fx['c' + i] = c; fx['m' + i] = m; fx['r' + i] = r;
        if (i === 0) fx.obj = c;
      }
      return fx;
    },

    // ---- finish_pass --------------------------------------------------------------------------------------
    // 14 cinfos sweeping master->pass_type (+0x14) over 0, 1, 2 and four values that fall to the default arm,
    // crossed with cinfo->optimize_coding (+0xb0) 0 and 1, and with pass_number (+0x18) / scan_number (+0x20)
    // seeded to values whose increment is visible (including the 0xffffffff wrap).
    c3bs_finish() {
      const rows = [
        //  pass_type, optimize_coding, pass_number, scan_number
        [0, 0, 0, 0], [0, 1, 5, 7], [0, 2, 1, 1],
        [1, 0, 3, 9], [1, 1, 0, 0],
        [2, 0, 2, 4], [2, 1, 8, 3], [2, -1, 0, 0xffffffff],
        [3, 0, 1, 1], [-1, 1, 6, 6], [0x7fffffff, 0, 0, 0], [99, 1, 0xffffffff, 2],
        [2, 0, 0, 0], [1, 1, 0xfffffffe, 0xfffffffe],
      ];
      const fx = {};
      const entropy = mkModule({ 8: recorder(5, false) });
      rows.forEach(function (r, i) {
        const c = alloc(0x180);
        const m = alloc(0x24, 0x47474747);
        const rec = alloc(REC, 0x3c3c3c3c);
        rec.writeU32(0);
        c.add(0xb0).writeS32(r[1]);
        c.add(0x144).writePointer(m);
        c.add(0x164).writePointer(entropy);
        c.add(0x170).writePointer(rec);
        m.add(0x14).writeS32(r[0]);
        m.add(0x18).writeU32(r[2] >>> 0);
        m.add(0x20).writeU32(r[3] >>> 0);
        fx['c' + i] = c; fx['m' + i] = m; fx['r' + i] = rec;
        if (i === 0) fx.obj = c;
      });
      return fx;
    },

    // ---- spreadShade --------------------------------------------------------------------------------------
    // The tile types decide which of the 225 cells of a stamp are skipped, and both accumulators start from a
    // pattern that reaches 0 and 0xff after a large value, so both clamp sides fire.
    c3bs_shade() { seedTiles(); seedShade(); return {}; },

    // ---- spreadAllShade: one fixture per total-weight case --------------------------------------------------
    // `weight` entries [row, col, byte] sum to the total; `pos` and `neg` are the source cells that produce
    // the spreadShade calls. The two accumulators are pre-filled so the unconditional clears are visible.
    _c3bsAll(weights, pos, neg) {
      seedTiles();
      seedShade();
      writeCell(SHADE_WEIGHT, weights);
      writeCell(SHADE_SRC_POS, pos);
      writeCell(SHADE_SRC_NEG, neg);
      return {};
    },
    // total 0: the early return at 0x00461732, after both clears.
    c3bs_all_empty() { return this._c3bsAll([], [[10, 10, 50]], [[11, 11, 60]]); },
    // total 9: the last total that still returns early (the `cmp ebx, 0xa` at 0x00461721).
    c3bs_all_below() { return this._c3bsAll([[0, 0, 9]], [[10, 10, 50]], [[11, 11, 60]]); },
    // total 10: the first total that spreads.
    c3bs_all_at10() { return this._c3bsAll([[0, 0, 10]], [[20, 20, 1]], []); },
    // total 1000, positive sources only.
    c3bs_all_pos() {
      return this._c3bsAll([[1, 1, 250], [2, 2, 250], [3, 3, 250], [4, 4, 250]],
                           [[10, 10, 1], [12, 30, 40], [40, 8, 200], [0, 0, 255], [49, 49, 90], [25, 25, 7]], []);
    },
    // total 1000, negative sources only.
    c3bs_all_neg() {
      return this._c3bsAll([[1, 1, 250], [2, 2, 250], [3, 3, 250], [4, 4, 250]], [],
                           [[10, 10, 1], [12, 30, 40], [40, 8, 200], [0, 0, 255], [49, 49, 90], [25, 25, 7]]);
    },
    // total exactly 0x6d6 (1750): the last total the `jle` at 0x0046173e lets through uncapped.
    c3bs_all_at1750() {
      const w = [];
      for (let i = 0; i < 7; i++) w.push([i, i, 250]);
      return this._c3bsAll(w, [[15, 15, 100]], [[16, 16, 100]]);
    },
    // total 5000 > 0x6d6: the cap at 0x00461740, with both source grids populated.
    c3bs_all_clamped() {
      const w = [];
      for (let i = 0; i < 20; i++) w.push([i, i, 250]);
      return this._c3bsAll(w, [[5, 5, 200], [30, 31, 90], [48, 1, 255], [20, 20, 3]],
                           [[6, 6, 200], [30, 32, 90], [48, 2, 255], [21, 21, 3]]);
    },

    // ---- Surface::blitRLE ---------------------------------------------------------------------------------
    // Each vector gets its own object (0x5c bytes, +0x58 -> a 0x10-byte header with width +8 and height +0xa),
    // its own payload, its own destination buffer (pre-filled 0xaa so every written byte shows) and its own
    // surface + display + recorder. `runs` is the per-row byte list AFTER the one byte every row skips, so a
    // row is [skip, ...runs]; the run lengths of a row add up to exactly its width (the inner loop tests
    // `w != 0` at 0x00482a44, so an overshoot would never stop).
    c3bs_rle() {
      const rows16 = [];
      for (let i = 0; i < 16; i++) rows16.push(0x30 + i);
      const lit127 = [];
      for (let i = 0; i < 127; i++) lit127.push((i * 3 + 1) & 0xff);
      const ones20 = [];
      for (let i = 0; i < 20; i++) ones20.push(1, 0x40 + i);
      const specs = [
        // [width, height, [row runs...]]
        [8, 1, [[0xfc, 0x11, 0x22, 0x33, 0x44, 0x04, 0x55]]],
        [8, 2, [[0xfc, 1, 2, 3, 4, 4, 0x77], [8, 0x99]]],
        [1, 1, [[1, 0x5a]]],
        [0, 3, [[], [], []]],
        [8, 0, []],
        [16, 1, [[0x10, 0xc3]]],
        [16, 1, [[0xf0].concat(rows16)]],
        [12, 3, [[0x0c, 0x01], [0xf4, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0x5f, 0x5e], [6, 0x20, 0xfa, 1, 2, 3, 4, 5, 6]]],
        [127, 1, [[0x7f, 0x6d]]],
        [127, 1, [[0x81].concat(lit127)]],
        [20, 2, [ones20, [0x14, 0xee]]],
        [6, 5, [[6, 1], [0xfa, 1, 2, 3, 4, 5, 6], [3, 7, 3, 8], [0xfd, 9, 9, 9, 3, 0], [2, 0x11, 0xfc, 1, 2, 3, 4]]],
        [3, 1, [[0xfd, 0xa1, 0xa2, 0xa3]]],
        [4, 4, [[4, 0x01], [4, 0x02], [0xfc, 1, 2, 3, 4], [2, 5, 2, 6]]],
      ];
      const fx = {};
      specs.forEach(function (s, i) {
        const hdr = alloc(0x10, 0x7e7e7e7e);
        hdr.add(8).writeU16(s[0]);
        hdr.add(0xa).writeU16(s[1]);
        const self = alloc(0x5c, 0x6d6d6d6d);
        self.add(0x58).writePointer(hdr);
        const bytes = [0xde, 0xad, 0xbe, 0xef, 0x01, 0x02];      // the 6 bytes blitRLE skips at chunk+0
        s[2].forEach(function (row) { bytes.push(0xcc); row.forEach(function (b) { bytes.push(b & 0xff); }); });
        const chunk = alloc((bytes.length + 7) & ~3, 0);
        bytes.forEach(function (b, k) { chunk.add(k).writeU8(b); });
        const dst = alloc(0xc0, 0xaaaaaaaa);   // one size for every vector (the widest row is 127 bytes)
        const rec = alloc(0x40, 0x3c3c3c3c);
        rec.writeU32(0);
        const surf = alloc(0x10);
        surf.writePointer(RLE_VTBL);
        surf.add(4).writePointer(dst);
        surf.add(8).writePointer(rec);
        const disp = alloc(0x10, 0x5e5e5e5e);
        disp.add(4).writePointer(surf);
        fx['t' + i] = self; fx['k' + i] = chunk; fx['d' + i] = disp;
        fx['b' + i] = dst; fx['r' + i] = rec;
        if (i === 0) fx.obj = self;
      });
      return fx;
    },

    // ---- spawnObject --------------------------------------------------------------------------------------
    // The object table is pre-filled 0x5a with the 16-bit type field at +0x20 set per record; `free` is the
    // index whose type is -1 (or -1 for a full table). The kind table gets 16 records of 0x25 whose byte +0
    // covers positive, negative and 0x80 values. Each of the lot maps 0..7 has its own set of accepting
    // (0xff) cells inside the 4x4 block Random::range(4) can reach, so the accepted pair differs per lot;
    // never zero accepting cells, or the retry loop at 0x0040129d would not end.
    _c3bsSpawn(free) {
      for (let i = 0; i < 100; i++) {
        const rec = OBJ_TABLE.add(i * 0x3c);
        fill(rec, 0x3c, 0x5a5a5a5a);
        rec.add(0x20).writeS16(i === free ? -1 : (free < 0 ? 3 : 7));
      }
      const kinds = [0, 1, 2, 0x7f, 0x80, 0xff, 5, 0x40, 0xfe, 0x33, 0x81, 0x7e, 0x10, 0xc0, 0x09, 0x55];
      for (let i = 0; i < 16; i++) {
        fillBytes(KIND_TABLE.add(i * 0x25), 0x25, 0x1d);
        KIND_TABLE.add(i * 0x25).writeU8(kinds[i]);
      }
      const accept = [
        [[0, 0], [0, 1], [0, 2], [0, 3], [1, 0], [1, 1], [1, 2], [1, 3],
         [2, 0], [2, 1], [2, 2], [2, 3], [3, 0], [3, 1], [3, 2], [3, 3]],
        [[0, 0]], [[3, 3]], [[1, 2]], [[0, 1], [2, 3]], [[2, 0]],
        [[0, 0], [1, 1], [2, 2], [3, 3]], [[3, 0], [0, 3]],
      ];
      for (let lot = 0; lot < 8; lot++) {
        fillBytes(LOT_MAP.add(lot * 0x74), 0x74, 0x20);
        accept[lot].forEach(function (c) { LOT_MAP.add(lot * 0x74 + c[0] * 6 + c[1]).writeU8(0xff); });
      }
      RNG_SEED.writeU32(0x12345678);
      return {};
    },
    c3bs_spawn0() { return this._c3bsSpawn(0); },     // record 0 free: the search loop is not entered
    c3bs_spawn5() { return this._c3bsSpawn(5); },     // records 0..4 taken: five turns of the search loop
    c3bs_spawnfull() { return this._c3bsSpawn(-1); }, // every record taken: the empty return at 0x004011f9

    // ---- pickGolferName -----------------------------------------------------------------------------------
    // 16 roster records of 0x4c (the table spawnWalker 0x00402970 fills). Each one sets the flag byte +0x12,
    // the type byte +0x13 and, for the custom-name vectors, a synthetic NUL-terminated string at +0x2a.
    // Only type bytes 0xfe, 0xfd, 0xfc and 0xfb are used on the table path: the slot is
    // ((flags >> 3) & 1) - (2 * type + 4), so those four give slots 0..7, the eight entries of the pointer
    // arrays at 0x004c148c and 0x004c1464 that are inside the table. 0xfa takes the fixed-string arm.
    // The text buffer starts from our own marker so every append is measured from the same prefix.
    c3bs_names() {
      const rows = [
        // [flags, type, customName]
        [0x00, 0xfe, null], [0x08, 0xfe, null],
        [0x00, 0xfd, null], [0x08, 0xfd, null],
        [0x00, 0xfc, null], [0x08, 0xfc, null],
        [0x00, 0xfb, null], [0x08, 0xfb, null],
        [0x00, 0xfa, null], [0x08, 0xfa, null],
        [0x00, 0xfe, 'Zq1'], [0x08, 0xfa, 'C3bsLongSyntheticName'], [0x00, 0xfd, 'Y'],
        [0xf7, 0xfd, null], [0xff, 0xfd, null], [0x08, 0xfc, null],
      ];
      rows.forEach(function (r, i) {
        const rec = ROSTER.add(i * 0x4c);
        fillBytes(rec, 0x4c, 0x27);
        rec.add(0x12).writeU8(r[0]);
        rec.add(0x13).writeU8(r[1]);
        if (r[2]) {
          for (let k = 0; k < r[2].length; k++) rec.add(0x2a + k).writeU8(r[2].charCodeAt(k));
          rec.add(0x2a + r[2].length).writeU8(0);
        } else {
          rec.add(0x2a).writeU8(0);
        }
      });
      fillBytes(TEXT, 768, 0);
      const prefix = 'c3bs>';
      for (let k = 0; k < prefix.length; k++) TEXT.add(k).writeU8(prefix.charCodeAt(k));
      return {};
    },
  });
})();
