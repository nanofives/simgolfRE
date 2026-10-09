// Fixtures for C3 batch c3bu: jgld.dll's ten remaining Sprite draw rows (0x2ed60, 0x30290, 0x41f80,
// 0x43430, 0x44940, 0x45e50, 0x473b0, 0x48910, 0x20430, 0x62dd0). Built from re/frida/js/fixtures.d/c3bp.js,
// which batch c3bp used for the same family, with the 8-bit block 0x62dd0 needs added.
//
// Everything is OURS, built on jgld's own vtables so the accessors the blitters call are jgld's own code:
//   Surface  0x820 bytes, +0 = Surface_vtable (RVA 0x11d0b0). Fields these bodies reach:
//              +0x24 depth, +0x28 format code (slot +0xe4 returns this+0x24, so element [1] is this+0x28:
//                    0 -> the Palette's RGB565 table, 1 -> its RGB555 table, anything else -> return 1;
//                    0x62dd0 never reads it)
//              +0x40 row stride   (slot +0xe0, 0x1000aaf0)
//              +0x44 clip RECT    (slot +0xcc, 0x1000a990)
//              +0x54 bounds RECT  (not read by this batch; filled for faithfulness)
//              +0x4c0 pixel base  (slot +0x10 copies it to +0x4cc, bumps the count at +0x4c8 when that copy
//                    is not NULL, and returns the COPY, so a surface with no base makes the guard return 3.
//                    Slot +0x18 (0x1000a750, the one 0x62dd0 calls at 0x10062fbd) and slot +0x20
//                    (0x1000a7b0, the one the nine 16-bit members call) both tail-call it.)
//   Palette  0x820 bytes, +0 = Palette_vtable (RVA 0x11db10): slot +0x18 returns this+0x40c (RGB565 table)
//            and slot +0x1c this+0x60c (RGB555), 256 16-bit entries each, filled with our own values.
//   Sprite   0x80 bytes, +0 = Sprite_vtable (RVA 0x11d380). +0x10 Palette, +0x14 payload, +0x2c pitch,
//            +0x30 width, +0x34 height. The payload is a PLAIN 8-bit bitmap, not run-length encoded.
//
// THE PAYLOAD SITS IN THE MIDDLE OF ITS SLICE, for the reason c3bp recorded (finding 2 of
// log/c3/c3bp_notes.txt): six of the eight bodies of every one of these functions walk the source cursor
// outside the payload rectangle. Each sprite therefore gets a 0x4000-byte slice filled end to end with the
// alphabet and its +0x14 points at slice + 0x1000: every read of either arm is inside our own allocation
// and identical for both.
//
// THE FROZEN FIRST THREAD. Every fixture here suspends the game's first thread for the duration of the key's
// vectors and restores, before resuming it, every module global these bodies write. Two reasons, both
// measured over one 12 s boot at the menu (log/c3/alt/c3bu_probe.py here, c3bp_probe.py before it):
//   * the live Sprite::draw16_1001bc40 0x1bc40 parks its loop state in the same seven 16-bit globals, five
//     of which change hundreds of times in twelve seconds. This is c3av's Surface::fillRectC8 problem
//     exactly, and the row counter is worse than a skip: the loops DECREMENT 0x10128578 (and, in 0x62dd0,
//     0x10128670) in memory, so a value the game parks mid-run ends ours early or never.
//   * the three scale globals 0x10122dc0 / 0x10122dc4 / 0x10122dc8 decide which of the eight bodies runs,
//     and 0x1bc40 reads all three on every call, so they can only be written with that thread down.
// The suspend happens after every allocation and module lookup, and nothing the vectors call allocates or
// takes a lock. The resume is a setTimeout(0): Frida's JS runtime is single-threaded, so it cannot run until
// diff_hook's `run` call has returned (the recipe c3av proved and c3bm re-checked).
//
// 0x62dd0's COLOUR TABLE. The 8-bit member reads its 256-byte colour table from 0x10128658 (0x10063023) and
// never writes it: no argument of the function carries it. The freeze wrapper therefore installs our own
// table there, with the thread already down, and the range restore puts the module's value back before the
// resume. The pointer is NOT a state region (neither arm writes it); which table was read is proved through
// $pix, since every non-0xf8 pixel the function writes comes out of it.
Object.assign(globalThis.DIFF_FIXTURES, {
  // The objects, with nothing frozen and no global written: the wrappers below add that. Built once per
  // boot; diff_hook snapshots and restores every state region around each vector, so the content both arms
  // start from is the same for every vector of every key.
  c3bu_objects() {
    if (globalThis.__c3bu) return globalThis.__c3bu;
    const jgld = Process.getModuleByName('jgld.dll').base;
    const vtSurface = jgld.add(0x11d0b0);
    const vtPalette = jgld.add(0x11db10);
    const vtSprite = jgld.add(0x11d380);

    // ---- two Palettes with four different tables, so a vector proves which object and which table was read.
    // Every entry is below 0x2000 because 0x20430 indexes a 16-bit table with a DESTINATION pixel and
    // because the loops write colours back into the destination, where the next pixel to be read has to
    // stay in range too.
    const mkPal = (a565, b565, a555, b555) => {
      const p = Memory.alloc(0x820);
      for (let o = 0; o < 0x820; o += 4) p.add(o).writeU32(0);
      p.writePointer(vtPalette);
      for (let i = 0; i < 256; i++) {
        p.add(0x40c + i * 2).writeU16((i * a565 + b565) & 0x1fff);
        p.add(0x60c + i * 2).writeU16((i * a555 + b555) & 0x1fff);
      }
      return p;
    };
    const pal = mkPal(0x0105, 0x0234, 0x0203, 0x0678);
    const pal2 = mkPal(0x0307, 0x0abc, 0x0409, 0x0ef0);

    // ---- the pixel block. Surface i draws into $pix + i * 0x1000; one state region covers all of them and
    // the hash says which one changed. 0x1000 holds the widest surface here (32 x 16 at stride 32, 16 bits
    // = 1024 bytes) with room to spare; 0x62dd0 treats the same stride as BYTES and uses 512 of them.
    const NSURF = 6, SLICE = 0x1000;
    const pix = Memory.alloc(NSURF * SLICE);

    //      depth fmt pitch bounds            clip               base
    const SPECS = [
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   // 0 d0  the main destination, clip == bounds
      [16, 0, 32, [0, 0, 32, 16], [4, 2, 20, 12], true],   // 1 d1  clip strictly inside: trims the rectangle
      [16, 1, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   // 2 d2  format 1: the RGB555 table
      [16, 2, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   // 3 d3  format 2: the switch default
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], false],  // 4 d4  no pixel base: the guard's lock returns 0
      [16, 0, 24, [0, 0, 24, 12], [0, 0, 24, 12], true],   // 5 d5  a narrower surface at stride 24
    ];
    const SNAMES = ['d0', 'd1', 'd2', 'd3', 'd4', 'd5'];
    for (let i = 0; i < NSURF; i++) {
      for (let k = 0; k < SLICE / 2; k++) {
        pix.add(i * SLICE + k * 2).writeU16((k * 4099 + i * 311 + 7) & 0x1fff);
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

    // ---- the sprite payloads: three 0x4000 slices, each filled end to end so that the six bodies that
    // leave the payload rectangle still read our own bytes. One alphabet drives every arm of the ten
    // operations:
    //   0x00..0xa5        ordinary colour indices for all ten
    //   0xf8              0x20430's and 0x62dd0's destination remap (`cmp byte ptr [esi], 0xf8`), and the
    //                     first byte the eight blend members skip when their flag is non-zero
    //   0xf9, 0xfd        more flag-gated bytes for the eight; the ordinary path for 0x20430 and 0x62dd0
    //   0xfe              transparent for 0x20430 and 0x62dd0, still drawn by the eight
    //   0xff              transparent for the eight
    const NPAY = 3, PAY = 0x4000, PAYOFF = 0x1000;
    const pay = Memory.alloc(NPAY * PAY);
    const ALPH = [0x00, 0x01, 0x05, 0x0f, 0x10, 0x3c, 0x7f, 0xa5, 0xf8, 0xf9, 0xfd, 0xfe, 0xff];
    const STEP = [5, 3, 7];
    for (let i = 0; i < NPAY; i++) {
      for (let j = 0; j < PAY; j++) pay.add(i * PAY + j).writeU8(ALPH[(j * STEP[i] + i * 4) % 13]);
    }

    // ---- the sprites.  palette, payload slice (-1 = none), pitch, width, height
    const SP = [
      [pal, 0, 16, 16, 8],     // 0 spA   the main sprite
      [pal2, 1, 20, 20, 10],   // 1 spB   wider, the other Palette and the other alphabet phase
      [0, 2, 16, 16, 8],       // 2 spNP  no Palette of its own: the client fallback at 0x1002edc8
      [pal, 0, 4, 1, 1],       // 3 spD   1 x 1: one column and one row
      [pal, -1, 16, 16, 8],    // 4 spNB  no payload: the guard at 0x1002ed86
    ];
    const SPN = ['spA', 'spB', 'spNP', 'spD', 'spNB'];
    const spr = Memory.alloc(SP.length * 0x80);
    SP.forEach((s, i) => {
      const p = spr.add(i * 0x80);
      for (let o = 0; o < 0x80; o += 4) p.add(o).writeU32(0);
      p.writePointer(vtSprite);
      p.add(0x10).writePointer(s[0] ? s[0] : ptr(0));
      p.add(0x14).writePointer(s[1] < 0 ? ptr(0) : pay.add(s[1] * PAY + PAYOFF));
      p.add(0x2c).writeS32(s[2]);
      p.add(0x30).writeS32(s[3]);
      p.add(0x34).writeS32(s[4]);
      fx[SPN[i]] = p;
    });
    fx.spr = spr;
    fx.spix = pay;

    // ---- the lookup tables the two table-driven members are handed.
    //   $m   256 bytes, the index remap both park (0x10128548 for 0x20430, 0x1012862c for 0x62dd0); $m2 the
    //        same block read from 0x80 in, inside its 0x200 bytes, so a vector proves which pointer was used
    //   $t   0x10000 bytes of 16-bit entries, 0x20430's destination remap: indexed by a destination pixel,
    //        which stays below 0x2000 because every value written back comes from a Palette table or from
    //        $t itself and both are masked to 0x1fff. $t2 is the same block read from 0x80 in.
    //   $n   256 bytes, 0x62dd0's destination remap (its destination is 8-bit); $n2 the same from 0x80 in
    //   $c8  256 bytes, the colour table 0x62dd0 reads at 0x10128658 (installed by the freeze wrapper)
    const m = Memory.alloc(0x200);
    for (let i = 0; i < 0x200; i++) m.add(i).writeU8((i * 37 + 11) & 0xff);
    const t = Memory.alloc(0x10000);
    for (let i = 0; i < 0x8000; i++) t.add(i * 2).writeU16((i * 7919 + 13) & 0x1fff);
    const n = Memory.alloc(0x200);
    for (let i = 0; i < 0x200; i++) n.add(i).writeU8((i * 97 + 29) & 0xff);
    const c8 = Memory.alloc(0x100);
    for (let i = 0; i < 0x100; i++) c8.add(i).writeU8((i * 151 + 71) & 0xff);
    fx.m = m;
    fx.m2 = m.add(0x80);
    fx.t = t;
    fx.t2 = t.add(0x80);
    fx.n = n;
    fx.n2 = n.add(0x80);
    fx.c8 = c8;

    // ---- our own palette client, for the module global at 0x1012873c (read at 0x1002edc8 when a sprite
    // carries no Palette of its own). Two of them: one with a Palette, one with none, which is the only way
    // to reach the `return 16` exit at 0x1002edef. The global is swapped by the wrappers below, with the
    // game's thread already suspended, and put back before it is resumed.
    const client = Memory.alloc(8);
    client.writeU32(0);
    client.add(4).writePointer(pal2);
    const clientNull = Memory.alloc(8);
    clientNull.writeU32(0);
    clientNull.add(4).writePointer(ptr(0));
    fx.client = client;
    fx.clientNull = clientNull;

    // ---- the module globals that are state regions of every key (see the header of registry.d/c3bu.py).
    // The 16-bit block, the one the nine 16-bit members own:
    fx.gvstep = jgld.add(0x128540);     // 0x10128540 the 16.16 vertical step, 0x10128544 its low half << 16
    fx.gtbl = jgld.add(0x128548);       // 0x20430's fourth argument
    fx.gpos = jgld.add(0x128554);       // 0x10128554 / 0x10128558 the 16.16 start position
    fx.gcols = jgld.add(0x128570);      // 0x10128570 source columns consumed, 0x10128574 0x20430's fifth arg
    fx.gxstep = jgld.add(0x128580);     // 580 integer step, 584, 588 the 16.16 step, 58c / 590 the ratios
    // The 8-bit block, 0x62dd0's:
    fx.g8vstep = jgld.add(0x128624);    // 0x10128624 the 16.16 vertical step, 0x10128628 its low half << 16
    fx.g8tbl = jgld.add(0x12862c);      // 0x62dd0's fourth argument
    fx.g8pos = jgld.add(0x12863c);      // 0x1012863c / 0x10128640 the 16.16 start position
    fx.g8cols = jgld.add(0x128660);     // source columns consumed
    fx.g8pix = jgld.add(0x12866c);      // 0x62dd0's fifth argument
    fx.g8step = jgld.add(0x12867c);     // 0x1012867c integer step, 0x10128680 source rows consumed
    fx.g8xstep = jgld.add(0x128688);    // 688 the 16.16 step, 68c / 690 the ratios
    // the two ints each module pointer pair addresses, written as `*p = ...` and read back as
    // `[ebx + eax*4]` with eax 0 or -1, so each pair is (pointer - 4, pointer)
    const rowPtr = jgld.add(0x122e20).readPointer();
    fx.growpair = rowPtr.isNull() ? jgld.add(0x12854c) : rowPtr.sub(4);
    // log/c3/alt/c3bu_probe.py, one 12 s boot at the menu: 0x10122e20 -> jgld + 0x128550 and 0x10122e1c ->
    // jgld + 0x12854c; 0x10122e58 -> jgld + 0x128638 and 0x10122e54 -> jgld + 0x128634. Both pairs are
    // adjacent as `[ebx + eax*4]` with eax 0 or -1 requires, and neither slot nor target changed.
    const rowPtr8 = jgld.add(0x122e58).readPointer();
    fx.g8rowpair = rowPtr8.isNull() ? jgld.add(0x128634) : rowPtr8.sub(4);

    globalThis.__c3bu = fx;
    return fx;
  },
});

// Suspends the game's first thread, writes the three scale globals, swaps the palette client, installs
// 0x62dd0's colour table at 0x10128658, and schedules the restore + resume for the moment diff_hook's `run`
// call returns. Everything it needs is read from `fx`, which the caller built (and cached) before the
// suspend.
//
//   scale  [horizontal numerator, vertical numerator, denominator] for 0x10122dc0 / 0x10122dc4 / 0x10122dc8
//   client the pointer to store into 0x1012873c
function c3buFreeze(fx, scale, client) {
  const jgld = Process.getModuleByName('jgld.dll').base;
  // Five ranges cover every global these ten bodies write, plus the two slots they only read:
  //   0x101284d0..0x10128698  the seven shared 16-bit loop globals, the 16-bit stepped half's own block,
  //                           the row-step pair at 0x1012854c / 0x10128550, and the WHOLE 8-bit block
  //                           0x1012861c..0x10128694 including the colour-table slot 0x10128658
  //   0x10122dc0..0x10122dcc  the scale numerators and the denominator
  //   0x1012873c              the palette client
  //   the two row-step pairs, again, by the addresses the module pointers actually hold
  const RANGES = [[jgld.add(0x1284d0), 0x1c8], [jgld.add(0x122dc0), 0xc], [jgld.add(0x12873c), 4],
                  [fx.growpair, 8], [fx.g8rowpair, 8]];
  // shared frozen-thread helper (re/frida/js/freeze.js): safe-point suspend, saves restored before the resume
  __freeze.thread();
  RANGES.forEach((r) => __freeze.save(r[0], r[1]));
  if (scale) {
    jgld.add(0x122dc0).writeS32(scale[0]);
    jgld.add(0x122dc4).writeS32(scale[1]);
    jgld.add(0x122dc8).writeS32(scale[2]);
  }
  jgld.add(0x12873c).writePointer(client);
  jgld.add(0x128658).writePointer(fx.c8);
  return fx;
}

// One fixture per setting of the three scale globals, because they pick which of the eight bodies runs (the
// round-5 rule: a function whose behaviour depends on a fixture-set global gets one registry entry per
// setting). `abs(0x10122dc0) == abs(0x10122dc8) && abs(0x10122dc4) == abs(0x10122dc8)` (0x1002ef0c and
// 0x1002ef33 in 0x2ed60, 0x10062ec0 and 0x10062ee7 in 0x62dd0) chooses the unscaled half, and the SIGNS of
// the two numerators choose the mirror body.
//   _s111   ( 1,  1, 1)  unscaled, neither numerator negative
//   _sm11   (-1,  1, 1)  unscaled, horizontal mirror
//   _s1m1   ( 1, -1, 1)  unscaled, vertical mirror
//   _sm1m1  (-1, -1, 1)  unscaled, both
//   _s223   ( 2,  2, 3)  stepped, step 1.5 in both directions so the fractional carries of `adc esi` and of
//                        the row selector `sbb eax, eax` both occur
//   _sm223  (-2,  2, 3)  stepped, horizontal mirror
//   _s2m23  ( 2, -2, 3)  stepped, vertical mirror
//   _sm2m23 (-2, -2, 3)  stepped, both
//   _s323   ( 3,  2, 3)  abs(dc0) == abs(dc8) but abs(dc4) != abs(dc8): the stepped half entered through the
//                        SECOND conjunct alone, with an integer horizontal step
//   _s553   ( 5,  5, 3)  a magnification whose 16.16 horizontal step is 0x9999 (odd low half)
// `_nopal` points the palette client at a client with no Palette, the only way to the `return 16` exit.
function c3buScale(scale) {
  const fx = globalThis.DIFF_FIXTURES.c3bu_objects();
  return c3buFreeze(fx, scale, fx.client);
}

Object.assign(globalThis.DIFF_FIXTURES, {
  c3bu_s111() { return c3buScale([1, 1, 1]); },
  c3bu_sm11() { return c3buScale([-1, 1, 1]); },
  c3bu_s1m1() { return c3buScale([1, -1, 1]); },
  c3bu_sm1m1() { return c3buScale([-1, -1, 1]); },
  c3bu_s223() { return c3buScale([2, 2, 3]); },
  c3bu_sm223() { return c3buScale([-2, 2, 3]); },
  c3bu_s2m23() { return c3buScale([2, -2, 3]); },
  c3bu_sm2m23() { return c3buScale([-2, -2, 3]); },
  c3bu_s323() { return c3buScale([3, 2, 3]); },
  c3bu_s553() { return c3buScale([5, 5, 3]); },
  c3bu_nopal() {
    const fx = globalThis.DIFF_FIXTURES.c3bu_objects();
    return c3buFreeze(fx, [1, 1, 1], fx.clientNull);
  },
});
