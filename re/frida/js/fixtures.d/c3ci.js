// Fixtures for C3 batch c3ci: the jgld.dll methods reached only through a slot of the Sprite vtable at
// RVA 0x11d380 (VA 0x1011d380), which Sprite::ctor 0x10014af0, Sprite::ctorN 0x10014cd0 and Sprite::dtor
// 0x10014de0 store at +0 of every Sprite. Built from re/frida/js/fixtures.d/c3bu.js, c3bz.js and c3bm.js,
// which the three earlier Sprite batches used for the blitters these slots dispatch to.
//
// Everything is OURS, built on jgld's own vtables so every accessor the bodies and their callees reach is
// jgld's own code; nothing live (the Display 0x10128420, a screen surface, a real Sprite) is passed in.
//   Surface  0x820 bytes, +0 = Surface_vtable (RVA 0x11d0b0). Fields reached here:
//              +0x24 depth       slot +0xe4 (0x1000ab30) returns this+0x24, and the dispatchers read
//                                *format(), so the DEPTH is what picks their inner arm
//              +0x28 format code element [1] of the same pointer: the blitters use it to pick the Palette
//                                table (0 -> RGB565 at Palette+0x40c, 1 -> RGB555 at +0x60c, else return 1)
//              +0x40 row stride  slot +0xe0 (0x1000aaf0)
//              +0x44 clip RECT   slot +0xcc (0x1000a990)
//              +0x54 bounds RECT slot +0xd4 (0x1000aa30)
//              +0x4c0 pixel base slot +0x10 copies it to +0x4cc, bumps the count at +0x4c8 when that copy is
//                                not NULL, and returns the copy, so a surface with no base makes a blitter's
//                                guard return 3
//   Palette  0x820 bytes, +0 = Palette_vtable (RVA 0x11db10): slot +0x18 returns this+0x40c (the RGB565
//            table) and slot +0x1c this+0x60c (RGB555), 256 16-bit entries each, our own values.
//   Sprite   0x80 bytes, +0 = Sprite_vtable (RVA 0x11d380). +0x10 key colour (slots 4 and 5), +0x14 payload
//            (slot 8, which every dispatcher calls as its validity test), +0x18 flag word (slot 0x180e0
//            returns bit 0, which the dispatchers call as isBusy), +0x20 bits per pixel (slot 10, the outer
//            switch of every dispatcher), +0x24 the span byte count remap8 uses when bit 0 of +0x18 is set,
//            +0x28 (slot 7), +0x2c pitch (slot 11), +0x30 width (slot 12), +0x34 height (slot 13).
//            The payload is a PLAIN 8-bit bitmap, not run-length encoded (log/c3/c3bh_notes.txt).
//
// THE PAYLOAD SITS IN THE MIDDLE OF ITS SLICE, for the reason c3bp recorded (finding 2 of
// log/c3/c3bp_notes.txt): six of the eight bodies of every draw16_* blitter walk the source cursor outside
// the payload rectangle. Each sprite therefore points into a 0x4000-byte slice filled end to end with the
// alphabet, so every read of either arm is inside our own allocation and identical for both.
//
// THE FROZEN FIRST THREAD, for the keys whose vectors reach a blitter. The live Sprite::draw16_1001bc40
// 0x1bc40 and Sprite::draw_1005ed80 0x5ed80 park their loop state in the same module globals and DECREMENT
// the row counters 0x10128578 / 0x10128670 in memory, and the three scale globals 0x10122dc0 / 0x10122dc4 /
// 0x10122dc8 (which pick which of a blitter's eight bodies runs) are read on every one of those calls. The
// wrapper below suspends the game's first thread, writes the scale globals, swaps the palette client at
// 0x1012873c, and restores every range before it resumes (the recipe c3av proved and c3bm, c3bp, c3bu and
// c3bz re-checked; the resume is a setTimeout(0), which Frida's single-threaded runtime cannot run until
// diff_hook's `run` call has returned).
//
// THE PLAIN FIXTURE. The accessor slots, Sprite::remap 0x150d0 and Sprite::drawQ 0x175d0 reach no blitter:
// remap8 0x1bae0 reads and writes only the sprite's own payload (c3bh: a leaf with no global) and drawQ's
// every arm is a _CrtDbgReport. Those keys use c3ci_plain, which freezes nothing and writes no global.
Object.assign(globalThis.DIFF_FIXTURES, {
  // Built once per boot and cached; diff_hook snapshots and restores every state region around each vector,
  // so the content both arms start from is the same for every vector of every key.
  c3ci_objects() {
    if (globalThis.__c3ci) return globalThis.__c3ci;
    const jgld = Process.getModuleByName('jgld.dll').base;
    const vtSurface = jgld.add(0x11d0b0);
    const vtPalette = jgld.add(0x11db10);
    const vtSprite = jgld.add(0x11d380);

    // ---- two Palettes with four different tables, so a vector proves which object and which table was
    // read. Every entry is below 0x2000 because the blitters write colours back into a destination whose
    // pixels are read again by the next vector's lookups.
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
    // = 1024 bytes); the 8-bit destinations treat the same stride as BYTES and use 512 of them.
    const NSURF = 14, SLICE = 0x1000;
    const pix = Memory.alloc(NSURF * SLICE);

    //      depth fmt pitch bounds            clip               base
    const SPECS = [
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   //  0 d0  16-bit, clip == bounds
      [16, 0, 32, [0, 0, 32, 16], [4, 2, 20, 12], true],   //  1 d1  clip strictly inside: trims the rectangle
      [16, 1, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   //  2 d2  format 1: the RGB555 table
      [16, 2, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   //  3 d3  format 2: the blitter's switch default
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], false],  //  4 d4  no pixel base: the blitter's lock gives 0
      [16, 0, 24, [0, 0, 24, 12], [0, 0, 24, 12], true],   //  5 d5  narrower, stride 24
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   //  6 d6  bltS2's second destination
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],    //  7 e0  8-bit destination
      [8, 0, 32, [0, 0, 32, 16], [4, 2, 20, 12], true],    //  8 e1  8-bit, clip strictly inside
      [8, 0, 24, [0, 0, 24, 12], [0, 0, 24, 12], true],    //  9 e2  8-bit, stride 24
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], false],   // 10 e3  8-bit, no pixel base
      [24, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   // 11 x0  depth 24: the dispatchers' format default
      [12, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],   // 12 x1  depth 12: the same arm, another value
      [0, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true],    // 13 x2  depth 0: the same arm, zero
    ];
    const SNAMES = ['d0', 'd1', 'd2', 'd3', 'd4', 'd5', 'd6', 'e0', 'e1', 'e2', 'e3', 'x0', 'x1', 'x2'];
    for (let i = 0; i < NSURF; i++) {
      const b0 = i * SLICE;
      if (SPECS[i][0] === 16) {
        for (let k = 0; k < SLICE / 2; k++) pix.add(b0 + k * 2).writeU16((k * 4099 + i * 311 + 7) & 0x1fff);
      } else {
        for (let j = 0; j < SLICE; j++) pix.add(b0 + j).writeU8((j * 37 + i * 13 + 5) & 0xff);
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

    // ---- the sprite payloads: four 0x4000 slices, each filled end to end so that the bodies that leave the
    // payload rectangle still read our own bytes. The alphabet drives every arm of the blitters these slots
    // reach: 0x00..0xa5 ordinary colour indices, 0xf8 the destination-remap byte of 0x60cf0 / 0x62dd0 and
    // the first byte the blend members skip, 0xf9 / 0xfd more bytes above 0xf8, 0xfe transparent for every
    // member whose threshold is 0xfe, 0xff transparent for the rest.
    const NPAY = 4, PAY = 0x4000;
    const pay = Memory.alloc(NPAY * PAY);
    const ALPH = [0x00, 0x01, 0x05, 0x0f, 0x10, 0x3c, 0x7f, 0xa5, 0xf8, 0xf9, 0xfd, 0xfe, 0xff];
    const STEP = [5, 3, 7, 11];
    for (let i = 0; i < NPAY; i++) {
      for (let j = 0; j < PAY; j++) pay.add(i * PAY + j).writeU8(ALPH[(j * STEP[i] + i * 4) % 13]);
    }

    // ---- the sprites, in ONE block so that a single state region says whether any field moved. Word +0x10
    // is the one the two sets differ in: the dispatchers treat it as the key colour (slot 4 writes it, slot
    // 5 reads it, and every dispatcher saves and restores it around its work), while the blitters read the
    // SAME word as their Palette pointer. The sixteen `sp*` / `a*` sprites below therefore carry a small
    // distinct integer there and are used by the accessor, remap and drawQ keys, which reach no blitter;
    // the nine `b*` sprites carry a Palette and are the ones handed to a dispatcher or a blt forwarder.
    //   palette (unused here), payload slice (-1 = none), payload offset, m_10, m_18, m_20, m_24, m_28,
    //   pitch, width, height
    const SP = [
      [pal, 0, 0x1000, 0x11, 0, 8, 0x30, 0x00, 16, 16, 8],     //  0 spA   8 bpp, bit 0 of +0x18 clear
      [pal2, 1, 0x1000, 0x12, 0, 8, 0x31, 0x11, 20, 20, 10],   //  1 spB   wider
      [0, 2, 0x1000, 0x13, 0, 8, 0x32, 0x22, 16, 16, 8],       //  2 spNP
      [pal, 0, 0x1040, 0x14, 0, 8, 0x33, 0x55, 4, 1, 1],       //  3 spD   1 x 1: four payload bytes
      [pal, -1, 0, 0x15, 0, 8, 0x34, 0x66, 16, 16, 8],         //  4 spNB  no payload: slot 8 returns 0
      [pal, 3, 0x1000, 0x16, 0, 8, 0x35, 0x77, 16, 16, 8],     //  5 spW
      [pal, 0, 0x13c0, 0x17, 1, 8, 0x36, 0x88, 16, 16, 8],     //  6 spBsy bit 0 of +0x18 set: isBusy, and
                                                               //          remap8 takes its length from +0x24
      [pal, 1, 0x1000, 0x18, 0, 0x10, 0x37, 0x99, 16, 16, 8],  //  7 sp16  +0x20 = 16: the second outer case
      [pal, 2, 0x1000, 0x19, 0, 24, 0x38, 0xaa, 16, 16, 8],    //  8 spX   +0x20 = 24: the outer default
      [pal2, 3, 0x1080, 0x21, 2, 1, 0x40, 0x41, 7, 3, 5],      //  9 a0
      [0, 0, 0x1100, 0x22, 3, 2, 0x41, 0x7f, 9, 4, 6],         // 10 a1    bit 0 of +0x18 set
      [pal, 1, 0x1180, 0x23, 0, 4, 0x42, 0xfe, 11, 5, 7],      // 11 a2
      [pal2, 2, 0x1200, 0x24, 1, 0x20, 0x43, 0xff, 13, 6, 9],  // 12 a3    bit 0 set
      [0, 3, 0x1280, 0x25, 5, 0x18, 0x44, 0x02, 15, 7, 11],    // 13 a4    bit 0 set
      [pal, -1, 0, 0x26, 7, 0x30, 0x45, 0x80, 17, 8, 13],      // 14 a5    no payload and bit 0 set
      [pal2, 0, 0x1300, 0x27, 9, 0x40, 0x46, 0x33, 19, 9, 15], // 15 a6    bit 0 set
    ];
    const SPN = ['spA', 'spB', 'spNP', 'spD', 'spNB', 'spW', 'spBsy', 'sp16', 'spX',
                 'a0', 'a1', 'a2', 'a3', 'a4', 'a5', 'a6'];
    //   palette, payload slice (-1 = none), payload offset, m_18, m_20, pitch, width, height
    const BSP = [
      [pal, 0, 0x1000, 0, 8, 16, 16, 8],      // 0 bA    the main sprite
      [pal2, 1, 0x1000, 0, 8, 20, 20, 10],    // 1 bB    wider, the other Palette and alphabet phase
      [0, 2, 0x1000, 0, 8, 16, 16, 8],        // 2 bNP   no Palette of its own: the client's is used
      [pal, 0, 0x1040, 0, 8, 4, 1, 1],        // 3 bD    1 x 1
      [pal, -1, 0, 0, 8, 16, 16, 8],          // 4 bNB   no payload: slot 8 returns 0
      [pal, 3, 0x1000, 0, 8, 16, 16, 8],      // 5 bW    the weight sprite of bltS / bltS2
      [pal, 1, 0x1000, 1, 8, 16, 16, 8],      // 6 bBsy  bit 0 of +0x18 set: isBusy
      [pal, 2, 0x1000, 0, 0x10, 16, 16, 8],   // 7 b16   +0x20 = 16: a dispatcher's second outer case
      [pal, 3, 0x1000, 0, 24, 16, 16, 8],     // 8 bX    +0x20 = 24: a dispatcher's outer default
    ];
    const BSPN = ['bA', 'bB', 'bNP', 'bD', 'bNB', 'bW', 'bBsy', 'b16', 'bX'];
    const spr = Memory.alloc((SP.length + BSP.length) * 0x80);
    SP.forEach((s, i) => {
      const p = spr.add(i * 0x80);
      for (let o = 0; o < 0x80; o += 4) p.add(o).writeU32(0);
      p.writePointer(vtSprite);
      p.add(0x10).writeS32(s[3]);
      p.add(0x14).writePointer(s[1] < 0 ? ptr(0) : pay.add(s[1] * PAY + s[2]));
      p.add(0x18).writeS32(s[4]);
      p.add(0x20).writeS32(s[5]);
      p.add(0x24).writeS32(s[6]);
      p.add(0x28).writeS32(s[7]);
      p.add(0x2c).writeS32(s[8]);
      p.add(0x30).writeS32(s[9]);
      p.add(0x34).writeS32(s[10]);
      fx[SPN[i]] = p;
    });
    BSP.forEach((s, i) => {
      const p = spr.add((SP.length + i) * 0x80);
      for (let o = 0; o < 0x80; o += 4) p.add(o).writeU32(0);
      p.writePointer(vtSprite);
      p.add(0x10).writePointer(s[0] ? s[0] : ptr(0));
      p.add(0x14).writePointer(s[1] < 0 ? ptr(0) : pay.add(s[1] * PAY + s[2]));
      p.add(0x18).writeS32(s[3]);
      p.add(0x20).writeS32(s[4]);
      p.add(0x24).writeS32(0x40);
      p.add(0x2c).writeS32(s[5]);
      p.add(0x30).writeS32(s[6]);
      p.add(0x34).writeS32(s[7]);
      fx[BSPN[i]] = p;
    });
    fx.spr = spr;
    fx.nspr = (SP.length + BSP.length) * 0x80;
    fx.spix = pay;

    // ---- the lookup tables the table-driven blitters are handed.
    //   $t   0xe4100 bytes of 16-bit entries masked to 0x1fff: 0x298b0's fourth argument (blt16a), parked at
    //        0x10128574. It is indexed in ENTRIES by ((payload - 0xf8) << 15) | destination (VA 0x10029be5),
    //        so with a payload alphabet reaching 0xfe and destination pixels below 0x2000 the largest byte
    //        offset is 0x63ffe; c3bp sized the same table 0xe4100 for the same reason. A 0x10200-byte table
    //        read GREEN on the first run here and then "access violation accessing 0x505204c" once another
    //        allocation moved, which is what reading past it looks like. $t2 is the same block read from 0x80
    //        in, so a vector proves which pointer the loop used, and the slack at the end keeps it in range.
    //   $m   256-byte index remap, 0x26240's fourth argument (drawH's 16-bit arm), parked at 0x10128548.
    //   $b   a 64 KB BYTE table: 0x63f60 (drawL) indexes it with (payload << 8) | destination, 0x61db0
    //        (drawH's 8-bit arm), 0x60cf0 (drawF) and 0x62dd0 (drawJ) with a single byte.
    const t = Memory.alloc(0xe4100);
    for (let i = 0; i < 0x72080; i++) t.add(i * 2).writeU16((i * 7919 + 13) & 0x1fff);
    const m = Memory.alloc(0x200);
    for (let i = 0; i < 0x200; i++) m.add(i).writeU8((i * 37 + 11) & 0xff);
    const b = Memory.alloc(0x10100);
    for (let i = 0; i < 0x10100; i++) b.add(i).writeU8((i * 97 + 29) & 0xff);
    fx.t = t;
    fx.t2 = t.add(0x80);
    fx.m = m;
    fx.m2 = m.add(0x80);
    fx.b = b;
    fx.b2 = b.add(0x80);
    //   $c8  the 256-byte colour table 0x62dd0 (the blitter drawJ forwards to) reads from the module global
    //        at 0x10128658 (0x10063023) and that no argument carries. c3bu measured that global at 0 over a
    //        12 s boot at the menu, so without the fixture installing one the function dereferences a null
    //        pointer: the first Sprite::drawJ run here read "access violation accessing 0xd9". The freeze
    //        wrapper installs this table with the game's thread already down and the restore puts the
    //        module's value back.
    const c8 = Memory.alloc(0x100);
    for (let i = 0; i < 0x100; i++) c8.add(i).writeU8((i * 151 + 71) & 0xff);
    fx.c8 = c8;

    // ---- our own palette client, for the module global at 0x1012873c, which a 16-bit blitter reads when
    // the sprite carries no Palette. Two of them: one with a Palette, one with none, which is the only way
    // to the blitters' `return 16` exit.
    const client = Memory.alloc(8);
    client.writeU32(0);
    client.add(4).writePointer(pal2);
    const clientNull = Memory.alloc(8);
    clientNull.writeU32(0);
    clientNull.add(4).writePointer(ptr(0));
    fx.client = client;
    fx.clientNull = clientNull;

    // ---- the module globals that are state regions of the blitter-reaching keys (see the header of
    // registry.d/c3ci.py). The 16-bit block:
    fx.gmask = jgld.add(0x128530);      // 0x10128530 / 34 / 38, the flat-colour and mask slots
    fx.gvstep = jgld.add(0x128540);     // 0x10128540 the 16.16 vertical step, 0x10128544 its low half << 16
    fx.gtbl = jgld.add(0x128548);       // the 256-byte index remap slot
    fx.gpos = jgld.add(0x128554);       // 0x10128554 / 0x10128558 the 16.16 start position
    fx.glevel = jgld.add(0x128568);
    fx.gcols = jgld.add(0x128570);      // 0x10128570 source columns consumed, 0x10128574 the 16-bit table
    fx.gxstep = jgld.add(0x128580);     // 580 integer step, 584, 588 the 16.16 step, 58c / 590 the ratios
    // The 8-bit block:
    fx.g8vstep = jgld.add(0x128624);    // 0x10128624 the 16.16 vertical step, 0x10128628 its low half << 16
    fx.g8tbl = jgld.add(0x12862c);      // the byte-table argument slot
    fx.g8pos = jgld.add(0x12863c);      // 0x1012863c / 0x10128640 the 16.16 start position
    fx.g8cols = jgld.add(0x128660);     // source columns consumed
    fx.g8colour = jgld.add(0x128664);
    fx.g8pix = jgld.add(0x12866c);      // the fifth argument of 0x62dd0
    fx.g8step = jgld.add(0x12867c);     // 0x1012867c integer step, 0x10128680 source rows consumed
    fx.g8xstep = jgld.add(0x128688);    // 688 the 16.16 step, 68c / 690 the ratios
    // the two ints each module pointer pair addresses, written as `*p = ...` and read back as
    // `[ebx + eax*4]` with eax 0 or -1, so each pair is (pointer - 4, pointer). c3bu's probe read
    // 0x10122e20 -> jgld + 0x128550 and 0x10122e58 -> jgld + 0x128638, both unchanged over a 12 s boot.
    const rowPtr = jgld.add(0x122e20).readPointer();
    fx.growpair = rowPtr.isNull() ? jgld.add(0x12854c) : rowPtr.sub(4);
    const rowPtr8 = jgld.add(0x122e58).readPointer();
    fx.g8rowpair = rowPtr8.isNull() ? jgld.add(0x128634) : rowPtr8.sub(4);

    globalThis.__c3ci = fx;
    return fx;
  },
});

// Suspends the game's first thread, writes the three scale globals, swaps the palette client, and schedules
// the restore + resume for the moment diff_hook's `run` call returns. Everything it needs is read from `fx`,
// which the caller built (and cached) before the suspend.
function c3ciFreeze(fx, scale, client) {
  const jgld = Process.getModuleByName('jgld.dll').base;
  // Five ranges cover every global the blitters these slots reach write, plus the two slots they only read:
  //   0x101284d0..0x10128698  the shared 16-bit loop globals, the 16-bit stepped half's block, the draw16t
  //                           block, the row-step pair at 0x1012854c / 0x10128550 and the WHOLE 8-bit block
  //   0x10122dc0..0x10122dcc  the scale numerators and the denominator
  //   0x1012873c              the palette client
  //   the two row-step pairs, again, by the addresses the module pointers actually hold
  //   0x10123378              jgld's _CrtDbgMode[_CRT_WARN] (see c3ciQuiet below)
  const RANGES = [[jgld.add(0x1284d0), 0x1c8], [jgld.add(0x122dc0), 0xc], [jgld.add(0x12873c), 4],
                  [fx.growpair, 8], [fx.g8rowpair, 8], [jgld.add(0x123378), 4]];
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
  jgld.add(0x123378).writeS32(0);
  return fx;
}

// THE DEBUG REPORTS HAVE TO BE SILENCED FOR THE RUN. Every arm of a dispatcher that reaches no blitter is a
// `_RPTF0(_CRT_WARN, ...)`, and jgld.dll's own _CrtDbgMode[_CRT_WARN] (the dword at RVA 0x123378, 2 in the
// file) is _CRTDBG_MODE_DEBUG, so _CrtDbgReport 0x10081f90 calls OutputDebugStringA (VA 0x10082283).
// OutputDebugStringA raises DBG_PRINTEXCEPTION_C, which Frida's NativeFunction steals and reports as
// "Error: system error", and that aborts the whole rpc call before a single vector is recorded (measured:
// the first run of Sprite::remap and Sprite::drawQ failed that way and both pass with this in place).
// Writing 0 there makes _CrtDbgReport take none of its three output arms (`and ecx, 1` VA 0x1008226e,
// `and ecx, 2` VA 0x10082275, `and ecx, 4` VA 0x10082293 all fall through) and return 0 through
// `xor eax, eax` at VA 0x10082319, which is what it returns in mode 2 as well, so neither arm's return value
// nor its memory changes. The value is restored before the rpc call returns.
function c3ciQuiet() {
  const jgld = Process.getModuleByName('jgld.dll').base;
  const p = jgld.add(0x123378);
  const saved = p.readByteArray(4);
  p.writeS32(0);
  setTimeout(function () { p.writeByteArray(saved); }, 0);
}

Object.assign(globalThis.DIFF_FIXTURES, {
  // no freeze and no blitter: only the debug-report mode is silenced and put back
  c3ci_plain() {
    const fx = globalThis.DIFF_FIXTURES.c3ci_objects();
    c3ciQuiet();
    return fx;
  },
  // the unscaled body of every blitter, with a palette client that carries a Palette
  c3ci_s111() {
    const fx = globalThis.DIFF_FIXTURES.c3ci_objects();
    return c3ciFreeze(fx, [1, 1, 1], fx.client);
  },
  // the 16.16 stepped body (step 1.5 in both directions)
  c3ci_s223() {
    const fx = globalThis.DIFF_FIXTURES.c3ci_objects();
    return c3ciFreeze(fx, [2, 2, 3], fx.client);
  },
  // the unscaled body with a palette client carrying no Palette: the blitters' `return 16` exit
  c3ci_nopal() {
    const fx = globalThis.DIFF_FIXTURES.c3ci_objects();
    return c3ciFreeze(fx, [1, 1, 1], fx.clientNull);
  },
});
