// Batch c3al fixtures (jgld.dll). Only synthetic values: png_struct / png_info images built from the field
// offsets the functions touch (transformations +0x70, interlaced +0x123, bit_depth +0x127; valid +8,
// palette +0x10, num_palette +0x14, srgb_intent +0x2c, sig_bit +0x44, x_offset +0x64, y_offset +0x68,
// offset_unit_type +0x6c, hist +0x7c), plus jgld list/matrix objects (layouts from re/match/jgld_list.cpp
// and jgld_math.cpp). Every buffer is larger than the fields that are read, so a write outside them is caught.
Object.assign(globalThis.DIFF_FIXTURES, {
  // Twelve png_struct images (0x200 bytes each) with distinct transformations (+0x70), bit_depth (+0x127) and
  // interlaced (+0x123) values, and six png_info images (0x100 bytes each) with distinct valid (+8), palette
  // (+0x10) and num_palette (+0x14) values. Four out-parameter words for png_get_PLTE, two sig_bit records.
  c3al_png() {
    const PS = [            // [transformations, bit_depth, interlaced]
      [0x00000000, 0x10, 0x01], [0x00001234, 0x08, 0x00], [0xffffffff, 0x10, 0x00], [0x5a5a5a5a, 0x01, 0x07],
      [0x00000001, 0x10, 0xff], [0x00000010, 0x02, 0x00], [0x00000002, 0x10, 0x01], [0x80000000, 0x04, 0x02],
      [0x0000000f, 0x10, 0x00], [0x12345678, 0x08, 0x01], [0xfffffffe, 0x11, 0x00], [0x7fffffff, 0x10, 0xff],
    ];
    const INFO = [          // [valid, palette, num_palette]
      [0x00000000, 0x40404040, 0x0000], [0x00000008, 0x50505050, 0x0101], [0xfffffff7, 0x60606060, 0xffff],
      [0x0000000c, 0x70707070, 0x0003], [0xffffffff, 0x80808080, 0x8000], [0x00000009, 0x00000000, 0x0001],
    ];
    const o = {};
    PS.forEach((p, i) => {
      const s = Memory.alloc(0x200);
      s.writeByteArray(new Array(0x200).fill(0x3c));  // non-zero filler: a stray write shows up
      s.add(0x70).writeU32(p[0]);
      s.add(0x127).writeU8(p[1]);
      s.add(0x123).writeU8(p[2]);
      o['ps' + i] = s;
    });
    INFO.forEach((v, i) => {
      const f = Memory.alloc(0x100);
      f.writeByteArray(new Array(0x100).fill(0x5e));  // non-zero filler: a stray write shows up
      f.add(8).writeU32(v[0]);
      f.add(0x10).writeU32(v[1]);
      f.add(0x14).writeU16(v[2]);
      o['info' + i] = f;
    });
    o.outp = Memory.alloc(8); o.outn = Memory.alloc(8);
    o.outp2 = Memory.alloc(8); o.outn2 = Memory.alloc(8);
    [o.outp, o.outn, o.outp2, o.outn2].forEach((p) => { p.writeU32(0xa5a5a5a5); p.add(4).writeU32(0xa5a5a5a5); });
    o.sb0 = Memory.alloc(8); o.sb1 = Memory.alloc(8);
    [0x01, 0x02, 0x03, 0x04, 0x05, 0xee, 0xee, 0xee].forEach((b, i) => o.sb0.add(i).writeU8(b));
    [0xff, 0x7f, 0x00, 0x10, 0x81, 0xee, 0xee, 0xee].forEach((b, i) => o.sb1.add(i).writeU8(b));
    return o;
  },

  // Sixteen four-byte big-endian samples for png_get_uint_32, each in its own 8-byte buffer (the trailing four
  // bytes are a guard pattern the function must not read).
  c3al_u32() {
    const V = [
      [0x00, 0x00, 0x00, 0x00], [0x00, 0x00, 0x00, 0x01], [0x00, 0x00, 0x01, 0x00], [0x00, 0x01, 0x00, 0x00],
      [0x01, 0x00, 0x00, 0x00], [0xff, 0xff, 0xff, 0xff], [0x80, 0x00, 0x00, 0x00], [0x7f, 0xff, 0xff, 0xff],
      [0x12, 0x34, 0x56, 0x78], [0xde, 0xad, 0xbe, 0xef], [0x00, 0xff, 0x00, 0xff], [0xff, 0x00, 0xff, 0x00],
      [0x80, 0x80, 0x80, 0x80], [0x00, 0x00, 0x00, 0xff], [0x01, 0x02, 0x03, 0x04], [0xaa, 0x55, 0xaa, 0x55],
    ];
    const o = {};
    V.forEach((bytes, i) => {
      const p = Memory.alloc(8);
      bytes.forEach((b, k) => p.add(k).writeU8(b));
      [0xc3, 0xc3, 0xc3, 0xc3].forEach((b, k) => p.add(4 + k).writeU8(b));
      o['b' + i] = p;
    });
    return o;
  },

  // Twelve LinkedList images (0x1c bytes) differing only in m_count (+0xc); the other fields hold filler so a
  // stray read of them would not look like a count.
  c3al_lists() {
    const COUNT = [0, 1, 2, 5, 7, 42, 0x100, 0x10000, 0x7fffffff, -1, -0x80000000, 3];
    const o = {};
    COUNT.forEach((c, i) => {
      const l = Memory.alloc(0x1c);
      l.writePointer(ptr(0));                       // vptr: never called by count
      l.add(4).writePointer(ptr(0x1000 + i));       // m_data
      l.add(8).writeU8(0x30 + i);                   // m_flag
      l.add(0xc).writeS32(c);                       // m_count
      l.add(0x10).writePointer(ptr(0x2000 + i));    // m_head
      l.add(0x14).writePointer(ptr(0x3000 + i));    // m_cur
      l.add(0x18).writeS32(0x4000 + i);             // m_ordered
      o['l' + i] = l;
    });
    return o;
  },

  // Three ListNode images (0x14 bytes) pre-filled with a pattern, so the zero stores into prev (+4) and next
  // (+8) change state, plus one data pointer that is a real address.
  c3al_nodes() {
    const o = {};
    for (let i = 0; i < 3; i++) {
      const n = Memory.alloc(0x14);
      n.writePointer(ptr(0x7000 + i));
      n.add(4).writePointer(ptr(0x7100 + i));
      n.add(8).writePointer(ptr(0x7200 + i));
      n.add(0xc).writePointer(ptr(0x7300 + i));
      n.add(0x10).writeU8(0x90 + i);
      o['n' + i] = n;
    }
    o.data = Memory.alloc(0x10);
    o.data.writeU32(0x13579bdf);
    return o;
  },

  // Three 4x4 matrices (0x40 bytes, filled with an increasing dword pattern so the three stores at +0x34..+0x3c
  // are visible) and four three-dword vectors.
  c3al_matrix() {
    const o = {};
    for (let i = 0; i < 3; i++) {
      const m = Memory.alloc(0x40);
      for (let k = 0; k < 0x10; k++) m.add(4 * k).writeU32(0x01010101 * (k + 1) + i);
      o['m' + i] = m;
    }
    const V = [[0, 0, 0], [0x3f800000, 0xbf800000, 0x40000000], [0xffffffff, 0x7fffffff, 0x80000000],
               [0x11111111, 0x22222222, 0x33333333]];
    V.forEach((v, i) => {
      const p = Memory.alloc(0x10);
      v.forEach((d, k) => p.add(4 * k).writeU32(d));
      p.add(0xc).writeU32(0xcccccccc);   // guard: setTranslation copies three dwords, not four
      o['v' + i] = p;
    });
    return o;
  },
});
