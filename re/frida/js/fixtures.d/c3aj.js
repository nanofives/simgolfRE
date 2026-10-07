// Fixtures for C3 batch c3aj (matchUpdate 0x00427380). Only our own synthetic values (no game text). Names are
// prefixed c3aj_. Every fixture seeds, in this order: the golfer records [0x005794b8, +0x800) (stride 0x100), the
// hole table [0x00575ab0, +0x2698) plus holes 19 and 20 (stride 0x208), the per-type table 0x005849e0 and the
// weekly money tables 0x00584210, the course-record table 0x0056a524 with the string store that holds the record
// holders' names (0x0056fcb0 / 0x0059d81c / 0x005a46b8), the per-type stroke totals 0x0056ae90, the 0x4c-stride
// record sweep 0x00585862, patronEvent 0x004266b0's globals and the scalar globals matchUpdate and its callees
// read. The flags dword 0x0059e7b8 is only read-modify-written on the three bits this function and its callees
// test (0x200000, 0x4000000, 0x1000000): the running menu loop reads the rest.
//
// The par byte of hole h is at 0x00575ab0 + h*0x208, and 0x00575cb8 + h*0x208 is the same byte for hole h + 1
// (0x00575cb8 = 0x00575ab0 + 0x208). So `g.lastHole` is the whole course layout: holes 1..lastHole have par 4 and
// every later hole has par 0, which is what makes a golfer standing on lastHole "at the end of the match"
// (0x00427d7b), what sends the round-end block through 0x00428327, and what makes the tail call patronEvent
// 0x004266b0 (0x00428780) instead of moving the golfer on.
Object.assign(globalThis.DIFF_FIXTURES, {
  _c3aj_rw(addr, size) { Memory.protect(ptr(addr), size, 'rw-'); },
  _c3aj_zero(addr, size) {
    globalThis.DIFF_FIXTURES._c3aj_rw(addr, size);
    const p = ptr(addr);
    for (let i = 0; i + 4 <= size; i += 4) p.add(i).writeU32(0);
  },
  _c3aj_merge(base, o) {
    Object.keys(o || {}).forEach(function (k) { base[k] = o[k]; });
    return base;
  },
  // Flat score table: n strokes on every hole 1..18.
  _c3aj_flat(n) { const s = {}; for (let h = 1; h <= 18; h++) s[h] = n; return s; },

  // One golfer record: field <base> + golfer*0x100 (0x0042738b). o.scores[h] is the stroke count of hole h,
  // written to 0x005794db + h (the per-hole score array the match sums read at 0x00427833). matchUpdate
  // overwrites the entry of the golfer's current hole with o.strokes (0x0042745e) before any of it is read, so
  // every fixture gives the called golfer the same value in both.
  _c3aj_setGolfer(i, o) {
    const at = (base) => ptr(base).add(i * 0x100);
    at('0x005794b8').writeS32(o.x | 0);            // world x (pointsPopup / playSoundAt / postEvent)
    at('0x005794bc').writeS32(o.y | 0);            // world y
    at('0x005794c8').writeU32((o.c8 | 0) >>> 0);   // golfer flags (bit 0x100000 keeps the rolling average)
    at('0x005794d0').writeU8(o.kind | 0);          // kind byte (0 normal, 0x40 commissioner, 0x60 CEO)
    at('0x005794d1').writeU8(o.kflags | 0);        // low nibble = histogram column, & 0xf0 = in a match
    at('0x005794d4').writeU8(1);
    at('0x005794d9').writeS8(o.hole | 0);          // hole just finished
    at('0x005794da').writeS8(o.strokes | 0);       // strokes on it
    at('0x005794dc').writeU8(o.dc === undefined ? 1 : o.dc);            // gates the course-record message
    at('0x005794ee').writeS8(o.ee | 0);            // decayed by sign() at 0x00428906
    at('0x0057953c').writeU8(o.thought === undefined ? 1 : o.thought);  // 0 calls emitGolferThought
    at('0x00579556').writeS16(0);
    at('0x0057955a').writeS16(o.partner | 0);      // partner index (word)
    at('0x0057955c').writeS16(o.sat | 0);          // satisfaction word = the greens fee
    at('0x0057956c').writeS16(o.rolling | 0);      // rolling average word
    at('0x0057956e').writeS16(o.type | 0);         // golfer type index
    at('0x00579572').writeS8(0);                   // band for the 0x0056ae90 totals
    at('0x00579573').writeU8(o.tag | 0);           // logTick tag
    at('0x00579576').writeS16(0);
    at('0x00579578').writeS32(o.lastDate | 0);     // last date stamp
    at('0x0057958c').writeS32(7);
    const sc = o.scores || {};
    for (let h = 1; h <= 18; h++) at('0x005794db').add(h).writeS8(sc[h] === undefined ? 0 : sc[h]);
    for (let h = 1; h <= 18; h++) at('0x005794ef').add(h).writeU8(0);
    for (let h = 1; h <= 18; h++) at('0x00579514').add(h).writeU8(0);
  },

  _c3aj_base(g) {
    const F = globalThis.DIFF_FIXTURES;

    // ---- golfer records 0..7 ------------------------------------------------------------------------------
    F._c3aj_zero('0x005794b8', 0x800);
    for (let i = 0; i < 8; i++) F._c3aj_setGolfer(i, g.golfer(i));

    // ---- hole table: the par bytes 0x00575ab0 (par 4 up to g.lastHole, then 0), the magazine flag dwords
    // 0x00575cb0 (bits 1 and 2 add 2 to the fee), the takings 0x00575ca4 and the waiting counter 0x00575c9c.
    F._c3aj_zero('0x00575ab0', 0x2698);
    for (let h = 0; h <= 20; h++) {
      F._c3aj_rw(ptr('0x00575ab0').add(h * 0x208), 0x210);
      ptr('0x00575ab0').add(h * 0x208).writeS8((h >= 1 && h <= g.lastHole) ? 4 : 0);
      ptr('0x00575cb0').add(h * 0x208).writeU32((g.holeFlags | 0) >>> 0);
      ptr('0x00575ca4').add(h * 0x208).writeS32(0);
      ptr('0x00575c9c').add(h * 0x208).writeS32(0);
    }

    // ---- per-type table 0x005849e0 (stride 0x2c): +0 best gross, +1 average over par, +2 rank/flag byte (low
    // three bits: > 3 adds the fee surcharge at 0x004274dc and 1 << rank is the come-back threshold at
    // 0x004286e5), +3+h the per-hole bits & 3 read at 0x00428394, +0x2a the par word read at 0x00428942.
    // The weekly money tables 0x00584210 (income) and 0x0058421e (purses) sit just below it, 100 rows of 0x14.
    F._c3aj_zero('0x00584210', 0x14e2);
    for (let t = 0; t < 76; t++) {
      const e = ptr('0x005849e0').add(t * 0x2c);
      e.writeU8(g.best | 0);                               // +0 best gross (0 takes the "no best yet" side)
      e.add(1).writeS8(g.avg | 0);                         // +1 average (0 takes the "no average yet" side)
      e.add(2).writeU8(t & 7);                             // +2 rank = type & 7
      for (let h = 0; h <= 19; h++) e.add(3 + h).writeU8(g.typeBits ? (h & 3) : 0);
      e.add(0x2a).writeS16(4);                             // +0x2a par word
    }

    // ---- course records 0x0056a524 (10 dwords) and the string store the holders' names live in -------------
    F._c3aj_rw('0x0056a524', 0x30);
    for (let k = 0; k < 10; k++) ptr('0x0056a524').add(k * 4).writeS32(g.records[k]);
    F._c3aj_zero('0x0056fcb0', 0x1002);
    F._c3aj_rw('0x0059d81c', 0x100);
    F._c3aj_rw('0x005a46b8', 0x100);
    for (let i = 0; i < 128; i++) {
      ptr('0x0059d81c').add(i * 2).writeS16(-1);
      ptr('0x005a46b8').add(i * 2).writeS16(0);
    }
    // Slots 0x14..0x1d hold three-byte synthetic names, so appendString 0x0045b9f0 finds an entry and
    // appendToBuffer45b8b0 0x0045b8b0 has to pack the store down (0x00428463 / 0x00428469).
    for (let k = 0; k < 10; k++) {
      const off = k * 3;
      ptr('0x0056fcb0').add(off).writeUtf8String('R' + k);
      ptr('0x0059d81c').add((0x14 + k) * 2).writeS16(off);
      ptr('0x005a46b8').add((0x14 + k) * 2).writeS16(3);
    }

    // ---- per-type stroke totals 0x0056ae90 / 0x0056aedc (index (hole + band*0x2e)*4, band 0..28) ------------
    F._c3aj_zero('0x0056ae90', 0x1600);

    // ---- the 0x4c-stride records swept at 0x00427736 (flag byte +0, golfer byte +1) -------------------------
    F._c3aj_rw('0x00585862', 0x1300);
    for (let p = 0; p < 0x1300; p += 0x4c) {
      ptr('0x00585862').add(p).writeU8(((p / 0x4c) % 3 === 0) ? 0 : 1);
      ptr('0x00585862').add(p + 1).writeS8((p / 0x4c) % 8);
    }

    // ---- patronEvent 0x004266b0's globals (the tail calls it whenever the golfer has no next hole) ----------
    F._c3aj_zero('0x0058bcb8', 0x1000);            // placed objects (landmarkName 0x004074a0 scans them)
    ptr('0x0053a450').writeS32(3);
    ptr('0x00543cfc').writeS32(0);
    ptr('0x00567a1c').writeS32(0);
    ptr('0x00572cac').writeS32(1);
    ptr('0x0059aaf8').writeS32(0);
    ptr('0x00822c70').writeS32(0);
    ptr('0x00569628').writeS32(0);                 // appendCents 0x0042dd50

    // ---- scalar globals -----------------------------------------------------------------------------------
    ptr('0x00543cf4').writeS32(g.mode | 0);        // 2 doubles the fee (0x0042747a)
    ptr('0x00543cd8').writeS32(g.feeBonus | 0);    // added to the fee (0x004274ba)
    ptr('0x00543cc4').writeS32(g.scoreBonus | 0);  // added to the come-back score (0x004286c3)
    ptr('0x00543cd4').writeS32(1);                 // divisor base: (1*5 + 0xf) * 8 = 160 (0x00428966)
    ptr('0x00822c88').writeS32(g.diff | 0);        // difficulty 0..2
    ptr('0x00834170').writeS32(g.date | 0);        // tick/date dword (0x00427753)
    ptr('0x005685f0').writeS32(g.lastHole + 1);    // holes + 1 (0x004283be, 0x00428682, 0x004285a6)
    ptr('0x0059b730').writeS32(g.purse | 0);       // purse counter (0x004278b7)
    ptr('0x005a6d3c').writeS16(g.week | 0);        // week word (0 enables the first-fee tutorial)
    ptr('0x00571fd4').writeS32(g.cash | 0);        // cash
    ptr('0x0056d1b0').writeS32(5);                 // member count (0x004286b2)
    ptr('0x004c2850').writeS32(0);                 // fee scratch
    ptr('0x004c2e0c').writeS32(-1);                // first-fee / record marker
    ptr('0x005a59f8').writeS32(-1);                // message argument
    ptr('0x005787cc').writeS32(0);                 // match-win counter
    ptr('0x005a9ccc').writeS32(0);                 // tutorial step (set to 3 at 0x0042764a)
    ptr('0x005a9cd8').writeS32(0);                 // playSoundAt scratch
    ptr('0x005a9cdc').writeS32(0);

    // startMessage 0x0040cb00 preconditions (idle unless g.busy), its RNG and the popup / event / log rings.
    ptr('0x00569498').writeU8(g.busy ? 1 : 0);
    ptr('0x0053df54').writeS32(0);
    ptr('0x00567afc').writeS32(0);
    ptr('0x005694a4').writeS32(0);
    ptr('0x00822d9c').writeS32(0x1234);            // RNG seed (state region, restored per vector)
    ptr('0x0059abb0').writeS32(0);                 // pointsPopup slot
    ptr('0x0059bf90').writeS32(1);                 // course id read by postEvent
    F._c3aj_zero('0x004c15a0', 0x300);             // postEvent records
    F._c3aj_zero('0x00568600', 1000);              // logTick log
    ptr('0x00584210').writeS16(g.income0 | 0);     // week 0's income word (0 arms the tutorial)
    ptr('0x0051a068').writeU8(0);                  // empty the text buffer

    // Only the bits matchUpdate and its callees test are changed in the live flags dword 0x0059e7b8: 0x200000
    // (skips the fee, the match narration and the round end), 0x4000000 (with 0x200000 the 0x4200000 wrap
    // branch at 0x00428720; also postEvent's guard) and 0x1000000 (pointsPopup's guard).
    const fl = ptr('0x0059e7b8').readU32();
    ptr('0x0059e7b8').writeU32(((fl & ~0x5200000) | ((g.flags | 0) >>> 0)) >>> 0);

    return { obj: ptr('0x005794b8') };
  },

  // Default golfer: not in a match (kflags & 0xf0 == 0), kind 0, thought gate closed, standing on hole 3 of a
  // round whose first three holes are played.
  _c3aj_golfer(i, o) {
    return globalThis.DIFF_FIXTURES._c3aj_merge({
      x: 1000 + i * 100, y: 2000 + i * 100, c8: 0x100000, kind: 0, kflags: i & 7, hole: 3, strokes: 4 + (i & 1),
      dc: 1, ee: (i & 1) ? 3 : -2, thought: 1, partner: i ^ 1, sat: 8 + i, rolling: 10 + i, type: 1 + (i % 4),
      tag: 0x10 + i, lastDate: (i === 4) ? 600 : 100, scores: { 1: 4, 2: 5, 3: 3 },
    }, o);
  },

  _c3aj_defaults(over) {
    const F = globalThis.DIFF_FIXTURES;
    return F._c3aj_base(F._c3aj_merge({
      flags: 0, mode: 0, feeBonus: 0, scoreBonus: 0, diff: 1, date: 500, purse: 5, week: 7, cash: 1000,
      holeFlags: 0, busy: 0, best: 0, avg: 0, lastHole: 18, typeBits: 1, income0: 0,
      records: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
      golfer: function (i) { return F._c3aj_golfer(i, {}); },
    }, over));
  },

  // A pair fixture for the end of the match: the course has 17 holes, the called golfer (2, 5 or 6) stands on
  // hole 17 and its partner on hole 18, so the partner is "ahead" (0x004277f9), the match is over (0x00427d7b),
  // the round-end block runs (0x00428327) and the tail calls patronEvent (0x00428780). Each golfer plays `n(i)`
  // strokes on every hole, including the one the call overwrites.
  _c3aj_pairs(n, over) {
    const F = globalThis.DIFF_FIXTURES;
    return F._c3aj_defaults(F._c3aj_merge({
      lastHole: 17,
      golfer: function (i) {
        const called = (i === 2 || i === 5 || i === 6);
        return F._c3aj_golfer(i, {
          kflags: 0xf0 | (i & 7), hole: called ? 17 : 18, partner: i ^ 1,
          scores: F._c3aj_flat(n(i)), strokes: n(i),
        });
      },
    }, over));
  },

  // ---- variants ---------------------------------------------------------------------------------------------

  // 1. First greens fee: week 0 and an empty weekly income table, so the tutorial message is built and
  // startMessage 0x0040cb00 runs (0x0042752a and 0x00427538 both taken). Types 1..4 keep the rank <= 3 side of
  // 0x004274dc. Hole 3 of an 18-hole course: the match block is skipped (kflags & 0xf0 == 0, 0x004277cd), the
  // round-end block is skipped (hole 4 has par, 0x00428327) and the tail moves the golfer on without calling
  // patronEvent (0x00428780 / 0x00428798).
  c3aj_fee() {
    const F = globalThis.DIFF_FIXTURES;
    return F._c3aj_defaults({
      week: 0,
      // Golfer 3 has no hole index and golfer 5 stands on hole 19, so a vector on golfer 2 takes the
      // `partner hole == 0` exit of the tail (0x00428798) and one on golfer 4 the `partner hole == 0x13`
      // exit (0x004287a0); both call patronEvent 0x004266b0 and return.
      golfer: function (i) {
        return F._c3aj_golfer(i, i === 3 ? { hole: 0 } : (i === 5 ? { hole: 19 } : {}));
      },
    });
  },

  // 1b. The same first-fee path with the week-0 income word already non-zero, so the tutorial is refused at
  // 0x00427538 while the week test at 0x0042752a still falls through.
  c3aj_fee3() { return globalThis.DIFF_FIXTURES._c3aj_defaults({ week: 0, income0: 25 }); },

  // 2. Fee with every surcharge: mode 2 doubles it (0x0042747a), both magazine bits of the hole are set
  // (0x00427499, 0x004274a6), the fee bonus is 4 (0x004274ba) and the golfer types are 4, 5 and 6, so rank > 3
  // adds 2 (rank 4, golfer 3) or 5 (rank 6 and 5, golfers 2 and 4) and playSoundAt 0x0040c500 runs. Week 7
  // skips the tutorial.
  c3aj_fee2() {
    const F = globalThis.DIFF_FIXTURES;
    return F._c3aj_defaults({
      mode: 2, feeBonus: 4, holeFlags: 3, week: 7,
      golfer: function (i) { return F._c3aj_golfer(i, { type: 4 + (i % 3) }); },
    });
  },

  // 3. Flags bit 0x200000: the fee block, the match narration and the round-end block are all skipped
  // (0x0042751c, 0x004277bd, 0x00428304) and 0x4200000 takes the wrap branch at 0x00428720. Golfers 2 and 3
  // stand on hole 3, so hole 4 has par and the index is not reset; golfers 4 and 5 on hole 18, the last of the
  // course, so the index wraps to 1 (0x0042873a). Neither reaches patronEvent, because the new hole's score
  // byte is 0 (0x00428761).
  c3aj_nofee() {
    const F = globalThis.DIFF_FIXTURES;
    return F._c3aj_defaults({
      flags: 0x200000,
      golfer: function (i) {
        return F._c3aj_golfer(i, (i >= 4) ? { hole: 18, scores: { 2: 5, 3: 3 } } : {});
      },
    });
  },

  // 4. Match in progress: both partners have kflags & 0xf0 (0x004277cd, 0x004277e5), the partner is one hole
  // ahead (0x004277f9) and hole 6 of the 18-hole course has par, so the hole-winner purse blocks run and the
  // end-of-match block does not (0x00427d7b). Golfer 2 plays 6 strokes against golfer 3's 3, so on a vector of
  // golfer 2 the odd partner wins the hole (0x004278af); golfer 4 plays 3 against golfer 5's 6, so on a vector
  // of golfer 4 the even one does (0x00427b0e).
  c3aj_match_mid() {
    const F = globalThis.DIFF_FIXTURES;
    const n = (i) => ((i === 2 || i === 5) ? 6 : 3);
    return F._c3aj_defaults({
      golfer: function (i) {
        const called = (i === 2 || i === 4);
        return F._c3aj_golfer(i, {
          kflags: 0x10 | (i & 7), hole: called ? 5 : 6, partner: i ^ 1,
          scores: F._c3aj_flat(n(i)), strokes: n(i),
        });
      },
    });
  },

  // 5. End of the match with the called golfer ahead (3 strokes a hole against its partner's 6). The purse arms
  // are asymmetric in the golfer's parity, so this one fixture takes both: on golfer 2 (even, sumMine <
  // sumPartner) the course pays the purse (0x00427de2) and 0x0059b730 is decremented, and the hole purse goes
  // to the even golfer (0x00427b0e); on golfer 5 (odd, the same ordering) the course collects (0x00427f34),
  // posts event 5 and logs the tick, and the hole purse goes to the odd one (0x004278af). The standings take
  // the "trails" arm (0x00428185) with both parities of its playSound selector.
  c3aj_match_win() {
    return globalThis.DIFF_FIXTURES._c3aj_pairs((i) => ((i === 2 || i === 5 || i === 6) ? 3 : 6));
  },

  // 6. End of the match behind (5 strokes a hole against 3), the mirror of fixture 5: on golfer 2 the course
  // collects, on golfer 5 it pays, and the standings take the "leads" arm (0x00428185 jle not taken).
  c3aj_match_lose() {
    return globalThis.DIFF_FIXTURES._c3aj_pairs((i) => ((i === 2 || i === 5 || i === 6) ? 5 : 3));
  },

  // 7. End of the match level: both partners play the same number of strokes on every hole, so the sums are
  // equal, the tie arm at 0x00427ef4 runs and the standings are the "tied at" form (0x00428098). The three
  // vectors pick the three par arms against the course's par of 4 a hole (17 holes = 68): pair 2/3 plays 4 a
  // hole (even par, 0x00428110), pair 4/5 plays 3 (under par, 0x00428169) and pair 6/7 plays 5 (over par).
  c3aj_match_tie() {
    return globalThis.DIFF_FIXTURES._c3aj_pairs((i) => ((i < 4) ? 4 : (i < 6 ? 3 : 5)));
  },

  // 8. Round end with a full course-record table: every hole of the 17-hole round has a score, so allPlayed
  // stays 1 (0x004283cf) and the table 60, 62, ... is searched. Golfer 2's total of 51 lands in slot 0 and
  // shifts the rest down (0x00428437, 0x00428449); golfer 5's 85 is worse than every record, so its search
  // leaves the loop through 0x00428419 and the course-record message is refused at 0x004284e9.
  c3aj_records() {
    const F = globalThis.DIFF_FIXTURES;
    return F._c3aj_pairs((i) => (i < 4 ? 3 : 5), {
      records: [60, 62, 64, 66, 68, 70, 72, 74, 76, 78],
      // Golfer 6 plays 4 a hole and 13 on the last, a total of 77: it beats only the tenth record, so the
      // insert goes to slot 9 and the shift loop is skipped (0x0042843a).
      golfer: function (i) {
        const called = (i === 2 || i === 5 || i === 6);
        const n = (i < 4) ? 3 : (i === 6 ? 4 : 5);
        return F._c3aj_golfer(i, {
          kflags: 0xf0 | (i & 7), hole: called ? 17 : 18, partner: i ^ 1,
          scores: F._c3aj_flat(n), strokes: (i === 6) ? 13 : n,
        });
      },
    });
  },

  // 9. Round end with an empty record table: the first dword is 0, so the total is written into slot 0 through
  // the 0x00428420 arm. The per-type best (40) and average (6) are non-zero, so 0x00428658 and 0x0042866c take
  // their other sides.
  c3aj_records_empty() {
    const F = globalThis.DIFF_FIXTURES;
    return F._c3aj_pairs((i) => (i < 4 ? 3 : 5), {
      best: 40, avg: 6, typeBits: 0,
      // Golfer 6 has not played hole 2 and its satisfaction is 2: allPlayed stays 0 (0x004283cf), so the
      // record insert and the course-record message are both skipped (0x004283f9, 0x004284d8), the
      // satisfaction test at 0x00428631 takes its other side and, with every per-hole type bit 0, the
      // come-back score stays below the threshold (0x004286e5).
      golfer: function (i) {
        const called = (i === 2 || i === 5 || i === 6);
        const sc = F._c3aj_flat(i < 4 ? 3 : 5);
        if (i === 6) sc[2] = 0;
        return F._c3aj_golfer(i, {
          kflags: 0xf0 | (i & 7), hole: called ? 17 : 18, partner: i ^ 1,
          scores: sc, strokes: (i < 4 ? 3 : 5), sat: (i === 6) ? 2 : 8 + i,
        });
      },
    });
  },

  // 10. The course-record message. Its guard at 0x004284e5 reads the first record dword AFTER the insert loop
  // has written this round's total into the table, so `total < 0x0056a524` can only hold when the total is 0:
  // every golfer scores 1 on holes 1..16 and -16 on hole 17 (the score array at 0x005794db is signed), so every
  // byte is non-zero, allPlayed stays 1 and the 17 holes played sum to 0 with an empty record table. This is
  // the only input that reaches 0x00428505..0x00428613.
  c3aj_record_msg() {
    const F = globalThis.DIFF_FIXTURES;
    const alt = {};
    for (let h = 1; h <= 18; h++) alt[h] = 1;
    alt[17] = -16;
    return F._c3aj_defaults({
      lastHole: 17,
      golfer: function (i) {
        const called = (i === 2 || i === 5 || i === 6);
        return F._c3aj_golfer(i, {
          kflags: 0xf0 | (i & 7), hole: called ? 17 : 18, partner: i ^ 1, scores: alt, strokes: -16,
          dc: (i === 6) ? 0 : 1,
        });
      },
    });
  },

  // 11. The VIP line and the commissioner decay: kind 0x40 (county commissioner) for the even golfers and 0x60
  // (corporate CEO) for the odd ones, standing on hole 16 of a 17-hole course, so the tail moves them onto hole
  // 17 (which has par, 0x00428780) and the line at 0x004287a6 is built because hole 18 has none. The kind also
  // selects the second satisfaction decay at 0x00428986 and skips the histogram block at 0x00427390. The two
  // name pointers 0x004c2c10 / 0x004c2c18 that buildGolferName 0x004676e0 reads for these kinds are pointed at
  // a string constant of the image so the fixture never depends on save state.
  c3aj_vip() {
    const F = globalThis.DIFF_FIXTURES;
    F._c3aj_rw('0x004c2c10', 0x10);
    ptr('0x004c2c10').writePointer(ptr('0x004c52b8'));
    ptr('0x004c2c18').writePointer(ptr('0x004c52b8'));
    return F._c3aj_defaults({
      lastHole: 17,
      golfer: function (i) {
        return F._c3aj_golfer(i, { kind: (i === 4) ? 0 : ((i & 1) ? 0x60 : 0x40), hole: 16, sat: 3 + i });
      },
    });
  },

  // 12. The come-back flag: the golfer types are 0 and 8, whose rank byte is 0, so the threshold 1 << 0 is 1 and
  // the score bonus 4 clears both 0x004286e5 and 0x004286eb; the record gets bit 0x80000000 and the word -8
  // (0x004286f8, 0x004286fe). Difficulty 0 also takes the "no difficulty" arm of 0x0042869a, and golfer 6 has
  // bit 0x100000 cleared so the rolling average is overwritten at 0x004288e3 instead of stored at 0x004288d3.
  c3aj_comeback() {
    const F = globalThis.DIFF_FIXTURES;
    return F._c3aj_pairs(() => 4, {
      diff: 0, scoreBonus: 4, week: 0, best: 90, avg: 2, typeBits: 0,
      golfer: function (i) {
        const called = (i === 2 || i === 5 || i === 6);
        return F._c3aj_golfer(i, {
          kflags: 0xf0 | (i & 7), hole: called ? 17 : 18, partner: i ^ 1,
          scores: F._c3aj_flat(4), strokes: 4, type: 8 * (i & 1), sat: 40,
          c8: (i === 6) ? 0 : 0x100000,
        });
      },
    });
  },

  // 13. patronEvent through the round-end fast path: the golfers stand on hole 18 of an 18-hole course, so the
  // round-end block runs through its `hole == 0x12` arm (0x00428312) and the tail calls patronEvent and returns
  // (0x004289cb). The partner is on the same hole, so the match block is skipped at 0x004277f9.
  c3aj_patron() {
    const F = globalThis.DIFF_FIXTURES;
    return F._c3aj_defaults({
      // Golfer 3 has no hole index, so a vector on golfer 2 enters the narration through the
      // `partner hole == 0` side of 0x004277fd and a vector on golfer 3 runs it with no hole played at all
      // (the sum loop is skipped at 0x0042781f). Golfer 5 is a kind-0x20 golfer, so its vector skips the
      // histogram block (0x00427398) and the come-back flag (0x004286cf).
      golfer: function (i) {
        return F._c3aj_golfer(i, {
          kflags: 0xf0 | (i & 7), hole: (i === 3) ? 0 : 18, partner: i ^ 1,
          scores: F._c3aj_flat(4), strokes: 4, kind: (i === 5) ? 0x20 : 0,
        });
      },
    });
  },

  // 13b. The narration with a partner that is not in a match and with an odd golfer that is not: on golfer 6
  // the partner field points at golfer 7, whose kflags have no high nibble, so the whole narration is skipped
  // at 0x004277e5; on golfer 4 the partner field points at golfer 3 (in a match) while the odd golfer 5 is
  // not, so the narration runs but the purse arms are skipped at 0x00427ddc; on golfer 2 the pair is normal
  // and one stroke apart, so the standings end in the singular form (0x004282b6).
  c3aj_match_nopair() {
    const F = globalThis.DIFF_FIXTURES;
    return F._c3aj_defaults({
      lastHole: 17,
      golfer: function (i) {
        const called = (i === 2 || i === 4 || i === 6);
        const noMatch = (i === 5 || i === 7);
        return F._c3aj_golfer(i, {
          kflags: (noMatch ? 0x00 : 0xf0) | (i & 7), hole: called ? 17 : 18,
          partner: (i === 4) ? 3 : (i ^ 1),
          scores: F._c3aj_flat(4), strokes: (i === 2) ? 5 : 4,
        });
      },
    });
  },

  // 14. patronEvent through the wrap branch: the flags bit 0x200000 takes 0x00428720, and the new hole already
  // has a score, so patronEvent is called at 0x00428768 and the tail continues instead of returning.
  c3aj_patron_wrap() {
    const F = globalThis.DIFF_FIXTURES;
    return F._c3aj_defaults({
      flags: 0x200000,
      golfer: function (i) { return F._c3aj_golfer(i, { scores: { 1: 4, 2: 5, 3: 3, 4: 6 } }); },
    });
  },
});
