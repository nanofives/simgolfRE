// Batch c3ax fixtures (jgld.dll). Every block is allocated here; nothing live in the running jgld.dll (no
// Display, Surface, Palette, Font, png_struct or inflate state the game is using) is handed to the A/B. Layouts
// come from the functions' own field offsets, cited in shim/src/re/c3ax.cpp.
Object.assign(globalThis.DIFF_FIXTURES, {

  // jgld's own small objects: 16 Vector3 (12 of the 16 bytes of each slot used), 16 source RECTs and 8
  // destination RECTs (16 bytes used of 32) and 16 ListNode objects (12 bytes used of 32). The whole block is
  // pre-filled with a per-offset word pattern and every object sits at a larger stride than its size, so a store
  // one word too wide or a field left unwritten changes the block hash.
  c3ax_jgld() {
    const blk = Memory.alloc(0x600);
    for (let i = 0; i < 0x600; i += 4) blk.add(i).writeU32((0x5A5A0000 + i) >>> 0);
    const out = { jblk: blk };

    // Components that are not exactly representable in binary, so a product rounded at the wrong width shows.
    const V = [
      [0.1, 0.3333333432674408, 1e-7],
      [3e5, 0.7, 1234.567],
      [1.0000001, 0.9999999, 123456.79],
      [0.1, 0.3, 7.0],
      [1e-7, 1e7, 0.1],
      [1e7, 1e-7, 0.1],
      [16777215.0, 1.0, 0.5],
      [1.0000001, 16777215.0, 3.0],
      [0.2, 0.30000001192092896, 0.7],
      [0.8, 0.6, 0.9],
      [-2.5e-5, 1.5e6, 123.456],
      [0.0077, -33000.0, 1e-7],
      [-0.1, -0.3333333432674408, -1e-7],
      [1e-30, 1e30, 1.0],
      [0.0, -0.0, 1.0],
      [123.456, 0.0077, 65535.5],
    ];
    for (let i = 0; i < 16; i++) {
      const p = blk.add(i * 0x10);
      V[i].forEach((x, j) => p.add(4 * j).writeFloat(x));
      out['v' + i] = p;
    }

    // RECT {LONG left, top, right, bottom}: overlapping, contained, edge-touching, disjoint, negative, empty
    // (zero area, left == right, top == bottom) and inverted (right < left) rectangles, plus two pairs that
    // differ in a single edge.
    const R = [
      [0, 0, 10, 10], [5, 5, 15, 15], [10, 10, 20, 20], [20, 20, 30, 30],
      [-5, -5, 5, 5], [0, 0, 0, 0], [2, 3, 8, 9], [0, 0, 10, 10],
      [-100, -100, 100, 100], [7, 7, 7, 20], [7, 7, 20, 7], [20, 20, 7, 7],
      [1, 2, 3, 4], [1, 2, 3, 5], [0, 0, 1, 1], [0, 0, 1, 2],
    ];
    for (let i = 0; i < 16; i++) {
      const p = blk.add(0x100 + i * 0x20);
      R[i].forEach((x, j) => p.add(4 * j).writeS32(x));
      out['r' + i] = p;
    }
    // destinations keep the pattern words until a call writes them
    for (let i = 0; i < 8; i++) out['d' + i] = blk.add(0x300 + i * 0x20);
    // ListNode objects: the three words the destructor writes start at values it cannot produce
    for (let i = 0; i < 16; i++) out['n' + i] = blk.add(0x400 + i * 0x20);
    return out;
  },

  // libpng scratch records. png_struct is used only through its running CRC at +0x110 (png_reset_crc) and as a
  // never-dereferenced pointer argument (png_memcpy_check / png_memset_check, whose error path cannot run);
  // png_info is 0xb8 bytes inside a 0x100 slot, so the bytes past the record must survive png_info_init.
  c3ax_png() {
    const blk = Memory.alloc(0x2400);
    for (let i = 0; i < 0x2400; i += 4) blk.add(i).writeU32((0x37370000 + i) >>> 0);
    const out = { pblk: blk };
    for (let i = 0; i < 12; i++) {
      const p = blk.add(i * 0x140);
      p.add(0x110).writeU32((0xC0DE0000 + i * 0x1111) >>> 0);   // a different running CRC per struct
      out['pc' + i] = p;
    }
    for (let i = 0; i < 12; i++) out['pi' + i] = blk.add(0x1000 + i * 0x100);
    out.mdst = blk.add(0x1C00);
    out.msrc = blk.add(0x1E00);
    out.mdst2 = blk.add(0x2000);
    out.msrc2 = blk.add(0x2100);
    out.pngp = blk.add(0x2200);        // non-null, never dereferenced
    // a source pattern with no repeat in any 4-byte window, so a wrong length or offset shows in the hash
    for (let i = 0; i < 0x200; i++) out.msrc.add(i).writeU8((i * 7 + 0x13) & 0xff);
    for (let i = 0; i < 0x100; i++) out.msrc2.add(i).writeU8((i * 11 + 0x55) & 0xff);
    return out;
  },

  // libpng row transformations. A png_row_info is 0xc bytes: width +0, rowbytes +4, color_type +8, bit_depth +9,
  // channels +0xa, pixel_depth +0xb. Each function gets its own contiguous array of headers (so $xxx0 is a block
  // base a state region can cover) and its own row buffer, sized so that every header's walk stays inside it.
  c3ax_rows() {
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
    const mkRow = (name, n) => {
      const p = Memory.alloc(n);
      for (let i = 0; i < n; i++) p.add(i).writeU8((i * 7 + 0x13) & 0xff);
      out[name] = p;
    };

    // png_do_bgr: needs bit 1 of color_type, then bit_depth 8 or 16 and color_type 2 or 6. Largest walk:
    // 8 pixels * 8 bytes = 64 of the 128-byte buffer.
    mkHeaders('bgr', [
      [8, 24, 2, 8, 3, 24],     // 0  8-bit RGB, 8 pixels
      [0, 0, 2, 8, 3, 24],      // 1  width 0: the 8-bit RGB loop body never runs
      [1, 3, 2, 8, 3, 24],      // 2  one pixel
      [8, 32, 6, 8, 4, 32],     // 3  8-bit RGBA, 8 pixels
      [1, 4, 6, 8, 4, 32],      // 4  one pixel
      [8, 48, 2, 16, 3, 48],    // 5  16-bit RGB, 8 pixels
      [0, 0, 2, 16, 3, 48],     // 6  width 0: the 16-bit RGB loop body never runs
      [8, 64, 6, 16, 4, 64],    // 7  16-bit RGBA, 8 pixels
      [2, 16, 6, 16, 4, 64],    // 8  two pixels
      [8, 8, 0, 8, 1, 8],       // 9  color_type 0: the colour-bit guard returns
      [8, 16, 4, 8, 2, 16],     // 10 color_type 4: the colour-bit guard returns
      [8, 24, 3, 8, 1, 8],      // 11 colour bit set, depth 8, color_type neither 2 nor 6: nothing written
      [8, 24, 7, 16, 4, 64],    // 12 colour bit set, depth 16, color_type 7: nothing written
      [8, 24, 2, 4, 3, 12],     // 13 bit_depth 4: neither byte nor 16-bit path
      [8, 24, 6, 1, 4, 4],      // 14 bit_depth 1: neither path
    ]);
    mkRow('bgrrow', 128);

    // png_do_unpack: bit_depth below 8 only; 1, 2 and 4 unpack, any other value below 8 only rewrites the
    // header. Writes width bytes from row[0], reads ceil(width / pixels-per-byte); largest width 16.
    mkHeaders('unp', [
      [8, 1, 0, 1, 1, 1],       // 0  depth 1, 8 pixels (shift starts at 0, wraps once)
      [1, 1, 0, 1, 1, 1],       // 1  one pixel (shift starts at 7)
      [7, 1, 0, 1, 1, 1],       // 2  7 pixels (shift starts at 1)
      [16, 2, 0, 1, 1, 1],      // 3  16 pixels (two source bytes)
      [0, 0, 0, 1, 1, 1],       // 4  width 0: no pixel moves, the header tail still runs
      [8, 2, 0, 2, 1, 2],       // 5  depth 2, 8 pixels
      [3, 1, 0, 2, 1, 2],       // 6  depth 2, 3 pixels (shift starts at 2)
      [8, 4, 0, 4, 1, 4],       // 7  depth 4, 8 pixels
      [5, 3, 0, 4, 1, 4],       // 8  depth 4, 5 pixels (shift starts at 4)
      [8, 8, 0, 8, 1, 8],       // 9  depth 8: the guard returns, nothing at all changes
      [8, 16, 2, 16, 3, 48],    // 10 depth 16: the guard returns
      [8, 1, 0, 3, 1, 3],       // 11 depth 3: no case matches, only the header tail runs
      [8, 0, 0, 0, 1, 0],       // 12 depth 0: no case matches, only the header tail runs
      [4, 1, 0, 2, 3, 6],       // 13 channels 3: the tail's pixel_depth and rowbytes differ
      [6, 1, 0, 1, 2, 2],       // 14 channels 2
    ]);
    mkRow('unprow', 64);

    // png_do_gray_to_rgb: bit_depth >= 8 and the colour bit clear; color_type 0 (grey) or 4 (grey+alpha).
    // Largest walk: 8 pixels * 8 bytes = 64 of the 128-byte buffer.
    mkHeaders('g2r', [
      [8, 8, 0, 8, 1, 8],       // 0  grey 8-bit, 8 pixels
      [1, 1, 0, 8, 1, 8],       // 1  one pixel
      [0, 0, 0, 8, 1, 8],       // 2  width 0: no pixel moves, the header tail still runs
      [8, 16, 0, 16, 1, 16],    // 3  grey 16-bit, 8 pixels
      [2, 4, 0, 16, 1, 16],     // 4  two pixels
      [8, 16, 4, 8, 2, 16],     // 5  grey+alpha 8-bit, 8 pixels
      [1, 2, 4, 8, 2, 16],      // 6  one pixel
      [8, 32, 4, 16, 2, 32],    // 7  grey+alpha 16-bit, 8 pixels
      [2, 8, 4, 16, 2, 32],     // 8  two pixels
      [8, 8, 0, 4, 1, 4],       // 9  bit_depth 4: the depth guard returns
      [8, 24, 2, 8, 3, 24],     // 10 colour bit already set: the second guard returns
      [8, 32, 6, 8, 4, 32],     // 11 colour bit already set (RGBA): returns
      [8, 8, 1, 8, 1, 8],       // 12 color_type 1: passes both guards, no pixel path, tail only
      [8, 8, 5, 8, 1, 8],       // 13 color_type 5: passes both guards, no pixel path, tail only
      [4, 8, 0, 16, 1, 16],     // 14 grey 16-bit, 4 pixels
    ]);
    mkRow('g2rrow', 128);

    // png_do_read_swap_alpha / png_do_read_invert_alpha: color_type 6 or 4 only; both pointers start at
    // row + rowbytes and walk back width * (4, 8, 2 or 4) bytes, which every header below keeps inside the
    // 128-byte buffer.
    const alpha = [
      [8, 32, 6, 8, 4, 32],     // 0  RGBA 8-bit, 8 pixels (32 bytes)
      [1, 4, 6, 8, 4, 32],      // 1  one pixel
      [0, 0, 6, 8, 4, 32],      // 2  width 0: the loop body never runs
      [8, 64, 6, 16, 4, 64],    // 3  RGBA 16-bit, 8 pixels (64 bytes)
      [2, 16, 6, 16, 4, 64],    // 4  two pixels
      [8, 16, 4, 8, 2, 16],     // 5  grey+alpha 8-bit, 8 pixels
      [1, 2, 4, 8, 2, 16],      // 6  one pixel
      [8, 32, 4, 16, 2, 32],    // 7  grey+alpha 16-bit, 8 pixels
      [3, 12, 4, 16, 2, 32],    // 8  three pixels
      [8, 24, 2, 8, 3, 24],     // 9  color_type 2: nothing happens
      [8, 8, 0, 8, 1, 8],       // 10 color_type 0: nothing happens
      [4, 32, 6, 1, 4, 4],      // 11 color_type 6 with bit_depth 1: the "not 8" path (8 bytes per pixel)
      [4, 32, 4, 2, 2, 4],      // 12 color_type 4 with bit_depth 2: the "not 8" path (4 bytes per pixel)
      [6, 24, 6, 8, 4, 32],     // 13 rowbytes 24 with width 6: the end pointer comes from rowbytes
    ];
    mkHeaders('swa', alpha);
    mkRow('swarow', 128);
    mkHeaders('ina', alpha);
    mkRow('inarow', 128);
    return out;
  },

  // zlib: one 16 KB data buffer for crc32 / adler32 (both read it and write nothing), and one block holding four
  // inflate_blocks_state scratch records, their four windows and the dictionary bytes for
  // inflate_set_dictionary. The state's fields used are the window pointer at +0x24 and the two pointers at
  // +0x2c and +0x30; everything else keeps the pattern.
  c3ax_zlib() {
    const data = Memory.alloc(0x4000);
    for (let i = 0; i < 0x4000; i++) data.add(i).writeU8((i * 31 + (i >> 8) * 7 + 0x3b) & 0xff);
    const blk = Memory.alloc(0xb00);
    for (let i = 0; i < 0xb00; i += 4) blk.add(i).writeU32((0x7E7E0000 + i) >>> 0);
    const out = { data, data1: data.add(1), data3: data.add(3), data7: data.add(7), iblk: blk };
    for (let i = 0; i < 4; i++) {
      const st = blk.add(i * 0x40);
      st.add(0x24).writePointer(blk.add(0x200 + i * 0x200));   // window
      out['st' + i] = st;
    }
    out.dict = blk.add(0xa00);
    out.dict8 = blk.add(0xa08);
    for (let i = 0; i < 0x100; i++) blk.add(0xa00 + i).writeU8((i * 13 + 0x27) & 0xff);
    return out;
  },
});
