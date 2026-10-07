// Fixtures for C3 batch c3ac (holeMagazineEvent 0x0042dea0). Only our own synthetic values (no game text). Names are
// prefixed c3ac_. Each fixture seeds the whole hole-table state region [0x00575ab0, +0x2698) to zero, then 18 holes,
// the membership type table 0x005849e0, the placed-object table 0x0058bcb8, the difficulty / notice / force globals
// and the startMessage preconditions, so every read of holeMagazineEvent is deterministic and both A/B arms draw the
// same Random numbers (the RNG seed 0x00822d9c is a state region). A hole record field is at fieldBase + hole*0x208;
// the shot histogram at 0x00575ada + hole*0x208 holds signed shorts, element[row][col] at +row*2 + col*22 (row is the
// 0-based element index; the function weights it by row+1).
Object.assign(globalThis.DIFF_FIXTURES, {
  _c3ac_setHole(h, o) {
    const at = (base) => ptr(base).add(h * 0x208);
    at('0x00575ab0').writeS8(o.par | 0);        // par byte (0 skips the hole)
    at('0x00575ab4').writeS16(o.yards | 0);     // yards word
    at('0x00575ad0').writeS32(o.ad0 | 0);       // fun-rating denominator term; also >7 test in the 575cac block
    at('0x00575ad4').writeS32(o.ad4 | 0);
    at('0x00575c08').writeS16(o.c08 | 0);       // fun-rating numerator word
    at('0x00575cb0').writeU32((o.flags | 0) >>> 0);  // per-hole flags (bit0 rated, 0x4/0x8 hard/easy, 0x700 notable)
    at('0x00575aa8').writeU32((o.aa8 | 0) >>> 0);    // xored with flags & 0x60 in the 575cac block
    at('0x00575be2').writeS16(o.be2 | 0);
    at('0x00575be4').writeS16(o.be4 | 0);
    at('0x005758a8').writeS8(o.s8a8 | 0);       // compared with par in the 575cac block
    at('0x00575ca4').writeS32(o.ca4 | 0);       // hole rating (>200 / >400 gate the two magazine events)
    const c = o.coords || [0, 0, 0, 0, 0, 0, 0, 0];
    at('0x005758c0').writeS32(c[0]); at('0x005758b0').writeS32(c[1]);   // tee/pin dx,dy for angle a1
    at('0x005758c4').writeS32(c[2]); at('0x005758b4').writeS32(c[3]);
    at('0x00575ac8').writeS32(c[4]); at('0x00575ab8').writeS32(c[5]);   // dx,dy for angle a2; *0x400+0x200 -> postEvent
    at('0x00575acc').writeS32(c[6]); at('0x00575abc').writeS32(c[7]);
    const hb = ptr('0x00575ada').add(h * 0x208);
    (o.shots || []).forEach(function (s) { hb.add(s[0] * 2 + s[1] * 22).writeS16(s[2]); });
  },

  // Generic hole: balanced histogram (all four scored columns equal -> every skill delta 0, notable 0, no magazine),
  // so a generic hole never calls startMessage first. ad0/aa8/be2/s8a8/coords vary with the index to reach both sides
  // of the 575cac-block tests.
  _c3ac_generic(h) {
    return {
      par: 3 + (h % 3), yards: 300 + h * 7,
      ad0: (h % 2) ? 10 : 0, ad4: h * 2, c08: h,
      flags: 0, aa8: (h % 2) ? 0x60 : 0, be2: (h % 2) ? 0 : 1, be4: 0,
      s8a8: (h % 4) ? (3 + (h % 3)) : (3 + (h % 3) + 1), ca4: 40 + h * 3,
      coords: [10, 0, 0, 0, 10, 0, 0, 0],
      shots: [[4, 6, 5], [4, 5, 5], [4, 3, 5], [4, 7, 5]],
    };
  },

  // notable-7, big-skill hole used as the magazine firer: length column 6 heavy at row index 8 (weight 9), column 7
  // light at row index 0 (weight 1); histogram total 20 and weighted 100 keep it out of the too-hard/too-easy bands,
  // so flags stay clear. Length delta ~444, accuracy/imagination ~167 -> notable 7, skill delta ~778.
  _c3ac_firer(ca4, flags) {
    return {
      par: 4, yards: 450, ad0: 10, ad4: 8, c08: 7,
      flags: flags, aa8: 0, be2: 0, be4: 0, s8a8: 4, ca4: ca4,
      coords: [10, 0, 0, 0, 10, 0, 0, 0],
      shots: [[8, 6, 10], [0, 7, 10]],
    };
  },

  _c3ac_base(g) {
    // Zero the hole-table state region so unset fields read 0.
    const ht = ptr('0x00575ab0');
    for (let i = 0; i < 0x2698; i += 4) ht.add(i).writeU32(0);

    // 18 holes (1..18). Hole 1 is balanced (notable 0, low rating) so nothing fires before hole 2. Hole 2 is the
    // firer (config in g.firer). Hole 5 has par 0 (skipped). Hole 9 has column 7 summing to -8 so col40[7] == 0 and
    // the skill block is skipped. The rest are generic.
    for (let h = 1; h <= 18; h++) {
      let o;
      if (h === 5) o = { par: 0 };
      else if (h === 1) o = {
        par: 4, yards: 400, ad0: 10, ad4: 4, c08: 3, flags: 0, aa8: 0, be2: 0, be4: 0, s8a8: 4, ca4: 50,
        coords: [10, 0, 0, 0, 10, 0, 0, 0], shots: [[4, 6, 5], [4, 5, 5], [4, 3, 5], [4, 7, 5]],
      };
      else if (h === 2) o = globalThis.DIFF_FIXTURES._c3ac_firer(g.firer.ca4, g.firer.flags);
      // Hole 3: too hard. par 3, 16 shots all at row index 8 (weight 9) -> total 16 (>= 10), weighted 144 > (par+2)*16,
      // so flag bit 0x4 is set (0x0042e085). Columns balanced -> every skill delta 0, notable 0, rating low.
      else if (h === 3) o = {
        par: 3, yards: 320, ad0: 10, ad4: 4, c08: 2, flags: 0, aa8: 0, be2: 0, be4: 0, s8a8: 3, ca4: 60,
        coords: [10, 0, 0, 0, 10, 0, 0, 0],
        shots: [[8, 0, 2], [8, 1, 2], [8, 2, 2], [8, 3, 2], [8, 4, 2], [8, 5, 2], [8, 6, 2], [8, 7, 2]],
      };
      // Hole 4: too easy. par 5, 16 shots all at row index 0 (weight 1) -> weighted 16 < par*16 - 16/2, so flag bit
      // 0x8 is set (0x0042e0b5). Columns balanced -> notable 0, rating low.
      else if (h === 4) o = {
        par: 5, yards: 520, ad0: 0, ad4: 2, c08: 1, flags: 0, aa8: 0x60, be2: 1, be4: 0, s8a8: 6, ca4: 55,
        coords: [10, 0, 0, 0, 0, 10, 0, 0],
        shots: [[0, 0, 2], [0, 1, 2], [0, 2, 2], [0, 3, 2], [0, 4, 2], [0, 5, 2], [0, 6, 2], [0, 7, 2]],
      };
      else if (h === 9) o = {
        par: 3, yards: 330, ad0: 0, ad4: 0, c08: 0, flags: 0, aa8: 0x60, be2: 1, be4: 0, s8a8: 9, ca4: 60,
        coords: [5, 0, 0, 5, 0, 5, 5, 0], shots: [[0, 7, -4], [1, 7, -4]],
      };
      else o = globalThis.DIFF_FIXTURES._c3ac_generic(h);
      globalThis.DIFF_FIXTURES._c3ac_setHole(h, o);
    }

    // Membership type table 0x005849e0, 76 entries of 0x2c: type byte at +2 (type&7 > 2 counts), ban byte at +0x29
    // (0xff = banned). A mix so both sides of the member loop run.
    const mt = ptr('0x005849e0');
    for (let i = 0; i < 76; i++) {
      Memory.protect(mt.add(i * 0x2c), 0x2c, 'rw-');
      mt.add(i * 0x2c + 2).writeU8(i % 8);
      mt.add(i * 0x2c + 0x29).writeU8(i % 5 === 0 ? 0xff : 0);
    }
    // Placed objects 0x0058bcb8, 256 entries of 0x10: word 0 == 5 for some (decrements the member count).
    const po = ptr('0x0058bcb8');
    for (let i = 0; i < 256; i++) {
      Memory.protect(po.add(i * 0x10), 2, 'rw-');
      po.add(i * 0x10).writeS16(i % 7 === 0 ? 5 : 1);
    }

    // Difficulty / force / notice globals and outputs.
    ptr('0x00822c88').writeS32(g.diff | 0);                 // 0, 1 or 2
    ptr('0x0056c7b4').writeU32((g.c7b4 | 0) >>> 0);         // bit 1 forces the hole-type notice
    ptr('0x005685f8').writeU32((g.e5f8 | 0) >>> 0);         // notice-fired mask (bit 1<<notable)
    ptr('0x004c2850').writeS32(0);                          // magazine counter
    ptr('0x0056d1b0').writeS32(0);                          // member-count scratch (output)

    // startMessage preconditions: idle unless g.busy (direction byte set -> every startMessage refuses with prio<=0).
    ptr('0x00569498').writeU8(g.busy ? 1 : 0);
    ptr('0x0053df54').writeS32(0);
    ptr('0x00567afc').writeS32(0);
    ptr('0x005694a4').writeS32(0);
    ptr('0x00822d9c').writeS32(0x1234);                     // RNG seed (state region, restored per vector)

    // logTick (0x00568600 word log) / postEvent read the tick dword 0x00834170 and course dword 0x0059bf90.
    ptr('0x00834170').writeS32(0x2468);
    ptr('0x0059bf90').writeS32(1);
    ptr('0x0051a068').writeU8(0);                           // empty the text buffer

    return { obj: ht };
  },

  // Magazine event 1 (best-100) firer: hole 2 rating 300, flags 0 -> block 1 fires; its notice bit (1<<7) is pre-set
  // in 0x005685f8 so the notice is skipped and block 1's startMessage is the first (idle) one and succeeds.
  c3ac_mag1() { return globalThis.DIFF_FIXTURES._c3ac_base({ diff: 0, c7b4: 0, e5f8: 0x80, busy: 0, firer: { ca4: 300, flags: 0 } }); },
  // Magazine event 2 (Top-18) firer: hole 2 rating 500, flags bit0 set -> block 1 blocked, block 2 fires first.
  c3ac_mag2() { return globalThis.DIFF_FIXTURES._c3ac_base({ diff: 0, c7b4: 0, e5f8: 0x80, busy: 0, firer: { ca4: 500, flags: 1 } }); },
  // Busy: direction byte 1, notice mask 0 -> every startMessage (notice and both magazine blocks) refuses.
  c3ac_busy() { return globalThis.DIFF_FIXTURES._c3ac_base({ diff: 0, c7b4: 0, e5f8: 0, busy: 1, firer: { ca4: 500, flags: 0 } }); },
  // Difficulty 2: raises the notable / too-hard thresholds and turns off the difficulty<2 overrides.
  c3ac_diff2() { return globalThis.DIFF_FIXTURES._c3ac_base({ diff: 2, c7b4: 0, e5f8: 0x80, busy: 0, firer: { ca4: 300, flags: 0 } }); },
  // Force: 0x0056c7b4 bit 1 set -> the hole-type notice is forced for hole 1 (notable := 4, hole index reset to 1).
  c3ac_force() { return globalThis.DIFF_FIXTURES._c3ac_base({ diff: 0, c7b4: 2, e5f8: 0, busy: 0, firer: { ca4: 300, flags: 0 } }); },
});
