// Fixtures for C3 batch c3ay: jgld.dll's 16-bit Surface drawing primitives (clear16, fillRect16, ditherRect16,
// hline16, vline16, dashedHLine16) and the 8-bit keyed copy blit8from8.
//
// Two kinds of object are built, both OURS, both on jgld's own vtables, so every virtual these primitives call is
// jgld's own code and every byte they read or write is in memory this fixture allocated. Nothing live is touched:
// not the Display at 0x10128420, not a screen surface, not DirectDraw, not the module's Palette.
//
//   Surface  0x820 bytes, +0 = the real Surface vtable (RVA 0x11d0b0, re/analysis/systems/jgld_2.md). Fields the
//            vtable accessors and these bodies read, each proven by its body:
//              +0x24 depth        (slot +0xe4 0x1000ab30 returns this+0x24; the pixel-address switch at
//                                  VA 0x100088b3 reads it: 8 -> 1 byte per pixel, 16 -> 2, 24 -> 3, 32 -> 4,
//                                  anything else NULL at 0x100088c6)
//              +0x28 format code  (element [1] of the slot-+0xe4 pointer: 0 = RGB565, 1 = RGB555, else abort)
//              +0x40 row stride   (slot +0xe0 0x1000aaf0 returns it, VA 0x1000ab10)
//              +0x44 clip RECT    (slot +0xcc 0x1000a990 returns this+0x44, VA 0x1000a9b0)
//              +0x54 bounds RECT  (slot +0xd4 0x1000aa30 returns this+0x54, VA 0x1000aa50; width() slot +0xd8 is
//                                  +0x5c - +0x54 at VA 0x1000aa93, height() slot +0xdc is +0x60 - +0x58 at
//                                  VA 0x1000aad3)
//              +0x7c palette      (read directly by all six bodies, e.g. clear16 at VA 0x1000daa7)
//              +0x4c0 pixel base  (slot +0x10 copies it to +0x4cc and bumps the use count at +0x4c8, VA 0x10008753)
//
//   Palette  0x820 bytes, +0 = the real Palette vtable (RVA 0x11db10, written by Palette::ctor 0x1006a150).
//            Slot +0x18 (thunk 0x100010c8 -> 0x1006b000) returns this+0x40c and slot +0x1c (thunk 0x10001af0 ->
//            0x1006b040) returns this+0x60c, the two 256-entry 16-bit colour tables Palette::apply 0x1006a4a0
//            rebuilds. Only those two tables are filled here, with our own synthetic values.
//
// All fourteen pixel buffers live inside ONE block ($pix), so a single state region covers them and the final
// hash says which surface changed; the per-surface lock windows ($l0..$l13 = surface+0x4c0, 0x14 bytes) are
// separate state regions so a missing slot-+0x24 release reads RED.
//
// The two module globals these functions park loop state in are deliberately NOT state regions and not touched
// here: 0x10128458 (fillRect16's row skip, written at VA 0x1000deaf, never read back) and 0x10128450
// (dashedHLine16's pixel count, written at VA 0x1000d380 and read back at 0x1000d38b). What is parked is still
// verified by the multi-row and multi-pixel vectors through $pix.
Object.assign(globalThis.DIFF_FIXTURES, {
  c3ay_surfaces() {
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

    // One pixel block: surface i draws into $pix + i * 0x800. 0x800 bytes is 1024 words, and the widest 16-bit
    // surface here is 32 x 16 at stride 32, whose last byte is at (15 * 32 + 31) * 2 = 1022.
    const NSURF = 14, STRIDE = 0x800, PIX = NSURF * STRIDE;
    const pix = Memory.alloc(PIX);
    for (let i = 0; i < PIX; i++) pix.add(i).writeU8((i * 37 + 11) & 0xff);
    // The slices of the three 8-bit surfaces blit8from8 writes into (s11, s12, s13) get a seven-value alphabet
    // instead, so a key byte matches roughly one pixel in seven and both sides of the per-pixel compare at
    // VA 0x10013acd run many times per vector; the surface it reads from (s10) keeps the wide pattern above, so
    // the bytes it copies in are all different.
    for (let s = 11; s <= 13; s++) {
      for (let j = 0; j < STRIDE; j++) pix.add(s * STRIDE + j).writeU8(0x40 + ((j * 3) % 7));
    }

    const fx = { obj: pix, pix: pix, pal: pal, pal2: pal2 };

    // depth, format code, pitch, bounds {l,t,r,b}, clip {l,t,r,b}, hasBits, palette
    const SPECS = [
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal],    // s0 RGB565, clip == bounds
      [16, 0, 32, [0, 0, 32, 16], [4, 2, 20, 12], true, pal],    // s1 clip strictly inside the bounds
      [16, 1, 19, [0, 0, 19, 11], [0, 0, 19, 11], true, pal],    // s2 RGB555, odd width and odd stride
      [16, 2, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal],    // s3 format code 2: the switch default
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], false, pal],   // s4 no pixel base: every pixel address is NULL
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, 0],      // s5 no surface palette: the global fallback arm
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal],     // s6 depth 8: the pixel address scales by one
      [12, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal],    // s7 depth 12: the address switch has no case
      [16, 0, 32, [0, 0, 16, 8], [0, 0, 32, 4], true, pal],      // s8 clip wider than the bounds, clip w == pitch
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal2],   // s9 the second palette
      // The four 8-bit surfaces blit8from8 works on. s10 is the one it READS (the `this` of the call) and
      // s11/s12/s13 are the ones it is handed as the first argument and WRITES into.
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal],     // s10 `this`: 32 x 16, stride 32
      [8, 0, 24, [0, 0, 24, 12], [0, 0, 24, 12], true, pal],     // s11 argument: 24 x 12, clip == bounds
      [8, 0, 24, [0, 0, 24, 12], [2, 1, 18, 9], true, pal],      // s12 argument: clip inside the bounds
      [8, 0, 24, [0, 0, 24, 12], [0, 0, 24, 12], false, pal],    // s13 argument: no base -> NULL, returns 3
    ];
    const surf = Memory.alloc(SPECS.length * 0x820);
    SPECS.forEach((sp, i) => {
      const [depth, fmt, pitch, b, k, hasBits, p] = sp;
      const s = surf.add(i * 0x820);
      for (let o = 0; o < 0x820; o += 4) s.add(o).writeU32(0);
      s.writePointer(vtSurface);
      s.add(0x24).writeS32(depth);
      s.add(0x28).writeS32(fmt);
      s.add(0x40).writeS32(pitch);
      for (let j = 0; j < 4; j++) s.add(0x44 + j * 4).writeS32(k[j]);
      for (let j = 0; j < 4; j++) s.add(0x54 + j * 4).writeS32(b[j]);
      s.add(0x7c).writePointer(p ? p : ptr(0));
      s.add(0x4c0).writePointer(hasBits ? pix.add(i * STRIDE) : ptr(0));
      fx['s' + i] = s;
      fx['l' + i] = s.add(0x4c0);       // +0x4c0 base, +0x4c8 use count, +0x4cc current base
    });

    // Rectangles for the two rectangle primitives. r_out / r_empty / r_inv make IntersectRect fail; r_offsurf
    // sits inside s8's wide clip rectangle but past the surface's width, so the pixel address comes back NULL.
    const RECTS = {
      r_full: [0, 0, 32, 16], r_in: [4, 2, 20, 12], r_odd: [3, 1, 14, 10], r_even: [2, 2, 14, 10],
      r_1x1: [5, 5, 6, 6], r_5x3: [1, 1, 6, 4], r_7x5: [2, 3, 9, 8], r_w32h15: [0, 0, 32, 15],
      r_w32h4: [0, 0, 32, 4], r_1x4: [5, 5, 6, 9], r_s19: [0, 0, 19, 11], r_s16: [0, 0, 16, 10],
      r_out: [100, 100, 110, 110], r_empty: [10, 10, 10, 10], r_inv: [20, 20, 5, 5], r_neg: [-10, -5, 8, 6],
      r_offsurf: [20, 1, 30, 3],
    };
    const rblock = Memory.alloc(Object.keys(RECTS).length * 0x10);
    Object.keys(RECTS).forEach((name, i) => {
      const r = rblock.add(i * 0x10);
      RECTS[name].forEach((v, j) => r.add(j * 4).writeS32(v));
      fx[name] = r;
    });

    return fx;
  },
});
