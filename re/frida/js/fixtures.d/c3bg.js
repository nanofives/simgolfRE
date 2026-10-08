// Batch c3bg fixtures (jgld.dll). Every block is allocated here; nothing live in the running jgld.dll
// (no Display, Surface, Palette, Font, the game's png_struct / png_info, its inflate state or its heap
// blocks) is handed to the A/B. Layouts come from the functions' own field offsets, cited in
// shim/src/re/c3bg.cpp. Blocks are pre-filled with a per-offset word pattern and every object sits at
// a larger stride than the bytes it uses, so a store one word too wide or a field left unwritten
// changes the block hash.
Object.assign(globalThis.DIFF_FIXTURES, {

  // ---------------------------------------------------------------- libpng row transformations.
  // A png_row_info is 0xc bytes: width +0, rowbytes +4, color_type +8, bit_depth +9, channels +0xa,
  // pixel_depth +0xb. Each function gets its own contiguous header array (so $xxx0 is a base a state
  // region can cover) and its own row block. png_do_read_filler and png_do_expand write BELOW and
  // ABOVE the row pointer they are given (the 16-bit RGB bodies of read_filler end about 4 * width
  // bytes under it), so every row sits well inside its block and the whole block is the state region.
  c3bg_rows() {
    const out = {};
    const mkHeaders = (name, rows) => {
      const blk = Memory.alloc(rows.length * 0xc);
      rows.forEach((r, i) => {
        const p = blk.add(i * 0xc);
        p.writeU32(r[0]);               // width
        p.add(4).writeU32(r[1]);        // rowbytes
        p.add(8).writeU8(r[2]);         // color_type
        p.add(9).writeU8(r[3]);         // bit_depth
        p.add(0xa).writeU8(r[4]);       // channels
        p.add(0xb).writeU8(r[5]);       // pixel_depth
        out[name + i] = p;
      });
      out[name + 'blk'] = blk;
    };
    const fillBlk = (n, seed) => {
      const blk = Memory.alloc(n);
      for (let i = 0; i < n; i += 4) blk.add(i).writeU32(((seed << 16) + i) >>> 0);
      return blk;
    };
    const setBytes = (p, bytes) => bytes.forEach((b, i) => p.add(i).writeU8(b & 0xff));

    // ---- png_do_strip_filler 0x1006dac0: channels 4 or 2, bit_depth 8 or anything else, and the
    // filler-after flag 0x80; everything else returns without touching the row or the header.
    mkHeaders('sf', [
      [6, 24, 6, 8, 4, 32],    // 0  channels 4, depth 8, flag set
      [6, 24, 6, 8, 4, 32],    // 1  channels 4, depth 8, flag clear
      [4, 32, 6, 16, 4, 64],   // 2  channels 4, depth 16, flag set
      [4, 32, 6, 16, 4, 64],   // 3  channels 4, depth 16, flag clear
      [8, 16, 4, 8, 2, 16],    // 4  channels 2, depth 8, flag set
      [8, 16, 4, 8, 2, 16],    // 5  channels 2, depth 8, flag clear
      [5, 20, 4, 16, 2, 32],   // 6  channels 2, depth 16, flag set
      [5, 20, 4, 16, 2, 32],   // 7  channels 2, depth 16, flag clear
      [6, 18, 2, 8, 3, 24],    // 8  channels 3: neither half runs
      [1, 4, 6, 8, 4, 32],     // 9  channels 4, depth 8, flag set, width 1: the loop from 1 is empty
      [0, 0, 6, 16, 4, 64],    // 10 width 0 with a loop from 1
      [0, 0, 4, 8, 2, 16],     // 11 width 0 with a loop from 0
      [0, 0, 6, 8, 4, 32],     // 12 width 0 with a loop from 0
      [1, 4, 4, 16, 2, 32],    // 13 channels 2, depth 16, flag set, width 1
      [4, 16, 6, 4, 4, 16],    // 14 channels 4, depth 4: takes the same body as depth 16
      [6, 24, 6, 8, 4, 32],    // 15 channels 4, depth 8, run with a flag word that has no bit 0x80
    ]);
    {
      const blk = fillBlk(0x200, 0x5353);
      const row = blk.add(0x40);
      for (let i = 0; i < 0x80; i++) row.add(i).writeU8((i * 7 + 0x13) & 0xff);
      out.sfrowblk = blk;
      out.sfrow = row;
    }

    // ---- png_do_read_filler 0x10072bd0: color_type 0 or 2, bit_depth 8 or 16, the same 0x80 flag.
    mkHeaders('rf', [
      [6, 6, 0, 8, 1, 8],      // 0  grey 8, flag set
      [6, 6, 0, 8, 1, 8],      // 1  grey 8, flag clear
      [4, 8, 0, 16, 1, 16],    // 2  grey 16, flag set
      [4, 8, 0, 16, 1, 16],    // 3  grey 16, flag clear
      [5, 15, 2, 8, 3, 24],    // 4  RGB 8, flag set
      [5, 15, 2, 8, 3, 24],    // 5  RGB 8, flag clear
      [3, 18, 2, 16, 3, 48],   // 6  RGB 16, flag set
      [3, 18, 2, 16, 3, 48],   // 7  RGB 16, flag clear
      [6, 6, 3, 8, 1, 8],      // 8  color_type 3: neither half runs
      [6, 3, 0, 4, 1, 4],      // 9  grey with depth 4: neither depth test matches
      [4, 4, 2, 1, 3, 3],      // 10 RGB with depth 1: neither depth test matches
      [1, 1, 0, 8, 1, 8],      // 11 width 1, flag set: the loop is empty, the trailing store is not
      [0, 0, 0, 16, 1, 16],    // 12 width 0, flag set: two trailing stores only
      [0, 0, 2, 8, 3, 24],     // 13 width 0, flag clear
      [2, 2, 0, 8, 1, 8],      // 14 width 2, flag clear
      [6, 6, 0, 8, 1, 8],      // 15 grey 8, run with a flag word that has no bit 0x80
    ]);
    {
      const blk = fillBlk(0x400, 0x4646);
      const row = blk.add(0x180);
      for (let i = -0x60; i < 0x60; i++) row.add(i).writeU8((i * 11 + 0x29) & 0xff);
      out.rfrowblk = blk;
      out.rfrow = row;
    }

    // ---- png_do_gamma 0x10076760: the two-part guard, the colour-type jump table at 0x10076ea0 and
    // four depth bodies inside the grey arm.
    mkHeaders('gm', [
      [5, 15, 2, 8, 3, 24],    // 0  RGB 8
      [3, 18, 2, 16, 3, 48],   // 1  RGB 16, shift 4
      [4, 16, 6, 8, 4, 32],    // 2  RGBA 8
      [3, 24, 6, 16, 4, 64],   // 3  RGBA 16, shift 8
      [6, 12, 4, 8, 2, 16],    // 4  grey+alpha 8
      [4, 16, 4, 16, 2, 32],   // 5  grey+alpha 16, shift 0
      [8, 2, 0, 2, 1, 2],      // 6  grey depth 2
      [7, 4, 0, 4, 1, 4],      // 7  grey depth 4
      [8, 8, 0, 8, 1, 8],      // 8  grey depth 8
      [4, 8, 0, 16, 1, 16],    // 9  grey depth 16, shift 2
      [8, 8, 3, 8, 1, 8],      // 10 colour type 3: the jump table sends it to the exit
      [8, 8, 1, 8, 1, 8],      // 11 colour type 1: the same
      [8, 8, 5, 8, 1, 8],      // 12 colour type 5: the same
      [8, 8, 7, 8, 1, 8],      // 13 colour type 7: the unsigned range test returns
      [5, 15, 2, 8, 3, 24],    // 14 run with gamma_table null: the first half of the guard fails
      [3, 18, 2, 16, 3, 48],   // 15 run with gamma_16_table null: the second half fails
      [3, 18, 2, 16, 3, 48],   // 16 run with gamma_table null at depth 16: the second half passes
      [0, 0, 2, 8, 3, 24],     // 17 width 0: the loop body never runs
    ]);
    {
      const blk = fillBlk(0x100, 0x4747);
      const row = blk.add(0x20);
      for (let i = 0; i < 0x40; i++) row.add(i).writeU8((i * 29 + 0x5b) & 0xff);
      out.gmrowblk = blk;
      out.gmrow = row;
      // gamma_table: 256 bytes indexed by one sample.
      const gt = Memory.alloc(256);
      for (let i = 0; i < 256; i++) gt.add(i).writeU8((255 - i) ^ 0x3c);
      out.gmtab = gt;
      // gamma_16_table: 256 pointers (indexed by the low byte of the sample shifted right) into 16
      // sub-tables of 256 unsigned shorts (indexed by the high byte).
      const sub = Memory.alloc(8 * 256 * 2);
      for (let i = 0; i < 8 * 256; i++) sub.add(i * 2).writeU16((i * 2731 + 0x1234) & 0xffff);
      const ptrs = Memory.alloc(256 * 4);
      for (let i = 0; i < 256; i++) ptrs.add(i * 4).writePointer(sub.add((i & 7) * 512));
      out.gm16sub = sub;
      out.gm16 = ptrs;
    }

    // ---- png_do_expand 0x10077300: the grey arm (three sub-8 depths plus the 8/16-bit tRNS stage)
    // and the RGB arm, both only when trans_value is not null for the second half.
    mkHeaders('ex', [
      [8, 1, 0, 1, 1, 1],      // 0  grey depth 1 unpack, then the 8-bit tRNS stage
      [7, 2, 0, 2, 1, 2],      // 1  grey depth 2 unpack
      [5, 3, 0, 4, 1, 4],      // 2  grey depth 4 unpack
      [4, 2, 0, 3, 1, 3],      // 3  grey depth 3: below 8 with no case, the tail still runs
      [6, 6, 0, 8, 1, 8],      // 4  grey depth 8, tRNS
      [3, 6, 0, 16, 1, 16],    // 5  grey depth 16, tRNS
      [6, 6, 0, 8, 1, 8],      // 6  grey depth 8 with trans_value null: no second stage
      [8, 1, 0, 1, 1, 1],      // 7  grey depth 1 with trans_value null: unpack only
      [0, 0, 0, 8, 1, 8],      // 8  width 0
      [4, 12, 2, 8, 3, 24],    // 9  RGB 8 with tRNS
      [2, 12, 2, 16, 3, 48],   // 10 RGB 16 with tRNS
      [4, 12, 2, 8, 3, 24],    // 11 RGB 8 with trans_value null: nothing runs
      [4, 6, 2, 4, 3, 12],     // 12 RGB depth 4: neither depth body, header rewrite only
      [4, 4, 3, 8, 1, 8],      // 13 colour type 3: neither arm
      [4, 16, 6, 8, 4, 32],    // 14 colour type 6: neither arm
      [3, 6, 0, 16, 1, 16],    // 15 grey depth 16 with a tRNS value no pixel matches
    ]);
    {
      const blk = fillBlk(0x200, 0x4545);
      const row = blk.add(0x80);
      // the first twelve bytes are chosen so that the equality tests of both arms take both sides:
      // byte 4 is 0x03 (= trans gray of tv3), the pair at 2..3 is 0xaa55 (= gray of tv4) and the
      // group 6..8 is 0x11,0x22,0x33 (= red, green, blue of tv0).
      setBytes(row, [0xff, 0x00, 0xaa, 0x55, 0x03, 0x11, 0x11, 0x22, 0x33, 0x40, 0x7f, 0x80]);
      for (let i = 12; i < 0x60; i++) row.add(i).writeU8((i * 13 + 0x3d) & 0xff);
      out.exrowblk = blk;
      out.exrow = row;
      // png_color_16 at a 0x10 stride: red +2, green +4, blue +6, gray +8.
      const tv = Memory.alloc(8 * 0x10);
      const TV = [
        [0x11, 0x22, 0x33, 0x0001],   // tv0 gray 1   -> 1 * 0xff = 0xff matches the depth-1 bytes
        [0x00, 0x00, 0x00, 0x0003],   // tv1 gray 3   -> 3 * 0x55 = 0xff matches the depth-2 value 3
        [0x00, 0x00, 0x00, 0x000f],   // tv2 gray 15  -> 15 * 0x11 = 0xff matches the depth-4 nibble
        [0x11, 0x22, 0x33, 0x0003],   // tv3 gray 3   -> matches row byte 4 at depth 8
        [0x00, 0x00, 0x00, 0xaa55],   // tv4 gray     -> matches the big-endian pair at row 2..3
        [0x1122, 0x3340, 0x7f80, 0x1234],  // tv5 the 16-bit RGB triple of row 6..11, gray no match
      ];
      TV.forEach((t, i) => {
        const p = tv.add(i * 0x10);
        p.add(2).writeU16(t[0]);
        p.add(4).writeU16(t[1]);
        p.add(6).writeU16(t[2]);
        p.add(8).writeU16(t[3]);
        out['tv' + i] = p;
      });
      out.extvblk = tv;
    }
    return out;
  },

  // ---------------------------------------------------------------- png_do_rgb_to_gray 0x100736c0.
  // Seven scratch png_struct records of 0x240 bytes at a 0x280 stride differ only in which of the
  // four gamma pointers are set and in the three coefficients, which is what selects the eight
  // bodies and their two null tests. Fields: gamma_shift +0x158, gamma_from_1 +0x168, gamma_to_1
  // +0x16c, gamma_16_from_1 +0x174, gamma_16_to_1 +0x178, the coefficients +0x215, +0x216, +0x217.
  c3bg_gray() {
    const out = {};
    const gt1 = Memory.alloc(256);                 // gamma_to_1
    const gf1 = Memory.alloc(256);                 // gamma_from_1
    for (let i = 0; i < 256; i++) {
      gt1.add(i).writeU8((i * 7 + 0x11) & 0xff);
      gf1.add(i).writeU8((255 - i) ^ 0x5a);
    }
    const sub16to = Memory.alloc(8 * 256 * 2);
    const sub16from = Memory.alloc(8 * 256 * 2);
    for (let i = 0; i < 8 * 256; i++) {
      sub16to.add(i * 2).writeU16((i * 1237 + 0x0507) & 0xffff);
      sub16from.add(i * 2).writeU16((i * 7919 + 0x4321) & 0xffff);
    }
    const p16to = Memory.alloc(256 * 4);
    const p16from = Memory.alloc(256 * 4);
    for (let i = 0; i < 256; i++) {
      p16to.add(i * 4).writePointer(sub16to.add((i & 7) * 512));
      p16from.add(i * 4).writePointer(sub16from.add((i & 7) * 512));
    }
    out.gt1 = gt1; out.gf1 = gf1;
    out.g16tosub = sub16to; out.g16fromsub = sub16from;
    out.g16to = p16to; out.g16from = p16from;

    const blk = Memory.alloc(7 * 0x280);
    for (let i = 0; i < 7 * 0x280; i += 4) blk.add(i).writeU32((0x6767 << 16) + i >>> 0);
    //    name,      from_1, to_1,   16_from_1, 16_to_1, shift, rc,  gc,  bc
    const PS = [
      ['ps_nog',     null,   null,   null,      null,    0,     54,  183, 19],
      ['ps_g8',      gf1,    gt1,    null,      null,    0,     54,  183, 19],
      ['ps_g16',     null,   null,   p16from,   p16to,   4,     54,  183, 19],
      ['ps_half8',   gf1,    null,   null,      null,    0,     54,  183, 19],
      ['ps_half16',  null,   null,   p16from,   null,    4,     54,  183, 19],
      ['ps_half16b', null,   null,   null,      p16to,   4,     54,  183, 19],
      ['ps_both',    gf1,    gt1,    p16from,   p16to,   2,     100, 50,  105],
    ];
    PS.forEach((s, i) => {
      const p = blk.add(i * 0x280);
      p.add(0x158).writeS32(s[5]);
      p.add(0x168).writePointer(s[1] === null ? ptr(0) : s[1]);
      p.add(0x16c).writePointer(s[2] === null ? ptr(0) : s[2]);
      p.add(0x174).writePointer(s[3] === null ? ptr(0) : s[3]);
      p.add(0x178).writePointer(s[4] === null ? ptr(0) : s[4]);
      p.add(0x215).writeU8(s[6]);
      p.add(0x216).writeU8(s[7]);
      p.add(0x217).writeU8(s[8]);
      out[s[0]] = p;
    });
    out.gblk = blk;

    const hdr = Memory.alloc(16 * 0xc);
    [[5, 15, 2, 8, 3, 24],    // 0  RGB 8
     [5, 15, 2, 8, 3, 24],    // 1  RGB 8 with the two 8-bit gamma tables
     [4, 24, 2, 16, 3, 48],   // 2  RGB 16
     [4, 24, 2, 16, 3, 48],   // 3  RGB 16 with the two 16-bit gamma tables
     [4, 16, 6, 8, 4, 32],    // 4  RGBA 8
     [4, 16, 6, 8, 4, 32],    // 5  RGBA 8 with gamma
     [2, 16, 6, 16, 4, 64],   // 6  RGBA 16
     [2, 16, 6, 16, 4, 64],   // 7  RGBA 16 with gamma
     [5, 5, 0, 8, 1, 8],      // 8  colour type 0: the colour-mask guard returns 0
     [4, 4, 3, 8, 1, 8],      // 9  colour type 3: the mask passes, no loop, header rewrite only
     [0, 0, 2, 8, 3, 24],     // 10 width 0
     [5, 15, 2, 8, 3, 24],    // 11 with gamma_from_1 set and gamma_to_1 null
     [4, 24, 2, 16, 3, 48],   // 12 with gamma_16_to_1 null
     [2, 16, 6, 16, 4, 64],   // 13 with gamma_16_from_1 null
     [4, 16, 6, 8, 4, 32],    // 14 RGBA 8 with gamma and the second coefficient set
     [3, 18, 2, 16, 3, 48],   // 15 RGB 16 with gamma and shift 2
    ].forEach((r, i) => {
      const p = hdr.add(i * 0xc);
      p.writeU32(r[0]); p.add(4).writeU32(r[1]); p.add(8).writeU8(r[2]);
      p.add(9).writeU8(r[3]); p.add(0xa).writeU8(r[4]); p.add(0xb).writeU8(r[5]);
      out['gh' + i] = p;
    });
    out.ghblk = hdr;

    // Two rows: the 8-bit one has grey pixels (0x40,0x40,0x40) and coloured ones, read as groups of
    // three or of four; the 16-bit one has big-endian triples that are equal and triples that are not.
    const rowblk = Memory.alloc(0x100);
    for (let i = 0; i < 0x100; i += 4) rowblk.add(i).writeU32((0x6868 << 16) + i >>> 0);
    const r8 = rowblk.add(0x20), r16 = rowblk.add(0x80);
    [0x40, 0x40, 0x40, 0x10, 0x20, 0x30, 0x80, 0x80, 0x80, 0xff, 0x00, 0x7f,
     0x05, 0x05, 0x05, 0xaa, 0xbb, 0xcc, 0x01, 0x01, 0x01, 0x77, 0x88, 0x99]
      .forEach((b, i) => r8.add(i).writeU8(b));
    [0x12, 0x34, 0x12, 0x34, 0x12, 0x34,
     0x00, 0x01, 0x80, 0x00, 0xff, 0xfe,
     0x0a, 0x0b, 0x0a, 0x0b, 0x0a, 0x0b,
     0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
     0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
     0x41, 0x42, 0x43, 0x44, 0x45, 0x46]
      .forEach((b, i) => r16.add(i).writeU8(b));
    out.growblk = rowblk;
    out.grow8 = r8;
    out.grow16 = r16;
    return out;
  },

  // ------------------------------------- png_do_read_interlace 0x1007c810 and png_combine_row
  // 0x1007c310. read_interlace needs only a header and a row; combine_row needs a scratch png_struct
  // (width +0xc8, transformations +0x70, row_buf +0xec, row_info.pixel_depth +0x10b).
  c3bg_png() {
    const out = {};
    const hdr = Memory.alloc(16 * 0xc);
    [[4, 1, 0, 1, 1, 1],      // 0  pixel_depth 1, pass 0
     [4, 1, 0, 1, 1, 1],      // 1  pixel_depth 1, pass 0, packswap
     [3, 1, 0, 2, 1, 2],      // 2  pixel_depth 2, pass 2
     [3, 1, 0, 2, 1, 2],      // 3  pixel_depth 2, pass 2, packswap
     [2, 1, 0, 4, 1, 4],      // 4  pixel_depth 4, pass 4
     [2, 1, 0, 4, 1, 4],      // 5  pixel_depth 4, pass 4, packswap
     [4, 4, 0, 8, 1, 8],      // 6  pixel_depth 8 (the default body), pass 1
     [3, 6, 0, 16, 1, 16],    // 7  pixel_depth 16, pass 3
     [2, 6, 2, 24, 3, 24],    // 8  pixel_depth 24, pass 5, packswap (the default body ignores it)
     [2, 8, 6, 32, 4, 32],    // 9  pixel_depth 32, pass 6 (repeat count 1)
     [5, 1, 0, 1, 1, 1],      // 10 pixel_depth 1, pass 6
     [0, 0, 0, 2, 1, 2],      // 11 width 0
     [4, 1, 0, 1, 1, 1],      // 12 run with row null
     [4, 1, 0, 1, 1, 1],      // 13 (unused header: the row_info argument of that vector is null)
     [3, 2, 0, 4, 1, 4],      // 14 pixel_depth 4, pass 1, packswap
     [2, 2, 0, 8, 1, 8],      // 15 pixel_depth 8, pass 0, packswap
    ].forEach((r, i) => {
      const p = hdr.add(i * 0xc);
      p.writeU32(r[0]); p.add(4).writeU32(r[1]); p.add(8).writeU8(r[2]);
      p.add(9).writeU8(r[3]); p.add(0xa).writeU8(r[4]); p.add(0xb).writeU8(r[5]);
      out['ih' + i] = p;
    });
    out.ihblk = hdr;

    const irow = Memory.alloc(0x100);
    for (let i = 0; i < 0x100; i++) irow.add(i).writeU8((i * 37 + 0x5f) & 0xff);
    out.irow = irow;

    // Thirteen scratch png_struct records at a 0x140 stride; only the four fields above are read.
    const pblk = Memory.alloc(13 * 0x140);
    for (let i = 0; i < 13 * 0x140; i += 4) pblk.add(i).writeU32((0x7070 << 16) + i >>> 0);
    const rbuf = Memory.alloc(0x80);
    for (let i = 0; i < 0x80; i++) rbuf.add(i).writeU8((i * 83 + 0x0d) & 0xff);
    const crow = Memory.alloc(0x80);
    for (let i = 0; i < 0x80; i++) crow.add(i).writeU8((i * 113 + 0x07) & 0xff);
    out.cblk = pblk; out.crbuf = rbuf; out.crow = crow;
    //   width, transformations, pixel_depth
    const CP = [
      [8, 0, 1],          // c0  depth 1
      [8, 0x10000, 1],    // c1  depth 1, packswap
      [8, 0, 2],          // c2  depth 2
      [8, 0x10000, 2],    // c3  depth 2, packswap
      [6, 0, 4],          // c4  depth 4
      [6, 0x10000, 4],    // c5  depth 4, packswap
      [6, 0, 8],          // c6  the default body, one byte per pixel
      [4, 0, 16],         // c7  two bytes per pixel
      [3, 0x10000, 24],   // c8  three bytes per pixel
      [2, 0, 32],         // c9  four bytes per pixel
      [0, 0, 8],          // c10 width 0
      [16, 0, 1],         // c11 sixteen pixels: the bit walker wraps from 1 back to 0x80
      [10, 0, 8],         // c12 ten pixels through the default body: its own walker wraps too
    ];
    CP.forEach((c, i) => {
      const p = pblk.add(i * 0x140);
      p.add(0x70).writeU32(c[1]);
      p.add(0xc8).writeU32(c[0]);
      p.add(0xec).writePointer(rbuf);
      p.add(0x10b).writeU8(c[2]);
      out['c' + i] = p;
    });
    return out;
  },

  // --------------------------------------------- png_set_sRGB_gAMA_and_cHRM 0x1007e120. The
  // png_struct is only tested against null; the png_info takes every store: valid +8, gamma +0x28,
  // srgb_intent +0x2c and the eight chromaticities +0x80..+0x9c.
  c3bg_set() {
    const blk = Memory.alloc(12 * 0x100);
    for (let i = 0; i < 12 * 0x100; i += 4) blk.add(i).writeU32((0x7373 << 16) + i >>> 0);
    const out = { sblk: blk };
    // a non-null png_struct of our own; no field of it is read
    const png = Memory.alloc(0x100);
    for (let i = 0; i < 0x100; i += 4) png.add(i).writeU32((0x7474 << 16) + i >>> 0);
    out.spng = png;
    // eleven png_info records, each with a different starting `valid` word so the three OR-stores
    // (0x10 from sRGB, 1 from gAMA, 4 from cHRM) land on a different value every time
    const VALID = [0, 1, 4, 0x10, 0x15, 0xff, 0x8000, 0x12345678, 2, 0x20, 0xffffffff];
    VALID.forEach((v, i) => {
      const p = blk.add(i * 0x100);
      p.add(8).writeU32(v);
      out['si' + i] = p;
    });
    return out;
  },

  // ---------------------------------------------------------------------- inflate_fast 0x100a0340.
  // Sixteen independent (inflate_blocks_state, z_stream, window, input) sets and seven Huffman tables
  // of our own. An inflate_huft is eight bytes: Exop +0, Bits +1, Base or Next +4.
  //   state:  bitk +0x1c, bitb +0x20, window +0x24, end +0x28, read +0x2c, write +0x30
  //   stream: next_in +0, avail_in +4, total_in +8, msg +0x18
  // Nothing here is zlib's own state: the trees are built below and the windows are ours, so the only
  // module data the body touches is the read-only inflate_mask table and the two message strings.
  c3bg_zlib() {
    const out = {};
    const WIN = 512;
    const tblk = Memory.alloc(0x400);
    for (let i = 0; i < 0x400; i += 4) tblk.add(i).writeU32((0x7A7A << 16) + i >>> 0);
    out.tblk = tblk;
    let tnext = 0;
    // entries: [exop, bits, base] or [exop, bits, {next: <NativePointer>}]
    const mkTree = (name, entries) => {
      const p = tblk.add(tnext);
      tnext += entries.length * 8;
      entries.forEach((e, i) => {
        const q = p.add(i * 8);
        q.writeU8(e[0]);
        q.add(1).writeU8(e[1]);
        q.add(2).writeU16(0);
        if (e[2] !== null && typeof e[2] === 'object') q.add(4).writePointer(e[2]);
        else q.add(4).writeU32(e[2] >>> 0);
      });
      out[name] = p;
      return p;
    };

    // second level of the literal/length tree, reached through exop 1 (one index bit)
    const SUB = mkTree('zsub', [
      [0, 3, 0x42],      // literal 'B'
      [16, 3, 4],        // length 4, no extra bits
    ]);
    // main literal/length tree, bl = 2 so the index is two bits
    mkTree('ztlA', [
      [0, 2, 0x41],      // 0 literal 'A'
      [16, 2, 3],        // 1 length 3, no extra bits
      [1, 2, SUB],       // 2 one more index bit in the second level
      [96, 2, 0],        // 3 end of block (32 | 64)
    ]);
    mkTree('ztlB', [
      [0, 2, 0x41],
      [16, 2, 3],
      [1, 2, SUB],
      [64, 2, 0],        // 3 invalid literal/length code
    ]);
    mkTree('ztlC', [
      [0, 2, 0x41],
      [17, 2, 6],        // 1 length 6 plus one extra bit
      [1, 2, SUB],
      [96, 2, 0],
    ]);
    mkTree('ztlD', [
      [0, 2, 0x43],      // 0 literal 'C'
      [16, 13, 3],       // 1 a thirteen-bit code: the next refill loops
      [0, 2, 0x44],      // 2 literal 'D'
      [96, 2, 0],
    ]);
    // distance trees, bd = 1 so the index is one bit
    const TDSUB = mkTree('ztdsub', [
      [16, 2, 3],
      [16, 2, 7],
    ]);
    mkTree('ztdA', [
      [16, 1, 1],        // 0 distance 1, no extra bits
      [18, 1, 5],        // 1 distance 5 plus two extra bits
    ]);
    mkTree('ztdB', [
      [16, 1, 4],
      [64, 1, 0],        // 1 invalid distance code
    ]);
    mkTree('ztdC', [
      [1, 1, TDSUB],     // 0 one more index bit in the second level
      [16, 1, 2],
    ]);
    mkTree('ztdD', [
      [24, 13, 2],       // 0 a thirteen-bit code with eight extra bits: the refill loops again
      [16, 1, 1],
    ]);

    // the sixteen (state, stream) sets
    const sblk = Memory.alloc(16 * 0x80);
    for (let i = 0; i < 16 * 0x80; i += 4) sblk.add(i).writeU32((0x7B7B << 16) + i >>> 0);
    const wblk = Memory.alloc(16 * (WIN + 0x80));
    out.zsblk = sblk;
    out.zwblk = wblk;
    //   readOff, writeOff, availIn, firstInputByte, bitb, bitk
    const ST = [
      [110, 10, 12, 0x00, 0, 0],     // 0  literal, read ahead of write
      [110, 10, 12, 0x01, 0, 0],     // 1  length 3, distance 1, the direct copy
      [110, 2, 12, 0x05, 0, 0],      // 2  distance past the write pointer, copy length not past the end
      [110, 0, 12, 0x05, 0, 0],      // 3  the same with a copy that does cross the end of the window
      [110, 10, 12, 0x02, 0, 0],     // 4  second-level literal
      [110, 10, 12, 0x06, 0, 0],     // 5  second-level length
      [110, 10, 12, 0x03, 0, 0],     // 6  end of block
      [110, 10, 12, 0x03, 0, 0],     // 7  invalid literal/length code
      [110, 10, 12, 0x05, 0, 0],     // 8  invalid distance code
      [110, 10, 12, 0x01, 0, 0],     // 9  second-level distance
      [110, 10, 12, 0xf1, 0, 0],     // 10 thirteen-bit codes: both later refill loops run
      [110, 10, 12, 0xaa, 0, 24],    // 11 the bit buffer is already full: the first refill does not run
      [400, 0, 20, 0x0c, 0, 0],      // 12 room and input for a second pass of the outer loop
      [0, 20, 5, 0x00, 0, 0],        // 13 the write pointer at or after the read pointer
      [110, 10, 12, 0x1d, 0, 0],     // 14 a distance with two extra bits set
      [110, 10, 12, 0x02, 0, 0],     // 15 a literal out of another tree
    ];
    ST.forEach((s, i) => {
      const st = sblk.add(i * 0x80);
      const z = sblk.add(i * 0x80 + 0x40);
      // the input buffer sits 0x20 bytes past the end of the window so that `end` never points into
      // it; the thirteen-bit vectors read five of its bytes
      const win = wblk.add(i * (WIN + 0x80));
      const inp = win.add(WIN + 0x20);
      for (let j = 0; j < WIN; j++) win.add(j).writeU8((j * 31 + i * 7 + 0x3b) & 0xff);
      for (let j = 0; j < 0x40; j++) inp.add(j).writeU8(0);
      inp.writeU8(s[3]);
      st.add(0x1c).writeU32(s[5]);                 // bitk
      st.add(0x20).writeU32(s[4]);                 // bitb
      st.add(0x24).writePointer(win);              // window
      st.add(0x28).writePointer(win.add(WIN));     // end
      st.add(0x2c).writePointer(win.add(s[0]));    // read
      st.add(0x30).writePointer(win.add(s[1]));    // write
      z.writePointer(inp);                         // next_in
      z.add(4).writeU32(s[2]);                     // avail_in
      z.add(8).writeU32(i * 1000);                 // total_in
      z.add(0x18).writePointer(ptr(0));            // msg
      out['zs' + i] = st;
      out['zz' + i] = z;
    });
    return out;
  },

  // ---------------------------------------------------------------------- inflate_codes 0x1009f370.
  // Eighteen independent sets, each with its own inflate_codes_state (mode +0, len +4, the union at
  // +8 and +0xc, lbits +0x10, dbits +0x11, ltree +0x14, dtree +0x18), inflate_blocks_state, z_stream,
  // 512-byte window, input buffer and output buffer, plus five Huffman tables. The blocks state's
  // check function (+0x34) is left null, so inflate_flush copies without calling anything.
  c3bg_codes() {
    const out = {};
    const WIN = 512;
    const tblk = Memory.alloc(0x200);
    for (let i = 0; i < 0x200; i += 4) tblk.add(i).writeU32((0x6363 << 16) + i >>> 0);
    out.ctblk = tblk;
    let tnext = 0;
    const mkTree = (name, entries) => {
      const p = tblk.add(tnext);
      tnext += entries.length * 8;
      entries.forEach((e, i) => {
        const q = p.add(i * 8);
        q.writeU8(e[0]);
        q.add(1).writeU8(e[1]);
        q.add(2).writeU16(0);
        if (e[2] !== null && typeof e[2] === 'object') q.add(4).writePointer(e[2]);
        else q.add(4).writeU32(e[2] >>> 0);
      });
      out[name] = p;
      return p;
    };
    const SUB = mkTree('csub', [[0, 3, 0x42], [16, 3, 4]]);
    const TL1 = mkTree('ctl1', [[0, 2, 0x41], [16, 2, 3], [1, 2, SUB], [96, 2, 0]]);
    const TL2 = mkTree('ctl2', [[0, 2, 0x41], [16, 2, 3], [1, 2, SUB], [64, 2, 0]]);
    const TD1 = mkTree('ctd1', [[16, 1, 1], [18, 1, 5]]);
    const TD2 = mkTree('ctd2', [[16, 1, 4], [64, 1, 0]]);
    const TREES = [[TL1, TD1], [TL2, TD1], [TL1, TD2]];

    const cblk = Memory.alloc(18 * 0x40);          // the eighteen inflate_codes_state records
    const sblk = Memory.alloc(18 * 0x80);          // the eighteen (blocks state, stream) pairs
    const wblk = Memory.alloc(18 * (WIN + 0x80));  // the windows and their input buffers
    const oblk = Memory.alloc(18 * 0x800);         // the output buffers inflate_flush writes into
    for (let i = 0; i < 18 * 0x40; i += 4) cblk.add(i).writeU32((0x6464 << 16) + i >>> 0);
    for (let i = 0; i < 18 * 0x80; i += 4) sblk.add(i).writeU32((0x6565 << 16) + i >>> 0);
    out.ccblk = cblk; out.csblk = sblk; out.cwblk = wblk; out.coblk = oblk;

    //  mode, readOff, writeOff, availIn, firstInputByte, availOut, len, subA, subB, treeSel
    const V = [
      [0, 110, 10, 4, 0x00, 1024, 0, 0, 0, 0],      // 0  START with no room for inflate_fast
      [0, 400, 0, 20, 0x0c, 1024, 0, 0, 0, 0],      // 1  START that does call inflate_fast
      [1, 110, 10, 4, 0x01, 1024, 0, 'L', 2, 0],    // 2  LEN -> LENEXT -> DIST -> DISTEXT -> COPY
      [1, 110, 10, 4, 0x02, 1024, 0, 'L', 2, 0],    // 3  LEN -> the second-level table
      [1, 110, 10, 4, 0x03, 1024, 0, 'L', 2, 0],    // 4  LEN -> end of block
      [1, 110, 10, 4, 0x03, 1024, 0, 'L', 2, 1],    // 5  LEN -> invalid literal/length code
      [3, 110, 10, 4, 0x01, 1024, 0, 'D', 1, 2],    // 6  DIST -> invalid distance code
      [5, 100, 512, 4, 0x00, 1024, 4, 0, 2, 0],     // 7  COPY with the write pointer at the end
      [5, 51, 50, 4, 0x00, 0, 4, 0, 2, 0],          // 8  COPY with no output room at all
      [5, 51, 50, 4, 0x00, 1024, 4, 0, 2, 0],       // 9  COPY where the flush frees room
      [6, 110, 10, 2, 0x00, 1024, 0, 0x5a, 0, 0],   // 10 LIT
      [7, 100, 10, 4, 0x00, 1024, 0, 0, 0, 0],      // 11 WASH where the read pointer catches up
      [7, 100, 10, 4, 0x00, 8, 0, 0, 0, 0],         // 12 WASH where it does not
      [8, 110, 10, 4, 0x00, 1024, 0, 0, 0, 0],      // 13 END
      [9, 110, 10, 4, 0x00, 1024, 0, 0, 0, 0],      // 14 BADCODE
      [10, 110, 10, 4, 0x00, 1024, 0, 0, 0, 0],     // 15 a mode past the jump table
      [1, 110, 10, 0, 0x00, 1024, 0, 'L', 2, 0],    // 16 LEN with no input: NEEDBITS leaves at once
      [4, 110, 10, 4, 0x00, 1024, 3, 1, 2, 0],      // 17 DISTEXT with one extra bit, then COPY
    ];
    V.forEach((v, i) => {
      const cs = cblk.add(i * 0x40);
      const st = sblk.add(i * 0x80);
      const z = sblk.add(i * 0x80 + 0x40);
      const win = wblk.add(i * (WIN + 0x80));
      const inp = win.add(WIN + 0x20);
      const obuf = oblk.add(i * 0x800);
      for (let j = 0; j < WIN; j++) win.add(j).writeU8((j * 29 + i * 11 + 0x17) & 0xff);
      for (let j = 0; j < 0x40; j++) inp.add(j).writeU8(0);
      inp.writeU8(v[4]);
      for (let j = 0; j < 0x800; j++) obuf.add(j).writeU8((j * 37 + i * 5 + 0x91) & 0xff);
      const trees = TREES[v[9]];
      cs.writeU32(v[0]);                            // mode
      cs.add(4).writeU32(v[6]);                     // len
      if (v[7] === 'L') cs.add(8).writePointer(trees[0]);
      else if (v[7] === 'D') cs.add(8).writePointer(trees[1]);
      else cs.add(8).writeU32(v[7] >>> 0);          // sub.lit or sub.copy.get
      cs.add(0xc).writeU32(v[8] >>> 0);             // sub.code.need or sub.copy.dist
      cs.add(0x10).writeU8(2);                      // lbits
      cs.add(0x11).writeU8(1);                      // dbits
      cs.add(0x14).writePointer(trees[0]);          // ltree
      cs.add(0x18).writePointer(trees[1]);          // dtree
      st.add(0x0c).writePointer(cs);                // s->sub.decode.codes
      st.add(0x1c).writeU32(0);                     // bitk
      st.add(0x20).writeU32(0);                     // bitb
      st.add(0x24).writePointer(win);               // window
      st.add(0x28).writePointer(win.add(WIN));      // end
      st.add(0x2c).writePointer(win.add(v[1]));     // read
      st.add(0x30).writePointer(win.add(v[2]));     // write
      st.add(0x34).writePointer(ptr(0));            // checkfn: none
      st.add(0x38).writeU32(0x1234 + i);            // check
      z.writePointer(inp);                          // next_in
      z.add(4).writeU32(v[3]);                      // avail_in
      z.add(8).writeU32(i * 100);                   // total_in
      z.add(0xc).writePointer(obuf);                // next_out
      z.add(0x10).writeU32(v[5]);                   // avail_out
      z.add(0x14).writeU32(i * 10);                 // total_out
      z.add(0x18).writePointer(ptr(0));             // msg
      z.add(0x30).writeU32(0x9abc + i);             // adler
      out['cs' + i] = st;
      out['cz' + i] = z;
    });
    return out;
  },
});
