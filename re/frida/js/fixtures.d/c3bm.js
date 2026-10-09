// Fixtures for C3 batch c3bm: jgld.dll's Sprite blitters (blitPair 0x25900, blitPair1 0x25dd0,
// draw16t_1001ab40 0x1ab40, draw16t_1001b060 0x1b060, draw16m_1001df80 0x1df80, draw16m_1001e970 0x1e970).
//
// Everything is OURS, built on jgld's own vtables so the accessors the blitters call are jgld's own code:
//   Surface  0x820 bytes, +0 = Surface_vtable (RVA 0x11d0b0). Fields the blitters reach:
//              +0x24 depth, +0x28 format code (slot +0xe4 returns this+0x24, so element [1] is this+0x28:
//                    0 -> the Palette's RGB565 table, 1 -> its RGB555 table, anything else -> return 1)
//              +0x40 row stride   (slot +0xe0, 0x1000aaf0)
//              +0x44 clip RECT    (slot +0xcc, 0x1000a990)
//              +0x54 bounds RECT  (not read by this batch; filled for faithfulness)
//              +0x4c0 pixel base  (slot +0x10 copies it to +0x4cc, bumps the count at +0x4c8 when it is not
//                                  NULL, and returns +0x4cc; slots +0x18 and +0x20 tail-call slot +0x10)
//   Palette  0x820 bytes, +0 = Palette_vtable (RVA 0x11db10): slot +0x18 returns this+0x40c (RGB565 table)
//            and slot +0x1c this+0x60c (RGB555), 256 16-bit entries each, filled with our own values.
//   Sprite   0x80 bytes, +0 = Sprite_vtable (RVA 0x11d380). +0x10 Palette, +0x14 payload, +0x28 key colour,
//            +0x2c pitch, +0x30 width, +0x34 height. The payload is a PLAIN bitmap, not run-length encoded
//            (log/c3/c3bh_notes.txt): 8 bits per pixel for blitPair/blitPair1/draw16m, 16 for draw16t.
//
// THE FROZEN RENDER THREAD. Every fixture here suspends the game's first thread for the duration of the key's
// vectors and restores, before resuming it, every module global these bodies write. Two reasons, both
// measured by log/c3/alt/c3bm_probe.py over one 12 s boot at the menu:
//   * the live Sprite::draw16_1001bc40 0x1bc40 (2_047 entries in that boot) parks its loop state in the same
//     block as blitPair, blitPair1 and draw16m: 0x1012857c changed 523 times, 0x10128564 524, 0x10128560 524,
//     0x1012856c 130, 0x10128578 25. This is c3av's Surface::fillRectC8 problem exactly, and the row counter
//     is worse than a skip: the loops DECREMENT 0x10128578 in memory, so a value the game parks mid-run ends
//     ours early or never.
//   * the three scale globals 0x10122dc0 / 0x10122dc4 / 0x10122dc8 decide which half of every draw16t and
//     draw16m runs, and 0x1bc40 reads them on every call. They measured 1, 1, 1 and never changed, so a
//     fixture that does not write them can only reach ONE of the bodies; writing them while 0x1bc40 runs
//     would mirror or scale what the menu draws.
// The suspend happens after every allocation and module lookup, and nothing the vectors call allocates or
// takes a lock. The resume is a setTimeout(0): Frida's JS runtime is single-threaded, so it cannot run until
// diff_hook's `run` call has returned (the recipe c3av proved).
//
// The draw16t block (0x101284d8..0x1012851c) and draw16m's extras (0x10128530/34/38, 0x10128568,
// 0x1012858c/90) measured 0 changes in the same boot and no function that writes them is ever entered by the
// game, so they ARE state regions of the keys below: what the bodies park in them is compared directly. The
// seven shared ones are NOT, for the reason c3av recorded for 0x1012847c; what they are set to is still
// verified through $pix, deterministically (a wrong source pitch, column count or row skip puts the written
// pixels at the wrong offsets, and a wrong row count writes the wrong number of rows).
Object.assign(globalThis.DIFF_FIXTURES, {
  // The objects, with nothing frozen and no global written: the six wrappers below add that.
  c3bm_objects() {
    const jgld = Process.getModuleByName('jgld.dll').base;
    const vtSurface = jgld.add(0x11d0b0);
    const vtPalette = jgld.add(0x11db10);
    const vtSprite = jgld.add(0x11d380);

    // ---- two Palettes with four different tables, so a vector proves which object and which table was read.
    const mkPal = (a565, b565, a555, b555) => {
      const p = Memory.alloc(0x820);
      for (let o = 0; o < 0x820; o += 4) p.add(o).writeU32(0);
      p.writePointer(vtPalette);
      for (let i = 0; i < 256; i++) {
        p.add(0x40c + i * 2).writeU16((i * a565 + b565) & 0xffff);
        p.add(0x60c + i * 2).writeU16((i * a555 + b555) & 0xffff);
      }
      return p;
    };
    const pal = mkPal(0x0105, 0x1234, 0x0203, 0x5678);
    const pal2 = mkPal(0x0307, 0x9abc, 0x0409, 0xdef0);

    // ---- the pixel block. Surface i draws into $pix + i * 0x1000; one state region covers all of them and
    // the hash says which one changed. 0x1000 holds the widest surface here (32 x 16 at stride 32, 16 bits =
    // 1024 bytes) with room to spare.
    const NSURF = 12, SLICE = 0x1000;
    const pix = Memory.alloc(NSURF * SLICE);

    //      depth fmt pitch bounds            clip               base
    const SPECS = [
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   //  0 d0   the main destination, clip == bounds
      [16, 0, 32, [0, 0, 32, 16], [4, 2, 20, 12], true],   //  1 d1   clip strictly inside: trims the rectangle
      [16, 1, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   //  2 d2   format 1: the RGB555 table
      [16, 2, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   //  3 d3   format 2: the switch default
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], false],  //  4 d4   no pixel base: slot +0x10 returns NULL
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   //  5 d5   blitPair's second destination
      [16, 0, 24, [0, 0, 24, 12], [0, 0, 24, 12], true],   //  6 d6   a narrower surface at stride 24
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   //  7 d7   a third destination (dst2 variations)
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],    //  8 m0   draw16m's mask surface
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],    //  9 m1   a mask with the other byte pattern
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], false],   // 10 mnb  a mask with no base: slot +0x10 NULL
      [8, 0, 20, [0, 0, 20, 10], [0, 0, 20, 10], true],    // 11 m2   a mask at a different stride
    ];
    const SNAMES = ['d0', 'd1', 'd2', 'd3', 'd4', 'd5', 'd6', 'd7', 'm0', 'm1', 'mnb', 'm2'];

    // 16-bit slices: every word is below 0x2000, because draw16t_1001b060 indexes its table with the 32-bit
    // value (source << 16) | destination taken as a BYTE offset (0x1001b2d4..0x1001b2dd); keeping the
    // destination word small bounds that offset. The values still spread over the whole range so the blend
    // and the table lookups differ pixel by pixel.
    // 8-bit slices (the masks): a six-value alphabet around the `level` argument, so both sides of
    // `cmp byte ptr [ecx], bl` (0x1001e367, 0x1001ed47) run thousands of times in one vector.
    const MASKALPH = [0x00, 0x20, 0x40, 0x80, 0xc0, 0xff];
    for (let i = 0; i < NSURF; i++) {
      const base = i * SLICE;
      if (SPECS[i][0] === 16) {
        for (let k = 0; k < SLICE / 2; k++) pix.add(base + k * 2).writeU16((k * 4099 + i * 311 + 7) & 0x1fff);
      } else {
        for (let j = 0; j < SLICE; j++) pix.add(base + j).writeU8(MASKALPH[(j * 5 + i * 3) % 6]);
      }
    }

    const fx = { obj: pix, pix: pix, pal: pal, pal2: pal2 };
    const surf = Memory.alloc(NSURF * 0x820);
    SPECS.forEach((sp, i) => {
      const depth = sp[0], fmt = sp[1], pitch = sp[2], b = sp[3], k = sp[4], hasBits = sp[5];
      const s = surf.add(i * 0x820);
      for (let o = 0; o < 0x820; o += 4) s.add(o).writeU32(0);
      s.writePointer(vtSurface);
      s.add(0x24).writeS32(depth);
      s.add(0x28).writeS32(fmt);
      s.add(0x40).writeS32(pitch);
      for (let j = 0; j < 4; j++) s.add(0x44 + j * 4).writeS32(k[j]);
      for (let j = 0; j < 4; j++) s.add(0x54 + j * 4).writeS32(b[j]);
      s.add(0x4c0).writePointer(hasBits ? pix.add(i * SLICE) : ptr(0));
      fx[SNAMES[i]] = s;
      fx['l' + i] = s.add(0x4c0);       // +0x4c0 base, +0x4c8 use count, +0x4cc current base
    });

    // ---- the sprite payloads. One block, six slices of 0x800.
    //   0, 1, 2, 3  8-bit, for blitPair / blitPair1 / draw16m. Alphabet A is the colour side: it contains
    //       0xff (the `jae` skip at 0x10025b91 and the `je` at 0x1001e362), 0xfe (draw16m's flat-colour arm
    //       at 0x1001e36b and the `jae` of the twin at 0x1001ed42) and four ordinary indices. Alphabet B is
    //       the weight side: it contains 0x00 (the no-blend arm at 0x10025ba8), 0xff (the second skip at
    //       0x10025b9a) and four weights that make `mul cl` overflow its five bits at the top end.
    //   4   16-bit, for draw16t_1001ab40: low bytes 0x00 (the destination-table arm at 0x1001ada3), 0xfe
    //       (the key of sp16a) and three ordinary values; the high byte varies so a copied word is visible.
    //   5   16-bit, for draw16t_1001b060: every word below 0x0004 except the transparent 0x00ff, because the
    //       table offset is (word << 16) | destination.
    const NPAY = 6, PAY = 0x800;
    const pay = Memory.alloc(NPAY * PAY);
    const ALPH_A = [0x00, 0x01, 0x3c, 0x7f, 0xa5, 0xfe, 0xff];
    const ALPH_B = [0x00, 0x01, 0x40, 0x80, 0xc0, 0xfe, 0xff];
    const ALPH_16A = [0x0000, 0x1101, 0x2202, 0x33fe, 0x4403, 0x55ff];
    const ALPH_16B = [0x0000, 0x0001, 0x0002, 0x0003, 0x00ff];
    for (let j = 0; j < PAY; j++) {
      pay.add(0 * PAY + j).writeU8(ALPH_A[(j * 3) % 7]);
      pay.add(1 * PAY + j).writeU8(ALPH_B[(j * 5 + 2) % 7]);
      pay.add(2 * PAY + j).writeU8(ALPH_A[(j * 5 + 4) % 7]);
      pay.add(3 * PAY + j).writeU8(ALPH_B[(j * 3 + 1) % 7]);
    }
    for (let k = 0; k < PAY / 2; k++) {
      pay.add(4 * PAY + k * 2).writeU16(ALPH_16A[(k * 3) % 6]);
      pay.add(5 * PAY + k * 2).writeU16(ALPH_16B[(k * 3 + 1) % 5]);
    }

    // ---- the sprites.  palette, payload slice, key(+0x28), pitch, width, height
    const SP = [
      [pal, 0, 0x00, 16, 16, 8],     // 0 spA    the colour sprite of the pair blitters
      [0, 1, 0x00, 16, 16, 8],       // 1 spNP   no Palette of its own: the client fallback at 0x10025931
      [pal2, 2, 0x00, 20, 20, 10],   // 2 spC    the other Palette, a wider sprite
      [pal, 3, 0x00, 16, 16, 8],     // 3 spW    the weight sprite handed in as `other`
      [pal, 0, 0x00, 4, 1, 1],       // 4 spD    1 x 1: one column and one row
      [pal, 0, 0x00, 16, 16, 8],     // 5 spNB   payload cleared to NULL below: the guard at 0x1001ab60
      [pal, 4, 0xfe, 16, 16, 8],     // 6 sp16a  draw16t_1001ab40, key 0xfe
      [pal, 4, 0x00, 16, 16, 8],     // 7 sp16c  the same payload with key 0x00: the arms swap
      [pal, 5, 0x00, 16, 16, 8],     // 8 sp16b  draw16t_1001b060 (which overwrites +0x28 with 0xff)
      [pal, 4, 0x11, 12, 12, 6],     // 9 sp16d  key 0x11, a smaller 16-bit sprite at a different pitch
    ];
    const SPN = ['spA', 'spNP', 'spC', 'spW', 'spD', 'spNB', 'sp16a', 'sp16c', 'sp16b', 'sp16d'];
    const spr = Memory.alloc(SP.length * 0x80);
    SP.forEach((s, i) => {
      const p = spr.add(i * 0x80);
      for (let o = 0; o < 0x80; o += 4) p.add(o).writeU32(0);
      p.writePointer(vtSprite);
      p.add(0x10).writePointer(s[0] ? s[0] : ptr(0));
      p.add(0x14).writePointer(SPN[i] === 'spNB' ? ptr(0) : pay.add(s[1] * PAY));
      p.add(0x28).writeS32(s[2]);
      p.add(0x2c).writeS32(s[3]);
      p.add(0x30).writeS32(s[4]);
      p.add(0x34).writeS32(s[5]);
      fx[SPN[i]] = p;
    });
    fx.spr = spr;
    fx.spix = pay;

    // ---- the two lookup tables draw16t is handed as its fourth argument.
    //   $t16  0x20000 bytes: 0x10000 16-bit entries indexed by the DESTINATION pixel (0x1001adab,
    //         `mov bx, word ptr [ebp + ebx*2]`), so any destination word is in range.
    //   $t16b the same size with other values, so a vector proves which table was used.
    //   $t32  0x32000 bytes, indexed by the BYTE offset (source << 16) | destination (0x1001b2dd). The
    //         largest offset any registered vector can form is (3 << 16) | 0x1fff = 0x31fff.
    // Every entry is below 0x2000 for the same reason the 16-bit pixel slices are.
    const t16 = Memory.alloc(0x20000), t16b = Memory.alloc(0x20000), t32 = Memory.alloc(0x32000);
    for (let i = 0; i < 0x10000; i++) {
      t16.add(i * 2).writeU16((i * 7919 + 13) & 0x1fff);
      t16b.add(i * 2).writeU16((i * 104729 + 977) & 0x1fff);
    }
    for (let i = 0; i < 0x19000; i++) t32.add(i * 2).writeU16((i * 2749 + 31) & 0x1fff);
    fx.t16 = t16;
    fx.t16b = t16b;
    fx.t32 = t32;

    // ---- our own palette client, for the module global at 0x1012873c (read at 0x10025931 and its four
    // twins when a sprite carries no Palette of its own). Two of them: one with a Palette, one with none,
    // which is the only way to reach the `return 16` exits. The global is swapped by the wrappers below,
    // with the game's thread already suspended, and put back before it is resumed.
    const client = Memory.alloc(8);
    client.writeU32(0);
    client.add(4).writePointer(pal2);
    const clientNull = Memory.alloc(8);
    clientNull.writeU32(0);
    clientNull.add(4).writePointer(ptr(0));
    fx.client = client;
    fx.clientNull = clientNull;
    return fx;
  },
});

// Suspends the game's first thread, writes the scale globals, swaps the palette client, and schedules the
// restore + resume for the moment diff_hook's `run` returns. Everything it needs is read from `fx`, which the
// caller built before the suspend.
//
//   scale  [horizontal numerator, vertical numerator, denominator] for 0x10122dc0 / 0x10122dc4 / 0x10122dc8,
//          or null to leave them alone (the pair blitters do not read them).
//   client the pointer to store into 0x1012873c.
function c3bmFreeze(fx, scale, client) {
  const jgld = Process.getModuleByName('jgld.dll').base;
  // Three ranges cover every global these six bodies write, plus the client pointer:
  //   0x101284d0..0x10128598  draw16t's block, draw16m's extras and the seven shared loop globals
  //   0x10122dc0..0x10122dcc  the scale numerators and the denominator
  //   0x1012873c              the palette client
  const RANGES = [[0x1284d0, 0xc8], [0x122dc0, 0xc], [0x12873c, 4]];
  fx.gdraw16t = jgld.add(0x1284d8);      // 0x101284d8..0x1012851c, a state region of the draw16t keys
  fx.gmask = jgld.add(0x128530);         // 0x10128530 / 34 / 38, state regions of the draw16m keys
  fx.glevel = jgld.add(0x128568);
  fx.gratio = jgld.add(0x12858c);        // 0x1012858c / 0x10128590
  // shared frozen-thread helper (re/frida/js/freeze.js): safe-point suspend, saves restored before the resume
  __freeze.thread();
  RANGES.forEach((r) => __freeze.save(jgld.add(r[0]), r[1]));
  if (scale) {
    jgld.add(0x122dc0).writeS32(scale[0]);
    jgld.add(0x122dc4).writeS32(scale[1]);
    jgld.add(0x122dc8).writeS32(scale[2]);
  }
  jgld.add(0x12873c).writePointer(client);
  return fx;
}

// One fixture per setting of the three scale globals, because they pick the body (the round-5 rule: a function
// whose behaviour depends on a fixture-set global gets one registry entry per setting).
//   _s111   (1, 1, 1)     abs(dc0) == abs(dc8) and abs(dc4) == abs(dc8), both non-negative: the pixel loop
//   _sm11   (-1, 1, 1)    the same test passes, dc0 < 0: the loop is skipped (0x1001acdf / 0x1001e26a)
//   _s1m1   (1, -1, 1)    dc4 < 0: skipped by the second sign test (0x1001acec / 0x1001e277)
//   _s112   (1, 1, 2)     abs(dc0) != abs(dc8): the scaled half (0x1001ac21 / 0x1001e196)
//   _s212   (2, 1, 2)     abs(dc0) == abs(dc8) but abs(dc4) != abs(dc8): the second conjunct on its own
// `_nopal` variants point the palette client at a client with no Palette, the only way to the `return 16`
// exits at 0x10025958, 0x10025e28, 0x1001e060 and 0x1001ea50.
Object.assign(globalThis.DIFF_FIXTURES, {
  c3bm_pair() {
    const fx = globalThis.DIFF_FIXTURES.c3bm_objects();
    return c3bmFreeze(fx, null, fx.client);
  },
  c3bm_pair_nopal() {
    const fx = globalThis.DIFF_FIXTURES.c3bm_objects();
    return c3bmFreeze(fx, null, fx.clientNull);
  },
  c3bm_s111() {
    const fx = globalThis.DIFF_FIXTURES.c3bm_objects();
    return c3bmFreeze(fx, [1, 1, 1], fx.client);
  },
  c3bm_s111_nopal() {
    const fx = globalThis.DIFF_FIXTURES.c3bm_objects();
    return c3bmFreeze(fx, [1, 1, 1], fx.clientNull);
  },
  c3bm_sm11() {
    const fx = globalThis.DIFF_FIXTURES.c3bm_objects();
    return c3bmFreeze(fx, [-1, 1, 1], fx.client);
  },
  c3bm_s1m1() {
    const fx = globalThis.DIFF_FIXTURES.c3bm_objects();
    return c3bmFreeze(fx, [1, -1, 1], fx.client);
  },
  c3bm_s112() {
    const fx = globalThis.DIFF_FIXTURES.c3bm_objects();
    return c3bmFreeze(fx, [1, 1, 2], fx.client);
  },
  c3bm_s212() {
    const fx = globalThis.DIFF_FIXTURES.c3bm_objects();
    return c3bmFreeze(fx, [2, 1, 2], fx.client);
  },
});
