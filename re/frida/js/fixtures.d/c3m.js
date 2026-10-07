// C3 batch c3m fixtures (2026-10-07). Main-menu state only; every value written is synthetic (our own ASCII in the
// text and name buffers, small integers elsewhere). The text buffer 0x0051a068 is a state region of every c3m entry.
Object.assign(globalThis.DIFF_FIXTURES, {
  // The menu keeps loading for a few seconds after the window appears, and meanwhile it draws from the RNG seed
  // 0x00822d9c and writes the text buffer 0x0051a068, both state regions here (observed 2026-10-07: seed and buffer
  // changed for ~2 s, then stayed constant). Once per boot, wait (Frida's JS thread, not the game's) until both are
  // unchanged for 1.5 s, at most 15 s.
  _c3m_settle() {
    if (globalThis.__c3mSettled) return;
    const read = () => ptr('0x00822d9c').readU32() + ':' + ptr('0x0051a068').readCString(32);
    let last = read(), stable = 0;
    for (let i = 0; i < 60 && stable < 6; i++) {
      Thread.sleep(0.25);
      const now = read();
      stable = now === last ? stable + 1 : 0;
      last = now;
    }
    globalThis.__c3mSettled = true;
  },
  // ---------------------------------------------------------------------------------------- buildGolferName
  // Golfer records 0x100 bytes apart: kind byte 0x005794d0 + g*0x100, name-slot byte 0x00579573 + g*0x100, type word
  // 0x0057956e + g*0x100. Name slots 0x0058dd50 + k*0x38 (k = 0..3) get synthetic names. Golfers 40..63 cover kind
  // 0x20 (sub-kind 4 / other), 0x40, 0x60 with sub-kinds 0..9 and 31, 0x80 (odd and even golfer), 0x00/0xa0/0xc0/0xe0
  // (golfer-type name). The suffix global 0x0053a450 is set per variant.
  _c3m_names(suffix) {
    globalThis.DIFF_FIXTURES._c3m_settle();
    ptr('0x0051a068').writeUtf8String('c3m:');
    for (let k = 0; k < 4; k++) ptr('0x0058dd50').add(k * 0x38).writeUtf8String('c3mPro' + k);
    const rows = [
      [40, 0x24, 0, 0], [41, 0x21, 1, 0], [42, 0x20, 3, 0], [43, 0x2f, 0, 0],
      [44, 0x40, 0, 0], [45, 0x45, 0, 0],
      [46, 0x60, 0, 0], [47, 0x61, 0, 0], [48, 0x62, 0, 0], [49, 0x63, 0, 0], [50, 0x64, 0, 0], [51, 0x65, 0, 0],
      [52, 0x66, 0, 0], [53, 0x67, 0, 0], [54, 0x68, 0, 0], [55, 0x69, 0, 0], [56, 0x7f, 0, 0],
      [57, 0x80, 0, 0], [58, 0x80, 0, 0], [59, 0x9f, 0, 0],
      [60, 0x00, 0, 5], [61, 0xa0, 0, 60], [62, 0xc0, 0, 0], [63, 0xe0, 0, 75],
    ];
    for (const [g, kind, slot, type] of rows) {
      const b = ptr('0x005794d0').add(g * 0x100);
      b.writeU8(kind);
      ptr('0x00579573').add(g * 0x100).writeU8(slot);
      ptr('0x0057956e').add(g * 0x100).writeS16(type);
    }
    ptr('0x0053a450').writeU32(suffix);
    return {};
  },
  c3m_names_s0() { return globalThis.DIFF_FIXTURES._c3m_names(0x80); },   // & 0x7f = 0: no suffix
  c3m_names_s5() { return globalThis.DIFF_FIXTURES._c3m_names(0x05); },   // suffix 5
  c3m_names_s9() { return globalThis.DIFF_FIXTURES._c3m_names(0x09); },   // above 8: default suffix

  // ----------------------------------------------------------------------------------------- decorationName
  // Category bytes 0x00578376 + t*0x30 for types 0..0x24 (0 unless listed); tile type bytes 0x005722e8, object bytes
  // 0x0056988c, flag words 0x0053caf0 (cell = x*50 + y) on cells in columns 20..24; placed objects 0xf0..0xf5 at
  // 0x0058bcb8 + o*16 (kind word +0, x +2, y +4, landmark id dword +8). Course 0 (dword 0x0059bf90) with record byte
  // 0x00571ff4 and the course type byte 0x005a34e0 set per variant.
  _c3m_deco(ct, rec) {
    globalThis.DIFF_FIXTURES._c3m_settle();
    ptr('0x0051a068').writeUtf8String('c3m:');
    const cat = {4: 4, 0xa: 4, 0xc: 4, 0x1a: 4, 0xd: 0xd, 0xe: 0xd, 0xf: 0xd, 0x10: 0xd, 0x1b: 0xd, 0x11: 0x11,
                 0x13: 0x11, 7: 7, 8: 0xc, 9: 0x12, 0x14: 0x13, 0x1c: 0x15, 0x1d: 0x16, 0x1e: 5, 0x1f: 0, 0x20: 0x17,
                 0x21: -1};
    for (let t = 0; t <= 0x24; t++) ptr('0x00578376').add(t * 0x30).writeS8(cat[t] === undefined ? 0 : cat[t]);
    const setCell = (x, y, type, obj, flags) => {
      const c = x * 50 + y;
      ptr('0x005722e8').add(c).writeU8(type);
      ptr('0x0056988c').add(c).writeU8(obj);
      ptr('0x0053caf0').add(c * 2).writeU16(flags);
    };
    const setObj = (o, kind, x, y, id) => {
      const p = ptr('0x0058bcb8').add(o * 16);
      p.writeS16(kind); p.add(2).writeS16(x); p.add(4).writeS16(y); p.add(8).writeS32(id);
    };
    setObj(0xf0, 4, 20, 0, 3); setObj(0xf1, 2, 20, 1, 0); setObj(0xf2, 0, 20, 2, 0);
    setObj(0xf3, 4, 20, 4, 9); setObj(0xf4, 7, 21, 0, 0); setObj(0xf5, 1, 22, 5, 0);
    setCell(20, 0, 0x16, 0xf0, 0); setCell(20, 1, 0x15, 0xf1, 0); setCell(20, 2, 0x16, 0xf2, 0);
    setCell(20, 3, 0x15, 0xf2, 0); setCell(20, 4, 0x16, 0xf3, 0);
    setCell(21, 0, 7, 0, 0);
    [5, 11, 17, 23, 29].forEach((v, i) => setCell(22, i, 4, v, 0x1000));   // v % 5 = 0..4
    setCell(22, 5, 4, 44, 0x1020);
    setCell(23, 0, 4, 0, 0);
    setCell(24, 0, 0x11, 0, 0x20); setCell(24, 1, 0x11, 3, 0); setCell(24, 2, 0x11, 0, 0); setCell(24, 3, 0x1f, 2, 0);
    ptr('0x0059bf90').writeS32(0);
    ptr('0x00571ff4').writeU8(rec);
    ptr('0x005a34e0').writeS8(ct);
    return {};
  },
  c3m_deco_ct0() { return globalThis.DIFF_FIXTURES._c3m_deco(0, 0x0d); },
  c3m_deco_ct1() { return globalThis.DIFF_FIXTURES._c3m_deco(1, 0x0a); },
  c3m_deco_ct1b() { return globalThis.DIFF_FIXTURES._c3m_deco(1, 0x0d); },
  c3m_deco_ct2() { return globalThis.DIFF_FIXTURES._c3m_deco(2, 0x00); },
  c3m_deco_ct3() { return globalThis.DIFF_FIXTURES._c3m_deco(3, 0x0d); },
  c3m_deco_ct4() { return globalThis.DIFF_FIXTURES._c3m_deco(4, 0x0a); },

  // --------------------------------------------------------------------------------------- announceHoleType
  // Hole records 0x208 apart: tile x dword 0x00575ac8 + h*0x208, y dword 0x00575acc + h*0x208 for holes 0..3. Mode
  // dword 0x00822c88 per variant. postEvent 0x0046e7b0 posts only when bit 0x4000000 of 0x0059e7b8 is clear (cleared
  // here) and the kind's slot dword 0x004c15a0 + kind*0x30 is 0 (zeroed for kinds 0/1/4/9, or non-zero for _busy);
  // the date dword 0x00834170 it stores = 0x2a3b.
  _c3m_holes(mode, busy) {
    globalThis.DIFF_FIXTURES._c3m_settle();
    ptr('0x0051a068').writeUtf8String('c3m:');
    for (let h = 0; h < 4; h++) {
      ptr('0x00575ac8').add(h * 0x208).writeS32(3 + h * 5);
      ptr('0x00575acc').add(h * 0x208).writeS32(40 - h * 3);
    }
    ptr('0x00822c88').writeS32(mode);
    const fl = ptr('0x0059e7b8');
    fl.writeU32((fl.readU32() & ~0x4000000) >>> 0);
    for (const k of [0, 1, 4, 9]) ptr('0x004c15a0').add(k * 0x30).writeU32(busy ? 0x100 + k : 0);
    ptr('0x00834170').writeS32(0x2a3b);
    return {};
  },
  c3m_holes_m2() { return globalThis.DIFF_FIXTURES._c3m_holes(2, false); },
  c3m_holes_m1() { return globalThis.DIFF_FIXTURES._c3m_holes(1, false); },
  c3m_holes_busy() { return globalThis.DIFF_FIXTURES._c3m_holes(2, true); },

  // --------------------------------------------------------------------------------------- updateMembership
  // Writes: slot counter 0x0059ae7c; spawn tile 0x00578150/0x00578154 = 7/9; byte 0x00575cb9 = 0x5a; bit 0x40 of
  // 0x0059e7b8; category counts 0x0059dea0 (4 rows of 8 dwords); RNG seed 0x00822d9c; golfer-type records
  // 0x004d6098 + t*0x230 for t = 1..75: flags byte +0x10 = [2, 1, 4, 5, 0x10, 8, 0x20][t % 7] (2 / 0x20 / 0x10..:
  // every combination of the & 0x11 / & 0xc tests), bit 7 of +0x11 set when t % 6 == 0 (typeBit7Clear 0), dword +0x1c
  // = 0x18 + t when t % 9 == 0 (the category override), else 0; type table 0x005849e0 + t*0x2c: member byte +2 and
  // banned byte +0x29 per variant; status byte 0x005794d9 / type word 0x0057956e of every golfer (0 / 0, then the
  // variant's conflicting golfers); mode dwords 0x00822c88, 0x00543cc4, 0x00543ccc; date dword 0x00834170 = 0x2a3b.
  _c3m_mem(o) {
    globalThis.DIFF_FIXTURES._c3m_settle();
    ptr('0x0051a068').writeUtf8String('c3m:');
    ptr('0x0059ae7c').writeS32(o.counter);
    ptr('0x00578150').writeS32(7);
    ptr('0x00578154').writeS32(9);
    ptr('0x00575cb9').writeU8(0x5a);
    const fl = ptr('0x0059e7b8');
    fl.writeU32(o.all ? (fl.readU32() | 0x40) >>> 0 : (fl.readU32() & ~0x40) >>> 0);
    for (let i = 0; i < 32; i++) ptr('0x0059dea0').add(i * 4).writeS32(o.counts[i]);
    ptr('0x00822d9c').writeU32(o.seed >>> 0);
    for (let t = 1; t <= 75; t++) {
      const rec = ptr('0x004d6098').add(t * 0x230);
      Memory.protect(rec.add(0x10), 0x10, 'rw-');
      rec.add(0x10).writeU8([2, 1, 4, 5, 0x10, 8, 0x20][t % 7]);
      const b = rec.add(0x11).readU8();
      rec.add(0x11).writeU8(t % 6 === 0 ? (b | 0x80) : (b & 0x7f));
      rec.add(0x1c).writeU32(t % 9 === 0 ? 0x18 + t : 0);
    }
    for (let t = 0; t <= 76; t++) {
      ptr('0x005849e2').add(t * 0x2c).writeU8(o.member(t) ? 1 : 0);
      ptr('0x00584a09').add(t * 0x2c).writeU8(o.banned(t) ? 0xff : 0);
    }
    for (let j = 0; j < 0x98; j++) {
      ptr('0x005794d9').add(j * 0x100).writeU8(0);
      ptr('0x0057956e').add(j * 0x100).writeS16(0);
    }
    for (const [j, status, type] of o.golfers) {
      ptr('0x005794d9').add(j * 0x100).writeU8(status);
      ptr('0x0057956e').add(j * 0x100).writeS16(type);
    }
    ptr('0x00822c88').writeS32(o.mode);
    ptr('0x00543cc4').writeS32(o.cc4);
    ptr('0x00543ccc').writeS32(o.ccc);
    ptr('0x00834170').writeS32(0x2a3b);
    return {};
  },
  // rows: [ties among the eligible c in {3,5,6,7}], [one minimum], [all equal], [descending]
  _c3m_counts: [5, 2, 7, 3, 1, 3, 9, 3, 0, 4, 4, 8, 0, 8, 8, 2, 1, 1, 1, 1, 1, 1, 1, 1, 6, 5, 4, 3, 2, 1, 0, 7],
  // a few golfers in the way: status 0xff with types 5 and 30 (t % 19 = 5, 11), positive status with types 7 and 40,
  // status 0x80 (negative: ignored) with type 8, status 0 with type 9
  _c3m_few: [[20, 0xff, 5], [21, 0xff, 30], [22, 2, 7], [23, 1, 40], [24, 0x80, 8], [25, 0, 9]],
  c3m_mem_a() {
    const F = globalThis.DIFF_FIXTURES;
    return F._c3m_mem({counter: 5, all: false, counts: F._c3m_counts, seed: 0x12345678, member: (t) => t % 5 !== 0,
                       banned: () => false, golfers: F._c3m_few, mode: 0, cc4: 0, ccc: 0});
  },
  c3m_mem_all() {   // every category eligible; slot 0x97 (odd); mode 2, 0x00543cc4 = 3, 0x00543ccc = 1
    const F = globalThis.DIFF_FIXTURES;
    return F._c3m_mem({counter: 0x98 * 3 + 0x97, all: true, counts: F._c3m_counts, seed: 0x0badf00d,
                       member: (t) => t % 4 !== 2, banned: (t) => t % 11 === 3, golfers: F._c3m_few, mode: 2,
                       cc4: 3, ccc: 1});
  },
  c3m_mem_conf() {  // status-0xff golfers on 13 residues mod 19: most draws are refused before one is accepted
    const F = globalThis.DIFF_FIXTURES;
    const golfers = F._c3m_few.slice();
    for (let k = 0; k < 13; k++) golfers.push([30 + k, 0xff, k === 5 ? 24 : k]);
    return F._c3m_mem({counter: 0x98 + 10, all: false, counts: F._c3m_counts, seed: 0xdeadbeef,
                       member: (t) => t % 3 !== 0, banned: (t) => t % 4 === 1, golfers, mode: 1, cc4: 0, ccc: 1});
  },
  c3m_mem_fail() {  // every type banned: 999 refusals, the decline message and the counter rolled back
    const F = globalThis.DIFF_FIXTURES;
    return F._c3m_mem({counter: 0x98 * 2 + 7, all: false, counts: F._c3m_counts, seed: 1, member: () => true,
                       banned: () => true, golfers: F._c3m_few, mode: 0, cc4: 0, ccc: 0});
  },
  c3m_mem_big() {   // counts 9999 / 10000: the tie count then starts from the argument (ebx = membersOnly)
    const F = globalThis.DIFF_FIXTURES;
    const counts = [];
    for (let i = 0; i < 32; i++) counts.push(i % 3 === 0 ? 10000 : 9999);
    return F._c3m_mem({counter: 33, all: true, counts, seed: 0x41c64e6d, member: (t) => t % 2 === 0,
                       banned: () => false, golfers: [], mode: 0, cc4: 5, ccc: 0});
  },
});
