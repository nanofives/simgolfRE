// Batch c3ao fixtures (jgld.dll). Every block is allocated here; nothing live in the running jgld.dll (no Display,
// Surface, Palette, Font or png_struct the game is using) is handed to the A/B. Layouts come from the functions'
// own field offsets, cited in shim/src/re/c3ao.cpp.
Object.assign(globalThis.DIFF_FIXTURES, {

  // libpng row transformations. A png_row_info is 0xc bytes: width +0, rowbytes +4, color_type +8, bit_depth +9,
  // channels +0xa, pixel_depth +0xb. Each of the four functions gets its own array of 12 headers (one block, so
  // $inv0 / $swp0 / $pks0 / $chp0 are the block bases a state region can cover) and its own 64-byte row buffer.
  // Every header keeps its work inside those 64 bytes: invert and packswap touch rowbytes bytes, swap and chop
  // touch 2 * width * channels bytes.
  c3ao_rows() {
    const out = {};
    const mkHeaders = (name, rows) => {
      const blk = Memory.alloc(rows.length * 0xc);
      rows.forEach((r, i) => {
        const p = blk.add(i * 0xc);
        p.writeU32(r[0]);                 // width
        p.add(4).writeU32(r[1]);          // rowbytes
        p.add(8).writeU8(r[2]);           // color_type
        p.add(9).writeU8(r[3]);           // bit_depth
        p.add(0xa).writeU8(r[4]);         // channels
        p.add(0xb).writeU8(r[5]);         // pixel_depth
        out[name + i] = p;
      });
    };
    const mkRow = (name) => {
      const p = Memory.alloc(64);
      // a byte pattern with no repeats in any 4-byte window, so a wrong index or stride shows in the state hash
      for (let i = 0; i < 64; i++) p.add(i).writeU8((i * 7 + 0x13) & 0xff);
      out[name] = p;
    };

    // invert: works only at bit_depth 1 with color_type 0; rowbytes drives the loop (0 = no write).
    mkHeaders('inv', [
      [8, 8, 0, 1, 1, 1],      // 0  inverts 8 bytes
      [0, 0, 0, 1, 1, 1],      // 1  rowbytes 0: loop body never runs
      [8, 1, 0, 1, 1, 1],      // 2  one byte
      [64, 32, 0, 1, 1, 1],    // 3  32 bytes
      [8, 8, 4, 1, 1, 1],      // 4  color_type 4: early return
      [8, 8, 0, 8, 1, 8],      // 5  bit_depth 8: early return
      [8, 8, 0, 2, 1, 2],      // 6  bit_depth 2: early return
      [8, 8, 3, 1, 1, 1],      // 7  color_type 3: early return
      [8, 8, 2, 16, 3, 48],    // 8  bit_depth 16: early return
      [8, 16, 0, 1, 1, 1],     // 9  16 bytes
      [8, 64, 0, 1, 1, 1],     // 10 the whole buffer
      [8, 5, 0, 1, 1, 1],      // 11 odd length
    ]);
    mkRow('invrow');

    // swap: bit_depth 16 only; the sample count is width * channels, each sample two bytes.
    mkHeaders('swp', [
      [8, 16, 2, 16, 1, 16],   // 0  8 samples
      [0, 0, 2, 16, 1, 16],    // 1  width 0: loop body never runs
      [1, 2, 2, 16, 1, 16],    // 2  one sample
      [4, 24, 2, 16, 3, 48],   // 3  12 samples (channels 3)
      [2, 16, 6, 16, 4, 64],   // 4  8 samples (channels 4)
      [32, 64, 0, 16, 1, 16],  // 5  32 samples: the whole buffer
      [8, 16, 0, 8, 1, 8],     // 6  bit_depth 8: early return
      [8, 16, 0, 1, 1, 1],     // 7  bit_depth 1: early return
      [8, 16, 0, 0, 1, 0],     // 8  bit_depth 0: early return
      [16, 32, 2, 16, 1, 16],  // 9  16 samples
      [5, 10, 2, 16, 2, 32],   // 10 10 samples
      [8, 16, 2, 16, 0, 0],    // 11 channels 0: count 0
    ]);
    mkRow('swprow');

    // packswap: bit_depth below 8 only, and only 1, 2 and 4 select a table.
    mkHeaders('pks', [
      [8, 8, 0, 1, 1, 1],      // 0  table at 0x10122e6c
      [8, 8, 0, 2, 1, 2],      // 1  table at 0x10122f6c
      [8, 8, 0, 4, 1, 4],      // 2  table at 0x1012306c
      [8, 8, 0, 3, 1, 3],      // 3  below 8 but no table: returns
      [8, 8, 0, 8, 1, 8],      // 4  bit_depth 8: early return
      [8, 8, 0, 16, 1, 16],    // 5  bit_depth 16: early return
      [8, 0, 0, 1, 1, 1],      // 6  rowbytes 0: loop body never runs
      [8, 64, 0, 1, 1, 1],     // 7  the whole buffer
      [8, 1, 0, 2, 1, 2],      // 8  one byte
      [8, 32, 0, 4, 1, 4],     // 9  32 bytes
      [8, 8, 0, 5, 1, 5],      // 10 below 8 but no table
      [8, 8, 0, 0, 1, 0],      // 11 bit_depth 0: below 8, no table
    ]);
    mkRow('pksrow');

    // chop: bit_depth 16 only; it also rewrites the header, so the header block is part of its state.
    mkHeaders('chp', [
      [8, 16, 2, 16, 1, 16],   // 0  8 samples
      [0, 0, 2, 16, 1, 16],    // 1  no sample, header still rewritten
      [1, 2, 2, 16, 1, 16],    // 2  one sample
      [4, 24, 6, 16, 3, 48],   // 3  12 samples
      [2, 16, 6, 16, 4, 64],   // 4  8 samples
      [32, 64, 0, 16, 1, 16],  // 5  32 samples
      [8, 16, 0, 8, 1, 8],     // 6  bit_depth 8: early return, header untouched
      [8, 16, 0, 1, 1, 1],     // 7  bit_depth 1: early return
      [16, 32, 2, 16, 1, 16],  // 8  16 samples
      [5, 10, 2, 16, 2, 32],   // 9  10 samples
      [3, 6, 2, 16, 1, 16],    // 10 3 samples
      [8, 16, 2, 16, 0, 0],    // 11 channels 0: no sample, pixel_depth and rowbytes become 0
    ]);
    mkRow('chprow');
    return out;
  },

  // fillWords: one 160-byte destination, prefilled so an unwritten tail is visible in the state hash. The largest
  // vector (n = 40) writes 80 bytes.
  c3ao_fill() {
    const p = Memory.alloc(160);
    for (let i = 0; i < 160; i++) p.add(i).writeU8((i * 11 + 5) & 0xff);
    return { fw: p };
  },

  // Palette::toRGBQuads: 12 palette objects, each 0xc header bytes plus 256 four-byte entries, in one block
  // ($pal0 is the base). Entry bytes are generated so the three copied bytes differ from one another and from the
  // fourth (which the function never reads), and so no two palettes agree. $quads is the 1 KB destination,
  // prefilled with a constant that the function must overwrite completely.
  c3ao_pal() {
    const SZ = 0xc + 0x400;
    const blk = Memory.alloc(12 * SZ);
    const out = {};
    for (let i = 0; i < 12; i++) {
      const p = blk.add(i * SZ);
      for (let k = 0; k < 0xc; k++) p.add(k).writeU8(0xa5);        // header bytes, never read by this function
      for (let j = 0; j < 256; j++) {
        const e = p.add(0xc + j * 4);
        e.writeU8((j * (i + 1) + 1) & 0xff);
        e.add(1).writeU8((j ^ (i * 7 + 3)) & 0xff);
        e.add(2).writeU8((j * 3 + i * 29) & 0xff);
        e.add(3).writeU8((0xd0 + i) & 0xff);
      }
      out['pal' + i] = p;
    }
    const q = Memory.alloc(0x400);
    for (let k = 0; k < 0x400; k++) q.add(k).writeU8(0x77);
    out.quads = q;
    return out;
  },

  // png_sig_cmp: five 8-byte buffers. $sig_ok holds the 8 signature bytes of the PNG specification (the same
  // sequence the module keeps at 0x10123198), the others differ in the first byte, in the last byte, everywhere
  // (zeros) and everywhere (0xff), so the comparison returns 0, a negative and a positive difference.
  c3ao_sig() {
    const sig = [0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a];
    const mk = (bytes) => { const p = Memory.alloc(8); bytes.forEach((b, i) => p.add(i).writeU8(b)); return p; };
    const bad0 = sig.slice(); bad0[0] = 0x8a;
    const bad7 = sig.slice(); bad7[7] = 0x0b;
    return {
      sig_ok: mk(sig), sig_bad0: mk(bad0), sig_bad7: mk(bad7),
      sig_zero: mk([0, 0, 0, 0, 0, 0, 0, 0]), sig_ff: mk([255, 255, 255, 255, 255, 255, 255, 255]),
    };
  },

  // png_calculate_crc: 12 scratch png_structs of 0x140 bytes in one block ($crc0 is the base, so one state region
  // covers every running CRC at +0x110). Only three fields matter: the chunk-name byte at +0x11c (bit 0x20 picks
  // the ancillary arm), the flag dword at +0x6c and the CRC at +0x110. $cdata is the buffer fed to crc32.
  c3ao_crc() {
    const SZ = 0x140;
    // [chunk_name[0], flags, starting crc]
    const defs = [
      [0x62, 0x00000300, 0x00000000],   // 0  ancillary, both flag bits: no CRC
      [0x62, 0x00000100, 0x11111111],   // 1  ancillary, one bit only: CRC computed
      [0x62, 0x00000200, 0x22222222],   // 2  ancillary, the other bit: CRC computed
      [0x62, 0x00000000, 0x33333333],   // 3  ancillary, no bits: CRC computed
      [0x62, 0x000003ff, 0x44444444],   // 4  ancillary, both bits inside a wider mask: no CRC
      [0x49, 0x00000800, 0x55555555],   // 5  critical, ignore bit: no CRC
      [0x49, 0x00000000, 0x66666666],   // 6  critical, no bits: CRC computed
      [0x49, 0x00000300, 0x77777777],   // 7  critical: the 0x300 pair does not apply, CRC computed
      [0x49, 0x00000fff, 0x88888888],   // 8  critical, ignore bit inside a wider mask: no CRC
      [0x20, 0x00000000, 0x99999999],   // 9  bit 0x20 alone marks it ancillary: CRC computed
      [0xff, 0x00000300, 0xaaaaaaaa],   // 10 ancillary, both bits: no CRC
      [0x00, 0x00000400, 0xffffffff],   // 11 critical, an unrelated bit: CRC computed
    ];
    const blk = Memory.alloc(defs.length * SZ);
    const out = {};
    defs.forEach((d, i) => {
      const p = blk.add(i * SZ);
      for (let o = 0; o < SZ; o += 4) p.add(o).writeU32(0x5a5a5a5a);
      p.add(0x6c).writeU32(d[1]);
      p.add(0x110).writeU32(d[2]);
      p.add(0x11c).writeU32(d[0]);
      out['crc' + i] = p;
    });
    const data = Memory.alloc(64);
    for (let i = 0; i < 64; i++) data.add(i).writeU8((i * 31 + 7) & 0xff);
    out.cdata = data;
    return out;
  },

  // png_set_bKGD / png_set_tIME / png_set_tRNS: one scratch png_info per function (0x100 bytes, prefilled so a
  // missed or misplaced write shows in the state hash) plus the sources they copy from. $pngB is only tested for
  // null by bKGD and tRNS; $pngT0..$pngT3 differ in the dword at +0x6c that tIME tests for bit 0x10000.
  c3ao_set() {
    const out = {};
    const mkInfo = (name) => {
      const p = Memory.alloc(0x100);
      for (let o = 0; o < 0x100; o += 4) p.add(o).writeU32(0x33333333);
      out[name] = p;
    };
    mkInfo('infoB'); mkInfo('infoT'); mkInfo('infoR');
    out.pngB = Memory.alloc(0x100);
    [0x00000000, 0x00010000, 0xffffffff, 0xfffeffff].forEach((f, i) => {
      const p = Memory.alloc(0x100);
      for (let o = 0; o < 0x100; o += 4) p.add(o).writeU32(0);
      p.add(0x6c).writeU32(f);
      out['pngT' + i] = p;
    });
    // 16-byte sources (the copies are 10 and 8 bytes; the tail proves the length is not over-copied)
    for (let i = 0; i < 8; i++) {
      const p = Memory.alloc(16);
      for (let k = 0; k < 16; k++) p.add(k).writeU8((k * 13 + i * 41 + 1) & 0xff);
      out['bk' + i] = p;
      if (i < 6) out['tm' + i] = p;
      if (i < 4) out['tv' + i] = p;
    }
    out.tr = Memory.alloc(32);
    out.tr2 = Memory.alloc(32);
    return out;
  },

  // png_set_strip_alpha / png_set_error_fn / png_init_io: three arrays of 12 scratch png_structs of 0x80 bytes,
  // one block each ($sa0 / $ef0 / $io0 are the bases, so one state region per function covers all 12). The
  // transformations dword at +0x70 starts at a different value in every $sa object, including ones that already
  // have bit 0x40000 set.
  c3ao_simple() {
    const out = {};
    const mkBlock = (name, fill) => {
      const blk = Memory.alloc(12 * 0x80);
      for (let i = 0; i < 12; i++) {
        const p = blk.add(i * 0x80);
        for (let o = 0; o < 0x80; o += 4) p.add(o).writeU32(0x0b0b0b0b);
        if (fill) fill(p, i);
        out[name + i] = p;
      }
    };
    const tr = [0x00000000, 0x00040000, 0xffffffff, 0xfffbffff, 0x00000001, 0x80000000,
                0x00040001, 0x12345678, 0x0004ffff, 0x7fffffff, 0x00000400, 0xaaaaaaaa];
    mkBlock('sa', (p, i) => p.add(0x70).writeU32(tr[i]));
    mkBlock('ef', (p, i) => {
      p.add(0x40).writeU32(0x40404000 + i);
      p.add(0x44).writeU32(0x44444400 + i);
      p.add(0x48).writeU32(0x48484800 + i);
    });
    mkBlock('io', (p, i) => p.add(0x54).writeU32(0x54545400 + i));
    return out;
  },
});
