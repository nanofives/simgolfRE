// Fixtures for C3 batch c3cm (sound.dll: statically linked MSVC CRT functions). Every string, buffer and object
// is allocated here; no live sound object, device, list, buffer or CRT heap block is handed to an arm, and none
// of the functions under test allocates or frees. Layouts and byte patterns come from the disassembly cited in
// shim/src/re/c3cm.cpp. All text is synthetic (invented here, not taken from the game).
//
// Each arena is 0x1000 bytes so the allocation is page aligned and the offsets below are dword aligned exactly
// as written: strchr 0x10045680 and strcpy 0x1004a010 both branch on the low two bits of a pointer
// (0x10045690, 0x1004a085), so the alignment of every string matters.
//
// ONE FIXTURE TOUCHES MODULE MEMORY: c3cm_cvtdate returns the address of the six dwords cvtdate writes
// (0x10068900..0x1006891b). The fixture only computes that address; the registry entry lists it as the state
// region, so diff_hook snapshots it, runs each arm from the same bytes and restores it afterwards.
Object.assign(globalThis.DIFF_FIXTURES, {

  // --- 0x10045680 strchr -------------------------------------------------------------------------------
  // s_alpha   0x000  16 letters + terminator, dword aligned: the four byte positions of the first dword, a
  //                  later dword and the terminator itself (c == 0).
  // s_alpha1/2/3     the same string entered at offsets 1, 2 and 3: the byte loop at 0x10045698 runs 3, 2 and
  //                  1 times before the pointer is aligned.
  // s_nul1    0x011  a zero byte at an odd address: the byte loop ends at 0x100456a1.
  // s_empty   0x040  a zero byte at an aligned address.
  // s_fp_c    0x020  41 42 43 F8 | 44 45 46 78 | 00. Searched for 0x78, the first dword sets bit 31 of the
  //                  candidate mask because 0xF8 ^ 0x78 == 0x80 while holding neither 0x78 nor a zero byte, so
  //                  the candidate block falls through to the loop again (0x1004571e); the second dword really
  //                  ends in 0x78 (0x10045718).
  // s_fp_z    0x030  41 42 43 80 | 44 45 00 47. Searched for 0x7a, the first dword sets bit 31 of the ZERO mask
  //                  (top byte 0x80) and is rejected at 0x100456ea; the second holds a real zero in byte 2
  //                  (0x100456e8).
  // s_z0..z2  0x050/0x060/0x070  a zero byte before the first 0x71 in byte 0, 1 and 2 of the dword
  //                  (0x10045701, 0x10045709, 0x10045714).
  // s_z3      0x080  72 73 74 00: searched for 0x80 the mask fires on the top byte and the zero is found at
  //                  0x1004571c; searched for 0x71 no candidate fires and the zero exit is 0x100456f2.
  c3cm_chr() {
    const a = __keepAlloc(0x1000);
    for (let i = 0; i < 0x400; i++) a.add(i * 4).writeU32(0);
    a.add(0x00).writeUtf8String('abcdefghijklmnop');
    a.add(0x20).writeByteArray([0x41, 0x42, 0x43, 0xF8, 0x44, 0x45, 0x46, 0x78, 0x00]);
    a.add(0x30).writeByteArray([0x41, 0x42, 0x43, 0x80, 0x44, 0x45, 0x00, 0x47]);
    a.add(0x50).writeByteArray([0x00, 0x71, 0x71, 0x71, 0x00]);
    a.add(0x60).writeByteArray([0x72, 0x00, 0x71, 0x71, 0x00]);
    a.add(0x70).writeByteArray([0x72, 0x73, 0x00, 0x71, 0x00]);
    a.add(0x80).writeByteArray([0x72, 0x73, 0x74, 0x00, 0x00]);
    a.add(0x90).writeUtf8String('zz');
    return {
      obj: a,
      s_alpha: a, s_alpha1: a.add(1), s_alpha2: a.add(2), s_alpha3: a.add(3), s_nul1: a.add(0x11),
      s_fp_c: a.add(0x20), s_fp_z: a.add(0x30), s_empty: a.add(0x40),
      s_z0: a.add(0x50), s_z1: a.add(0x60), s_z2: a.add(0x70), s_z3: a.add(0x80), s_zz: a.add(0x90),
    };
  },

  // --- 0x10042690 strstr -------------------------------------------------------------------------------
  // Haystacks and needles in one arena so a returned pointer reads as obj+0x.. in the CSV. h_axbab makes the
  // second character mismatch at 0x100426cf, h_abcdx the even-position mismatch at 0x100426e2 and h_abcxe the
  // odd-position one at 0x100426f5; n_empty and n_a take the two early exits (0x1004269f and the strchr tail
  // jump at 0x100426fe).
  c3cm_str() {
    const a = __keepAlloc(0x1000);
    for (let i = 0; i < 0x400; i++) a.add(i * 4).writeU32(0);
    const put = (off, s) => { a.add(off).writeUtf8String(s); return a.add(off); };
    return {
      obj: a,
      h_abcabcabd: put(0x00, 'abcabcabd'),
      h_aaab: put(0x10, 'aaab'),
      h_fox: put(0x20, 'the quick brown fox jumps over the lazy dog'),
      h_empty: put(0x50, ''), n_empty: a.add(0x50),
      n_a: put(0x54, 'a'),
      n_ab: put(0x58, 'ab'),
      n_abc: put(0x5c, 'abc'),
      n_abcd: put(0x60, 'abcd'), h_abcd: a.add(0x60),
      n_abd: put(0x68, 'abd'),
      n_zzz: put(0x70, 'zzz'),
      n_lazydog: put(0x78, 'lazy dog'),
      n_dog: put(0x84, 'dog'),
      h_abcabd: put(0x88, 'abcabd'), n_abcabd: a.add(0x88),
      h_axbab: put(0x90, 'axbab'),
      h_xx: put(0x98, 'xxabcdefxx'),
      h_abcde: put(0xa8, 'abcde'), n_abcde: a.add(0xa8),
      h_abcdx: put(0xb0, 'abcdx'),
      n_c: put(0xb8, 'c'),
      n_aab: put(0xc0, 'aab'),
      h_abcxe: put(0xc8, 'abcxe'),
      n_abcdef: put(0xd0, 'abcdef'),
    };
  },

  // --- 0x1004a010 strcpy -------------------------------------------------------------------------------
  // Sources of length 0, 4, 5, 6, 7, 15 and 20 at aligned addresses put the terminator in each of the four byte
  // positions of a dword; c_u1 / c_u3 start at odd addresses so the byte prologue at 0x1004a08d runs three and
  // one time, c_u1nul is a terminator at an odd address (0x1004a092), and c_fp holds a 0x80 byte in the top
  // position of its first dword, the false positive that stores the dword and continues at 0x1004a0d8.
  // The four destinations lie inside one 0x100-byte block (dstmem) which the registry lists as the state
  // region, with a pre-filled pattern so that every byte written and every byte left alone is compared.
  c3cm_cpy() {
    const a = __keepAlloc(0x1000);
    for (let i = 0; i < 0x400; i++) a.add(i * 4).writeU32(0);
    a.add(0x00).writeUtf8String('abcdefghijklmno');
    a.add(0x20).writeUtf8String('abcd');
    a.add(0x30).writeUtf8String('abcde');
    a.add(0x40).writeUtf8String('abcdef');
    a.add(0x50).writeUtf8String('abcdefg');
    a.add(0x60).writeUtf8String('');
    a.add(0x71).writeUtf8String('xyz');
    a.add(0x7d).writeUtf8String('');
    a.add(0x83).writeUtf8String('Q');
    a.add(0x90).writeByteArray([0x41, 0x42, 0x43, 0x80, 0x44, 0x00, 0x00, 0x00]);
    a.add(0xa0).writeUtf8String('0123456789ABCDEFGHIJ');
    for (let i = 0; i < 0x40; i++) a.add(0x200 + i * 4).writeU32((0x5a5a0000 + i) >>> 0);  // dstmem pattern
    return {
      obj: a, dstmem: a.add(0x200),
      d0: a.add(0x200), d1: a.add(0x241), d2: a.add(0x282), d3: a.add(0x2c3),
      c_15: a.add(0x00), c_4: a.add(0x20), c_5: a.add(0x30), c_6: a.add(0x40), c_7: a.add(0x50),
      c_0: a.add(0x60), c_u1: a.add(0x71), c_u1nul: a.add(0x7d), c_u3: a.add(0x83),
      c_fp: a.add(0x90), c_long: a.add(0xa0),
    };
  },

  // --- 0x10041aad iosResetVtable -----------------------------------------------------------------------
  // Twelve independent objects. Each block b<i> is 0x100 bytes filled with a per-object dword pattern; the
  // object pointer o<i> is block+0x40, so the dword the function reads at [this-8] is block+0x38. That dword
  // points at a small virtual-base table whose second dword is the offset, and the store at 0x10041ab3 lands at
  // block+0x38+offset, which stays inside the block for every offset used here (the largest is 0xa0).
  c3cm_ios() {
    const offsets = [0, 4, 8, 0xc, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x80, 0xa0];
    const out = {};
    offsets.forEach((off, i) => {
      const b = __keepAlloc(0x100);
      for (let k = 0; k < 0x40; k++) b.add(k * 4).writeU32((0x61000000 + i * 0x1000 + k) >>> 0);
      const vb = __keepAlloc(0x10);
      vb.writeU32((0xbb000000 + i) >>> 0);
      vb.add(4).writeU32(off);
      vb.add(8).writeU32((0xcc000000 + i) >>> 0);
      b.add(0x38).writePointer(vb);
      out['b' + i] = b;
      out['o' + i] = b.add(0x40);
    });
    return out;
  },

  // --- 0x10049604 crtValidatePointer -------------------------------------------------------------------
  // Two writable blocks. The vectors that pass an address in the first 64 KB of the address space use a plain
  // number, which diff_hook turns into a pointer; that range is never mapped, so the import reports it bad.
  c3cm_valid() {
    const buf = __keepAlloc(0x200);
    for (let i = 0; i < 0x80; i++) buf.add(i * 4).writeU32((0x77000000 + i) >>> 0);
    const buf2 = __keepAlloc(0x40);
    for (let i = 0; i < 0x10; i++) buf2.add(i * 4).writeU32((0x88000000 + i) >>> 0);
    return { buf, buf2 };
  },

  // --- 0x10051319 cvtdate ------------------------------------------------------------------------------
  // The function reads two 13-entry dword tables at 0x1006891c and 0x10068950 and one dword at 0x10068874, and
  // writes six dwords between 0x10068900 and 0x1006891b. Nothing is seeded: the tables and the dword at
  // 0x10068874 are read-only inputs both arms see identically, and the six written dwords are the state region
  // (returned here), so diff_hook compares them after each arm and restores them afterwards.
  c3cm_cvtdate() {
    return { dst: Process.getModuleByName('sound.dll').base.add(0x00068900) };
  },
});
