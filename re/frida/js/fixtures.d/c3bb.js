// Batch c3bb fixtures (jgld.dll). Every block is allocated here; nothing live in the running jgld.dll
// (no Display, Surface, Palette, Font, the game's png_struct, inflate state or heap blocks) is handed
// to the A/B. Layouts come from the functions' own field offsets, cited in shim/src/re/c3bb.cpp.
// Blocks are pre-filled with a per-offset word pattern and every object sits at a larger stride than
// the bytes it uses, so a store one word too wide or a field left unwritten changes the block hash.
Object.assign(globalThis.DIFF_FIXTURES, {

  // jgld's own one-field accessors. Array<T> is {vtable +0, buffer +4, capacity +8, count +0xc,
  // growth +0x10}; the four count/reset keys get sixteen objects each at a 0x20 stride, with sixteen
  // DIFFERENT counts at +0xc (none of them 0, so every reset vector changes the block). The sixteen
  // initDepthInfo records keep a pattern at +0 and a non-zero word at +4, which is the only word the
  // body writes.
  c3bb_small() {
    const blk = Memory.alloc(0xa00);
    for (let i = 0; i < 0xa00; i += 4) blk.add(i).writeU32((0x4B4B0000 + i) >>> 0);
    const out = { sblk: blk };
    const COUNTS = [1, 2, 3, 7, 16, 255, 256, 1000, 0x7fffffff, -1, -2, -1000,
                    -0x80000000, 0x12345678, 4, 100];
    const group = (base, name) => {
      for (let i = 0; i < 16; i++) {
        const p = blk.add(base + i * 0x20);
        p.add(0xc).writeS32(COUNTS[i]);
        out[name + i] = p;
      }
    };
    group(0x000, 'c14_');     // Array_Rec14::count
    group(0x200, 'r14_');     // Array_Rec14::reset
    group(0x400, 'c94_');     // Array_Rec94::count
    group(0x600, 'r94_');     // Array_Rec94::reset
    for (let i = 0; i < 16; i++) {
      const p = blk.add(0x800 + i * 0x20);
      p.add(4).writeS32(COUNTS[i]);          // the word initDepthInfo clears
      out['dep_' + i] = p;
    }
    return out;
  },

  // Surface::getField810 reads one dword 0x810 bytes into the object, so each scratch surface needs
  // 0x814 bytes; twelve of them sit at a 0x820 stride inside one block.
  c3bb_surfaces() {
    const blk = Memory.alloc(12 * 0x820);
    for (let i = 0; i < 12 * 0x820; i += 4) blk.add(i).writeU32((0x61610000 + i) >>> 0);
    const out = { fblk: blk };
    const V = [0, 1, 2, 8, 15, 16, 32, 0x100, 0x7fffffff, -1, -0x80000000, 0x5A5A5A5A];
    for (let i = 0; i < 12; i++) {
      const p = blk.add(i * 0x820);
      p.add(0x810).writeS32(V[i]);
      out['sf_' + i] = p;
    }
    return out;
  },

  // Array<Rec14>::add and Array<Rec94>::add on the "there is room" path only: every object is given a
  // buffer with spare capacity (count < cap), so the branch that calls operator new / operator delete
  // on the game's heap is never taken. The growth step at +0x10 differs per object and is not read on
  // this path, which the identical results prove.
  c3bb_arrays() {
    const blk = Memory.alloc(0x4000);
    for (let i = 0; i < 0x4000; i += 4) blk.add(i).writeU32((0x2D2D0000 + i) >>> 0);
    const out = { ablk: blk };
    // Rec14: twelve objects at 0x000, their 8-entry (8 * 0x14 = 160 byte) buffers at 0x200 + i * 0x100
    for (let i = 0; i < 12; i++) {
      const o = blk.add(i * 0x20), buf = blk.add(0x200 + i * 0x100);
      o.add(4).writePointer(buf);
      o.add(8).writeS32(i < 8 ? 8 : 12);        // capacity
      o.add(0xc).writeS32(i < 8 ? i : i - 1);   // count, always below the capacity
      o.add(0x10).writeS32(4 + i);              // growth step (unused on this path)
      out['a14_' + i] = o;
    }
    // Rec94: twelve objects at 0xe00, their 4-entry (4 * 0x94 = 592 byte) buffers at 0x1000 + i * 0x400
    for (let i = 0; i < 12; i++) {
      const o = blk.add(0xe00 + i * 0x20), buf = blk.add(0x1000 + i * 0x400);
      o.add(4).writePointer(buf);
      o.add(8).writeS32(4);
      o.add(0xc).writeS32(i % 4);
      o.add(0x10).writeS32(2 + i);
      out['a94_' + i] = o;
    }
    return out;
  },

  // libpng row transformations. A png_row_info is 0xc bytes: width +0, rowbytes +4, color_type +8,
  // bit_depth +9, channels +0xa, pixel_depth +0xb. Each function gets its own contiguous header array
  // (so $xxx0 is a base a state region can cover) and its own row buffer, sized so every walk stays
  // inside it. The palette, the transparency table and the two dither lookups are read-only inputs and
  // are covered by their own state regions, which is how "it does not write them" is tested.
  c3bb_rows() {
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
    const mkBuf = (name, n, f) => {
      const p = Memory.alloc(n);
      for (let i = 0; i < n; i++) p.add(i).writeU8(f(i) & 0xff);
      out[name] = p;
      return p;
    };

    // ---- png_do_unshift: color_type 3 returns; the colour bit picks three shifts, the alpha bit a
    // fourth; a shift <= 0 is clamped and only a positive one lets the body run; bit_depth - 2 selects
    // the switch (1 wraps past the unsigned range test, 5 reaches the table's default).
    mkHeaders('uns', [
      [8, 8, 3, 8, 1, 8],       // 0  palette: returns at once
      [8, 24, 2, 8, 3, 24],     // 1  sig_bits == bit_depth: every shift 0, nothing runs
      [8, 24, 2, 8, 3, 24],     // 2  case 8, three shifts of 3
      [4, 16, 6, 8, 4, 32],     // 3  colour + alpha, case 8, four different shifts
      [8, 8, 0, 8, 1, 8],       // 4  grey: the single gray shift
      [6, 12, 4, 8, 2, 16],     // 5  grey + alpha: two shifts
      [4, 8, 2, 2, 3, 6],       // 6  case 2 (rowbytes 8)
      [4, 12, 2, 4, 3, 12],     // 7  case 4 (rowbytes 12)
      [4, 24, 2, 16, 3, 48],    // 8  case 16, 12 samples
      [3, 24, 6, 16, 4, 64],    // 9  case 16 with four channels
      [0, 0, 2, 8, 3, 24],      // 10 case 8 with width 0: the loop body never runs
      [4, 0, 2, 2, 3, 6],       // 11 case 2 with rowbytes 0: the loop body never runs
      [4, 8, 2, 5, 3, 15],      // 12 bit_depth 5: in range, no case, nothing happens
      [4, 4, 2, 1, 3, 3],       // 13 bit_depth 1: 1 - 2 wraps, the unsigned range test returns
      [8, 24, 2, 8, 3, 24],     // 14 shifts 0, 8 and 3: one clamped, two positive
      [8, 8, 0, 8, 1, 8],       // 15 grey with sig_bits above bit_depth: a negative shift, clamped
    ]);
    // png_color_8 {red, green, blue, gray, alpha} at a 8-byte stride, one per header above
    const sig = Memory.alloc(16 * 8);
    [[8, 8, 8, 8, 8], [8, 8, 8, 0, 0], [5, 5, 5, 0, 0], [5, 6, 7, 0, 4], [0, 0, 0, 4, 0], [0, 0, 0, 4, 2],
     [1, 1, 1, 0, 0], [2, 2, 2, 0, 0], [10, 10, 10, 0, 0], [12, 12, 12, 0, 8], [5, 5, 5, 0, 0],
     [1, 1, 1, 0, 0], [2, 2, 2, 0, 0], [0, 0, 0, 0, 0], [8, 0, 5, 0, 0], [0, 0, 0, 9, 0]
    ].forEach((s, i) => {
      const p = sig.add(i * 8);
      s.forEach((v, j) => p.add(j).writeU8(v));
      out['sig' + i] = p;
    });
    mkBuf('unsrow', 64, (i) => i * 7 + 0x13);

    // ---- png_do_expand_palette: only color_type 3 does anything; a bit_depth below 8 is unpacked
    // first (1, 2 and 4 have a body, 3 only runs the tail) and the result goes through the 8-bit stage,
    // which writes four bytes per pixel when trans is not null and three when it is.
    mkHeaders('exp', [
      [8, 8, 0, 8, 1, 8],       // 0  not a palette row: returns
      [8, 8, 3, 8, 1, 8],       // 1  trans null: RGB
      [8, 8, 3, 8, 1, 8],       // 2  trans with num_trans 4: both sides of the index test
      [1, 1, 3, 8, 1, 8],       // 3  num_trans 0: every index takes the 0xff side
      [4, 4, 3, 8, 1, 8],       // 4  num_trans 256: every index takes the trans[] side
      [8, 1, 3, 1, 1, 1],       // 5  depth 1 unpack, then RGB
      [7, 2, 3, 2, 1, 2],       // 6  depth 2 unpack, then RGBA
      [5, 3, 3, 4, 1, 4],       // 7  depth 4 unpack, then RGB
      [4, 2, 3, 3, 1, 3],       // 8  depth 3: below 8 with no case, tail only
      [0, 0, 3, 1, 1, 1],       // 9  depth 1 with width 0: both loops empty
      [0, 0, 3, 8, 1, 8],       // 10 depth 8 with width 0: the RGB loop never runs
      [4, 8, 3, 16, 1, 16],     // 11 depth 16: neither stage has a case, nothing changes
      [2, 2, 3, 8, 1, 8],       // 12 two pixels, RGB
      [8, 4, 3, 4, 1, 4],       // 13 depth 4 unpack, then RGBA with num_trans 2
      [4, 1, 3, 2, 1, 2],       // 14 depth 2 unpack, then RGB
      [3, 1, 3, 1, 1, 1],       // 15 depth 1 unpack, then RGBA
    ]);
    // the first eight bytes are small and large indices on purpose, so num_trans 4 splits them
    const EXP0 = [3, 0, 200, 1, 255, 2, 130, 5];
    mkBuf('exprow', 128, (i) => (i < 8 ? EXP0[i] : i * 7 + 0x13));
    mkBuf('pal', 256 * 3, (i) => i * 11 + 0x27);        // png_color {red, green, blue}, 3 bytes
    mkBuf('trans', 256, (i) => i * 13 + 0x3d);

    // ---- png_do_dither: three bodies, each needing its color_type, a non-null lookup and bit_depth 8.
    mkHeaders('dit', [
      [8, 24, 2, 8, 3, 24],     // 0  RGB -> index
      [0, 0, 2, 8, 3, 24],      // 1  RGB with width 0: the loop body never runs, the header tail does
      [1, 3, 2, 8, 3, 24],      // 2  one pixel
      [8, 24, 2, 8, 3, 24],     // 3  run with palette_lookup null: no body at all
      [8, 48, 2, 16, 3, 48],    // 4  bit_depth 16: no body
      [8, 32, 6, 8, 4, 32],     // 5  RGBA -> index
      [2, 8, 6, 8, 4, 32],      // 6  two pixels
      [0, 0, 6, 8, 4, 32],      // 7  RGBA with width 0
      [8, 32, 6, 8, 4, 32],     // 8  run with palette_lookup null: no body
      [8, 8, 3, 8, 1, 8],       // 9  palette -> dither_lookup in place
      [0, 0, 3, 8, 1, 8],       // 10 palette with width 0
      [8, 8, 3, 8, 1, 8],       // 11 run with dither_lookup null: no body
      [8, 48, 6, 16, 4, 64],    // 12 RGBA with bit_depth 16: the third test of the second guard
      [8, 4, 3, 4, 1, 4],       // 13 palette with bit_depth 4: no body
      [16, 48, 2, 8, 3, 24],    // 14 RGB, 16 pixels
      [16, 64, 6, 8, 4, 32],    // 15 RGBA, 16 pixels
    ]);
    mkBuf('ditrow', 128, (i) => i * 23 + 0x5b);
    mkBuf('plut', 0x8000, (i) => i * 17 + (i >> 8) * 3 + 0x29);   // indexed by a 15-bit 5-5-5 colour
    mkBuf('dlut', 256, (i) => i * 19 + 0x71);
    return out;
  },

  // libpng's error formatter and row-filter reader.
  c3bb_png() {
    const blk = Memory.alloc(0x2000);
    for (let i = 0; i < 0x2000; i += 4) blk.add(i).writeU32((0x39390000 + i) >>> 0);
    const out = { pblk: blk };
    // twelve scratch png_struct records at a 0x140 stride; only the four bytes at +0x11c are read
    const NAMES = [
      [0x49, 0x48, 0x44, 0x52],   // 0  four plain characters
      [0x00, 0x01, 0x02, 0x03],   // 1  all below 0x29: escaped
      [0x7b, 0x7c, 0x7d, 0x7e],   // 2  all above 0x7a: escaped
      [0x5b, 0x5c, 0x5d, 0x5e],   // 3  all between 0x5a and 0x61: escaped
      [0x5a, 0x61, 0x5a, 0x61],   // 4  the two closed boundaries: not escaped
      [0x29, 0x7a, 0x29, 0x7a],   // 5  the other two boundaries: not escaped
      [0x28, 0x7b, 0x60, 0x41],   // 6  one of each escape reason plus one plain byte
      [0xff, 0xfe, 0x80, 0x90],   // 7  bytes with the top bit set: escaped (the load zero-extends)
      [0x70, 0x48, 0x59, 0x73],   // 8  plain
      [0x74, 0x45, 0x58, 0x74],   // 9  plain
      [0x49, 0x00, 0x44, 0x5d],   // 10 mixed
      [0x30, 0x31, 0x32, 0x33],   // 11 plain digits
    ];
    NAMES.forEach((n, i) => {
      const p = blk.add(i * 0x140);
      n.forEach((b, j) => p.add(0x11c + j).writeU8(b));
      out['ps' + i] = p;
    });
    // twenty output buffers at a 0x80 stride: the formatter writes at most 4 * 4 + 2 + 64 = 82 bytes
    for (let i = 0; i < 20; i++) out['ob' + i] = blk.add(0x1000 + i * 0x80);
    // four message buffers; 64 bytes are always copied, whatever the text's length
    for (let i = 0; i < 4; i++) {
      const p = blk.add(0x1a00 + i * 0x80);
      for (let j = 0; j < 0x80; j++) p.add(j).writeU8((j * (7 + i * 10) + 0x21 + i) & 0xff);
      out['msg' + i] = p;
    }
    // png_read_filter_row's png_struct: only the warning handler at +0x44 is read, and only by the
    // default branch. It points at a one-byte `ret` (0xc3) of our own, so the branch runs without
    // reaching png_default_warning, which writes to a C stream.
    const code = Memory.alloc(Process.pageSize);
    Memory.protect(code, Process.pageSize, 'rwx');
    code.writeByteArray([0xc3]);                       // __cdecl no-op
    const fpng = blk.add(0x1c00);
    fpng.add(0x44).writePointer(code);
    out.fpng = fpng;
    // eight png_row_info headers; only rowbytes (+4) and pixel_depth (+0xb) are read
    const fh = Memory.alloc(8 * 0xc);
    [[8, 16, 2, 8, 1, 8], [8, 24, 2, 8, 3, 24], [8, 32, 6, 8, 4, 32], [8, 4, 0, 1, 1, 1],
     [4, 24, 2, 16, 3, 48], [8, 1, 2, 8, 1, 8], [8, 0, 2, 8, 1, 8], [8, 32, 4, 8, 2, 16]
    ].forEach((r, i) => {
      const p = fh.add(i * 0xc);
      p.writeU32(r[0]); p.add(4).writeU32(r[1]); p.add(8).writeU8(r[2]);
      p.add(9).writeU8(r[3]); p.add(0xa).writeU8(r[4]); p.add(0xb).writeU8(r[5]);
      out['fh' + i] = p;
    });
    // one 0x100 buffer holding two rows (written) and two previous rows (read only), 0x40 apart, which
    // is above the largest rowbytes above (32)
    const fbuf = Memory.alloc(0x100);
    for (let i = 0; i < 0x40; i++) {
      fbuf.add(i).writeU8((i * 29 + 0x41) & 0xff);
      fbuf.add(0x40 + i).writeU8((i * 83 + 0x0d) & 0xff);
      fbuf.add(0x80 + i).writeU8((i * 113 + 0x07) & 0xff);
      fbuf.add(0xc0 + i).writeU8((i * 37 + 0xb1) & 0xff);
    }
    out.fbuf = fbuf;
    out.frow = fbuf;
    out.frow2 = fbuf.add(0x40);
    out.fprev = fbuf.add(0x80);
    out.fprev2 = fbuf.add(0xc0);
    return out;
  },

  // zlib's inflate_flush: ten inflate_blocks_state records with their own 64-byte sliding windows, six
  // z_stream records with their own output buffers, and two check functions of our own (zlib's own
  // adler32 is hooked by another batch, and a stub keeps this key free of hooked callees).
  //   state:  window +0x24, end +0x28, read +0x2c, write +0x30, checkfn +0x34, check +0x38
  //   stream: next_out +0xc, avail_out +0x10, total_out +0x14, adler +0x30
  c3bb_zlib() {
    const blk = Memory.alloc(0x1000);
    for (let i = 0; i < 0x1000; i += 4) blk.add(i).writeU32((0x7A7A0000 + i) >>> 0);
    const out = { zblk: blk };
    const code = Memory.alloc(Process.pageSize);
    Memory.protect(code, Process.pageSize, 'rwx');
    // check + n: mov eax,[esp+4] / add eax,[esp+0ch] / ret
    code.writeByteArray([0x8b, 0x44, 0x24, 0x04, 0x03, 0x44, 0x24, 0x0c, 0xc3]);
    // a constant: mov eax,0a5a5a5a5h / ret
    code.add(0x10).writeByteArray([0xb8, 0xa5, 0xa5, 0xa5, 0xa5, 0xc3]);
    const sum = code, konst = code.add(0x10);

    // read offset, write offset, check function, initial check
    const ST = [
      [0, 32, null, 0x1111],      // s0 read <= write, 32 bytes to the write pointer
      [48, 16, null, 0x2222],     // s1 read > write, 16 bytes to the end, then a wrap
      [32, 64, null, 0x3333],     // s2 write at the end: the wrap also moves the write pointer
      [64, 64, null, 0x4444],     // s3 read already at the end, nothing to copy
      [0, 0, null, 0x5555],       // s4 read == write at the start: n is 0 and there is no wrap
      [0, 32, sum, 0x6666],       // s5 the check function over the first block
      [48, 16, sum, 0x7777],      // s6 the check function over both blocks
      [8, 40, konst, 0x8888],     // s7 a check function that ignores its arguments
      [64, 24, null, 0x9999],     // s8 first block empty, second block not: the second -5 correction
      [64, 24, sum, 0xaaaa],      // s9 the same with a check function
    ];
    ST.forEach((s, i) => {
      const st = blk.add(i * 0x40), win = blk.add(0x300 + i * 0x80);
      for (let j = 0; j < 64; j++) win.add(j).writeU8((j * 31 + i * 7 + 0x3b) & 0xff);
      st.add(0x24).writePointer(win);
      st.add(0x28).writePointer(win.add(64));
      st.add(0x2c).writePointer(win.add(s[0]));
      st.add(0x30).writePointer(win.add(s[1]));
      st.add(0x34).writePointer(s[2] === null ? ptr(0) : s[2]);
      st.add(0x38).writeU32(s[3]);
      out['s' + i] = st;
    });
    // avail_out, total_out, adler
    const ZS = [[1000, 0, 0x12345678], [8, 100, 0], [0, 7, 0xffffffff],
                [24, 0xfffffff0, 1], [1000, 5, 0xabcdef01], [16, 1, 2]];
    ZS.forEach((z, i) => {
      const r = blk.add(0x800 + i * 0x40), buf = blk.add(0xa00 + i * 0x100);
      r.add(0xc).writePointer(buf);
      r.add(0x10).writeU32(z[0]);
      r.add(0x14).writeU32(z[1]);
      r.add(0x30).writeU32(z[2]);
      out['z' + i] = r;
    });
    return out;
  },
});
