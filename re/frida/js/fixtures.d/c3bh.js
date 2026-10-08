// Fixtures for C3 batch c3bh: jgld.dll's 8-bit -> 16-bit block copy Surface::blitT_8to16 0x12db0.
//
// Built on batch c3bc's fixture (re/frida/js/fixtures.d/c3bc.js) with three changes the function forces:
//   * the per-surface pixel slice is 0x1000 bytes instead of 0x400, because two of the five copy bodies walk
//     each row forward from a cursor placed cols - 4 columns right of the rectangle (VA 0x10013357 and
//     0x100134b3) and step forward again by dstPitch + cols pixels per row, so a 10 x 6 copy reaches about
//     550 bytes past the start of the surface. Every such write stays inside $pix, which is one state region;
//   * three source surfaces carry planted bytes, because the last copy body counts its rows with the LAST
//     SOURCE BYTE of each row (`dec eax` at 0x100135a0, the register `mov al, byte ptr [esi]` at 0x10013587
//     loads into) and ends only on the row whose last byte is 1. A row ending in 0 makes the next lookup read
//     about 8 GB past the table, so the bytes the two A/B vectors of that body read last are set to 1 and 5;
//   * the surfaces are the pairs this function needs: 8-bit sources, 16-bit destinations, and six single
//     surfaces used as both (its `dst == this` split at 0x10013013).
//
// Nothing live is handed in: not the Display at 0x10128420, not a screen surface, not DirectDraw, not the
// module's own Palette. The one deliberate exception is a branch of the body: a surface with no Palette of its
// own sends the function to the client object the module global 0x1012873c points at (0x10012f5c) and it READS
// that client's Palette tables. c3ay's probe showed those tables stable, and log/c3/alt/c3bh_probe.py measured
// 0 entries into 0x12db0 and into its only caller Surface::blitT 0x91a0 over 12 s at the menu, and 0 changes
// in each of the four module globals the body parks loop state in (0x10128498, 0x1012848c, 0x10128494,
// 0x1012849c).
//
//   Surface  0x820 bytes, +0 = the real Surface vtable (RVA 0x11d0b0, re/analysis/systems/jgld_2.md). Fields:
//              +0x24 depth        (the pixel-address switch at VA 0x100088b3 reads it: 8 -> 1 byte per pixel,
//                                  16 -> 2, 24 -> 3, 32 -> 4, any other depth gives NULL)
//              +0x28 format code  (read as a FIELD by this function at VA 0x10012f7a: 0 = RGB565 table,
//                                  1 = RGB555 table, else return 1)
//              +0x40 row stride   (read as a FIELD at VA 0x10012fff and 0x10013008)
//              +0x44 clip RECT    (slot +0xcc returns this+0x44, VA 0x1000a9b0)
//              +0x54 bounds RECT  (width() slot +0xd8 is +0x5c - +0x54, height() slot +0xdc is +0x60 - +0x58)
//              +0x7c palette      (read directly at VA 0x10012f4b)
//              +0x4c0 pixel base  (slot +0x10 copies it to +0x4cc and bumps the use count at +0x4c8; this
//                                  function tests the field itself at VA 0x10012dd6 and returns 7 when 0)
//
//   Palette  0x820 bytes, +0 = the real Palette vtable (RVA 0x11db10, written by Palette::ctor 0x1006a150).
//            Slot +0x18 returns this+0x40c (0x1006b020) and slot +0x1c this+0x60c (0x1006b060), the two
//            256-entry 16-bit colour tables. Only those two are filled, with our own synthetic values.
//
// All pixel buffers live inside ONE block ($pix) so a single state region covers them and the final hash says
// which surface changed; the per-surface lock windows ($l0..$l17 = surface+0x4c0, 0x14 bytes) are separate
// regions so a missing slot-+0x24 release reads RED.
Object.assign(globalThis.DIFF_FIXTURES, {
  c3bh_surfaces() {
    const jgld = Process.getModuleByName('jgld.dll').base;
    const vtSurface = jgld.add(0x11d0b0);
    const vtPalette = jgld.add(0x11db10);

    // Two palettes with four different tables, so a vector proves which object and which table was read.
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

    // depth, format code, pitch, bounds {l,t,r,b}, clip {l,t,r,b}, hasBits, palette
    const SPECS = [
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal],    // 0  s8a   the 8-bit source
      [8, 0, 24, [0, 0, 24, 12], [0, 0, 24, 12], true, pal2],   // 1  s8b   24 x 12, the OTHER palette
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, 0],      // 2  s8np  no palette: the client fallback
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], false, pal],   // 3  s8nb  no pixel base: returns 7
      [12, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal],   // 4  s12   depth 12: every address is NULL
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal2],  // 5  d16a  16-bit destination, clip == bounds
      [16, 0, 32, [0, 0, 32, 16], [4, 2, 20, 12], true, pal2],  // 6  d16b  clip strictly inside: trims
      [16, 1, 19, [0, 0, 19, 11], [0, 0, 19, 11], true, pal2],  // 7  d16c  format 1: the RGB555 table
      [16, 2, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal2],  // 8  d16d  format 2: the switch default
      [16, 0, 32, [0, 0, 16, 8], [0, 0, 32, 4], true, pal2],    // 9  d16e  clip wider than the bounds
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], false, pal2], // 10 d16f  no pixel base: lockAt16 is NULL
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal],    // 11 m1    dst == this, format 0
      [8, 1, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal],    // 12 m1f1  dst == this, format 1
      [12, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal],   // 13 m12   dst == this, depth 12
      [8, 0, 32, [0, 0, 16, 8], [0, 0, 32, 8], true, pal],      // 14 mclip dst == this, clip wider than bounds
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal],    // 15 mb1   the last body, one row
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal],    // 16 mb2   the last body, two rows
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal],    // 17 mb3   the last body, cols 3, one row
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal2],  // 18 m16   blitT_16to16, dst == this
      [16, 0, 32, [0, 0, 16, 8], [0, 0, 32, 8], true, pal2],    // 19 m16c  the same, clip wider than bounds
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal2],  // 20 f16a  fill16: a seven-word alphabet
      [16, 0, 32, [0, 0, 32, 16], [4, 2, 20, 12], true, pal2],  // 21 f16b  the same with a clip inside
    ];
    const NAMES = ['s8a', 's8b', 's8np', 's8nb', 's12', 'd16a', 'd16b', 'd16c', 'd16d', 'd16e', 'd16f',
                   'm1', 'm1f1', 'm12', 'mclip', 'mb1', 'mb2', 'mb3', 'm16', 'm16c', 'f16a', 'f16b'];
    // The two fill16 surfaces hold a seven-value alphabet instead of the wide pattern, so a c1 drawn from it
    // matches about one word in seven over the whole rectangle and a c1 outside it matches none: both sides of
    // the compare at VA 0x1000b0ba run thousands of times in one vector.
    const ALPH16 = [0x0801, 0x1203, 0x2305, 0x3407, 0x4509, 0x560b, 0x670d];
    const ALPH16_FROM = 20;

    // One pixel block: surface i draws into $pix + i * 0x1000. 0x1000 bytes holds the widest surface here
    // (32 x 16 at stride 32 and 16 bits = 1024 bytes) with 3 KB of room for the two bodies that walk past the
    // rectangle; the furthest write any registered vector makes is about 550 bytes into a slice.
    const NSURF = SPECS.length, SLICE = 0x1000;
    const pix = Memory.alloc(NSURF * SLICE);
    for (let i = 0; i < NSURF; i++) {
      const base = i * SLICE;
      if (i >= ALPH16_FROM) {
        for (let k = 0; k < SLICE / 2; k++) pix.add(base + k * 2).writeU16(ALPH16[(k * 3 + i * 5) % 7]);
      } else if (SPECS[i][0] === 16) {
        // 16-bit slices: a wide pattern, different per surface, so every word a copy overwrites is visible.
        for (let k = 0; k < SLICE / 2; k++) pix.add(base + k * 2).writeU16((k * 2731 + i * 877 + 101) & 0xffff);
      } else {
        // 8-bit slices: a wide pattern, different per surface, so every byte a copy reads is visible.
        for (let j = 0; j < SLICE; j++) pix.add(base + j).writeU8((j * 37 + i * 101 + 11) & 0xff);
      }
    }

    // The three planted bytes of the last copy body (dst == this, dx >= sx, dy >= sy, VA 0x1001343e). Its row
    // loop ends when the last source byte of a row is 1 (`dec eax` 0x100135a0 over the byte loaded at
    // 0x10013587). With a surface of stride 32 and depth 8 the cursor starts at
    //     s0 = (sx + k) + (sy + h - 1) * 32,   k = cols < 4 ? cols - 1 : cols - 4   (0x10013442)
    // and the last byte of the first row is s0 + cols - 1; after a row the cursor moves by
    // cols - (32 - cols) = 2 * cols - 32 bytes (0x10013598).
    //   mb1, vector (sx 0, sy 0, dx 16, dy 4, cols 10, rows 3): s0 = 6 + 2 * 32 = 70, last byte 79 -> 1,
    //        so the loop copies exactly one row.
    //   mb2, the same vector: 79 -> 5 (the loop goes on), row 2 starts at 70 + 10 - 22 = 58 and its last byte
    //        67 -> 1, so the loop copies exactly two rows. The first row writes bytes 214..233 and the second
    //        212..231, so neither planted byte is overwritten before it is read.
    //   mb3, vector (sx 0, sy 0, dx 16, dy 4, cols 3, rows 3): k = 2, s0 = 2 + 2 * 32 = 66, last byte 68 -> 1.
    const plant = (surfIdx, off, v) => pix.add(surfIdx * SLICE + off).writeU8(v);
    plant(15, 79, 1);
    plant(16, 79, 5);
    plant(16, 67, 1);
    plant(17, 68, 1);

    const fx = { obj: pix, pix: pix, pal: pal, pal2: pal2 };

    const surf = Memory.alloc(NSURF * 0x820);
    SPECS.forEach((sp, i) => {
      const depth = sp[0], fmt = sp[1], pitch = sp[2], b = sp[3], k = sp[4], hasBits = sp[5], p = sp[6];
      const s = surf.add(i * 0x820);
      for (let o = 0; o < 0x820; o += 4) s.add(o).writeU32(0);
      s.writePointer(vtSurface);
      s.add(0x24).writeS32(depth);
      s.add(0x28).writeS32(fmt);
      s.add(0x40).writeS32(pitch);
      for (let j = 0; j < 4; j++) s.add(0x44 + j * 4).writeS32(k[j]);
      for (let j = 0; j < 4; j++) s.add(0x54 + j * 4).writeS32(b[j]);
      s.add(0x7c).writePointer(p ? p : ptr(0));
      s.add(0x4c0).writePointer(hasBits ? pix.add(i * SLICE) : ptr(0));
      fx[NAMES[i]] = s;
      fx['l' + i] = s.add(0x4c0);       // +0x4c0 base, +0x4c8 use count, +0x4cc current base
    });

    return fx;
  },

  // Sprite::remap8 0x1bae0 reads five fields of a Sprite and nothing else: the 8-bit bitmap pointer at +0x14
  // (null -> 7, VA 0x1001bb00), the flag word at +0x18 whose bit 0 chooses the byte count (0x1001bb37), the
  // packed count at +0x24 (0x1001bb41), and the pitch at +0x2c times the height at +0x34 (0x1001bb66). It calls
  // nothing and touches no global, so the fixture is seven plain structures and one block of bitmaps. The
  // vtable slot at +0 is set to jgld's own Sprite vtable (RVA 0x11d380) for faithfulness; the function never
  // reads it (its only indirect call is the frame check 0x1007e780).
  //
  // Every bitmap carries the same planted layout, which is what drives the eight compare sites of the block
  // unrolled four times. With `from` = 0x41 the matching indices are 3, 4, 5, 6, 7, 9, 14, 16 and 41:
  //   index 3 matches at the fourth position, so the NEXT group starts in the after-a-match chain (0x1001bbb2);
  //   indices 4, 5, 6, 7 then take the equal side of all four of its compares (0x1001bbb4, 0x1001bbbb,
  //   0x1001bbc2, 0x1001bbc9); index 8 takes the other side of the first, index 10 of the third, index 15 of
  //   the fourth; indices 9, 14 and 16 take the equal side of the no-match chain's compares at 0x1001bba0,
  //   0x1001bba5 and 0x1001bb9b, and indices 0, 1, 2, 11, 12, 13, 17, 18, 19 the other side. Index 41 is in the
  //   one-byte tail of the sprite whose count is 43 (0x1001bbd9), with 40 and 42 not matching.
  // Every other byte is forced away from 0x41, so the layout is exactly the list above.
  c3bh_sprites() {
    const jgld = Process.getModuleByName('jgld.dll').base;
    const vtSprite = jgld.add(0x11d380);

    const NSPR = 7, SLICE = 0x100;
    const bits = Memory.alloc(NSPR * SLICE);
    const MATCHES = [3, 4, 5, 6, 7, 9, 14, 16, 41];
    // A five-value alphabet that does NOT contain 0x41, so the planted list below is exactly the set of bytes
    // a `from` of 0x41 matches; the other four values give the vectors that pass a different `from` a spread
    // of their own.
    const ALPH = [0x40, 0x42, 0x43, 0x44, 0x45];
    for (let i = 0; i < NSPR; i++) {
      for (let j = 0; j < SLICE; j++) bits.add(i * SLICE + j).writeU8(ALPH[(j * 7 + i * 3) % 5]);
      for (const m of MATCHES) bits.add(i * SLICE + m).writeU8(0x41);
    }

    // bits?, flags, m_24, m_2c (pitch), m_34 (height)
    const SPECS = [
      [true, 1, 40, 0, 0],      // 0 sp0   bit 0 set: 40 bytes -> 10 groups, no tail
      [true, 1, 43, 0, 0],      // 1 sp1   43 bytes -> 10 groups and a 3-byte tail
      [true, 0, 0, 8, 5],       // 2 sp2   bit 0 clear: pitch 8 x height 5 = 40 -> 10 groups, no tail
      [true, 0, 0, 7, 5],       // 3 sp3   7 x 5 = 35 -> 8 groups and a 3-byte tail
      [true, 1, 3, 0, 0],       // 4 sp4   3 bytes -> 0 groups: the tail is skipped and nothing is written
      [false, 1, 40, 0, 0],     // 5 sp5   no bitmap -> 7
      [true, 0, 0, 5, 1],       // 6 sp6   5 x 1 = 5 -> 1 group and a 1-byte tail
    ];
    const SNAMES = ['sp0', 'sp1', 'sp2', 'sp3', 'sp4', 'sp5', 'sp6'];
    const fx = { obj: bits, spix: bits };
    const spr = Memory.alloc(NSPR * 0x80);
    SPECS.forEach((sp, i) => {
      const p = spr.add(i * 0x80);
      for (let o = 0; o < 0x80; o += 4) p.add(o).writeU32(0);
      p.writePointer(vtSprite);
      p.add(0x14).writePointer(sp[0] ? bits.add(i * SLICE) : ptr(0));
      p.add(0x18).writeS32(sp[1]);
      p.add(0x24).writeS32(sp[2]);
      p.add(0x2c).writeS32(sp[3]);
      p.add(0x34).writeS32(sp[4]);
      fx[SNAMES[i]] = p;
    });
    fx.spr = spr;
    return fx;
  },
});
