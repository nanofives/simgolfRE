// Fixtures for C3 batch c3bp: jgld.dll's Sprite::draw16_* / draw_* family (0x1cde0, 0x56450, 0x576a0,
// 0x1f2d0, 0x26240, 0x27430, 0x28630, 0x298b0, 0x2bcf0, 0x2d890).
//
// Everything is OURS, built on jgld's own vtables so the accessors the blitters call are jgld's own code:
//   Surface  0x820 bytes, +0 = Surface_vtable (RVA 0x11d0b0). Fields these bodies reach:
//              +0x24 depth, +0x28 format code (slot +0xe4 returns this+0x24, so element [1] is this+0x28:
//                    0 -> the Palette's RGB565 table, 1 -> its RGB555 table, anything else -> return 1)
//              +0x40 row stride   (slot +0xe0, 0x1000aaf0)
//              +0x44 clip RECT    (slot +0xcc, 0x1000a990)
//              +0x54 bounds RECT  (not read by this batch; filled for faithfulness)
//              +0x4c0 pixel base  (slot +0x10 copies it to +0x4cc, bumps the count at +0x4c8 when that copy
//                    is not NULL, and returns the COPY, so a surface with no base makes the guard return 3)
//   Palette  0x820 bytes, +0 = Palette_vtable (RVA 0x11db10): slot +0x18 returns this+0x40c (RGB565 table)
//            and slot +0x1c this+0x60c (RGB555), 256 16-bit entries each, filled with our own values.
//   Sprite   0x80 bytes, +0 = Sprite_vtable (RVA 0x11d380). +0x10 Palette, +0x14 payload, +0x2c pitch,
//            +0x30 width, +0x34 height. The payload is a PLAIN 8-bit bitmap, not run-length encoded
//            (log/c3/c3bh_notes.txt, confirmed for these ten in re/match/jgld_blit16*.cpp).
//
// THE PAYLOAD SITS IN THE MIDDLE OF ITS SLICE. Six of the eight bodies of every one of these functions walk
// the source cursor outside the payload rectangle: the three unscaled mirror bodies step the cursor backwards
// (`dec esi` 0x1001d2c8) or the rows upwards (`sub esi, [0x1012853c]` 0x1001d1ed), and the stepped half's
// vertical mirror starts on the LAST payload row (0x1001d761) and still advances forward, both inside the row
// and from row to row (`*0x10122e20 = (step >> 16) * pitch + 0x10128570`, 0x1001d78e..0x1001d7a2). Each
// sprite therefore gets a 0x4000-byte slice filled end to end with the alphabet and its +0x14 points at
// slice + 0x1000: every read of either arm is inside our own allocation and identical for both.
//
// THE FROZEN FIRST THREAD. Every fixture here suspends the game's first thread for the duration of the key's
// vectors and restores, before resuming it, every module global these bodies write. Two reasons, both
// measured by log/c3/alt/c3bp_probe.py over one 12 s boot at the menu:
//   * the live Sprite::draw16_1001bc40 0x1bc40 (1_704 entries in that boot, its caller 0x15480 89_378, both
//     on the first thread) parks its loop state in the same seven globals: 0x1012857c changed 462 times,
//     0x10128564 462, 0x10128560 462, 0x1012856c 122 and 0x10128578 6. This is c3av's Surface::fillRectC8
//     problem exactly, and the row counter is worse than a skip: the loops DECREMENT 0x10128578 in memory
//     (0x1001d124), so a value the game parks mid-run ends ours early or never.
//   * the three scale globals 0x10122dc0 / 0x10122dc4 / 0x10122dc8 decide which of the eight bodies runs, and
//     0x1bc40 reads all three on every call. They measured 1, 1, 1 and never changed, so a fixture that does
//     not write them can only reach ONE body; writing them while 0x1bc40 runs would mirror or scale what the
//     menu draws.
// The suspend happens after every allocation and module lookup, and nothing the vectors call allocates or
// takes a lock. The resume is a setTimeout(0): Frida's JS runtime is single-threaded, so it cannot run until
// diff_hook's `run` call has returned (the recipe c3av proved and c3bm re-checked).
//
// The scaled half's own globals (0x10128540, 0x10128544, 0x10128554, 0x10128558, 0x10128570, 0x10128580,
// 0x10128584, 0x10128588), the two float ratios (0x1012858c, 0x10128590), the flat colour slot (0x10128530)
// and the two table slots (0x10128548, 0x10128574) all measured 0 changes in the same boot and no function
// that writes them is ever entered by the game, so they ARE state regions of the keys below. So are the two
// ints the module pointers 0x10122e20 and 0x10122e1c address (the probe found them at jgld + 0x128550 and
// jgld + 0x12854c, adjacent as `[ebx + eax*4]` with eax 0 or -1 requires, and unchanged over the boot).
// The SEVEN SHARED loop globals are NOT, for the reason c3av recorded for 0x1012847c: they belong to the
// module, not to a vector. What they are set to is still verified through $pix, deterministically (a wrong
// source pitch, column count or row skip puts the written pixels at the wrong offsets and a wrong row count
// writes the wrong number of rows).
Object.assign(globalThis.DIFF_FIXTURES, {
  // The objects, with nothing frozen and no global written: the wrappers below add that. Built once per boot
  // (the tables are 1 MB); diff_hook snapshots and restores every state region around each vector, so the
  // content both arms start from is the same for every vector of every key.
  c3bp_objects() {
    if (globalThis.__c3bp) return globalThis.__c3bp;
    const jgld = Process.getModuleByName('jgld.dll').base;
    const vtSurface = jgld.add(0x11d0b0);
    const vtPalette = jgld.add(0x11db10);
    const vtSprite = jgld.add(0x11d380);

    // ---- two Palettes with four different tables, so a vector proves which object and which table was read.
    // Every entry is below 0x2000 because 0x27430 and 0x28630 index a 16-bit table with a colour and
    // 0x298b0 and 0x2bcf0 index one with the destination pixel, and because the loops write colours back
    // into the destination, where the next function to read one has to stay in range too.
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
    // = 1024 bytes) with room to spare.
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

    // ---- the sprite payloads: three 0x4000 slices, each filled end to end so that the six bodies that leave
    // the payload rectangle still read our own bytes. One alphabet drives every arm of the ten operations:
    //   0x00              0x2bcf0 writes 0 (0x1002bfa3)
    //   0x01, 0x05, 0x0f  0x2bcf0's plane arm (0x1002bfaa: value - 1 below 0xf)
    //   0x10              0x2bcf0's skip arm (value - 1 equal to 0xf)
    //   0x3c, 0x7f, 0xa5  ordinary colour indices
    //   0xf8              0x28630's destination remap (0x1002894e) and the first plane of 0x298b0
    //   0xf9, 0xfd        more planes of 0x298b0 (0x10029bd1) and the flag-gated skip of 0x2d890
    //   0xfe              transparent for 0x1f2d0 and 0x2bcf0, the last plane for 0x298b0
    //   0xff              transparent for the other eight
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
      [0, 2, 16, 16, 8],       // 2 spNP  no Palette of its own: the client fallback at 0x1001ce48
      [pal, 0, 4, 1, 1],       // 3 spD   1 x 1: one column and one row
      [pal, -1, 16, 16, 8],    // 4 spNB  no payload: the guard at 0x1001ce06
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

    // ---- the lookup tables the four table-driven members are handed as their fourth argument.
    //   $t   0xe4100 bytes of 16-bit entries, indexed in ENTRIES:
    //          0x27430 / 0x28630  by a colour or a destination pixel, both below 0x2000
    //          0x298b0            by ((payload - 0xf8) << 15) | destination, at most 0x31fff (0x10029be5)
    //          0x2bcf0            by ((payload - 1) << 15) | destination, at most 0x71fff (0x1002bfb0)
    //        so the largest byte offset any registered vector forms is 0xe3ffe.
    //   $t2  the same block read from 0x80 bytes in, so a vector proves which table pointer was used; the
    //        0x100 bytes of slack at the end keep its largest offset inside the allocation.
    //   $m   256 bytes, the index remap 0x26240 applies before the colour table (0x10026560); $m2 the same
    //        block read from 0x80 in, inside its 0x200 bytes.
    const t = Memory.alloc(0xe4100);
    for (let i = 0; i < 0x72080; i++) t.add(i * 2).writeU16((i * 7919 + 13) & 0x1fff);
    const m = Memory.alloc(0x200);
    for (let i = 0; i < 0x200; i++) m.add(i).writeU8((i * 37 + 11) & 0xff);
    fx.t = t;
    fx.t2 = t.add(0x80);
    fx.m = m;
    fx.m2 = m.add(0x80);

    // ---- our own palette client, for the module global at 0x1012873c (read at 0x1001ce48 when a sprite
    // carries no Palette of its own). Two of them: one with a Palette, one with none, which is the only way
    // to reach the `return 16` exit at 0x1001ce6f. The global is swapped by the wrappers below, with the
    // game's thread already suspended, and put back before it is resumed.
    const client = Memory.alloc(8);
    client.writeU32(0);
    client.add(4).writePointer(pal2);
    const clientNull = Memory.alloc(8);
    clientNull.writeU32(0);
    clientNull.add(4).writePointer(ptr(0));
    fx.client = client;
    fx.clientNull = clientNull;

    // ---- the module globals that are state regions of every key (see the header).
    fx.gflat = jgld.add(0x128530);      // 0x1f2d0's resolved colour
    fx.gvstep = jgld.add(0x128540);     // 0x10128540 the 16.16 vertical step, 0x10128544 its low half << 16
    fx.gtbl = jgld.add(0x128548);       // the fourth argument of 0x26240 / 0x27430 / 0x28630 / 0x2bcf0
    fx.gpos = jgld.add(0x128554);       // 0x10128554 / 0x10128558 the 16.16 start position
    fx.gcols = jgld.add(0x128570);      // 0x10128570 source columns consumed, 0x10128574 0x298b0's table
    fx.gxstep = jgld.add(0x128580);     // 580 integer step, 584, 588 the 16.16 step, 58c / 590 the ratios
    // the two ints the module pointers address, written at 0x1001d614 and 0x1001d62a and read back as
    // `[ebx + eax*4]` with eax 0 or -1 (0x1001d6d5), so the pair is (pointer - 4, pointer)
    const rowPtr = jgld.add(0x122e20).readPointer();
    fx.growpair = rowPtr.isNull() ? jgld.add(0x12854c) : rowPtr.sub(4);

    globalThis.__c3bp = fx;
    return fx;
  },
});

// Suspends the game's first thread, writes the three scale globals, swaps the palette client, and schedules
// the restore + resume for the moment diff_hook's `run` call returns. Everything it needs is read from `fx`,
// which the caller built (and cached) before the suspend.
//
//   scale  [horizontal numerator, vertical numerator, denominator] for 0x10122dc0 / 0x10122dc4 / 0x10122dc8
//   client the pointer to store into 0x1012873c
function c3bpFreeze(fx, scale, client) {
  const jgld = Process.getModuleByName('jgld.dll').base;
  const k32 = Process.getModuleByName('kernel32.dll');
  const openThread = new NativeFunction(k32.getExportByName('OpenThread'), 'pointer',
                                        ['uint32', 'int', 'uint32'], 'stdcall');
  const suspend = new NativeFunction(k32.getExportByName('SuspendThread'), 'uint32', ['pointer'], 'stdcall');
  const resume = new NativeFunction(k32.getExportByName('ResumeThread'), 'uint32', ['pointer'], 'stdcall');
  const closeHandle = new NativeFunction(k32.getExportByName('CloseHandle'), 'int', ['pointer'], 'stdcall');
  // Four ranges cover every global these ten bodies write, plus the client pointer:
  //   0x101284d0..0x10128598  the seven shared loop globals, the stepped half's own block and the three
  //                           parameter slots (and, at 0x1012854c / 0x10128550, the row-step pair)
  //   0x10122dc0..0x10122dcc  the scale numerators and the denominator
  //   0x1012873c              the palette client
  //   the row-step pair, again, by the address the module pointer actually holds
  const RANGES = [[jgld.add(0x1284d0), 0xc8], [jgld.add(0x122dc0), 0xc], [jgld.add(0x12873c), 4],
                  [fx.growpair, 8]];
  const threads = Process.enumerateThreads();
  const tid = threads.length ? threads[0].id : 0;
  if (!tid || tid === Process.getCurrentThreadId()) return fx;
  const h = openThread(0x0002 /* THREAD_SUSPEND_RESUME */, 0, tid);
  if (h.isNull()) return fx;
  if (suspend(h) === 0xffffffff) { closeHandle(h); return fx; }
  const saved = RANGES.map((r) => r[0].readByteArray(r[1]));
  globalThis.__c3bpFreeze = { tid: tid, resumed: -1 };
  if (scale) {
    jgld.add(0x122dc0).writeS32(scale[0]);
    jgld.add(0x122dc4).writeS32(scale[1]);
    jgld.add(0x122dc8).writeS32(scale[2]);
  }
  jgld.add(0x12873c).writePointer(client);
  setTimeout(function () {
    RANGES.forEach((r, i) => r[0].writeByteArray(saved[i]));
    globalThis.__c3bpFreeze.resumed = resume(h);
    closeHandle(h);
  }, 0);
  return fx;
}

// One fixture per setting of the three scale globals, because they pick which of the eight bodies runs (the
// round-5 rule: a function whose behaviour depends on a fixture-set global gets one registry entry per
// setting). `abs(0x10122dc0) == abs(0x10122dc8) && abs(0x10122dc4) == abs(0x10122dc8)` (0x1001cf8c and
// 0x1001cfb3) chooses the unscaled half, and the SIGNS of the two numerators (0x1001d045, 0x1001d052,
// 0x1001d209 unscaled; 0x1001d52b, 0x1001d538, 0x1001d87c stepped) choose the mirror body.
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
// `_nopal` points the palette client at a client with no Palette, the only way to the `return 16` exit.
function c3bpScale(scale) {
  const fx = globalThis.DIFF_FIXTURES.c3bp_objects();
  return c3bpFreeze(fx, scale, fx.client);
}

Object.assign(globalThis.DIFF_FIXTURES, {
  c3bp_s111() { return c3bpScale([1, 1, 1]); },
  c3bp_sm11() { return c3bpScale([-1, 1, 1]); },
  c3bp_s1m1() { return c3bpScale([1, -1, 1]); },
  c3bp_sm1m1() { return c3bpScale([-1, -1, 1]); },
  c3bp_s223() { return c3bpScale([2, 2, 3]); },
  c3bp_sm223() { return c3bpScale([-2, 2, 3]); },
  c3bp_s2m23() { return c3bpScale([2, -2, 3]); },
  c3bp_sm2m23() { return c3bpScale([-2, -2, 3]); },
  c3bp_s323() { return c3bpScale([3, 2, 3]); },
  // (+-5, +-5, 3): a magnification whose 16.16 horizontal step is 0x9999, the only setting registered here
  // whose fractional position is ever ODD. 0x56450 and 0x576a0 test bit 0 of that position in their stepped
  // halves (`mov eax, edx; and eax, 1; jne` at 0x10056d5b and its three twins, 0x10057fdf and its three),
  // and with an even step that test can only ever go one way.
  c3bp_s553() { return c3bpScale([5, 5, 3]); },
  c3bp_sm553() { return c3bpScale([-5, 5, 3]); },
  c3bp_s5m53() { return c3bpScale([5, -5, 3]); },
  c3bp_sm5m53() { return c3bpScale([-5, -5, 3]); },
  c3bp_nopal() {
    const fx = globalThis.DIFF_FIXTURES.c3bp_objects();
    return c3bpFreeze(fx, [1, 1, 1], fx.clientNull);
  },
});
