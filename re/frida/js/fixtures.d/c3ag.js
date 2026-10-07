// C3 batch c3ag fixtures (2026-10-08): golfer/course event builders. Main-menu state only. Every value written here
// is synthetic (our own ASCII in the name slots and in the template text, small integers elsewhere); the two token
// strings and the notice fragments are never reproduced, they are read back from the binary at their own addresses.
// The shared text buffer 0x0051a068 is a state region of every c3ag entry.
Object.assign(globalThis.DIFF_FIXTURES, {
  // The menu keeps loading for several seconds after the window appears, and meanwhile it writes the RNG seed
  // 0x00822d9c, the text buffer 0x0051a068, the __itoa scratch 0x0058a528 and the message-ticker globals, all of
  // them state regions here. With only the seed and the buffer watched, the first key of a boot still read RED on
  // one or two vectors about half the time (observed 2026-10-08); watching every volatile state address and
  // requiring 3 s of quiet removed it. Once per boot, at most 30 s.
  _c3ag_settle() {
    if (globalThis.__c3agSettled) return;
    const watch = ['0x00822d9c', '0x0053df54', '0x005a34ec', '0x005a7144', '0x004c2e08', '0x005694a4',
                   '0x0056d1a8', '0x0056d1ac', '0x00571fd4', '0x0059aaf8', '0x005a9ccc', '0x00567a1c',
                   '0x00543cfc', '0x00822c70', '0x00572cac', '0x0053a450', '0x00567afc'];
    const read = () => watch.map((a) => ptr(a).readU32()).join(',') + '|' +
                       ptr('0x00569498').readU8() + '|' + ptr('0x0051a068').readCString(64) + '|' +
                       ptr('0x0058a528').readCString(24) + '|' + ptr('0x005a6d40').readCString(64);
    let last = read(), stable = 0;
    for (let i = 0; i < 120 && stable < 12; i++) {
      Thread.sleep(0.25);
      const now = read();
      stable = now === last ? stable + 1 : 0;
      last = now;
    }
    globalThis.__c3agSettled = true;
  },

  // ------------------------------------------------------------------------------ substituteStoryNames 0x0045c460
  // Golfer records are 0x100 bytes apart. buildGolferName 0x004676e0 (called for g and for g ^ 1) reads the kind byte
  // 0x005794d0 + g*0x100, the name-slot byte 0x00579573 + g*0x100, the type word 0x0057956e + g*0x100 and the suffix
  // dword 0x0053a450. Golfers 40..57 are seeded in consecutive pairs so that both g and g ^ 1 of every vector are
  // defined, and the pairs cover every form of the name: kind 0x20 sub-kind 4 and other sub-kinds, kind 0x40,
  // kind 0x60 with sub-kinds 0 / 3 / above 8, kind 0x80 for an even and an odd golfer, and the 0x00/0xa0/0xc0/0xe0
  // default form. `tpl` is a callback that builds the text already in the buffer out of the two token strings.
  _c3ag_story(tpl) {
    globalThis.DIFF_FIXTURES._c3ag_settle();
    for (let k = 0; k < 6; k++) ptr('0x0058dd50').add(k * 0x38).writeUtf8String('c3agName' + k);
    const rows = [
      [40, 0x24, 0, 0], [41, 0x21, 1, 0],
      [42, 0x40, 0, 0], [43, 0x60, 0, 0],
      [44, 0x63, 0, 0], [45, 0x6a, 0, 0],
      [46, 0x80, 0, 0], [47, 0x80, 0, 0],
      [48, 0x00, 0, 1], [49, 0xa0, 0, 2],
      [50, 0xc0, 0, 3], [51, 0xe0, 0, 0],
      [52, 0x2f, 2, 0], [53, 0x25, 3, 0],
      [54, 0x41, 0, 0], [55, 0x61, 0, 0],
      [56, 0x20, 4, 0], [57, 0x24, 5, 0],
    ];
    for (const [g, kind, slot, type] of rows) {
      ptr('0x005794d0').add(g * 0x100).writeU8(kind);
      ptr('0x00579573').add(g * 0x100).writeU8(slot);
      ptr('0x0057956e').add(g * 0x100).writeS16(type);
    }
    ptr('0x0053a450').writeU32(0x80);   // & 0x7f == 0: the kind-0x40 form adds no numeral suffix

    // The two tokens replaceInText 0x0045b7c0 looks for, read from the binary (0x004d395c is pushed at 0x0045c52d,
    // 0x004d3954 at 0x0045c541). Never written as literals here.
    const tokSelf = ptr('0x004d395c').readCString();
    const tokPartner = ptr('0x004d3954').readCString();
    ptr('0x0051a068').writeUtf8String(tpl(tokSelf, tokPartner));
    return {};
  },

  // Both tokens present, self first: both replaceInText calls find their key.
  c3ag_story_both() {
    return globalThis.DIFF_FIXTURES._c3ag_story((s, p) => 'c3ag[' + s + ']-and-[' + p + ']-end');
  },
  // Tokens in the other order: the second call's key sits before the first call's replacement.
  c3ag_story_rev() {
    return globalThis.DIFF_FIXTURES._c3ag_story((s, p) => 'c3ag{' + p + '}{' + s + '}tail');
  },
  // Only the partner token: the first replaceInText finds nothing (its strstr arm at 0x0045b7d9 not taken).
  c3ag_story_partner() {
    return globalThis.DIFF_FIXTURES._c3ag_story((s, p) => 'c3ag-only-' + p + '-here');
  },
  // Neither token: both replaceInText calls leave the buffer alone, so the result is the saved text copied back.
  c3ag_story_none() {
    return globalThis.DIFF_FIXTURES._c3ag_story(() => 'c3ag-plain-text-without-any-token');
  },
  // The self token twice: replaceInText replaces only the first occurrence, so the second survives.
  c3ag_story_dup() {
    return globalThis.DIFF_FIXTURES._c3ag_story((s, p) => 'c3ag<' + s + '><' + s + '><' + p + '>');
  },

  // --------------------------------------------------------------------------- LandmarkAvailableNotice 0x004722c0
  // Tile map: the type byte at 0x005722e8 + x*50 + y, the word at 0x0053caf0 + (x*50 + y)*2, and the type record
  // 0x00578370 + type*0x30 whose byte +6 must be 4 for a tile to be accepted (0x004722f6). Whole map = type 0x14, the
  // type tileBlocked 0x0040bf60 reports as blocked, so every random walk that leaves a carved region ends the function
  // at 0x0047234c. Carved regions (all 5x5):
  //   A  x 5..9,   y 5..9    type 1  (+6 = 4), word 0            -> accepted on the first test
  //   B  x 15..19, y 15..19  type 2  (+6 = 3)                    -> rejected at 0x004722fc, walk leaves into 0x14
  //   C  x 25..29, y 25..29  type 1, centre (27,27) type 0x16    -> rejected at 0x00472301, walk accepts a neighbour
  //   D  x 35..39, y 35..39  type 1, centre (37,37) word 0x20    -> rejected at 0x00472314, walk accepts a neighbour
  // Golfers: 10 -> (7,7) in A, 11 -> (17,17) in B, 12 -> (27,27) in C, 13 -> (37,37) in D, 15 -> (45,45) on 0x14.
  // The golfer world position is the dword pair 0x005794b8 / 0x005794bc + g*0x100, shifted right by 10 (0x004722d6).
  // Letter table 0x0053a454, 50-byte rows: row `who` gives the switch byte at 0x0047235e. Rows 0..10 are the eleven
  // letters the jump tables at 0x004724a8 / 0x0047247c name, rows 11..13 are three bytes that take the default arm
  // (one inside the 0..0x17 window, one below 0x41, one above 0x58).
  _c3ag_landmark(busy) {
    globalThis.DIFF_FIXTURES._c3ag_settle();

    // Type records: byte +6 of 0x00578370 + type*0x30.
    const typeByte6 = (t, v) => { const p = ptr('0x00578370').add(t * 0x30 + 6); Memory.protect(p, 1, 'rw-'); p.writeU8(v); };
    typeByte6(1, 4);      // accepted type
    typeByte6(2, 3);      // rejected by the +6 test
    typeByte6(0x16, 4);   // +6 passes, rejected by the type == 0x16 test
    typeByte6(0x14, 0);   // blocked type (tileBlocked)

    const types = ptr('0x005722e8');
    const words = ptr('0x0053caf0');
    for (let i = 0; i < 2500; i++) { types.add(i).writeU8(0x14); words.add(i * 2).writeU16(0); }
    const fill = (x0, y0, t) => {
      for (let x = x0; x < x0 + 5; x++) for (let y = y0; y < y0 + 5; y++) types.add(x * 50 + y).writeU8(t);
    };
    fill(5, 5, 1);
    fill(15, 15, 2);
    fill(25, 25, 1);
    types.add(27 * 50 + 27).writeU8(0x16);
    fill(35, 35, 1);
    words.add((37 * 50 + 37) * 2).writeU16(0x20);   // one bit of the 0x320 mask

    const place = (g, x, y) => {
      ptr('0x005794b8').add(g * 0x100).writeS32(x << 10);
      ptr('0x005794bc').add(g * 0x100).writeS32(y << 10);
    };
    place(10, 7, 7);
    place(11, 17, 17);
    place(12, 27, 27);
    place(13, 37, 37);
    place(15, 45, 45);

    const letters = [0x41, 0x43, 0x46, 0x47, 0x48, 0x4c, 0x4d, 0x50, 0x52, 0x53, 0x58, 0x42, 0x40, 0x59];
    const lt = ptr('0x0053a454');
    Memory.protect(lt, 14 * 50, 'rw-');
    for (let i = 0; i < letters.length; i++) lt.add(i * 50).writeU8(letters[i]);

    ptr('0x00822c70').writeU32(0);    // landmark bitmask A (state region)
    ptr('0x00543cfc').writeU32(0);    // landmark bitmask B (state region)
    ptr('0x00822d9c').writeS32(0x1234);  // RNG seed (state region, restored per vector)
    ptr('0x0051a068').writeUtf8String('c3ag:');

    // startMessage 0x0040cb00 preconditions: idle unless `busy` (direction byte set -> it refuses, prio 0 <= 0).
    ptr('0x00569498').writeU8(busy ? 1 : 0);
    ptr('0x0053df54').writeS32(0);
    ptr('0x00567afc').writeS32(0);
    ptr('0x005694a4').writeS32(0);
    ptr('0x00822c88').writeS32(0);    // startMessage's display-length divisor term
    return {};
  },
  c3ag_landmark() { return globalThis.DIFF_FIXTURES._c3ag_landmark(0); },
  c3ag_landmark_busy() { return globalThis.DIFF_FIXTURES._c3ag_landmark(1); },

  // ------------------------------------------------------------------------------------- patronEvent 0x004266b0
  // Golfer record stride 0x100. Fields the function and its callees read: kind byte 0x005794d0 (its top three bits
  // pick the block and buildGolferName's form, its low five bits the sub-kind), hole byte 0x005794d9, mood word
  // 0x0057955c, partner word 0x0057955a, flags dword 0x005794c8 (bit 0x200 drives the tail), name-slot byte
  // 0x00579573 and type word 0x0057956e for buildGolferName. The hole table 0x00575ab0 + hole*0x208 gets par 0 for
  // holes 0..5 (unbuilt, the accept side) and par 4 for holes 6..0x12 (the refuse side). The CEO counter 0x00572cac
  // is 1, so hole > 2 invests and hole <= 2 takes the short refusal.
  // Golfers 20..37 cover: CEO invest with mood above and below 4 (20, 21), CEO short refusal (22), CEO refusal with
  // and without the partner sentence (23, 24); commissioner approve (25) and both refusals (26, 27); heiress donate
  // with three different moods (28, 29, 32) and both refusals (30, 31); and five kinds that match no block, one of
  // them with flags bit 0x200 (33..37). Golfers 38 and 39 are the partners.
  _c3ag_patron(g) {
    globalThis.DIFF_FIXTURES._c3ag_settle();

    for (let k = 0; k < 6; k++) ptr('0x0058dd50').add(k * 0x38).writeUtf8String('c3agName' + k);

    // Hole table par bytes.
    for (let h = 0; h <= 0x12; h++) ptr('0x00575ab0').add(h * 0x208).writeS8(h <= 5 ? 0 : 4);

    //        golfer kind  hole mood partner flags
    const rows = [
      [20, 0x60, 3, 5, 38, 0], [21, 0x61, 3, 3, 39, 0], [22, 0x62, 1, 5, 38, 0],
      [23, 0x63, 7, 5, 39, 0], [24, 0x64, 3, 1, 38, 0],
      [25, 0x40, 4, 6, 39, 0], [26, 0x41, 8, 6, 38, 0], [27, 0x42, 4, 0, 39, 0],
      [28, 0x80, 5, 4, 38, 0], [29, 0x81, 5, 9, 39, 0], [30, 0x82, 9, 7, 38, 0],
      [31, 0x83, 5, 2, 39, 0], [32, 0x80, 5, 20, 38, 0],
      [33, 0x00, 3, 5, 38, 0x200], [34, 0x20, 3, 5, 39, 0], [35, 0xa0, 7, 1, 38, 0],
      [36, 0xc0, 3, 5, 39, 0x200], [37, 0xe0, 5, 4, 38, 0],
      [38, 0x24, 2, 3, 20, 0], [39, 0x80, 2, 3, 21, 0],
    ];
    for (const [i, kind, hole, mood, partner, flags] of rows) {
      const r = i * 0x100;
      ptr('0x005794d0').add(r).writeU8(kind);
      ptr('0x005794d9').add(r).writeS8(hole);
      ptr('0x0057955c').add(r).writeS16(mood);
      ptr('0x0057955a').add(r).writeS16(partner);
      ptr('0x005794c8').add(r).writeU32(flags);
      ptr('0x00579573').add(r).writeU8(i % 6);
      ptr('0x0057956e').add(r).writeS16(i % 4);
      ptr('0x0057958c').add(r).writeS32(0x5a5a);   // the tail zeroes this (state region)
    }
    ptr('0x0053a450').writeU32(g.a450 >>> 0);   // commissioner multiplier; bit 7 is cleared by the block
    ptr('0x00572cac').writeS32(1);              // CEO counter: 2*1 = 2, so hole 3 invests and hole 1 refuses
    ptr('0x00543cfc').writeU32(g.taken >>> 0);  // landmark bitmask: a clear bit ends the heiress draw loop
    ptr('0x00822c70').writeU32(0);
    ptr('0x00822c88').writeS32(g.diff | 0);     // <= 1 picks the __itoa arms, > 1 the appendCents arms
    ptr('0x0059aaf8').writeS32(3);
    ptr('0x00571fd4').writeS32(1000);           // cash
    ptr('0x005a6d3c').writeS16(g.week | 0);     // % 100 selects the word at 0x0058421e + slot*0x14
    ptr('0x00567a1c').writeS32(0);
    ptr('0x005a9ccc').writeS32(0);
    ptr('0x005a59f8').writeS32(0);
    ptr('0x00569628').writeS32(0);              // appendCents scratch
    for (let i = 0; i < 100; i++) ptr('0x0058421e').add(i * 0x14).writeS16(0);

    ptr('0x0051a068').writeUtf8String('c3ag:');
    ptr('0x0058a528').writeUtf8String('');
    ptr('0x00834170').writeS32(0x2468);         // tick dword logTick divides
    ptr('0x00822d9c').writeS32(0x1234);         // RNG seed (state region)

    // startMessage preconditions, as in the landmark fixtures.
    ptr('0x00569498').writeU8(g.busy ? 1 : 0);
    ptr('0x0053df54').writeS32(0);
    ptr('0x00567afc').writeS32(0);
    ptr('0x005694a4').writeS32(0);
    return {};
  },
  c3ag_patron() { return globalThis.DIFF_FIXTURES._c3ag_patron({ diff: 0, taken: 0, a450: 2, week: 7, busy: 0 }); },
  // 0x00822c88 = 2: the commissioner and heiress figures go through appendCents instead of __itoa.
  c3ag_patron_diff2() { return globalThis.DIFF_FIXTURES._c3ag_patron({ diff: 2, taken: 0, a450: 0x85, week: 103, busy: 0 }); },
  // The message ticker is already busy, so every startMessage refuses.
  c3ag_patron_busy() { return globalThis.DIFF_FIXTURES._c3ag_patron({ diff: 0, taken: 0, a450: 2, week: 7, busy: 1 }); },
  // Landmark bitmask with exactly one of the low four bits clear: golfer 28 has mood 4, so Random::range(4) draws
  // 0..3 and the heiress loop can only stop on that index, which selects one arm of the switch at 0x00426da6.
  c3ag_patron_lm0() { return globalThis.DIFF_FIXTURES._c3ag_patron({ diff: 0, taken: 0xfffe, a450: 2, week: 7, busy: 0 }); },
  c3ag_patron_lm1() { return globalThis.DIFF_FIXTURES._c3ag_patron({ diff: 0, taken: 0xfffd, a450: 2, week: 7, busy: 0 }); },
  c3ag_patron_lm2() { return globalThis.DIFF_FIXTURES._c3ag_patron({ diff: 0, taken: 0xfffb, a450: 2, week: 7, busy: 0 }); },
  c3ag_patron_lm3() { return globalThis.DIFF_FIXTURES._c3ag_patron({ diff: 0, taken: 0xfff7, a450: 2, week: 7, busy: 0 }); },
});
