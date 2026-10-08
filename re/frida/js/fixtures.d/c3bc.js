// Fixtures for C3 batch c3bc: jgld.dll's two surface-to-surface copies (Surface::blitT_8to8 0x13c50,
// Surface::blitKey8to16 0x12900), the shade blend Surface::blit16from16 0xf6a0 and the two Bresenham line
// drawers Surface::line8 0xfb70 and Surface::line16 0xb140.
//
// Everything the five functions read or write belongs to this fixture. The Surface and Palette objects are
// built on jgld's OWN vtables, so every virtual they call is the module's code, but no live object is handed
// in: not the Display at 0x10128420, not a screen surface, not DirectDraw, not the module's own Palette. The
// one exception is deliberate and is a branch of two of the bodies: a surface with no Palette of its own sends
// blitKey8to16 (VA 0x10012a90) and line16 (VA 0x1000b1d3) to the client object the module global 0x1012873c
// points at, and they READ the two colour tables of its Palette. c3ay's probe showed those tables stable.
//
//   Surface  0x820 bytes, +0 = the real Surface vtable (RVA 0x11d0b0, re/analysis/systems/jgld_2.md). Fields:
//              +0x24 depth        (slot +0xe4 returns this+0x24; the pixel-address switch at VA 0x100088b3
//                                  reads it: 8 -> 1 byte per pixel, 16 -> 2, 24 -> 3, 32 -> 4, else NULL)
//              +0x28 format code  (element [1] of the slot-+0xe4 pointer: 0 = RGB565, 1 = RGB555, else abort)
//              +0x40 row stride   (slot +0xe0 returns it at VA 0x1000ab10; blitT_8to8 reads the FIELD directly
//                                  at VA 0x10013e99 / 0x10013ea2)
//              +0x44 clip RECT    (slot +0xcc returns this+0x44, VA 0x1000a9b0; slot +0xc8 copies it out)
//              +0x54 bounds RECT  (slot +0xd4 returns this+0x54, VA 0x1000aa50; width() slot +0xd8 is
//                                  +0x5c - +0x54, height() slot +0xdc is +0x60 - +0x58)
//              +0x7c palette      (read directly by blitKey8to16 at VA 0x10012a7f and line16 at 0x1000b1c2)
//              +0x4c0 pixel base  (slot +0x10 copies it to +0x4cc and bumps the use count at +0x4c8;
//                                  blitT_8to8 tests the field itself at VA 0x10013c76 and returns 7 when 0)
//
//   Palette  0x820 bytes, +0 = the real Palette vtable (RVA 0x11db10, written by Palette::ctor 0x1006a150).
//            Slot +0x18 returns this+0x40c (0x1006b020) and slot +0x1c this+0x60c (0x1006b060), the two
//            256-entry 16-bit colour tables. Only those two are filled, with our own synthetic values.
//
// All nineteen pixel buffers live inside ONE block ($pix) so a single state region covers them and the final
// hash says which surface changed; the per-surface lock windows ($l0..$l18 = surface+0x4c0, 0x14 bytes) are
// separate regions so a missing slot-+0x24 release reads RED. $rects is a state region too: blit16from16
// intersects its RECT argument IN PLACE (VA 0x1000f706).
//
// The module globals the five bodies park loop state in (0x101284b4, 0x1012848c, 0x10128490, 0x10128494,
// 0x1012849c, 0x101284a0, 0x101284a4) are deliberately NOT state regions and are not touched here, for the
// reason c3av recorded for 0x1012847c: they belong to the module, not to a vector. log/c3/alt/c3bc_probe.py
// measured 0 changes in each of them over 12 s at the menu and 0 entries into all five functions. What each
// one is set to is still verified, deterministically, through $pix: a wrong row skip puts later rows at the
// wrong offsets and a wrong count writes the wrong number of pixels per row.
Object.assign(globalThis.DIFF_FIXTURES, {
  c3bc_surfaces() {
    const jgld = Process.getModuleByName('jgld.dll').base;
    const vtSurface = jgld.add(0x11d0b0);
    const vtPalette = jgld.add(0x11db10);

    // Two palettes with four different tables, so a vector proves which object and which table was read.
    const mkPal = (a565, b565, a555, b555) => {
      const p = Memory.alloc(0x820);
      for (let o = 0; o < 0x820; o += 4) p.add(o).writeU32(0);
      p.writePointer(vtPalette);
      const t565 = [], t555 = [];
      for (let i = 0; i < 256; i++) {
        t565.push((i * a565 + b565) & 0xffff);
        t555.push((i * a555 + b555) & 0xffff);
        p.add(0x40c + i * 2).writeU16(t565[i]);
        p.add(0x60c + i * 2).writeU16(t555[i]);
      }
      return { p: p, t565: t565, t555: t555 };
    };
    const pal = mkPal(0x0105, 0x1234, 0x0203, 0x5678);
    const pal2 = mkPal(0x0307, 0x9abc, 0x0409, 0xdef0);

    // depth, format code, pitch, bounds {l,t,r,b}, clip {l,t,r,b}, hasBits, palette
    const SPECS = [
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal],    // s0  16-bit, clip == bounds
      [16, 0, 32, [0, 0, 32, 16], [4, 2, 20, 12], true, pal],    // s1  16-bit, clip strictly inside
      [16, 1, 19, [0, 0, 19, 11], [0, 0, 19, 11], true, pal],    // s2  16-bit RGB555, odd width and stride
      [16, 2, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal],    // s3  format code 2: the switch default
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], false, pal],   // s4  no pixel base: every address is NULL
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, 0],      // s5  no palette: the client fallback
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal2],   // s6  the second palette
      [16, 0, 32, [0, 0, 16, 8], [0, 0, 32, 4], true, pal],      // s7  clip wider than the bounds
      [16, 1, 19, [0, 0, 19, 11], [0, 0, 19, 11], true, pal],    // s8  the 19-wide twin of s2
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal],     // s9  8-bit, clip == bounds
      [8, 0, 24, [0, 0, 24, 12], [0, 0, 24, 12], true, pal],     // s10 8-bit 24 x 12
      [8, 0, 24, [0, 0, 24, 12], [2, 1, 18, 9], true, pal],      // s11 8-bit, clip strictly inside
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], false, pal],    // s12 8-bit, no pixel base
      [12, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal],    // s13 depth 12: the address switch has no case
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal],     // s14 8-bit, the same-surface copy target
      [8, 0, 32, [0, 0, 16, 8], [0, 0, 32, 4], true, pal],       // s15 8-bit, clip wider than the bounds
      [8, 0, 32, [0, 0, 32, 16], [4, 2, 20, 12], true, pal],     // s16 8-bit, clip strictly inside (line8)
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, 0],       // s17 8-bit, no palette: the client fallback
      [8, 0, 32, [0, 0, 16, 4], [0, 0, 32, 16], true, pal],      // s18 8-bit, clip TALLER and wider than bounds
    ];

    // One pixel block: surface i draws into $pix + i * 0x400. 0x400 bytes is 512 words, and the widest surface
    // here is 32 x 16 at stride 32, whose last 16-bit word ends at (15 * 32 + 31) * 2 + 1 = 1023.
    const NSURF = SPECS.length, STRIDE = 0x400, PIX = NSURF * STRIDE;
    const pix = Memory.alloc(PIX);
    for (let i = 0; i < NSURF; i++) {
      const base = i * STRIDE;
      if (SPECS[i][0] === 16) {
        // 16-bit slices carry a seven-value alphabet taken from the palette table the surface's format code
        // selects, so a blitKey8to16 key in 0x40..0x46 matches roughly one word in seven and 0x50 none; one
        // word in thirteen is 0x7c1f, the magic blit16from16 tests for at VA 0x1000f7c6. Neither palette maps
        // any index to 0x7c1f, so the two never collide.
        const tbl = SPECS[i][1] === 1 ? pal.t555 : pal.t565;
        for (let k = 0; k < STRIDE / 2; k++) {
          const v = ((k + i) % 13) === 5 ? 0x7c1f : tbl[0x40 + ((k * 3 + i * 5) % 7)];
          pix.add(base + k * 2).writeU16(v);
        }
      } else {
        // 8-bit slices carry a wide pattern, different per surface, so every byte a copy moves is visible.
        for (let j = 0; j < STRIDE; j++) pix.add(base + j).writeU8((j * 37 + i * 101 + 11) & 0xff);
      }
    }

    const fx = { obj: pix, pix: pix, pal: pal.p, pal2: pal2.p };

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
      s.add(0x7c).writePointer(p ? p.p : ptr(0));
      s.add(0x4c0).writePointer(hasBits ? pix.add(i * STRIDE) : ptr(0));
      fx['s' + i] = s;
      fx['l' + i] = s.add(0x4c0);       // +0x4c0 base, +0x4c8 use count, +0x4cc current base
    });

    // blit16from16's shade table. The index is (15 - n) << 15 OR'd with a 16-bit colour in its low half, so the
    // largest index is 7 << 16 | 0xffff = 0x7ffff and the table needs 0x80000 words. $tbl2 is the same block
    // seen 0x80 bytes in, which proves the `table` argument is the pointer the loop indexes.
    // The entry must depend on the WHOLE index: any `i * m & 0xffff` is periodic with 0x10000, which would make
    // the shade's high half invisible and leave only two distinct results across the alpha vectors.
    const TBL = 0x80000, tbl = Memory.alloc(TBL * 2 + 0x100);
    for (let i = 0; i < TBL + 0x80; i++) {
      tbl.add(i * 2).writeU16((i * 0x3b9 + (i >>> 15) * 0x1d7 + 0x51ed) & 0xffff);
    }
    fx.tbl = tbl;
    fx.tbl2 = tbl.add(0x80);

    // blit16from16 intersects its RECT argument in place, so these live in one block that is a state region.
    const RECTS = {
      br_full: [0, 0, 32, 16], br_in: [4, 2, 20, 12], br_odd: [3, 1, 14, 10], br_1x1: [5, 5, 6, 6],
      br_2x3: [1, 1, 3, 4], br_neg: [-6, -4, 10, 7], br_over: [20, 8, 50, 30], br_out: [40, 40, 50, 50],
      br_s19: [0, 0, 19, 11], br_s19in: [2, 1, 15, 9],
    };
    const names = Object.keys(RECTS);
    const rects = Memory.alloc(names.length * 0x10);
    names.forEach((name, i) => {
      const r = rects.add(i * 0x10);
      RECTS[name].forEach((v, j) => r.add(j * 4).writeS32(v));
      fx[name] = r;
    });
    fx.rects = rects;

    return fx;
  },
});
