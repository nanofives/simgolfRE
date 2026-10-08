// Batch c3bl fixtures (jgld.dll): png_do_background 0x10074750 and png_do_read_transformations
// 0x100717d0. Every block is allocated here; nothing live in the running jgld.dll (no Display,
// Surface, Palette, Font, the game's png_struct / png_info or its heap blocks) is handed to the A/B.
// Layouts come from the two bodies' own field offsets, cited in shim/src/re/c3bl.cpp. Blocks are
// pre-filled with a per-offset word pattern and every object sits at a larger stride than the bytes
// it uses, so a store one word too wide or a field left unwritten changes the block hash.
Object.assign(globalThis.DIFF_FIXTURES, {

  // ------------------------------------------------------------------ png_do_background 0x10074750.
  // Twelve arguments. A png_row_info is 0xc bytes (width +0, rowbytes +4, color_type +8, bit_depth
  // +9, channels +0xa, pixel_depth +0xb) and a png_color_16 is ten bytes (red +2, green +4, blue +6,
  // gray +8); the records here sit at a 0x10 stride so a store past the tenth byte shows up.
  // Six row areas at a 0x40 stride inside one 0x400 block give each body a source laid out for it;
  // every body writes at or below its source pointer, so all of it stays in the block, and the whole
  // block is one state region.
  c3bl_bg() {
    const out = {};
    const fill = (p, n, seed) => { for (let i = 0; i < n; i += 4) p.add(i).writeU32(((seed << 16) + i) >>> 0); };
    const bytes = (p, a) => a.forEach((b, i) => p.add(i).writeU8(b & 0xff));

    // ---- the row block.
    const rowblk = Memory.alloc(0x400);
    fill(rowblk, 0x400, 0x4242);
    out.bgrowblk = rowblk;
    const rgray = rowblk.add(0x80);
    const rga8 = rowblk.add(0xc0);
    const rga16 = rowblk.add(0x100);
    const rrgba8 = rowblk.add(0x140);
    const rrgba16 = rowblk.add(0x180);
    const rrgb16 = rowblk.add(0x1c0);
    const rrgb8 = rowblk.add(0x200);
    // grey / RGB-8 source: byte 0 is 0x00 and byte 1 is 0xff, so at depths 1, 2 and 4 the first byte
    // matches a trans gray of 0 in every field and the second matches in none.
    bytes(rgray, [0x00, 0xff, 0x55, 0xaa, 0x12, 0x34, 0x56, 0x78,
                  0x80, 0x80, 0x80, 0xff, 0x11, 0x22, 0x33, 0x00,
                  0x01, 0x02, 0x03, 0x04, 0xf0, 0x0f, 0xc3, 0x3c,
                  0xab, 0xcd, 0xef, 0x10, 0x20, 0x30, 0x40, 0x50]);
    // grey + alpha, eight bits: twelve (gray, alpha) pairs with alpha 0xff, 0 and six other values.
    bytes(rga8, [0x10, 0xff, 0x20, 0x00, 0x30, 0x80, 0x40, 0x01,
                 0x50, 0xfe, 0x60, 0x7f, 0x70, 0xff, 0x80, 0x00,
                 0x90, 0x40, 0xa0, 0xc0, 0xb0, 0x20, 0xc0, 0xe0]);
    // grey + alpha, sixteen bits: eight (gray, alpha) big-endian pairs, alpha 0xffff, 0 and six more.
    bytes(rga16, [0x12, 0x34, 0xff, 0xff, 0x56, 0x78, 0x00, 0x00,
                  0x9a, 0xbc, 0x80, 0x00, 0xde, 0xf0, 0x00, 0x01,
                  0x11, 0x22, 0x7f, 0xff, 0x33, 0x44, 0xff, 0xfe,
                  0x55, 0x66, 0x40, 0x00, 0x77, 0x88, 0xc0, 0xde]);
    // RGBA eight bits: eight quads, alpha 0xff, 0 and six more.
    bytes(rrgba8, [0x10, 0x20, 0x30, 0xff, 0x40, 0x50, 0x60, 0x00,
                   0x70, 0x80, 0x90, 0x80, 0xa0, 0xb0, 0xc0, 0x01,
                   0xd0, 0xe0, 0xf0, 0xfe, 0x01, 0x02, 0x03, 0x7f,
                   0x04, 0x05, 0x06, 0xff, 0x07, 0x08, 0x09, 0x00]);
    // RGBA sixteen bits: four octets, alpha 0xffff, 0 and two more.
    bytes(rrgba16, [0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xff, 0xff,
                    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x00, 0x00,
                    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x80, 0x00,
                    0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x12, 0x34]);
    // RGB sixteen bits, five triples against tv_rgb16 (0x00ff, 0x55aa, 0x1234): the first and the
    // fifth match all three, the second matches only red, the third red and green, the fourth none.
    // Without the second and the third the green and blue comparisons are only ever reached by a
    // pixel that matches them, and their jne side is never taken.
    bytes(rrgb16, [0x00, 0xff, 0x55, 0xaa, 0x12, 0x34,
                   0x00, 0xff, 0x11, 0x22, 0x33, 0x44,
                   0x00, 0xff, 0x55, 0xaa, 0x99, 0x99,
                   0x56, 0x78, 0x80, 0x80, 0x80, 0xff,
                   0x00, 0xff, 0x55, 0xaa, 0x12, 0x34]);
    // RGB eight bits, four pixels against tv_rgb8 (0x00, 0xff, 0x55), the same four cases.
    bytes(rrgb8, [0x00, 0xff, 0x55,
                  0x00, 0x12, 0x34,
                  0x00, 0xff, 0x34,
                  0x56, 0x78, 0x90]);
    out.bgrgray = rgray; out.bgrga8 = rga8; out.bgrga16 = rga16;
    out.bgrrgba8 = rrgba8; out.bgrrgba16 = rrgba16; out.bgrrgb16 = rrgb16; out.bgrrgb8 = rrgb8;

    // ---- the png_color_16 records: four trans_values, two backgrounds and two background_1.
    const COL = [
      ['tvg0', 0x0000, 0x0000, 0x0000, 0x0000],   // grey 0: matches every zero field at depths 1/2/4
      ['tvg55', 0x0000, 0x0000, 0x0000, 0x0055],  // grey 0x55: matches rgray[2] at depth 8
      ['tvg55aa', 0x0000, 0x0000, 0x0000, 0x55aa],// grey 0x55aa: matches rgray[2..3] at depth 16
      ['tvrgb8', 0x0000, 0x00ff, 0x0055, 0x0000], // red 0, green 0xff, blue 0x55: rgray pixel 0
      ['tvrgb16', 0x00ff, 0x55aa, 0x1234, 0x0000],// the first and fourth triple of rrgb16
      ['bk1', 0x0011, 0x0022, 0x0033, 0x0001],    // background for the eight-bit bodies
      ['bk2', 0x1234, 0x5678, 0x9abc, 0xabcd],    // background for the sixteen-bit bodies
      ['bk1a', 0x0044, 0x0055, 0x0066, 0x0007],   // background_1 for the eight-bit bodies
      ['bk2a', 0x8765, 0x4321, 0x0fed, 0x4321],   // background_1 for the sixteen-bit bodies
      ['tvg1', 0x0000, 0x0000, 0x0000, 0x0001],   // grey 1: the other polarity at depth 1
      ['bk0', 0x0000, 0x0000, 0x0000, 0x0000],    // a background of zeros
    ];
    const colblk = Memory.alloc(COL.length * 0x10);
    fill(colblk, COL.length * 0x10, 0x4343);
    COL.forEach((c, i) => {
      const q = colblk.add(i * 0x10);
      q.add(2).writeU16(c[1]); q.add(4).writeU16(c[2]);
      q.add(6).writeU16(c[3]); q.add(8).writeU16(c[4]);
      out['bg_' + c[0]] = q;
    });
    out.bgcolblk = colblk;

    // ---- the gamma tables. gamma_table, gamma_from_1 and gamma_to_1 are 256 bytes each; each
    // sixteen-bit table is 256 pointers into eight sub-tables of 256 unsigned shorts.
    const mk8 = (name, f) => {
      const t = Memory.alloc(256);
      for (let i = 0; i < 256; i++) t.add(i).writeU8(f(i) & 0xff);
      out[name] = t;
      return t;
    };
    mk8('bggt', i => (255 - i) ^ 0x3c);
    mk8('bggf1', i => (i * 7 + 0x11));
    mk8('bggt1', i => (i * 13 + 0x55));
    const mk16 = (name, f) => {
      const sub = Memory.alloc(8 * 256 * 2);
      for (let i = 0; i < 8 * 256; i++) sub.add(i * 2).writeU16(f(i) & 0xffff);
      const ptrs = Memory.alloc(256 * 4);
      for (let i = 0; i < 256; i++) ptrs.add(i * 4).writePointer(sub.add((i & 7) * 512));
      out[name + 'sub'] = sub;
      out[name] = ptrs;
      return ptrs;
    };
    mk16('bgg16', i => i * 2731 + 0x1234);
    mk16('bgg16f', i => i * 7919 + 0x4321);
    mk16('bgg16t', i => i * 1237 + 0x0507);

    // ---- the headers, one per vector that needs its own.
    //   width, rowbytes, color_type, bit_depth, channels, pixel_depth
    const H = [
      [16, 2, 0, 1, 1, 1],      // 0  grey depth 1, two bytes: 0x00 then 0xff
      [8, 2, 0, 2, 1, 2],       // 1  grey depth 2
      [4, 2, 0, 4, 1, 4],       // 2  grey depth 4
      [8, 8, 0, 8, 1, 8],       // 3  grey depth 8
      [4, 8, 0, 16, 1, 16],     // 4  grey depth 16
      [4, 2, 0, 3, 1, 3],       // 5  grey depth 3: the byte table sends it to the break
      [4, 12, 0, 24, 1, 24],    // 6  grey depth 24: the range test returns
      [4, 12, 2, 8, 3, 24],     // 7  RGB 8
      [5, 30, 2, 16, 3, 48],    // 8  RGB 16
      [6, 12, 4, 8, 2, 16],     // 9  grey + alpha 8
      [8, 32, 4, 16, 2, 32],    // 10 grey + alpha 16
      [8, 32, 6, 8, 4, 32],     // 11 RGBA 8
      [4, 32, 6, 16, 4, 64],    // 12 RGBA 16
      [4, 4, 3, 8, 1, 8],       // 13 colour type 3: the switch sends it to the tail, no alpha bit
      [4, 4, 1, 8, 1, 8],       // 14 colour type 1: the same
      [4, 4, 5, 8, 1, 8],       // 15 colour type 5: the switch exits but the tail runs
      [4, 4, 7, 8, 1, 8],       // 16 colour type 7: the range test returns and the tail runs
      [0, 0, 0, 8, 1, 8],       // 17 width 0: no loop body runs
      [4, 16, 6, 8, 4, 32],     // 18 RGBA 8 for the trans_values-null guard
      [16, 2, 0, 1, 1, 1],      // 19 grey depth 1 with the other trans polarity
    ];
    const hblk = Memory.alloc(H.length * 0xc);
    fill(hblk, H.length * 0xc, 0x4444);
    H.forEach((r, i) => {
      const q = hblk.add(i * 0xc);
      q.writeU32(r[0]); q.add(4).writeU32(r[1]); q.add(8).writeU8(r[2]);
      q.add(9).writeU8(r[3]); q.add(0xa).writeU8(r[4]); q.add(0xb).writeU8(r[5]);
      out['bgh' + i] = q;
    });
    out.bghblk = hblk;
    return out;
  },

  // -------------------------------------------------- png_do_read_transformations 0x100717d0.
  // One scratch png_struct per vector at a 0x280 stride (the highest field the body touches is
  // rgb_to_gray_status at +0x214), each with its own 0x200-byte row area whose row_buf sits 0x80
  // bytes in, so a callee that writes below or above the row still writes inside the block. The
  // warning handler at +0x44 is a `ret` of our own, which is what lets the rgb_to_gray warning arm
  // run without any I/O; the user transform function is a three-byte stub that stores one byte into
  // the row it is given.
  c3bl_rt() {
    const out = {};
    const fill = (p, n, seed) => { for (let i = 0; i < n; i += 4) p.add(i).writeU32(((seed << 16) + i) >>> 0); };

    // Two stubs on one rwx page: +0 is `ret` (the png_warning handler, __cdecl, two arguments);
    // +0x10 is `mov eax,[esp+0xc]; mov byte [eax],0xa5; ret` (the user transform, three arguments,
    // writing into the row so the call is visible in the state).
    const code = Memory.alloc(Process.pageSize);
    Memory.protect(code, Process.pageSize, 'rwx');
    code.writeByteArray([0xc3]);
    code.add(0x10).writeByteArray([0x8b, 0x44, 0x24, 0x0c, 0xc6, 0x00, 0xa5, 0xc3]);
    out.rtwarn = code;
    out.rtuser = code.add(0x10);

    // Read-only inputs shared by every record.
    const pal = Memory.alloc(768);                 // 256 png_color {red, green, blue}
    for (let i = 0; i < 256; i++) {
      pal.add(i * 3).writeU8((i * 5 + 1) & 0xff);
      pal.add(i * 3 + 1).writeU8((i * 11 + 2) & 0xff);
      pal.add(i * 3 + 2).writeU8((i * 23 + 3) & 0xff);
    }
    const trans = Memory.alloc(256);
    for (let i = 0; i < 256; i++) trans.add(i).writeU8((i * 17 + 0x40) & 0xff);
    const gt = Memory.alloc(256);
    for (let i = 0; i < 256; i++) gt.add(i).writeU8(((255 - i) ^ 0x3c) & 0xff);
    const dither = Memory.alloc(256);
    for (let i = 0; i < 256; i++) dither.add(i).writeU8((i * 31 + 7) & 0xff);
    // png_do_dither indexes palette_lookup with a fifteen-bit 5/5/5 value (pngconf.h sets
    // PNG_DITHER_RED_BITS, _GREEN_BITS and _BLUE_BITS to 5), so the table is 0x8000 bytes.
    const lookup = Memory.alloc(0x8000);
    for (let i = 0; i < 0x8000; i++) lookup.add(i).writeU8((i * 37 + 0x19) & 0xff);
    const g16sub = Memory.alloc(8 * 256 * 2);
    for (let i = 0; i < 8 * 256; i++) g16sub.add(i * 2).writeU16((i * 2731 + 0x1234) & 0xffff);
    const g16 = Memory.alloc(256 * 4);
    for (let i = 0; i < 256; i++) g16.add(i * 4).writePointer(g16sub.add((i & 7) * 512));
    out.rtpal = pal; out.rttrans = trans; out.rtgt = gt; out.rtdither = dither;
    out.rtlookup = lookup; out.rtg16 = g16; out.rtg16sub = g16sub;

    const GREY = [0x40, 0x40, 0x40, 0x80, 0x80, 0x80, 0x05, 0x05, 0x05, 0x20, 0x20, 0x20];
    const MIX = [0x10, 0x20, 0x30, 0x40, 0x40, 0x40, 0x70, 0x80, 0x90, 0xa0, 0xb0, 0xc0];
    const DEF = [0x00, 0xff, 0x55, 0xaa, 0x12, 0x34, 0x56, 0x78,
                 0x80, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
                 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08];
    const ALPHA = [0x10, 0x20, 0x30, 0xff, 0x40, 0x50, 0x60, 0x00,
                   0x70, 0x80, 0x90, 0x80, 0xa0, 0xb0, 0xc0, 0x01];
    const IDX = [0x00, 0x01, 0x02, 0x03, 0x04, 0x05];

    // Each record: the transformations word, the png_struct's own color_type (+0x126), num_trans
    // (+0x11a), the row_info six-tuple, the row bytes, and the optional pointers. `g8` sets the three
    // eight-bit gamma tables, `g16` the sixteen-bit one; `look` sets palette_lookup and dither_index.
    const V = [
      // T,          ct,  ntr, [w, rowbytes, ct, depth, ch, pdepth], row,   opts
      [0x000000, 0, 0, [8, 8, 0, 8, 1, 8], DEF, {}],                       // 0  nothing runs
      [0x001000, 3, 4, [6, 6, 3, 8, 1, 8], IDX, { pal: 1, tr: 1 }],        // 1  EXPAND -> expand_palette
      [0x001000, 0, 2, [6, 6, 0, 8, 1, 8], DEF, {}],                       // 2  EXPAND -> expand(&trans_values)
      [0x001000, 0, 0, [6, 6, 0, 8, 1, 8], DEF, {}],                       // 3  EXPAND -> expand(NULL)
      [0x040000, 6, 0, [4, 16, 6, 8, 4, 32], ALPHA, {}],                   // 4  STRIP_ALPHA
      [0x600000, 2, 0, [4, 12, 2, 8, 3, 24], GREY, {}],                    // 5  RGB_TO_GRAY, all grey
      [0x400000, 2, 0, [4, 12, 2, 8, 3, 24], MIX, {}],                     // 6  RGB_TO_GRAY_WARN
      [0x600000, 2, 0, [4, 12, 2, 8, 3, 24], MIX, {}],                     // 7  nongray, neither warn nor error
      [0x004000, 0, 0, [4, 4, 0, 8, 1, 8], DEF, {}],                       // 8  GRAY_TO_RGB, first call
      [0x004000, 0, 0, [4, 4, 0, 8, 1, 8], DEF, { flags: 0x20000 }],       // 9  GRAY_TO_RGB, second call
      [0x000080, 0, 2, [6, 6, 0, 8, 1, 8], DEF, { g8: 1 }],                // 10 BACKGROUND via num_trans
      [0x000080, 6, 0, [4, 16, 6, 8, 4, 32], ALPHA, { g8: 1 }],            // 11 BACKGROUND via color_type & 4
      [0x000080, 2, 0, [4, 12, 2, 8, 3, 24], MIX, { g8: 1 }],              // 12 BACKGROUND skipped
      [0x002000, 2, 0, [4, 12, 2, 8, 3, 24], MIX, { g8: 1 }],              // 13 GAMMA
      [0x002000, 3, 0, [4, 4, 3, 8, 1, 8], IDX, { g8: 1 }],                // 14 GAMMA skipped, color_type 3
      [0x002080, 0, 2, [6, 6, 0, 8, 1, 8], DEF, { g8: 1 }],                // 15 GAMMA skipped by num_trans
      [0x002080, 6, 0, [4, 16, 6, 8, 4, 32], ALPHA, { g8: 1 }],            // 16 GAMMA skipped by color_type & 4
      [0x002080, 2, 0, [4, 12, 2, 8, 3, 24], MIX, { g8: 1 }],              // 17 BACKGROUND skipped, GAMMA runs
      [0x000400, 2, 0, [4, 24, 2, 16, 3, 48], DEF, {}],                    // 18 16_TO_8
      [0x000040, 2, 0, [6, 18, 2, 8, 3, 24], DEF, { look: 1 }],            // 19 DITHER
      [0x000020, 0, 0, [16, 2, 0, 1, 1, 1], DEF, {}],                      // 20 INVERT_MONO
      [0x000008, 2, 0, [4, 12, 2, 8, 3, 24], MIX, { sig: [5, 5, 5, 5, 5] }],// 21 SHIFT
      [0x000004, 0, 0, [8, 2, 0, 2, 1, 2], DEF, {}],                       // 22 PACK
      [0x000001, 2, 0, [4, 12, 2, 8, 3, 24], MIX, {}],                     // 23 BGR
      [0x010000, 0, 0, [8, 2, 0, 2, 1, 2], DEF, {}],                       // 24 PACKSWAP
      [0x008000, 0, 0, [4, 4, 0, 8, 1, 8], DEF, { filler: 0x1234, flags: 0x80 }],  // 25 FILLER
      [0x080000, 6, 0, [4, 16, 6, 8, 4, 32], ALPHA, {}],                   // 26 INVERT_ALPHA
      [0x020000, 6, 0, [4, 16, 6, 8, 4, 32], ALPHA, {}],                   // 27 SWAP_ALPHA
      [0x000010, 0, 0, [4, 8, 0, 16, 1, 16], DEF, {}],                     // 28 SWAP_BYTES
      [0x100000, 0, 0, [4, 4, 0, 8, 1, 8], DEF, {}],                       // 29 USER_TRANSFORM, no fn, no depth
      [0x100000, 0, 0, [4, 4, 0, 8, 1, 8], DEF, { user: 1, ud: 4, uc: 2 }],// 30 fn, depth and channels
      [0x100000, 0, 0, [4, 4, 0, 8, 1, 8], DEF, { user: 1, ud: 8, uc: 0 }],// 31 fn and depth, no channels
      [0x100000, 0, 0, [4, 4, 0, 8, 1, 8], DEF, { uc: 3 }],                // 32 channels only
      [0x000035, 0, 0, [8, 2, 0, 2, 1, 2], DEF, {}],                       // 33 INVERT_MONO|PACK|BGR|SWAP_BYTES
      [0x049000, 0, 2, [4, 4, 0, 8, 1, 8], DEF, { filler: 0x55aa, flags: 0x80, g8: 1 }],  // 34 EXPAND|STRIP_ALPHA|FILLER
    ];

    const blk = Memory.alloc(V.length * 0x280);
    fill(blk, V.length * 0x280, 0x5151);
    const rows = Memory.alloc(V.length * 0x200);
    fill(rows, V.length * 0x200, 0x5252);
    out.rtblk = blk;
    out.rtrowblk = rows;

    V.forEach((v, i) => {
      const p = blk.add(i * 0x280);
      const o = v[5];
      const rowbuf = rows.add(i * 0x200 + 0x80);
      v[4].forEach((b, j) => rowbuf.add(1 + j).writeU8(b & 0xff));

      p.add(0x44).writePointer(code);                                 // the png_warning handler
      p.add(0x58).writePointer(o.user ? code.add(0x10) : ptr(0));     // read_user_transform_fn
      p.add(0x64).writeU8(o.ud || 0);                                 // user_transform_depth
      p.add(0x65).writeU8(o.uc || 0);                                 // user_transform_channels
      p.add(0x6c).writeU32(o.flags || 0);                             // flags
      p.add(0x70).writeU32(v[0] >>> 0);                               // transformations
      p.add(0xe4).writeU32(100 + i);                                  // row_number
      p.add(0xec).writePointer(rowbuf);                               // row_buf
      p.add(0x100).writeU32(v[3][0]);                                 // row_info.width
      p.add(0x104).writeU32(v[3][1]);                                 // row_info.rowbytes
      p.add(0x108).writeU8(v[3][2]);                                  // row_info.color_type
      p.add(0x109).writeU8(v[3][3]);                                  // row_info.bit_depth
      p.add(0x10a).writeU8(v[3][4]);                                  // row_info.channels
      p.add(0x10b).writeU8(v[3][5]);                                  // row_info.pixel_depth
      p.add(0x114).writePointer(o.pal ? pal : ptr(0));                // palette
      p.add(0x11a).writeU16(v[2]);                                    // num_trans
      p.add(0x124).writeU8(i & 7);                                    // pass
      p.add(0x126).writeU8(v[1]);                                     // the png_struct's color_type
      p.add(0x12e).writeU16(o.filler || 0);                           // filler
      // background +0x138 and background_1 +0x142, ten bytes each (red +2, green +4, blue +6,
      // gray +8); trans_values +0x18c the same.
      p.add(0x138 + 2).writeU16(0x0011); p.add(0x138 + 4).writeU16(0x0022);
      p.add(0x138 + 6).writeU16(0x0033); p.add(0x138 + 8).writeU16(0x0001);
      p.add(0x142 + 2).writeU16(0x0044); p.add(0x142 + 4).writeU16(0x0055);
      p.add(0x142 + 6).writeU16(0x0066); p.add(0x142 + 8).writeU16(0x0007);
      p.add(0x18c + 2).writeU16(0x0010); p.add(0x18c + 4).writeU16(0x0020);
      p.add(0x18c + 6).writeU16(0x0030); p.add(0x18c + 8).writeU16(0x0000);
      p.add(0x158).writeS32(4);                                       // gamma_shift
      p.add(0x164).writePointer(o.g8 ? gt : ptr(0));                  // gamma_table
      p.add(0x168).writePointer(o.g8 ? gt : ptr(0));                  // gamma_from_1
      p.add(0x16c).writePointer(o.g8 ? gt : ptr(0));                  // gamma_to_1
      p.add(0x170).writePointer(o.g8 ? g16 : ptr(0));                 // gamma_16_table
      p.add(0x174).writePointer(ptr(0));                              // gamma_16_from_1
      p.add(0x178).writePointer(ptr(0));                              // gamma_16_to_1
      const sig = o.sig || [8, 8, 8, 8, 8];
      for (let j = 0; j < 5; j++) p.add(0x181 + j).writeU8(sig[j]);   // sig_bits
      p.add(0x188).writePointer(o.tr ? trans : ptr(0));               // trans
      p.add(0x1ec).writePointer(o.look ? lookup : ptr(0));            // palette_lookup
      p.add(0x1f0).writePointer(o.look ? dither : ptr(0));            // dither_index
      p.add(0x214).writeU8(0);                                        // rgb_to_gray_status
      // png_do_rgb_to_gray's three weights, the bytes at +0x215, +0x216 and +0x217 (batch c3bg's
      // fixture c3bg_gray reads them at the same offsets).
      p.add(0x215).writeU8(54); p.add(0x216).writeU8(183); p.add(0x217).writeU8(19);
      out['rt' + i] = p;
    });
    return out;
  },
});
