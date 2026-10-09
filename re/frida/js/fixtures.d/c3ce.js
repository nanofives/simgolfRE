// Fixtures for C3 batch c3ce: the jgld.dll Surface methods that are reached only through a slot of the Surface
// vtable at RVA 0x11d0b0 (VA 0x1011d0b0), installed by Surface::ctor 0x10007660, Surface::ctorOwner 0x10007970 and
// Surface::dtor 0x10007bf0.
//
// Everything the vectors touch is OURS. One block ($obj) holds sixteen Surface objects, two Palette objects, the
// pixel buffers, the destination rectangles of the two "copy a RECT out" slots, the argument rectangles, a
// 256-byte lookup table and the fake device-context values. Nothing live is passed in or written: not the Display
// at 0x10128420, not a screen surface, not DirectDraw, not the module's own Palette. +0 of each object is the REAL
// vtable, so every virtual these bodies dispatch through is jgld's own code.
//
// Surface fields the bodies in this batch read or write, each proven by the instruction that touches it:
//   +0x24  depth          slot +0xe4 0x1000ab30 returns this+0x24 (VA 0x1000ab50); the callers deref it
//                         (0x10009364 in fillRect, 0x10009474 in fill2, 0x10009514 in drawLine, 0x10009c84 in
//                         ditherRectDispatch) and pixelAddr's switch reads it at VA 0x100088b3
//   +0x28  format code    element [1] of that same pointer (0 = RGB565, 1 = RGB555, 2 = the switch default)
//   +0x40  row stride     slot +0xe0 0x1000aaf0 returns it (VA 0x1000ab10); pixelAddr multiplies by it at
//                         VA 0x100088e6 / 0x100088fd / 0x10008917 / 0x10008931
//   +0x44  clip RECT      slot +0xcc 0x1000a990 returns this+0x44 (VA 0x1000a9b0); getClip 0x1000a930 copies the
//                         four dwords from this+0x44 (VA 0x1000a950); setClip 0x100083c0 writes it (VA 0x10008415)
//   +0x54  bounds RECT    slot +0xd4 0x1000aa30 returns this+0x54 (VA 0x1000aa50); width 0x1000aa70 is
//                         [this+0x5c] - [this+0x54] (VA 0x1000aa93) and height 0x1000aab0 is
//                         [this+0x60] - [this+0x58] (VA 0x1000aad3)
//   +0x7c  palette        returned by 0x1000ab70 (VA 0x1000ab90) and read by the 16-bit primitives
//   +0x4b8 current DC     written by acquireDC 0x10008640 from +0x4bc (VA 0x10008669), cleared by releaseDC
//                         0x100086b0 (VA 0x100086f1)
//   +0x4bc stored DC      read by acquireDC (VA 0x10008663)
//   +0x4c0 pixel base     read by acquireDC2 0x10008730 (VA 0x10008753)
//   +0x4c4 DC use count   bumped by acquireDC (VA 0x1000867b), decreased by releaseDC (VA 0x100086dc)
//   +0x4c8 bits use count bumped by acquireDC2 (VA 0x1000877a), decreased by releaseDC2 (VA 0x100087dc)
//   +0x4cc current bits   written by acquireDC2 (VA 0x10008759), cleared by releaseDC2 (VA 0x100087f1)
//   +0x4d0 spare field    written by 0x1000a5d0 (VA 0x1000a5f3), read by 0x1000a620 (VA 0x1000a640)
//
// Palette (0x820 bytes, +0 = the real Palette vtable RVA 0x11db10 written by Palette::ctor 0x1006a150): slot
// +0x18 returns this+0x40c and slot +0x1c this+0x60c, the two 256-entry 16-bit colour tables the 16-bit
// primitives read. Only those two tables are filled, with our own synthetic values (c3ay established this).
Object.assign(globalThis.DIFF_FIXTURES, {
  c3ce_surfaces() {
    const jgld = Process.getModuleByName('jgld.dll').base;
    const vtSurface = jgld.add(0x11d0b0);
    const vtPalette = jgld.add(0x11db10);

    // --- one block, so every pointer the slots return reads as obj+0x... in the CSV and both arms compare.
    const NSURF = 16, OSIZE = 0x820, PSTRIDE = 0x400;
    const OFF_SURF = 0x0000;                      // 16 * 0x820 = 0x8200
    const OFF_PAL = 0x8200;                       // 2 * 0x820 = 0x1040
    const OFF_PIX = 0x9400;                       // 16 * 0x400 = 0x4000, plus 0x800 of slack
    const PIXLEN = 0x4800;
    const OFF_OUT = 0xDC00;                       // eight 0x10-byte destination rectangles
    const OFF_RECT = 0xDC80;                      // the argument rectangles
    const OFF_LUT = 0xDD80;                       // one 256-byte byte-mapping table
    const OFF_DC = 0xDE80;                        // the fake device-context values (never dereferenced)
    const TOTAL = 0xE000;
    const blk = Memory.alloc(TOTAL);
    for (let o = 0; o < TOTAL; o += 4) blk.add(o).writeU32(0);

    const fx = { obj: blk, pix: blk.add(OFF_PIX), out: blk.add(OFF_OUT), lut: blk.add(OFF_LUT) };

    // Pixel bytes: (i * 37 + 11) & 0xff is a permutation of 0..255 over any 256 consecutive bytes, so a colour
    // the 8-bit replace fill looks for matches some pixels of every surface and misses others.
    for (let i = 0; i < PIXLEN; i++) blk.add(OFF_PIX + i).writeU8((i * 37 + 11) & 0xff);

    // Two palettes with four different tables, so a result says which object and which table was read.
    const mkPal = (k, a565, b565, a555, b555) => {
      const p = blk.add(OFF_PAL + k * OSIZE);
      p.writePointer(vtPalette);
      for (let i = 0; i < 256; i++) {
        p.add(0x40c + i * 2).writeU16((i * a565 + b565) & 0xffff);
        p.add(0x60c + i * 2).writeU16((i * a555 + b555) & 0xffff);
      }
      return p;
    };
    const pal = mkPal(0, 0x0105, 0x1234, 0x0203, 0x5678);
    const pal2 = mkPal(1, 0x0307, 0x9abc, 0x0409, 0xdef0);
    fx.pal = pal;
    fx.pal2 = pal2;

    // depth, format code, pitch, bounds {l,t,r,b}, clip {l,t,r,b}, hasBits, palette, hasDC, dcCount, bitsCount, f4d0
    const SPECS = [
      // 8-bit surfaces
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal, true, 0, 0, 0x11111111],   // s0 clip == bounds, a DC
      [8, 0, 32, [0, 0, 32, 16], [4, 2, 20, 12], true, pal, false, 0, 0, 0x22222222],  // s1 clip inside the bounds
      [8, 1, 19, [0, 0, 19, 11], [0, 0, 19, 11], true, pal2, true, 3, 2, 0x00000000],  // s2 odd width and stride
      // 16-bit surfaces
      [16, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal, false, 0, 0, -1],         // s3 RGB565
      [16, 1, 16, [0, 0, 16, 10], [0, 0, 16, 10], true, pal2, true, 1, 1, 0x7FFFFFFF], // s4 RGB555
      [16, 2, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal, false, 0, 0, 5],          // s5 format code 2
      // depths the pixel-address switch treats differently
      [12, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], true, pal, false, 0, 0, 6],          // s6 depth 12: no case
      [24, 0, 10, [0, 0, 10, 8], [0, 0, 10, 8], true, pal, false, 0, 0, 7],            // s7 depth 24: x * 3
      [32, 0, 8, [0, 0, 8, 8], [0, 0, 8, 8], true, pal, false, 0, 0, 8],               // s8 depth 32: x * 4
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 32, 16], false, 0, false, 0, 0, 9],            // s9 no base, no palette
      [8, 0, 32, [0, 0, 32, 16], [0, 0, 64, 40], true, pal, false, 0, 0, 10],          // s10 clip past the bounds
      [16, 0, 32, [0, 0, 32, 16], [4, 2, 20, 12], true, pal2, true, 5, 4, 11],         // s11 16-bit, clip inside
      [8, 0, 24, [2, 1, 26, 13], [2, 1, 26, 13], true, pal, false, 0, 0, 12],          // s12 bounds not at 0,0
      [8, 0, 32, [0, 0, 0, 0], [0, 0, 0, 0], true, 0, false, 0, 7, 13],                // s13 empty bounds
      // The pixel-address switch subtracts 8 from the depth and refuses anything above 0x18 (VA 0x100088c2 cmp,
      // 0x100088c6 ja). Of the depths above, 32 lands exactly on 0x18 and 12 lands on 4, so that test is never
      // taken by s0..s13: 48 (0x28, above the range) and 4 (which wraps to 0xfffffffc) are the two surfaces
      // that take it, from each side of the subtraction.
      [48, 0, 16, [0, 0, 16, 8], [0, 0, 16, 8], true, pal, false, 0, 0, 14],           // s14 depth 48
      [4, 0, 16, [0, 0, 16, 8], [0, 0, 16, 8], true, pal, false, 0, 0, 15],            // s15 depth 4
    ];
    // Twelve of the sixteen have no stored device context (+0x4bc = 0). Surface::callBounds 0x1000a8d0 reaches
    // setClip 0x100083c0, whose GDI block (DeleteObject / CreateRectRgnIndirect / SelectClipRgn, VA 0x10008468,
    // 0x100084bc, 0x100084a4) runs only when slot +0x28 returns a non-NULL context (VA 0x10008446 cmp,
    // 0x1000844a je), so its vectors use only those twelve and no GDI call is ever made with a stand-in handle.
    SPECS.forEach((sp, i) => {
      const [depth, fmt, pitch, b, k, hasBits, p, hasDC, dcN, bitsN, f4d0] = sp;
      const s = blk.add(OFF_SURF + i * OSIZE);
      s.writePointer(vtSurface);
      s.add(0x24).writeS32(depth);
      s.add(0x28).writeS32(fmt);
      s.add(0x40).writeS32(pitch);
      for (let j = 0; j < 4; j++) s.add(0x44 + j * 4).writeS32(k[j]);
      for (let j = 0; j < 4; j++) s.add(0x54 + j * 4).writeS32(b[j]);
      s.add(0x7c).writePointer(p ? p : ptr(0));
      // +0x4bc is the device context acquireDC hands out. It is never dereferenced by any body in this batch
      // (acquireDC only copies it, releaseDC only clears the copy), so a distinct address inside this block is a
      // safe stand-in and keeps every pointer result readable as obj+0x...
      const dc = hasDC ? blk.add(OFF_DC + i * 0x10) : ptr(0);
      const bits = hasBits ? blk.add(OFF_PIX + i * PSTRIDE) : ptr(0);
      // A non-zero use count means the object is already holding what it handed out, so the clearing arm of
      // releaseDC / releaseDC2 has something to clear.
      s.add(0x4b8).writePointer(dcN > 0 ? dc : ptr(0));
      s.add(0x4bc).writePointer(dc);
      s.add(0x4c0).writePointer(bits);
      s.add(0x4c4).writeS32(dcN);
      s.add(0x4c8).writeS32(bitsN);
      s.add(0x4cc).writePointer(bitsN > 0 ? bits : ptr(0));
      s.add(0x4d0).writeS32(f4d0);
      fx['s' + i] = s;
      fx['o' + i] = s.add(0x40);          // +0x40 stride, +0x44 clip RECT, +0x54 bounds RECT  (0x28 bytes)
      fx['w' + i] = s.add(0x4b8);         // +0x4b8 .. +0x4d7: the DC, bits and spare fields   (0x20 bytes)
    });

    // Eight destination rectangles for the two slots that copy a RECT out (0x1000a930, 0x1000a9d0). They start
    // filled with a marker so a slot that writes nothing is distinguishable from one that writes zeros.
    for (let i = 0; i < 8; i++) {
      const d = blk.add(OFF_OUT + i * 0x10);
      for (let j = 0; j < 4; j++) d.add(j * 4).writeS32(-0x5a5a5a5a + i);
      fx['out' + i] = d;
    }

    // Argument rectangles. r_out / r_empty / r_inv make the intersection inside the primitives fail.
    const RECTS = {
      r_full: [0, 0, 32, 16], r_in: [4, 2, 20, 12], r_odd: [3, 1, 14, 10], r_1x1: [5, 5, 6, 6],
      r_5x3: [1, 1, 6, 4], r_7x5: [2, 3, 9, 8], r_w32h15: [0, 0, 32, 15], r_s19: [0, 0, 19, 11],
      r_s16: [0, 0, 16, 10], r_s24: [0, 0, 10, 8], r_s32: [0, 0, 8, 8], r_s12: [2, 1, 26, 13],
      r_out: [100, 100, 110, 110], r_empty: [10, 10, 10, 10], r_inv: [20, 20, 5, 5], r_neg: [-10, -5, 8, 6],
    };
    Object.keys(RECTS).forEach((name, i) => {
      const r = blk.add(OFF_RECT + i * 0x10);
      RECTS[name].forEach((v, j) => r.add(j * 4).writeS32(v));
      fx[name] = r;
    });

    // The 256-byte table Surface::fill2 0x10009440 hands to fill2_8 0x1000f9e0 as its fifth argument. 7 is odd, so
    // i * 7 + 3 is a permutation of 0..255 and every pixel of the rectangle changes.
    for (let i = 0; i < 0x100; i++) blk.add(OFF_LUT + i).writeU8((i * 7 + 3) & 0xff);

    return fx;
  },
});
