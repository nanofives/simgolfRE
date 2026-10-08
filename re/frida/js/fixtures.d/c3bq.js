// Batch c3bq fixtures (jgld.dll): png_read_transform_info 0x100714c0, png_set_IHDR 0x1007dc70,
// png_get_IHDR 0x1006eae0, png_set_read_fn 0x10078cb0, Palette::reset 0x1006a380, Palette::apply
// 0x1006a4a0 and Matrix::loadIdentity 0x100045f0.
//
// Every block is allocated here; nothing live in the running jgld.dll (no Display, Surface, Palette,
// Font, the game's png_struct / png_info or its heap blocks) is handed to the A/B. Layouts come from
// the bodies' own field offsets, cited in shim/src/re/c3bq.cpp. Blocks are pre-filled with a
// per-offset word pattern and every record sits at a larger stride than the bytes it uses, so a store
// one field too wide or a field left unwritten changes the block hash.
//
// The three libpng entry points can reach png_warning 0x10079040, which calls the handler the
// png_struct carries at +0x44 and only falls back to png_default_warning 0x10079300 when that slot is
// null (0x1007905b-0x1007906e). Every scratch png_struct here carries a one-byte `ret` of our own
// there, so the warning arms run for real without any I/O.
Object.assign(globalThis.DIFF_FIXTURES, {

  // ------------------------------------------------ png_read_transform_info 0x100714c0.
  // One scratch png_struct per vector at a 0x200 stride (the highest field the body reads is
  // palette_lookup at +0x1ec) and one png_info per vector at a 0x80 stride (the highest field it
  // writes is the ten-byte background at +0x5a). The png_struct block is a state region of its own:
  // the body reads it and writes nothing to it, and that is what the region tests.
  c3bq_rti() {
    const out = {};
    const fill = (p, n, seed) => { for (let i = 0; i < n; i += 4) p.add(i).writeU32(((seed << 16) + i) >>> 0); };

    // palette_lookup: a table the body only tests for null, never indexes.
    const lookup = Memory.alloc(0x400);
    for (let i = 0; i < 0x400; i++) lookup.add(i).writeU8((i * 37 + 0x19) & 0xff);
    out.rtiLookup = lookup;

    // T, info color_type, info bit_depth, png num_trans, width, options
    //   look: palette_lookup is set; ud/uc: user_transform_depth / _channels.
    const V = [
      [0x000000, 0, 8, 0, 16, {}],                       // 0  no bit set at all
      [0x001000, 3, 8, 4, 16, {}],                       // 1  EXPAND, palette with transparency
      [0x001000, 3, 8, 0, 16, {}],                       // 2  EXPAND, palette without
      [0x001000, 0, 4, 2, 16, {}],                       // 3  EXPAND, other, trans, depth under 8
      [0x001000, 2, 8, 0, 16, {}],                       // 4  EXPAND, other, no trans, depth 8
      [0x000080, 6, 8, 3, 16, {}],                       // 5  BACKGROUND: alpha bit and the colour copy
      [0x002000, 2, 8, 0, 16, {}],                       // 6  GAMMA
      [0x000400, 0, 16, 0, 16, {}],                      // 7  16_TO_8 at depth 16
      [0x000400, 0, 8, 0, 16, {}],                       // 8  16_TO_8 at depth 8: nothing happens
      [0x000040, 2, 8, 0, 16, { look: 1 }],              // 9  DITHER, colour type 2
      [0x000040, 6, 8, 0, 16, { look: 1 }],              // 10 DITHER, colour type 6
      [0x000040, 0, 8, 0, 16, { look: 1 }],              // 11 DITHER, colour type rejects
      [0x000040, 2, 8, 0, 16, {}],                       // 12 DITHER, palette_lookup null
      [0x000040, 2, 16, 0, 16, { look: 1 }],             // 13 DITHER, depth rejects
      [0x000004, 0, 2, 0, 16, {}],                       // 14 PACK, depth under 8
      [0x000004, 0, 8, 0, 16, {}],                       // 15 PACK, depth 8
      [0x004000, 0, 8, 0, 16, {}],                       // 16 GRAY_TO_RGB
      [0x200000, 2, 8, 0, 16, {}],                       // 17 RGB_TO_GRAY, the low bit of the mask
      [0x400000, 6, 8, 0, 16, {}],                       // 18 RGB_TO_GRAY, the high bit of the mask
      [0x000000, 3, 8, 0, 16, {}],                       // 19 channels: palette
      [0x000000, 2, 8, 0, 16, {}],                       // 20 channels: colour
      [0x000000, 0, 8, 0, 16, {}],                       // 21 channels: grey
      [0x040000, 6, 8, 0, 16, {}],                       // 22 STRIP_ALPHA
      [0x000000, 4, 8, 0, 16, {}],                       // 23 the alpha bit adds a channel
      [0x008000, 2, 8, 0, 16, {}],                       // 24 FILLER, colour type 2
      [0x008000, 0, 8, 0, 16, {}],                       // 25 FILLER, colour type 0
      [0x008000, 6, 8, 0, 16, {}],                       // 26 FILLER, colour type 6: no channel
      [0x100000, 0, 8, 0, 16, { ud: 16, uc: 4 }],        // 27 USER_TRANSFORM raises both
      [0x100000, 2, 8, 0, 16, { ud: 4, uc: 1 }],         // 28 USER_TRANSFORM raises neither
      [0x00b080, 3, 8, 5, 16, { look: 1 }],              // 29 EXPAND|BACKGROUND|GAMMA|FILLER
      [0x144044, 2, 2, 0, 16, { look: 1, ud: 16, uc: 4 }],  // 30 DITHER|PACK|GRAY_TO_RGB|STRIP_ALPHA|USER
      [0x000000, 6, 16, 0, 0x1000, {}],                  // 31 a wide row: the rowbytes arithmetic
    ];

    const pblk = Memory.alloc(V.length * 0x200);
    fill(pblk, V.length * 0x200, 0x6161);
    const iblk = Memory.alloc(V.length * 0x80);
    fill(iblk, V.length * 0x80, 0x6262);
    out.rtiPng = pblk;
    out.rtiInfo = iblk;

    V.forEach((v, i) => {
      const p = pblk.add(i * 0x200);
      const q = iblk.add(i * 0x80);
      const o = v[5];
      p.add(0x64).writeU8(o.ud || 0);                  // user_transform_depth
      p.add(0x65).writeU8(o.uc || 0);                  // user_transform_channels
      p.add(0x70).writeU32(v[0] >>> 0);                // transformations
      p.add(0x11a).writeU16(v[3]);                     // num_trans
      // background +0x138, ten bytes (red +2, green +4, blue +6, gray +8); each record gets its own
      // values so the ten-byte copy of the BACKGROUND arm is visible in the info block.
      p.add(0x138 + 2).writeU16((0x1100 + i) & 0xffff);
      p.add(0x138 + 4).writeU16((0x2200 + i) & 0xffff);
      p.add(0x138 + 6).writeU16((0x3300 + i) & 0xffff);
      p.add(0x138 + 8).writeU16((0x4400 + i) & 0xffff);
      p.add(0x15c).writeU32((0x7f000000 + i * 0x101) >>> 0);   // gamma, moved as a dword
      p.add(0x1ec).writePointer(o.look ? lookup : ptr(0));     // palette_lookup

      q.writeU32(v[4]);                                // width
      q.add(4).writeU32(0x20 + i);                     // height: never read by this body
      q.add(0xc).writeU32(0xdeadbe00 + i);             // rowbytes: always overwritten
      q.add(0x16).writeU16(0x1000 + i);                // num_trans
      q.add(0x18).writeU8(v[2]);                       // bit_depth
      q.add(0x19).writeU8(v[1]);                       // color_type
      q.add(0x1d).writeU8(0x77);                       // channels: always overwritten
      q.add(0x1e).writeU8(0x88);                       // pixel_depth: always overwritten
      q.add(0x28).writeU32(0x11000000 + i);            // gamma
      for (let j = 0; j < 10; j++) q.add(0x5a + j).writeU8((0xa0 + j + i) & 0xff);   // background
      out['rtiP' + i] = p;
      out['rtiI' + i] = q;
    });
    return out;
  },

  // ------------------------------------------------------------- png_set_IHDR 0x1007dc70.
  // One png_info per vector at a 0x80 stride plus one shared png_struct, which the body only tests for
  // null and hands to png_warning. A bit_depth of 0 would make the divisor at 0x1007dd5a zero and the
  // original faults on the `div`, so no vector passes one.
  c3bq_sihdr() {
    const out = {};
    const fill = (p, n, seed) => { for (let i = 0; i < n; i += 4) p.add(i).writeU32(((seed << 16) + i) >>> 0); };

    const code = Memory.alloc(Process.pageSize);
    Memory.protect(code, Process.pageSize, 'rwx');
    code.writeByteArray([0xc3]);                       // the png_warning handler: __cdecl `ret`
    out.siWarn = code;

    const png = Memory.alloc(0x200);
    fill(png, 0x200, 0x6363);
    png.add(0x44).writePointer(code);
    out.siPng = png;

    // width, height, bit_depth, color_type, interlace, compression, filter
    const V = [
      [16, 8, 8, 0, 0, 0, 0],            // 0  png_ptr null (the vector passes 0 for it)
      [16, 8, 8, 0, 0, 0, 0],            // 1  info_ptr null
      [16, 8, 8, 0, 0, 0, 0],            // 2  grey 8
      [16, 8, 8, 2, 0, 0, 0],            // 3  colour type 2
      [16, 8, 8, 3, 0, 0, 0],            // 4  colour type 3
      [16, 8, 8, 4, 0, 0, 0],            // 5  colour type 4
      [16, 8, 8, 6, 0, 0, 0],            // 6  colour type 6
      [16, 8, 8, 1, 0, 0, 0],            // 7  colour type 1
      [16, 8, 8, 5, 0, 0, 0],            // 8  colour type 5
      [16, 8, 8, 7, 0, 0, 0],            // 9  colour type 7
      [100, 50, 16, 6, 0, 0, 0],         // 10 depth 16, four channels
      [7, 3, 1, 0, 0, 0, 0],             // 11 depth 1
      [5, 5, 2, 3, 1, 0, 0],             // 12 depth 2, palette
      [9, 9, 4, 0, 1, 2, 3],             // 13 the three trailing types, each a different value
      [0x10000000, 1, 16, 6, 0, 0, 0],   // 14 eight bytes a pixel: one past 0x7fffffff / 8
      [0x0fffffff, 1, 16, 6, 0, 0, 0],   // 15 exactly 0x7fffffff / 8: the `jbe` boundary
      [0x7fffffff, 1, 8, 0, 0, 0, 0],    // 16 one byte a pixel, the largest width that passes
      [0x80000000, 1, 8, 0, 0, 0, 0],    // 17 one byte a pixel, the first width that warns
      [0, 0, 8, 2, 0, 0, 0],             // 18 width 0
    ];
    const iblk = Memory.alloc(V.length * 0x80);
    fill(iblk, V.length * 0x80, 0x6464);
    out.siInfo = iblk;
    V.forEach((v, i) => {
      const q = iblk.add(i * 0x80);
      q.add(0xc).writeU32(0xcafe0000 + i);             // rowbytes: overwritten on both arms
      out['siI' + i] = q;      // the nine scalar arguments are literals in re/frida/registry.d/c3bq.py
    });
    return out;
  },

  // ------------------------------------------------------------- png_get_IHDR 0x1006eae0.
  // One png_info and one nine-dword out area per vector (0x80 and 0x40 strides), plus one shared
  // png_struct for the warning. The body reads its own results back out of the out area, so each
  // vector's area is its own region of the one block.
  c3bq_gihdr() {
    const out = {};
    const fill = (p, n, seed) => { for (let i = 0; i < n; i += 4) p.add(i).writeU32(((seed << 16) + i) >>> 0); };

    const code = Memory.alloc(Process.pageSize);
    Memory.protect(code, Process.pageSize, 'rwx');
    code.writeByteArray([0xc3]);
    out.giWarn = code;

    const png = Memory.alloc(0x200);
    fill(png, 0x200, 0x6565);
    png.add(0x44).writePointer(code);
    out.giPng = png;

    // width, height, bit_depth, color_type, compression, filter, interlace
    const V = [
      [16, 8, 8, 0, 0, 0, 0],            // 0  png_ptr null
      [16, 8, 8, 0, 0, 0, 0],            // 1  info_ptr null
      [16, 8, 8, 0, 0, 0, 0],            // 2  width null
      [16, 8, 8, 0, 0, 0, 0],            // 3  height null
      [16, 8, 8, 0, 0, 0, 0],            // 4  bit_depth null
      [16, 8, 8, 0, 0, 0, 0],            // 5  color_type null
      [16, 8, 8, 0, 1, 2, 3],            // 6  the three optional pointers all null
      [16, 8, 8, 0, 1, 2, 3],            // 7  all nine given
      [16, 8, 8, 2, 1, 2, 3],            // 8  interlace null only
      [16, 8, 8, 2, 1, 2, 3],            // 9  compression null only
      [16, 8, 8, 2, 1, 2, 3],            // 10 filter null only
      [16, 8, 8, 3, 0, 0, 0],            // 11 colour type 3
      [16, 8, 8, 2, 0, 0, 0],            // 12 colour type 2
      [16, 8, 8, 4, 0, 0, 0],            // 13 colour type 4
      [16, 8, 8, 6, 0, 0, 0],            // 14 colour type 6
      [100, 50, 16, 6, 0, 0, 0],         // 15 depth 16, four channels
      [0x10000000, 1, 16, 6, 0, 0, 0],   // 16 one past 0x7fffffff / 8: the warning
      [0x0fffffff, 1, 16, 6, 0, 0, 0],   // 17 exactly the quotient: the `jbe` boundary
      [16, 8, 8, 1, 0, 0, 0],            // 18 colour type 1
      [0x7fffffff, 1, 1, 0, 0, 0, 0],    // 19 one byte a pixel at the largest width
    ];
    const iblk = Memory.alloc(V.length * 0x80);
    fill(iblk, V.length * 0x80, 0x6666);
    const oblk = Memory.alloc(V.length * 0x40);
    fill(oblk, V.length * 0x40, 0x6767);
    out.giInfo = iblk;
    out.giOut = oblk;
    V.forEach((v, i) => {
      const q = iblk.add(i * 0x80);
      q.writeU32(v[0]);                                // width
      q.add(4).writeU32(v[1]);                         // height
      q.add(0x18).writeU8(v[2]);                       // bit_depth
      q.add(0x19).writeU8(v[3]);                       // color_type
      q.add(0x1a).writeU8(v[4]);                       // compression_type
      q.add(0x1b).writeU8(v[5]);                       // filter_type
      q.add(0x1c).writeU8(v[6]);                       // interlace_type
      out['giI' + i] = q;
      const o = oblk.add(i * 0x40);
      for (let j = 0; j < 9; j++) o.add(j * 4).writeU32((0x5a5a0000 + i * 0x10 + j) >>> 0);
      for (let j = 0; j < 7; j++) out['giO' + i + '_' + j] = o.add(j * 4);
    });
    return out;
  },

  // ---------------------------------------------------------- png_set_read_fn 0x10078cb0.
  // One scratch png_struct per vector at a 0x200 stride (the highest field written is +0x14c). Half of
  // them carry a non-null write_data_fn at +0x4c, which is the arm that clears it and warns twice.
  c3bq_readfn() {
    const out = {};
    const fill = (p, n, seed) => { for (let i = 0; i < n; i += 4) p.add(i).writeU32(((seed << 16) + i) >>> 0); };

    const code = Memory.alloc(Process.pageSize);
    Memory.protect(code, Process.pageSize, 'rwx');
    code.writeByteArray([0xc3]);                       // the png_warning handler
    code.add(0x10).writeByteArray([0xc3]);             // a read_data_fn of our own: never called here
    out.rfWarn = code;
    out.rfFn = code.add(0x10);

    const io = Memory.alloc(0x100);
    fill(io, 0x100, 0x6868);
    out.rfIo = io;

    const N = 12;
    const blk = Memory.alloc(N * 0x200);
    fill(blk, N * 0x200, 0x6969);
    out.rfBlk = blk;
    for (let i = 0; i < N; i++) {
      const p = blk.add(i * 0x200);
      p.add(0x44).writePointer(code);
      p.add(0x4c).writeU32((i & 1) ? (0x41410000 + i) : 0);     // write_data_fn
      p.add(0x50).writeU32((0x42420000 + i) >>> 0);             // read_data_fn before the call
      p.add(0x54).writeU32((0x43430000 + i) >>> 0);             // io_ptr before the call
      p.add(0x14c).writeU32((0x44440000 + i) >>> 0);            // the field cleared at 0x10078d24
      out['rf' + i] = p;
    }
    return out;
  },

  // ------------------------------ Palette::reset 0x1006a380 and Palette::apply 0x1006a4a0.
  // A Palette uses 0x818 bytes (256 four-byte entries at +0xc, a 16-bit table at +0x40c, a second at
  // +0x60c, the serial at +0x810 and the serial of the last build at +0x814); the records sit at a
  // 0x840 stride so a store past the last field shows in the block hash. The first twelve are for
  // reset, the next fourteen for apply.
  c3bq_palette() {
    const out = {};
    const fill = (p, n, seed) => { for (let i = 0; i < n; i += 4) p.add(i).writeU32(((seed << 16) + i) >>> 0); };

    const NR = 12, NA = 14;
    const blk = Memory.alloc((NR + NA) * 0x840);
    fill(blk, (NR + NA) * 0x840, 0x6a6a);
    out.palBlk = blk;

    for (let i = 0; i < NR; i++) {
      const p = blk.add(i * 0x840);
      p.add(0x810).writeU32((0x30300000 + i * 7) >>> 0);
      p.add(0x814).writeU32((0x31310000 + i * 11) >>> 0);
      out['palR' + i] = p;
    }

    // The entry generators: ramps, the extremes 0x00 / 0x80 / 0xff and values whose low bits the two
    // masks drop, so a wrong shift or a wrong mask changes a table.
    const GEN = [
      (i) => [i, i, i],
      (i) => [255 - i, i, (i * 3) & 0xff],
      (i) => [(i & 0xf8), (i & 0xfc), (i & 0xf8)],
      (i) => [(i * 7) & 0xff, (i * 11) & 0xff, (i * 23) & 0xff],
      (i) => [0, 0, 0],
      (i) => [255, 255, 255],
      (i) => [0x80, 0x80, 0x80],
      (i) => [i < 128 ? 0 : 255, i & 1 ? 0xff : 0, (i * 5) & 0xff],
      (i) => [(i ^ 0x5a) & 0xff, (i ^ 0xa5) & 0xff, (i ^ 0x3c) & 0xff],
      (i) => [i, 255 - i, 0x7f],
      (i) => [(i >> 1) & 0xff, (i << 1) & 0xff, (i * 13) & 0xff],
      (i) => [(i * 17) & 0xff, (i * 19) & 0xff, (i * 29) & 0xff],
      (i) => [i & 7, i & 3, i & 1],
      (i) => [0xf8, 0xfc, 0x07],
    ];
    for (let i = 0; i < NA; i++) {
      const p = blk.add((NR + i) * 0x840);
      for (let j = 0; j < 256; j++) {
        const c = GEN[i](j);
        p.add(0x0c + j * 4).writeU8(c[0] & 0xff);      // the byte read at 0x1006a538 / 0x1006a586
        p.add(0x0d + j * 4).writeU8(c[1] & 0xff);      // 0x1006a521 / 0x1006a56f
        p.add(0x0e + j * 4).writeU8(c[2] & 0xff);      // 0x1006a50f / 0x1006a55d
        p.add(0x0f + j * 4).writeU8((j * 3) & 0xff);   // the fourth byte: never read
      }
      // Vector 0 gets two equal serials, which is the early exit at 0x1006a4cf; the rest differ.
      p.add(0x810).writeU32((0x32320000 + i) >>> 0);
      p.add(0x814).writeU32(i === 0 ? ((0x32320000 + i) >>> 0) : ((0x33330000 + i) >>> 0));
      out['palA' + i] = p;
    }
    return out;
  },

  // ----------------------------------------------------- Matrix::loadIdentity 0x100045f0.
  // Twelve objects at a 0x60 stride; the body writes this+4 .. this+0x40 and never touches this+0, so
  // the dword before the cells is part of the region on purpose.
  c3bq_matrix() {
    const out = {};
    const N = 12;
    const blk = Memory.alloc(N * 0x60);
    for (let i = 0; i < N * 0x60; i += 4) blk.add(i).writeU32(((0x6b6b << 16) + i) >>> 0);
    out.matBlk = blk;
    for (let i = 0; i < N; i++) {
      const p = blk.add(i * 0x60);
      p.writeU32((0x76760000 + i) >>> 0);              // the slot the body leaves alone
      for (let j = 0; j < 16; j++) p.add(4 + j * 4).writeU32((0x40000000 + i * 0x100 + j) >>> 0);
      out['mat' + i] = p;
    }
    return out;
  },
});
