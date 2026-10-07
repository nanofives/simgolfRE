// Fixtures for C3 batch c3ah (addScore 0x004732d0, clearObjectFootprint 0x0040e400, membershipReport 0x00406670).
// Only our own synthetic values are written (no game text is produced or recorded); names are prefixed c3ah_.
// Every table the function and its callees read is seeded so each branch is reachable and both A/B arms draw the
// same Random numbers (the RNG seed 0x00822d9c is a state region, restored per vector).
Object.assign(globalThis.DIFF_FIXTURES, {

  // ---------------------------------------------------------------- addScore 0x004732d0
  // Scoreboard rows: 10 x 0x9c at 0x00541ce0 (name +0, text +0x40, fun +0x80, skill +0x84, cash +0x88,
  // level word +0x96, id +0x98). With skill = cash = level = 0 a row's ranking value is simply its fun field,
  // and with cash 1000 / skill 0 / fun 0 / level 0 the course's own score is 1000/10 = 100: `funs` therefore
  // decides which row the course beats first, and `dupRow` which row already carries the current course id
  // 0x00822c78 (= 7 here).
  _c3ah_score(funs, dupRow) {
    ptr('0x00571fd4').writeS32(1000);      // cash   (0x004732db)
    ptr('0x00541cd8').writeS32(0);         // skill rating (0x004732d5)
    ptr('0x0059ae78').writeS32(0);         // fun rating   (0x004732ec)
    ptr('0x00822c88').writeS32(0);         // level        (0x004732f3)
    ptr('0x00822c78').writeS32(7);         // current course id
    ptr('0x008392a8').writeS32(0x5555);    // overwritten with -1 then with the inserted row
    const base = ptr('0x00541ce0');
    for (let i = 0; i < 10; i++) {
      const r = base.add(i * 0x9c);
      for (let k = 0; k < 0x9c; k += 4) r.add(k).writeU32(0);
      r.writeUtf8String('c3ahRow' + i);
      r.add(0x40).writeUtf8String('c3ahTxt' + i);
      r.add(0x80).writeS32(funs[i]);
      r.add(0x84).writeS32(0);
      r.add(0x88).writeS32(0);
      r.add(0x96).writeS16(0);
      r.add(0x98).writeS32(i === dupRow ? 7 : 100 + i);
    }
    // appendCourseTitle 0x0040daa0 inputs: appendString's word table (0xffff -> appends nothing and returns 0),
    // the current course 0, its site byte and site name, and the hole count that picks the suffix arm.
    ptr('0x0059d81c').writeU16(0xffff);
    ptr('0x0059bf90').writeS32(0);
    ptr('0x00571ff4').writeU8(0);
    Memory.protect(ptr('0x004c1ea9'), 32, 'rw-');
    ptr('0x004c1ea9').writeUtf8String('c3ahClub');
    ptr('0x005685f0').writeS32(5);
    ptr('0x0051a068').writeUtf8String('');
    return { obj: base };
  },
  _c3ah_funs(firstBeaten) {
    const f = [];
    for (let i = 0; i < 10; i++) f.push(i < firstBeaten ? 200 : 50);
    return f;
  },
  // beats row 0; no row holds the course id -> slot 9, all nine rows shift down, returns 0
  c3ah_score_top() { return globalThis.DIFF_FIXTURES._c3ah_score(globalThis.DIFF_FIXTURES._c3ah_funs(0), -1); },
  // beats row 5 -> shift of rows 5..8, returns 5
  c3ah_score_mid() { return globalThis.DIFF_FIXTURES._c3ah_score(globalThis.DIFF_FIXTURES._c3ah_funs(5), -1); },
  // beats only row 9 -> slot == i == 9, the shift loop is skipped, returns 9
  c3ah_score_last() { return globalThis.DIFF_FIXTURES._c3ah_score(globalThis.DIFF_FIXTURES._c3ah_funs(9), -1); },
  // beats row 3 and row 6 already holds the course id -> slot 6, only rows 3..5 shift, returns 3
  c3ah_score_dup() { return globalThis.DIFF_FIXTURES._c3ah_score(globalThis.DIFF_FIXTURES._c3ah_funs(3), 6); },
  // beats row 4 which itself holds the course id -> slot == i == 4, no shift, returns 4
  c3ah_score_dupi() { return globalThis.DIFF_FIXTURES._c3ah_score(globalThis.DIFF_FIXTURES._c3ah_funs(4), 4); },
  // beats nothing and row 2 holds the course id -> refused at 0x00473348, returns -1
  c3ah_score_rejid() { return globalThis.DIFF_FIXTURES._c3ah_score(globalThis.DIFF_FIXTURES._c3ah_funs(10), 2); },
  // beats nothing and no row holds the id -> the scan runs off row 9 (0x00473357), returns -1
  c3ah_score_rejend() { return globalThis.DIFF_FIXTURES._c3ah_score(globalThis.DIFF_FIXTURES._c3ah_funs(10), -1); },

  // -------------------------------------------------------- clearObjectFootprint 0x0040e400
  // kind = the course-kind byte 0x005a34e0 (0 and 2 give a type-0xc object the replacement tile 0x11),
  // course = the course index 0x0059bf90 (>= 0xc adds one to the default replacement tile 4).
  _c3ah_obj(kind, course) {
    globalThis.DIFF_FIXTURES.golf_tables();

    // Per-tile tables the footprint loop rewrites and rebuildHeightfield 0x0042f7a0 re-reads.
    for (let i = 0; i < 2500; i++) {
      ptr('0x0053caf0').add(i * 2).writeU16((0xffff ^ (i * 7)) & 0xffff);
      ptr('0x005722e8').add(i).writeU8((i * 11) % 0x24);     // tile types stay inside the 0x26-entry type table
      ptr('0x0056988c').add(i).writeU8((i * 5) & 0xff);
    }
    // Corner heights cornerHeights 0x0040bfe0 returns (8 bytes per cell at 0x0051b770).
    const ch = ptr('0x0051b770');
    for (let i = 0; i < 2500; i++)
      for (let k = 0; k < 8; k++) ch.add(i * 8 + k).writeS8(((i * 3 + k * 5) % 7) + 1);
    // Tile-type records (0x30 each from 0x00578370). The behaviour dword at +0xc is zeroed so rebuildHeightfield's
    // convergence loop finishes in one pass and cornerRange is not called; the bytes at +2/+3/+6/+7 feed rateLot.
    Memory.protect(ptr('0x00578370'), 0x26 * 0x30, 'rw-');
    for (let t = 0; t < 0x26; t++) {
      ptr('0x0057837c').add(t * 0x30).writeU32(0);
      ptr('0x00578372').add(t * 0x30).writeS8((t % 5) - 1);
      ptr('0x00578373').add(t * 0x30).writeS8((t % 7) + 1);
      ptr('0x00578376').add(t * 0x30).writeS8(t % 3);
      ptr('0x00578377').add(t * 0x30).writeS8(t % 4);
    }

    // Placed objects: 256 records of 0x10 (type word +0, x word +2, y word +4, cost dword +8); all free (0xffff)
    // except the ten the vectors address. 0 lot with a cost, 1 lot without a cost, 2 type 0xf (sets flag 0x800),
    // 3 type 0xc (the course-kind replacement tile), 4 type 7 (no extra footprint), 5 type 8 (extra footprint),
    // 6 type 3 (below 6), 7 type 0x10 (footprint 0 -> the loop is skipped), 8 type 0, 9 type 6.
    const po = ptr('0x0058bcb8');
    Memory.protect(po, 256 * 0x10, 'rw-');
    for (let i = 0; i < 256; i++) {
      const r = po.add(i * 0x10);
      r.writeS16(-1); r.add(2).writeS16(0); r.add(4).writeS16(0); r.add(8).writeS32(0);
    }
    const defs = [[5, 10, 10, 5000], [5, 12, 14, 0], [0xf, 20, 20, 100], [0xc, 5, 5, 0], [7, 30, 30, 0],
                  [8, 35, 8, 0], [3, 2, 40, 0], [0x10, 40, 2, 0], [0, 0, 0, 0], [6, 44, 44, 0]];
    defs.forEach(function (d, i) {
      const r = po.add(i * 0x10);
      r.writeS16(d[0]); r.add(2).writeS16(d[1]); r.add(4).writeS16(d[2]); r.add(8).writeS32(d[3]);
    });

    // Footprint side per object type (signed byte at 0x004c26c0 + type*0x14) and the extra-size dwords at
    // 0x005a8c38 + type*4 that types above 5 (other than 7) add minus one. Type 0x10 gets 0 + 1 - 1 = 0.
    const sides = { 0: 1, 3: 2, 5: 2, 6: 1, 7: 3, 8: 1, 12: 2, 15: 2, 16: 0 };
    Memory.protect(ptr('0x004c26c0'), 0x26 * 0x14, 'rw-');
    Memory.protect(ptr('0x005a8c38'), 0x26 * 4, 'rw-');
    for (let t = 0; t < 0x26; t++) {
      ptr('0x004c26c0').add(t * 0x14).writeS8(sides[t] === undefined ? 1 : sides[t]);
      ptr('0x005a8c38').add(t * 4).writeS32(t === 16 ? 1 : (t % 3) + 1);
    }

    ptr('0x00571fd4').writeS32(250000);          // cash
    ptr('0x005a6d3c').writeS16(357);             // date -> revenue row 57
    for (let i = 0; i < 100; i++) ptr('0x00584212').add(i * 0x14).writeS16((i * 13) % 1000);
    ptr('0x0059e7b8').writeU32(2);               // flags: bit 0 (slope guard), 0x800 and 0x1000000 all clear

    // The 100 position records freeRecordAtPos 0x004011b0 walks (0x3c each from 0x0056d1d8: marker word +0,
    // x word +6, y word +8). Records 10 and 60 sit on the lot of object 0.
    for (let i = 0; i < 100; i++) {
      const r = ptr('0x0056d1d8').add(i * 0x3c);
      r.writeS16(i);
      r.add(6).writeS16(i % 50);
      r.add(8).writeS16(i % 50);
    }

    ptr('0x005a34e0').writeU8(kind);
    ptr('0x0059bf90').writeS32(course);
    // Hole table zeroed so rateLot 0x0042ef40 skips every per-hole term (no division by a seeded zero); then the
    // four words pointsPopup 0x0040c890 reads, its ring index and the lot-bonus dword rateLot scales by.
    for (let i = 0; i < 0x2698; i += 4) ptr('0x00575ab0').add(i).writeU32(0);
    for (let w = 0; w < 4; w++) ptr('0x00575ca0').add(w * 0x208).writeS32(1000 + w);
    ptr('0x0059abb0').writeS32(3);
    ptr('0x00543cd0').writeS32(0);
    return { obj: po };
  },
  c3ah_obj_k0() { return globalThis.DIFF_FIXTURES._c3ah_obj(0, 0); },    // type 0xc -> 0x11, others -> 4
  c3ah_obj_k1() { return globalThis.DIFF_FIXTURES._c3ah_obj(1, 20); },   // no 0x11 arm, course >= 0xc -> 5
  c3ah_obj_k2() { return globalThis.DIFF_FIXTURES._c3ah_obj(2, 5); },    // type 0xc -> 0x11, others -> 4

  // ------------------------------------------------------------ membershipReport 0x00406670
  // free  : membership slots 4, 8, ... 36 are empty, so the join sentence runs.
  // report: bit 3 of 0x005a5a00 (the report-enabled flag).
  // busy  : the ticker direction byte 0x00569498, which makes startMessage refuse at priority 0.
  _c3ah_mem(free, report, busy) {
    globalThis.DIFF_FIXTURES.golf_tables();

    // Golfer records (0x100 each from 0x005794c0): the flag dword +0x08 with bit 31 set (the report clears it)
    // and the membership-slot word +0xae. Golfer g uses slot 60 + g so the slots the census sweeps and the slots
    // the join loop may pick stay disjoint from the golfers' own slots.
    for (let g = 0; g < 0x20; g++) {
      const r = ptr('0x005794c0').add(g * 0x100);
      r.add(8).writeU32((0x80000000 | (g * 7)) >>> 0);
      r.add(0xae).writeS16(60 + (g % 12));
    }
    // Golfer-type records (0x230 each from 0x004d6098): the name at +0 of the slots the join loop can pick, and
    // bit 7 of the byte at +0x11 that typeBit7Clear 0x0046c940 tests for the golfers' own slots.
    Memory.protect(ptr('0x004d6098'), 76 * 0x230, 'rw-');
    for (let s = 4; s <= 36; s += 4) ptr('0x004d6098').add(s * 0x230).writeUtf8String('c3ahName' + s);
    for (let g = 0; g < 12; g++) {
      const b = ptr('0x004d60a9').add((60 + g) * 0x230);
      b.writeU8(g % 2 ? 0x80 : 0x00);
    }

    // Membership type table: 76 entries of 0x2c at 0x005849e0 (tier byte +2, ban byte +0x29, census word +0x2a).
    // Slots 60..71 carry 7 + (g % 6) so the bumped value (byte + 1) & 7 runs over the tiers 0..5 and no golfer
    // slot is ever empty; slots 0..59 and 72..75 are 1..5, with the multiples of 4 up to 36 emptied when `free`.
    const mt = ptr('0x005849e0');
    Memory.protect(mt, 76 * 0x2c, 'rw-');
    for (let i = 0; i < 76; i++) {
      const e = mt.add(i * 0x2c);
      let tierByte;
      if (i >= 60 && i < 72) tierByte = 7 + ((i - 60) % 6);
      else if (free && i >= 4 && i <= 36 && i % 4 === 0) tierByte = 0;
      else tierByte = (i % 5) + 1;
      e.add(2).writeU8(tierByte);
      e.add(0x29).writeU8(i % 9 === 0 ? 0xff : 0);
      e.add(0x2a).writeS16(i % 4 === 0 ? 0 : i + 1);
    }

    // The tier-name pointer table 0x004c2a88 (entries 1..5; entry 3 is also read directly at 0x004c2a94).
    Memory.protect(ptr('0x004c2a88'), 0x20, 'rw-');
    for (let k = 1; k <= 5; k++) {
      const m = Memory.alloc(16);
      m.writeUtf8String('c3ahT' + k);
      ptr('0x004c2a88').add(k * 4).writePointer(m);
    }

    ptr('0x0051a068').writeUtf8String('');
    ptr('0x0058a528').writeUtf8String('');
    // Only bit 3 of 0x005a5a00 (the report-enabled rule) is forced; overwriting the whole byte takes away the
    // other mode bits the menu's own loop runs on and the game dies a few dozen injected calls later.
    const rb = ptr('0x005a5a00');
    rb.writeU8(((rb.readU8() & ~8) | (report ? 8 : 0)) & 0xff);
    ptr('0x004c2e0c').writeS32(0x103);          // equals golfer 3 | 0x100 -> the panel-reset arm
    ptr('0x005a9ccc').writeS32(0);
    ptr('0x00569498').writeU8(busy ? 1 : 0);
    ptr('0x0053df54').writeS32(0);
    ptr('0x00567afc').writeS32(0);
    ptr('0x005694a4').writeS32(0);
    ptr('0x00822d9c').writeS32(0x1234);          // RNG seed (state region, restored per vector)
    ptr('0x00822c88').writeS32(0);               // startMessage's length divisor term
    return { obj: mt };
  },
  // ------------------------------------------------------------- announceBuilding 0x0040e720
  // Hole records are 0x208 apart from 0x00575ab0 (par byte +0, orientation byte +2, yards word +4, tee +8/+0xc,
  // third point +0x10/+0x14, pin +0x18/+0x1c, flags dword +0x200). `c.hole` is the hole being closed, `c.rec`
  // its record, `c.lastHole` the last hole whose par byte is non-zero (the tail renumbers the counter from the
  // first zero par), `c.bi` the next-building index 0x005a6364, `c.tick` the clock 0x00834170, `c.diff` the
  // difficulty 0x00822c88, `c.pen` the per-step length penalty 0x005a8c60 and `c.gf40` bit 0x40 of the global flags 0x0059e7b8.
  _c3ah_ab(c) {
    globalThis.DIFF_FIXTURES.golf_tables();
    for (let i = 0; i < 0x26a8; i += 4) ptr('0x00575ab0').add(i).writeU32(0);
    const put = function (h, o) {
      const at = (b) => ptr(b).add(h * 0x208);
      at('0x00575ab0').writeS8(o.par | 0);
      at('0x00575ab4').writeS16(o.yards | 0);
      at('0x00575ab8').writeS32(o.tx | 0); at('0x00575abc').writeS32(o.ty | 0);
      at('0x00575ac0').writeS32(o.ax | 0); at('0x00575ac4').writeS32(o.ay | 0);
      at('0x00575ac8').writeS32(o.px | 0); at('0x00575acc').writeS32(o.py | 0);
      at('0x00575cb0').writeU32((o.flags | 0) >>> 0);
    };
    const last = c.lastHole === undefined ? 12 : c.lastHole;
    for (let h = 1; h <= 18; h++)
      put(h, { par: h <= last ? 3 + (h % 3) : 0, yards: 100 + h * 7, tx: h, ty: h + 1,
               ax: h * 2, ay: h, px: h + 10, py: h + 3, flags: 0 });
    put(c.hole, c.rec);
    ptr('0x00578150').writeS32(30);          // the hole-19 slot the hole-1 arm averages with hole 1's tee
    ptr('0x00578154').writeS32(42);

    ptr('0x005685f0').writeS32(c.hole);
    // Only bit 0x40 of the global flags (the "a third point is required" rule) is forced; the rest of the live
    // dword is left alone, because overwriting it puts the menu's own loop into a state it dies in after a few
    // dozen injected calls.
    const gfl = ptr('0x0059e7b8');
    gfl.writeU32((((gfl.readU32() & ~0x40) | (c.gf40 ? 0x40 : 0)) >>> 0));
    ptr('0x00834170').writeS32(c.tick | 0);
    ptr('0x005a8c60').writeS32(c.pen | 0);
    ptr('0x00822c88').writeS32(c.diff | 0);
    ptr('0x005a6364').writeS32(c.bi | 0);

    // Building-name records (0x14 each from 0x004c26b0; only the name at +0 is written, not the footprint byte
    // at +0x10) and the class-name pointers appendUpgradeText 0x0040e5f0 reads at 0x004c2a18.
    Memory.protect(ptr('0x004c26b0'), 0x26 * 0x14, 'rw-');
    for (let i = 0; i < 20; i++) ptr('0x004c26b0').add(i * 0x14).writeUtf8String('c3ahB' + i);
    Memory.protect(ptr('0x004c2a18'), 0x20, 'rw-');
    for (let k = 0; k < 4; k++) {
      const m = Memory.alloc(16);
      m.writeUtf8String('c3ahU' + k);
      ptr('0x004c2a18').add(k * 4).writePointer(m);
    }

    // Only the 18 tee cells this pass can touch are seeded (cell = tee y + tee x * 50); the rest of the tile
    // tables keeps its live content, which both A/B arms see identically because the whole table is a state
    // region. Rewriting all 2500 flag words made the menu's loop die after a few dozen injected calls.
    for (let h = 1; h <= 18; h++) {
      const cell = (h + 1) + h * 50;
      ptr('0x0053caf0').add(cell * 2).writeU16((h * 37) & 0x3fff);
      ptr('0x00578804').add(cell).writeU8((h * 3) & 0xff);
    }
    for (let i = 0; i < 10; i++) ptr('0x0056a524').add(i * 4).writeS32(0x1000 + i);
    ptr('0x004c2848').writeS32(0x77);
    ptr('0x00585860').writeU8(0); ptr('0x00585861').writeU8(0);

    ptr('0x0051a068').writeUtf8String(''); ptr('0x0058a528').writeUtf8String('');
    ptr('0x00569498').writeU8(0); ptr('0x0053df54').writeS32(0); ptr('0x00567afc').writeS32(0);
    ptr('0x005694a4').writeS32(0); ptr('0x00822d9c').writeS32(0x1234);
    ptr('0x0059bf90').writeS32(1);
    return { obj: ptr('0x00575ab0') };
  },
  _c3ah_rec(o) {
    const d = { par: 4, yards: 0, tx: 10, ty: 10, ax: 4, ay: 6, px: 12, py: 12, flags: 0 };
    Object.keys(o || {}).forEach(function (k) { d[k] = o[k]; });
    return d;
  },
  // par 6: tee (3,4) to pin (40,45) is 1380 yards before the over-250 bonus and the 25-per-step penalty.
  c3ah_ab_h7() {
    return globalThis.DIFF_FIXTURES._c3ah_ab({ hole: 7, bi: 5, tick: 0x2468, diff: 0, pen: 5, gf40: 0,
      rec: globalThis.DIFF_FIXTURES._c3ah_rec({ tx: 3, ty: 4, px: 40, py: 45, flags: 0x60 }) });
  },
  // par 3 short (70 yards); hole 1's par byte is 0 so the tail leaves the counter at 1
  c3ah_ab_p3short() {
    return globalThis.DIFF_FIXTURES._c3ah_ab({ hole: 7, bi: 5, tick: 0x2468, diff: 0, pen: 0, gf40: 0, lastHole: 0,
      rec: globalThis.DIFF_FIXTURES._c3ah_rec({ px: 12, py: 12 }) });
  },
  // par 2 (25 yards); every hole has a par byte so the tail walks to the end of the table
  c3ah_ab_p2() {
    return globalThis.DIFF_FIXTURES._c3ah_ab({ hole: 7, bi: 5, tick: 0x2468, diff: 0, pen: 0, gf40: 0, lastHole: 18,
      rec: globalThis.DIFF_FIXTURES._c3ah_rec({ px: 11, py: 10 }) });
  },
  // par 4 short (312 yards)
  c3ah_ab_p4short() {
    return globalThis.DIFF_FIXTURES._c3ah_ab({ hole: 7, bi: 5, tick: 0x2468, diff: 0, pen: 0, gf40: 0,
      rec: globalThis.DIFF_FIXTURES._c3ah_rec({ px: 22, py: 10 }) });
  },
  // par 5 (562 yards) -> the par-5 event
  c3ah_ab_p5() {
    return globalThis.DIFF_FIXTURES._c3ah_ab({ hole: 7, bi: 5, tick: 0x2468, diff: 0, pen: 0, gf40: 0,
      rec: globalThis.DIFF_FIXTURES._c3ah_rec({ px: 30, py: 10 }) });
  },
  // yards preset (the distance is not recomputed), every optional clause flag set, difficulty 2 (no events)
  c3ah_ab_p5long() {
    return globalThis.DIFF_FIXTURES._c3ah_ab({ hole: 7, bi: 5, tick: 0x2468, diff: 2, pen: 0, gf40: 0,
      rec: globalThis.DIFF_FIXTURES._c3ah_rec({ yards: 600, flags: 0x3060 }) });
  },
  // preset lengths inside a par band but past its shape threshold: par 3 long (220), par 4 long (440),
  // par 5 short (480)
  c3ah_ab_p3long() {
    return globalThis.DIFF_FIXTURES._c3ah_ab({ hole: 7, bi: 5, tick: 0x2468, diff: 0, pen: 0, gf40: 0,
      rec: globalThis.DIFF_FIXTURES._c3ah_rec({ yards: 220, px: 20, py: 18 }) });
  },
  c3ah_ab_p4long() {
    return globalThis.DIFF_FIXTURES._c3ah_ab({ hole: 7, bi: 5, tick: 0x2468, diff: 0, pen: 0, gf40: 0,
      rec: globalThis.DIFF_FIXTURES._c3ah_rec({ yards: 440, px: 20, py: 18 }) });
  },
  c3ah_ab_p5short() {
    return globalThis.DIFF_FIXTURES._c3ah_ab({ hole: 7, bi: 5, tick: 0x2468, diff: 0, pen: 0, gf40: 0,
      rec: globalThis.DIFF_FIXTURES._c3ah_rec({ yards: 480, px: 20, py: 18 }) });
  },
  // flags 0x60 with a short hole: the over-250 bonus is skipped although the flag is set, and the global flag
  // 0x40 is on with a third point present (the pass side of the third refusal arm)
  c3ah_ab_flag60short() {
    return globalThis.DIFF_FIXTURES._c3ah_ab({ hole: 7, bi: 5, tick: 0x2468, diff: 0, pen: 7, gf40: 1,
      rec: globalThis.DIFF_FIXTURES._c3ah_rec({ ax: 4, px: 12, py: 12, flags: 0x60 }) });
  },
  // hole 1 -> the two course-centre bytes; building index 3 (the first blurb case)
  c3ah_ab_h1() {
    return globalThis.DIFF_FIXTURES._c3ah_ab({ hole: 1, bi: 3, tick: 0x2468, diff: 0, pen: 0, gf40: 0,
      rec: globalThis.DIFF_FIXTURES._c3ah_rec({ tx: 2, ty: 3, px: 20, py: 25 }) });
  },
  c3ah_ab_h6() {
    return globalThis.DIFF_FIXTURES._c3ah_ab({ hole: 6, bi: 5, tick: 0x2468, diff: 0, pen: 0, gf40: 0,
      rec: globalThis.DIFF_FIXTURES._c3ah_rec({ px: 20, py: 18 }) });
  },
  c3ah_ab_h10() {
    return globalThis.DIFF_FIXTURES._c3ah_ab({ hole: 10, bi: 5, tick: 0x2468, diff: 0, pen: 0, gf40: 0,
      rec: globalThis.DIFF_FIXTURES._c3ah_rec({ px: 20, py: 18 }) });
  },
  // hole 18: even hole -> the other lead-in string, blurb case 8, the closing event and upgrade text 2
  c3ah_ab_h18() {
    return globalThis.DIFF_FIXTURES._c3ah_ab({ hole: 18, bi: 8, tick: 0x2468, diff: 0, pen: 0, gf40: 0, lastHole: 18,
      rec: globalThis.DIFF_FIXTURES._c3ah_rec({ px: 20, py: 18 }) });
  },
  // hole 9: the nine-hole event; blurb case 14 (the last)
  c3ah_ab_h9() {
    return globalThis.DIFF_FIXTURES._c3ah_ab({ hole: 9, bi: 14, tick: 0x2468, diff: 0, pen: 0, gf40: 0,
      rec: globalThis.DIFF_FIXTURES._c3ah_rec({ px: 20, py: 18 }) });
  },
  // building index 4: the jump-table entry that falls through to the shared tail without a blurb
  c3ah_ab_bi4() {
    return globalThis.DIFF_FIXTURES._c3ah_ab({ hole: 3, bi: 4, tick: 0x2468, diff: 0, pen: 0, gf40: 0,
      rec: globalThis.DIFF_FIXTURES._c3ah_rec({ px: 20, py: 18 }) });
  },
  // building index 0: below the jump table's range, so the blurb is skipped
  c3ah_ab_bi0() {
    return globalThis.DIFF_FIXTURES._c3ah_ab({ hole: 3, bi: 0, tick: 0x2468, diff: 0, pen: 0, gf40: 0,
      rec: globalThis.DIFF_FIXTURES._c3ah_rec({ px: 20, py: 18 }) });
  },
  // building index 15: at the cap, so the whole building paragraph is skipped
  c3ah_ab_bi15() {
    return globalThis.DIFF_FIXTURES._c3ah_ab({ hole: 3, bi: 15, tick: 0x2468, diff: 0, pen: 0, gf40: 0,
      rec: globalThis.DIFF_FIXTURES._c3ah_rec({ px: 20, py: 18 }) });
  },
  // hole 2 with building index 10: the hole is too early for the paragraph; clock 0 -> no sound, no message
  c3ah_ab_skipbi() {
    return globalThis.DIFF_FIXTURES._c3ah_ab({ hole: 2, bi: 10, tick: 0, diff: 0, pen: 0, gf40: 0,
      rec: globalThis.DIFF_FIXTURES._c3ah_rec({ px: 20, py: 18 }) });
  },
  // the three refusal arms
  c3ah_ab_rej_tee() {
    return globalThis.DIFF_FIXTURES._c3ah_ab({ hole: 7, bi: 5, tick: 0x2468, diff: 0, pen: 0, gf40: 0,
      rec: globalThis.DIFF_FIXTURES._c3ah_rec({ tx: 0, px: 20, py: 18 }) });
  },
  c3ah_ab_rej_pin() {
    return globalThis.DIFF_FIXTURES._c3ah_ab({ hole: 7, bi: 5, tick: 0x2468, diff: 0, pen: 0, gf40: 0,
      rec: globalThis.DIFF_FIXTURES._c3ah_rec({ px: 0, py: 18 }) });
  },
  // global flag 0x40 set and the third point missing
  c3ah_ab_rej_third() {
    return globalThis.DIFF_FIXTURES._c3ah_ab({ hole: 7, bi: 5, tick: 0x2468, diff: 0, pen: 0, gf40: 1,
      rec: globalThis.DIFF_FIXTURES._c3ah_rec({ ax: 0, px: 20, py: 18 }) });
  },

  c3ah_mem_free() { return globalThis.DIFF_FIXTURES._c3ah_mem(true, true, false); },
  c3ah_mem_full() { return globalThis.DIFF_FIXTURES._c3ah_mem(false, true, false); },
  c3ah_mem_busy() { return globalThis.DIFF_FIXTURES._c3ah_mem(true, true, true); },
  c3ah_mem_quiet() { return globalThis.DIFF_FIXTURES._c3ah_mem(true, false, false); },
});
